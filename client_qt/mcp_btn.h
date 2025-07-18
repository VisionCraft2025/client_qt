#ifndef MCP_BTN_H
#define MCP_BTN_H

#include <QWidget>

class QPushButton;
class QLabel;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

class MCPButton : public QWidget {
    Q_OBJECT

public:
    explicit MCPButton(QWidget* parent = nullptr);
    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QPushButton* mainButton;
    QLabel* textLabel;
    QLabel* badgeDot;

    //멋진 이펙트
    QGraphicsOpacityEffect* ringEffect;
    QPropertyAnimation* pulseAnim;
};

#endif // MCP_BTN_H
