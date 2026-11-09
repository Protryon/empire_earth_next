// SetComponent bindings of the scroller, scrollbar-scroller and tab-control configs (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// Binds the scroller config to its scroller (the linker folds this body into the identical one at 0x0078f960)
void ScrollerConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScroller=static_cast<Scroller*>(component);
}
// Binds the scrollbar-scroller config to its component (the linker folds this body into the identical one at 0x0078f860)
void ScrollbarScrollerConfig::SetComponent(Component* component)
{
    ScrollerConfig::SetComponent(component);
    this->mScrollbarScroller=static_cast<ScrollbarScroller*>(component);
}
// Binds the tab-control config to its tab control (the linker folds this body into the identical one at 0x0078f960)
void TabCtrlConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mTabCtrl=static_cast<TabCtrl*>(component);
}
}
