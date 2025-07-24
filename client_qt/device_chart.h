#ifndef DEVICE_CHART_H
#define DEVICE_CHART_H

#include <QObject>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QList>
#include <QPushButton>
#include <QWidget>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolTip>
#include <QCursor>

struct SpeedDataPoint {
    QDateTime timestamp;
    int currentSpeed;
    int averageSpeed;

    SpeedDataPoint(const QDateTime &time, int current, int average)
        : timestamp(time), currentSpeed(current), averageSpeed(average) {}
};

class DeviceChart : public QObject
{
    Q_OBJECT

public:
    explicit DeviceChart(const QString &deviceName, QObject *parent = nullptr);
    ~DeviceChart();

    // 메인 위젯 반환 (차트 + 새로고침 버튼)
    QWidget* getChartWidget() const;

    // 속도 데이터 추가
    void addSpeedData(int currentSpeed, int averageSpeed);

    // 차트 초기화
    void initializeChart();

    // 차트 새로고침
    void refreshChart();

signals:
    void refreshRequested(const QString &deviceName);

private slots:
    void onRefreshButtonClicked();

private:
    // 차트 관련 멤버
    QChart *chart;
    QChartView *chartView;
    QLineSeries *currentSpeedSeries;    // 현재속도 (주황색)
    QLineSeries *averageSpeedSeries;    // 평균속도 (연한 주황색)
    QScatterSeries *currentSpeedPoints; // 현재속도 툴팁용 (투명)
    QScatterSeries *averageSpeedPoints; // 평균속도 툴팁용 (투명)
    QValueAxis *axisX;
    QValueAxis *axisY;

    // UI 관련 멤버
    QWidget *mainWidget;
    QPushButton *refreshButton;

    // 데이터 관련 멤버
    QString deviceName;
    QList<SpeedDataPoint> speedDataHistory;
    static const int MAX_DATA_POINTS = 10;

    // 내부 함수들
    void setupChart();
    void setupUI();
    void updateChart();
    void setupTooltips();
};

#endif // DEVICE_CHART_H
