
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

// Creates the config for an etched frame group
ComponentConfig* MSEtchedFrameGroupConfig::CfgFactory() { return new MSEtchedFrameGroupConfig; }
// Creates the config for a scroll container
ComponentConfig* MSScrollContainerConfig::CfgFactory() { return new MSScrollContainerConfig; }
// Creates the config for a list header control
ComponentConfig* MSHeaderControlConfig::CfgFactory() { return new MSHeaderControlConfig; }
// Creates the config for a list control
ComponentConfig* MSListCtrlConfig::CfgFactory() { return new MSListCtrlConfig; }

// Creates the config for an input box
ComponentConfig* MSInputBoxConfig::CfgFactory() { return new MSInputBoxConfig; }
// Creates the config for a scrollbar
ComponentConfig* MSScrollbarConfig::CfgFactory() { return new MSScrollbarConfig; }
// Creates the config for an arrow button
ComponentConfig* MSArrowButtonConfig::CfgFactory() { return new MSArrowButtonConfig; }
// Creates the config for a close button
ComponentConfig* MSCloseButtonConfig::CfgFactory() { return new MSCloseButtonConfig; }
// Creates the config for a separator
ComponentConfig* MSSeperatorConfig::CfgFactory() { return new MSSeperatorConfig; }
// Creates the config for a dialog background
ComponentConfig* MSDlgBackConfig::CfgFactory() { return new MSDlgBackConfig; }
// Creates the config for a 3D frame
ComponentConfig* MS3DFrameConfig::CfgFactory() { return new MS3DFrameConfig; }
// Creates the config for an etched frame
ComponentConfig* MSEtchedFrameConfig::CfgFactory() { return new MSEtchedFrameConfig; }

// Registers every Windows-style control type (MSButton ... MSHeaderControl) by name with its config
// and component factory
void MSComponentConfig_Init(ResourceConfigTable *theTable)
{
	theTable->AddAllocator("MSButton",MSButtonConfig::CfgFactory,MSButtonConfig::ButtonFactory);
	theTable->AddAllocator("MSCheckbox",MSButtonConfig::CfgFactory,MSButtonConfig::CheckboxFactory);
	theTable->AddAllocator("MSRadioButton",MSButtonConfig::CfgFactory,MSButtonConfig::RadioFactory);
	theTable->AddAllocator("MSLabel",MSButtonConfig::CfgFactory,MSButtonConfig::LabelFactory);
	theTable->AddAllocator("MSInputBox",MSInputBoxConfig::CfgFactory,MSInputBoxConfig::CompFactory);
	theTable->AddAllocator("MSComboBox",MSComboBoxConfig::CfgFactory,MSComboBoxConfig::CompFactory);
	theTable->AddAllocator("MSArrowButton",MSArrowButtonConfig::CfgFactory,MSArrowButtonConfig::CompFactory);
	theTable->AddAllocator("MSCloseButton",MSCloseButtonConfig::CfgFactory,MSCloseButtonConfig::CompFactory);
	theTable->AddAllocator("MSTabButton",MSTabButtonConfig::CfgFactory,MSTabButtonConfig::CompFactory);
	theTable->AddAllocator("MSHeaderButton",MSHeaderButtonConfig::CfgFactory,MSHeaderButtonConfig::CompFactory);
	theTable->AddAllocator("MSScrollbar",MSScrollbarConfig::CfgFactory,MSScrollbarConfig::CompFactory);
	theTable->AddAllocator("MSScrollContainer",MSScrollContainerConfig::CfgFactory,MSScrollContainerConfig::CompFactory);
	theTable->AddAllocator("MSSeperator",MSSeperatorConfig::CfgFactory,MSSeperatorConfig::CompFactory);
	theTable->AddAllocator("MS3DFrame",MS3DFrameConfig::CfgFactory,MS3DFrameConfig::CompFactory);
	theTable->AddAllocator("MSEtchedFrame",MSEtchedFrameConfig::CfgFactory,MSEtchedFrameConfig::CompFactory);
	theTable->AddAllocator("MSEtchedFrameGroup",MSEtchedFrameGroupConfig::CfgFactory,MSEtchedFrameGroupConfig::CompFactory);
	theTable->AddAllocator("MSDlgBack",MSDlgBackConfig::CfgFactory,MSDlgBackConfig::CompFactory);
	theTable->AddAllocator("MSTabCtrl",MSTabCtrlConfig::CfgFactory,MSTabCtrlConfig::CompFactory);
	theTable->AddAllocator("MSListCtrl",MSListCtrlConfig::CfgFactory,MSListCtrlConfig::CompFactory);
	theTable->AddAllocator("MSHeaderControl",MSHeaderControlConfig::CfgFactory,MSHeaderControlConfig::CompFactory);
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
// Creates the config for a Windows-style button, check box, radio button or label
ComponentConfig* MSButtonConfig::CfgFactory() { return new MSButtonConfig; }
// Creates a Windows-style check box
Component* MSButtonConfig::CheckboxFactory() { return WONShellNew(EeWonShellKind::MsCheckbox); }
}

namespace WONAPI {
// Creates a tab button
Component* MSTabButtonConfig::CompFactory() { return WONShellNew(EeWonShellKind::MsTabButton,0,true,3); }
// Creates a Windows-style label
Component* MSButtonConfig::LabelFactory() { return WONShellNew(EeWonShellKind::MsLabel); }
}
