#ifndef ERRORCHARTMANAGER_H
#define ERRORCHARTMANAGER_H

#include <QObject>
#include <QChart>
#include <QChartView>
#include <QBarSeries>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QJsonObject>
#include <QSet>
#include <QMap>

class ErrorChartManager : public QObject
{
    Q_OBJECT
public:
    explicit ErrorChartManager(QObject* parent = nullptr);
    ~ErrorChartManager() override = default;

    QChartView* chartView() const;                // 차트 뷰 반환
    void        processErrorData(const QJsonObject& errorJson);

private:
    void initChart();                             // 한 번만 호출
    void refreshBars();                           // 막대 업데이트
    QStringList recentSixMonths() const;          // X축 레이블

    /* Qt Charts 구성 요소 */
    QChart*          m_chart          { nullptr };
    QChartView*      m_chartView      { nullptr };
    QBarSeries*      m_series         { nullptr };
    QBarSet*         m_setFeeder      { nullptr };
    QBarSet*         m_setConveyor    { nullptr };
    QBarCategoryAxis*m_axisX          { nullptr };
    QValueAxis*      m_axisY          { nullptr };

    /* 월별‧디바이스별 오류일 집계   monthKey -> device -> {dayStrings} */
    QMap<QString, QMap<QString, QSet<QString>>> m_monthlyErrorDays;
};

#endif // ERRORCHARTMANAGER_H
