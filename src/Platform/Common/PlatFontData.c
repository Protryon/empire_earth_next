/* Platform layer, every system: the game's fonts built into the program (include/platform/PlatFont.h,
   PlatFontRegisterBuiltIn). Liberation Sans, Serif and Mono 2.1.5 (SIL Open Font License 1.1; toolchain/fonts, fetched by
   tools/fetch_posix.py fonts) are metric-compatible with the faces the game's font table names, and are registered
   under those names: Arial, Times New Roman, Courier New, each in its four styles. Tahoma (the table's fourth face)
   and MS Sans Serif (the lobby's dialogs) are aliases of Arial. The files are embedded with #embed (C23; clang accepts
   it in earlier dialects), so no font file has to be installed with the game. */
#include <stdint.h>

#include "platform/PlatFont.h"

static const unsigned char kSansRegular[] = {
#embed "LiberationSans-Regular.ttf"
};
static const unsigned char kSansBold[] = {
#embed "LiberationSans-Bold.ttf"
};
static const unsigned char kSansItalic[] = {
#embed "LiberationSans-Italic.ttf"
};
static const unsigned char kSansBoldItalic[] = {
#embed "LiberationSans-BoldItalic.ttf"
};
static const unsigned char kSerifRegular[] = {
#embed "LiberationSerif-Regular.ttf"
};
static const unsigned char kSerifBold[] = {
#embed "LiberationSerif-Bold.ttf"
};
static const unsigned char kSerifItalic[] = {
#embed "LiberationSerif-Italic.ttf"
};
static const unsigned char kSerifBoldItalic[] = {
#embed "LiberationSerif-BoldItalic.ttf"
};
static const unsigned char kMonoRegular[] = {
#embed "LiberationMono-Regular.ttf"
};
static const unsigned char kMonoBold[] = {
#embed "LiberationMono-Bold.ttf"
};
static const unsigned char kMonoItalic[] = {
#embed "LiberationMono-Italic.ttf"
};
static const unsigned char kMonoBoldItalic[] = {
#embed "LiberationMono-BoldItalic.ttf"
};

int PlatFontRegisterBuiltIn(void)
{
    static const struct {
        const char* mFace;
        int mBold, mItalic;
        const unsigned char* mData;
        uint32_t mSize;
    } kFaces[] = {
        {"Arial", 0, 0, kSansRegular, sizeof(kSansRegular)},
        {"Arial", 1, 0, kSansBold, sizeof(kSansBold)},
        {"Arial", 0, 1, kSansItalic, sizeof(kSansItalic)},
        {"Arial", 1, 1, kSansBoldItalic, sizeof(kSansBoldItalic)},
        {"Times New Roman", 0, 0, kSerifRegular, sizeof(kSerifRegular)},
        {"Times New Roman", 1, 0, kSerifBold, sizeof(kSerifBold)},
        {"Times New Roman", 0, 1, kSerifItalic, sizeof(kSerifItalic)},
        {"Times New Roman", 1, 1, kSerifBoldItalic, sizeof(kSerifBoldItalic)},
        {"Courier New", 0, 0, kMonoRegular, sizeof(kMonoRegular)},
        {"Courier New", 1, 0, kMonoBold, sizeof(kMonoBold)},
        {"Courier New", 0, 1, kMonoItalic, sizeof(kMonoItalic)},
        {"Courier New", 1, 1, kMonoBoldItalic, sizeof(kMonoBoldItalic)},
    };
    int theResult = 1;
    for (unsigned i = 0; i < sizeof(kFaces) / sizeof(kFaces[0]); i++)
        if (!PlatFontRegister(kFaces[i].mFace, kFaces[i].mBold, kFaces[i].mItalic, kFaces[i].mData, kFaces[i].mSize))
            theResult = 0;
    PlatFontAlias("Tahoma", "Arial");
    PlatFontAlias("MS Sans Serif", "Arial");
    return theResult;
}
