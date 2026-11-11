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
// FUNCTION: WON 0x007facb0
// Returns a new browser component config
ComponentConfig* BrowserComponentConfig::CfgFactory()
{
    return new BrowserComponentConfig;
}
}

#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x0078f960
// Binds the config to the browser component it configures
void BrowserComponentConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mBrowserComponent=static_cast<BrowserComponent*>(component);
}
// FUNCTION: WON 0x007fa7e0
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
// FUNCTION: WON 0x007fa8a0
// Handles the TRANSPARENT, SCROLLER and FONTSIZE instructions; throws on an unknown font size name
// In the original TRANSPARENT takes no boolean argument and there is no BACKGROUND instruction
bool BrowserComponentConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
        this->mBrowserComponent->SetTransparent(true);
    else if(aKeyIdx==1)
        this->mBrowserComponent->SetScroller(SafeConfigGetComponent<Scroller*>(this->ReadComponent(),"Scroller"));
    else if(aKeyIdx==2)
    {
        HTMLFontSize size;
        GUIString value=this->ReadString(false);
        const int aNameIdx_value=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_BROWSERCOMPONENTCONFIG_VIRTUAL24_VALUE,value.mData->mText,value.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_value==0) size=HTMLFontSize_Smallest;
        else if(aNameIdx_value==1) size=HTMLFontSize_Small;
        else if(aNameIdx_value==2) size=HTMLFontSize_Medium;
        else if(aNameIdx_value==3) size=HTMLFontSize_Large;
        else if(aNameIdx_value==4) size=HTMLFontSize_Largest;
        else throw ConfigObjectException("Invalid font size: "+std::string(value));
        this->mBrowserComponent->SetFontSize(size);
    }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
// FUNCTION: WON 0x007fab70
// Registers the BrowserComponent type with its config and component factories
void BrowserComponentConfig::Init(ResourceConfigTable* table)
{
    table->AddAllocator("BrowserComponent",BrowserComponentConfig::CfgFactory,BrowserComponentConfig::CompFactory);
}
// FUNCTION: WON 0x007fac50
// Returns a new browser component
Component* BrowserComponentConfig::CompFactory()
{
    return new BrowserComponent;
}
}
