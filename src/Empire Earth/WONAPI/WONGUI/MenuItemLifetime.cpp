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
// FUNCTION: WON 0x006fcd30
// Creates an enabled menu item with the given text and listener and menu id 0
MenuItem::MenuItem(const GUIString& text,ComponentListener* listener)
    :TextListItem(text),mListener(listener)
{
    this->mDisabled=false;
    this->mMenuId=0;
}
// FUNCTION: WON 0x006fd400
// Closes the open submenu and clears the selection
void PopupMenu::Reset()
{
    this->mCurSubMenu=(PopupMenu*)NULL;
    this->SetSelItem(-1,true);
}
// FUNCTION: WON 0x006fd440
// FUNCTION: WON 0x006fd460
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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fcdc0
// (public: virtual void * __thiscall WONAPI::MenuItem::`scalar deleting destructor'(unsigned int)    [??_GMenuItem@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006fcde0
// (public: virtual __thiscall WONAPI::MenuItem::~MenuItem(void)    [??1MenuItem@WONAPI@@UAE@XZ])
