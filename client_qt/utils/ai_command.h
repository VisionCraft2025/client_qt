// ai_command.h
#ifndef AI_COMMAND_H
#define AI_COMMAND_H

#include <QObject>
#include <QString>
#include <functional>

class QWidget;

class GeminiRequester : public QObject {
    Q_OBJECT

public:
    explicit GeminiRequester(QObject* parent = nullptr, const QString& apiKey = "");
    void askGemini(QWidget* parent);  // 단순 입력, 응답을 QMessageBox로 표시
    void askGemini(QWidget* parent, const QString& userText, std::function<void(QString)> callback); //오버로딩, 사용자 입력, 응답은 콜백 처리

    QString getApiKey() const { return apiKey; } 
private:
    QString apiKey;
};

#endif // AI_COMMAND_H
