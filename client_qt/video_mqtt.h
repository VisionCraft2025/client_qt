#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include <functional>

struct VideoInfo {
    QString video_id;
    QString error_log_id;
    QString device_id;
    QString http_url;
    QString file_path;
    int video_duration;
    qint64 file_size;
    qint64 video_created_time;
    QString video_quality;
};

using VideoQueryCallback = std::function<void(const QList<VideoInfo>&)>;

class MqttClient : public QObject {
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    void connectToHost();
    void queryVideos(const QString& device_id = "",
                     const QString& error_log_id = "",
                     qint64 start_time = 0,
                     qint64 end_time = 0,
                     int limit = 50,
                     VideoQueryCallback callback = nullptr);

private slots:
    void onConnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

private:
    QMqttClient* m_client;
    QMap<QString, VideoQueryCallback> m_pendingQueries;
    QTimer* m_timeoutTimer;
};
