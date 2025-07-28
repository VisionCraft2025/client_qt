#include "MCPAgentClient.h"
#include "ToolExamples.h"
#include "PromptGenerators.h"
#include "DataFormatter.h" 
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

MCPAgentClient::MCPAgentClient(const QString& mcpServerUrl, 
                               const QString& geminiApiKey, 
                               QObject* parent)
    : QObject(parent)
    , m_mcpServerUrl(mcpServerUrl)
    , m_geminiApiKey(geminiApiKey)
    , m_networkManager(new QNetworkAccessManager(this))
{
    if (m_mcpServerUrl.endsWith('/')) {
        m_mcpServerUrl.chop(1);
    }
    
    initializeToolExamples();
}

MCPAgentClient::~MCPAgentClient() = default;

void MCPAgentClient::initializeToolExamples() {
    m_toolExamples = ToolExamples::getToolExamples();
}

QString MCPAgentClient::generatePromptForToolDiscovery(const QString& userQuery, 
                                                      const QVector<ToolInfo>& tools) {
    return PromptGenerators::generateToolDiscoveryPrompt(userQuery, tools);
}

QString MCPAgentClient::generatePromptForToolExecution(ConversationContext* context) {
    return PromptGenerators::generateToolExecutionPrompt(context);
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
    request.setTransferTimeout(10000); // 10초 타임아웃
    
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
        
        // 예시 추가
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

void MCPAgentClient::pipelineToolDiscovery(const QString& userQuery) {
    emit logMessage("🔍 도구 발견 파이프라인 시작...", 0);
    
    m_pendingUserQuery = userQuery;
    
    // 도구 목록 가져오기
    if (!m_toolsCache.has_value()) {
        connect(this, &MCPAgentClient::toolsFetched, this, [this]() {
            disconnect(this, &MCPAgentClient::toolsFetched, nullptr, nullptr);
            
            if (!m_toolsCache.has_value() || m_toolsCache->isEmpty()) {
                ConversationContext context;
                context.userQuery = m_pendingUserQuery;
                context.executionResult = "도구 목록을 가져올 수 없습니다. 서버 연결을 확인해주세요.";
                emit toolDiscoveryCompleted(context);
                return;
            }
            
            // Gemini API 호출
            QString prompt = generatePromptForToolDiscovery(m_pendingUserQuery, m_toolsCache.value());
            m_currentPipelineType = PipelineType::TOOL_DISCOVERY;
            sendGeminiRequest(prompt, PipelineType::TOOL_DISCOVERY);
        });
        
        fetchTools();
    } else {
        // Gemini API 호출
        QString prompt = generatePromptForToolDiscovery(userQuery, m_toolsCache.value());
        m_currentPipelineType = PipelineType::TOOL_DISCOVERY;
        sendGeminiRequest(prompt, PipelineType::TOOL_DISCOVERY);
    }
}

void MCPAgentClient::sendGeminiRequest(const QString& prompt, PipelineType pipelineType) {
    // Gemini 2.5 Flash 사용
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
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("Gemini API 호출 실패: %1").arg(reply->errorString()));
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        emit errorOccurred("잘못된 Gemini API 응답");
        return;
    }
    
    // 응답 파싱
    QJsonObject root = doc.object();
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) {
        emit errorOccurred("Gemini API 응답에 후보가 없습니다");
        return;
    }
    
    QJsonObject candidate = candidates[0].toObject();
    QJsonObject content = candidate["content"].toObject();
    QJsonArray parts = content["parts"].toArray();
    
    if (parts.isEmpty()) {
        emit errorOccurred("Gemini API 응답에 컨텐츠가 없습니다");
        return;
    }
    
    QString responseText = parts[0].toObject()["text"].toString();
    
    if (m_currentPipelineType == PipelineType::TOOL_DISCOVERY) {
        // 도구 발견 응답 처리
        auto [selectedTool, finalResponse] = parseToolDiscoveryResponse(responseText);
        
        // 컨텍스트 생성
        m_currentContext = std::make_unique<ConversationContext>();
        m_currentContext->userQuery = m_pendingUserQuery;
        m_currentContext->availableTools = m_toolsCache.value();
        m_currentContext->selectedTool = selectedTool;
        m_currentContext->executionResult = finalResponse;
        
        // 대화 기록 추가
        m_currentContext->conversationHistory.append({
            "user", m_pendingUserQuery
        });
        m_currentContext->conversationHistory.append({
            "assistant", finalResponse
        });
        
        emit logMessage("✅ 도구 발견 완료", 0);
        if (selectedTool.has_value()) {
            emit logMessage(QString("🛠️  선택된 도구: %1").arg(selectedTool.value()), 0);
        }
        
        emit toolDiscoveryCompleted(*m_currentContext);
        
    } else if (m_currentPipelineType == PipelineType::TOOL_EXECUTION) {
        // 도구 실행을 위한 매개변수 파싱
        QString parametersText = responseText.trimmed();
        
        // 코드 블록 제거
        if (parametersText.contains("```json")) {
            int start = parametersText.indexOf("```json") + 7;
            int end = parametersText.indexOf("```", start);
            if (end != -1) {
                parametersText = parametersText.mid(start, end - start).trimmed();
            }
        } else if (parametersText.contains("```")) {
            int start = parametersText.indexOf("```") + 3;
            int end = parametersText.indexOf("```", start);
            if (end != -1) {
                parametersText = parametersText.mid(start, end - start).trimmed();
            }
        }
        
        QJsonParseError parseError;
        QJsonDocument paramDoc = QJsonDocument::fromJson(parametersText.toUtf8(), &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            emit errorOccurred(QString("매개변수 파싱 실패: %1").arg(parseError.errorString()));
            return;
        }
        
        QJsonObject parameters = paramDoc.object();
        m_currentContext->toolParameters = parameters;
        
        emit logMessage(QString("📋 매개변수: %1").arg(QString::fromUtf8(paramDoc.toJson(QJsonDocument::Compact))), 0);
        
        // 도구 실행
        executeToolRequest(m_currentContext->selectedTool.value(), parameters);
    }
}

std::pair<std::optional<QString>, QString> MCPAgentClient::parseToolDiscoveryResponse(
    const QString& responseText) {
    QStringList lines = responseText.split('\n');
    std::optional<QString> selectedTool;
    QString finalResponse;
    
    for (const QString& line : lines) {
        if (line.contains("적합한 도구:")) {
            QString toolPart = line.split("적합한 도구:").last().trimmed();
            if (toolPart.toLower() != "없음" && !toolPart.isEmpty()) {
                selectedTool = toolPart;
            }
        } else if (line.contains("응답:")) {
            finalResponse = line.split("응답:").last().trimmed();
        }
    }
    
    // 응답이 비어있으면 전체 텍스트 사용
    if (finalResponse.isEmpty()) {
        finalResponse = responseText;
    }
    
    return {selectedTool, finalResponse};
}

void MCPAgentClient::pipelineToolExecution(const QString& userQuery, 
                                          ConversationContext* context) {
    emit logMessage("⚡ 도구 실행 파이프라인 시작...", 0);
    
    // 컨텍스트 확인
    if (!context) {
        context = m_currentContext.get();
    }
    
    if (!context || !context->selectedTool.has_value()) {
        // 도구가 선택되지 않은 경우, 먼저 도구 발견 실행
        pipelineToolDiscovery(userQuery);
        return;
    }
    
    // 대화 맥락에 새 질문 추가
    context->userQuery = userQuery;
    context->conversationHistory.append({"user", userQuery});
    
    // Gemini API 호출하여 매개변수 생성
    QString prompt = generatePromptForToolExecution(context);
    m_currentPipelineType = PipelineType::TOOL_EXECUTION;
    sendGeminiRequest(prompt, PipelineType::TOOL_EXECUTION);
}

void MCPAgentClient::executeToolRequest(const QString& toolName, 
                                       const QJsonObject& parameters) {
    QJsonObject payload;
    payload["name"] = toolName;
    payload["arguments"] = parameters;
    
    QNetworkRequest request(QUrl(m_mcpServerUrl + "/tools/call"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(30000); // 30초 타임아웃
    
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
        emit toolExecutionCompleted(errorMsg);
        return;
    }
    
    QByteArray data = reply->readAll();
    
    // 원시 데이터 터미널 출력
    qDebug() << "=== MCP 도구 실행 결과 (원시 데이터) ===";
    qDebug() << data;
    qDebug() << "=== 원시 데이터 끝 ===";
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        QString errorMsg = "잘못된 도구 실행 응답";
        emit errorOccurred(errorMsg);
        emit toolExecutionCompleted(errorMsg);
        return;
    }
    
    QJsonObject result = doc.object();
    bool isError = result["isError"].toBool(false);
    QJsonArray contentArray = result["content"].toArray();
    
    if (contentArray.isEmpty()) {
        QString errorMsg = "도구 실행 결과가 비어있습니다";
        emit errorOccurred(errorMsg);
        emit toolExecutionCompleted(errorMsg);
        return;
    }
    
    QString resultText = contentArray[0].toObject()["text"].toString();
    
    if (isError) {
        emit logMessage(QString("❌ 오류: %1").arg(resultText), 2);
    } else {
        emit logMessage(QString("✅ 실행 성공: %1").arg(resultText), 0);
        
        // 포맷팅 단계 추가
        if (m_currentContext && m_currentContext->selectedTool.has_value()) {
            QString toolName = m_currentContext->selectedTool.value();
            QString formattedResult = DataFormatter::formatExecutionResult(toolName, resultText);
            
            // 포맷팅된 결과 저장
            m_currentContext->formattedResult = formattedResult;
            
            // raw_data 처리 (기존 코드 유지)
            if (result.contains("raw_data")) {
                QJsonArray rawData = result["raw_data"].toArray();
                if (!rawData.isEmpty()) {
                    // 터미널에 상세 데이터 출력
                    qDebug() << QString("=== 상세 데이터 (%1개) ===").arg(rawData.size());
                    
                    int count = std::min(10, static_cast<int>(rawData.size())); // 터미널에는 10개까지
                    for (int i = 0; i < count; ++i) {
                        QJsonDocument itemDoc(rawData[i].toObject());
                        qDebug() << QString("  %1. %2").arg(i+1)
                            .arg(QString::fromUtf8(itemDoc.toJson(QJsonDocument::Compact)));
                    }
                    
                    if (rawData.size() > 10) {
                        qDebug() << QString("  ... 그리고 %1개 더").arg(rawData.size() - 10);
                    }
                    qDebug() << "=== 상세 데이터 끝 ===";
                    
                    // 챗봇용 출력 (기존 유지)
                    emit logMessage(QString("📊 상세 데이터 (%1개)").arg(rawData.size()), 0);
                    
                    int displayCount = std::min(3, static_cast<int>(rawData.size()));
                    for (int i = 0; i < displayCount; ++i) {
                        QJsonDocument itemDoc(rawData[i].toObject());
                        emit logMessage(QString("  %1. %2").arg(i+1)
                            .arg(QString::fromUtf8(itemDoc.toJson(QJsonDocument::Indented))), 0);
                    }
                    
                    if (rawData.size() > 3) {
                        emit logMessage(QString("  ... 그리고 %1개 더").arg(rawData.size() - 3), 0);
                    }
                }
            }
        }
    }
    
    // 대화 기록 업데이트
    if (m_currentContext) {
        // 포맷팅된 결과가 있으면 그것을 사용, 없으면 원본 사용
        QString finalResult = m_currentContext->formattedResult.value_or(resultText);
        m_currentContext->conversationHistory.append({"assistant", finalResult});
        m_currentContext->executionResult = finalResult;
    }
    
    // 포맷팅된 결과 전송
    QString finalOutput = m_currentContext && m_currentContext->formattedResult.has_value() 
        ? m_currentContext->formattedResult.value() 
        : resultText;
    
    emit toolExecutionCompleted(finalOutput);
}

void MCPAgentClient::clearContext() {
    m_currentContext.reset();
    emit logMessage("🔄 대화 맥락이 초기화되었습니다.", 0);
}