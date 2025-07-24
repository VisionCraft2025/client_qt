#include "device_chart.h"
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>

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
    , refreshButton(nullptr)
    , deviceName(deviceName)
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

    setupChart();
    setupUI();
    setupTooltips();

    qDebug() << "DeviceChart 초기화 완료:" << deviceName;
}

void DeviceChart::setupChart()
{
    // 차트 생성
    chart = new QChart();
    chartView = new QChartView(chart);

    // 라인 시리즈 생성
    currentSpeedSeries = new QLineSeries();
    averageSpeedSeries = new QLineSeries();

    // 꼭짓점 표시용 포인트 시리즈 생성
    currentSpeedPoints = new QScatterSeries();
    averageSpeedPoints = new QScatterSeries();

    // 색상 설정 - 주황색 계열
    QColor currentColor = QColor(255, 140, 0);  // 주황색 (DarkOrange)
    QColor averageColor = QColor(255, 165, 0);  // 연한 주황색 (Orange)

    // 라인 설정
    currentSpeedSeries->setName("현재속도");
    currentSpeedSeries->setColor(currentColor);
    currentSpeedSeries->setPen(QPen(currentColor, 3));

    averageSpeedSeries->setName("평균속도");
    averageSpeedSeries->setColor(averageColor);
    averageSpeedSeries->setPen(QPen(averageColor, 3));

    // ✅ 꼭짓점 포인트 설정 (범례에 안 나타나도록)
    currentSpeedPoints->setColor(currentColor);
    currentSpeedPoints->setMarkerSize(6);
    currentSpeedPoints->setBorderColor(currentColor);

    averageSpeedPoints->setColor(averageColor);
    averageSpeedPoints->setMarkerSize(6);
    averageSpeedPoints->setBorderColor(averageColor);

    // ✅ 라인만 차트에 먼저 추가 (범례용)
    chart->addSeries(currentSpeedSeries);
    chart->addSeries(averageSpeedSeries);

    // ✅ 포인트는 나중에 추가 (범례 생성 후)
    chart->addSeries(currentSpeedPoints);
    chart->addSeries(averageSpeedPoints);

    // 차트 설정
    chart->setTitle("");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // ✅ 범례 크기 최소화 및 스타일링
    QFont legendFont = chart->legend()->font();
    legendFont.setPointSize(9);  // 작은 폰트
    chart->legend()->setFont(legendFont);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeRectangle);

    // ✅ 포인트 시리즈를 범례에서 숨기기 (더 확실한 방법)
    foreach(QLegendMarker* marker, chart->legend()->markers(currentSpeedPoints)) {
        marker->setVisible(false);
    }
    foreach(QLegendMarker* marker, chart->legend()->markers(averageSpeedPoints)) {
        marker->setVisible(false);
    }

    // 격자 제거 및 여백 최소화
    chart->setBackgroundBrush(QBrush(Qt::white));
    chart->setPlotAreaBackgroundBrush(QBrush(Qt::white));
    chart->setPlotAreaBackgroundVisible(false);
    chart->setMargins(QMargins(5, 5, 5, 20));  // 하단 여백만 조금 더

    // X축 설정
    axisX = new QValueAxis();
    axisX->setTitleText("");
    axisX->setRange(0, 10);
    axisX->setTickCount(6);
    axisX->setLabelFormat("%d");
    axisX->setGridLineVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);

    // Y축 설정 (0-80)
    axisY = new QValueAxis();
    axisY->setTitleText("속도");
    axisY->setRange(0, 80);
    axisY->setTickCount(5);
    axisY->setLabelFormat("%d");
    axisY->setGridLineVisible(false);
    chart->addAxis(axisY, Qt::AlignLeft);

    // 축 연결
    currentSpeedSeries->attachAxis(axisX);
    currentSpeedSeries->attachAxis(axisY);
    averageSpeedSeries->attachAxis(axisX);
    averageSpeedSeries->attachAxis(axisY);
    currentSpeedPoints->attachAxis(axisX);
    currentSpeedPoints->attachAxis(axisY);
    averageSpeedPoints->attachAxis(axisX);
    averageSpeedPoints->attachAxis(axisY);

    // 차트뷰 설정
    chartView->setRenderHint(QPainter::Antialiasing);

    qDebug() << "차트 설정 완료:" << deviceName;
}

void DeviceChart::setupUI()
{
    // 메인 위젯 생성
    mainWidget = new QWidget();

    // 새로고침 버튼 생성
    refreshButton = new QPushButton("새로고침");
    refreshButton->setMaximumSize(80, 30);
    refreshButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #4CAF50;"
        "  border: none;"
        "  color: white;"
        "  padding: 5px 10px;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #45a049;"
        "}"
        );

    // 시그널 연결
    connect(refreshButton, &QPushButton::clicked,
            this, &DeviceChart::onRefreshButtonClicked);

    // 레이아웃 설정
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    // 상단: 새로고침 버튼 (오른쪽 정렬)
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addStretch(); // 왼쪽 공간
    topLayout->addWidget(refreshButton);

    // 전체 레이아웃 구성
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(chartView);

    // 여백 설정
    mainLayout->setContentsMargins(5, 5, 5, 5);
    topLayout->setContentsMargins(0, 0, 0, 0);

    qDebug() << "UI 설정 완료:" << deviceName;
}

void DeviceChart::setupTooltips()
{
    // 현재속도 포인트 툴팁
    connect(currentSpeedPoints, &QScatterSeries::hovered,
            [this](const QPointF &point, bool state) {
                if (state) {
                    qDebug() << "현재속도 포인트 호버:" << point.y();
                    // Qt Charts 기본 툴팁 표시됨
                }
            });

    // 평균속도 포인트 툴팁
    connect(averageSpeedPoints, &QScatterSeries::hovered,
            [this](const QPointF &point, bool state) {
                if (state) {
                    qDebug() << "평균속도 포인트 호버:" << point.y();
                }
            });

    qDebug() << "툴팁 설정 완료:" << deviceName;
}

void DeviceChart::addSpeedData(int currentSpeed, int averageSpeed)
{
    qDebug() << "속도 데이터 추가:" << deviceName
             << "현재:" << currentSpeed << "평균:" << averageSpeed;

    // 새 데이터 포인트 추가
    SpeedDataPoint newPoint(QDateTime::currentDateTime(), currentSpeed, averageSpeed);
    speedDataHistory.append(newPoint);

    // 10개 이상이면 가장 오래된 데이터 제거
    while (speedDataHistory.size() > MAX_DATA_POINTS) {
        speedDataHistory.removeFirst();
    }

    // 차트 업데이트
    updateChart();
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

    // 새 데이터 추가
    for (int i = 0; i < speedDataHistory.size(); ++i) {
        const SpeedDataPoint &point = speedDataHistory[i];

        // 라인 데이터
        currentSpeedSeries->append(i, point.currentSpeed);
        averageSpeedSeries->append(i, point.averageSpeed);

        // 꼭짓점 포인트 데이터 (툴팁용)
        currentSpeedPoints->append(i, point.currentSpeed);
        averageSpeedPoints->append(i, point.averageSpeed);
    }

    // Y축 범위 고정 (0-80)
    axisY->setRange(0, 80);

    qDebug() << "차트 업데이트 완료:" << deviceName
             << "포인트 수:" << speedDataHistory.size();
}

void DeviceChart::refreshChart()
{
    qDebug() << "차트 새로고침 요청:" << deviceName;
    emit refreshRequested(deviceName);
}

void DeviceChart::onRefreshButtonClicked()
{
    qDebug() << "새로고침 버튼 클릭됨:" << deviceName;

    emit refreshRequested(deviceName);

    qDebug() << "refreshRequested 시그널 발생됨:" << deviceName;
}
