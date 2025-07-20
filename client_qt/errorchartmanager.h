#ifndef ERRORCHARTMANAGER_H
#define ERRORCHARTMANAGER_H

#include <QObject>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QStringList>


    class ErrorChartManager : public QObject
{
    Q_OBJECT

public:
    explicit ErrorChartManager(QObject *parent = nullptr);
    ~ErrorChartManager();

    // 차트 뷰 반환
    QChartView* getChartView() const;

    // 오류 데이터 처리 (하루에 1개만 카운트)
    void processErrorData(const QJsonObject &errorData);

    // 차트 초기화
    void initializeChart();

private:
    // 차트 관련 멤버
    QChart *chart;
    QChartView *chartView;
    QBarSeries *barSeries;
    QBarSet *feederBarSet;
    QBarSet *conveyorBarSet;
    QBarCategoryAxis *axisX;
    QValueAxis *axisY;

    // 데이터 저장: [월][디바이스타입][일자셋]
    QMap<QString, QMap<QString, QSet<QString>>> monthlyErrorDays;

    // 내부 함수들
    void setupChart();
    //QStringList getLast6Months();
    QStringList getTargetMonths();
    void updateErrorChart();
    void printChartDataStatus();
    void clearChartData();
};

#endif // ERRORCHARTMANAGER_H
