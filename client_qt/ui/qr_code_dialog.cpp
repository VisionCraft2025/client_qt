#include "qr_code_dialog.h"
#include "../utils/font_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QFont>
#include <QResizeEvent>

QRCodeDialog::QRCodeDialog(const QString &userId, const QString &secret, QWidget *parent)
    : QDialog(parent)
{
    setupUI(userId, secret);
}

QRCodeDialog::~QRCodeDialog()
{
}

void QRCodeDialog::setupUI(const QString &userId, const QString &secret)
{
    // 윈도우 플래그 설정으로 타이틀바 제거
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setFixedSize(450, 550);  // 크기 고정 (조정 불가)

    // 배경색을 완전한 흰색으로 설정하고 테두리 추가
    setStyleSheet("QRCodeDialog { background-color: #FFFFFF; border: 2px solid #CCCCCC; }");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(15);
    layout->setContentsMargins(0, 0, 0, 20);  // 상단 여백 0으로 설정

    // 헤더 영역을 위한 위젯과 레이아웃
    QWidget *headerWidget = new QWidget();
    headerWidget->setStyleSheet("QWidget { background-color: #25282A; }");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(15, 15, 15, 15);
    headerLayout->setSpacing(0);

    // Authentication 제목 라벨
    QLabel *headerLabel = new QLabel("Authentication");
    headerLabel->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 14));
    headerLabel->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    headerLayout->addWidget(headerLabel);

    // 스페이서로 제목을 왼쪽에, 닫기 버튼을 오른쪽에 배치
    headerLayout->addStretch();

    // X 닫기 버튼
    QPushButton *closeXButton = new QPushButton("✕");
    closeXButton->setFixedSize(30, 30);
    closeXButton->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 16));
    closeXButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #FFFFFF;
            border: none;
        }
        QPushButton:hover {
            background-color: #FF4444;
            border-radius: 15px;
        }
        QPushButton:pressed {
            background-color: #CC3333;
        }
    )");
    connect(closeXButton, &QPushButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(closeXButton);

    layout->addWidget(headerWidget);

    // 나머지 컨텐츠를 위한 내부 레이아웃 (여백 추가)
    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(20, 20, 20, 0);

    layout->addWidget(contentWidget);

    // QR 코드 라벨 먼저 생성 (크기 참조를 위해)
    qrCodeLabel = new QLabel("QR 코드 로딩 중...");
    qrCodeLabel->setAlignment(Qt::AlignCenter);
    qrCodeLabel->setMinimumSize(300, 300);
    qrCodeLabel->setStyleSheet("QLabel { border: 2px solid #333; background-color: white; padding: 20px; }");

    // QR 코드 테두리 너비 계산 (최소 크기 300 + 패딩 20*2 + 테두리 2*2 = 344)
    int qrBorderWidth = 344;

    QLabel *secretLabel = new QLabel(secret);
    secretLabel->setAlignment(Qt::AlignCenter);
    secretLabel->setWordWrap(true);
    secretLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    secretLabel->setFixedWidth(qrBorderWidth);  // QR 코드 테두리와 동일한 너비
    secretLabel->setFont(FontManager::getFont(FontManager::HANWHA_REGULAR, 10));
    secretLabel->setStyleSheet("QLabel { color: #000000; padding: 5px 10px 10px 10px; margin-left: 7px; }");
    contentLayout->addWidget(secretLabel, 0, Qt::AlignCenter);

    contentLayout->addWidget(qrCodeLabel, 1, Qt::AlignCenter);

    QLabel *warningLabel = new QLabel("⚠️ 경고: 이 화면을 절대 타인에게 노출하지 마세요!");
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setWordWrap(true);
    warningLabel->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 10));
    warningLabel->setStyleSheet("QLabel { color: #d32f2f; margin-top: 10px; }");
    contentLayout->addWidget(warningLabel);

    QPushButton *closeButton = new QPushButton("확인");
    closeButton->setDefault(true);
    closeButton->setFixedWidth(310);
    closeButton->setFixedHeight(40);
    closeButton->setFont(FontManager::getFont(FontManager::HANWHA_BOLD, 13));
    closeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #F89B6C;
            color: white;
            border: none;
            border-radius: 8px;
        }
        QPushButton:enabled {
            background-color: #FF6633;
        }
        QPushButton:enabled:hover {
            background-color: #E55529;
        }
    )");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    contentLayout->addWidget(closeButton, 0, Qt::AlignCenter);
}

void QRCodeDialog::setQRCodeImage(const QPixmap &pixmap)
{
    if (!pixmap.isNull()) {
        originalQRCode = pixmap;
        int margin = 20;
        int size = qMin(pixmap.width(), pixmap.height());
        QPixmap paddedPixmap(size + 2 * margin, size + 2 * margin);
        paddedPixmap.fill(Qt::white);
        QPainter painter(&paddedPixmap);
        painter.drawPixmap(margin, margin, pixmap);
        painter.end();
        int labelWidth = qrCodeLabel->width() - 40;
        int labelHeight = qrCodeLabel->height() - 40;
        int targetSize = qMin(labelWidth, labelHeight);
        targetSize = qMax(targetSize, 250);
        QPixmap scaledPixmap = paddedPixmap.scaled(targetSize, targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qrCodeLabel->setPixmap(scaledPixmap);
    } else {
        qrCodeLabel->setText("QR 코드 표시 실패");
    }
}

void QRCodeDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (!originalQRCode.isNull()) {
        setQRCodeImage(originalQRCode);
    }
}
