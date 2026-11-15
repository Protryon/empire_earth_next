
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
namespace WONAPI {
// Creates a component configuration with no width or height set and no children initialised
ComponentConfig::ComponentConfig()
{
    this->mHasSetWidth=false;
    this->mHasSetHeight=false;
    this->mHaveInitedComponents=false;
}
// Destroys the component configuration (members only)
ComponentConfig::~ComponentConfig()
{
}
// Creates a container configuration with no layout reference and layout id 0
ContainerConfig::ContainerConfig()
{
    this->mLayoutRef=NULL;
    this->mCurLayoutId=0;
}
}

namespace WONAPI {
// Class statics: the shared resource configuration table and the resource folder
SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;
std::string ComponentConfig::mResourceFolder;
}
