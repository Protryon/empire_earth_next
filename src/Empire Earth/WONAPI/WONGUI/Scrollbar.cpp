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
#include "rust/ee_won.h"
// The thumb's layout and drag and the line and page steps are ee_won's guiscroll module
// (rust/crates/ee_won/src/guiscroll.rs); this unit keeps the controls and their SetPos / SetSize / SetVisible calls.
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
	EeWonThumbIn anIn;
	anIn.page = this->mPageSize;
	anIn.total = this->mTotalSize;
	anIn.position = this->mPosition;
	anIn.track_top = this->S_Top(this->mScrollRect.get());
	anIn.track_len = this->S_Height(this->mScrollRect.get());
	anIn.thumb_top = this->S_Top(this->mThumb.get());
	anIn.thumb_len = this->S_Height(this->mThumb.get());
	anIn.resize_thumb = this->mResizeThumb;
	anIn.min_thumb = this->mMinThumbSize;
	anIn.change_pos = changePos;
	EeWonThumbOut anOut = ee_won_gui_thumb_layout(anIn);
	if(anOut.hidden)
	{
		this->mThumb->SetVisible(false);
		return;
	}

	this->mThumb->SetVisible(!this->IsDisabled());

	int aThumbWidth = this->S_Width(this->mThumb.get());
	if(aThumbWidth==0)
		aThumbWidth = this->S_Width(this);

	if(anOut.set_pos)
		this->S_SetPosSize(this->mThumb.get(),this->S_Left(this->mThumb.get()),anOut.pos,aThumbWidth,anOut.len);
	else
		this->S_SetSize(this->mThumb.get(),aThumbWidth,anOut.len);
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

	int oldPosition = this->mPosition;
	int aTop = 0;
	this->mPosition = ee_won_gui_thumb_drag(aNewY,this->S_Top(this->mScrollRect.get()),
		this->S_Height(this->mScrollRect.get()),this->S_Height(this->mThumb.get()),this->mTotalSize,this->mPageSize,&aTop);
	aNewY = aTop;

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
	EeWonScrollPress aPress;
	int aMouseY = 0;
	if(theEvent->mComponent==this->mUpArrow.get())
		aPress = EeWonScrollPress::LineUp;
	else if(theEvent->mComponent==this->mDownArrow.get())
		aPress = EeWonScrollPress::LineDown;
	else if(theEvent->mComponent==this->mScrollRect.get())
	{
		int aMouseX;
		this->GetMousePos(aMouseX, aMouseY);
		if(!this->mVertical)
			aMouseY = aMouseX;
		aPress = EeWonScrollPress::Track;
	}
	else
	{
		Container::HandleComponentEvent(theEvent);
		return;
	}

	this->mPosition = ee_won_gui_scroll_step(aPress,aMouseY,this->S_Top(this->mThumb.get()),
		this->S_Height(this->mThumb.get()),this->mPosition,this->mLineUpScrollSize,this->mLineDownScrollSize,
		this->mPageScrollSize,this->mTotalSize,this->mPageSize);

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
