#include "chatbot_widget.h"
#include "ai_command.h"
#include "MCPAgentClient.h"

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

ChatBotWidget::ChatBotWidget(QWidget *parent)
    : QWidget(parent), waitingForResponse(false)
{
    setFixedSize(360, 500);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // 가장 바깥 배경 프레임 (주황색 배경, 둥근 창)
    QFrame *outerFrame = new QFrame(this);
    outerFrame->setObjectName("outerFrame");
    outerFrame->setStyleSheet(R"(
            QFrame#outerFrame {
                background-color: #f97316;
                border-radius: 16px;
            }
        )");
    // outerFrame->setGeometry(0, 0, width(), height());
    outerFrame->setFixedSize(360, 500);

    // 내부 흰색 영역 프레임
    QFrame *innerFrame = new QFrame(outerFrame);
    innerFrame->setObjectName("innerFrame");
    innerFrame->setStyleSheet(R"(
            QFrame#innerFrame {
                background-color: white;
                border-bottom-left-radius: 16px;
                border-bottom-right-radius: 16px;
            }
        )");
    // innerFrame->setGeometry(0, 50, width(), height() - 50);
    innerFrame->setMinimumSize(360, 440); // 헤더 제외한 높이 정도

    // 헤더
    QWidget *header = new QWidget(outerFrame);
    header->setMinimumHeight(60); // 헤더 높이 보장
    // header->setGeometry(0, 0, width(), 50);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    QVBoxLayout *outerLayout = new QVBoxLayout(outerFrame); // 추가
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);     // 헤더 먼저
    outerLayout->addWidget(innerFrame); // 그 아래 흰 배경

    QLabel *title = new QLabel("<b>🤖 VisionCraft AI</b>");
    title->setStyleSheet("color: white; font-size: 15px;");
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);                             // 여기!! 줄바꿈 방지
    title->setTextInteractionFlags(Qt::NoTextInteraction); // 드래그 방지

    QLabel *subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setStyleSheet("color: white; font-size: 10px;");

    QVBoxLayout *titleBox = new QVBoxLayout;
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
    QVBoxLayout *mainLayout = new QVBoxLayout(innerFrame);
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

    // scrollArea->setMinimumWidth(innerFrame->width());  // 여기!!
    // messageContainer->setMinimumWidth(innerFrame->width() - 20); // 여기!!

    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setMinimumWidth(scrollArea->width() - 20);

    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout *quickLayout = new QHBoxLayout;
    QStringList quickTexts = {
        "컨베이어 켜줘",  // MCP 도구 실행
        "오늘 로그 조회", // DB 조회
        "전체 장비 상태"  // 통계 조회
    };
    for (const QString &text : quickTexts)
    {
        QPushButton *quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 11px; padding: 6px; background-color: #f3f4f6; border-radius: 6px;");
        connect(quick, &QPushButton::clicked, this, [=]()
                {
                input->setText(text);
                handleSend(); });
        quickLayout->addWidget(quick);
    }
    mainLayout->addLayout(quickLayout);

    // 입력창 + 전송
    QHBoxLayout *inputLayout = new QHBoxLayout;

    input = new QLineEdit(this);
    input->setPlaceholderText("AI에게 질문해보세요...");
    input->setStyleSheet(R"(
            background-color: #f3f4f6;
            border: none;
            border-radius: 10px;
            padding: 8px;
            font-size: 12px;
        )"); // //여기!!

    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(48, 32); // //여기!!
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
        "안녕하세요.\n스마트팩토리 AI 어시스턴트입니다.\n\n"
        "장비 제어, 로그 조회, 통계 분석 등을 도와드립니다.\n"
        "어떤 도움이 필요하신가요?",
        getCurrentTime()};
    addMessage(welcome);
}
void ChatBotWidget::setMcpServerUrl(const QString &url)
{

    mcpServerUrl = url;
}

void ChatBotWidget::setGemini(GeminiRequester *requester)
{
    this->gemini = requester;

    // MCP 클라이언트 초기화
    if (gemini && !gemini->getApiKey().isEmpty())  // apiKey 대신 getApiKey() 사용
    {
        mcpClient = std::make_unique<MCPAgentClient>(
            mcpServerUrl,
            gemini->getApiKey(),  // apiKey 대신 getApiKey() 사용
            this);

        // MCP 시그널 연결
        connect(mcpClient.get(), &MCPAgentClient::toolDiscoveryCompleted,
                this, &ChatBotWidget::onToolDiscoveryCompleted);
        connect(mcpClient.get(), &MCPAgentClient::toolExecutionCompleted,
                this, &ChatBotWidget::onToolExecutionCompleted);
        connect(mcpClient.get(), &MCPAgentClient::errorOccurred,
                this, &ChatBotWidget::onMcpError);

        // 도구 목록 미리 가져오기
        mcpClient->fetchTools(true);
    }
}

void ChatBotWidget::onToolDiscoveryCompleted(const ConversationContext &context)
{
    if (context.executionResult.has_value())
    {
        ChatMessage botMsg = {"bot", context.executionResult.value(), getCurrentTime()};
        addMessage(botMsg);
    }

    waitingForResponse = false;
    sendButton->setEnabled(true);
}

void ChatBotWidget::onToolExecutionCompleted(const QString &result)
{
    ChatMessage botMsg = {"bot", result, getCurrentTime()};
    addMessage(botMsg);

    waitingForResponse = false;
    sendButton->setEnabled(true);
}

void ChatBotWidget::onMcpError(const QString &error)
{
    ChatMessage errorMsg = {
        "bot",
        QString("⚠️ 오류가 발생했습니다: %1").arg(error),
        getCurrentTime()};
    addMessage(errorMsg);

    waitingForResponse = false;
    sendButton->setEnabled(true);
}

void ChatBotWidget::addMessage(const ChatMessage &msg)
{
    QLabel *msgLabel = new QLabel(msg.content);
    msgLabel->setWordWrap(true);
    msgLabel->setMaximumWidth(260);
    msgLabel->adjustSize();
    // msgLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel *timeLabel = new QLabel(msg.time);
    timeLabel->setStyleSheet("font-size: 10px; color: gray;");

    QVBoxLayout *bubbleLayout = new QVBoxLayout;
    bubbleLayout->setSpacing(4);
    bubbleLayout->setSizeConstraint(QLayout::SetMinimumSize);
    bubbleLayout->setAlignment(msg.sender == "bot" ? Qt::AlignLeft : Qt::AlignRight);

    if (msg.sender == "bot")
    {
        msgLabel->setStyleSheet("background-color: #f3f4f6; padding: 8px; border-radius: 8px;");
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignLeft);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignLeft);
    }
    else
    {
        msgLabel->setStyleSheet("background-color: #fb923c; color: white; padding: 8px; border-radius: 8px;");
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignRight);
        timeLabel->setAlignment(msg.sender == "bot" ? Qt::AlignLeft : Qt::AlignRight);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);
    }

    QWidget *bubble = new QWidget;
    bubble->setLayout(bubbleLayout);
    bubble->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    bubble->adjustSize();
    messageLayout->addWidget(bubble);

    // 자동 스크롤
    QTimer::singleShot(0, this, [=]()
                       { scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum()); });
}

void ChatBotWidget::processWithMcp(const QString &userInput)
{
    if (!mcpClient)
    {
        // MCP가 없으면 기존 Gemini 직접 호출
        if (gemini)
        {
            gemini->askGemini(this, userInput, [=](const QString &response)
                              {
                    ChatMessage botMsg = { "bot", response, getCurrentTime() };
                    addMessage(botMsg);
                    waitingForResponse = false;
                    sendButton->setEnabled(true); });
        }
        return;
    }

    // MCP 컨텍스트 확인
    ConversationContext *context = mcpClient->getCurrentContext();

    // 도구 실행 키워드 확인
    if (context && context->selectedTool.has_value())
    {
        QStringList execKeywords = {
            "해줘", "하자", "실행", "만들어", "계산",
            "저장", "작성", "조회", "보여", "확인", "분석",
            "켜", "꺼", "시작", "정지", "멈춰"};

        for (const QString &keyword : execKeywords)
        {
            if (userInput.contains(keyword))
            {
                mcpClient->pipelineToolExecution(userInput);
                return;
            }
        }
    }

    // 도구 발견 파이프라인 실행
    mcpClient->pipelineToolDiscovery(userInput);
}

void ChatBotWidget::handleSend()
{
    QString text = input->text().trimmed();
    if (text.isEmpty())
        return;

    ChatMessage userMsg = {"user", text, getCurrentTime()};
    addMessage(userMsg);
    input->clear();

    waitingForResponse = true;
    sendButton->setEnabled(false);

    // MCP로 처리
    processWithMcp(text);

    // // Gemini 호출
    // if (gemini) {
    //     gemini->askGemini(this, text, [=](const QString& response) {
    //         ChatMessage botMsg = { "bot", response, getCurrentTime() };
    //         addMessage(botMsg);
    //     });
    // } else {
    //     ChatMessage botMsg = { "bot", "Gemini가 연결되지 않았습니다.", getCurrentTime() };
    //     addMessage(botMsg);
    // }
}

void ChatBotWidget::handleQuickReplyClicked()
{
    // 퀵메뉴 누르면..
}

QString ChatBotWidget::getCurrentTime()
{
    return QTime::currentTime().toString("hh:mm AP");
}
