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

// FUNCTION: WON 0x00796d70
// Returns a copy of the skin's push button
static Component* PushButtonFactory() { return gSkin->GetPushButton(); }

// FUNCTION: WON 0x00796d80
// Returns a copy of the skin's radio button
static Component* RadioButtonFactory() { return gSkin->GetRadioButton(); }

// FUNCTION: WON 0x00796d90
// Returns a copy of the skin's check box
static Component* CheckboxFactory() { return gSkin->GetCheckbox(); }

// FUNCTION: WON 0x00796da0
// Returns a copy of the skin's tab button
static Component* TabButtonFactory() { return gSkin->GetTabButton(); }

// FUNCTION: WON 0x00796db0
// Returns a copy of the skin's header button
static Component* HeaderButtonFactory() { return gSkin->GetHeaderButton(); }

// FUNCTION: WON 0x00796dc0
// Returns a copy of the skin's close button
static Component* CloseButtonFactory() { return gSkin->GetCloseButton(); }

// FUNCTION: WON 0x00796dd0
// Returns a copy of the skin's input box
static Component* InputBoxFactory() { return gSkin->GetInputBox(); }

// FUNCTION: WON 0x00796de0
// Returns a copy of the skin's combo box
static Component* ComboBoxFactory() { return gSkin->GetComboBox(); }

// FUNCTION: WON 0x00796df0
// Returns a copy of the skin's scroll container
static Component* ScrollContainerFactory() { return gSkin->GetScrollContainer(); }

// FUNCTION: WON 0x00796e00
// Returns a copy of the skin's list control
static Component* ListCtrlFactory() { return gSkin->GetListCtrl(); }

// FUNCTION: WON 0x00796e10
// Returns a copy of the skin's tab control
static Component* TabCtrlFactory() { return gSkin->GetTabCtrl(); }

// FUNCTION: WON 0x00796e20
// Returns a new frame component
static Component* WONFrameComponentFactory() { return new WONFrameComponent; }

// FUNCTION: WON 0x00796e90
// Returns a new frame component with the skin's popup background frame
static Component* PopupBackFactory() { return gSkin->GetPopupBack(); }

// FUNCTION: WON 0x00796ea0
// Returns a new frame component with the skin's dialog background frame
static Component* DialogBackFactory() { return gSkin->GetDialogBack(); }

// FUNCTION: WON 0x00796eb0
// Returns a new skinned button
static Component* WONButtonSkinFactory() { return new WONButton; }

// FUNCTION: WON 0x00796f10
// Returns a new skinned header button
static Component* WONHeaderButtonSkinFactory() { return new WONHeaderButton; }

// FUNCTION: WON 0x00796f70
// Returns a new simple (image-only) button
static Component* WONSimpleButtonSkinFactory() { return new WONSimpleButton; }

// FUNCTION: WON 0x00796fd0
// Returns a new skinned scroll container
static Component* WONScrollContainerSkinFactory() { return new WONScrollContainer; }

// FUNCTION: WON 0x00797030
// Returns a new skinned list control
static Component* WONListCtrlSkinFactory() { return new WONListCtrl; }

// FUNCTION: WON 0x00797090
// Returns a new skinned tab control on the current skin
static Component* WONTabCtrlSkinFactory() { return new WONTabCtrl(gSkin.get()); }

// FUNCTION: WON 0x00797100
// Returns a new skinned combo box
static Component* WONComboBoxSkinFactory() { return new WONComboBox; }

// FUNCTION: WON 0x00797160
// Returns a new skinned scroll bar
static Component* WONScrollbarSkinFactory() { return new WONScrollbar; }

namespace WONAPI {
// FUNCTION: WON 0x00796b50
// Returns a new frame component configuration
ComponentConfig* WONFrameComponentConfig::CfgFactory() { return new WONFrameComponentConfig; }
// FUNCTION: WON 0x00796bc0
// Returns a new skinned scroll bar configuration
ComponentConfig* WONScrollbarConfig::CfgFactory() { return new WONScrollbarConfig; }
// FUNCTION: WON 0x00796c20
// Returns a new skinned scroll container configuration
ComponentConfig* WONScrollContainerConfig::CfgFactory() { return new WONScrollContainerConfig; }
// FUNCTION: WON 0x00796ca0
// Returns a new skinned list control configuration
ComponentConfig* WONListCtrlConfig::CfgFactory() { return new WONListCtrlConfig; }
// FUNCTION: WON 0x00796d20
// Returns a new WON skin configuration
ComponentConfig* WONSkinConfig::CfgFactory() { return new WONSkinConfig; }

// FUNCTION: WON 0x007969c0
// Returns a new simple button configuration
ComponentConfig* WONSimpleButtonConfig::CfgFactory() { return new WONSimpleButtonConfig; }
// FUNCTION: WON 0x00796a30
// Returns a new skinned button configuration
ComponentConfig* WONButtonConfig::CfgFactory() { return new WONButtonConfig; }

// FUNCTION: WON 0x00795fa0
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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00796af0
// (public: static class WONAPI::ComponentConfig * __cdecl WONAPI::WONTabCtrlConfig::CfgFactory(void)    [?CfgFactory@WONTabCtrlConfig@WONAPI@@SAPAVCompon)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00796c80
// (public: virtual void __thiscall WONAPI::WONScrollContainerConfig::Virtual34(class WONAPI::Component *)    [?Virtual34@WONScrollContainerConfig@WONAPI@)

// TEMPLATE: WON 0x00795f70
// TEMPLATE: WON 0x00795f80
// TEMPLATE: WON 0x00795f60
// TEMPLATE: WON 0x00795f90
// TEMPLATE: WON 0x00796a90
