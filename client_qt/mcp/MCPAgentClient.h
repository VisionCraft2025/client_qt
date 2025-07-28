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
#include <QMqttClient>
#include <QMqttTopicName>
#include <memory>
#include <optional>
#include "DataStructures.h"

// Forward declarations
class ChatBotWidget;

// 파이프라인 상태 추가
enum class PipelineState {
    IDLE,                    // 대기 중
    DISCOVERING_TOOL,        // 도구 발견 중
    PREPARING_EXECUTION,     // 실행 준비 중
    EXECUTING_TOOL,         // 도구 실행 중
    FORMATTING_RESULT       // 결과 포맷팅 중
};

class MCPAgentClient : public QObject {
    Q_OBJECT

public:
    explicit MCPAgentClient(const QString& mcpServerUrl, 
                           const QString& geminiApiKey, 
                           QObject* parent = nullptr);
    ~MCPAgentClient();

    // 통합 파이프라인 - 한 번의 호출로 전체 프로세스 완료
    void executeUnifiedPipeline(const QString& userQuery);
    
    // 도구 목록 갱신
    void fetchTools(bool forceRefresh = false);
    
    // MQTT 클라이언트 설정
    void setMqttClient(QMqttClient* mqttClient);
    
    // 대화 컨텍스트 관리
    void clearContext();
    ConversationContext* getCurrentContext() { return m_currentContext.get(); }
    
    // 파이프라인 상태 조회
    PipelineState getCurrentState() const { return m_pipelineState; }

signals:
    void toolsFetched(const QVector<ToolInfo>& tools);
    void pipelineStateChanged(PipelineState newState);
    void pipelineCompleted(const QString& result);
    void errorOccurred(const QString& error);
    void logMessage(const QString& message, int level);

private slots:
    void handleFetchToolsReply();
    void handleGeminiReply();
    void handleExecuteToolReply();
    void updateLoadingAnimation();

private:
    // 상태 전환
    void setPipelineState(PipelineState newState);
    
    // 통합 프롬프트 생성
    QString generateUnifiedPrompt(const QString& userQuery);
    
    // Gemini 응답 파싱
    struct UnifiedResponse {
        std::optional<QString> selectedTool;
        std::optional<QJsonObject> toolParameters;
        QString userMessage;
        bool requiresToolExecution;
    };
    UnifiedResponse parseUnifiedResponse(const QString& responseText);
    
    // 도구 실행
    void executeToolWithParameters(const QString& toolName, const QJsonObject& parameters);
    
    // 네트워크 요청
    void sendGeminiRequest(const QString& prompt);
    
    // 유틸리티
    void initializeToolExamples();
    QString getKoreanToolName(const QString& englishToolName);
    QString normalizeDeviceId(const QString& rawDeviceId);

    // 멤버 변수
    QString m_mcpServerUrl;
    QString m_geminiApiKey;
    QNetworkAccessManager* m_networkManager;
    QMqttClient* m_mqttClient;
    
    // 도구 캐시
    std::optional<QVector<ToolInfo>> m_toolsCache;
    std::optional<QDateTime> m_lastCacheUpdate;
    const int m_cacheDurationSeconds = 300;
    
    // 대화 컨텍스트
    std::unique_ptr<ConversationContext> m_currentContext;
    
    // 도구 예시
    QHash<QString, QVector<QString>> m_toolExamples;
    
    // 파이프라인 상태
    PipelineState m_pipelineState = PipelineState::IDLE;
    
    // 로딩 애니메이션
    QTimer* m_loadingTimer;
    int m_loadingDots;
    
    // MQTT 데이터 캐싱
    struct StatisticsCache {
        QString deviceId;
        double averageSpeed = 0.0;
        double currentSpeed = 0.0;
        QDateTime lastUpdate;
        bool isValid = false;
    };
    
    struct FailureStatsCache {
        QString deviceId;
        double failureRate = 0.0;
        int totalCount = 0;
        int passCount = 0;
        int failCount = 0;
        QDateTime lastUpdate;
        bool isValid = false;
    };
    
    QHash<QString, StatisticsCache> m_statisticsCache;
    QHash<QString, FailureStatsCache> m_failureStatsCache;

public slots:
    // MQTT 데이터 캐싱을 위한 슬롯
    void cacheStatisticsData(const QString& deviceId, double avgSpeed, double currentSpeed);
    void cacheFailureStatsData(const QString& deviceId, double failureRate, int total, int pass, int fail);
    
    // 캐시된 데이터 조회
    QString getCachedStatistics(const QString& deviceId);
    QString getCachedFailureStats(const QString& deviceId);
};

#endif // MCPAGENTCLIENT_H