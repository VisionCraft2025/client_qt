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
    , emergencyStopActive(false) //초기는 정상!
{
    ui->setupUi(this);
    setWindowTitle("Conveyor Control");
    setupLogWidgets();
    setupControlButtons();

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
    m_client->setClientId("VisionCraft_contayor" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    connect(m_client, &QMqttClient::connected, this, &ConveyorWindow::onMqttConnected); // QMqttClient가 연결이 되었다면 ConveyorWindow에 있는 저 함수중에 onMQTTCONNECTED를 실행
    connect(m_client, &QMqttClient::disconnected, this, &ConveyorWindow::onMqttDisConnected);
    //connect(m_client, &QMqttClient::messageReceived, this, &ConveyorWindow::onMqttMessageReceived);
    connect(reconnectTimer, &QTimer::timeout, this, &ConveyorWindow::connectToMqttBroker);
}

void ConveyorWindow::connectToMqttBroker(){ //브로커 연결  실제 연결 시도만!

    if(m_client->state() == QMqttClient::Disconnected){
        m_client->connectToHost();
    }

}

void ConveyorWindow::onMqttConnected(){
    qDebug() << "MQTT Connected - contayor Control";
    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived,
                this, &ConveyorWindow::onMqttMessageReceived);
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
        logMessage("컨테이너가 시작되었습니다.");
        logError("컨테이너가 시작되었습니다.");
        showConveyorError("컨테이너가 시작되었습니다.");
        updateErrorStatus();
    }
    else if(messageStr == "off"){
        logMessage("컨테이너가 정지되었습니다.");
        showConveyorNormal();
    }
    else if(messageStr == "reverse"){
        logError("반대로 돌았습니다.");
        showConveyorError("반대로 돌았습니다.");
        updateErrorStatus();
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
    ui->labelEvent->setText(conveyorErrorType + "이(가) 감지되었습니다");
    ui->labelErrorValue->setText(conveyorErrorType);
    ui->labelTimeValue->setText(datetime);
    ui->labelLocationValue->setText("컨테이너 구역");
    ui->labelCameraValue->setText("contayor_CAMERA1");

    ui->labelCamRPi->setText("RaspberryPi CAM [컨테이너 모니터링]");
    ui->labelCamHW->setText("한화비전 카메라 [컨테이너 추적 모드]");
}

void ConveyorWindow::showConveyorNormal(){
    qDebug() << "정상 상태 함수 호출됨";

    ui->labelEvent->setText("컨테이너 시스템이 정상 작동 중");
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

    //QPushButton *btnConveyorOn = new QPushButton("contayor 켜기");
    btnConveyorOn = new QPushButton("컨테이너 시작");
    mainLayout->addWidget(btnConveyorOn);
    connect(btnConveyorOn, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOnClicked);

    //QPushButton *btnConveyorOff = new QPushButton("contayor 끄기");
    btnConveyorOff = new QPushButton("컨테이너 정지");
    mainLayout->addWidget(btnConveyorOff);
    connect(btnConveyorOff, &QPushButton::clicked, this, &ConveyorWindow::onConveyorOffClicked);

    //QPushButton *btnConveyorOff = new QPushButton("contayor 역방향");
    // btnConveyorReverse = new QPushButton("컨테이너 역방향");
    // mainLayout->addWidget(btnConveyorReverse);
    // connect(btnConveyorReverse, &QPushButton::clicked, this, &ConveyorWindow::onConveyorReverseClicked);

    //QPushButton *btnEmergencyStop = new QPushButton("비상 정지");
    btnEmergencyStop = new QPushButton("비상 정지");
    mainLayout->addWidget(btnEmergencyStop);
    connect(btnEmergencyStop, &QPushButton::clicked, this, &ConveyorWindow::onEmergencyStop);

    //QPushButton *btnShutdown = new QPushButton("전원끄기");
    btnShutdown = new QPushButton("전원끄기");
    mainLayout->addWidget(btnShutdown);
    connect(btnShutdown, &QPushButton::clicked, this, &ConveyorWindow::onShutdown);

    //QLabel *speedTitle = new QLabel("속도제어: ");
    QLabel *speedTitle = new QLabel("속도제어: ");
    speedLabel = new QLabel("속도 : 0%");
    speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0,100);
    speedSlider->setValue(0);

    mainLayout->addWidget(speedTitle);
    mainLayout->addWidget(speedLabel);
    mainLayout->addWidget(speedSlider);
    connect(speedSlider, &QSlider::valueChanged, this, &ConveyorWindow::onSpeedChange);

    //QPushButton *btnSystemReset = new QPushButton("시스템 리셋");
    btnSystemReset = new QPushButton("시스템 리셋");
    mainLayout->addWidget(btnSystemReset);
    connect(btnSystemReset, &QPushButton::clicked, this, &ConveyorWindow::onSystemReset);
    ui->groupControl->setLayout(mainLayout);
}

void ConveyorWindow::onConveyorOnClicked(){
    qDebug()<<"컨테이너 시작 버튼 클릭됨";
    publishControlMessage("on");

}

void ConveyorWindow::onConveyorOffClicked(){
    qDebug()<<"컨테이너 정지 버튼 클릭됨";
    publishControlMessage("off");
}

void ConveyorWindow::onEmergencyStop(){
    if(!emergencyStopActive){
        emergencyStopActive=true;

        btnConveyorOn->setEnabled(false);
        btnConveyorOff->setEnabled(false);
        btnEmergencyStop->setText("비상 정지!");
        speedSlider->setEnabled(false);

        qDebug()<<"비상 정지 버튼 클릭됨";
        publishControlMessage("off");//EMERGENCY_STOP
        logMessage("비상정지 명령 전송!");
    }
}

void ConveyorWindow::onSystemReset(){
    emergencyStopActive= false;
    btnConveyorOn->setEnabled(true);
    btnConveyorOff->setEnabled(true);
    //btnConveyorReverse->setEnabled(true);
    speedSlider->setEnabled(true);
    btnEmergencyStop->setText("비상정지");
    btnEmergencyStop->setStyleSheet("");

    qDebug()<<"다시 시작";
    publishControlMessage("off");
    logMessage("컨테이너 시스템 리셋 완료!");
}

void ConveyorWindow::onShutdown(){
    qDebug()<<"정상 종료 버튼 클릭됨";
    publishControlMessage("off");//SHUTDOWN
    logMessage("정상 종료 명령 전송");
}

void ConveyorWindow::onSpeedChange(int value){
    qDebug()<<"컨테이너 속도 변경 됨" <<value << "%";
    speedLabel->setText(QString("컨테이너 속도:%1%").arg(value));
    QString cmd = QString("SPEED_%1").arg(value);
    publishControlMessage(cmd);
    logMessage(QString("컨테이너 속도 변경: %1%").arg(value));
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

void ConveyorWindow::updateErrorStatus(){
    if(!textErrorStatus){
        return;
    }

    QString statsText;

    if(errorCounts.isEmpty()){
        statsText = "오류없음";
    }else{
        for(const QString& errorType : errorCounts.keys()){
            int count = errorCounts[errorType];
            statsText += QString("- %1: %2회\n")
                             .arg(errorType)
                             .arg(count);
        }

        QString mostFrequentError;
        int maxCount =0;

        for(const QString& errorType : errorCounts.keys()){
            int count = errorCounts[errorType];
            if(count > maxCount){
                maxCount = count;
                mostFrequentError = errorType;
            }
        }

        if(!mostFrequentError.isEmpty()){
            statsText += QString("\n 가장 빈번한 오류: %1")
                             .arg(mostFrequentError);
        }
    }

    textErrorStatus->setText(statsText);
}

void ConveyorWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 컨테이너 오류" + errorType);
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

        QGroupBox *statusGroup = new QGroupBox("오류 통계");
        QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
        textErrorStatus = new QTextEdit();
        textErrorStatus->setReadOnly(true);
        textErrorStatus->setMaximumWidth(300);
        statusLayout->addWidget(textErrorStatus);

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
    if(ui->label){
        ui->label->setText("컨테이너 오류 로그");
        ui->label->setStyleSheet("font-weight: bold; font-size: 14px;");
    }

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
}

void ConveyorWindow::addErrorLog(const QJsonObject &errorData){
    if(!ui->listWidget) return;

    QString currentTime = QDateTime::currentDateTime().toString("MM:dd hh:mm:ss");
    QString logText = QString("%1 [%2]")
                          .arg(errorData["log_code"].toString())
                          .arg(currentTime);

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
    emit requestErrorLogs("conveyor_02");
}

// 부모로부터 로그 응답 받는 슬롯
void ConveyorWindow::onErrorLogsReceived(const QList<QJsonObject> &logs){
    if(!ui->listWidget) return;
    QList<QJsonObject> conveyorLogs;
    for(const QJsonObject &log : logs) {
        if(log["device_id"].toString() == "conveyor_02") {
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
        qint64 timestamp = log["timestamp"].toVariant().toLongLong();
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString logTime = dateTime.toString("MM-dd hh:mm:ss");

        QString logText = QString("[%1] %2")
                              .arg(logTime)
                              .arg(log["log_code"].toString());

        QListWidgetItem *item = new QListWidgetItem(logText);
        item->setData(Qt::UserRole, log["error_log_id"].toString());
        ui->listWidget->addItem(item);
        qDebug() << "ConveyorWindow - 컨베이어 로그 추가:" << logText;
    }

    qDebug() << "ConveyorWindow - 최종 로그 개수:" << ui->listWidget->count() << "개";

}

void ConveyorWindow::onErrorLogBroadcast(const QJsonObject &errorData){
    QString deviceId = errorData["device_id"].toString();

    if(deviceId == "conveyor_02"){
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
    QString deviceId = "conveyor_02"; // 컨베이어 화면에서는 항상 conveyor_02

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
    qDebug() << "📡 요청 URL:" << httpUrl;

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
