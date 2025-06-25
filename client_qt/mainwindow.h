#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include <QtMqtt/QMqttSubscription>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결


private:
    Ui::MainWindow *ui;
    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;

    QString mqttBroker = "mqtt.eclipseprojects.io";
    int mqttPort = 1883;
    QString mqttTopic = "myled/status";

    void logMessage(const QString &message); //로그 메시지 남기는 것
    void setupMqttClient();


};
#endif // MAINWINDOW_H
