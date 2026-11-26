// The component configs (R6 WONLobby round 25): ComponentConfig's readers, look-ups, config map and instructions,
// ResourceConfig and ResourceConfigTable are ee_won_gui's compcfg.rs objects; their forwards and host are
// ee/wonapi/WONCompCfg.inl. This unit compiles them, the load of a config file (ReadConfig_Unsafe: ee_won_gui's
// cfgload.rs over ee/wonapi/WONCfgLoad.inl), the table reader's host (the 49 table-shaped config handlers are
// ee_won's cfgtab.rs programs; their reads, frame and setters are ee/wonapi/WONCfgTable.inl, shared with
// tests/won/won_cfgtab_test.cpp; ConfigTableHost.cpp until R6 WON lobby and network round 39), the config subclasses'
// bodies, and keeps the statics and the missing-component report.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h" // the page view's text area
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/WONPercentLayout.h" // the percent layouts WONCfgBody.inl makes
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_gui/guiconf.h"
bool gShowParseTime = false; // (SHOWPARSETIME sets it during a parse: WONCfgTable.inl)
int gLinesParsed = 0; // (WONCfgTable.inl counts the lines)
namespace WONAPI {
#include "ee/wonapi/WONCompCfg.inl"

SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;

#include "ee/wonapi/WONCfgLoad.inl" // ReadConfig_Unsafe (R6 WON round 34: cfgload.rs)

// The init record (type 2) that hands the configuration to InitComponent
ComponentConfigInit::ComponentConfigInit(ComponentConfig* theConfig) : ComponentInit{2}, mConfig(theConfig) {}

// A container configuration of a container kind with no layout reference and layout id 0
ContainerConfig::ContainerConfig(EeWonCfgKind inKind) : ComponentConfig(inKind), mContainer(NULL), mLayoutRef(NULL), mCurLayoutId(0) {}
// The component as the container (the original's ScreenContainerConfig bound the component's base alone: no container)
void ContainerConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    if(this->mKind!=EeWonCfgKind::ScreenContainer) this->mContainer=static_cast<Container*>(component);
}
// Sets the component the config reads its settings into
void ComponentConfig::SetComponent(Component* component)
{
    this->mComponent=component;
}

// The component configured under the name, or NULL when there is none
Component* ComponentConfigGet(ComponentConfig* config,const char* name)
{
    return config->Fn386860(std::string(name));
}
}
namespace WONAPI {
void CfgSkinClickSound(ComponentConfig*); // below
#include "ee/wonapi/WONCfgTable.inl"
#include "ee/wonapi/WONCfgBody.inl" // the config subclasses' bodies (R6 WON round 34: cfgbody.rs)
}

// What the handlers' units kept besides their instructions (ButtonConfig.cpp's FITTEXT, WONSkinConfig's click sound);
// since R6 WONLobby round 26 the bindings, predefined children and named values of the deleted config classes are their
// kinds' (ee_won_gui cfgkind.rs, WONCompCfg.inl).
namespace WONAPI {
// A config of a kind: a ContainerConfig for the container kinds, a MediaConfig for the media kinds (the original
// classes' constructors)
ComponentConfig* CfgNewOfKind(EeWonCfgKind inKind)
{
    typedef EeWonCfgKind K;
    if(ee_won_cfg_kind_is(inKind,K::Container)) return new ContainerConfig(inKind);
    if(ee_won_cfg_kind_is(inKind,K::Image) || inKind==K::Font || inKind==K::PercentLayout || inKind==K::ColorScheme) return new MediaConfig(inKind);
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
namespace WONAPI {
// Reads a PERCENTLAYOUT group into a percent layout (relative to the component if no reference is given) or a
// COLORSCHEME group into the container's colour scheme; other instructions go to the base config (cfgbody.rs)
bool ContainerConfig::HandleInstruction(ConfigParser* p, const std::string& t) { return CfgBodyCall(this, EeWonCfgBody::ContainerInstruction, (intptr_t)p, (intptr_t)t.data(), (intptr_t)t.size()) != 0; }
// ---- the media configs (R6 WON GUI round 36: the original's image, font, percent layout and colour scheme config
// classes as kinds; their bodies cfgbody.rs's; ComponentLayoutConfigs.cpp, FontConfig.cpp and ImageConfig.cpp held
// nothing else)
// A config of a media kind: the image kinds and the font make no child components; a stretch image config makes its
// empty image (0x38b4e0, 0x38bf90), a scheme config its own colour scheme (0x788260); a percent layout config has no
// references and every parameter unset (0x787fb0); a font config needs its font looked up (0x38a1c0)
MediaConfig::MediaConfig(EeWonCfgKind inKind) : ComponentConfig(inKind), mStretchImage(NULL), mStretchCenterImage(NULL),
    mImageIsRawImage(false), mNeedRecalcFont(true), target(NULL), refs(), params{-1,-1,-1,-1,-1,-1,-1,-1}
{
    this->mHaveInitedComponents=inKind!=EeWonCfgKind::PercentLayout && inKind!=EeWonCfgKind::ColorScheme;
    if(inKind==EeWonCfgKind::StretchImage) this->mImage=this->mStretchImage=new StretchImage(NULL,0,0,0,0);
    if(inKind==EeWonCfgKind::StretchCenterImage) this->mImage=this->mStretchCenterImage=new StretchCenterImage;
    if(inKind==EeWonCfgKind::ColorScheme) this->mColorScheme=new ColorScheme;
}
// The percent layout finds the component it is named after in the parent config (an error if it is missing); the
// scheme starts from the colours of the nearest parent component with its own scheme, else the window manager's
bool MediaConfig::PreParse(ConfigParser* p)
{
    if(this->mKind==EeWonCfgKind::PercentLayout) return CfgBodyCall(this, EeWonCfgBody::PercentPreParse, (intptr_t)p) != 0;
    if(this->mKind==EeWonCfgKind::ColorScheme) return CfgBodyCall(this, EeWonCfgBody::SchemePreParse, (intptr_t)p) != 0;
    return ComponentConfig::PreParse(p);
}
// The font's SIZE or HEIGHT (false without a font), the image's WIDTH or HEIGHT; false for other names
bool MediaConfig::GetNamedValue(const std::string& n, int& v)
{
    if(this->mKind==EeWonCfgKind::Font) return CfgBodyCall(this, EeWonCfgBody::FontNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0;
    if(this->Is(EeWonCfgKind::Image)) return CfgBodyCall(this, EeWonCfgBody::ImageNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0;
    return ComponentConfig::GetNamedValue(n, v);
}
// The configured font, first fetched from the default window when the descriptor changed and no matching font is
// held (cfgbody.rs)
Font* MediaConfig::GetFont() { return (Font*)CfgBodyCall(this, EeWonCfgBody::FontGet); }
// Sets the font directly, taking its descriptor, so no lookup is needed
void MediaConfig::SetFont(Font* f) { CfgBodyCall(this, EeWonCfgBody::FontSet, (intptr_t)f); }
// The filter's raw image (also as the current image), its native image not yet built
void MediaConfig::SetRawImage(RawImage* inImage)
{
    this->mRawImage=inImage;
    this->mImage=inImage;
    this->mImageIsRawImage=true;
}
// The image; the filter's first converts a pending raw image to a native one on the default window (cfgbody.rs)
Image* MediaConfig::GetImage() { return this->mKind==EeWonCfgKind::ImageFilter ? (Image*)CfgBodyCall(this, EeWonCfgBody::FilterImage) : this->mImage.get(); }
}
