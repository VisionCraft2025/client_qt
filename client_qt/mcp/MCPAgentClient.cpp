#include "MCPAgentClient.h"
#include "ToolExamples.h"
#include "DataFormatter.h"
#include "../chatbot_widget.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

MCPAgentClient::MCPAgentClient(const QString& mcpServerUrl, 
                               const QString& geminiApiKey, 
                               QObject* parent)
    : QObject(parent)
    , m_mcpServerUrl(mcpServerUrl)
    , m_geminiApiKey(geminiApiKey)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_mqttClient(nullptr)
    , m_loadingTimer(new QTimer(this))
    , m_loadingDots(0)
{
    if (m_mcpServerUrl.endsWith('/')) {
        m_mcpServerUrl.chop(1);
    }
    
    // 로딩 애니메이션 타이머 설정
    m_loadingTimer->setInterval(500);
    connect(m_loadingTimer, &QTimer::timeout, this, &MCPAgentClient::updateLoadingAnimation);
    
    initializeToolExamples();
}

MCPAgentClient::~MCPAgentClient() = default;

void MCPAgentClient::setPipelineState(PipelineState newState) {
    if (m_pipelineState != newState) {
        m_pipelineState = newState;
        emit pipelineStateChanged(newState);
        
        QString stateStr;
        switch(newState) {
            case PipelineState::IDLE: stateStr = "대기 중"; break;
            case PipelineState::DISCOVERING_TOOL: stateStr = "🔍 도구 분석 중"; break;
            case PipelineState::PREPARING_EXECUTION: stateStr = "⚡ 실행 준비 중"; break;
            case PipelineState::EXECUTING_TOOL: stateStr = "🔧 도구 실행 중"; break;
            case PipelineState::FORMATTING_RESULT: stateStr = "📋 결과 정리 중"; break;
        }
        emit logMessage(stateStr, 0);
    }
}

void MCPAgentClient::setMqttClient(QMqttClient* mqttClient) {
    m_mqttClient = mqttClient;
}

void MCPAgentClient::executeUnifiedPipeline(const QString& userQuery) {
    emit logMessage("🚀 통합 파이프라인 시작", 0);
    
    // 새로운 컨텍스트 생성 또는 기존 컨텍스트 업데이트
    if (!m_currentContext) {
        m_currentContext = std::make_unique<ConversationContext>();
    }
    m_currentContext->userQuery = userQuery;
    m_currentContext->conversationHistory.append({"user", userQuery});
    
    // 도구 목록 확인
    if (!m_toolsCache.has_value()) {
        connect(this, &MCPAgentClient::toolsFetched, this, [this]() {
            disconnect(this, &MCPAgentClient::toolsFetched, nullptr, nullptr);
            
            if (!m_toolsCache.has_value() || m_toolsCache->isEmpty()) {
                emit errorOccurred("도구 목록을 가져올 수 없습니다.");
                setPipelineState(PipelineState::IDLE);
                return;
            }
            
            // 도구 목록 로드 후 통합 프롬프트 실행
            QString prompt = generateUnifiedPrompt(m_currentContext->userQuery);
            setPipelineState(PipelineState::DISCOVERING_TOOL);
            sendGeminiRequest(prompt);
        });
        
        fetchTools();
    } else {
        // 통합 프롬프트 즉시 실행
        QString prompt = generateUnifiedPrompt(userQuery);
        setPipelineState(PipelineState::DISCOVERING_TOOL);
        sendGeminiRequest(prompt);
    }
}

QString MCPAgentClient::generateUnifiedPrompt(const QString& userQuery) {
    // 도구 정보 구성
    QStringList toolsInfo;
    for (const auto& tool : m_toolsCache.value()) {
        QString toolStr = QString("• %1: %2").arg(tool.name, tool.description);
        if (!tool.examples.isEmpty()) {
            QStringList examples = tool.examples.mid(0, 3);
            toolStr += QString("\n  예시: %1").arg(examples.join(", "));
        }
        toolsInfo.append(toolStr);
    }
    
    // 현재 날짜 정보
    QDate currentDate = QDate::currentDate();
    QString dateInfo = QString("현재 날짜: %1년 %2월 %3일")
        .arg(currentDate.year())
        .arg(currentDate.month())
        .arg(currentDate.day());
    
    // MQTT 제어 정보
    QString mqttDevices = R"(
MQTT 제어 가능 기기:
- 피더2: feeder_02/cmd
- 컨베이어2: factory/conveyor_02/cmd  
- 컨베이어3: conveyor_03/cmd
- 로봇팔: robot_arm_01/cmd
명령어: "on" 또는 "off"
)";

    QString prompt = QString(R"(당신은 스마트 팩토리 AI 어시스턴트입니다.
사용자 요청을 분석하고 적절한 응답을 생성해주세요.

사용자 요청: "%1"

사용 가능한 도구:
%2

%3

%4

디바이스 매핑:
- 컨베이어1, 컨베이어 1번 → conveyor_01
- 컨베이어2, 컨베이어 2번 → conveyor_02  
- 컨베이어3, 컨베이어 3번 → conveyor_03
- 피더1, 피더 1번 → feeder_01
- 피더2, 피더 2번, 피더02 → feeder_02
- 로봇팔, 로봇암, 로봇 → robot_arm_01

응답 형식 (JSON):
{
    "analysis": "요청 분석 내용",
    "requiresTool": true/false,
    "selectedTool": "도구명 또는 null",
    "toolParameters": { 매개변수 객체 } 또는 null,
    "userMessage": "사용자에게 보여줄 메시지"
}

특별 지침:
1. 기능 소개 요청 시: requiresTool=false, 다음 메시지 사용:
   "스마트 팩토리 AI 어시스턴트입니다. 장비 제어, 실시간 모니터링, 데이터 조회 및 분석, 통계 정보 제공 등을 도와드립니다."
2. MQTT 장비 제어: 피더2, 컨베이어2/3, 로봇팔은 mqtt_device_control 사용
3. HTTP 장비 제어: 피더1, 컨베이어1은 device_control 사용
4. 데이터 조회: db_find 사용
5. 통계/분석: db_aggregate 또는 전용 통계 도구 사용

JSON만 응답하고 다른 설명은 하지 마세요.)").arg(userQuery)
          .arg(toolsInfo.join("\n"))
          .arg(dateInfo)
          .arg(mqttDevices);
    
    return prompt;
}

void MCPAgentClient::sendGeminiRequest(const QString& prompt) {
    // 로딩 애니메이션 시작
    m_loadingDots = 0;
    m_loadingTimer->start();
    
    QString url = QString("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%1")
        .arg(m_geminiApiKey);
    
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject requestBody;
    QJsonArray contents;
    QJsonObject content;
    QJsonArray parts;
    QJsonObject part;
    part["text"] = prompt;
    parts.append(part);
    content["parts"] = parts;
    contents.append(content);
    requestBody["contents"] = contents;
    
    QJsonDocument doc(requestBody);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &MCPAgentClient::handleGeminiReply);
}

void MCPAgentClient::handleGeminiReply() {
    m_loadingTimer->stop();
    
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("Gemini API 호출 실패: %1").arg(reply->errorString()));
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        emit errorOccurred("잘못된 Gemini API 응답");
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    // 응답 파싱
    QJsonObject root = doc.object();
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) {
        emit errorOccurred("Gemini API 응답에 후보가 없습니다");
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QJsonObject candidate = candidates[0].toObject();
    QJsonObject content = candidate["content"].toObject();
    QJsonArray parts = content["parts"].toArray();
    
    if (parts.isEmpty()) {
        emit errorOccurred("Gemini API 응답에 컨텐츠가 없습니다");
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QString responseText = parts[0].toObject()["text"].toString();
    
    // 통합 응답 파싱
    UnifiedResponse response = parseUnifiedResponse(responseText);
    
    // 컨텍스트 업데이트
    m_currentContext->selectedTool = response.selectedTool;
    m_currentContext->toolParameters = response.toolParameters;
    
    // 사용자 메시지 즉시 출력
    emit logMessage(response.userMessage, 0);
    
    if (response.requiresToolExecution && response.selectedTool.has_value()) {
        // 도구 실행 필요
        setPipelineState(PipelineState::EXECUTING_TOOL);
        executeToolWithParameters(response.selectedTool.value(), response.toolParameters.value_or(QJsonObject()));
    } else {
        // 도구 실행 불필요 - 바로 완료
        m_currentContext->conversationHistory.append({"assistant", response.userMessage});
        emit pipelineCompleted(response.userMessage);
        setPipelineState(PipelineState::IDLE);
    }
}

MCPAgentClient::UnifiedResponse MCPAgentClient::parseUnifiedResponse(const QString& responseText) {
    UnifiedResponse result;
    
    // JSON 파싱
    QString jsonText = responseText.trimmed();
    
    // 코드 블록 제거
    if (jsonText.contains("```json")) {
        int start = jsonText.indexOf("```json") + 7;
        int end = jsonText.indexOf("```", start);
        if (end != -1) {
            jsonText = jsonText.mid(start, end - start).trimmed();
        }
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON 파싱 실패:" << parseError.errorString();
        result.userMessage = "응답 처리 중 오류가 발생했습니다.";
        result.requiresToolExecution = false;
        return result;
    }
    
    QJsonObject obj = doc.object();
    
    result.requiresToolExecution = obj["requiresTool"].toBool();
    result.userMessage = obj["userMessage"].toString();
    
    if (result.requiresToolExecution) {
        QString toolName = obj["selectedTool"].toString();
        if (!toolName.isEmpty() && toolName != "null") {
            result.selectedTool = toolName;
            result.toolParameters = obj["toolParameters"].toObject();
            
            QString koreanToolName = getKoreanToolName(toolName);
            emit logMessage(QString("🛠️ %1 도구 선택됨").arg(koreanToolName), 0);
        }
    }
    
    return result;
}

void MCPAgentClient::executeToolWithParameters(const QString& toolName, const QJsonObject& parameters) {
    // mqtt_device_control 도구 처리
    if (toolName == "mqtt_device_control") {
        QString topic = parameters["topic"].toString();
        QString command = parameters["command"].toString();
        
        if (m_mqttClient && m_mqttClient->state() == QMqttClient::Connected) {
            // MQTT 명령 전송
            m_mqttClient->publish(QMqttTopicName(topic), command.toUtf8());
            
            // 기기 ID 추출
            QString deviceId;
            if (topic == "feeder_02/cmd") deviceId = "feeder_02";
            else if (topic == "factory/conveyor_02/cmd") deviceId = "conveyor_02";
            else if (topic == "conveyor_03/cmd") deviceId = "conveyor_03";
            else if (topic == "robot_arm_01/cmd") deviceId = "robot_arm_01";
            
            // ChatBotWidget에 제어 대기 상태 설정
            if (auto* chatBot = qobject_cast<ChatBotWidget*>(parent())) {
                // 대기 중인 제어 명령 저장
                chatBot->m_pendingControls[deviceId] = command;
                
                // 5초 타임아웃 타이머 설정
                QTimer* timer = new QTimer(chatBot);
                timer->setSingleShot(true);
                connect(timer, &QTimer::timeout, chatBot, [chatBot, deviceId]() {
                    chatBot->handleMqttControlTimeout(deviceId);
                });
                timer->start(5000);
                chatBot->m_controlTimers[deviceId] = timer;
                
                emit logMessage("🔧 기기 제어 명령을 전송했습니다. 응답을 기다리는 중...", 0);
            }
        } else {
            emit errorOccurred("MQTT 연결이 되어있지 않습니다.");
        }
        
        setPipelineState(PipelineState::IDLE);
        return;
    }

    if (toolName == "conveyor_failure_stats") {
        // 불량률 통계 요청
        if (m_mqttClient && m_mqttClient->state() == QMqttClient::Connected) {
            m_mqttClient->publish(QMqttTopicName("factory/conveyor_01/log/request"), "{}");
            emit logMessage("불량률 통계를 요청했습니다. 잠시만 기다려주세요.", 0);
        }
        setPipelineState(PipelineState::IDLE);
        return;
    }
    else if (toolName == "device_statistics") {
        // 장비 통계 요청
        QString deviceId = parameters["device_id"].toString();
        QJsonObject timeRange = parameters["time_range"].toObject();
        
        QJsonObject request;
        request["device_id"] = deviceId;
        request["time_range"] = timeRange;
        
        QJsonDocument doc(request);
        if (m_mqttClient && m_mqttClient->state() == QMqttClient::Connected) {
            m_mqttClient->publish(QMqttTopicName("factory/statistics"), doc.toJson());
            emit logMessage("장비 통계를 요청했습니다. 잠시만 기다려주세요.", 0);
        }
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    // 나머지는 기존 MCP 서버로 처리
    QJsonObject payload;
    payload["name"] = toolName;
    payload["arguments"] = parameters;
    
    QNetworkRequest request(QUrl(m_mcpServerUrl + "/tools/call"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(30000);
    
    QJsonDocument doc(payload);
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &MCPAgentClient::handleExecuteToolReply);
}

void MCPAgentClient::handleExecuteToolReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("도구 실행 중 오류 발생: %1").arg(reply->errorString());
        emit errorOccurred(errorMsg);
        emit pipelineCompleted(errorMsg);
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        QString errorMsg = "잘못된 도구 실행 응답";
        emit errorOccurred(errorMsg);
        emit pipelineCompleted(errorMsg);
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QJsonObject result = doc.object();
    bool isError = result["isError"].toBool(false);
    QJsonArray contentArray = result["content"].toArray();
    
    if (contentArray.isEmpty()) {
        QString errorMsg = "도구 실행 결과가 비어있습니다";
        emit errorOccurred(errorMsg);
        emit pipelineCompleted(errorMsg);
        setPipelineState(PipelineState::IDLE);
        return;
    }
    
    QString resultText = contentArray[0].toObject()["text"].toString();
    
    // 결과 포맷팅
    setPipelineState(PipelineState::FORMATTING_RESULT);
    QString formattedResult = resultText;
    
    if (m_currentContext && m_currentContext->selectedTool.has_value()) {
        formattedResult = DataFormatter::formatExecutionResult(
            m_currentContext->selectedTool.value(), resultText);
    }
    
    // 대화 기록 업데이트
    if (m_currentContext) {
        m_currentContext->conversationHistory.append({"assistant", formattedResult});
        m_currentContext->executionResult = formattedResult;
    }
    
    // 완료
    emit pipelineCompleted(formattedResult);
    setPipelineState(PipelineState::IDLE);
}

void MCPAgentClient::initializeToolExamples() {
    m_toolExamples = ToolExamples::getToolExamples();
}

void MCPAgentClient::fetchTools(bool forceRefresh) {
    // 캐시 확인
    if (!forceRefresh && m_toolsCache.has_value() && m_lastCacheUpdate.has_value()) {
        auto cacheAge = m_lastCacheUpdate->secsTo(QDateTime::currentDateTime());
        if (cacheAge < m_cacheDurationSeconds) {
            emit logMessage("캐시된 도구 목록 사용", 0);
            emit toolsFetched(m_toolsCache.value());
            return;
        }
    }
    
    QNetworkRequest request(QUrl(m_mcpServerUrl + "/tools"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(10000);
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MCPAgentClient::handleFetchToolsReply);
}

void MCPAgentClient::handleFetchToolsReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("도구 목록 가져오기 실패: %1").arg(reply->errorString()));
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        emit errorOccurred("잘못된 JSON 응답");
        return;
    }
    
    QJsonObject root = doc.object();
    QJsonArray toolsArray = root["tools"].toArray();
    
    QVector<ToolInfo> tools;
    for (const QJsonValue& value : toolsArray) {
        QJsonObject toolObj = value.toObject();
        ToolInfo tool;
        tool.name = toolObj["name"].toString();
        tool.description = toolObj["description"].toString();
        tool.inputSchema = toolObj["inputSchema"].toObject();
        
        if (m_toolExamples.contains(tool.name)) {
            tool.examples = m_toolExamples[tool.name];
        }
        
        tools.append(tool);
    }
    
    // 캐시 업데이트
    m_toolsCache = tools;
    m_lastCacheUpdate = QDateTime::currentDateTime();
    
    emit logMessage(QString("도구 목록 가져오기 성공: %1개 도구").arg(tools.size()), 0);
    emit toolsFetched(tools);
}

void MCPAgentClient::clearContext() {
    m_currentContext.reset();
    emit logMessage("🔄 대화 맥락이 초기화되었습니다.", 0);
}

QString MCPAgentClient::getKoreanToolName(const QString& englishToolName) {
    static QHash<QString, QString> toolNameMap = {
        {"db_find", "데이터베이스 조회"},
        {"db_count", "데이터 개수 조회"},
        {"db_aggregate", "데이터 집계 분석"},
        {"db_info", "데이터베이스 정보"},
        {"device_control", "장비 제어"},
        {"mqtt_device_control", "MQTT 장비 제어"},
        {"conveyor_failure_stats", "컨베이어 장애 통계"},
        {"device_statistics", "장비 통계"}
    };
    
    return toolNameMap.value(englishToolName, englishToolName);
}

void MCPAgentClient::updateLoadingAnimation() {
    m_loadingDots = (m_loadingDots % 3) + 1;
    QString dots = QString(".").repeated(m_loadingDots);
    emit logMessage(QString("🤔 생각 중%1").arg(dots), 0);
}