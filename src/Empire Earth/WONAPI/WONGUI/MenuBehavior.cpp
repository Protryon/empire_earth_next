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
#include "ee/wonapi/WONGUIDraw.h"
// The submenu delay, the swallowed first release, the keyboard mode's end, an item's size and the submenu's place
// are ee_won's guiwin module (rust/crates/ee_won/src/guiwin.rs); the keys and the submenus' open, close and
// selection rules are its guimenu module (guimenu.rs); this unit keeps the menus and submenus; the item's paint is guidraw's menu_item.
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
    // guidraw.rs menu_item
    Image* anImage=this->mImage.get();
    EeWonMenuItemPaint r;
    r.selected=this->mSelected;
    r.full_row_select=area->ListFlagSet(0x10);
    r.has_image=anImage!=NULL;
    r.disabled=this->mDisabled;
    r.has_submenu=this->mSubMenu.get()!=NULL;
    r.sel=WONDrawSelOf(area->mSelColor);
    r.color=(uint32_t)this->mColor;
    r.paint_width=area->GetPaintWidth();
    r.width=this->mWidth;
    r.height=this->mHeight;
    r.image_height=anImage ? anImage->mHeight : 0;
    r.font_height=this->mFont->GetHeight();
    r.view_width=area->mScrollArea.width;
    WONDrawSlots aSlots;
    aSlots.mImages[0]=anImage;
    aSlots.mFont=this->mFont.get();
    aSlots.mString=&this->mText;
    WONDrawOps anOps([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_menu_item(&r,o,n); });
    WONDrawRun(g,anOps,aSlots);
}

}

