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
using namespace WONAPI;
typedef EeWonCfgKind K;
typedef EeWonShellKind S;
// A copy of one of the skin's templates (skinobj.rs EeWonSkinGet); a WON tab control on the game's skin
template <EeWonSkinGet G> static Component* SkinFactory() { return (Component*)ee_won_skin_get(WONSkinGlobal(), G, 0); }
static Component* WONTabCtrlFactory() { return WONShellNew(S::WonTabCtrl, (intptr_t)WONSkinGlobal()); }
namespace WONAPI {
// Registers the base GUI component types and their configs' kinds, then the Windows skin's, the image and font
// configs, the WON skin's, and the Ok and Cancel control ids
void WONGUIConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Component",NewConfigOf<K::Component>,NewComponentOf<Component>);
    table->AddAllocator("LayoutPoint",NewConfigOf<K::Component>,NewComponentOf<LayoutPoint>);
    table->AddAllocator("Container",NewConfigOf<K::Container>,NewComponentOf<Container>);
    table->AddAllocator("Dialog",NewConfigOf<K::Dialog>,NewComponentOf<Dialog>);
    table->AddAllocator("ImageButton",NewConfigOf<K::ImageButton>,NewComponentOf<ImageButton>);
    table->AddAllocator("Label",NewConfigOf<K::Label>,NewComponentOf<Label>);
    table->AddAllocator("ListArea",NewConfigOf<K::ListArea>,NewComponentOf<ListArea>);
    table->AddAllocator("MultiListArea",NewConfigOf<K::MultiListArea>,NewComponentOf<MultiListArea>);
    table->AddAllocator("MultiListHeader",NewConfigOf<K::MultiListHeader>,NewComponentOf<MultiListHeader>);
    table->AddAllocator("InputBox",NewConfigOf<K::InputBox>,NewComponentOf<InputBox>);
    table->AddAllocator("Splitter",NewConfigOf<K::Splitter>,NewComponentOf<Splitter>);
    table->AddAllocator("TextArea",NewConfigOf<K::TextArea>,NewComponentOf<TextArea>);
    table->AddAllocator("TabCtrl",NewConfigOf<K::TabCtrl>,NewComponentOf<TabCtrl>);
    table->AddAllocator("TreeArea",NewConfigOf<K::TreeArea>,NewComponentOf<TreeArea>);
    table->AddAllocator("RectangleComponent",NewConfigOf<K::Rectangle>,NewComponentOf<RectangleComponent>);
    table->AddAllocator("RectangleComponent3D",NewConfigOf<K::Rectangle3D>,NewComponentOf<RectangleComponent3D>);
    table->AddAllocator("ImageComponent",NewConfigOf<K::ImageComponent>,NewComponentOf<ImageComponent>);
    table->AddAllocator("ScreenContainer",NewConfigOf<K::ScreenContainer>,NewComponentOf<ScreenContainer>);
    // the Windows skin (MSComponentConfig_Init, 0x0078dcb0; the configs of MSInputBox, MSCloseButton, MSHeaderButton,
    // MSSeperator, MSEtchedFrame and MSDlgBack were their bases' with a factory)
    table->AddAllocator("MSButton",NewConfigOf<K::MsButton>,NewShellOf<S::MsButton>);
    table->AddAllocator("MSCheckbox",NewConfigOf<K::MsButton>,NewShellOf<S::MsCheckbox>);
    table->AddAllocator("MSRadioButton",NewConfigOf<K::MsButton>,NewShellOf<S::MsRadioButton>);
    table->AddAllocator("MSLabel",NewConfigOf<K::MsButton>,NewShellOf<S::MsLabel>);
    table->AddAllocator("MSInputBox",NewConfigOf<K::InputBox>,NewShellOf<S::MsInputBox>);
    table->AddAllocator("MSComboBox",NewConfigOf<K::MsComboBox>,NewShellOf<S::MsComboBox>);
    table->AddAllocator("MSArrowButton",NewConfigOf<K::MsArrowButton>,NewShellOf<S::MsArrowButton,Direction_Up>);
    table->AddAllocator("MSCloseButton",NewConfigOf<K::Button>,NewShellOf<S::MsCloseButton>);
    table->AddAllocator("MSTabButton",NewConfigOf<K::MsTabButton>,NewShellOf<S::MsTabButton,0,true,3>);
    table->AddAllocator("MSHeaderButton",NewConfigOf<K::MsButton>,NewShellOf<S::MsHeaderButton>);
    table->AddAllocator("MSScrollbar",NewConfigOf<K::MsScrollbar>,NewShellOf<S::MsScrollbar,true>);
    table->AddAllocator("MSScrollContainer",NewConfigOf<K::MsScrollContainer>,NewShellOf<S::MsScrollContainer>);
    table->AddAllocator("MSSeperator",NewConfigOf<K::Component>,NewShellOf<S::MsSeperator,6>);
    table->AddAllocator("MS3DFrame",NewConfigOf<K::Ms3DFrame>,NewShellOf<S::Ms3DFrame,3,false>);
    table->AddAllocator("MSEtchedFrame",NewConfigOf<K::Component>,NewShellOf<S::MsEtchedFrame>);
    table->AddAllocator("MSEtchedFrameGroup",NewConfigOf<K::MsEtchedFrameGroup>,NewShellOf<S::MsEtchedFrameGroup>);
    table->AddAllocator("MSDlgBack",NewConfigOf<K::Component>,NewShellOf<S::MsDlgBack>);
    table->AddAllocator("MSTabCtrl",NewConfigOf<K::MsTabCtrl>,NewShellOf<S::MsTabCtrl>);
    table->AddAllocator("MSListCtrl",NewConfigOf<K::MsListCtrl>,NewShellOf<S::MsListCtrl>);
    table->AddAllocator("MSHeaderControl",NewConfigOf<K::MsHeaderControl>,NewShellOf<S::MsHeaderControl>);
    // the image and font configs (ImageConfig_Init 0x0078adf0, FontConfig_Init 0x0078a080)
    table->AddAllocator("Image",NewConfigOf<ImageConfig>,NoComponent);
    table->AddAllocator("StretchImage",NewConfigOf<StretchImageConfig>,NoComponent);
    table->AddAllocator("StretchCenterImage",NewConfigOf<StretchCenterImageConfig>,NoComponent);
    table->AddAllocator("ImageFilter",NewConfigOf<ImageFilterConfig>,NoComponent);
    table->AddAllocator("Font",NewConfigOf<FontConfig>,NoComponent);
    // the WON skin and its controls (WONControlsConfig_Init, 0x00795fa0): its templates and the controls made from them
    table->AddAllocator("WONSkin",NewConfigOf<K::WonSkin>,NewComponentOf<Component>);
    table->AddAllocator("WONButtonSkin",NewConfigOf<K::WonButton>,NewComponentOf<WONButton>);
    table->AddAllocator("WONScrollbarSkin",NewConfigOf<K::WonScrollbar>,NewShellOf<S::WonScrollbar,1>);
    table->AddAllocator("WONHeaderButtonSkin",NewConfigOf<K::WonButton>,NewShellOf<S::WonHeaderButton>);
    table->AddAllocator("WONSimpleButtonSkin",NewConfigOf<K::WonSimpleButton>,NewShellOf<S::WonSimpleButton>);
    table->AddAllocator("WONScrollContainerSkin",NewConfigOf<K::WonScrollContainer>,NewShellOf<S::WonScrollContainer>);
    table->AddAllocator("WONListCtrlSkin",NewConfigOf<K::WonListCtrl>,NewShellOf<S::WonListCtrl>);
    table->AddAllocator("WONTabCtrlSkin",NewConfigOf<K::WonTabCtrl>,WONTabCtrlFactory);
    table->AddAllocator("WONComboBoxSkin",NewConfigOf<K::WonComboBox>,NewShellOf<S::WonComboBox>);
    table->AddAllocator("WONButton",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::PushButton>);
    table->AddAllocator("WONCheckbox",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::Checkbox>);
    table->AddAllocator("WONRadioButton",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::RadioButton>);
    table->AddAllocator("WONTabButton",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::TabButton>);
    table->AddAllocator("WONHeaderButton",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::HeaderButton>);
    table->AddAllocator("WONCloseButton",NewConfigOf<K::WonButton>,SkinFactory<EeWonSkinGet::CloseButton>);
    table->AddAllocator("WONPopupBack",NewConfigOf<K::WonFrameComponent>,SkinFactory<EeWonSkinGet::PopupBack>);
    table->AddAllocator("WONDialogBack",NewConfigOf<K::WonFrameComponent>,SkinFactory<EeWonSkinGet::DialogBack>);
    table->AddAllocator("WONInputBox",NewConfigOf<K::InputBox>,SkinFactory<EeWonSkinGet::InputBox>);
    table->AddAllocator("WONComboBox",NewConfigOf<K::WonComboBox>,SkinFactory<EeWonSkinGet::ComboBox>);
    table->AddAllocator("WONTabCtrl",NewConfigOf<K::WonTabCtrl>,SkinFactory<EeWonSkinGet::TabCtrl>);
    table->AddAllocator("WONScrollContainer",NewConfigOf<K::WonScrollContainer>,SkinFactory<EeWonSkinGet::ScrollContainer>);
    table->AddAllocator("WONListCtrl",NewConfigOf<K::WonListCtrl>,SkinFactory<EeWonSkinGet::ListCtrl>);
    table->AddAllocator("WONFrameComponent",NewConfigOf<K::WonFrameComponent>,NewShellOf<S::WonFrameComponent>);
    table->RegisterControlId("Ok",ControlId_Ok);
    table->RegisterControlId("Cancel",ControlId_Cancel);
}
}
