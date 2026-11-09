// Menu item drawing and popup menu input: submenus, hover timer, mouse and keys (file name inferred)
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
#include "ee/wonapi/WONLobbyPopupEvent.h"

namespace WONAPI {
// FUNCTION: WON 0x006fcee0
// Measures the menu item as text plus room for the icon and submenu arrow (32 wider, 4 taller)
void MenuItem::CalcDimensions(ListArea* area)
{
    TextListItem::CalcDimensions(area);
    this->mHeight+=4;
    this->mWidth+=32;
}

// FUNCTION: WON 0x006fcf10
// Draws the menu item: icon, selection highlight with an icon box, text (embossed grey when disabled)
// and a submenu arrow; the box's bottom edge runs to x = height-1, not to the box width
void MenuItem::Paint(Graphics& g,ListArea* area)
{
    int color=this->mColor;
    if(this->mImage.get()!=NULL)
    {
        int y=(this->mHeight-this->mImage->mHeight)/2;
        if(y<0) y=0;
        g.DrawImage(this->mImage.get(),2,y);
    }
    if(this->mSelected)
    {
        g.SetColor(area->mSelColor.GetBackgroundColor(g));
        int width=area->mPaintWidth;
        if(!area->ListFlagSet(0x10)) width=this->mWidth;
        int x=0;
        if(this->mImage.get()!=NULL) x=20;
        g.FillRect(x,0,width,this->mHeight);
        if(this->mImage.get()!=NULL)
        {
            g.SetSysColor(StandardColor_3DHilight);
            int boxHeight=this->mHeight;
            g.DrawRect(0,0,20,boxHeight);
            g.SetSysColor(StandardColor_3DShadow);
            g.DrawLine(19,0,19,boxHeight-1);
            g.DrawLine(0,boxHeight-1,boxHeight-1,boxHeight-1);
        }
        color=area->mSelColor.GetForegroundColor(g,this->mColor);
    }
    g.SetColor(color);
    g.SetFont(this->mFont.get());
    int y=(this->mHeight-this->mFont->GetHeight())/2;
    if(this->mDisabled)
    {
        g.SetColor(0xffffff);
        if(!this->mSelected)
        {
            g.DrawString(this->mText,23,y+1);
            g.SetColor(0x848484);
        }
        else g.SetColor(0xd0d0d0);
        g.DrawString(this->mText,22,y);
    }
    else g.DrawString(this->mText,22,y);
    if(this->mSubMenu.get()!=NULL)
    {
        int width=area->mScrollArea.width;
        int height=this->mHeight;
        bool disabled=this->mDisabled && !this->mSelected;
        int x=width-8,y=height/2;
        for(int i=0;i<4;++i)
        {
            g.DrawLine(x+3-i,y-i,x+3-i,y+i);
            if(disabled && i<3)
            {
                g.SetColor(0xffffff);
                g.DrawLine(x+3-i,y+i+1,x+3-i,y+i+2);
                g.SetColor(0x848484);
            }
        }
        if(disabled) g.SetPixel(x+4,y+1,0xffffff);
    }
}

// FUNCTION: WON 0x006fd580
// Closes the open submenu and forgets it, returning to hover selection unless driven by keys
void PopupMenu::EndSubMenu()
{
    if(this->mCurSubMenu.get()!=NULL)
    {
        this->GetRoot()->ClosePopup(this->mCurSubMenu.get());
        if(!this->mKeyboardMode) this->SetListFlags(4,true);
        this->mCurSubMenu=(PopupMenu*)NULL;
        this->mCurSubMenuItem=(ListItem*)NULL;
    }
}

// FUNCTION: WON 0x006fd7d0
// Keeps the submenu's parent item selected while the submenu has a selection; other events go to
// the scroll area
void PopupMenu::HandleComponentEvent(ComponentEvent* event)
{
    if(event->mComponent==this->mCurSubMenu.get() && event->mType==ComponentEvent_ListSelChanged)
    {
        if(this->mCurSubMenu->mSelItem!=NULL)
            this->SetSelItem(this->GetItemPos(this->mCurSubMenuItem.get()));
    }
    else ScrollArea::HandleComponentEvent(event);
}

// FUNCTION: WON 0x006fd820
// Opens or closes the submenu of the hovered item after 350 ms; returns whether to keep the timer
bool PopupMenu::TimerEvent(int delta)
{
    if(this->mKeyboardMode) return false;
    this->mTimerAcc+=delta;
    if(this->mTimerAcc>=350)
    {
        this->CheckSubMenu();
        return false;
    }
    return true;
}

// FUNCTION: WON 0x006fd860
// Restarts the submenu timer on a mouse selection change, clearing the open submenu's selection when
// another item is hovered
void PopupMenu::ListSelChangedHook()
{
    this->mTimerAcc=0;
    if(!this->mKeyboardMode)
    {
        if(this->mCurSubMenu.get()!=NULL && this->mSelItem!=this->mCurSubMenuItem.get())
            this->mCurSubMenu->SetSelItem(-1);
        this->RequestTimer(true);
    }
}

// FUNCTION: WON 0x006fdb10
// Leaves keyboard mode once the mouse really moves, then tracks the item under it
void PopupMenu::MouseMove(int x,int y)
{
    if(this->mKeyboardMode)
    {
        if(x==this->mLastX && y==this->mLastY) return;
        this->mKeyboardMode=false;
        this->SetListFlags(4,this->mCurSubMenu.get()==NULL);
    }
    this->mLastX=x;
    this->mLastY=y;
    ListArea::MouseMove(x,y);
}

// FUNCTION: WON 0x006fdc10
// Ignores the first left-button release after the menu opens; others go to the list box
void PopupMenu::MouseUp(int x,int y,int button)
{
    if(button==0)
    {
        ++this->mMouseUpCount;
        if(this->mMouseUpCount<2) return;
    }
    ListArea::MouseUp(x,y,button);
}
// FUNCTION: WON 0x006fd610
// Opens the submenu of the selected enabled item beside it (closing any other), or closes the open
// one when the selection has none
void PopupMenu::CheckSubMenu()
{
    this->mTimerAcc=0;
    MenuItem* item=dynamic_cast<MenuItem*>(this->mSelItem);
    if(item==NULL)
    {
        if(this->mCurSubMenu.get()!=NULL) this->EndSubMenu();
        return;
    }
    PopupMenu* submenu=item->mSubMenu.get();
    if(this->mCurSubMenu.get()==submenu) return;
    this->EndSubMenu();
    if(item->mSubMenu.get()!=NULL && !item->mDisabled)
    {
        this->SetListFlags(4,false);
        this->mCurSubMenu=submenu;
        this->mCurSubMenuItem=item;
        int x=this->mScrollArea.width-2,y=item->mTop;
        this->ScrollUntranslate(x,y);
        this->LocalToRoot(x,y);
        submenu->Reset();
        PopupParams params(submenu,0x33c7,this,x,y);
        params.mAvoidRect=WONRectangle(x-this->mScrollArea.width+2,0,this->mScrollArea.width-2,0);
        this->GetRoot()->QueueEvent(new DoPopupEvent(params));
    }
}

// FUNCTION: WON 0x006fdb70
// Turns a left-button press into a queued release at the same spot (so a click picks an item); other
// buttons go to the list box
void PopupMenu::MouseDown(int x,int y,int button)
{
    if(button==0)
    {
        this->mMouseUpCount=0;
        Window* window=this->GetRoot();
        this->LocalToRoot(x,y);
        window->QueueEvent(new MouseEvent(InputEvent_MouseUp,x,y,window->GetKeyMod(),button));
        return;
    }
    ListArea::MouseDown(x,y,button);
}

}

namespace WONAPI {
// Handles a key in the menu or its open submenu: arrows move and open or close submenus, Esc closes
// the submenu; true when used (not identified in the original exe)
bool PopupMenu::TryKeyDown(int theKey)
{
	this->mKeyboardMode = true;
	if(this->mCurSubMenu.get()!=NULL)
	{
		if(this->mCurSubMenu->TryKeyDown(theKey))
			return true;
		else if(theKey==VK_LEFT || theKey==VK_ESCAPE)
		{
			this->EndSubMenu();
			return true;
		}
		else if(theKey==VK_RIGHT && this->mCurSubMenu->GetSelItem()==NULL)
		{
			this->mCurSubMenu->SetSelItem(0);
			return true;
		}
		else
			return false;
	}
	else if(theKey==VK_RIGHT)
	{
		this->CheckSubMenu();
		if(this->mCurSubMenu.get()!=NULL)
		{
			this->mCurSubMenu->SetSelItem(0);
			return true;
		}
		else
			return false;
	}
	else if(theKey==VK_UP || theKey==VK_DOWN)
	{
		ListArea::KeyDown(theKey);
		return true;
	}
	else
		return false;
}
}
