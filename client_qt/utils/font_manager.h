#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QDebug>
#include <QFile>

class FontManager
{
public:
    // 폰트 타입 열거형
    enum FontType {
        HANWHA_BOLD = 0,      // 04HanwhaGothicB.ttf
        HANWHA_REGULAR = 1,   // 05HanwhaGothicR.ttf
        HANWHA_LIGHT = 2,     // 06HanwhaGothicL.ttf
        HANWHA_EXTRA_LIGHT = 3, // 07HanwhaGothicEL.ttf
        HANWHA_THIN = 4       // 08HanwhaGothicT.ttf
    };

    // 폰트 경로 매핑
    static const QString FONT_PATHS[5];

    // 폰트 ID 저장 배열
    static int fontIds[5];

    // 폰트 패밀리명 저장 배열
    static QString fontFamilies[5];

    // 모든 폰트 초기화
    static bool initializeFonts();

    // 특정 폰트 가져오기
    static QFont getFont(FontType type, int pointSize = 10);

    // 폰트 패밀리명 가져오기
    static QString getFontFamily(FontType type);

    // 폰트 로드 상태 확인
    static bool isFontLoaded(FontType type);

    // 디버그용: 모든 폰트 정보 출력
    static void printFontInfo();

private:
    static bool loadFont(FontType type);
};

#endif // FONT_MANAGER_H
