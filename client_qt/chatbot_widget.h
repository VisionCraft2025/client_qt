#ifndef CHATBOT_WIDGET_H
#define CHATBOT_WIDGET_H

#include <QWidget>
#include <QVector>
#include <functional>
#include "DataStructures.h"

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

signals:
    void closed(); // 닫기 버튼 눌렸을 때

private slots:
    void handleSend();
    void handleQuickReplyClicked(); // 빠른 응답 선택
    void onToolDiscoveryCompleted(const ConversationContext &context);
    void onToolExecutionCompleted(const QString &result);
    void onMcpError(const QString &error);

private:
    void addMessage(const ChatMessage &message);
    QString getCurrentTime();
    void processWithMcp(const QString &userInput);

    QWidget *messageContainer;
    QVBoxLayout *messageLayout;
    QScrollArea *scrollArea;

    QLineEdit *input;
    QPushButton *sendButton;
    QPushButton *closeButton;

    GeminiRequester *gemini = nullptr; // Gemini 연결

    // MCP 클라이언트 추가
    std::unique_ptr<MCPAgentClient> mcpClient;
    QString mcpServerUrl = "http://mcp.kwon.pics:8080"; // 기본값
    bool waitingForResponse = false;
};

#endif // CHATBOT_WIDGET_H
