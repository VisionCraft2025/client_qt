#include "chatbot_widget.h"
#include "ai_command.h"
#include "factory_mcp.h"

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
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

ChatBotWidget::ChatBotWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(360, 500);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // 가장 바깥 배경 프레임 (주황색 배경, 둥근 창)
    QFrame* outerFrame = new QFrame(this);
    outerFrame->setObjectName("outerFrame");
    outerFrame->setStyleSheet(R"(
            QFrame#outerFrame {
                background-color: #f97316;
                border-radius: 16px;
            }
        )");
    //outerFrame->setGeometry(0, 0, width(), height());
    outerFrame->setFixedSize(360, 500);


    // 내부 흰색 영역 프레임
    QFrame* innerFrame = new QFrame(outerFrame);
    innerFrame->setObjectName("innerFrame");
    innerFrame->setStyleSheet(R"(
            QFrame#innerFrame {
                background-color: white;
                border-bottom-left-radius: 16px;
                border-bottom-right-radius: 16px;
            }
        )");
    //innerFrame->setGeometry(0, 50, width(), height() - 50);
    innerFrame->setMinimumSize(360, 440);  // 헤더 제외한 높이 정도



    // 헤더
    QWidget* header = new QWidget(outerFrame);
    header->setMinimumHeight(60);  //헤더 높이 보장
    //header->setGeometry(0, 0, width(), 50);
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);


    QVBoxLayout* outerLayout = new QVBoxLayout(outerFrame); // 추가
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);      // 헤더 먼저
    outerLayout->addWidget(innerFrame);  // 그 아래 흰 배경


    QLabel* title = new QLabel("<b>🤖 VisionCraft AI</b>");
    title->setStyleSheet("color: white; font-size: 15px;");
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);  // 여기!! 줄바꿈 방지
    title->setTextInteractionFlags(Qt::NoTextInteraction);  // 드래그 방지

    QLabel* subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setStyleSheet("color: white; font-size: 10px;");

    QVBoxLayout* titleBox = new QVBoxLayout;
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    // 높이 고정
    titleBox->setSpacing(0);
    titleBox->setContentsMargins(0, 0, 0, 0);

    closeButton = new QPushButton("\u2715");
    closeButton->setStyleSheet("background: transparent; color: white; border: none; font-size: 14px;");
    closeButton->setFixedSize(24, 24);
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);

    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    // 내부 레이아웃
    QVBoxLayout* mainLayout = new QVBoxLayout(innerFrame);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 메시지 스크롤 영역
    scrollArea = new QScrollArea(innerFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
                background-color: transparent;  //
            }
        )");

    messageContainer = new QWidget;
    messageContainer->setStyleSheet("background-color: white;");
    messageLayout = new QVBoxLayout(messageContainer);
    messageLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(messageContainer);

    //scrollArea->setMinimumWidth(innerFrame->width());
    //messageContainer->setMinimumWidth(innerFrame->width() - 20);

    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setMinimumWidth(scrollArea->width() - 20);



    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout* quickLayout = new QHBoxLayout;
    QStringList quickTexts = { "설비 상태 확인", "고장률 분석", "서버연결 확인" };
    for (const QString& text : quickTexts) {
        QPushButton* quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 11px; padding: 6px; background-color: #f3f4f6; border-radius: 6px;");
        connect(quick, &QPushButton::clicked, this, &ChatBotWidget::handleQuickReplyClicked);
        quickLayout->addWidget(quick);
    }
    mainLayout->addLayout(quickLayout);

    // 입력창 + 전송
    QHBoxLayout* inputLayout = new QHBoxLayout;

    input = new QLineEdit(this);
    input->setPlaceholderText("AI에게 질문해보세요...");
    input->setStyleSheet(R"(
            background-color: #f3f4f6;
            border: none;
            border-radius: 10px;
            padding: 8px;
            font-size: 12px;
        )");

    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(48, 32);
    sendButton->setStyleSheet(R"(
            background-color: #fb923c;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 12px;
        )");


    connect(sendButton, &QPushButton::clicked, this, &ChatBotWidget::handleSend);
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendButton);
    mainLayout->addLayout(inputLayout);

    // 초기 메시지
    ChatMessage welcome = {
        "bot",
        "안녕하세요.\n스마트팩토리 AI 어시스턴트입니다.\n\n어떤 도움이 필요하신가요?",
        getCurrentTime()
    };
    addMessage(welcome);
}

void ChatBotWidget::setGemini(GeminiRequester* requester) {
    this->gemini = requester;
}

void ChatBotWidget::addMessage(const ChatMessage& msg) {
    QLabel* msgLabel = new QLabel(msg.content);
    msgLabel->setWordWrap(true);
    // msgLabel->setMaximumWidth(260); // 최대 너비 제한 제거
    msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    msgLabel->adjustSize();
    //msgLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* timeLabel = new QLabel(msg.time);
    timeLabel->setStyleSheet("font-size: 10px; color: gray;");

    QVBoxLayout* bubbleLayout = new QVBoxLayout;
    bubbleLayout->setSpacing(4);
    bubbleLayout->setSizeConstraint(QLayout::SetMinimumSize);
    bubbleLayout->setAlignment(msg.sender == "bot" ? Qt::AlignLeft : Qt::AlignRight);

    if (msg.sender == "bot") {
        msgLabel->setStyleSheet("background-color: #f3f4f6; padding: 8px; border-radius: 8px;");
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignLeft);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignLeft);
    } else {
        msgLabel->setStyleSheet("background-color: #fb923c; color: white; padding: 8px; border-radius: 8px;");
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignRight);
        timeLabel->setAlignment(msg.sender == "bot" ? Qt::AlignLeft : Qt::AlignRight);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);
    }

    QWidget* bubble = new QWidget;
    bubble->setLayout(bubbleLayout);
    bubble->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    bubble->adjustSize();
    messageLayout->addWidget(bubble);

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

    // Gemini 호출 (프롬프트에 JSON만 반환하도록 유도중인데 이거 나중에 바꿀거임)
    if (gemini) {
        // 이거 병수님이랑 JSON 형식 맞추기
        QString prompt = "아래 명령을 JSON 형식으로만 반환해줘. 예시: {\"command\": \"STOP_CONVEYOR\", \"target\": \"conveyor_01\"}\n명령: " + text;
        gemini->askGemini(this, prompt, [=](const QString& response) {
            // Gemini 응답을 JSON으로 파싱 시도
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject json = doc.object();
                if (mcp) {
                    mcp->sendCommand(json);
                    ChatMessage botMsg = { "bot", "명령을 MCP 서버로 전송했습니다.", getCurrentTime() };
                    addMessage(botMsg);
                } else {
                    ChatMessage botMsg = { "bot", "MCP 서버와 연결되어 있지 않습니다.", getCurrentTime() };
                    addMessage(botMsg);
                }
            } else {
                // JSON 파싱 실패: 자연어 답변만 챗봇에 표시
                ChatMessage botMsg = { "bot", response, getCurrentTime() };
                addMessage(botMsg);
            }
        });
    } else {
        ChatMessage botMsg = { "bot", "Gemini가 연결되지 않았습니다.", getCurrentTime() };
        addMessage(botMsg);
    }
}

void ChatBotWidget::handleQuickReplyClicked() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString text = btn->text();

    // 1. 사용자 메시지 먼저 추가
    addMessage({"user", text, getCurrentTime()});

    if (text == "서버연결 확인") {
        if (mcp) {
            bool connected = mcp->isConnected();
            QString msg = connected ? "MCP 서버에 연결되어 있습니다." : "MCP 서버에 연결되어 있지 않습니다.";
            addMessage({"bot", msg, getCurrentTime()});
        } else {
            addMessage({"bot", "MCP 핸들러가 없습니다.", getCurrentTime()});
        }
        return;
    }
    // 나머지 퀵버튼은 기존대로 입력창에 텍스트 넣고 전송
    input->setText(text);
    handleSend();
}

QString ChatBotWidget::getCurrentTime() {
    return QTime::currentTime().toString("hh:mm AP");
}

void ChatBotWidget::setMcpHandler(FactoryMCP* mcpHandler) {
    mcp = mcpHandler;
    if (mcp) {
        connect(mcp, &FactoryMCP::commandResponseReceived, this, &ChatBotWidget::onMcpResponse);
        connect(mcp, &FactoryMCP::errorOccurred, this, &ChatBotWidget::onMcpError);
    }
}

// MCP 명령 전송 예시 (Gemini 결과를 받아서 MCP로 전송)
void ChatBotWidget::handleGeminiResult(const QJsonObject& json) {
    if (mcp) {
        mcp->sendCommand(json);
        // 사용자 메시지로도 추가 가능: "명령 전송 중..."
        ChatMessage msg = { "bot", "명령을 MCP 서버로 전송했습니다.", getCurrentTime() };
        addMessage(msg);
    } else {
        ChatMessage msg = { "bot", "MCP 서버와 연결되어 있지 않습니다.", getCurrentTime() };
        addMessage(msg);
    }
}

// MCP 응답 처리
void ChatBotWidget::onMcpResponse(const QByteArray& response) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(response, &err);
    QString msg;
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        QString result = obj.value("result").toString();
        QString action = obj.value("action").toString();
        QString target = obj.value("target").toString();
        if (result == "OK") {
            msg = QString("[%1] 명령이 성공적으로 처리되었습니다. (대상: %2)").arg(action, target);
        } else if (!result.isEmpty()) {
            QString reason = obj.value("reason").toString();
            msg = QString("[%1] 명령이 실패했습니다. %2").arg(action, reason);
        } else {
            msg = QString::fromUtf8(response);
        }
    } else {
        msg = "MCP 응답: " + QString::fromUtf8(response);
    }
    addMessage({"bot", msg, getCurrentTime()});
}

// MCP 에러 처리
void ChatBotWidget::onMcpError(const QString& reason) {
    ChatMessage msg = { "bot", "MCP 에러: " + reason, getCurrentTime() };
    addMessage(msg);
}


