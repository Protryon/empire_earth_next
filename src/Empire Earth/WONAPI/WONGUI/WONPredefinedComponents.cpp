// Named scrollbars of the WON scroll container config (file name inferred)
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
// FUNCTION: WON 0x007989f0
// Returns the HORZSCROLLBAR or VERTSCROLLBAR child with a new WON scrollbar config; asks the base config otherwise
bool WONScrollContainerConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HORZSCROLLBAR") {
        config=new WONScrollbarConfig;
        component=this->mScrollContainer->mHorzScrollbar;
    } else if(name=="VERTSCROLLBAR") {
        config=new WONScrollbarConfig;
        component=this->mScrollContainer->mVertScrollbar;
    } else return ScrollbarScrollerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
