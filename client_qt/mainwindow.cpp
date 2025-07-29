#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QFile>
#include "videoplayer.h"
#include "video_mqtt.h"
#include "video_client_functions.hpp"
#include "cardevent.h"
//#include "ui_mainwindow.h"

#include <QMouseEvent>
#include "cardhovereffect.h"
#include "error_message_card.h"
#include <QKeyEvent>

#include "sectionboxwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(nullptr)
    , subscription(nullptr)
    , DeviceLockActive(false) //초기는 정상!
    , startDateEdit(nullptr)
    , endDateEdit(nullptr)
    , btnDateRangeSearch(nullptr)
    , statisticsTimer(nullptr)
    , errorCard(nullptr) // 추가
{

    ui->setupUi(this);
    setWindowTitle("Feeder Control");

    ui->labelCamRPi->setStyleSheet("background-color: black; border-radius: 12px;");
    ui->labelCamHW->setStyleSheet("background-color: black; border-radius: 12px;");

    setupErrorCardUI(); // conveyor와 동일하게 ErrorMessageCard UI 추가
    showFeederNormal();
    setupLogWidgets();
    setupControlButtons();
    setupHomeButton();
    setupRightPanel();
    setupMqttClient();
    connectToMqttBroker();

    ui->menubar->hide();
    ui->statusbar->hide();

    this->setStyleSheet("background-color: #FBFBFB;");

    // 1. ✅ QMainWindow 전체 배경 흰색
    // setStyleSheet("QMainWindow { background-color: white; }");

    // 로그 더블클릭 이벤트 연결
    //connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::on_listWidget_itemDoubleClicked);

    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    rpiStreamer = new Streamer("rtsp://192.168.0.76:8554/process1", this);

    // 한화 카메라 스트리머 객체 생성
    hwStreamer = new Streamer("rtsp://192.168.0.18:8553/stream_pno", this);

    // signal-slot
    connect(rpiStreamer, &Streamer::newFrame, this, &MainWindow::updateRPiImage);
    rpiStreamer->start();

    // 한화 signal-slot 연결
    connect(hwStreamer, &Streamer::newFrame, this, &MainWindow::updateHWImage);
    hwStreamer->start();

    statisticsTimer = new QTimer(this);
    connect(statisticsTimer, &QTimer::timeout, this, &MainWindow::requestStatisticsData);

    //차트

    deviceChart = new DeviceChart("피더", this);
    connect(deviceChart, &DeviceChart::refreshRequested, this, &MainWindow::onChartRefreshRequested);

    // deviceChart = nullptr;
    QTimer::singleShot(100, this, [this]() {
        initializeDeviceChart();
    });


}

MainWindow::~MainWindow()
{
    rpiStreamer->stop();
    rpiStreamer->wait();

    hwStreamer->stop();
    hwStreamer->wait();

    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    this->showFullScreen();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        this->showNormal();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::setupMqttClient(){ //mqtt 클라이언트 초기 설정 MQTT 클라이언트 설정 (주소, 포트, 시그널 연결 등)
    m_client = new QMqttClient(this);
    reconnectTimer = new QTimer(this);
    m_client->setHostname(mqttBroker); //브로커 서버에 연결 공용 mqtt 서버
    m_client->setPort(mqttPort);
    m_client->setClientId("VisionCraft_Feeder" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    connect(m_client, &QMqttClient::connected, this, &MainWindow::onMqttConnected); // QMqttClient가 연결이 되었다면 mainwindow에 있는 저 함수중에 onMQTTCONNECTED를 실행
    connect(m_client, &QMqttClient::disconnected, this, &MainWindow::onMqttDisConnected);
    //connect(m_client, &QMqttClient::messageReceived, this, &MainWindow::onMqttMessageReceived);
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::connectToMqttBroker);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
}

void MainWindow::connectToMqttBroker(){ //브로커 연결  실제 연결 시도만!

    if(m_client->state() == QMqttClient::Disconnected){
        m_client->connectToHost();
    }

}

void MainWindow::onMqttConnected(){
    qDebug() << "MQTT Connected - Feeder Control";
    qDebug() << "🔵 MQTT Connected - Feeder Control";
    qDebug() << "🔵 클라이언트 ID:" << m_client->clientId();
    qDebug() << "🔵 연결 상태:" << m_client->state();
    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived,
                this, &MainWindow::onMqttMessageReceived);
    }

    // auto statsSubscription = m_client->subscribe(QString("factory/feeder_01/msg/statistics"));
    // if(statsSubscription){
    //     connect(statsSubscription, &QMqttSubscription::messageReceived,
    //             this, &MainWindow::onMqttMessageReceived);
    //     qDebug() << "MainWindow - feeder_01 통계 토픽 구독됨";
    // }

    // if(statisticsTimer && !statisticsTimer->isActive()) {
    //     statisticsTimer->start(60000);  // 3초마다 요청
    // }
    auto statsSubscription = m_client->subscribe(QString("factory/feeder_01/msg/statistics"));
    if(statsSubscription){
        connect(statsSubscription, &QMqttSubscription::messageReceived,
                this, &MainWindow::onMqttMessageReceived);
        qDebug() << "✅ MainWindow - feeder_01 통계 토픽 구독됨";

        // 구독 상태 확인
        connect(statsSubscription, &QMqttSubscription::stateChanged, [](QMqttSubscription::SubscriptionState state) {
            if (state == QMqttSubscription::Subscribed) {
                qDebug() << "✅ feeder_01 통계 토픽 구독 성공!";
            } else if (state == QMqttSubscription::Error) {
                qDebug() << "❌ feeder_01 통계 토픽 구독 실패!";
            }
        });
    } else {
        qDebug() << "❌ statsSubscription 생성 실패!";
    }

    //if(statisticsTimer && !statisticsTimer->isActive()) {
    //    statisticsTimer->start(60000);
    //    qDebug() << "🔵 통계 타이머 시작됨 (60초마다)";
    //}


    reconnectTimer->stop(); //연결이 성공하면 재연결 타이며 멈추기!

}

void MainWindow::onMqttDisConnected(){
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(3000);
    }

    if(statisticsTimer && statisticsTimer->isActive()) {
        statisticsTimer->stop();
    }
    subscription=NULL; //초기화
}

void MainWindow::onMqttMessageReceived(const QMqttMessage &message){  //매개변수 수정
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  //토픽 정보도 가져올 수 있음
    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가

    if(isFeederDateSearchMode && (topicStr.contains("/log/error") || topicStr.contains("/log/info"))) {
        qDebug() << "🚫 [피더] 날짜 검색 모드이므로 실시간 로그 무시:" << topicStr;
        return;  // 실시간 로그 무시!
    }

    if(topicStr == "factory/feeder_01/msg/statistics") {
        qDebug() << "🎯 [SUCCESS] 피더 통계 메시지 감지됨!";
        qDebug() << "🎯 메시지 내용:" << messageStr;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(messageStr.toUtf8(), &parseError);

        if(parseError.error != QJsonParseError::NoError) {
            qDebug() << "❌ JSON 파싱 오류:" << parseError.errorString();
            return;
        }

        QJsonObject data = doc.object();
        qDebug() << "🎯 파싱된 데이터:" << data;

        onDeviceStatsReceived("feeder_01", data);
        qDebug() << "🎯 onDeviceStatsReceived 호출 완료";
        return;
    }

    if(topicStr.contains("factory/feeder_01/log/info")){
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1];

        if(deviceId == "feeder_01"){
            showFeederNormal(); // 에러 상태 초기화
            logMessage("피더 정상 동작");
        }
        return;
    }

    if(topicStr == "factory/feeder_01/msg/statistics") {
        qDebug() << "[DEBUG] 피더 통계 메시지 감지됨!";
        qDebug() << "  - 메시지 내용:" << messageStr;

        QJsonDocument doc = QJsonDocument::fromJson(messageStr.toUtf8());
        QJsonObject data = doc.object();
        onDeviceStatsReceived("feeder_01", data); //02로 되어있었음
        return;
    }

    if(topicStr == "feeder_02/status"){
        if(messageStr == "on"){
            logMessage("피더가 시작되었습니다.");
            //logError("피더가 시작되었습니다.");
            showFeederNormal();
            //showFeederError("피더가 시작되었습니다.");
            updateErrorStatus();
            emit deviceStatusChanged("feeder_02", "on");
        } else if(messageStr == "off"){
            logMessage("피더가 정지되었습니다.");
            showFeederNormal();
            emit deviceStatusChanged("feeder_02", "off");
        }
        // 나머지 명령은 무시
    } else if(topicStr == "feeder_01/status"){
        if(messageStr != "on" && messageStr != "off"){
            // reverse, speed 등 기타 명령 처리 (필요시 기존 코드 복사)
            if(messageStr == "reverse"){
                logError("피더가 반대로 돌았습니다.");
                showFeederError("피더가 반대로 돌았습니다.");
                updateErrorStatus();
            } else if(messageStr.startsWith("SPEED_") || messageStr.startsWith("MOTOR_")){
                logError("피더 오류 감지: " + messageStr);
            }
        }
    }

}


void MainWindow::onMqttError(QMqttClient::ClientError error){
    logMessage("MQTT 에러 발생");

}

void MainWindow::publishControlMessage(const QString &command){
    // Home에서 MQTT 처리 - 시그널로 전달
    emit requestMqttPublish(mqttControllTopic, command);
    logMessage("제어 명령 요청: " + command);
}


void MainWindow::logMessage(const QString &message){
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog != NULL){
        textEventLog->append("[" + timer +  "]" + message);
    }
}

//메시지 출력
void MainWindow::showFeederError(QString feederErrorType){
    qDebug() << "오류 상태 함수 호출됨";
    QString datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    if (errorCard) {
        errorCard->setErrorState(feederErrorType, datetime, "피더 구역", "FEEDER_CAMERA1");
    }
}

void MainWindow::showFeederNormal(){
    qDebug() << "정상 상태 함수 호출됨";
    if (errorCard) {
        errorCard->setNormalState();
    }
}


void MainWindow::initializeUI(){

}

void MainWindow::setupControlButtons() {
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // === 피더 시작 버튼 (btnFeederOn) ===
    btnFeederOn = new QPushButton("피더 시작");
    btnFeederOn->setFixedHeight(32); // h-8과 동일
    btnFeederOn->setStyleSheet(R"(
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
    mainLayout->addWidget(btnFeederOn);
    connect(btnFeederOn, &QPushButton::clicked, this, &MainWindow::onFeederOnClicked);

    // === 피더 정지 버튼 (btnFeederOff) ===
    btnFeederOff = new QPushButton("피더 정지");
    btnFeederOff->setFixedHeight(32);
    btnFeederOff->setStyleSheet(R"(
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
    mainLayout->addWidget(btnFeederOff);
    connect(btnFeederOff, &QPushButton::clicked, this, &MainWindow::onFeederOffClicked);

    // === 기기 잠금 버튼 (btnDeviceLock) ===
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
    mainLayout->addWidget(btnDeviceLock);
    connect(btnDeviceLock, &QPushButton::clicked, this, &MainWindow::onDeviceLock);

    // === 시스템 리셋 버튼 (btnSystemReset) ===
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
    mainLayout->addWidget(btnSystemReset);
    connect(btnSystemReset, &QPushButton::clicked, this, &MainWindow::onSystemReset);

    // 여백 추가
    mainLayout->addStretch();
}


void MainWindow::onFeederOnClicked(){
    qDebug()<<"피더 시작 버튼 클릭됨";
    publishControlMessage("on");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "feeder_02";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    //emit requestMqttPublish("factory/msg/status", "on");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

    btnFeederOn->setStyleSheet(R"(
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

}//피더 정지안됨

void MainWindow::onFeederOffClicked(){
    qDebug()<<"피더 정지 버튼 클릭됨";
    publishControlMessage("off");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "feeder_02";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    emit requestMqttPublish("factory/msg/status", "off");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

    btnFeederOn->setStyleSheet(R"(
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

void MainWindow::onDeviceLock(){
    if(!DeviceLockActive){
        DeviceLockActive=true;

        btnFeederOn->setEnabled(false);
        btnFeederOff->setEnabled(false);
        //btnFeederReverse->setEnabled(false);
        btnDeviceLock->setText("기기 잠금");
        //speedSlider->setEnabled(false);

        qDebug()<<"기기 잠금 버튼 클릭됨";
        //publishControlMessage("off");//기기 진행
        logMessage("기기 잠금 명령 전송!");
    }else {
        // 잠금 해제 - 시스템 리셋 호출
        onSystemReset();
    }
}

void MainWindow::onSystemReset(){
    DeviceLockActive= false;
    btnFeederOn->setEnabled(true);
    btnFeederOff->setEnabled(true);
    //btnFeederReverse->setEnabled(true);
    //speedSlider->setEnabled(true);
    btnDeviceLock->setText("기기 잠금");
    btnDeviceLock->setStyleSheet("");
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


    qDebug()<<"피더 시스템 리셋";
    //publishControlMessage("off"); //기기 진행
    btnFeederOn->setStyleSheet(defaultButtonStyle);
    btnFeederOff->setStyleSheet(defaultButtonStyle);
    btnDeviceLock->setStyleSheet(defaultButtonStyle);
    btnSystemReset->setStyleSheet(defaultButtonStyle);
    logMessage("피더 시스템 리셋 완료!");
}


void MainWindow::setupHomeButton(){

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
    connect(btnHome, &QPushButton::clicked, this, &MainWindow::gobackhome);

    // 제목 섹션 (아이콘 옆)
    QWidget* titleWidget = new QWidget();
    QVBoxLayout* titleLayout = new QVBoxLayout(titleWidget);
    titleLayout->setSpacing(2);
    titleLayout->setContentsMargins(10, 0, 0, 0);

    // 메인 제목
    QLabel* mainTitle = new QLabel("Feeder Control Dashboard");
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

void MainWindow::gobackhome(){
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

// void MainWindow::requestStatisticsData() {
//     if(m_client && m_client->state() == QMqttClient::Connected) {
//         QJsonObject request;
//         request["device_id"] = "feeder_01";

//         // QJsonObject timeRange;
//         // QDateTime now = QDateTime::currentDateTime();
//         // QDateTime oneMinuteAgo = now.addSecs(-);  // 1초
//         // timeRange["start"] = oneMinuteAgo.toMSecsSinceEpoch();
//         // timeRange["end"] = now.toMSecsSinceEpoch();
//         // request["time_range"] = timeRange;

//         QJsonDocument doc(request);

//         m_client->publish(QString("factory/statistics"), doc.toJson(QJsonDocument::Compact));
//         qDebug() << "MainWindow - 피더 통계 요청 전송";
//     }
// }

//실시간 에러 로그 + 통계
void MainWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 피더 오류 " + errorType);
    }
}

void MainWindow::setupLogWidgets() {
    QHBoxLayout *bottomLayout = qobject_cast<QHBoxLayout*>(ui->bottomSectionWidget->layout());
    if (!bottomLayout) return;

    // 기존 위젯 제거
    delete ui->textLog;
    delete ui->groupControl;
    ui->textLog = nullptr;
    ui->groupControl = nullptr;

    // === 로그 영역 ===
    textEventLog = new QTextEdit(this);
    textEventLog->setMinimumHeight(240);

    // === 기기 상태 ===
    textErrorStatus = new QTextEdit(this);
    textErrorStatus->setReadOnly(true);
    textErrorStatus->setMinimumHeight(300);
    textErrorStatus->setStyleSheet("border: none; background-color: transparent;");
    textErrorStatus->setText("현재 속도: 로딩중...\n평균 속도: 로딩중...");

    // === 제어 버튼 구성 ===
    setupControlButtons();  // 버튼 생성 + 스타일 적용

    QList<QWidget*> controlWidgets = {
        btnFeederOn, btnFeederOff, btnDeviceLock, btnSystemReset
    };

    // === SectionBoxWidget으로 묶기 ===
    SectionBoxWidget* card = new SectionBoxWidget(this);
    card->addSection("실시간 이벤트 로그", { textEventLog }, 20);
    card->addDivider();
    card->addSection("기기 상태", { textErrorStatus }, 60);
    card->addDivider();
    card->addSection("제어 메뉴", controlWidgets, 20);

    // === 외부 Frame 감싸기 ===
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

    // 최종 추가
    bottomLayout->addWidget(outerFrame);

    updateErrorStatus();
}



// 라즈베리 카메라
void MainWindow::updateRPiImage(const QImage& image)
{
    // 영상 QLabel에 출력
    ui->labelCamRPi->setPixmap(QPixmap::fromImage(image).scaled(
        ui->labelCamRPi->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 한화 카메라
void MainWindow::updateHWImage(const QImage& image)
{
    ui->labelCamHW->setPixmap(QPixmap::fromImage(image).scaled(
        ui->labelCamHW->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}


void MainWindow::setupRightPanel() {
    qDebug() << "=== MainWindow setupRightPanel 시작 ===";

    // 1. ERROR LOG 라벨 추가 (그대로 유지)
    static QLabel* errorLogLabel = nullptr;
    QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->widget_6->layout());
    if (!rightLayout) {
        rightLayout = new QVBoxLayout(ui->widget_6);
        ui->widget_6->setLayout(rightLayout);
    }
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

    // 2. 검색창(입력창+버튼) 스타일 적용 -수정됨
    if (!ui->lineEdit) ui->lineEdit = new QLineEdit();
    if (!ui->pushButton) ui->pushButton = new QPushButton();
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

    // 중요: 기존 연결 해제 후 재연결
    disconnect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);

    // 엔터키 이벤트 연결
    disconnect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchClicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchClicked);

    QWidget* searchContainer = new QWidget();
    QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(ui->lineEdit);
    searchLayout->addWidget(ui->pushButton);
    rightLayout->insertWidget(1, searchContainer);

    // 3. 날짜 필터 위젯 설정
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
    if (!startDateEdit) startDateEdit = new QDateEdit(QDate::currentDate());
    startDateEdit->setStyleSheet(dateEditStyle);
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("MM-dd");
    startCol->addWidget(startLabel);
    startCol->addWidget(startDateEdit);

    // 종료일
    QVBoxLayout* endCol = new QVBoxLayout();
    QLabel* endLabel = new QLabel("종료일:");
    endLabel->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
    if (!endDateEdit) endDateEdit = new QDateEdit(QDate::currentDate());
    endDateEdit->setStyleSheet(dateEditStyle);
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat("MM-dd");
    endCol->addWidget(endLabel);
    endCol->addWidget(endDateEdit);

    // 적용 버튼
    QPushButton* applyButton = new QPushButton("적용");
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

    // 날짜 필터 버튼 연결
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";
        QDate start = startDateEdit ? startDateEdit->date() : QDate();
        QDate end = endDateEdit ? endDateEdit->date() : QDate();

        qDebug() << "🔍 피더 날짜 검색 적용 버튼 클릭됨";
        qDebug() << "  - 검색어:" << searchText;
        qDebug() << "  - 시작일:" << start.toString("MM-dd");
        qDebug() << "  - 종료일:" << end.toString("MM-dd");

        emit requestFeederLogSearch(searchText, start, end);
    });

    connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
        if(startDateEdit && endDateEdit) {
            startDateEdit->setDate(QDate::currentDate());
            endDateEdit->setDate(QDate::currentDate());
        }
        if(ui->lineEdit) ui->lineEdit->clear();
        isFeederDateSearchMode = false;

        qDebug() << "🔄 피더 날짜 필터 초기화됨";
        emit requestFeederLogSearch("", QDate(), QDate());
    });

    // 4. 스크롤 영역 설정
    if (!errorCardLayout) {
        if (ui->scrollArea) {
            QWidget* errorCardContent = new QWidget();
            errorCardLayout = new QVBoxLayout(errorCardContent);
            errorCardLayout->setSpacing(6);
            errorCardLayout->setContentsMargins(4, 2, 4, 4);
            errorCardLayout->addStretch();
            ui->scrollArea->setWidget(errorCardContent);
            ui->scrollArea->setWidgetResizable(true);
        }
    }

    qDebug() << "✅ MainWindow setupRightPanel 완료";
}

void MainWindow::addErrorLog(const QJsonObject &errorData){
    //if(!ui->listWidget) return;

    QString currentTime = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    QString logText = QString("[%1] %2")
                          .arg(currentTime)
                          .arg(errorData["log_code"].toString());
}

void MainWindow::loadPastLogs(){
    // 부모에게 시그널로 과거 로그 요청
    qDebug() << "MainWindow - 과거 로그 요청";
    emit requestErrorLogs("feeder_01");
}

// 부모로부터 로그 응답 받는 슬롯
void MainWindow::onErrorLogsReceived(const QList<QJsonObject> &logs){
    if(!errorCardLayout) return;
    for(int i = logs.size() - 1; i >= 0; --i) {
        const QJsonObject &log = logs[i];
        if(log["device_id"].toString() == "feeder_01") {
            if (log["log_level"].toString() != "error") continue;
            addErrorCardUI(log);
        }
    }

    if(textErrorStatus) {
        QString initialText = "현재 속도: 0\n";
        initialText += "평균 속도: 0\n";
        textErrorStatus->setText(initialText);
    }
}

void MainWindow::onErrorLogBroadcast(const QJsonObject &errorData) {
    QString deviceId = errorData["device_id"].toString();

    // 피더 로그가 아니면 무시
    if(!deviceId.startsWith("feeder_")) {
        return;
    }

    // ✅ 날짜 필터가 설정되어 있으면 실시간 로그도 필터링
    if(startDateEdit && endDateEdit) {  // ✅ 피더용 변수명으로 수정
        QDate currentStartDate = startDateEdit->date();
        QDate currentEndDate = endDateEdit->date();
        QDate today = QDate::currentDate();

        bool hasDateFilter = (currentStartDate.isValid() && currentEndDate.isValid() &&
                              (currentStartDate != today || currentEndDate != today));

        if(hasDateFilter) {
            qDebug() << "🚫 MainWindow 피더 날짜 필터 활성화로 실시간 로그 차단";
            return;
        }
    }

    // 기존 실시간 로그 처리 로직 유지
    QString logCode = errorData["log_code"].toString();
    QString logLevel = errorData["log_level"].toString();

    qDebug() << "피더 로그 수신 - 코드:" << logCode << "레벨:" << logLevel;

    if(logCode == "INF" || logLevel == "info" || logLevel == "INFO") {
        showFeederNormal();   // ✅ ConveyorWindow와 유사한 패턴
    } else if(logLevel == "error" || logLevel == "ERROR") {
        showFeederError(logCode);  // ✅ ConveyorWindow와 유사한 패턴
        logError(logCode);
        updateErrorStatus();
        addErrorLog(errorData);
    }
}

void MainWindow::onSearchClicked() {
    qDebug() << "🔍 MainWindow 피더 검색 시작!";
    qDebug() << "함수 시작 - 현재 시간:" << QDateTime::currentDateTime().toString();

    // UI 컴포넌트 존재 확인
    if(!ui->lineEdit) {
        qDebug() << "❌ lineEdit null!";
        QMessageBox::warning(this, "UI 오류", "검색 입력창이 초기화되지 않았습니다.");
        return;
    }

    // 검색어 가져오기
    QString searchText = ui->lineEdit->text().trimmed();
    qDebug() << "🔍 피더 검색어:" << searchText;

    // 날짜 위젯 확인
    if(!startDateEdit || !endDateEdit) {
        qDebug() << "❌ 피더 날짜 위젯이 null입니다!";
        qDebug() << "startDateEdit:" << startDateEdit;
        qDebug() << "endDateEdit:" << endDateEdit;
        QMessageBox::warning(this, "UI 오류", "날짜 선택 위젯이 초기화되지 않았습니다.");
        return;
    }

    QDate startDate = startDateEdit->date();
    QDate endDate = endDateEdit->date();

    if(startDate.isValid() && endDate.isValid()) {
        isFeederDateSearchMode = true;  // 날짜 검색 모드 활성화
        qDebug() << "📅 피더 날짜 검색 모드 활성화";
    } else {
        isFeederDateSearchMode = false; // 실시간 모드
        qDebug() << "📡 피더 실시간 모드 활성화";
    }

    qDebug() << "🔍 피더 검색 조건:";
    qDebug() << "  - 검색어:" << (searchText.isEmpty() ? "(전체)" : searchText);
    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

    // 날짜 유효성 검사
    if(!startDate.isValid() || !endDate.isValid()) {
        qDebug() << "❌ 잘못된 날짜";
        QMessageBox::warning(this, "날짜 오류", "올바른 날짜를 선택해주세요.");
        return;
    }

    if(startDate > endDate) {
        qDebug() << "❌ 시작일이 종료일보다 늦음";
        QMessageBox::warning(this, "날짜 오류", "시작일이 종료일보다 늦을 수 없습니다.");
        return;
    }

    // 날짜 범위 제한
    QDate currentDate = QDate::currentDate();
    if(endDate > currentDate) {
        qDebug() << "⚠️ 종료일이 현재 날짜보다 미래임 - 현재 날짜로 조정";
        endDate = currentDate;
        endDateEdit->setDate(endDate);
    }

    qDebug() << "📡 피더 통합 검색 요청 - Home으로 시그널 전달";

    // 검색어와 날짜 모두 전달
    emit requestFeederLogSearch(searchText, startDate, endDate);

    qDebug() << "✅ 피더 검색 시그널 발송 완료";

    // 버튼 비활성화 (중복 검색 방지)
    if(ui->pushButton) {
        ui->pushButton->setEnabled(false);
        //ui->pushButton->setText("검색 중...");
    }

    // 타임아웃 설정 (30초 후 버튼 재활성화)
    QTimer::singleShot(30000, this, [this]() {
        if(ui->pushButton && !ui->pushButton->isEnabled()) {
            qDebug() << "⏰ 검색 타임아웃 - 버튼 재활성화";
            ui->pushButton->setEnabled(true);
            ui->pushButton->setText("검색");
        }
    });
}


void MainWindow::onSearchResultsReceived(const QList<QJsonObject> &results) {
    qDebug() << "🔧 MainWindow 검색 결과 수신:" << results.size() << "개";

    // 기존 카드들 클리어
    if (errorCardLayout) {
        while (errorCardLayout->count() > 1) {
            QLayoutItem* item = errorCardLayout->takeAt(0);
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
    }

    // 현재 검색어 확인
    QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";

    // 현재 설정된 날짜 필터 확인 (피더용 변수 사용)
    QDate currentStartDate, currentEndDate;
    bool hasDateFilter = false;

    // 피더용 날짜 위젯 사용 (startDateEdit, endDateEdit)
    if(startDateEdit && endDateEdit) {
        currentStartDate = startDateEdit->date();
        currentEndDate = endDateEdit->date();

        QDate today = QDate::currentDate();
        hasDateFilter = (currentStartDate.isValid() && currentEndDate.isValid() &&
                         (currentStartDate != today || currentEndDate != today));

        qDebug() << "📅 MainWindow 피더 날짜 필터 상태:";
        qDebug() << "  - 시작일:" << currentStartDate.toString("yyyy-MM-dd");
        qDebug() << "  - 종료일:" << currentEndDate.toString("yyyy-MM-dd");
        qDebug() << "  - 필터 활성:" << hasDateFilter;
    }

    int errorCount = 0;

    // HOME 방식으로 변경: 역순 for loop (최신순)
    for(int i = results.size() - 1; i >= 0; --i) {
        const QJsonObject &log = results[i];

        // 피더 로그만 처리
        QString deviceId = log["device_id"].toString();
        if(!deviceId.startsWith("feeder_")) continue;
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
                    qDebug() << "🚫 MainWindow 피더 날짜 필터로 제외:" << logDate.toString("yyyy-MM-dd");
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
    qDebug() << "✅ MainWindow 피더 필터링 완료:" << errorCount << "개 표시 (최신순)";
}


void MainWindow::updateErrorStatus(){

}

void MainWindow::onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData) {
    if(deviceId != "feeder_01") {
        return;
    }

    if(!textErrorStatus) {
        qDebug() << "textErrorStatus가 null입니다";
        return;
    }

    int currentSpeed = statsData.value("current_speed").toInt();
    int average = statsData.value("average").toInt();

    qDebug() << "피더 통계 데이터 수신 - 현재:" << currentSpeed << "평균:" << average;

    // 0 데이터든 아니든 무조건 차트에 추가 (ConveyorWindow와 동일)
    if (deviceChart) {
        deviceChart->addSpeedData(currentSpeed, average);
        qDebug() << "피더 차트 데이터 추가 완료";
    } else {
        qDebug() << "차트가 아직 초기화되지 않음";
    }
}

//차트
void MainWindow::setupChartInUI() {
    qDebug() << "차트 UI 설정 시작";

    // 모든 필수 요소들이 존재하는지 확인
    if (!textErrorStatus) {
        qDebug() << "❌ textErrorStatus가 null";
        return;
    }

    if (!deviceChart) {
        qDebug() << "❌ deviceChart가 null";
        return;
    }

    QWidget *chartWidget = deviceChart->getChartWidget();
    if (!chartWidget) {
        qDebug() << "❌ 차트 위젯이 null";
        return;
    }

    // 부모 위젯 안전하게 찾기
    QWidget *parentWidget = textErrorStatus->parentWidget();
    if (!parentWidget) {
        qDebug() << "❌ 부모 위젯을 찾을 수 없음";
        return;
    }

    QLayout *parentLayout = parentWidget->layout();
    if (!parentLayout) {
        qDebug() << "❌ 부모 레이아웃을 찾을 수 없음";
        return;
    }
    try {
        textErrorStatus->hide();
        parentLayout->removeWidget(textErrorStatus);

        // ✅ 차트 높이를 적당히 설정
        chartWidget->setMinimumHeight(220);
        chartWidget->setMaximumHeight(260);
        // ✅ 차트 위젯 자체의 여백도 최소화
        chartWidget->setContentsMargins(0, 0, 0, 0);

        parentLayout->addWidget(chartWidget);

        qDebug() << "✅ 차트만 UI 설정 완료";

    } catch (...) {
        qDebug() << "❌ 차트 UI 설정 중 예외 발생";
    }
}

void MainWindow::onChartRefreshRequested(const QString &deviceName) {
    qDebug() << "차트 새로고침 요청됨:" << deviceName;

    if (deviceChart) {
        deviceChart->clearAllData();
    }

    // 통계 데이터 다시 요청
    requestStatisticsData();

    qDebug() << "통계 데이터 재요청 완료";
}

void MainWindow::initializeDeviceChart() {
    qDebug() << "차트 초기화 시작";

    // textErrorStatus가 존재하는지 확인
    if (!textErrorStatus) {
        qDebug() << "textErrorStatus가 null입니다. 차트 초기화 건너뜀";
        return;
    }

    // 차트 객체 생성
    deviceChart = new DeviceChart("피더", this);
    connect(deviceChart, &DeviceChart::refreshRequested,
            this, &MainWindow::onChartRefreshRequested);

    qDebug() << "차트 객체 생성 완료";

    // UI 배치 (안전하게)
    setupChartInUI();
}

// 영상 다운로드 및 재생
void MainWindow::downloadAndPlayVideoFromUrl(const QString& httpUrl, const QString& deviceId) {
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

void MainWindow::addErrorCardUI(const QJsonObject &errorData) {
    if (errorData["device_id"].toString() != "feeder_01") return;
    QWidget* card = new QWidget();
    card->setFixedHeight(84);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setStyleSheet(R"(
        background-color: #F3F4F6;
        border: 1px solid #E5E7EB;
        border-radius: 12px;
    )");
    card->setProperty("errorData", QVariant::fromValue(errorData));

    // 카드 생성 시 이벤트 필터 및 시그널 연결, 디버깅 로그 추가
    static CardEventFilter* filter = nullptr;
    if (!filter) {
        filter = new CardEventFilter(this);
        connect(filter, &CardEventFilter::cardDoubleClicked, this, &MainWindow::onCardDoubleClicked);
    }
    qDebug() << "[MainWindow] 카드에 이벤트 필터 설치";
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
    QString devStyle = dev.contains("feeder")
                           ? R"(
            background-color: #FFF4DE;
            color: #FF9138;
            border: none;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 11px;
            font-weight: 500;
        )"
                           : R"(
            background-color: #E1F5FF;
            color: #56A5FF;
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

    // 삽입
    if (errorCardLayout) {
        errorCardLayout->insertWidget(0, card);
    }

    // 카드 생성 후 아래 코드 추가
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
}

void MainWindow::onCardDoubleClicked(QObject* cardWidget) {
    qDebug() << "[MainWindow] onCardDoubleClicked 호출됨";
    QWidget* card = qobject_cast<QWidget*>(cardWidget);
    if (!card) return;
    QVariant v = card->property("errorData");
    if (!v.isValid()) return;
    QJsonObject errorData = v.value<QJsonObject>();

    // 로그 정보 추출
    qint64 timestamp = errorData["timestamp"].toVariant().toLongLong();
    QString deviceId = errorData["device_id"].toString();

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    int currentYear = dateTime.date().year();
    QString month = dateTime.toString("MM");
    QString day = dateTime.toString("dd");
    QString hour = dateTime.toString("hh");
    QString minute = dateTime.toString("mm");
    QString second = dateTime.toString("ss");

    QDateTime ts = QDateTime::fromString(
        QString("%1%2%3%4%5%6").arg(currentYear).arg(month).arg(day).arg(hour).arg(minute).arg(second),
        "yyyyMMddhhmmss"
        );

    qint64 startTime = ts.addSecs(-60).toMSecsSinceEpoch();
    qint64 endTime = ts.addSecs(+300).toMSecsSinceEpoch();

    // --- 여기서 MQTT 명령 전송 ---
    if (m_client && m_client->state() != QMqttClient::Connected) {
        qDebug() << "[MainWindow] MQTT disconnected, retry";
        m_client->connectToHost();
        // 연결 완료 시 publish하도록 콜백 등록 필요
        connect(m_client, &QMqttClient::connected, this, [this]() {
            qDebug() << "[MainWindow] MQTT reconnected success, publish 시도";
            m_client->publish(QMqttTopicName("factory/hanwha/cctv/zoom"), QByteArray("100"));
            m_client->publish(QMqttTopicName("factory/hanwha/cctv/cmd"), QByteArray("autoFocus"));
        });
    } else if (m_client && m_client->state() == QMqttClient::Connected) {
        m_client->publish(QMqttTopicName("factory/hanwha/cctv/zoom"), QByteArray("100"));
        m_client->publish(QMqttTopicName("factory/hanwha/cctv/cmd"), QByteArray("autoFocus"));
    }
    qDebug() << "[MainWindow] m_client:" << m_client << "state:" << (m_client ? m_client->state() : -1);
    qDebug() << "[MainWindow] publish zoom 100, autoFocus";

    VideoClient* client = new VideoClient(this);
    client->queryVideos(deviceId, "", startTime, endTime, 1,
                        [this, errorData](const QList<VideoInfo>& videos) {
                            if (videos.isEmpty()) {
                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
                                return;
                            }
                            QString httpUrl = videos.first().http_url;
                            // --- 여기서 MQTT 명령 전송 --- (줌 아웃 -100, autoFocus) 코드를 삭제
                            this->downloadAndPlayVideoFromUrl(httpUrl, errorData["device_id"].toString());
                        }
                        );
}
void MainWindow::setupErrorCardUI() {
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


void MainWindow::requestStatisticsData() {
    if(m_client && m_client->state() == QMqttClient::Connected) {
        qDebug() << "🔵 피더 통계 요청 시작";

        QJsonObject request;
        request["device_id"] = "feeder_01";

        QDateTime now = QDateTime::currentDateTime();
        QDateTime oneMinuteAgo = now.addSecs(-60);
        QJsonObject timeRange;
        timeRange["start"] = oneMinuteAgo.toMSecsSinceEpoch();
        timeRange["end"] = now.toMSecsSinceEpoch();
        request["time_range"] = timeRange;

        QJsonDocument doc(request);
        QByteArray payload = doc.toJson(QJsonDocument::Compact);

        qDebug() << "🔵 요청 JSON:" << doc.toJson(QJsonDocument::Indented);

        bool result = m_client->publish(QString("factory/statistics"), payload);
        qDebug() << "🔵 MQTT 전송 결과:" << (result ? "성공" : "실패");
        qDebug() << "🔵 MainWindow - 피더 통계 요청 전송 (1분마다)";
    } else {
        qDebug() << "❌ MQTT 연결 안됨! 현재 상태:" << m_client->state();
    }
}

void MainWindow::addNoResultsMessage() {
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
