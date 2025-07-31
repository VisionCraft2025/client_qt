#include "video_mqtt.h"
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QtMqtt/QMqttTopicFilter>
#include <QtMqtt/QMqttTopicName>

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_client->setHostname("mqtt.kwon.pics");
    m_client->setPort(1883);

    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);

    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(10000); // 10초 타임아웃
}

MqttClient::~MqttClient() {
    if (m_client->state() == QMqttClient::Connected) {
        m_client->disconnectFromHost();
    }
}

void MqttClient::connectToHost() {
    if (m_client->state() != QMqttClient::Connected) {
        m_client->connectToHost();
    }
}

void MqttClient::onConnected() {
    qDebug() << "MQTT Connected";
    m_client->subscribe(QMqttTopicFilter("factory/query/videos/response"), 1);
}

void MqttClient::queryVideos(const QString& device_id,
                             const QString& error_log_id,
                             qint64 start_time,
                             qint64 end_time,
                             int limit,
                             VideoQueryCallback callback) {

    if (m_client->state() != QMqttClient::Connected) {
        connectToHost();
        QTimer::singleShot(2000, [=]() {
            queryVideos(device_id, error_log_id, start_time, end_time, limit, callback);
        });
        return;
    }

    QString query_id = QString("video_query_%1").arg(QDateTime::currentMSecsSinceEpoch());

    QJsonObject query;
    query["query_id"] = query_id;
    query["query_type"] = "videos";

    QJsonObject filters;
    if (!device_id.isEmpty()) filters["device_id"] = device_id;
    if (!error_log_id.isEmpty()) filters["error_log_id"] = error_log_id;
    if (start_time > 0 && end_time > 0) {
        QJsonObject time_range;
        time_range["start"] = start_time;
        time_range["end"] = end_time;
        filters["time_range"] = time_range;
    }
    filters["limit"] = limit;
    query["filters"] = filters;

    if (callback) {
        m_pendingQueries[query_id] = callback;
    }

    QJsonDocument doc(query);
    m_client->publish(QMqttTopicName("factory/query/videos/request"), doc.toJson(), 1);

    qDebug() << "Published query:" << query_id;
}

void MqttClient::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic) {
    if (topic.name() != "factory/query/videos/response") return;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return;
    }

    QJsonObject response = doc.object();
    QString query_id = response["query_id"].toString();
    QString status = response["status"].toString();

    if (!m_pendingQueries.contains(query_id)) return;

    VideoQueryCallback callback = m_pendingQueries.take(query_id);

    if (status != "success") {
        qWarning() << "Query failed:" << response["error"].toString();
        callback(QList<VideoInfo>());
        return;
    }

    QList<VideoInfo> videos;
    QJsonArray data = response["data"].toArray();

    for (const auto& item : data) {
        QJsonObject obj = item.toObject();
        VideoInfo video;
        video.video_id = obj["_id"].toString();
        video.error_log_id = obj["error_log_id"].toString();
        video.device_id = obj["device_id"].toString();
        video.http_url = obj["http_url"].toString();
        video.file_path = obj["file_path"].toString();
        video.video_duration = obj["video_duration"].toInt();
        video.file_size = obj["file_size"].toVariant().toLongLong();
        video.video_created_time = obj["video_created_time"].toVariant().toLongLong();
        video.video_quality = obj["video_quality"].toString();
        videos.append(video);
    }

    qDebug() << "Received" << videos.size() << "videos for query" << query_id;
    callback(videos);
}
