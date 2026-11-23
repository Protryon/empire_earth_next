// Registration of the GUI component types with the skin loader and their config and component factories (the
// Windows skin's MSComponentConfigRegistration.cpp, the WON skin's WONControlsConfigRegistration.cpp, the font and
// image configs' since R6 WONLobby round 26: the configs are kinds, ee_won_gui cfgkind.rs)
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
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_gui/guireg.h"
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
    case E::Image: return inConfig ? (intptr_t)NewConfigOf<ImageConfig> : (intptr_t)NoComponent;
    case E::StretchImage: return inConfig ? (intptr_t)NewConfigOf<StretchImageConfig> : (intptr_t)NoComponent;
    case E::StretchCenterImage: return inConfig ? (intptr_t)NewConfigOf<StretchCenterImageConfig> : (intptr_t)NoComponent;
    case E::ImageFilter: return inConfig ? (intptr_t)NewConfigOf<ImageFilterConfig> : (intptr_t)NoComponent;
    case E::Font: return inConfig ? (intptr_t)NewConfigOf<FontConfig> : (intptr_t)NoComponent;
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
