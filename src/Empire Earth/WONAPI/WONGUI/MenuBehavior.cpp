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
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guiwin.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "rust/won_gui/leafpaint.h"
// The submenu delay, the swallowed first release, the keyboard mode's end, an item's size and the submenu's place
// are ee_won's guiwin module (rust/crates/ee_won_gui/src/guiwin.rs); the keys and the submenus' open, close and
// selection rules are its guimenu module (guimenu.rs); this unit keeps the menus and submenus; the item's paint is guidraw's menu_item.
#include "ee/wonapi/WONLobbyPopupEvent.h"

namespace WONAPI {
// the menu item's own reads for leafpaint.rs (the text item's are TextListItem.cpp's)
extern "C" intptr_t ee_won_guicpp_textitem_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b);
static intptr_t MenuItemCall(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    MenuItem* t = (MenuItem*)static_cast<TextListItem*>(inMe);
    switch (op) {
    case EeWonLeafOp::MenuDisabled: return t->mDisabled ? 1 : 0;
    case EeWonLeafOp::MenuHasSubMenu: return t->mSubMenu.get() != NULL ? 1 : 0;
    case EeWonLeafOp::MenuListSel: *(EeWonSelColor*)b = WONDrawSelOf(((ListArea*)a)->mSelColor); return 0;
    case EeWonLeafOp::MenuListViewWidth: return ((ListArea*)a)->GetViewWidth();
    default: return ee_won_guicpp_textitem_call(inMe, op, a, b);
    }
}
// Measures the menu item as text plus room for the icon and submenu arrow (32 wider, 4 taller)
void MenuItem::CalcDimensions(ListArea* area) { ee_won_leaf_call(static_cast<TextListItem*>(this), MenuItemCall, EeWonLeafCall::MenuItemMeasure, (intptr_t)area, 0); }
// Draws the menu item: icon, selection highlight with an icon box, text (embossed grey when disabled)
// and a submenu arrow; the box's bottom edge runs to x = height-1, not to the box width
void MenuItem::Paint(Graphics& g,ListArea* area)
{
    ee_won_leaf_call(static_cast<TextListItem*>(this), MenuItemCall, EeWonLeafCall::MenuItemPaint, (intptr_t)&g, (intptr_t)area);
}
}
