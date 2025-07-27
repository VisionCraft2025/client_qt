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
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>


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

void LoginWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    this->showFullScreen();
}

void LoginWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        this->showNormal();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

QWidget* LoginWindow::createLoginWidget()
{
    QWidget *widget = new QWidget();
    widget->setObjectName("loginBg");
    widget->setStyleSheet(R"(
        #loginBg {
            background-image: url(:/new/prefix1/images/background_orange.png);
            background-repeat: no-repeat;
            background-position: center;
            background-size: cover;
        }
    )");

    QVBoxLayout *outerLayout = new QVBoxLayout(widget);
    outerLayout->setContentsMargins(30, 30, 30, 30);

    // 카드
    QWidget *card = new QWidget();
    card->setObjectName("loginCard");
    card->setFixedWidth(420);
    card->setStyleSheet(R"(
        #loginCard {
            background-color: #fff;
            border-radius: 16px;
            padding: 30px;
        }
    )");

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 60));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(16);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setAlignment(Qt::AlignTop);

    // 잠금 아이콘
    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QPixmap(":/assets/lock_icon.png").scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(iconLabel);

    // 타이틀
    QLabel *titleLabel = new QLabel("MFA 로그인");
    titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #333;");
    titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel("보안을 위해 인증이 필요합니다");
    descLabel->setStyleSheet("color: #666;");
    descLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(descLabel);

    // 사용자 ID 입력
    QWidget *idRow = new QWidget();
    QVBoxLayout *idRowLayout = new QVBoxLayout(idRow);
    idRowLayout->setContentsMargins(0, 0, 0, 0);
    idRowLayout->setSpacing(2);
    QLabel *idLabel = new QLabel("사용자 ID:");
    idLabel->setStyleSheet("color: #555; background: none;");
    loginIdEdit = new QLineEdit();
    loginIdEdit->setPlaceholderText("사용자 아이디를 입력하세요");
    loginIdEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f8f8f8;
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 10px;
            font-size: 15px;
        }
        QLineEdit:focus {
            border: 2px solid #f37321;
        }
    )");
    idRowLayout->addWidget(idLabel);
    idRowLayout->addWidget(loginIdEdit);
    cardLayout->addWidget(idRow);

    // OTP 입력
    QWidget *otpRow = new QWidget();
    QVBoxLayout *otpRowLayout = new QVBoxLayout(otpRow);
    otpRowLayout->setContentsMargins(0, 0, 0, 0);
    otpRowLayout->setSpacing(2);
    QLabel *otpLabel = new QLabel("인증 코드 (6자리):");
    otpLabel->setStyleSheet("color: #555; background: none;");
    loginOtpEdit = new QLineEdit();
    loginOtpEdit->setPlaceholderText("123456");
    loginOtpEdit->setMaxLength(6);
    loginOtpEdit->setAlignment(Qt::AlignCenter);
    loginOtpEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f8f8f8;
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 10px;
            font-size: 20px;
            letter-spacing: 4px;
        }
        QLineEdit:focus {
            border: 2px solid #f37321;
        }
    )");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(QRegularExpression("^[0-9]{0,6}$"), loginOtpEdit);
    loginOtpEdit->setValidator(validator);
    connect(loginOtpEdit, &QLineEdit::textChanged, this, &LoginWindow::onOtpTextChanged);
    otpRowLayout->addWidget(otpLabel);
    otpRowLayout->addWidget(loginOtpEdit);
    cardLayout->addWidget(otpRow);

    // 로그인 버튼
    loginButton = new QPushButton("로그인");
    loginButton->setEnabled(false);
    loginButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(
                x1:0, y1:0, x2:1, y2:0,
                stop:0 #f37321,
                stop:1 #f89b6c
            );
            color: white;
            border: none;
            border-radius: 10px;
            height: 44px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: qlineargradient(
                x1:0, y1:0, x2:1, y2:0,
                stop:0 #e56a1e,
                stop:1 #f7925f
            );
        }
    )");
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    cardLayout->addWidget(loginButton);

    // 회원가입 링크
    QPushButton *switchButton = new QPushButton("계정이 없으신가요? 회원가입");
    switchButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            color: #f37321;
            font-weight: bold;
            text-decoration: underline;
        }
        QPushButton:hover {
            color: #e56a1e;
        }
    )");
    connect(switchButton, &QPushButton::clicked, this, &LoginWindow::showRegisterPage);
    cardLayout->addWidget(switchButton, 0, Qt::AlignCenter);

    // 카드 중앙 배치
    outerLayout->addStretch();
    outerLayout->addWidget(card, 0, Qt::AlignCenter);
    outerLayout->addStretch();

    // 푸터
    QLabel *footer = new QLabel("© 2025 VisionCraft. All rights reserved.");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    outerLayout->addSpacing(16);
    outerLayout->addWidget(footer);

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
    this->showFullScreen();
}

void LoginWindow::showRegisterPage()
{
    stackedWidget->setCurrentIndex(1);
    this->showFullScreen();
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
