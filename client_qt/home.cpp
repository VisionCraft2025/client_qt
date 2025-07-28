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
#include <QTimeZone>
#include "video_mqtt.h"
#include "video_client_functions.hpp"

//mcp
#include <QProcess>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>

#include <QMouseEvent>

#include "cardevent.h"
#include "cardhovereffect.h"


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
    , isDateSearchMode(false)          // 👈 마지막에 추가
    , currentSearchStartDate(QDate())  // 👈 마지막에 추가
    , currentSearchEndDate(QDate())    // 👈 마지막에 추가
{

    ui->setupUi(this);
    setWindowTitle("기계 동작 감지 스마트팩토리 관제 시스템");

    m_errorChartManager = new ErrorChartManager(this);
    if(ui->chartWidget) {
        QVBoxLayout *layout = new QVBoxLayout(ui->chartWidget);
        layout->addWidget(m_errorChartManager->getChartView());
        ui->chartWidget->setLayout(layout);
    }

    //setupNavigationPanel();

    setupRightPanel();
    //m_errorChartManager = new ErrorChartManager(this);
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

        connect(mainWin, &MainWindow::requestFeederLogSearch,
                this, [this, mainWin](const QString &errorCode, const QDate &startDate, const QDate &endDate) {
                    qDebug() << " MainWindow에서 피더 로그 검색 요청받음";
                    qDebug() << "  - 검색어:" << errorCode;
                    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
                    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

                    //  현재 피더 윈도우 저장
                    currentFeederWindow = mainWin;

                    // 기존 함수 그대로 사용
                    this->requestFilteredLogs(errorCode, startDate, endDate, false);
                });

        qDebug() << " Home - MainWindow 시그널 연결 완료";
    }

    if(auto* conveyorWin = qobject_cast<ConveyorWindow*>(childWindow)) {
        // ConveyorWindow 연결
        connect(conveyorWin, &ConveyorWindow::errorLogGenerated, this, &Home::onErrorLogGenerated);
        connect(conveyorWin, &ConveyorWindow::requestErrorLogs, this, &Home::onErrorLogsRequested);
        connect(this, &Home::errorLogsResponse, conveyorWin, &ConveyorWindow::onErrorLogsReceived);
        connect(this, &Home::newErrorLogBroadcast, conveyorWin, &ConveyorWindow::onErrorLogBroadcast);
        connect(conveyorWin, &ConveyorWindow::deviceStatusChanged, this, &Home::onDeviceStatusChanged);
        connect(this, &Home::deviceStatsReceived, conveyorWin, &ConveyorWindow::onDeviceStatsReceived);
        connect(conveyorWin, &ConveyorWindow::requestConveyorLogSearch, this, &Home::handleConveyorLogSearch);

        connect(conveyorWin, &ConveyorWindow::requestConveyorLogSearch,
                this, [this, conveyorWin](const QString &errorCode, const QDate &startDate, const QDate &endDate) {
                    qDebug() << " ConveyorWindow에서 컨베이어 로그 검색 요청받음";
                    qDebug() << "  - 검색어:" << errorCode;
                    qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
                    qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

                    //  현재 컨베이어 윈도우 저장
                    currentConveyorWindow = conveyorWin;

                    //  컨베이어 전용 검색 함수 호출
                    this->handleConveyorLogSearch(errorCode, startDate, endDate);
                });

        qDebug() << " Home - ConveyorWindow 시그널 연결 완료";
    }

}

void Home::requestStatisticsToday(const QString& deviceId) {
    if(m_client && m_client->state() == QMqttClient::Connected) {
        QJsonObject request;
        request["device_id"] = deviceId;

        QJsonObject timeRange;
        QDateTime now = QDateTime::currentDateTime();
        QDateTime startOfDay = QDateTime(now.date(), QTime(0, 0, 0));
        timeRange["start"] = startOfDay.toMSecsSinceEpoch();
        timeRange["end"] = now.toMSecsSinceEpoch();
        request["time_range"] = timeRange;

        QJsonDocument doc(request);
        m_client->publish(QMqttTopicName("factory/statistics"), doc.toJson(QJsonDocument::Compact));
        qDebug() << deviceId << " 오늘 하루치 통계 요청! (time_range 포함)";
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

    //requestStatisticsToday("feeder_01");

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
        // 모든 피더 디바이스 로그 가져오기
        QList<QJsonObject> feederLogs;
        for(const QJsonObject &log : errorLogHistory) {
            QString deviceId = log["device_id"].toString();
            if(deviceId.startsWith("feeder_")) {  // feeder_01, feeder_02 모두
                feederLogs.append(log);
            }
        }
        qDebug() << "Home - 피더 탭에 피더 로그" << feederLogs.size() << "개 전달";

        if(feederWindow) {
            feederWindow->onErrorLogsReceived(feederLogs);
        }
    });
}

void Home::onContainerTabClicked(){
    this->hide();

    //requestStatisticsToday("conveyor_01");
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
        // 모든 컨베이어 디바이스 로그 가져오기
        QList<QJsonObject> conveyorLogs;
        for(const QJsonObject &log : errorLogHistory) {
            QString deviceId = log["device_id"].toString();
            if(deviceId.startsWith("conveyor_")) {  // conveyor_01, conveyor_03 모두
                conveyorLogs.append(log);
            }
        }
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
        qDebug() << " Home - feeder_02/status 구독됨";
    }

    auto feederSubscription2  = m_client->subscribe(QString("feeder_02/status"));
    if(feederSubscription2){
        connect(feederSubscription2, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
        qDebug() << " Home - feeder_01/status 구독됨";
    }

    auto conveyorSubscription = m_client->subscribe(QString("conveyor_01/status"));
    if(conveyorSubscription){
        connect(conveyorSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
        qDebug() << " Home - conveyor_03/status 구독됨";
    }

    auto conveyorSubscription3 = m_client->subscribe(QString("conveyor_02/status"));
    if(conveyorSubscription3){
        connect(conveyorSubscription3, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
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

    // INF 메시지를 받기 위한 info 토픽 구독 추가
    auto infoSubscription = m_client->subscribe(QString("factory/+/log/info"));
    connect(infoSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);
    qDebug() << " Home - factory/+/log/info 구독됨";

    //기기 상태
    //auto feederStatsSubscription = m_client->subscribe(QString("factory/feeder_01/msg/statistics"));
    //connect(feederStatsSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);

    //auto conveyorStatsSubscription = m_client->subscribe(QString("factory/conveyor_01/msg/statistics"));
    //connect(conveyorStatsSubscription, &QMqttSubscription::messageReceived, this, &Home::onMqttMessageReceived);

    QTimer::singleShot(1000, this, &Home::requestPastLogs); //MQTT 연결이 완전히 안정된 후 1초 뒤에 과거 로그를 자동으로 요청
    QTimer::singleShot(3000, [this](){
        requestStatisticsToday("feeder_01");
        requestStatisticsToday("conveyor_01");
    });

    QTimer::singleShot(1000, this, &Home::requestPastLogs);    // UI용 (2000개)
    QTimer::singleShot(2000, this, &Home::loadAllChartData);   // 차트용 (전체)
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

    //  검색 중일 때는 실시간 로그 무시 수정햄
    // if(isLoadingMoreLogs && topicStr.contains("/log/error")) {
    //     qDebug() << " 검색 중이므로 실시간 로그 무시:" << topicStr;
    //     return;
    // }
    //  검색 중이거나 날짜 검색 모드일 때는 실시간 로그 무시
    if((isLoadingMoreLogs || isDateSearchMode) && (topicStr.contains("/log/error") || topicStr.contains("/log/info"))) {
        qDebug() << "🚫 검색 중이거나 날짜 검색 모드이므로 실시간 로그 무시:" << topicStr;
        qDebug() << "  - isLoadingMoreLogs:" << isLoadingMoreLogs;
        qDebug() << "  - isDateSearchMode:" << isDateSearchMode;
        return;  // ✅ 여기서 완전히 차단
    }

    //db 로그 받기
    // if(topicStr.contains("/log/error")){
    //     QStringList parts = topicStr.split('/');
    //     QString deviceId = parts[1];

    //     QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    //     QJsonObject errorData = doc.object();
    //     errorData["device_id"] = deviceId;

    //     qDebug() << " 실시간 에러 로그 수신:" << deviceId;
    //     qDebug() << "에러 데이터:" << errorData;

    //     onErrorLogGenerated(errorData);
    //     m_errorChartManager->processErrorData(errorData);
    //     qDebug() << " 실시간 데이터를 차트 매니저로 전달함";        addErrorLog(errorData);  // 부모가 직접 처리

    //     addErrorLog(errorData);
    //     emit newErrorLogBroadcast(errorData);

    //     return;
    // }

    //db 로그 받기 (error와 info 모두 처리)
    //db 로그 받기 (error와 info 모두 처리)
    if(topicStr.contains("/log/error") || topicStr.contains("/log/info")){
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1];

        QJsonDocument doc = QJsonDocument::fromJson(message.payload());
        QJsonObject logData = doc.object();
        logData["device_id"] = deviceId;

        QString logCode = logData["log_code"].toString();

        qDebug() << " 실시간 로그 수신:" << deviceId << "log_code:" << logCode;

        if(isDateSearchMode) {
            qDebug() << "🚫 날짜 검색 모드이므로 실시간 로그를 히스토리에 추가하지 않음";
            return;
        }

        // 상태가 바뀔 때만 UI 업데이트
        if(lastDeviceStatus[deviceId] != logCode) {
            lastDeviceStatus[deviceId] = logCode;

            qDebug() << deviceId << "상태 변경:" << logCode;

            // INF(정상)일 때와 ERROR일 때 구분 처리
            if(logCode == "INF") {
                // 정상 상태 처리
                qDebug() << " 정상 상태 감지:" << deviceId;
                emit newErrorLogBroadcast(logData);  // 자식 윈도우에 정상 상태 전달
            } else {
                // 에러 상태 처리 (기존 로직)
                qDebug() << " 에러 로그 수신:" << deviceId;
                onErrorLogGenerated(logData);
                m_errorChartManager->processErrorData(logData);
                addErrorLog(logData);
                emit newErrorLogBroadcast(logData);
            }
        } else {
            qDebug() << deviceId << "상태 유지:" << logCode << "(UI 업데이트 스킵)";
        }

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
    else if(topicStr == "feeder_02/status"){
        if(messageStr == "on" || messageStr == "off"){
            qDebug() << "Home - 피더_01 on/off 처리";
            // 기존 on/off 처리 코드 유지
            if(messageStr == "on"){
                qDebug() << "Home - 피더 정방향 시작";
            } else if(messageStr == "off"){
                qDebug() << "Home - 피더 정지됨";
            }
        }
        // 나머지 명령은 무시
    }
    else if(topicStr == "feeder_01/status"){
        if(messageStr != "on" && messageStr != "off"){
            qDebug() << "Home - 피더_02 기타 명령 처리";
            // reverse, speed 등 기타 명령 처리 (필요시 기존 코드 복사)
            if(messageStr == "reverse"){
                qDebug() << "Home - 피더 역방향 시작";
            } else if(messageStr.startsWith("SPEED_") || messageStr.startsWith("MOTOR_")){
                qDebug() << "Home - 피더 오류 감지:" << messageStr;
            }
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
    else if(topicStr == "conveyor_03/status"){
        if(messageStr == "on" || messageStr == "off"){
            qDebug() << "Home - 컨베이어_01 on/off 처리";
            // 기존 on/off 처리 코드 유지
            if(messageStr == "on"){
                qDebug() << "Home - 컨베이어 정방향 시작";
            } else if(messageStr == "off"){
                qDebug() << "Home - 컨베이어 정지됨";
            }
        }
        // 나머지 명령은 무시
    }
    else if(topicStr == "conveyor_02/status"){
        if(messageStr != "on" && messageStr != "off"){
            qDebug() << "Home - 컨베이어_02 기타 명령 처리";
            // error_mode, speed 등 기타 명령 처리 (필요시 기존 코드 복사)
            if(messageStr == "error_mode"){
                qDebug() << "Home - 컨베이어 속도";
            } else if(messageStr.startsWith("SPEED_")){
                qDebug() << "Home - 컨베이어 오류 감지:" << messageStr;
            }
        }
    }
    else if(topicStr == "conveyor_01/status"){
        if(messageStr != "on" && messageStr != "off"){
            qDebug() << "Home - 컨베이어_03 기타 명령 처리";
            // error_mode, speed 등 기타 명령 처리 (필요시 기존 코드 복사)
            if(messageStr == "error_mode"){
                qDebug() << "Home - 컨베이어 속도";
            } else if(messageStr.startsWith("SPEED_")){
                qDebug() << "Home - 컨베이어 오류 감지:" << messageStr;
            }
        }
    }
    else if(topicStr.contains("/msg/statistics")) {
        QStringList parts = topicStr.split('/');
        QString deviceId = parts[1]; // feeder_02 또는 conveyor_03

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

    // ERROR LOG 라벨 추가
    static QLabel* errorLogLabel = nullptr;
    QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    if (!rightLayout) {
        rightLayout = new QVBoxLayout(ui->rightPanel);
        ui->rightPanel->setLayout(rightLayout);
    }
    if (!errorLogLabel) {
        errorLogLabel = new QLabel("ERROR LOG");
        errorLogLabel->setStyleSheet(R"(
            color: #FF6900;
            font-weight: bold;
            font-size: 15px;
            margin-top: 8px;
            margin-bottom: 12px;
            margin-left: 2px;
            padding-left: 2px;
            text-align: left;
        )");
        // ERROR LOG 라벨 항상 맨 위에
        if (errorLogLabel) {
            rightLayout->removeWidget(errorLogLabel);
        }
        rightLayout->insertWidget(0, errorLogLabel);

        // 기존 spacing 제거 (중복 방지)
        if (rightLayout->itemAt(1) && rightLayout->itemAt(1)->spacerItem()) {
            rightLayout->removeItem(rightLayout->itemAt(1));
        }
        // 간격 추가
        rightLayout->insertSpacing(1, 16);
    }

    // 검색창 디자인 개선
    ui->lineEdit->setPlaceholderText("검색어 입력...");
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
    disconnect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);

    // 검색창 커스텀 박스 추가
    QWidget* searchContainer = new QWidget();
    QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);

    // 기존 위젯을 삭제
    if (ui->lineEdit) {
        ui->lineEdit->deleteLater();
        ui->lineEdit = nullptr;
    }
    if (ui->pushButton) {
        ui->pushButton->deleteLater();
        ui->pushButton = nullptr;
    }

    ui->lineEdit = new QLineEdit();
    ui->lineEdit->setPlaceholderText("검색어 입력...");
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

    ui->pushButton = new QPushButton("검색");
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

    searchLayout->addWidget(ui->lineEdit);
    searchLayout->addWidget(ui->pushButton);

    // 기존 검색창 위치
    //QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    if (!rightLayout) {
        rightLayout = new QVBoxLayout(ui->rightPanel);
        ui->rightPanel->setLayout(rightLayout);
    }

    rightLayout->insertWidget(1, searchContainer);
    disconnect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);


    // 날짜 선택 위젯 추가
    QWidget* rightPanel = ui->rightPanel;
    if(rightPanel) {
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(rightPanel->layout());
        if(!layout) {
            layout = new QVBoxLayout(rightPanel);
            qDebug() << "새로운 레이아웃 생성";
        }

        // 날짜 필터 그룹 박스 생성
        QGroupBox* dateGroup = new QGroupBox();
        QVBoxLayout* dateLayout = new QVBoxLayout(dateGroup);

        QLabel* filterTitle = new QLabel("날짜 필터");
        filterTitle->setStyleSheet("color: #374151; font-weight: bold; font-size: 15px; background: transparent;");
        dateLayout->addWidget(filterTitle);  // 상단에 직접 추가

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
        QDateEdit::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 25px;
            border-left-width: 1px;
            border-left-color: #d1d5db;
            border-left-style: solid;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #fb923c, stop:1 #f97316);
        }
        QDateEdit::drop-down:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f97316, stop:1 #ea580c);
        }
        QDateEdit::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-top: 8px solid white;
            margin-top: 2px;
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
        startDateEdit = new QDateEdit(QDate::currentDate());
        startDateEdit->setCalendarPopup(true);
        startDateEdit->setDisplayFormat("yyyy-MM-dd");
        startDateEdit->setStyleSheet(dateEditStyle);
        startDateEdit->setFixedWidth(90);
        startCol->addWidget(startLabel);
        startCol->addWidget(startDateEdit);

        // 종료일
        QVBoxLayout* endCol = new QVBoxLayout();
        QLabel* endLabel = new QLabel("종료일:");
        endLabel->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
        endDateEdit = new QDateEdit(QDate::currentDate());
        endDateEdit->setCalendarPopup(true);
        endDateEdit->setDisplayFormat("yyyy-MM-dd");
        endDateEdit->setStyleSheet(dateEditStyle);
        endDateEdit->setFixedWidth(90);
        endCol->addWidget(endLabel);
        endCol->addWidget(endDateEdit);


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

        // 수평 정렬: 시작 + 종료 + 버튼
        QHBoxLayout* inputRow = new QHBoxLayout();
        inputRow->addLayout(startCol);
        inputRow->addLayout(endCol);
        inputRow->addWidget(applyButton);
        // 버튼을 아래로 정렬
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
        dateLayout->addSpacing(3); // 날짜 필터, 카드 사이 간격
        dateLayout->addWidget(resetDateBtn);

        // 삽입
        layout->insertWidget(2, dateGroup);

        // 시그널 연결
        connect(applyButton, &QPushButton::clicked, this, [this]() {
            QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";
            QDate start = startDateEdit ? startDateEdit->date() : QDate();
            QDate end = endDateEdit ? endDateEdit->date() : QDate();
            requestFilteredLogs(searchText, start, end, false);
        });

        connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
            if(startDateEdit && endDateEdit) {
                startDateEdit->setDate(QDate::currentDate());
                endDateEdit->setDate(QDate::currentDate());
            }
            if(ui->lineEdit) ui->lineEdit->clear();
            lastSearchErrorCode.clear();
            lastSearchStartDate = QDate();
            lastSearchEndDate = QDate();
            currentPage = 0;
            requestFilteredLogs("", QDate(), QDate(), false);
        });

        qDebug() << "날짜 필터 구성 완료";
    }

    // scrollArea 설정
    if (ui->scrollArea) {
        QWidget* content = new QWidget();
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QVBoxLayout* layout = new QVBoxLayout(content);
        layout->setSpacing(6);
        layout->setContentsMargins(4, 2, 4, 4);
        layout->addStretch();
        ui->scrollArea->setWidget(content);
        ui->scrollArea->setWidgetResizable(true);
    }

    disconnect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &Home::onSearchClicked);
    qDebug() << "=== setupRightPanel 완료 ===";

    // 검색창을 ERROR LOG 아래에 배치
    // lineEdit, pushButton을 담을 컨테이너 생성
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(ui->lineEdit);
    searchLayout->addWidget(ui->pushButton);

    // 이미 레이아웃에 있던 경우 제거
    rightLayout->removeWidget(ui->lineEdit);
    rightLayout->removeWidget(ui->pushButton);

    // ERROR LOG 라벨 바로 아래(두 번째)에 삽입
    rightLayout->insertWidget(1, searchContainer);

    qDebug() << "=== setupRightPanel 완료 ===";

    // 🎯 더미 모드 버튼 강제 추가 (테스트)
    //qDebug() << "더미 모드 버튼 강제 생성 시작";

    ////QPushButton* testDummyBtn = new QPushButton("더미모드테스트");
    //testDummyBtn->setFixedSize(100, 30);
    ////testDummyBtn->setStyleSheet("background-color: red; color: white;");

    //QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    // if(rightLayout) {
    //     rightLayout->addWidget(testDummyBtn);
    //     qDebug() << "더미 모드 테스트 버튼 추가 완료!";
    // } else {
    //     qDebug() << "rightLayout이 null입니다!";
    // }

    // // 버튼 클릭 시 더미 데이터 로드
    // connect(testDummyBtn, &QPushButton::clicked, this, [this]() {
    //     qDebug() << "테스트 버튼 클릭됨!";
    //     clearAllErrorLogsFromUI();
    //     loadDummyErrorData();
    //     qDebug() << "더미 데이터 로드 완료!";
    // });
}



void Home::addErrorLogUI(const QJsonObject &errorData){
    addErrorCardUI(errorData);
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

        m_client->publish(QMqttTopicName("feeder_02/cmd"), command.toUtf8());
        //m_client->publish(QMqttTopicName("conveyor_03/cmd"), command.toUtf8());
        //m_client->publish(QMqttTopicName("factory/conveyor_02/cmd"), command.toUtf8());
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
    qDebug() << "=== 서버 응답 수신됨! ===";

    QString messageStr = QString::fromUtf8(message.payload());
    QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    if(!doc.isObject()){
        qDebug() << " 잘못된 JSON 응답";
        return;
    }

    QJsonObject response = doc.object();
    QString responseQueryId = response["query_id"].toString();
    QString status = response["status"].toString();

    qDebug() << "응답 쿼리 ID:" << responseQueryId;
    qDebug() << "응답 상태:" << status;

    //  쿼리 ID로 구분해서 처리
    if(responseQueryId == chartQueryId) {
        // 차트용 데이터
        qDebug() << " 차트용 응답 처리";
        processChartDataResponse(response);
    } else if(responseQueryId == currentQueryId) {
        // UI 로그용 데이터
        qDebug() << " UI 로그용 응답 처리";
        processPastLogsResponse(response);
    } else if(responseQueryId == feederQueryId) {
        qDebug() << "피더 전용 응답 처리";
        processFeederResponse(response);
    } else if(responseQueryId == conveyorQueryId) {
        //  컨베이어 전용 응답 처리 추가
        qDebug() << "컨베이어 전용 응답 처리";
        processConveyorResponse(response);
    } else if(feederQueryMap.contains(responseQueryId)) {
        //  피더 쿼리 맵에서 처리
        qDebug() << "피더 쿼리 맵 응답 처리";
        MainWindow* targetWindow = feederQueryMap.take(responseQueryId);
        if(targetWindow) {
            processFeederSearchResponse(response, targetWindow);
        }
    } else if(conveyorQueryMap.contains(responseQueryId)) {
        //  컨베이어 쿼리 맵에서 처리
        qDebug() << " 컨베이어 쿼리 맵 응답 처리";
        ConveyorWindow* targetWindow = conveyorQueryMap.take(responseQueryId);
        if(targetWindow) {
            processConveyorSearchResponse(response, targetWindow);
        }
    } else {
        qDebug() << " 알 수 없는 쿼리 ID:" << responseQueryId;
    }
}


void Home::processConveyorResponse(const QJsonObject &response) {
    qDebug() << "컨베이어 응답 처리 시작";

    QString status = response["status"].toString();
    if(status != "success") {
        qDebug() << " 컨베이어 쿼리 실패:" << response["error"].toString();
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    QList<QJsonObject> conveyorResults;

    // 컨베이어 로그만 필터링
    for(const QJsonValue &value : dataArray) {
        QJsonObject logData = value.toObject();
        if(logData["device_id"].toString() == "conveyor_01" && logData["log_level"].toString() == "error") {
            conveyorResults.append(logData);
            qDebug() << " 컨베이어 에러 로그 추가:" << logData["log_code"].toString();
        }
    }

    qDebug() << " 컨베이어 결과:" << conveyorResults.size() << "개";

    //  ConveyorWindow로 결과 전달
    if(conveyorWindow) {
        conveyorWindow->onSearchResultsReceived(conveyorResults);
    }
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
    filters["limit"] = 500;    //  500개씩 나눠서 받기
    filters["offset"] = 0;     //  첫 페이지

    queryRequest["filters"] = filters;

    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "초기 로그 요청 (500개): " << payload;
    m_client->publish(mqttQueryRequestTopic, payload);

}


void Home::processPastLogsResponse(const QJsonObject &response) {
    isLoadingMoreLogs = false;

    // 날짜 검색이 아닌 경우에만 실시간 모드로 복귀
    if(!isDateSearchMode) {
        qDebug() << "📡 일반 검색 완료 - 실시간 로그 수신 재개";
    } else {
        qDebug() << "📅 날짜 검색 완료 - 실시간 로그 수신 계속 차단";
    }

    qDebug() << "=== 로그 응답 수신 ===";

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

    // 🔥🔥🔥 로그 카운터 추가 🔥🔥🔥
    int displayedLogCount = 0;

    if(isDateSearch && isFirstPage) {
        qDebug() << "📅 날짜 검색 모드 - 기존 로그 무시하고 서버 결과만 표시";
        // UI는 이미 clearAllErrorLogsFromUI()로 클리어된 상태
        // 서버 결과만 추가
        for(int i = dataArray.size() - 1; i >= 0; --i){
            QJsonObject logData = dataArray[i].toObject();
            if (logData["log_level"].toString() != "error") continue;

            // ✅ 날짜 검색 모드에서는 히스토리에 추가하지 않고 UI에만 표시
            addErrorLogUI(logData);  // UI에만 표시
            displayedLogCount++;    // 🔥 카운터 증가
        }
    } else {
        // 실시간 모드에서는 기존 방식 유지
        for(int i = dataArray.size() - 1; i >= 0; --i){
            QJsonObject logData = dataArray[i].toObject();
            if (logData["log_level"].toString() != "error") continue;
            addErrorLog(logData);    // 히스토리에 추가
            addErrorLogUI(logData);  // UI에 표시
            displayedLogCount++;    // 🔥 카운터 증가
        }
    }

    // 🔥🔥🔥 검색 결과가 없을 때 메시지 표시 🔥🔥🔥
    if(displayedLogCount == 0) {
        addNoResultsMessage();
    }

    qDebug() << " 로그 처리 완료:";
    qDebug() << "  - 처리된 로그:" << displayedLogCount << "개";  // 🔥 실제 표시된 로그 수
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


void Home::requestFeederLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate, MainWindow* targetWindow) {
    qDebug() << " requestFeederLogs 호출됨!";
    qDebug() << "매개변수 체크:";
    qDebug() << "  - errorCode:" << errorCode;
    qDebug() << "  - startDate:" << (startDate.isValid() ? startDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - endDate:" << (endDate.isValid() ? endDate.toString("yyyy-MM-dd") : "무효한 날짜");

    // MQTT 연결 상태 확인
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << " MQTT 연결 상태 오류!";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.");
        return;
    }

    //  피더 전용 쿼리 ID 생성
    QString feederQueryId = generateQueryId();
    qDebug() << " 피더 쿼리 ID:" << feederQueryId;

    //  피더 쿼리 ID와 대상 윈도우 매핑 저장
    feederQueryMap[feederQueryId] = targetWindow;

    //  서버가 기대하는 JSON 구조로 생성
    QJsonObject queryRequest;
    queryRequest["query_id"] = feederQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();

    QJsonObject filters;

    //  에러 코드 필터 (피더만)
    if(!errorCode.isEmpty()) {
        filters["log_code"] = errorCode;
        //filters["device_id"] = "feeder_02";
        qDebug() << " 에러 코드 필터:" << errorCode;
    }

    //  디바이스 필터 (피더만)
    filters["device_id"] = "feeder_02";
    qDebug() << " 디바이스 필터: feeder_02";

    //  날짜 필터 설정
    if(startDate.isValid() && endDate.isValid()) {
        qDebug() << " 날짜 검색 모드 활성화";

        // 안전한 날짜 변환
        QDateTime startDateTime;
        startDateTime.setDate(startDate);
        startDateTime.setTime(QTime(0, 0, 0, 0));
        startDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();

        QDateTime endDateTime;
        endDateTime.setDate(endDate);
        endDateTime.setTime(QTime(23, 59, 59, 999));
        endDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 endTimestamp = endDateTime.toMSecsSinceEpoch();

        //  서버가 기대하는 time_range 구조
        QJsonObject timeRange;
        timeRange["start"] = startTimestamp;
        timeRange["end"] = endTimestamp;
        filters["time_range"] = timeRange;

        // 날짜 검색에서는 충분한 limit 설정
        filters["limit"] = 10000;

        qDebug() << " time_range 필터 설정:";
        qDebug() << "  - 시작:" << startDate.toString("yyyy-MM-dd") << "→" << startTimestamp;
        qDebug() << "  - 종료:" << endDate.toString("yyyy-MM-dd") << "→" << endTimestamp;
        qDebug() << "  - limit:" << 10000;

    } else {
        qDebug() << " 일반 최신 로그 모드";
        filters["limit"] = 500;
        filters["offset"] = 0;
    }

    //  로그 레벨 필터
    filters["log_level"] = "error";

    queryRequest["filters"] = filters;

    // JSON 문서 생성 및 전송
    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "=== 피더 MQTT 전송 시도 ===";
    qDebug() << "토픽:" << mqttQueryRequestTopic;
    qDebug() << "페이로드 크기:" << payload.size() << "bytes";
    qDebug() << "전송할 JSON:";
    qDebug() << doc.toJson(QJsonDocument::Indented);

    //  MQTT 전송
    bool result = m_client->publish(mqttQueryRequestTopic, payload);
    qDebug() << "MQTT 전송 결과:" << (result ? " 성공" : "️ 비동기 (정상)");

    qDebug() << " 피더 MQTT 전송 완료! 응답 대기 중...";
}

//  requestFilteredLogs 함수 완전 수정 - 서버 JSON 구조에 맞춤
/*void Home::requestFilteredLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate, bool loadMore) {
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
    // 더보기가 아닌 경우에만 검색 조건 저장
    if(!loadMore) {
        currentPage = 0;
        lastSearchErrorCode = errorCode;
        lastSearchStartDate = startDate;
        lastSearchEndDate = endDate;

        // 👈👈👈 여기부터 새로 추가 👈👈👈
        // 실제 전달받은 날짜로 모드 판단
        bool isActualDateSearch = (startDate.isValid() && endDate.isValid());

        if(isActualDateSearch) {
            // 실제 날짜 검색 모드
            isDateSearchMode = true;
            currentSearchStartDate = startDate;
            currentSearchEndDate = endDate;
            qDebug() << "📅 날짜 검색 모드 활성화됨";
            qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
            qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

            // 기존 로그 목록 클리어 (날짜 검색 시에만)
            clearAllErrorLogsFromUI();

        } else {
            // 실시간 모드 (날짜가 무효하거나 빈 경우)
            isDateSearchMode = false;
            currentSearchStartDate = QDate();
            currentSearchEndDate = QDate();
            qDebug() << "📡 실시간 모드 활성화됨";
            qDebug() << "  - 날짜가 무효하므로 실시간 로그 수신 재개";

            // 실시간 모드일 때 기존 로그 클리어 (선택사항)
            clearAllErrorLogsFromUI();
        }
        qDebug() << "=== 모드 설정 완료 - isDateSearchMode:" << isDateSearchMode << "===";
        // 👈👈👈 여기까지 새로 추가 👈👈👈

        qDebug() << " 새 검색 - 조건 저장됨:";
        qDebug() << "  - errorCode:" << lastSearchErrorCode;
        qDebug() << "  - startDate:" << (lastSearchStartDate.isValid() ? lastSearchStartDate.toString("yyyy-MM-dd") : "무효");
        qDebug() << "  - endDate:" << (lastSearchEndDate.isValid() ? lastSearchEndDate.toString("yyyy-MM-dd") : "무효");

    } else {
        currentPage++;
        qDebug() << " 더보기 - 저장된 조건 사용 (페이지:" << currentPage << ")";
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
        //startDateTime.setTimeSpec(Qt::LocalTime);
        startDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();

        QDateTime endDateTime;
        endDateTime.setDate(useEndDate);
        endDateTime.setTime(QTime(23, 59, 59, 999));
        //endDateTime.setTimeSpec(Qt::LocalTime);
        endDateTime.setTimeZone(QTimeZone::systemTimeZone());
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

    //  MQTT 전송 (false 무시)
    qDebug() << "📡 MQTT publish 시도...";
    qDebug() << "  - 클라이언트 ID:" << m_client->clientId();
    qDebug() << "  - 호스트:" << m_client->hostname() << ":" << m_client->port();

    bool result = m_client->publish(mqttQueryRequestTopic, payload);
    qDebug() << "MQTT 전송 결과:" << (result ? " 성공" : "️ 비동기 (정상)");

    //  false여도 실제로는 전송되므로 에러 처리 제거

    qDebug() << " MQTT 전송 완료! 응답 대기 중...";
}
*/

void Home::requestFilteredLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate, bool loadMore) {
    qDebug() << "🔧 requestFilteredLogs 호출됨!";
    qDebug() << "매개변수 체크:";
    qDebug() << "  - errorCode:" << errorCode;
    qDebug() << "  - startDate:" << (startDate.isValid() ? startDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - endDate:" << (endDate.isValid() ? endDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - loadMore:" << loadMore;

    // MQTT 연결 상태 확인
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << "❌ MQTT 연결 상태 오류!";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.");
        return;
    }

    // 더보기가 아닌 경우에만 검색 조건 저장
    if(!loadMore) {
        currentPage = 0;
        lastSearchErrorCode = errorCode;
        lastSearchStartDate = startDate;
        lastSearchEndDate = endDate;

        // 실제 전달받은 날짜로 모드 판단
        bool isActualDateSearch = (startDate.isValid() && endDate.isValid());

        if(isActualDateSearch) {
            // 실제 날짜 검색 모드
            isDateSearchMode = true;
            currentSearchStartDate = startDate;
            currentSearchEndDate = endDate;
            qDebug() << "📅 날짜 검색 모드 활성화됨";
            qDebug() << "  - 시작일:" << startDate.toString("yyyy-MM-dd");
            qDebug() << "  - 종료일:" << endDate.toString("yyyy-MM-dd");

            // 기존 로그 목록 클리어 (날짜 검색 시에만)
            clearAllErrorLogsFromUI();

        } else {
            // 실시간 모드 (날짜가 무효하거나 빈 경우)
            isDateSearchMode = false;
            currentSearchStartDate = QDate();
            currentSearchEndDate = QDate();
            qDebug() << "📡 실시간 모드 활성화됨";
            qDebug() << "  - 날짜가 무효하므로 실시간 로그 수신 재개";

            // 실시간 모드일 때 기존 로그 클리어 (선택사항)
            clearAllErrorLogsFromUI();
        }
        qDebug() << "=== 모드 설정 완료 - isDateSearchMode:" << isDateSearchMode << "===";

        qDebug() << "🔧 새 검색 - 조건 저장됨:";
        qDebug() << "  - errorCode:" << lastSearchErrorCode;
        qDebug() << "  - startDate:" << (lastSearchStartDate.isValid() ? lastSearchStartDate.toString("yyyy-MM-dd") : "무효");
        qDebug() << "  - endDate:" << (lastSearchEndDate.isValid() ? lastSearchEndDate.toString("yyyy-MM-dd") : "무효");

    } else {
        currentPage++;
        qDebug() << "🔧 더보기 - 저장된 조건 사용 (페이지:" << currentPage << ")";
    }

    // 로딩 상태 방지
    if(isLoadingMoreLogs) {
        qDebug() << "⚠️ 이미 로딩 중입니다!";
        return;
    }
    isLoadingMoreLogs = true;

    // 쿼리 ID 생성
    currentQueryId = generateQueryId();
    qDebug() << "🔧 쿼리 정보:";
    qDebug() << "  - 쿼리 ID:" << currentQueryId;
    qDebug() << "  - 페이지:" << currentPage;
    qDebug() << "  - 페이지 크기:" << pageSize;

    // ✅✅✅ 핵심 수정: feederQueryMap에 저장 ✅✅✅
    if(currentFeederWindow) {
        feederQueryMap[currentQueryId] = currentFeederWindow;
        qDebug() << "🎯 핵심 수정: feederQueryMap에 저장됨!";
        qDebug() << "  - 쿼리 ID:" << currentQueryId;
        qDebug() << "  - MainWindow 포인터:" << currentFeederWindow;
    } else {
        qDebug() << "❌ currentFeederWindow가 null입니다!";
    }
    // ✅✅✅ 핵심 수정 끝 ✅✅✅

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
    // if(!useErrorCode.isEmpty()) {
    //     filters["log_code"] = useErrorCode;
    //     qDebug() << "🔧 에러 코드 필터:" << useErrorCode;
    // }
    // ✅ 수정된 코드:
    if(!useErrorCode.isEmpty()) {
        if(useErrorCode == "feeder_01" || useErrorCode == "conveyor_01") {
            filters["device_id"] = useErrorCode;  // device_id로 변경
            qDebug() << "🔧 디바이스 ID 필터:" << useErrorCode;
        } else {
            filters["log_code"] = useErrorCode;   // 기존 에러 코드
            qDebug() << "🔧 에러 코드 필터:" << useErrorCode;
        }
    }

    //  핵심: time_range 객체 사용 (서버가 기대하는 구조)
    if(useStartDate.isValid() && useEndDate.isValid()) {
        qDebug() << "🔧 날짜 검색 모드 - 모든 데이터 한번에 가져오기";

        // 안전한 날짜 변환
        QDateTime startDateTime;
        startDateTime.setDate(useStartDate);
        startDateTime.setTime(QTime(0, 0, 0, 0));
        startDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();

        QDateTime endDateTime;
        endDateTime.setDate(useEndDate);
        endDateTime.setTime(QTime(23, 59, 59, 999));
        endDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 endTimestamp = endDateTime.toMSecsSinceEpoch();

        //  서버가 기대하는 time_range 구조
        QJsonObject timeRange;
        timeRange["start"] = startTimestamp;
        timeRange["end"] = endTimestamp;
        filters["time_range"] = timeRange;

        qDebug() << "🔧 시간 범위 설정:";
        qDebug() << "  - start timestamp:" << startTimestamp;
        qDebug() << "  - end timestamp:" << endTimestamp;
        qDebug() << "  - start 날짜:" << startDateTime.toString("yyyy-MM-dd hh:mm:ss");
        qDebug() << "  - end 날짜:" << endDateTime.toString("yyyy-MM-dd hh:mm:ss");

        // 날짜 검색에서는 한번에 많이 가져오기
        filters["limit"] = 2000;
        filters["offset"] = 0;
        filters["log_level"] = "error";

    } else {
        qDebug() << "🔧 실시간/일반 검색 모드";
        filters["limit"] = pageSize;
        filters["offset"] = currentPage * pageSize;
        filters["log_level"] = "error";
    }

    queryRequest["filters"] = filters;

    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "🔧 MQTT 쿼리 요청:";
    qDebug() << "  - 토픽:" << mqttQueryRequestTopic;
    qDebug() << "  - 페이로드:" << payload;

    // 타임아웃 타이머 설정
    QTimer::singleShot(30000, this, [this]() {
        if(isLoadingMoreLogs) {
            isLoadingMoreLogs = false;
            qDebug() << "⏰ 로그 요청 타임아웃";
            QMessageBox::warning(this, "타임아웃", "로그 요청 시간이 초과되었습니다.");
        }
    });

    //  MQTT 전송
    qDebug() << "📡 MQTT publish 시도...";
    qDebug() << "  - 클라이언트 ID:" << m_client->clientId();
    qDebug() << "  - 호스트:" << m_client->hostname() << ":" << m_client->port();

    bool result = m_client->publish(mqttQueryRequestTopic, payload);
    qDebug() << "MQTT 전송 결과:" << (result ? "✅ 성공" : "⚠️ 비동기 (정상)");

    qDebug() << "🔧 MQTT 전송 완료! 응답 대기 중...";
}

void Home::onSearchClicked() {
    qDebug() << " 검색 버튼 클릭됨!!!! ";
    qDebug() << "함수 시작 - 현재 시간:" << QDateTime::currentDateTime().toString();

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


void Home::processFeederResponse(const QJsonObject &response) {
    qDebug() << " 피더 응답 처리 시작";

    QString status = response["status"].toString();
    if(status != "success") {
        qDebug() << " 피더 쿼리 실패:" << response["error"].toString();
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    QList<QJsonObject> feederResults;

    // 피더 로그만 필터링
    for(const QJsonValue &value : dataArray) {
        QJsonObject logData = value.toObject();
        if(logData["device_id"].toString() == "feeder_01" && logData["log_level"].toString() == "error") {
            feederResults.append(logData);
            qDebug() << " 에러 로그 추가:" << logData["log_code"].toString();
        }

        qDebug() << " 피더 결과:" << feederResults.size() << "개";

        //  MainWindow로 결과 전달 (기존 함수 재사용)
        if(feederWindow) {
            feederWindow->onSearchResultsReceived(feederResults);
        }
    }
}

void Home::processFeederSearchResponse(const QJsonObject &response, MainWindow* targetWindow) {
    qDebug() << " 피더 검색 응답 처리 시작";

    QString status = response["status"].toString();
    if(status != "success"){
        QString errorMsg = response["error"].toString();
        qDebug() << " 피더 쿼리 실패:" << errorMsg;
        QMessageBox::warning(this, "조회 실패", "피더 로그 조회에 실패했습니다: " + errorMsg);
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    qDebug() << " 피더 로그 수신:" << dataArray.size() << "개";

    //  QJsonObject 리스트로 변환
    QList<QJsonObject> feederLogs;

    for(const QJsonValue &value : dataArray) {
        QJsonObject logData = value.toObject();

        //  피더 로그만 필터링 (서버에서 필터링되지만 클라이언트에서도 확인)
        QString deviceId = logData["device_id"].toString();
        if(deviceId == "feeder_01" &&
            logData["log_level"].toString() == "error") {
            feederLogs.append(logData);
            qDebug() << " 에러 로그 추가:" << logData["log_code"].toString();
        }
    }

    qDebug() << " 최종 피더 로그:" << feederLogs.size() << "개";

    //  MainWindow로 검색 결과 전달
    if(targetWindow) {
        QMetaObject::invokeMethod(targetWindow, "onSearchResultsReceived",
                                  Qt::QueuedConnection,
                                  Q_ARG(QList<QJsonObject>, feederLogs));
        qDebug() << " 피더 검색 결과를 MainWindow로 전달 완료";
    } else {
        qDebug() << " targetWindow가 null입니다!";
    }

    isLoadingMoreLogs = false;
    qDebug() << "🔧 피더 응답 처리 완료";
}

void Home::processConveyorSearchResponse(const QJsonObject &response, ConveyorWindow* targetWindow) {
    qDebug() << " 컨베이어 검색 응답 처리 시작";

    QString status = response["status"].toString();
    if(status != "success"){
        QString errorMsg = response["error"].toString();
        qDebug() << " 컨베이어 쿼리 실패:" << errorMsg;
        QMessageBox::warning(this, "조회 실패", "컨베이어 로그 조회에 실패했습니다: " + errorMsg);
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    qDebug() << " 컨베이어 로그 수신:" << dataArray.size() << "개";

    //  QJsonObject 리스트로 변환
    QList<QJsonObject> conveyorLogs;

    for(const QJsonValue &value : dataArray) {
        QJsonObject logData = value.toObject();

        //  컨베이어 로그만 필터링 (서버에서 필터링되지만 클라이언트에서도 확인)
        QString deviceId = logData["device_id"].toString();
        if(deviceId == "conveyor_01" &&
            logData["log_level"].toString() == "error") {
            conveyorLogs.append(logData);
            qDebug() << " 컨베이어 에러 로그 추가:" << logData["log_code"].toString();
        }
    }

    qDebug() << " 최종 컨베이어 로그:" << conveyorLogs.size() << "개";

    //  ConveyorWindow로 검색 결과 전달
    if(targetWindow) {
        QMetaObject::invokeMethod(targetWindow, "onSearchResultsReceived",
                                  Qt::QueuedConnection,
                                  Q_ARG(QList<QJsonObject>, conveyorLogs));
        qDebug() << " 컨베이어 검색 결과를 ConveyorWindow로 전달 완료";
    } else {
        qDebug() << " targetWindow가 null입니다!";
    }
}

void Home::loadAllChartData() {
    if(isLoadingChartData) return;

    isLoadingChartData = true;

    qDebug() << "[CHART] 차트용 1-6월 데이터 단일 요청 시작...";

    // 배치 대신 단일 요청으로
    loadChartDataSingle();
}

void Home::loadChartDataSingle() {
    if(!m_client || m_client->state() != QMqttClient::Connected) {
        isLoadingChartData = false;
        return;
    }

    chartQueryId = generateQueryId();

    QJsonObject queryRequest;
    queryRequest["query_id"] = chartQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();

    QJsonObject filters;
    filters["log_level"] = "error";

    // 핵심: 1-6월만 time_range로 한 번에 요청
    QJsonObject timeRange;

    QDateTime startDateTime = QDateTime::fromString("2025-01-16T00:00:00", Qt::ISODate);
    QDateTime endDateTime = QDateTime::fromString("2025-06-17T23:59:59", Qt::ISODate);

    timeRange["start"] = startDateTime.toMSecsSinceEpoch();
    timeRange["end"] = endDateTime.toMSecsSinceEpoch();
    filters["time_range"] = timeRange;

    // 큰 limit으로 1-6월 데이터 모두 한 번에
    filters["limit"] = 2000;  // 충분히 큰 값
    filters["offset"] = 0;

    queryRequest["filters"] = filters;

    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "[CHART] 1-6월 전체 데이터 단일 요청";
    qDebug() << "[CHART] time_range: 2025-01-16 ~ 2025-06-17";
    qDebug() << "[CHART] limit: 2000";

    m_client->publish(mqttQueryRequestTopic, payload);
}

void Home::processChartDataResponse(const QJsonObject &response) {
    qDebug() << "[HOME] ===== 차트용 데이터 응답 수신 =====";
    qDebug() << "[HOME] 응답 상태:" << response["status"].toString();

    QString status = response["status"].toString();
    if(status != "success"){
        qDebug() << "[HOME] 차트 데이터 쿼리 실패:" << response["error"].toString();
        qDebug() << "[HOME] 전체 응답:" << response;
        isLoadingChartData = false;
        return;
    }

    QJsonArray dataArray = response["data"].toArray();
    int totalDataCount  = dataArray.size();

    qDebug() << "[HOME] 차트 배치 처리: " << totalDataCount  << "개";

    if(totalDataCount  == 0) {
        qDebug() << "️ [HOME] 받은 데이터가 0개입니다!";
        qDebug() << "️ [HOME] 서버에 1-6월 데이터가 없는 것 같습니다.";
        isLoadingChartData = false;
        return;
    }

    // 샘플 데이터 확인
    qDebug() << "[HOME] 첫 번째 데이터 샘플:";

    if(totalDataCount  > 0) {
        QJsonObject firstData = dataArray[0].toObject();
        qDebug() << "  device_id:" << firstData["device_id"].toString();
        qDebug() << "  timestamp:" << firstData["timestamp"];
        qDebug() << "  log_level:" << firstData["log_level"].toString();
        qDebug() << "  log_code:" << firstData["log_code"].toString();
    }

    int processedCount = 0;
    int validDateCount = 0;
    int feederCount = 0;
    int conveyorCount = 0;
    int errorLevelCount = 0;

    for(const QJsonValue &value : dataArray) {
        QJsonObject logData = value.toObject();

        // 로그 레벨 체크
        if(logData["log_level"].toString() == "error") {
            errorLevelCount++;
        }

        // 디바이스 타입 체크
        QString deviceId = logData["device_id"].toString();
        if(deviceId.contains("feeder")) {
            feederCount++;
        } else if(deviceId.contains("conveyor")) {
            conveyorCount++;
        }

        // 타임스탬프 처리
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

        // 날짜 확인
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString dateStr = dateTime.toString("yyyy-MM-dd");

        // 1-6월 범위인지 확인
        QDate targetDate = dateTime.date();
        QDate startRange(2025, 1, 16);
        QDate endRange(2025, 6, 17);

        if(targetDate >= startRange && targetDate <= endRange) {
            validDateCount++;
            if(validDateCount <= 5) {
                qDebug() << "[HOME] 유효한 날짜 데이터" << validDateCount << ":" << dateStr;

            }
        }

        QJsonObject completeLogData = logData;
        completeLogData["timestamp"] = timestamp;

        // 차트에 전달
        if(m_errorChartManager) {
            m_errorChartManager->processErrorData(completeLogData);
            processedCount++;
        }
    }

    qDebug() << "[HOME] ===== 차트 데이터 처리 완료 =====";
    qDebug() << "[HOME] 전체 받은 데이터:" << totalDataCount << "개";
    qDebug() << "[HOME] 차트로 전달된 데이터:" << processedCount << "개";
    qDebug() << "[HOME] 1-6월 범위 데이터:" << validDateCount << "개";
    qDebug() << "[HOME] 에러 레벨 데이터:" << errorLevelCount << "개";
    qDebug() << "[HOME] 피더 데이터:" << feederCount << "개";
    qDebug() << "[HOME] 컨베이어 데이터:" << conveyorCount << "개";

    // 차트 데이터 로딩 완료
    isLoadingChartData = false;
    qDebug() << "[HOME] 차트 데이터 로딩 완료!";
}

//  컨베이어 날짜 검색 처리 함수 (피더와 똑같은 로직)
void Home::handleConveyorLogSearch(const QString& errorCode, const QDate& startDate, const QDate& endDate) {

    qDebug() << "=== Home::handleConveyorLogSearch 호출됨 ===";

    qDebug() << "매개변수 체크:";
    qDebug() << "  - errorCode:" << errorCode;
    qDebug() << "  - startDate:" << (startDate.isValid() ? startDate.toString("yyyy-MM-dd") : "무효한 날짜");
    qDebug() << "  - endDate:" << (endDate.isValid() ? endDate.toString("yyyy-MM-dd") : "무효한 날짜");

    // MQTT 연결 상태 확인
    if(!m_client || m_client->state() != QMqttClient::Connected){
        qDebug() << " MQTT 연결 오류!";
        QMessageBox::warning(this, "연결 오류", "MQTT 서버에 연결되지 않았습니다.");
        return;
    }

    //  컨베이어 전용 쿼리 ID 생성
    QString conveyorQueryId = generateQueryId();
    qDebug() << " 컨베이어 쿼리 ID:" << conveyorQueryId;

    //  컨베이어 쿼리 ID와 대상 윈도우 매핑 저장
    conveyorQueryMap[conveyorQueryId] = currentConveyorWindow;

    //  서버가 기대하는 JSON 구조로 생성
    QJsonObject queryRequest;
    queryRequest["query_id"] = conveyorQueryId;
    queryRequest["query_type"] = "logs";
    queryRequest["client_id"] = m_client->clientId();

    QJsonObject filters;

    //  에러 코드 필터 (컨베이어만)
    if(!errorCode.isEmpty()) {
        filters["log_code"] = errorCode;
        qDebug() << " 에러 코드 필터:" << errorCode;
    }

    //  디바이스 필터 (컨베이어만)
    filters["device_id"] = "conveyor_01";
    qDebug() << " 디바이스 필터: conveyor_01";

    //  날짜 필터 설정
    if(startDate.isValid() && endDate.isValid()) {
        qDebug() << " 날짜 검색 모드 활성화";

        // 안전한 날짜 변환
        QDateTime startDateTime;
        startDateTime.setDate(startDate);
        startDateTime.setTime(QTime(0, 0, 0, 0));
        startDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();

        QDateTime endDateTime;
        endDateTime.setDate(endDate);
        endDateTime.setTime(QTime(23, 59, 59, 999));
        endDateTime.setTimeZone(QTimeZone::systemTimeZone());
        qint64 endTimestamp = endDateTime.toMSecsSinceEpoch();

        //  서버가 기대하는 time_range 구조
        QJsonObject timeRange;
        timeRange["start"] = startTimestamp;
        timeRange["end"] = endTimestamp;
        filters["time_range"] = timeRange;

        // 날짜 검색에서는 충분한 limit 설정
        filters["limit"] = 10000;

        qDebug() << " 컨베이어 time_range 필터 설정:";
        qDebug() << "  - 시작:" << startDate.toString("yyyy-MM-dd") << "→" << startTimestamp;
        qDebug() << "  - 종료:" << endDate.toString("yyyy-MM-dd") << "→" << endTimestamp;
        qDebug() << "  - limit:" << 10000;

    } else {
        qDebug() << " 일반 최신 로그 모드";
        filters["limit"] = 2000;
        filters["offset"] = 0;
    }

    //  로그 레벨 필터
    filters["log_level"] = "error";

    queryRequest["filters"] = filters;

    // JSON 문서 생성 및 전송
    QJsonDocument doc(queryRequest);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    qDebug() << "=== 컨베이어 MQTT 전송 시도 ===";
    qDebug() << "토픽:" << mqttQueryRequestTopic;
    qDebug() << "페이로드 크기:" << payload.size() << "bytes";
    qDebug() << "전송할 JSON:";
    qDebug() << doc.toJson(QJsonDocument::Indented);

    //  MQTT 전송
    bool result = m_client->publish(mqttQueryRequestTopic, payload);
    qDebug() << "MQTT 전송 결과:" << (result ? " 성공" : "️ 비동기 (정상)");

    qDebug() << " 컨베이어 MQTT 전송 완료! 응답 대기 중...";
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


// 로그 카드
void Home::addErrorCardUI(const QJsonObject &errorData) {
    QWidget* card = new QWidget();
    card->setFixedHeight(84);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setStyleSheet(R"(
        background-color: #ffffff;
        border: 1px solid #e5e7eb;
        border-left: 2px solid #f97316;
        border-radius: 12px;
    )");
    card->setProperty("errorData", QVariant::fromValue(errorData));

    // 카드 더블클릭 이벤트 필터 설치
    static CardEventFilter* filter = nullptr;
    if (!filter) {
        filter = new CardEventFilter(this);
        connect(filter, &CardEventFilter::cardDoubleClicked, this, &Home::onCardDoubleClicked);
    }
    card->installEventFilter(filter);

    QVBoxLayout* outer = new QVBoxLayout(card);
    outer->setContentsMargins(12, 6, 12, 6);
    outer->setSpacing(4);

    // 상단: 오류 배지 + 시간
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    topRow->setContentsMargins(0, 0, 0, 0);

    QLabel* badge = new QLabel("오류");
    badge->setStyleSheet(R"(
        background-color: #b91c1c;
        color: white;
        padding: 3px 8px;
        min-height: 18px;
        font-size: 10px;
        border-radius: 8px;
        border: none;
    )");

    QHBoxLayout* left = new QHBoxLayout();
    left->addWidget(badge);
    left->setSpacing(4);
    left->setContentsMargins(0, 0, 0, 0);
    left->addStretch();

    QLabel* timeLabel = new QLabel(
        QDateTime::fromMSecsSinceEpoch(errorData["timestamp"].toVariant().toLongLong()).toString("MM-dd hh:mm")
        );
    timeLabel->setStyleSheet("color: #6b7280; font-size: 10px; border: none;");
    timeLabel->setMaximumWidth(70); // 최대 폭 제한
    timeLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topRow->addLayout(left);
    topRow->addWidget(timeLabel);

    // 메시지
    QString logCode = errorData["log_code"].toString();
    QString messageText = (logCode == "SPD") ? "SPD(모터 속도)" : logCode;
    QLabel* message = new QLabel(messageText);
    message->setStyleSheet("color: #374151; font-size: 13px; border: none;");

    // 기기 배지
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->addStretch();

    QLabel* device = new QLabel(errorData["device_id"].toString());
    device->setMinimumHeight(24);
    QString dev = errorData["device_id"].toString();
    QString devStyle = dev.contains("feeder")
                           ? R"(
            background-color: #fed7aa;
            color: #7c2d12;
            border: 1px solid #fdba74;
            padding: 2px 6px;
            border-radius: 9999px;
        )"
                           : R"(
            background-color: #ffedd5;
            color: #78350f;
            border: 1px solid #fcd34d;
            padding: 2px 6px;
            border-radius: 9999px;
        )";
    device->setStyleSheet(devStyle);

    bottomRow->addWidget(device);

    // 조립
    outer->addLayout(topRow);
    outer->addWidget(message);
    outer->addLayout(bottomRow);

    // 삽입
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->scrollArea->widget()->layout());
    if (layout) {
        layout->insertWidget(0, card);
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

void Home::onCardDoubleClicked(QObject* cardWidget) {
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

    VideoClient* client = new VideoClient(this);
    client->queryVideos(deviceId, "", startTime, endTime, 1,
                        [this](const QList<VideoInfo>& videos) {
                            if (videos.isEmpty()) {
                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
                                return;
                            }
                            QString httpUrl = videos.first().http_url;
                            this->downloadAndPlayVideoFromUrl(httpUrl);
                        }
                        );
}


// 👈👈👈 파일 맨 끝에 다음 2개 함수 추가 👈👈👈

// 실시간 모드로 돌아가는 함수
void Home::enableRealTimeMode() {
    qDebug() << "📡 실시간 모드 활성화";
    isDateSearchMode = false;
    isLoadingMoreLogs = false;

    // 현재 검색 날짜 초기화
    currentSearchStartDate = QDate();
    currentSearchEndDate = QDate();

    qDebug() << "✅ 실시간 로그 수신 재개됨";

    // 기존 로그 클리어하고 최신 로그 요청
    clearAllErrorLogsFromUI();
    // requestPastLogs();  // 이 함수가 있다면 주석 해제
}

// UI에서 모든 에러 로그 클리어하는 함수
void Home::clearAllErrorLogsFromUI() {
    qDebug() << "=== 에러 로그 UI 클리어 시작 ===";

    // scrollArea 내부의 모든 카드 위젯 제거
    if (ui->scrollArea && ui->scrollArea->widget()) {
        QWidget* content = ui->scrollArea->widget();
        if (content && content->layout()) {
            QVBoxLayout* vboxLayout = qobject_cast<QVBoxLayout*>(content->layout());
            if (vboxLayout) {
                // 모든 아이템을 역순으로 제거 (안전한 제거)
                while (QLayoutItem* item = vboxLayout->takeAt(0)) {
                    if (QWidget* widget = item->widget()) {
                        widget->deleteLater();  // 메모리 안전 삭제
                    }
                    delete item;
                }

                // stretch 다시 추가 (setupRightPanel()과 동일하게)
                vboxLayout->addStretch();

                qDebug() << "scrollArea 내부 위젯들 모두 제거 완료";
            } else {
                qDebug() << "경고: VBoxLayout으로 캐스팅 실패";
            }
        } else {
            qDebug() << "경고: scrollArea content 또는 layout이 없음";
        }
    } else {
        qDebug() << "경고: scrollArea 또는 widget이 없음";
    }
}

void Home::addNoResultsMessage() {
    if (!ui->scrollArea || !ui->scrollArea->widget()) return;

    QWidget* content = ui->scrollArea->widget();
    if (!content || !content->layout()) return;

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(content->layout());
    if (!layout) return;

    // 검색 결과 없음 카드 생성
    QWidget* noResultCard = new QWidget();
    noResultCard->setFixedHeight(100);
    noResultCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    noResultCard->setStyleSheet(R"(
        background-color: #f8f9fa;
        border: 2px dashed #dee2e6;
        border-radius: 12px;
    )");

    QVBoxLayout* cardLayout = new QVBoxLayout(noResultCard);
    cardLayout->setContentsMargins(20, 15, 20, 15);
    cardLayout->setSpacing(5);

    QLabel* iconLabel = new QLabel("🔍");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 24px; color: #6c757d; border: none;");

    QLabel* messageLabel = new QLabel("검색 결과가 없습니다");
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #6c757d; border: none;");

    QLabel* subMessageLabel = new QLabel("다른 검색 조건을 시도해보세요");
    subMessageLabel->setAlignment(Qt::AlignCenter);
    subMessageLabel->setStyleSheet("font-size: 12px; color: #868e96; border: none;");

    cardLayout->addWidget(iconLabel);
    cardLayout->addWidget(messageLabel);
    cardLayout->addWidget(subMessageLabel);

    // stretch 위에 삽입 (맨 위에)
    layout->insertWidget(0, noResultCard);

    qDebug() << "📝 Home '검색 결과 없음' 메시지 카드 추가됨";
}
