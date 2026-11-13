// Menu item measure and drawing (file name inferred; the popup menu's input is the Rust PopupMenu, menuobj.rs)
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
#include "rust/ee_won.h"
// The submenu delay, the swallowed first release, the keyboard mode's end, an item's size and the submenu's place
// are ee_won's guiwin module (rust/crates/ee_won/src/guiwin.rs); the keys and the submenus' open, close and
// selection rules are its guimenu module (guimenu.rs); this unit keeps the menus, submenus and the paint.
#include "ee/wonapi/WONLobbyPopupEvent.h"

namespace WONAPI {
// FUNCTION: WON 0x006fcee0
// Measures the menu item as text plus room for the icon and submenu arrow (32 wider, 4 taller)
void MenuItem::CalcDimensions(ListArea* area)
{
    TextListItem::CalcDimensions(area);
    ee_won_gui_menu_item_size(&this->mWidth,&this->mHeight);
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
        int width=area->GetPaintWidth();
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

}

