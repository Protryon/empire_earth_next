// Font configuration: font selection, style flags and default/named font registration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLayoutConfigs.h" // CfgBodyCall
// The named values' and the lazy values' bodies are ee_won_gui cfgbody.rs's (R6 WON round 34) over WONCfgBody.inl.
namespace WONAPI {
// The configured font, first fetched from the default window when the descriptor changed and no matching font is
// held (cfgbody.rs)
Font* FontConfig::GetFont() { return (Font*)CfgBodyCall(this, EeWonCfgBody::FontGet); }
// Sets the font directly, taking its descriptor, so no lookup is needed
void FontConfig::SetFont(Font* f) { CfgBodyCall(this, EeWonCfgBody::FontSet, (intptr_t)f); }
// The font's SIZE or HEIGHT as a named value; false without a font or for other names
bool FontConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::FontNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }
}


namespace WONAPI {
// Creates a font config that needs its font looked up
FontConfig::FontConfig() : ComponentConfig(EeWonCfgKind::Font)
{
    this->mHaveInitedComponents=true;
    this->mNeedRecalcFont=true;
}
}
