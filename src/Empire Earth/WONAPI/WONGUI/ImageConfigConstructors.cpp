// Image configuration constructors
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
#include "ee/wonapi/WONLobbyConfigImages.h"
namespace WONAPI {
// Creates an image configuration, marked as having no child components to create
ImageConfig::ImageConfig()
{
    this->mHaveInitedComponents=true;
}
// Creates an image filter configuration with no pending raw image
ImageFilterConfig::ImageFilterConfig()
{
    this->mImageIsRawImage=false;
}
}
