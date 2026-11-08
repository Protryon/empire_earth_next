// Popup menu constructor (file name inferred)
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
#include "ee/wonapi/WONLobbyMSPainters.h"
namespace WONAPI {
// Creates a popup menu with the menu font and colors, a popup background and 4-pixel padding; mLastX/Y stay
// unset (emitted at 0x006fd150)
PopupMenu::PopupMenu()
{
    this->SetFont(this->GetNamedFont("Menu"));
    this->SetListFlags(0x806,true);
    this->SetBackground(new MSPopup);
    this->mTextColor=0x8000000c;
    this->mMouseUpCount=0;
    this->mTimerAcc=0;
    this->mKeyboardMode=false;
    this->SetBorderPadding(4,4,4,4);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fd320
// (public: virtual void * __thiscall WONAPI::PopupMenu::`scalar deleting destructor'(unsigned int)    [??_GPopupMenu@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006fd340
// (public: virtual __thiscall WONAPI::PopupMenu::~PopupMenu(void)    [??1PopupMenu@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fd150
// (public: __thiscall WONAPI::PopupMenu::PopupMenu(void)    [??0PopupMenu@WONAPI@@QAE@XZ])
