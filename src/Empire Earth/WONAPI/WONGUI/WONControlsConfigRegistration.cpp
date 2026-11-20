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
// Returns a new frame component configuration
ComponentConfig* WONFrameComponentConfig::CfgFactory() { return new WONFrameComponentConfig; }
// Returns a new skinned scroll bar configuration
ComponentConfig* WONScrollbarConfig::CfgFactory() { return new WONScrollbarConfig; }
// Returns a new skinned scroll container configuration
ComponentConfig* WONScrollContainerConfig::CfgFactory() { return new WONScrollContainerConfig; }
// Returns a new skinned list control configuration
ComponentConfig* WONListCtrlConfig::CfgFactory() { return new WONListCtrlConfig; }
// Returns a new WON skin configuration
ComponentConfig* WONSkinConfig::CfgFactory() { return new WONSkinConfig; }

// Returns a new simple button configuration
ComponentConfig* WONSimpleButtonConfig::CfgFactory() { return new WONSimpleButtonConfig; }
// Returns a new skinned button configuration
ComponentConfig* WONButtonConfig::CfgFactory() { return new WONButtonConfig; }

// Registers the WON skin and skinned control types (skin templates and skin-made controls) with their
// configuration and component factories
void WONControlsConfig_Init(ResourceConfigTable *theTable)
{
	theTable->AddAllocator("WONSkin",WONSkinConfig::CfgFactory,ComponentConfig::CompFactory);
	theTable->AddAllocator("WONButtonSkin",WONButtonConfig::CfgFactory,WONButtonSkinFactory);
	theTable->AddAllocator("WONScrollbarSkin",WONScrollbarConfig::CfgFactory,KindFactory<EeWonShellKind::WonScrollbar>);
	theTable->AddAllocator("WONHeaderButtonSkin",WONButtonConfig::CfgFactory,KindFactory<EeWonShellKind::WonHeaderButton>);
	theTable->AddAllocator("WONSimpleButtonSkin",WONSimpleButtonConfig::CfgFactory,KindFactory<EeWonShellKind::WonSimpleButton>);
	theTable->AddAllocator("WONScrollContainerSkin",WONScrollContainerConfig::CfgFactory,KindFactory<EeWonShellKind::WonScrollContainer>);
	theTable->AddAllocator("WONListCtrlSkin",WONListCtrlConfig::CfgFactory,KindFactory<EeWonShellKind::WonListCtrl>);
	theTable->AddAllocator("WONTabCtrlSkin",WONTabCtrlConfig::CfgFactory,KindFactory<EeWonShellKind::WonTabCtrl>);
	theTable->AddAllocator("WONComboBoxSkin",WONComboBoxConfig::CfgFactory,KindFactory<EeWonShellKind::WonComboBox>);

	theTable->AddAllocator("WONButton",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::PushButton>);
	theTable->AddAllocator("WONCheckbox",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::Checkbox>);
	theTable->AddAllocator("WONRadioButton",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::RadioButton>);
	theTable->AddAllocator("WONTabButton",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::TabButton>);
	theTable->AddAllocator("WONHeaderButton",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::HeaderButton>);
	theTable->AddAllocator("WONCloseButton",WONButtonConfig::CfgFactory,SkinFactory<EeWonSkinGet::CloseButton>);
	theTable->AddAllocator("WONPopupBack",WONFrameComponentConfig::CfgFactory,SkinFactory<EeWonSkinGet::PopupBack>);
	theTable->AddAllocator("WONDialogBack",WONFrameComponentConfig::CfgFactory,SkinFactory<EeWonSkinGet::DialogBack>);

	theTable->AddAllocator("WONInputBox",InputBoxConfig::CfgFactory,SkinFactory<EeWonSkinGet::InputBox>);
	theTable->AddAllocator("WONComboBox",WONComboBoxConfig::CfgFactory,SkinFactory<EeWonSkinGet::ComboBox>);
	theTable->AddAllocator("WONTabCtrl",WONTabCtrlConfig::CfgFactory,SkinFactory<EeWonSkinGet::TabCtrl>);
	theTable->AddAllocator("WONScrollContainer",WONScrollContainerConfig::CfgFactory,SkinFactory<EeWonSkinGet::ScrollContainer>);
	theTable->AddAllocator("WONListCtrl",WONListCtrlConfig::CfgFactory,SkinFactory<EeWonSkinGet::ListCtrl>);
	theTable->AddAllocator("WONFrameComponent",WONFrameComponentConfig::CfgFactory,KindFactory<EeWonShellKind::WonFrameComponent>);
}
}
