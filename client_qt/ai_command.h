// ai_command.h
#ifndef AI_COMMAND_H
#define AI_COMMAND_H

#include <QObject>
#include <QString>

class QWidget;

class GeminiRequester : public QObject {
    Q_OBJECT

public:
    explicit GeminiRequester(QObject* parent = nullptr, const QString& apiKey = "");

    void askGemini(QWidget* parent);

private:
    QString apiKey;
};

#endif // AI_COMMAND_H
