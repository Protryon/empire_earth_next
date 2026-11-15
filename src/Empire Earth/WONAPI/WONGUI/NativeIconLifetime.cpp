// Native icon constructor and destructor (file name inferred)
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
// Creates a native icon bound to a display context (emitted at 0x00728990)
NativeIcon::NativeIcon(DisplayContext* context) : mContext(context) {}
// Destroys the native icon (members only; emitted at 0x00728a00)
NativeIcon::~NativeIcon() {}
}
