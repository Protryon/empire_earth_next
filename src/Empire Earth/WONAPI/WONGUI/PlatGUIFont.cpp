// The WONAPI GUI's fonts on the platform layer (WONLobbyPlatform.h), MSFont's replacement. R6 WON round 40: the font
// is ee_won's Rust object (rust/crates/ee_won_gui/src/platfont.rs: the platform fonts at the descriptor's size and at a
// surface's pixel scale, the metrics, the widths, the drawing); the class is its shell, and draws the underline the
// Rust answers with the context's Graphics.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "platform/PlatString.h"
#include "rust/ee_won.h"

namespace WONAPI {

std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength)
{
    std::string theText;
    if (inText == NULL || inLength == 0)
        return theText;
    uint32_t theCount = PlatWideToUtf8(inText, inLength, NULL, 0);
    theText.resize(theCount);
    if (theCount != 0)
        PlatWideToUtf8(inText, inLength, &theText[0], theCount);
    return theText;
}

PlatGUIFont::PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor)
    : Font(inContext, inDescriptor),
      mRust(ee_won_plat_font_new(inDescriptor.mFace.c_str(), inDescriptor.mSize, inDescriptor.mStyle)) {}

PlatGUIFont::~PlatGUIFont() { ee_won_plat_font_free(this->mRust); }

// Selecting the font into a DC: nothing to do, each string names its font
void PlatGUIFont::PrepareContext(DisplayContext*) {}

void PlatGUIFont::DrawString(DisplayContext* inContext, const GUIString& inText, unsigned short inOffset,
                             unsigned short inLength, int x, int y)
{
    if (inContext == NULL)
        return;
    Graphics* theGraphics = inContext->mGraphics;
    int theLine[3];
    if (ee_won_plat_font_draw(this->mRust, inContext->mSurface != NULL ? inContext->mSurface->GetRust() : NULL,
                              inContext->mPixelScale, theGraphics != NULL,
                              theGraphics != NULL ? (uint32_t)(theGraphics->mColor & 0x00FFFFFF) : 0,
                              inText.mData->mText + inOffset, inLength, x, y, theLine))
        theGraphics->DrawLineHook(theLine[0], theLine[1], theLine[2], theLine[1]);
}

int PlatGUIFont::GetStringWidth(const GUIString& inText, unsigned short inOffset, unsigned short inLength)
{
    return inLength == 0 ? 0 : ee_won_plat_font_width(this->mRust, inText.mData->mText + inOffset, inLength);
}

int PlatGUIFont::GetCharacterWidth(int inChar)
{
    wchar_t theWide = (wchar_t)inChar;
    return ee_won_plat_font_width(this->mRust, &theWide, 1);
}

int PlatGUIFont::GetHeight() { return ee_won_plat_font_metric(this->mRust, 0); }
int PlatGUIFont::GetAscent() { return ee_won_plat_font_metric(this->mRust, 1); }
int PlatGUIFont::GetDescent() { return ee_won_plat_font_metric(this->mRust, 2); }

} // namespace WONAPI
