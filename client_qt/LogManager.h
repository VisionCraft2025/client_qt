#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QJsonObject>
#include <QList>

class LogManager {
public:
    static QList<QJsonObject> errorLogs;

    static void addLog(const QJsonObject &log) {
        errorLogs.prepend(log);  // 맨 앞에 추가 (최신 로그가 위로)
        if(errorLogs.size() > 100) {
            errorLogs.removeLast();  // 최대 100개까지만 보관
        }
    }

    static QList<QJsonObject> getAllLogs() {
        return errorLogs;
    }

    static QList<QJsonObject> getLogsForDevice(const QString &deviceId) {
        QList<QJsonObject> filteredLogs;
        for(const QJsonObject &log : errorLogs) {
            if(log["device_id"].toString() == deviceId) {
                filteredLogs.append(log);
            }
        }
        return filteredLogs;
    }
};

#endif // LOGMANAGER_H
