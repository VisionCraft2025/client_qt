#include "font_manager.h"

// 정적 멤버 변수 초기화
const QString FontManager::FONT_PATHS[5] = {
    ":/fonts/fonts/04HanwhaGothicB.ttf",    // HANWHA_BOLD
    ":/fonts/fonts/05HanwhaGothicR.ttf",    // HANWHA_REGULAR
    ":/fonts/fonts/06HanwhaGothicL.ttf",    // HANWHA_LIGHT
    ":/fonts/fonts/07HanwhaGothicEL.ttf",   // HANWHA_EXTRA_LIGHT
    ":/fonts/fonts/08HanwhaGothicT.ttf"     // HANWHA_THIN
};

int FontManager::fontIds[5] = {-1, -1, -1, -1, -1};
QString FontManager::fontFamilies[5] = {"", "", "", "", ""};

bool FontManager::initializeFonts()
{
    qDebug() << "=== font setting start ===";

    bool allSuccess = true;

    for (int i = 0; i < 5; ++i) {
        if (!loadFont(static_cast<FontType>(i))) {
            allSuccess = false;
        }
    }

    if (allSuccess) {
        qDebug() << "[✓] All of fonts loaded";
    } else {
        qWarning() << "[!] font load fail";
    }

    printFontInfo();
    return allSuccess;
}

bool FontManager::loadFont(FontType type)
{
    int index = static_cast<int>(type);
    QString fontPath = FONT_PATHS[index];

    // 폰트 파일 존재 확인
    if (!QFile::exists(fontPath)) {
        qWarning() << "font family empty" << fontPath;
        return false;
    }

    // 폰트 로드
    int fontId = QFontDatabase::addApplicationFont(fontPath);

    if (fontId == -1) {
        qWarning() << "font load fail:" << fontPath;
        return false;
    }

    // 폰트 패밀리명 가져오기
    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning() << "font family empty:" << fontPath;
        return false;
    }

    // 정보 저장
    fontIds[index] = fontId;
    fontFamilies[index] = families.at(0);

    qDebug() << "[✓] font load success:" << families.at(0) << "(" << fontPath << ")";
    return true;
}

QFont FontManager::getFont(FontType type, int pointSize)
{
    int index = static_cast<int>(type);

    if (!isFontLoaded(type)) {
        qWarning() << "font load fail";
        return QFont();
    }

    QFont font;
    font.setFamily(fontFamilies[index]);
    font.setPointSize(pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);

    return font;
}

QString FontManager::getFontFamily(FontType type)
{
    int index = static_cast<int>(type);
    return fontFamilies[index];
}

bool FontManager::isFontLoaded(FontType type)
{
    int index = static_cast<int>(type);
    return fontIds[index] != -1 && !fontFamilies[index].isEmpty();
}

void FontManager::printFontInfo()
{
    qDebug() << "\n=== font info ===";
    QStringList fontNames = {"BOLD", "REGULAR", "LIGHT", "EXTRA_LIGHT", "THIN"};

    for (int i = 0; i < 5; ++i) {
        if (isFontLoaded(static_cast<FontType>(i))) {
            qDebug() << QString("[✓] %1: %2 (ID: %3)")
                            .arg(fontNames[i])
                            .arg(fontFamilies[i])
                            .arg(fontIds[i]);
        } else {
            qDebug() << QString("[✗] %1: load fail")
                            .arg(fontNames[i]);
        }
    }
    qDebug() << "================\n";
}
