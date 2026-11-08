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
	this->SetSize(this->mFont->GetStringWidth(this->mText)+10,this->mFont->GetHeight()+4);
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
	if(anId > 0)
	{
		MenuBarItem *aPrev = this->mItemVector.back();
		anItem->SetPos((aPrev->Left()+aPrev->Width())+1,aPrev->Top());
	}
	else
		anItem->SetPos(2,2);

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
	if(this->mState==MenuBarState_None || this->mState==MenuBarState_Up)
	{
		if(this->mCurItem!=NULL && theCheck!=this->mCurItem)
			this->mCurItem->SetMenuBarState(MenuBarState_None);

		if(theCheck==NULL)
			return;

		this->mCurItem = dynamic_cast<MenuBarItem*>(theCheck);
		if(this->mCurItem!=NULL)
			this->mCurItem->SetMenuBarState(MenuBarState_Up);
	}
	else if(this->mState==MenuBarState_Down)
	{
		MenuBarItem *anItem = dynamic_cast<MenuBarItem*>(theCheck);
		if(anItem!=NULL && anItem!=this->mCurItem)
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
				int x = this->mCurItem->Left();
				int y = (this->mCurItem->Top()+this->mCurItem->Height());
				this->LocalToRoot(x,y);

//				SetComponentFlags(0x8000,true);
				int aFlags = PopupFlag_StandardPopup;
				aFlags &= ~PopupFlag_WantInput;
				aFlags &= ~PopupFlag_EndOnEscape;

				aPopup->Reset();
				PopupParams aParams(aPopup,aFlags,this,x,y);
				aParams.mInputComponent = this;
				aParams.mAvoidRect = WONRectangle(0,y-this->mCurItem->Height(),0,this->mCurItem->Height());
//				GetRoot()->DoPopup(aParams);
				this->GetRoot()->QueueEvent(new DoPopupEvent(aParams));
			}
		}
	}

}

void MenuBar::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(theEvent->mType==ComponentEvent_EndPopup && this->mState==MenuBarState_Down)
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
	else if(theEvent->mType==ComponentEvent_ListItemUpClicked)
	{
		ListItemClickedEvent *anEvent = static_cast<ListItemClickedEvent*>(theEvent);
		MenuItem *anItem = dynamic_cast<MenuItem*>(anEvent->mItem.get());
		if(anItem!=NULL && anItem->mSubMenu.get()==NULL && !anItem->mDisabled)
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
	if(x==this->mLastX && y==this->mLastY)
		return;
	this->mLastX = x;
	this->mLastY = y;
	this->CheckItem(this->mMouseChild);
}

bool MenuBar::KeyDown(int theKey)
{
	if(this->mCurItem==NULL)
		return false;

	if(this->mState==MenuBarState_Down)
	{
		PopupMenu *aPopup = this->mCurItem->GetPopup();
		if(aPopup->TryKeyDown(theKey))
			return true;
	}
	else 
		this->mState = MenuBarState_Up;

	int anId = this->mCurItem->mControlId;

	if(theKey==VK_ESCAPE)
	{
		this->mState = MenuBarState_Up;
		if(this->mCurItem!=NULL)
		{
			this->GetRoot()->ClosePopup(this->mCurItem->GetPopup());
			this->CheckItem(this->mCurItem);
		}
	}
	else if(theKey==VK_RIGHT)
	{
		anId++;
		if(anId>=this->mItemVector.size())
			anId = 0;
	}
	else if(theKey==VK_LEFT)
	{
		anId--;
		if(anId<0)
			anId = this->mItemVector.size()-1;
	}
	else if(this->mState==MenuBarState_Up && (theKey==VK_RETURN || theKey==VK_DOWN))
	{
		this->mCurItem = NULL;
		this->mState = MenuBarState_Down;
	}

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
	if(this->mState==MenuBarState_Down)
	{
		this->mState = MenuBarState_Up;
		if(this->mCurItem!=NULL)
			this->GetRoot()->ClosePopup(this->mCurItem->GetPopup());
	}
	else
	{
		this->mCurItem = NULL;
		this->mState = MenuBarState_Down;
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
