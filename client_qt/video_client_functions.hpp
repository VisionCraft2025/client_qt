#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <functional>
#include "video_mqtt.h"

using VideoDownloadCallback = std::function<void(bool success, const QString& local_path)>;

class VideoClient : public QObject {
    Q_OBJECT

private:
    QNetworkAccessManager* m_networkManager;
    QString m_tempDir;
    MqttClient* m_mqttClient;

public:
    VideoClient(QObject* parent = nullptr) : QObject(parent) {
        m_networkManager = new QNetworkAccessManager(this);
        m_mqttClient = new MqttClient(this);

        // 임시 디렉토리 설정
        m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/factory_videos";
        QDir().mkpath(m_tempDir);

        // MQTT 연결
        m_mqttClient->connectToHost();
    }

    // 1. 비디오 목록 조회 (MQTT 통신)
    void queryVideos(const QString& device_id = "",
                     const QString& error_log_id = "",
                     qint64 start_time = 0,
                     qint64 end_time = 0,
                     int limit = 50,
                     VideoQueryCallback callback = nullptr) {

        m_mqttClient->queryVideos(device_id, error_log_id, start_time, end_time, limit, callback);
    }

    // 2. 비디오 파일 다운로드
    void downloadVideo(const QString& http_url,
                       VideoDownloadCallback callback = nullptr,
                       QProgressBar* progressBar = nullptr,
                       QLabel* statusLabel = nullptr) {

        QNetworkRequest request(http_url);
        request.setRawHeader("User-Agent", "Factory Video Client");

        QNetworkReply* reply = m_networkManager->get(request);

        // 파일명 추출
        QString fileName = http_url.split('/').last();
        QString localPath = m_tempDir + "/" + fileName;

        QFile* file = new QFile(localPath);
        if (!file->open(QIODevice::WriteOnly)) {
            if (callback) callback(false, "");
            delete file;
            return;
        }

        // 상태 표시
        if (statusLabel) {
            statusLabel->setText(QString("Downloading: %1").arg(fileName));
        }

        // 진행률 업데이트
        connect(reply, &QNetworkReply::downloadProgress, [progressBar](qint64 received, qint64 total) {
            if (progressBar && total > 0) {
                progressBar->setMaximum(total);
                progressBar->setValue(received);
            }
        });

        // 데이터 수신
        connect(reply, &QNetworkReply::readyRead, [reply, file]() {
            file->write(reply->readAll());
        });

        // 완료 처리
        connect(reply, &QNetworkReply::finished, [reply, file, localPath, callback, statusLabel]() {
            file->close();
            delete file;

            bool success = (reply->error() == QNetworkReply::NoError);

            if (statusLabel) {
                statusLabel->setText(success ? "Download completed" : "Download failed");
            }

            if (callback) {
                callback(success, success ? localPath : "");
            }

            reply->deleteLater();
        });
    }

    // 3. 비디오 재생
    void playVideo(const QString& localPath,
                   QMediaPlayer* mediaPlayer,
                   QVideoWidget* videoWidget) {

        if (!QFile::exists(localPath)) {
            qWarning() << "Video file not found:" << localPath;
            return;
        }

        mediaPlayer->setVideoOutput(videoWidget);
        mediaPlayer->setSource(QUrl::fromLocalFile(localPath));
        mediaPlayer->play();
    }

    // 4. 캐시 관리
    void clearCache() {
        QDir cacheDir(m_tempDir);
        cacheDir.removeRecursively();
        QDir().mkpath(m_tempDir);
    }

    QString getCacheDir() const {
        return m_tempDir;
    }

    // 5. 파일 크기 포맷팅 유틸리티
    static QString formatFileSize(qint64 bytes) {
        if (bytes < 1024) return QString("%1 B").arg(bytes);
        if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }

    // 6. 시간 포맷팅 유틸리티
    static QString formatDuration(int seconds) {
        int minutes = seconds / 60;
        int secs = seconds % 60;
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    }
};

// 사용 예시 함수들
namespace VideoClientExample {

// 에러 비디오 목록을 QListWidget에 표시
inline void populateVideoList(QListWidget* listWidget, VideoClient* client) {
    client->queryVideos("", "", 0, 0, 100, [listWidget](const QList<VideoInfo>& videos) {
        listWidget->clear();
        for (const auto& video : videos) {
            QString itemText = QString("[%1] %2 - %3 (%4)")
            .arg(video.device_id)
                .arg(video.error_log_id)
                .arg(VideoClient::formatFileSize(video.file_size))
                .arg(VideoClient::formatDuration(video.video_duration));

            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, video.http_url);
            listWidget->addItem(item);
        }
    });
}

// 선택된 비디오 다운로드 및 재생
inline void playSelectedVideo(QListWidget* listWidget,
                              VideoClient* client,
                              QMediaPlayer* mediaPlayer,
                              QVideoWidget* videoWidget,
                              QProgressBar* progressBar,
                              QLabel* statusLabel) {

    QListWidgetItem* currentItem = listWidget->currentItem();
    if (!currentItem) return;

    QString http_url = currentItem->data(Qt::UserRole).toString();

    client->downloadVideo(http_url,
                          [client, mediaPlayer, videoWidget](bool success, const QString& localPath) {
                              if (success) {
                                  client->playVideo(localPath, mediaPlayer, videoWidget);
                              }
                          }, progressBar, statusLabel);
}
}
