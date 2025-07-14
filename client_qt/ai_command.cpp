#include "ai_command.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>

AICommandDialog::AICommandDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AI 명령어 입력");
    setFixedSize(400, 250);

    inputField = new QLineEdit(this);
    sendButton = new QPushButton("ASK AI", this);
    responseBox = new QPlainTextEdit(this);
    responseBox->setReadOnly(true);
    responseBox->setPlaceholderText("AI 답변이 여기에 표시도 ㅣㅁ");
    responseBox->setStyleSheet("background-color: #f6f6f6;");



    inputField->setPlaceholderText("ex)내가 뭘 할 수 있는지 알려줘");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(inputField);
    layout->addWidget(sendButton);
    layout->addWidget(responseBox);

    connect(sendButton, &QPushButton::clicked, this, [this]() {
        emit commandEntered(inputField->text());
        accept();
    });
}

QString AICommandDialog::getCommand() const {
    return inputField->text();
}

void AICommandDialog::showResponse(const QString &response) {
    responseBox->setPlainText(response);
}
