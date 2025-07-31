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
#include <QDate>
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
#include "mainwindow.h"
#include "conveyor.h"
#include "../video/streamer.h"
#include "../charts/errorchartmanager.h"


#include "../mcp/factory_mcp.h" //mcp용
#include "../utils/ai_command.h"
#include "../mcp/mcp_btn.h"
#include "../mcp/chatbot_widget.h"

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
    void onCardDoubleClicked(QObject* cardWidget);

    void onDeviceStatusChanged(const QString &deviceId, const QString &status); //off

    //더미
    void clearAllErrorLogsFromUI();  // 혹시 없다면 추가

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    void requestFeederLogs(const QString &errorCode, const QDate &startDate, const QDate &endDate, MainWindow* targetWindow);
    //void handleConveyorLogSearch(const QString& errorCode, const QDate& startDate, const QDate& endDate);


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
    void processFeederSearchResponse(const QJsonObject &response, MainWindow* targetWindow);

    void enableRealTimeMode();
    //void clearAllErrorLogsFromUI();

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

    bool isDateSearchMode;
    QDate currentSearchStartDate;
    QDate currentSearchEndDate;

    //QString mqttQueryRequestTopic = "factory/query/videos/request";    // 쿼리 요청 토픽
    //QString mqttQueryResponseTopic = "factory/query/videos/response";  // 쿼리 응답 토픽
    QString currentQueryId;
    MainWindow* currentFeederWindow = nullptr;

    void requestPastLogs(); //db에게 과거로그 요청 보내기
    void processPastLogsResponse(const QJsonObject &response); //db에게 받은거 화면에 표시
    QString generateQueryId();

    //mcp
    FactoryMCP* mcpHandler = nullptr;
    //QPushButton* btnAICommand = nullptr;
    MCPButton* aiButton = nullptr;
    QString apiKey;
    GeminiRequester* gemini;
    ChatBotWidget* chatBot = nullptr;

    //검색
    void requestFilteredLogs(const QString &errorCode);
    QChartView *chartView;
    QChart *chart;
    QBarSeries *barSeries;
    QBarSet *feederBarSet;
    QBarSet *conveyorBarSet;
    QMap<QString, QMap<QString, QSet<QString>>> monthlyErrorDays;

    QMap<QString, QString> lastDeviceStatus;

    // 날짜 선택 위젯들
    QDateEdit* startDateEdit;
    QDateEdit* endDateEdit;
    void processFeederResponse(const QJsonObject &response);
    QString feederQueryId;
    // 페이지네이션
    int pageSize = 500;
    int currentPage = 0;
    bool isLoadingMoreLogs = false;


    // 마지막 검색 조건 저장
    QString lastSearchErrorCode;
    QDate lastSearchStartDate;
    QDate lastSearchEndDate;

    ErrorChartManager *m_errorChartManager;

    void requestFilteredLogs(const QString &errorCode, const QDate &startDate = QDate(), const QDate &endDate = QDate(), bool loadMore = false);
    void updateLoadMoreButton(bool showButton);
    bool isLoadingChartData = false;
    QString chartQueryId;

    // 차트용 별도 함수
    void loadAllChartData();
    //void loadChartDataBatch(int offset);
    void processChartDataResponse(const QJsonObject &response);

    void sendFactoryStatusLog(const QString &logCode, const QString &message);
    qint64 lastOldestTimestamp = 0;
    qint64 lastTimestamp = 0;
    QSet<QString> receivedLogIds;
    QMap<QString, MainWindow*> feederQueryMap;

    void handleConveyorLogSearch(const QString& errorCode, const QDate& startDate, const QDate& endDate);
    void processConveyorSearchResponse(const QJsonObject &response, ConveyorWindow* targetWindow);
    ConveyorWindow* currentConveyorWindow = nullptr;

    void loadChartDataSingle();

    QString conveyorQueryId;
    QMap<QString, ConveyorWindow*> conveyorQueryMap;
    void processConveyorResponse(const QJsonObject &response);

    void requestFeederStatistics();
    void requestConveyorStatistics();

    // 로그 영상
    void downloadAndPlayVideo(const QString& filename);
    void tryPlayVideo(const QString& originalUrl);
    //void tryNextUrl(QStringList* urls, int index);
    void downloadAndPlayVideoFromUrl(const QString &httpUrl, const QString &deviceId);
    void requestStatisticsToday(const QString& deviceId);

    void handleFeederLogSearch(const QString& errorCode, const QDate& startDate, const QDate& endDate);  // ✅ 피더 검색 처리

private:
    QStringList getVideoServerUrls() const;
    void addErrorCardUI(const QJsonObject &errorData);
    QTimer *statisticsTimer;
    void addNoResultsMessage();

    void setupSidebarStyles();
    void setupPanelStyles();
};

#endif // HOME_H
