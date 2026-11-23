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
// The bodies are ee_won_gui cfgbody.rs's (R6 WON round 34) over WONCfgBody.inl; these are the forwards.
using namespace WONAPI;
// Finds the component this config is named after in the parent config; reports an error if it is missing
bool PercentLayoutConfig::PreParse(ConfigParser* p) { return WONAPI::CfgBodyCall(this, EeWonCfgBody::PercentPreParse, (intptr_t)p) != 0; }
// Starts from the colors of the nearest parent component with its own scheme, else the window manager's
bool ColorSchemeConfig::PreParse(ConfigParser* p) { return WONAPI::CfgBodyCall(this, EeWonCfgBody::SchemePreParse, (intptr_t)p) != 0; }
