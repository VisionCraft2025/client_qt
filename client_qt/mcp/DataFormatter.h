#ifndef DATAFORMATTER_H
#define DATAFORMATTER_H

#include <QString>
#include <QJsonObject>

namespace DataFormatter {
    // 메인 포맷팅 함수
    QString formatExecutionResult(const QString& toolName, const QString& rawResult);
    
    // 특정 데이터 타입별 포매터
    QString formatDateStats(const QString& rawData);
    QString formatDeviceStats(const QString& rawData);
    QString formatErrorCodeStats(const QString& rawData);
    QString formatDatabaseInfo(const QString& rawData);
    QString formatGenericResult(const QString& rawResult);

    // 로그 조회 결과 포맷터
    QString formatLogQueryResult(const QString& rawResult);
    QString getTimeAgo(const QDateTime& dateTime);

    // MQTT 제어 결과 포맷터
    QString formatMqttControlResult(const QString& rawResult);

    // 에러 통계 전용 포맷터
    QString formatErrorStatistics(const QString& rawData);
    QString formatErrorStatisticsFromText(const QString& rawData);

    // 에러 코드 설명 헬퍼 함수 추가
    QString getErrorDescription(const QString& errorCode);
}

#endif // DATAFORMATTER_H