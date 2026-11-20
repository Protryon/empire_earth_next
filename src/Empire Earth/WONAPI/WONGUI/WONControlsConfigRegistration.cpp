// Resource table registration of the WON skin and skinned controls, with their factories
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
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
#include "ee/wonapi/WONLobbyWONFrameConfig.h"
#include "ee/wonapi/WONLobbyWONFrameValueConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
using namespace WONAPI;

// A copy of one of the skin's templates (skinobj.rs EeWonSkinGet)
template <EeWonSkinGet G> static Component* SkinFactory() { return (Component*)ee_won_skin_get(WONSkinGlobal(), G, 0); }
// A new widget of one of the WON skin's kinds (the tab control on the game's skin)
template <EeWonShellKind K> static Component* KindFactory() { return WONShellNew(K, K == EeWonShellKind::WonTabCtrl ? (intptr_t)WONSkinGlobal() : K == EeWonShellKind::WonScrollbar ? 1 : 0); }
// Returns a new skinned button
static Component* WONButtonSkinFactory() { return new WONButton; }

namespace WONAPI {


// Registers the WON skin and skinned control types (skin templates and skin-made controls) with their
// configuration and component factories
void WONControlsConfig_Init(ResourceConfigTable *theTable)
{
	theTable->AddAllocator("WONSkin",NewConfigOf<WONSkinConfig>,NewComponentOf<Component>);
	theTable->AddAllocator("WONButtonSkin",NewConfigOf<WONButtonConfig>,WONButtonSkinFactory);
	theTable->AddAllocator("WONScrollbarSkin",NewConfigOf<WONScrollbarConfig>,KindFactory<EeWonShellKind::WonScrollbar>);
	theTable->AddAllocator("WONHeaderButtonSkin",NewConfigOf<WONButtonConfig>,KindFactory<EeWonShellKind::WonHeaderButton>);
	theTable->AddAllocator("WONSimpleButtonSkin",NewConfigOf<WONSimpleButtonConfig>,KindFactory<EeWonShellKind::WonSimpleButton>);
	theTable->AddAllocator("WONScrollContainerSkin",NewConfigOf<WONScrollContainerConfig>,KindFactory<EeWonShellKind::WonScrollContainer>);
	theTable->AddAllocator("WONListCtrlSkin",NewConfigOf<WONListCtrlConfig>,KindFactory<EeWonShellKind::WonListCtrl>);
	theTable->AddAllocator("WONTabCtrlSkin",NewConfigOf<WONTabCtrlConfig>,KindFactory<EeWonShellKind::WonTabCtrl>);
	theTable->AddAllocator("WONComboBoxSkin",NewConfigOf<WONComboBoxConfig>,KindFactory<EeWonShellKind::WonComboBox>);

	theTable->AddAllocator("WONButton",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::PushButton>);
	theTable->AddAllocator("WONCheckbox",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::Checkbox>);
	theTable->AddAllocator("WONRadioButton",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::RadioButton>);
	theTable->AddAllocator("WONTabButton",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::TabButton>);
	theTable->AddAllocator("WONHeaderButton",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::HeaderButton>);
	theTable->AddAllocator("WONCloseButton",NewConfigOf<WONButtonConfig>,SkinFactory<EeWonSkinGet::CloseButton>);
	theTable->AddAllocator("WONPopupBack",NewConfigOf<WONFrameComponentConfig>,SkinFactory<EeWonSkinGet::PopupBack>);
	theTable->AddAllocator("WONDialogBack",NewConfigOf<WONFrameComponentConfig>,SkinFactory<EeWonSkinGet::DialogBack>);

	theTable->AddAllocator("WONInputBox",NewConfigOf<InputBoxConfig>,SkinFactory<EeWonSkinGet::InputBox>);
	theTable->AddAllocator("WONComboBox",NewConfigOf<WONComboBoxConfig>,SkinFactory<EeWonSkinGet::ComboBox>);
	theTable->AddAllocator("WONTabCtrl",NewConfigOf<WONTabCtrlConfig>,SkinFactory<EeWonSkinGet::TabCtrl>);
	theTable->AddAllocator("WONScrollContainer",NewConfigOf<WONScrollContainerConfig>,SkinFactory<EeWonSkinGet::ScrollContainer>);
	theTable->AddAllocator("WONListCtrl",NewConfigOf<WONListCtrlConfig>,SkinFactory<EeWonSkinGet::ListCtrl>);
	theTable->AddAllocator("WONFrameComponent",NewConfigOf<WONFrameComponentConfig>,KindFactory<EeWonShellKind::WonFrameComponent>);
}
}
