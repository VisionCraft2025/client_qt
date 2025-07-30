#include "error_message_card.h"
#include "font_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>

ErrorMessageCard::ErrorMessageCard(QWidget *parent) : QFrame(parent)
{
    setObjectName("errorMessageCard");
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    labelTitle = new QLabel("Error Message");
    labelTitle->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 12));

    labelError = new QLabel("감지된 오류:");
    labelTime = new QLabel("오류 발생 날짜 및 시간:");
    labelLocation = new QLabel("발생 위치:");
    labelCamera = new QLabel("대상 카메라:");

    labelErrorValue = new QLabel("-");
    labelTimeValue = new QLabel("-");
    labelLocationValue = new QLabel("-");
    labelCameraValue = new QLabel("-");


    labelError->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelTime->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelLocation->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelCamera->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);


    labelErrorValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelTimeValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelLocationValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    labelCameraValue->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));

    // 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(12);

    // 제목
    labelTitle->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(labelTitle);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(60);  // 좌우 묶음 간격

    // 왼쪽 묶음
    QVBoxLayout *leftCol = new QVBoxLayout();
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->addWidget(labelError);
    row1->addWidget(labelErrorValue);
    row1->addStretch();
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->addWidget(labelLocation);
    row2->addWidget(labelLocationValue);
    row2->addStretch();

    leftCol->addLayout(row1);
    leftCol->addLayout(row2);
    leftCol->setSpacing(4);

    // 오른쪽 묶음
    QVBoxLayout *rightCol = new QVBoxLayout();
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->addWidget(labelTime);
    row3->addWidget(labelTimeValue);
    row3->addStretch();
    QHBoxLayout *row4 = new QHBoxLayout();
    row4->addWidget(labelCamera);
    row4->addWidget(labelCameraValue);
    row4->addStretch();
    rightCol->addLayout(row3);
    rightCol->addLayout(row4);
    rightCol->setSpacing(4);

    // 전체 수평 레이아웃에 좌우 묶음 삽입
    contentLayout->addLayout(leftCol);
    contentLayout->addLayout(rightCol);
    mainLayout->addLayout(contentLayout);

    setLayout(mainLayout);


    setNormalState();
}

void ErrorMessageCard::setErrorState(const QString &errorText, const QString &time,
                                     const QString &location, const QString &camera)
{
    labelErrorValue->setText(errorText);
    labelTimeValue->setText(time);
    labelLocationValue->setText(location);
    labelCameraValue->setText(camera);
    updateStyle(true);
}

void ErrorMessageCard::setNormalState()
{
    labelErrorValue->setText("오류 없음");
    labelTimeValue->setText("-");
    labelLocationValue->setText("-");
    labelCameraValue->setText("-");
    updateStyle(false);
}

void ErrorMessageCard::updateStyle(bool isError)
{
    QString bgColor = isError ? "#fff1f2" : "#ffffff";
    QString borderColor = isError ? "#dc2626" : "#f37121";  // 한화 오렌지
    QString titleColor = isError ? "#b91c1c" : "#f97316";

    setStyleSheet(QString(R"(
        QFrame#errorMessageCard {
            background-color: %1;
            border: 1px solid %2;
            border-left: 5px solid %2;
            border-radius: 12px;
        }
    )").arg(bgColor, borderColor));

    labelTitle->setStyleSheet(QString("color: %1;").arg(titleColor));
}
