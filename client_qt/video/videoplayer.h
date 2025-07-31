#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileInfo>
#include <QCloseEvent>

/**
 * @brief 독립적인 비디오 재생 창
 *
 * 로컬 비디오 파일을 재생하는 별도의 창입니다.
 * 기본적인 재생 컨트롤(재생/일시정지, 시간 슬라이더)을 제공합니다.
 */
class VideoPlayer : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayer(const QString& videoPath, const QString& deviceId, QWidget *parent = nullptr);
    ~VideoPlayer();

signals:
    void videoPlayerClosed();

private slots:
    /// 재생/일시정지 버튼 클릭 처리
    void onPlayPauseClicked();
    /// 비디오 재생 위치 변경 처리
    void onPositionChanged(qint64 position);
    /// 비디오 전체 길이 변경 처리
    void onDurationChanged(qint64 duration);
    /// 사용자가 슬라이더를 이동했을 때 처리
    void onSliderMoved(int position);
    /// 미디어 상태 변경 처리
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    /// 에러 발생 처리
    void onErrorOccurred(QMediaPlayer::Error error, const QString& errorString);

private:
    /// UI 컴포넌트 초기화
    void setupUI();
    /// 시그널-슬롯 연결 설정
    void setupConnections();
    /// 시간 형식 변환 (ms -> MM:SS)
    QString formatTime(qint64 timeMs) const;
    /// 비디오 로드 및 재생 시작
    void loadAndPlayVideo();

    // === UI 컴포넌트 ===
    QVBoxLayout* m_mainLayout;          ///< 메인 레이아웃
    QHBoxLayout* m_controlsLayout;      ///< 컨트롤 레이아웃

    QVideoWidget* m_videoWidget;        ///< 비디오 출력 위젯
    QMediaPlayer* m_mediaPlayer;        ///< Qt6 미디어 플레이어

    QPushButton* m_playPauseBtn;        ///< 재생/일시정지 버튼
    QSlider* m_positionSlider;          ///< 재생 위치 슬라이더
    QLabel* m_timeLabel;                ///< 시간 표시 레이블

    // === 데이터 ===
    QString m_videoPath;                ///< 비디오 파일 경로
    QString m_deviceId;                 ///< 비디오 재생 장치 ID

    // === 상수 ===
    static constexpr int DEFAULT_WINDOW_WIDTH = 800;
    static constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    static constexpr int MIN_VIDEO_WIDTH = 640;
    static constexpr int MIN_VIDEO_HEIGHT = 480;
    static constexpr int CONTROL_BUTTON_WIDTH = 40;
    static constexpr int CONTROL_BUTTON_HEIGHT = 30;
    static constexpr int TIME_LABEL_MIN_WIDTH = 80;

protected:
    void closeEvent(QCloseEvent* event) override;
};
