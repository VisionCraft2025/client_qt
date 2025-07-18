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
#include <QDateEdit>  // 추가!
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateEdit>
#include <QJsonArray>
#include <QUuid>
#include <QTimeZone>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include "mainwindow.h"
#include "conveyor.h"
#include "streamer.h"
#include "errorchartmanager.h"
#include <qlistwidget.h>


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

    void onDeviceStatusChanged(const QString &deviceId, const QString &status); //off
    void on_listWidget_itemDoubleClicked(QListWidgetItem* item);


signals:
    void errorLogsResponse(const QList<QJsonObject> &logs);     // 로그 응답 시그널
    void newErrorLogBroadcast(const QJsonObject &errorData);
    void deviceStatsReceived(const QString &deviceId, const QJsonObject &statsData);


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
    void onQueryResponseReceived(const QMqttMessage &message);

    // stream
    void updateFeederImage(const QImage& image); // v피더캠 영상 표시
    void updateConveyorImage(const QImage& image); //컨베이어 영상
    void updateHWImage(const QImage& image); //한화 카메라

    void onSearchClicked();

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
    QString mqttTopic = "factory/status"; //sub
    QString mqttControlTopic = "factory/control"; //pub

    // UI 컴포넌트들
    QPushButton *btnFeederTab;
    QPushButton *btnConveyorTab;
    QPushButton *btnFactoryToggle;
    QLabel *lblConnectionStatus;
    QLabel *lblFactoryStatus;
    QTableWidget *logTable;
    QList<QJsonObject> errorLogHistory;


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
    void controlALLDevices(bool start);
    void initializeChildWindows();
    QMqttSubscription *queryResponseSubscription;  // 쿼리 응답 구독 추가
    QString mqttQueryRequestTopic = "factory/query/logs/request";    // 쿼리 요청 토픽
    QString mqttQueryResponseTopic = "factory/query/logs/response";  // 쿼리 응답 토픽

    //QString mqttQueryRequestTopic = "factory/query/videos/request";    // 쿼리 요청 토픽
    //QString mqttQueryResponseTopic = "factory/query/videos/response";  // 쿼리 응답 토픽
    QString currentQueryId;

    void requestPastLogs(); //db에게 과거로그 요청 보내기
    void processPastLogsResponse(const QJsonObject &response); //db에게 받은거 화면에 표시
    QString generateQueryId();

    //검색
    //void requestFilteredLogs(const QString &errorCode);
    //void requestFilteredLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate);
    QChartView *chartView;
    QChart *chart;
    QBarSeries *barSeries;
    QBarSet *feederBarSet;
    QBarSet *conveyorBarSet;
    QMap<QString, QMap<QString, QSet<QString>>> monthlyErrorDays;

    // 날짜 선택 위젯들
    QDateEdit* startDateEdit;
    QDateEdit* endDateEdit;

    // 페이지네이션
    int pageSize = 500;
    int currentPage = 0;
    bool isLoadingMoreLogs = false;

    // 🔥 마지막 검색 조건 저장
    QString lastSearchErrorCode;
    QDate lastSearchStartDate;
    QDate lastSearchEndDate;

    ErrorChartManager *m_errorChartManager;

    void requestFilteredLogs(const QString &errorCode, const QDate &startDate = QDate(), const QDate &endDate = QDate(), bool loadMore = false);
    void updateLoadMoreButton(bool showButton);

    void setupErrorChart();
    void updateErrorChart();
    void processErrorForChart(const QJsonObject &errorData);
    QStringList getLast6Months();

    void sendFactoryStatusLog(const QString &logCode, const QString &message);
    qint64 lastOldestTimestamp = 0;
    qint64 lastTimestamp = 0;
    QSet<QString> receivedLogIds;

    // 로그 영상
    void downloadAndPlayVideo(const QString& filename);
    void tryPlayVideo(const QString& originalUrl);
    //void tryNextUrl(QStringList* urls, int index);
    void downloadAndPlayVideoFromUrl(const QString& httpUrl);
private:
    QStringList getVideoServerUrls() const;
};

#endif // HOME_H
