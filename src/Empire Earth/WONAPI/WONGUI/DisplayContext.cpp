// DisplayContext image factory
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x007cedd0
// Creates a blank Windows bitmap image of the given size for this display context
SmartPtr<NativeImage> DisplayContext::CreateImage(int width,int height)
{
    return new MSNativeImage(this,width,height);
}
}
