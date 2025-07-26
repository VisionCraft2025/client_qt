#include "cardhovereffect.h"
#include <QEvent>
#include <QWidget>

CardHoverEffect::CardHoverEffect(QWidget* parent, QGraphicsDropShadowEffect* s, QPropertyAnimation* a)
    : QObject(parent), shadow(s), anim(a) {}

bool CardHoverEffect::eventFilter(QObject* obj, QEvent* event) {
    QWidget* widget = qobject_cast<QWidget*>(obj);
    if (!widget) return QObject::eventFilter(obj, event);

    if (event->type() == QEvent::Enter) {
        anim->setDirection(QAbstractAnimation::Forward);
        anim->start();
        // 카드 배경색 오렌지로 변경
        widget->setStyleSheet(R"(
            background-color: #fff7ed;
            border: 1px solid #e5e7eb;
            border-left: 2px solid #f97316;
            border-radius: 12px;
            padding: 5px 4px;
        )");
    } else if (event->type() == QEvent::Leave) {
        anim->setDirection(QAbstractAnimation::Backward);
        anim->start();
        // 카드 배경색 원래대로 복구
        widget->setStyleSheet(R"(
            border-radius: 12px;
            border: 1px solid #E5E7EB;
            background: #F3F4F6;
            padding: 5px 4px;
        )");
    }
    return QObject::eventFilter(obj, event);
}
