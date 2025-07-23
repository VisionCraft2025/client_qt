#pragma once
#include <QObject>

class ErrorCardEventFilter : public QObject {
    Q_OBJECT
public:
    explicit ErrorCardEventFilter(QObject* parent = nullptr);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
signals:
    void cardDoubleClicked(QObject* cardWidget);
};
