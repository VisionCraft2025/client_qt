#pragma once

#include <QDialog>
#include <QPlainTextEdit>

class QLineEdit;
class QPushButton;

class AICommandDialog : public QDialog {
    Q_OBJECT
public:
    explicit AICommandDialog(QWidget *parent = nullptr);
    QString getCommand() const;
    void showResponse(const QString &response);  //mcp 응답 표시용
signals:
    void commandEntered(const QString &command);

private:
    QLineEdit *inputField;
    QPushButton *sendButton;
    QPlainTextEdit *responseBox; //응답
};
