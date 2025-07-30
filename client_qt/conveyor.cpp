#include "conveyor.h"
#include "./ui_conveyor.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QFile>
#include "videoplayer.h"
#include "video_mqtt.h"
#include "video_client_functions.hpp"

#include <QMouseEvent>
#include "cardhovereffect.h"
#include "error_message_card.h"
#include <QKeyEvent>

#include "sectionboxwidget.h"


ConveyorWindow::ConveyorWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ConveyorWindow)
    , m_client(nullptr)
    , subscription(nullptr)
    , DeviceLockActive(false) //초기는 정상!
    , conveyorStartDateEdit(nullptr)  //  초기화 추가
    , conveyorEndDateEdit(nullptr)    //  초기화 추가
    , statisticsTimer(nullptr)
{

    ui->setupUi(this);
    setWindowTitle("Conveyor Control");
    setupErrorCardUI();

    // 1.  QMainWindow 전체 배경 흰색
    setStyleSheet("QMainWindow { background-color: white; }");

    // 2. Central Widget 흰색 + 적절한 여백
    if (ui->centralwidget) {
        ui->centralwidget->setContentsMargins(12, 12, 12, 15);
        ui->centralwidget->setStyleSheet("QWidget { background-color: white; }");

        if (ui->centralwidget->layout()) {
            ui->centralwidget->layout()->setContentsMargins(0, 0, 0, 0);
            ui->centralwidget->layout()->setSpacing(5);
        }
    }

    // 3. Frame 흰색
    if (ui->frame) {
        ui->frame->setStyleSheet("QFrame { background-color: white; }");
        if (ui->frame->layout()) {
            ui->frame->layout()->setContentsMargins(5, 5, 5, 5);
            ui->frame->layout()->setSpacing(5);
        }
    }

    // 4.  메인 위젯(widget) 전체 흰색
    if (ui->widget) {
        ui->widget->setStyleSheet("QWidget { background-color: white; }");
        if (ui->widget->layout()) {
            ui->widget->layout()->setContentsMargins(5, 5, 5, 5);
        }
    }

    // 5.  bottomSectionWidget 흰색 + 아래쪽 여백
    if (ui->bottomSectionWidget) {
        ui->bottomSectionWidget->setStyleSheet("QWidget { background-color: white; }");
        if (ui->bottomSectionWidget->layout()) {
            ui->bottomSectionWidget->layout()->setContentsMargins(5, 5, 5, 15);
        }
    }

    // 6.  모든 하위 위젯들도 흰색
    if (ui->topBannerWidget) {
        ui->topBannerWidget->setStyleSheet("QWidget { background-color: white; }");
    }
    if (ui->cameraSectionWidget) {
        ui->cameraSectionWidget->setStyleSheet("QWidget { background-color: white; }");
    }
    if (ui->groupControl) {
        ui->groupControl->setStyleSheet("QGroupBox { background-color: white; }");
    }


    showConveyorNormal();

    setupControlButtons(); // 먼저 호출!
    setupLogWidgets();     // 나중에 호출!
    setupRightPanel();

    setupHomeButton();
    setupMqttClient(); //mqtt 설정
    connectToMqttBroker(); //연결 시도

    // 로그 더블클릭 이벤트 연결
    //connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &ConveyorWindow::on_listWidget_itemDoubleClicked);


    ui->labelCamRPi->setStyleSheet("background-color: black; border-radius: 12px;");
    ui->labelCamHW->setStyleSheet("background-color: black; border-radius: 12px;");


    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    rpiStreamer = new Streamer("rtsp://192.168.0.52:8555/process2", this);

    // 한화 카메라 스트리머 객체 생성
    hwStreamer = new Streamer("rtsp://192.168.0.18:8553/stream_pno", this);

    // signal-slot
    connect(rpiStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateRPiImage);
    rpiStreamer->start();

    // 한화 signal-slot 연결
    connect(hwStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateHWImage);
    hwStreamer->start();

    statisticsTimer = new QTimer(this);
    connect(statisticsTimer, &QTimer::timeout, this, &ConveyorWindow::requestStatisticsData);

    //차트
    deviceChart = new DeviceChart("컨베이어", this);
    connect(deviceChart, &DeviceChart::refreshRequested, this, &ConveyorWindow::onChartRefreshRequested);

    QTimer::singleShot(100, this, [this]() {
        initializeDeviceChart();
    });
    if (failureRateSeries) {
        updateFailureRate(0.0);  // 0% 불량률 = 100% 정상
        qDebug() << "불량률 초기값 설정: 0% (정상 100%)";
    }

}

ConveyorWindow::~ConveyorWindow()
{
    rpiStreamer->stop();
    rpiStreamer->wait();

    hwStreamer->stop();
    hwStreamer->wait();

    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->disconnectFromHost();
    }
    delete ui;
}

void ConveyorWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    this->showFullScreen();
}

void ConveyorWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        this->showNormal();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void ConveyorWindow::setupMqttClient(){ //mqtt 클라이언트 초기 설정 MQTT 클라이언트 설정 (주소, 포트, 시그널 연결 등)
    m_client = new QMqttClient(this);
    reconnectTimer = new QTimer(this);
    m_client->setHostname(mqttBroker); //브로커 서버에 연결 공용 mqtt 서버
    m_client->setPort(mqttPort);
    m_client->setClientId("VisionCraft_conveyor" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    connect(m_client, &QMqttClient::connected, this, &ConveyorWindow::onMqttConnected); // QMqttClient가 연결이 되었다면 ConveyorWindow에 있는 저 함수중에 onMQTTCONNECTED를 실행
    connect(m_client, &QMqttClient::disconnected, this, &ConveyorWindow::onMqttDisConnected);
    //connect(m_client, &QMqttClient::messageReceived, this, &ConveyorWindow::onMqttMessageReceived);
    connect(reconnectTimer, &QTimer::timeout, this, &ConveyorWindow::connectToMqttBroker);
    connect(ui->pushButton, &QPushButton::clicked, this, &ConveyorWindow::onSearchClicked);
}

void ConveyorWindow::connectToMqttBroker(){ //브로커 연결  실제 연결 시도만!

    if(m_client->state() == QMqttClient::Disconnected){
        m_client->connectToHost();
    }

}

void ConveyorWindow::onMqttConnected(){
    qDebug() << "MQTT Connected - conveyor Control";
    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived,
                this, &ConveyorWindow::onMqttMessageReceived);
    }

    auto statsSubscription = m_client->subscribe(QString("factory/conveyor_01/msg/statistics"));
    if(statsSubscription){
        connect(statsSubscription, &QMqttSubscription::messageReceived,
                this, &ConveyorWindow::onMqttMessageReceived);
        qDebug() << "ConveyorWindow - 통계 토픽 구독됨";
    }

    auto failureSubscription = m_client->subscribe(QString("factory/conveyor_01/log/response"));
    if(failureSubscription){
        connect(failureSubscription, &QMqttSubscription::messageReceived,
                this, &ConveyorWindow::onMqttMessageReceived);
    }

    auto failureTimer = new QTimer(this);
    connect(failureTimer, &QTimer::timeout, this, &ConveyorWindow::requestFailureRate);
    failureTimer->start(60000); // 60초마다 불량률 요청

    //if(statisticsTimer && !statisticsTimer->isActive()) {
    //    statisticsTimer->start(60000);  // 3초마다 요청
    //}


    reconnectTimer->stop(); //연결이 성공하면 재연결 타이며 멈추기!


}

void ConveyorWindow::onMqttDisConnected(){
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(5000);
    }

    if(statisticsTimer && statisticsTimer->isActive()) {
        statisticsTimer->stop();
    }
    subscription=NULL; //초기화
}

void ConveyorWindow::onMqttMessageReceived(const QMqttMessage &message){  //매개변수 수정
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  //토픽 정보도 가져올 수 있음

    if(isConveyorDateSearchMode && (topicStr.contains("/log/error") || topicStr.contains("/log/info"))) {
        qDebug() << "[컨베이어] 날짜 검색 모드이므로 실시간 로그 무시:" << topicStr;
        return;  // 실시간 로그 무시!
    }

    // 🐛 모든 메시지 디버깅
    qDebug() << "=== MainWindow 메시지 수신 ===";
    qDebug() << "토픽:" << topicStr;
    qDebug() << "내용:" << messageStr;

    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가

    if(topicStr.contains("factory/conveyor_01/log/info")){
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1];

        if(deviceId == "conveyor_01"){
            showConveyorNormal(); // 에러 상태 초기화
            logMessage("컨베이어 정상 동작");
        }
        return;
    }

    if(topicStr == "factory/conveyor_01/msg/statistics") {
        QJsonDocument doc = QJsonDocument::fromJson(messageStr.toUtf8());
        QJsonObject data = doc.object();
        onDeviceStatsReceived("conveyor_01", data);
        // logMessage(QString("컨베이어 통계 - 평균:%1 현재:%2")
        //                .arg(data["average"].toInt())
        //                .arg(data["current_speed"].toInt()));
        return;
    }

    if(topicStr == "factory/conveyor_01/log/response") {
        QJsonDocument doc = QJsonDocument::fromJson(messageStr.toUtf8());
        QJsonObject response = doc.object();

        if(response.contains("data")) {
            QJsonObject data = response["data"].toObject();
            if(data.contains("message")) {
                QJsonObject message = data["message"].toObject();
                QString failureRate = message["failure"].toString();

                // 백분률로 변환 (1.0000 → 100%)
                double rate = failureRate.toDouble() * 100;

                if (failureRateSeries) {
                    updateFailureRate(rate);
                    qDebug() << "불량률 자동 업데이트:" << rate << "%";
                }

                QString displayRate = QString::number(rate, 'f', 2) + "%";

                //  textErrorStatus에 불량률 업데이트
                if(textErrorStatus) {
                    QString currentText = textErrorStatus->toPlainText();
                    // "불량률: 계산중..." 부분을 실제 값으로 교체
                    currentText.replace("불량률: 계산중...", "불량률: " + displayRate);
                    textErrorStatus->setText(currentText);
                }
            }
        }
        return;
    }

    if(topicStr == "conveyor_03/status"){
        if(messageStr == "on"){
            //logMessage("컨베이어가 시작되었습니다.");
            logError("컨베이어가 시작되었습니다.");
            showConveyorNormal();
            showConveyorError("컨베이어가 시작되었습니다.");
            updateErrorStatus();
            emit deviceStatusChanged("conveyor_03", "on");
        } else if(messageStr == "off"){
            logMessage("컨베이어가 정지되었습니다.");
            showConveyorNormal();
            emit deviceStatusChanged("conveyor_03", "off");
        }
        // 나머지 명령은 무시
    } else if(topicStr == "conveyor_01/status"){
        if(messageStr != "on" && messageStr != "off"){
            // error_mode, speed 등 기타 명령 처리 (필요시 기존 코드 복사)
            if(messageStr == "error_mode"){
                logError("컨베이어 속도 오류");
            } else if(messageStr.startsWith("SPEED_")){
                logError("컨베이어 오류 감지: " + messageStr);
            }
        }
    }
}

void ConveyorWindow::onMqttError(QMqttClient::ClientError error){
    logMessage("MQTT 에러 발생");

}

void ConveyorWindow::publishControlMessage(const QString &cmd){
    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->publish(mqttControllTopic, cmd.toUtf8());
        logMessage("제어 명령 전송: " + cmd);
        qDebug() << "MQTT 발송:" << mqttControllTopic << cmd;
    }
    else{
        logMessage("MQTT 연결 안됨");
        qDebug() << "MQTT 상태:" << m_client->state(); // 이 줄 추가

    }
}


void ConveyorWindow::logMessage(const QString &message){
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog != NULL){
        textEventLog->append("[" + timer +  "]" + message);
    }
}

void ConveyorWindow::showConveyorError(QString conveyorErrorType){
    qDebug() << "오류 상태 함수 호출됨";
    QString datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    if (errorCard) {
        errorCard->setErrorState(conveyorErrorType, datetime, "컨베이어 구역", "conveyor_CAMERA1");
    }
}

void ConveyorWindow::showConveyorNormal(){
    qDebug() << "정상 상태 함수 호출됨";
    if (errorCard) {
        errorCard->setNormalState();
    }

}


void ConveyorWindow::initializeUI(){

}


void ConveyorWindow::setupControlButtons() {
    // === 컨베이어 시작 버튼 ===
    btnConveyorOn = new QPushButton("컨베이어 시작");
    btnConveyorOn->setFixedHeight(32);
    btnConveyorOn->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:pressed {
            background-color: #ea580c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
            cursor: not-allowed;
        }
    )");
    connect(btnConveyorOn, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOnClicked);

    // === 컨베이어 정지 버튼 ===
    btnConveyorOff = new QPushButton("컨베이어 정지");
    btnConveyorOff->setFixedHeight(32);
    btnConveyorOff->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:pressed {
            background-color: #ea580c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
            cursor: not-allowed;
        }
    )");
    connect(btnConveyorOff, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOffClicked);

    // === 기기 잠금 버튼 ===
    btnDeviceLock = new QPushButton("기기 잠금");
    btnDeviceLock->setFixedHeight(32);
    btnDeviceLock->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:pressed {
            background-color: #ea580c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
            cursor: not-allowed;
        }
    )");
    connect(btnDeviceLock, &QPushButton::clicked, this, &ConveyorWindow::onDeviceLock);

    // === 시스템 리셋 버튼 ===
    btnSystemReset = new QPushButton("시스템 리셋");
    btnSystemReset->setFixedHeight(32);
    btnSystemReset->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:pressed {
            background-color: #ea580c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
            cursor: not-allowed;
        }
    )");
    connect(btnSystemReset, &QPushButton::clicked, this, &ConveyorWindow::onSystemReset);

    qDebug() << " setupControlButtons 완료";
}

void ConveyorWindow::onConveyorOnClicked(){
    qDebug()<<"컨베이어 시작 버튼 클릭됨";
    publishControlMessage("on");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "conveyor_03";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    //emit requestMqttPublish("factory/msg/status", "on");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

    btnConveyorOn->setStyleSheet(R"(
        QPushButton {
            background-color: #fb923c;
            color: white;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #ea580c;
            color: white;
        }
    )");
}

void ConveyorWindow::onConveyorOffClicked(){
    qDebug()<<"컨베이어 정지 버튼 클릭됨";
    publishControlMessage("off");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "conveyor_03";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    //emit requestMqttPublish("factory/msg/status", "off");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

    btnConveyorOn->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
        }
    )");
}

void ConveyorWindow::requestFailureRate() {
    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->publish(QMqttTopicName("factory/conveyor_01/log/request"), "{}");
    }
}
void ConveyorWindow::onDeviceLock(){
    if(!DeviceLockActive){
        DeviceLockActive=true;

        btnConveyorOn->setEnabled(false);
        btnConveyorOff->setEnabled(false);
        btnDeviceLock->setText("기기 잠금");
        //speedSlider->setEnabled(false);

        btnDeviceLock->setStyleSheet(R"(
            QPushButton {
                background-color: #fb923c;
                color: white;
                font-size: 12px;
                border: none;
                border-radius: 6px;
                padding: 6px 12px;
            }
            QPushButton:hover {
                background-color: #ea580c;
                color: white;
            }
        )");
        qDebug()<<"기기 잠금 버튼 클릭됨";
        //publishControlMessage("off");//EMERGENCY_STOP
        logMessage("기기 잠금 명령 전송!");
    }else {
        // 잠금 해제 - 시스템 리셋 호출
        onSystemReset();
    }
}

void ConveyorWindow::onSystemReset(){
    DeviceLockActive= false;
    btnConveyorOn->setEnabled(true);
    btnConveyorOff->setEnabled(true);
    //btnConveyorReverse->setEnabled(true);
    //speedSlider->setEnabled(true);
    btnDeviceLock->setText("기기 잠금");
    //btnDeviceLock->setStyleSheet("");

    QString defaultButtonStyle = R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
        QPushButton:disabled {
            background-color: #d1d5db;
            color: #9ca3af;
        }
    )";

    btnConveyorOn->setStyleSheet(defaultButtonStyle);
    btnConveyorOff->setStyleSheet(defaultButtonStyle);
    btnDeviceLock->setStyleSheet(defaultButtonStyle);
    btnSystemReset->setStyleSheet(defaultButtonStyle);


    qDebug()<<"다시 시작";
    //publishControlMessage("off");

    logMessage("컨베이어 시스템 리셋 완료!");
}


void ConveyorWindow::setupHomeButton(){

    QHBoxLayout *topLayout = qobject_cast<QHBoxLayout*>(ui->topBannerWidget->layout());

    // 홈 버튼
    QPushButton* btnHome = new QPushButton();
    btnHome->setIcon(QIcon(":/ui/icons/images/home.png"));
    btnHome->setIconSize(QSize(20, 20));
    btnHome->setFixedSize(35, 35);
    btnHome->setStyleSheet(R"(
        QPushButton {
            background-color: #f97316;
            border-radius: 8px;
            border: none;
        }
        QPushButton:hover {
            background-color: #ffb366;
        }
    )");
    topLayout->insertWidget(0, btnHome);
    connect(btnHome, &QPushButton::clicked, this, &ConveyorWindow::gobackhome);

    // 제목 섹션 (아이콘 옆)
    QWidget* titleWidget = new QWidget();
    QVBoxLayout* titleLayout = new QVBoxLayout(titleWidget);
    titleLayout->setSpacing(2);
    titleLayout->setContentsMargins(10, 0, 0, 0);

    // 메인 제목
    QLabel* mainTitle = new QLabel("Conveyor Control Dashboard");
    mainTitle->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            font-weight: bold;
        }
    )");

    // 서브 제목
    QLabel* subTitle = new QLabel("통합 모니터링 및 제어 시스템");
    subTitle->setStyleSheet(R"(
        QLabel {
            color: #6b7280;
            font-size: 12px;
        }
    )");

    titleLayout->addWidget(mainTitle);
    titleLayout->addWidget(subTitle);
    topLayout->insertWidget(1, titleWidget);
}

void ConveyorWindow::gobackhome(){
    this->hide();

    if(this->parent()){
        QWidget *parentWidget = qobject_cast<QWidget*>(this->parent());
        if(parentWidget){
            parentWidget->show();
            parentWidget->raise();
            parentWidget->activateWindow();
        }
    }

}

void ConveyorWindow::requestStatisticsData() {
    if(m_client && m_client->state() == QMqttClient::Connected) {
        QJsonObject request;
        request["device_id"] = "conveyor_01";

        QDateTime now = QDateTime::currentDateTime();
        QDateTime oneMinuteAgo = now.addSecs(-60);
        QJsonObject timeRange;
        timeRange["start"] = oneMinuteAgo.toMSecsSinceEpoch();
        timeRange["end"] = now.toMSecsSinceEpoch();
        request["time_range"] = timeRange;

        QJsonDocument doc(request);

        m_client->publish(QString("factory/statistics"), doc.toJson(QJsonDocument::Compact));
        m_client->publish(QMqttTopicName("factory/conveyor_01/log/request"), "{}");
        qDebug() << "ConveyorWindow - 컨베이어 통계 요청 전송";
    }
}

void ConveyorWindow::updateErrorStatus(){
}

void ConveyorWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 컨베이어 오류" + errorType);
    }
}

void ConveyorWindow::setupLogWidgets() {
    QHBoxLayout *bottomLayout = qobject_cast<QHBoxLayout*>(ui->bottomSectionWidget->layout());
    if (!bottomLayout) return;

    // 기존 위젯 제거
    delete ui->textLog;
    delete ui->groupControl;
    ui->textLog = nullptr;
    ui->groupControl = nullptr;

    // 로그
    textEventLog = new QTextEdit(this);
    textEventLog->setMinimumHeight(240);
    textEventLog->setStyleSheet("border: none; background-color: transparent;");

    // 상태
    textErrorStatus = new QTextEdit(this);
    textErrorStatus->setReadOnly(true);
    textErrorStatus->setMinimumHeight(240);
    textErrorStatus->setStyleSheet("border: none; background-color: transparent;");

    // 컨트롤 버튼 생성
    setupControlButtons();

    QList<QWidget*> controlWidgets = {
        btnConveyorOn, btnConveyorOff, btnDeviceLock, btnSystemReset
    };

    // SectionBoxWidget 생성
    SectionBoxWidget* card = new SectionBoxWidget(this);
    card->addSection("실시간 이벤트 로그", { textEventLog }, 20);
    card->addDivider();
    card->addSection("기기 상태", { textErrorStatus }, 60);
    card->addDivider();
    card->addSection("제어 메뉴", controlWidgets, 20);

    // 바깥을 감쌀 Frame (진짜 흰색 배경)
    QFrame* outerFrame = new QFrame(this);
    outerFrame->setStyleSheet(R"(
        QFrame {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
    )");
    QHBoxLayout* outerLayout = new QHBoxLayout(outerFrame);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(card);

    // bottomLayout에 최종 추가
    bottomLayout->addWidget(outerFrame);

    // 과거 에러 로그 불러오기 및 에러카드 자동 표시
    emit requestConveyorLogSearch("", QDate(), QDate());
    updateErrorStatus();
}


// 라즈베리 카메라
void ConveyorWindow::updateRPiImage(const QImage& image)
{
    // 영상 QLabel에 출력
    ui->labelCamRPi->setPixmap(QPixmap::fromImage(image).scaled(
        ui->labelCamRPi->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 한화 카메라
void ConveyorWindow::updateHWImage(const QImage& image)
{
    ui->labelCamHW->setPixmap(QPixmap::fromImage(image).scaled(
        ui->labelCamHW->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ConveyorWindow::setupRightPanel() {
    qDebug() << "=== ConveyorWindow 검색 패널 설정 시작 ===";
    QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->widget_6->layout());
    if (!rightLayout) {
        rightLayout = new QVBoxLayout(ui->widget_6);
        ui->widget_6->setLayout(rightLayout);
    }
    // 1. ERROR LOG 라벨 추가
    static QLabel* errorLogLabel = nullptr;
    if (!errorLogLabel) {
        errorLogLabel = new QLabel("에러 로그");
        errorLogLabel->setStyleSheet(R"(
            color: #374151;
            font-weight: bold;
            font-size: 15px;
            margin-top: 8px;
            margin-bottom: 12px;
            margin-left: 2px;
            padding-left: 2px;
            text-align: left;
        )");
    }
    rightLayout->removeWidget(errorLogLabel);
    rightLayout->insertWidget(0, errorLogLabel);
    if (rightLayout->itemAt(1) && rightLayout->itemAt(1)->spacerItem()) {
        rightLayout->removeItem(rightLayout->itemAt(1));
    }
    rightLayout->insertSpacing(1, 16);

    // 2. 검색창(입력창+버튼) 스타일 적용
    ui->lineEdit->setPlaceholderText("검색어 입력(SPD 등)");
    ui->lineEdit->setFixedHeight(36);
    ui->lineEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f3f4f6;
            border: none;
            border-top-left-radius: 12px;
            border-bottom-left-radius: 12px;
            padding-left: 12px;
            font-size: 13px;
            color: #374151;
        }
        QLineEdit:focus {
            border: 1px solid #fb923c;
            background-color: #ffffff;
        }
    )");
    ui->pushButton->setText("검색");
    ui->pushButton->setFixedHeight(36);
    ui->pushButton->setFixedWidth(60);
    ui->pushButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            border: none;
            border-top-right-radius: 12px;
            border-bottom-right-radius: 12px;
            font-size: 13px;
            color: #374151;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
    )");
    disconnect(ui->pushButton, &QPushButton::clicked, 0, 0);
    connect(ui->pushButton, &QPushButton::clicked, this, &ConveyorWindow::onConveyorSearchClicked);
    disconnect(ui->lineEdit, &QLineEdit::returnPressed, this, &ConveyorWindow::onConveyorSearchClicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &ConveyorWindow::onConveyorSearchClicked);

    QWidget* searchContainer = new QWidget();
    QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(ui->lineEdit);
    searchLayout->addWidget(ui->pushButton);
    rightLayout->insertWidget(1, searchContainer);

    // 3. 날짜 필터(QGroupBox) 스타일 적용
    QGroupBox* dateGroup = new QGroupBox();
    QVBoxLayout* dateLayout = new QVBoxLayout(dateGroup);
    QLabel* filterTitle = new QLabel("날짜 필터");
    filterTitle->setStyleSheet("color: #374151; font-weight: bold; font-size: 15px; background: transparent;");
    dateLayout->addWidget(filterTitle);
    dateGroup->setStyleSheet(R"(
        QGroupBox {
            background-color: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 8px;
            margin-top: 8px;
            font-weight: bold;
            color: #374151;
        }
    )");
    QString dateEditStyle = R"(
    QDateEdit {
        background-color: #ffffff;
        border: 1px solid #d1d5db;
        border-radius: 6px;
        padding: 4px 8px;
        font-size: 12px;
        min-width: 80px;
    }
    QDateEdit:focus {
        border-color: #fb923c;
        outline: none;
    }
    QCalendarWidget QWidget {
        alternate-background-color: #f9fafb;
        background-color: white;
    }
    QCalendarWidget QAbstractItemView:enabled {
        background-color: white;
        selection-background-color: #fb923c;
        selection-color: white;
    }
    QCalendarWidget QWidget#qt_calendar_navigationbar {
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
            stop:0 #fb923c, stop:1 #f97316);
        border-radius: 8px;
        margin: 2px;
    }
    QCalendarWidget QToolButton {
        background-color: transparent;
        color: white;
        border: none;
        border-radius: 6px;
        padding: 6px;
        font-weight: bold;
        font-size: 16px;
    }
    QCalendarWidget QToolButton:hover {
        background-color: rgba(255, 255, 255, 0.2);
        border-radius: 6px;
    }
    QCalendarWidget QToolButton:pressed {
        background-color: rgba(255, 255, 255, 0.3);
    }
    QCalendarWidget QSpinBox {
        background-color: white;
        border: 1px solid #fb923c;
        border-radius: 4px;
        color: #374151;
    }
)";
    // 시작일
    QVBoxLayout* startCol = new QVBoxLayout();
    QLabel* startLabel = new QLabel("시작일:");
    startLabel->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
    if (!conveyorStartDateEdit) conveyorStartDateEdit = new QDateEdit(QDate::currentDate());
    conveyorStartDateEdit->setCalendarPopup(true);
    conveyorStartDateEdit->setDisplayFormat("MM-dd");
    conveyorStartDateEdit->setStyleSheet(dateEditStyle);
    conveyorStartDateEdit->setFixedWidth(90);
    startCol->addWidget(startLabel);
    startCol->addWidget(conveyorStartDateEdit);
    // 종료일
    QVBoxLayout* endCol = new QVBoxLayout();
    QLabel* endLabel = new QLabel("종료일:");
    endLabel->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
    if (!conveyorEndDateEdit) conveyorEndDateEdit = new QDateEdit(QDate::currentDate());
    conveyorEndDateEdit->setCalendarPopup(true);
    conveyorEndDateEdit->setDisplayFormat("MM-dd");
    conveyorEndDateEdit->setStyleSheet(dateEditStyle);
    conveyorEndDateEdit->setFixedWidth(90);
    endCol->addWidget(endLabel);
    endCol->addWidget(conveyorEndDateEdit);
    // 적용 버튼
    QPushButton* applyButton = new QPushButton("적용");
    applyButton->setFixedHeight(28);
    applyButton->setFixedWidth(60);
    applyButton->setStyleSheet(R"(
        QPushButton {
            background-color: #fb923c;
            color: white;
            font-size: 12px;
            border: none;
            padding: 6px 12px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #f97316;
        }
    )");
    QHBoxLayout* inputRow = new QHBoxLayout();
    inputRow->addLayout(startCol);
    inputRow->addLayout(endCol);
    inputRow->addWidget(applyButton);
    inputRow->setAlignment(applyButton, Qt::AlignBottom);
    dateLayout->addLayout(inputRow);
    // 전체 초기화 버튼
    QPushButton* resetDateBtn = new QPushButton("전체 초기화 (최신순)");
    resetDateBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f3f4f6;
            color: #374151;
            font-size: 12px;
            border: none;
            padding: 6px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #fb923c;
            color: white;
        }
    )");
    dateLayout->addSpacing(3);
    dateLayout->addWidget(resetDateBtn);
    rightLayout->insertWidget(2, dateGroup);
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";
        QDate start = conveyorStartDateEdit ? conveyorStartDateEdit->date() : QDate();
        QDate end = conveyorEndDateEdit ? conveyorEndDateEdit->date() : QDate();
        emit requestConveyorLogSearch(searchText, start, end);
    });
    connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
        if(conveyorStartDateEdit && conveyorEndDateEdit) {
            conveyorStartDateEdit->setDate(QDate::currentDate());
            conveyorEndDateEdit->setDate(QDate::currentDate());
        }
        if(ui->lineEdit) ui->lineEdit->clear();
        isConveyorDateSearchMode = false;  // 실시간 모드로 전환
        emit requestConveyorLogSearch("", QDate(), QDate());
    });
    // 4. QScrollArea+QVBoxLayout(카드 쌓기) 구조 적용
    if (ui->scrollArea) {
        if (!errorCardContainer) {
            errorCardContainer = new QWidget();
            errorCardLayout = new QVBoxLayout(errorCardContainer);
            errorCardLayout->setSpacing(6);
            errorCardLayout->setContentsMargins(4, 2, 4, 4);
            errorCardLayout->addStretch();
            ui->scrollArea->setWidget(errorCardContainer);
            ui->scrollArea->setWidgetResizable(true);
        }
    }
}

void ConveyorWindow::clearErrorCards() {
    if (!errorCardLayout) return;
    // stretch 제외 모두 삭제
    while (errorCardLayout->count() > 1) {
        QLayoutItem* item = errorCardLayout->takeAt(0);
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
}

void ConveyorWindow::onErrorLogsReceived(const QList<QJsonObject> &logs){

    clearErrorCards();
    for(int i = logs.size() - 1; i >= 0; --i) {
        const QJsonObject &log = logs[i];
        if(log["device_id"].toString() == "conveyor_01") {
            if (log["log_level"].toString() != "error") continue;
            addErrorCardUI(log);
        }
    }
}

void ConveyorWindow::onErrorLogBroadcast(const QJsonObject &errorData){
    QString deviceId = errorData["device_id"].toString();

    if(deviceId.startsWith("conveyor_")) {  // conveyor_01, conveyor_03 모두
        QString logCode = errorData["log_code"].toString();
        QString logLevel = errorData["log_level"].toString();

        qDebug() << "컨베이어 로그 수신 - 코드:" << logCode << "레벨:" << logLevel;

        // 정상 상태 로그 처리
        if(logCode == "INF" || logLevel == "info" || logLevel == "INFO") {
            qDebug() << "컨베이어 정상 상태 감지";
            showConveyorNormal();  // 정상 상태 표시
            // 정상 상태는 에러 리스트에 추가하지 않음
        }
        // 실제 오류 로그만 처리 (error 레벨만)
        else if(logLevel == "error" || logLevel == "ERROR") {
            qDebug() << "컨베이어 오류 상태 감지:" << logCode;
            showConveyorError(logCode);  // 오류 상태 표시
            logError(logCode);
            updateErrorStatus();
            addErrorLog(errorData);  // 오류만 리스트에 추가
        }
        // 기타 로그 (warning, debug 등)는 무시
        else {
            qDebug() << "컨베이어 기타 로그 무시 - 코드:" << logCode << "레벨:" << logLevel;
        }

        qDebug() << "ConveyorWindow - 실시간 컨베이어 로그 처리 완료:" << logCode;
    } else {
        qDebug() << "ConveyorWindow - 컨베이어가 아닌 디바이스 로그 무시:" << deviceId;
    }
}


//  기본 검색 함수 (기존 onSearchClicked 유지)
void ConveyorWindow::onSearchClicked(){
    QString searchText = ui->lineEdit->text().trimmed();
    emit requestFilteredLogs("conveyor_01", searchText);
}


void ConveyorWindow::onSearchResultsReceived(const QList<QJsonObject> &results) {
    qDebug() << "ConveyorWindow 검색 결과 수신:" << results.size() << "개";
    clearErrorCards();

    // 현재 검색어 확인
    QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";

    // 현재 설정된 날짜 필터 확인
    QDate currentStartDate, currentEndDate;
    bool hasDateFilter = false;

    if(conveyorStartDateEdit && conveyorEndDateEdit) {
        currentStartDate = conveyorStartDateEdit->date();
        currentEndDate = conveyorEndDateEdit->date();

        QDate today = QDate::currentDate();
        hasDateFilter = (currentStartDate.isValid() && currentEndDate.isValid() &&
                         (currentStartDate != today || currentEndDate != today));

        qDebug() << "ConveyorWindow 날짜 필터 상태:";
        qDebug() << "  - 시작일:" << currentStartDate.toString("yyyy-MM-dd");
        qDebug() << "  - 종료일:" << currentEndDate.toString("yyyy-MM-dd");
        qDebug() << "  - 필터 활성:" << hasDateFilter;
    }

    int errorCount = 0;

    //  HOME 방식으로 변경: 역순 for loop (최신순)
    for(int i = results.size() - 1; i >= 0; --i) {
        const QJsonObject &log = results[i];

        if(log["device_id"].toString() != "conveyor_01") continue;
        if(log["log_level"].toString() != "error") continue;

        bool shouldInclude = true;

        // 날짜 필터링 적용
        if(hasDateFilter) {
            qint64 timestamp = log["timestamp"].toVariant().toLongLong();
            if(timestamp > 0) {
                QDateTime logDateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
                QDate logDate = logDateTime.date();

                if(logDate < currentStartDate || logDate > currentEndDate) {
                    shouldInclude = false;
                    qDebug() << "ConveyorWindow 날짜 필터로 제외:" << logDate.toString("yyyy-MM-dd");
                }
            }
        }

        // 검색어 필터링 적용
        if(shouldInclude && !searchText.isEmpty()) {
            QString logCode = log["log_code"].toString();
            QString deviceIdForSearch = log["device_id"].toString();
            if(!logCode.contains(searchText, Qt::CaseInsensitive) &&
                !deviceIdForSearch.contains(searchText, Qt::CaseInsensitive)) {
                shouldInclude = false;
            }
        }

        if(shouldInclude) {
            addErrorCardUI(log);
            errorCount++;
        }
    }

    if(errorCount == 0) {
        addNoResultsMessage();
    }

    updateErrorStatus();
    qDebug() << " ConveyorWindow 필터링 완료:" << errorCount << "개 표시 (최신순)";
}

void ConveyorWindow::onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData){
    if(deviceId != "conveyor_01" || !textErrorStatus) {
        return;
    }

    qDebug() << "컨베이어 통계 데이터 수신:" << QJsonDocument(statsData).toJson(QJsonDocument::Compact);

    int currentSpeed = statsData.value("current_speed").toInt();
    int average = statsData.value("average").toInt();
    //double failureRate = statsData.value("failure_rate").toDouble();

    qDebug() << "컨베이어 통계 - 현재속도:" << currentSpeed << "평균속도:" << average;

    //  0 데이터여도 차트 리셋하지 않음 (addSpeedData에서 처리)
    if (deviceChart) {
        deviceChart->addSpeedData(currentSpeed, average);
        qDebug() << "컨베이어 차트 데이터 추가 완료";
    } else {
        qDebug() << "컨베이어 차트가 아직 초기화되지 않음";

        // 차트가 없으면 기존처럼 텍스트 표시
        QString statsText = QString("현재 속도: %1\n평균 속도: %2\n불량률: 계산중...").arg(currentSpeed).arg(average);
        textErrorStatus->setText(statsText);
    }

    //if (failureRateSeries) {
    //    updateFailureRate(failureRate);
    //}

}


// 로그 더블클릭 시 영상 재생
void ConveyorWindow::on_listWidget_itemDoubleClicked(QListWidgetItem* item) {
    static bool isProcessing = false;
    if (isProcessing) return;
    isProcessing = true;

    QString errorLogId = item->data(Qt::UserRole).toString();
    QString logText = item->text();

    // 로그 형식 파싱
    QRegularExpression re(R"(\[(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})\])");
    QRegularExpressionMatch match = re.match(logText);

    QString month, day, hour, minute, second = "00";
    QString deviceId = "conveyor_01";

    if (match.hasMatch()) {
        month = match.captured(1);
        day = match.captured(2);
        hour = match.captured(3);
        minute = match.captured(4);
        second = match.captured(5);
    } else {
        QMessageBox::warning(this, "형식 오류", "로그 형식을 해석할 수 없습니다.\n로그: " + logText);
        isProcessing = false;
        return;
    }

    // 현재 년도 사용
    int currentYear = QDateTime::currentDateTime().date().year();
    QDateTime timestamp = QDateTime::fromString(
        QString("%1%2%3%4%5%6").arg(currentYear).arg(month,2,'0').arg(day,2,'0')
            .arg(hour,2,'0').arg(minute,2,'0').arg(second,2,'0'),
        "yyyyMMddhhmmss");

    qint64 startTime = timestamp.addSecs(-60).toMSecsSinceEpoch();
    qint64 endTime = timestamp.addSecs(+300).toMSecsSinceEpoch();

    VideoClient* client = new VideoClient(this);
    client->queryVideos(deviceId, "", startTime, endTime, 1,
                        [this, deviceId](const QList<VideoInfo>& videos) {
                            //static bool isProcessing = false;
                            isProcessing = false; // 재설정

                            if (videos.isEmpty()) {
                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
                                return;
                            }

                            QString httpUrl = videos.first().http_url;
                            this->downloadAndPlayVideoFromUrl(httpUrl, deviceId);
                        });
}

// 영상 다운로드 및 재생
void ConveyorWindow::downloadAndPlayVideoFromUrl(const QString& httpUrl, const QString& deviceId) {
    qDebug() << "요청 URL:" << httpUrl;

    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkRequest request(httpUrl);
    request.setRawHeader("User-Agent", "Factory Video Client");

    QNetworkReply* reply = manager->get(request);

    QString fileName = httpUrl.split('/').last();
    QString savePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + fileName;

    QFile* file = new QFile(savePath);
    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "파일 오류", "임시 파일을 생성할 수 없습니다.");
        delete file;
        return;
    }

    connect(reply, &QNetworkReply::readyRead, [reply, file]() {
        file->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, [this, reply, file, savePath, deviceId]() {
        file->close();
        delete file;

        bool success = (reply->error() == QNetworkReply::NoError);

        if (success) {
            qDebug() << "영상 저장 성공:" << savePath;
            VideoPlayer* player = new VideoPlayer(savePath, deviceId, this);
            player->setAttribute(Qt::WA_DeleteOnClose);
            // --- 닫힐 때 MQTT 명령 전송 ---
            connect(player, &VideoPlayer::videoPlayerClosed, this, [this]() {
                if (m_client && m_client->state() == QMqttClient::Connected) {
                    m_client->publish(QMqttTopicName("factory/hanwha/cctv/zoom"), QByteArray("-100"));
                    m_client->publish(QMqttTopicName("factory/hanwha/cctv/cmd"), QByteArray("autoFocus"));
                }
            });
            player->show();
        } else {
            qWarning() << "영상 다운로드 실패:" << reply->errorString();
            QMessageBox::warning(this, "다운로드 오류", "영상 다운로드에 실패했습니다.\n" + reply->errorString());
        }

        reply->deleteLater();
    });
}


void ConveyorWindow::onConveyorSearchClicked() {
    qDebug() << " ConveyorWindow 컨베이어 검색 시작!";
    qDebug() << "함수 시작 - 현재 시간:" << QDateTime::currentDateTime().toString();

    //  UI 컴포넌트 존재 확인
    if(!ui->lineEdit) {
        qDebug() << " lineEdit null!";
        QMessageBox::warning(this, "UI 오류", "검색 입력창이 초기화되지 않았습니다.");
        return;
    }

    //if(ui->listWidget) { // listWidget 삭제됨
    //    qDebug() << " listWidget null!";
    //    QMessageBox::warning(this, "UI 오류", "결과 리스트가 초기화되지 않았습니다.");
    //    return;
    //}

    //  검색어 가져오기
    QString searchText = ui->lineEdit->text().trimmed();
    qDebug() << " 컨베이어 검색어:" << searchText;

    //  날짜 위젯 확인 및 기본값 설정
    if(!conveyorStartDateEdit || !conveyorEndDateEdit) {
        qDebug() << " 컨베이어 날짜 위젯이 null입니다!";
        qDebug() << "conveyorStartDateEdit:" << conveyorStartDateEdit;
        qDebug() << "conveyorEndDateEdit:" << conveyorEndDateEdit;
        QMessageBox::warning(this, "UI 오류", "날짜 선택 위젯이 초기화되지 않았습니다.");
        return;
    }

    QDate startDate = conveyorStartDateEdit->date();
    QDate endDate = conveyorEndDateEdit->date();

    if(startDate.isValid() && endDate.isValid()) {
        isConveyorDateSearchMode = true;  // 날짜 검색 모드 활성화
        qDebug() << "컨베이어 날짜 검색 모드 활성화";
    } else {
        isConveyorDateSearchMode = false; // 실시간 모드
        qDebug() << " 컨베이어 실시간 모드 활성화";
    }

    qDebug() << " 컨베이어 검색 조건:";
    qDebug() << "  - 검색어:" << (searchText.isEmpty() ? "(전체)" : searchText);
    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

    //  날짜 유효성 검사
    if(!startDate.isValid() || !endDate.isValid()) {
        qDebug() << " 잘못된 날짜";
        QMessageBox::warning(this, "날짜 오류", "올바른 날짜를 선택해주세요.");
        return;
    }

    if(startDate > endDate) {
        qDebug() << " 시작일이 종료일보다 늦음";
        QMessageBox::warning(this, "날짜 오류", "시작일이 종료일보다 늦을 수 없습니다.");
        return;
    }

    //  날짜 범위 제한 (옵션)
    QDate currentDate = QDate::currentDate();
    if(endDate > currentDate) {
        qDebug() << "️ 종료일이 현재 날짜보다 미래임 - 현재 날짜로 조정";
        endDate = currentDate;
        conveyorEndDateEdit->setDate(endDate);
    }

    //  검색 진행 표시
    //ui->listWidget->clear(); // listWidget 삭제됨
    //ui->listWidget->addItem(" 컨베이어 검색 중... 잠시만 기다려주세요."); // listWidget 삭제됨
    //ui->pushButton->setEnabled(false);  // 중복 검색 방지 // listWidget 삭제됨

    qDebug() << " 컨베이어 통합 검색 요청 - Home으로 시그널 전달";

    //  검색어와 날짜 모두 전달
    emit requestConveyorLogSearch(searchText, startDate, endDate);

    qDebug() << " 컨베이어 검색 시그널 발송 완료";

    //  타임아웃 설정 (30초 후 버튼 재활성화)
    QTimer::singleShot(30000, this, [this]() {
        //if(!ui->pushButton->isEnabled()) { // listWidget 삭제됨
        //    qDebug() << " 컨베이어 검색 타임아웃 - 버튼 재활성화";
        //    ui->pushButton->setEnabled(true);

        //    if(ui->listWidget && ui->listWidget->count() == 1) { // listWidget 삭제됨
        //        QString firstItem = ui->listWidget->item(0)->text(); // listWidget 삭제됨
        //        if(firstItem.contains("검색 중")) { // listWidget 삭제됨
        //            ui->listWidget->clear(); // listWidget 삭제됨
        //            ui->listWidget->addItem(" 검색 시간이 초과되었습니다. 다시 시도해주세요."); // listWidget 삭제됨
        //        }
        //    }
        //}
    });
}

void ConveyorWindow::addErrorCardUI(const QJsonObject& errorData) {
    if (errorData["device_id"].toString() != "conveyor_01") return;
    QWidget* card = new QWidget();
    card->setFixedHeight(84);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setStyleSheet(R"(
        background-color: #F3F4F6;
        border: 1px solid #E5E7EB;
        border-radius: 12px;
    )");
    card->setProperty("errorData", QVariant::fromValue(errorData));

    // 카드 더블클릭 이벤트 필터 설치
    static CardEventFilter* filter = nullptr;
    if (!filter) {
        filter = new CardEventFilter(this);
        connect(filter, &CardEventFilter::cardDoubleClicked, this, &ConveyorWindow::onCardDoubleClicked);
    }
    card->installEventFilter(filter);

    QVBoxLayout* outer = new QVBoxLayout(card);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(6);

    // 상단: 오류 배지 + 시간
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    topRow->setContentsMargins(0, 0, 0, 0);

    QLabel* badge = new QLabel();
    QPixmap errorPixmap(":/ui/icons/images/error.png");
    if (!errorPixmap.isNull()) {
        badge->setPixmap(errorPixmap.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        badge->setStyleSheet("border: none; background: transparent;");
    } else {
        // 아이콘이 로드되지 않으면 텍스트로 대체
        badge->setText("⚠");
        badge->setStyleSheet("color: #ef4444; font-size: 14px; border: none; background: transparent;");
    }

    QHBoxLayout* left = new QHBoxLayout();
    left->addWidget(badge);
    left->setSpacing(4);
    left->setContentsMargins(0, 0, 0, 0);

    // 에러 메시지 라벨 추가
    QString logCode = errorData["log_code"].toString();
    QString messageText = (logCode == "SPD") ? "SPD(모터속도 오류)" : logCode;
    QLabel* errorLabel = new QLabel(messageText);
    errorLabel->setStyleSheet("color: #374151; font-size: 12px; font-weight: 500; border: none;");
    left->addWidget(errorLabel);
    left->addStretch();

    QLabel* timeLabel = new QLabel(
        QDateTime::fromMSecsSinceEpoch(errorData["timestamp"].toVariant().toLongLong()).toString("MM-dd hh:mm")
        );
    timeLabel->setStyleSheet("color: #6b7280; font-size: 10px; border: none;");
    timeLabel->setMaximumWidth(70);
    timeLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topRow->addLayout(left);
    topRow->addStretch();

    // 하단: 사람 아이콘 + 디바이스명 + 시간 (하얀 상자로 감싸기)
    QWidget* whiteContainer = new QWidget();
    whiteContainer->setStyleSheet(R"(
        background-color: #FFF;
        border-radius: 12px;
    )");
    QHBoxLayout* whiteLayout = new QHBoxLayout(whiteContainer);
    whiteLayout->setContentsMargins(12, 10, 12, 10);
    whiteLayout->setSpacing(6);

    // 사람 아이콘
    QLabel* personIcon = new QLabel();
    QPixmap personPixmap(":/ui/icons/images/person.png");
    if (!personPixmap.isNull()) {
        personIcon->setPixmap(personPixmap.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        personIcon->setStyleSheet("border: none; background: transparent;");
    } else {
        // 아이콘이 로드되지 않으면 텍스트로 대체
        personIcon->setText("👤");
        personIcon->setStyleSheet("color: #6b7280; font-size: 14px; border: none; background: transparent;");
    }
    whiteLayout->addWidget(personIcon);

    // 디바이스명 배지
    QLabel* device = new QLabel(errorData["device_id"].toString());
    device->setMinimumHeight(24);
    QString dev = errorData["device_id"].toString();
    QString devStyle = dev.contains("conveyor")
                           ? R"(
            background-color: #E1F5FF;
            color: #56A5FF;
            border: none;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 11px;
            font-weight: 500;
        )"
                           : R"(
            background-color: #FFF4DE;
            color: #FF9138;
            border: none;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 11px;
            font-weight: 500;
        )";
    device->setStyleSheet(devStyle);

    whiteLayout->addWidget(device);
    whiteLayout->addStretch();

    // 시간 아이콘과 텍스트
    QLabel* clockIcon = new QLabel();
    QPixmap clockPixmap(":/ui/icons/images/clock.png");
    if (!clockPixmap.isNull()) {
        clockIcon->setPixmap(clockPixmap.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        clockIcon->setStyleSheet("border: none; background: transparent;");
    } else {
        // 아이콘이 로드되지 않으면 텍스트로 대체
        clockIcon->setText("🕐");
        clockIcon->setStyleSheet("color: #6b7280; font-size: 12px; border: none; background: transparent;");
    }
    whiteLayout->addWidget(clockIcon);

    QLabel* timeText = new QLabel(
        QDateTime::fromMSecsSinceEpoch(errorData["timestamp"].toVariant().toLongLong()).toString("MM-dd hh:mm")
        );
    timeText->setStyleSheet("color: #6b7280; font-size: 10px; border: none;");
    whiteLayout->addWidget(timeText);

    // 조립
    outer->addLayout(topRow);
    outer->addWidget(whiteContainer);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(255, 140, 0, 0));
    shadow->setOffset(0, 0);
    card->setGraphicsEffect(shadow);
    QPropertyAnimation* anim = new QPropertyAnimation(shadow, "color", card);
    anim->setDuration(200);
    anim->setStartValue(QColor(255, 140, 0, 0));
    anim->setEndValue(QColor(255, 140, 0, 64));
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    card->installEventFilter(new CardHoverEffect(card, shadow, anim));

    if (errorCardLayout) {
        errorCardLayout->insertWidget(0, card);
    }
}

void ConveyorWindow::onCardDoubleClicked(QObject* cardWidgetObj) {
    QWidget* cardWidget = qobject_cast<QWidget*>(cardWidgetObj);
    if (!cardWidget) return;
    QJsonObject logData = cardWidget->property("errorData").toJsonObject();
    QString deviceId = logData["device_id"].toString();
    if (deviceId != "conveyor_01") return;
    qint64 timestamp = logData["timestamp"].toVariant().toLongLong();
    qint64 startTime = timestamp - 60 * 1000;
    qint64 endTime = timestamp + 5 * 60 * 1000;

    // --- 여기서 MQTT 명령 전송 ---
    if (m_client && m_client->state() == QMqttClient::Connected) {
        m_client->publish(QMqttTopicName("factory/hanwha/cctv/zoom"), QByteArray("-100"));
        m_client->publish(QMqttTopicName("factory/hanwha/cctv/cmd"), QByteArray("autoFocus"));
    }

    VideoClient* client = new VideoClient(this);
    client->queryVideos(deviceId, "", startTime, endTime, 1,
                        [this, deviceId](const QList<VideoInfo>& videos) {
                            if (videos.isEmpty()) {
                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
                                return;
                            }
                            QString httpUrl = videos.first().http_url;
                            // --- 여기서 MQTT 명령 전송 ---
                            if (m_client && m_client->state() == QMqttClient::Connected) {
                                m_client->publish(QMqttTopicName("factory/hanwha/cctv/zoom"), QByteArray("100"));
                                m_client->publish(QMqttTopicName("factory/hanwha/cctv/cmd"), QByteArray("autoFocus"));
                            }
                            this->downloadAndPlayVideoFromUrl(httpUrl, deviceId);
                        });
}

void ConveyorWindow::loadPastLogs() {
    emit requestErrorLogs("conveyor_01");
}

void ConveyorWindow::addErrorLog(const QJsonObject &errorData) {
    if(errorData["device_id"].toString() != "conveyor_01") return;
    if(errorData["log_level"].toString() != "error") return;
    addErrorCardUI(errorData);
}


void ConveyorWindow::setupErrorCardUI() {
    // 이미 레이아웃이 있으면 건너뜀
    if (!ui->errorMessageContainer->layout()) {
        QVBoxLayout* layout = new QVBoxLayout(ui->errorMessageContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        ui->errorMessageContainer->setLayout(layout);
    }

    errorCard = new ErrorMessageCard(this);
    errorCard->setStyleSheet("background-color: #ffffff; border-radius: 12px;");
    ui->errorMessageContainer->layout()->addWidget(errorCard);
}

//차트
// void ConveyorWindow::setupChartInUI() {
//     qDebug() << "컨베이어 차트 UI 설정 시작";

//     if (!textErrorStatus) {
//         qDebug() << " textErrorStatus가 null";
//         return;
//     }

//     if (!deviceChart) {
//         qDebug() << " deviceChart가 null";
//         return;
//     }

//     QWidget *chartWidget = deviceChart->getChartWidget();
//     if (!chartWidget) {
//         qDebug() << " 차트 위젯이 null";
//         return;
//     }

//     QWidget *parentWidget = textErrorStatus->parentWidget();
//     if (!parentWidget) {
//         qDebug() << " 부모 위젯을 찾을 수 없음";
//         return;
//     }

//     QLayout *parentLayout = parentWidget->layout();
//     if (!parentLayout) {
//         qDebug() << " 부모 레이아웃을 찾을 수 없음";
//         return;
//     }

//     try {
//         textErrorStatus->hide();
//         parentLayout->removeWidget(textErrorStatus);

//         //  새로운 컨테이너 위젯 생성 (반으로 나누기 위해)
//         QWidget *chartContainer = new QWidget();
//         QHBoxLayout *chartLayout = new QHBoxLayout(chartContainer);
//         chartLayout->setContentsMargins(0, 0, 0, 0);
//         chartLayout->setSpacing(5);

//         //  왼쪽: 속도 차트 (50%)
//         chartWidget->setMinimumHeight(220);
//         chartWidget->setMaximumHeight(260);
//         chartLayout->addWidget(chartWidget, 1);  // stretch factor 1

//         //  오른쪽: 불량률 원형 그래프 (50%)
//         createFailureRateChart(chartLayout);

//         // 전체 컨테이너를 부모 레이아웃에 추가
//         parentLayout->addWidget(chartContainer);

//         qDebug() << " 컨베이어 차트 UI 설정 완료 (반반 분할)";
//     } catch (...) {
//         qDebug() << " 차트 UI 설정 중 예외 발생";
//     }
// }

void ConveyorWindow::setupChartInUI() {
    qDebug() << "컨베이어 차트 UI 설정 시작";

    if (!textErrorStatus || !deviceChart) {
        qDebug() << " 필수 요소가 null";
        return;
    }

    QWidget *chartWidget = deviceChart->getChartWidget();
    if (!chartWidget) {
        qDebug() << " 차트 위젯이 null";
        return;
    }

    QWidget *parentWidget = textErrorStatus->parentWidget();
    QLayout *parentLayout = parentWidget->layout();

    if (!parentWidget || !parentLayout) {
        qDebug() << " 부모 위젯/레이아웃을 찾을 수 없음";
        return;
    }

    try {
        textErrorStatus->hide();
        parentLayout->removeWidget(textErrorStatus);

        // 반반 분할 컨테이너 생성
        QWidget *chartContainer = new QWidget();
        QHBoxLayout *chartLayout = new QHBoxLayout(chartContainer);
        chartLayout->setContentsMargins(0, 0, 0, 0);
        chartLayout->setSpacing(5);

        // 왼쪽: 속도 차트 (50%)
        chartWidget->setMinimumHeight(220);
        chartWidget->setMaximumHeight(260);
        chartLayout->addWidget(chartWidget, 1);

        // 오른쪽: 불량률 원형 그래프 (50%)
        createFailureRateChart(chartLayout);

        // 전체 컨테이너를 부모 레이아웃에 추가
        parentLayout->addWidget(chartContainer);

        qDebug() << " 컨베이어 차트 UI 설정 완료";
    } catch (...) {
        qDebug() << " 차트 UI 설정 중 예외 발생";
    }
}

void ConveyorWindow::createFailureRateChart(QHBoxLayout *parentLayout) {
    QWidget* cardContainer = new QWidget();
    cardContainer->setMinimumHeight(260);
    cardContainer->setMaximumHeight(260);
    cardContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    cardContainer->setStyleSheet("background-color: transparent; border: none;");

    QVBoxLayout* cardLayout = new QVBoxLayout(cardContainer);
    cardLayout->setContentsMargins(15, 12, 15, 12);
    cardLayout->setSpacing(8);

    // 헤더
    QWidget* headerWidget = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel("🗑️");
    iconLabel->setStyleSheet(
        "font-size: 14px;"
        "color: #6b7280;"
        "background-color: #f9fafb;"
        "border: none;"
        "border-radius: 4px;"
        "padding: 2px;"
        "min-width: 18px;"
        "min-height: 18px;"
        );
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel* titleLabel = new QLabel("페트병 분리 현황");
    titleLabel->setStyleSheet(
        "font-size: 13px;"
        //"font-weight: 600;"
        "color: #111827;"
        "background: transparent;"
        "border: none;"
        );

    percentDisplayLabel = new QLabel("투명 페트병 100.0%");
    percentDisplayLabel->setStyleSheet(
        "font-size: 11px;"
        "font-weight: 700;"
        "color: #22c55e;"
        "background: transparent;"
        "border: 1px solid #e5e7eb;"
        "border-radius: 12px;"
        "padding: 2px 6px;"
        );

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(percentDisplayLabel);

    // 차트 삭제
    if (failureRateChart) {
        failureRateChart->deleteLater();
        failureRateChart = nullptr;
    }
    if (failureRateChartView) {
        failureRateChartView->deleteLater();
        failureRateChartView = nullptr;
    }
    if (failureRateSeries) {
        delete failureRateSeries;
        failureRateSeries = nullptr;
    }

    // 도넛 차트 생성
    failureRateChart = new QChart();
    failureRateChartView = new QChartView(failureRateChart);
    failureRateSeries = new QPieSeries();

    failureRateSeries->setHoleSize(0.5);
    failureRateSeries->setPieSize(0.85);

    //  12시 방향부터 시작 (Qt Charts 각도 체계)
    failureRateSeries->setPieStartAngle(0);  // 12시 방향은 90도
    failureRateSeries->setPieEndAngle(360); // 90도에서 시계방향으로 360도 회전

    // 초기값: 투명 페트병 100%
    QPieSlice *transparentSlice = failureRateSeries->append("투명 페트병", 100.0);
    transparentSlice->setColor(QColor(34, 197, 94));  // #22c55e 녹색
    transparentSlice->setLabelVisible(false);
    transparentSlice->setBorderWidth(0);
    transparentSlice->setBorderColor(Qt::transparent);
    transparentSlice->setPen(QPen(Qt::NoPen));

    // 차트 설정
    failureRateChart->addSeries(failureRateSeries);
    failureRateChart->setTitle("");
    failureRateChart->legend()->setVisible(false);
    failureRateChart->setMargins(QMargins(5, 2, 5, 2));
    failureRateChart->setBackgroundBrush(QBrush(Qt::white));
    failureRateChart->setPlotAreaBackgroundBrush(QBrush(Qt::white));

    failureRateChartView->setRenderHint(QPainter::Antialiasing);
    failureRateChartView->setMinimumHeight(140);
    failureRateChartView->setMaximumHeight(160);
    failureRateChartView->setFrameStyle(QFrame::NoFrame);
    failureRateChartView->setStyleSheet("background: white; border: none;");

    // ⭐ 범례 - 동그라미 추가
    QWidget* legendWidget = new QWidget();
    legendWidget->setMinimumHeight(25);
    legendWidget->setMaximumHeight(30);
    legendLayout = new QHBoxLayout(legendWidget);
    legendLayout->setAlignment(Qt::AlignCenter);
    legendLayout->setSpacing(20);
    legendLayout->setContentsMargins(5, 4, 5, 4);

    //  투명 페트병 범례 - 초록색 동그라미 추가
    transparentLegendWidget = new QWidget();
    transparentLegendWidget->setStyleSheet("border: none; background: transparent;");
    QHBoxLayout* transparentLayout = new QHBoxLayout(transparentLegendWidget);
    transparentLayout->setContentsMargins(0, 0, 0, 0);
    transparentLayout->setSpacing(4);

    //  초록색 동그라미 복원
    QLabel* transparentCircle = new QLabel();
    transparentCircle->setFixedSize(8, 8);
    transparentCircle->setStyleSheet(
        "background-color: #22c55e;"  // 초록색
        "border-radius: 4px;"  // 원형
        "border: none;"
        );

    transparentLegendLabel = new QLabel("투명 페트병 100.0%");
    transparentLegendLabel->setStyleSheet(
        "font-size: 11px;"
        "font-weight: 500;"
        "color: #374151;"  //  일반 색상 (텍스트는 검정)
        "background: transparent;"
        "border: none;"
        );

    //  동그라미 + 텍스트 함께 추가
    transparentLayout->addWidget(transparentCircle);
    transparentLayout->addWidget(transparentLegendLabel);

    //  색상 페트병 범례 - 주황색 동그라미 추가
    coloredLegendWidget = new QWidget();
    coloredLegendWidget->setStyleSheet("border: none; background: transparent;");
    coloredLegendWidget->setVisible(false);
    QHBoxLayout* coloredLayout = new QHBoxLayout(coloredLegendWidget);
    coloredLayout->setContentsMargins(0, 0, 0, 0);
    coloredLayout->setSpacing(4);

    //  주황색 동그라미 복원
    QLabel* coloredCircle = new QLabel();
    coloredCircle->setFixedSize(8, 8);
    coloredCircle->setStyleSheet(
        "background-color: #f97316;"  // 주황색
        "border-radius: 4px;"  // 원형
        "border: none;"
        );

    coloredLegendLabel = new QLabel("색상 페트병 0.0%");
    coloredLegendLabel->setStyleSheet(
        "font-size: 11px;"
        "font-weight: 500;"
        "color: #374151;"  //  일반 색상 (텍스트는 검정)
        "background: transparent;"
        "border: none;"
        );

    //  동그라미 + 텍스트 함께 추가
    coloredLayout->addWidget(coloredCircle);
    coloredLayout->addWidget(coloredLegendLabel);

    legendLayout->addWidget(transparentLegendWidget);
    legendLayout->addWidget(coloredLegendWidget);

    // 카드에 추가
    cardLayout->addWidget(headerWidget);
    cardLayout->addWidget(failureRateChartView, 1);
    cardLayout->addWidget(legendWidget);
    cardLayout->addStretch(0);

    parentLayout->addWidget(cardContainer, 1);

    qDebug() << " 페트병 분리 현황 도넛 차트 생성 완료 (동그라미 범례 + 12시 방향 시작)";
}

void ConveyorWindow::initializeDeviceChart() {
    qDebug() << "컨베이어 차트 초기화 시작";

    //  디버깅 로그 추가
    if (!textErrorStatus) {
        qDebug() << " 컨베이어 textErrorStatus가 null입니다!";
        qDebug() << "textErrorStatus 주소:" << textErrorStatus;
        return;
    }

    qDebug() << " textErrorStatus 존재 확인됨";

    if (!deviceChart) {
        qDebug() << " deviceChart가 null입니다!";
        return;
    }

    qDebug() << " deviceChart 존재 확인됨";

    qDebug() << "차트 initializeChart() 호출 시작";
    deviceChart->initializeChart();
    qDebug() << "차트 initializeChart() 완료";

    qDebug() << "setupChartInUI() 호출 시작";
    setupChartInUI();
    qDebug() << "setupChartInUI() 완료";

    qDebug() << " 컨베이어 차트 초기화 완료";
}

void ConveyorWindow::onChartRefreshRequested(const QString &deviceName) {
    qDebug() << "컨베이어 차트 새로고침 요청됨:" << deviceName;

    // 통계 데이터 다시 요청
    requestStatisticsData();

    qDebug() << "컨베이어 통계 데이터 재요청 완료";
}

void ConveyorWindow::updateFailureRate(double failureRate) {
    if (!failureRateSeries) return;

    if (failureRate < 0) failureRate = 0.0;
    if (failureRate > 100) failureRate = 100.0;

    double transparentRate = 100.0 - failureRate;  // 투명 페트병 비율

    failureRateSeries->clear();

    //  12시 방향 시작 설정 (매번 확인)
    failureRateSeries->setPieStartAngle(0);  // 12시 방향은 90도
    failureRateSeries->setPieEndAngle(360); // 90도에서 시계방향으로 360도 회전

    // 헤더 퍼센트 업데이트
    if (percentDisplayLabel) {
        percentDisplayLabel->setText(QString("투명 페트병 %1%").arg(transparentRate, 0, 'f', 1));

        QString color = "#22c55e";  // 기본 녹색
        //if (failureRate > 50) color = "#f97316";  // 주황색

        percentDisplayLabel->setStyleSheet(QString(
                                               "font-size: 11px;"
                                               "font-weight: 700;"
                                               "color: %1;"
                                               "background: transparent;"
                                               "border: 1px solid #e5e7eb;"
                                               "border-radius: 12px;"
                                               "padding: 2px 6px;"
                                               ).arg(color));
    }

    QPieSlice *coloredSlice = nullptr;
    QPieSlice *transparentSlice = nullptr;

    if (failureRate == 0.0) {
        // 투명만 표시
        transparentSlice = failureRateSeries->append("투명 페트병", 100.0);
        transparentSlice->setColor(QColor(34, 197, 94));  //  투명 = 녹색
        transparentSlice->setLabelVisible(false);
        transparentSlice->setBorderWidth(0);
        transparentSlice->setBorderColor(Qt::transparent);
        transparentSlice->setPen(QPen(Qt::NoPen));

        if (transparentLegendLabel) transparentLegendLabel->setText("투명 페트병 100.0%");
        if (coloredLegendWidget) coloredLegendWidget->setVisible(false);
        if (transparentLegendWidget) transparentLegendWidget->setVisible(true);

    } else if (failureRate == 100.0) {
        // 색상만 표시
        coloredSlice = failureRateSeries->append("색상 페트병", 100.0);
        coloredSlice->setColor(QColor(249, 115, 22));  //  색상 = 주황색
        coloredSlice->setLabelVisible(false);
        coloredSlice->setBorderWidth(0);
        coloredSlice->setBorderColor(Qt::transparent);
        coloredSlice->setPen(QPen(Qt::NoPen));

        if (coloredLegendLabel) coloredLegendLabel->setText("색상 페트병 100.0%");
        if (coloredLegendWidget) coloredLegendWidget->setVisible(true);
        if (transparentLegendWidget) transparentLegendWidget->setVisible(false);

    } else {
        //  중요: 투명 페트병을 먼저 추가 (12시 방향부터 시계방향으로)
        transparentSlice = failureRateSeries->append("투명 페트병", transparentRate);
        coloredSlice = failureRateSeries->append("색상 페트병", failureRate);

        //  정확한 색상 매핑
        transparentSlice->setColor(QColor(34, 197, 94));   // 투명 = 녹색 #22c55e
        coloredSlice->setColor(QColor(249, 115, 22));      // 색상 = 주황색 #f97316

        transparentSlice->setLabelVisible(false);
        coloredSlice->setLabelVisible(false);

        // 경계선 제거
        transparentSlice->setBorderWidth(0);
        transparentSlice->setBorderColor(Qt::transparent);
        transparentSlice->setPen(QPen(Qt::NoPen));

        coloredSlice->setBorderWidth(0);
        coloredSlice->setBorderColor(Qt::transparent);
        coloredSlice->setPen(QPen(Qt::NoPen));

        // 범례 업데이트
        if (percentDisplayLabel) {
            percentDisplayLabel->setText(QString("투명 페트병 %1%").arg(transparentRate, 0, 'f', 1));
            percentDisplayLabel->setStyleSheet(
                "font-size: 11px;"
                "font-weight: 700;"
                "color: #22c55e;"  // 항상 초록색
                "background: transparent;"
                "border: none;"
                );
        }

        //  범례 업데이트
        if (transparentLegendLabel) {
            transparentLegendLabel->setText(QString("투명 페트병 %1%").arg(transparentRate, 0, 'f', 1));
            transparentLegendLabel->setStyleSheet(
                "font-size: 11px;"
                "font-weight: 500;"
                "color: #374151;"
                "background: transparent;"
                "border: none;"
                );
        }
        if (coloredLegendLabel) {
            coloredLegendLabel->setText(QString("색상 페트병 %1%").arg(failureRate, 0, 'f', 1));
            coloredLegendLabel->setStyleSheet(
                "font-size: 11px;"
                "font-weight: 500;"
                "color: #374151;"
                "background: transparent;"
                "border: none;"
                );
        }

        // 범례 표시/숨김 로직
        bool showBoth = (failureRate > 0.0 && transparentRate > 0.0);

        if (coloredLegendWidget) {
            coloredLegendWidget->setVisible(showBoth || failureRate == 100.0);
        }

        if (transparentLegendWidget) {
            transparentLegendWidget->setVisible(showBoth || transparentRate == 100.0);
        }

        //if (transparentLegendLabel) transparentLegendLabel->setText(QString("투명 페트병 %1%").arg(transparentRate, 0, 'f', 1));
        //if (coloredLegendLabel) coloredLegendLabel->setText(QString("색상 페트병 %1%").arg(failureRate, 0, 'f', 1));
        //if (coloredLegendWidget) coloredLegendWidget->setVisible(true);
        //if (transparentLegendWidget) transparentLegendWidget->setVisible(true);
    }

    qDebug() << " 페트병 분리 현황 업데이트 - 12시부터 시계방향: 투명(녹색)" << transparentRate << "% → 색상(주황)" << failureRate << "%";
}
void ConveyorWindow::addNoResultsMessage() {
    if (!errorCardLayout) return;

    QWidget* noResultCard = new QWidget();
    noResultCard->setFixedHeight(100);
    noResultCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    noResultCard->setStyleSheet(R"(
        background-color: #f8f9fa;
        border: 2px dashed #dee2e6;
        border-radius: 12px;
    )");

    QVBoxLayout* layout = new QVBoxLayout(noResultCard);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(5);

    // 아이콘
    QLabel* iconLabel = new QLabel("🔍");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 24px; color: #6c757d; border: none;");

    // 메시지
    QLabel* messageLabel = new QLabel("검색 결과가 없습니다");
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #6c757d; border: none;");

    // 서브 메시지
    QLabel* subMessageLabel = new QLabel("다른 검색 조건을 시도해보세요");
    subMessageLabel->setAlignment(Qt::AlignCenter);
    subMessageLabel->setStyleSheet("font-size: 12px; color: #868e96; border: none;");

    layout->addWidget(iconLabel);
    layout->addWidget(messageLabel);
    layout->addWidget(subMessageLabel);

    // 카드를 레이아웃에 추가 (stretch 위에)
    errorCardLayout->insertWidget(0, noResultCard);

    qDebug() << "📝 '검색 결과 없음' 메시지 카드 추가됨";
}




