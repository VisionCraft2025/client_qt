#include "login_window.h"
#include "home.h"
#include "font_manager.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // FontManager를 사용하여 모든 폰트 초기화
    if (!FontManager::initializeFonts()) {
        qWarning() << "폰트 초기화에 실패했습니다.";
    }

    // 기본 폰트 설정 (REGULAR 폰트 사용)
    QFont defaultFont = FontManager::getFont(FontManager::HANWHA_REGULAR, 10);
    a.setFont(defaultFont);
    qDebug() << "[✓] 기본 폰트 설정 완료:" << defaultFont.family();



    LoginWindow *login = new LoginWindow();
    Home *home = nullptr;

    QObject::connect(login, &LoginWindow::loginSuccess, [&]() {
        home = new Home();
        home->show();
        login->close();
        login->deleteLater();
    });

    login->show();


    // 로그인 건너뛰고 바로 Home 창 실행
    // Home *home = new Home();
    // home->show();

    return a.exec();
}
