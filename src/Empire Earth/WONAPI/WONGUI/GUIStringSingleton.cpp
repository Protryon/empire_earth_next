// The shared empty GUIStringData
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
// Empty wide string defined in EEWonNet.cpp
extern wchar_t gEmptyWideString[];
namespace WONAPI {
// Returns the shared empty string data, creating it on first use
GUIStringData* GetEmptyGUIStringData()
{
    static SmartPtr<GUIStringData> aNullStr=new GUIStringData(gEmptyWideString);
    return aNullStr.get();
}
}
