#include "chatbot_widget.h"
#include "ai_command.h"
#include "mcp/MCPAgentClient.h"

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

ChatBotWidget::ChatBotWidget(QWidget *parent)
    : QWidget(parent), waitingForResponse(false)
{
    setFixedSize(504, 760); // 720 → 760으로 높이 더 증가 (40px 추가)
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
    outerFrame->setFixedSize(504, 760); // 720 → 760으로 높이 더 증가 (40px 추가)

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
    innerFrame->setMinimumSize(504, 685); // 675 → 685로 높이 증가 (10px 추가)

    // 헤더
    QWidget *header = new QWidget(outerFrame);
    header->setMinimumHeight(75);         // 85 → 75로 높이 줄임 (10px 감소)
    header->setMaximumHeight(75);         // 최대 높이도 감소
    header->setCursor(Qt::SizeAllCursor); // 드래그 가능 커서 설정
    m_headerWidget = header;              // 드래그를 위해 헤더 위젯 저장
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 12, 14, 12); // 상하 패딩 줄임 (18 → 12)

    QVBoxLayout *outerLayout = new QVBoxLayout(outerFrame);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);
    outerLayout->addWidget(innerFrame);

    QLabel *title = new QLabel("<b>🤖 VisionCraft AI</b>");
    title->setStyleSheet("color: white; font-size: 17px; padding: 3px 0px; line-height: 1.2;"); // 패딩 줄임 (6px → 3px), 라인높이 조정
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setWordWrap(false);
    title->setTextInteractionFlags(Qt::NoTextInteraction);
    title->setMinimumHeight(24); // 최소 높이 줄임 (32 → 24)

    QLabel *subtitle = new QLabel("스마트 팩토리 CCTV AI 어시스턴트");
    subtitle->setStyleSheet("color: white; font-size: 12px; padding: 4px 0px; line-height: 1.2;"); // 패딩 줄임 (10px → 4px), 라인높이 조정
    subtitle->setMinimumHeight(20);                                                                // 최소 높이 줄임 (35 → 20)

    QVBoxLayout *titleBox = new QVBoxLayout;
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    titleBox->setSpacing(2);                  // 간격 줄임 (6 → 2)
    titleBox->setContentsMargins(0, 2, 0, 2); // 상하 여백 줄임 (6 → 2)

    closeButton = new QPushButton("\u2715");
    closeButton->setStyleSheet("background: transparent; color: white; border: none; font-size: 16px;");
    closeButton->setFixedSize(28, 28);
    connect(closeButton, &QPushButton::clicked, this, &ChatBotWidget::hide);

    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    // 내부 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(innerFrame);
    mainLayout->setContentsMargins(14, 14, 14, 22); // 하단 여백 더 증가 (18 → 22)
    mainLayout->setSpacing(15);                     // 간격 더 증가 (12 → 15)

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
        "기능 소개",
        "컨베이어1 정보",
        "불량률 통계",
        "피더2 켜줘"};

    for (const QString &text : quickTexts)
    {
        QPushButton *quick = new QPushButton(text);
        quick->setStyleSheet("font-size: 13px; padding: 7px; background-color: #f3f4f6; border-radius: 7px;");
        connect(quick, &QPushButton::clicked, this, [=]()
                {
            QString command = text;
            if (text == "기능 소개") {
                command = "어떤 기능이 있어?";
            } else if (text == "컨베이어1 정보") {
                command = "컨베이어1 오늘 정보 보여줘";
            } else if (text == "불량률 통계") {
                command = "컨베이어1 불량률 알려줘";
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
            padding: 15px;
            font-size: 14px;
            min-height: 22px;
        )");

    sendButton = new QPushButton("전송");
    sendButton->setFixedSize(55, 52); // 크기 더 증가 (50x42 → 55x52)
    sendButton->setStyleSheet(R"(
            background-color: #fb923c;
            color: white;
            border: none;
            border-radius: 12px;
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

    // MQTT 클라이언트 초기화
    initializeMqttClient();
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
    // QTextEdit 대신 QLabel 사용 (높이 계산이 더 정확함)
    QLabel *msgLabel = new QLabel();
    msgLabel->setWordWrap(true);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // HTML 형식으로 변환
    QString formattedContent = msg.content;
    formattedContent.replace("\n", "<br>");

    // ### 헤더 처리 (### 제거하고 볼드로 변경)
    QRegularExpression headerRegex(R"(###\s*(.*?)<br>)");
    formattedContent.replace(headerRegex, "<b>\\1</b><br>");

    // 리스트 항목 처리 (* 제거)
    QRegularExpression listRegex(R"(\*\s+(.*?)<br>)");
    formattedContent.replace(listRegex, "\\1<br>");

    // 볼드 처리 (**text**)
    QRegularExpression boldRegex(R"(\*\*(.*?)\*\*)");
    formattedContent.replace(boldRegex, "<b>\\1</b>");

    // 코드 블록 처리
    QRegularExpression codeRegex(R"(```(.*?)```)");
    formattedContent.replace(codeRegex, "<pre style='background-color: #f0f0f0; padding: 5px;'>\\1</pre>");

    msgLabel->setText(formattedContent);

    // 동적 크기 계산 - 현재 위젯 크기를 기반으로 계산
    int containerWidth = this->width() - 48; // 양쪽 마진 (24px * 2)
    int minWidth = containerWidth * 0.35;    // 컨테이너 너비의 35%로 증가 (기존 23%)
    int maxWidth = containerWidth * 0.93;    // 컨테이너 너비의 93%

    // HTML 태그 제거한 순수 텍스트로 길이 계산
    QString plainText = msg.content;
    plainText.remove(QRegularExpression("<[^>]*>"));

    // 텍스트 길이에 따른 폭 계산
    QFontMetrics fm(msgLabel->font());

    // 실제 렌더링될 텍스트 기준으로 계산 (HTML 포함)
    QTextDocument textDoc;
    textDoc.setHtml(formattedContent);
    textDoc.setDefaultFont(msgLabel->font());
    textDoc.setTextWidth(maxWidth - 28);

    int textWidth = textDoc.idealWidth() + 50; // 패딩 고려

    int finalWidth = qBound(minWidth, textWidth, maxWidth);

    // 짧은 텍스트는 더 작게, 긴 텍스트는 최대 폭 사용
    if (plainText.length() > 80 || msg.content.contains("\n"))
    {
        finalWidth = maxWidth;
    }
    else if (plainText.length() < 20)
    {
        // 매우 짧은 텍스트는 더 작은 폭 사용
        int smallerWidth = static_cast<int>(containerWidth * 0.6);
        finalWidth = qMin(finalWidth, smallerWidth);
    }

    msgLabel->setMinimumWidth(finalWidth);
    msgLabel->setMaximumWidth(finalWidth);

    // 스타일 적용
    if (msg.sender == "bot")
    {
        msgLabel->setStyleSheet(QString(R"(
            QLabel {
                background-color: #f3f4f6; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Hanwha Gothic", "Malgun Gothic", sans-serif;
                font-size: 14px;
                color: black;
            }
        )"));
    }
    else
    {
        msgLabel->setStyleSheet(QString(R"(
            QLabel {
                background-color: #fb923c; 
                color: white; 
                padding: 14px; 
                border-radius: 14px;
                font-family: "Hanwha Gothic", "Malgun Gothic", sans-serif;
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

    if (msg.sender == "bot")
    {
        bubbleLayout->setAlignment(Qt::AlignLeft);
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignLeft);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignLeft);
    }
    else
    {
        bubbleLayout->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(msgLabel, 0, Qt::AlignRight);
        timeLabel->setAlignment(Qt::AlignRight);
        bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);
    }

    QWidget *bubble = new QWidget;
    bubble->setLayout(bubbleLayout);
    messageLayout->addWidget(bubble);

    // 자동 스크롤
    QTimer::singleShot(50, this, [=]()
                       { scrollArea->verticalScrollBar()->setValue(
                             scrollArea->verticalScrollBar()->maximum()); });
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
        "factory/conveyor_02/status", 
        "conveyor_03/status",
        "robot_arm_01/status"
    };

    for (const QString& topic : statusTopics) {
        auto sub = m_mqttClient->subscribe(topic);
        if (sub) {
            connect(sub, &QMqttSubscription::messageReceived,
                    this, &ChatBotWidget::onMqttStatusReceived);
            qDebug() << "ChatBot subscribed to:" << topic;
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
    if (topic == "feeder_02/status") deviceId = "feeder_02";
    else if (topic == "factory/conveyor_02/status") deviceId = "conveyor_02";
    else if (topic == "conveyor_03/status") deviceId = "conveyor_03";
    else if (topic == "robot_arm_01/status") deviceId = "robot_arm_01";
    
    // 대기 중인 제어 명령이 있는지 확인
    if (m_pendingControls.contains(deviceId)) {
        QString expectedCommand = m_pendingControls[deviceId];
        
        if (payload == expectedCommand) {
            // 타이머 정지
            if (m_controlTimers.contains(deviceId)) {
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
                QString("✅ %1이(가) %2.").arg(deviceKorean, actionText),
                getCurrentTime()
            };
            addMessage(successMsg);
        }
    }
}

// 타임아웃 처리
void ChatBotWidget::handleMqttControlTimeout(const QString& deviceId)
{
    if (m_pendingControls.contains(deviceId)) {
        m_pendingControls.remove(deviceId);
        
        QString deviceKorean = getDeviceKoreanName(deviceId);
        ChatMessage errorMsg = {
            "bot",
            QString("⚠️ %1 제어 응답 시간이 초과되었습니다. 기기 상태를 확인해주세요.").arg(deviceKorean),
            getCurrentTime()
        };
        addMessage(errorMsg);
    }
    
    // 타이머 제거
    if (m_controlTimers.contains(deviceId)) {
        m_controlTimers[deviceId]->deleteLater();
        m_controlTimers.remove(deviceId);
    }
}

// 기기 이름 한글 변환 헬퍼
QString ChatBotWidget::getDeviceKoreanName(const QString& deviceId)
{
    if (deviceId == "feeder_02") return "피더 2번";
    else if (deviceId == "conveyor_02") return "컨베이어 2번";
    else if (deviceId == "conveyor_03") return "컨베이어 3번";
    else if (deviceId == "robot_arm_01") return "로봇팔";
    return deviceId;
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
        if (mcpClient) {
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
            if (mcpClient) {
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
        ChatMessage errorMsg = {
            "bot",
            QString("⚠️ 알 수 없는 장비입니다: %1").arg(deviceName),
            getCurrentTime()};
        addMessage(errorMsg);
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
        return "factory/conveyor_02/cmd";
    }
    else if (device.contains("컨베이어3") || device.contains("컨베이어 3") || device.contains("컨베이어03") || device == "conveyor_03")
    {
        return "conveyor_03/cmd";
    }
    else if (device.contains("로봇팔") || device.contains("로봇암") || device.contains("로봇") || device == "robot_arm_01")
    {
        return "robot_arm_01/cmd";
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
    if (event->button() == Qt::LeftButton && m_headerWidget)
    {
        // 헤더 영역에서 클릭했는지 확인
        QPoint headerPos = m_headerWidget->mapToGlobal(QPoint(0, 0));
        QRect headerRect(headerPos, m_headerWidget->size());

        if (headerRect.contains(event->globalPos()))
        {
            m_dragging = true;
            m_dragStartPosition = event->globalPos() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void ChatBotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && m_dragging)
    {
        move(event->globalPos() - m_dragStartPosition);
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
