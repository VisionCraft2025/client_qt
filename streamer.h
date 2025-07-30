#ifndef STREAMER_H
#define STREAMER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <opencv2/opencv.hpp>

class Streamer : public QThread
{
    Q_OBJECT

public:
    explicit Streamer(const QString& url, QObject* parent = nullptr);
    ~Streamer();

    void stop();
    bool isOpened() const;

signals:
    void newFrame(const QImage& image);

protected:
    void run() override;

private:
    QString streamUrl;
    cv::VideoCapture cap;
    mutable QMutex mutex;
    bool running = false;

    QImage cvMatToQImage(const cv::Mat& mat);
};

#endif // STREAMER_H
