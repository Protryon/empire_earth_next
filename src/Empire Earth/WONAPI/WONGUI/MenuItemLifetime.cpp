// Menu item construction, popup menu reset and the text-only AddItem overloads (file name inferred)
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
// Creates an enabled menu item with the given text and listener and menu id 0
MenuItem::MenuItem(const GUIString& text,ComponentListener* listener)
    :TextListItem(text),mListener(listener)
{
    this->mDisabled=false;
    this->mMenuId=0;
}
// Adds a text item with a menu id and no listener (this overload is 0x006fd440; the listener overload
// below is 0x006fd460)
MenuItem* PopupMenu::AddItem(const GUIString& text,int id)
{
    return this->AddItem(text,static_cast<ComponentListener*>(NULL),id);
}
// Adds a text item with a listener and menu id and no icon (0x006fd460)
MenuItem* PopupMenu::AddItem(const GUIString& text,ComponentListener* listener,int id)
{
    return this->AddItem(text,static_cast<NativeImage*>(NULL),listener,id);
}
}
