// Named child components of the WON list control config (file name inferred)
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
// FUNCTION: WON 0x00798e30
// Returns the list's HEADER or MULTILISTAREA child with a new config for it; asks the scroll container config otherwise
bool WONListCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HEADER") {
        component=this->mListCtrl->GetHeader();
        config=new WONHeaderControlConfig;
    } else if(name=="MULTILISTAREA") {
        component=this->mListCtrl->GetListArea();
        config=new MultiListAreaConfig;
    } else return WONScrollContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
