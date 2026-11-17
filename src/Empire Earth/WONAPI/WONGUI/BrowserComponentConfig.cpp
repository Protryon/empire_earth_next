// Resource configuration of the lobby's page view (BrowserComponent: plain text, WONGUI/BrowserComponent.cpp)
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
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"

namespace WONAPI {
// Returns a new browser component config
ComponentConfig* BrowserComponentConfig::CfgFactory()
{
    return new BrowserComponentConfig;
}
}

#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/won_gui/guiconf.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// Binds the config to the browser component it configures
void BrowserComponentConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mBrowserComponent=static_cast<BrowserComponent*>(component);
}
// Resolves HTMLVIEW to the page view's text area (configured as a scroll area); other names go to the container
bool BrowserComponentConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0)
    {
        config=new ScrollAreaConfig;
        component=this->mBrowserComponent->GetHTMLView();
    }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// Registers the BrowserComponent type with its config and component factories
void BrowserComponentConfig::Init(ResourceConfigTable* table)
{
    table->AddAllocator("BrowserComponent",BrowserComponentConfig::CfgFactory,BrowserComponentConfig::CompFactory);
}
// Returns a new browser component
Component* BrowserComponentConfig::CompFactory()
{
    return new BrowserComponent;
}
}
