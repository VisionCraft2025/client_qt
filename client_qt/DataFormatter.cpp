#include "DataFormatter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDateTime>
#include <algorithm>
#include <cmath>

namespace DataFormatter {

QString formatDateStats(const QString& rawData) {
    QStringList lines = rawData.split('\n');
    QMap<QString, int> dateData;
    
    // 날짜 데이터 파싱
    QRegularExpression dateRegex(R"---("(\d{4}-\d{2}-\d{2})": (\d+))---");
    for (const QString& line : lines) {
        QRegularExpressionMatch match = dateRegex.match(line);
        if (match.hasMatch()) {
            QString date = match.captured(1);
            int count = match.captured(2).toInt();
            dateData[date] = count;
        }
    }
    
    if (dateData.isEmpty()) {
        return rawData;  // 파싱 실패 시 원본 반환
    }
    
    // 통계 계산
    int total = 0;
    int max = 0;
    int min = INT_MAX;
    QString maxDate, minDate;
    
    for (auto it = dateData.begin(); it != dateData.end(); ++it) {
        total += it.value();
        if (it.value() > max) {
            max = it.value();
            maxDate = it.key();
        }
        if (it.value() < min) {
            min = it.value();
            minDate = it.key();
        }
    }
    
    double average = static_cast<double>(total) / dateData.size();
    
    // 월별 그룹화
    QMap<QString, int> monthlyData;
    for (auto it = dateData.begin(); it != dateData.end(); ++it) {
        QString month = it.key().left(7);  // YYYY-MM
        monthlyData[month] += it.value();
    }
    
    // 포맷팅된 출력 생성
    QString formatted;
    formatted += "📊 **날짜별 로그 통계 분석**\n\n";
    
    // 요약 정보
    formatted += "### 📈 전체 요약\n";
    formatted += QString("- 총 로그 수: **%1개**\n").arg(total);
    formatted += QString("- 분석 기간: %1 ~ %2 (%3일간)\n")
        .arg(dateData.firstKey())
        .arg(dateData.lastKey())
        .arg(dateData.size());
    formatted += QString("- 일 평균: **%1개**\n").arg(QString::number(average, 'f', 1));
    formatted += QString("- 최다 발생일: %1 (**%2개**)\n").arg(maxDate).arg(max);
    formatted += QString("- 최소 발생일: %1 (**%2개**)\n\n").arg(minDate).arg(min);
    
    // 월별 요약
    formatted += "### 📅 월별 통계\n";
    formatted += "```\n";
    for (auto it = monthlyData.begin(); it != monthlyData.end(); ++it) {
        QString monthStr = QDateTime::fromString(it.key() + "-01", "yyyy-MM-dd").toString("yyyy년 MM월");
        int barLength = static_cast<int>((it.value() / static_cast<double>(total)) * 50);
        QString bar(barLength, QChar(0x2588)); // █ 문자
        formatted += QString("%1: %2 %3개\n")
            .arg(monthStr, -12)
            .arg(bar, -50)
            .arg(it.value(), 4);
    }
    formatted += "```\n\n";
    
    // 주요 이상치 표시
    formatted += "### ⚠️ 주목할 만한 날짜\n";
    QList<QPair<QString, int>> sortedDates;
    for (auto it = dateData.begin(); it != dateData.end(); ++it) {
        sortedDates.append({it.key(), it.value()});
    }
    std::sort(sortedDates.begin(), sortedDates.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // 상위 5개 표시
    for (int i = 0; i < std::min(5, static_cast<int>(sortedDates.size())); ++i) {
        QString date = sortedDates[i].first;
        int count = sortedDates[i].second;
        if (count > average * 2) {  // 평균의 2배 이상인 경우만
            QString dayOfWeek = QDate::fromString(date, "yyyy-MM-dd").toString("ddd");
            formatted += QString("- %1 (%2): **%3개** 🔴 평균 대비 %4%% 증가\n")
                .arg(date)
                .arg(dayOfWeek)
                .arg(count)
                .arg(static_cast<int>((count / average - 1) * 100));
        }
    }
    
    return formatted;
}

QString formatDeviceStats(const QString& rawData) {
    QStringList lines = rawData.split('\n');
    QMap<QString, int> deviceData;
    
    // 디바이스 데이터 파싱
    QRegularExpression deviceRegex(R"---("([^"]+)": (\d+))---");
    for (const QString& line : lines) {
        QRegularExpressionMatch match = deviceRegex.match(line);
        if (match.hasMatch()) {
            QString device = match.captured(1);
            int count = match.captured(2).toInt();
            deviceData[device] = count;
        }
    }
    
    if (deviceData.isEmpty()) {
        return rawData;
    }
    
    // 통계 계산
    int total = 0;
    for (auto count : deviceData.values()) {
        total += count;
    }
    
    // 포맷팅된 출력 생성
    QString formatted;
    formatted += "🏭 **디바이스별 로그 통계**\n\n";
    
    // 디바이스 유형별 그룹화
    int conveyorTotal = 0, feederTotal = 0, robotTotal = 0;
    
    for (auto it = deviceData.begin(); it != deviceData.end(); ++it) {
        if (it.key().contains("conveyor")) conveyorTotal += it.value();
        else if (it.key().contains("feeder")) feederTotal += it.value();
        else if (it.key().contains("robot")) robotTotal += it.value();
    }
    
    // 전체 요약
    formatted += "### 📊 전체 현황\n";
    formatted += QString("- 총 로그 수: **%1개**\n").arg(total);
    formatted += QString("- 디바이스 수: **%1대**\n\n").arg(deviceData.size());
    
    // 디바이스 타입별 통계
    formatted += "### 🔧 장비 타입별 분석\n";
    formatted += "```\n";
    if (conveyorTotal > 0) {
        int percent = (conveyorTotal * 100) / total;
        QString bar(percent/2, QChar(0x2588)); // █ 문자
        formatted += QString("컨베이어: %1 %2개 (%3%%)\n")
            .arg(bar, -50).arg(conveyorTotal, 4).arg(percent);
    }
    if (feederTotal > 0) {
        int percent = (feederTotal * 100) / total;
        QString bar(percent/2, QChar(0x2588)); // █ 문자
        formatted += QString("피더    : %1 %2개 (%3%%)\n")
            .arg(bar, -50).arg(feederTotal, 4).arg(percent);
    }
    if (robotTotal > 0) {
        int percent = (robotTotal * 100) / total;
        QString bar(percent/2, QChar(0x2588)); // █ 문자
        formatted += QString("로봇팔  : %1 %2개 (%3%%)\n")
            .arg(bar, -50).arg(robotTotal, 4).arg(percent);
    }
    formatted += "```\n\n";
    
    // 개별 디바이스 상세
    formatted += "### 📋 디바이스별 상세\n";
    
    // 정렬
    QList<QPair<QString, int>> sortedDevices;
    for (auto it = deviceData.begin(); it != deviceData.end(); ++it) {
        sortedDevices.append({it.key(), it.value()});
    }
    std::sort(sortedDevices.begin(), sortedDevices.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& device : sortedDevices) {
        QString deviceName = device.first;
        int count = device.second;
        int percent = (count * 100) / total;
        
        // 디바이스 이름을 한글로 변환
        QString displayName = deviceName;
        if (deviceName.contains("conveyor")) {
            displayName = QString("컨베이어 %1번").arg(deviceName.right(2));
        } else if (deviceName.contains("feeder")) {
            displayName = QString("피더 %1번").arg(deviceName.right(2));
        } else if (deviceName.contains("robot")) {
            displayName = "로봇팔";
        }
        
        // 상태 아이콘 결정
        QString statusIcon = "🟢";  // 정상
        if (percent > 50) statusIcon = "🔴";  // 높음
        else if (percent > 30) statusIcon = "🟡";  // 주의
        
        formatted += QString("%1 **%2**: %3개 (%4%%)\n")
            .arg(statusIcon)
            .arg(displayName)
            .arg(count)
            .arg(percent);
    }
    
    return formatted;
}

QString formatErrorCodeStats(const QString& rawData) {
    QStringList lines = rawData.split('\n');
    QMap<QString, int> errorData;
    
    // 에러 코드 데이터 파싱
    QRegularExpression errorRegex(R"---("([^"]+)": (\d+))---");
    for (const QString& line : lines) {
        QRegularExpressionMatch match = errorRegex.match(line);
        if (match.hasMatch()) {
            QString errorCode = match.captured(1);
            int count = match.captured(2).toInt();
            errorData[errorCode] = count;
        }
    }
    
    if (errorData.isEmpty()) {
        return rawData;
    }
    
    // 에러 코드 설명 매핑
    QMap<QString, QString> errorDescriptions = {
        {"SPD", "모터 속도 이상"},
        {"EMG", "비상정지"},
        {"TMP", "온도 경고"},
        {"VIB", "진동 이상"},
        {"PWR", "전원 이상"},
        {"SEN", "센서 오류"},
        {"OVL", "과부하"},
        {"COM", "통신 오류"}
    };
    
    // 통계 계산
    int total = 0;
    for (auto count : errorData.values()) {
        total += count;
    }
    
    // 포맷팅된 출력 생성
    QString formatted;
    formatted += "🚨 **오류 코드별 통계 분석**\n\n";
    
    // 전체 요약
    formatted += "### 📊 오류 현황 요약\n";
    formatted += QString("- 총 오류 수: **%1개**\n").arg(total);
    formatted += QString("- 오류 유형: **%1종류**\n\n").arg(errorData.size());
    
    // 오류 코드별 상세
    formatted += "### 🔍 오류 유형별 분석\n";
    
    // 정렬
    QList<QPair<QString, int>> sortedErrors;
    for (auto it = errorData.begin(); it != errorData.end(); ++it) {
        sortedErrors.append({it.key(), it.value()});
    }
    std::sort(sortedErrors.begin(), sortedErrors.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // 상위 오류 표시
    for (const auto& error : sortedErrors) {
        QString errorCode = error.first;
        int count = error.second;
        int percent = (count * 100) / total;
        
        QString description = errorDescriptions.value(errorCode, "기타 오류");
        
        // 심각도에 따른 아이콘
        QString severityIcon;
        if (errorCode == "EMG" || errorCode == "PWR") severityIcon = "🔴";  // 심각
        else if (errorCode == "SPD" || errorCode == "TMP") severityIcon = "🟡";  // 주의
        else severityIcon = "🟢";  // 일반
        
        // 바 차트
        int barLength = std::min(percent / 2, 40);
        QString bar(barLength, QChar(0x2588)); // █ 문자
        
        formatted += QString("\n%1 **%2** - %3\n").arg(severityIcon).arg(errorCode).arg(description);
        formatted += QString("   %1 %2개 (%3%%)\n").arg(bar, -40).arg(count, 4).arg(percent);
    }
    
    // 권장사항
    formatted += "\n### 💡 권장 조치사항\n";
    if (sortedErrors.size() > 0 && sortedErrors[0].second > total * 0.3) {
        QString topError = sortedErrors[0].first;
        QString desc = errorDescriptions.value(topError, "");
        formatted += QString("- **%1(%2)** 오류가 전체의 %3%%를 차지합니다. 우선적인 점검이 필요합니다.\n")
            .arg(topError)
            .arg(desc)
            .arg((sortedErrors[0].second * 100) / total);
    }
    
    return formatted;
}

QString formatDatabaseInfo(const QString& rawData) {
    // 데이터베이스 정보를 보기 좋게 포맷팅
    QString formatted;
    formatted += "🗄️ **데이터베이스 정보**\n\n";
    
    if (rawData.contains("컬렉션")) {
        formatted += "### 📁 컬렉션 목록\n";
        // 컬렉션 정보 파싱 및 포맷팅
    }
    
    return formatted;
}

QString formatExecutionResult(const QString& toolName, const QString& rawResult) {
    // 도구 이름과 결과 내용에 따라 적절한 포매터 선택
    if (rawResult.contains("날짜별 통계") || rawResult.contains("date_stats")) {
        return formatDateStats(rawResult);
    } else if (rawResult.contains("디바이스별 통계") || rawResult.contains("device_stats")) {
        return formatDeviceStats(rawResult);
    } else if (rawResult.contains("오류 코드") || rawResult.contains("error_stats")) {
        return formatErrorCodeStats(rawResult);
    } else if (toolName == "db_info") {
        return formatDatabaseInfo(rawResult);
    }
    
    // 기본 포맷팅
    return formatGenericResult(rawResult);
}

QString formatGenericResult(const QString& rawResult) {
    // 일반적인 결과를 보기 좋게 포맷팅
    QString formatted = rawResult;
    
    // 숫자 강조
    QRegularExpression numRegex(R"---((\d+)개)---");
    formatted.replace(numRegex, "**\\1개**");
    
    // 리스트 항목 개선
    QStringList lines = formatted.split('\n');
    QString result;
    for (const QString& line : lines) {
        if (line.trimmed().startsWith("-")) {
            result += line + "\n";
        } else if (line.contains(":") && line.count('"') >= 2) {
            // "key": value 형식을 보기 좋게
            result += "  • " + line.trimmed() + "\n";
        } else {
            result += line + "\n";
        }
    }
    
    return result;
}

} // namespace DataFormatter