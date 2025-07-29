#include "MCPAgentClient.h"
#include "ToolExamples.h"
#include "DataFormatter.h"
#include "chatbot_widget.h"
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
- 컨베이어3: conveyor_03/cmd
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
- 컨베이어3, 컨베이어 3번 → conveyor_03
- 피더1, 피더 1번 → feeder_01
- 피더2, 피더 2번, 피더02 → feeder_02

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
2. MQTT 장비 제어: 피더2, 컨베이어3은 mqtt_device_control 사용
3. HTTP 장비 제어: 피더1, 컨베이어1은 device_control 사용
4. 컨베이어2 관련 요청: requiresTool=false, "컨베이어2는 지원하지 않는 기능입니다." 메시지 사용
5. 데이터 조회: db_find 사용
6. 통계/분석: 다음 규칙 적용
   - 속도, 평균, 성능, 운영 통계 → device_statistics 사용 (캐시된 데이터 조회)
   - 불량률, 양품, 불량품, 생산량 → conveyor_failure_stats 사용 (캐시된 데이터 조회)
   - 일반 로그, 기록 → db_find 사용

도구 매개변수 가이드:
- mqtt_device_control: {"topic": "토픽명", "command": "명령어"}
  예: {"topic": "feeder_02/cmd", "command": "on"}
- device_statistics: {"device_id": "장비ID"} (예: conveyor_01, feeder_01)
- conveyor_failure_stats: {"device_id": "컨베이어ID"} (선택사항, 기본: conveyor_01)

예시:
- "피더2 켜줘" → mqtt_device_control + {"topic": "feeder_02/cmd", "command": "on"}
- "컨베이어3 꺼줘" → mqtt_device_control + {"topic": "conveyor_03/cmd", "command": "off"}
- "컨베이어1 속도 통계" → device_statistics + {"device_id": "conveyor_01"}
- "불량률 알려줘" → conveyor_failure_stats + {"device_id": "conveyor_01"}
- "피더2 성능" → device_statistics + {"device_id": "feeder_02"}

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
        
        // 디버깅 정보 출력
        qDebug() << "=== MQTT 제어 디버깅 ===";
        qDebug() << "도구명:" << toolName;
        qDebug() << "전체 매개변수:" << parameters;
        qDebug() << "추출된 토픽:" << topic;
        qDebug() << "추출된 명령:" << command;
        qDebug() << "MQTT 연결 상태:" << (m_mqttClient ? m_mqttClient->state() : -1);
        
        if (m_mqttClient && m_mqttClient->state() == QMqttClient::Connected) {
            // MQTT 명령 전송
            qDebug() << "MQTT 발행 시도:" << "토픽=" << topic << "페이로드=" << command;
            m_mqttClient->publish(QMqttTopicName(topic), command.toUtf8());
            qDebug() << "MQTT 발행 완료";
            
            // 기기 ID 추출
            QString deviceId;
            if (topic == "feeder_02/cmd") deviceId = "feeder_02";
            else if (topic == "conveyor_03/cmd") deviceId = "conveyor_03";
            else {
                emit errorOccurred("지원하지 않는 기기입니다.");
                setPipelineState(PipelineState::IDLE);
                return;
            }
            
            // ChatBotWidget에 제어 대기 상태 설정
            if (auto* chatBot = qobject_cast<ChatBotWidget*>(parent())) {
                // 현재 기기 상태 확인
                QString currentState = chatBot->m_deviceStates.value(deviceId, "off");
                QString deviceKorean = (deviceId == "feeder_02") ? "피더 2번" : "컨베이어 3번";
                
                // 현재 상태와 요청된 명령이 같은지 확인
                if (currentState == command) {
                    QString actionText = (command == "on") ? "이미 켜져있습니다" : "이미 꺼져있습니다";
                    emit logMessage(QString("ℹ️ %1은(는) %2.").arg(deviceKorean, actionText), 0);
                    
                    // 파이프라인 완료
                    emit pipelineCompleted(QString("ℹ️ %1은(는) %2.").arg(deviceKorean, actionText));
                    setPipelineState(PipelineState::IDLE);
                    return;
                }
                
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
        // 캐시된 불량률 통계 반환
        QString deviceId = parameters.value("device_id").toString();
        if (deviceId.isEmpty()) {
            deviceId = "conveyor_01"; // 기본값
        } else {
            deviceId = normalizeDeviceId(deviceId); // 디바이스명 정규화
            
            // 컨베이어02 요청 확인
            if (deviceId == "UNSUPPORTED_CONVEYOR_02") {
                QString result = "컨베이어2는 지원하지 않는 기능입니다.";
                if (m_currentContext) {
                    m_currentContext->conversationHistory.append({"assistant", result});
                    m_currentContext->executionResult = result;
                }
                emit pipelineCompleted(result);
                setPipelineState(PipelineState::IDLE);
                return;
            }
            
            // 피더1 요청 확인
            if (deviceId == "UNSUPPORTED_FEEDER_01") {
                QString result = "피더1은 존재하지 않습니다. 피더2만 사용 가능합니다.";
                if (m_currentContext) {
                    m_currentContext->conversationHistory.append({"assistant", result});
                    m_currentContext->executionResult = result;
                }
                emit pipelineCompleted(result);
                setPipelineState(PipelineState::IDLE);
                return;
            }
        }
        QString result = getCachedFailureStats(deviceId);
        
        // 결과를 즉시 반환
        if (m_currentContext) {
            m_currentContext->conversationHistory.append({"assistant", result});
            m_currentContext->executionResult = result;
        }
        emit pipelineCompleted(result);
        setPipelineState(PipelineState::IDLE);
        return;
    }
    else if (toolName == "device_statistics") {
        // 캐시된 장비 통계 반환
        QString deviceId = parameters["device_id"].toString();
        if (!deviceId.isEmpty()) {
            deviceId = normalizeDeviceId(deviceId); // 디바이스명 정규화
            
            // 컨베이어02 요청 확인
            if (deviceId == "UNSUPPORTED_CONVEYOR_02") {
                QString result = "컨베이어2는 지원하지 않는 기능입니다.";
                if (m_currentContext) {
                    m_currentContext->conversationHistory.append({"assistant", result});
                    m_currentContext->executionResult = result;
                }
                emit pipelineCompleted(result);
                setPipelineState(PipelineState::IDLE);
                return;
            }
            
            // 피더1 요청 확인
            if (deviceId == "UNSUPPORTED_FEEDER_01") {
                QString result = "피더1은 존재하지 않습니다. 피더2만 사용 가능합니다.";
                if (m_currentContext) {
                    m_currentContext->conversationHistory.append({"assistant", result});
                    m_currentContext->executionResult = result;
                }
                emit pipelineCompleted(result);
                setPipelineState(PipelineState::IDLE);
                return;
            }
        } else {
            deviceId = "conveyor_01"; // 기본값
        }
        QString result = getCachedStatistics(deviceId);
        
        // 결과를 즉시 반환
        if (m_currentContext) {
            m_currentContext->conversationHistory.append({"assistant", result});
            m_currentContext->executionResult = result;
        }
        emit pipelineCompleted(result);
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

QString MCPAgentClient::normalizeDeviceId(const QString& rawDeviceId) {
    static QHash<QString, QString> deviceMap = {
        // 컨베이어 매핑
        {"컨베이어1", "conveyor_01"}, {"컨베이어 1", "conveyor_01"}, {"컨베이어 1번", "conveyor_01"},
        {"첫 번째 컨베이어", "conveyor_01"}, {"컨베이어01", "conveyor_01"}, {"conveyor1", "conveyor_01"},
        {"컨베이어3", "conveyor_03"}, {"컨베이어 3", "conveyor_03"}, {"컨베이어 3번", "conveyor_03"},
        {"세 번째 컨베이어", "conveyor_03"}, {"컨베이어03", "conveyor_03"}, {"conveyor3", "conveyor_03"},
        
        // 피더 매핑 (피더2만 존재)
        {"피더2", "feeder_02"}, {"피더 2", "feeder_02"}, {"피더 2번", "feeder_02"},
        {"두 번째 피더", "feeder_02"}, {"피더02", "feeder_02"}, {"feeder2", "feeder_02"}
    };
    
    // 정확한 매칭 시도
    if (deviceMap.contains(rawDeviceId)) {
        return deviceMap[rawDeviceId];
    }
    
    // 컨베이어02 요청 감지
    QString lowerInput = rawDeviceId.toLower();
    if (lowerInput.contains("컨베이어2") || lowerInput.contains("컨베이어 2") || 
        lowerInput.contains("컨베이어02") || lowerInput == "conveyor_02") {
        return "UNSUPPORTED_CONVEYOR_02";
    }
    
    // 피더1 요청 감지 (존재하지 않음)
    if (lowerInput.contains("피더1") || lowerInput.contains("피더 1") || 
        lowerInput.contains("피더01") || lowerInput == "feeder_01" ||
        lowerInput.contains("첫 번째 피더")) {
        return "UNSUPPORTED_FEEDER_01";
    }
    
    // 이미 정규화된 ID인 경우 그대로 반환
    if (rawDeviceId.startsWith("conveyor_") || rawDeviceId.startsWith("feeder_")) {
        return rawDeviceId;
    }
    
    // 기본값 반환
    return rawDeviceId;
}

void MCPAgentClient::updateLoadingAnimation() {
    m_loadingDots = (m_loadingDots % 3) + 1;
    QString dots = QString(".").repeated(m_loadingDots);
    emit logMessage(QString("🤔 생각 중%1").arg(dots), 0);
}

void MCPAgentClient::cacheStatisticsData(const QString& deviceId, double avgSpeed, double currentSpeed) {
    StatisticsCache cache;
    cache.deviceId = deviceId;
    cache.averageSpeed = avgSpeed;
    cache.currentSpeed = currentSpeed;
    cache.lastUpdate = QDateTime::currentDateTime();
    cache.isValid = true;
    
    m_statisticsCache[deviceId] = cache;
    qDebug() << "통계 데이터 캐시됨:" << deviceId << "평균:" << avgSpeed << "현재:" << currentSpeed;
}

void MCPAgentClient::cacheFailureStatsData(const QString& deviceId, double failureRate, int total, int pass, int fail) {
    FailureStatsCache cache;
    cache.deviceId = deviceId;
    cache.failureRate = failureRate;
    cache.totalCount = total;
    cache.passCount = pass;
    cache.failCount = fail;
    cache.lastUpdate = QDateTime::currentDateTime();
    cache.isValid = true;
    
    m_failureStatsCache[deviceId] = cache;
    qDebug() << "불량률 데이터 캐시됨:" << deviceId << "불량률:" << failureRate << "%";
}

QString MCPAgentClient::getCachedStatistics(const QString& deviceId) {
    qDebug() << "통계 데이터 조회 요청:" << deviceId;
    qDebug() << "캐시된 통계 키 목록:" << m_statisticsCache.keys();
    
    if (!m_statisticsCache.contains(deviceId) || !m_statisticsCache[deviceId].isValid) {
        qDebug() << "통계 데이터 없음:" << deviceId;
        return QString("❌ %1의 속도 통계 데이터가 없습니다.\n💡 잠시 후 다시 시도해주세요. (MQTT로부터 실시간 데이터 수신 대기 중)").arg(deviceId);
    }
    
    const StatisticsCache& cache = m_statisticsCache[deviceId];
    QString deviceName = deviceId.contains("conveyor") ? "컨베이어" : 
                        deviceId.contains("feeder") ? "피더" : "장비";
    
    QString statsMsg = QString("📊 **%1 속도 통계**\n").arg(deviceName);
    statsMsg += QString("• 장비 ID: %1\n").arg(deviceId);
    statsMsg += QString("• 현재 속도: %1\n").arg(cache.currentSpeed);
    statsMsg += QString("• 평균 속도: %1\n").arg(cache.averageSpeed);
    statsMsg += QString("• 마지막 업데이트: %1").arg(cache.lastUpdate.toString("hh:mm:ss"));
    
    qDebug() << "통계 데이터 반환:" << statsMsg;
    return statsMsg;
}

QString MCPAgentClient::getCachedFailureStats(const QString& deviceId) {
    qDebug() << "불량률 데이터 조회 요청:" << deviceId;
    qDebug() << "캐시된 불량률 키 목록:" << m_failureStatsCache.keys();
    
    if (!m_failureStatsCache.contains(deviceId) || !m_failureStatsCache[deviceId].isValid) {
        qDebug() << "불량률 데이터 없음:" << deviceId;
        return QString("❌ %1의 불량률 데이터가 없습니다.\n💡 잠시 후 다시 시도해주세요. (MQTT로부터 실시간 데이터 수신 대기 중)").arg(deviceId);
    }
    
    const FailureStatsCache& cache = m_failureStatsCache[deviceId];
    QString deviceName = deviceId.contains("conveyor") ? "컨베이어" : "장비";
    
    QString failureMsg = QString("📊 **%1 불량률 통계**\n").arg(deviceName);
    failureMsg += QString("• 장비 ID: %1\n").arg(deviceId);
    failureMsg += QString("• 전체 생산: %1개\n").arg(cache.totalCount);
    failureMsg += QString("• 양품: %1개\n").arg(cache.passCount);
    failureMsg += QString("• 불량품: %1개\n").arg(cache.failCount);
    failureMsg += QString("• 불량률: %1%\n").arg(cache.failureRate, 0, 'f', 2);
    failureMsg += QString("• 마지막 업데이트: %1").arg(cache.lastUpdate.toString("hh:mm:ss"));
    
    qDebug() << "불량률 데이터 반환:" << failureMsg;
    return failureMsg;
}