#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(nullptr)
    , subscription(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("led");
    setupMqttClient(); //mqtt 설정
    connectToMqttBroker(); //연결 시도
}

MainWindow::~MainWindow()
{
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
    m_client->setClientId("VisionCraft_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
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
    qDebug() << "MQTT Connected";
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

void MainWindow::onMqttMessageReceived(const QMqttMessage &message){  // 🔧 매개변수 수정
    QString messageStr = QString::fromUtf8(message.payload());  // 🔧 message.payload() 사용
    QString topicStr = message.topic().name();  // 🔧 토픽 정보도 가져올 수 있음

    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 🔧 디버그 추가

    if(messageStr == "ON"){
        logMessage("led가 켜졌습니다.");
    }
    else if(messageStr == "OFF"){
        logMessage("led가 꺼졌습니다.");
    }
}

void MainWindow::onMqttError(QMqttClient::ClientError error){
    logMessage("MQTT 에러 발생");
}


void MainWindow::logMessage(const QString &message){
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->append("[" + timer +  "]" + message);
}

