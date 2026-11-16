#include "rust/ee_won.h"
// PlatGUIFont.cpp's statements after the port (R6 WON round 40: the shell over ee_won's platfont.rs), copied from the
// working tree by build/r40/gen_platfont.py; won_platfont_test.cpp compiles them over its stand-ins.

PlatGUIFont::PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor)
    : Font(inContext, inDescriptor),
      mRust(ee_won_plat_font_new(inDescriptor.mFace.c_str(), inDescriptor.mSize, inDescriptor.mStyle)) {}

PlatGUIFont::~PlatGUIFont() { ee_won_plat_font_free(this->mRust); }

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

