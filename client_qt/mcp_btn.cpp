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
    // 초기 크기 설정 (나중에 updateButtonSize()에서 동적으로 조정됨)
    setFixedSize(80, 80);

    mainButton = new QPushButton(this);
    // 버튼 크기는 updateButtonSize()에서 설정
    mainButton->setStyleSheet(R"(
        QPushButton {
            border-radius: 32px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f97316, stop:1 #ea580c);
            color: white;
            font-weight: bold;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fb923c, stop:1 #f97316);
        }
    )");

    mainButton->setText("AI\n도우미");
    
    // Qt에서 텍스트 정렬을 위한 설정
    mainButton->setStyleSheet(R"(
        QPushButton {
            border-radius: 32px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f97316, stop:1 #ea580c);
            color: white;
            font-weight: bold;
            font-size: 12px;
            font-family: "Malgun Gothic", "나눔스퀘어", sans-serif;
            border: none;
            text-align: center;
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
    badgeDot->setFixedSize(10, 10);
    badgeDot->setStyleSheet("background-color: red; border-radius: 5px;");

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
    
    // 부모 위젯의 너비를 기반으로 동적 계산
    int parentWidth = parentWidget()->width();
    if (parentWidth <= 0) parentWidth = 400; // 기본값
    
    // 좌우 여백 (전체 너비의 5%)과 버튼 간 간격 (전체 너비의 2%) 고려
    int sideMargin = parentWidth * 0.05;
    int buttonSpacing = parentWidth * 0.02;
    
    // 4개 버튼을 배치할 때의 개별 버튼 크기 계산
    // (전체너비 - 좌우여백*2 - 버튼간격*3) / 4
    int availableWidth = parentWidth - (sideMargin * 2) - (buttonSpacing * 3);
    int buttonSize = availableWidth / 4;
    
    // 최소/최대 크기 제한
    buttonSize = qBound(50, buttonSize, 120);
    
    // 위젯 전체 크기 설정
    setFixedSize(buttonSize, buttonSize);
    
    // 메인 버튼 크기 및 위치 (위젯 크기의 80%, 중앙 정렬)
    int mainButtonSize = buttonSize * 0.8;
    int buttonMargin = (buttonSize - mainButtonSize) / 2;
    mainButton->setFixedSize(mainButtonSize, mainButtonSize);
    mainButton->move(buttonMargin, buttonMargin);
    
    // 버튼의 border-radius를 크기에 맞춰 조정
    int borderRadius = mainButtonSize / 2;
    
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
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fb923c, stop:1 #f97316);
        }
    )").arg(borderRadius).arg(buttonSize / 6));
    
    // 뱃지 도트 위치 조정 (버튼 우상단)
    int badgeSize = qMax(8, buttonSize / 8);
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
