#include "home.h"
#include "mainwindow.h"
#include "conveyor.h"
#include "./ui_home.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QDebug>

#include "factory_mcp.h" // mcp용
#include "ai_command.h"
#include "mcp_btn.h"
#include "chatbot_widget.h"


#include "videoplayer.h"
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QFile>
#include <QDesktopServices>

#include "video_mqtt.h"
#include "video_client_functions.hpp"

//mcp
#include <QProcess>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>

Home::Home(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Home)
    , m_client(nullptr)
    , subscription(nullptr)
    , queryResponseSubscription(nullptr)
    , factoryRunning(false)
    , feederWindow(nullptr)
    , conveyorWindow(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("기계 동작 감지 스마트팩토리 관제 시스템");

    connect(ui->listWidget, &QListWidget::itemDoubleClicked,
            this, &Home::on_listWidget_itemDoubleClicked);

    //setupNavigationPanel();

    setupRightPanel();
    setupErrorChart();
    setupMqttClient();
    connectToMqttBroker();

    // MCP 핸들러
    mcpHandler = new FactoryMCP(m_client, this);
    connect(mcpHandler, &FactoryMCP::errorOccurred, this,
            [](const QString &msg){ QMessageBox::warning(nullptr, "MCP 전송 실패", msg); });

    //QString keyPath = "client_qt/config/gemini.key";
    QString keyPath = QCoreApplication::applicationDirPath() + "/../../config/gemini.key";


    QFile keyFile(keyPath);
    if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&keyFile);
        apiKey = in.readLine().trimmed();
        keyFile.close();
        qDebug() << "[Gemini] API 키 로딩 성공";
    } else {
        QMessageBox::critical(this, "API 키 오류", "Gemini API 키 파일을 찾을 수 없습니다.\n" + keyPath);
        return;
    }
    gemini = new GeminiRequester(this, apiKey);

    //플로팅 버 튼 ㄴ
    aiButton = new MCPButton(this);
    aiButton->show();

    // 챗봇 창 ui
    chatBot = new ChatBotWidget(this);
    chatBot->setGemini(gemini);
    chatBot->hide();  // 시작 시 숨겨둠

    connect(aiButton, &MCPButton::clicked, this, [=]() {
        QPoint btnPos = aiButton->pos();
        int x = btnPos.x();
        int y = btnPos.y() - chatBot->height() - 12;

        chatBot->move(x, y);
        chatBot->show();
        chatBot->raise();  // 항상 위에
    });

    setupNavigationPanel();

    // 라파 카메라(feeder) 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    feederStreamer = new Streamer("rtsp://192.168.0.76:8554/stream1", this);

    // 라파 카메라(feeder) 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    conveyorStreamer = new Streamer("rtsp://192.168.0.52:8555/stream2", this);

    // 한화 카메라 스트리머 객체 생성
    hwStreamer = new Streamer("rtsp://192.168.0.76:8553/stream_pno", this);

    // signal-slot
    connect(feederStreamer, &Streamer::newFrame, this, &Home::updateFeederImage);
    feederStreamer->start();

    // signal-slot 컨베이어
    connect(conveyorStreamer, &Streamer::newFrame, this, &Home::updateConveyorImage);
    conveyorStreamer->start();

    // 한화 signal-slot 연결
    connect(hwStreamer, &Streamer::newFrame, this, &Home::updateHWImage);
    hwStreamer->start();

    //initializeChildWindows();
}

Home::~Home(){
    delete ui;
}

void Home::connectChildWindow(QObject *childWindow) {
    // 자식 윈도우와 시그널-슬롯 연결
    if(auto* mainWin = qobject_cast<MainWindow*>(childWindow)){
        connect(mainWin, &MainWindow::errorLogGenerated, this, &Home::onErrorLogGenerated);
        connect(mainWin, &MainWindow::requestErrorLogs, this, &Home::onErrorLogsRequested);
        connect(this, &Home::errorLogsResponse, mainWin, &MainWindow::onErrorLogsReceived);
        connect(this, &Home::newErrorLogBroadcast, mainWin, &MainWindow::onErrorLogBroadcast);
        connect(mainWin, &MainWindow::requestMqttPublish, this, &Home::onMqttPublishRequested);
        connect(mainWin, &MainWindow::deviceStatusChanged, this, &Home::onDeviceStatusChanged);
        connect(this, &Home::deviceStatsReceived, mainWin, &MainWindow::onDeviceStatsReceived);
        qDebug() << " Home - MainWindow 시그널 연결 완료";

    } else {
        qDebug() << " Home - MainWindow 캐스팅 실패!";
    }
    if(auto* conveyorWin = qobject_cast<ConveyorWindow*>(childWindow)) {
        // ConveyorWindow 연결
        connect(conveyorWin, &ConveyorWindow::errorLogGenerated, this, &Home::onErrorLogGenerated);
        connect(conveyorWin, &ConveyorWindow::requestErrorLogs, this, &Home::onErrorLogsRequested);
        connect(this, &Home::errorLogsResponse, conveyorWin, &ConveyorWindow::onErrorLogsReceived);
        connect(this, &Home::newErrorLogBroadcast, conveyorWin, &ConveyorWindow::onErrorLogBroadcast);
        connect(conveyorWin, &ConveyorWindow::deviceStatusChanged, this, &Home::onDeviceStatusChanged);
        connect(this, &Home::deviceStatsReceived, conveyorWin, &ConveyorWindow::onDeviceStatsReceived);
    }
}


void Home::onErrorLogGenerated(const QJsonObject &errorData) {
    addErrorLog(errorData);
    addErrorLogUI(errorData);
}

void Home::onErrorLogsRequested(const QString &deviceId) {
    QList<QJsonObject> filteredLogs = getErrorLogsForDevice(deviceId);
    emit errorLogsResponse(filteredLogs);
}

void Home::addErrorLog(const QJsonObject &errorData) {
    errorLogHistory.prepend(errorData);
    if(errorLogHistory.size() > 100) {
        errorLogHistory.removeLast();
    }
}

QList<QJsonObject> Home::getAllErrorLogs() const {
    return errorLogHistory;
}

QList<QJsonObject> Home::getErrorLogsForDevice(const QString &deviceId) const {
    QList<QJsonObject> filteredLogs;
    for(const QJsonObject &log : errorLogHistory) {
        if(log["device_id"].toString() == deviceId) {
            filteredLogs.append(log);
        }
    }
    return filteredLogs;
}

void Home::onFeederTabClicked(){
    this->hide();

    if(!feederWindow){
        feederWindow = new MainWindow(this);
        connectChildWindow(feederWindow);
        qDebug() << "Home - 피더 윈도우 생성 및 연결 완료";
    } else {
        qDebug() << "Home - 기존 피더 윈도우 재사용";
    }

    feederWindow->show();
    feederWindow->raise();
    feederWindow->activateWindow();

    QTimer::singleShot(300, [this](){
        QList<QJsonObject> feederLogs = getErrorLogsForDevice("feeder_01");
        qDebug() << "Home - 피더 탭에 피더 로그" << feederLogs.size() << "개 전달";

        if(feederWindow) {
            feederWindow->onErrorLogsReceived(feederLogs);
        }
    });
}

void Home::onContainerTabClicked(){
    this->hide();

    if(!conveyorWindow){
        conveyorWindow = new ConveyorWindow(this);
        connectChildWindow(conveyorWindow);
        qDebug() << "Home - 컨베이어 윈도우 생성 및 연결 완료";
    } else {
        qDebug() << "Home - 기존 컨베이어 윈도우 재사용";
    }

    conveyorWindow->show();
    conveyorWindow->raise();
    conveyorWindow->activateWindow();

    QTimer::singleShot(300, [this](){
        QList<QJsonObject> conveyorLogs = getErrorLogsForDevice("conveyor_01");
        qDebug() << "Home - 컨베이어 탭에 컨베이어 로그" << conveyorLogs.size() << "개 전달";

        if(conveyorWindow) {
            conveyorWindow->onErrorLogsReceived(conveyorLogs);
        }
    });
}

//전체 제어
void Home::onFactoryToggleClicked(){
    factoryRunning = !factoryRunning;

    if(factoryRunning){
        publicFactoryCommand("START");
        controlALLDevices(true);
    }
    else{
        publicFactoryCommand("STOP");
        controlALLDevices(false);
        sendFactoryStatusLog("SHD", "off");
    }
    updateFactoryStatus(factoryRunning);
}

void Home::publicFactoryCommand(const QString &command){
    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->publish(mqttControlTopic, command.toUtf8());

        if(command == "START"){
            qDebug() << "공장 가동 시작 명령 전송됨" ;
        }
        else if(command == "STOP"){
            qDebug() << "공장 중지 명령 전송됨";
        }
        else if(command == "EMERGENCY_STOP"){
            qDebug() << "공장 비상정지 명령 전송됨";
            QMessageBox::warning(this, "비상정지", "공장 비상정지 명령이 전송되었습니다!");
        }
    }
    else{
        qDebug() << "Home - MQTT 연결 안됨, 명령 전송 실패";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.\n명령을 전송할 수 없습니다.");
    }


}

void Home::onMqttConnected(){
    static bool alreadySubscribed = false;

    if(alreadySubscribed) {
        qDebug() << "Home - 이미 구독됨, 건너뜀";
        return;
    }

    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
    }

    auto feederSubscription  = m_client->subscribe(QString("feeder_01/status"));
    if(feederSubscription){
        connect(feederSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
        qDebug() << " Home - feeder_01/status 구독됨";
    }

    auto conveyorSubscription = m_client->subscribe(QString("conveyor_01/status"));
    if(conveyorSubscription){
        connect(conveyorSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
        qDebug() << " Home - conveyor_01/status 구독됨";
    }

    //db 연결 mqtt
    auto errorSubscription = m_client->subscribe(QString("factory/+/log/error"));
    connect(errorSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
    qDebug() << " Home - factory/+/log/error 구독됨";

    queryResponseSubscription = m_client->subscribe(mqttQueryResponseTopic);
    if(queryResponseSubscription){
        connect(queryResponseSubscription, &QMqttSubscription::messageReceived, this, &Home::onQueryResponseReceived); //응답이 오면 onQueryResponseReceived 함수가 자동으로 호출되도록 연결
        qDebug() << "response 됨";
    }

    auto infoSubscription = m_client->subscribe(QString("factory/msg/status"));
    connect(infoSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
    alreadySubscribed = true;
    reconnectTimer->stop();

    //기기 상태
    auto feederStatsSubscription = m_client->subscribe(QString("factory/feeder_01/msg/statistics"));
    connect(feederStatsSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);

    auto conveyorStatsSubscription = m_client->subscribe(QString("factory/conveyor_01/msg/statistics"));
    connect(conveyorSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);

    QTimer::singleShot(1000, this, &Home::requestPastLogs); //MQTT 연결이 완전히 안정된 후 1초 뒤에 과거 로그를 자동으로 요청
}

void Home::onMqttDisConnected(){
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(5000);
    }
    subscription=NULL; //초기화
    queryResponseSubscription = NULL;
}

void Home::onMqttMessageReceived(const QMqttMessage &message){
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  //토픽 정보도 가져올 수 있음
    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가


    //db 로그 받기
    if(topicStr.contains("/log/error")){
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1];

        QJsonDocument doc = QJsonDocument::fromJson(message.payload());
        QJsonObject errorData = doc.object();
        errorData["device_id"] = deviceId;

        onErrorLogGenerated(errorData);
        processErrorForChart(errorData);
        addErrorLog(errorData);  // 부모가 직접 처리

        // if(deviceId == "feeder_01") {
        //     pendingFeederLogs.append(errorData);
        //     if(pendingFeederLogs.size() > 10) {
        //         pendingFeederLogs.removeFirst(); // 최대 10개만 유지
        //     }
        // }

        // if(deviceId == "conveyor_01") {
        //     pendingConveyorLogs.append(errorData);
        //     if(pendingConveyorLogs.size() > 10) {
        //         pendingConveyorLogs.removeFirst(); // 최대 10개만 유지
        //     }
        // }

        emit newErrorLogBroadcast(errorData);

        return;
    }

    if(topicStr == "factory/status"){
        if(messageStr == "RUNNING"){
            factoryRunning = true;
            updateFactoryStatus(true);
        }
        else if(messageStr == "STOPPED"){
            factoryRunning = false;
            updateFactoryStatus(false);
        }
    }
    else if(topicStr == "feeder_01/status"){
        if(messageStr == "on"){
            qDebug() << "Home - 피더 정방향 시작";       // 로그 메시지 개선
        }
        else if(messageStr == "off"){
            qDebug() << "Home - 피더 정지됨";           // 로그 메시지 개선
        }
        else if(messageStr == "reverse"){               // reverse 추가
            qDebug() << "Home - 피더 역방향 시작";
        }
        else if(messageStr.startsWith("SPEED_") || messageStr.startsWith("MOTOR_")){  // 오류 감지 개선
            qDebug() << "Home - 피더 오류 감지:" << messageStr;
        }
    }
    else if(topicStr == "robot_arm_01/status"){
        if(messageStr == "on"){
            qDebug() << "Home - 로봇팔 시작됨";
        }
        else if(messageStr == "off"){
            qDebug() << "Home - 로봇팔 정지됨";
        }
    }
    else if(topicStr == "conveyor_01/status"){
        if(messageStr == "on"){
            qDebug() << "Home - 컨베이어 정방향 시작";       // 로그 메시지 개선
        }
        else if(messageStr == "off"){
            qDebug() << "Home - 컨베이어 정지됨";           // 로그 메시지 개선
        }
        else if(messageStr == "error_mode"){
            qDebug() << "Home - 컨베이어 속도";
        }
        else if(messageStr.startsWith("SPEED_")){  // 오류 감지 개선
            qDebug() << "Home - 컨베이어 오류 감지:" << messageStr;
        }
    }
    else if(topicStr == "conveyor_02/status"){
        if(messageStr == "on"){
            qDebug() << "Home - 컨베이어 시작됨";
        }
        else if(messageStr == "off"){
            qDebug() << "Home - 컨베이어 정지됨";
        }
    }
    else if(topicStr.contains("/msg/statistics")) {
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1]; // feeder_01 또는 conveyor_01

        QJsonDocument doc = QJsonDocument::fromJson(message.payload());
        QJsonObject statsData = doc.object();

        // 해당 탭으로 전달
        emit deviceStatsReceived(deviceId, statsData);
    }
}

void Home::connectToMqttBroker(){
    if(m_client->state() == QMqttClient::Disconnected){
        m_client->connectToHost();
    }
}

void Home::setupNavigationPanel(){
    if(!ui->leftPanel) {
        qDebug() << "leftPanel이 null입니다!";
        return;
    }

    QVBoxLayout *leftLayout = qobject_cast<QVBoxLayout*>(ui->leftPanel->layout());

    if(!leftLayout) {
        leftLayout = new QVBoxLayout(ui->leftPanel);
    }

    // 탭 이동 버튼 생성
    btnFeederTab = new QPushButton("Feeder Tab");
    btnConveyorTab = new QPushButton("Conveyor Tab");


    // 사이즈 공장이랑 맞춰줌
    int buttonHeight = 40;
    btnFeederTab->setFixedHeight(buttonHeight);
    btnConveyorTab->setFixedHeight(buttonHeight);


    initializeFactoryToggleButton();


    // 레이아웃에 버튼 추가
    leftLayout->addSpacing(15);    // visioncraft 밑에 마진
    leftLayout->addWidget(btnFactoryToggle);
    leftLayout->addWidget(btnFeederTab);
    leftLayout->addWidget(btnConveyorTab);

    connect(btnFeederTab, &QPushButton::clicked, this, &Home::onFeederTabClicked);
    connect(btnConveyorTab, &QPushButton::clicked, this, &Home::onContainerTabClicked);

    leftLayout->addStretch();


    //mcp 설 정
    // AI 제어 버튼  (cmd 창 → ssh → q chat)
    // btnAICommand = new QPushButton("AI 제어");
    // btnAICommand->setFixedHeight(40);
    // leftLayout->addWidget(btnAICommand);

    //새 터미널에서 Amazon Q CLI 진입
    // connect(btnAICommand, &QPushButton::clicked, this, [this]() {  //  여기!
    // #ifdef Q_OS_WIN
    //     QProcess::startDetached("cmd.exe", {
    //                                            "/k",
    //                                            "ssh", "-tt",
    //                                            "-i", "C:\\Users\\white\\Downloads\\cornsoosoo.pem",
    //                                            "ec2-user@public_ip",
    //                                            "q", "chat"
    //                                        });
    // #else
    //         QProcess::startDetached(
    //             "gnome-terminal",
    //             { "--", "bash", "-c", "ssh -t amazon-q 'q chat'" }
    //             );
    // #endif
    //     });


}

void Home::setupMqttClient(){
    m_client = new QMqttClient(this);
    reconnectTimer = new QTimer(this);
    m_client->setHostname(mqttBroker); //브로커 서버에 연결 공용 mqtt 서버
    m_client->setPort(mqttPort);
    m_client->setClientId("VisionCraft_Home" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    connect(m_client, &QMqttClient::connected, this, &Home::onMqttConnected); // QMqttClient가 연결이 되었다면 mainwindow에 있는 저 함수중에 onMQTTCONNECTED를 실행
    connect(m_client, &QMqttClient::disconnected, this, &Home::onMqttDisConnected);
    //connect(m_client, &QMqttClient::messageReceived, this, &MainWindow::onMqttMessageReceived);
    connect(reconnectTimer, &QTimer::timeout, this, &Home::connectToMqttBroker);

}

void Home::updateFactoryStatus(bool running) {
    if(!btnFactoryToggle) {
        qDebug() << "btnFactoryToggle이 null입니다!";
        return;
    }
    if(running) {
        btnFactoryToggle->setText("Factory Stop");
        btnFactoryToggle->setChecked(true);
        qDebug() << "Home - 공장 가동 중 표시";
    } else {
        btnFactoryToggle->setText("Factory Start");
        btnFactoryToggle->setChecked(false);
        qDebug() << "Home - 공장 정지 중 표시";
    }
}

void Home::initializeFactoryToggleButton(){
    btnFactoryToggle = new QPushButton("공장 전체 on/off");
    btnFactoryToggle->setMinimumHeight(40);
    btnFactoryToggle->setCheckable(true);
    btnFactoryToggle->setChecked(factoryRunning);

    updateFactoryStatus(factoryRunning);
    connect(btnFactoryToggle, &QPushButton::clicked, this, &Home::onFactoryToggleClicked);

}

void Home::setupRightPanel(){
    // if(ui->label){
    //     ui->label->setText("실시간 오류 로그");
    //     ui->label->setStyleSheet("font-weight: bold; font-size: 14px;");
    // }

    if(ui->lineEdit){
        ui->lineEdit->setPlaceholderText("검색...");
    }

    if(ui->pushButton){
        ui->pushButton->setText("검색");
    }

    if(ui->listWidget){
        ui->listWidget->clear();
        ui->listWidget->setAlternatingRowColors(true);

    }

    connect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
}

// void Home::addErrorLogUI(const QJsonObject &errorData){
//     if(!ui->listWidget) return;
//     // 기기 이름 변환
//     QString deviceId = errorData["device_id"].toString();
//     QString deviceName = deviceId;

//     // 현재 시간
//     QString currentTime = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");

//     // 로그 텍스트 구성
//     QString logText = QString("[%1] %2 %3")
//                           .arg(currentTime)
//                           .arg(deviceName)
//                           .arg(errorData["log_code"].toString());


//     QListWidgetItem *item = new QListWidgetItem(logText);
//     item->setForeground(QBrush(Qt::black)); // 검은색 글자

//     // 맨 위에 새 항목 추가
//     ui->listWidget->insertItem(0, logText);

//     // 최대 20개 항목만 유지
//     if(ui->listWidget->count() > 50){
//         delete ui->listWidget->takeItem(50);
//     }

//     // 첫 번째 항목 선택해서 강조
//     ui->listWidget->setCurrentRow(0);
// }


void Home::addErrorLogUI(const QJsonObject &errorData){
    if(!ui->listWidget) return;

    QString deviceId = errorData["device_id"].toString();
    QString deviceName = deviceId;
    QString currentTime = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");
    QString logText = QString("[%1] %2 %3")
                          .arg(currentTime)
                          .arg(deviceName)
                          .arg(errorData["log_code"].toString());

    QListWidgetItem *item = new QListWidgetItem(logText);
    item->setForeground(QBrush(Qt::black));

    //error_log_id를 Qt::UserRole에 저장
    item->setData(Qt::UserRole, errorData["error_log_id"].toString());

    ui->listWidget->insertItem(0, item);

    if(ui->listWidget->count() > 50){
        delete ui->listWidget->takeItem(50);
    }

    ui->listWidget->setCurrentRow(0);
}


void Home::onMqttPublishRequested(const QString &topic, const QString &message) {
    if(m_client && m_client->state() == QMqttClient::Connected) {
        m_client->publish(QMqttTopicName(topic), message.toUtf8());
        qDebug() << " Home - MQTT 발송:" << topic << message;
    } else {
        qDebug() << " Home - MQTT 연결 안됨, 발송 실패:" << topic;
    }
}

void Home::controlALLDevices(bool start){
    if(m_client && m_client->state() == QMqttClient::Connected){
        QString command = start ? "on" : "off";

        m_client->publish(QMqttTopicName("feeder_01/cmd"), command.toUtf8());
        m_client->publish(QMqttTopicName("conveyor_01/cmd"), command.toUtf8());
        m_client->publish(QMqttTopicName("conveyor_02/cmd"), command.toUtf8());
        m_client->publish(QMqttTopicName("robot_arm_01/cmd"), command.toUtf8());


        qDebug() << "전체 기기 제어: " <<command;

    }
}

// 라즈베리 카메라 feeder
void Home::updateFeederImage(const QImage& image)
{
    // 영상 QLabel에 출력
    ui->cam1->setPixmap(QPixmap::fromImage(image).scaled(
        ui->cam1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 라즈베리 카메라 conveyor
void Home::updateConveyorImage(const QImage& image)
{
    // 영상 QLabel에 출력
    ui->cam2->setPixmap(QPixmap::fromImage(image).scaled(
        ui->cam2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 한화 카메라
void Home::updateHWImage(const QImage& image)
{
    ui->cam3->setPixmap(QPixmap::fromImage(image).scaled(
        ui->cam3->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}


void Home::onQueryResponseReceived(const QMqttMessage &message){
    QString messageStr = QString::fromUtf8(message.payload());
    qDebug() << "쿼리 응답 수신 : " << messageStr; //자동으로 호출이 됨

    QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    if(!doc.isObject()){
        qDebug() << "잘못된 JSON 응답";
        return;
    }

    QJsonObject response = doc.object();

    QString queryId = response["query_id"].toString(); //id가 맞으면 화면 표시하는 함수 호출
    qDebug() << "받은 queryId:" << queryId;        // ← 추가
    qDebug() << "현재 queryId:" << currentQueryId;  // ← 추가
    if(queryId != currentQueryId){
        qDebug() << "다른 쿼리 응답";
        return;

    }

    qDebug() << "processPastLogsResponse 호출 예정";
    processPastLogsResponse(response);
}

QString Home::generateQueryId(){ //고유한 id 만들어줌
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void Home::requestPastLogs(){
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << "MQTT 연결안됨";
        return;

    }

    currentQueryId = generateQueryId();

    QJsonObject queryRequest;
    queryRequest["query_id"] = currentQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();


    QJsonObject filters;
    filters["log_level"] = "error";
    filters["limit"] = 50;

    queryRequest["filters"] = filters;

    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "모든 과거 로그 요청 전송: " << payload;
    m_client->publish(mqttQueryRequestTopic, payload);

}

void Home::processPastLogsResponse(const QJsonObject &response){
    QString status = response["status"].toString();

    if(status != "success"){
        qDebug() << "에러";
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    int count = response["count"].toInt();
    qDebug() << "과거 로그" << count << "개 수신됨";

    if(ui->listWidget && count > 0) {
        ui->listWidget->clear();  // 검색 결과 표시 전에만 지우기
    }

    for(const QJsonValue &value : dataArray){
        QJsonObject logData = value.toObject();

        QString deviceId = logData["device_id"].toString();
        QString deviceName = deviceId;


        qint64 timestamp = 0;
        if(logData.contains("timestamp")) {
            QJsonValue timestampValue = logData["timestamp"];
            if(timestampValue.isDouble()) {
                timestamp = (qint64)timestampValue.toDouble();
            } else if(timestampValue.isString()) {
                timestamp = timestampValue.toString().toLongLong();
            } else {
                timestamp = timestampValue.toVariant().toLongLong();
            }
        }

        if(timestamp == 0) {
            timestamp = QDateTime::currentMSecsSinceEpoch();
        }

        // 완전한 로그 데이터 구성
        QJsonObject completeLogData = logData;
        completeLogData["timestamp"] = timestamp;

        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString logTime = dateTime.toString("MM-dd hh:mm");

        QString logText = QString("[%1] %2 %3")
                              .arg(logTime)
                              .arg(deviceName)
                              .arg(logData["log_code"].toString());

        if(ui->listWidget){
            ui->listWidget->addItem(logText);
        }else {
            qDebug() << "ui->listWidget이 null!";  // ← 추가
            QListWidgetItem *item = new QListWidgetItem(logText);
            // error_log_id를 Qt::UserRole에 저장
            item->setData(Qt::UserRole, logData["error_log_id"].toString());
            ui->listWidget->addItem(item);
        }

        addErrorLog(completeLogData);
        processErrorForChart(completeLogData);
    }

}

void Home::requestFilteredLogs(const QString &errorCode){
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << "MQTT 연결안됨";
        return;
    }

    currentQueryId = generateQueryId();

    //DB 서버로 보낼 JSON 요청
    QJsonObject queryRequest;
    queryRequest["query_id"] = currentQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();

    //검색 필터 설정
    QJsonObject filters;
    filters["log_level"] = ""; //" "이거를 "" 모든 레벨 문자열 받기
    filters["log_code"] = errorCode;
    filters["limit"] = 50;

    queryRequest["filters"] = filters;

    //JSON을 바이트 배열로 변경하고 MQTT로 전송하기
    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact); //공백이 없는 압축된 json형태

    qDebug() << "필터된 로그 요청: " << payload;
    m_client->publish(mqttQueryRequestTopic, payload);

}


void Home::onSearchClicked() {
    QString searchText = ui->lineEdit->text().trimmed();
    requestFilteredLogs(searchText);  // 필터된 로그
}

void Home::setupErrorChart(){
    chart = new QChart();
    chartView = new QChartView(chart); //차트를 화면에 보여주는것
    barSeries = new QBarSeries(); //막대 그래프

    feederBarSet = new QBarSet("피더");
    conveyorBarSet = new QBarSet("컨베이어");

    //초기 데이터 설정
    QStringList months = getLast6Months();
    for(int i = 0; i< months.size(); ++i){
        feederBarSet->append(0);
        conveyorBarSet->append(0);
    }

    barSeries->append(feederBarSet); //막대 세트를 시리즈에 묶음
    barSeries->append(conveyorBarSet);

    chart->addSeries(barSeries);
    chart->setTitle("월 별 오류 현황");
    chart->legend()->setVisible(true);
    chart->setBackgroundVisible(false);

    //x축 월
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(months);
    axisX->setGridLineVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    //오류 개수
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0,10);
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d");
    axisY->setGridLineVisible(false);
    chart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    chartView->setRenderHint(QPainter::Antialiasing);

    if(ui->chartWidget) {
        QVBoxLayout *layout = new QVBoxLayout(ui->chartWidget);
        layout->addWidget(chartView);
        ui->chartWidget->setLayout(layout);
    }

}

//지금부터 최근 6개월 월 라벨 생성
QStringList Home::getLast6Months(){
    QStringList months;
    QDateTime current = QDateTime::currentDateTime();

    for(int i = 5; i>=0; --i){
        QDateTime monthDate = current.addMonths(-i);
        months.append(monthDate.toString("MM월"));
    }
    return months;
}

void Home::processErrorForChart(const QJsonObject &errorData){
    QString deviceId = errorData["device_id"].toString();
    qint64 timestamp = errorData["timestamp"].toVariant().toLongLong();

    if(timestamp == 0){
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    QString monthKey = dateTime.toString("yyyy-MM");
    QString dayKey = dateTime.toString("yyyy-MM-dd");

    QString deviceType;
    if(deviceId.contains("feeder")){
        deviceType="feeder";
    }else if(deviceId.contains("conveyor")){
        deviceType="conveyor";
    } else {
        return;
    }

    if(!monthlyErrorDays[monthKey][deviceType].contains(dayKey)) {
        monthlyErrorDays[monthKey][deviceType].insert(dayKey);//해당 월의 해당 디바이스에서 그 날짜가 이미 기록되었는지 확인
        updateErrorChart();
    }

}

//차트의 막대 높이 업데이트
void Home::updateErrorChart(){
    if(!feederBarSet || !conveyorBarSet){
        return;
    }

    QStringList months = getLast6Months();

    feederBarSet->remove(0, feederBarSet->count());
    conveyorBarSet->remove(0, conveyorBarSet->count());

    for(const QString &month : months){
        QString monthKey = QDateTime::currentDateTime().addMonths(-(5-(months.indexOf(month)))).toString("yyyy-MM");
        int feederCount = monthlyErrorDays[monthKey]["feeder"].size();
        int conveyorCount = monthlyErrorDays[monthKey]["conveyor"].size();

        feederBarSet->append(feederCount);
        conveyorBarSet->append(conveyorCount);
        }
}

//db에 SHD 추가
void Home::sendFactoryStatusLog(const QString &logCode, const QString &message) {
    if(m_client && m_client->state() == QMqttClient::Connected) {
        QJsonObject logData;
        logData["log_code"] = logCode;
        logData["message"] = message;
        logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

        QJsonDocument doc(logData);
        QByteArray payload = doc.toJson(QJsonDocument::Compact);

        //factory/msg/status 토픽으로 전송(on/off)
        m_client->publish(QMqttTopicName("factory/msg/status"), payload);
        qDebug() << "Factory status log sent:" << logCode << message;
    }
}

void Home::onDeviceStatusChanged(const QString &deviceId, const QString &status) {
    //QString message = deviceId + " has " + status;
    sendFactoryStatusLog("SHD", deviceId);
}

void Home::on_listWidget_itemDoubleClicked(QListWidgetItem* item) {

    static bool isProcessing = false;
    if (isProcessing) return;
    isProcessing = true;


    QString errorLogId = item->data(Qt::UserRole).toString();
    QString logText = item->text();

    // 두 가지 로그 형식 지원: [MM:dd hh:mm:ss] 또는 [MM-dd hh:mm]
    QRegularExpression re1(R"(\[(\d{2}):(\d{2}) (\d{2}):(\d{2}):(\d{2})\] ([^ ]+))"); // 실시간 로그
    QRegularExpression re2(R"(\[(\d{2})-(\d{2}) (\d{2}):(\d{2})\] ([^ ]+))");        // 과거 로그

    QRegularExpressionMatch match1 = re1.match(logText);
    QRegularExpressionMatch match2 = re2.match(logText);

    QString month, day, hour, minute, second = "00", deviceId;

    if (match1.hasMatch()) {
        // 실시간 로그 형식: [MM:dd hh:mm:ss]
        month = match1.captured(1);
        day = match1.captured(2);
        hour = match1.captured(3);
        minute = match1.captured(4);
        second = match1.captured(5);
        deviceId = match1.captured(6);
    } else if (match2.hasMatch()) {
        // 과거 로그 형식: [MM-dd hh:mm]
        month = match2.captured(1);
        day = match2.captured(2);
        hour = match2.captured(3);
        minute = match2.captured(4);
        second = "00"; // 초는 00으로 설정
        deviceId = match2.captured(5);
    } else {
        QMessageBox::warning(this, "형식 오류", "로그 형식을 해석할 수 없습니다.\n로그: " + logText);
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

void Home::downloadAndPlayVideoFromUrl(const QString& httpUrl) {
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

//서버에서 영상 다운로드 후 VideoPlayer로 재생
void Home::downloadAndPlayVideo(const QString& filename) {
    QUrl url("http://mqtt.kwon.pics:8080/video/" + filename);
    downloadAndPlayVideoFromUrl(url.toString());
}


void Home::tryPlayVideo(const QString& originalUrl) {
    QString altUrl = originalUrl;
    altUrl.replace("video.kwon.pics:8081", "mqtt.kwon.pics:8080");
    altUrl.replace("localhost:8081", "mqtt.kwon.pics:8080");

    // 경로 구조가 다를 수 있으므로 파일명만 사용하는 URL도 시도
    QString fileName = originalUrl.split('/').last();
    QString simpleUrl = "http://mqtt.kwon.pics:8080/video/" + fileName;

    qDebug() << "시도할 URL 1:" << altUrl;
    qDebug() << "시도할 URL 2:" << simpleUrl;

    // 테스트용 - 실제 작동하는 영상 URL로 교체
    // QString testUrl = "https://sample-videos.com/zip/10/mp4/SampleVideo_1280x720_1mb.mp4";
    VideoPlayer* player = new VideoPlayer(simpleUrl, this);
    player->setAttribute(Qt::WA_DeleteOnClose);
    player->show();
}


void Home::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    if (aiButton) {
        int x = 20;  // 왼쪽 아래
        int y = height() - aiButton->height() - 20;
        aiButton->move(x, y);
    }
}
