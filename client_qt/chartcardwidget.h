#ifndef CHARTCARDWIDGET_H
#define CHARTCARDWIDGET_H

#include <QWidget>

class QChartView;

class ChartCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChartCardWidget(QChartView* chartView, QWidget* parent = nullptr);
};

#endif // CHARTCARDWIDGET_H
