// C:\EEX\Low-Level Engine\Graphics Engine\GEText.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// 0x10032ae2..0x10032bda: ComputeTextExtent only (the rest of GEText is header-inline; GEBoundingBox.cpp's inline
// block emits it). No init chains.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "platform/PlatFont.h"

// FUNCTION: LLE 0x10032ae2
// Measures the text's pixel extent with its font (platform/PlatFont.h: GetTextExtentPoint32's), in chunks of 256
// characters; the height is the font's cell
void GEText::ComputeTextExtent()
{
    PlatFont* theFont = this->mFont->GetOSFont();
    if (theFont == NULL)
        return;
    PlatFontMetrics theMetrics;
    PlatFontGetMetrics(theFont, &theMetrics);
    const unsigned char* theString = reinterpret_cast<const unsigned char*>((const char*)this->mText);
    unsigned long theChunks = this->mText.GetLength() / 256;
    this->mTextExtent.SetRight(0.0f);
    for (unsigned long i = 0; i < theChunks; i++) {
        const unsigned char* theNext = PlatMbsNInc(theString, 256);
        this->mTextExtent.SetLeft(0.0f);
        this->mTextExtent.SetRight(this->mTextExtent.GetRight()
                                   + PlatFontMeasure(theFont, reinterpret_cast<const char*>(theString), theNext - theString));
        this->mTextExtent.SetTop(0.0f);
        this->mTextExtent.SetBottom((float)theMetrics.mHeight);
        theString = theNext;
    }
    this->mTextExtent.SetLeft(0.0f);
    this->mTextExtent.SetRight(this->mTextExtent.GetRight()
                               + PlatFontMeasure(theFont, reinterpret_cast<const char*>(theString),
                                                 strlen(reinterpret_cast<const char*>(theString))));
    this->mTextExtent.SetTop(0.0f);
    this->mTextExtent.SetBottom((float)theMetrics.mHeight);
}
