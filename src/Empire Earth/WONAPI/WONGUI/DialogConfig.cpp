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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00789f80
// Handles the dialog config instructions ALLOWDRAG, KEEPINROOT, ENDONESCAPE, DONTENDONESCAPE and
// DEFAULTBUTTON; passes others to the container config
bool DialogConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) {
        bool value=this->ReadBool();
        this->mDialog->mAllowDrag=value;
    } else if(aKeyIdx==1) {
        bool value=this->ReadBool();
        this->mDialog->mKeepInRoot=value;
    } else if(aKeyIdx==2) {
        int id=this->ReadInt();
        this->mDialog->SetEndOnEscape(id);
    } else if(aKeyIdx==3) this->mDialog->SetDontEndOnEscape();
    else if(aKeyIdx==4) {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->mDialog->SetDefaultButton(button);
    } else return ContainerConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a1eb0
// (class WONAPI::Button * __cdecl WONAPI::SafeConfigGetComponent(class WONAPI::Component *,char const *)    [?SafeConfigGetComponent@WONAPI@@YAPAVButton@)
