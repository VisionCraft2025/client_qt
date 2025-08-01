#include "error_message_card.h"
#include "../utils/font_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>
ErrorMessageCard::ErrorMessageCard(QWidget *parent) : QFrame(parent)
{
    setObjectName("errorMessageCard");
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    labelTitle = new QLabel("Error Message");
    labelTitle->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 12));
    labelError = new QLabel("감지된 오류:");
    labelTime = new QLabel("오류 발생 날짜 및 시간:");
    labelLocation = new QLabel("발생 위치:");
    labelCamera = new QLabel("대상 카메라:");
    labelErrorValue = new QLabel("-");
    labelTimeValue = new QLabel("-");
    labelLocationValue = new QLabel("-");
    labelCameraValue = new QLabel("-");
    labelError->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelTime->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelLocation->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelCamera->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelErrorValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelTimeValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelLocationValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelCameraValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    // 깜빡임 타이머 초기화
    blinkTimer = new QTimer(this);
    isBlinkVisible = true;
    isErrorMode = false;
    connect(blinkTimer, &QTimer::timeout, this, &ErrorMessageCard::onBlinkTimer);
    // 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(12);
    // 제목
    labelTitle->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(labelTitle);
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(60);  // 좌우 묶음 간격
    // 왼쪽 묶음
    QVBoxLayout *leftCol = new QVBoxLayout();
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->addWidget(labelError);
    row1->addWidget(labelErrorValue);
    row1->addStretch();
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->addWidget(labelLocation);
    row2->addWidget(labelLocationValue);
    row2->addStretch();
    leftCol->addLayout(row1);
    leftCol->addLayout(row2);
    leftCol->setSpacing(4);
    // 오른쪽 묶음
    QVBoxLayout *rightCol = new QVBoxLayout();
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->addWidget(labelTime);
    row3->addWidget(labelTimeValue);
    row3->addStretch();
    QHBoxLayout *row4 = new QHBoxLayout();
    row4->addWidget(labelCamera);
    row4->addWidget(labelCameraValue);
    row4->addStretch();
    rightCol->addLayout(row3);
    rightCol->addLayout(row4);
    rightCol->setSpacing(4);
    // 전체 수평 레이아웃에 좌우 묶음 삽입
    contentLayout->addLayout(leftCol);
    contentLayout->addLayout(rightCol);
    mainLayout->addLayout(contentLayout);
    setLayout(mainLayout);
    setNormalState();
}
void ErrorMessageCard::setErrorState(const QString &errorText, const QString &time,
                                     const QString &location, const QString &camera)
{
    labelErrorValue->setText(errorText);
    labelTimeValue->setText(time);
    labelLocationValue->setText(location);
    labelCameraValue->setText(camera);
    isErrorMode = true;
    isBlinkVisible = true;
    // 깜빡임 시작 (1초 간격)
    blinkTimer->start(1000);
    updateStyle(true, true);
}
void ErrorMessageCard::setNormalState()
{
    labelErrorValue->setText("오류 없음");
    labelTimeValue->setText("-");
    labelLocationValue->setText("-");
    labelCameraValue->setText("-");
    isErrorMode = false;
    // 깜빡임 중지
    blinkTimer->stop();
    updateStyle(false);
}
void ErrorMessageCard::onBlinkTimer()
{
    if (!isErrorMode) return;
    // 깜빡임 상태 토글
    isBlinkVisible = !isBlinkVisible;
    // 스타일 업데이트
    updateStyle(true, isBlinkVisible);
}
void ErrorMessageCard::updateStyle(bool isError, bool blinkState)
{
    QString bgColor, borderColor, titleColor;
    if (isError) {
        if (blinkState) {
            // 밝은 빨간색 (기존 에러 색상)
            bgColor = "#FFF1F2";
            borderColor = "#DC2626";
            titleColor = "#B91C1C";
        } else {
            // 어두운 빨간색 (깜빡임 시 더 진한 색)
            bgColor = "#FECACA";
            borderColor = "#991B1B";
            titleColor = "#7F1D1D";
        }
    } else {
        // 정상 상태 (기존 코드 유지)
        bgColor = "#FFFFFF";
        borderColor = "#F37121";  // 한화 오렌지
        titleColor = "#F97316";
    }
    setStyleSheet(QString(R"(
        QFrame#errorMessageCard {
            background-color: %1;
            border: 1px solid %2;
            border-left: 5px solid %2;
            border-radius: 12px;
        }
    )").arg(bgColor, borderColor));
    labelTitle->setStyleSheet(QString("color: %1;").arg(titleColor));
}
