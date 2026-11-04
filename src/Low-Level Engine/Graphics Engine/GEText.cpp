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
#include "lle/GEDeviceContext.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"

// FUNCTION: LLE 0x10032ae2
// Measures the text's pixel extent with its font, in chunks of 256 characters
void GEText::ComputeTextExtent()
{
    HDC theContext;
    unsigned long i;
    HGDIOBJ theOldFont;
    SIZE theSize;
    HFONT theFont = this->mFont->GetOSFont();
    const unsigned char* theString;
    theContext = GEDeviceContext::GetDeviceContext(NULL);
    if (theContext != NULL) {
        theOldFont = SelectObject(theContext, theFont);
        if (theOldFont != NULL) {
            unsigned long theChunks = this->mText.GetLength() / 256;
            theString = reinterpret_cast<const unsigned char*>((const char*)this->mText);
            this->mTextExtent.SetRight(0.0f);
            for (i = 0; i < theChunks; i++) {
                // Measure this chunk only (this measured the whole text once per chunk).
                const unsigned char* theNext = _mbsninc(theString, 256);
                if (!GetTextExtentPoint32(theContext, reinterpret_cast<const char*>(theString), theNext - theString, &theSize)) {
                    GetLastError();
                    break;
                }
                this->mTextExtent.SetLeft(0.0f);
                this->mTextExtent.SetRight(this->mTextExtent.GetRight() + theSize.cx);
                this->mTextExtent.SetTop(0.0f);
                this->mTextExtent.SetBottom((float)theSize.cy);
                theString = theNext;
            }
            if (GetTextExtentPoint32(theContext, reinterpret_cast<const char*>(theString), strlen(reinterpret_cast<const char*>(theString)), &theSize)) {
                this->mTextExtent.SetLeft(0.0f);
                this->mTextExtent.SetRight(this->mTextExtent.GetRight() + theSize.cx);
                this->mTextExtent.SetTop(0.0f);
                this->mTextExtent.SetBottom((float)theSize.cy);
            } else
                GetLastError();
            SelectObject(theContext, theOldFont);
        }
    }
    GEDeviceContext::ReleaseDeviceContext(NULL, theContext);
}
