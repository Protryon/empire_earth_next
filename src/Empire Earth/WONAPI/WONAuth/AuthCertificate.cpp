// AuthCertificate: version 1 auth certificate (user, community, trust level, public key)
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
// Destroys the certificate, releasing its public key
AuthCertificate::~AuthCertificate() {}
// FUNCTION: WON 0x006dd120
// Reads the user id, community id, trust level and public key; false when the key is invalid
bool AuthCertificate::UnpackHook(ReadBuffer& input)
{
    if (!AuthBase::UnpackHook(input))
        return false;
    this->mUserId = input.ReadLong();
    this->mCommunityId = input.ReadLong();
    this->mTrustLevel = input.ReadShort();
    unsigned short keyLength = input.ReadShort();
    if (!this->mPubKey.SetPublicKey(input.ReadBytes(keyLength), keyLength))
        return false;
    return true;
}
// FUNCTION: WON 0x006dd190
// Checks the user and community ids (0 = any) and that the trust level is at least the given minimum
bool AuthCertificate::VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
{
    if (userId != 0 && this->mUserId != userId)
        return false;
    if (community != 0 && this->mCommunityId != community)
        return false;
    return this->mTrustLevel >= minTrust;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dd100
// (protected: virtual void * __thiscall WONAPI::AuthCertificate::`scalar deleting destructor'(unsigned int)    [??_GAuthCertificate@WONAPI@@MAEPAXI@Z])
