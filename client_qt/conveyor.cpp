#include "conveyor.h"
#include "./ui_conveyor.h"
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

ConveyorWindow::ConveyorWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ConveyorWindow)
    , m_client(nullptr)
    , subscription(nullptr)
    , DeviceLockActive(false) //초기는 정상!
    , conveyorStartDateEdit(nullptr)  //  초기화 추가
    , conveyorEndDateEdit(nullptr)    //  초기화 추가
{
    ui->setupUi(this);
    setWindowTitle("Conveyor Control");
    setupLogWidgets();
    setupControlButtons();
    setupRightPanel();

    setupHomeButton();
    setupMqttClient(); //mqtt 설정
    connectToMqttBroker(); //연결 시도

    // 로그 더블클릭 이벤트 연결
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &ConveyorWindow::on_listWidget_itemDoubleClicked);


    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    rpiStreamer = new Streamer("rtsp://192.168.0.52:8555/stream2", this);

    // 한화 카메라 스트리머 객체 생성
    hwStreamer = new Streamer("rtsp://192.168.0.76:8553/stream_pno", this);

    // signal-slot
    connect(rpiStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateRPiImage);
    rpiStreamer->start();

    // 한화 signal-slot 연결
    connect(hwStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateHWImage);
    hwStreamer->start();

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

    reconnectTimer->stop(); //연결이 성공하면 재연결 타이며 멈추기!


}

void ConveyorWindow::onMqttDisConnected(){
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(5000);
    }
    subscription=NULL; //초기화
}

void ConveyorWindow::onMqttMessageReceived(const QMqttMessage &message){  //매개변수 수정
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  //토픽 정보도 가져올 수 있음
    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가

    if(messageStr == "on"){
        logMessage("컨베이어가 시작되었습니다.");
        logError("컨베이어가 시작되었습니다.");
        showConveyorError("컨베이어가 시작되었습니다.");
        updateErrorStatus();
        emit deviceStatusChanged("conveyor_01", "on");
    }
    else if(messageStr == "off"){
        logMessage("컨베이어가 정지되었습니다.");
        showConveyorNormal();
        emit deviceStatusChanged("conveyor_01", "off");
    }
    // else if(messageStr == "reverse"){
    //     logError("반대로 돌았습니다.");
    //     showConveyorError("반대로 돌았습니다.");
    //     updateErrorStatus();
    // }
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
    ui->labelEvent->setText(conveyorErrorType + "이(가) 감지되었습니다");
    ui->labelErrorValue->setText(conveyorErrorType);
    ui->labelTimeValue->setText(datetime);
    ui->labelLocationValue->setText("컨베이어 구역");
    ui->labelCameraValue->setText("conveyor_CAMERA1");

    //ui->labelCamRPi->setText("RaspberryPi CAM [컨베이어 모니터링]");
    //ui->labelCamHW->setText("한화비전 카메라 [컨베이어 추적 모드]");
}

void ConveyorWindow::showConveyorNormal(){
    qDebug() << "정상 상태 함수 호출됨";

    ui->labelEvent->setText("컨베이어 시스템이 정상 작동 중");
    ui->labelErrorValue->setText("오류가 없습니다.");
    ui->labelTimeValue->setText("-");
    ui->labelLocationValue->setText("-");
    ui->labelCameraValue->setText("-");

    ui->labelCamRPi->setText("RaspberryPi CAM [정상 모니터링]");
    ui->labelCamHW->setText("한화비전 카메라 [정상 모니터]");
}


void ConveyorWindow::initializeUI(){

}

void ConveyorWindow::setupControlButtons(){
    QVBoxLayout *mainLayout = new QVBoxLayout(ui->groupControl);

    //QPushButton *btnConveyorOn = new QPushButton("conveyor 켜기");
    btnConveyorOn = new QPushButton("컨베이어 시작");
    mainLayout->addWidget(btnConveyorOn);
    connect(btnConveyorOn, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOnClicked);

    //QPushButton *btnConveyorOff = new QPushButton("conveyor 끄기");
    btnConveyorOff = new QPushButton("컨베이어 정지");
    mainLayout->addWidget(btnConveyorOff);
    connect(btnConveyorOff, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOffClicked);

    //QPushButton *btnConveyorOff = new QPushButton("conveyor 역방향");
    // btnConveyorReverse = new QPushButton("컨베이어 역방향");
    // mainLayout->addWidget(btnConveyorReverse);
    // connect(btnConveyorReverse, &QPushButton::clicked, this, &ConveyorWindow::onConveyorReverseClicked);

    //QPushButton *btnDeviceLock = new QPushButton("비상 정지");
    btnDeviceLock = new QPushButton("기기 잠금");
    mainLayout->addWidget(btnDeviceLock);
    connect(btnDeviceLock, &QPushButton::clicked, this, &ConveyorWindow::onDeviceLock);

    //QPushButton *btnShutdown = new QPushButton("전원끄기");
    //btnShutdown = new QPushButton("전원끄기");
    //mainLayout->addWidget(btnShutdown);
    //connect(btnShutdown, &QPushButton::clicked, this, &ConveyorWindow::onShutdown);

    //QLabel *speedTitle = new QLabel("속도제어: ");
    // QLabel *speedTitle = new QLabel("속도제어: ");
    // speedLabel = new QLabel("속도 : 0%");
    // speedSlider = new QSlider(Qt::Horizontal);
    // speedSlider->setRange(0,100);
    // speedSlider->setValue(0);

    // mainLayout->addWidget(speedTitle);
    // mainLayout->addWidget(speedLabel);
    // mainLayout->addWidget(speedSlider);
    // connect(speedSlider, &QSlider::valueChanged, this, &ConveyorWindow::onSpeedChange);

    //QPushButton *btnSystemReset = new QPushButton("시스템 리셋");
    btnSystemReset = new QPushButton("시스템 리셋");
    mainLayout->addWidget(btnSystemReset);
    connect(btnSystemReset, &QPushButton::clicked, this, &ConveyorWindow::onSystemReset);
    ui->groupControl->setLayout(mainLayout);
}

void ConveyorWindow::onConveyorOnClicked(){
    qDebug()<<"컨베이어 시작 버튼 클릭됨";
    publishControlMessage("on");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "conveyor_01";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    //emit requestMqttPublish("factory/msg/status", "on");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));


}

void ConveyorWindow::onConveyorOffClicked(){
    qDebug()<<"컨베이어 정지 버튼 클릭됨";
    publishControlMessage("off");

    // 공통 제어 - JSON 형태로
    QJsonObject logData;
    logData["log_code"] = "SHD";
    logData["message"] = "conveyor_01";
    logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QJsonDocument doc(logData);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    //emit requestMqttPublish("factory/msg/status", "off");
    emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

}

void ConveyorWindow::onDeviceLock(){
    if(!DeviceLockActive){
        DeviceLockActive=true;

        btnConveyorOn->setEnabled(false);
        btnConveyorOff->setEnabled(false);
        btnDeviceLock->setText("기기 잠금");
        //speedSlider->setEnabled(false);

        qDebug()<<"기기 잠금 버튼 클릭됨";
        //publishControlMessage("off");//EMERGENCY_STOP
        logMessage("기기 잠금 명령 전송!");
    }
}

void ConveyorWindow::onSystemReset(){
    DeviceLockActive= false;
    btnConveyorOn->setEnabled(true);
    btnConveyorOff->setEnabled(true);
    //btnConveyorReverse->setEnabled(true);
    //speedSlider->setEnabled(true);
    btnDeviceLock->setText("기기 잠금");
    btnDeviceLock->setStyleSheet("");

    qDebug()<<"다시 시작";
    //publishControlMessage("off");
    logMessage("컨베이어 시스템 리셋 완료!");
}

// void ConveyorWindow::onShutdown(){
//     qDebug()<<"정상 종료 버튼 클릭됨";
//     publishControlMessage("off");//SHUTDOWN
//     logMessage("정상 종료 명령 전송");
// }

// void ConveyorWindow::onSpeedChange(int value){
//     qDebug()<<"컨베이어 속도 변경 됨" <<value << "%";
//     speedLabel->setText(QString("컨베이어 속도:%1%").arg(value));
//     QString cmd = QString("SPEED_%1").arg(value);
//     publishControlMessage(cmd);
//     logMessage(QString("컨베이어 속도 변경: %1%").arg(value));
// }


void ConveyorWindow::setupHomeButton(){

    QHBoxLayout *topLayout = qobject_cast<QHBoxLayout*>(ui->topBannerWidget->layout());

    btnbackhome = new QPushButton("홈화면으로 이동");
    topLayout->insertWidget(0, btnbackhome);
    connect(btnbackhome, &QPushButton::clicked, this, &ConveyorWindow::gobackhome);
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

void ConveyorWindow::updateErrorStatus(){
}

void ConveyorWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 컨베이어 오류" + errorType);
    }
}
void ConveyorWindow::setupLogWidgets(){
    QHBoxLayout *bottomLayout = qobject_cast<QHBoxLayout*>(ui->bottomSectionWidget->layout());

    if(bottomLayout){
        QWidget* oldTextLog = ui->textLog;
        bottomLayout->removeWidget(oldTextLog);
        oldTextLog->hide();

        QSplitter *logSplitter = new QSplitter(Qt::Horizontal);
        QGroupBox *eventLogGroup = new QGroupBox("실시간 이벤트 로그");
        QVBoxLayout *eventLayout = new QVBoxLayout(eventLogGroup);
        textEventLog = new QTextEdit();
        eventLayout->addWidget(textEventLog);

        QGroupBox *statusGroup = new QGroupBox("기기 상태");
        QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
        textErrorStatus = new QTextEdit();
        textErrorStatus->setReadOnly(true);
        textErrorStatus->setMaximumWidth(300);
        statusLayout->addWidget(textErrorStatus);

        if(textErrorStatus){
            QString initialText = "평균 속도: \n";
            initialText += "현재 속도: \n";
            initialText += "불량률 : ";
            textErrorStatus->setText(initialText);
        }

        logSplitter->addWidget(eventLogGroup);
        logSplitter->addWidget(statusGroup);
        logSplitter->setStretchFactor(0,50);
        logSplitter->setStretchFactor(1,50);

        bottomLayout->insertWidget(0,logSplitter);

        updateErrorStatus();

    }
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

void ConveyorWindow::setupRightPanel(){
    qDebug() << "=== ConveyorWindow 검색 패널 설정 시작 ===";

    // 레이블 설정
    if(ui->label){
        ui->label->setText("컨베이어 오류 로그");
        ui->label->setStyleSheet("font-weight: bold; font-size: 14px;");
    }

    // 검색 입력창 설정 (피더와 동일)
    if(ui->lineEdit){
        ui->lineEdit->setPlaceholderText("컨베이어 오류 코드 (예: SPD)");
    }

    // 검색 버튼 설정 (피더와 동일)
    if(ui->pushButton){
        ui->pushButton->setText("날짜 조회 (최신순)");
        disconnect(ui->pushButton, &QPushButton::clicked, 0, 0);
        connect(ui->pushButton, &QPushButton::clicked, this, &ConveyorWindow::onConveyorSearchClicked);
    }

    //  widget_6을 사용해서 날짜 위젯 추가 (MainWindow와 동일한 방식)
    if(ui->widget_6) {
        QVBoxLayout *rightLayout = qobject_cast<QVBoxLayout*>(ui->widget_6->layout());
        if(!rightLayout) {
            rightLayout = new QVBoxLayout(ui->widget_6);
        }

        //  날짜 검색 위젯을 검색창과 리스트 사이에 추가
        if(!conveyorStartDateEdit && !conveyorEndDateEdit) {
            QGroupBox* dateGroup = new QGroupBox("날짜 필터");
            QVBoxLayout* dateLayout = new QVBoxLayout(dateGroup);

            // 시작 날짜
            QHBoxLayout* startLayout = new QHBoxLayout();
            startLayout->addWidget(new QLabel("시작일:"));
            conveyorStartDateEdit = new QDateEdit();
            conveyorStartDateEdit->setDate(QDate::currentDate().addDays(-7)); // 기본: 일주일 전
            conveyorStartDateEdit->setCalendarPopup(true);
            conveyorStartDateEdit->setDisplayFormat("yyyy-MM-dd");
            startLayout->addWidget(conveyorStartDateEdit);

            // 종료 날짜
            QHBoxLayout* endLayout = new QHBoxLayout();
            endLayout->addWidget(new QLabel("종료일:"));
            conveyorEndDateEdit = new QDateEdit();
            conveyorEndDateEdit->setDate(QDate::currentDate()); // 기본: 오늘
            conveyorEndDateEdit->setCalendarPopup(true);
            conveyorEndDateEdit->setDisplayFormat("yyyy-MM-dd");
            endLayout->addWidget(conveyorEndDateEdit);

            dateLayout->addLayout(startLayout);
            dateLayout->addLayout(endLayout);

            //  초기화 버튼 (피더와 동일)
            QPushButton* resetDateBtn = new QPushButton("전체 초기화 (최신순)");
            connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
                qDebug() << "🔄 컨베이어 전체 초기화 버튼 클릭됨";

                // 날짜 초기화
                if(conveyorStartDateEdit && conveyorEndDateEdit) {
                    conveyorStartDateEdit->setDate(QDate::currentDate().addDays(-7));
                    conveyorEndDateEdit->setDate(QDate::currentDate());
                    qDebug() << " 컨베이어 날짜 필터 초기화됨";
                }

                // 검색어 초기화
                if(ui->lineEdit) {
                    ui->lineEdit->clear();
                    qDebug() << " 컨베이어 검색어 초기화됨";
                }

                // 최신 로그 다시 불러오기
                qDebug() << "🔄 컨베이어 최신 로그 다시 불러오기 시작...";
                emit requestConveyorLogSearch("", QDate(), QDate());
            });
            dateLayout->addWidget(resetDateBtn);

            //  레이아웃에 추가 (검색창 아래, 리스트 위)
            // widget_7(검색위젯) 다음 위치에 삽입
            int insertIndex = 2; // label(0), widget_7(1), dateGroup(2), listWidget(3)
            rightLayout->insertWidget(insertIndex, dateGroup);

            qDebug() << " 컨베이어 날짜 검색 위젯을 검색창과 리스트 사이에 생성 완료";
            qDebug() << "  - conveyorStartDateEdit 주소:" << conveyorStartDateEdit;
            qDebug() << "  - conveyorEndDateEdit 주소:" << conveyorEndDateEdit;
        }
    }

    // 리스트 위젯 설정
    if(ui->listWidget){
        ui->listWidget->clear();
        ui->listWidget->setAlternatingRowColors(true);
    }

    //  초기 로그 로딩 (500ms 후)
    QTimer::singleShot(500, this, [this]() {
        loadPastLogs();
    });

    qDebug() << "=== ConveyorWindow 검색 패널 설정 완료 ===";
}

void ConveyorWindow::addErrorLog(const QJsonObject &errorData){
    if(!ui->listWidget) return;

    QString currentTime = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");
    QString logText = QString("[%1] %2")
                          .arg(currentTime)
                          .arg(errorData["log_code"].toString());

    QListWidgetItem *item = new QListWidgetItem(logText);
    item->setData(Qt::UserRole, errorData["error_log_id"].toString());
    ui->listWidget->insertItem(0, item);

    if(ui->listWidget->count() > 20){
        delete ui->listWidget->takeItem(20);
    }

    ui->listWidget->setCurrentRow(0);
}

void ConveyorWindow::loadPastLogs(){
    // 부모에게 시그널로 과거 로그 요청
    emit requestErrorLogs("conveyor_01");
}

// 부모로부터 로그 응답 받는 슬롯
void ConveyorWindow::onErrorLogsReceived(const QList<QJsonObject> &logs){
    if(!ui->listWidget) return;
    QList<QJsonObject> conveyorLogs;
    for(const QJsonObject &log : logs) {
        if(log["device_id"].toString() == "conveyor_01") {
            conveyorLogs.append(log);
        }
    }

    if(conveyorLogs.isEmpty()) {
        qDebug() << "ConveyorWindow - 컨베이어 로그가 없음, 무시";
        return;
    }

    int existingCount = ui->listWidget->count();
    qDebug() << "ConveyorWindow - 기존로그:" << existingCount << "개, 새로 받는 컨베이어 로그:" << conveyorLogs.size() << "개";

    ui->listWidget->clear();

    for(const QJsonObject &log : conveyorLogs){
        qint64 timestamp = 0;
        QJsonValue timestampValue = log["timestamp"];
        if(timestampValue.isDouble()) {
            timestamp = (qint64)timestampValue.toDouble();
        } else if(timestampValue.isString()) {
            timestamp = timestampValue.toString().toLongLong();
        } else {
            timestamp = timestampValue.toVariant().toLongLong();
        }

        if(timestamp == 0) {
            timestamp = QDateTime::currentMSecsSinceEpoch();
        }


        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString logTime = dateTime.toString("MM-dd hh:mm:ss");

        QString logText = QString("[%1] %2")
                              .arg(logTime)
                              .arg(log["log_code"].toString());


        ui->listWidget->addItem(logText);
        QString logCode = log["log_code"].toString();
        if(!logCode.isEmpty()) {
            logError(logCode);
            showConveyorError(logCode);
        }

        QListWidgetItem *item = new QListWidgetItem(logText);
        item->setData(Qt::UserRole, log["error_log_id"].toString());
        ui->listWidget->addItem(item);

        qDebug() << "ConveyorWindow - 컨베이어 로그 추가:" << logText;
    }

    updateErrorStatus();
    qDebug() << "ConveyorWindow - 최종 로그 개수:" << ui->listWidget->count() << "개";

}

void ConveyorWindow::onErrorLogBroadcast(const QJsonObject &errorData){
    QString deviceId = errorData["device_id"].toString();

    if(deviceId == "conveyor_01"){
        QString logCode = errorData["log_code"].toString();
        showConveyorError(logCode);
        logError(logCode);
        updateErrorStatus();
        addErrorLog(errorData);

        qDebug() << "MainWindow - 실시간 컨베이어 로그 추가:" << logCode;
    } else {
        qDebug() << "MainWindow - 다른 디바이스 로그 무시:" << deviceId;
    }
}


//  기본 검색 함수 (기존 onSearchClicked 유지)
void ConveyorWindow::onSearchClicked(){
    QString searchText = ui->lineEdit->text().trimmed();
    emit requestFilteredLogs("conveyor_01", searchText);
}

void ConveyorWindow::onSearchResultsReceived(const QList<QJsonObject> &results) {
    qDebug() << " 컨베이어 검색 결과 수신됨: " << results.size() << "개";

    // 버튼 재활성화
    if(ui->pushButton) {
        ui->pushButton->setEnabled(true);
    }

    if(!ui->listWidget) {
        qDebug() << " listWidget이 null입니다!";
        return;
    }

    ui->listWidget->clear();

    if(results.isEmpty()) {
        ui->listWidget->addItem(" 검색 조건에 맞는 컨베이어 로그가 없습니다.");
        return;
    }

    //  에러 로그만 필터링 및 표시
    int errorCount = 0;
    for(const QJsonObject &log : results) {
        //  에러 레벨 체크
        QString logLevel = log["log_level"].toString();
        if(logLevel != "error") {
            qDebug() << " 일반 로그 필터링됨:" << log["log_code"].toString() << "레벨:" << logLevel;
            continue; // INF, WRN 등 일반 로그 제외
        }

        //  타임스탬프 처리
        qint64 timestamp = 0;
        QJsonValue timestampValue = log["timestamp"];
        if(timestampValue.isDouble()) {
            timestamp = (qint64)timestampValue.toDouble();
        } else if(timestampValue.isString()) {
            timestamp = timestampValue.toString().toLongLong();
        } else {
            timestamp = timestampValue.toVariant().toLongLong();
        }

        if(timestamp == 0) {
            timestamp = QDateTime::currentMSecsSinceEpoch();
        }

        //  시간 형식 변경 (간단하게)
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString logTime = dateTime.toString("MM-dd hh:mm");

        //  출력 형식: [시간] 오류코드
        QString logCode = log["log_code"].toString();
        QString logText = QString("[%1] %2")
                              .arg(logTime)
                              .arg(logCode);

        ui->listWidget->addItem(logText);
        errorCount++;

        // 통계 업데이트
        if(!logCode.isEmpty()) {
            logError(logCode);
            showConveyorError(logCode);
        }

        qDebug() << " 컨베이어 에러 로그 추가:" << logText;
    }

    updateErrorStatus();
    qDebug() << " 최종 컨베이어 에러 로그:" << errorCount << "개 표시됨 (INF 제외)";
}


void ConveyorWindow::onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData){
    qDebug() << "ConveyorWindow - 통계 데이터 수신됨!";
    qDebug() << "Device ID:" << deviceId;
    qDebug() << "Stats Data:" << QJsonDocument(statsData).toJson(QJsonDocument::Compact);

    if(deviceId != "conveyor_01") {
        qDebug() << "ConveyorWindow - 컨베이어가 아님, 무시";
        return;
    }

    // textErrorStatus 존재 확인
    if(!textErrorStatus) {
        qDebug() << "ConveyorWindow - textErrorStatus가 null입니다!";
        return;
    }

    // 새로운 JSON 형식에 맞게 수정
    int currentSpeed = statsData["current_speed"].toInt();
    int average = statsData["average"].toInt();

    qDebug() << "Current Speed:" << currentSpeed << "Average:" << average;

    QString statsText;
    statsText += QString("현재 속도: %1\n").arg(currentSpeed);
    statsText += QString("평균 속도: %1\n").arg(average);
    statsText += QString("불량률: \n");  // 나중에 추가될 데이터

    textErrorStatus->setText(statsText);
    qDebug() << "ConveyorWindow - 통계 텍스트 업데이트됨:" << statsText;
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
                        [this](const QList<VideoInfo>& videos) {
                            //static bool isProcessing = false;
                            isProcessing = false; // 재설정

                            if (videos.isEmpty()) {
                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
                                return;
                            }

                            QString httpUrl = videos.first().http_url;
                            this->downloadAndPlayVideoFromUrl(httpUrl);
                        });
}

// 영상 다운로드 및 재생
void ConveyorWindow::downloadAndPlayVideoFromUrl(const QString& httpUrl) {
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

    connect(reply, &QNetworkReply::finished, [this, reply, file, savePath]() {
        file->close();
        delete file;

        bool success = (reply->error() == QNetworkReply::NoError);

        if (success) {
            qDebug() << "영상 저장 성공:" << savePath;
            VideoPlayer* player = new VideoPlayer(savePath, this);
            player->setAttribute(Qt::WA_DeleteOnClose);
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

    if(!ui->listWidget) {
        qDebug() << " listWidget null!";
        QMessageBox::warning(this, "UI 오류", "결과 리스트가 초기화되지 않았습니다.");
        return;
    }

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
    ui->listWidget->clear();
    ui->listWidget->addItem(" 컨베이어 검색 중... 잠시만 기다려주세요.");
    ui->pushButton->setEnabled(false);  // 중복 검색 방지

    qDebug() << " 컨베이어 통합 검색 요청 - Home으로 시그널 전달";

    //  검색어와 날짜 모두 전달
    emit requestConveyorLogSearch(searchText, startDate, endDate);

    qDebug() << " 컨베이어 검색 시그널 발송 완료";

    //  타임아웃 설정 (30초 후 버튼 재활성화)
    QTimer::singleShot(30000, this, [this]() {
        if(!ui->pushButton->isEnabled()) {
            qDebug() << " 컨베이어 검색 타임아웃 - 버튼 재활성화";
            ui->pushButton->setEnabled(true);

            if(ui->listWidget && ui->listWidget->count() == 1) {
                QString firstItem = ui->listWidget->item(0)->text();
                if(firstItem.contains("검색 중")) {
                    ui->listWidget->clear();
                    ui->listWidget->addItem(" 검색 시간이 초과되었습니다. 다시 시도해주세요.");
                }
            }
        }
    });
}
