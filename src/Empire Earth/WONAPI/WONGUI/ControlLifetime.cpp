// The screen container's constructor (file name inferred)
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
// Creates a screen container showing no screen (emitted at 0x00706a20)
ScreenContainer::ScreenContainer() { this->mCurScreen=NULL; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00705560
// (public: virtual void * __thiscall WONAPI::Scroller::`scalar deleting destructor'(unsigned int)    [??_GScroller@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00705740
// (public: virtual void * __thiscall WONAPI::ScrollbarScroller::`scalar deleting destructor'(unsigned int)    [??_GScrollbarScroller@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00705e90
// (public: virtual void * __thiscall WONAPI::Scrollbar::`scalar deleting destructor'(unsigned int)    [??_GScrollbar@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00706a20
// (public: __thiscall WONAPI::ScreenContainer::ScreenContainer(void)    [??0ScreenContainer@WONAPI@@QAE@XZ])
