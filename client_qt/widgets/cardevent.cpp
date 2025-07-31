#include "cardevent.h"
#include <QEvent>

CardEventFilter::CardEventFilter(QObject* parent) : QObject(parent) {}

bool CardEventFilter::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        emit cardDoubleClicked(obj);
        return true;
    }
    return QObject::eventFilter(obj, event);
}
