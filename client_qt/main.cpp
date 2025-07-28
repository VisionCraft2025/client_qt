#include "login_window.h"
#include "home.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString fontPath = ":/fonts/fonts/05HanwhaGothicR.ttf";
    int fontId = QFontDatabase::addApplicationFont(fontPath);

    if (fontId == -1) {
        qDebug() << "font error!";
        qDebug() << "    - path:" << fontPath;
        qDebug() << "    - file:" << QFile::exists(fontPath);
    } else {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        qDebug() << "font ID:" << fontId;
        qDebug() << "    - familylist:" << families;

        if (!families.isEmpty()) {
            QFont defaultFont;
            defaultFont.setFamily(families.at(0));
            defaultFont.setPointSize(10);
            defaultFont.setStyleStrategy(QFont::PreferAntialias);
            a.setFont(defaultFont);
            qDebug() << "[✓] normal font:" << families.at(0);
        } else {
            qWarning() << "font family empty";
        }
    }


    // 로그인 서버가 꺼져있을 때를 위한 임시 우회 코드

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
