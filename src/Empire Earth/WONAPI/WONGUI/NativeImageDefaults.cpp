// Native image default icon and raw-pixel accessors (file name inferred)
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
// FUNCTION: WON 0x0072b9d0
// Returns no icon (base native images cannot make one)
SmartPtr<NativeIcon> NativeImage::GetIcon() { return NULL; }
// FUNCTION: WON 0x00728bd0
// Returns no raw 32-bit copy (base native images cannot make one)
SmartPtr<RawImage32> NativeImage::GetRaw() { return NULL; }
}
