// Predefined child components of the Windows-style list control config
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
// FUNCTION: WON 0x00790020
// Maps HEADER / MULTILISTAREA to the list control's header and list area with a new config for each;
// other names go to the scroll container config
bool MSListCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HEADER") {
        component=this->mListCtrl->GetHeader();
        config=new MSHeaderControlConfig;
    } else if(name=="MULTILISTAREA") {
        component=this->mListCtrl->GetListArea();
        config=new MultiListAreaConfig;
    } else return MSScrollContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
