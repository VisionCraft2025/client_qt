#include "streamer.h"

Streamer::Streamer(const QString& url, QObject* parent)
    : QThread(parent), streamUrl(url)
{
}

Streamer::~Streamer()
{
    stop();
    wait(); // 스레드 안전 종료
}

void Streamer::stop()
{
    QMutexLocker locker(&mutex);
    running = false;
}

bool Streamer::isOpened() const
{
    QMutexLocker locker(&mutex);
    return cap.isOpened();
}

void Streamer::run()
{
    cap.open(streamUrl.toStdString());
    if (!cap.isOpened())
        return;

    {
        QMutexLocker locker(&mutex);
        running = true;
    }

    while (true) {
        {
            QMutexLocker locker(&mutex);
            if (!running)
                break;
        }

        cv::Mat frame;
        if (cap.read(frame) && !frame.empty()) {
            QImage image = cvMatToQImage(frame);
            emit newFrame(image);
        }

        msleep(30); // 약 30FPS
    }

    cap.release();
}

QImage Streamer::cvMatToQImage(const cv::Mat& mat)
{
    if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    } else if (mat.channels() == 1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    } else {
        return QImage();
    }
}
