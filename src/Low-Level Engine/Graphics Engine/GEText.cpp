// C:\EEX\Low-Level Engine\Graphics Engine\GEText.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// 0x10032ae2..0x10032bda: ComputeTextExtent only (the rest of GEText is header-inline; GEBoundingBox.cpp's inline
// block emits it). No init chains.
#include <windows.h>

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
#include "GERustMath.h"
#include "GERustText.h"

// FUNCTION: LLE 0x10032ae2
// Measures the text's pixel extent with its font (platform/PlatFont.h: GetTextExtentPoint32's), in chunks of 256
// characters; the height is the font's cell. The chunked measure is Rust (ee_ge font.rs); the font's absence and its
// metrics are read here.
void GEText::ComputeTextExtent()
{
    PlatFont* theFont = this->mFont->GetOSFont();
    if (theFont == NULL)
        return;
    PlatFontMetrics theMetrics;
    PlatFontGetMetrics(theFont, &theMetrics);
    ee_ge_text_extent(GERustTextOps(), theFont, (const char*)this->mText, this->mText.GetLength(), theMetrics.mHeight,
                      GERust(this->mTextExtent));
}
