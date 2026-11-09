// Component extra state and destructors (file name inferred)
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
// FUNCTION: WON 0x006f4890
// Creates the extra component state with no background grabber and tooltips allowed
ComponentExtra::ComponentExtra()
{
    this->mGrabBG=NULL;
    this->mDontDoTip=false;
}
// Destroys the extra state, deleting its background grabber before the other members (emitted at 0x006f48d0)
ComponentExtra::~ComponentExtra() { delete this->mGrabBG; }
// Destroys the component, deleting its extra state (emitted at 0x006f4a70)
Component::~Component() { delete this->mExtraStuff; }
// FUNCTION: WON 0x006f4ac0
// Allocates the extra component state on first use
void Component::AllocateExtraStuff()
{
    if(this->mExtraStuff==NULL) this->mExtraStuff=new ComponentExtra;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f48b0
// (public: virtual void * __thiscall WONAPI::ComponentExtra::`scalar deleting destructor'(unsigned int)    [??_GComponentExtra@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006f48d0
// (public: virtual __thiscall WONAPI::ComponentExtra::~ComponentExtra(void)    [??1ComponentExtra@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006f4a70
// (public: virtual __thiscall WONAPI::Component::~Component(void)    [??1Component@WONAPI@@UAE@XZ])
