// MSFont: a font drawn through Windows GDI
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
using namespace WONAPI;

// FUNCTION: WON 0x007cee60
// Creates a GDI font from the descriptor (points at a fixed 96 dpi, bold/italic, ANSI charset) in its
// own memory DC and caches its text metrics
MSFont::MSFont(DisplayContext* context,const FontDescriptor& descriptor)
 : Font(context,descriptor) {
    this->mDC=CreateCompatibleDC(context->mDC);
    int height=-MulDiv(descriptor.mSize,96,72);
    int style=descriptor.mStyle;
    this->mFont=CreateFontA(height,0,0,0,(style&FontStyle_Bold)?FW_BOLD:FW_NORMAL,
        (style&FontStyle_Italic)?TRUE:FALSE,FALSE,FALSE,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FF_DONTCARE,
        descriptor.mFace.c_str());
    SelectObject(this->mDC,this->mFont);
    GetTextMetricsA(this->mDC,&this->mTextMetrics);
}

// FUNCTION: WON 0x007cef30
// Destroys the font, deleting the GDI font and its DC
MSFont::~MSFont() {
    DeleteObject(this->mFont);
    DeleteDC(this->mDC);
}

// FUNCTION: WON 0x007cef60
// Selects the font into the context's DC
void MSFont::PrepareContext(DisplayContext* context) {
    SelectObject(context->mDC,this->mFont);
}

// FUNCTION: WON 0x007cef80
// Draws part of a string at x, y and, for an underlined font, draws the underline itself just above
// the descent
void MSFont::DrawString(DisplayContext* context,const GUIString& text,
    unsigned short offset,unsigned short length,int x,int y) {
    TextOutW(context->mDC,x,y,text.mData->mText+offset,length);
    if(!(this->mDescriptor.mStyle&FontStyle_Underline))
        return;
    Graphics* g=context->mGraphics;
    if(!g)
        return;
    int descent=this->GetDescent();
    int mod;
    if(descent<3) mod=0;
    else if(descent==3) mod=1;
    else mod=descent>>1;
    int yy=y+this->GetHeight()-mod-1;
    g->DrawLineHook(x,yy,x+this->GetStringWidth(text,offset,length),yy);
}

// FUNCTION: WON 0x007cf040
// Returns the pixel width of part of a string (0 when empty)
int MSFont::GetStringWidth(const GUIString& text,unsigned short offset,unsigned short length) {
    if(length==0) return 0;
    SIZE size;
    GetTextExtentPoint32W(this->mDC,text.mData->mText+offset,length,&size);
    return size.cx;
}

// FUNCTION: WON 0x007cf090
// Returns the pixel width of one character
int MSFont::GetCharacterWidth(int character) {
    int width=0;
    GetCharWidthW(this->mDC,character,character,&width);
    return width;
}

// FUNCTION: WON 0x007cf0c0
// Returns the font's height
int MSFont::GetHeight() { return this->mTextMetrics.tmHeight; }
// FUNCTION: WON 0x007cf0d0
// Returns the font's ascent
int MSFont::GetAscent() { return this->mTextMetrics.tmAscent; }
// FUNCTION: WON 0x007cf0e0
// Returns the font's descent
int MSFont::GetDescent() { return this->mTextMetrics.tmDescent; }

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cef10
// (public: virtual void * __thiscall WONAPI::MSFont::`scalar deleting destructor'(unsigned int)    [??_GMSFont@WONAPI@@UAEPAXI@Z])
