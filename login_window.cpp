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
#include <QFontDatabase>
#include <QApplication>

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(std::make_unique<QNetworkAccessManager>(this))
    , qrDialog(nullptr)
{
    loadCustomFonts();
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
    setWindowTitle("VisionCraft Login");
    setMinimumSize(400, 450);  // 최소 높이를 450으로 증가
    resize(450, 500);          // 기본 높이도 증가
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

void LoginWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
}

QWidget* LoginWindow::createLoginWidget()
{
    QWidget *widget = new QWidget();
    widget->setObjectName("loginBg");
    widget->setStyleSheet(R"(
        #loginBg {
            background-color: #FBFBFB;
        }
    )");

    QVBoxLayout *outerLayout = new QVBoxLayout(widget);
    outerLayout->setObjectName("outerLayout");
    outerLayout->setContentsMargins(30, 30, 30, 30);

    // 상단 여백 추가
    outerLayout->addStretch();

    // 카드를 중앙에 배치하기 위한 수평 레이아웃
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setObjectName("centerLayout");
    centerLayout->addStretch();  // 좌측 여백

    // 카드 컨테이너 (그림자 효과 포함)
    QWidget *card = new QWidget();
    card->setObjectName("loginCard");
    card->setMaximumWidth(420);  // 최대 너비 제한 추가
    card->setMinimumWidth(300);  // 최소 너비 보장
    card->setStyleSheet(R"(
        #loginCard {
            background-color: #fff;
            border-radius: 16px;
        }
    )");

    // 그림자 효과 추가
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 60));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setObjectName("cardLayout");
    cardLayout->setSpacing(16);
    cardLayout->setContentsMargins(30, 30, 30, 30);
    cardLayout->setAlignment(Qt::AlignTop);

    // 잠금 아이콘
    QLabel *iconLabel = new QLabel();
    iconLabel->setObjectName("iconLabel");
    iconLabel->setPixmap(QPixmap(":/assets/lock_icon.png").scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(iconLabel);

    // 타이틀 (이미지로 교체) - 리소스 경로 사용, 크기 더 줄임
    QLabel *titleLabel = new QLabel();
    titleLabel->setObjectName("titleLabel");
    titleLabel->setPixmap(QPixmap(":/images/visioncraft1.png").scaled(150, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLabel->setAlignment(Qt::AlignLeft);
    titleLabel->setFixedWidth(300);  // 입력 칸과 동일한 너비
    cardLayout->addWidget(titleLabel, 0, Qt::AlignCenter);  // 타이틀 자체는 중앙에 배치

    // 사용자 ID 입력
    QWidget *idRow = new QWidget();
    QVBoxLayout *idRowLayout = new QVBoxLayout(idRow);
    idRowLayout->setContentsMargins(0, 0, 0, 0);
    idRowLayout->setSpacing(2);
    idRowLayout->setAlignment(Qt::AlignCenter);  // 가운데 정렬 추가
    loginIdEdit = new QLineEdit();
    loginIdEdit->setObjectName("loginIdEdit");
    loginIdEdit->setPlaceholderText("아이디");
    loginIdEdit->setFixedWidth(300);  // 고정 너비로 확실히 줄임
    loginIdEdit->setMinimumHeight(45);  // 세로 길이 증가
    loginIdEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f8f8f8;
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 12px;
            font-size: 15px;
            color: #333;
        }
        QLineEdit::placeholder {
            color: #ddd;
        }
        QLineEdit:focus {
            border: 2px solid #f37321;
        }
    )");
    idRowLayout->addWidget(loginIdEdit, 0, Qt::AlignCenter);  // 가운데 정렬 추가
    cardLayout->addWidget(idRow);

    // 입력 칸들 사이 간격 조정
    cardLayout->addSpacing(-8);  // 음수 여백으로 간격 줄임

    // OTP 입력
    QWidget *otpRow = new QWidget();
    QVBoxLayout *otpRowLayout = new QVBoxLayout(otpRow);
    otpRowLayout->setContentsMargins(0, 0, 0, 0);
    otpRowLayout->setSpacing(2);
    otpRowLayout->setAlignment(Qt::AlignCenter);  // 가운데 정렬 추가
    loginOtpEdit = new QLineEdit();
    loginOtpEdit->setObjectName("loginOtpEdit");
    loginOtpEdit->setPlaceholderText("인증번호");
    loginOtpEdit->setMaxLength(6);
    loginOtpEdit->setFixedWidth(300);  // 고정 너비로 확실히 줄임
    loginOtpEdit->setMinimumHeight(45);  // 세로 길이 증가
    loginOtpEdit->setAlignment(Qt::AlignLeft);
    loginOtpEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f8f8f8;
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 12px;
            font-size: 15px;
            letter-spacing: 1px;
            color: #333;
        }
        QLineEdit::placeholder {
            color: #ddd;
        }
        QLineEdit:focus {
            border: 2px solid #f37321;
        }
    )");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(QRegularExpression("^[0-9]{0,6}$"), loginOtpEdit);
    loginOtpEdit->setValidator(validator);
    connect(loginOtpEdit, &QLineEdit::textChanged, this, &LoginWindow::onOtpTextChanged);
    connect(loginOtpEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);  // 엔터 키 이벤트 추가
    otpRowLayout->addWidget(loginOtpEdit, 0, Qt::AlignCenter);  // 가운데 정렬 추가
    cardLayout->addWidget(otpRow);

    // 로그인 버튼
    loginButton = new QPushButton("로그인");
    loginButton->setObjectName("loginButton");
    loginButton->setEnabled(false);
    loginButton->setFixedWidth(300);  // 입력 칸과 동일한 300px
    loginButton->setFixedHeight(49);  // 입력 칸(45px) + 테두리(2px*2) = 49px
    loginButton->setStyleSheet(R"(
        QPushButton {
            background-color: #F89B6C;
            color: white;
            border: none;
            border-radius: 8px;
            font-weight: 900;
            font-size: 17px;
            font-family: "Hanwha Gothic B";
        }
        QPushButton:enabled {
            background-color: #FF6633;
        }
        QPushButton:enabled:hover {
            background-color: #E55529;
        }
    )");
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    cardLayout->addWidget(loginButton, 0, Qt::AlignCenter);  // 가운데 정렬 추가

    // 회원가입 링크
    QPushButton *switchButton = new QPushButton("계정등록");
    switchButton->setObjectName("switchButton");
    switchButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            color: #6B7280;
            font-weight: bold;
            font-size: 16px;
        }
        QPushButton:hover {
            color: #374151;
        }
    )");
    connect(switchButton, &QPushButton::clicked, this, &LoginWindow::showRegisterPage);
    cardLayout->addWidget(switchButton, 0, Qt::AlignCenter);

    // 카드를 중앙 레이아웃에 추가
    centerLayout->addWidget(card);
    centerLayout->addStretch();  // 우측 여백

    outerLayout->addLayout(centerLayout);
    outerLayout->addStretch();  // 하단 여백

    // 푸터
    QLabel *footer = new QLabel("© 2025 VisionCraft. All rights reserved.");
    footer->setObjectName("footer");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    outerLayout->addSpacing(16);
    outerLayout->addWidget(footer);

    return widget;
}

QWidget* LoginWindow::createRegisterWidget()
{
    QWidget *widget = new QWidget();
    widget->setObjectName("registerBg");
    widget->setStyleSheet(R"(
        #registerBg {
            background-color: #FBFBFB;
        }
    )");

    QVBoxLayout *outerLayout = new QVBoxLayout(widget);
    outerLayout->setObjectName("registerOuterLayout");
    outerLayout->setContentsMargins(30, 30, 30, 30);

    // 상단 여백 추가
    outerLayout->addStretch();

    // 카드를 중앙에 배치하기 위한 수평 레이아웃
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setObjectName("registerCenterLayout");
    centerLayout->addStretch();  // 좌측 여백

    // 카드 컨테이너 (그림자 효과 포함) - 로그인 창과 동일
    QWidget *card = new QWidget();
    card->setObjectName("registerCard");
    card->setMaximumWidth(420);  // 최대 너비 제한 추가
    card->setMinimumWidth(300);  // 최소 너비 보장
    card->setStyleSheet(R"(
        #registerCard {
            background-color: #fff;
            border-radius: 16px;
        }
    )");

    // 그림자 효과 추가 - 로그인 창과 동일
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 60));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setObjectName("registerCardLayout");
    cardLayout->setSpacing(16);
    cardLayout->setContentsMargins(30, 30, 30, 30);
    cardLayout->setAlignment(Qt::AlignTop);

    // 잠금 아이콘 - 로그인 창과 동일
    QLabel *iconLabel = new QLabel();
    iconLabel->setObjectName("registerIconLabel");
    iconLabel->setPixmap(QPixmap(":/assets/lock_icon.png").scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(iconLabel);

    // 타이틀 (이미지로 교체) - 로그인 창과 완전히 동일
    QLabel *titleLabel = new QLabel();
    titleLabel->setObjectName("registerTitleLabel");
    titleLabel->setPixmap(QPixmap(":/images/visioncraft1.png").scaled(150, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLabel->setAlignment(Qt::AlignLeft);  // 로그인 창과 정확히 동일
    titleLabel->setFixedWidth(300);  // 입력 칸과 동일한 너비
    cardLayout->addWidget(titleLabel, 0, Qt::AlignCenter);  // 타이틀 자체는 중앙에 배치

    // 계정 등록 서브타이틀 - vision과 동일한 크기, 더 얇은 굵기
    QLabel *subtitleLabel = new QLabel("Register");
    subtitleLabel->setObjectName("registerSubtitleLabel");
    subtitleLabel->setStyleSheet("color: #333; font-size: 28px; font-weight: 300;");
    subtitleLabel->setAlignment(Qt::AlignLeft);  // visioncraft와 동일한 정렬
    subtitleLabel->setFixedWidth(300);  // 입력 칸과 동일한 너비
    cardLayout->addWidget(subtitleLabel, 0, Qt::AlignCenter);  // 서브타이틀도 중앙에 배치

    // 줄어든 간격 (카드 크기 보상을 위해)
    cardLayout->addSpacing(5);

    // 사용자 ID 입력 - 로그인 창과 동일 구조
    QWidget *idRow = new QWidget();
    QVBoxLayout *idRowLayout = new QVBoxLayout(idRow);
    idRowLayout->setContentsMargins(0, 0, 0, 0);
    idRowLayout->setSpacing(2);
    idRowLayout->setAlignment(Qt::AlignCenter);  // 가운데 정렬 추가
    registerIdEdit = new QLineEdit();
    registerIdEdit->setObjectName("registerIdEdit");
    registerIdEdit->setPlaceholderText("아이디");
    registerIdEdit->setFixedWidth(300);  // 고정 너비로 확실히 줄임
    registerIdEdit->setMinimumHeight(45);  // 세로 길이 증가
    registerIdEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #f8f8f8;
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 12px;
            font-size: 15px;
        }
        QLineEdit:focus {
            border: 2px solid #f37321;
        }
    )");
    idRowLayout->addWidget(registerIdEdit, 0, Qt::AlignCenter);  // 가운데 정렬 추가
    cardLayout->addWidget(idRow);

    // 입력 칸들 사이 간격 조정 - 로그인 창과 동일
    cardLayout->addSpacing(-8);  // 음수 여백으로 간격 줄임

    // 등록 버튼까지의 간격 줄이기 (빈 공간 제거)
    // 빈 공간 대신 작은 여백만 추가
    cardLayout->addSpacing(10);

    // 등록 버튼 - 로그인 버튼과 동일한 위치와 스타일
    registerButton = new QPushButton("등록");
    registerButton->setObjectName("registerButton");
    registerButton->setEnabled(false);
    registerButton->setFixedWidth(300);  // 입력 칸과 동일한 300px
    registerButton->setFixedHeight(49);  // 입력 칸(45px) + 테두리(2px*2) = 49px
    registerButton->setStyleSheet(R"(
        QPushButton {
            background-color: #F89B6C;
            color: white;
            border: none;
            border-radius: 8px;
            font-weight: 900;
            font-size: 17px;
            font-family: "Hanwha Gothic B";
        }
        QPushButton:enabled {
            background-color: #FF6633;
        }
        QPushButton:enabled:hover {
            background-color: #E55529;
        }
    )");
    connect(registerIdEdit, &QLineEdit::textChanged, this, &LoginWindow::onRegisterIdTextChanged);
    connect(registerIdEdit, &QLineEdit::returnPressed, this, &LoginWindow::onRegisterClicked);
    connect(registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    cardLayout->addWidget(registerButton, 0, Qt::AlignCenter);  // 가운데 정렬 추가

    // 로그인 링크 - 계정등록 링크와 동일한 위치와 스타일
    QPushButton *switchButton = new QPushButton("로그인 화면으로 이동");
    switchButton->setObjectName("registerSwitchButton");
    switchButton->setStyleSheet(R"(
        QPushButton {
            border: none;
            color: #6B7280;
            font-weight: bold;
            font-size: 16px;
        }
        QPushButton:hover {
            color: #374151;
        }
    )");
    connect(switchButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);
    cardLayout->addWidget(switchButton, 0, Qt::AlignCenter);

    // 카드를 중앙 레이아웃에 추가
    centerLayout->addWidget(card);
    centerLayout->addStretch();  // 우측 여백

    outerLayout->addLayout(centerLayout);
    outerLayout->addStretch();  // 하단 여백

    // 푸터 - 로그인 창과 동일
    QLabel *footer = new QLabel("© 2025 VisionCraft. All rights reserved.");
    footer->setObjectName("registerFooter");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    outerLayout->addSpacing(16);
    outerLayout->addWidget(footer);

    return widget;
}

void LoginWindow::updateResponsiveLayout()
{
    // 반응형 기능을 비활성화 - 화면 크기와 상관없이 항상 동일한 레이아웃 유지
    // 이 함수를 비워두어 이미지와 카드 크기가 변경되지 않도록 함
}

void LoginWindow::applyCompactMode(QWidget* widget)
{
    // 이 함수는 더 이상 사용되지 않지만 호환성을 위해 유지
}

void LoginWindow::applyExtraCompactMode(QWidget* widget)
{
    // 이 함수는 더 이상 사용되지 않지만 호환성을 위해 유지
}

void LoginWindow::applyNormalMode(QWidget* widget)
{
    // 이 함수는 더 이상 사용되지 않지만 호환성을 위해 유지
}

void LoginWindow::showLoginPage()
{
    stackedWidget->setCurrentIndex(0);
    // this->showFullScreen();
}

void LoginWindow::showRegisterPage()
{
    stackedWidget->setCurrentIndex(1);
    // this->showFullScreen();
}

void LoginWindow::onOtpTextChanged(const QString &text)
{
    loginButton->setEnabled(text.length() == 6 && !loginIdEdit->text().isEmpty());
}

void LoginWindow::onRegisterIdTextChanged(const QString &text)
{
    registerButton->setEnabled(!text.trimmed().isEmpty());
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
        showMessage("로그인 실패", "아이디 또는 인증번호가 일치하지 않습니다.");  // 구체적인 에러 메시지 제거
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
        showMessage("등록 실패", "이미 등록되어 있는 아이디입니다.");  // 메시지 변경
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
            // QR 코드 다이얼로그에서 확인을 눌렀을 때 등록 성공 메시지 표시
            showMessage("등록 성공", "계정이 성공적으로 등록되었습니다.");
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
    QWidget *overlay = nullptr;

    // 로그인 실패, 등록 실패, 등록 성공, 로그인 성공 메시지에 특별한 스타일 적용
    if (title == "로그인 실패" || title == "등록 실패" || title == "등록 성공" || title == "로그인 성공") {
        // 오버레이 위젯 생성
        overlay = new QWidget(this);
        overlay->setStyleSheet("background-color: rgba(0, 0, 0, 150);");
        overlay->setGeometry(this->rect());
        overlay->setFocusPolicy(Qt::StrongFocus);  // 포커스 받을 수 있도록 설정
        overlay->grabKeyboard();  // 키보드 입력을 가로챔
        overlay->show();

        // 커스텀 팝업 위젯 생성
        QWidget *popup = new QWidget(overlay);
        popup->setFixedSize(380, 220);
        popup->setStyleSheet(R"(
            QWidget {
                background-color: white;
                border: 1px solid #ddd;
                border-radius: 12px;
            }
        )");

        // 팝업을 화면 중앙에 배치
        int x = (overlay->width() - popup->width()) / 2;
        int y = (overlay->height() - popup->height()) / 2;
        popup->move(x, y);

        // 레이아웃 생성
        QVBoxLayout *layout = new QVBoxLayout(popup);
        layout->setContentsMargins(0, 50, 0, 0);  // 위쪽 여백을 늘려서 텍스트를 아래로 이동
        layout->setSpacing(20);

        // 제목 라벨
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-family: 'Hanwha Gothic B'; font-size: 24px; font-weight: 900; color: #000; border: none;");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);

        // 메시지 라벨
        QLabel *messageLabel = new QLabel(message);
        messageLabel->setStyleSheet("font-family: 'Hanwha Gothic B'; font-size: 14px; color: #555; border: none; font-weight: bold;");
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setWordWrap(true);
        layout->addWidget(messageLabel);

        // 간격 줄이기
        layout->addSpacing(-10);

        // 추가 메시지 라벨
        QLabel *additionalLabel = new QLabel("다시 시도해주시기 바랍니다.");
        additionalLabel->setStyleSheet("font-family: 'Hanwha Gothic B'; font-size: 14px; color: #FFFFFF; border: none; font-weight: bold;");
        additionalLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(additionalLabel);

        // 스페이서 추가 (버튼을 아래로 밀기 위해)
        layout->addStretch();

        // 확인 버튼
        QPushButton *okButton = new QPushButton("확인");
        okButton->setFixedSize(380, 50);
        okButton->setStyleSheet(R"(
            QPushButton {
                background-color: #FBFBFB;
                color: #FF6633;
                border: none;
                border-top: 1px solid #ccc;
                border-top-left-radius: 0px;
                border-top-right-radius: 0px;
                border-bottom-left-radius: 12px;
                border-bottom-right-radius: 12px;
                font-weight: bold;
                font-size: 14px;
                font-family: "Hanwha Gothic B";
            }
            QPushButton:hover {
                background-color: #e0e0e0;
                color: #FF6633;
            }
            QPushButton:pressed {
                background-color: #d0d0d0;
                color: #FF6633;
            }
        )");

        // 버튼을 왼쪽으로 배치
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addWidget(okButton);
        buttonLayout->addStretch();
        layout->addLayout(buttonLayout);

        // 버튼 클릭 시 팝업 닫기
        connect(okButton, &QPushButton::clicked, [overlay]() {
            overlay->releaseKeyboard();  // 키보드 가로채기 해제
            overlay->deleteLater();
        });

        popup->show();
        return;
    }

    // 다른 메시지들은 기존 QMessageBox 사용
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(R"(
        QMessageBox {
            background-color: white;
            border: 1px solid #ddd;
            border-radius: 8px;
        }
        QMessageBox QLabel {
            color: #333;
            font-size: 14px;
            font-family: "Hanwha Gothic R";
            background-color: transparent;
            padding: 10px;
        }
        QMessageBox QPushButton {
            background-color: #FF6633;
            color: white;
            border: none;
            border-radius: 6px;
            font-weight: bold;
            font-size: 12px;
            font-family: "Hanwha Gothic B";
            min-width: 50px;
            min-height: 25px;
            padding: 4px 8px;
        }
        QMessageBox QPushButton:hover {
            background-color: #E55529;
        }
        QMessageBox QPushButton:pressed {
            background-color: #CC4422;
        }
    )");

    msgBox.exec();
}



void LoginWindow::loadCustomFonts()
{
    // Regular 폰트 로드
    int regularFontId = QFontDatabase::addApplicationFont(":/fonts/fonts/05HanwhaGothicR.ttf");
    if (regularFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(regularFontId);
        if (!fontFamilies.isEmpty()) {
            qDebug() << "Loaded Regular font:" << fontFamilies.first();
            // 기본 폰트로 설정
            QFont appFont(fontFamilies.first(), 10);
            QApplication::setFont(appFont);
        }
    } else {
        qDebug() << "Failed to load Regular font";
    }

    // Bold 폰트 로드
    int boldFontId = QFontDatabase::addApplicationFont(":/fonts/fonts/04HanwhaGothicB.ttf");
    if (boldFontId != -1) {
        QStringList boldFontFamilies = QFontDatabase::applicationFontFamilies(boldFontId);
        if (!boldFontFamilies.isEmpty()) {
            qDebug() << "Loaded Bold font:" << boldFontFamilies.first();
        }
    } else {
        qDebug() << "Failed to load Bold font";
    }

    // Light 폰트 로드
    int lightFontId = QFontDatabase::addApplicationFont(":/fonts/fonts/06HanwhaGothicL.ttf");
    if (lightFontId != -1) {
        QStringList lightFontFamilies = QFontDatabase::applicationFontFamilies(lightFontId);
        if (!lightFontFamilies.isEmpty()) {
            qDebug() << "Loaded Light font:" << lightFontFamilies.first();
        }
    } else {
        qDebug() << "Failed to load Light font";
    }

    // Extra Light 폰트 로드
    int extraLightFontId = QFontDatabase::addApplicationFont(":/fonts/fonts/07HanwhaGothicEL.ttf");
    if (extraLightFontId != -1) {
        QStringList extraLightFontFamilies = QFontDatabase::applicationFontFamilies(extraLightFontId);
        if (!extraLightFontFamilies.isEmpty()) {
            qDebug() << "Loaded Extra Light font:" << extraLightFontFamilies.first();
        }
    } else {
        qDebug() << "Failed to load Extra Light font";
    }
}
