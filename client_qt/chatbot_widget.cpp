#include "chatbot_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QScrollBar>

ChatBotWidget::ChatBotWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(360, 500);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setStyleSheet("background-color: white; border-radius: 12px;");

    // 전체 레이아웃
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout;
    QLabel* title = new QLabel("🤖 VisionCraft AI");
    title->setStyleSheet("font-weight: bold; font-size: 16px;");
    closeButton = new QPushButton("X");
    closeButton->setFixedSize(24, 24);
    closeButton->setStyleSheet("border: none; color: gray;");
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);
    mainLayout->addLayout(headerLayout);

    // 메시지 영역
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("border: none;");
    messageContainer = new QWidget;
    messageLayout = new QVBoxLayout(messageContainer);
    messageLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(messageContainer);
    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout* quickLayout = new QHBoxLayout;
    QStringList quickTexts = { "설비 상태 확인", "고장률 분석", "한화비전 문의" };
    for (const QString& text : quickTexts) {
        QPushButton* quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 11px; padding: 6px; background-color: #f3f4f6; border-radius: 6px;");
        connect(quick, &QPushButton::clicked, this, [=]() {
            input->setText(text);
            handleSend();
        });
        quickLayout->addWidget(quick);
    }
    mainLayout->addLayout(quickLayout);

    // 입력창 + 전송
    QHBoxLayout* inputLayout = new QHBoxLayout;
    input = new QLineEdit(this);
    input->setPlaceholderText("AI에게 질문해보세요...");
    sendButton = new QPushButton("전송");
    sendButton->setStyleSheet("background-color: #fb923c; color: white; padding: 6px 12px; border-radius: 6px;");
    connect(sendButton, &QPushButton::clicked, this, &ChatBotWidget::handleSend);
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendButton);
    mainLayout->addLayout(inputLayout);

    // 초기 메시지
    ChatMessage welcome = {
        "bot",
        "안녕하세요. \n스마트팩토리 AI 어시스턴트입니다.\n\n어떤 도움이 필요하신가요?",
        getCurrentTime()
    };
    addMessage(welcome);
}

void ChatBotWidget::addMessage(const ChatMessage& msg) {
    QLabel* msgLabel = new QLabel;
    msgLabel->setWordWrap(true);
    msgLabel->setText(msg.content + "\n<small>" + msg.time + "</small>");

    if (msg.sender == "bot") {
        msgLabel->setStyleSheet("background-color: #f3f4f6; padding: 8px; border-radius: 8px;");
        messageLayout->addWidget(msgLabel, 0, Qt::AlignLeft);
    } else {
        msgLabel->setStyleSheet("background-color: #fb923c; color: white; padding: 8px; border-radius: 8px;");
        messageLayout->addWidget(msgLabel, 0, Qt::AlignRight);
    }

    // 자동 스크롤
    QTimer::singleShot(0, this, [=]() {
        scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatBotWidget::handleSend() {
    QString text = input->text().trimmed();
    if (text.isEmpty()) return;

    ChatMessage userMsg = { "user", text, getCurrentTime() };
    addMessage(userMsg);
    input->clear();

    // 응답 시뮬레이션
    QTimer::singleShot(700, this, [=]() {
        ChatMessage botMsg = {
            "bot",
            "문의 내용을 분석하고 있습니다.\n관련 설비 데이터를 확인 중입니다.",
            getCurrentTime()
        };
        addMessage(botMsg);
    });
}

QString ChatBotWidget::getCurrentTime() {
    return QTime::currentTime().toString("hh:mm AP");
}
