// The WONAPI GUI's fonts on platform/PlatFont.h (WONLobbyPlatform.h), in place of the GDI backend's MSFont.cpp: the
// same font request (the point size at a fixed 96 dpi as a character height, bold, italic), text drawn with its
// cell's top-left at x, y in the graphics' colour on a transparent background (TextOut, TA_TOP | TA_LEFT,
// TRANSPARENT), and MSFont's own underline. Text is converted to UTF-8 (decision D6; PlatFont draws any character the
// font has). On a
// surface with a pixel scale (WONLobbyPlatform.h) the text is rasterised at the font size times the scale and
// smoothed (the metrics and every measurement stay the logical font's, so the layout does not move).
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <math.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "platform/PlatFont.h"
#include "platform/PlatString.h"

namespace WONAPI {

// The lobby's text as GDI drew it at DEFAULT_QUALITY on the era's systems: small sizes without smoothing (at a pixel
// scale above 1 the text is smoothed: there are pixels enough for it)
static const int kPlatGUIFontAntialiased = 0;

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

// MulDiv: a * b / c rounded half away from zero
static int MulDivRound(int a, int b, int c)
{
    long long theProduct = (long long)a * b;
    long long theHalf = c / 2;
    return (int)((theProduct >= 0 ? theProduct + theHalf : theProduct - theHalf) / c);
}

PlatGUIFont::PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor)
    : Font(inContext, inDescriptor), mScaledFont(NULL), mScaledFontScale(0.0f)
{
    int theHeight = -MulDivRound(inDescriptor.mSize, 96, 72);
    int theStyle = inDescriptor.mStyle;
    this->mFont = PlatFontCreate(inDescriptor.mFace.c_str(), theHeight, (theStyle & FontStyle_Bold) ? 1 : 0,
                                 (theStyle & FontStyle_Italic) ? 1 : 0, 0, kPlatGUIFontAntialiased);
    PlatFontMetrics theMetrics = PlatFontMetrics();
    if (this->mFont != NULL)
        PlatFontGetMetrics(this->mFont, &theMetrics);
    this->mHeight = theMetrics.mHeight;
    this->mAscent = theMetrics.mAscent;
    this->mDescent = theMetrics.mDescent;
}

PlatGUIFont::~PlatGUIFont()
{
    if (this->mScaledFont != NULL)
        PlatFontDestroy(this->mScaledFont);
    if (this->mFont != NULL)
        PlatFontDestroy(this->mFont);
}

// The font for a surface of pixel scale inScale: the logical font at 1, else the same request with the character
// height times the scale, smoothed (the last one is kept)
const PlatFont* PlatGUIFont::FontForScale(float inScale)
{
    if (inScale == 1.0f || this->mFont == NULL)
        return this->mFont;
    if (this->mScaledFont != NULL && this->mScaledFontScale == inScale)
        return this->mScaledFont;
    if (this->mScaledFont != NULL)
        PlatFontDestroy(this->mScaledFont);
    int theHeight = -(int)floorf((float)MulDivRound(this->mDescriptor.mSize, 96, 72) * inScale + 0.5f);
    int theStyle = this->mDescriptor.mStyle;
    this->mScaledFont = PlatFontCreate(this->mDescriptor.mFace.c_str(), theHeight, (theStyle & FontStyle_Bold) ? 1 : 0,
                                       (theStyle & FontStyle_Italic) ? 1 : 0, 0, 1);
    this->mScaledFontScale = inScale;
    return this->mScaledFont != NULL ? this->mScaledFont : this->mFont;
}

// Selecting the font into a DC: nothing to do, each string names its font
void PlatGUIFont::PrepareContext(DisplayContext*) {}

// Draws part of a string with its cell's top-left at x, y in the context graphics' colour, within the surface's
// clip; an underlined font draws its underline as MSFont did (a line just above the descent)
void PlatGUIFont::DrawString(DisplayContext* inContext, const GUIString& inText, unsigned short inOffset,
                             unsigned short inLength, int x, int y)
{
    if (inContext == NULL)
        return;
    SurfaceBase* theSurface = inContext->mSurface;
    Graphics* theGraphics = inContext->mGraphics;
    float theScale = inContext->mPixelScale > 0.0f ? inContext->mPixelScale : 1.0f;
    if (theSurface != NULL && theSurface->GetPixelsBase() != NULL && this->mFont != NULL && inLength != 0) {
        std::string theBytes = PlatGUIToUtf8(inText.mData->mText + inOffset, inLength);
        PlatFontTarget theTarget;
        theTarget.mPixels = theSurface->GetPixelsBase();
        theTarget.mPitch = theSurface->GetBytesPerRow();
        theTarget.mWidth = theSurface->GetWidth();
        theTarget.mHeight = theSurface->GetHeight();
        theTarget.mFlags = 0;
        theTarget.mScale = 0.0f;
        PlatFontRect theClip;
        theClip.mLeft = theSurface->GetClipX1();
        theClip.mTop = theSurface->GetClipY1();
        theClip.mRight = theSurface->GetClipX2() + 1;
        theClip.mBottom = theSurface->GetClipY2() + 1;
        if (theClip.mRight > theClip.mLeft && theClip.mBottom > theClip.mTop) {
            uint32_t theColor = theGraphics != NULL ? (theGraphics->mColor & 0x00FFFFFF) : 0;
            // scaled: the cell's top-left at the logical point's pixel (its ascent lines up within a pixel)
            PlatFontDraw(this->FontForScale(theScale), theBytes.data(), (uint32_t)theBytes.size(),
                         PlatGUIPixels(x, theScale), PlatGUIPixels(y, theScale), theColor, 0, 0, &theClip, &theTarget);
        }
    }
    if (!(this->mDescriptor.mStyle & FontStyle_Underline))
        return;
    if (theGraphics == NULL)
        return;
    int theDescent = this->GetDescent();
    int theMod;
    if (theDescent < 3)
        theMod = 0;
    else if (theDescent == 3)
        theMod = 1;
    else
        theMod = theDescent >> 1;
    int theY = y + this->GetHeight() - theMod - 1;
    theGraphics->DrawLineHook(x, theY, x + this->GetStringWidth(inText, inOffset, inLength), theY);
}

// The pixel width of part of a string (0 when empty)
int PlatGUIFont::GetStringWidth(const GUIString& inText, unsigned short inOffset, unsigned short inLength)
{
    if (inLength == 0 || this->mFont == NULL)
        return 0;
    std::string theBytes = PlatGUIToUtf8(inText.mData->mText + inOffset, inLength);
    return PlatFontMeasure(this->mFont, theBytes.data(), (uint32_t)theBytes.size());
}

// The pixel width of one character
int PlatGUIFont::GetCharacterWidth(int inChar)
{
    if (this->mFont == NULL)
        return 0;
    wchar_t theWide = (wchar_t)inChar;
    std::string theByte = PlatGUIToUtf8(&theWide, 1);
    return PlatFontMeasure(this->mFont, theByte.data(), (uint32_t)theByte.size());
}

int PlatGUIFont::GetHeight() { return this->mHeight; }
int PlatGUIFont::GetAscent() { return this->mAscent; }
int PlatGUIFont::GetDescent() { return this->mDescent; }

} // namespace WONAPI
