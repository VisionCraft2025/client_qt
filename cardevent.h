#ifndef CARDEVENT_H
#define CARDEVENT_H
#include <QObject>
#include <QEvent>
class CardEventFilter : public QObject {
    Q_OBJECT
public:
    explicit CardEventFilter(QObject* parent = nullptr);
signals:
    void cardDoubleClicked(QObject* cardWidget);
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};
#endif // CARDEVENT_H
