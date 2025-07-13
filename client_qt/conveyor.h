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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "streamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ConveyorWindow; }
QT_END_NAMESPACE

class ConveyorWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConveyorWindow(QWidget *parent = nullptr);
    ~ConveyorWindow();

public slots:
    void onErrorLogsReceived(const QList<QJsonObject> &logs);  // 로그 응답 슬롯
    void onErrorLogBroadcast(const QJsonObject &errorData);

signals:
    void errorLogGenerated(const QJsonObject &errorData);     // 오류 로그 발생 시그널
    void requestErrorLogs(const QString &deviceId);           // 과거 로그 요청 시그널

private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void onConveyorOnClicked();
    void onConveyorOffClicked();
    //void onConveyorReverseClicked();
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
    QString mqttTopic = "conveyor/status";
    QString mqttControllTopic = "conveyor/cmd";

    //error message
    bool convorRunning;//hasError
    bool isConnected;
    int conveyorDirection;
    bool emergencyStopActive;

    QPushButton *btnConveyorOn;
    QPushButton *btnConveyorOff;
    //QPushButton *btnConveyorcmd;
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
    void setupRightPanel();
    void addErrorLog(const QJsonObject &errorData);
    void showConveyorError(QString conveyorErrorType="컨테이너 오류");
    void showConveyorNormal();
    void loadPastLogs();

};

#endif // CONVEYOR_H
