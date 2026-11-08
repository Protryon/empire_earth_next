// Palette destructor (file name inferred)
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
// Destroys the palette, freeing its color table (emitted at 0x00709880)
Palette::~Palette() { delete this->mColors; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00709880
// (public: virtual __thiscall WONAPI::Palette::~Palette(void)    [??1Palette@WONAPI@@UAE@XZ])
