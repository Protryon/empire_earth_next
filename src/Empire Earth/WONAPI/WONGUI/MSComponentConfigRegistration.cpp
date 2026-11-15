
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
#include "ee/wonapi/WONLobbyMSBorderPainters.h"
// Factories for the Windows-style controls and their configs, and the table that registers them (file name inferred)
namespace WONAPI {
// Creates a Windows-style push button
Component* MSButtonConfig::ButtonFactory() { return new MSButton; }
// Creates a Windows-style radio button
Component* MSButtonConfig::RadioFactory() { return new MSRadioButton; }
// Creates a Windows-style input box
Component* MSInputBoxConfig::CompFactory() { return new MSInputBox; }
// Creates a Windows-style combo box
Component* MSComboBoxConfig::CompFactory() { return new MSComboBox; }
// Creates a Windows-style arrow button
Component* MSArrowButtonConfig::CompFactory() { return new MSArrowButton; }
// Creates a Windows-style scrollbar
Component* MSScrollbarConfig::CompFactory() { return new MSScrollbar; }
// Creates a dialog background
Component* MSDlgBackConfig::CompFactory() { return new MSDlgBack; }
// Creates a 3D frame
Component* MS3DFrameConfig::CompFactory() { return new MS3DFrame; }
// Creates a Windows-style tab control
Component* MSTabCtrlConfig::CompFactory() { return new MSTabCtrl; }
// Creates a Windows-style scroll container
Component* MSScrollContainerConfig::CompFactory() { return new MSScrollContainer; }

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
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
namespace WONAPI {
// Creates a list header button
Component* MSHeaderButtonConfig::CompFactory() { return new MSHeaderButton; }
// Creates a list header control
Component* MSHeaderControlConfig::CompFactory() { return new MSHeaderControl; }
// Creates a Windows-style list control
Component* MSListCtrlConfig::CompFactory() { return new MSListCtrl; }
}

namespace WONAPI {
// Creates an etched frame group
Component* MSEtchedFrameGroupConfig::CompFactory() { return new MSEtchedFrameGroup; }
}

namespace WONAPI {
// Creates the config for a Windows-style button, check box, radio button or label
ComponentConfig* MSButtonConfig::CfgFactory() { return new MSButtonConfig; }
// Creates a Windows-style check box
Component* MSButtonConfig::CheckboxFactory() { return new MSCheckbox; }
}

namespace WONAPI {
// Creates a tab button
Component* MSTabButtonConfig::CompFactory() { return new MSTabButton; }
// Creates a Windows-style label
Component* MSButtonConfig::LabelFactory() { return new MSLabel; }
}
