#ifndef CHATBOT_WIDGET_H
#define CHATBOT_WIDGET_H

#include <QWidget>
#include <QVector>
#include <functional>

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


signals:
    void closed();  // 닫기 버튼 눌렸을 때

private slots:
    void handleSend();
    void handleQuickReplyClicked(); // 빠른 응답 선택

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
};

#endif // CHATBOT_WIDGET_H
