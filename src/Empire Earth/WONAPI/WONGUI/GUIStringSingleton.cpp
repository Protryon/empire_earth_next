// The shared empty GUIStringData and the empty GUIString (GUIStringData's edits are ee_won's guistrobj.rs, its reads
// guistring.rs; the class is inline in WONLobbyResource.h)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stddef.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
// Empty wide string defined in EEWonNet.cpp
extern wchar_t gEmptyWideString[];
namespace WONAPI {
// guistrobj.rs edits the three members as one block (EeWonGuiStrBuf: the text, the length, the capacity)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(GUIStringData, mLength) == offsetof(GUIStringData, mText) + offsetof(EeWonGuiStrBuf, length) &&
              offsetof(GUIStringData, mCapacity) == offsetof(GUIStringData, mText) + offsetof(EeWonGuiStrBuf, capacity),
              "guistrobj.rs EeWonGuiStrBuf");
// ... read in place at this offset (guistrobj.rs data_units, EE_WON_GSTR_DATA_AT)
static_assert(offsetof(GUIStringData, mText) == 2 * sizeof(void*), "guistrobj.rs EE_WON_GSTR_DATA_AT");
#pragma clang diagnostic pop
// Returns the shared empty string data, creating it on first use
GUIStringData* GetEmptyGUIStringData()
{
    static SmartPtr<GUIStringData> aNullStr=new GUIStringData(gEmptyWideString);
    return aNullStr.get();
}
// The global empty GUIString (_$E31; was GUIStringDataMutations.cpp's)
GUIString gEmptyGUIString;
}
