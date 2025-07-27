#include "chartcardwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QChartView>
#include <QPixmap>
#include <QStackedLayout>

/* 작은 뱃지 생성 헬퍼 */
static QLabel* badge(const QString& text, const QString& c1, const QString& c2){
    auto* b = new QLabel(text);
    b->setStyleSheet(QString(
                         "padding:4px 10px; font-size:10px; color:white;"
                         "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %1,stop:1 %2);"
                         "border-radius:10px;").arg(c1,c2));
    return b;
}

ChartCardWidget::ChartCardWidget(QChartView* chartView, QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background:transparent;border:none;border-radius:16px;");

    // 카드 전체 프레임 (각지게)
    auto* cardFrame = new QFrame(this);
    cardFrame->setStyleSheet(
        "background:white;"
        "border:none;"
        "border-radius:18px;"
        );

    // 카드 레이아웃
    auto* cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(0,0,0,0);
    cardLayout->setSpacing(0);

    // 헤더 (기존대로)
    auto* header = new QFrame;
    header->setFixedHeight(64);
    header->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #111827,stop:1 #1f2937);"
        "border:none;"
        "border-top-left-radius:18px;"
        "border-top-right-radius:18px;"
        "border-bottom-left-radius:0;"
        "border-bottom-right-radius:0;"
        );

    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(16,8,16,8);

    auto* title = new QLabel("Device Error Analytics");
    title->setStyleSheet("color:white; font-size:16px; margin:0; padding:0 0 1px 0; vertical-align:middle;");

    auto* subtitle = new QLabel("Real-time failure rate monitoring");
    subtitle->setStyleSheet("font-size:11px;color:#cbd5e1;background:transparent;");

    auto* left = new QVBoxLayout;
    left->setSpacing(0); // title과 subtitle 사이 간격 최소화
    auto* row  = new QHBoxLayout;
    row->setAlignment(Qt::AlignVCenter); // 수직 중앙 정렬
    row->addWidget(title); // 타이틀만 왼쪽 정렬
    left->addLayout(row); left->addWidget(subtitle);

    // 오른쪽: 동그라미 범례
    auto* right = new QHBoxLayout;
    auto* feederDot = new QLabel;
    feederDot->setFixedSize(14,14);
    feederDot->setStyleSheet("background:#fb923c; border-radius:3px; margin-right:4px;");
    auto* feederText = new QLabel("Feeder");
    feederText->setStyleSheet("color:#cbd5e1;font-size:12px;margin-right:10px;background:transparent;");
    auto* feederBox = new QHBoxLayout;
    feederBox->addWidget(feederDot); feederBox->addWidget(feederText);
    feederBox->setSpacing(4);
    feederBox->setContentsMargins(0,0,0,0);
    auto* feederWidget = new QWidget; feederWidget->setLayout(feederBox); feederWidget->setStyleSheet("background:transparent;");

    auto* conveyorDot = new QLabel;
    conveyorDot->setFixedSize(14,14);
    conveyorDot->setStyleSheet("background:#60a5fa; border-radius:3px; margin-right:4px;");
    auto* conveyorText = new QLabel("Conveyor");
    conveyorText->setStyleSheet("color:#cbd5e1;font-size:12px;background:transparent;");
    auto* conveyorBox = new QHBoxLayout;
    conveyorBox->addWidget(conveyorDot); conveyorBox->addWidget(conveyorText);
    conveyorBox->setSpacing(4);
    conveyorBox->setContentsMargins(0,0,0,0);
    auto* conveyorWidget = new QWidget; conveyorWidget->setLayout(conveyorBox); conveyorWidget->setStyleSheet("background:transparent;");

    right->addWidget(feederWidget);
    right->addWidget(conveyorWidget);
    right->setSpacing(10);

    h->addLayout(left); h->addStretch(); h->addLayout(right);
    cardLayout->addWidget(header);

    // 차트 영역 (body)
    auto* body = new QFrame;
    body->setStyleSheet(
        "background:white;"
        "border:none;"
        "border-top-left-radius:0px;"
        "border-top-right-radius:0px;"
        "border-bottom-left-radius:18px;"
        "border-bottom-right-radius:18px;"
        );
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(16,0,16,16);
    bodyLay->addWidget(chartView);

    cardLayout->addWidget(body);

    // ChartCardWidget의 메인 레이아웃에 cardFrame을 추가
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(cardFrame);

}
