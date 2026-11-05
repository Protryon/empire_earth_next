// C:\EEX\Low-Level Engine\Graphics Engine\GEFont.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100151b7..0x100152e4: the ctor and the dtor (the accessors are header-inline, emitted by GEBoundingBox.cpp).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEFont.h"
#include "platform/PlatFont.h"

// FUNCTION: LLE 0x100151b7
// Creates a font (name, pixel size, bold/italic/underline style, quality: platform/PlatFont.h, CreateFont's
// arguments; only the antialiased quality is antialiased) and measures its height (tmHeight)
GEFont::GEFont(const UString& inName, unsigned long inSize, unsigned long inStyle, unsigned long inQuality)
    : mName(inName)
{
    long theHeight = 0;
    this->mSize = inSize;
    this->mStyle = inStyle;
    this->mQuality = inQuality;
    this->mFont = PlatFontCreate(this->mName, -(long)inSize, (inStyle & kFontStyleBold) != 0, (inStyle >> 1) & 1,
                                 (inStyle >> 2) & 1, inQuality == kFontQualityAntialiased);
    if (this->mFont != NULL) {
        PlatFontMetrics theMetrics;
        PlatFontGetMetrics(this->mFont, &theMetrics);
        theHeight = theMetrics.mHeight;
    }
    this->mFontBitmap = NULL;
    this->mHeight = theHeight;
}

// FUNCTION: LLE 0x100152b6
// Destroys the font and releases its glyph bitmap
GEFont::~GEFont()
{
    PlatFontDestroy(this->mFont);
    if (this->mFontBitmap)
        this->mFontBitmap->Dereference();
}
