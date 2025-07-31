#ifndef ERROR_MESSAGE_CARD_H
#define ERROR_MESSAGE_CARD_H

#include <QFrame>
#include <QLabel>

class ErrorMessageCard : public QFrame
{
    Q_OBJECT

public:
    explicit ErrorMessageCard(QWidget *parent = nullptr);

    void setErrorState(const QString &errorText, const QString &time,
                       const QString &location, const QString &camera);
    void setNormalState();

private:
    QLabel *labelTitle;
    QLabel *labelError;
    QLabel *labelTime;
    QLabel *labelLocation;
    QLabel *labelCamera;

    QLabel *labelErrorValue;
    QLabel *labelTimeValue;
    QLabel *labelLocationValue;
    QLabel *labelCameraValue;

    void updateStyle(bool isError);
};

#endif // ERROR_MESSAGE_CARD_H
