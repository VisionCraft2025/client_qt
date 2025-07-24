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

// void DeviceChart::setupChart()
// {
//     // 차트 생성
//     chart = new QChart();
//     chartView = new QChartView(chart);

//     // 라인 시리즈 생성
//     currentSpeedSeries = new QLineSeries();
//     averageSpeedSeries = new QLineSeries();

//     // 꼭짓점 표시용 포인트 시리즈 생성
//     currentSpeedPoints = new QScatterSeries();
//     averageSpeedPoints = new QScatterSeries();

//     // 색상 설정 - 주황색 계열
//     QColor currentColor = QColor(255, 140, 0);  // 주황색 (DarkOrange)
//     QColor averageColor = QColor(255, 165, 0);  // 연한 주황색 (Orange)

//     // 라인 설정
//     currentSpeedSeries->setName("현재속도");
//     currentSpeedSeries->setColor(currentColor);
//     currentSpeedSeries->setPen(QPen(currentColor, 3));

//     averageSpeedSeries->setName("평균속도");
//     averageSpeedSeries->setColor(averageColor);
//     averageSpeedSeries->setPen(QPen(averageColor, 3));

//     // ✅ 꼭짓점 포인트 설정 (범례에 안 나타나도록)
//     currentSpeedPoints->setColor(currentColor);
//     currentSpeedPoints->setMarkerSize(8);  // 크기 증가
//     currentSpeedPoints->setBorderColor(currentColor);

//     averageSpeedPoints->setColor(averageColor);
//     averageSpeedPoints->setMarkerSize(8);  // 크기 증가
//     averageSpeedPoints->setBorderColor(averageColor);

//     // 차트에 시리즈 추가
//     chart->addSeries(currentSpeedSeries);
//     chart->addSeries(averageSpeedSeries);
//     chart->addSeries(currentSpeedPoints);
//     chart->addSeries(averageSpeedPoints);

//     // 차트 제목 및 설정
//     chart->setTitle(QString("%1 속도 차트").arg(deviceName));
//     chart->setAnimationOptions(QChart::SeriesAnimations);

//     // 축 생성
//     axisX = new QValueAxis();
//     axisX->setTitleText("시간 순서");
//     axisX->setLabelFormat("%i");
//     axisX->setRange(0, 9);

//     axisY = new QValueAxis();
//     axisY->setTitleText("속도 (RPM)");
//     axisY->setLabelFormat("%i");
//     axisY->setRange(0, 80);

//     // 축을 차트에 추가
//     chart->addAxis(axisX, Qt::AlignBottom);
//     chart->addAxis(axisY, Qt::AlignLeft);

//     // 시리즈를 축에 연결
//     currentSpeedSeries->attachAxis(axisX);
//     currentSpeedSeries->attachAxis(axisY);
//     averageSpeedSeries->attachAxis(axisX);
//     averageSpeedSeries->attachAxis(axisY);
//     currentSpeedPoints->attachAxis(axisX);
//     currentSpeedPoints->attachAxis(axisY);
//     averageSpeedPoints->attachAxis(axisX);
//     averageSpeedPoints->attachAxis(axisY);

//     // ✅ 범례에서 포인트 시리즈 숨기기
//     chart->legend()->markers(currentSpeedPoints).first()->setVisible(false);
//     chart->legend()->markers(averageSpeedPoints).first()->setVisible(false);

//     // 차트뷰 설정
//     chartView->setRenderHint(QPainter::Antialiasing);

//     qDebug() << "차트 설정 완료:" << deviceName;
// }


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

    // ✅ 점 크기를 훨씬 크게
    currentSpeedPoints->setMarkerSize(9);  // 8 → 15로 점 크게
    currentSpeedPoints->setColor(QColor(255, 127, 0, 180));  // 완전 투명 → 약간 보이게
    currentSpeedPoints->setBorderColor(QColor(255, 127, 0));  // 테두리 색상 추가
    currentSpeedPoints->setPen(QPen(QColor(255, 127, 0), 2));  // 테두리 굵기

    averageSpeedPoints->setMarkerSize(9);  // 8 → 15로 점 크게
    averageSpeedPoints->setColor(QColor(255, 179, 102, 180));  // 완전 투명 → 약간 보이게
    averageSpeedPoints->setBorderColor(QColor(255, 179, 102));  // 테두리 색상 추가
    averageSpeedPoints->setPen(QPen(QColor(255, 179, 102), 2));  // 테두리 굵기

    // 차트에 시리즈 추가
    chart->addSeries(currentSpeedSeries);
    chart->addSeries(averageSpeedSeries);
    chart->addSeries(currentSpeedPoints);
    chart->addSeries(averageSpeedPoints);

    // ✅ 차트 제목 및 설정 - 간격 줄이기
    chart->setTitle(QString("%1 속도 차트").arg(deviceName));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // ✅ 차트 여백 줄이기
    chart->setMargins(QMargins(10, 5, 10, 5));  // 기본보다 작게

    // ✅ 범례 설정 - 간격 줄이기
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeRectangle);

    // 축 생성
    axisX = new QValueAxis();
    axisX->setTitleText("시간 순서");
    axisX->setLabelFormat("%i");
    // ✅ 컨베이어용이면 x축을 5로 제한
    if (deviceName == "컨베이어") {
        axisX->setRange(0, 4);  // 0~4 (5개 포인트)
    } else {
        axisX->setRange(0, 9);  // 기존 피더용
    }

    axisY = new QValueAxis();
    axisY->setTitleText("속도 (RPM)");
    axisY->setLabelFormat("%i");
    axisY->setRange(0, 80);

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

    // ✅ 범례에서 포인트 시리즈 숨기기
    chart->legend()->markers(currentSpeedPoints).first()->setVisible(false);
    chart->legend()->markers(averageSpeedPoints).first()->setVisible(false);

    // 차트뷰 설정
    chartView->setRenderHint(QPainter::Antialiasing);

    qDebug() << "차트 설정 완료:" << deviceName;
}


void DeviceChart::setupUI()
{
    // 메인 위젯 생성
    mainWidget = new QWidget();

    // 새로고침 버튼 생성 - 크기 줄이기
    refreshButton = new QPushButton("새로고침");
    refreshButton->setMaximumSize(65, 22);
    refreshButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #4CAF50;"
        "  border: none;"
        "  color: white;"
        "  padding: 2px 6px;"
        "  border-radius: 3px;"
        "  font-weight: bold;"
        "  font-size: 10px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3e8e41;"
        "}"
        );

    // 시그널 연결
    connect(refreshButton, &QPushButton::clicked,
            this, &DeviceChart::onRefreshButtonClicked, Qt::DirectConnection);

    // 레이아웃 설정
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    // 상단: 새로고침 버튼 (오른쪽 정렬) - 높이 줄이기
    QHBoxLayout *topLayout = new QHBoxLayout();

    // 갱신 안내 라벨 - 크기 줄이기
    QLabel *updateLabel = new QLabel("1분마다 자동 갱신");
    updateLabel->setStyleSheet(
        "QLabel {"
        "  color: #FF7F00;"
        "  font-size: 9px;"
        "  padding: 1px;"
        "}"
        );
    updateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    topLayout->addWidget(updateLabel);
    topLayout->addStretch();
    topLayout->addWidget(refreshButton);

    // ✅ 차트 높이를 더 크게 설정
    chartView->setMinimumHeight(280);
    chartView->setMaximumHeight(320);

    // 전체 레이아웃 구성
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(chartView, 1);

    // ✅ 여백을 더 줄이기
    mainLayout->setContentsMargins(1, 1, 1, 1);  // 3,3,3,3 → 2,2,2,2
    topLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(1);  // 3 → 2

    qDebug() << "UI 설정 완료:" << deviceName;
}

void DeviceChart::setupTooltips()
{
    // ✅ 현재속도 포인트 툴팁 (수정됨)
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

    // ✅ 평균속도 포인트 툴팁 (수정됨)
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

    // ✅ X축 범위 동적 조정
    if (deviceName == "컨베이어") {
        axisX->setRange(0, qMax(4, speedDataHistory.size() - 1));  // 컨베이어는 0~4 (5개)
    } else {
        axisX->setRange(0, qMax(9, speedDataHistory.size() - 1));  // 피더는 0~9 (10개)
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

void DeviceChart::clearAllData()
{
    speedDataHistory.clear();
    updateChart();  // 빈 차트로 업데이트
    qDebug() << "차트 데이터 완전 리셋됨:" << deviceName;
}

void DeviceChart::onRefreshButtonClicked()
{
    qDebug() << "✅ 새로고침 버튼 클릭됨:" << deviceName;

    // 즉시 시그널 발생
    emit refreshRequested(deviceName);

    // 버튼 비활성화 후 다시 활성화 (시각적 피드백)
    refreshButton->setEnabled(false);
    QTimer::singleShot(1000, [this]() {
        refreshButton->setEnabled(true);
    });

    qDebug() << "✅ refreshRequested 시그널 발생됨:" << deviceName;
}
