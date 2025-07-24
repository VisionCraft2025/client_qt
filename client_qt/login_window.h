#ifndef LOGIN_WINDOW_H
#define LOGIN_WINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QPixmap;
QT_END_NAMESPACE

class QRCodeDialog;

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSuccess(); // 로그인 성공 시그널 추가

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void showLoginPage();
    void showRegisterPage();
    void onLoginClicked();
    void onLoginResponse();
    void onRegisterClicked();
    void onRegisterResponse();
    void onQRCodeDownloaded();
    void onOtpTextChanged(const QString &text);

private:
    void setupUI();
    QWidget* createLoginWidget();
    QWidget* createRegisterWidget();
    void sendLoginRequest(const QString &userId, const QString &otpCode);
    void sendRegisterRequest(const QString &userId);
    void downloadQRCode(const QString &url, const QString &userId, const QString &secret);
    void showMessage(const QString &title, const QString &message);

    QStackedWidget *stackedWidget;
    QLineEdit *loginIdEdit;
    QLineEdit *loginOtpEdit;
    QPushButton *loginButton;
    QLineEdit *registerIdEdit;
    QPushButton *registerButton;
    std::unique_ptr<QNetworkAccessManager> networkManager;
    QRCodeDialog *qrDialog;
    QString currentUserId;
    QString currentSecret;
    static constexpr const char* SERVER_URL = "http://auth.kwon.pics:8443";
};

#endif // LOGIN_WINDOW_H
