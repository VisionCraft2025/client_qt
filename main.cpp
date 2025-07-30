#include "login_window.h"
#include "home.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LoginWindow *login = new LoginWindow();
    Home *home = nullptr;

    QObject::connect(login, &LoginWindow::loginSuccess, [&]() {
        home = new Home();
        home->show();
        login->close();
        login->deleteLater();
    });

    login->show();
    return a.exec();
}
