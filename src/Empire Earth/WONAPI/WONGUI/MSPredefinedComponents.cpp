// Predefined child components of the Windows-style scroll container config
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
// FUNCTION: WON 0x0078fdd0
// Maps HORZSCROLLBAR / VERTSCROLLBAR to the scroll container's scrollbars with a new scrollbar config;
// other names go to the scroller config base
bool MSScrollContainerConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HORZSCROLLBAR") {
        config=new MSScrollbarConfig;
        component=this->mScrollContainer->mHorzScrollbar;
    } else if(name=="VERTSCROLLBAR") {
        config=new MSScrollbarConfig;
        component=this->mScrollContainer->mVertScrollbar;
    } else return ScrollbarScrollerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
