// The etched frame group's predefined child (file name inferred)
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
// FUNCTION: WON 0x0078f980
// Returns the group's title label and a label config for the name LABEL; other names go to the
// container config
bool MSEtchedFrameGroupConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="LABEL") {
        config=LabelConfig::CfgFactory();
        component=this->mFrame->GetLabel();
    } else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
