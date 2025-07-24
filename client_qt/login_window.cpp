#include "login_window.h"
#include "qr_code_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPixmap>
#include <QRegularExpressionValidator>
#include <QGroupBox>

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(std::make_unique<QNetworkAccessManager>(this))
    , qrDialog(nullptr)
{
    setupUI();
}

LoginWindow::~LoginWindow()
{
    if (qrDialog) {
        delete qrDialog;
    }
}

void LoginWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(createLoginWidget());
    stackedWidget->addWidget(createRegisterWidget());
    mainLayout->addWidget(stackedWidget);
    setWindowTitle("MFA 인증 시스템");
    setMinimumSize(400, 350);
    resize(450, 400);
    showLoginPage();
}

QWidget* LoginWindow::createLoginWidget()
{
    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(15);
    layout->setContentsMargins(30, 30, 30, 30);
    QLabel *titleLabel = new QLabel("MFA 로그인");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(titleLabel);
    QGroupBox *inputGroup = new QGroupBox("로그인 정보");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(10);
    QLabel *idLabel = new QLabel("사용자 ID:");
    loginIdEdit = new QLineEdit();
    loginIdEdit->setPlaceholderText("사용자 ID를 입력하세요");
    loginIdEdit->setMinimumHeight(30);
    inputLayout->addWidget(idLabel);
    inputLayout->addWidget(loginIdEdit);
    QLabel *otpLabel = new QLabel("인증 코드 (6자리):");
    loginOtpEdit = new QLineEdit();
    loginOtpEdit->setPlaceholderText("123456");
    loginOtpEdit->setMaxLength(6);
    loginOtpEdit->setMinimumHeight(30);
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(QRegularExpression("^[0-9]{0,6}$"), loginOtpEdit);
    loginOtpEdit->setValidator(validator);
    connect(loginOtpEdit, &QLineEdit::textChanged, this, &LoginWindow::onOtpTextChanged);
    inputLayout->addWidget(otpLabel);
    inputLayout->addWidget(loginOtpEdit);
    layout->addWidget(inputGroup);
    layout->addSpacing(10);
    loginButton = new QPushButton("로그인");
    loginButton->setEnabled(false);
    loginButton->setMinimumHeight(40);
    loginButton->setStyleSheet("QPushButton { font-size: 16px; }");
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    layout->addWidget(loginButton);
    QPushButton *switchButton = new QPushButton("계정이 없으신가요? 회원가입");
    switchButton->setStyleSheet("QPushButton { border: none; color: #0066cc; text-decoration: underline; }");
    connect(switchButton, &QPushButton::clicked, this, &LoginWindow::showRegisterPage);
    layout->addWidget(switchButton);
    layout->addStretch();
    return widget;
}

QWidget* LoginWindow::createRegisterWidget()
{
    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(15);
    layout->setContentsMargins(30, 30, 30, 30);
    QLabel *titleLabel = new QLabel("계정 등록");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(titleLabel);
    QGroupBox *inputGroup = new QGroupBox("등록 정보");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    QLabel *idLabel = new QLabel("사용자 ID:");
    registerIdEdit = new QLineEdit();
    registerIdEdit->setPlaceholderText("원하는 ID를 입력하세요");
    registerIdEdit->setMinimumHeight(30);
    inputLayout->addWidget(idLabel);
    inputLayout->addWidget(registerIdEdit);
    layout->addWidget(inputGroup);
    layout->addSpacing(10);
    registerButton = new QPushButton("등록하기");
    registerButton->setMinimumHeight(40);
    registerButton->setStyleSheet("QPushButton { font-size: 16px; }");
    connect(registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    layout->addWidget(registerButton);
    QLabel *infoLabel = new QLabel("등록 후 Google Authenticator QR 코드가 표시됩니다.");
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; margin-top: 10px;");
    layout->addWidget(infoLabel);
    QPushButton *switchButton = new QPushButton("이미 계정이 있으신가요? 로그인");
    switchButton->setStyleSheet("QPushButton { border: none; color: #0066cc; text-decoration: underline; }");
    connect(switchButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);
    layout->addWidget(switchButton);
    layout->addStretch();
    return widget;
}

void LoginWindow::showLoginPage()
{
    stackedWidget->setCurrentIndex(0);
}

void LoginWindow::showRegisterPage()
{
    stackedWidget->setCurrentIndex(1);
}

void LoginWindow::onOtpTextChanged(const QString &text)
{
    loginButton->setEnabled(text.length() == 6 && !loginIdEdit->text().isEmpty());
}

void LoginWindow::onLoginClicked()
{
    QString userId = loginIdEdit->text().trimmed();
    QString otpCode = loginOtpEdit->text();
    if (userId.isEmpty()) {
        showMessage("오류", "사용자 ID를 입력해주세요.");
        return;
    }
    if (otpCode.length() != 6) {
        showMessage("오류", "인증 코드는 6자리여야 합니다.");
        return;
    }
    sendLoginRequest(userId, otpCode);
}

void LoginWindow::onRegisterClicked()
{
    QString userId = registerIdEdit->text().trimmed();
    if (userId.isEmpty()) {
        showMessage("오류", "사용자 ID를 입력해주세요.");
        return;
    }
    sendRegisterRequest(userId);
}

void LoginWindow::sendLoginRequest(const QString &userId, const QString &otpCode)
{
    QNetworkRequest request(QUrl(QString("%1/api/authenticate").arg(SERVER_URL)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject json;
    json["user_id"] = userId;
    json["otp_code"] = otpCode;
    QJsonDocument doc(json);
    QNetworkReply *reply = networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &LoginWindow::onLoginResponse);
    loginButton->setEnabled(false);
    loginButton->setText("인증 중...");
}

void LoginWindow::sendRegisterRequest(const QString &userId)
{
    QNetworkRequest request(QUrl(QString("%1/api/register").arg(SERVER_URL)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject json;
    json["user_id"] = userId;
    QJsonDocument doc(json);
    QNetworkReply *reply = networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &LoginWindow::onRegisterResponse);
    registerButton->setEnabled(false);
    registerButton->setText("등록 중...");
}

void LoginWindow::onLoginResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    loginButton->setEnabled(true);
    loginButton->setText("로그인");
    if (reply->error() != QNetworkReply::NoError) {
        showMessage("로그인 실패", "서버 연결에 실패했습니다: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();
    bool success = obj["success"].toBool();
    QString message = obj["message"].toString();
    if (success) {
        showMessage("로그인 성공", "인증이 완료되었습니다!");
        loginOtpEdit->clear();
        emit loginSuccess(); // 로그인 성공 시그널 emit
    } else {
        showMessage("로그인 실패", message.isEmpty() ? "인증에 실패했습니다." : message);
    }
    reply->deleteLater();
}

void LoginWindow::onRegisterResponse()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    registerButton->setEnabled(true);
    registerButton->setText("등록하기");
    if (reply->error() != QNetworkReply::NoError) {
        showMessage("등록 실패", "서버 연결에 실패했습니다: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();
    bool success = obj["success"].toBool();
    if (success) {
        QString userId = obj["user_id"].toString();
        QString secret = obj["secret"].toString();
        QString qrUrl = obj["qr_code_url"].toString();
        if (!qrUrl.isEmpty() && !secret.isEmpty()) {
            downloadQRCode(qrUrl, userId, secret);
        } else {
            showMessage("등록 실패", "QR 코드 정보를 받아올 수 없습니다.");
        }
    } else {
        QString message = obj["message"].toString();
        showMessage("등록 실패", message.isEmpty() ? "등록에 실패했습니다." : message);
    }
    reply->deleteLater();
}

void LoginWindow::downloadQRCode(const QString &url, const QString &userId, const QString &secret)
{
    currentUserId = userId;
    currentSecret = secret;
    QUrl qrUrl(url);
    QNetworkRequest request(qrUrl);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &LoginWindow::onQRCodeDownloaded);
}

void LoginWindow::onQRCodeDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    if (reply->error() != QNetworkReply::NoError) {
        showMessage("오류", "QR 코드를 다운로드할 수 없습니다: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray imageData = reply->readAll();
    QPixmap pixmap;
    if (pixmap.loadFromData(imageData)) {
        if (qrDialog) {
            delete qrDialog;
        }
        qrDialog = new QRCodeDialog(currentUserId, currentSecret, this);
        qrDialog->setQRCodeImage(pixmap);
        if (qrDialog->exec() == QDialog::Accepted) {
            showMessage("등록 완료", "계정이 성공적으로 등록되었습니다.\n이제 Google Authenticator 앱의 6자리 코드로 로그인할 수 있습니다.");
        }
        delete qrDialog;
        qrDialog = nullptr;
    } else {
        showMessage("오류", "QR 코드 이미지를 표시할 수 없습니다.");
    }
    reply->deleteLater();
}

void LoginWindow::showMessage(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}
