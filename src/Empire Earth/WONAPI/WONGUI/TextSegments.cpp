// Text area segments: word wrap, selected text, painting and character hit tests
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00710aa0
// Moves a segment that does not fit onto a new line (unless its line is empty), indented by the wrap
// indent
void TextSegmentBase::Wrap(TextArea *theTextArea, TextLine *theLine)
{
	if(!theLine->mSegmentList.empty())
		theLine = theTextArea->AddLine();

	theLine->mWidth = theTextArea->mWrapIndentSize;
	theTextArea->AddSeg(this);
}

// FUNCTION: WON 0x00710ae0
// Appends a line break to the copied text when the selection spans this segment's line break
void TextSegmentBase::GetSelText(TextArea *theTextArea, GUIString &theStr)
{
	if(this->mLineBreak && theTextArea->mSelStartChar<=this->mAbsStartChar && theTextArea->mSelEndChar>this->mAbsEndChar)
		theStr.append("\r\n");
}

// FUNCTION: WON 0x00710d70
// Sizes the segment from its text width and its font's height and ascent
void TextSegment::CalcDimensions()
{
	this->mWidth = this->mFont->GetStringWidth(this->mText);
	this->mHeight = this->mFont->GetHeight();
	this->mAscent = this->mFont->GetAscent();
}

// FUNCTION: WON 0x710db0
// Appends the selected part of the segment's text, plus a line break when the selection runs past
// its line break
void TextSegment::GetSelText(TextArea *theTextArea, GUIString &theStr)
{
	int aSelStartChar = theTextArea->mSelStartChar;
	int aSelEndChar = theTextArea->mSelEndChar;

	if(this->mAbsStartChar > aSelStartChar)
		aSelStartChar = this->mAbsStartChar;

	if(this->mAbsEndChar < aSelEndChar)
		aSelEndChar = this->mAbsEndChar;

	if(aSelStartChar >= aSelEndChar)
		return;

	aSelStartChar -= this->mAbsStartChar;
	aSelEndChar -= this->mAbsStartChar;

	theStr.append(this->mText, aSelStartChar, aSelEndChar-aSelStartChar);
	if(this->mLineBreak && theTextArea->mSelEndChar>this->mAbsEndChar)
		theStr.append("\r\n");
}

// FUNCTION: WON 0x710fb0
// Word-wraps the text into pieces over new lines of the text area, breaking at spaces (mid-word when
// a word fills a whole line); the segment's line becomes the first line with a piece
void TextSegment::Wrap(TextArea *theTextArea, TextLine *theLine)
{
	this->mLine = NULL; // set mLine to NULL then reset it to aLine on the first line to which we add a segment piece

	int aCurPos = 0;
	GUIString &aStr = this->mText;
	Font *aFM = this->mFont.get();
	int aTextBoxWidth = theTextArea->mScrollArea.width; 
	TextLine *aLine = theLine;

	while(aCurPos < aStr.length())
	{
		int aWidth = aLine->mWidth;
		int aBreakWidth = aLine->mWidth;
		int aStartPos = aCurPos;
		int aBreakPos = aStartPos-1;
		while(aCurPos < aStr.length())
		{
			int aChar = aStr[aCurPos];
			aWidth += aFM->GetCharacterWidth(aChar);
			if(aChar==' ') 
			{
				aBreakPos = aCurPos;
				aBreakWidth = aWidth;
			}

			if(aWidth > aTextBoxWidth)
				break;

			aCurPos++;
		}

		bool forceBreak = false;
		if(aBreakPos < aStartPos)
		{
			if(aWidth < aTextBoxWidth)
				aBreakPos = aStr.length(); 
			else if(aLine->mSegmentList.empty()) // force the break in the middle of the word if we're at the beginning of the line
			{
				aBreakPos = aCurPos;
				aBreakWidth = aTextBoxWidth;
				if(aBreakPos==aStartPos)	// need at least one character per line
					aBreakPos++;

				forceBreak = true;
			}
			else	// otherwise try again on new line
				aCurPos = 0;
		}

		if(aBreakPos >= aStartPos)
		{
			if(aBreakPos!=aStartPos)
			{
				theTextArea->AddSeg(new TextSegmentPiece(this, aStartPos, aBreakPos, aBreakWidth/*aTextBoxWidth-aLine->mWidth*/));
				if(this->mLine==NULL)
					this->mLine = aLine;
			}

			aCurPos = aBreakPos;
			if(!forceBreak)	// Skip the space where we wrapped
				aCurPos++;
		}

		if(aCurPos>=aStr.length())
			break;

		aLine = theTextArea->AddLine(this->mHeight, this->mAscent);
		if(theTextArea->mLineAlign==HorzAlign_Left)
			aLine->mWidth = theTextArea->mWrapIndentSize;

		aWidth = aFM->GetStringWidth(aStr, aCurPos, aStr.length()-aCurPos);
		if(!(aWidth <= aTextBoxWidth - aLine->mWidth))
			continue;
		theTextArea->AddSeg(new TextSegmentPiece(this, aCurPos, aStr.length(), aWidth));
		if(this->mLine==NULL)
			this->mLine = aLine;

		break;
	}
}

// FUNCTION: WON 0x711200
// Paints a character range of the text, drawing its selected part in the area's selection colours
void TextSegment::Paint(Graphics &g, TextArea *theTextArea, int theStartChar, int theEndChar, int xpos, int ypos, int lineHeight)
{
	int aSelStartChar = theTextArea->mSelStartChar - this->mAbsStartChar;
	int aSelEndChar = theTextArea->mSelEndChar - this->mAbsStartChar;

	if(theStartChar > aSelStartChar)
		aSelStartChar = theStartChar;

	if(theEndChar < aSelEndChar)
		aSelEndChar = theEndChar;

	g.SetColor(this->mColor);	
	g.SetFont(this->mFont.get());
	
	bool fullSelection = aSelStartChar==theStartChar && aSelEndChar==theEndChar;
	if(!fullSelection)
		g.DrawString(this->mText, theStartChar, theEndChar - theStartChar, xpos, ypos);

	if(!(aSelEndChar > aSelStartChar))
		return;
	int x = xpos + this->mFont->GetStringWidth(this->mText,theStartChar,aSelStartChar-theStartChar);
	int aWidth = this->mFont->GetStringWidth(this->mText,aSelStartChar,aSelEndChar-aSelStartChar);

	g.SetColor(theTextArea->mSelColor.GetBackgroundColor(g));
	g.FillRect(x,0,aWidth,lineHeight);

	g.SetColor(theTextArea->mSelColor.GetForegroundColor(g,this->mColor));
	g.DrawString(this->mText,aSelStartChar,aSelEndChar-aSelStartChar,x,ypos);
}

// FUNCTION: WON 0x00711330
// Paints the whole segment at its position on its line
void TextSegment::Paint(Graphics &g, TextArea *theTextArea)
{
	this->Paint(g,theTextArea,0,this->mText.length(),this->mx,this->my,this->mLine->mHeight);
}

// FUNCTION: WON 0x711360
// Returns the text position nearest to a pixel offset within a character range (rounding at half a
// character)
int TextSegment::CharAt(int theWidth, int theStartChar, int theEndChar)
{
	int aPos = theStartChar;

	int aWidth = 0;
	int aCharWidth = 0;
	while(aPos<theEndChar && aWidth<theWidth)
	{
		aCharWidth = this->mFont->GetCharacterWidth(this->mText[aPos]);
		aWidth+=aCharWidth;
		aPos++;
	}

	if(theWidth>=0 && theWidth<aWidth-aCharWidth/2)
		aPos--;

	return aPos + this->mAbsStartChar;	
}

// FUNCTION: WON 0x007113c0
// Returns the text position nearest to x on the segment's line
int TextSegment::CharAt(int x)
{
	return this->CharAt(x-this->mx, 0, this->mText.length());
}

// FUNCTION: WON 0x007114a0
// Paints this piece's character range of its segment
void TextSegmentPiece::Paint(Graphics &g, TextArea *theTextArea)
{
	this->mSeg->Paint(g,theTextArea,this->GetStartChar(),this->GetEndChar(),this->mx,this->my,this->mLine->mHeight);
}

// FUNCTION: WON 0x007114d0
// Returns the text position nearest to x within this piece
int TextSegmentPiece::CharAt(int x)
{
	return this->mSeg->CharAt(x-this->mx,this->GetStartChar(),this->GetEndChar());
}
}
