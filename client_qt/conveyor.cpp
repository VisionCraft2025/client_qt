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

        showConveyorNormal();

        setupLogWidgets();
        setupControlButtons();
        setupRightPanel();

        setupHomeButton();
        setupMqttClient(); //mqtt 설정
        connectToMqttBroker(); //연결 시도

        // 로그 더블클릭 이벤트 연결
        //connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &ConveyorWindow::on_listWidget_itemDoubleClicked);


        // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
        rpiStreamer = new Streamer("rtsp://192.168.0.52:8555/process2", this);

        // 한화 카메라 스트리머 객체 생성
        hwStreamer = new Streamer("rtsp://192.168.0.76:8553/stream_pno", this);

        // signal-slot
        connect(rpiStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateRPiImage);
        rpiStreamer->start();

        // 한화 signal-slot 연결
        connect(hwStreamer, &Streamer::newFrame, this, &ConveyorWindow::updateHWImage);
        hwStreamer->start();

        statisticsTimer = new QTimer(this);
        connect(statisticsTimer, &QTimer::timeout, this, &ConveyorWindow::requestStatisticsData);

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

        auto failureSubscription = m_client->subscribe(QString("factory/conveyor_01/log/response"));
        if(failureSubscription){
            connect(failureSubscription, &QMqttSubscription::messageReceived,
                    this, &ConveyorWindow::onMqttMessageReceived);
        }

        auto failureTimer = new QTimer(this);
        connect(failureTimer, &QTimer::timeout, this, &ConveyorWindow::requestFailureRate);
        failureTimer->start(60000); // 5초마다 요청

        if(statisticsTimer && !statisticsTimer->isActive()) {
            statisticsTimer->start(60000);  // 3초마다 요청
        }


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
                    QString displayRate = QString::number(rate, 'f', 2) + "%";

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
        logData["message"] = "conveyor_03";
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
        logData["message"] = "conveyor_03";
        logData["timestamp"] = QDateTime::currentMSecsSinceEpoch();

        QJsonDocument doc(logData);
        QString jsonString = doc.toJson(QJsonDocument::Compact);

        //emit requestMqttPublish("factory/msg/status", "off");
        emit requestMqttPublish("factory/msg/status", doc.toJson(QJsonDocument::Compact));

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

    void ConveyorWindow::requestStatisticsData() {
        if(m_client && m_client->state() == QMqttClient::Connected) {
            QJsonObject request;
            request["device_id"] = "conveyor_01";

            // QJsonObject timeRange;
            // QDateTime now = QDateTime::currentDateTime();
            // QDateTime oneMinuteAgo = now.addSecs(-1);  // 5초 전
            // timeRange["start"] = oneMinuteAgo.toMSecsSinceEpoch();
            // timeRange["end"] = now.toMSecsSinceEpoch();
            // request["time_range"] = timeRange;

            QJsonDocument doc(request);

            m_client->publish(QString("factory/statistics"), doc.toJson(QJsonDocument::Compact));
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

    void ConveyorWindow::setupLogWidgets(){
        QHBoxLayout *bottomLayout = qobject_cast<QHBoxLayout*>(ui->bottomSectionWidget->layout());

        if(bottomLayout){
            QWidget* oldTextLog = ui->textLog;
            bottomLayout->removeWidget(oldTextLog);
            oldTextLog->hide();

            // 기존 groupControl도 레이아웃에서 제거
            bottomLayout->removeWidget(ui->groupControl);

            // 전체를 하나의 QSplitter로 만들기
            QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);

            // 실시간 이벤트 로그 (작게!)
            QGroupBox *eventLogGroup = new QGroupBox("실시간 이벤트 로그");
            QVBoxLayout *eventLayout = new QVBoxLayout(eventLogGroup);
            textEventLog = new QTextEdit();
            eventLayout->addWidget(textEventLog);
            // 최대 너비 제한으로 강제로 작게 만들기
            eventLogGroup->setMaximumWidth(250);
            eventLogGroup->setMinimumWidth(200);

            // 기기 상태 (매우 크게!)
            QGroupBox *statusGroup = new QGroupBox("기기 상태");
            QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
            textErrorStatus = new QTextEdit();
            textErrorStatus->setReadOnly(true);
            // 기기 상태는 최대 너비 제한 제거
            textErrorStatus->setMaximumWidth(QWIDGETSIZE_MAX);
            statusLayout->addWidget(textErrorStatus);

            if(textErrorStatus){
                QString initialText = "현재 속도: 로딩중...\n";
                initialText += "평균 속도: 로딩중...\n";
                initialText += "불량률: 계산중...";
                textErrorStatus->setText(initialText);
            }

            // 기기 상태 및 제어 (작게!)
            ui->groupControl->setMaximumWidth(250);
            ui->groupControl->setMinimumWidth(200);

            // 3개 모두를 mainSplitter에 추가
            mainSplitter->addWidget(eventLogGroup);
            mainSplitter->addWidget(statusGroup);
            mainSplitter->addWidget(ui->groupControl);

            // 극단적 비율 설정: 실시간로그(10) + 기기상태(80) + 기기제어(10)
            mainSplitter->setStretchFactor(0, 10);  // 실시간 이벤트 로그 (매우 작게)
            mainSplitter->setStretchFactor(1, 80);  // 기기 상태 (매우 크게!)
            mainSplitter->setStretchFactor(2, 10);  // 기기 상태 및 제어 (매우 작게)

            // 사용자가 크기 조정할 수 있도록 설정
            mainSplitter->setChildrenCollapsible(false);

            bottomLayout->addWidget(mainSplitter);

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
        ui->lineEdit->setPlaceholderText("컨베이어 오류 코드 ...");
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

            // INF 로그 처리 (정상 상태)
            if(logCode == "INF" || logLevel == "info") {
                qDebug() << "컨베이어 정상 상태 감지";
                showConveyorNormal();  // 정상 상태 표시
                // INF는 에러 리스트에 추가하지 않음 (addErrorLog 호출 안 함)
            }
            // 실제 오류 로그만 처리
            else {
                qDebug() << "컨베이어 오류 상태 감지:" << logCode;
                showConveyorError(logCode);  // 오류 상태 표시
                logError(logCode);
                updateErrorStatus();
                addErrorLog(errorData);  // 오류만 리스트에 추가
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
        clearErrorCards();
        int errorCount = 0;
        for(const QJsonObject &log : results) {
            if(log["device_id"].toString() != "conveyor_01") continue;
            if(log["log_level"].toString() != "error") continue;
            addErrorCardUI(log);
            errorCount++;
        }
        updateErrorStatus();
        qDebug() << " 최종 컨베이어 에러 로그:" << errorCount << "개 표시됨 (INF 제외)";
    }


    void ConveyorWindow::onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData){
        if(deviceId != "conveyor_01" || !textErrorStatus) {
            return;
        }

        qDebug() << "받은 통계 데이터:" << QJsonDocument(statsData).toJson(QJsonDocument::Compact);

        int currentSpeed = statsData.value("current_speed").toInt();
        int average = statsData.value("average").toInt();

        qDebug() << "파싱된 값 - 현재속도:" << currentSpeed << "평균속도:" << average;

        QString statsText = QString("현재 속도: %1\n평균 속도: %2\n불량률: 계산중...").arg(currentSpeed).arg(average);
        textErrorStatus->setText(statsText);
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
                            [this](const QList<VideoInfo>& videos) {
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
                                this->downloadAndPlayVideoFromUrl(httpUrl);
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
