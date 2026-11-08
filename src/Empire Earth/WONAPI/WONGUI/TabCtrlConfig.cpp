// Tab control configuration: resource-file keys and the predefined page layout point
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00795740
// Handles a tab control configuration key (add tab, bar on right, arrows on left, bar height, tab and bar
// overlap, tab offsets); passes other keys to the container configuration
bool TabCtrlConfig::Virtual24(const std::string& key)
{
    if(key=="ADDTAB") {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->EnsureComma();
        Container* container=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mTabCtrl->AddTab(button,container);
    } else if(key=="TABBARONRIGHT") {
        this->mTabCtrl->SetTabBarOnRight(this->ReadBool());
    } else if(key=="ARROWSONLEFT") {
        this->mTabCtrl->SetArrowsOnLeft(this->ReadBool());
    } else if(key=="TABBARHEIGHT") {
        int height=this->ReadInt(); this->mTabCtrl->mTabBarHeight=height;
    } else if(key=="TABOVERLAP") {
        int overlap=this->ReadInt(); this->mTabCtrl->mBottomTabBar->mOverlap=overlap;
    } else if(key=="TABBAROVERLAP") {
        int overlap=this->ReadInt(); this->mTabCtrl->mTabBarOverlap=overlap;
    } else if(key=="TABVERTOFFSET") {
        int offset=this->ReadInt(); this->mTabCtrl->mBottomTabTopOffset=offset;
    } else if(key=="TABHORZOFFSET") {
        int offset=this->ReadInt(); this->mTabCtrl->mBottomTabLeftOffset=offset;
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x007958e0
// Returns a plain configuration and the tab control's page layout point for CONTAINERLAYOUT; passes other
// names to the container configuration
bool TabCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="CONTAINERLAYOUT") {
        config=new ComponentConfig;
        component=this->mTabCtrl->mContainerLayout;
    } else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00795720
// Creates a tab control configuration (base members only)
TabCtrlConfig::TabCtrlConfig()
{
}
}
