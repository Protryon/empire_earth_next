// Dialog configuration instructions
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
// FUNCTION: WON 0x00789f80
// Handles the dialog config instructions ALLOWDRAG, KEEPINROOT, ENDONESCAPE, DONTENDONESCAPE and
// DEFAULTBUTTON; passes others to the container config
bool DialogConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="ALLOWDRAG") {
        bool value=this->ReadBool();
        this->mDialog->mAllowDrag=value;
    } else if(instruction=="KEEPINROOT") {
        bool value=this->ReadBool();
        this->mDialog->mKeepInRoot=value;
    } else if(instruction=="ENDONESCAPE") {
        int id=this->ReadInt();
        this->mDialog->SetEndOnEscape(id);
    } else if(instruction=="DONTENDONESCAPE") this->mDialog->SetDontEndOnEscape();
    else if(instruction=="DEFAULTBUTTON") {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->mDialog->SetDefaultButton(button);
    } else return ContainerConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1eb0
// (class WONAPI::Button * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAVButton@)
