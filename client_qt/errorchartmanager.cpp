#include "errorchartmanager.h"

ErrorChartManager::ErrorChartManager(QObject *parent)
    : QObject(parent)
    , chart(nullptr)
    , chartView(nullptr)
    , barSeries(nullptr)
    , feederBarSet(nullptr)
    , conveyorBarSet(nullptr)
    , axisX(nullptr)
    , axisY(nullptr)
{
    initializeChart();
}

ErrorChartManager::~ErrorChartManager()
{
    // parent가 있으므로 자동 삭제됨
}

QChartView* ErrorChartManager::getChartView() const
{
    return chartView;
}

void ErrorChartManager::initializeChart()
{
    qDebug() << "=== 차트 초기화 시작 ===";

    setupChart();
    qDebug() << "차트 초기화 완료";
}

void ErrorChartManager::setupChart()
{
    // 차트 생성
    chart = new QChart();
    chartView = new QChartView(chart);

    // 바 시리즈 생성
    barSeries = new QBarSeries();

    // 바셋 생성 (feeder, conveyor만)
    feederBarSet = new QBarSet("feeder");
    conveyorBarSet = new QBarSet("conveyor");

    // 색상 설정
    feederBarSet->setColor(QColor(255, 99, 132));  // 빨간색
    conveyorBarSet->setColor(QColor(54, 162, 235)); // 파란색

    // 바셋을 시리즈에 추가
    barSeries->append(feederBarSet);
    barSeries->append(conveyorBarSet);

    // 차트에 시리즈 추가
    chart->addSeries(barSeries);
    chart->setTitle("월별 디바이스 오류 발생 일수 (2025.01.16 ~ 2025.06.17)");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // X축 (월별) - 1월부터 6월까지
    axisX = new QBarCategoryAxis();
    QStringList categories = getTargetMonths();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    // Y축 (오류 일수)
    axisY = new QValueAxis();
    axisY->setRange(0, 10);
    axisY->setTickCount(6);
    chart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    // 차트뷰 설정
    chartView->setRenderHint(QPainter::Antialiasing);

    // 초기 데이터 설정 (모두 0) - 6개월
    for(int i = 0; i < 6; i++) {
        feederBarSet->append(0);
        conveyorBarSet->append(0);
    }
    qDebug() << "차트 설정 완료 (2025.01.16 ~ 2025.06.17 범위)";
}

// 수정된 메소드: 2025년 1월-6월 반환
QStringList ErrorChartManager::getTargetMonths()
{
    QStringList months;
    // 2025년 1월부터 6월까지 (원래대로)
    for(int i = 1; i <= 6; i++) {
        months.append(QString("%1월").arg(i, 2, 10, QChar('0')));
    }
    qDebug() << "타겟 월들:" << months;
    return months;
}

void ErrorChartManager::processErrorData(const QJsonObject &errorData)
{
    qDebug() << " [CHART] 차트 데이터 처리 시작";
    qDebug() << " [CHART] 전체 errorData:" << errorData;

    QString deviceId = errorData["device_id"].toString();
    qint64 timestamp = errorData["timestamp"].toVariant().toLongLong();

    qDebug() << " [CHART] 디바이스:" << deviceId << "타임스탬프:" << timestamp;

    if(deviceId.isEmpty()) {
        qDebug() << " [CHART] deviceId가 비어있음!";
        return;
    }

    if(timestamp == 0) {
        qDebug() << " [CHART] 타임스탬프가 0이므로 처리 건너뜀";
        return;
    }

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    QString monthKey = dateTime.toString("yyyy-MM");
    QString dayKey = dateTime.toString("yyyy-MM-dd");

    qDebug() << " [CHART] 날짜 변환 - 월키:" << monthKey << "일키:" << dayKey;
    qDebug() << " [CHART] 변환된 날짜:" << dateTime.toString("yyyy-MM-dd hh:mm:ss");

    //  원래대로 6월까지만 (2025-01-16 ~ 2025-06-17)
    QDateTime startRange = QDateTime::fromString("2025-01-16T00:00:00", Qt::ISODate);
    QDateTime endRange = QDateTime::fromString("2025-06-17T23:59:59", Qt::ISODate);

    if(dateTime < startRange || dateTime > endRange) {
        qDebug() << " [CHART] 허용 범위(2025-01-16 ~ 2025-06-17) 외 데이터:" << dayKey;
        qDebug() << " [CHART] 현재 시간:" << dateTime.toString("yyyy-MM-dd hh:mm:ss");
        return;
    }

    // // 7월 16,17,18일 제외 (원래 로직 유지)
    // int year = dateTime.date().year();
    // int month = dateTime.date().month();
    // int day = dateTime.date().day();

    // if(year == 2025 && month == 7 && (day == 16 || day == 17 || day == 18)) {
    //     qDebug() << " [CHART] 7월 16,17,18일 테스트 데이터 제외:" << dayKey;
    //     return;
    // }

    // 디바이스 타입 인식
    QString deviceType;
    QString lowerDeviceId = deviceId.toLower();

    if(lowerDeviceId.contains("feeder")) {
        deviceType = "feeder";
    } else if(lowerDeviceId.contains("conveyor")) {
        deviceType = "conveyor";
    } else {
        qDebug() << " [CHART] 알 수 없는 디바이스 타입:" << deviceId;
        qDebug() << " [CHART] 지원되는 타입: feeder, conveyor";
        return;
    }

    qDebug() << " [CHART] 디바이스 타입:" << deviceType;
    qDebug() << " [CHART] 허용된 데이터 - 처리 진행";

    // 하루에 1개만 카운트
    bool isNewDay = !monthlyErrorDays[monthKey][deviceType].contains(dayKey);

    if(isNewDay) {
        monthlyErrorDays[monthKey][deviceType].insert(dayKey);
        qDebug() << " [CHART] 새로운 에러 날짜 추가:" << dayKey << deviceType;
        qDebug() << " [CHART] 현재" << monthKey << "월" << deviceType << "오류 일수:" << monthlyErrorDays[monthKey][deviceType].size();

        updateErrorChart();
    } else {
        qDebug() << "🔄 [CHART] 이미 존재하는 날짜:" << dayKey << deviceType;
    }

    // 현재 저장된 모든 데이터 출력
    qDebug() << "📊 [CHART] 현재 저장된 월별 오류 데이터:";
    for(auto monthIt = monthlyErrorDays.begin(); monthIt != monthlyErrorDays.end(); ++monthIt) {
        QString month = monthIt.key();
        for(auto deviceIt = monthIt.value().begin(); deviceIt != monthIt.value().end(); ++deviceIt) {
            QString device = deviceIt.key();
            int count = deviceIt.value().size();
            qDebug() << "  [CHART]" << month << "-" << device << ":" << count << "일";
        }
    }
}

void ErrorChartManager::updateErrorChart()
{
    if(!feederBarSet || !conveyorBarSet) {
        qDebug() << "차트 바셋이 null입니다!";
        return;
    }

    //qDebug() << "=== 차트 업데이트 시작 ===";
    //qDebug() << "현재 monthlyErrorDays 크기:" << monthlyErrorDays.size();

    // 기존 데이터 클리어
    feederBarSet->remove(0, feederBarSet->count());
    conveyorBarSet->remove(0, conveyorBarSet->count());

    int maxValue = 0;

    //  수정: 2025년 1월부터 6월까지 처리 (원래대로 복원)
    for(int month = 1; month <= 6; month++) {
        QString monthKey = QString("2025-%1").arg(month, 2, 10, QChar('0'));

        int feederCount = monthlyErrorDays[monthKey]["feeder"].size();
        int conveyorCount = monthlyErrorDays[monthKey]["conveyor"].size();

        feederBarSet->append(feederCount);
        conveyorBarSet->append(conveyorCount);

        qDebug() << "월:" << monthKey << "feeder:" << feederCount << "conveyor:" << conveyorCount;

        maxValue = qMax(maxValue, qMax(feederCount, conveyorCount));
    }

    // Y축 범위 동적 조정
    int yAxisMax = qMax(5, maxValue + 2);
    axisY->setRange(0, yAxisMax);
    //qDebug() << "Y축 범위 설정: 0 ~" << yAxisMax;

    //qDebug() << "=== 차트 업데이트 완료 (최대값:" << yAxisMax << ") ===";
}

//  추가: 차트 데이터 초기화 메서드 (디버깅용)
void ErrorChartManager::clearChartData()
{
    monthlyErrorDays.clear();
    updateErrorChart();
    //qDebug() << " 차트 데이터 초기화 완료";
}

//  추가: 현재 차트 데이터 상태 출력 (디버깅용)
void ErrorChartManager::printChartDataStatus()
{

    for(auto monthIt = monthlyErrorDays.begin(); monthIt != monthlyErrorDays.end(); ++monthIt) {
        QString month = monthIt.key();
       // qDebug() << " 월:" << month;

        for(auto deviceIt = monthIt.value().begin(); deviceIt != monthIt.value().end(); ++deviceIt) {
            QString device = deviceIt.key();
            QSet<QString> days = deviceIt.value();
           // qDebug() << " " << device << ":" << days.size() << "일";

            // 실제 날짜들 출력
            QStringList dayList = days.values();
            dayList.sort();
            //qDebug() << "    날짜들:" << dayList;
        }
    }
   // qDebug() << "==================";
}
