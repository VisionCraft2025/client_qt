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
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
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

    //void onLogItemDoubleCliked(QListWidgetItem * item);
    //void onClearLogsClicked();

private:
    Ui::Home *ui;

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
};

#endif // HOME_H
