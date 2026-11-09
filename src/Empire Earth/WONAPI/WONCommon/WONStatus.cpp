// WONStatus names (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "platform/PlatSocket.h"
// The socket error codes WON reports are Winsock's; the platform layer reports them in that numbering everywhere
static_assert(WS_WSAEINTR == PLAT_SOCK_EINTR, "Winsock numbering");
static_assert(WS_WSAEBADF == PLAT_SOCK_EBADF, "Winsock numbering");
static_assert(WS_WSAEACCES == PLAT_SOCK_EACCES, "Winsock numbering");
static_assert(WS_WSAEFAULT == PLAT_SOCK_EFAULT, "Winsock numbering");
static_assert(WS_WSAEINVAL == PLAT_SOCK_EINVAL, "Winsock numbering");
static_assert(WS_WSAEMFILE == PLAT_SOCK_EMFILE, "Winsock numbering");
static_assert(WS_WSAEWOULDBLOCK == PLAT_SOCK_EWOULDBLOCK, "Winsock numbering");
static_assert(WS_WSAEINPROGRESS == PLAT_SOCK_EINPROGRESS, "Winsock numbering");
static_assert(WS_WSAEALREADY == PLAT_SOCK_EALREADY, "Winsock numbering");
static_assert(WS_WSAENOTSOCK == PLAT_SOCK_ENOTSOCK, "Winsock numbering");
static_assert(WS_WSAEDESTADDRREQ == PLAT_SOCK_EDESTADDRREQ, "Winsock numbering");
static_assert(WS_WSAEMSGSIZE == PLAT_SOCK_EMSGSIZE, "Winsock numbering");
static_assert(WS_WSAEPROTOTYPE == PLAT_SOCK_EPROTOTYPE, "Winsock numbering");
static_assert(WS_WSAENOPROTOOPT == PLAT_SOCK_ENOPROTOOPT, "Winsock numbering");
static_assert(WS_WSAEPROTONOSUPPORT == PLAT_SOCK_EPROTONOSUPPORT, "Winsock numbering");
static_assert(WS_WSAESOCKTNOSUPPORT == PLAT_SOCK_ESOCKTNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEOPNOTSUPP == PLAT_SOCK_EOPNOTSUPP, "Winsock numbering");
static_assert(WS_WSAEPFNOSUPPORT == PLAT_SOCK_EPFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEAFNOSUPPORT == PLAT_SOCK_EAFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEADDRINUSE == PLAT_SOCK_EADDRINUSE, "Winsock numbering");
static_assert(WS_WSAEADDRNOTAVAIL == PLAT_SOCK_EADDRNOTAVAIL, "Winsock numbering");
static_assert(WS_WSAENETDOWN == PLAT_SOCK_ENETDOWN, "Winsock numbering");
static_assert(WS_WSAENETUNREACH == PLAT_SOCK_ENETUNREACH, "Winsock numbering");
static_assert(WS_WSAENETRESET == PLAT_SOCK_ENETRESET, "Winsock numbering");
static_assert(WS_WSAECONNABORTED == PLAT_SOCK_ECONNABORTED, "Winsock numbering");
static_assert(WS_WSAECONNRESET == PLAT_SOCK_ECONNRESET, "Winsock numbering");
static_assert(WS_WSAENOBUFS == PLAT_SOCK_ENOBUFS, "Winsock numbering");
static_assert(WS_WSAEISCONN == PLAT_SOCK_EISCONN, "Winsock numbering");
static_assert(WS_WSAENOTCONN == PLAT_SOCK_ENOTCONN, "Winsock numbering");
static_assert(WS_WSAESHUTDOWN == PLAT_SOCK_ESHUTDOWN, "Winsock numbering");
static_assert(WS_WSAETOOMANYREFS == PLAT_SOCK_ETOOMANYREFS, "Winsock numbering");
static_assert(WS_WSAETIMEDOUT == PLAT_SOCK_ETIMEDOUT, "Winsock numbering");
static_assert(WS_WSAECONNREFUSED == PLAT_SOCK_ECONNREFUSED, "Winsock numbering");
static_assert(WS_WSAELOOP == PLAT_SOCK_ELOOP, "Winsock numbering");
static_assert(WS_WSAENAMETOOLONG == PLAT_SOCK_ENAMETOOLONG, "Winsock numbering");
static_assert(WS_WSAEHOSTDOWN == PLAT_SOCK_EHOSTDOWN, "Winsock numbering");
static_assert(WS_WSAEHOSTUNREACH == PLAT_SOCK_EHOSTUNREACH, "Winsock numbering");
static_assert(WS_WSAENOTEMPTY == PLAT_SOCK_ENOTEMPTY, "Winsock numbering");
static_assert(WS_WSAEPROCLIM == PLAT_SOCK_EPROCLIM, "Winsock numbering");
static_assert(WS_WSAEUSERS == PLAT_SOCK_EUSERS, "Winsock numbering");
static_assert(WS_WSAEDQUOT == PLAT_SOCK_EDQUOT, "Winsock numbering");
static_assert(WS_WSAESTALE == PLAT_SOCK_ESTALE, "Winsock numbering");
static_assert(WS_WSAEREMOTE == PLAT_SOCK_EREMOTE, "Winsock numbering");
static_assert(WS_WSASYSNOTREADY == PLAT_SOCK_SYSNOTREADY, "Winsock numbering");
static_assert(WS_WSAVERNOTSUPPORTED == PLAT_SOCK_VERNOTSUPPORTED, "Winsock numbering");
static_assert(WS_WSANOTINITIALISED == PLAT_SOCK_NOTINITIALISED, "Winsock numbering");
static_assert(WS_WSAEDISCON == PLAT_SOCK_EDISCON, "Winsock numbering");
namespace WONAPI {
// Formatter storage for unnamed status codes.
char gWONStatusBuffer[1024];
// FUNCTION: WON 0x0069deb0
// Returns the WS_ name of a status code; unnamed codes are formatted as numbers into a shared buffer
const char* WONStatusToString(WONStatus inStatus)
{
    // the names are Rust's table (rust/crates/ee_won/src/status.rs, printed from this unit's former switch)
    return ee_won_status_name(inStatus,gWONStatusBuffer,sizeof(gWONStatusBuffer));
}
} // namespace WONAPI
