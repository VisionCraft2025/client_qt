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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(nullptr)
    , subscription(nullptr)
    , DeviceLockActive(false) //초기는 정상!
    , startDateEdit(nullptr)
    , endDateEdit(nullptr)
    , btnDateRangeSearch(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("Feeder Control");
    setupLogWidgets();
    setupControlButtons();
    setupHomeButton();
    setupRightPanel();
    setupMqttClient();

    // 로그 더블클릭 이벤트 연결
    //connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::on_listWidget_itemDoubleClicked);

    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    rpiStreamer = new Streamer("rtsp://192.168.0.76:8554/stream1", this);

    // 한화 카메라 스트리머 객체 생성
    hwStreamer = new Streamer("rtsp://192.168.0.76:8553/stream_pno", this);

    // signal-slot
    connect(rpiStreamer, &Streamer::newFrame, this, &MainWindow::updateRPiImage);
    rpiStreamer->start();

    // 한화 signal-slot 연결
    connect(hwStreamer, &Streamer::newFrame, this, &MainWindow::updateHWImage);
    hwStreamer->start();

}

MainWindow::~MainWindow()
{
    rpiStreamer->stop();
    rpiStreamer->wait();

    hwStreamer->stop();
    hwStreamer->wait();

    delete ui;
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
    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived,
                this, &MainWindow::onMqttMessageReceived);
    }

    auto statsSubscription = m_client->subscribe(QString("factory/feeder_01/msg/statistics"));
    if(statsSubscription){
        connect(statsSubscription, &QMqttSubscription::messageReceived,
                this, &MainWindow::onMqttMessageReceived);
        qDebug() << "MainWindow - feeder_01 통계 토픽 구독됨";
    }

    reconnectTimer->stop(); //연결이 성공하면 재연결 타이며 멈추기!
}

void MainWindow::onMqttDisConnected(){
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(5000);
    }
    subscription=NULL; //초기화
}

void MainWindow::onMqttMessageReceived(const QMqttMessage &message){  //매개변수 수정
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  //토픽 정보도 가져올 수 있음
    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가

    if(topicStr == "factory/feeder_01/msg/statistics") {
        qDebug() << "🎯 [DEBUG] 피더 통계 메시지 감지됨!";
        qDebug() << "  - 메시지 내용:" << messageStr;

        QJsonDocument doc = QJsonDocument::fromJson(messageStr.toUtf8());
        QJsonObject data = doc.object();
        onDeviceStatsReceived("feeder_02", data);

        logMessage(QString("피더 통계 - 평균:%1 현재:%2")
                       .arg(data["average"].toInt())
                       .arg(data["current_speed"].toInt()));
        return;
    }


    // 오류 로그 처리 - 시그널 발생
    // if(topicStr.contains("feeder") && topicStr.contains("/log/error")){
    //     QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    //     QJsonObject errorData = doc.object();

    //     // 부모에게 시그널 발생 (부모 클래스 참조 제거)
    //     emit errorLogGenerated(errorData);

    //     // 로컬 UI 업데이트
    //     addErrorLog(errorData);
    // }

    if(topicStr == "feeder_02/status"){
        if(messageStr == "on"){
            logMessage("피더가 시작되었습니다.");
            logError("피더가 시작되었습니다.");
            showFeederError("피더가 시작되었습니다.");
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
    ui->labelEvent->setText(feederErrorType + "이(가) 감지되었습니다");
    ui->labelErrorValue->setText(feederErrorType);
    ui->labelTimeValue->setText(datetime);
    ui->labelLocationValue->setText("피더 구역");
    ui->labelCameraValue->setText("FEEDER_CAMERA1");

    //ui->labelCamRPi->setText("RaspberryPi CAM [피더 모니터링]");
    //ui->labelCamHW->setText("한화비전 카메라 [피더 추적 모드]");
}

void MainWindow::showFeederNormal(){
    qDebug() << "정상 상태 함수 호출됨";

    ui->labelEvent->setText("피더 시스템이 정상 작동 중");
    ui->labelErrorValue->setText("오류가 없습니다.");
    ui->labelTimeValue->setText("-");
    ui->labelLocationValue->setText("-");
    ui->labelCameraValue->setText("-");

    ui->labelCamRPi->setText("RaspberryPi CAM [정상 모니터링]");
    ui->labelCamHW->setText("한화비전 카메라 [정상 모니터]");
}


void MainWindow::initializeUI(){

}

void MainWindow::setupControlButtons(){
    QVBoxLayout *mainLayout = new QVBoxLayout(ui->groupControl);

    //QPushButton *btnFeederOn = new QPushButton("Feeder 켜기");
    btnFeederOn = new QPushButton("피더 시작");
    mainLayout->addWidget(btnFeederOn);
    connect(btnFeederOn, &QPushButton::clicked, this, &MainWindow::onFeederOnClicked);

    //QPushButton *btnFeederOff = new QPushButton("Feeder 끄기");
    btnFeederOff = new QPushButton("피더 정지");
    mainLayout->addWidget(btnFeederOff);
    connect(btnFeederOff, &QPushButton::clicked, this, &MainWindow::onFeederOffClicked);

    //QPushButton *btnFeederOff = new QPushButton("Feeder 역방향");
    //btnFeederReverse = new QPushButton("피더 역방향");
    //mainLayout->addWidget(btnFeederReverse);
    //connect(btnFeederReverse, &QPushButton::clicked, this, &MainWindow::onFeederReverseClicked);

    //QPushButton *btnDeviceLock = new QPushButton("비상 정지");
    btnDeviceLock = new QPushButton("기기 잠금");
    mainLayout->addWidget(btnDeviceLock);
    connect(btnDeviceLock, &QPushButton::clicked, this, &MainWindow::onDeviceLock);

    //QPushButton *btnShutdown = new QPushButton("전원끄기");
    //btnShutdown = new QPushButton("전원끄기");
    //mainLayout->addWidget(btnShutdown);
    //connect(btnShutdown, &QPushButton::clicked, this, &MainWindow::onShutdown);

    //QLabel *speedTitle = new QLabel("속도제어: ");
    //QLabel *speedTitle = new QLabel("속도제어: ");
    //speedLabel = new QLabel("속도 : 0%");
    //speedSlider = new QSlider(Qt::Horizontal);
    //speedSlider->setRange(0,100);
    //speedSlider->setValue(0);

    //mainLayout->addWidget(speedTitle);
    //mainLayout->addWidget(speedLabel);
    //mainLayout->addWidget(speedSlider);
    //connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedChange);

    //QPushButton *btnSystemReset = new QPushButton("시스템 리셋");
    btnSystemReset = new QPushButton("시스템 리셋");
    mainLayout->addWidget(btnSystemReset);
    connect(btnSystemReset, &QPushButton::clicked, this, &MainWindow::onSystemReset);
    ui->groupControl->setLayout(mainLayout);
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


}

void MainWindow::onDeviceLock(){
    if(!DeviceLockActive){
        DeviceLockActive=true;

        btnFeederOn->setEnabled(false);
        btnFeederOff->setEnabled(false);
        //btnFeederReverse->setEnabled(false);
        btnDeviceLock->setText("기기 잠금!");
        //speedSlider->setEnabled(false);

        qDebug()<<"기기 잠금 버튼 클릭됨";
        //publishControlMessage("off");//기기 진행
        logMessage("기기 잠금 명령 전송!");
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

    qDebug()<<"피더 시스템 리셋";
    //publishControlMessage("off"); //기기 진행
    logMessage("피더 시스템 리셋 완료!");
}

//void MainWindow::onShutdown(){
//   qDebug()<<"정상 종료 버튼 클릭됨";
//   publishControlMessage("off");//SHUTDOWN
//   logMessage("정상 종료 명령 전송");
//}

// void MainWindow::onSpeedChange(int value){
//     qDebug()<<"피더 속도 변경 됨" <<value << "%";
//     speedLabel->setText(QString("피더 속도:%1%").arg(value));
//     QString command = QString("SPEED_%1").arg(value);
//     publishControlMessage(command);
//     logMessage(QString("피더 속도 변경: %1%").arg(value));
// }

// void MainWindow::onFeederReverseClicked(){
//     qDebug()<<"피더 역방향 버튼 클릭됨";
//     publishControlMessage("reverse");

// }

void MainWindow::setupHomeButton(){

    QHBoxLayout *topLayout = qobject_cast<QHBoxLayout*>(ui->topBannerWidget->layout());

    btnbackhome = new QPushButton("홈화면으로 이동");
    topLayout->insertWidget(0, btnbackhome);
    connect(btnbackhome, &QPushButton::clicked, this, &MainWindow::gobackhome);
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


//실시간 에러 로그 + 통계
void MainWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 피더 오류 " + errorType);
    }
}
void MainWindow::setupLogWidgets(){
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
            QString initialText = "현재 속도: 로딩중...\n";
            initialText += "평균 속도: 로딩중...";
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

//로그
void MainWindow::setupRightPanel() {
    // 1. ERROR LOG 라벨 추가
    static QLabel* errorLogLabel = nullptr;
    QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(ui->widget_6->layout());
    if (!rightLayout) {
        rightLayout = new QVBoxLayout(ui->widget_6);
        ui->widget_6->setLayout(rightLayout);
    }
    if (!errorLogLabel) {
        errorLogLabel = new QLabel("ERROR LOG");
        errorLogLabel->setStyleSheet(R"(
            color: #fb923c;
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
    if (!ui->lineEdit) ui->lineEdit = new QLineEdit();
    if (!ui->pushButton) ui->pushButton = new QPushButton();
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
    disconnect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    QWidget* searchContainer = new QWidget();
    QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(ui->lineEdit);
    searchLayout->addWidget(ui->pushButton);
    rightLayout->insertWidget(1, searchContainer);

    // 3. 날짜 필터(QGroupBox) home.cpp 스타일 적용
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
    if (!startDateEdit) startDateEdit = new QDateEdit(QDate::currentDate());
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("MM-dd");
    startDateEdit->setStyleSheet(dateEditStyle);
    startDateEdit->setFixedWidth(90);
    startCol->addWidget(startLabel);
    startCol->addWidget(startDateEdit);

    // 종료일
    QVBoxLayout* endCol = new QVBoxLayout();
    QLabel* endLabel = new QLabel("종료일:");
    endLabel->setStyleSheet("color: #6b7280; font-size: 12px; background: transparent;");
    if (!endDateEdit) endDateEdit = new QDateEdit(QDate::currentDate());
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat("MM-dd");
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

    // 삽입: 검색창 아래, 카드 스크롤 영역 위
    rightLayout->insertWidget(2, dateGroup);

    // 시그널 연결 (로직 유지)
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        QString searchText = ui->lineEdit ? ui->lineEdit->text().trimmed() : "";
        QDate start = startDateEdit ? startDateEdit->date() : QDate();
        QDate end = endDateEdit ? endDateEdit->date() : QDate();
        emit requestFeederLogSearch(searchText, start, end);
    });
    connect(resetDateBtn, &QPushButton::clicked, this, [this]() {
        if(startDateEdit && endDateEdit) {
            startDateEdit->setDate(QDate::currentDate());
            endDateEdit->setDate(QDate::currentDate());
        }
        if(ui->lineEdit) ui->lineEdit->clear();
        emit requestFeederLogSearch("", QDate(), QDate());
    });

    // 4. QScrollArea+QVBoxLayout(카드 쌓기) 구조 적용
    errorScrollArea = ui->scrollArea; // 반드시 .ui의 scrollArea 사용
    errorCardContent = new QWidget();
    errorCardLayout = new QVBoxLayout(errorCardContent);
    errorCardLayout->setSpacing(6);
    errorCardLayout->setContentsMargins(4, 2, 4, 4);
    errorCardContent->setLayout(errorCardLayout);
    errorScrollArea->setWidget(errorCardContent);

    // 기존 QListWidget 숨기기
    //if (ui->listWidget) ui->listWidget->hide();
}

void MainWindow::addErrorLog(const QJsonObject &errorData){
    //if(!ui->listWidget) return;

    QString currentTime = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    QString logText = QString("[%1] %2")
                          .arg(currentTime)
                          .arg(errorData["log_code"].toString());

    //QListWidgetItem *item = new QListWidgetItem(logText);
    //item->setData(Qt::UserRole, errorData["error_log_id"].toString());
    //ui->listWidget->insertItem(0, item);

    //ui->listWidget->insertItem(0, logText);

    //if(ui->listWidget->count() > 20){
    //    delete ui->listWidget->takeItem(20);
    //}

    //ui->listWidget->setCurrentRow(0);
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
}

void MainWindow::onErrorLogBroadcast(const QJsonObject &errorData){
    qDebug() << "브로드캐스트 수신됨!"<<errorData;
    QString deviceId = errorData["device_id"].toString();
    if(deviceId == "feeder_01"){
        QString logCode = errorData["log_code"].toString();
        this->setWindowTitle("브로드캐스트 받음: " + logCode + " - " + QTime::currentTime().toString());
        showFeederError(logCode);
        logError(logCode);
        updateErrorStatus();
        // addErrorLog(errorData);
        addErrorCardUI(errorData);
        qDebug() << "MainWindow - 실시간 피더 로그 추가:" << logCode;
    } else {
        qDebug() << "MainWindow - 다른 디바이스 로그 무시:" << deviceId;
    }
}

void MainWindow::onSearchClicked() {
    qDebug() << " MainWindow 피더 검색 시작!";
    qDebug() << "함수 시작 - 현재 시간:" << QDateTime::currentDateTime().toString();

    //  UI 컴포넌트 존재 확인
    if(!ui->lineEdit) {
        qDebug() << " lineEdit null!";
        QMessageBox::warning(this, "UI 오류", "검색 입력창이 초기화되지 않았습니다.");
        return;
    }

    //if(!ui->listWidget) {
    //    qDebug() << " listWidget null!";
    //    QMessageBox::warning(this, "UI 오류", "결과 리스트가 초기화되지 않았습니다.");
    //    return;
    //}

    //  검색어 가져오기
    QString searchText = ui->lineEdit->text().trimmed();
    qDebug() << " 피더 검색어:" << searchText;

    //  날짜 위젯 확인 및 기본값 설정
    if(!startDateEdit || !endDateEdit) {
        qDebug() << " 피더 날짜 위젯이 null입니다!";
        qDebug() << "startDateEdit:" << startDateEdit;
        qDebug() << "endDateEdit:" << endDateEdit;
        QMessageBox::warning(this, "UI 오류", "날짜 선택 위젯이 초기화되지 않았습니다.");
        return;
    }

    QDate startDate = startDateEdit->date();
    QDate endDate = endDateEdit->date();

    qDebug() << " 피더 검색 조건:";
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
        qDebug() << " 종료일이 현재 날짜보다 미래임 - 현재 날짜로 조정";
        endDate = currentDate;
        endDateEdit->setDate(endDate);
    }

    //  검색 진행 표시
    //ui->listWidget->clear();
    //ui->listWidget->addItem(" 검색 중... 잠시만 기다려주세요.");
    //ui->pushButton->setEnabled(false);  // 중복 검색 방지

    qDebug() << " 피더 통합 검색 요청 - Home으로 시그널 전달";

    //  검색어와 날짜 모두 전달
    emit requestFeederLogSearch(searchText, startDate, endDate);

    qDebug() << " 피더 검색 시그널 발송 완료";

    //  타임아웃 설정 (30초 후 버튼 재활성화)
    QTimer::singleShot(30000, this, [this]() {
        if(!ui->pushButton->isEnabled()) {
            qDebug() << " 검색 타임아웃 - 버튼 재활성화";
            ui->pushButton->setEnabled(true);

            //if(ui->listWidget && ui->listWidget->count() == 1) {
            //    QString firstItem = ui->listWidget->item(0)->text();
            //    if(firstItem.contains("검색 중")) {
            //        ui->listWidget->clear();
            //        ui->listWidget->addItem(" 검색 시간이 초과되었습니다. 다시 시도해주세요.");
            //    }
            //}a
        }
    });
}

void MainWindow::onSearchResultsReceived(const QList<QJsonObject> &results) {
    qDebug() << " 피더 검색 결과 수신됨: " << results.size() << "개";

    // 버튼 재활성화
    if(ui->pushButton) {
        ui->pushButton->setEnabled(true);
    }

    //if(!ui->listWidget) {
    //    qDebug() << " listWidget이 null입니다!";
    //    return;
    //}

    //ui->listWidget->clear();

    if(results.isEmpty()) {
        //ui->listWidget->addItem(" 검색 조건에 맞는 피더 로그가 없습니다.");
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

        //ui->listWidget->addItem(logText);
        errorCount++;

        // 통계 업데이트
        if(!logCode.isEmpty()) {
            logError(logCode);
            showFeederError(logCode);
        }

        qDebug() << " 에러 로그 추가:" << logText;
    }

    updateErrorStatus();
    qDebug() << " 최종 에러 로그:" << errorCount << "개 표시됨 (INF 제외)";
}

void MainWindow::updateErrorStatus(){

}

void MainWindow::onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData) {
    qDebug() << "📊 [DEBUG] MainWindow 통계 수신됨!";
    qDebug() << "  - deviceId:" << deviceId;
    qDebug() << "  - statsData:" << QJsonDocument(statsData).toJson(QJsonDocument::Compact);

    if(deviceId != "feeder_01") {
        qDebug() << "  - 피더가 아님, 무시";
        return;
    }

    if(!textErrorStatus) {
        qDebug() << "  - textErrorStatus가 null!";
        return;
    }

    // ✅ 각 값이 실제로 있는지 확인
    bool hasCurrentSpeed = statsData.contains("current_speed");
    bool hasAverage = statsData.contains("average");

    qDebug() << "  - current_speed 존재:" << hasCurrentSpeed;
    qDebug() << "  - average 존재:" << hasAverage;

    if(hasCurrentSpeed) {
        qDebug() << "  - current_speed 값:" << statsData["current_speed"];
    }
    if(hasAverage) {
        qDebug() << "  - average 값:" << statsData["average"];
    }

    int currentSpeed = statsData.value("current_speed").toInt();
    int average = statsData.value("average").toInt();

    qDebug() << "  - 최종 currentSpeed:" << currentSpeed;
    qDebug() << "  - 최종 average:" << average;

    QString statsText = QString("현재 속도: %1\n평균 속도: %2").arg(currentSpeed).arg(average);
    textErrorStatus->setText(statsText);

    qDebug() << "📊 [DEBUG] MainWindow 통계 업데이트 완료!";
}


// 로그 더블클릭 시 영상 재생
//void MainWindow::on_listWidget_itemDoubleClicked(QListWidgetItem* item) {
//    static bool isProcessing = false;
//    if (isProcessing) return;
//    isProcessing = true;

//    QString errorLogId = item->data(Qt::UserRole).toString();
//    QString logText = item->text();

//    // 로그 형식 파싱
//    QRegularExpression re(R"(\[(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})\])");
//    QRegularExpressionMatch match = re.match(logText);

//    QString month, day, hour, minute, second = "00";
//    QString deviceId = "feeder_01"; // 피더 화면에서는 항상 feeder_02

//    if (match.hasMatch()) {
//        month = match.captured(1);
//        day = match.captured(2);
//        hour = match.captured(3);
//        minute = match.captured(4);
//        second = match.captured(5);
//    } else {
//        QMessageBox::warning(this, "형식 오류", "로그 형식을 해석할 수 없습니다.\n로그: " + logText);
//        isProcessing = false;
//        return;
//    }

//    // 현재 년도 사용
//    int currentYear = QDateTime::currentDateTime().date().year();
//    QDateTime timestamp = QDateTime::fromString(
//        QString("%1%2%3%4%5%6").arg(currentYear).arg(month,2,'0').arg(day,2,'0')
//            .arg(hour,2,'0').arg(minute,2,'0').arg(second,2,'0'),
//        "yyyyMMddhhmmss");

//    qint64 startTime = timestamp.addSecs(-60).toMSecsSinceEpoch();
//    qint64 endTime = timestamp.addSecs(+300).toMSecsSinceEpoch();

//    VideoClient* client = new VideoClient(this);
//    client->queryVideos(deviceId, "", startTime, endTime, 1,
//                        [this](const QList<VideoInfo>& videos) {
//                            //static bool isProcessing = false;
//                            isProcessing = false; // 재설정

//                            if (videos.isEmpty()) {
//                                QMessageBox::warning(this, "영상 없음", "해당 시간대에 영상을 찾을 수 없습니다.");
//                                return;
//                            }

//                            QString httpUrl = videos.first().http_url;
//                            this->downloadAndPlayVideoFromUrl(httpUrl);
//                        });
//}

// 영상 다운로드 및 재생
void MainWindow::downloadAndPlayVideoFromUrl(const QString& httpUrl) {
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

void MainWindow::addErrorCardUI(const QJsonObject &errorData) {
    if (errorData["device_id"].toString() != "feeder_01") return;
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
        connect(filter, &CardEventFilter::cardDoubleClicked, this, &MainWindow::onCardDoubleClicked);
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
    timeLabel->setMaximumWidth(70);
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
    if (errorCardLayout) {
        errorCardLayout->insertWidget(0, card);
    }
}

void MainWindow::onCardDoubleClicked(QObject* cardWidget) {
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
