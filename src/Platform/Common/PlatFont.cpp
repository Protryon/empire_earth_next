// Platform layer, every system: fonts and text drawing on stb_truetype (include/platform/PlatFont.h).
//
// GDI's metrics are rebuilt from the font's tables as Windows computes them for a TrueType font: the scale is the
// requested em height over unitsPerEm (or the cell height over usWinAscent + usWinDescent for a positive height),
// tmAscent / tmDescent are the OS/2 table's usWinAscent / usWinDescent scaled and rounded, the advance of each
// character is its advance width scaled and rounded (GDI's hinted widths can differ by a pixel at small sizes: the
// open point in PORT.md). A font renders all of its 224 characters when it is created and never changes after, so
// drawing needs no lock. A bold request without a bold face is GDI's synthetic bold: one more pixel of advance and
// every glyph drawn twice, one pixel apart.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <mutex>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "platform/PlatFont.h"

namespace {

struct Face {
    std::string mName;
    bool mBold;
    bool mItalic;
    const unsigned char* mData;
    stbtt_fontinfo mInfo;
};

struct Alias {
    std::string mName;
    std::string mTarget;
};

std::mutex sFaceLock;
std::vector<Face*> sFaces;
std::vector<Alias> sAliases;

bool SameNoCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++) {
        unsigned char x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
        if ((x >= 'A' && x <= 'Z' ? x + 32 : x) != (y >= 'A' && y <= 'Z' ? y + 32 : y))
            return false;
    }
    return true;
}

// Windows-1252 0x80..0x9F (0: undefined there)
const uint16_t kCP1252[32] = {0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                              0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
                              0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                              0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};

int Codepoint(unsigned char c)
{
    if (c >= 0x80 && c < 0xA0)
        return kCP1252[c - 0x80] ? kCP1252[c - 0x80] : '?';
    return c;
}

int Round(float v)
{
    return static_cast<int>(floorf(v + 0.5f));
}

int U16(const unsigned char* p)
{
    return (p[0] << 8) | p[1];
}

int S16(const unsigned char* p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(U16(p)));
}

struct Glyph {
    int mAdvance;                   // pixels
    int mX0, mY0;                   // bitmap offset from the pen on the baseline
    int mWidth, mHeight;
    std::vector<unsigned char> mCoverage;
};

}   // namespace

struct PlatFont {
    PlatFontMetrics mMetrics;
    bool mAntialiased;
    bool mUnderline;
    bool mSyntheticBold;
    int mUnderlineOffset;           // from the baseline, down
    int mUnderlineThickness;
    Glyph mGlyphs[256];
};

extern "C" int PlatFontRegister(const char* inFace, int inBold, int inItalic, const void* inTrueType, uint32_t inSize)
{
    if (!inFace || !inTrueType || inSize < 12)
        return 0;
    Face* theFace = new Face;
    theFace->mName = inFace;
    theFace->mBold = inBold != 0;
    theFace->mItalic = inItalic != 0;
    theFace->mData = static_cast<const unsigned char*>(inTrueType);
    int theOffset = stbtt_GetFontOffsetForIndex(theFace->mData, 0);
    if (theOffset < 0 || !stbtt_InitFont(&theFace->mInfo, theFace->mData, theOffset)) {
        delete theFace;
        return 0;
    }
    std::lock_guard<std::mutex> theLock(sFaceLock);
    sFaces.push_back(theFace);
    return 1;
}

extern "C" int PlatFontAlias(const char* inFace, const char* inRegisteredFace)
{
    if (!inFace || !inRegisteredFace)
        return 0;
    std::lock_guard<std::mutex> theLock(sFaceLock);
    Alias theAlias = {inFace, inRegisteredFace};
    sAliases.push_back(theAlias);
    return 1;
}

// The registered face closest to the request (sFaceLock held): name, then style
static Face* FindFace(const std::string& inName, bool inBold, bool inItalic)
{
    std::string theName = inName;
    for (const Alias& theAlias : sAliases)
        if (SameNoCase(theAlias.mName, theName))
            theName = theAlias.mTarget;
    Face* theBest = NULL;
    int theBestScore = -1;
    for (Face* theFace : sFaces) {
        int theScore = (SameNoCase(theFace->mName, theName) ? 4 : 0) + (theFace->mBold == inBold ? 2 : 0)
            + (theFace->mItalic == inItalic ? 1 : 0);
        if (theScore > theBestScore) {
            theBest = theFace;
            theBestScore = theScore;
        }
    }
    return theBest;
}

extern "C" PlatFont* PlatFontCreate(const char* inFace, int32_t inHeight, int inBold, int inItalic, int inUnderline,
                                    int inAntialiased)
{
    Face* theFace;
    {
        std::lock_guard<std::mutex> theLock(sFaceLock);
        theFace = FindFace(inFace ? inFace : "", inBold != 0, inItalic != 0);
    }
    if (!theFace)
        return NULL;
    const stbtt_fontinfo* theInfo = &theFace->mInfo;
    const unsigned char* theData = theFace->mData;
    int theUnitsPerEm = U16(theData + theInfo->head + 18);
    // usWinAscent / usWinDescent (OS/2), else hhea's ascender / descender
    int theWinAscent, theWinDescent, theAverage = 0;
    uint32_t theOS2 = stbtt__find_table(const_cast<unsigned char*>(theData), theInfo->fontstart, "OS/2");
    if (theOS2) {
        theWinAscent = U16(theData + theOS2 + 74);
        theWinDescent = U16(theData + theOS2 + 76);
        theAverage = S16(theData + theOS2 + 2);
    } else {
        int theAscent, theDescent, theGap;
        stbtt_GetFontVMetrics(theInfo, &theAscent, &theDescent, &theGap);
        theWinAscent = theAscent;
        theWinDescent = -theDescent;
    }
    float theScale;
    if (inHeight > 0)
        theScale = static_cast<float>(inHeight) / static_cast<float>(theWinAscent + theWinDescent);
    else
        theScale = static_cast<float>(inHeight < 0 ? -inHeight : 12) / static_cast<float>(theUnitsPerEm);

    PlatFont* theFont = new PlatFont;
    PlatFontMetrics& m = theFont->mMetrics;
    m.mAscent = Round(theWinAscent * theScale);
    m.mDescent = Round(theWinDescent * theScale);
    m.mHeight = m.mAscent + m.mDescent;
    m.mInternalLeading = m.mHeight - Round(theUnitsPerEm * theScale);
    if (m.mInternalLeading < 0)
        m.mInternalLeading = 0;
    int theAscender, theDescender, theLineGap;
    stbtt_GetFontVMetrics(theInfo, &theAscender, &theDescender, &theLineGap);
    // GDI: the external leading is what the Macintosh line spacing (hhea) adds over the Windows cell
    int theExternal = Round((theAscender - theDescender + theLineGap) * theScale) - m.mHeight;
    m.mExternalLeading = theExternal > 0 ? theExternal : 0;
    m.mFirstChar = 0x20;
    m.mLastChar = 0xFF;
    theFont->mAntialiased = inAntialiased != 0;
    theFont->mUnderline = inUnderline != 0;
    theFont->mSyntheticBold = inBold && !theFace->mBold;
    theFont->mUnderlineThickness = Round(theUnitsPerEm * theScale / 14.0f) > 0 ? Round(theUnitsPerEm * theScale / 14.0f)
                                                                              : 1;
    theFont->mUnderlineOffset = (m.mDescent + 1) / 2;
    int theMax = 0;
    for (int c = 0; c < 256; c++) {
        Glyph& g = theFont->mGlyphs[c];
        g.mAdvance = 0;
        g.mX0 = g.mY0 = g.mWidth = g.mHeight = 0;
        if (c < m.mFirstChar)
            continue;
        int theIndex = stbtt_FindGlyphIndex(theInfo, Codepoint(static_cast<unsigned char>(c)));
        int theAdvance, theBearing;
        stbtt_GetGlyphHMetrics(theInfo, theIndex, &theAdvance, &theBearing);
        g.mAdvance = Round(theAdvance * theScale) + (theFont->mSyntheticBold ? 1 : 0);
        if (g.mAdvance > theMax)
            theMax = g.mAdvance;
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(theInfo, theIndex, theScale, theScale, &x0, &y0, &x1, &y1);
        if (x1 > x0 && y1 > y0) {
            g.mX0 = x0;
            g.mY0 = y0;
            g.mWidth = x1 - x0;
            g.mHeight = y1 - y0;
            g.mCoverage.resize(static_cast<size_t>(g.mWidth) * g.mHeight);
            stbtt_MakeGlyphBitmap(theInfo, g.mCoverage.data(), g.mWidth, g.mHeight, g.mWidth, theScale, theScale,
                                  theIndex);
        }
    }
    m.mMaxCharWidth = theMax;
    m.mAveCharWidth = theAverage > 0 ? Round(theAverage * theScale) : theFont->mGlyphs['x'].mAdvance;
    return theFont;
}

extern "C" void PlatFontDestroy(PlatFont* inFont)
{
    delete inFont;
}

extern "C" void PlatFontGetMetrics(const PlatFont* inFont, PlatFontMetrics* outMetrics)
{
    *outMetrics = inFont->mMetrics;
}

extern "C" int32_t PlatFontMeasure(const PlatFont* inFont, const char* inText, uint32_t inLength)
{
    int32_t theWidth = 0;
    for (uint32_t i = 0; i < inLength; i++)
        theWidth += inFont->mGlyphs[static_cast<unsigned char>(inText[i])].mAdvance;
    return theWidth;
}

namespace {

struct Clip {
    int32_t mLeft, mTop, mRight, mBottom;
};

inline void Blend(unsigned char* p, uint32_t inColor, int inCoverage, bool inPremultiplied)
{
    unsigned char b = inColor & 0xFF, g = (inColor >> 8) & 0xFF, r = (inColor >> 16) & 0xFF;
    if (inCoverage >= 255) {
        p[0] = b;
        p[1] = g;
        p[2] = r;
        p[3] = 0xFF;
    } else {
        p[0] = static_cast<unsigned char>((b * inCoverage + p[0] * (255 - inCoverage) + 127) / 255);
        p[1] = static_cast<unsigned char>((g * inCoverage + p[1] * (255 - inCoverage) + 127) / 255);
        p[2] = static_cast<unsigned char>((r * inCoverage + p[2] * (255 - inCoverage) + 127) / 255);
        p[3] = inPremultiplied ? static_cast<unsigned char>(inCoverage + (p[3] * (255 - inCoverage) + 127) / 255) : 0xFF;
    }
}

void Fill(const PlatFontTarget* t, const Clip& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t inColor)
{
    if (x0 < c.mLeft) x0 = c.mLeft;
    if (y0 < c.mTop) y0 = c.mTop;
    if (x1 > c.mRight) x1 = c.mRight;
    if (y1 > c.mBottom) y1 = c.mBottom;
    for (int32_t y = y0; y < y1; y++) {
        unsigned char* p = static_cast<unsigned char*>(t->mPixels) + static_cast<intptr_t>(y) * t->mPitch + x0 * 4;
        for (int32_t x = x0; x < x1; x++, p += 4)
            Blend(p, inColor, 255, false);
    }
}

void DrawGlyph(const PlatFont* f, const Glyph& g, const PlatFontTarget* t, const Clip& c, int32_t inX, int32_t inY,
               uint32_t inColor)
{
    for (int gy = 0; gy < g.mHeight; gy++) {
        int32_t y = inY + g.mY0 + gy;
        if (y < c.mTop || y >= c.mBottom)
            continue;
        unsigned char* theRow = static_cast<unsigned char*>(t->mPixels) + static_cast<intptr_t>(y) * t->mPitch;
        for (int gx = 0; gx < g.mWidth; gx++) {
            int32_t x = inX + g.mX0 + gx;
            if (x < c.mLeft || x >= c.mRight)
                continue;
            int theCoverage = g.mCoverage[static_cast<size_t>(gy) * g.mWidth + gx];
            if (!f->mAntialiased)
                theCoverage = theCoverage >= 128 ? 255 : 0;
            if (theCoverage)
                Blend(theRow + x * 4, inColor, theCoverage, (t->mFlags & PLAT_FONT_TARGET_PREMULTIPLIED) != 0);
        }
    }
}

}   // namespace

extern "C" void PlatFontDraw(const PlatFont* inFont, const char* inText, uint32_t inLength, int32_t inX, int32_t inY,
                             uint32_t inColor, int inOpaque, uint32_t inBackground, const PlatFontRect* inClip,
                             const PlatFontTarget* inTarget)
{
    Clip c = {0, 0, inTarget->mWidth, inTarget->mHeight};
    if (inClip) {
        if (inClip->mLeft > c.mLeft) c.mLeft = inClip->mLeft;
        if (inClip->mTop > c.mTop) c.mTop = inClip->mTop;
        if (inClip->mRight < c.mRight) c.mRight = inClip->mRight;
        if (inClip->mBottom < c.mBottom) c.mBottom = inClip->mBottom;
    }
    if (c.mLeft >= c.mRight || c.mTop >= c.mBottom)
        return;
    int32_t theWidth = PlatFontMeasure(inFont, inText, inLength);
    if (inOpaque)
        Fill(inTarget, c, inX, inY, inX + theWidth, inY + inFont->mMetrics.mHeight, inBackground);
    int32_t theBaseline = inY + inFont->mMetrics.mAscent;
    int32_t thePen = inX;
    for (uint32_t i = 0; i < inLength; i++) {
        const Glyph& g = inFont->mGlyphs[static_cast<unsigned char>(inText[i])];
        if (g.mWidth) {
            DrawGlyph(inFont, g, inTarget, c, thePen, theBaseline, inColor);
            if (inFont->mSyntheticBold)
                DrawGlyph(inFont, g, inTarget, c, thePen + 1, theBaseline, inColor);
        }
        thePen += g.mAdvance;
    }
    if (inFont->mUnderline) {
        int32_t y = theBaseline + inFont->mUnderlineOffset;
        Fill(inTarget, c, inX, y, inX + theWidth, y + inFont->mUnderlineThickness, inColor);
    }
}
