#ifndef MCPAGENTCLIENT_H
#define MCPAGENTCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include <optional>
#include "DataStructures.h"

class MCPAgentClient : public QObject {
    Q_OBJECT

public:
    explicit MCPAgentClient(const QString& mcpServerUrl, 
                           const QString& geminiApiKey, 
                           QObject* parent = nullptr);
    ~MCPAgentClient();

    // 공개 메서드
    void fetchTools(bool forceRefresh = false);
    void pipelineToolDiscovery(const QString& userQuery);
    void pipelineToolExecution(const QString& userQuery, 
                              ConversationContext* context = nullptr);
    void clearContext();
    
    // Getter
    ConversationContext* getCurrentContext() { return m_currentContext.get(); }

signals:
    void toolsFetched(const QVector<ToolInfo>& tools);
    void toolDiscoveryCompleted(const ConversationContext& context);
    void toolExecutionCompleted(const QString& result);
    void errorOccurred(const QString& error);
    void logMessage(const QString& message, int level); // 0:INFO, 1:WARNING, 2:ERROR

private slots:
    void handleFetchToolsReply();
    void handleGeminiReply();
    void handleExecuteToolReply();

private:
    // 내부 메서드
    QString generatePromptForToolDiscovery(const QString& userQuery, 
                                          const QVector<ToolInfo>& tools);
    QString generatePromptForToolExecution(ConversationContext* context);
    std::pair<std::optional<QString>, QString> parseToolDiscoveryResponse(
        const QString& responseText);
    void executeToolRequest(const QString& toolName, const QJsonObject& parameters);
    void sendGeminiRequest(const QString& prompt, PipelineType pipelineType);
    void initializeToolExamples();

    // 멤버 변수
    QString m_mcpServerUrl;
    QString m_geminiApiKey;
    QNetworkAccessManager* m_networkManager;
    
    // 도구 캐시
    std::optional<QVector<ToolInfo>> m_toolsCache;
    std::optional<QDateTime> m_lastCacheUpdate;
    const int m_cacheDurationSeconds = 300;
    
    // 대화 컨텍스트
    std::unique_ptr<ConversationContext> m_currentContext;
    
    // 도구 예시 매핑
    QHash<QString, QVector<QString>> m_toolExamples;
    
    // 현재 파이프라인 타입
    PipelineType m_currentPipelineType;
    
    // 임시 저장
    QString m_pendingUserQuery;
};

#endif // MCPAGENTCLIENT_H