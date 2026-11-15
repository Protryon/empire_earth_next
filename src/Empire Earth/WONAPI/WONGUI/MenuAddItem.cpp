// Adding a menu item with an icon to a popup menu (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x006fd480
// Adds a menu item with text, icon, listener and menu id, resizes the menu to fit and returns the item
MenuItem* PopupMenu::AddItem(const GUIString& text,NativeImage* icon,ComponentListener* listener,int id)
{
    SmartPtr<MenuItem> item=new MenuItem(text,listener);
    item->mMenuId=id;
    item->mImage=icon;
    this->InsertItem(item.get());
    this->SetSizeEqualToTotal(-1,-1);
    return item.get();
}
}
