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
// Registers the base GUI component types and their config factories, then the MS, image, font and WON
// control types and the Ok and Cancel control ids
void WONGUIConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Component",NewConfigOf<ComponentConfig>,NewComponentOf<Component>);
    table->AddAllocator("LayoutPoint",NewConfigOf<ComponentConfig>,NewComponentOf<LayoutPoint>);
    table->AddAllocator("Container",NewConfigOf<ContainerConfig>,NewComponentOf<Container>);
    table->AddAllocator("Dialog",NewConfigOf<DialogConfig>,NewComponentOf<Dialog>);
    table->AddAllocator("ImageButton",NewConfigOf<ImageButtonConfig>,NewComponentOf<ImageButton>);
    table->AddAllocator("Label",NewConfigOf<LabelConfig>,NewComponentOf<Label>);
    table->AddAllocator("ListArea",NewConfigOf<ListAreaConfig>,NewComponentOf<ListArea>);
    table->AddAllocator("MultiListArea",NewConfigOf<MultiListAreaConfig>,NewComponentOf<MultiListArea>);
    table->AddAllocator("MultiListHeader",NewConfigOf<MultiListHeaderConfig>,NewComponentOf<MultiListHeader>);
    table->AddAllocator("InputBox",NewConfigOf<InputBoxConfig>,NewComponentOf<InputBox>);
    table->AddAllocator("Splitter",NewConfigOf<SplitterConfig>,NewComponentOf<Splitter>);
    table->AddAllocator("TextArea",NewConfigOf<TextAreaConfig>,NewComponentOf<TextArea>);
    table->AddAllocator("TabCtrl",NewConfigOf<TabCtrlConfig>,NewComponentOf<TabCtrl>);
    table->AddAllocator("TreeArea",NewConfigOf<TreeAreaConfig>,NewComponentOf<TreeArea>);
    table->AddAllocator("RectangleComponent",NewConfigOf<RectangleComponentConfig>,NewComponentOf<RectangleComponent>);
    table->AddAllocator("RectangleComponent3D",NewConfigOf<RectangleComponent3DConfig>,NewComponentOf<RectangleComponent3D>);
    table->AddAllocator("ImageComponent",NewConfigOf<ImageComponentConfig>,NewComponentOf<ImageComponent>);
    table->AddAllocator("ScreenContainer",NewConfigOf<ScreenContainerConfig>,NewComponentOf<ScreenContainer>);
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






}

namespace WONAPI {

// Creates a new container config (the linker folds this body into the identical one at 0x00695842)


// Creates a new dialog config (the linker folds this body into the identical one at 0x0069585c)










}

namespace WONAPI {
}

namespace WONAPI {
}

namespace WONAPI {
}

namespace WONAPI {
}

namespace WONAPI {
}
