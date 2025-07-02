#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
//#include "streamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    //MainWindow(Home *homeParnet);
    ~MainWindow();

private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void onLedOnClicked();
    void onLedOffClicked();
    void onEmergencyStop();
    void onShutdown();
    void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시
    void gobackhome();

private:
    Ui::MainWindow *ui;
    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;
    //Streamer* rpiStreamer;

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
    void showLedError(QString ledErrorType="LED 오류");
    void showLedNormal();

};

#endif // MAINWINDOW_H
