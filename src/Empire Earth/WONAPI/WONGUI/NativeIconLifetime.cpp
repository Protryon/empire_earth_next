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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728990
// (public: __thiscall WONAPI::NativeIcon::NativeIcon(class WONAPI::DisplayContext *)    [??0NativeIcon@WONAPI@@QAE@PAVDisplayContext@1@@Z])
// TEMPLATE: WON 0x007289e0
// (public: virtual void * __thiscall WONAPI::NativeIcon::`scalar deleting destructor'(unsigned int)    [??_GNativeIcon@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00728a00
// (public: virtual __thiscall WONAPI::NativeIcon::~NativeIcon(void)    [??1NativeIcon@WONAPI@@UAE@XZ])
