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
// FUNCTION: WON 0x00705fa0
// Copies line scroll steps, thumb resizing and minimum thumb size from another scrollbar
void Scrollbar::CopyAttributes(Scrollbar *theCopyFrom)
{
	this->mLineUpScrollSize =  theCopyFrom->mLineUpScrollSize;
	this->mLineDownScrollSize =  theCopyFrom->mLineDownScrollSize;
	this->mResizeThumb = theCopyFrom->mResizeThumb;
	this->mMinThumbSize = theCopyFrom->mMinThumbSize;
}

// FUNCTION: WON 0x00705fe0
// Adds the arrow, thumb and track controls the first time the scrollbar gets a parent
void Scrollbar::AddedToParent()
{
	if(this->mUpArrow->mParent!=this)
		this->AddControls();

	Container::AddedToParent();
}

// FUNCTION: WON 0x00706000
// Creates the track button if missing, sets button flag 2 on both arrows and the track, and adds all four controls
void Scrollbar::AddControls()
{
	if(this->mScrollRect.get()==NULL)
		this->mScrollRect = new Button;

	this->mUpArrow->SetButtonFlags(2,true);
	this->mDownArrow->SetButtonFlags(2,true);	
	this->mScrollRect->SetButtonFlags(2,true);

	this->AddChild(this->mScrollRect.get());
	this->AddChild(this->mThumb.get());
	this->AddChild(this->mUpArrow.get());
	this->AddChild(this->mDownArrow.get());
}

// FUNCTION: WON 0x00706100
// Positions a control along/across the scrollbar (x and y swapped when horizontal)
void Scrollbar::S_SetPos(Component *theComponent, int x, int y)
{
	if(this->mVertical)
		theComponent->SetPos(x,y);
	else
		theComponent->SetPos(y,x);
}

// FUNCTION: WON 0x00706140
// Sizes a control across/along the scrollbar (width and height swapped when horizontal)
void Scrollbar::S_SetSize(Component *theComponent, int width, int height)
{
	if(this->mVertical)
		theComponent->SetSize(width,height);
	else
		theComponent->SetSize(height,width);
}

// FUNCTION: WON 0x00706180
// Positions and sizes a control in scrollbar orientation (swapped when horizontal)
void Scrollbar::S_SetPosSize(Component *theComponent, int x, int y, int width, int height)
{
	if(this->mVertical)
		theComponent->SetPosSize(x,y,width,height);
	else
		theComponent->SetPosSize(y,x,height,width);
}

// FUNCTION: WON 0x007061d0
// Sizes and places the thumb for the page and total size (hidden when everything fits), kept inside the track
void Scrollbar::LayoutThumb(bool changePos)
{
	if(this->mPageSize >= this->mTotalSize)
	{
		this->mThumb->SetVisible(false);
		return;
	}

	this->mThumb->SetVisible(!this->IsDisabled());

	int aScrollHeight = this->S_Height(this->mScrollRect.get());
	int aThumbSize = this->S_Height(this->mThumb.get());
	if(this->mResizeThumb)
	{
		aThumbSize = aScrollHeight*this->mPageSize/this->mTotalSize;
		if(aThumbSize<this->mMinThumbSize)
			aThumbSize = this->mMinThumbSize;
	}

	int aThumbPos = this->S_Top(this->mThumb.get());
	if(changePos)
		aThumbPos = this->S_Top(this->mScrollRect.get())+(aScrollHeight-aThumbSize)*this->mPosition/(this->mTotalSize-this->mPageSize);

	if(aThumbPos<this->S_Top(this->mScrollRect.get()))
	{
		if(this->mVertical) aThumbPos = this->mScrollRect->Top(); else aThumbPos = this->mScrollRect->Left();
		changePos = true;
	}
	if(aThumbPos+aThumbSize >= this->S_Bottom(this->mScrollRect.get()))
	{
		aThumbPos = this->S_Bottom(this->mScrollRect.get())-aThumbSize;
		changePos = true;
	}

	int aThumbWidth = this->S_Width(this->mThumb.get());
	if(aThumbWidth==0)
		aThumbWidth = this->S_Width(this);

	if(changePos)
		this->S_SetPosSize(this->mThumb.get(),this->S_Left(this->mThumb.get()),aThumbPos,aThumbWidth,aThumbSize);
	else
		this->S_SetSize(this->mThumb.get(),aThumbWidth,aThumbSize);
}

// FUNCTION: WON 0x00706380
// Lays out the scrollbar: square arrows when unsized, down arrow at the end, track between, then the thumb
void Scrollbar::Virtual08()
{
	Container::Virtual08();

	if(this->S_Height(this->mUpArrow.get())==0)
		this->S_SetSize(this->mUpArrow.get(), this->S_Width(this),this->S_Width(this));
	if(this->S_Height(this->mDownArrow.get())==0)
		this->S_SetSize(this->mDownArrow.get(),this->S_Width(this),this->S_Width(this));

	this->S_SetPos(this->mDownArrow.get(),0,this->S_Height(this)-this->S_Height(this->mDownArrow.get()));
	this->S_SetPosSize(this->mScrollRect.get(),0,this->S_Bottom(this->mUpArrow.get()),this->S_Width(this),this->S_Height(this)-this->S_Height(this->mUpArrow.get())-this->S_Height(this->mDownArrow.get()));
	this->LayoutThumb(false);
}

// FUNCTION: WON 0x007064c0
// On a left press on the thumb, remembers where in the thumb it was grabbed
void Scrollbar::MouseDown(int x, int y, int theButton)
{
	Container::MouseDown(x,y,theButton);
	if(theButton==0 && this->mMouseChild==this->mThumb.get())
	{
		this->mDragX = x-this->mThumb->Left();
		this->mDragY = y-this->mThumb->Top();
	}
}

// FUNCTION: WON 0x00706510
// Drags the thumb inside the track, updates the position and fires a scroll event when it changed
void Scrollbar::MouseDrag(int x, int y)
{
	Container::MouseDrag(x,y);
	if(this->mMouseChild!=this->mThumb.get())
		return;
	int dx = (x-this->mThumb->Left())-this->mDragX;
	int dy = (y-this->mThumb->Top())-this->mDragY;

	int aNewY = this->mVertical? (this->mThumb->Top() + dy) : (this->mThumb->Left() + dx);

	if(aNewY<this->S_Top(this->mScrollRect.get()))
		if(this->mVertical) aNewY = this->mScrollRect->Top(); else aNewY = this->mScrollRect->Left();
	if(aNewY+this->S_Height(this->mThumb.get()) > this->S_Bottom(this->mScrollRect.get()))
		aNewY = this->S_Bottom(this->mScrollRect.get())-this->S_Height(this->mThumb.get());

	int oldPosition = this->mPosition;
	int aHeightDiff = this->S_Height(this->mScrollRect.get()) - this->S_Height(this->mThumb.get());
	if(aHeightDiff<=0)
		this->mPosition = 0;
	else
		this->mPosition = (this->mTotalSize-this->mPageSize)*(aNewY-this->S_Top(this->mScrollRect.get()))/aHeightDiff;

	if(aNewY!=this->S_Top(this->mThumb.get()))
	{
		this->S_SetPos(this->mThumb.get(),this->S_Left(this->mThumb.get()),aNewY);
		this->mScrollRect->Invalidate();
	}
	if(this->mPosition!=oldPosition)
	{
		this->Fn2F5380(ComponentEvent_Scroll,0);
	}

}

// FUNCTION: WON 0x00706690
// Scrolls by a line for the arrows or a page for a track click, clamps, then relayouts and fires a scroll event
void Scrollbar::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(theEvent->mType!=ComponentEvent_ButtonPressed)
	{
		Container::HandleComponentEvent(theEvent);
		return;
	}

	int anOldPosition = this->mPosition;
	if(theEvent->mComponent==this->mUpArrow.get())
		this->mPosition-=this->mLineUpScrollSize;
	else if(theEvent->mComponent==this->mDownArrow.get())
		this->mPosition+=this->mLineDownScrollSize;
	else if(theEvent->mComponent==this->mScrollRect.get())
	{
		int aMouseX, aMouseY;
		this->GetMousePos(aMouseX, aMouseY);
		if(!this->mVertical)
			aMouseY = aMouseX;

		if(aMouseY < this->S_Top(this->mThumb.get()))
			this->mPosition-=this->mPageScrollSize;
		else if(aMouseY >= this->S_Bottom(this->mThumb.get()))
			this->mPosition+=this->mPageScrollSize;
	}
	else
	{
		Container::HandleComponentEvent(theEvent);
		return;
	}


	int aMaxPos = this->mTotalSize - this->mPageSize;
	if(this->mPosition>aMaxPos)
		this->mPosition = aMaxPos;
	if(this->mPosition<0)
		this->mPosition = 0;

	if(this->mPosition==anOldPosition)
		return;
	this->LayoutThumb(true);
	this->mScrollRect->Invalidate();
	this->Fn2F5380(ComponentEvent_Scroll,0);
}

// FUNCTION: WON 0x007067e0
// Enables or disables the scrollbar and its arrows; the thumb shows only when enabled and content overflows
void Scrollbar::SetEnabled(bool isEnabled)
{
	if(isEnabled==!this->IsDisabled())
		return;

	Component::SetEnabled(isEnabled);
	this->mUpArrow->SetEnabled(isEnabled);
	this->mDownArrow->SetEnabled(isEnabled);
	if(this->mTotalSize > this->mPageSize)
		this->mThumb->SetVisible(isEnabled);
}

}
