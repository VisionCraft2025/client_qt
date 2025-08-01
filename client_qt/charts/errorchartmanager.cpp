#include "errorchartmanager.h"
#include "../utils/font_manager.h"

#include <QDate>
#include <QDateTime>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QToolTip>
#include <QCursor>
#include <QDebug>

ErrorChartManager::ErrorChartManager(QObject* parent)
    : QObject(parent)
{
    initChart();
}

/* ---------- public ---------- */

QChartView* ErrorChartManager::chartView() const
{
    return m_chartView;
}

void ErrorChartManager::processErrorData(const QJsonObject& errorJson)
{
    const QString deviceId  = errorJson["device_id"].toString();
    const qint64  tsMillis  = errorJson["timestamp"].toVariant().toLongLong();
    if (deviceId.isEmpty() || tsMillis == 0) return;

    const QDateTime dt       = QDateTime::fromMSecsSinceEpoch(tsMillis);
    const QString   monthKey = dt.toString("yyyy-MM");
    const QString   dayKey   = dt.toString("yyyy-MM-dd");

    const QString deviceType =
        deviceId.contains("feeder",   Qt::CaseInsensitive)  ? "feeder"   :
            deviceId.contains("conveyor", Qt::CaseInsensitive)  ? "conveyor" :
            QString();

    if (deviceType.isEmpty()) return;

    if (!m_monthlyErrorDays[monthKey][deviceType].contains(dayKey)) {
        m_monthlyErrorDays[monthKey][deviceType].insert(dayKey);
        refreshBars();
    }
}

/* ---------- private ---------- */

void ErrorChartManager::initChart()
{
    /* 1. 차트 객체 */
    m_chart = new QChart;
    m_chart->setTitle(""); // 타이틀 제거
    m_chart->legend()->setVisible(false); // 범례 숨김
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    /* 2. 막대 세트 */
    m_setFeeder   = new QBarSet("feeder");
    m_setConveyor = new QBarSet("conveyor");

    QLinearGradient gradFeeder(0,0,0,1);
    gradFeeder.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradFeeder.setColorAt(0.0, QColor("#fb923c"));
    gradFeeder.setColorAt(1.0, QColor("#ea580c"));
    m_setFeeder->setBrush(QBrush(gradFeeder));

    QLinearGradient gradConv(0,0,0,1);
    gradConv.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradConv.setColorAt(0.0, QColor("#60a5fa"));
    gradConv.setColorAt(1.0, QColor("#2563eb"));
    m_setConveyor->setBrush(QBrush(gradConv));

    m_series = new QBarSeries;
    m_series->append(m_setFeeder);
    m_series->append(m_setConveyor);
    m_chart->addSeries(m_series);

    /* 3. 배경 패턴 */
    m_chart->setBackgroundBrush(QBrush(Qt::white)); // 배경 흰색

    /* 4. 축 */
    m_axisX = new QBarCategoryAxis;
    m_axisX->append(recentSixMonths());
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_series->attachAxis(m_axisX);

    m_axisY = new QValueAxis;
    m_axisY->setRange(0, 12);
    m_axisY->setTickCount(5);
    m_axisY->setTitleText("Error Days");
    m_axisY->setTitleFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisY);

    /* 5. ChartView */
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet(
        "QChartView { background: transparent; border-radius: 0px; }");

    /* 6. 툴팁 */
    connect(m_series, &QBarSeries::hovered,
            [](bool state, int idx, QBarSet* set){
                if (state) {
                    QToolTip::showText(QCursor::pos(),
                                       QString("%1 : %2 일")
                                           .arg(set->label())
                                           .arg(set->at(idx)));
                }
            });

    /* 첫 데이터 0으로 초기화 */
    for (int i=0;i<6;i++){ m_setFeeder->append(0); m_setConveyor->append(0); }
}

void ErrorChartManager::refreshBars()
{
    m_setFeeder->remove(0, m_setFeeder->count());
    m_setConveyor->remove(0, m_setConveyor->count());

    int yMax = 5;
    const QDate now = QDate::currentDate();
    const int currentYear = now.year();

    // 1-6월 데이터 가져오기
    for (int month = 1; month <= 6; ++month){
        const QString key = QString("%1-%2").arg(currentYear).arg(month, 2, 10, QChar('0'));
        const int fCnt   = m_monthlyErrorDays[key]["feeder"].size();
        const int cCnt   = m_monthlyErrorDays[key]["conveyor"].size();
        m_setFeeder->append(fCnt);
        m_setConveyor->append(cCnt);
        yMax = qMax(yMax, qMax(fCnt,cCnt)+2);
    }
    m_axisY->setRange(0, yMax);
}

QStringList ErrorChartManager::recentSixMonths() const
{
    QStringList list;
    // 고정된 1-6월로 설정
    for (int month = 1; month <= 6; ++month)
        list << QString("%1월").arg(month, 2, 10, QChar('0'));
    return list;
}
