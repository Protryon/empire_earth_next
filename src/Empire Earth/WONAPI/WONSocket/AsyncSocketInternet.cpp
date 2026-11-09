// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006b09a0
// Whether the machine is online. The original asked wininet's InternetGetConnectedState (true when wininet.dll was
// missing); on every system now: an up network adapter with an IPv4 address that is neither 0.0.0.0 nor loopback
// (rust/crates/ee_won/src/ipaddr.rs)
bool AsyncSocket::HasInternetConnection()
{
    return ee_won_has_internet_connection();
}
}

// Compiler-generated for this unit:
// SYNTHETIC: WON 0x006b0a30
// (_$E45: atexit destructor of HasInternetConnection's static anUnload; gone with the wininet lookup)
