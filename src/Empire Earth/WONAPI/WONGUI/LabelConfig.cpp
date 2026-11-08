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

namespace WONAPI {
// FUNCTION: WON 0x0078d2b0
// Handles the label configuration keys TEXT, FONT, TEXTCOLOR, BACKGROUND, VERTALIGN, HORZALIGN, WRAP and
// ADJUSTHEIGHTTOFITTEXT; passes other keys to the base configuration
bool LabelConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="TEXT") this->mLabel->SetText(this->ReadString(false),true);
    else if(instruction=="FONT") this->mLabel->SetFont(this->ReadFont());
    else if(instruction=="TEXTCOLOR") this->mLabel->SetTextColor(this->ReadColor());
    else if(instruction=="BACKGROUND") this->mLabel->SetBackground(this->ReadBackground());
    else if(instruction=="VERTALIGN") this->mLabel->SetVertAlign(this->ReadVertAlign());
    else if(instruction=="HORZALIGN") this->mLabel->SetHorzAlign(this->ReadHorzAlign());
    else if(instruction=="WRAP") this->mLabel->SetWrap(this->ReadBool());
    else if(instruction=="ADJUSTHEIGHTTOFITTEXT") this->mLabel->SetAdjustHeightToFitText(this->ReadBool());
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
