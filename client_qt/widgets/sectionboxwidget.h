#ifndef SECTIONBOXWIDGET_H
#define SECTIONBOXWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QList>

class SectionBoxWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SectionBoxWidget(QWidget* parent = nullptr);
    void addSection(const QString& label, const QList<QWidget*>& widgets, int stretchFactor = 1);
    void addDivider();

private:
    QHBoxLayout* mainLayout;
    QFrame* createSection(const QString& labelText, const QList<QWidget*>& contentWidgets);
};

#endif // SECTIONBOXWIDGET_H
