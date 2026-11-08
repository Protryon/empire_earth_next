// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x00704fd0
// Positions the viewport at the scroll offset, sized to the total content size
void ScrollArea::SetDelegatePos()
{
    this->mViewport->SetPosSize(this->mScrollArea.x-this->mHorzOffset,this->mScrollArea.y-this->mVertOffset,this->mTotalWidth,this->mTotalHeight);
}
// FUNCTION: WON 0x00705030
// Clamps the scroll offsets to the content, tells the scroller, moves the viewport and repaints
// (a scrollbar-driven change marks the user as having scrolled)
void ScrollArea::AdjustPosition(bool scrollbar)
{
    if(scrollbar) this->mUserHasScrolled=true;
    if(this->mVertOffset+this->mScrollArea.height>this->mTotalHeight+this->mExtraHeight)
        this->mVertOffset=this->mTotalHeight+this->mExtraHeight-this->mScrollArea.height;
    if(this->mVertOffset<0) this->mVertOffset=0;
    if(this->mHorzOffset+this->mScrollArea.width>this->mTotalWidth) this->mHorzOffset=this->mTotalWidth-this->mScrollArea.width;
    if(this->mHorzOffset<0) this->mHorzOffset=0;
    if(this->mScroller!=NULL) this->mScroller->NotifyScrollChange(scrollbar);
    this->mAVertOffset=this->mVertOffset;
    this->mAHorzOffset=this->mHorzOffset;
    this->mATotalHeight=this->mTotalHeight;
    this->mATotalWidth=this->mTotalWidth;
    this->mOldScrollAreaWidth=this->mScrollArea.width;
    this->mOldScrollAreaHeight=this->mScrollArea.height;
    this->SetDelegatePos();
    this->Invalidate();
}
// FUNCTION: WON 0x00705130
// Scrolls to a vertical offset
void ScrollArea::SetVertOffset(int offset) { this->mVertOffset=offset; this->AdjustPosition(false); }
// FUNCTION: WON 0x00705150
// Scrolls to a horizontal and vertical offset
void ScrollArea::SetOffsets(int horizontal,int vertical)
{
    this->mHorzOffset=horizontal;
    this->mVertOffset=vertical;
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x00705190
// Returns true when scrolled down (offset > 0) to the end of the content
bool ScrollArea::OnBottom()
{
    return this->mVertOffset>0 && this->mVertOffset+this->mScrollArea.height>=this->mTotalHeight;
}
// FUNCTION: WON 0x007051c0
// Recomputes the visible area inside the padding, keeps the view pinned to the bottom if it was there, re-clamps
void ScrollArea::Virtual08()
{
    Container::Virtual08();
    bool bottom=this->OnBottom();
    this->mScrollArea.x=this->mLeftPad;
    this->mScrollArea.y=this->mTopPad;
    this->mScrollArea.width=this->Width()-this->mRightPad-this->mLeftPad;
    this->mScrollArea.height=this->Height()-this->mBottomPad-this->mTopPad;
    if(this->mScrollArea.Width()<0) this->mScrollArea.width=0;
    if(this->mScrollArea.Height()<0) this->mScrollArea.height=0;
    if(bottom) this->mVertOffset=this->mTotalHeight+this->mExtraHeight-this->mScrollArea.Height();
    this->ScrollSizeChanged();
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x00705270
// Sets the border padding on each side and lays out again when it changed
void ScrollArea::SetBorderPadding(int left,int top,int right,int bottom)
{
    if(this->mLeftPad==left && this->mRightPad==right && this->mTopPad==top && this->mBottomPad==bottom) return;
    this->mLeftPad=left;this->mTopPad=top;this->mRightPad=right;this->mBottomPad=bottom;
    this->Virtual08();
}
// FUNCTION: WON 0x00705390
// Converts a point from scroll area coordinates to content coordinates
void ScrollArea::ScrollTranslate(int& x,int& y)
{
    x+=this->mHorzOffset-this->mScrollArea.x;
    y+=this->mVertOffset-this->mScrollArea.y;
}
// FUNCTION: WON 0x007053d0
// Converts a point from content coordinates to scroll area coordinates
void ScrollArea::ScrollUntranslate(int& x,int& y)
{
    x+=this->mScrollArea.x-this->mHorzOffset;
    y+=this->mScrollArea.y-this->mVertOffset;
}
// FUNCTION: WON 0x00705410
// Scrolls up by the wheel amount unless a child handled the wheel; always handled
bool ScrollArea::MouseWheel(int amount)
{
    if(Container::MouseWheel(amount)) return true;
    this->mUserHasScrolled=true;
    this->mVertOffset-=amount;
    this->AdjustPosition(false);
    return true;
}
// FUNCTION: WON 0x00705450
// Resizes the scroll area to fit the whole content (at least the given minimum) plus padding
void ScrollArea::SetSizeEqualToTotal(int minWidth,int minHeight)
{
    int width=this->mTotalWidth,height=this->mTotalHeight;
    if(minWidth>width) width=minWidth;
    if(minHeight>height) height=minHeight;
    this->SetSize(width+this->mLeftPad+this->mRightPad,height+this->mTopPad+this->mBottomPad);
}
// FUNCTION: WON 0x007054a0
// Returns the content size, capped at 300x300
void ScrollArea::GetDesiredSize(int& width,int& height)
{
    if(this->mTotalWidth<300) width=this->mTotalWidth;
    else width=300;
    if(this->mTotalHeight<300) height=this->mTotalHeight;
    else height=300;
}
// WONRectangle::Intersects expands inline here (out-of-line copy at 0x00749450); its edge tests are asymmetric.
// FUNCTION: WON 0x007054f0
// Returns true when a content rectangle overlaps the currently visible part of the content
bool ScrollArea::IsRectVisible(const WONRectangle& r)
{
    WONRectangle view=this->mScrollArea;
    view.x=this->mHorzOffset;
    view.y=this->mVertOffset;
    return r.Intersects(view);
}
}
namespace WONAPI {
// FUNCTION: WON 0x007052d0
// Paints the background at the scroll offset, then the children and the scrolled content clipped to the view
void ScrollArea::Paint(Graphics &g)
{
	this->mBackground.Paint(g,0,0,this->Width(),this->Height(),this->mHorzOffset,this->mVertOffset);
	g.PushClipRect(this->mScrollArea.x,this->mScrollArea.y,this->mScrollArea.width,this->mScrollArea.height);

	Container::Paint(g);

	int dx = 0, dy = 0;
	this->ScrollTranslate(dx,dy);
	g.Translate(-dx,-dy);

	this->ScrollPaint(g);

	g.Translate(dx,dy);

	g.PopClipRect();
}
}

namespace WONAPI {
// FUNCTION: WON 0x00704df0
// Creates an empty scroll area with its viewport child, line scroll steps of 10 and no padding or scroller
ScrollArea::ScrollArea()
{
    this->mViewport=new ScrollAreaViewport(this);
    this->AddChild(this->mViewport.get());
    this->mUpScrollAmount=this->mDownScrollAmount=10;
    this->mLeftScrollAmount=this->mRightScrollAmount=10;
    this->mVertOffset=this->mHorzOffset=this->mTotalHeight=this->mTotalWidth=0;
    this->mAVertOffset=this->mAHorzOffset=this->mATotalHeight=this->mATotalWidth=0;
    this->mExtraHeight=0;
    this->mOldScrollAreaWidth=this->mOldScrollAreaHeight=0;
    this->mLeftPad=this->mRightPad=this->mTopPad=this->mBottomPad=0;
    this->mScroller=NULL;
    this->mUserHasScrolled=false;
}
}

namespace WONAPI {
// Paints no scrolled content by default (the linker folds this empty body into the identical one at 0x006fa500)
void ScrollArea::ScrollPaint(Graphics&) {}
}

namespace WONAPI {
// Sets the content size and re-clamps the scroll position (not identified in the original exe)
void ScrollArea::SetScrollDimension(int theWidth, int theHeight)
{
	this->mTotalWidth = theWidth;
	this->mTotalHeight = theHeight;
	this->AdjustPosition(false);
}
}

namespace WONAPI {
// Attach a scrollbar or scroll button, creating the matching scroller on first use (not identified in the original exe)
void ScrollArea::SetScrollbar(Scrollbar *theScrollbar)
{
	ScrollbarScroller *aScroller = dynamic_cast<ScrollbarScroller*>(this->mScroller);
	if(aScroller==NULL)
	{
		aScroller = new ScrollbarScroller;
		this->SetScroller(aScroller);
	}

	aScroller->SetScrollbar(theScrollbar);
}

void ScrollArea::SetScrollButton(Button *theButton, Direction theDirection)
{
	ButtonScroller *aScroller = dynamic_cast<ButtonScroller*>(this->mScroller);
	if(aScroller==NULL)
	{
		aScroller = new ButtonScroller;
		this->SetScroller(aScroller);
	}

	aScroller->SetButton(theButton, theDirection);
}
}

namespace WONAPI {
// Hook called when the visible area is resized; empty by default (not identified in the original exe)
void ScrollArea::ScrollSizeChanged()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00704f80
// (public: virtual void * __thiscall WONAPI::ScrollArea::`scalar deleting destructor'(unsigned int)    [??_GScrollArea@WONAPI@@UAEPAXI@Z])
