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
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSplitter>
#include <QGroupBox>
#include "streamer.h"
#include <qlistwidget.h>


class Home;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void onErrorLogsReceived(const QList<QJsonObject> &logs);  // 로그 응답 슬롯
    void onErrorLogBroadcast(const QJsonObject &errorData);

signals:
    void errorLogGenerated(const QJsonObject &errorData);     // 오류 로그 발생 시그널
    void requestErrorLogs(const QString &deviceId);           // 과거 로그 요청 시그널
    void requestMqttPublish(const QString &topic, const QString &message); // MQTT 발송 요청

private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void onFeederOnClicked();
    void onFeederOffClicked();
    void onFeederReverseClicked();
    void onEmergencyStop();
    void onShutdown();
    void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시
    void updateHWImage(const QImage& image); //한화 카메라
    void gobackhome();
    void on_listWidget_itemDoubleClicked(QListWidgetItem* item);

private:
    Ui::MainWindow *ui;
    Streamer* rpiStreamer;
    Streamer* hwStreamer;  // 한화 카메라 스트리머

    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;
    //Home* parentHome;

    QString mqttBroker = "mqtt.kwon.pics";
    int mqttPort = 1883;
    QString mqttTopic = "feeder_01/status";
    QString mqttControllTopic = "feeder_01/cmd";

    //error message
    bool feederRunning;//hasError
    bool isConnected;
    int feederDirection;
    bool emergencyStopActive;

    QPushButton *btnFeederOn;
    QPushButton *btnFeederOff;
    QPushButton *btnFeederReverse;
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

    //error message 함수
    void showFeederError(QString feederErrorType="피더 오류");
    void showFeederNormal();
    void loadPastLogs();


    void downloadAndPlayVideoFromUrl(const QString& httpUrl);

};

#endif // MAINWINDOW_H
