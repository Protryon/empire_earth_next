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
    int ref=-1,param=-1;
    if(instruction=="REF") ref=0;
    else if(instruction=="LEFT") ref=0;
    else if(instruction=="RIGHT") ref=1;
    else if(instruction=="TOP") ref=2;
    else if(instruction=="BOTTOM") ref=3;
    else if(instruction=="PX") param=0;
    else if(instruction=="PY") param=1;
    else if(instruction=="PW") param=2;
    else if(instruction=="PH") param=3;
    else if(instruction=="LP") param=4;
    else if(instruction=="TP") param=5;
    else if(instruction=="RP") param=6;
    else if(instruction=="BP") param=7;
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
    int color=0;
    if(instruction=="3DDARKSHADOW") color=0;
    else if(instruction=="3DFACE") color=1;
    else if(instruction=="3DHILIGHT") color=2;
    else if(instruction=="3DSHADOW") color=3;
    else if(instruction=="SCROLLBAR") color=4;
    else if(instruction=="BUTTONTEXT") color=5;
    else if(instruction=="GRAYTEXT") color=6;
    else if(instruction=="HILIGHT") color=7;
    else if(instruction=="HILIGHTTEXT") color=8;
    else if(instruction=="TOOLTIPBACK") color=9;
    else if(instruction=="TOOLTIPTEXT") color=10;
    else if(instruction=="MENUBACK") color=11;
    else if(instruction=="MENUTEXT") color=12;
    else if(instruction=="BACK") color=13;
    else if(instruction=="TEXT") color=14;
    else if(instruction=="LINK") color=15;
    else if(instruction=="LINKDOWN") color=16;
    else return false;
    ColorScheme* destination=this->mColorScheme.get();
    DWORD value=this->ReadColor();
    destination->SetStandardColor(color,value);
    return true;
}
