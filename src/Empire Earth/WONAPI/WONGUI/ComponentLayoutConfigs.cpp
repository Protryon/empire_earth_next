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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
// FUNCTION: WON 0x00788000
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
// FUNCTION: WON 0x007880c0
// Reads a percent layout instruction: a reference component per edge (REF = LEFT, RIGHT, TOP, BOTTOM)
// or one of the percentages (PX, PY, PW, PH, LP, TP, RP, BP)
bool PercentLayoutConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    int ref=-1,param=-1;
    if(aKeyIdx==0) ref=0;
    else if(aKeyIdx==1) ref=0;
    else if(aKeyIdx==2) ref=1;
    else if(aKeyIdx==3) ref=2;
    else if(aKeyIdx==4) ref=3;
    else if(aKeyIdx==5) param=0;
    else if(aKeyIdx==6) param=1;
    else if(aKeyIdx==7) param=2;
    else if(aKeyIdx==8) param=3;
    else if(aKeyIdx==9) param=4;
    else if(aKeyIdx==10) param=5;
    else if(aKeyIdx==11) param=6;
    else if(aKeyIdx==12) param=7;
    else return false;
    if(ref>=0) this->refs[ref]=this->ReadComponent();
    else if(param>=0) this->params[param]=this->ReadInt();
    return true;
}
// FUNCTION: WON 0x007883b0
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
// FUNCTION: WON 0x00788410
// Sets one standard color of the scheme from its instruction name (3DFACE, HILIGHT, LINK ...)
bool ColorSchemeConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    int color=0;
    if(aKeyIdx==0) color=0;
    else if(aKeyIdx==1) color=1;
    else if(aKeyIdx==2) color=2;
    else if(aKeyIdx==3) color=3;
    else if(aKeyIdx==4) color=4;
    else if(aKeyIdx==5) color=5;
    else if(aKeyIdx==6) color=6;
    else if(aKeyIdx==7) color=7;
    else if(aKeyIdx==8) color=8;
    else if(aKeyIdx==9) color=9;
    else if(aKeyIdx==10) color=10;
    else if(aKeyIdx==11) color=11;
    else if(aKeyIdx==12) color=12;
    else if(aKeyIdx==13) color=13;
    else if(aKeyIdx==14) color=14;
    else if(aKeyIdx==15) color=15;
    else if(aKeyIdx==16) color=16;
    else return false;
    ColorScheme* destination=this->mColorScheme.get();
    DWORD value=this->ReadColor();
    destination->SetStandardColor(color,value);
    return true;
}
