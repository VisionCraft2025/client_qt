// ===== monthly_statistics_popup.cpp 파일 생성 =====

#include "monthly_statistics_popup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QFrame>
#include <QDebug>

MonthlyStatisticsPopup::MonthlyStatisticsPopup(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_headerLayout(nullptr)
    , m_titleLabel(nullptr)
    , m_closeButton(nullptr)
    , m_chartContainer(nullptr)
{
    setupUI();
}

MonthlyStatisticsPopup::~MonthlyStatisticsPopup()
{
    // 자동으로 정리됨
}

void MonthlyStatisticsPopup::setupUI()
{
    setWindowTitle("월별 에러 통계");
    setFixedSize(900, 650);
    setModal(true);  // 모달 팝업으로 설정

    // 메인 레이아웃
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(15);

    // 헤더 레이아웃 (제목 + 닫기 버튼)
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setSpacing(10);

    // 제목 라벨
    m_titleLabel = new QLabel("📊 월별 에러 발생 통계 (최근 6개월)");

    // 폰트 설정
    QFont titleFont;
    titleFont.setBold(true);
    titleFont.setPointSize(18);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("QLabel { color: #2563eb; padding: 10px 0px; }");

    // 닫기 버튼 (X)
    m_closeButton = new QPushButton("✕");
    m_closeButton->setFixedSize(30, 30);

    QFont closeFont;
    closeFont.setBold(true);
    closeFont.setPointSize(14);
    m_closeButton->setFont(closeFont);
    m_closeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ef4444;
            color: white;
            border: none;
            border-radius: 15px;
        }
        QPushButton:hover {
            background-color: #dc2626;
        }
    )");

    // 닫기 버튼 클릭 시그널 연결
    connect(m_closeButton, &QPushButton::clicked, this, &MonthlyStatisticsPopup::onCloseClicked);

    // 헤더에 제목과 닫기 버튼 추가
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();  // 중간에 여백
    m_headerLayout->addWidget(m_closeButton);

    m_mainLayout->addLayout(m_headerLayout);

    // 구분선
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("QFrame { color: #e5e7eb; }");
    m_mainLayout->addWidget(line);

    // 차트 컨테이너 (일단 빈 공간)
    m_chartContainer = new QWidget();
    m_chartContainer->setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            border: 2px solid #e5e7eb;
            border-radius: 12px;
        }
    )");
    m_chartContainer->setMinimumHeight(450);

    // 임시 텍스트 (차트가 들어갈 자리)
    QVBoxLayout *tempLayout = new QVBoxLayout(m_chartContainer);
    QLabel *tempLabel = new QLabel("여기에 월별 차트가 들어갈 예정입니다");
    tempLabel->setAlignment(Qt::AlignCenter);
    tempLabel->setStyleSheet("color: #6b7280; font-size: 16px;");
    tempLayout->addWidget(tempLabel);

    m_mainLayout->addWidget(m_chartContainer);

    qDebug() << "MonthlyStatisticsPopup 기본 UI 설정 완료";
}

void MonthlyStatisticsPopup::onCloseClicked()
{
    qDebug() << "월별 통계 팝업 닫기 버튼 클릭됨";
    close();  // 팝업창 닫기
}
