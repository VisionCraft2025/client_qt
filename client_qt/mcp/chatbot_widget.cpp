#include "chatbot_widget.h"
#include "../ai_command.h"
#include "MCPAgentClient.h"
#include "../font_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextDocument>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QScrollBar>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttTopicName>
#include <QPropertyAnimation>
#include <QEasingCurve>

ChatBotWidget::ChatBotWidget(QWidget *parent)
    : QWidget(parent), waitingForResponse(false)
{
    // 초기 크기를 BIG으로 설정
    m_currentSizeMode = BIG;
    setFixedSize(SIZES[BIG].width, SIZES[BIG].height);

    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // 마우스 추적 활성화 (커서 변경을 위해)
    setMouseTracking(true);

    QFrame *outerFrame = new QFrame(this);
    outerFrame->setObjectName("outerFrame");
    outerFrame->setStyleSheet("QFrame#outerFrame { background-color: #f97316; border-radius: 16px; }");
    outerFrame->setFixedSize(504, 760);

    QFrame *innerFrame = new QFrame(outerFrame);
    innerFrame->setObjectName("innerFrame");
    innerFrame->setStyleSheet("QFrame#innerFrame { background-color: white; border-bottom-left-radius: 16px; border-bottom-right-radius: 16px; }");
    innerFrame->setMinimumSize(504, 685);

    QWidget *header = new QWidget(outerFrame);
    header->setMinimumHeight(75);
    header->setMaximumHeight(75);
    header->setCursor(Qt::SizeAllCursor);
    m_headerWidget = header;
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 12, 14, 12);

    QVBoxLayout *outerLayout = new QVBoxLayout(outerFrame);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);
    outerLayout->addWidget(innerFrame);

    QLabel *title = new QLabel("🤖 VisionCraft AI");
    title->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 15));
    title->setStyleSheet("color: white; padding: 3px 0px; line-height: 1.2;");
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);
    title->setTextInteractionFlags(Qt::NoTextInteraction);
    title->setMinimumHeight(24);

    QLabel *subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 10));
    subtitle->setStyleSheet("color: white; padding: 4px 0px; line-height: 1.2;");
    subtitle->setMinimumHeight(20);

    QVBoxLayout *titleBox = new QVBoxLayout;
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    titleBox->setSpacing(2);
    titleBox->setContentsMargins(0, 2, 0, 2);
    m_titleBox = titleBox;

    closeButton = new QPushButton("\u2715");
    closeButton->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 16));
    closeButton->setStyleSheet("background: transparent; color: white; border: none;");
    closeButton->setFixedSize(28, 28);
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);

    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(innerFrame);
    mainLayout->setContentsMargins(14, 14, 14, 22);
    mainLayout->setSpacing(15);

    scrollArea = new QScrollArea(innerFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");

    messageContainer = new QWidget;
    messageContainer->setStyleSheet("background-color: white;");
    messageLayout = new QVBoxLayout(messageContainer);
    messageLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(messageContainer);
    mainLayout->addWidget(scrollArea, 1);

    // 빠른 응답 버튼
    QHBoxLayout *quickLayout = new QHBoxLayout;
    QStringList quickTexts = {
                              "기능 소개",
                              "컨베이어 정보",
                              "불량률 통계",
                              "피더 켜줘"};

    for (const QString &text : quickTexts)
    {
        QPushButton *quick = new QPushButton(text);
        quick->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 11));
        quick->setStyleSheet("padding: 7px; background-color: #f3f4f6; border-radius: 7px;");
        connect(quick, &QPushButton::clicked, this, [=]()
                {
                    QString command = text;
                    if (text == "기능 소개") {
                        command = "어떤 기능이 있어?";
                    } else if (text == "컨베이어 정보") {
                        command = "컨베이어1 오늘 정보 보여줘";
                    } else if (text == "불량률 통계") {
                        command = "컨베이어1 불량률 알려줘";
                    } else if (text == "피더 켜줘") {
                        command = "피더2 기기를 켜줘";
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
    input->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 12));
    input->setStyleSheet("background-color: #f3f4f6; border: none; border-radius: 12px; padding: 15px; min-height: 22px;");
    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(55, 52);
    sendButton->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 12));
    sendButton->setStyleSheet("background-color: #fb923c; color: white; border: none; border-radius: 12px;");
    connect(sendButton, &QPushButton::clicked, this, &ChatBotWidget::handleSend);
    connect(input, &QLineEdit::returnPressed, this, &ChatBotWidget::handleSend);
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendButton);
    mainLayout->addLayout(inputLayout);

    ChatMessage welcome = {"bot", "안녕하세요.\n스마트팩토리 AI 어시스턴트입니다.\n\n장비 제어, 로그 조회, 통계 분석 등을 도와드립니다.\n어떤 도움이 필요하신가요?", getCurrentTime()};
    addMessage(welcome);

    initializeMqttClient();
    m_deviceStates["feeder_02"] = "off";
    m_deviceStates["conveyor_03"] = "off";
}

void ChatBotWidget::updateSizeMode(SizeMode newMode)
{
    if (m_currentSizeMode == newMode)
        return;

    m_currentSizeMode = newMode;
    const ChatSize &size = SIZES[newMode];

    // 애니메이션 없이 즉시 적용 (또는 매우 짧은 애니메이션)
    QPropertyAnimation *animation = new QPropertyAnimation(this, "size");
    animation->setDuration(150); // 200ms에서 150ms로 단축
    animation->setStartValue(this->size());
    animation->setEndValue(QSize(size.width, size.height));
    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, this, [this, size]()
            { applySize(size); });

    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void ChatBotWidget::applySize(const ChatSize &size)
{
    setFixedSize(size.width, size.height);

    // 외부 프레임 크기 조정
    QFrame *outerFrame = findChild<QFrame *>("outerFrame");
    if (outerFrame)
    {
        outerFrame->setFixedSize(size.width, size.height);
    }

    // 내부 프레임 크기 조정
    QFrame *innerFrame = findChild<QFrame *>("innerFrame");
    if (innerFrame)
    {
        int headerHeight = 75 * size.height / SIZES[BIG].height;
        innerFrame->setMinimumSize(size.width, size.height - headerHeight);
        innerFrame->setMaximumSize(size.width, size.height - headerHeight);
    }

    // 헤더 높이 조정
    if (m_headerWidget && m_headerWidget->layout()) {
        QHBoxLayout* headerLayout = qobject_cast<QHBoxLayout*>(m_headerWidget->layout());
        if (headerLayout) {
            if (m_currentSizeMode == SMALL) {
                headerLayout->setContentsMargins(8, 4, 8, 4);  // 상하 여백을 8에서 4로 줄임
            } else if (m_currentSizeMode == MIDDLE) {
                headerLayout->setContentsMargins(10, 8, 10, 8);
            } else {
                headerLayout->setContentsMargins(14, 12, 14, 12);
            }
        }
    }

    if (m_titleBox) {
        if (m_currentSizeMode == SMALL) {
            m_titleBox->setSpacing(0);  // 타이틀과 서브타이틀 간격 최소화
            m_titleBox->setContentsMargins(0, 0, 0, 0);  // 여백 제거
        } else if (m_currentSizeMode == MIDDLE) {
            m_titleBox->setSpacing(1);
            m_titleBox->setContentsMargins(0, 1, 0, 1);
        } else {
            m_titleBox->setSpacing(2);
            m_titleBox->setContentsMargins(0, 2, 0, 2);
        }
    }

    // 헤더 높이도 Small 모드에서 더 줄임
    if (m_headerWidget) {
        if (m_currentSizeMode == SMALL) {
            m_headerWidget->setMinimumHeight(50);  // 75 * 0.67
            m_headerWidget->setMaximumHeight(50);
        } else if (m_currentSizeMode == MIDDLE) {
            m_headerWidget->setMinimumHeight(60);
            m_headerWidget->setMaximumHeight(60);
        } else {
            m_headerWidget->setMinimumHeight(75);
            m_headerWidget->setMaximumHeight(75);
        }
    }

    // 메인 레이아웃 여백 조정
    QFrame *innerFrameWidget = findChild<QFrame *>("innerFrame");
    if (innerFrameWidget && innerFrameWidget->layout())
    {
        QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(innerFrameWidget->layout());
        if (mainLayout)
        {
            int margin = 14 * size.width / SIZES[BIG].width;
            int spacing = 15 * size.height / SIZES[BIG].height;
            mainLayout->setContentsMargins(margin, margin, margin, margin + 8);
            mainLayout->setSpacing(spacing);
        }
    }

    // 타이틀과 서브타이틀 폰트 크기 및 패딩 조정
    QList<QLabel*> labels = findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->text().contains("VisionCraft")) {
            if (m_currentSizeMode == SMALL) {
                label->setText("🤖 VisionCraft AI");
                label->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
                label->setStyleSheet("color: white; padding: 0px; margin: 0px; line-height: 1.0;");
            } else if (m_currentSizeMode == MIDDLE) {
                label->setText("🤖 VisionCraft AI");
                label->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 12));
                label->setStyleSheet("color: white; padding: 2px 0px; margin: 0px; line-height: 1.1;");
            } else {
                label->setText("🤖 VisionCraft AI");
                label->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 15));
                label->setStyleSheet("color: white; padding: 3px 0px; margin: 0px; line-height: 1.2;");
            }
        } else if (label->text().contains("스마트 팩토리")) {
            if (m_currentSizeMode == SMALL) {
                label->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 7));
                label->setStyleSheet("color: white; padding: 0px; margin: 0px; line-height: 1.0;");
            } else if (m_currentSizeMode == MIDDLE) {
                label->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 9));
                label->setStyleSheet("color: white; padding: 2px 0px; margin: 0px; line-height: 1.1;");
            } else {
                label->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 10));
                label->setStyleSheet("color: white; padding: 4px 0px; margin: 0px; line-height: 1.2;");
            }
        }
    }

    // 헤더 레이아웃 여백 조정 - Small 모드에서는 더 작게
    if (m_headerWidget && m_headerWidget->layout()) {
        QHBoxLayout* headerLayout = qobject_cast<QHBoxLayout*>(m_headerWidget->layout());
        if (headerLayout) {
            if (m_currentSizeMode == SMALL) {
                headerLayout->setContentsMargins(8, 8, 8, 8);  // 더 작은 여백
            } else if (m_currentSizeMode == MIDDLE) {
                headerLayout->setContentsMargins(10, 10, 10, 10);
            } else {
                headerLayout->setContentsMargins(14, 12, 14, 12);
            }
        }
    }

    // 닫기 버튼 크기 조정
    if (closeButton)
    {
        int btnSize = 28 * size.width / SIZES[BIG].width;
        closeButton->setFixedSize(btnSize, btnSize);
        int fontSize = 16 * size.width / SIZES[BIG].width;
        closeButton->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, fontSize));
        closeButton->setStyleSheet("background: transparent; color: white; border: none;");
    }

    // 입력창과 버튼 크기 조정
    if (input)
    {
        int minHeight = 22 * size.height / SIZES[BIG].height;
        input->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, size.fontSize));
        input->setStyleSheet(QString(R"(
            background-color: #f3f4f6;
            border: none;
            border-radius: %1px;
            padding: %2px;
            min-height: %3px;
        )")
                                 .arg(12 * size.width / SIZES[BIG].width)
                                 .arg(size.padding)
                                 .arg(minHeight));
    }

    if (sendButton)
    {
        int btnWidth = 55 * size.width / SIZES[BIG].width;
        sendButton->setFixedSize(btnWidth, size.buttonHeight);
        sendButton->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, size.fontSize));
        sendButton->setStyleSheet(QString(R"(
            background-color: #fb923c;
            color: white;
            border: none;
            border-radius: %1px;
        )")
                                      .arg(12 * size.width / SIZES[BIG].width));
    }

    // 빠른 응답 버튼들 크기 조정
    QList<QPushButton *> quickButtons = findChildren<QPushButton *>();
    for (QPushButton *btn : quickButtons)
    {
        if (btn != sendButton && btn != closeButton &&
            (btn->text() == "기능 소개" || btn->text() == "컨베이어 정보" ||
             btn->text() == "불량률 통계" || btn->text() == "피더 켜줘"))
        {
            int btnFontSize = qMax(8, size.fontSize - 1); // 최소 8px
            int btnPadding = qMax(3, size.padding / 2);   // 최소 3px
            btn->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, btnFontSize));
            btn->setStyleSheet(QString(R"(
                padding: %1px;
                background-color: #f3f4f6;
                border-radius: 7px;
            )")
                                   .arg(btnPadding));
            btn->setMinimumHeight(size.quickBtnHeight);
        }
    }

    // 기존 메시지들의 크기 조정
    refreshAllMessages();
}

// 모든 메시지 새로 고침
void ChatBotWidget::refreshAllMessages()
{
    const ChatSize &currentSize = SIZES[m_currentSizeMode];

    QList<QWidget *> bubbles = messageContainer->findChildren<QWidget *>();
    for (QWidget *bubble : bubbles)
    {
        QList<QLabel *> labels = bubble->findChildren<QLabel *>();
        for (QLabel *label : labels)
        {
            QString currentStyle = label->styleSheet();

            // 메시지 라벨인지 시간 라벨인지 구분
            if (currentStyle.contains("padding") && currentStyle.contains("border-radius: 14px"))
            {
                // 메시지 라벨
                label->setMaximumWidth(width() * 0.75);
                label->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, currentSize.fontSize));
                currentStyle.replace(QRegularExpression("padding:\\s*\\d+px"),
                                     QString("padding: %1px").arg(currentSize.padding));
            }
            else if (currentStyle.contains("color: gray"))
            {
                // 시간 라벨
                label->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, currentSize.fontSize - 2));
            }

            label->setStyleSheet(currentStyle);
        }
    }
}

bool ChatBotWidget::isInResizeArea(const QPoint &pos)
{
    if (!m_headerWidget)
        return false;

    // 헤더 영역인지 확인
    QRect headerRect = m_headerWidget->geometry();
    if (!headerRect.contains(pos))
        return false;

    // 좌우 모서리 영역 확인 (30픽셀로 확대)
    return (pos.x() <= RESIZE_MARGIN || pos.x() >= width() - RESIZE_MARGIN);
}

ChatBotWidget::SizeMode ChatBotWidget::calculateNewSizeMode(int deltaX)
{
    // 드래그 방향과 거리에 따라 새로운 크기 결정

    if (abs(deltaX) < RESIZE_THRESHOLD)
    {
        return m_currentSizeMode;
    }

    // 현재 크기와 드래그 방향에 따라 다음 크기 결정
    bool isDraggingRight = (m_resizeStartX <= RESIZE_MARGIN) ? (deltaX > 0) : (deltaX > 0);
    bool wantSmaller = (m_resizeStartX <= RESIZE_MARGIN) ? isDraggingRight : !isDraggingRight;

    if (wantSmaller)
    {
        // 작아지는 방향
        switch (m_currentSizeMode)
        {
        case BIG:
            return (abs(deltaX) > RESIZE_THRESHOLD) ? MIDDLE : BIG;
        case MIDDLE:
            return (abs(deltaX) > RESIZE_THRESHOLD) ? SMALL : MIDDLE;
        case SMALL:
            return SMALL;
        }
    }
    else
    {
        // 커지는 방향
        switch (m_currentSizeMode)
        {
        case SMALL:
            return (abs(deltaX) > RESIZE_THRESHOLD) ? MIDDLE : SMALL;
        case MIDDLE:
            return (abs(deltaX) > RESIZE_THRESHOLD) ? BIG : MIDDLE;
        case BIG:
            return BIG;
        }
    }

    return m_currentSizeMode;
}

void ChatBotWidget::setMcpServerUrl(const QString &url)
{

    mcpServerUrl = url;
}

void ChatBotWidget::setGemini(GeminiRequester *requester)
{
    this->gemini = requester;

    // MCP 클라이언트 초기화
    if (gemini && !gemini->getApiKey().isEmpty())
    {
        mcpClient = std::make_unique<MCPAgentClient>(
            mcpServerUrl,
            gemini->getApiKey(),
            this);

        // MQTT 클라이언트 전달
        mcpClient->setMqttClient(m_mqttClient);

        // 통합 파이프라인 시그널 연결
        connect(mcpClient.get(), &MCPAgentClient::pipelineCompleted,
                this, [this](const QString &result)
                {
                    // 로딩 메시지가 표시되어 있다면 제거
                    if (m_isLoadingMessageShown && messageLayout->count() > 0) {
                        // 마지막 메시지(로딩 메시지) 제거
                        QLayoutItem* item = messageLayout->takeAt(messageLayout->count() - 1);
                        if (item && item->layout()) {
                            QLayoutItem* childItem;
                            while ((childItem = item->layout()->takeAt(0)) != nullptr) {
                                delete childItem->widget();
                                delete childItem;
                            }
                            delete item;
                        }
                        m_isLoadingMessageShown = false;
                    }

                    ChatMessage botMsg = {"bot", result, getCurrentTime()};
                    addMessage(botMsg);
                    waitingForResponse = false;
                    sendButton->setEnabled(true); });

        connect(mcpClient.get(), &MCPAgentClient::pipelineStateChanged,
                this, [this](PipelineState state)
                {
                    // 상태별 UI 업데이트 (옵션)
                    QString stateMsg;
                    switch (state)
                    {
                    case PipelineState::DISCOVERING_TOOL:
                        stateMsg = "🔍 요청 분석 중...";
                        break;
                    case PipelineState::EXECUTING_TOOL:
                        stateMsg = "⚡ 도구 실행 중...";
                        break;
                    case PipelineState::FORMATTING_RESULT:
                        stateMsg = "📋 결과 정리 중...";
                        break;
                    default:
                        return;
                    }
                    // 임시 메시지 표시 (옵션)
                });

        connect(mcpClient.get(), &MCPAgentClient::errorOccurred,
                this, [this](const QString &error)
                {
                    ChatMessage errorMsg = {
                        "bot",
                        QString("⚠️ 오류가 발생했습니다: %1").arg(error),
                        getCurrentTime()
                    };
                    addMessage(errorMsg);
                    waitingForResponse = false;
                    sendButton->setEnabled(true); });

        // 도구 목록 미리 가져오기
        mcpClient->fetchTools(true);
    }
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
    // 1. 메시지 라벨(말풍선 내용) 생성
    QLabel *msgLabel = new QLabel();
    msgLabel->setWordWrap(true); // 자동 줄바꿈 활성화 (핵심!)
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // 챗봇 위젯의 전체 너비(504px)에서 여백 등을 뺀 값으로 최대 너비를 고정
    int maxBubbleWidth = this->width() * 0.75; // 약 75% 정도로 설정
    msgLabel->setMaximumWidth(maxBubbleWidth);

    // HTML 서식 적용
    QString formattedContent = msg.content;
    formattedContent.replace("\n", "<br>");
    formattedContent.replace(QRegularExpression(R"(###\s*(.*?)<br>)"), "<b>\\1</b><br>");
    formattedContent.replace(QRegularExpression(R"(\*\s+(.*?)<br>)"), "\\1<br>");
    formattedContent.replace(QRegularExpression(R"(\*\*(.*?)\*\*)"), "<b>\\1</b>");
    formattedContent.replace(QRegularExpression(R"(```(.*?)```)"), "<pre style='background-color: #f0f0f0; padding: 5px;'>\\1</pre>");
    msgLabel->setText(formattedContent);

    const ChatSize &currentSize = SIZES[m_currentSizeMode];

    // 메시지 라벨 생성 시 폰트 크기 적용
    msgLabel->setMaximumWidth(this->width() * 0.75);

    // 2. 발신자에 따라 스타일시트 적용
    if (msg.sender == "bot")
    {
        msgLabel->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, currentSize.fontSize));
        msgLabel->setStyleSheet(QString(R"(
            background-color: #f3f4f6;
            color: black;
            padding: %1px;
            border-radius: 14px;
        )")
                                    .arg(currentSize.padding));
    }
    else
    {
        msgLabel->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, currentSize.fontSize));
        msgLabel->setStyleSheet(QString(R"(
            background-color: #fb923c;
            color: white;
            padding: %1px;
            border-radius: 14px;
        )")
                                    .arg(currentSize.padding));
    }

    // 3. 시간 라벨 생성
    QLabel *timeLabel = new QLabel(msg.time);
    timeLabel->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, currentSize.fontSize - 2));
    timeLabel->setStyleSheet("color: gray;");

    // 4. 메시지와 시간을 묶는 수직 레이아웃 (실제 말풍선)
    QVBoxLayout *bubbleContentLayout = new QVBoxLayout();
    bubbleContentLayout->setSpacing(4);
    bubbleContentLayout->addWidget(msgLabel);
    bubbleContentLayout->addWidget(timeLabel);

    QWidget *bubbleWidget = new QWidget();
    bubbleWidget->setLayout(bubbleContentLayout);

    // 5. [핵심 변경] Spacer를 이용한 좌/우 정렬
    QHBoxLayout *wrapperLayout = new QHBoxLayout();
    wrapperLayout->setContentsMargins(0, 0, 0, 0);

    if (msg.sender == "bot")
    {
        wrapperLayout->addWidget(bubbleWidget);
        // 오른쪽에 빈 공간(Spacer)을 추가하여 말풍선을 왼쪽으로 밀어냅니다.
        wrapperLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred));
        // 시간 라벨도 왼쪽 정렬
        timeLabel->setAlignment(Qt::AlignLeft);
    }
    else
    {
        // 왼쪽에 빈 공간(Spacer)을 추가하여 말풍선을 오른쪽으로 밀어냅니다.
        wrapperLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred));
        wrapperLayout->addWidget(bubbleWidget);
        // 시간 라벨도 오른쪽 정렬
        timeLabel->setAlignment(Qt::AlignRight);
    }

    // 전체를 감싸는 wrapper를 최종 레이아웃에 추가
    messageLayout->addLayout(wrapperLayout);

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
            // 로딩 메시지 추가
            ChatMessage loadingMsg = {"bot", "🤔 생각 중입니다...", getCurrentTime()};
            addMessage(loadingMsg);

            gemini->askGemini(this, userInput, [=](const QString &response)
                              {
                                  // 메시지 레이아웃에서 마지막 메시지(로딩 메시지) 제거
                                  if (messageLayout->count() > 0) {
                                      QLayoutItem* item = messageLayout->takeAt(messageLayout->count() - 1);
                                      if (item && item->layout()) {
                                          QLayoutItem* childItem;
                                          while ((childItem = item->layout()->takeAt(0)) != nullptr) {
                                              delete childItem->widget();
                                              delete childItem;
                                          }
                                          delete item;
                                      }
                                  }

                                  ChatMessage botMsg = { "bot", response, getCurrentTime() };
                                  addMessage(botMsg);
                                  waitingForResponse = false;
                                  sendButton->setEnabled(true); });
        }
        return;
    }

    // 로딩 메시지 추가
    ChatMessage loadingMsg = {"bot", "🤔 요청을 처리하고 있습니다...", getCurrentTime()};
    addMessage(loadingMsg);
    m_isLoadingMessageShown = true; // 플래그 추가 (헤더에 선언 필요)

    // 통합 파이프라인 실행 - 모든 처리를 MCP가 담당
    mcpClient->executeUnifiedPipeline(userInput);
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

    // MCP 통합 파이프라인으로 처리
    processWithMcp(text);
}

void ChatBotWidget::handleQuickReplyClicked()
{
    // 퀵메뉴 누르면..
}

QString ChatBotWidget::getCurrentTime()
{
    return QTime::currentTime().toString("hh:mm AP");
}

void ChatBotWidget::initializeMqttClient()
{
    m_mqttClient = new QMqttClient(this);
    m_mqttClient->setHostname("mqtt.kwon.pics");
    m_mqttClient->setPort(1883);

    connect(m_mqttClient, &QMqttClient::connected, this, &ChatBotWidget::onMqttConnected);

    // MQTT 서버에 연결
    m_mqttClient->connectToHost();
}

// onMqttConnected 메서드 수정 - 응답 토픽 구독 추가
void ChatBotWidget::onMqttConnected()
{
    qDebug() << "ChatBot MQTT Connected";

    // 기기 상태 토픽 구독
    QStringList statusTopics = {
                                "feeder_02/status",
                                "conveyor_03/status"};

    for (const QString &topic : statusTopics)
    {
        auto sub = m_mqttClient->subscribe(topic);
        if (sub)
        {
            connect(sub, &QMqttSubscription::messageReceived,
                    this, &ChatBotWidget::onMqttStatusReceived);
            qDebug() << "ChatBot subscribed to:" << topic;
        }
    }

    // 기기 명령 토픽도 구독 (기기 시뮬레이션용)
    QStringList commandTopics = {
                                 "feeder_02/cmd",
                                 "conveyor_03/cmd"};

    for (const QString &topic : commandTopics)
    {
        auto sub = m_mqttClient->subscribe(topic);
        if (sub)
        {
            connect(sub, &QMqttSubscription::messageReceived,
                    this, &ChatBotWidget::onMqttCommandReceived);
            qDebug() << "ChatBot subscribed to command topic:" << topic;
        }
    }

    // 데이터베이스 쿼리 응답 토픽 구독
    auto queryResponseSub = m_mqttClient->subscribe(QString("factory/query/response"));
    if (queryResponseSub)
    {
        connect(queryResponseSub, &QMqttSubscription::messageReceived,
                this, &ChatBotWidget::onMqttMessageReceived);
    }

    // 통계 응답 토픽 구독
    auto statsSub = m_mqttClient->subscribe(QString("factory/+/msg/statistics"));
    if (statsSub)
    {
        connect(statsSub, &QMqttSubscription::messageReceived,
                this, &ChatBotWidget::onMqttMessageReceived);
    }

    // 불량률 정보 토픽 구독
    auto failureSub = m_mqttClient->subscribe(QString("factory/+/log/info"));
    if (failureSub)
    {
        connect(failureSub, &QMqttSubscription::messageReceived,
                this, &ChatBotWidget::onMqttMessageReceived);
    }
}

// MQTT 상태 메시지 수신 처리
void ChatBotWidget::onMqttStatusReceived(const QMqttMessage &message)
{
    QString topic = message.topic().name();
    QString payload = QString::fromUtf8(message.payload());

    qDebug() << "ChatBot received status:" << topic << payload;

    // 기기 ID 추출
    QString deviceId;
    if (topic == "feeder_02/status")
        deviceId = "feeder_02";
    else if (topic == "conveyor_03/status")
        deviceId = "conveyor_03";
    else
        return; // 지원하지 않는 기기

    // 대기 중인 제어 명령이 있는지 확인
    if (m_pendingControls.contains(deviceId))
    {
        QString expectedCommand = m_pendingControls[deviceId];

        if (payload == expectedCommand)
        {
            // 타이머 정지
            if (m_controlTimers.contains(deviceId))
            {
                m_controlTimers[deviceId]->stop();
                m_controlTimers[deviceId]->deleteLater();
                m_controlTimers.remove(deviceId);
            }
            m_pendingControls.remove(deviceId);

            // 성공 메시지
            QString deviceKorean = getDeviceKoreanName(deviceId);
            QString actionText = (expectedCommand == "on") ? "켜졌습니다" : "꺼졌습니다";

            ChatMessage successMsg = {
                                      "bot",
                                      QString("✅ %1이 %2.").arg(deviceKorean, actionText),
                                      getCurrentTime()};
            addMessage(successMsg);
        }
    }
}

// 타임아웃 처리
void ChatBotWidget::handleMqttControlTimeout(const QString &deviceId)
{
    if (m_pendingControls.contains(deviceId))
    {
        m_pendingControls.remove(deviceId);

        QString deviceKorean = getDeviceKoreanName(deviceId);
        ChatMessage errorMsg = {
                                "bot",
                                QString("⚠️ %1 제어 응답 시간이 초과되었습니다. 기기 상태를 확인해주세요.").arg(deviceKorean),
                                getCurrentTime()};
        addMessage(errorMsg);
    }

    // 타이머 제거
    if (m_controlTimers.contains(deviceId))
    {
        m_controlTimers[deviceId]->deleteLater();
        m_controlTimers.remove(deviceId);
    }
}

// 기기 이름 한글 변환 헬퍼
QString ChatBotWidget::getDeviceKoreanName(const QString &deviceId)
{
    if (deviceId == "feeder_02")
        return "피더 2번";
    else if (deviceId == "conveyor_03")
        return "컨베이어 3번";
    return deviceId;
}

// MQTT 명령 수신 처리 (기기 시뮬레이션)
void ChatBotWidget::onMqttCommandReceived(const QMqttMessage &message)
{
    QString topic = message.topic().name();
    QString command = QString::fromUtf8(message.payload());

    qDebug() << "ChatBot received command:" << topic << command;

    // 기기 ID 추출
    QString deviceId;
    if (topic == "feeder_02/cmd")
        deviceId = "feeder_02";
    else if (topic == "conveyor_03/cmd")
        deviceId = "conveyor_03";
    else
        return; // 지원하지 않는 기기

    // 기기 상태 업데이트
    m_deviceStates[deviceId] = command;

    // 상태 토픽으로 응답 발행 (기기 시뮬레이션)
    QString statusTopic = QString("%1/status").arg(deviceId.split("/")[0]);
    QTimer::singleShot(500, this, [this, statusTopic, command]()
                       {
                           if (m_mqttClient && m_mqttClient->state() == QMqttClient::Connected) {
                               m_mqttClient->publish(QMqttTopicName(statusTopic), command.toUtf8());
                               qDebug() << "기기 시뮬레이션 응답:" << statusTopic << "->" << command;
                           } });
}

void ChatBotWidget::onMqttMessageReceived(const QMqttMessage &message)
{
    QString topic = message.topic().name();
    QJsonDocument doc = QJsonDocument::fromJson(message.payload());
    QJsonObject response = doc.object();

    qDebug() << "ChatBot MQTT 수신:" << topic;

    // 데이터베이스 쿼리 응답
    if (topic == "factory/query/response")
    {
        QString queryId = response["query_id"].toString();
        if (queryId == m_currentQueryId)
        {
            processMqttQueryResponse(response);
        }
    }
    // 통계 응답
    else if (topic.contains("/msg/statistics"))
    {
        QString deviceId = response["device_id"].toString();
        double avgSpeed = response["average"].toDouble();
        double currentSpeed = response["current_speed"].toDouble();

        // MCPAgentClient에 데이터 캐싱 (출력하지 않음)
        if (mcpClient)
        {
            mcpClient->cacheStatisticsData(deviceId, avgSpeed, currentSpeed);
        }

        qDebug() << "통계 데이터 캐시됨:" << deviceId << "평균:" << avgSpeed << "현재:" << currentSpeed;
    }
    // 불량률 정보
    else if (topic.contains("/log/info"))
    {
        QJsonObject msgData = response["message"].toObject();
        if (msgData.contains("failure"))
        {
            QString deviceId = topic.split('/')[1];
            double failureRate = msgData["failure"].toString().toDouble() * 100;
            int total = msgData["total"].toString().toInt();
            int pass = msgData["pass"].toString().toInt();
            int fail = msgData["fail"].toString().toInt();

            // MCPAgentClient에 데이터 캐싱 (출력하지 않음)
            if (mcpClient)
            {
                mcpClient->cacheFailureStatsData(deviceId, failureRate, total, pass, fail);
            }

            qDebug() << "불량률 데이터 캐시됨:" << deviceId << "불량률:" << failureRate << "%";
        }
    }
}

// 쿼리 ID 생성
QString ChatBotWidget::generateQueryId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// MQTT 쿼리 응답 처리
void ChatBotWidget::processMqttQueryResponse(const QJsonObject &response)
{
    QString status = response["status"].toString();
    if (status != "success")
    {
        ChatMessage errorMsg = {
                                "bot",
                                QString("⚠️ 데이터 조회 실패: %1").arg(response["error"].toString()),
                                getCurrentTime()};
        addMessage(errorMsg);
        return;
    }

    QJsonArray dataArray = response["data"].toArray();

    // 데이터 포맷팅 및 표시
    QString resultMsg = "📊 **조회 결과**\n\n";

    int count = 0;
    for (const QJsonValue &value : dataArray)
    {
        if (count >= 10)
        {
            resultMsg += QString("\n... 그리고 %1개 더").arg(dataArray.size() - 10);
            break;
        }

        QJsonObject item = value.toObject();
        QString deviceId = item["device_id"].toString();
        QString logCode = item["log_code"].toString();
        qint64 timestamp = item["timestamp"].toVariant().toLongLong();

        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);

        resultMsg += QString("• %1 | %2 | %3\n")
                         .arg(dateTime.toString("MM-dd hh:mm"))
                         .arg(deviceId)
                         .arg(logCode);

        count++;
    }

    ChatMessage botMsg = {"bot", resultMsg, getCurrentTime()};
    addMessage(botMsg);
}

void ChatBotWidget::controlMqttDevice(const QString &deviceName, const QString &action)
{
    if (!m_mqttClient || m_mqttClient->state() != QMqttClient::Connected)
    {
        ChatMessage errorMsg = {
                                "bot",
                                "⚠️ MQTT 서버에 연결되지 않았습니다. 잠시 후 다시 시도해주세요.",
                                getCurrentTime()};
        addMessage(errorMsg);
        return;
    }

    QString topic = getTopicForDevice(deviceName);
    if (topic.isEmpty())
    {
        // 컨베이어2인지 확인
        QString device = deviceName.toLower();
        if (device.contains("컨베이어2") || device.contains("컨베이어 2") || device.contains("컨베이어02"))
        {
            ChatMessage errorMsg = {
                                    "bot",
                                    "컨베이어2는 지원하지 않는 기능입니다.",
                                    getCurrentTime()};
            addMessage(errorMsg);
        }
        else
        {
            ChatMessage errorMsg = {
                                    "bot",
                                    QString("⚠️ 알 수 없는 장비입니다: %1").arg(deviceName),
                                    getCurrentTime()};
            addMessage(errorMsg);
        }
        return;
    }

    QString command = (action == "켜" || action == "시작" || action == "가동" || action == "on") ? "on" : "off";

    m_mqttClient->publish(QMqttTopicName(topic), command.toUtf8());

    QString actionText = (command == "on") ? "켜짐" : "꺼짐";
    ChatMessage successMsg = {
                              "bot",
                              QString("✅ %1이(가) %2되었습니다.").arg(deviceName, actionText),
                              getCurrentTime()};
    addMessage(successMsg);

    qDebug() << "MQTT 명령 전송:" << topic << "->" << command;
}

QString ChatBotWidget::getTopicForDevice(const QString &deviceName)
{
    // 장비명을 토픽으로 매핑
    QString device = deviceName.toLower();

    if (device.contains("피더2") || device.contains("피더 2") || device.contains("피더02") || device == "feeder_02")
    {
        return "feeder_02/cmd";
    }
    else if (device.contains("컨베이어2") || device.contains("컨베이어 2") || device.contains("컨베이어02") || device == "conveyor_02")
    {
        return ""; // 컨베이어2는 지원하지 않음
    }
    else if (device.contains("컨베이어3") || device.contains("컨베이어 3") || device.contains("컨베이어03") || device == "conveyor_03")
    {
        return "conveyor_03/cmd";
    }

    return ""; // 알 수 없는 장비
}

QString ChatBotWidget::getKoreanToolName(const QString &englishToolName)
{
    static QHash<QString, QString> toolNameMap = {
                                                  {"db_find", "데이터베이스 조회"},
                                                  {"db_count", "데이터 개수 조회"},
                                                  {"db_aggregate", "데이터 집계 분석"},
                                                  {"db_info", "데이터베이스 정보"},
                                                  {"device_control", "장비 제어"},
                                                  {"mqtt_device_control", "MQTT 장비 제어"},
                                                  {"conveyor_failure_stats", "컨베이어 장애 통계"},
                                                  {"device_statistics", "장비 통계"}};

    return toolNameMap.value(englishToolName, englishToolName);
}

// 드래그 기능 구현
void ChatBotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        QPoint localPos = event->pos();

        // 크기 조절 영역 확인
        if (isInResizeArea(localPos))
        {
            m_resizing = true;
            m_resizeStartX = localPos.x();
            m_initialWidth = width();
            event->accept();
            return;
        }

        // 기존 드래그 로직
        if (m_headerWidget)
        {
            QPoint headerPos = m_headerWidget->mapToGlobal(QPoint(0, 0));
            QRect headerRect(headerPos, m_headerWidget->size());

            if (headerRect.contains(event->globalPosition().toPoint()))
            {
                m_dragging = true;
                m_dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void ChatBotWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 커서 변경
    if (!m_dragging && !m_resizing)
    {
        if (isInResizeArea(event->pos()))
        {
            setCursor(Qt::SizeHorCursor);
        }
        else if (m_headerWidget)
        {
            QPoint headerPos = m_headerWidget->mapToGlobal(QPoint(0, 0));
            QRect headerRect(headerPos, m_headerWidget->size());
            if (headerRect.contains(event->globalPosition().toPoint()))
            {
                setCursor(Qt::SizeAllCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
    }

    // 크기 조절 중
    if (event->buttons() & Qt::LeftButton && m_resizing)
    {
        int deltaX = event->pos().x() - m_resizeStartX;
        SizeMode newMode = calculateNewSizeMode(deltaX);

        if (newMode != m_currentSizeMode)
        {
            updateSizeMode(newMode);
        }

        event->accept();
        return;
    }

    // 기존 드래그 로직
    if (event->buttons() & Qt::LeftButton && m_dragging)
    {
        move(event->globalPosition().toPoint() - m_dragStartPosition);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void ChatBotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = false;
        m_resizing = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ChatBotWidget::onToolDiscoveryCompleted(const ConversationContext &context)
{
    // 도구 발견 완료 시 처리
    qDebug() << "Tool discovery completed for context";

    // 필요한 경우 UI 업데이트 로직 추가
    // 예: 로딩 상태 해제, 발견된 도구 정보 표시 등
}

void ChatBotWidget::onToolExecutionCompleted(const QString &result)
{
    // 도구 실행 완료 시 처리
    qDebug() << "Tool execution completed with result:" << result;

    // 결과를 채팅에 추가
    ChatMessage botMessage;
    botMessage.sender = "bot";
    botMessage.content = result;
    botMessage.time = getCurrentTime();
    addMessage(botMessage);

    waitingForResponse = false;
}
