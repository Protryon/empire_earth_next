// SetComponent bindings of the component configs: each stores the component under its own type (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00782d50
// Sets the component the config reads its settings into
void ComponentConfig::SetComponent(Component* component)
{
    this->mComponent=component;
}

// Binds the button config to its button (the linker folds this body into the identical one at 0x0078f8d0)
void ButtonConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mButton=static_cast<Button*>(component);
}

// Binds the container config to its container (the linker folds this body into the identical one at 0x0078f8d0)
void ContainerConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mContainer=static_cast<Container*>(component);
}

// Binds the dialog config to its dialog (the linker folds this body into the identical one at 0x0078f960)
void DialogConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mDialog=static_cast<Dialog*>(component);
}

// Binds the input-box config to its input box (the linker folds this body into the identical one at 0x0078f8d0)
void InputBoxConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mInputBox=static_cast<InputBox*>(component);
}

// Binds the label config to its label (the linker folds this body into the identical one at 0x0078f8d0)
void LabelConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mLabel=static_cast<Label*>(component);
}

// Binds the scroll-area config to its scroll area (the linker folds this body into the identical one at 0x0078f960)
void ScrollAreaConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScrollArea=static_cast<ScrollArea*>(component);
}

// Binds the list-area config to its list area (the linker folds this body into the identical one at 0x0078f860)
void ListAreaConfig::SetComponent(Component* component)
{
    ScrollAreaConfig::SetComponent(component);
    this->mListArea=static_cast<ListArea*>(component);
}

// FUNCTION: WON 0x0078fd10
// Binds the multi-column list config to its list area
void MultiListAreaConfig::SetComponent(Component* component)
{
    ListAreaConfig::SetComponent(component);
    this->mMultiListArea=static_cast<MultiListArea*>(component);
}

// Binds the scrollbar config to its scrollbar (the linker folds this body into the identical one at 0x0078f960)
void ScrollbarConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScrollbar=static_cast<Scrollbar*>(component);
}

// FUNCTION: WON 0x0079b5f0
// Binds the tree config to its tree area
void TreeAreaConfig::SetComponent(Component* component)
{
    this->mTreeArea=static_cast<TreeArea*>(component);
    ListAreaConfig::SetComponent(component);
}

// Binds the combo-box config to its combo box (the linker folds this body into the identical one at 0x0078f960)
void ComboBoxConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mComboBox=static_cast<ComboBox*>(component);
}

// FUNCTION: WON 0x0078e7d0
// Binds the Windows-style combo-box config to its combo box
void MSComboBoxConfig::SetComponent(Component* component)
{
    this->mMSComboBox=static_cast<MSComboBox*>(component);
    ComboBoxConfig::SetComponent(component);
}

// Binds the Windows-style scrollbar config to its scrollbar (the linker folds this body into the identical one at 0x0078f860)
void MSScrollbarConfig::SetComponent(Component* component)
{
    ScrollbarConfig::SetComponent(component);
    this->mMSScrollbar=static_cast<MSScrollbar*>(component);
}

// Binds the etched frame-group config to its frame (the linker folds this body into the identical one at 0x0078f960)
void MSEtchedFrameGroupConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mFrame=static_cast<MSEtchedFrameGroup*>(component);
}

// FUNCTION: WON 0x0078f100
// Binds the Windows-style tab-control config to its tab control
void MSTabCtrlConfig::SetComponent(Component* component)
{
    TabCtrlConfig::SetComponent(component);
    this->mMSTabCtrl=static_cast<MSTabCtrl*>(component);
}

// FUNCTION: WON 0x0078fd10
// Binds the Windows-style scroll container config to its scroll container (one folded body with MultiListAreaConfig's)
void MSScrollContainerConfig::SetComponent(Component* component)
{
    ScrollbarScrollerConfig::SetComponent(component);
    this->mScrollContainer=static_cast<MSScrollContainer*>(component);
}

// FUNCTION: WON 0x0078f2a0
// Binds the Windows-style header-control config to its header
void MSHeaderControlConfig::SetComponent(Component* component)
{
    this->mMSHeader=static_cast<MSHeaderControl*>(component);
    MultiListHeaderConfig::SetComponent(component);
}

// FUNCTION: WON 0x0078f380
// Binds the Windows-style list-control config to its list control
void MSListCtrlConfig::SetComponent(Component* component)
{
    this->mListCtrl=static_cast<MSListCtrl*>(component);
    MSScrollContainerConfig::SetComponent(component);
}

// FUNCTION: WON 0x0078f100
// Binds the WON scrollbar config to its scrollbar (one folded body with MSTabCtrlConfig's)
void WONScrollbarConfig::SetComponent(Component* component)
{
    ScrollbarConfig::SetComponent(component);
    this->mScrollbar=static_cast<WONScrollbar*>(component);
}

// FUNCTION: WON 0x00798f70
// Binds the WON header-control config to its header
void WONHeaderControlConfig::SetComponent(Component* component)
{
    MultiListHeaderConfig::SetComponent(component);
    this->mHeader=static_cast<WONHeaderControl*>(component);
}

// FUNCTION: WON 0x00796d00
// Binds the WON list-control config to its list control
void WONListCtrlConfig::SetComponent(Component* component)
{
    WONScrollContainerConfig::SetComponent(component);
    this->mListCtrl=static_cast<WONListCtrl*>(component);
}

}
