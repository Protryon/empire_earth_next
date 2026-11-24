// The resource configs' table reader's host (R6 WON round 39): the 45 pure-table config handlers are ee_won's
// cfgtab.rs programs; their reads, frame and setters are WONCfgTable.inl (shared with tests/won/won_cfgtab_test.cpp).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/WONPercentLayout.h" // the percent layouts WONCfgBody.inl makes
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/won_gui/guiconf.h"
#include "ee/wonapi/WONGUIShell.h"
extern bool gShowParseTime; // ComponentConfigHost.cpp's
extern int gLinesParsed;
namespace WONAPI {
void CfgSkinClickSound(ComponentConfig*); // below
#include "ee/wonapi/WONCfgTable.inl"
#include "ee/wonapi/WONCfgBody.inl" // the config subclasses' bodies (R6 WON round 34: cfgbody.rs)
}

// What the handlers' units kept besides their instructions (ButtonConfig.cpp's FITTEXT, WONSkinConfig's click sound);
// since R6 WONLobby round 26 the bindings, predefined children and named values of the deleted config classes are their
// kinds' (ee_won_gui cfgkind.rs, WONCompCfg.inl).
namespace WONAPI {
// A config of a kind: a ContainerConfig for the container kinds (the original classes' constructors)
ComponentConfig* CfgNewOfKind(EeWonCfgKind inKind)
{
    if(ee_won_cfg_kind_is(inKind,EeWonCfgKind::Container)) return new ContainerConfig(inKind);
    return new ComponentConfig(inKind);
}
// Grows the button to fit the largest of a comma-separated list of texts, keeping its current text (cfgbody.rs)
void ComponentConfig::HandleFitText() { CfgBodyCall(this, EeWonCfgBody::FitText); }
// WONSkinConfig's BUTTONCLICKSOUND: a sound read, then the click sound of the skin's template buttons and every named
// button type
void CfgSkinClickSound(ComponentConfig* inConfig)
{
    SmartPtr<Sound> sound=inConfig->ReadSound();
    ee_won_skin_click_sound(WONSkinGlobal(),sound.get());
}
}
// ---- the config subclasses' forwards to their bodies (ee_won_gui cfgbody.rs over WONCfgBody.inl, R6 WON round 34;
// here since R6 WON GUI round 36: ComponentLayoutConfigs.cpp, ContainerConfigGroups.cpp, FontConfig.cpp and
// ImageConfig.cpp held nothing else)
// Finds the component this config is named after in the parent config; reports an error if it is missing
bool PercentLayoutConfig::PreParse(WONAPI::ConfigParser* p) { return WONAPI::CfgBodyCall(this, EeWonCfgBody::PercentPreParse, (intptr_t)p) != 0; }
// Starts from the colors of the nearest parent component with its own scheme, else the window manager's
bool ColorSchemeConfig::PreParse(WONAPI::ConfigParser* p) { return WONAPI::CfgBodyCall(this, EeWonCfgBody::SchemePreParse, (intptr_t)p) != 0; }
namespace WONAPI {
// Reads a PERCENTLAYOUT group into a percent layout (relative to the component if no reference is given) or a
// COLORSCHEME group into the container's colour scheme; other instructions go to the base config (cfgbody.rs)
bool ContainerConfig::HandleInstruction(ConfigParser* p, const std::string& t) { return CfgBodyCall(this, EeWonCfgBody::ContainerInstruction, (intptr_t)p, (intptr_t)t.data(), (intptr_t)t.size()) != 0; }
// Creates a font config that needs its font looked up
FontConfig::FontConfig() : ComponentConfig(EeWonCfgKind::Font)
{
    this->mHaveInitedComponents=true;
    this->mNeedRecalcFont=true;
}
// The configured font, first fetched from the default window when the descriptor changed and no matching font is
// held (cfgbody.rs)
Font* FontConfig::GetFont() { return (Font*)CfgBodyCall(this, EeWonCfgBody::FontGet); }
// Sets the font directly, taking its descriptor, so no lookup is needed
void FontConfig::SetFont(Font* f) { CfgBodyCall(this, EeWonCfgBody::FontSet, (intptr_t)f); }
// The font's SIZE or HEIGHT as a named value; false without a font or for other names
bool FontConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::FontNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }
// The image's WIDTH or HEIGHT as a named value; false for any other name (cfgbody.rs)
bool ImageConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::ImageNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }
// Sets the raw image (also as the current image) and marks the native image as not yet built
void ImageFilterConfig::SetRawImage(RawImage* inImage)
{
    this->mRawImage=inImage;
    this->mImage=inImage;
    this->mImageIsRawImage=true;
}
// R6 WON round 41: ImageFilterConfig's instructions (SRC, FILTER and HandleFilter's filters, DUPLICATE,
// MULTIHORZCROP, EXPORT) are programs of ee_won's table reader (cfgtab.rs, cfgimg.rs) over WONCfgTable.inl.
// The image, first converting a pending raw image to a native one on the default window (cfgbody.rs)
Image* ImageFilterConfig::GetImage() { return (Image*)CfgBodyCall(this, EeWonCfgBody::FilterImage); }
}
