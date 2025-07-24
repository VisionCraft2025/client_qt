#include "qr_code_dialog.h"
#include <QVBoxLayout>
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
    setWindowTitle("Google Authenticator QR 코드");
    setModal(true);
    setMinimumSize(400, 500);
    resize(450, 550);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);
    QLabel *userLabel = new QLabel(QString("사용자: %1").arg(userId));
    userLabel->setAlignment(Qt::AlignCenter);
    QFont userFont = userLabel->font();
    userFont.setPointSize(12);
    userLabel->setFont(userFont);
    layout->addWidget(userLabel);
    QLabel *secretTitleLabel = new QLabel("시크릿 키:");
    secretTitleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = secretTitleLabel->font();
    titleFont.setBold(true);
    secretTitleLabel->setFont(titleFont);
    layout->addWidget(secretTitleLabel);
    QLabel *secretLabel = new QLabel(secret);
    secretLabel->setAlignment(Qt::AlignCenter);
    secretLabel->setWordWrap(true);
    secretLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    secretLabel->setStyleSheet("QLabel { font-family: monospace; font-size: 11pt; background-color: #f0f0f0; padding: 10px; border: 1px solid #ccc; border-radius: 4px; }");
    layout->addWidget(secretLabel);
    qrCodeLabel = new QLabel("QR 코드 로딩 중...");
    qrCodeLabel->setAlignment(Qt::AlignCenter);
    qrCodeLabel->setMinimumSize(300, 300);
    qrCodeLabel->setStyleSheet("QLabel { border: 2px solid #333; background-color: white; padding: 20px; }");
    layout->addWidget(qrCodeLabel, 1, Qt::AlignCenter);
    QLabel *warningLabel = new QLabel("⚠️ 경고: 이 QR 코드와 시크릿 키를 절대 타인에게 노출하지 마세요!");
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("QLabel { color: #d32f2f; font-weight: bold; margin-top: 10px; }");
    layout->addWidget(warningLabel);
    QPushButton *closeButton = new QPushButton("확인");
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignCenter);
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
