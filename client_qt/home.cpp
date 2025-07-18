#include "home.h"
#include "mainwindow.h"
#include "conveyor.h"
#include "./ui_home.h"
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QDebug>


#include "videoplayer.h"
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QFile>
#include <QDesktopServices>
#include <QTimeZone>
#include "video_mqtt.h"
#include "video_client_functions.hpp"

Home::Home(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Home)
    , m_client(nullptr)
    , subscription(nullptr)
    , queryResponseSubscription(nullptr)
    , factoryRunning(false)
    , feederWindow(nullptr)
    , startDateEdit(nullptr)      // 추가
    , endDateEdit(nullptr)        // 추가
    , currentPage(0)              // 추가
    , pageSize(2000)               // 추가
    , isLoadingMoreLogs(false)    // 추가
    , conveyorWindow(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("기계 동작 감지 스마트팩토리 관제 시스템");

    setupNavigationPanel();
    setupRightPanel();
    setupErrorChart();
    setupMqttClient();
    connectToMqttBroker();

    connect(ui->listWidget, &QListWidget::itemDoubleClicked,
            this, &Home::on_listWidget_itemDoubleClicked);


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

        //  피더 로그 검색 시그널 연결 (추가)
        connect(mainWin, &MainWindow::requestFeederLogSearch,
                this, [this](const QString &errorCode, const QDate &startDate, const QDate &endDate) {
                    qDebug() << " MainWindow에서 피더 로그 검색 요청받음";
                    qDebug() << "  - 검색어:" << errorCode;
                    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
                    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

                    // 피더만 필터링해서 검색
                    this->requestFilteredLogs(errorCode, startDate, endDate, false);
                });

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
    connect(conveyorStatsSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
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

    //  검색 중일 때는 실시간 로그 무시
    if(isLoadingMoreLogs && topicStr.contains("/log/error")) {
        qDebug() << "🚫 검색 중이므로 실시간 로그 무시:" << topicStr;
        return;
    }

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
    qDebug() << "=== setupRightPanel 시작 ===";

    if(ui->lineEdit){
        ui->lineEdit->setPlaceholderText("검색어 입력...");
        qDebug() << "검색 입력창 설정 완료";
    }

    if(ui->pushButton){
        ui->pushButton->setText("검색");
        qDebug() << "검색 버튼 텍스트 설정 완료";
    }

    // 날짜 선택 위젯 추가
    QWidget* rightPanel = ui->rightPanel;
    if(rightPanel) {
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(rightPanel->layout());
        if(!layout) {
            layout = new QVBoxLayout(rightPanel);
            qDebug() << "새로운 레이아웃 생성";
        }

        // 날짜 필터 그룹 박스 생성
        QGroupBox* dateGroup = new QGroupBox("날짜 필터");
        QVBoxLayout* dateLayout = new QVBoxLayout(dateGroup);

        // 시작 날짜
        QHBoxLayout* startLayout = new QHBoxLayout();
        startLayout->addWidget(new QLabel("시작일:"));
        startDateEdit = new QDateEdit();
        startDateEdit->setDate(QDate::currentDate().addDays(-7)); // 기본: 일주일 전
        startDateEdit->setCalendarPopup(true);
        startDateEdit->setDisplayFormat("yyyy-MM-dd");
        startLayout->addWidget(startDateEdit);

        // 종료 날짜
        QHBoxLayout* endLayout = new QHBoxLayout();
        endLayout->addWidget(new QLabel("종료일:"));
        endDateEdit = new QDateEdit();
        endDateEdit->setDate(QDate::currentDate()); // 기본: 오늘
        endDateEdit->setCalendarPopup(true);
        endDateEdit->setDisplayFormat("yyyy-MM-dd");
        endLayout->addWidget(endDateEdit);

        dateLayout->addLayout(startLayout);
        dateLayout->addLayout(endLayout);

        //  초기화 버튼 기능 강화 - 날짜 초기화 + 최신 로그 다시 불러오기
        QPushButton* resetDateBtn = new QPushButton("전체 초기화 (최신순)");
        connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
            qDebug() << "🔄 전체 초기화 버튼 클릭됨";

            // 1. 날짜 초기화
            if(startDateEdit && endDateEdit) {
                startDateEdit->setDate(QDate::currentDate().addDays(-7));
                endDateEdit->setDate(QDate::currentDate());
                qDebug() << " 날짜 필터 초기화됨";
            }

            // 2. 검색어 초기화
            if(ui->lineEdit) {
                ui->lineEdit->clear();
                qDebug() << " 검색어 초기화됨";
            }

            // 3. 검색 조건 완전 초기화
            lastSearchErrorCode.clear();
            lastSearchStartDate = QDate();
            lastSearchEndDate = QDate();
            currentPage = 0;
            qDebug() << " 검색 조건 초기화됨";

            // 4. 최신 로그 다시 불러오기 (날짜 필터 없이)
            qDebug() << " 최신 로그 다시 불러오기 시작...";
            requestFilteredLogs("", QDate(), QDate(), false);  // 모든 조건 비우고 최신 로그
        });
        dateLayout->addWidget(resetDateBtn);

        // 레이아웃에 추가 (검색창 아래, 리스트 위에)
        int insertIndex = 2; // label(0), 검색위젯(1), 날짜그룹(2), 리스트(3)
        layout->insertWidget(insertIndex, dateGroup);

        qDebug() << "날짜 위젯 생성 완료";
        qDebug() << "startDateEdit 주소:" << startDateEdit;
        qDebug() << "endDateEdit 주소:" << endDateEdit;
    }

    if(ui->listWidget){
        ui->listWidget->clear();
        ui->listWidget->setAlternatingRowColors(true);
    }

    // 검색 버튼 연결 - 기존 연결 제거 후 새로 연결
    disconnect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    qDebug() << "검색 버튼 시그널 연결 완료";
    qDebug() << "=== setupRightPanel 완료 ===";
}



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
    qDebug() << " === 서버 응답 수신됨! ===";

    QString messageStr = QString::fromUtf8(message.payload());
    qDebug() << "응답 크기:" << messageStr.length() << "bytes";
    qDebug() << "응답 내용 (첫 500자):" << messageStr.left(500);

    QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    if(!doc.isObject()){
        qDebug() << " 잘못된 JSON 응답";
        return;
    }

    QJsonObject response = doc.object();
    QString status = response["status"].toString();
    qDebug() << "응답 상태:" << status;

    if(status == "success") {
        QJsonArray dataArray = response["data"].toArray();
        qDebug() << " 성공! 받은 로그 개수:" << dataArray.size();
    } else {
        qDebug() << " 실패:" << response["error"].toString();
    }

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
    filters["limit"] = 2000;    //  500개씩 나눠서 받기
    filters["offset"] = 0;     //  첫 페이지

    queryRequest["filters"] = filters;

    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "초기 로그 요청 (500개): " << payload;
    m_client->publish(mqttQueryRequestTopic, payload);

}

// void Home::processPastLogsResponse(const QJsonObject &response){
//     QString status = response["status"].toString();

//     if(status != "success"){
//         qDebug() << "에러";
//         return;
//     }

//     QJsonArray dataArray = response["data"].toArray();
//     int count = response["count"].toInt();
//     qDebug() << "과거 로그" << count << "개 수신됨";

//     if(ui->listWidget && count > 0) {
//         ui->listWidget->clear();  // 검색 결과 표시 전에만 지우기
//     }

//     for(const QJsonValue &value : dataArray){
//         QJsonObject logData = value.toObject();

//         QString deviceId = logData["device_id"].toString();
//         QString deviceName = deviceId;


//         qint64 timestamp = 0;
//         if(logData.contains("timestamp")) {
//             QJsonValue timestampValue = logData["timestamp"];
//             if(timestampValue.isDouble()) {
//                 timestamp = (qint64)timestampValue.toDouble();
//             } else if(timestampValue.isString()) {
//                 timestamp = timestampValue.toString().toLongLong();
//             } else {
//                 timestamp = timestampValue.toVariant().toLongLong();
//             }
//         }

//         if(timestamp == 0) {
//             timestamp = QDateTime::currentMSecsSinceEpoch();
//         }

//         // 완전한 로그 데이터 구성
//         QJsonObject completeLogData = logData;
//         completeLogData["timestamp"] = timestamp;

//         QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
//         QString logTime = dateTime.toString("MM-dd hh:mm");

//         QString logText = QString("[%1] %2 %3")
//                               .arg(logTime)
//                               .arg(deviceName)
//                               .arg(logData["log_code"].toString());

//         if(ui->listWidget){
//             ui->listWidget->addItem(logText);
//         }else {
//             qDebug() << "ui->listWidget이 null!";  // ← 추가
//             QListWidgetItem *item = new QListWidgetItem(logText);
//             // error_log_id를 Qt::UserRole에 저장
//             item->setData(Qt::UserRole, logData["error_log_id"].toString());
//             ui->listWidget->addItem(item);
//         }

//         addErrorLog(completeLogData);
//         processErrorForChart(completeLogData);
//     }

// }

// void Home::processPastLogsResponse(const QJsonObject &response){
//     QString status = response["status"].toString();

//     if(status != "success"){
//         qDebug() << "과거 로그 요청 실패:" << response["error"].toString();
//         return;
//     }

//     QJsonArray dataArray = response["data"].toArray();
//     int count = response["count"].toInt();
//     qDebug() << "=== 과거 로그 처리 시작 - 총" << count << "개 ===";

//     if(ui->listWidget && count > 0) {
//         ui->listWidget->clear();
//     }

//     int chartProcessedCount = 0;
//     QSet<QString> processedDates;

//     for(const QJsonValue &value : dataArray){
//         QJsonObject logData = value.toObject();

//         QString deviceId = logData["device_id"].toString();
//         QString deviceName = deviceId;

//         qint64 timestamp = 0;
//         if(logData.contains("timestamp")) {
//             QJsonValue timestampValue = logData["timestamp"];
//             if(timestampValue.isDouble()) {
//                 timestamp = (qint64)timestampValue.toDouble();
//             } else if(timestampValue.isString()) {
//                 timestamp = timestampValue.toString().toLongLong();
//             } else {
//                 timestamp = timestampValue.toVariant().toLongLong();
//             }
//         }

//         if(timestamp == 0) {
//             timestamp = QDateTime::currentMSecsSinceEpoch();
//         }

//         // 날짜별 분포 확인
//         QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
//         QString dateKey = dateTime.toString("yyyy-MM-dd");
//         processedDates.insert(dateKey);

//         // 완전한 로그 데이터 구성
//         QJsonObject completeLogData = logData;
//         completeLogData["timestamp"] = timestamp;

//         QString logTime = dateTime.toString("MM-dd hh:mm");
//         QString logText = QString("[%1] %2 %3")
//                               .arg(logTime)
//                               .arg(deviceName)
//                               .arg(logData["log_code"].toString());

//         if(ui->listWidget){
//             QListWidgetItem *item = new QListWidgetItem(logText);
//             item->setData(Qt::UserRole, logData["error_log_id"].toString());
//             ui->listWidget->addItem(item);
//         }

//         addErrorLog(completeLogData);

//         // 차트 처리 (핵심!)
//         processErrorForChart(completeLogData);
//         chartProcessedCount++;
//     }

//     qDebug() << "=== 과거 로그 처리 완료 ===";
//     qDebug() << "총 처리된 로그:" << count << "개";
//     qDebug() << "차트 처리된 로그:" << chartProcessedCount << "개";
//     qDebug() << "고유 날짜 수:" << processedDates.size() << "개";
//     qDebug() << "날짜 분포:" << processedDates.values();
// }

void Home::processPastLogsResponse(const QJsonObject &response) {
    isLoadingMoreLogs = false;  // 로딩 상태 해제

    qDebug() << "=== 📨 로그 응답 수신 ===";

    QString status = response["status"].toString();
    if(status != "success"){
        QString errorMsg = response["error"].toString();
        qDebug() << " 쿼리 실패:" << errorMsg;
        QMessageBox::warning(this, "조회 실패", "로그 조회에 실패했습니다: " + errorMsg);
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    bool isFirstPage = (currentPage == 0);

    // 날짜 검색인지 확인
    bool isDateSearch = (lastSearchStartDate.isValid() && lastSearchEndDate.isValid());

    qDebug() << " 로그 응답 상세:";
    qDebug() << "  - 받은 로그 수:" << dataArray.size();
    qDebug() << "  - 첫 페이지:" << isFirstPage;
    qDebug() << "  - 날짜 검색:" << isDateSearch;

    // 🔧 첫 페이지면 차트 초기화
    if(isFirstPage) {
        if(ui->listWidget) {
            ui->listWidget->clear();
        }

        // 차트 데이터 초기화
        monthlyErrorDays.clear();
        if(feederBarSet && conveyorBarSet) {
            QStringList months = getLast6Months();
            for(int i = 0; i < months.size(); ++i) {
                feederBarSet->replace(i, 0);
                conveyorBarSet->replace(i, 0);
            }
            qDebug() << "🔄 차트 데이터 초기화 완료";
        }
    }

    // 로그 데이터 처리
    for(const QJsonValue &value : dataArray){
        QJsonObject logData = value.toObject();

        // 날짜 필터링 (클라이언트 측 추가 검증)
        if(isDateSearch) {
            qint64 timestamp = 0;

            QJsonValue timestampValue = logData["timestamp"];
            if(timestampValue.isDouble()) {
                timestamp = static_cast<qint64>(timestampValue.toDouble());
            } else if(timestampValue.isString()) {
                bool ok;
                timestamp = timestampValue.toString().toLongLong(&ok);
                if(!ok) timestamp = 0;
            } else {
                timestamp = timestampValue.toVariant().toLongLong();
            }

            if(timestamp > 0) {
                QDateTime logTime = QDateTime::fromMSecsSinceEpoch(timestamp);
                QDate logDate = logTime.date();

                if(lastSearchStartDate.isValid() && logDate < lastSearchStartDate) {
                    continue;
                }
                if(lastSearchEndDate.isValid() && logDate > lastSearchEndDate) {
                    continue;
                }
            }
        }

        // UI 표시
        QString deviceId = logData["device_id"].toString();
        qint64 timestamp = 0;

        QJsonValue timestampValue = logData["timestamp"];
        if(timestampValue.isDouble()) {
            timestamp = static_cast<qint64>(timestampValue.toDouble());
        } else if(timestampValue.isString()) {
            bool ok;
            timestamp = timestampValue.toString().toLongLong(&ok);
            if(!ok) timestamp = QDateTime::currentMSecsSinceEpoch();
        } else {
            timestamp = timestampValue.toVariant().toLongLong();
        }

        if(timestamp == 0) {
            timestamp = QDateTime::currentMSecsSinceEpoch();
        }

        QJsonObject completeLogData = logData;
        completeLogData["timestamp"] = timestamp;

        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString logTime = dateTime.toString("MM-dd hh:mm");
        QString logText = QString("[%1] %2 %3")
                              .arg(logTime)
                              .arg(deviceId)
                              .arg(logData["log_code"].toString());

        if(ui->listWidget){
            QListWidgetItem *item = new QListWidgetItem(logText);
            item->setData(Qt::UserRole, logData["error_log_id"].toString());
            ui->listWidget->addItem(item);
        }

        addErrorLog(completeLogData);
        processErrorForChart(completeLogData);
    }

    //  더보기 버튼 호출 제거 - 사용자 요구사항
    // updateLoadMoreButton(hasMore);  ← 이 줄 제거

    updateErrorChart();

    qDebug() << " 로그 처리 완료:";
    qDebug() << "  - 처리된 로그:" << dataArray.size() << "개";
    qDebug() << "  - 총 리스트 아이템:" << (ui->listWidget ? ui->listWidget->count() : 0) << "개";
    qDebug() << " 더보기 버튼 없음 - 현재 결과만 표시";
}

void Home::updateLoadMoreButton(bool showButton) {
    //  더보기 버튼 완전 제거 - 사용자 요구사항
    qDebug() << " 더보기 버튼 제거됨 - 사용자 요구사항에 따라 사용 안함";

    // 기존 더보기 버튼이 있다면 완전히 제거
    static QPushButton* loadMoreBtn = nullptr;
    if(loadMoreBtn) {
        loadMoreBtn->setVisible(false);
        loadMoreBtn->deleteLater();
        loadMoreBtn = nullptr;
        qDebug() << " 기존 더보기 버튼 완전 삭제됨";
    }

    // 더이상 더보기 버튼을 생성하지 않음
    return;
}


// 🔧 requestFilteredLogs 함수 완전 수정 - 서버 JSON 구조에 맞춤
void Home::requestFilteredLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate, bool loadMore) {
    qDebug() << " requestFilteredLogs 호출됨! ";
    qDebug() << "매개변수 체크:";
    qDebug() << "  - errorCode:" << errorCode;
    qDebug() << "  - startDate:" << (startDate.isValid() ? startDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - endDate:" << (endDate.isValid() ? endDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - loadMore:" << loadMore;

    // MQTT 연결 상태 확인
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << " MQTT 연결 상태 오류!";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.");
        return;
    }

    // 더보기가 아닌 경우에만 검색 조건 저장
    if(!loadMore) {
        currentPage = 0;
        lastSearchErrorCode = errorCode;
        lastSearchStartDate = startDate;
        lastSearchEndDate = endDate;

        qDebug() << " 새 검색 - 조건 저장됨:";
        qDebug() << "  - errorCode:" << lastSearchErrorCode;
        qDebug() << "  - startDate:" << (lastSearchStartDate.isValid() ? lastSearchStartDate.toString("yyyy-MM-dd") : "무효");
        qDebug() << "  - endDate:" << (lastSearchEndDate.isValid() ? lastSearchEndDate.toString("yyyy-MM-dd") : "무효");

        if(ui->listWidget) {
            ui->listWidget->clear();
            qDebug() << " 기존 검색 결과 지움";
        }
    } else {
        currentPage++;
        qDebug() << "📄 더보기 - 저장된 조건 사용 (페이지:" << currentPage << ")";
    }

    // 로딩 상태 방지
    if(isLoadingMoreLogs) {
        qDebug() << "️ 이미 로딩 중입니다!";
        return;
    }
    isLoadingMoreLogs = true;

    // 쿼리 ID 생성
    currentQueryId = generateQueryId();
    qDebug() << " 쿼리 정보:";
    qDebug() << "  - 쿼리 ID:" << currentQueryId;
    qDebug() << "  - 페이지:" << currentPage;
    qDebug() << "  - 페이지 크기:" << pageSize;

    //  서버가 기대하는 JSON 구조로 변경
    QJsonObject queryRequest;
    queryRequest["query_id"] = currentQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();

    QJsonObject filters;

    // 더보기일 때 저장된 조건 사용
    QString useErrorCode = loadMore ? lastSearchErrorCode : errorCode;
    QDate useStartDate = loadMore ? lastSearchStartDate : startDate;
    QDate useEndDate = loadMore ? lastSearchEndDate : endDate;

    // 에러 코드 필터
    if(!useErrorCode.isEmpty()) {
        filters["log_code"] = useErrorCode;
        qDebug() << " 에러 코드 필터:" << useErrorCode;
    }

    //  핵심: time_range 객체 사용 (서버가 기대하는 구조)
    if(useStartDate.isValid() && useEndDate.isValid()) {
        qDebug() << " 날짜 검색 모드 - 모든 데이터 한번에 가져오기";

        // 안전한 날짜 변환
        QDateTime startDateTime;
        startDateTime.setDate(useStartDate);
        startDateTime.setTime(QTime(0, 0, 0, 0));
        startDateTime.setTimeSpec(Qt::LocalTime);
        qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();

        QDateTime endDateTime;
        endDateTime.setDate(useEndDate);
        endDateTime.setTime(QTime(23, 59, 59, 999));
        endDateTime.setTimeSpec(Qt::LocalTime);
        qint64 endTimestamp = endDateTime.toMSecsSinceEpoch();

        //  서버가 기대하는 time_range 구조
        QJsonObject timeRange;
        timeRange["start"] = startTimestamp;
        timeRange["end"] = endTimestamp;
        filters["time_range"] = timeRange;

        // 날짜 검색에서는 큰 limit으로 모든 데이터 가져오기
        filters["limit"] = 10000;  // 충분히 큰 값

        qDebug() << " time_range 필터 설정:";
        qDebug() << "  - 시작:" << useStartDate.toString("yyyy-MM-dd") << "→" << startTimestamp;
        qDebug() << "  - 종료:" << useEndDate.toString("yyyy-MM-dd") << "→" << endTimestamp;
        qDebug() << "  - limit:" << 10000;

    } else {
        qDebug() << " 일반 최신 로그 모드 - 페이지네이션 사용";

        // 일반 검색에서는 페이지네이션 적용
        filters["limit"] = pageSize;
        filters["offset"] = currentPage * pageSize;

        qDebug() << " 일반 필터 설정:";
        qDebug() << "  - limit:" << pageSize;
        qDebug() << "  - offset:" << (currentPage * pageSize);
    }

    queryRequest["filters"] = filters;

    // JSON 문서 생성 및 전송
    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "=== MQTT 전송 시도 ===";
    qDebug() << "토픽:" << mqttQueryRequestTopic;
    qDebug() << "페이로드 크기:" << payload.size() << "bytes";
    qDebug() << "클라이언트 상태:" << m_client->state();

    //  서버 기대 구조와 비교 출력
    qDebug() << "전송할 JSON (서버 기대 구조):";
    qDebug() << doc.toJson(QJsonDocument::Indented);

    // JSON 필드 타입 검증
    qDebug() << "=== JSON 필드 타입 검증 ===";
    QJsonObject debugFilters = filters;
    for(auto it = debugFilters.begin(); it != debugFilters.end(); ++it) {
        QJsonValue value = it.value();
        QString key = it.key();

        if(value.isString()) {
            qDebug() << key << ": (문자열)" << value.toString();
        } else if(value.isDouble()) {
            qDebug() << key << ": (숫자)" << value.toDouble();
        } else if(value.isObject()) {
            qDebug() << key << ": (객체)" << QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        } else {
            qDebug() << key << ": (기타)" << value.toVariant();
        }
    }

    // 타임아웃 설정
    QTimer::singleShot(30000, this, [this]() {
        if(isLoadingMoreLogs) {
            isLoadingMoreLogs = false;
            qDebug() << " 검색 타임아웃!";
            QMessageBox::warning(this, "타임아웃", "로그 요청 시간이 초과되었습니다.");
        }
    });

    // 🔧 MQTT 전송 (false 무시)
    qDebug() << "📡 MQTT publish 시도...";
    qDebug() << "  - 클라이언트 ID:" << m_client->clientId();
    qDebug() << "  - 호스트:" << m_client->hostname() << ":" << m_client->port();

    bool result = m_client->publish(mqttQueryRequestTopic, payload);
    qDebug() << "MQTT 전송 결과:" << (result ? " 성공" : "️ 비동기 (정상)");

    //  false여도 실제로는 전송되므로 에러 처리 제거

    qDebug() << " MQTT 전송 완료! 응답 대기 중...";
}

void Home::onSearchClicked() {
    qDebug() << " 검색 버튼 클릭됨!!!! ";
    qDebug() << "함수 시작 - 현재 시간:" << QDateTime::currentDateTime().toString();

    if(!ui->lineEdit) {
        qDebug() << " lineEdit null!";
        return;
    }

    QString searchText = ui->lineEdit->text().trimmed();
    qDebug() << " 검색어:" << searchText;

    // 날짜 위젯 존재 확인
    if(!startDateEdit || !endDateEdit) {
        qDebug() << " 날짜 위젯이 null입니다!";
        qDebug() << "startDateEdit:" << startDateEdit;
        qDebug() << "endDateEdit:" << endDateEdit;
        QMessageBox::warning(this, "UI 오류", "날짜 선택 위젯이 초기화되지 않았습니다.");
        return;
    }

    QDate startDate = startDateEdit->date();
    QDate endDate = endDateEdit->date();

    qDebug() << " 검색 파라미터:";
    qDebug() << "  - 검색어:" << searchText;
    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

    // MQTT 연결 확인
    qDebug() << "MQTT 상태 확인:";
    qDebug() << "  - m_client 존재:" << (m_client != nullptr);
    if(m_client) {
        qDebug() << "  - 연결 상태:" << m_client->state();
        qDebug() << "  - Connected 값:" << QMqttClient::Connected;
        qDebug() << "  - 호스트:" << m_client->hostname();
        qDebug() << "  - 포트:" << m_client->port();
    }

    if(!m_client || m_client->state() != QMqttClient::Connected) {
        qDebug() << " MQTT 연결 안됨!";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.");
        return;
    }

    qDebug() << " MQTT 연결 OK - 검색 요청 전송...";
    requestFilteredLogs(searchText, startDate, endDate, false);
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

    //qDebug() << "=== 차트 데이터 처리 시작 ===";
    //qDebug() << "디바이스:" << deviceId << "타임스탬프:" << timestamp;

    if(timestamp == 0){
        timestamp = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "타임스탬프가 0이므로 현재시간 사용:" << timestamp;
    }

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    QString monthKey = dateTime.toString("yyyy-MM");
    QString dayKey = dateTime.toString("yyyy-MM-dd");

    //qDebug() << "날짜 변환 - 월키:" << monthKey << "일키:" << dayKey;
    //qDebug() << "전체 날짜:" << dateTime.toString("yyyy-MM-dd hh:mm:ss");

    QString deviceType;
    if(deviceId.contains("feeder")){
        deviceType="feeder";
    }else if(deviceId.contains("conveyor")){
        deviceType="conveyor";
    } else {
        qDebug() << "알 수 없는 디바이스 타입:" << deviceId << "- 차트 처리 건너뜀";
        return;
    }

    // if(!monthlyErrorDays[monthKey][deviceType].contains(dayKey)) {
    //     monthlyErrorDays[monthKey][deviceType].insert(dayKey);//해당 월의 해당 디바이스에서 그 날짜가 이미 기록되었는지 확인
    //     updateErrorChart();
    // }
    //qDebug() << "디바이스 타입:" << deviceType;

    // 현재 해당 월의 에러 일수 확인
    int beforeCount = monthlyErrorDays[monthKey][deviceType].size();
    //qDebug() << "추가 전" << monthKey << deviceType << "에러 일수:" << beforeCount;

    // 현재 저장된 모든 월별 데이터 출력
    //qDebug() << "=== 현재 저장된 모든 월별 데이터 ===";
    for(auto monthIt = monthlyErrorDays.begin(); monthIt != monthlyErrorDays.end(); ++monthIt) {
        QString month = monthIt.key();
        for(auto deviceIt = monthIt.value().begin(); deviceIt != monthIt.value().end(); ++deviceIt) {
            QString device = deviceIt.key();
            int count = deviceIt.value().size();
            //qDebug() << "월:" << month << "디바이스:" << device << "에러일수:" << count;
        }
    }

    if(!monthlyErrorDays[monthKey][deviceType].contains(dayKey)) {
        monthlyErrorDays[monthKey][deviceType].insert(dayKey);
        int afterCount = monthlyErrorDays[monthKey][deviceType].size();
        //qDebug() << "새로운 에러 날짜 추가!" << dayKey;
        //qDebug() << "추가 후" << monthKey << deviceType << "에러 일수:" << afterCount;

        updateErrorChart();
    } else {
        //qDebug() << "이미 존재하는 날짜:" << dayKey << "- 차트 업데이트 없음";
    }

}

//차트의 막대 높이 업데이트
void Home::updateErrorChart(){
    if(!feederBarSet || !conveyorBarSet){
        //qDebug() << "차트 바셋이 null입니다!";
        return;
    }

    //qDebug() << "=== 차트 업데이트 시작 ===";

    QStringList months = getLast6Months();
    //qDebug() << "최근 6개월:" << months;

    feederBarSet->remove(0, feederBarSet->count());
    conveyorBarSet->remove(0, conveyorBarSet->count());

    int maxValue = 0;
    QDateTime current = QDateTime::currentDateTime();

    for(int i = 0; i < months.size(); ++i){
        QDateTime monthDate = current.addMonths(-5 + i);
        QString monthKey = monthDate.toString("yyyy-MM");

        int feederCount = monthlyErrorDays[monthKey]["feeder"].size();
        int conveyorCount = monthlyErrorDays[monthKey]["conveyor"].size();

        //qDebug() << "인덱스:" << i << "월:" << months[i] << "키:" << monthKey
        //         << "피더:" << feederCount << "컨베이어:" << conveyorCount;

        feederBarSet->append(feederCount);
        conveyorBarSet->append(conveyorCount);

        maxValue = qMax(maxValue, qMax(feederCount, conveyorCount));
    }

    //qDebug() << "최대값:" << maxValue;

    // Y축 범위 동적 조정
    auto axes = chart->axes(Qt::Vertical);
    if(!axes.isEmpty()) {
        QValueAxis* yAxis = qobject_cast<QValueAxis*>(axes.first());
        if(yAxis) {
            int currentMax = yAxis->max();
            int newMax = qMax(10, maxValue + 2);

        //    qDebug() << "Y축 현재 최대값:" << currentMax << "새 최대값:" << newMax;

            if(currentMax != newMax) {
                yAxis->setRange(0, newMax);
        //        qDebug() << "Y축 범위 업데이트됨: 0 -" << newMax;
            }
        }
    }

    // 강제 차트 업데이트
    chart->update();
    if(chartView) {
        chartView->update();
        chartView->repaint();
    }

    //qDebug() << "=== 차트 업데이트 완료 ===";
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
                            static bool isProcessing = false;
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


