#ifndef CONVEYOR_H
#define CONVEYOR_H

#include <QMainWindow>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include <QtMqtt/QMqttSubscription>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QSlider>
#include <QImage>
#include <QMap>
#include <QSplitter>
#include <QGroupBox>
#include "streamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ConveyorWindow; }
QT_END_NAMESPACE

class ConveyorWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConveyorWindow(QWidget *parent = nullptr);
    //MainWindow(Home *homeParnet);
    ~ConveyorWindow();

private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void oncontayorOnClicked();
    void oncontayorOffClicked();
    void oncontayorReverseClicked();
    void onEmergencyStop();
    void onShutdown();
    void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시
    void updateHWImage(const QImage& image); //한화 카메라
    void gobackhome();

private:
    Ui::ConveyorWindow *ui;
    Streamer* rpiStreamer;
    Streamer* hwStreamer;  // 한화 카메라 스트리머

    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;

    QString mqttBroker = "mqtt.kwon.pics";
    int mqttPort = 1883;
    QString mqttTopic = "contayor/status";
    QString mqttControllTopic = "contayor/cmd";

    //error message
    bool contayorRunning;//hasError
    bool isConnected;
    int contayorDirection;
    bool emergencyStopActive;

    QPushButton *btncontayorOn;
    QPushButton *btncontayorOff;
    QPushButton *btncontayorReverse;
    QPushButton *btnEmergencyStop;
    QPushButton *btnShutdown;
    QLabel *lblConnectionStatus;
    QLabel *lblDeviceStatus;
    QProgressBar *progressActivity;
    QSlider *speedSlider;
    QLabel *speedLabel;
    QPushButton *btnSystemReset;
    QPushButton *btnbackhome;
    QTextEdit *textEventLog;
    QTextEdit *textErrorStatus;
    QMap<QString, int> errorCounts;

    //Home *homeWindow;

    void logMessage(const QString &message); //로그 메시지 남기는 것
    void setupMqttClient();
    void initializeUI();
    void setupControlButtons();
    void updateConnectionStatus();
    void updateDeviceStatus();
    void setupLogWidgets(); //로그 위젯 추가함
    void publishControlMessage(const QString &command);
    void backhome();
    void setupHomeButton();
    void logError(const QString &errorType);
    void updateErrorStatus();

    //error message 함수
    void showcontayorError(QString contayorErrorType="피더 오류");
    void showcontayorNormal();

};

#endif // CONVEYOR_H
