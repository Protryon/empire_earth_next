// Rich text multi-line element: hit testing, selection, search, segment list and painting (file name inferred)
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

// FUNCTION: WON 0x0070CA30
// Returns the segment under a point (made relative to it), checking lines then floats; the element itself if none
LineSegment* MultiLineElement::GetSegmentAt(int &x, int &y, bool lenient) 
{ 
	LineVector::iterator anItr = this->FindLineAt(y);
	FloatingSegment *aFloat = NULL; 
	if(anItr!=this->mLineVector.end())
	{
		LineElement *aLine = anItr->get();
		if(y>=aLine->my && y<aLine->my+aLine->GetHeight() && aLine->mLeftMargin<=x && aLine->mLeftMargin+aLine->mWidth>=x)
		{
			y-=aLine->my;
			return aLine->GetSegmentAt(x,y,lenient);
		}
		else if(aLine->mLeftMargin>x) // find left floating element
			aFloat = this->FindFloatingSegmentAt(x,y,true);
		else // find right floating element
			aFloat = this->FindFloatingSegmentAt(x,y,false);

		if(lenient && aFloat==NULL)
		{
			y-=aLine->my;
			return aLine->GetSegmentAt(x,y,lenient);
		}
	}
	else
	{
		aFloat = this->FindFloatingSegmentAt(x,y,true);
		if(aFloat==NULL)
			aFloat = this->FindFloatingSegmentAt(x,y,false);
	}

	if(aFloat==NULL)
		return this;

	x-=aFloat->mx;
	y-=aFloat->my;
	return aFloat->mSegment->GetSegmentAt(x,y,lenient);
}

// FUNCTION: WON 0x0070CB60
// Attaches the element, each of its lines and every floating segment to a scroll viewport at their offsets
void MultiLineElement::SetViewport(ScrollAreaViewport *theViewport, int x, int y)
{
	LineSegment::SetViewport(theViewport,x,y);
	LineVector::iterator anItr = this->mLineVector.begin();
	while(anItr!=this->mLineVector.end())
	{
		LineElement *aLine = anItr->get();
		aLine->SetViewport(theViewport,x,y+aLine->my);
		++anItr;
	}

	for(int i=0; i<2; i++)
	{
		for(FloatingVector::iterator anItr =  this->mFloatingItems[i].begin(); anItr!=this->mFloatingItems[i].end(); ++anItr)
		{
			FloatingSegment &aSeg = *anItr;
			aSeg.mSegment->SetViewport(theViewport,x + aSeg.mx, y + aSeg.my);
		}
	}

}

// FUNCTION: WON 0x0070CBF0
// Numbers the element and then each of its segments for selection
void MultiLineElement::SetSelIndex()
{
	LineSegment::SetSelIndex();
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		anItr->mSegment->SetSelIndex();
		++anItr;
	}
}

// FUNCTION: WON 0x0070CC50
// Appends the selected text of the segments inside the selection range (no separators are added between lines)
void MultiLineElement::GetSelText(GUIString &theText, ElementSelectionInfo &theInfo)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->mSegment.get();
		if(aSeg->GetEndSelIndex()>=theInfo.mStartSelSegment)
			break;

		++anItr;
	}

	bool hadText = false;
	while(anItr!=this->mSegmentList.end())
	{
		SegmentInfo &aSegInfo = *anItr;
		LineSegment *aSeg = aSegInfo.mSegment.get();
		if(aSeg->mStartSelIndex<=theInfo.mEndSelSegment)
		{
			if((aSegInfo.mFlags&4) && hadText && aSeg->mStartSelIndex!=theInfo.mStartSelSegment)
			{
				hadText = false;
			}

			int anOldLen = theText.length();
			aSeg->GetSelText(theText,theInfo);
			hadText = theText.length()!=anOldLen;
		
			if((aSegInfo.mFlags&8) && hadText && aSeg->mStartSelIndex!=theInfo.mEndSelSegment)
			{
				hadText = false;
			}
		}
		else
			break;			

		++anItr;
	}
}

// FUNCTION: WON 0x0070CCD0
// Finds the position of a selection character in the lines, then in the floats; returns false if none has it
bool MultiLineElement::GetSelCharPos(int &x, int &y, int theSelIndex, int theSelChar)
{
	LineVector::iterator aLineItr = this->mLineVector.begin();
	while(aLineItr!=this->mLineVector.end())
	{
		LineElement *anElement = aLineItr->get();
		if(anElement->GetSelCharPos(x,y,theSelIndex,theSelChar))
			return true;

		++aLineItr;
	}

	for(int i=0; i<2; i++)
	{
		FloatingVector::iterator anItr = this->mFloatingItems[i].begin();
		while(anItr!=this->mFloatingItems[i].end())
		{
			FloatingSegment &aSegment = *anItr;
			if(aSegment.mSegment->GetSelCharPos(x,y,theSelIndex,theSelChar))
				return true;

			++anItr;
		}
	}

	return false;
}

// FUNCTION: WON 0x0070CD70
// Searches the segments for the find string (forward, or the last hit when searching backwards) and sets its position
bool MultiLineElement::FindStr(ElementFindInfo &theInfo)
{
	SegmentList::iterator anItr = this->mSegmentList.begin();
	while(anItr!=this->mSegmentList.end())
	{
		LineSegment *aSeg = anItr->mSegment.get();
		if(aSeg->GetEndSelIndex()>=theInfo.mStartSelSegment)
			break;

		++anItr;
	}

	bool found = false;
	SegmentList::iterator aStartFindItr = this->mSegmentList.end();
	ElementFindInfo aFoundInfo;

	while(anItr!=this->mSegmentList.end())
	{
		SegmentInfo &aSegInfo = *anItr;
		LineSegment *aSeg = aSegInfo.mSegment.get();
		if(aSeg->mStartSelIndex<=theInfo.mEndSelSegment)
		{
			if((aSegInfo.mFlags&4))
				theInfo.mFoundStartSegment = -1;

			if(aSeg->FindStr(theInfo))
			{
				found = true;
				aFoundInfo = theInfo;
				if(theInfo.mFindPrev)
				{
					if(aStartFindItr!=this->mSegmentList.end())
						anItr = aStartFindItr;

					theInfo.mStartSelSegment = theInfo.mFoundStartSegment;
					theInfo.mStartSelSegmentPos = theInfo.mFoundStartChar+1;
					theInfo.mCurFoundLength = 0;
					theInfo.mFoundStartSegment = -1;
					continue;
				}
				else
					break;
			}
			else if(theInfo.mCurFoundLength>0 && aStartFindItr==this->mSegmentList.end())
				aStartFindItr = anItr;
			else
				aStartFindItr = this->mSegmentList.end();


			if((aSegInfo.mFlags&8))
				theInfo.mFoundStartSegment = -1;
		}
		else
			break;			

		++anItr;
	}

	if(!found)
	{
		theInfo.mStartSelSegment = -1;
		return false;
	}
	else
	{
		theInfo = aFoundInfo;
		this->GetSelCharPos(theInfo.mFoundX,theInfo.mFoundY,theInfo.mFoundStartSegment,theInfo.mFoundStartChar);
		return true;
	}
}

// FUNCTION: WON 0x0070D570
// Appends a segment with its layout flags, updating the element's minimum and maximum widths
void MultiLineElement::AddSegment(LineSegment *theSegment, unsigned short theFlags)
{
	int aMinWidth = theSegment->GetMinWidth();
	if(aMinWidth>this->mMinWidth)
		this->mMinWidth = aMinWidth;

	if(theFlags & 4)
		this->mCurLineWidth = 0;

	this->mCurLineWidth += theSegment->GetMaxWidth();
	if(this->mCurLineWidth>this->mMaxWidth)
		this->mMaxWidth = this->mCurLineWidth;

	if(theFlags & 8)
		this->mCurLineWidth = 0;


	this->mSegmentList.push_back(SegmentInfo(theSegment,theFlags));
}

// FUNCTION: WON 0x0070D6E0
// Paints the lines inside the clip rectangle and then every floating segment
void MultiLineElement::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	int aTop = theInfo.mClipY-y;
	int aBottom = (theInfo.mClipY+theInfo.mClipHeight)-y;
	if(aTop>this->mHeight || theInfo.mClipX-x>this->mWidth || aBottom<0 || (theInfo.mClipX+theInfo.mClipWidth)-x<0)
		return;

	LineVector::iterator anItr;
	if(aTop>0)
		anItr = this->FindLineAt(aTop);
	else
		anItr = this->mLineVector.begin();

	while(anItr!=this->mLineVector.end())
	{
		LineElement *anElement = anItr->get();
		if(anElement->my > aBottom)
			break;

		anElement->Paint(g,x,y+anElement->my,theInfo);
		++anItr;
	}

	FloatingVector::iterator aFloatItr;
	for(int i=0; i<2; i++)
	{
		for(aFloatItr=this->mFloatingItems[i].begin(); aFloatItr!=this->mFloatingItems[i].end(); ++aFloatItr)
		{
			FloatingSegment *aSegment = &*aFloatItr;
			theInfo.mLineY = y+aSegment->my;
			theInfo.mLineHeight = aSegment->GetHeight();
			aSegment->mSegment->Paint(g,x+aSegment->mx,y+aSegment->my,theInfo);
		}
	}
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00724380
// (void __cdecl std::_Construct(struct WONAPI::SegmentInfo *,struct WONAPI::SegmentInfo const &)    [?_Construct@std@@YAXPAUSegmentInfo@WONAPI@@ABU23@@Z])
