// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyPopupEvent.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_rust.h"
// The bar's keys, item choice, menu place and flags, presses, events and the items' places and sizes are ee_won's
// guimenu module (rust/crates/ee_won/src/guimenu.rs); this unit keeps the bar, its items, the menus and the paint.
namespace WONAPI {
// The menu bar, its items and the menu-item-selected event (not identified in the original exe)
MenuBarItem::MenuBarItem()
{
	this->mState = MenuBarState_None;
	this->mFont = this->GetNamedFont("Menu");
}

void MenuBarItem::SetMenuBarState(MenuBarState theState)
{
	this->mState = theState;
	this->Invalidate();
}

void MenuBarItem::SetText(const GUIString &theText)
{
	this->mText = theText;
	int aHeight;
	int aWidth = ee_won_gui_menubar_item_size(this->mFont->GetStringWidth(this->mText),this->mFont->GetHeight(),&aHeight);
	this->SetSize(aWidth,aHeight);
}

void MenuBarItem::SetPopup(PopupMenu *thePopup)
{
	this->mPopup = thePopup;	
}

void MenuBarItem::Paint(Graphics &g)
{
	Component::Paint(g);

	bool down = this->mState==MenuBarState_Down;
	bool over = this->mState==MenuBarState_Up;
	int offset = down?1:0;

	g.SetSysColor(StandardColor_3DFace);
	g.FillRect(0,0,this->Width(),this->Height());

	if(down || over)
	{
		g.SetColor(down?g.GetStandardColor(StandardColor_3DShadow):g.GetStandardColor(StandardColor_3DHilight));
		g.DrawLine(0,0,this->Width()-1,0);
		g.DrawLine(0,0,0,this->Height()-1);
	
		g.SetColor(over?g.GetStandardColor(StandardColor_3DShadow):g.GetStandardColor(StandardColor_3DHilight));
		g.DrawLine(0,this->Height()-1,this->Width()-1,this->Height()-1);
		g.DrawLine(this->Width()-1,0,this->Width()-1,this->Height()-1);
	}

	int x = 3;
	int y = (this->Height() - this->mFont->GetHeight())/2;
	g.SetFont(this->mFont);
	g.SetColor(g.GetStandardColor(StandardColor_ButtonText));
	g.DrawString(this->mText,x,y);
}

void MenuItemSelectedEvent::Deliver()
{
	if(this->mItem->mListener.get()!=NULL)
		this->mItem->mListener->HandleComponentEvent(this);
	else
		ComponentEvent::Deliver();
}

MenuBar::MenuBar()
{
	this->mState = MenuBarState_None;
	this->mCurItem = NULL;
	this->mBackground.SetColor(ColorScheme::GetColorRef(StandardColor_3DFace));

	this->SetComponentFlags(ComponentFlag_WantFocus,true);
}

void MenuBar::AddSeperator(Component *theComponent)
{
	SmartPtr<MSSeperator> aSep;
	if(theComponent==NULL)
	{
		aSep = new MSSeperator(2);
		theComponent = aSep;
	}

	this->Fn2F6140(theComponent,0x8001,this,0,0,0,0,0);
	this->AddChild(theComponent);
}

void MenuBar::AddItem(const GUIString &theText, PopupMenu *thePopup)
{
	SmartPtr<MenuBarItem> anItem = new MenuBarItem;
	int anId = this->mItemVector.size();
	MenuBarItem *aPrev = anId > 0 ? this->mItemVector.back() : NULL;
	int aY;
	int aX = ee_won_gui_menubar_item_place(aPrev!=NULL, aPrev!=NULL ? aPrev->Left() : 0, aPrev!=NULL ? aPrev->Top() : 0,
	                                       aPrev!=NULL ? aPrev->Width() : 0, &aY);
	anItem->SetPos(aX,aY);

	anItem->mControlId=anId;
	anItem->SetText(theText);
	if(thePopup==NULL)
		thePopup = new PopupMenu;

	anItem->SetPopup(thePopup);
	if(thePopup!=NULL)
		thePopup->AddListener(new ComponentListenerComp(this));

	this->mItemVector.push_back(anItem);
	this->AddChild(anItem);
}

void MenuBar::CheckItem(Component *theCheck)
{
	MenuBarItem *anItem = dynamic_cast<MenuBarItem*>(theCheck);
	uint8_t aSteps = ee_won_gui_menubar_check(this->mState, this->mCurItem!=NULL, theCheck!=NULL,
	                                          theCheck==this->mCurItem, anItem!=NULL);
	if(this->mState==MenuBarState_None || this->mState==MenuBarState_Up)
	{
		if(aSteps & 1)
			this->mCurItem->SetMenuBarState(MenuBarState_None);

		if(!(aSteps & 2))
			return;

		this->mCurItem = anItem;
		if(this->mCurItem!=NULL)
			this->mCurItem->SetMenuBarState(MenuBarState_Up);
	}
	else
	{
		if(aSteps & 4)
		{
			MenuBarItem *anOldCurItem = this->mCurItem;
			this->mCurItem = anItem;
			if(anOldCurItem!=NULL)
			{
				anOldCurItem->SetMenuBarState(MenuBarState_None);
				if(anOldCurItem->GetPopup()!=NULL)
					this->GetRoot()->ClosePopup(anOldCurItem->GetPopup());
			}

			anItem->SetMenuBarState(MenuBarState_Down);
			PopupMenu *aPopup = this->mCurItem->GetPopup();
			if(aPopup!=NULL)
			{
				int y;
				int x = ee_won_gui_menubar_popup_place(this->mCurItem->Left(), this->mCurItem->Top(),
				                                       this->mCurItem->Height(), &y);
				this->LocalToRoot(x,y);

//				SetComponentFlags(0x8000,true);
				int aFlags = ee_won_gui_menubar_popup_flags();

				aPopup->Reset();
				PopupParams aParams(aPopup,aFlags,this,x,y);
				aParams.mInputComponent = this;
				int anAvoid[4];
				ee_won_gui_menubar_popup_avoid(y, this->mCurItem->Height(), anAvoid);
				aParams.mAvoidRect = WONRectangle(anAvoid[0],anAvoid[1],anAvoid[2],anAvoid[3]);
//				GetRoot()->DoPopup(aParams);
				this->GetRoot()->QueueEvent(new DoPopupEvent(aParams));
			}
		}
	}

}

void MenuBar::HandleComponentEvent(ComponentEvent *theEvent)
{
	uint8_t anEvent = ee_won_gui_menubar_event(theEvent->mType, this->mState);
	if(anEvent==1)
	{
		if(this->mCurItem!=NULL && this->mCurItem->GetPopup()==theEvent->mComponent)
		{
			this->mState = MenuBarState_None;
			if(this->GetRoot()->mMouseIsInClientArea)
				this->MouseAt();
			else
				this->MouseExit();
		}		
	}
	else if(anEvent==2)
	{
		ListItemClickedEvent *aClick = static_cast<ListItemClickedEvent*>(theEvent);
		MenuItem *anItem = dynamic_cast<MenuItem*>(aClick->mItem.get());
		if(ee_won_gui_menu_item_chosen(anItem!=NULL, anItem!=NULL && anItem->mSubMenu.get()!=NULL,
		                               anItem!=NULL && anItem->mDisabled))
		{
			this->DeliverEvent(new MenuItemSelectedEvent(theEvent->mComponent,anItem),false);
			this->GetRoot()->ClosePopup(NULL);
		}
	}
	else
		Container::HandleComponentEvent(theEvent);
}

void MenuBar::MouseMove(int x, int y)
{
	Container::MouseMove(x,y);
	if(!ee_won_gui_menubar_moved(x, y, this->mLastX, this->mLastY))
		return;
	this->mLastX = x;
	this->mLastY = y;
	this->CheckItem(this->mMouseChild);
}

bool MenuBar::KeyDown(int theKey)
{
	if(this->mCurItem==NULL)
		return false;

	if(ee_won_gui_menubar_key_asks_popup(this->mState))
	{
		PopupMenu *aPopup = this->mCurItem->GetPopup();
		if(aPopup->TryKeyDown(theKey))
			return true;
	}
	else
		this->mState = MenuBarState_Up;

	int aState, anId;
	uint8_t aStep = ee_won_gui_menubar_key(this->mState, theKey, this->mCurItem->mControlId, this->mItemVector.size(),
	                                       &aState, &anId);
	this->mState = (MenuBarState)aState;
	if(aStep==1)
	{
		if(this->mCurItem!=NULL)
		{
			this->GetRoot()->ClosePopup(this->mCurItem->GetPopup());
			this->CheckItem(this->mCurItem);
		}
	}
	else if(aStep==2)
		this->mCurItem = NULL;

	this->CheckItem(this->mItemVector[anId]);
	if(this->mState!=MenuBarState_Down || this->mCurItem==NULL)
		return true;
	PopupMenu *aPopup = this->mCurItem->GetPopup();
	if(aPopup!=NULL)
		aPopup->SetSelItem(0);

	return true;
}

void MenuBar::MouseDown(int x, int y, int theButton)
{
	Container::MouseDown(x,y,theButton);
	if(theButton!=0)
		return;

	this->RequestFocus();
	if(dynamic_cast<MenuBarItem*>(this->mMouseChild)==NULL)
		return;
	int aState;
	bool opens = ee_won_gui_menubar_press(this->mState, &aState);
	this->mState = (MenuBarState)aState;
	if(!opens)
	{
		if(this->mCurItem!=NULL)
			this->GetRoot()->ClosePopup(this->mCurItem->GetPopup());
	}
	else
	{
		this->mCurItem = NULL;
		Window *aWindow = this->GetRoot();
		this->LocalToRoot(x,y);
		aWindow->QueueEvent(new MouseEvent(InputEvent_MouseUp,x,y,aWindow->GetKeyMod(),theButton));
	}


	this->CheckItem(this->mMouseChild);
}

void MenuBar::MouseDrag(int x, int y)
{
	if(this->mState==MenuBarState_Down)
	{
		this->UpdateMouseChild(x,y);
		this->CheckItem(this->mMouseChild);
	}
	else
		Container::MouseDrag(x,y);
}

void MenuBar::MouseUp(int x, int y, int theButton)
{
	Container::MouseUp(x,y,theButton);
	if(theButton!=0)
		return;
}

void MenuBar::MouseExit()
{
	Container::MouseExit();
	this->CheckItem(NULL);
}

void MenuBar::Paint(Graphics &g)
{
	if(this->mInvalidState>=2)
		this->mBackground.Paint(g,0,0,this->Width(),this->Height(),0,0);
	
	Container::Paint(g);
}
}
