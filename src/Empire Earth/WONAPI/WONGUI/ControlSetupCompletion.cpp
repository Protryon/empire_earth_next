// The WON scroll container's constructor and attribute copy (file name inferred)
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
// Creates a WON scroll container with no header and an opaque background (emitted at 0x00719c50)
WONScrollContainer::WONScrollContainer()
{
    this->mHeaderHeight=0;
    this->mTransparent=false;
}
// Copies another scroll container's frame and corner component
void WONScrollContainer::CopyAttributes(WONScrollContainer* from)
{
    this->mFrame=from->mFrame;
    this->SetCorner(from->mCorner.get());
}
}
