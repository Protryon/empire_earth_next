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
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
// The wrap's breaks, the selected part and the character under a point are ee_won's guitext module
// (rust/crates/ee_won/src/guitext.rs); this unit keeps the segments, the font (it measures every unit and the
// rest of a wrapped text) and the paint.
namespace WONAPI {
struct TextSegmentMeasure { Font* mFont; const GUIString* mText; };
static int32_t TextRestWidthOf(const void* inMeasure, int32_t inStart)
{
    const TextSegmentMeasure* aMeasure = static_cast<const TextSegmentMeasure*>(inMeasure);
    return aMeasure->mFont->GetStringWidth(*aMeasure->mText, (unsigned short)inStart,
                                           (unsigned short)(aMeasure->mText->length() - inStart));
}
static int32_t TextCharWidthOf(const void* inMeasure, int32_t inPos)
{
    const TextSegmentMeasure* aMeasure = static_cast<const TextSegmentMeasure*>(inMeasure);
    return aMeasure->mFont->GetCharacterWidth((*aMeasure->mText)[inPos]);
}
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
	if(ee_won_gui_text_sel_break(false, this->mLineBreak, theTextArea->mSelStartChar, theTextArea->mSelEndChar,
	                             this->mAbsStartChar, this->mAbsEndChar))
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
	int anOffset, aLength;
	if(!ee_won_gui_text_sel_part(theTextArea->mSelStartChar, theTextArea->mSelEndChar, this->mAbsStartChar,
	                             this->mAbsEndChar, &anOffset, &aLength))
		return;

	theStr.append(this->mText, anOffset, aLength);
	if(ee_won_gui_text_sel_break(true, this->mLineBreak, theTextArea->mSelStartChar, theTextArea->mSelEndChar,
	                             this->mAbsStartChar, this->mAbsEndChar))
		theStr.append("\r\n");
}

// FUNCTION: WON 0x710fb0
// Word-wraps the text into pieces over new lines of the text area, breaking at spaces (mid-word when
// a word fills a whole line); the segment's line becomes the first line with a piece
void TextSegment::Wrap(TextArea *theTextArea, TextLine *theLine)
{
	this->mLine = NULL; // set mLine to NULL then reset it to aLine on the first line to which we add a segment piece

	GUIString &aStr = this->mText;
	Font *aFM = this->mFont.get();
	int aLength = aStr.length();
	// every unit measured once (the original measured the units its loop reached, again after a restart)
	std::vector<int> aWidths(aLength);
	for(int i = 0; i < aLength; i++)
		aWidths[i] = aFM->GetCharacterWidth(aStr[i]);
	TextSegmentMeasure aMeasure = { aFM, &aStr };
	std::vector<EeWonTextWrapOp> anOps(3 * aLength + 8);
	bool aLeft = theTextArea->mLineAlign==HorzAlign_Left;
	size_t aCount = ee_won_gui_text_wrap(aStr.mData->mText, (uint16_t)aLength, sizeof(wchar_t),
	                                     aLength ? &aWidths[0] : NULL, theTextArea->mScrollArea.width, theLine->mWidth,
	                                     theLine->mSegmentList.empty(), aLeft, theTextArea->mWrapIndentSize,
	                                     TextRestWidthOf, &aMeasure, &anOps[0], anOps.size());
	if(aCount > anOps.size())
	{
		anOps.resize(aCount);
		aCount = ee_won_gui_text_wrap(aStr.mData->mText, (uint16_t)aLength, sizeof(wchar_t), &aWidths[0],
		                              theTextArea->mScrollArea.width, theLine->mWidth, theLine->mSegmentList.empty(),
		                              aLeft, theTextArea->mWrapIndentSize, TextRestWidthOf, &aMeasure, &anOps[0],
		                              anOps.size());
	}
	TextLine *aLine = theLine;
	for(size_t i = 0; i < aCount; i++)
	{
		const EeWonTextWrapOp &anOp = anOps[i];
		if(anOp.kind == 1)
		{
			aLine = theTextArea->AddLine(this->mHeight, this->mAscent);
			if(aLeft)
				aLine->mWidth = anOp.width;
			continue;
		}
		theTextArea->AddSeg(new TextSegmentPiece(this, anOp.start, anOp.end, anOp.width));
		if(this->mLine==NULL)
			this->mLine = aLine;
	}
}

// FUNCTION: WON 0x711200
// Paints a character range of the text, drawing its selected part in the area's selection colours
void TextSegment::Paint(Graphics &g, TextArea *theTextArea, int theStartChar, int theEndChar, int xpos, int ypos, int lineHeight)
{
	// the text, then its selected part over the selection background (guidraw.rs segment)
	EeWonSegmentPaint s;
	s.area_sel_start = theTextArea->mSelStartChar;
	s.area_sel_end = theTextArea->mSelEndChar;
	s.abs_start = this->mAbsStartChar;
	s.start = theStartChar;
	s.end = theEndChar;
	s.x = xpos;
	s.y = ypos;
	s.line_height = lineHeight;
	s.color = (uint32_t)this->mColor;
	s.sel = WONDrawSelOf(theTextArea->mSelColor);
	WONDrawMeasure aMeasure = { this->mFont.get(), &this->mText };
	EeWonTextMeasure m = aMeasure.Get();
	WONDrawSlots aSlots;
	aSlots.mFont = this->mFont.get();
	aSlots.mString = &this->mText;
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_segment(&s,&m,o,n); });
	WONDrawRun(g,anOps,aSlots);
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
	TextSegmentMeasure aMeasure = { this->mFont.get(), &this->mText };
	return ee_won_gui_text_char_at(theWidth, theStartChar, theEndChar, this->mAbsStartChar, TextCharWidthOf, &aMeasure);
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
