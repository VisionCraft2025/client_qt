#ifndef QR_CODE_DIALOG_H
#define QR_CODE_DIALOG_H
#include <QDialog>
#include <QPainter>
#include <memory>

QT_BEGIN_NAMESPACE
class QLabel;
class QPixmap;
class QHBoxLayout;  // 추가된 전방 선언
QT_END_NAMESPACE

class QRCodeDialog : public QDialog
{
    Q_OBJECT
public:
    QRCodeDialog(const QString &userId, const QString &secret, QWidget *parent = nullptr);
    ~QRCodeDialog();
    void setQRCodeImage(const QPixmap &pixmap);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI(const QString &userId, const QString &secret);
    QLabel *qrCodeLabel;
    QPixmap originalQRCode;
};

#endif // QR_CODE_DIALOG_H
