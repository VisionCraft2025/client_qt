// #ifndef LED_H
// #define LED_H

// #include <QObject>
// #include <QtMqtt/QMqttClient>
// #include <QtMqtt/QMqttMessage>
// #include <QtMqtt/QMqttSubscription>
// #include <QTimer>
// #include <QPushButton>
// #include <QSlider>
// #include <QLabel>
// #include <QVBoxLayout>

// class LedController : public QObject
// {
//     Q_OBJECT

// public:
//     explicit LedController(QObject *parent = nullptr);
//     ~LedController();

//     // UI 설정
//     void setupControlUI(QWidget *parentWidget);
//     void setupMqttClient();
//     void connectToMqttBroker();

// signals:
//     void statusChanged(const QString &status);
//     void errorOccurred(const QString &errorType);
//     void normalStatus();
//     void logMessage(const QString &message);

// private slots:
//     void onMqttConnected();
//     void onMqttDisconnected();
//     void onMqttMessageReceived(const QMqttMessage &message);
//     void onMqttError(QMqttClient::ClientError error);
//     void onLedOnClicked();
//     void onLedOffClicked();
//     void onEmergencyStop();
//     void onShutdown();
//     void onSpeedChange(int value);
//     void onSystemReset();

// private:
//     QMqttClient *m_client;
//     QMqttSubscription *subscription;
//     QTimer *reconnectTimer;

//     QString mqttBroker = "mqtt.eclipseprojects.io";
//     int mqttPort = 1883;
//     QString mqttTopic = "myled/status";
//     QString mqttControlTopic = "myled/control";

//     bool emergencyStopActive;

//     // UI 컴포넌트
//     QPushButton *btnLedOn;
//     QPushButton *btnLedOff;
//     QPushButton *btnEmergencyStop;
//     QPushButton *btnShutdown;
//     QPushButton *btnSystemReset;
//     QSlider *speedSlider;
//     QLabel *speedLabel;

//     void publishControlMessage(const QString &command);
// };

// #endif // LED_H
