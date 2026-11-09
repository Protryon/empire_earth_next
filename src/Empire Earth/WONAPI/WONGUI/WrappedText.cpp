// Word-wrapped text: line breaking to a maximum width, plain or rich, and painting
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

// FUNCTION: WON 0x0074a7c0
// Resets the text to wrapping, left-aligned and zero size with no maximum width
void WrappedText::Init()
{
	this->mWrap = true;
	this->mIsCentered = false;
	this->mWidth = this->mHeight = this->mMaxWidth = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074a7e0
// Creates empty wrapped text with the default settings
WrappedText::WrappedText()
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


// FUNCTION: WON 0x0074a880
// Destroys the wrapped text and frees it when asked (scalar deleting destructor)
// (??_GWrappedText@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0074a8a0
// Destroys the wrapped text, releasing its string, font, rich text and line breaks
// (??1WrappedText@WONAPI@@UAE@XZ)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074a9b0
// Recomputes the line breaks and the text's width and height for the font and maximum width
// (rich text without wrapping grows its maximum width by 10 on every call)
void WrappedText::CalcWrap()
{
	if(this->mFont.get()==NULL)
		return;

	if(this->mRichText.get()!=NULL)
	{
		if(this->mWrap)
		{
			if(this->mMaxWidth==0)
				return;

			this->mRichText->SetMaxWidth(this->mMaxWidth);
		}
		else
			this->mRichText->SetMaxWidth(this->mRichText->GetMaxWidth()+10);

		this->mWidth = this->mRichText->GetWidth();
		this->mHeight = this->mRichText->GetHeight();
		return;
	}

	if(!this->mWrap)
	{
		this->mWidth = this->mFont->GetStringWidth(this->mText,0,this->mText.length());
		this->mHeight = this->mFont->GetHeight();
		return;
	}

	if(this->mMaxWidth==0)
		return;

	this->mBreakList.clear();

	int aBreakPos = -1, aBreakWidth = 0;
	int aPos = 0, aWidth = 0, aMaxWidth = 0;
	while(aPos<this->mText.length())
	{
		int aChar = this->mText[aPos];
		int aCharWidth = this->mFont->GetCharacterWidth(aChar);
		if(aChar==' ')
		{
			aBreakPos = aPos;
			aBreakWidth = aWidth;
		}
		else if(aChar=='\r' || aChar=='\n')
		{
			aBreakPos = aPos;
			aBreakWidth = aWidth;
			if(aChar=='\r' && this->mText[aPos+1]=='\n')
				aPos++;

			aPos++;
			this->mBreakList.push_back(BreakInfo(aBreakPos,aPos,aBreakWidth));
			if(aBreakWidth > aMaxWidth)
				aMaxWidth = aBreakWidth;

			aBreakPos = -1;
			aWidth = 0;
			aBreakWidth = 0;
			continue;
		}

		if(aWidth+aCharWidth>this->mMaxWidth)
		{
			if(aBreakPos!=-1)
			{
				this->mBreakList.push_back(BreakInfo(aBreakPos,aBreakPos+1,aBreakWidth));
				aPos = aBreakPos;
				if(aBreakWidth > aMaxWidth)
					aMaxWidth = aBreakWidth;
			}
			else
			{
				this->mBreakList.push_back(BreakInfo(aPos,aPos,aWidth));
				if(aWidth > aMaxWidth)
					aMaxWidth = aWidth;
	
				if(aWidth>0)
					aPos--;
			}

			aBreakPos = -1;
			aWidth = 0;
			aBreakWidth = 0;
		}
		else
			aWidth += aCharWidth;

		aPos++;
	}
	if(aWidth>aMaxWidth)
		aMaxWidth = aWidth;

	if(aMaxWidth==0)
		this->mWidth = aWidth;
	else
		this->mWidth = aMaxWidth;

	this->mHeight = (this->mBreakList.size() + 1)*this->mFont->GetHeight();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074acc0
// Sets the text and rewraps it
void WrappedText::SetText(const GUIString &theText)
{
	this->mText = theText;
	this->CalcWrap();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// SetRichText is not defined in this file



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074ad20
// Turns wrapping on or off, rewrapping when it changes
void WrappedText::SetWrap(bool wrap)
{
	if(wrap!=this->mWrap)
	{
		this->mWrap = wrap;
		this->CalcWrap();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074ad40
// Sets whether each line is centred
void WrappedText::SetCentered(bool isCentered)
{
	this->mIsCentered = isCentered;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074ad50
// Sets the font and rewraps the text
void WrappedText::SetFont(Font *theFont)
{
	this->mFont = theFont;
	this->CalcWrap();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074adb0
// Sets the maximum line width (at least 0), rewrapping only when the new width can change the breaks
void WrappedText::SetMaxWidth(int theMaxWidth)
{
	if(theMaxWidth<0)
		theMaxWidth = 0;

	if(theMaxWidth==this->mMaxWidth)
		return;
	int anOldMax = this->mMaxWidth;
	this->mMaxWidth = theMaxWidth;
	if(!this->mWrap)
		return;

	bool needCalc = this->mRichText.get() || anOldMax==0 || !this->mBreakList.empty() || this->mMaxWidth<anOldMax;
	if(needCalc)
		this->CalcWrap();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074adf0
// Draws the rich text, or each wrapped line of plain text (centred if set) from the given top-left point
void WrappedText::Paint(Graphics &g, int x, int y)
{
	if(this->mRichText.get()!=NULL)
	{
		ElementPaintInfo anInfo;
		anInfo.mClipX = x;
		anInfo.mClipY = y;
		anInfo.mClipWidth = this->GetWidth();
		anInfo.mClipHeight = this->mHeight==0 ? this->mFont->GetHeight() : this->mHeight;
		anInfo.mStartSelSegment = anInfo.mEndSelSegment = -1;
		this->mRichText->Paint(g,x,y,anInfo);
		return;
	}

	if(this->mFont.get()==NULL)
		return;

	g.SetFont(this->mFont.get());

	int aYPos = y;
	int aStrPos = 0;
	for(BreakList::iterator anItr = this->mBreakList.begin(); anItr!=this->mBreakList.end(); ++anItr)
	{
		if(aStrPos>=this->mText.length())
			break;

		int aBreakPos = anItr->mBreakPos;
		int anXPos = x;
		if(this->mIsCentered)
			anXPos += (this->mWidth - anItr->mWidth)/2;

		g.DrawString(this->mText, aStrPos, aBreakPos - aStrPos, anXPos, aYPos);
		aYPos += this->mFont->GetHeight();
		aStrPos = anItr->mNextStartPos;
	}
	if(!(aStrPos<this->mText.length()))
		return;
	if(this->mIsCentered && !this->mBreakList.empty())
	{
		int aWidth = this->mFont->GetStringWidth(this->mText,aStrPos,this->mText.length()-aStrPos);
		int anXPos = x + (this->mWidth - aWidth)/2;
		g.DrawString(this->mText, aStrPos, this->mText.length()-aStrPos, anXPos, aYPos);
	}
	else
		g.DrawString(this->mText, aStrPos, this->mText.length()-aStrPos, x, aYPos);
}	

namespace WONAPI {
// Creates wrapped text holding the given string (not identified in the original exe)

WrappedText::WrappedText(const GUIString &theText)
{
	this->Init();
	this->mText = theText;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007508c0
// (protected: void __thiscall std::list<struct WONAPI::WrappedText::BreakInfo,class std::allocator<struct WONAPI::WrappedText::BreakInfo> >::_Incsize(uns)
// TEMPLATE: WON 0x00754bc0
// (void __cdecl std::_Construct(struct WONAPI::WrappedText::BreakInfo *,struct WONAPI::WrappedText::BreakInfo const &))
