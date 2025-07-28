#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QDateTime>
#include <optional>

enum class PipelineType {
    TOOL_DISCOVERY,
    TOOL_EXECUTION
};

enum class OutputFormat {
    RAW,        // 원본 그대로
    TABLE,      // 테이블 형식
    CHART,      // 차트 형식 (ASCII)
    SUMMARY     // 요약 형식
};

struct ToolInfo {
    QString name;
    QString description;
    QJsonObject inputSchema;
    QVector<QString> examples;
};

struct ConversationMessage {
    QString role;
    QString content;
};

struct ConversationContext {
    QString userQuery;
    QVector<ToolInfo> availableTools;
    std::optional<QString> selectedTool;
    std::optional<QJsonObject> toolParameters;
    std::optional<QString> executionResult;
    std::optional<QString> formattedResult; 
    QVector<ConversationMessage> conversationHistory;
    OutputFormat preferredFormat = OutputFormat::SUMMARY;
};

#endif // DATASTRUCTURES_H