// Duplicate of a Windows-skin image, used when skin frames are deep-copied
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
#include "ee/wonapi/WONLobbyMSSkinImage.h"
namespace WONAPI {
// FUNCTION: WON 0x00715a10
// Returns a new skin image of the same type, flags and size
SmartPtr<Image> WONMSSkin::MSImage::Duplicate()
{
    return new MSImage(this->mImageType,this->mImageFlags,this->mWidth,this->mHeight);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00715ad0
// (public: virtual __thiscall WONAPI::WONMSSkin::MSImage::~MSImage(void)    [??1MSImage@WONMSSkin@WONAPI@@UAE@XZ])
