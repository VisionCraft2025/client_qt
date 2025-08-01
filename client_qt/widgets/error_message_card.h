#ifndef ERROR_MESSAGE_CARD_H
#define ERROR_MESSAGE_CARD_H
#include <QFrame>
#include <QLabel>
#include <QTimer>  // 깜빡임 효과를 위한 타이머 추가
class ErrorMessageCard : public QFrame
{
    Q_OBJECT
public:
    explicit ErrorMessageCard(QWidget *parent = nullptr);
    void setErrorState(const QString &errorText, const QString &time,
                       const QString &location, const QString &camera);
    void setNormalState();
private slots:
    void onBlinkTimer();  // 깜빡임 타이머 슬롯
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
    QTimer *blinkTimer;      // 깜빡임 타이머
    bool isBlinkVisible;     // 깜빡임 상태 (true: 밝은색, false: 어두운색)
    bool isErrorMode;        // 현재 에러 모드인지 확인
    void updateStyle(bool isError, bool blinkState = true);
};
#endif // ERROR_MESSAGE_CARD_H
