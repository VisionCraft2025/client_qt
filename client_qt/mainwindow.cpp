#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
//#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(nullptr)
    , subscription(nullptr)
    , emergencyStopActive(false) //초기는 정상!
{
    ui->setupUi(this);
    setWindowTitle("Feeder Control");
    setupLogWidgets();
    setupControlButtons();
    setupHomeButton();
    setupMqttClient(); //mqtt 설정
    connectToMqttBroker(); //연결 시도


    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    //rpiStreamer = new Streamer("rtsp://192.168.0.76:8554/stream1", this);

    //rpiStreamer = new Streamer("rtsp://192.168.219.43:8554/process1", this);

    // signal-slot
    //connect(rpiStreamer, &Streamer::newFrame, this, &MainWindow::updateRPiImage);
    //rpiStreamer->start();
}

MainWindow::~MainWindow()
{

    //if(rpiStreamer) {
    //    rpiStreamer->stop();
    //    rpiStreamer->wait();
    //}

    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->disconnectFromHost();
    }
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

    if(messageStr == "on"){
        logMessage("피더가 시작되었습니다.");
        logError("피더가 시작되었습니다.");
        showFeederError("피더가 시작되었습니다.");
        updateErrorStatus();
    }
    else if(messageStr == "off"){
        logMessage("피더가 정지되었습니다.");
        showFeederNormal();
    }
    else if(messageStr == "reverse"){
        logError("반대로 돌았습니다.");
        showFeederError("반대로 돌았습니다.");
        updateErrorStatus();
    }
}

void MainWindow::onMqttError(QMqttClient::ClientError error){
    logMessage("MQTT 에러 발생");

}

void MainWindow::publishControlMessage(const QString &command){
    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->publish(mqttControllTopic, command.toUtf8());
        logMessage("제어 명령 전송: " + command);
    }
    else{
        logMessage("MQTT 연결 안됨");

    }
}


void MainWindow::logMessage(const QString &message){
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog != NULL){
        textEventLog->append("[" + timer +  "]" + message);
    }
}

void MainWindow::showFeederError(QString feederErrorType){
    qDebug() << "오류 상태 함수 호출됨";
    QString datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->labelEvent->setText(feederErrorType + "이(가) 감지되었습니다");
    ui->labelErrorValue->setText(feederErrorType);
    ui->labelTimeValue->setText(datetime);
    ui->labelLocationValue->setText("피더 구역");
    ui->labelCameraValue->setText("FEEDER_CAMERA1");

    ui->labelCamRPi->setText("RaspberryPi CAM [피더 모니터링]");
    ui->labelCamHW->setText("한화비전 카메라 [피더 추적 모드]");
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
     btnFeederReverse = new QPushButton("피더 역방향");
     mainLayout->addWidget(btnFeederReverse);
     connect(btnFeederReverse, &QPushButton::clicked, this, &MainWindow::onFeederReverseClicked);

     //QPushButton *btnEmergencyStop = new QPushButton("비상 정지");
     btnEmergencyStop = new QPushButton("비상 정지");
     mainLayout->addWidget(btnEmergencyStop);
     connect(btnEmergencyStop, &QPushButton::clicked, this, &MainWindow::onEmergencyStop);

     //QPushButton *btnShutdown = new QPushButton("전원끄기");
     btnShutdown = new QPushButton("전원끄기");
     mainLayout->addWidget(btnShutdown);
     connect(btnShutdown, &QPushButton::clicked, this, &MainWindow::onShutdown);

     //QLabel *speedTitle = new QLabel("속도제어: ");
     QLabel *speedTitle = new QLabel("속도제어: ");
     speedLabel = new QLabel("속도 : 0%");
     speedSlider = new QSlider(Qt::Horizontal);
     speedSlider->setRange(0,100);
     speedSlider->setValue(0);

     mainLayout->addWidget(speedTitle);
     mainLayout->addWidget(speedLabel);
     mainLayout->addWidget(speedSlider);
     connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedChange);

     //QPushButton *btnSystemReset = new QPushButton("시스템 리셋");
     btnSystemReset = new QPushButton("시스템 리셋");
     mainLayout->addWidget(btnSystemReset);
     connect(btnSystemReset, &QPushButton::clicked, this, &MainWindow::onSystemReset);
     ui->groupControl->setLayout(mainLayout);
 }

 void MainWindow::onFeederOnClicked(){
     qDebug()<<"피더 시작 버튼 클릭됨";
     publishControlMessage("on");

 }

 void MainWindow::onFeederOffClicked(){
     qDebug()<<"피더 정지 버튼 클릭됨";
     publishControlMessage("off");
 }

 void MainWindow::onEmergencyStop(){
     if(!emergencyStopActive){
         emergencyStopActive=true;

         btnFeederOn->setEnabled(false);
         btnFeederOff->setEnabled(false);
         btnFeederReverse->setEnabled(false);
         btnEmergencyStop->setText("비상 정지!");
         speedSlider->setEnabled(false);

         qDebug()<<"비상 정지 버튼 클릭됨";
         publishControlMessage("off");//EMERGENCY_STOP
         logMessage("비상정지 명령 전송!");
     }
 }

 void MainWindow::onSystemReset(){
     emergencyStopActive= false;
     btnFeederOn->setEnabled(true);
     btnFeederOff->setEnabled(true);
     btnFeederReverse->setEnabled(true);
     speedSlider->setEnabled(true);
     btnEmergencyStop->setText("비상정지");
     btnEmergencyStop->setStyleSheet("");

     qDebug()<<"다시 시작";
     publishControlMessage("off");
     logMessage("피더 시스템 리셋 완료!");
 }

 void MainWindow::onShutdown(){
     qDebug()<<"정상 종료 버튼 클릭됨";
     publishControlMessage("off");//SHUTDOWN
    logMessage("정상 종료 명령 전송");
 }

 void MainWindow::onSpeedChange(int value){
     qDebug()<<"피더 속도 변경 됨" <<value << "%";
     speedLabel->setText(QString("피더 속도:%1%").arg(value));
     QString command = QString("SPEED_%1").arg(value);
     publishControlMessage(command);
     logMessage(QString("피더 속도 변경: %1%").arg(value));
 }

 void MainWindow::onFeederReverseClicked(){
     qDebug()<<"피더 역방향 버튼 클릭됨";
     publishControlMessage("reverse");

 }
void MainWindow::updateRPiImage(const QImage& image)
{
    // 영상 QLabel에 출력
    //ui->labelCamRPi->setPixmap(QPixmap::fromImage(image).sca(
    //    ui->labelCamRPi->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

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

void MainWindow::updateErrorStatus(){
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

void MainWindow::logError(const QString &errorType){
    errorCounts[errorType]++;
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    if(textEventLog){
        textEventLog->append("[" + timer + "] 피더 오류" + errorType);
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
