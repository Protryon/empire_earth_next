// Container mouse refresh and the WON scroll container's constructor and attribute copy (file name inferred)
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
#include "ee/wonapi/WONLobbySkinnedContainers.h"
namespace WONAPI {
// FUNCTION: WON 0x006f66c0
// Re-sends the current mouse position as a move if the mouse is inside the window
void Container::MouseAt()
{
    if(!this->GetRoot()->mMouseIsInClientArea)
        return;
    int x,y;
    this->GetMousePos(x,y);
    this->MouseMove(x,y);
}
// Creates a WON scroll container with no header and an opaque background (emitted at 0x00719c50)
WONScrollContainer::WONScrollContainer()
{
    this->mHeaderHeight=0;
    this->mTransparent=false;
}
// FUNCTION: WON 0x00719f70
// Copies another scroll container's frame and corner component
void WONScrollContainer::CopyAttributes(WONScrollContainer* from)
{
    this->mFrame=from->mFrame;
    this->SetCorner(from->mCorner.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719c50
// (public: __thiscall WONAPI::WONScrollContainer::WONScrollContainer(void)    [??0WONScrollContainer@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719cc0
// (public: virtual void * __thiscall WONAPI::WONScrollContainer::`scalar deleting destructor'(unsigned int)    [??_GWONScrollContainer@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719ce0
// (public: virtual __thiscall WONAPI::WONScrollContainer::~WONScrollContainer(void))
