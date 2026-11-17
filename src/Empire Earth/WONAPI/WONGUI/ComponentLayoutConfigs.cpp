// Percent layout and color scheme configuration readers (file name inferred)
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
#include "ee/wonapi/WONLayoutConfigs.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
// Finds the component this config is named after in the parent config; reports an error if it is missing
bool PercentLayoutConfig::PreParse(ConfigParser* parser)
{
    ComponentConfig* parent=static_cast<ComponentConfig*>(this->mParent);
    this->target=parent->Fn386860(this->mName);
    if(this->target==NULL) {
        parser->SetError("Component not found: "+this->mName,false);
        return false;
    }
    return true;
}
// Starts from the colors of the nearest parent component with its own scheme, else the window manager's
bool ColorSchemeConfig::PreParse(ConfigParser*)
{
    ComponentConfig* parent=static_cast<ComponentConfig*>(this->mParent);
    while(parent!=NULL) {
        Component* component=parent->mComponent.get();
        if(component!=NULL) {
            ColorScheme* scheme=component->GetOwnColorScheme();
            if(scheme!=NULL) {
                memcpy(this->mColorScheme->mColors,scheme->mColors,sizeof(this->mColorScheme->mColors));
                return true;
            }
        }
        parent=static_cast<ComponentConfig*>(parent->mParent);
    }
    ColorScheme* scheme=WONAPI::WindowManager::GetDefaultWindowManager()->mColorScheme.get();
    memcpy(this->mColorScheme->mColors,scheme->mColors,sizeof(this->mColorScheme->mColors));
    return true;
}
