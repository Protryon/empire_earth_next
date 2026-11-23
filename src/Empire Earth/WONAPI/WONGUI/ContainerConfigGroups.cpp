// Container config: PERCENTLAYOUT and COLORSCHEME groups (file name inferred)
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
#include "ee/wonapi/WONPercentLayout.h"
#include "rust/won_gui/guiconf.h"
// The groups' reads and objects are ee_won_gui cfgbody.rs's (R6 WON round 34) over WONCfgBody.inl; this is the forward.
namespace WONAPI {
// Reads a PERCENTLAYOUT group into a percent layout (relative to the component if no reference is given) or a
// COLORSCHEME group into the container's colour scheme; other instructions go to the base config (cfgbody.rs)
bool ContainerConfig::HandleInstruction(ConfigParser* p, const std::string& t) { return CfgBodyCall(this, EeWonCfgBody::ContainerInstruction, (intptr_t)p, (intptr_t)t.data(), (intptr_t)t.size()) != 0; }
}
