#include "streamer.h"
#include <QDebug>
#include <QElapsedTimer> //지연시간 측정


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

    // 내부 버퍼 최소화 시도
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    cap.open(streamUrl.toStdString());

    if (!cap.isOpened()) {
        qDebug() << "[Streamer] Failed to open stream:" << streamUrl;
        return;
    } else {
        qDebug() << "[Streamer] Stream opened successfully:" << streamUrl;
    }

    {
        QMutexLocker locker(&mutex);
        running = true;
    }

    // FPS, 지연시간 측정용 타이머
    QElapsedTimer fpsTimer;
    fpsTimer.start();
    int frameCount = 0;

    QElapsedTimer latencyTimer;

    while (true) {
        {
            QMutexLocker locker(&mutex);
            if (!running)
                break;
        }

        latencyTimer.restart();

        // 최신 프레임 유지 (grab → retrieve)
        if (!cap.grab()) continue;

        cv::Mat frame;
        if (!cap.retrieve(frame) || frame.empty()) continue;

        // OpenCV → QImage
        QImage image = cvMatToQImage(frame);
        emit newFrame(image);

        // FPS 계산
        frameCount++;
        if (fpsTimer.elapsed() >= 1000) {
            qDebug() << "[FPS]" << frameCount << "fps";
            fpsTimer.restart();
            frameCount = 0;
        }

        // 지연시간 출력
        qDebug() << "[Latency]" << latencyTimer.elapsed() << "ms";

        // 부하 조절: 약간의 sleep으로 CPU 소모 완화
        QThread::msleep(10); // 너무 짧거나 길게 잡지 않기
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
