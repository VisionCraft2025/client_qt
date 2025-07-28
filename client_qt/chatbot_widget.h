#ifndef CHATBOT_WIDGET_H
#define CHATBOT_WIDGET_H

#include <QWidget>
#include <QVector>
#include <functional>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
#include "mcp/DataStructures.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;
class QFrame;
class GeminiRequester;
class MCPAgentClient;

struct ChatMessage
{
    QString sender; // "user" or "bot"
    QString content;
    QString time;
};

class ChatBotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatBotWidget(QWidget *parent = nullptr);
    void setGemini(GeminiRequester *requester);
    void setMcpServerUrl(const QString &url);

    // MQTT 제어 관련 - public으로 이동
    QMap<QString, QTimer*> m_controlTimers;  // 기기별 타이머
    QMap<QString, QString> m_pendingControls; // 대기 중인 제어 명령
    void handleMqttControlTimeout(const QString& deviceId);

signals:
    void closed(); // 닫기 버튼 눌렸을 때

private slots:
    void handleSend();
    void handleQuickReplyClicked(); // 빠른 응답 선택
    void onToolDiscoveryCompleted(const ConversationContext &context);
    void onToolExecutionCompleted(const QString &result);
    void onMcpError(const QString &error);
    void onMqttConnected();

private:
    void addMessage(const ChatMessage &message);
    QString getCurrentTime();
    void processWithMcp(const QString &userInput);
    void initializeMqttClient();
    void controlMqttDevice(const QString &deviceName, const QString &action);
    QString getTopicForDevice(const QString &deviceName);
    QString getKoreanToolName(const QString& englishToolName);

protected:
    // 드래그 기능을 위한 이벤트 핸들러
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QWidget *messageContainer;
    QVBoxLayout *messageLayout;
    QScrollArea *scrollArea;

    QLineEdit *input;
    QPushButton *sendButton;
    QPushButton *closeButton;

    GeminiRequester *gemini = nullptr; // Gemini 연결

    // MCP 클라이언트 추가
    std::unique_ptr<MCPAgentClient> mcpClient;
    QString mcpServerUrl = "http://mcp.kwon.pics:8080";
    
    // MQTT 쿼리 응답 처리
    void processMqttQueryResponse(const QJsonObject& response);
    void requestDatabaseQuery(const QString& queryType, const QJsonObject& filters);

    bool waitingForResponse = false;
    
    // MQTT 클라이언트 추가
    QMqttClient *m_mqttClient = nullptr;

    // MQTT 쿼리 ID 관리
    QString generateQueryId();
    QString m_currentQueryId;

    QString getDeviceKoreanName(const QString& deviceId);
    
    // 드래그 기능을 위한 변수들
    bool m_dragging = false;
    QPoint m_dragStartPosition;
    QWidget *m_headerWidget = nullptr;

private slots:
    void onMqttMessageReceived(const QMqttMessage& message);
    void onMqttStatusReceived(const QMqttMessage& message);
};

#endif // CHATBOT_WIDGET_H
