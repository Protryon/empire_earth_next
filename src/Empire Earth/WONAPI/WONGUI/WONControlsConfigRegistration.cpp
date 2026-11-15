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
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyWONInputBox.h"
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
#include "ee/wonapi/WONLobbyWONFrameConfig.h"
#include "ee/wonapi/WONLobbyWONFrameValueConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
using namespace WONAPI;
SmartPtr<WONSkin> gSkin;

// Returns a copy of the skin's push button
static Component* PushButtonFactory() { return gSkin->GetPushButton(); }

// Returns a copy of the skin's radio button
static Component* RadioButtonFactory() { return gSkin->GetRadioButton(); }

// Returns a copy of the skin's check box
static Component* CheckboxFactory() { return gSkin->GetCheckbox(); }

// Returns a copy of the skin's tab button
static Component* TabButtonFactory() { return gSkin->GetTabButton(); }

// Returns a copy of the skin's header button
static Component* HeaderButtonFactory() { return gSkin->GetHeaderButton(); }

// Returns a copy of the skin's close button
static Component* CloseButtonFactory() { return gSkin->GetCloseButton(); }

// Returns a copy of the skin's input box
static Component* InputBoxFactory() { return gSkin->GetInputBox(); }

// Returns a copy of the skin's combo box
static Component* ComboBoxFactory() { return gSkin->GetComboBox(); }

// Returns a copy of the skin's scroll container
static Component* ScrollContainerFactory() { return gSkin->GetScrollContainer(); }

// Returns a copy of the skin's list control
static Component* ListCtrlFactory() { return gSkin->GetListCtrl(); }

// Returns a copy of the skin's tab control
static Component* TabCtrlFactory() { return gSkin->GetTabCtrl(); }

// Returns a new frame component
static Component* WONFrameComponentFactory() { return new WONFrameComponent; }

// Returns a new frame component with the skin's popup background frame
static Component* PopupBackFactory() { return gSkin->GetPopupBack(); }

// Returns a new frame component with the skin's dialog background frame
static Component* DialogBackFactory() { return gSkin->GetDialogBack(); }

// Returns a new skinned button
static Component* WONButtonSkinFactory() { return new WONButton; }

// Returns a new skinned header button
static Component* WONHeaderButtonSkinFactory() { return new WONHeaderButton; }

// Returns a new simple (image-only) button
static Component* WONSimpleButtonSkinFactory() { return new WONSimpleButton; }

// Returns a new skinned scroll container
static Component* WONScrollContainerSkinFactory() { return new WONScrollContainer; }

// Returns a new skinned list control
static Component* WONListCtrlSkinFactory() { return new WONListCtrl; }

// Returns a new skinned tab control on the current skin
static Component* WONTabCtrlSkinFactory() { return new WONTabCtrl(gSkin.get()); }

// Returns a new skinned combo box
static Component* WONComboBoxSkinFactory() { return new WONComboBox; }

// Returns a new skinned scroll bar
static Component* WONScrollbarSkinFactory() { return new WONScrollbar; }

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
	theTable->AddAllocator("WONScrollbarSkin",WONScrollbarConfig::CfgFactory,WONScrollbarSkinFactory);
	theTable->AddAllocator("WONHeaderButtonSkin",WONButtonConfig::CfgFactory,WONHeaderButtonSkinFactory);
	theTable->AddAllocator("WONSimpleButtonSkin",WONSimpleButtonConfig::CfgFactory,WONSimpleButtonSkinFactory);
	theTable->AddAllocator("WONScrollContainerSkin",WONScrollContainerConfig::CfgFactory,WONScrollContainerSkinFactory);
	theTable->AddAllocator("WONListCtrlSkin",WONListCtrlConfig::CfgFactory,WONListCtrlSkinFactory);
	theTable->AddAllocator("WONTabCtrlSkin",WONTabCtrlConfig::CfgFactory,WONTabCtrlSkinFactory);
	theTable->AddAllocator("WONComboBoxSkin",WONComboBoxConfig::CfgFactory,WONComboBoxSkinFactory);

	theTable->AddAllocator("WONButton",WONButtonConfig::CfgFactory,PushButtonFactory);
	theTable->AddAllocator("WONCheckbox",WONButtonConfig::CfgFactory,CheckboxFactory);
	theTable->AddAllocator("WONRadioButton",WONButtonConfig::CfgFactory,RadioButtonFactory);
	theTable->AddAllocator("WONTabButton",WONButtonConfig::CfgFactory,TabButtonFactory);
	theTable->AddAllocator("WONHeaderButton",WONButtonConfig::CfgFactory,HeaderButtonFactory);
	theTable->AddAllocator("WONCloseButton",WONButtonConfig::CfgFactory,CloseButtonFactory);
	theTable->AddAllocator("WONPopupBack",WONFrameComponentConfig::CfgFactory,PopupBackFactory);
	theTable->AddAllocator("WONDialogBack",WONFrameComponentConfig::CfgFactory,DialogBackFactory);

	theTable->AddAllocator("WONInputBox",InputBoxConfig::CfgFactory,InputBoxFactory);
	theTable->AddAllocator("WONComboBox",WONComboBoxConfig::CfgFactory,ComboBoxFactory);
	theTable->AddAllocator("WONTabCtrl",WONTabCtrlConfig::CfgFactory,TabCtrlFactory);
	theTable->AddAllocator("WONScrollContainer",WONScrollContainerConfig::CfgFactory,ScrollContainerFactory);
	theTable->AddAllocator("WONListCtrl",WONListCtrlConfig::CfgFactory,ListCtrlFactory);
	theTable->AddAllocator("WONFrameComponent",WONFrameComponentConfig::CfgFactory,WONFrameComponentFactory);
}
}
