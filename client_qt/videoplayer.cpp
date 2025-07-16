#include "videoplayer.h"
#include <QUrl>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>

VideoPlayer::VideoPlayer(const QString& videoPath, QWidget *parent)
    : QWidget(parent)
    , m_videoPath(videoPath)
    , m_mediaPlayer(new QMediaPlayer(this))
{
    // 비디오 파일 존재 확인
    QFileInfo fileInfo(videoPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::critical(this, "File Error",
                              QString("비디오 파일을 찾을 수 없습니다: %1").arg(videoPath));
        close();
        return;
    }

    setupUI();
    setupConnections();

    // 창 제목에 파일명 표시
    setWindowTitle(QString("Video Player - %1").arg(fileInfo.fileName()));
    resize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

    // 비디오 로드 및 재생
    loadAndPlayVideo();
}

VideoPlayer::~VideoPlayer() = default;

void VideoPlayer::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // 비디오 위젯 설정
    m_videoWidget = new QVideoWidget;
    m_videoWidget->setMinimumSize(MIN_VIDEO_WIDTH, MIN_VIDEO_HEIGHT);
    m_videoWidget->setStyleSheet("QVideoWidget { background-color: black; }");
    m_mainLayout->addWidget(m_videoWidget);

    // 컨트롤 패널 설정
    m_controlsLayout = new QHBoxLayout;
    m_controlsLayout->setSpacing(10);

    // 재생/일시정지 버튼
    m_playPauseBtn = new QPushButton("⏸");
    m_playPauseBtn->setFixedSize(CONTROL_BUTTON_WIDTH, CONTROL_BUTTON_HEIGHT);
    m_playPauseBtn->setToolTip("재생/일시정지");
    m_playPauseBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #e0e0e0; }");

    // 재생 위치 슬라이더
    m_positionSlider = new QSlider(Qt::Horizontal);
    m_positionSlider->setMinimum(0);
    m_positionSlider->setToolTip("재생 위치 조절");
    m_positionSlider->setStyleSheet(
        "QSlider::groove:horizontal { border: 1px solid #999; height: 8px; background: #ddd; }"
        "QSlider::handle:horizontal { background: #007acc; border: 1px solid #5c5c5c; width: 18px; margin: -2px 0; border-radius: 3px; }");

    // 시간 표시 레이블
    m_timeLabel = new QLabel("00:00 / 00:00");
    m_timeLabel->setMinimumWidth(TIME_LABEL_MIN_WIDTH);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet("QLabel { font-family: monospace; font-size: 12px; color: #333; }");

    // 레이아웃 구성
    m_controlsLayout->addWidget(m_playPauseBtn);
    m_controlsLayout->addWidget(m_positionSlider, 1); // 슬라이더가 대부분의 공간 차지
    m_controlsLayout->addWidget(m_timeLabel);

    m_mainLayout->addLayout(m_controlsLayout);
}

void VideoPlayer::setupConnections() {
    // UI 컨트롤 연결
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPlayer::onPlayPauseClicked);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &VideoPlayer::onSliderMoved);

    // 미디어 플레이어 연결
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoPlayer::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoPlayer::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayer::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred, this, &VideoPlayer::onErrorOccurred);
}

void VideoPlayer::loadAndPlayVideo() {
    QUrl videoUrl = QUrl::fromLocalFile(m_videoPath);
    m_mediaPlayer->setSource(videoUrl);
    m_mediaPlayer->setVideoOutput(m_videoWidget);

    // 자동 재생 시작
    m_mediaPlayer->play();
}

QString VideoPlayer::formatTime(qint64 timeMs) const {
    if (timeMs < 0) return "00:00";

    int totalSeconds = timeMs / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void VideoPlayer::onPlayPauseClicked() {
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
        m_playPauseBtn->setText("▶");
    } else {
        m_mediaPlayer->play();
        m_playPauseBtn->setText("⏸");
    }
}

void VideoPlayer::onPositionChanged(qint64 position) {
    // 사용자가 슬라이더를 드래그하고 있을 때는 업데이트하지 않음
    if (!m_positionSlider->isSliderDown()) {
        m_positionSlider->setValue(static_cast<int>(position));
    }

    // 시간 표시 업데이트
    qint64 duration = m_mediaPlayer->duration();
    QString timeText = QString("%1 / %2")
                           .arg(formatTime(position))
                           .arg(formatTime(duration));

    m_timeLabel->setText(timeText);
}

void VideoPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    switch (status) {
    case QMediaPlayer::LoadedMedia:
        // 미디어 로드 완료
        break;
    case QMediaPlayer::InvalidMedia:
        QMessageBox::warning(this, "Media Error", "지원되지 않는 비디오 형식입니다.");
        break;
    case QMediaPlayer::EndOfMedia:
        // 재생 완료 시 재생 버튼 상태 리셋
        m_playPauseBtn->setText("▶");
        break;
    default:
        break;
    }
}

void VideoPlayer::onErrorOccurred(QMediaPlayer::Error error, const QString& errorString) {
    QString errorMsg;
    switch (error) {
    case QMediaPlayer::ResourceError:
        errorMsg = "비디오 리소스 오류: " + errorString;
        break;
    case QMediaPlayer::FormatError:
        errorMsg = "비디오 형식 오류: " + errorString;
        break;
    case QMediaPlayer::NetworkError:
        errorMsg = "네트워크 오류: " + errorString;
        break;
    case QMediaPlayer::AccessDeniedError:
        errorMsg = "접근 거부 오류: " + errorString;
        break;
    default:
        errorMsg = "알 수 없는 오류: " + errorString;
        break;
    }

    QMessageBox::critical(this, "Video Player Error", errorMsg);
}

void VideoPlayer::onDurationChanged(qint64 duration) {
    m_positionSlider->setMaximum(duration);
}

void VideoPlayer::onSliderMoved(int position) {
    m_mediaPlayer->setPosition(position);
}
