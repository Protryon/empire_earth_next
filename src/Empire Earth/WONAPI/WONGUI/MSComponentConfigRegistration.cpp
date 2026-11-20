
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
#include "ee/wonapi/WONLobbyMSConfig.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONGUIShell.h"
// Factories for the Windows-style controls and their configs, and the table that registers them (file name inferred)
namespace WONAPI {
// Creates a Windows-style push button
Component* MSButtonConfig::ButtonFactory() { return WONShellNew(EeWonShellKind::MsButton); }
// Creates a Windows-style radio button
Component* MSButtonConfig::RadioFactory() { return WONShellNew(EeWonShellKind::MsRadioButton); }
// Creates a Windows-style input box
Component* MSInputBoxConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsInputBox); }
// Creates a Windows-style combo box
Component* MSComboBoxConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsComboBox); }
// Creates a Windows-style arrow button
Component* MSArrowButtonConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsArrowButton,Direction_Up); }
// Creates a Windows-style scrollbar
Component* MSScrollbarConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsScrollbar,true); }
// Creates a dialog background
Component* MSDlgBackConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsDlgBack); }
// Creates a 3D frame
Component* MS3DFrameConfig::CompFactory() { return WONShellNew(EeWonShellKind::Ms3DFrame,3,false); }
// Creates a Windows-style tab control
Component* MSTabCtrlConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsTabCtrl); }
// Creates a Windows-style scroll container
Component* MSScrollContainerConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsScrollContainer); }



// Registers every Windows-style control type (MSButton ... MSHeaderControl) by name with its config
// and component factory
void MSComponentConfig_Init(ResourceConfigTable *theTable)
{
	theTable->AddAllocator("MSButton",NewConfigOf<MSButtonConfig>,MSButtonConfig::ButtonFactory);
	theTable->AddAllocator("MSCheckbox",NewConfigOf<MSButtonConfig>,MSButtonConfig::CheckboxFactory);
	theTable->AddAllocator("MSRadioButton",NewConfigOf<MSButtonConfig>,MSButtonConfig::RadioFactory);
	theTable->AddAllocator("MSLabel",NewConfigOf<MSButtonConfig>,MSButtonConfig::LabelFactory);
	theTable->AddAllocator("MSInputBox",NewConfigOf<MSInputBoxConfig>,MSInputBoxConfig::CompFactory);
	theTable->AddAllocator("MSComboBox",NewConfigOf<MSComboBoxConfig>,MSComboBoxConfig::CompFactory);
	theTable->AddAllocator("MSArrowButton",NewConfigOf<MSArrowButtonConfig>,MSArrowButtonConfig::CompFactory);
	theTable->AddAllocator("MSCloseButton",NewConfigOf<MSCloseButtonConfig>,MSCloseButtonConfig::CompFactory);
	theTable->AddAllocator("MSTabButton",NewConfigOf<MSTabButtonConfig>,MSTabButtonConfig::CompFactory);
	theTable->AddAllocator("MSHeaderButton",NewConfigOf<MSHeaderButtonConfig>,MSHeaderButtonConfig::CompFactory);
	theTable->AddAllocator("MSScrollbar",NewConfigOf<MSScrollbarConfig>,MSScrollbarConfig::CompFactory);
	theTable->AddAllocator("MSScrollContainer",NewConfigOf<MSScrollContainerConfig>,MSScrollContainerConfig::CompFactory);
	theTable->AddAllocator("MSSeperator",NewConfigOf<MSSeperatorConfig>,MSSeperatorConfig::CompFactory);
	theTable->AddAllocator("MS3DFrame",NewConfigOf<MS3DFrameConfig>,MS3DFrameConfig::CompFactory);
	theTable->AddAllocator("MSEtchedFrame",NewConfigOf<MSEtchedFrameConfig>,MSEtchedFrameConfig::CompFactory);
	theTable->AddAllocator("MSEtchedFrameGroup",NewConfigOf<MSEtchedFrameGroupConfig>,MSEtchedFrameGroupConfig::CompFactory);
	theTable->AddAllocator("MSDlgBack",NewConfigOf<MSDlgBackConfig>,MSDlgBackConfig::CompFactory);
	theTable->AddAllocator("MSTabCtrl",NewConfigOf<MSTabCtrlConfig>,MSTabCtrlConfig::CompFactory);
	theTable->AddAllocator("MSListCtrl",NewConfigOf<MSListCtrlConfig>,MSListCtrlConfig::CompFactory);
	theTable->AddAllocator("MSHeaderControl",NewConfigOf<MSHeaderControlConfig>,MSHeaderControlConfig::CompFactory);
}

}

// Factories declared inline in the class headers
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
#include "ee/wonapi/WONLobbyMSConfig.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONGUIShell.h"
namespace WONAPI {
// Creates a list header button
Component* MSHeaderButtonConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsHeaderButton); }
// Creates a list header control
Component* MSHeaderControlConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsHeaderControl); }
// Creates a Windows-style list control
Component* MSListCtrlConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsListCtrl); }
}

namespace WONAPI {
// Creates an etched frame group
Component* MSEtchedFrameGroupConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsEtchedFrameGroup); }
}

namespace WONAPI {
// Creates a Windows-style check box
Component* MSButtonConfig::CheckboxFactory() { return WONShellNew(EeWonShellKind::MsCheckbox); }
}

namespace WONAPI {
// Creates a tab button
Component* MSTabButtonConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsTabButton,0,true,3); }
// Creates a Windows-style label
Component* MSButtonConfig::LabelFactory() { return WONShellNew(EeWonShellKind::MsLabel); }
}
