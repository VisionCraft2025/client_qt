#include "chatbot_widget.h"
#include "ai_command.h"
#include "MCPAgentClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QTextEdit>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QScrollBar>

ChatBotWidget::ChatBotWidget(QWidget *parent)
    : QWidget(parent), waitingForResponse(false)
{
    setFixedSize(504, 646);
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
    outerFrame->setFixedSize(504, 646);

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
    innerFrame->setMinimumSize(504, 597);

    // 헤더
    QWidget *header = new QWidget(outerFrame);
    header->setMinimumHeight(49);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 9, 14, 9);

    QVBoxLayout *outerLayout = new QVBoxLayout(outerFrame);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);
    outerLayout->addWidget(innerFrame);

    QLabel *title = new QLabel("<b>🤖 VisionCraft AI</b>");
    title->setStyleSheet("color: white; font-size: 17px;");
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);
    title->setTextInteractionFlags(Qt::NoTextInteraction);

    QLabel *subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setStyleSheet("color: white; font-size: 12px;");

    QVBoxLayout *titleBox = new QVBoxLayout;
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    titleBox->setSpacing(0);
    titleBox->setContentsMargins(0, 0, 0, 0);

    closeButton = new QPushButton("\u2715");
    closeButton->setStyleSheet("background: transparent; color: white; border: none; font-size: 16px;");
    closeButton->setFixedSize(28, 28);
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);

    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    // 내부 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(innerFrame);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(9);

    // 메시지 스크롤 영역
    scrollArea = new QScrollArea(innerFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
                background-color: transparent;
            }
        )");

    messageContainer = new QWidget;
    messageContainer->setStyleSheet("background-color: white;");
    messageLayout = new QVBoxLayout(messageContainer);
    messageLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(messageContainer);

    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageContainer->setMinimumWidth(480);

    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout *quickLayout = new QHBoxLayout;
    QStringList quickTexts = {
        "기능 소개",           // 기능 안내
        "컨베이어1 정보",      // 7월 정보
        "피더2 켜줘",          // MQTT 제어
        "에러 통계"            // 7월 에러 통계
    };

    for (const QString &text : quickTexts)
    {
        QPushButton *quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 13px; padding: 7px; background-color: #f3f4f6; border-radius: 7px;");
        connect(quick, &QPushButton::clicked, this, [=]()
                {
                QString command = text;
                if (text == "기능 소개") {
                    command = "어떤거 할 수 있어?";
                } else if (text == "컨베이어1 정보") {
                    command = "컨베이어1 7월 정보 보여줘";
                } else if (text == "에러 통계") {
                    command = "7월 에러 통계 보여줘";
                }
                input->setText(command);
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
            border-radius: 12px;
            padding: 9px;
            font-size: 14px;
        )");

    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(39, 28);
    sendButton->setStyleSheet(R"(
            background-color: #fb923c;
            color: white;
            border: none;
            border-radius: 9px;
            font-size: 14px;
        )");

    connect(sendButton, &QPushButton::clicked, this, &ChatBotWidget::handleSend);
    connect(input, &QLineEdit::returnPressed, this, &ChatBotWidget::handleSend);
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
    if (gemini && !gemini->getApiKey().isEmpty()) // apiKey 대신 getApiKey() 사용
    {
        mcpClient = std::make_unique<MCPAgentClient>(
            mcpServerUrl,
            gemini->getApiKey(), // apiKey 대신 getApiKey() 사용
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
    // 도구가 선택되었고 "실행하겠습니다" 메시지가 포함된 경우 자동 실행
    if (context.selectedTool.has_value() &&
        context.executionResult.has_value() &&
        context.executionResult.value().contains("실행하겠습니다"))
    {
        // 메시지 표시
        ChatMessage botMsg = {"bot", context.executionResult.value(), getCurrentTime()};
        addMessage(botMsg);

        // 자동으로 도구 실행
        QTimer::singleShot(500, this, [this, context]()
                           { mcpClient->pipelineToolExecution(context.userQuery,
                                                              const_cast<ConversationContext *>(&context)); });
    }
    else if (context.executionResult.has_value())
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
    // QTextEdit 대신 QLabel 사용 (높이 계산이 더 정확함)
    QLabel *msgLabel = new QLabel();
    msgLabel->setWordWrap(true);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    
    // HTML 형식으로 변환
    QString formattedContent = msg.content;
    formattedContent.replace("\n", "<br>");
    
    // 볼드 처리
    QRegularExpression boldRegex(R"(\*\*(.*?)\*\*)");
    formattedContent.replace(boldRegex, "<b>\\1</b>");
    
    // 코드 블록 처리
    QRegularExpression codeRegex(R"(```(.*?)```)");
    formattedContent.replace(codeRegex, "<pre style='background-color: #f0f0f0; padding: 5px;'>\\1</pre>");
    
    msgLabel->setText(formattedContent);
    
    // 동적 크기 계산 - 현재 위젯 크기를 기반으로 계산
    int containerWidth = this->width() - 48; // 양쪽 마진 (24px * 2)
    int minWidth = containerWidth * 0.23;    // 컨테이너 너비의 23%
    int maxWidth = containerWidth * 0.93;    // 컨테이너 너비의 93%
    
    // 텍스트 길이에 따른 폭 계산
    QFontMetrics fm(msgLabel->font());
    int textWidth = fm.boundingRect(0, 0, maxWidth - 28, 0, 
                                   Qt::TextWordWrap | Qt::AlignLeft, 
                                   msgLabel->text()).width() + 28;
    
    int finalWidth = qBound(minWidth, textWidth, maxWidth);
    
    // 긴 텍스트는 최대 폭 사용
    if (msg.content.length() > 50 || msg.content.contains("\n")) {
        finalWidth = maxWidth;
    }
    
    msgLabel->setMinimumWidth(finalWidth);
    msgLabel->setMaximumWidth(finalWidth);
    
    // 스타일 적용
    if (msg.sender == "bot") {
        msgLabel->setStyleSheet(QString(R"(
            QLabel {
                background-color: #f3f4f6; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Malgun Gothic", sans-serif;
                font-size: 14px;
                color: black;
            }
        )"));
    } else {
        msgLabel->setStyleSheet(QString(R"(
            QLabel {
                background-color: #fb923c; 
                color: white; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Malgun Gothic", sans-serif;
                font-size: 14px;
            }
        )"));
    }
    
    // 시간 라벨
    QLabel *timeLabel = new QLabel(msg.time);
    timeLabel->setStyleSheet("font-size: 12px; color: gray;");
    
    // 레이아웃 구성
    QVBoxLayout *bubbleLayout = new QVBoxLayout;
    bubbleLayout->setSpacing(4);
    
    if (msg.sender == "bot") {
        bubbleLayout->setAlignment(Qt::AlignLeft);
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignLeft);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignLeft);
    } else {
        bubbleLayout->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignRight);
        timeLabel->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);
    }
    
    QWidget *bubble = new QWidget;
    bubble->setLayout(bubbleLayout);
    messageLayout->addWidget(bubble);
    
    // 자동 스크롤
    QTimer::singleShot(50, this, [=]() {
        scrollArea->verticalScrollBar()->setValue(
            scrollArea->verticalScrollBar()->maximum());
    });
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

    // MQTT 제어 키워드 확인
    QStringList mqttKeywords = {
        "피더2", "피더 2", "피더02", "피더 02", "피더 2번",
        "컨베이어2", "컨베이어 2", "컨베이어02", "컨베이어 02",
        "컨베이어3", "컨베이어 3", "컨베이어03", "컨베이어 03", 
        "로봇팔", "로봇암", "로봇"
    };
    
    QStringList controlKeywords = {
        "켜", "꺼", "시작", "정지", "멈춰", "가동", "작동", "끄", "킬"
    };
    
    bool hasMqttDevice = false;
    bool hasControlKeyword = false;
    
    for (const QString &keyword : mqttKeywords) {
        if (userInput.contains(keyword)) {
            hasMqttDevice = true;
            break;
        }
    }
    
    for (const QString &keyword : controlKeywords) {
        if (userInput.contains(keyword)) {
            hasControlKeyword = true;
            break;
        }
    }
    
    // MQTT 기기와 제어 키워드가 모두 있으면 바로 도구 발견
    if (hasMqttDevice && hasControlKeyword) {
        mcpClient->pipelineToolDiscovery(userInput);
        return;
    }

    // 통계 요청 키워드 체크
    QStringList statsKeywords = {
        "통계", "분석", "집계", "요약"
    };
    
    bool isStatsRequest = false;
    for (const QString &keyword : statsKeywords) {
        if (userInput.contains(keyword) && userInput.contains("에러")) {
            isStatsRequest = true;
            break;
        }
    }
    
    // 조회/확인 요청 키워드 체크
    QStringList queryKeywords = {
        "보여", "확인", "조회", "검색", "찾아", "알려",
        "로그", "데이터", "기록", "내역"
    };

    bool isQueryRequest = false;
    for (const QString &keyword : queryKeywords) {
        if (userInput.contains(keyword) && !isStatsRequest) {
            isQueryRequest = true;
            break;
        }
    }

    // 도구 실행 키워드 확인
    if (context && context->selectedTool.has_value()) {
        QStringList execKeywords = {
            "해줘", "하자", "실행", "만들어", "계산",
            "저장", "작성", "조회", "보여", "확인", "분석",
            "켜", "꺼", "시작", "정지", "멈춰"
        };

        for (const QString &keyword : execKeywords) {
            if (userInput.contains(keyword)) {
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
