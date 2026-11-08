// WON SDK NetStats static definitions (socket counts and byte/message totals)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
CriticalSection NetStats::mDataCrit;
bool NetStats::mEnabled = false;
int NetStats::mNumConstructedSockets = 0;
int NetStats::mNumOpenSockets = 0;
__int64 NetStats::mNumBytesSent = 0;
__int64 NetStats::mNumBytesReceived = 0;
__int64 NetStats::mNumMessagesSent = 0;
__int64 NetStats::mNumMessagesReceived = 0;
}

// Compiler-generated for this unit:
// SYNTHETIC: WON 0x006b38e0
// (_$E48)
// SYNTHETIC: WON 0x006b38f0
// (_$E44: mDataCrit constructor)
// SYNTHETIC: WON 0x006b3900
// (_$E47: registers _$E45 with atexit)
// SYNTHETIC: WON 0x006b3910
// (_$E45: mDataCrit destructor)
