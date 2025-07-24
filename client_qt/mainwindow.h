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
#include <QDateEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QDate>
#include <QLineEdit>
#include <QMessageBox>  // 경고창용 추가
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSplitter>
#include <QGroupBox>
#include "streamer.h"
#include "device_chart.h"
#include <qlistwidget.h>
#include <QScrollArea>

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
    void onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData);
    void onSearchResultsReceived(const QList<QJsonObject> &results);
    //void onDateRangeSearchClicked();
    void addErrorCardUI(const QJsonObject &errorData);
    void onCardDoubleClicked(QObject* cardWidget);

signals:
    void errorLogGenerated(const QJsonObject &errorData);     // 오류 로그 발생 시그널
    void requestErrorLogs(const QString &deviceId);           // 과거 로그 요청 시그널
    void requestMqttPublish(const QString &topic, const QString &message); // MQTT 발송 요청
    //void requestFilteredLogs(const QString &deviceId, const QString &searchText); //db 검색
    void deviceStatusChanged(const QString &deviceId, const QString &status); //off
    void requestFeederLogSearch(const QString &errorCode, const QDate &startDate, const QDate &endDate);
    void requestDateRangeSearch(const QDate &startDate, const QDate &endDate);
    void feederSearchResponse(const QList<QJsonObject> &results);
private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void onFeederOnClicked();
    void onFeederOffClicked();
    //void onFeederReverseClicked();
    void onDeviceLock();
    //void onShutdown();
    //void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시
    void updateHWImage(const QImage& image); //한화 카메라
    void gobackhome();

    void requestStatisticsData();

    //void onSearchResultsReceived(const QList<QJsonObject> &results);

    void onSearchClicked();

    //device_chart
    void onChartRefreshRequested(const QString &deviceName);

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
    QString mqttControllTopic = "feeder_02/cmd";

    //error message
    bool feederRunning;//hasError
    bool isConnected;
    int feederDirection;
    bool DeviceLockActive;

    QPushButton *btnFeederOn;
    QPushButton *btnFeederOff;
    //QPushButton *btnFeederReverse;
    QPushButton *btnDeviceLock;
    //QPushButton *btnShutdown;
    QLabel *lblConnectionStatus;
    QLabel *lblDeviceStatus;
    QProgressBar *progressActivity;
    //QSlider *speedSlider;
    //QLabel *speedLabel;
    QPushButton *btnSystemReset;
    QPushButton *btnbackhome;
    QTextEdit *textEventLog;
    QTextEdit *textErrorStatus;
    QMap<QString, int> errorCounts;
    qint64 lastErrorTimestamp = 0;

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
    //void setupRightPanel();
    void addErrorLog(const QJsonObject &errorData);

    //error message 함수
    void showFeederError(QString feederErrorType="피더 오류");
    void showFeederNormal();
    void loadPastLogs();

    //db 검색

    //void onSearchClicked();
    void downloadAndPlayVideoFromUrl(const QString& httpUrl);

    QDateEdit *startDateEdit;
    QDateEdit *endDateEdit;
    QPushButton *btnDateRangeSearch;
    QString lastSearchErrorCode;
    QDate lastSearchStartDate;
    QDate lastSearchEndDate;

    QDateEdit* conveyorStartDateEdit;
    QDateEdit* conveyorEndDateEdit;

    // MQTT 관련
    QString feederQueryId;
    bool isLoadingFeederLogs = false;

    void setupFeederLogSearch();  // 피더 로그 검색 UI 설정
    void requestFeederLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate);
    void setupRightPanel();  // 여기로 이동

    // 페이지네이션
    int pageSize = 2000;
    int currentPage = 0;
    bool isLoadingMoreLogs = false;

    QString currentQueryId;
    //QLineEdit *feederSearchEdit = nullptr;
    //QDateEdit *feederStartDateEdit = nullptr;
    //QDateEdit *feederEndDateEdit = nullptr;
    //QPushButton *feederSearchButton = nullptr;

    QPushButton *btnDateSearch;
    QTimer *statisticsTimer;

    QScrollArea* errorScrollArea = nullptr;
    QWidget* errorCardContent = nullptr;
    QVBoxLayout* errorCardLayout = nullptr;

    //device_chart
    void setupChartInUI();
    DeviceChart *deviceChart;
    void initializeDeviceChart();


};

#endif // MAINWINDOW_H
