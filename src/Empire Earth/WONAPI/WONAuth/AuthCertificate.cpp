// AuthCertificate: version 1 auth certificate (user, community, trust level, public key), unpacked by ee_won
// (rust/crates/ee_won/src/auth.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthCertificate.h"
namespace WONAPI {
// FUNCTION: WON 0x006dcfb0
// Destroys the certificate
AuthCertificate::~AuthCertificate() {}
// FUNCTION: WON 0x006dd190
// Checks the user and community ids (0 = any) and that the trust level is at least the given minimum
bool AuthCertificate::VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
{
    return this->GetParsed() != NULL && ee_won_auth_verify_permissions(this->GetParsed(), userId, community, minTrust);
}
}
