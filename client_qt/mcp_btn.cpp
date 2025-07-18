#include "mcp_btn.h"
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>


MCPButton::MCPButton(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(80, 80);  // 전체 위젯 크기

    mainButton = new QPushButton(this);
    mainButton->setFixedSize(64, 64);
    mainButton->move(8, 0);
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

    mainButton->setText("AI\n제어");  // ✅ 줄바꿈 포함 텍스트
    mainButton->setText("AI\n제어");

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
            padding-top: 8px;         /* 위에서 살짝 내려주고 */
            text-align: center;       /* ✅ 중앙 정렬 핵심 */
            qproperty-alignment: 'AlignCenter'; /* ✅ 이게 진짜 핵심!! */
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fb923c, stop:1 #f97316);
        }
    )");

    connect(mainButton, &QPushButton::clicked, this, &MCPButton::clicked);


    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(this);
    effect->setOffset(0, 4);
    effect->setBlurRadius(12);
    effect->setColor(QColor(0, 0, 0, 100));
    mainButton->setGraphicsEffect(effect);

    // textLabel = new QLabel("AI 제어", this);
    // textLabel->setStyleSheet(R"(
    //     color: white;
    //     font-size: 12px;
    //     font-weight: bold;
    //     font-family: "Malgun Gothic", "나눔스퀘어", sans-serif;
    //     background-color: transparent;
    // )");
    // textLabel->adjustSize();
    // textLabel->raise();  // 다른 위젯 위에 올라오게
    // textLabel->move(18, 64); // 버튼 아래 중앙쯤

    badgeDot = new QLabel(this);
    badgeDot->setFixedSize(10, 10);
    badgeDot->move(60, 4);
    badgeDot->setStyleSheet("background-color: red; border-radius: 5px;");

    QGraphicsOpacityEffect* ringEffect = new QGraphicsOpacityEffect(this);
    mainButton->setGraphicsEffect(ringEffect);

    QPropertyAnimation* pulseAnim = new QPropertyAnimation(ringEffect, "opacity", this);
    pulseAnim->setDuration(2000);
    pulseAnim->setStartValue(0.5);
    pulseAnim->setKeyValueAt(0.5, 1.0);
    pulseAnim->setEndValue(0.5);
    pulseAnim->setLoopCount(-1);  // 무한 반복
    pulseAnim->start();
}

QSize MCPButton::sizeHint() const {
    return QSize(80, 80);
}

void MCPButton::mousePressEvent(QMouseEvent* event) {
    emit clicked();
    QWidget::mousePressEvent(event);
}
