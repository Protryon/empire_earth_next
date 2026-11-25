// The component configs' hosts (R6 WON GUI round 37: ComponentConfigHost.cpp with ConfigTableHost.cpp and
// WONGUIConfig.cpp).
// The component configs (R6 WONLobby round 25): ComponentConfig's readers, look-ups, config map and instructions,
// ResourceConfig and ResourceConfigTable are ee_won_gui's compcfg.rs objects; their forwards and host are
// ee/wonapi/WONCompCfg.inl. This unit compiles them and the load of a config file (ReadConfig_Unsafe: ee_won_gui's
// cfgload.rs over ee/wonapi/WONCfgLoad.inl), and keeps the statics and the missing-component report.
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
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h" // the page view's text area
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/WONPercentLayout.h" // the percent layouts WONCfgBody.inl makes
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/guireg.h"
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

// ---- ConfigTableHost.cpp until R6 WON GUI round 37
// The resource configs' table reader's host (R6 WON round 39): the 45 pure-table config handlers are ee_won's
// cfgtab.rs programs; their reads, frame and setters are WONCfgTable.inl (shared with tests/won/won_cfgtab_test.cpp).
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

// ---- WONGUIConfig.cpp until R6 WON GUI round 37
// Registration of the GUI component types with the skin loader and their config and component factories (the
// Windows skin's MSComponentConfigRegistration.cpp, the WON skin's WONControlsConfigRegistration.cpp, the font and
// image configs' since R6 WONLobby round 26: the configs are kinds, ee_won_gui cfgkind.rs)
using namespace WONAPI;
typedef EeWonCfgKind K;
typedef EeWonShellKind S;
// A copy of one of the skin's templates (skinobj.rs EeWonSkinGet); a WON tab control on the game's skin
template <EeWonSkinGet G> static Component* SkinFactory() { return (Component*)ee_won_skin_get(WONSkinGlobal(), G, 0); }
static Component* WONTabCtrlFactory() { return WONShellNew(S::WonTabCtrl, (intptr_t)WONSkinGlobal()); }
// the registrations' factories (guireg.rs has the names and the order: R6 WON round 33)
static intptr_t RegHost(EeWonGuiReg inEntry, bool inConfig)
{
    typedef EeWonGuiReg E;
    switch (inEntry) {
    case E::Component: return inConfig ? (intptr_t)NewConfigOf<K::Component> : (intptr_t)NewComponentOf<Component>;
    case E::LayoutPoint: return inConfig ? (intptr_t)NewConfigOf<K::Component> : (intptr_t)NewComponentOf<LayoutPoint>;
    case E::Container: return inConfig ? (intptr_t)NewConfigOf<K::Container> : (intptr_t)NewComponentOf<Container>;
    case E::Dialog: return inConfig ? (intptr_t)NewConfigOf<K::Dialog> : (intptr_t)NewComponentOf<Dialog>;
    case E::ImageButton: return inConfig ? (intptr_t)NewConfigOf<K::ImageButton> : (intptr_t)NewComponentOf<ImageButton>;
    case E::Label: return inConfig ? (intptr_t)NewConfigOf<K::Label> : (intptr_t)NewComponentOf<Label>;
    case E::ListArea: return inConfig ? (intptr_t)NewConfigOf<K::ListArea> : (intptr_t)NewComponentOf<ListArea>;
    case E::MultiListArea: return inConfig ? (intptr_t)NewConfigOf<K::MultiListArea> : (intptr_t)NewComponentOf<MultiListArea>;
    case E::MultiListHeader: return inConfig ? (intptr_t)NewConfigOf<K::MultiListHeader> : (intptr_t)NewComponentOf<MultiListHeader>;
    case E::InputBox: return inConfig ? (intptr_t)NewConfigOf<K::InputBox> : (intptr_t)NewComponentOf<InputBox>;
    case E::Splitter: return inConfig ? (intptr_t)NewConfigOf<K::Splitter> : (intptr_t)NewComponentOf<Splitter>;
    case E::TextArea: return inConfig ? (intptr_t)NewConfigOf<K::TextArea> : (intptr_t)NewComponentOf<TextArea>;
    case E::TabCtrl: return inConfig ? (intptr_t)NewConfigOf<K::TabCtrl> : (intptr_t)NewComponentOf<TabCtrl>;
    case E::TreeArea: return inConfig ? (intptr_t)NewConfigOf<K::TreeArea> : (intptr_t)NewComponentOf<TreeArea>;
    case E::RectangleComponent: return inConfig ? (intptr_t)NewConfigOf<K::Rectangle> : (intptr_t)NewComponentOf<RectangleComponent>;
    case E::RectangleComponent3D: return inConfig ? (intptr_t)NewConfigOf<K::Rectangle3D> : (intptr_t)NewComponentOf<RectangleComponent3D>;
    case E::ImageComponent: return inConfig ? (intptr_t)NewConfigOf<K::ImageComponent> : (intptr_t)NewComponentOf<ImageComponent>;
    case E::ScreenContainer: return inConfig ? (intptr_t)NewConfigOf<K::ScreenContainer> : (intptr_t)NewComponentOf<ScreenContainer>;
    case E::MSButton: return inConfig ? (intptr_t)NewConfigOf<K::MsButton> : (intptr_t)NewShellOf<S::MsButton>;
    case E::MSCheckbox: return inConfig ? (intptr_t)NewConfigOf<K::MsButton> : (intptr_t)NewShellOf<S::MsCheckbox>;
    case E::MSRadioButton: return inConfig ? (intptr_t)NewConfigOf<K::MsButton> : (intptr_t)NewShellOf<S::MsRadioButton>;
    case E::MSLabel: return inConfig ? (intptr_t)NewConfigOf<K::MsButton> : (intptr_t)NewShellOf<S::MsLabel>;
    case E::MSInputBox: return inConfig ? (intptr_t)NewConfigOf<K::InputBox> : (intptr_t)NewShellOf<S::MsInputBox>;
    case E::MSComboBox: return inConfig ? (intptr_t)NewConfigOf<K::MsComboBox> : (intptr_t)NewShellOf<S::MsComboBox>;
    case E::MSArrowButton: return inConfig ? (intptr_t)NewConfigOf<K::MsArrowButton> : (intptr_t)NewShellOf<S::MsArrowButton,Direction_Up>;
    case E::MSCloseButton: return inConfig ? (intptr_t)NewConfigOf<K::Button> : (intptr_t)NewShellOf<S::MsCloseButton>;
    case E::MSTabButton: return inConfig ? (intptr_t)NewConfigOf<K::MsTabButton> : (intptr_t)NewShellOf<S::MsTabButton,0,true,3>;
    case E::MSHeaderButton: return inConfig ? (intptr_t)NewConfigOf<K::MsButton> : (intptr_t)NewShellOf<S::MsHeaderButton>;
    case E::MSScrollbar: return inConfig ? (intptr_t)NewConfigOf<K::MsScrollbar> : (intptr_t)NewShellOf<S::MsScrollbar,true>;
    case E::MSScrollContainer: return inConfig ? (intptr_t)NewConfigOf<K::MsScrollContainer> : (intptr_t)NewShellOf<S::MsScrollContainer>;
    case E::MSSeperator: return inConfig ? (intptr_t)NewConfigOf<K::Component> : (intptr_t)NewShellOf<S::MsSeperator,6>;
    case E::MS3DFrame: return inConfig ? (intptr_t)NewConfigOf<K::Ms3DFrame> : (intptr_t)NewShellOf<S::Ms3DFrame,3,false>;
    case E::MSEtchedFrame: return inConfig ? (intptr_t)NewConfigOf<K::Component> : (intptr_t)NewShellOf<S::MsEtchedFrame>;
    case E::MSEtchedFrameGroup: return inConfig ? (intptr_t)NewConfigOf<K::MsEtchedFrameGroup> : (intptr_t)NewShellOf<S::MsEtchedFrameGroup>;
    case E::MSDlgBack: return inConfig ? (intptr_t)NewConfigOf<K::Component> : (intptr_t)NewShellOf<S::MsDlgBack>;
    case E::MSTabCtrl: return inConfig ? (intptr_t)NewConfigOf<K::MsTabCtrl> : (intptr_t)NewShellOf<S::MsTabCtrl>;
    case E::MSListCtrl: return inConfig ? (intptr_t)NewConfigOf<K::MsListCtrl> : (intptr_t)NewShellOf<S::MsListCtrl>;
    case E::MSHeaderControl: return inConfig ? (intptr_t)NewConfigOf<K::MsHeaderControl> : (intptr_t)NewShellOf<S::MsHeaderControl>;
    case E::Image: return inConfig ? (intptr_t)NewConfigOf<K::Image> : (intptr_t)NoComponent;
    case E::StretchImage: return inConfig ? (intptr_t)NewConfigOf<K::StretchImage> : (intptr_t)NoComponent;
    case E::StretchCenterImage: return inConfig ? (intptr_t)NewConfigOf<K::StretchCenterImage> : (intptr_t)NoComponent;
    case E::ImageFilter: return inConfig ? (intptr_t)NewConfigOf<K::ImageFilter> : (intptr_t)NoComponent;
    case E::Font: return inConfig ? (intptr_t)NewConfigOf<K::Font> : (intptr_t)NoComponent;
    case E::WONSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonSkin> : (intptr_t)NewComponentOf<Component>;
    case E::WONButtonSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)NewComponentOf<WONButton>;
    case E::WONScrollbarSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonScrollbar> : (intptr_t)NewShellOf<S::WonScrollbar,1>;
    case E::WONHeaderButtonSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)NewShellOf<S::WonHeaderButton>;
    case E::WONSimpleButtonSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonSimpleButton> : (intptr_t)NewShellOf<S::WonSimpleButton>;
    case E::WONScrollContainerSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonScrollContainer> : (intptr_t)NewShellOf<S::WonScrollContainer>;
    case E::WONListCtrlSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonListCtrl> : (intptr_t)NewShellOf<S::WonListCtrl>;
    case E::WONTabCtrlSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonTabCtrl> : (intptr_t)WONTabCtrlFactory;
    case E::WONComboBoxSkin: return inConfig ? (intptr_t)NewConfigOf<K::WonComboBox> : (intptr_t)NewShellOf<S::WonComboBox>;
    case E::WONButton: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::PushButton>;
    case E::WONCheckbox: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::Checkbox>;
    case E::WONRadioButton: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::RadioButton>;
    case E::WONTabButton: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::TabButton>;
    case E::WONHeaderButton: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::HeaderButton>;
    case E::WONCloseButton: return inConfig ? (intptr_t)NewConfigOf<K::WonButton> : (intptr_t)SkinFactory<EeWonSkinGet::CloseButton>;
    case E::WONPopupBack: return inConfig ? (intptr_t)NewConfigOf<K::WonFrameComponent> : (intptr_t)SkinFactory<EeWonSkinGet::PopupBack>;
    case E::WONDialogBack: return inConfig ? (intptr_t)NewConfigOf<K::WonFrameComponent> : (intptr_t)SkinFactory<EeWonSkinGet::DialogBack>;
    case E::WONInputBox: return inConfig ? (intptr_t)NewConfigOf<K::InputBox> : (intptr_t)SkinFactory<EeWonSkinGet::InputBox>;
    case E::WONComboBox: return inConfig ? (intptr_t)NewConfigOf<K::WonComboBox> : (intptr_t)SkinFactory<EeWonSkinGet::ComboBox>;
    case E::WONTabCtrl: return inConfig ? (intptr_t)NewConfigOf<K::WonTabCtrl> : (intptr_t)SkinFactory<EeWonSkinGet::TabCtrl>;
    case E::WONScrollContainer: return inConfig ? (intptr_t)NewConfigOf<K::WonScrollContainer> : (intptr_t)SkinFactory<EeWonSkinGet::ScrollContainer>;
    case E::WONListCtrl: return inConfig ? (intptr_t)NewConfigOf<K::WonListCtrl> : (intptr_t)SkinFactory<EeWonSkinGet::ListCtrl>;
    case E::WONFrameComponent: return inConfig ? (intptr_t)NewConfigOf<K::WonFrameComponent> : (intptr_t)NewShellOf<S::WonFrameComponent>;
    }
    return 0;
}
namespace WONAPI {
// Registers the base GUI component types and their configs' kinds, then the Windows skin's, the image and font
// configs, the WON skin's, and the Ok and Cancel control ids
void WONGUIConfig_Init(ResourceConfigTable*) { ee_won_gui_config_init(RegHost); }
}
