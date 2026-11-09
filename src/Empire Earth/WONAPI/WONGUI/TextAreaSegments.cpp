// Text area segment overloads: plain text and inline images
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <deque>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyTextImage.h"
namespace WONAPI {
// FUNCTION: WON 0x00713c60
// FUNCTION: WON 0x00713c80
// FUNCTION: WON 0x00713da0
// Appends a text segment in the area's text colour (0x00713c60); the overloads below take a colour
// (0x00713c80) or a colour and font (0x00713da0)
void TextArea::AddSegment(const GUIString& text,bool lineBreak)
{
    this->AddSegment(text,this->mTextColor,lineBreak);
}
// Appends a text segment in the given colour and the area's font (the body at 0x00713c80)
void TextArea::AddSegment(const GUIString& text,int color,bool lineBreak)
{
    SmartPtr<TextSegment> seg=new TextSegment;
    seg->mText.mData=text.mData;
    seg->mColor=color;
    seg->mFont=this->mFont;
    seg->mLineBreak=lineBreak;
    this->AddSegment(seg.get());
}
// Appends a text segment in the given colour and font (the body at 0x00713da0)
void TextArea::AddSegment(const GUIString& text,int color,Font* font,bool lineBreak)
{
    SmartPtr<TextSegment> seg=new TextSegment;
    seg->mText.mData=text.mData;
    seg->mColor=color;
    seg->mFont=font;
    seg->mLineBreak=lineBreak;
    this->AddSegment(seg.get());
}
}

namespace WONAPI {
// Appends an inline image segment with its copy text (not identified in the original exe)
void TextArea::AddImage(Image *theImage, const GUIString &theCopyText, bool lineBreak)
{
	SmartPtr<TextImage> aSeg = new TextImage;
	aSeg->mImage = theImage;
	aSeg->mText = theCopyText;
	aSeg->mLineBreak = lineBreak;
	this->AddSegment(aSeg.get());
}
}
