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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00795740
// Handles a tab control configuration key (add tab, bar on right, arrows on left, bar height, tab and bar
// overlap, tab offsets); passes other keys to the container configuration
bool TabCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->EnsureComma();
        Container* container=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mTabCtrl->AddTab(button,container);
    } else if(aKeyIdx==1) {
        this->mTabCtrl->SetTabBarOnRight(this->ReadBool());
    } else if(aKeyIdx==2) {
        this->mTabCtrl->SetArrowsOnLeft(this->ReadBool());
    } else if(aKeyIdx==3) {
        int height=this->ReadInt(); this->mTabCtrl->mTabBarHeight=height;
    } else if(aKeyIdx==4) {
        int overlap=this->ReadInt(); this->mTabCtrl->mBottomTabBar->mOverlap=overlap;
    } else if(aKeyIdx==5) {
        int overlap=this->ReadInt(); this->mTabCtrl->mTabBarOverlap=overlap;
    } else if(aKeyIdx==6) {
        int offset=this->ReadInt(); this->mTabCtrl->mBottomTabTopOffset=offset;
    } else if(aKeyIdx==7) {
        int offset=this->ReadInt(); this->mTabCtrl->mBottomTabLeftOffset=offset;
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x007958e0
// Returns a plain configuration and the tab control's page layout point for CONTAINERLAYOUT; passes other
// names to the container configuration
bool TabCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) {
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
