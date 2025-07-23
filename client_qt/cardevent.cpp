#include "cardevent.h"
#include <QEvent>
#include <QMouseEvent>

ErrorCardEventFilter::ErrorCardEventFilter(QObject* parent) : QObject(parent) {}

bool ErrorCardEventFilter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        emit cardDoubleClicked(watched);
        return true;
    }
    return QObject::eventFilter(watched, event);
}
