#ifndef HOME_H
#define HOME_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include <QtMqtt/QMqttSubscription>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "mainwindow.h"
#include "conveyor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Home; }
QT_END_NAMESPACE

class Home : public QMainWindow
{
    Q_OBJECT

public:
    Home(QWidget *parent = nullptr);
    ~Home();

    // 자식들이 사용할 메서드들
    QList<QJsonObject> getAllErrorLogs() const;
    QList<QJsonObject> getErrorLogsForDevice(const QString &deviceId) const;

public slots:
    void onErrorLogGenerated(const QJsonObject &errorData);     // 오류 로그 수신 슬롯
    void onErrorLogsRequested(const QString &deviceId);        // 로그 요청 수신 슬롯
    void onMqttPublishRequested(const QString &topic, const QString &message); // MQTT 발송 요청 슬롯

signals:
    void errorLogsResponse(const QList<QJsonObject> &logs);     // 로그 응답 시그널
    void newErrorLogBroadcast(const QJsonObject &errorData);


private slots:
    // 탭 이동 슬롯들
    void onFeederTabClicked();
    void onContainerTabClicked();
    void onFactoryToggleClicked();

    // MQTT 관련 슬롯들
    void onMqttConnected();
    void onMqttDisConnected();
    void onMqttMessageReceived(const QMqttMessage &message);
    void connectToMqttBroker();

    // stream
    void updateFeederImage(const QImage& image); // v피더캠 영상 표시
    void updateConveyorImage(const QImage& image); //컨베이어 영상
    void updateHWImage(const QImage& image); //한화 카메라

    //void onLogItemDoubleCliked(QListWidgetItem * item);
    //void onClearLogsClicked();

private:
    Ui::Home *ui;

    Streamer* feederStreamer; //피더
    Streamer* conveyorStreamer; //컨베이어
    Streamer* hwStreamer;  // 한화 카메라 스트리머

    // MQTT 관련
    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;
    QString mqttBroker = "mqtt.kwon.pics";
    int mqttPort = 1883;
    QString mqttTopic = "factory/status";
    QString mqttControlTopic = "factory/control";

    // UI 컴포넌트들
    QPushButton *btnFeederTab;
    QPushButton *btnConveyorTab;
    QPushButton *btnFactoryToggle;
    QLabel *lblConnectionStatus;
    QLabel *lblFactoryStatus;
    QTableWidget *logTable;
    QList<QJsonObject> errorLogHistory;
    //QString currentQueryId;

    // 상태 변수들
    bool factoryRunning;

    // 윈도우 포인터들
    MainWindow *feederWindow;
    ConveyorWindow *conveyorWindow;

    // 초기화 함수들
    void setupNavigationPanel();
    void setupCenterPanel();
    void setupRightPanel();
    void setupMqttClient();
    void updateFactoryStatus(bool running);
    void publicFactoryCommand(const QString &command);
    void initializeFactoryToggleButton();
    void connectChildWindow(QObject *childWindow);

    //db
    void setupLogTable();
    void addErrorLog(const QJsonObject &errorData);
    void addErrorLogUI(const QJsonObject &errorData);
    //QList<QJsonObject> pendingFeederLogs;
    //QList<QJsonObject> pendingConveyorLogs;
    void controlALLDevices(bool start);
    void initializeChildWindows();

};

#endif // HOME_H
