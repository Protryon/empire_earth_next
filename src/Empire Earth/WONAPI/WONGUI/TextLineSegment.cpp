// Rich text line segments: metrics, selection, search, painting and word wrap
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <cwctype>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
#include "ee/wonapi/WONLobbyTextWrap.h"
namespace WONAPI {

// FUNCTION: WON 0x00709E70
// Returns (and caches) the width of the widest word; the last word after a space is never measured,
// so a text whose longest word comes last reports a smaller minimum
int TextLineSegment::GetMinWidth()
{
	if(this->mMinWidth==0)
	{
		int aWidth = 0;
		int aPos = 0;
		while(aPos<this->mText.length())
		{
			int aChar = this->mText[aPos];
			if(aChar==' ')
			{
				if(aWidth>this->mMinWidth)
					this->mMinWidth = aWidth;

				aWidth = 0;
			}
			else
				aWidth += this->mFont->GetCharacterWidth(aChar);

			aPos++;
		}

		if(this->mMinWidth==0)  // no space found
			this->mMinWidth = aWidth;
	}

	return this->mMinWidth;
}

// FUNCTION: WON 0x00709ed0
// Returns (and caches) the pixel width of the segment's text
int TextLineSegment::GetWidth()
{
	if(this->mWidth==0)
		this->mWidth = this->mFont->GetStringWidth(this->mText);

	return this->mWidth;
}

// FUNCTION: WON 0x00709F00
// Returns the pixel width of the text up to its first space
int TextLineSegment::GetFirstWrapWidth()
{
	int aWidth = 0;
	int aPos = 0;
	while(aPos<this->mText.length())
	{
		int aChar = this->mText[aPos];
		if(aChar==' ')
			return aWidth;
		else
			aWidth += this->mFont->GetCharacterWidth(aChar);

		aPos++;
	}

	return aWidth;
}

// FUNCTION: WON 0x00709F40
// Returns the character in a range nearest to pixel x (rounding at half a character)
int TextLineSegment::GetSelChar(int x, int y, int theStartChar, int theEndChar)
{
	int aWidth = 0;
	int aPos = theStartChar;
	while(aPos<theEndChar)
	{
		int aCharWidth = this->mFont->GetCharacterWidth(this->mText[aPos]);
		if(aWidth + (aCharWidth>>1) >= x)
			break;

		aWidth += aCharWidth;
		aPos++;
	}

	return aPos;

}

// Returns the character nearest to pixel x across the whole text (emitted at 0x00709f90)
int TextLineSegment::GetSelChar(int x, int y)
{
	return this->GetSelChar(x,y,0,this->mText.length());
}

// FUNCTION: WON 0x00709FC0
// Appends the selected part of the segment's text, if the segment is in the selection
void TextLineSegment::GetSelText(GUIString &theText, ElementSelectionInfo &theInfo)
{
	if(this->mStartSelIndex<theInfo.mStartSelSegment || this->mStartSelIndex>theInfo.mEndSelSegment)
		return;

	int aStartPos = 0;
	int anEndPos = this->mText.length();
	bool partial = false;
	if(theInfo.mStartSelSegment==this->mStartSelIndex)
	{
		aStartPos = theInfo.mStartSelSegmentPos;
		partial = true;
	}
	if(theInfo.mEndSelSegment==this->mStartSelIndex)
	{
		anEndPos = theInfo.mEndSelSegmentPos;
		partial = true;
	}

	if(partial)
		theText.append(this->mText.substr(aStartPos,anEndPos-aStartPos));
	else
		theText.append(this->mText);
}

// FUNCTION: WON 0x0070A260
// Returns the screen position of a character of this segment; false when it is another segment's
bool TextLineSegment::GetSelCharPos(int &x, int &y, int theSelIndex, int theSelChar)
{
	if(theSelIndex!=this->mStartSelIndex)
		return false;

	if(theSelChar<0 || theSelChar>this->mText.length())
		return false;

	x = this->mAbsX + this->mFont->GetStringWidth(this->mText,0,theSelChar);
	y = this->mAbsY;
	return true;
}

// FUNCTION: WON 0x0070A320
// Continues a match begun in an earlier segment; returns false on a mismatch, else records how much
// has matched and where it ends
bool TextLineSegment::FindPartial(ElementFindInfo &theInfo, int startChar, int endChar)
{
	int p1 = startChar, p2 = theInfo.mCurFoundLength;
	const GUIString &aText = theInfo.mFindText;
	int len2 = aText.length();
	const bool caseSensitive = theInfo.mCaseSensitive;

	while(p1<endChar && p2<len2)
	{
		if(caseSensitive)
		{
			if(this->mText[p1]!=aText[p2])
				break;
		}
		else if(towupper(this->mText[p1])!=towupper(aText[p2]))
			break;

		p1++; p2++;
	}

	if(p1<endChar && p2<len2)
		return false;
	else
	{
		theInfo.mCurFoundLength = p2;
		theInfo.mFoundEndSegment = this->mStartSelIndex;
		theInfo.mFoundEndChar = p1;
		return true;
	}
}


// FUNCTION: WON 0x0070a900
// Paints the whole segment
void TextLineSegment::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	this->Paint(g,x,y,this->mStartSelIndex,0,this->mText.length(),this->GetWidth(),this->GetHeight(),theInfo);
}

// FUNCTION: WON 0x0070A950
// Paints a character range of the text in its colour, drawing the selected part in highlight colours
void TextLineSegment::Paint(Graphics &g, int x, int y, int theSelIndex, int startChar, int endChar, int theWidth, int theHeight, ElementPaintInfo &theInfo)
{
	g.SetFont(this->mFont.get());
	if(theSelIndex>=theInfo.mStartSelSegment && theSelIndex<=theInfo.mEndSelSegment)
	{
		int aStartSelPos = startChar;
		int anEndSelPos = endChar;
		bool fullSelect = true;
		if(theSelIndex==theInfo.mStartSelSegment)
		{
			fullSelect = false;
			aStartSelPos = theInfo.mStartSelSegmentPos;
			if(aStartSelPos<startChar)
				aStartSelPos = startChar;
		}
		if(theSelIndex==theInfo.mEndSelSegment)
		{
			fullSelect = false;
			anEndSelPos = theInfo.mEndSelSegmentPos;
			if(anEndSelPos>endChar)
				anEndSelPos = endChar;
		}

		if(fullSelect)
		{
			g.SetSysColor(StandardColor_Hilight);
			g.FillRect(x,theInfo.mLineY,theWidth,theInfo.mLineHeight);
			g.SetSysColor(StandardColor_HilightText);
			g.DrawString(this->mText,startChar,endChar-startChar,x,y);
			return;
		}
		else if(aStartSelPos<anEndSelPos)
		{
			int aStartWidth = this->mFont->GetStringWidth(this->mText,startChar,aStartSelPos-startChar);
			int aSelWidth = this->mFont->GetStringWidth(this->mText,aStartSelPos,anEndSelPos-aStartSelPos);
			g.SetColor(this->mColor);
			g.DrawString(this->mText,startChar,aStartSelPos-startChar,x,y);
			g.DrawString(this->mText,anEndSelPos,endChar-anEndSelPos,x+aStartWidth+aSelWidth,y);
			g.SetSysColor(StandardColor_Hilight);
			g.FillRect(x+aStartWidth,theInfo.mLineY,aSelWidth,theInfo.mLineHeight);
			g.SetSysColor(StandardColor_HilightText);
			g.DrawString(this->mText,aStartSelPos,anEndSelPos-aStartSelPos,x+aStartWidth,y);
			return;
		}
	}

	g.SetColor(this->mColor);
	g.DrawString(this->mText,startChar,endChar-startChar,x,y);
}


// FUNCTION: WON 0x0070A590
// Word-wraps the text into pieces across lines of the builder, breaking at spaces and moving to a
// new line when a word does not fit
void TextLineSegment::Wrap(MultiLineBuilder *theContainer, bool needSegmentOnFirstLine)
{
	int aCurPos = 0;
	const GUIString &aStr = this->mText;
	Font *aFM = this->mFont.get();
	int aTextBoxWidth = theContainer->GetCurMaxLineWidth() - theContainer->CurLine()->GetWidth();
	
	int aWidth = 0;
	int aBreakWidth = 0;

	while(aCurPos < aStr.length())
	{
		int aStartPos = aCurPos;
		int aBreakPos = aStartPos-1;
		while(aCurPos < aStr.length())
		{
			int aChar = aStr[aCurPos];
			if(aChar==' ') 
			{
				aBreakPos = aCurPos;
				aBreakWidth = aWidth;
			}

			aWidth += aFM->GetCharacterWidth(aChar);

			if(aWidth>aTextBoxWidth && (aBreakPos>=aStartPos || !needSegmentOnFirstLine))
				break;

			aCurPos++;
		}

		if(aCurPos==aStr.length())
		{
			aBreakPos = aStr.length();
			aBreakWidth = aWidth;
		}
	
		needSegmentOnFirstLine = true;
		if(aBreakPos<aStartPos) // didn't find break
		{
			theContainer->AddLine();
			aCurPos = aStartPos;
			aTextBoxWidth = theContainer->GetCurMaxLineWidth();
			aWidth = aBreakWidth = 0;
			continue;
		}

		theContainer->AddSegmentToCurLine(new TextPieceLineSegment(this, aStartPos, aBreakPos, aBreakWidth));
		aCurPos = aBreakPos + 1; // Skip the space where we wrapped
		if(aCurPos>=aStr.length())
			break;

		theContainer->AddLine();
		aTextBoxWidth = theContainer->GetCurMaxLineWidth();
		aWidth = aFM->GetStringWidth(aStr,aCurPos, aStr.length()-aCurPos);
		if(aWidth <= aTextBoxWidth)
		{
			theContainer->AddSegmentToCurLine(new TextPieceLineSegment(this, aCurPos, aStr.length(), aWidth));
			break;
		}
		
		aWidth = aBreakWidth = 0;		
	}
}

// FUNCTION: WON 0x0070A2C0
// Returns the screen position of a character inside this piece; false when it lies outside
bool TextPieceLineSegment::GetSelCharPos(int &x, int &y, int theSelIndex, int theSelChar)
{
	if(theSelIndex!=this->mStartSelIndex)
		return false;

	if(theSelChar<this->mStartChar || theSelChar>this->mEndChar)
		return false;

	x = this->mAbsX + this->mSegment->mFont->GetStringWidth(this->mSegment->mText,this->mStartChar,theSelChar-this->mStartChar);
	y = this->mAbsY;
	return true;
}

// FUNCTION: WON 0x0070b330
// Returns the character of this piece nearest to pixel x
int TextPieceLineSegment::GetSelChar(int x,int y) { return this->mSegment->GetSelChar(x,y,this->mStartChar,this->mEndChar); }
// FUNCTION: WON 0x0070b350
// Paints this piece's character range of the segment
void TextPieceLineSegment::Paint(Graphics& g,int x,int y,ElementPaintInfo& info) {
    this->mSegment->Paint(g,x,y,this->mStartSelIndex,this->mStartChar,this->mEndChar,this->GetWidth(),this->GetHeight(),info);
}
}

namespace WONAPI {
// Paints only the selection background of a character range (not identified in the original exe)
void TextLineSegment::PaintSelection(Graphics &g, int x, int y, int theSelIndex, int startChar, int endChar, int theWidth, int theHeight, ElementPaintInfo &theInfo)
{
	g.SetFont(this->mFont.get());
	if(theSelIndex>=theInfo.mStartSelSegment && theSelIndex<=theInfo.mEndSelSegment)
	{
		int aStartSelPos = startChar;
		int anEndSelPos = endChar;
		bool fullSelect = true;
		if(theSelIndex==theInfo.mStartSelSegment)
		{
			fullSelect = false;
			aStartSelPos = theInfo.mStartSelSegmentPos;
			if(aStartSelPos<startChar)
				aStartSelPos = startChar;
		}
		if(theSelIndex==theInfo.mEndSelSegment)
		{
			fullSelect = false;
			anEndSelPos = theInfo.mEndSelSegmentPos;
			if(anEndSelPos>endChar)
				anEndSelPos = endChar;
		}

		if(fullSelect)
		{
			g.SetSysColor(StandardColor_Hilight);
			g.FillRect(x,theInfo.mLineY,theWidth,theInfo.mLineHeight);
//			g.ApplyColorSchemeColor(StandardColor_HilightText);
//			g.DrawString(mText,startChar,endChar-startChar,x,y);
			return;
		}
		else if(aStartSelPos<anEndSelPos)
		{
			int aStartWidth = this->mFont->GetStringWidth(this->mText,startChar,aStartSelPos-startChar);
			int aSelWidth = this->mFont->GetStringWidth(this->mText,aStartSelPos,anEndSelPos-aStartSelPos);
//			g.SetColor(mColor);
//			g.DrawString(mText,startChar,aStartSelPos-startChar,x,y);
//			g.DrawString(mText,anEndSelPos,endChar-anEndSelPos,x+aStartWidth+aSelWidth,y);
			g.SetSysColor(StandardColor_Hilight);
			g.FillRect(x+aStartWidth,theInfo.mLineY,aSelWidth,theInfo.mLineHeight);
//			g.ApplyColorSchemeColor(StandardColor_HilightText);
//			g.DrawString(mText,aStartSelPos,anEndSelPos-aStartSelPos,x+aStartWidth,y);
			return;
		}
	}

//	g.SetColor(mColor);
//	g.DrawString(mText,startChar,endChar-startChar,x,y);
}
}
// TEMPLATE: WON 0x0070a7d0
// (LineSegment::SetSelIndex, header inline; out-of-line copy for the vtable)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00709f90
// (public: virtual int __thiscall WONAPI::TextLineSegment::GetSelChar(int,int))
// TEMPLATE: WON 0x0070a810
// (public: virtual void __thiscall WONAPI::TextPieceLineSegment::MouseEnter(int,int))
// TEMPLATE: WON 0x0070a830
// (public: virtual void __thiscall WONAPI::TextPieceLineSegment::MouseExit(void))
// TEMPLATE: WON 0x0070a860
// (public: virtual void __thiscall WONAPI::TextPieceLineSegment::MouseUp(int,int,enum WONAPI::MouseButton))
// TEMPLATE: WON 0x0070a8a0
// (public: virtual void __thiscall WONAPI::TextPieceLineSegment::MouseDrag(int,int))
// TEMPLATE: WON 0x0070a8c0
// (public: virtual bool __thiscall WONAPI::TextPieceLineSegment::IsInvalid(void))
// TEMPLATE: WON 0x0070a8d0
// (public: virtual bool __thiscall WONAPI::TextPieceLineSegment::CanStartSelection(void))
// TEMPLATE: WON 0x0070a8e0
// (public: virtual int __thiscall WONAPI::TextPieceLineSegment::GetWidth(void))
// TEMPLATE: WON 0x0070a8f0
// (public: virtual int __thiscall WONAPI::TextPieceLineSegment::GetHeight(void))
