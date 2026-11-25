// The component configs (R6 WONLobby round 25): ComponentConfig's readers, look-ups, config map and instructions,
// ResourceConfig and ResourceConfigTable are ee_won_gui's compcfg.rs objects; their forwards and host are
// ee/wonapi/WONCompCfg.inl. This unit compiles them and the load of a config file (ReadConfig_Unsafe: ee_won_gui's
// cfgload.rs over ee/wonapi/WONCfgLoad.inl), and keeps the statics and the missing-component report.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h" // the page view's text area
bool gShowParseTime = false; // (SHOWPARSETIME sets it during a parse: WONCfgTable.inl)
int gLinesParsed = 0; // (ConfigTableHost.cpp counts the lines)
namespace WONAPI {
#include "ee/wonapi/WONCompCfg.inl"

SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;

#include "ee/wonapi/WONCfgLoad.inl" // ReadConfig_Unsafe (R6 WON round 34: cfgload.rs)

// The init record (type 2) that hands the configuration to InitComponent
ComponentConfigInit::ComponentConfigInit(ComponentConfig* theConfig) : ComponentInit{2}, mConfig(theConfig) {}

// A container configuration of a container kind with no layout reference and layout id 0
ContainerConfig::ContainerConfig(EeWonCfgKind inKind) : ComponentConfig(inKind), mContainer(NULL), mLayoutRef(NULL), mCurLayoutId(0) {}
// The component as the container (the original's ScreenContainerConfig bound the component's base alone: no container)
void ContainerConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    if(this->mKind!=EeWonCfgKind::ScreenContainer) this->mContainer=static_cast<Container*>(component);
}
// Sets the component the config reads its settings into
void ComponentConfig::SetComponent(Component* component)
{
    this->mComponent=component;
}

// The component configured under the name, or NULL when there is none
Component* ComponentConfigGet(ComponentConfig* config,const char* name)
{
    return config->Fn386860(std::string(name));
}
}
