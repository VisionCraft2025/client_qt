#ifndef CHATBOT_WIDGET_H
#define CHATBOT_WIDGET_H

#include <QWidget>
#include <QVector>
#include <functional>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttMessage>
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

    // MQTT 제어 관련 - public으로 이동
    QMap<QString, QTimer *> m_controlTimers;  // 기기별 타이머
    QMap<QString, QString> m_pendingControls; // 대기 중인 제어 명령
    void handleMqttControlTimeout(const QString &deviceId);

    // 기기 상태 시뮬레이션
    QHash<QString, QString> m_deviceStates; // deviceId -> "on"/"off"

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
    QString getKoreanToolName(const QString &englishToolName);

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
    void processMqttQueryResponse(const QJsonObject &response);
    void requestDatabaseQuery(const QString &queryType, const QJsonObject &filters);

    bool waitingForResponse = false;

    // MQTT 클라이언트 추가
    QMqttClient *m_mqttClient = nullptr;

    // MQTT 쿼리 ID 관리
    QString generateQueryId();
    QString m_currentQueryId;

    QString getDeviceKoreanName(const QString &deviceId);

    // 드래그 기능을 위한 변수들
    bool m_dragging = false;
    QPoint m_dragStartPosition;
    QWidget *m_headerWidget = nullptr;

    // 로딩 메시지 상태
    bool m_isLoadingMessageShown = false;

    // 크기 조절 관련
    enum SizeMode
    {
        SMALL,
        MIDDLE,
        BIG
    };

    SizeMode m_currentSizeMode = BIG;
    bool m_resizing = false;
    int m_resizeStartX = 0;
    int m_initialWidth = 0;

    // 크기별 치수
    struct ChatSize
    {
        int width;
        int height;
        int fontSize;
        int buttonHeight;
        int padding;
        int quickBtnHeight;
    };

    static constexpr ChatSize SIZES[3] = {
        {252, 380, 8, 20, 5, 25},      // SMALL - 더 작은 폰트
        {378, 570, 10, 35, 10, 32},    // MIDDLE - 중간 크기
        {504, 760, 12, 52, 15, 40}     // BIG - 현재 크기
    };

    // 드래그 임계값 조정
    static constexpr int RESIZE_THRESHOLD = 30;
    static constexpr int RESIZE_MARGIN = 30;
    void refreshAllMessages();
    void updateSizeMode(SizeMode newMode);
    void applySize(const ChatSize &size);
    bool isInResizeArea(const QPoint &pos);
    SizeMode calculateNewSizeMode(int deltaX);
    QVBoxLayout* m_titleBox = nullptr;

private slots:
    void onMqttMessageReceived(const QMqttMessage &message);
    void onMqttStatusReceived(const QMqttMessage &message);
    void onMqttCommandReceived(const QMqttMessage &message);
};

#endif // CHATBOT_WIDGET_H
