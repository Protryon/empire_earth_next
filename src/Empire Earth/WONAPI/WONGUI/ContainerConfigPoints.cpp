// Container config: NEWLAYOUTPOINT (file name inferred)
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
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00789a70
// Creates a named layout point for each name in a comma-separated list, registers its config and adds it
// to the container
bool ContainerConfig::HandleNewLayoutPoint()
{
    while(true) {
        SmartPtr<ComponentConfig> config=new ComponentConfig;
        SmartPtr<LayoutPoint> point=new LayoutPoint;
        config->SetComponent(point.get());
        config->mParent=this;
        this->mConfigMap[this->ReadString(false)]=config;
        this->mContainer->AddLayoutPoint(point.get());
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    return true;
}
}
