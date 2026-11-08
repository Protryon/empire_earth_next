// Resource configuration of the HTML browser component
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

namespace WONAPI {
// FUNCTION: WON 0x007facb0
// Returns a new browser component config
ComponentConfig* BrowserComponentConfig::CfgFactory()
{
    return new BrowserComponentConfig;
}
}

#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x0078f960
// Binds the config to the browser component it configures
void BrowserComponentConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mBrowserComponent=static_cast<BrowserComponent*>(component);
}
// FUNCTION: WON 0x007fa7e0
// Resolves HTMLVIEW to the browser's HTML view (configured as a scroll area); other names go to the container
bool BrowserComponentConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HTMLVIEW")
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
    if(instruction=="TRANSPARENT")
        this->mBrowserComponent->SetTransparent(true);
    else if(instruction=="SCROLLER")
        this->mBrowserComponent->SetScroller(SafeConfigGetComponent<Scroller*>(this->ReadComponent(),"Scroller"));
    else if(instruction=="FONTSIZE")
    {
        HTMLFontSize size;
        GUIString value=this->ReadString(false);
        if(value.CompareNoCase("Smallest")==0) size=HTMLFontSize_Smallest;
        else if(value.CompareNoCase("Small")==0) size=HTMLFontSize_Small;
        else if(value.CompareNoCase("Medium")==0) size=HTMLFontSize_Medium;
        else if(value.CompareNoCase("Large")==0) size=HTMLFontSize_Large;
        else if(value.CompareNoCase("Largest")==0) size=HTMLFontSize_Largest;
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
