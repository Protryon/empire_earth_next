// Tests of PlatFont (include/platform/PlatFont.h): `python -I tools/xbuild.py fonttest`, argument: the directory with
// the Liberation fonts (tools/fetch_posix.py fonts). On Windows it also measures the same requests with GDI and the
// installed Arial and prints where PlatFont (Liberation Sans as "Arial") differs: the metric-compatibility record.
// Exit code: the number of failed checks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "platform/PlatFont.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static int sChecks, sFailures;
#define CHECK(c) Check((c), #c, __LINE__)
static void Check(bool inOk, const char* inText, int inLine)
{
    sChecks++;
    if (!inOk) {
        sFailures++;
        printf("FAIL line %d: %s\n", inLine, inText);
    }
}

static std::vector<unsigned char> ReadAll(const std::string& inPath)
{
    std::vector<unsigned char> theData;
    FILE* f = fopen(inPath.c_str(), "rb");
    if (!f)
        return theData;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    theData.resize(n > 0 ? static_cast<size_t>(n) : 0);
    if (n > 0 && fread(theData.data(), 1, theData.size(), f) != theData.size())
        theData.clear();
    fclose(f);
    return theData;
}

static const char kSample[] = "Empire Earth: The Art of Conquest 0123456789";

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: font_test <font directory>\n");
        return 1;
    }
    std::string theDir = argv[1];
    const char* kFiles[4][3] = {{"LiberationSans-Regular.ttf", "0", "0"}, {"LiberationSans-Bold.ttf", "1", "0"},
                                {"LiberationSans-Italic.ttf", "0", "1"}, {"LiberationSans-BoldItalic.ttf", "1", "1"}};
    std::vector<std::vector<unsigned char>> theData(4);
    for (int i = 0; i < 4; i++) {
        theData[i] = ReadAll(theDir + "/" + kFiles[i][0]);
        CHECK(!theData[i].empty());
        if (!theData[i].empty())
            CHECK(PlatFontRegister("Arial", kFiles[i][1][0] == '1', kFiles[i][2][0] == '1', theData[i].data(),
                                   static_cast<uint32_t>(theData[i].size())));
    }
    CHECK(!PlatFontRegister("Broken", 0, 0, "not a font", 10));
    CHECK(PlatFontAlias("MS Sans Serif", "Arial"));

    // metrics and widths
    PlatFont* theFont = PlatFontCreate("Arial", -12, 0, 0, 0, 0);
    CHECK(theFont != NULL);
    if (!theFont)
        return 1;
    PlatFontMetrics m;
    PlatFontGetMetrics(theFont, &m);
    printf("Arial -12: height %d ascent %d descent %d internal %d external %d ave %d max %d chars %02x..%02x\n",
           m.mHeight, m.mAscent, m.mDescent, m.mInternalLeading, m.mExternalLeading, m.mAveCharWidth, m.mMaxCharWidth,
           m.mFirstChar, m.mLastChar);
    CHECK(m.mHeight == m.mAscent + m.mDescent && m.mHeight >= 12 && m.mHeight <= 16);
    CHECK(m.mFirstChar == 0x20 && m.mLastChar == 0xFF);
    CHECK(m.mInternalLeading == m.mHeight - 12);
    int32_t theWidth = PlatFontMeasure(theFont, kSample, sizeof(kSample) - 1);
    int32_t theSum = 0;
    for (const char* p = kSample; *p; p++)
        theSum += PlatFontMeasure(theFont, p, 1);
    CHECK(theWidth == theSum && theWidth > 150 && theWidth < 350);
    CHECK(PlatFontMeasure(theFont, "", 0) == 0);
    PlatFont* theCell = PlatFontCreate("Arial", 14, 0, 0, 0, 0);   // a positive height is the cell
    PlatFontMetrics mc;
    PlatFontGetMetrics(theCell, &mc);
    CHECK(mc.mHeight == 14);
    PlatFontDestroy(theCell);
    PlatFont* theAlias = PlatFontCreate("ms sans serif", -12, 0, 0, 0, 0);
    PlatFont* theUnknown = PlatFontCreate("No Such Face", -12, 0, 0, 0, 0);   // falls back to a registered face
    CHECK(theAlias && theUnknown && PlatFontMeasure(theAlias, kSample, 10) == PlatFontMeasure(theFont, kSample, 10));
    PlatFontDestroy(theAlias);
    PlatFontDestroy(theUnknown);
    PlatFont* theBold = PlatFontCreate("Arial", -12, 1, 0, 0, 0);
    CHECK(PlatFontMeasure(theBold, kSample, sizeof(kSample) - 1) > theWidth);

    // drawing: a white 200x20 target, black text; pixels change only inside the text's cell and the clip
    const int W = 200, H = 20;
    std::vector<uint32_t> thePixels(W * H, 0xFFFFFFFFu);
    PlatFontTarget theTarget = {thePixels.data(), W * 4, W, H};
    PlatFontDraw(theFont, "Empire", 6, 10, 2, 0x000000, 0, 0, NULL, &theTarget);
    int theInk = 0, theOutside = 0;
    int32_t theTextWidth = PlatFontMeasure(theFont, "Empire", 6);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (thePixels[y * W + x] != 0xFFFFFFFFu) {
                theInk++;
                if (x < 9 || x > 10 + theTextWidth + 1 || y < 2 || y >= 2 + m.mHeight)
                    theOutside++;
            }
    CHECK(theInk > 40);
    CHECK(theOutside == 0);
    // ASCII picture of the text
    for (int y = 2; y < 2 + m.mHeight; y++) {
        printf("   |");
        for (int x = 8; x < 12 + theTextWidth; x++)
            putchar(thePixels[y * W + x] == 0xFFFFFFFFu ? ' ' : '#');
        printf("|\n");
    }
    // clipping and the opaque background
    std::fill(thePixels.begin(), thePixels.end(), 0xFFFFFFFFu);
    PlatFontRect theClip = {0, 0, 20, H};
    PlatFontDraw(theFont, "Empire", 6, 10, 2, 0x000000, 1, 0x0000FF, &theClip, &theTarget);
    int theBlue = 0, thePastClip = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (thePixels[y * W + x] == 0xFF0000FFu)
                theBlue++;
            if (x >= 20 && thePixels[y * W + x] != 0xFFFFFFFFu)
                thePastClip++;
        }
    CHECK(theBlue > 0 && thePastClip == 0);
    // antialiased text has intermediate shades
    PlatFont* theSmooth = PlatFontCreate("Arial", -24, 0, 0, 0, 1);
    std::vector<uint32_t> theBig(100 * 40, 0xFFFFFFFFu);
    PlatFontTarget theBigTarget = {theBig.data(), 100 * 4, 100, 40};
    PlatFontDraw(theSmooth, "Ag", 2, 2, 2, 0x000000, 0, 0, NULL, &theBigTarget);
    int theGrey = 0;
    for (uint32_t p : theBig)
        if ((p & 0xFF) != 0 && (p & 0xFF) != 0xFF)
            theGrey++;
    CHECK(theGrey > 0);
    PlatFontDestroy(theSmooth);

#if defined(_WIN32)
    // GDI with the installed Arial, the same requests: the metric-compatibility record (printed, not checked)
    HDC theDC = CreateCompatibleDC(NULL);
    int theDiffers = 0, theCompared = 0;
    const int kSizes[] = {8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 36, 48, 72};
    for (int theStyle = 0; theStyle < 2; theStyle++)
        for (int theSize : kSizes) {
            HFONT theGDIFont = CreateFontA(-theSize, 0, 0, 0, theStyle ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
                                           "Arial");
            HGDIOBJ theOld = SelectObject(theDC, theGDIFont);
            TEXTMETRICA tm;
            GetTextMetricsA(theDC, &tm);
            SIZE theExtent;
            GetTextExtentPoint32A(theDC, kSample, sizeof(kSample) - 1, &theExtent);
            SelectObject(theDC, theOld);
            DeleteObject(theGDIFont);
            PlatFont* f = PlatFontCreate("Arial", -theSize, theStyle, 0, 0, 0);
            PlatFontMetrics pm;
            PlatFontGetMetrics(f, &pm);
            int32_t pw = PlatFontMeasure(f, kSample, sizeof(kSample) - 1);
            PlatFontDestroy(f);
            theCompared++;
            bool theSame = pm.mHeight == tm.tmHeight && pm.mAscent == tm.tmAscent && pw == theExtent.cx;
            if (!theSame)
                theDiffers++;
            printf("  %s %2d: GDI h %2ld a %2ld w %4ld | PlatFont h %2d a %2d w %4d %s\n", theStyle ? "bold" : "    ",
                   theSize, tm.tmHeight, tm.tmAscent, theExtent.cx, pm.mHeight, pm.mAscent, pw,
                   theSame ? "" : "<- differs");
        }
    DeleteDC(theDC);
    printf("GDI comparison: %d of %d requests differ\n", theDiffers, theCompared);
#endif

    PlatFontDestroy(theBold);
    PlatFontDestroy(theFont);
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures;
}
