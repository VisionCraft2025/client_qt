#include "mcp_btn.h"
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>


MCPButton::MCPButton(QWidget* parent)
    : QWidget(parent)
{
    // 초기 크기 설정
    setFixedSize(56, 56);

    mainButton = new QPushButton(this);
    mainButton->setText("AI\n도우미");
    
    // Qt에서 텍스트 정렬을 위한 설정
    mainButton->setStyleSheet(R"(
        QPushButton {
            border-radius: 22px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f97316, stop:1 #ea580c);
            color: white;
            font-weight: bold;
            font-size: 9px;
            font-family: "Hanwha Gothic", "Malgun Gothic", "나눔스퀘어", sans-serif;
            border: none;
            text-align: center;
            padding-top: -2px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fb923c, stop:1 #f97316);
        }
    )");

    connect(mainButton, &QPushButton::clicked, this, &MCPButton::clicked);

    // 그림자 효과
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(this);
    effect->setOffset(0, 4);
    effect->setBlurRadius(12);
    effect->setColor(QColor(0, 0, 0, 100));
    mainButton->setGraphicsEffect(effect);

    // 뱃지 도트
    badgeDot = new QLabel(this);
    badgeDot->setFixedSize(8, 8);  // 10 -> 8
    badgeDot->setStyleSheet("background-color: red; border-radius: 4px;");

    // 펄스 애니메이션 효과
    QGraphicsOpacityEffect* ringEffect = new QGraphicsOpacityEffect(this);
    mainButton->setGraphicsEffect(ringEffect);

    QPropertyAnimation* pulseAnim = new QPropertyAnimation(ringEffect, "opacity", this);
    pulseAnim->setDuration(2000);
    pulseAnim->setStartValue(0.5);
    pulseAnim->setKeyValueAt(0.5, 1.0);
    pulseAnim->setEndValue(0.5);
    pulseAnim->setLoopCount(-1);
    pulseAnim->start();

    // 초기 크기 설정
    updateButtonSize();
}

void MCPButton::updateButtonSize() {
    if (!parentWidget()) return;
    
    int parentWidth = parentWidget()->width();
    if (parentWidth <= 0) parentWidth = 400;
    
    int sideMargin = parentWidth * 0.05;
    int buttonSpacing = parentWidth * 0.02;
    
    int availableWidth = parentWidth - (sideMargin * 2) - (buttonSpacing * 3);
    int buttonSize = availableWidth / 4;
    
    // 최소/최대 크기 제한 - 30% 줄임
    buttonSize = qBound(35, buttonSize, 84);  // 50->35, 120->84
    
    setFixedSize(buttonSize, buttonSize);
    
    // 메인 버튼 크기 조정
    int mainButtonSize = buttonSize * 0.8;
    int buttonMargin = (buttonSize - mainButtonSize) / 2;
    mainButton->setFixedSize(mainButtonSize, mainButtonSize);
    mainButton->move(buttonMargin, buttonMargin);
    
    int borderRadius = mainButtonSize / 2;
    
    // 폰트 크기도 30% 줄임
    int fontSize = buttonSize / 8;  // 6 -> 8로 나누어 더 작게
    
    mainButton->setStyleSheet(QString(R"(
        QPushButton {
            border-radius: %1px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f97316, stop:1 #ea580c);
            color: white;
            font-weight: bold;
            font-size: %2px;
            font-family: "Malgun Gothic", "나눔스퀘어", sans-serif;
            border: none;
            text-align: center;
            padding-top: -2px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fb923c, stop:1 #f97316);
        }
    )").arg(borderRadius).arg(fontSize));
    
    // 뱃지 도트 위치 조정
    int badgeSize = qMax(6, buttonSize / 10);  // 8 -> 10으로 나누어 더 작게
    badgeDot->setFixedSize(badgeSize, badgeSize);
    badgeDot->move(buttonSize - badgeSize - 2, 2);
    badgeDot->setStyleSheet(QString("background-color: red; border-radius: %1px;").arg(badgeSize / 2));
}

QSize MCPButton::sizeHint() const {
    // 동적으로 계산된 크기 반환
    return size();
}

void MCPButton::mousePressEvent(QMouseEvent* event) {
    emit clicked();
    QWidget::mousePressEvent(event);
}

void MCPButton::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateButtonSize();  // 위젯이 표시될 때 크기 업데이트
}

void MCPButton::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (parentWidget()) {
        updateButtonSize();  // 부모 크기 변경 시 버튼 크기도 업데이트
    }
}
