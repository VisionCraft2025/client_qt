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
    void updateButtonSize();  // 동적 크기 계산 함수 추가

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;  // 위젯이 표시될 때 크기 업데이트
    void resizeEvent(QResizeEvent* event) override;  // 크기 변경 시 업데이트

private:
    QPushButton* mainButton;
    QLabel* textLabel;
    QLabel* badgeDot;

    //멋진 이펙트
    QGraphicsOpacityEffect* ringEffect;
    QPropertyAnimation* pulseAnim;
};

#endif // MCP_BTN_H
