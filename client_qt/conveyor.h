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
#include <QDateEdit>
#include <QProgressBar>
#include <QSlider>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QImage>
#include <QMap>
#include <QSplitter>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "streamer.h"
#include <qlistwidget.h>
#include "cardevent.h"
#include "error_message_card.h"
#include "device_chart.h"

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
    void onDeviceStatsReceived(const QString &deviceId, const QJsonObject &statsData);
    void onSearchResultsReceived(const QList<QJsonObject> &results);

signals:
    void errorLogGenerated(const QJsonObject &errorData);     // 오류 로그 발생 시그널
    void requestErrorLogs(const QString &deviceId);           // 과거 로그 요청 시그널
    void requestFilteredLogs(const QString &devicedId, const QString &searchText);
    void deviceStatusChanged(const QString &deviceId, const QString &status);//off
    void requestMqttPublish(const QString &topic, const QString &message);
    void requestConveyorLogSearch(const QString& searchText, const QDate& startDate, const QDate& endDate);  //추가



private slots: //행동하는 것
    void onMqttConnected(); //연결 되었는지
    void onMqttDisConnected(); //연결 안되었을 때
    void onMqttMessageReceived(const QMqttMessage &message); //메시지 내용, 토픽 on, myled/status
    void onMqttError(QMqttClient::ClientError error); //에러 났을 때
    void connectToMqttBroker(); //브로커 연결

    void onConveyorOnClicked();
    void onConveyorOffClicked();
    //void onConveyorReverseClicked();
    void onDeviceLock();
    //void onShutdown();
    //void onSpeedChange(int value);
    void onSystemReset();
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시
    void updateHWImage(const QImage& image); //한화 카메라
    void gobackhome();
    void requestStatisticsData();
    void requestFailureRate();
    void onConveyorSearchClicked();  // 수정
    void onSearchClicked();

    void on_listWidget_itemDoubleClicked(QListWidgetItem* item);

    //그래프
    void onChartRefreshRequested(const QString &deviceName);

private:
    Ui::ConveyorWindow *ui;
    Streamer* rpiStreamer;
    Streamer* hwStreamer;  // 한화 카메라 스트리머

    QMqttClient *m_client;
    QMqttSubscription *subscription;
    QTimer *reconnectTimer;

    QString mqttBroker = "mqtt.kwon.pics";
    int mqttPort = 1883;
    QString mqttTopic = "conveyor_01/status";
    QString mqttControllTopic = "conveyor_03/cmd";

    //error message
    bool convorRunning;//hasError
    bool isConnected;
    int conveyorDirection;
    bool DeviceLockActive;

    QPushButton *btnConveyorOn;
    QPushButton *btnConveyorOff;
    //QPushButton *btnConveyorcmd;
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

    //Home *homeWindow;

    void logMessage(const QString &message); //로그 메시지 남기는 것
    void setupMqttClient();
    void initializeUI();
    void setupControlButtons();
    //void updateConnectionStatus();
    //void updateDeviceStatus();
    void setupLogWidgets(); //로그 위젯 추가함
    void publishControlMessage(const QString &command);
    //void backhome();
    void setupHomeButton();
    void logError(const QString &errorType);
    void updateErrorStatus();
    void setupRightPanel();
    void addErrorLog(const QJsonObject &errorData);
    void showConveyorError(QString conveyorErrorType="컨테이너 오류");
    void showConveyorNormal();
    void loadPastLogs();

    QDateEdit* conveyorStartDateEdit;
    QDateEdit* conveyorEndDateEdit;
    QMap<QString, QWidget*> conveyorQueryMap;
    //void setupConveyorSearchPanel();

    void downloadAndPlayVideoFromUrl(const QString& httpUrl, const QString& deviceId);
    QTimer *statisticsTimer;


    QVBoxLayout* errorCardLayout = nullptr; // 카드 레이아웃
    QWidget* errorCardContainer = nullptr;  // 카드 컨테이너
    // 카드 더블클릭 이벤트 필터
    CardEventFilter* cardEventFilter = nullptr;
    void addErrorCardUI(const QJsonObject& logData); // 카드 UI 추가 함수
    void onCardDoubleClicked(QObject* cardWidget); // 시그널과 맞춤
    void clearErrorCards();

    //헤더
    ErrorMessageCard* errorCard = nullptr;
    void setupErrorCardUI();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    //그래프
    DeviceChart *deviceChart;
    void setupChartInUI();
    void initializeDeviceChart();

    QChart *failureRateChart = nullptr;
    QChartView *failureRateChartView = nullptr;
    QPieSeries *failureRateSeries = nullptr;

    void createFailureRateChart(QHBoxLayout *parentLayout);
    void updateFailureRate(double failureRate);  // 불량률 업데이트 함수

    //날짜
    bool isConveyorDateSearchMode = false;    // 컨베이어 날짜 검색 모드 플래그
    QDate currentConveyorStartDate;           // 현재 컨베이어 검색 시작일
    QDate currentConveyorEndDate;             // 현재 컨베이어 검색 종료일
    void addNoResultsMessage();


    void updateButtonStates(bool isRunning);

};

#endif // CONVEYOR_H
