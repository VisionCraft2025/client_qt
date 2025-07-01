#include "led.h"
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

LED::LED(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
    , subscription(nullptr)
    , emergencyStopActive(false) // 초기는 정상!
{
    setupUI(); // UI 설정
    setupControlButtons();
    setupMqttClient(); // mqtt 설정
    connectToMqttBroker(); // 연결 시도

    // 라파 카메라 스트리머 객체 생성 (URL은 네트워크에 맞게 수정해야 됨
    // rpiStreamer = new Streamer("rtsp://192.168.0.76:8554/stream1", this);
    // rpiStreamer = new Streamer("rtsp://192.168.219.43:8554/process1", this);

    // signal-slot
    // connect(rpiStreamer, &Streamer::newFrame, this, &LED::updateRPiImage);
    // rpiStreamer->start();

    qDebug() << "LED 모듈 초기화 완료";
}

LED::~LED()
{
    // if(rpiStreamer) {
    //     rpiStreamer->stop();
    //     rpiStreamer->wait();
    // }

    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->disconnectFromHost();
    }

    qDebug() << "LED 모듈 종료";
}

void LED::setupUI()
{
    // 메인 레이아웃 설정 (기존 UI 파일 구조 참고)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // === 상단 이벤트 배너 (topBannerWidget) ===
    QWidget *topBannerWidget = new QWidget();
    topBannerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    topBannerWidget->setStyleSheet("background-color: #f0f0f0; padding: 10px; border: 1px solid #ccc;");

    QHBoxLayout *bannerLayout = new QHBoxLayout(topBannerWidget);

    labelEvent = new QLabel("모터 속도 <b>저하</b>가 감지되었습니다");
    labelEvent->setTextFormat(Qt::RichText);
    labelEvent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    labelvc = new QLabel("VisionCraft");
    labelvc->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

    bannerLayout->addWidget(labelEvent);
    bannerLayout->addWidget(labelvc);

    // === 에러 메시지 그룹박스 (Error Message) ===
    QGroupBox *errorGroupBox = new QGroupBox("Error Message");
    QGridLayout *errorLayout = new QGridLayout(errorGroupBox);
    errorLayout->setHorizontalSpacing(0);

    // 첫 번째 행 (감지된 오류, 발생 위치)
    QWidget *errorWidget1 = new QWidget();
    QHBoxLayout *errorLayout1 = new QHBoxLayout(errorWidget1);
    QLabel *labelErrorTitle = new QLabel("감지된 오류:");
    labelErrorTitle->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelErrorValue = new QLabel("TextLabel");
    labelErrorValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    errorLayout1->addWidget(labelErrorTitle);
    errorLayout1->addWidget(labelErrorValue);

    QWidget *locationWidget = new QWidget();
    locationWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    QHBoxLayout *locationLayout = new QHBoxLayout(locationWidget);
    QLabel *labelLocationTitle = new QLabel("발생 위치:");
    labelLocationValue = new QLabel("TextLabel");
    labelLocationValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    locationLayout->addWidget(labelLocationTitle);
    locationLayout->addWidget(labelLocationValue);

    // 두 번째 행 (오류 발생 시간, 대상 카메라)
    QWidget *timeWidget = new QWidget();
    timeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *timeLayout = new QHBoxLayout(timeWidget);
    QLabel *labelTimeTitle = new QLabel("오류 발생 날짜 및 시간:");
    labelTimeTitle->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelTimeValue = new QLabel("TextLabel");
    labelTimeValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    timeLayout->addWidget(labelTimeTitle);
    timeLayout->addWidget(labelTimeValue);

    QWidget *cameraWidget = new QWidget();
    cameraWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    QHBoxLayout *cameraLayout = new QHBoxLayout(cameraWidget);
    QLabel *labelCameraTitle = new QLabel("대상 카메라:");
    labelCameraValue = new QLabel("TextLabel");
    labelCameraValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    cameraLayout->addWidget(labelCameraTitle);
    cameraLayout->addWidget(labelCameraValue);

    errorLayout->addWidget(errorWidget1, 1, 0);
    errorLayout->addWidget(locationWidget, 1, 1);
    errorLayout->addWidget(timeWidget, 2, 0);
    errorLayout->addWidget(cameraWidget, 2, 1);

    // === 카메라 섹션 (cameraSectionWidget) ===
    QWidget *cameraSectionWidget = new QWidget();
    QHBoxLayout *cameraSectionLayout = new QHBoxLayout(cameraSectionWidget);

    labelCamRPi = new QLabel("TextLabel");
    labelCamRPi->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    labelCamRPi->setStyleSheet("border: 1px solid #ccc; min-height: 200px; background-color: #f9f9f9;");
    labelCamRPi->setAlignment(Qt::AlignCenter);

    labelCamHW = new QLabel("TextLabel");
    labelCamHW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    labelCamHW->setStyleSheet("border: 1px solid #ccc; min-height: 200px; background-color: #f9f9f9;");
    labelCamHW->setAlignment(Qt::AlignCenter);

    cameraSectionLayout->addWidget(labelCamRPi);
    cameraSectionLayout->addWidget(labelCamHW);

    // === 하단 섹션 (bottomSectionWidget) ===
    QWidget *bottomSectionWidget = new QWidget();
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomSectionWidget);

    textLog = new QTextEdit();
    textLog->setPlaceholderText("로그 메시지가 여기에 표시됩니다...");

    groupControl = new QGroupBox("기기 상태 및 제어");
    groupControl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    bottomLayout->addWidget(textLog);
    bottomLayout->addWidget(groupControl);

    // 메인 레이아웃에 추가
    mainLayout->addWidget(topBannerWidget);
    mainLayout->addWidget(errorGroupBox);
    mainLayout->addWidget(cameraSectionWidget);
    mainLayout->addWidget(bottomSectionWidget);

    qDebug() << "UI 설정 완료";
}

void LED::setupMqttClient() // mqtt 클라이언트 초기 설정 MQTT 클라이언트 설정 (주소, 포트, 시그널 연결 등)
{
    m_client = new QMqttClient(this);
    reconnectTimer = new QTimer(this);
    m_client->setHostname(mqttBroker); // 브로커 서버에 연결 공용 mqtt 서버
    m_client->setPort(mqttPort);
    m_client->setClientId("VisionCraft_" + QString::number(QDateTime::currentMSecsSinceEpoch()));

    connect(m_client, &QMqttClient::connected, this, &LED::onMqttConnected); // QMqttClient가 연결이 되었다면 LED에 있는 저 함수중에 onMQTTCONNECTED를 실행
    connect(m_client, &QMqttClient::disconnected, this, &LED::onMqttDisConnected);
    // connect(m_client, &QMqttClient::messageReceived, this, &LED::onMqttMessageReceived);
    connect(reconnectTimer, &QTimer::timeout, this, &LED::connectToMqttBroker);

    qDebug() << "MQTT 클라이언트 설정 완료";
}

void LED::connectToMqttBroker() // 브로커 연결  실제 연결 시도만!
{
    if(m_client->state() == QMqttClient::Disconnected){
        m_client->connectToHost();
    }
}

void LED::onMqttConnected()
{
    qDebug() << "MQTT Connected";
    subscription = m_client->subscribe(mqttTopic);
    if(subscription){
        connect(subscription, &QMqttSubscription::messageReceived,
                this, &LED::onMqttMessageReceived);
    }
    reconnectTimer->stop(); // 연결이 성공하면 재연결 타이며 멈추기!
}

void LED::onMqttDisConnected()
{
    qDebug() << "MQTT 연결이 끊어졌습니다!";
    if(!reconnectTimer->isActive()){
        reconnectTimer->start(5000);
    }
    subscription = nullptr; // 초기화
}

void LED::onMqttMessageReceived(const QMqttMessage &message) // 매개변수 수정
{
    QString messageStr = QString::fromUtf8(message.payload());  // message.payload() 사용
    QString topicStr = message.topic().name();  // 토픽 정보도 가져올 수 있음
    qDebug() << "받은 메시지:" << topicStr << messageStr;  // 디버그 추가

    if(messageStr == "ON"){
        logMessage("led가 켜졌습니다.");
        showLedError("led가 켜졌습니다.");
    }
    else if(messageStr == "OFF"){
        logMessage("led가 꺼졌습니다.");
        showLedNormal();
    }
    else if(messageStr == "LED_POWER"){
        showLedError("LED 전원 공급 불안정");
    }
    else if(messageStr == "LED_DIM"){
        showLedError("LED 밝기 저하 감지");
    }
    else if(messageStr == "LED_HOT"){
        showLedError("LED 과열 상태");
    }
}

void LED::onMqttError(QMqttClient::ClientError error)
{
    logMessage("MQTT 에러 발생");
}

void LED::publishControlMessage(const QString &command)
{
    if(m_client && m_client->state() == QMqttClient::Connected){
        m_client->publish(mqttControllTopic, command.toUtf8());
        logMessage("제어 명령 전송: " + command);
    }
    else{
        logMessage("MQTT 연결 안됨");
    }
}

void LED::logMessage(const QString &message) // 로그 메시지 남기는 것
{
    QString timer = QDateTime::currentDateTime().toString("hh:mm:ss");
    textLog->append("[" + timer +  "]" + message);
}

void LED::showLedError(QString ledErrorType)
{
    qDebug() << "오류 상태 함수 호출됨";
    QString datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    labelEvent->setText(ledErrorType + "이(가) 감지되었습니다");
    labelErrorValue->setText(ledErrorType);
    labelTimeValue->setText(datetime);
    labelLocationValue->setText("LED 테스트 구역");
    labelCameraValue->setText("CAMERA1");

    labelCamRPi->setText("RaspberryPi CAM [오류 감지 모드]");
    labelCamHW->setText("한화비전 카메라 [추적 모드]");
}

void LED::showLedNormal()
{
    qDebug() << "정상 상태 함수 호출됨";

    labelEvent->setText("시스템이 정상 작동 중");
    labelErrorValue->setText("오류가 없습니다.");
    labelTimeValue->setText("-");
    labelLocationValue->setText("-");
    labelCameraValue->setText("-");

    labelCamRPi->setText("정상 cam");
    labelCamHW->setText("정상 카메라");
}

void LED::initializeUI()
{
    // 추가 UI 초기화가 필요한 경우 여기에 구현
}

void LED::setupControlButtons()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(groupControl);

    // QPushButton *btnLedOn = new QPushButton("LED 켜기");
    btnLedOn = new QPushButton("LED 켜기");
    mainLayout->addWidget(btnLedOn);
    connect(btnLedOn, &QPushButton::clicked, this, &LED::onLedOnClicked);

    // QPushButton *btnLedOff = new QPushButton("LED 끄기");
    btnLedOff = new QPushButton("LED 끄기");
    mainLayout->addWidget(btnLedOff);
    connect(btnLedOff, &QPushButton::clicked, this, &LED::onLedOffClicked);

    // QPushButton *btnEmergencyStop = new QPushButton("비상 정지");
    btnEmergencyStop = new QPushButton("비상 정지");
    mainLayout->addWidget(btnEmergencyStop);
    connect(btnEmergencyStop, &QPushButton::clicked, this, &LED::onEmergencyStop);

    // QPushButton *btnShutdown = new QPushButton("전원끄기");
    btnShutdown = new QPushButton("전원끄기");
    mainLayout->addWidget(btnShutdown);
    connect(btnShutdown, &QPushButton::clicked, this, &LED::onShutdown);

    // QLabel *speedTitle = new QLabel("속도제어: ");
    QLabel *speedTitle = new QLabel("속도제어: ");
    speedLabel = new QLabel("속도 : 0%");
    speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0,100);
    speedSlider->setValue(0);

    mainLayout->addWidget(speedTitle);
    mainLayout->addWidget(speedLabel);
    mainLayout->addWidget(speedSlider);
    connect(speedSlider, &QSlider::valueChanged, this, &LED::onSpeedChange);

    // QPushButton *btnSystemReset = new QPushButton("시스템 리셋");
    btnSystemReset = new QPushButton("시스템 리셋");
    mainLayout->addWidget(btnSystemReset);
    connect(btnSystemReset, &QPushButton::clicked, this, &LED::onSystemReset);

    groupControl->setLayout(mainLayout);

    qDebug() << "제어 버튼 설정 완료";
}

void LED::onLedOnClicked()
{
    qDebug()<<"led 켜기 버튼 클릭됨";
    publishControlMessage("ON");
}

void LED::onLedOffClicked()
{
    qDebug()<<"led 끄기 버튼 클릭됨";
    publishControlMessage("OFF");
}

void LED::onEmergencyStop()
{
    if(!emergencyStopActive){
        emergencyStopActive=true;

        btnLedOn->setEnabled(false);
        btnLedOff->setEnabled(false);
        btnEmergencyStop->setText("비상 정지!");
        speedSlider->setEnabled(false);

        qDebug()<<"비상 정지 버튼 클릭됨";
        publishControlMessage("EMERGENCY_STOP");
        logMessage("비상정지 명령 전송!");
    }
}

void LED::onSystemReset()
{
    emergencyStopActive= false;
    btnLedOn->setEnabled(true);
    btnLedOff->setEnabled(true);
    speedSlider->setEnabled(true);
    btnEmergencyStop->setText("비상정지");
    btnEmergencyStop->setStyleSheet("");

    qDebug()<<"다시 시작";
    publishControlMessage("reset");
    logMessage("다시 시작 전송!");
}

void LED::onShutdown()
{
    qDebug()<<"정상 종료 버튼 클릭됨";
    publishControlMessage("SHUTDOWN");
    logMessage("정상 종료 명령 전송");
}

void LED::onSpeedChange(int value)
{
    qDebug()<<"속도 변경 됨" <<value << "%";
    speedLabel->setText(QString("속도:%1%").arg(value));
    QString command = QString("SPEED_%1").arg(value);
    publishControlMessage(command);
    logMessage(QString("속도 변경: %1%").arg(value));
}

void LED::updateConnectionStatus()
{
    // 연결 상태 업데이트 로직 - 추후 구현
}

void LED::updateDeviceStatus()
{
    // 장치 상태 업데이트 로직 - 추후 구현
}

// void LED::updateRPiImage(const QImage& image)
// {
//     // 영상 QLabel에 출력
//     // labelCamRPi->setPixmap(QPixmap::fromImage(image).scaled(
//     //     labelCamRPi->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
// }
