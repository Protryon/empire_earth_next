// Table display: content sizing, selection autoscroll, painting, anchors and text search (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
// FUNCTION: WON 0x00710450
// While a mouse selection is in progress and the mouse is outside the view, scrolls toward it and extends
// the selection; returns whether the timer is still needed
bool Table::TimerEvent(int theDelta)
{
	bool needTimer = ScrollArea::TimerEvent(theDelta);
	needTimer = needTimer || this->mSelecting;

	if(!this->mSelecting)
		return needTimer;

	int x,y;
	this->GetMousePos(x,y);
	if(x>=0 && x<=this->Width() && y>=0 && y<=this->Height())
		return true;

	if(y>this->Height())
	{
		int aDelta = y-this->Height();
		this->mVertOffset+=aDelta;
	}
	else if(y<0)
	{
		int aDelta = -y;
		this->mVertOffset-=aDelta;
	}

	if(x>this->Width())
		this->mHorzOffset+=(x-this->Width());
	else if(x<0)
		this->mHorzOffset+=x;
		
	this->AdjustPosition(false);
	this->MouseDrag(x,y);

	return true;
}
// FUNCTION: WON 0x00710500
// Replaces the table's contents, detaching the old ones, clearing the selection and recomputing the size
void Table::SetContents(LineSegment *theContents)
{
	this->mViewport->Reset(); // this will end any popup being done by the contents
	if(this->mContents.get()!=NULL)
	{
		LineSegment::mTableContext = this;
		LineSegment::mCounter = 0;
		this->mContents->SetViewport(NULL,0,0);  // this will set the parents to null of any component line segment
	}

	this->mContents = theContents;
	this->mMouseSegment = NULL;
	this->mStartSelSegment = this->mEndSelSegment = -1;
	this->mSelecting = false;
	this->SetMaxWidth();
}
// FUNCTION: WON 0x007105b0
// Lays the contents out to the view width, rebuilds the anchors and total size, and optionally clamps the
// scroll position
void Table::SetMaxWidth(bool adjustPosition)
{
	this->mMouseSegment = NULL;
	this->mAnchorMap.clear();
	if(this->mContents.get()!=NULL)
	{
		this->mContents->SetMaxWidth(this->mScrollArea.width);

		LineSegment::mTableContext = this;
		LineSegment::mCounter = 0;
		this->mContents->SetViewport(this->GetViewport(),0,0);

		this->mTotalWidth = this->mContents->GetWidth();
		this->mTotalHeight = this->mContents->GetHeight();
	}
	else
	{
		this->mTotalWidth = this->mTotalHeight = 0;
	}
	
	if(adjustPosition)
		this->AdjustPosition(false);
}
// FUNCTION: WON 0x00710660
// Lays the contents out again for the new view size, keeping the scroll position
void Table::ScrollSizeChanged()
{
	this->SetMaxWidth(false);
}
// FUNCTION: WON 0x00710670
// Paints the visible part of the contents with the current selection highlighted
void Table::ScrollPaint(Graphics &g)
{
	if(this->mContents.get()==NULL)
		return;

	ElementPaintInfo anInfo;
	anInfo.mClipX = this->mHorzOffset;
	anInfo.mClipY = this->mVertOffset;
	anInfo.mClipWidth = this->mScrollArea.width;
	anInfo.mClipHeight = this->mScrollArea.height;
	anInfo.mStartSelSegment = this->mStartSelSegment;
	anInfo.mEndSelSegment = this->mEndSelSegment;
	anInfo.mStartSelSegmentPos = this->mStartSelSegmentPos;
	anInfo.mEndSelSegmentPos = this->mEndSelSegmentPos;

	this->mContents->Paint(g,0,0,anInfo);
}
// FUNCTION: WON 0x00710700
// Records a named anchor position in the contents (replacing one of the same name)
void Table::AddAnchor(const GUIString &theName, int x, int y)
{
	this->mAnchorMap[theName] = AnchorCoordinate(x,y);
}
// FUNCTION: WON 0x00710730
// Scrolls vertically to a named anchor; returns false when there is no such anchor
bool Table::GotoAnchor(const GUIString &theName)
{
	AnchorMap::iterator anItr = this->mAnchorMap.find(theName);
	if(anItr==this->mAnchorMap.end())
		return false;

//	mHorzOffset = anItr->second.mx;
	this->mVertOffset = anItr->second.my;
	this->AdjustPosition(false);
	return true;
}

// FUNCTION: WON 0x007107b0
// Finds the next or previous occurrence of a string from the current selection, selects it and scrolls it
// into view when it lies outside the view or within 20 pixels of an edge; returns whether it was found
bool Table::FindStr(const GUIString &theStr, bool next, bool caseSensitive)
{
	if(this->mContents.get()==NULL)
		return false;

	ElementFindInfo anInfo;
	ElementFindInfo aFoundInfo;

	anInfo.mFindText = theStr;
	anInfo.mCaseSensitive = caseSensitive;
	anInfo.mFindPrev = !next;

	bool haveSel = this->mStartSelSegment>=0 && (this->mEndSelSegment>this->mStartSelSegment || this->mEndSelSegmentPos>this->mStartSelSegmentPos);
	if(next)
	{
		anInfo.mEndSelSegment = 0x7ffffff;
		anInfo.mEndSelSegmentPos = 0;
		if(!haveSel)
		{
			anInfo.mStartSelSegment = -1;
			anInfo.mStartSelSegmentPos = 0;
		}
		else
		{
			anInfo.mStartSelSegment = this->mStartSelSegment;
			anInfo.mStartSelSegmentPos = this->mStartSelSegmentPos+1;
		}
	}
	else // find Prev
	{
		anInfo.mStartSelSegment = -1;
		anInfo.mStartSelSegmentPos = 0;
		if(!haveSel)
		{
			anInfo.mEndSelSegment = 0x7ffffff;
			anInfo.mEndSelSegmentPos = 0;
		}
		else
		{
			anInfo.mEndSelSegment = this->mEndSelSegment;
			anInfo.mEndSelSegmentPos = this->mEndSelSegmentPos-1;
		}
	}

	anInfo.mCurFoundLength = 0;
	anInfo.mFoundStartSegment = -1;
	anInfo.mFoundX = 0;
	anInfo.mFoundY = 0;

	if(!this->mContents->FindStr(anInfo))
		return false;

	this->mStartSelSegment = anInfo.mFoundStartSegment;
	this->mEndSelSegment = anInfo.mFoundEndSegment;
	this->mStartSelSegmentPos = anInfo.mFoundStartChar;
	this->mEndSelSegmentPos = anInfo.mFoundEndChar;
	int horzOffset = this->mHorzOffset;
	int vertOffset = this->mVertOffset;
	if(anInfo.mFoundX > horzOffset + this->mScrollArea.width-20 || anInfo.mFoundX<horzOffset+20)
		horzOffset = anInfo.mFoundX;
	if(anInfo.mFoundY > vertOffset + this->mScrollArea.height-20 || anInfo.mFoundY<vertOffset+20)
		vertOffset = anInfo.mFoundY;

	this->SetOffsets(horzOffset,vertOffset);
	this->Invalidate();
	return true;
}
// TEMPLATE: WON 0x0071b510
// (public: struct WONAPI::Table::AnchorCoordinate & __thiscall std::map<class WONAPI::GUIString,struct WONAPI::Table::AnchorCoordinate,struct WONAPI::GUI)
// TEMPLATE: WON 0x0071b680
// (public: int __thiscall WONAPI::GUIString::CompareNoCase(class WONAPI::GUIString const &)const     [?CompareNoCase@GUIString@WONAPI@@QBEHABV12@@Z])
// TEMPLATE: WON 0x007206f0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,struct std::m)
// TEMPLATE: WON 0x00720c20
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,struct std)
// TEMPLATE: WON 0x00720f20
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,struct st)
// TEMPLATE: WON 0x00723720
// (public: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,str)
// TEMPLATE: WON 0x00723a40
// (public: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,str)

// Compiler-generated and header-inline functions this unit emits:
}
