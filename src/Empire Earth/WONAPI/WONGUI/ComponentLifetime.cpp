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
// Allocates the extra component state on first use
void Component::AllocateExtraStuff()
{
    if(this->mExtraStuff==NULL) this->mExtraStuff=new ComponentExtra;
}
}
