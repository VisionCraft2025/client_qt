#ifndef LOGIN_WINDOW_H
#define LOGIN_WINDOW_H
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QResizeEvent>
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
    void resizeEvent(QResizeEvent *event) override;  // 반응형을 위한 추가
private slots:
    void showLoginPage();
    void showRegisterPage();
    void onLoginClicked();
    void onLoginResponse();
    void onRegisterClicked();
    void onRegisterResponse();
    void onQRCodeDownloaded();
    void onOtpTextChanged(const QString &text);
    void onRegisterIdTextChanged(const QString &text);
private:
    void setupUI();
    void loadCustomFonts();
    QWidget* createLoginWidget();
    QWidget* createRegisterWidget();
    void sendLoginRequest(const QString &userId, const QString &otpCode);
    void sendRegisterRequest(const QString &userId);
    void downloadQRCode(const QString &url, const QString &userId, const QString &secret);
    void showMessage(const QString &title, const QString &message);
    // 반응형 기능을 위한 추가 함수들
    void updateResponsiveLayout();
    void applyCompactMode(QWidget* widget);
    void applyNormalMode(QWidget* widget);
    void applyExtraCompactMode(QWidget* widget);  // 매우 작은 화면용 추가
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
    // 반응형을 위한 추가 멤버 변수
    bool isCompactMode = false;
    static constexpr const char* SERVER_URL = "http://auth.kwon.pics:8443";
};
#endif // LOGIN_WINDOW_H
