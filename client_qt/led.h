#ifndef LED_H
#define LED_H

#include <QWidget>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include <QtMqtt/QMqttSubscription>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QTextEdit>
#include <QGroupBox>
#include <QGridLayout>
#include <QImage>

// #include "streamer.h" // 추후 스트리머 기능 추가 시 사용

class LED : public QWidget
{
    Q_OBJECT

public:
    explicit LED(QWidget *parent = nullptr);
    ~LED();

private slots: // 행동하는 것
    void onMqttConnected(); // 연결 되었는지
    void onMqttDisConnected(); // 연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); // 메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); // 에러 났을 때
    void connectToMqttBroker(); // 브로커 연결

    void onLedOnClicked();
    void onLedOffClicked();
    void onEmergencyStop();
    void onShutdown();
    void onSpeedChange(int value);
    void onSystemReset();

    // void updateRPiImage(const QImage& image); // 라파캠 영상 표시 - 추후 구현

private:
    // MQTT 관련
    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;

    // Streamer* rpiStreamer; // 라즈베리파이 카메라 스트리머 - 추후 구현

    QString mqttBroker = "mqtt.eclipseprojects.io"; // 공용 mqtt 서버
    int mqttPort = 1883;
    QString mqttTopic = "myled/status";
    QString mqttControllTopic = "myled/control";

    // 상태 변수 (error message)
    bool hasError;
    bool isConnected;
    bool emergencyStopActive; // 초기는 정상!

    // UI 요소들
    QPushButton *btnLedOn;
    QPushButton *btnLedOff;
    QPushButton *btnEmergencyStop;
    QPushButton *btnShutdown;
    QLabel *lblConnectionStatus;
    QLabel *lblDeviceStatus;
    QProgressBar *progressActivity;
    QSlider *speedSlider;
    QLabel *speedLabel;
    QPushButton *btnSystemReset;
    QTextEdit *textLog;
    QGroupBox *groupControl;

    // 에러 메시지 UI (기존 UI 파일 구조 참고)
    QLabel *labelEvent;        // 상단 이벤트 메시지
    QLabel *labelvc;           // VisionCraft 라벨
    QLabel *labelErrorValue;   // 감지된 오류 값
    QLabel *labelTimeValue;    // 오류 발생 시간
    QLabel *labelLocationValue; // 발생 위치
    QLabel *labelCameraValue;  // 대상 카메라
    QLabel *labelCamRPi;       // 라즈베리파이 캠
    QLabel *labelCamHW;        // 한화비전 카메라

    // 메소드들
    void setupUI(); // UI 초기화
    void setupMqttClient(); // mqtt 클라이언트 초기 설정 (주소, 포트, 시그널 연결 등)
    void setupControlButtons(); // 제어 버튼 설정
    void initializeUI();
    void updateConnectionStatus();
    void updateDeviceStatus();

    void logMessage(const QString &message); // 로그 메시지 남기는 것
    void publishControlMessage(const QString &command);

    // error message 함수
    void showLedError(QString ledErrorType = "LED 오류");
    void showLedNormal();
};

#endif // LED_H
