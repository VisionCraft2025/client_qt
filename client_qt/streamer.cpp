#include "streamer.h"
#include <QDebug>


// 생성자, 스트림 URL을 받아 초기화
Streamer::Streamer(const QString& url, QObject* parent)
    : QThread(parent), streamUrl(url)
{
}

// 소멸자
Streamer::~Streamer()
{
    stop();
    wait(); // 스레드 안전 종료
}

// 스트리머 멈춤
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

//재생
void Streamer::run()
{
    qDebug() << "[Streamer] Trying to open stream:" << streamUrl;
    cap.open(streamUrl.toStdString()); //rtsp 스트림 열기

    if (!cap.isOpened()){
        qDebug() << "[Streamer] Failed to open stream:" << streamUrl;
        return;
    }else {
        qDebug() << "[Streamer] Stream opened successfully:" << streamUrl;
    }

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

// OpenCV Mat을 QImage로 변환
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
