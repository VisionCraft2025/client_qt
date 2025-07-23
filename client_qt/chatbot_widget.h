#ifndef CHATBOT_WIDGET_H
#define CHATBOT_WIDGET_H

#include <QWidget>
#include <QVector>
#include <functional>
#include "factory_mcp.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;
class QFrame;
class GeminiRequester;

struct ChatMessage {
    QString sender;  // "user" or "bot"
    QString content;
    QString time;
};

class ChatBotWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatBotWidget(QWidget* parent = nullptr);
    void setGemini(GeminiRequester* requester);
    void setMcpHandler(FactoryMCP* mcp); // MCP 핸들러 주입


signals:
    void closed();  // 닫기 버튼 눌렸을 때

private slots:
    void handleSend();
    void handleQuickReplyClicked(); // 빠른 응답 선택
    void handleGeminiResult(const QJsonObject& json); // Gemini 결과 → MCP 명령 전송
    void onMcpResponse(const QByteArray& response);   // MCP 응답 처리
    void onMcpError(const QString& reason);           // MCP 에러 처리

private:
    void addMessage(const ChatMessage& message);
    QString getCurrentTime();

    QWidget* messageContainer;
    QVBoxLayout* messageLayout;
    QScrollArea* scrollArea;

    QLineEdit* input;
    QPushButton* sendButton;
    QPushButton* closeButton;

    GeminiRequester* gemini = nullptr;  // Gemini 연결
    FactoryMCP* mcp = nullptr;          // MCP 핸들러
};

#endif // CHATBOT_WIDGET_H
