// Label configuration keys (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.

namespace WONAPI {
// FUNCTION: WON 0x0078d2b0
// Handles the label configuration keys TEXT, FONT, TEXTCOLOR, BACKGROUND, VERTALIGN, HORZALIGN, WRAP and
// ADJUSTHEIGHTTOFITTEXT; passes other keys to the base configuration
bool LabelConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mLabel->SetText(this->ReadString(false),true);
    else if(aKeyIdx==1) this->mLabel->SetFont(this->ReadFont());
    else if(aKeyIdx==2) this->mLabel->SetTextColor(this->ReadColor());
    else if(aKeyIdx==3) this->mLabel->SetBackground(this->ReadBackground());
    else if(aKeyIdx==4) this->mLabel->SetVertAlign(this->ReadVertAlign());
    else if(aKeyIdx==5) this->mLabel->SetHorzAlign(this->ReadHorzAlign());
    else if(aKeyIdx==6) this->mLabel->SetWrap(this->ReadBool());
    else if(aKeyIdx==7) this->mLabel->SetAdjustHeightToFitText(this->ReadBool());
    else return ComponentConfig::Virtual24(instruction);
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0078d290
// Creates a label configuration (base members only)
LabelConfig::LabelConfig()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078ef70
// (public: virtual __thiscall WONAPI::LabelConfig::~LabelConfig(void)    [??1LabelConfig@WONAPI@@UAE@XZ])
