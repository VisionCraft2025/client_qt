#include "sectionboxwidget.h"

SectionBoxWidget::SectionBoxWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("sectionbox");

    setStyleSheet(R"(
        #sectionbox {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
        .section {
            background-color: transparent;
            border: none;
        }
        .section:not(:last-child) {
            border-right: 1px solid #e5e7eb;
        }
    )");

    mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(16, 16, 16, 16); // 좌우 마진 4로 변경
    mainLayout->setAlignment(Qt::AlignTop); // 추가
    setLayout(mainLayout);
}

void SectionBoxWidget::addSection(const QString& labelText, const QList<QWidget*>& widgets, int stretchFactor)
{
    QFrame* section = createSection(labelText, widgets);
    section->setObjectName("section");
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding); // 모두 Expanding으로 통일
    mainLayout->addWidget(section, stretchFactor);
}

void SectionBoxWidget::addDivider()
{
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet("color: #e5e7eb;");
    mainLayout->addWidget(line);
}

QFrame* SectionBoxWidget::createSection(const QString& labelText, const QList<QWidget*>& contentWidgets)
{
    QFrame* section = new QFrame();
    section->setStyleSheet("QFrame { background-color: transparent; border: none; }");
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // 세로 공간 고정

    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop); // 추가

    if (!labelText.isEmpty()) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet("font-weight: bold; font-size: 17px; color: #374151;"); // 폰트 크기 18px로 변경
        layout->addWidget(label);
        if (labelText == "기기 상태") {
            layout->addSpacing(60); // 원하는 만큼 조정
        }
    }

    for (QWidget* w : contentWidgets) {
        layout->addWidget(w);
    }

    //layout->addStretch(); // 하단 정렬
    return section;
}
