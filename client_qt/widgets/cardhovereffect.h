#ifndef CARDHOVEREFFECT_H
#define CARDHOVEREFFECT_H

#include <QObject>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>

class CardHoverEffect : public QObject {
    Q_OBJECT
public:
    explicit CardHoverEffect(QWidget* parent, QGraphicsDropShadowEffect* s, QPropertyAnimation* a);
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QGraphicsDropShadowEffect* shadow;
    QPropertyAnimation* anim;
};

#endif // CARDHOVEREFFECT_H
