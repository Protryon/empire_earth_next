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
#include "rust/ee_rust.h"
#include "ee/wonapi/WONGUIDraw.h"
// The word wrap and the rewrap rule are ee_won's guiwrap module (rust/crates/ee_won/src/guiwrap.rs); this unit
// keeps the text, the font, the break list and the paint.



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
void WrappedText::CalcWrap()
{
	if(this->mFont.get()==NULL)
		return;

	if(!this->mWrap)
	{
		this->mWidth = this->mFont->GetStringWidth(this->mText,0,this->mText.length());
		this->mHeight = this->mFont->GetHeight();
		return;
	}

	if(this->mMaxWidth==0)
		return;

	this->mBreakList.clear();

	// the font measures each unit; the breaks are ee_won's guiwrap module
	int aLength = this->mText.length();
	std::vector<int> aWidths(aLength);
	for(int i = 0; i < aLength; i++)
		aWidths[i] = this->mFont->GetCharacterWidth(this->mText[i]);
	std::vector<EeWonBreak> aBreaks(2*aLength+1);
	int aWidth = 0;
	size_t aCount = ee_won_gui_wrap(this->mText.mData->mText, (uint16_t)aLength, sizeof(wchar_t),
		aLength ? &aWidths[0] : NULL, this->mMaxWidth, &aBreaks[0], aBreaks.size(), &aWidth);
	if(aCount > aBreaks.size())
	{
		aBreaks.resize(aCount);
		aCount = ee_won_gui_wrap(this->mText.mData->mText, (uint16_t)aLength, sizeof(wchar_t),
			aLength ? &aWidths[0] : NULL, this->mMaxWidth, &aBreaks[0], aBreaks.size(), &aWidth);
	}
	for(size_t k = 0; k < aCount; k++)
		this->mBreakList.push_back(BreakInfo(aBreaks[k].pos,aBreaks[k].next,aBreaks[k].width));
	this->mWidth = aWidth;

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
	int aWidth = 0;
	bool aRewrap = false;
	if(!ee_won_gui_max_width(theMaxWidth, this->mMaxWidth, this->mWrap, !this->mBreakList.empty(), &aWidth, &aRewrap))
		return;
	this->mMaxWidth = aWidth;
	if(aRewrap)
		this->CalcWrap();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0074adf0
// Draws each wrapped line of the text (centred if set) from the given top-left point
void WrappedText::Paint(Graphics &g, int x, int y)
{
	if(this->mFont.get()==NULL)
		return;

	// each broken line, centred when set, then the rest (guidraw.rs wrapped_text)
	std::vector<EeWonLineBreak> aBreaks;
	for(BreakList::iterator anItr = this->mBreakList.begin(); anItr!=this->mBreakList.end(); ++anItr)
	{
		EeWonLineBreak b;
		b.break_pos = anItr->mBreakPos;
		b.next_start = anItr->mNextStartPos;
		b.width = anItr->mWidth;
		aBreaks.push_back(b);
	}
	WONDrawMeasure aMeasure = { this->mFont.get(), &this->mText };
	EeWonTextMeasure m = aMeasure.Get();
	int aFontHeight = this->mFont->GetHeight();
	WONDrawSlots aSlots;
	aSlots.mFont = this->mFont.get();
	aSlots.mString = &this->mText;
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_wrapped_text(true,this->mText.length(),this->mIsCentered,this->mWidth,aFontHeight,
			aBreaks.empty() ? NULL : &aBreaks[0],aBreaks.size(),x,y,&m,o,n); });
	WONDrawRun(g,anOps,aSlots);
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
