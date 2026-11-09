// Registration of the GUI component types with the skin loader and their config and component factories
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
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyMSConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x0079a1a0
// Registers the base GUI component types and their config factories, then the MS, image, font and WON
// control types and the Ok and Cancel control ids
void WONGUIConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Component",ComponentConfig::CfgFactory,ComponentConfig::CompFactory);
    table->AddAllocator("LayoutPoint",ComponentConfig::CfgFactory,ContainerConfig::LayoutPointFactory);
    table->AddAllocator("Container",ContainerConfig::CfgFactory,ContainerConfig::CompFactory);
    table->AddAllocator("Dialog",DialogConfig::CfgFactory,DialogConfig::CompFactory);
    table->AddAllocator("ImageButton",ImageButtonConfig::CfgFactory,ImageButtonConfig::CompFactory);
    table->AddAllocator("Label",LabelConfig::CfgFactory,LabelConfig::CompFactory);
    table->AddAllocator("ListArea",ListAreaConfig::CfgFactory,ListAreaConfig::CompFactory);
    table->AddAllocator("MultiListArea",MultiListAreaConfig::CfgFactory,MultiListAreaConfig::CompFactory);
    table->AddAllocator("MultiListHeader",MultiListHeaderConfig::CfgFactory,MultiListHeaderConfig::CompFactory);
    table->AddAllocator("InputBox",InputBoxConfig::CfgFactory,InputBoxConfig::CompFactory);
    table->AddAllocator("Splitter",SplitterConfig::CfgFactory,SplitterConfig::CompFactory);
    table->AddAllocator("TextArea",TextAreaConfig::CfgFactory,TextAreaConfig::CompFactory);
    table->AddAllocator("TabCtrl",TabCtrlConfig::CfgFactory,TabCtrlConfig::CompFactory);
    table->AddAllocator("TreeArea",TreeAreaConfig::CfgFactory,TreeAreaConfig::CompFactory);
    table->AddAllocator("RectangleComponent",RectangleComponentConfig::CfgFactory,RectangleComponentConfig::CompFactory);
    table->AddAllocator("RectangleComponent3D",RectangleComponent3DConfig::CfgFactory,RectangleComponent3DConfig::CompFactory);
    table->AddAllocator("ImageComponent",ImageComponentConfig::CfgFactory,ImageComponentConfig::CompFactory);
    table->AddAllocator("AnimationComponent",AnimationComponentConfig::CfgFactory,AnimationComponentConfig::CompFactory);
    table->AddAllocator("ScreenContainer",ScreenContainerConfig::CfgFactory,ScreenContainerConfig::CompFactory);
    MSComponentConfig_Init(table);
    ImageConfig_Init(table);
    FontConfig_Init(table);
    WONControlsConfig_Init(table);
    table->RegisterControlId("Ok",ControlId_Ok);
    table->RegisterControlId("Cancel",ControlId_Cancel);
}
}

// Config factories the registration table calls
namespace WONAPI {

// FUNCTION: WON 0x0079aab0
// Creates a new plain component config for the skin loader
ComponentConfig* ComponentConfig::CfgFactory()
{
    return new ComponentConfig;
}

// FUNCTION: WON 0x0079b090
// Creates a new rectangle component config for the skin loader
ComponentConfig* RectangleComponentConfig::CfgFactory()
{
    return new RectangleComponentConfig;
}

// FUNCTION: WON 0x0079b140
// Creates a new 3D rectangle component config for the skin loader
ComponentConfig* RectangleComponent3DConfig::CfgFactory()
{
    return new RectangleComponent3DConfig;
}

// FUNCTION: WON 0x0079b1f0
// Creates a new image component config for the skin loader
ComponentConfig* ImageComponentConfig::CfgFactory()
{
    return new ImageComponentConfig;
}

// FUNCTION: WON 0x0079b2a0
// Creates a new animation component config for the skin loader
ComponentConfig* AnimationComponentConfig::CfgFactory()
{
    return new AnimationComponentConfig;
}

// FUNCTION: WON 0x0079b350
// Creates a new screen container config for the skin loader
ComponentConfig* ScreenContainerConfig::CfgFactory()
{
    return new ScreenContainerConfig;
}

}

namespace WONAPI {

// Creates a new container config (the linker folds this body into the identical one at 0x00695842)

ComponentConfig* ContainerConfig::CfgFactory()
{
    return new ContainerConfig;
}

// Creates a new dialog config (the linker folds this body into the identical one at 0x0069585c)

ComponentConfig* DialogConfig::CfgFactory()
{
    return new DialogConfig;
}

// FUNCTION: WON 0x0079ad10
// Creates a new image button config for the skin loader

ComponentConfig* ImageButtonConfig::CfgFactory()
{
    return new ImageButtonConfig;
}

// FUNCTION: WON 0x00782150
// Creates a new list area config for the skin loader

ComponentConfig* ListAreaConfig::CfgFactory()
{
    return new ListAreaConfig;
}

// FUNCTION: WON 0x0079ae90
// Creates a new multi-column list area config for the skin loader

ComponentConfig* MultiListAreaConfig::CfgFactory()
{
    return new MultiListAreaConfig;
}

// FUNCTION: WON 0x0079af50
// Creates a new multi-column list header config for the skin loader

ComponentConfig* MultiListHeaderConfig::CfgFactory()
{
    return new MultiListHeaderConfig;
}

// FUNCTION: WON 0x00782100
// Creates a new input box config for the skin loader

ComponentConfig* InputBoxConfig::CfgFactory()
{
    return new InputBoxConfig;
}

// FUNCTION: WON 0x0079b410
// Creates a new splitter config for the skin loader

ComponentConfig* SplitterConfig::CfgFactory()
{
    return new SplitterConfig;
}

// FUNCTION: WON 0x0079b4d0
// Creates a new text area config for the skin loader

ComponentConfig* TextAreaConfig::CfgFactory()
{
    return new TextAreaConfig;
}

// FUNCTION: WON 0x0079b590
// Creates a new tree area config for the skin loader

ComponentConfig* TreeAreaConfig::CfgFactory()
{
    return new TreeAreaConfig;
}

}

namespace WONAPI {
// FUNCTION: WON 0x0078f9e0
// Creates a new label config for the skin loader
ComponentConfig* LabelConfig::CfgFactory()
{
    return new LabelConfig;
}
// FUNCTION: WON 0x0079add0
// Creates a new tab control config for the skin loader
ComponentConfig* TabCtrlConfig::CfgFactory()
{
    return new TabCtrlConfig;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0079ac50
// Creates a new dialog for the skin loader
Component* DialogConfig::CompFactory()
{
    return new Dialog;
}
// FUNCTION: WON 0x0079ab90
// Creates a new input box for the skin loader
Component* InputBoxConfig::CompFactory()
{
    return new InputBox;
}
// FUNCTION: WON 0x0079afd0
// Creates a new label for the skin loader
Component* LabelConfig::CompFactory()
{
    return new Label;
}
// FUNCTION: WON 0x0079ad70
// Creates a new tab control for the skin loader
Component* TabCtrlConfig::CompFactory()
{
    return new TabCtrl;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0079b3b0
// Creates a new splitter for the skin loader
Component* SplitterConfig::CompFactory()
{
    return new Splitter;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00765120
// Creates a new container for the skin loader
Component* ContainerConfig::CompFactory()
{
    return new Container;
}
// FUNCTION: WON 0x0079ab10
// Creates a new layout point component
Component* ContainerConfig::LayoutPointFactory()
{
    return new LayoutPoint;
}
// FUNCTION: WON 0x0079acb0
// Creates a new image button for the skin loader
Component* ImageButtonConfig::CompFactory()
{
    return new ImageButton;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0079abf0
// Creates a new list area for the skin loader
Component* ListAreaConfig::CompFactory()
{
    return new ListArea;
}
// FUNCTION: WON 0x0079ae30
// Creates a new multi-column list area for the skin loader
Component* MultiListAreaConfig::CompFactory()
{
    return new MultiListArea;
}
// FUNCTION: WON 0x0079aef0
// Creates a new multi-column list header for the skin loader
Component* MultiListHeaderConfig::CompFactory()
{
    return new MultiListHeader;
}
// FUNCTION: WON 0x0079b530
// Creates a new tree area for the skin loader
Component* TreeAreaConfig::CompFactory()
{
    return new TreeArea;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079b470
// (public: static class WONAPI::Component * __cdecl WONAPI::TextAreaConfig::CompFactory(void)    [?CompFactory@TextAreaConfig@WONAPI@@SAPAVComponent@2@XZ)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079afb0
// (public: virtual void __thiscall WONAPI::MultiListHeaderConfig::Virtual34(class WONAPI::Component *)    [?Virtual34@MultiListHeaderConfig@WONAPI@@UAEXP)
