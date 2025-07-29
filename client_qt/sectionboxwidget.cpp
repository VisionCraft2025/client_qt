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
    mainLayout->setContentsMargins(24, 16, 24, 16);
    setLayout(mainLayout);
}

void SectionBoxWidget::addSection(const QString& labelText, const QList<QWidget*>& widgets, int stretchFactor)
{
    QFrame* section = createSection(labelText, widgets);
    section->setObjectName("section");
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

    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);

    if (!labelText.isEmpty()) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet("font-weight: bold; font-size: 13px; color: #374151;");
        layout->addWidget(label);
    }

    for (QWidget* w : contentWidgets) {
        layout->addWidget(w);
    }

    layout->addStretch(); // 하단 정렬
    return section;
}
