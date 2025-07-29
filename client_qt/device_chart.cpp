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

// void DeviceChart::addInitialZeroPoints()
// {
//     qDebug() << "초기 0 지점 데이터 추가:" << deviceName;

//     // 0분에 현재속도=0, 평균속도=0 추가
//     timeCounter = 1; // 1분부터 시작

//     // 0 지점 데이터 추가
//     SpeedDataPoint initialPoint(QDateTime::currentDateTime(), 0, 0);
//     speedDataHistory.append(initialPoint);

//     // 차트에 0 지점 표시
//     currentSpeedSeries->append(0, 0);
//     averageSpeedSeries->append(0, 0);
//     currentSpeedPoints->append(0, 0);
//     averageSpeedPoints->append(0, 0);

//     // X축 제목 업데이트
//     updateXAxisLabels();

//     qDebug() << "초기 0 지점 데이터 추가 완료";
// }

void DeviceChart::setupChart()
{
    // 차트 & 뷰 생성
    chart = new QChart();
    chartView = new QChartView(chart);

    // 시리즈 생성
    currentSpeedSeries = new QLineSeries();
    averageSpeedSeries = new QLineSeries();
    currentSpeedPoints = new QScatterSeries();
    averageSpeedPoints = new QScatterSeries();

    // 선 스타일
    currentSpeedSeries->setName("현재속도");
    currentSpeedSeries->setColor(QColor("#FB923C"));
    currentSpeedSeries->setPen(QPen(QColor("#FB923C"), 2.2));

    averageSpeedSeries->setName("평균속도");
    averageSpeedSeries->setColor(QColor("#4F936D"));
    averageSpeedSeries->setPen(QPen(QColor("#4F936D"), 2.2));

    // 포인트 스타일
    currentSpeedPoints->setMarkerSize(8);
    currentSpeedPoints->setColor(QColor("#FB923C"));
    currentSpeedPoints->setBorderColor(QColor("#FB923C"));
    currentSpeedPoints->setPen(QPen(QColor("#FB923C"), 1.5));

    averageSpeedPoints->setMarkerSize(8);
    averageSpeedPoints->setColor(QColor("#4F936D"));
    averageSpeedPoints->setBorderColor(QColor("#4F936D"));
    averageSpeedPoints->setPen(QPen(QColor("#4F936D"), 1.5));


    chart->setTitle("");  // 타이틀 제거
    chart->setTitleFont(QFont());  // 타이틀 폰트 제거
    chart->setMargins(QMargins(10, 5, 10, 5));  // 위쪽 여백 줄임


    // 차트에 시리즈 추가
    chart->addSeries(currentSpeedSeries);
    chart->addSeries(averageSpeedSeries);
    chart->addSeries(currentSpeedPoints);
    chart->addSeries(averageSpeedPoints);

    // 기본 스타일
    //chart->setTitle(QString("%1 속도 차트").arg(deviceName));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setMargins(QMargins(10, 10, 10, 5));
    chart->setBackgroundBrush(QBrush(Qt::white));
    chart->setPlotAreaBackgroundBrush(QBrush(Qt::white));
    chart->setPlotAreaBackgroundVisible(true);

    // X축
    axisX = new QValueAxis();
    axisX->setTitleText("\n시간 (분)");
    axisX->setLabelFormat("%i");
    axisX->setLabelsBrush(QBrush(QColor("#6B7280")));  // 중간 회색
    axisX->setTitleBrush(QBrush(QColor("#374151")));   // 진회색

    // 기본 범위
    axisX->setRange(0, (deviceName == "컨베이어") ? 4 : 9);

    // Y축
    axisY = new QValueAxis();
    axisY->setTitleText("RPM\n");
    axisY->setLabelFormat("%i");
    axisY->setRange(0, 60);
    axisY->setLabelsBrush(QBrush(QColor("#6B7280")));
    axisY->setTitleBrush(QBrush(QColor("#374151")));

    // 축 스타일 - 연한 그리드
    QPen gridPen(QColor("#E5E7EB")); // 연회색
    gridPen.setWidth(1);
    axisX->setGridLinePen(gridPen);
    axisY->setGridLinePen(gridPen);
    axisX->setLinePen(gridPen);
    axisY->setLinePen(gridPen);

    // 축 추가
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

    // 포인트는 범례에서 숨김
    chart->legend()->markers(currentSpeedPoints).first()->setVisible(false);
    chart->legend()->markers(averageSpeedPoints).first()->setVisible(false);

    // 범례 스타일
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeFromSeries);
    chart->legend()->setContentsMargins(0, 0, 0, 0); // legend의 margin을 0으로


    for (QLegendMarker* marker : chart->legend()->markers()) {
        marker->setLabelBrush(QBrush(QColor("#374151")));

    }

    // 차트뷰 기본 스타일
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setFrameStyle(QFrame::NoFrame);

    qDebug() << "✅ setupChart() 완료:" << deviceName;
}


void DeviceChart::updateXAxisLabels()
{
    int dataCount = speedDataHistory.size();

    if (dataCount > 0) {
        int endTime = timeCounter;
        int startTime = qMax(0, timeCounter - (dataCount - 1));

        QString axisTitle = QString("시간 (%1~%2분)").arg(startTime).arg(endTime);
        axisX->setTitleText(axisTitle);

        qDebug() << "X축 제목 업데이트:" << axisTitle;
    }
}


void DeviceChart::setupUI()
{
    mainWidget = new QWidget();
    mainWidget->setStyleSheet("background-color: #ffffff;"); // 전체 위젯도 흰색
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(1);

    //타이틀 + "1분" 라벨 수평 배치
    QWidget *titleRow = new QWidget();
    titleRow->setStyleSheet("background-color: #ffffff;"); // 타이틀 배경도 흰색
    QHBoxLayout *titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel(QString("%1 속도 차트").arg(deviceName));
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 16px;
            color: #000000;
        }
    )");

    QLabel *refreshLabel = new QLabel("1분");
    refreshLabel->setStyleSheet(R"(
        QLabel {
            background-color: #FFEDD5;
            color: #FB923C;
            font-size: 9px;
            padding: 2px 6px;
            border-radius: 4px;
            border: 1px solid #FDBA74;
        }
    )");

    titleLayout->addStretch();
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(refreshLabel);
    titleLayout->addStretch();

    // 차트 뷰 감싸는 컨테이너
    QWidget *chartContainer = new QWidget();
    chartContainer->setStyleSheet("background-color: #ffffff;"); // 차트 컨테이너 배경 흰색
    QVBoxLayout *containerLayout = new QVBoxLayout(chartContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(chartView);

    chartView->setMinimumHeight(220);
    chartView->setMaximumHeight(260);

    // 레이아웃에 배치
    mainLayout->addWidget(titleRow);
    mainLayout->addSpacing(0); // 간격을 0으로 (또는 아주 작게)
    mainLayout->addWidget(chartContainer, 1);

    qDebug() << "UI 설정 완료 (타이틀 + 1분 라벨 포함):" << deviceName;
}



void DeviceChart::setupTooltips()
{
    // 현재속도 포인트 툴팁 (수정됨)
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

    // 평균속도 포인트 툴팁 (수정됨)
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


// void DeviceChart::addSpeedData(int currentSpeed, int averageSpeed)
// {
//     qDebug() << "속도 데이터 추가:" << deviceName
//              << "현재:" << currentSpeed << "평균:" << averageSpeed;

//     globalTimeIndex++;

//     // 새 데이터 포인트 추가
//     SpeedDataPoint newPoint(QDateTime::currentDateTime(), currentSpeed, averageSpeed);
//     speedDataHistory.append(newPoint);

//     // ✅ 디바이스별 최대 개수에 맞춰 오래된 데이터 제거
//     int maxPoints = getMaxDataPoints();
//     while (speedDataHistory.size() > maxPoints) {
//         speedDataHistory.removeFirst();
//     }

//     // 10개 이상이면 가장 오래된 데이터 제거
//     while (speedDataHistory.size() > MAX_DATA_POINTS) {
//         speedDataHistory.removeFirst();
//     }

//     // 차트 업데이트
//     updateChart();
// }

//이거------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// void DeviceChart::addSpeedData(int currentSpeed, int averageSpeed)
// {
//     qDebug() << "RPM 데이터 추가:" << deviceName
//              << "현재:" << currentSpeed << "평균:" << averageSpeed;

//     // ✅ 실제 데이터 받으면 timeCounter 증가 (0→1→2→3...)
//     timeCounter++;

//     // 새 데이터 포인트 추가
//     SpeedDataPoint newPoint(QDateTime::currentDateTime(), currentSpeed, averageSpeed);
//     speedDataHistory.append(newPoint);

//     // ✅ 컨베이어는 5개, 피더는 10개로 제한
//     int maxPoints;
//     if (deviceName == "컨베이어") {
//         maxPoints = 5;
//     } else {
//         maxPoints = 10;
//     }

//     while (speedDataHistory.size() > maxPoints) {
//         speedDataHistory.removeFirst();  // 가장 오래된 데이터 제거
//     }

//     qDebug() << "현재 시간:" << timeCounter << "분, 데이터 개수:" << speedDataHistory.size();

//     // 차트 업데이트
//     updateChart();
// }
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

// void DeviceChart::updateChart()
// {
//     if (speedDataHistory.isEmpty()) {
//         qDebug() << "데이터가 없어서 차트 업데이트 건너뜀";
//         return;
//     }

//     // 기존 데이터 클리어
//     currentSpeedSeries->clear();
//     averageSpeedSeries->clear();
//     currentSpeedPoints->clear();
//     averageSpeedPoints->clear();

//     // ✅ 실제 시간 인덱스로 데이터 추가
//     for (int i = 0; i < speedDataHistory.size(); ++i) {
//         const SpeedDataPoint &point = speedDataHistory[i];
//         int timeIndex = dataTimeIndices[i];  // 실제 시간 인덱스 사용

//         // 라인 데이터
//         currentSpeedSeries->append(timeIndex, point.currentSpeed);
//         averageSpeedSeries->append(timeIndex, point.averageSpeed);

//         // 꼭짓점 포인트 데이터 (툴팁용)
//         currentSpeedPoints->append(timeIndex, point.currentSpeed);
//         averageSpeedPoints->append(timeIndex, point.averageSpeed);
//     }

//     // ✅ X축 범위를 실제 시간 범위로 설정
//     if (!dataTimeIndices.isEmpty()) {
//         int minTime = dataTimeIndices.first();
//         int maxTime = dataTimeIndices.last();

//         if (deviceName == "컨베이어") {
//             // 컨베이어는 5개 범위로 설정
//             axisX->setRange(minTime, qMax(minTime + 4, maxTime));
//         } else {
//             // 피더는 10개 범위로 설정
//             axisX->setRange(minTime, qMax(minTime + 9, maxTime));
//         }
//     }

//     // Y축 범위 고정 (0-80)
//     axisY->setRange(0, 80);

//     qDebug() << "차트 업데이트 완료:" << deviceName
//              << "포인트 수:" << speedDataHistory.size()
//              << "시간 범위:" << (dataTimeIndices.isEmpty() ? 0 : dataTimeIndices.first())
//              << "-" << (dataTimeIndices.isEmpty() ? 0 : dataTimeIndices.last());
// }

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

    // 컨베이어와 피더별로 다른 윈도우 크기
    int maxDataPoints = (deviceName == "컨베이어") ? 5 : 10;

    // 현재 윈도우 계산 - 고정된 슬라이딩 방식
    int dataCount = speedDataHistory.size();

    qDebug() << deviceName << "데이터 개수:" << dataCount << "timeCounter:" << timeCounter;

    // 실제 슬라이딩을 위한 데이터 인덱스 계산
    int startIndex = qMax(0, dataCount - maxDataPoints);  // 데이터 배열에서 시작 인덱스

    // 차트에 표시할 시간 범위 계산 (항상 연속된 정수)
    int displayStartTime = 0;  // 항상 0부터 시작
    if (dataCount >= maxDataPoints) {
        // 데이터가 충분할 때: 슬라이딩 시작
        displayStartTime = timeCounter - maxDataPoints + 1;
        // 하지만 음수가 되지 않도록 보장
        displayStartTime = qMax(0, displayStartTime);
    }

    for (int i = startIndex; i < dataCount; ++i) {
        const SpeedDataPoint &point = speedDataHistory[i];

        // 차트에 표시할 시간 인덱스 계산
        int chartTimeIndex = displayStartTime + (i - startIndex);

        // 라인 데이터
        currentSpeedSeries->append(chartTimeIndex, point.currentSpeed);
        averageSpeedSeries->append(chartTimeIndex, point.averageSpeed);

        // 포인트 데이터 (툴팁용)
        currentSpeedPoints->append(chartTimeIndex, point.currentSpeed);
        averageSpeedPoints->append(chartTimeIndex, point.averageSpeed);

        qDebug() << "차트 시간:" << chartTimeIndex << "분, 현재:" << point.currentSpeed << "RPM, 평균:" << point.averageSpeed << "RPM";
    }

    // X축 범위 설정 - 항상 연속된 양수
    if (deviceName == "컨베이어") {
        // 컨베이어: 5분 윈도우 (예: 0,1,2,3,4 → 1,2,3,4,5 → 2,3,4,5,6)
        int rangeStart = displayStartTime;
        int rangeEnd = displayStartTime + maxDataPoints - 1;

        axisX->setRange(rangeStart, rangeEnd);
        qDebug() << "컨베이어 X축 범위:" << rangeStart << "~" << rangeEnd << "분";
    } else {
        // 피더: 10분 윈도우
        int rangeStart = displayStartTime;
        int rangeEnd = displayStartTime + maxDataPoints - 1;

        axisX->setRange(rangeStart, rangeEnd);
        qDebug() << "피더 X축 범위:" << rangeStart << "~" << rangeEnd << "분";
    }

    // X축 제목 업데이트
    updateXAxisLabels();

    qDebug() << "차트 슬라이딩 업데이트 완료:" << deviceName;
}


// void DeviceChart::updateXAxisLabels()
// {
//     // ✅ 현재 윈도우의 실제 시간 계산
//     int dataCount = speedDataHistory.size();

//     if (deviceName == "컨베이어" && dataCount > 0) {
//         // 현재 윈도우: 예를 들어 timeCounter=7이면 3,4,5,6,7분
//         int endTime = timeCounter;
//         int startTime = endTime - dataCount + 1;

//         // X축 라벨을 실제 시간으로 설정
//         QStringList labels;
//         for (int i = 0; i < 5; i++) {
//             if (i < dataCount) {
//                 labels << QString("%1분").arg(startTime + i);
//             } else {
//                 labels << "";
//             }
//         }

//         qDebug() << "X축 라벨 업데이트:" << labels;
//         qDebug() << "실제 시간 윈도우:" << startTime << "~" << endTime << "분";
//     }
// }

// QString DeviceChart::getCurrentTimeWindow()
// {
//     int dataCount = speedDataHistory.size();
//     if (dataCount == 0) return "빈 윈도우";

//     int endTime = timeCounter;
//     int startTime = endTime - dataCount + 1;

//     return QString("%1~%2분").arg(startTime).arg(endTime);
// }



void DeviceChart::refreshChart()
{
    qDebug() << "차트 새로고침 요청:" << deviceName;
    emit refreshRequested(deviceName);
}

void DeviceChart::clearAllData() {
    currentSpeedSeries->clear();
    averageSpeedSeries->clear();
    speedDataHistory.clear();  // 데이터만 클리어
    // ✅ timeCounter는 리셋하지 않음! (이게 0->1->2 문제의 원인이었음)
    qDebug() << "차트 데이터 클리어 (timeCounter 유지:" << timeCounter << ")";
}
