#include "device_chart.h"
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QLabel>

DeviceChart::DeviceChart(const QString &deviceName, QObject *parent)
    : QObject(parent)
    , chart(nullptr)
    , chartView(nullptr)
    , currentSpeedSeries(nullptr)
    , averageSpeedSeries(nullptr)
    , currentSpeedPoints(nullptr)
    , averageSpeedPoints(nullptr)
    , axisX(nullptr)
    , axisY(nullptr)
    , mainWidget(nullptr)
    , deviceName(deviceName)
    , timeCounter(0)
{
    initializeChart();
}

DeviceChart::~DeviceChart()
{
    // parent가 있으므로 자동 삭제됨
}

QWidget* DeviceChart::getChartWidget() const
{
    return mainWidget;
}

void DeviceChart::initializeChart()
{
    qDebug() << "=== DeviceChart 초기화 시작 ===" << deviceName;

    timeCounter = 0;
    setupChart();
    setupUI();
    setupTooltips();

    //addInitialZeroPoints();

    qDebug() << "DeviceChart 초기화 완료:" << deviceName;
}

void DeviceChart::setupChart()
{
    // 차트 생성
    chart = new QChart();
    chartView = new QChartView(chart);

    // 시리즈 생성
    currentSpeedSeries = new QLineSeries();
    averageSpeedSeries = new QLineSeries();
    currentSpeedPoints = new QScatterSeries();
    averageSpeedPoints = new QScatterSeries();

    // 색상 및 스타일 설정
    currentSpeedSeries->setName("현재속도");
    currentSpeedSeries->setColor(QColor(255, 127, 0));
    currentSpeedSeries->setPen(QPen(QColor(255, 127, 0), 3));

    averageSpeedSeries->setName("평균속도");
    averageSpeedSeries->setColor(QColor(255, 179, 102));
    averageSpeedSeries->setPen(QPen(QColor(255, 179, 102), 3));

    // 점 크기 설정
    currentSpeedPoints->setMarkerSize(9);
    currentSpeedPoints->setColor(QColor(255, 127, 0, 180));
    currentSpeedPoints->setBorderColor(QColor(255, 127, 0));
    currentSpeedPoints->setPen(QPen(QColor(255, 127, 0), 2));

    averageSpeedPoints->setMarkerSize(9);
    averageSpeedPoints->setColor(QColor(255, 179, 102, 180));
    averageSpeedPoints->setBorderColor(QColor(255, 179, 102));
    averageSpeedPoints->setPen(QPen(QColor(255, 179, 102), 2));

    // 차트에 시리즈 추가
    chart->addSeries(currentSpeedSeries);
    chart->addSeries(averageSpeedSeries);
    chart->addSeries(currentSpeedPoints);
    chart->addSeries(averageSpeedPoints);

    // 차트 제목 및 설정
    chart->setTitle(QString("%1 속도 차트").arg(deviceName));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setMargins(QMargins(10, 25, 10, 5));

    // 범례 설정
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeRectangle);

    //  X축 설정
    axisX = new QValueAxis();
    axisX->setTitleText("시간 (분)");
    axisX->setLabelFormat("%i");

    //  초기 범위: 0부터 시작
    if (deviceName == "컨베이어") {
        axisX->setRange(0, 4);  // 0, 1, 2, 3, 4분
    } else {
        axisX->setRange(0, 9);  // 0~9분
    }

    //  Y축 설정
    axisY = new QValueAxis();
    axisY->setTitleText("RPM");
    axisY->setLabelFormat("%i");
    axisY->setRange(0, 60);

    // 축을 차트에 추가
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    // 시리즈를 축에 연결
    currentSpeedSeries->attachAxis(axisX);
    currentSpeedSeries->attachAxis(axisY);
    averageSpeedSeries->attachAxis(axisX);
    averageSpeedSeries->attachAxis(axisY);
    currentSpeedPoints->attachAxis(axisX);
    currentSpeedPoints->attachAxis(axisY);
    averageSpeedPoints->attachAxis(axisX);
    averageSpeedPoints->attachAxis(axisY);

    // 범례에서 포인트 시리즈 숨기기
    chart->legend()->markers(currentSpeedPoints).first()->setVisible(false);
    chart->legend()->markers(averageSpeedPoints).first()->setVisible(false);

    //  차트뷰 설정 - 테두리 제거
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setFrameStyle(QFrame::NoFrame);

    qDebug() << "차트 설정 완료:" << deviceName;
}

void DeviceChart::updateXAxisLabels()
{
    int dataCount = speedDataHistory.size();

    if (dataCount > 0) {
        if (deviceName == "피더") {
            //  피더 - 실제 시간 기준 라벨
            int startTime = timeCounter - dataCount + 1;
            int endTime = timeCounter;

            QString axisTitle = QString("시간 (%1~%2분)").arg(qMax(0, startTime)).arg(endTime);
            axisX->setTitleText(axisTitle);

            qDebug() << "피더 X축 제목 업데이트:" << axisTitle;
        } else {
            //  컨베이어 - 기존 방식 유지
            int endTime = timeCounter;
            int startTime = qMax(0, timeCounter - (dataCount - 1));

            QString axisTitle = QString("시간 (%1~%2분)").arg(startTime).arg(endTime);
            axisX->setTitleText(axisTitle);

            qDebug() << "컨베이어 X축 제목 업데이트:" << axisTitle;
        }
    }
}

void DeviceChart::setupUI()
{
    mainWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    // 차트 높이
    chartView->setMinimumHeight(220);
    chartView->setMaximumHeight(260);

    //  3. 차트에 텍스트 오버레이 추가
    QWidget *chartContainer = new QWidget();
    QVBoxLayout *containerLayout = new QVBoxLayout(chartContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(chartView);

    // 텍스트 오버레이 라벨
    QLabel *updateLabel = new QLabel("1분마다 자동 갱신");
    updateLabel->setStyleSheet(
        "QLabel {"
        "  color: #FF7F00;"
        "  font-size: 9px;"
        "  background-color: rgba(255, 255, 255, 200);"
        "  padding: 2px 6px;"
        "  border-radius: 3px;"
        "  border: 1px solid rgba(255, 127, 0, 100);"
        "}"
        );
    updateLabel->setAlignment(Qt::AlignCenter);
    updateLabel->setFixedSize(90, 20);

    // 차트 위에 오버레이로 배치
    updateLabel->setParent(chartContainer);
    updateLabel->move(10, 10);  // 왼쪽 위 모서리

    // 전체 레이아웃 구성
    mainLayout->addWidget(chartContainer, 1);

    // 여백 설정
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(1);

    qDebug() << "UI 설정 완료 (오버레이 텍스트 추가됨):" << deviceName;
}
void DeviceChart::setupTooltips()
{
    //  현재속도 포인트 툴팁 (수정됨)
    connect(currentSpeedPoints, &QScatterSeries::hovered,
            [this](const QPointF &point, bool state) {
                if (state) {
                    QString tooltipText = QString("현재속도: %1").arg((int)point.y());
                    QToolTip::showText(QCursor::pos(), tooltipText);
                    qDebug() << "현재속도 툴팁 표시:" << tooltipText;
                } else {
                    QToolTip::hideText();
                }
            });

    //  평균속도 포인트 툴팁 (수정됨)
    connect(averageSpeedPoints, &QScatterSeries::hovered,
            [this](const QPointF &point, bool state) {
                if (state) {
                    QString tooltipText = QString("평균속도: %1").arg((int)point.y());
                    QToolTip::showText(QCursor::pos(), tooltipText);
                    qDebug() << "평균속도 툴팁 표시:" << tooltipText;
                } else {
                    QToolTip::hideText();
                }
            });

    qDebug() << "툴팁 설정 완료:" << deviceName;
}
void DeviceChart::addSpeedData(int currentSpeed, int averageSpeed) {
    timeCounter++;  // 항상 증가 (1,2,3,4,5...)

    //  0 데이터든 아니든 무조건 차트에 추가
    SpeedDataPoint newPoint(QDateTime::currentDateTime(), currentSpeed, averageSpeed);
    speedDataHistory.append(newPoint);

    // 최대 개수 제한
    int maxDataPoints = (deviceName == "컨베이어") ? 5 : 10;
    while (speedDataHistory.size() > maxDataPoints) {
        speedDataHistory.removeFirst();
    }

    updateChart();  // 0도 정상적으로 차트에 표시
}


void DeviceChart::updateChart()
{
    if (speedDataHistory.isEmpty()) {
        qDebug() << "데이터가 없어서 차트 업데이트 건너뜀";
        return;
    }

    // 기존 데이터 클리어
    currentSpeedSeries->clear();
    averageSpeedSeries->clear();
    currentSpeedPoints->clear();
    averageSpeedPoints->clear();

    int maxDataPoints = (deviceName == "컨베이어") ? 5 : 10;
    int dataCount = speedDataHistory.size();

    qDebug() << deviceName << "데이터 개수:" << dataCount << "timeCounter:" << timeCounter;

    //  피더 차트 - 간단하고 정확한 방식
    if (deviceName == "피더") {
        // 피더는 항상 실제 시간 인덱스 사용 (정확한 위치)
        for (int i = 0; i < dataCount; ++i) {
            const SpeedDataPoint &point = speedDataHistory[i];

            //  실제 시간 계산 (2분이면 정확히 x=2에 위치)
            int actualTimeIndex = timeCounter - dataCount + 1 + i;

            // 라인 데이터
            currentSpeedSeries->append(actualTimeIndex, point.currentSpeed);
            averageSpeedSeries->append(actualTimeIndex, point.averageSpeed);

            // 포인트 데이터 (툴팁용)
            currentSpeedPoints->append(actualTimeIndex, point.currentSpeed);
            averageSpeedPoints->append(actualTimeIndex, point.averageSpeed);

            qDebug() << "피더 차트 시간:" << actualTimeIndex << "분, 현재:" << point.currentSpeed << "RPM, 평균:" << point.averageSpeed << "RPM";
        }

        //  X축 범위 - 실제 시간 기준으로 설정
        if (dataCount > 0) {
            int startTime = timeCounter - dataCount + 1;
            int endTime = timeCounter;

            // 10분 윈도우 유지하되, 실제 시간에 맞춤
            int rangeStart = qMax(0, startTime);
            int rangeEnd = qMax(9, endTime);  // 최소 0~9 범위 보장

            axisX->setRange(rangeStart, rangeEnd);
            qDebug() << "피더 X축 범위:" << rangeStart << "~" << rangeEnd << "분 (실제 데이터:" << startTime << "~" << endTime << ")";
        }

    } else {
        //  컨베이어 차트 - 기존 방식 유지 (이미 정확함)
        int startIndex = qMax(0, dataCount - maxDataPoints);
        int displayStartTime = 0;

        if (dataCount >= maxDataPoints) {
            displayStartTime = timeCounter - maxDataPoints + 1;
            displayStartTime = qMax(0, displayStartTime);
        }

        for (int i = startIndex; i < dataCount; ++i) {
            const SpeedDataPoint &point = speedDataHistory[i];
            int chartTimeIndex = displayStartTime + (i - startIndex);

            currentSpeedSeries->append(chartTimeIndex, point.currentSpeed);
            averageSpeedSeries->append(chartTimeIndex, point.averageSpeed);
            currentSpeedPoints->append(chartTimeIndex, point.currentSpeed);
            averageSpeedPoints->append(chartTimeIndex, point.averageSpeed);

            qDebug() << "컨베이어 차트 시간:" << chartTimeIndex << "분, 현재:" << point.currentSpeed << "RPM, 평균:" << point.averageSpeed << "RPM";
        }

        // 컨베이어 X축 범위
        int rangeStart = displayStartTime;
        int rangeEnd = displayStartTime + maxDataPoints - 1;
        axisX->setRange(rangeStart, rangeEnd);
        qDebug() << "컨베이어 X축 범위:" << rangeStart << "~" << rangeEnd << "분";
    }

    // X축 제목 업데이트
    updateXAxisLabels();

    qDebug() << "차트 업데이트 완료:" << deviceName;
}



void DeviceChart::refreshChart()
{
    qDebug() << "차트 새로고침 요청:" << deviceName;
    emit refreshRequested(deviceName);
}

void DeviceChart::clearAllData() {
    currentSpeedSeries->clear();
    averageSpeedSeries->clear();
    speedDataHistory.clear();  // 데이터만 클리어
    //  timeCounter는 리셋하지 않음! (이게 0->1->2 문제의 원인이었음)
    qDebug() << "차트 데이터 클리어 (timeCounter 유지:" << timeCounter << ")";
}
