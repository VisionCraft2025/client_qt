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
    // 크기를 16% 증가 (378 * 1.16 = 438, 525 * 1.16 = 609)
    setFixedSize(438, 609);
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
    outerFrame->setFixedSize(438, 609);

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
    innerFrame->setMinimumSize(438, 560); // 헤더 제외한 높이 정도 (609 - 49)

    // 헤더
    QWidget *header = new QWidget(outerFrame);
    header->setMinimumHeight(49); // 헤더 높이 증가 (42 * 1.16 = 49)
    // header->setGeometry(0, 0, width(), 50);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 9, 14, 9); // 마진도 비례 증가

    QVBoxLayout *outerLayout = new QVBoxLayout(outerFrame); // 추가
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);     // 헤더 먼저
    outerLayout->addWidget(innerFrame); // 그 아래 흰 배경

    QLabel *title = new QLabel("<b>🤖 VisionCraft AI</b>");
    title->setStyleSheet("color: white; font-size: 17px;"); // 15 * 1.16 = 17.4 → 17
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);                             // 여기!! 줄바꿈 방지
    title->setTextInteractionFlags(Qt::NoTextInteraction); // 드래그 방지

    QLabel *subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setStyleSheet("color: white; font-size: 12px;"); // 10 * 1.16 = 11.6 → 12

    QVBoxLayout *titleBox = new QVBoxLayout;
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    // 높이 고정
    titleBox->setSpacing(0);
    titleBox->setContentsMargins(0, 0, 0, 0);

    closeButton = new QPushButton("\u2715");
    closeButton->setStyleSheet("background: transparent; color: white; border: none; font-size: 16px;"); // 14 * 1.16 = 16.24 → 16
    closeButton->setFixedSize(28, 28); // 24 * 1.16 = 27.84 → 28
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);

    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    // 내부 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(innerFrame);
    mainLayout->setContentsMargins(14, 14, 14, 14); // 12 * 1.16 = 13.92 → 14
    mainLayout->setSpacing(9); // 8 * 1.16 = 9.28 → 9

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
    messageContainer->setMinimumWidth(415); // 새로운 챗봇 너비에 맞춤 (438 - 23)

    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout *quickLayout = new QHBoxLayout;
    QStringList quickTexts = {
        "피더02 켜줘",        // MQTT 제어
        "컨베이어 전체 꺼줘", // MQTT 전체 제어
        "오늘 로그 조회"      // DB 조회
    };
    for (const QString &text : quickTexts)
    {
        QPushButton *quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 13px; padding: 7px; background-color: #f3f4f6; border-radius: 7px;"); // 11*1.16=12.76→13, 6*1.16=6.96→7
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
            border-radius: 12px;
            padding: 9px;
            font-size: 14px;
        )"); // border-radius: 10*1.16=11.6→12, padding: 8*1.16=9.28→9, font-size: 12*1.16=13.92→14

    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(39, 28); // 16% 증가 (34*1.16=39.44→39, 24*1.16=27.84→28)
    sendButton->setStyleSheet(R"(
            background-color: #fb923c;
            color: white;
            border: none;
            border-radius: 9px;
            font-size: 14px;
        )"); // border-radius: 8*1.16=9.28→9, font-size: 12*1.16=13.92→14

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
    // 긴 텍스트를 위한 QTextEdit 사용
    QTextEdit *msgEdit = new QTextEdit();
    msgEdit->setReadOnly(true);
    msgEdit->setFrameStyle(QFrame::NoFrame);
    msgEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    msgEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // HTML 형식으로 변환 (마크다운 지원)
    QString formattedContent = msg.content;
    formattedContent.replace("\n", "<br>");

    // 볼드 처리 (**text** -> <b>text</b>)
    QRegularExpression boldRegex(R"(\*\*(.*?)\*\*)");
    formattedContent.replace(boldRegex, "<b>\\1</b>");

    // 코드 블록 처리 (```code``` -> <pre>code</pre>)
    QRegularExpression codeRegex(R"(```(.*?)```)");
    formattedContent.replace(codeRegex, "<pre>\\1</pre>");

    msgEdit->setHtml(formattedContent);

    // 동적 가로폭 계산 - 텍스트 길이에 따라 조절
    QFontMetrics fm(msgEdit->font());
    int textLength = formattedContent.length();
    int minWidth = 93;  // 최소 폭 (80 * 1.16)
    int maxWidth = 348; // 최대 폭 (300 * 1.16)
    
    // 텍스트 길이에 따른 동적 폭 계산
    QStringList lines = formattedContent.split("<br>");
    int longestLineWidth = 0;
    for (const QString& line : lines) {
        QString plainLine = line;
        plainLine.remove(QRegularExpression("<[^>]*>"));
        int lineWidth = fm.horizontalAdvance(plainLine) + 28; // 패딩 포함
        longestLineWidth = qMax(longestLineWidth, lineWidth);
    }

    // 계산된 폭을 최소/최대값 사이로 제한
    int calculatedWidth = qBound(minWidth, longestLineWidth, maxWidth);

    // 긴 텍스트는 최대 폭 사용
    if (textLength > 100 || formattedContent.contains("<br>")) {
        calculatedWidth = maxWidth;
    }

    // 크기 자동 조정
    msgEdit->document()->setTextWidth(calculatedWidth);
    int docHeight = msgEdit->document()->size().height();
    msgEdit->setMinimumHeight(docHeight + 23); // 패딩 여유 증가
    msgEdit->setMaximumHeight(docHeight + 23);
    msgEdit->setMinimumWidth(calculatedWidth);
    msgEdit->setMaximumWidth(calculatedWidth);
    msgEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QLabel *timeLabel = new QLabel(msg.time);
    timeLabel->setStyleSheet("font-size: 12px; color: gray;");

    QVBoxLayout *bubbleLayout = new QVBoxLayout;
    bubbleLayout->setSpacing(4);
    bubbleLayout->setSizeConstraint(QLayout::SetMinimumSize);

    if (msg.sender == "bot")
    {
        msgEdit->setStyleSheet(R"(
            QTextEdit {
                background-color: #f3f4f6; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Malgun Gothic", sans-serif;
                font-size: 14px;
                color: black;
                line-height: 1.4;
            }
        )");
        
        bubbleLayout->setAlignment(Qt::AlignLeft);
        bubbleLayout->addWidget(msgEdit, 0, Qt::AlignLeft);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignLeft);
    }
    else
    {
        msgEdit->setStyleSheet(R"(
            QTextEdit {
                background-color: #fb923c; 
                color: white; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Malgun Gothic", sans-serif;
                font-size: 14px;
                line-height: 1.4;
            }
        )");
        
        bubbleLayout->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(msgEdit, 0, Qt::AlignRight);
        timeLabel->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);
    }

    QWidget *bubble = new QWidget;
    bubble->setLayout(bubbleLayout);
    bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    bubble->adjustSize();
    messageLayout->addWidget(bubble);

    // 자동 스크롤
    QTimer::singleShot(50, this, [=]()
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

    // MQTT 제어 키워드 확인
    QStringList mqttKeywords = {
        "피더02", "피더2", "피더 2",
        "컨베이어02", "컨베이어2", "컨베이어 2",
        "컨베이어03", "컨베이어3", "컨베이어 3", 
        "로봇팔", "로봇암", "로봇"
    };
    
    QStringList controlKeywords = {
        "켜", "꺼", "시작", "정지", "멈춰", "가동", "작동"
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

    // 조회/확인 요청 키워드 체크 (도구 발견 전에도 체크)
    QStringList queryKeywords = {
        "보여", "확인", "조회", "검색", "찾아", "알려",
        "로그", "데이터", "기록", "내역"};

    bool isQueryRequest = false;
    for (const QString &keyword : queryKeywords)
    {
        if (userInput.contains(keyword))
        {
            isQueryRequest = true;
            break;
        }
    }

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
