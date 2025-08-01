// ===== monthly_statistics_popup.h 파일 생성 =====

#ifndef MONTHLY_STATISTICS_POPUP_H
#define MONTHLY_STATISTICS_POPUP_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class MonthlyStatisticsPopup : public QDialog
{
    Q_OBJECT

public:
    explicit MonthlyStatisticsPopup(QWidget *parent = nullptr);
    ~MonthlyStatisticsPopup();

private slots:
    void onCloseClicked();

private:
    void setupUI();

    // UI 컴포넌트들
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_headerLayout;

    QLabel *m_titleLabel;
    QPushButton *m_closeButton;

    QWidget *m_chartContainer;  // 나중에 차트를 넣을 컨테이너
};

#endif // MONTHLY_STATISTICS_POPUP_H
