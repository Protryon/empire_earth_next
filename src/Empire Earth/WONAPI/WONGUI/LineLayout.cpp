// Rich-text lines: build metrics, segment positioning, selection traversal and drawing (file name inferred)
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
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {

// FUNCTION: WON 0x0070B5D0
// Appends a segment to the line being built and grows its width, ascent and descent (top-aligned
// segments only raise the top-align height)
void LineBuilder::AddSegment(LineSegment *theSegment)
{
	this->mLine->mSegmentList.push_back(theSegment);
	this->mWidth += theSegment->GetWidth();

	int anAscent;
	int aDescent;
	int aTopHeight;
	switch(theSegment->GetVertAlign())
	{
		case 0: // align with baseline
			anAscent = theSegment->GetAscent();
			aDescent = theSegment->GetHeight() - anAscent;
			break;

		case 1: // align middle
			anAscent = aDescent = theSegment->GetHeight()/2;
			break;

		case 2: // align top
			aTopHeight = theSegment->GetHeight();
			if(aTopHeight>this->mTopAlignHeight)
				this->mTopAlignHeight = aTopHeight;
			return;
	}

	if(anAscent > this->mMaxAscent)
		this->mMaxAscent = anAscent;

	if(aDescent > this->mMaxDescent)
		this->mMaxDescent = aDescent;

}

// FUNCTION: WON 0x0070b7c0
// Creates an empty line with zero baseline, size and left margin, not a glue line
LineElement::LineElement()
{
    this->mBaseline=this->mWidth=this->mHeight=this->mLeftMargin=0;
    this->mIsGlueLine=false;
}
// FUNCTION: WON 0x0070B920
// Finds the segment under a line-relative point and makes the point relative to it; returns the line
// itself when the point misses (lenient picks the last segment and ignores height)
LineSegment* LineElement::GetSegmentAt(int &x, int &y, bool lenient)
{
	if(x<this->mLeftMargin && !lenient)
		return this;

	int anXPos = this->mLeftMargin;
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = (*anItr++).get();
		int aWidth = aSeg->GetWidth();
		if(anXPos + aWidth >= x || (lenient && anItr==this->mSegmentList.end()))
		{
			int aYPos = 0;
			int aHeight = aSeg->GetHeight();
			switch(aSeg->GetVertAlign())
			{
				case 0: aYPos = this->mBaseline-aSeg->GetAscent(); break;
				case 1: aYPos = this->mBaseline-aSeg->GetAscent()/2; break;
			}

			if((y>=aYPos && y<=aYPos+aHeight) || lenient)
			{
				x-=anXPos;
				y-=aYPos;
				return aSeg->GetSegmentAt(x, y, lenient);
			}
			else
				return this;
		}
		anXPos += aWidth;
	}

	return this;
}

// FUNCTION: WON 0x0070BA00
// Places the line in its viewport and lays its segments out left to right by their vertical alignment
void LineElement::SetViewport(ScrollAreaViewport *theViewport, int x, int y)
{
	LineSegment::SetViewport(theViewport,x,y);
	x+=this->mLeftMargin;
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		int aYPos = y;
		switch(aSeg->GetVertAlign())
		{
			case 0: aYPos = y+this->mBaseline-aSeg->GetAscent(); break;
			case 1: aYPos = y+this->mBaseline-aSeg->GetAscent()/2; break;
			case 2: aYPos = y; break;
		}

		aSeg->SetViewport(theViewport,x,aYPos);
		x += aSeg->GetWidth();
		++anItr;
	}
}

// FUNCTION: WON 0x0070BAB0
// Numbers the line and then each of its segments for selection
void LineElement::SetSelIndex()
{
	LineSegment::SetSelIndex();
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		(*anItr)->SetSelIndex();
		++anItr;
	}

}

// FUNCTION: WON 0x0070BB10
// Appends the selected text of the segments inside the selection range
void LineElement::GetSelText(GUIString &theText, ElementSelectionInfo &theInfo)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		if(aSeg->mStartSelIndex>=theInfo.mStartSelSegment)
			break;

		++anItr;
	}

	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		if(aSeg->mStartSelIndex<=theInfo.mEndSelSegment)
			aSeg->GetSelText(theText,theInfo);
		else
			break;			

		++anItr;
	}
}

// FUNCTION: WON 0x0070BB70
// Asks each segment for the position of a selection character; true when one finds it
bool LineElement::GetSelCharPos(int &x, int &y, int theSelIndex, int theSelChar)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		if(aSeg->GetSelCharPos(x,y,theSelIndex,theSelChar))
			return true;

		++anItr;
	}
	return false;
}

// FUNCTION: WON 0x0070BBD0
// Searches the segments inside the search range for the string; true on the first hit
bool LineElement::FindStr(ElementFindInfo &theInfo)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		if(aSeg->mStartSelIndex>=theInfo.mStartSelSegment)
			break;

		++anItr;
	}

	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		if(aSeg->mStartSelIndex<=theInfo.mEndSelSegment)
		{
			if(aSeg->FindStr(theInfo))
				return true;
		}
		else
			break;			

		++anItr;
	}

	return false;
}

// FUNCTION: WON 0x0070BC40
// Draws each segment at its aligned position, recording the line's top and height for the segments
// unless it is a glue line
void LineElement::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	int oldY = theInfo.mLineY, oldHeight = theInfo.mLineHeight;

	if(!this->mIsGlueLine)
	{
		theInfo.mLineY = y; 
		theInfo.mLineHeight = this->mHeight;

	}


	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->get();
		switch(aSeg->GetVertAlign())
		{
			case 0: aSeg->Paint(g,x+this->mLeftMargin,y+this->mBaseline-aSeg->GetAscent(),theInfo); break;
			case 1: aSeg->Paint(g,x+this->mLeftMargin,y+this->mBaseline-aSeg->GetAscent()/2,theInfo); break;
			case 2: aSeg->Paint(g,x+this->mLeftMargin,y,theInfo); break;
		}
		x+=aSeg->GetWidth();
		++anItr;
	}

	theInfo.mLineY = oldY; theInfo.mLineHeight = oldHeight;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070b880
// (public: virtual void * __thiscall WONAPI::LineElement::`scalar deleting destructor'(unsigned int)    [??_GLineElement@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070b8a0
// (public: virtual __thiscall WONAPI::LineElement::~LineElement(void))
