#include "mainwindow.h"
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
            qDebug() << "[✓] nomal font:" << families.at(0);
        } else {
            qWarning() << "font family empty";
        }
    }

    Home home;
    home.show();
    return a.exec();
}
