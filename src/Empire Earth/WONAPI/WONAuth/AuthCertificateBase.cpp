// AuthCertificateBase: base class of the WON auth certificates; the original's entry is at 0x2e5760
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthCertificate.h"
namespace WONAPI {
// FUNCTION: WON 0x006e1dc0
// Grants no permissions (base class default; always returns false)
bool AuthCertificateBase::VerifyPermissions(DWORD,DWORD,unsigned short) const
{
    return false;
}
}
