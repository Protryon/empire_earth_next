// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "platform/PlatSocket.h"
namespace WONAPI {
// FUNCTION: WON 0x006b09a0
// Whether the machine is online. The original asked wininet's InternetGetConnectedState (true when wininet.dll was
// missing); on every system now: an up network adapter with an IPv4 address that is neither 0.0.0.0 nor loopback
bool AsyncSocket::HasInternetConnection()
{
    PlatNetAdapter anAdapters[64];
    uint32_t aCount=PlatSocketGetAdapters(anAdapters,64);
    for(uint32_t i=0;i<aCount;++i)
    {
        uint32_t aHost=PlatNetToHost32(anAdapters[i].mAddress);
        if(aHost!=0 && (aHost>>24)!=127)
            return true;
    }
    return false;
}
}

// Compiler-generated for this unit:
// SYNTHETIC: WON 0x006b0a30
// (_$E45: atexit destructor of HasInternetConnection's static anUnload; gone with the wininet lookup)
