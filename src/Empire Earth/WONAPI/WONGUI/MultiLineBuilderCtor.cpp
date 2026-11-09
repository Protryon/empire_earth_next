// Rich-text line builder constructor (file name inferred)
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
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// Creates an empty line builder (members only; emitted at 0x0070bd60)
MultiLineBuilder::MultiLineBuilder() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070bd60
// (public: __thiscall WONAPI::MultiLineBuilder::MultiLineBuilder(void)    [??0MultiLineBuilder@WONAPI@@QAE@XZ])
