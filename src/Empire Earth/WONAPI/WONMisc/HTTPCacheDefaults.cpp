// HTTPCache default hooks whose bodies the linker folded into identical functions elsewhere
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// Same body as 0x6fa500 in the original (folded by the linker)
void HTTPCache::UpdateEntry(HTTPCacheEntry*) {}
}
