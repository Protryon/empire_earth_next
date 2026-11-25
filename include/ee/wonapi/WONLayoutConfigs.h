// The layout configs (the original's target-local classes, with global RTTI names).
#ifndef EE_WONAPI_WONLAYOUTCONFIGS_H
#define EE_WONAPI_WONLAYOUTCONFIGS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/cfgbody.h"
// a config subclass's body in ee_won_gui cfgbody.rs (R6 WON round 34; the host: WONCfgBody.inl in ConfigTableHost.cpp)
namespace WONAPI { intptr_t CfgBodyCall(ComponentConfig* inConfig, EeWonCfgBody inBody, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0); }
// PercentLayoutConfig (0xac bytes, constructor 0x787fb0) and ColorSchemeConfig (constructor 0x788260) are kinds of
// the one media config (WONLobbyConfigImages.h, R6 WON GUI round 36)
#include "ee/wonapi/WONLobbyConfigImages.h"
typedef WONAPI::MediaConfig PercentLayoutConfig;
typedef WONAPI::MediaConfig ColorSchemeConfig;
#endif
