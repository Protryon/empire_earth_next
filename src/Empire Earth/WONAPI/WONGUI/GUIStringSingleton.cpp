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
extern unsigned short gEmptyWideString[];
namespace WONAPI {
// FUNCTION: WON 0x00728450
// Returns the shared empty string data, creating it on first use
GUIStringData* GetEmptyGUIStringData()
{
    static SmartPtr<GUIStringData> aNullStr=new GUIStringData(gEmptyWideString);
    return aNullStr.get();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728500
// (_$E31)
// TEMPLATE: WON 0x00728510
// (public: __thiscall WONAPI::SmartPtr<class WONAPI::GUIStringData>::~SmartPtr<class WONAPI::GUIStringData>(void))
