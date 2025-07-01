#ifndef LEDWIDGET_H
#define LEDWIDGET_H

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
#include <QImage>

QT_BEGIN_NAMESPACE
namespace Ui { class LedWidget; }
QT_END_NAMESPACE

class LedWidget : public QWidget
{
    Q_OBJECT

public:
    LedWidget(QWidget *parent = nullptr);
    ~LedWidget();

signals:
    void backToHome();  // 홈으로 돌아가기 시그널 (나중에 추가)

private slots:
    void onMqttConnected();
    void onMqttDisConnected();
    void onMqttMessageReceived(const QMqttMessage &message);
    void onMqttError(QMqttClient::ClientError error);
    void connectToMqttBroker();
    void onLedOnClicked();
    void onLedOffClicked();
    void onEmergencyStop();
    void onShutdown();
    void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image);

private:
    Ui::LedWidget *ui;
    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;
    //Streamer* rpiStreamer;  // 나중에 활성화

    QString mqttBroker = "mqtt.eclipseprojects.io";
    int mqttPort = 1883;
    QString mqttTopic = "myled/status";
    QString mqttControllTopic = "myled/control";

    //error message
    bool hasError;
    bool isConnected;
    bool emergencyStopActive;

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

    void logMessage(const QString &message);
    void setupMqttClient();
    void initializeUI();
    void setupControlButtons();
    void updateConnectionStatus();
    void updateDeviceStatus();
    void publishControlMessage(const QString &command);
    void showLedError(QString ledErrorType="LED 오류");
    void showLedNormal();
};

#endif // LEDWIDGET_H
