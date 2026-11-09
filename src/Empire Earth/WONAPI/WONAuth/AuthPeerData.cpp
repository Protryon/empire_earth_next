// AuthPeerData: the client's certificates, public key block, private key and server clock delta (0x1c8 bytes)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthPeerData.h"
namespace WONAPI {
// FUNCTION: WON 0x006e1b60
// Creates peer data with empty certificates and the given key block (a new empty one when NULL)
AuthPeerData::AuthPeerData(const AuthPubKeyBlock* keyBlock)
    :mCertificate(new AuthCertificate),mCertificate2(new Auth2Certificate),
     mKeyBlock(keyBlock),mAuthDelta(0)
{
    if(this->mKeyBlock.get()==NULL) this->mKeyBlock=new AuthPubKeyBlock;
}
// FUNCTION: WON 0x006e1ee0
// Creates peer data from received certificates, key block, private key and server clock delta
AuthPeerData::AuthPeerData(const AuthCertificate* certificate,const AuthPubKeyBlock* keyBlock,
    const ElGamal& key,time_t delta,const Auth2Certificate* certificate2)
    :mCertificate(certificate),mCertificate2(certificate2),mKeyBlock(keyBlock),
     mPrivateKey(key),mAuthDelta(delta)
{
}

// FUNCTION: WON 0x006e2140
// Destroys the peer data, releasing its certificates and keys
AuthPeerData::~AuthPeerData() {}
// FUNCTION: WON 0x006e2290
// Verifies an auth block's signature against the public key block
bool AuthPeerData::Verify(const AuthBase* base) const { return this->mKeyBlock->Verify(base); }
// FUNCTION: WON 0x006e22a0
// Returns whether the certificate expires within the given seconds, in server time
bool AuthPeerData::CertificateExpired(time_t extra) const { return this->mCertificate->IsExpired(extra+this->mAuthDelta); }
// FUNCTION: WON 0x006e22c0
// Returns whether the auth2 certificate expires within the given seconds, in server time
bool AuthPeerData::Certificate2Expired(time_t extra) const { return this->mCertificate2->IsExpired(extra+this->mAuthDelta); }
// FUNCTION: WON 0x006e22e0
// Returns whether the key block and certificate are valid and a private key is held
bool AuthPeerData::IsValid() const
{
    return this->mKeyBlock->IsValid() && this->mCertificate->IsValid() && this->mPrivateKey.IsPrivate();
}
// FUNCTION: WON 0x006e2310
// Returns the seconds until the certificate expires, 0 when already expired
time_t AuthPeerData::GetExpireDelta() const
{
    if(this->CertificateExpired()) return 0;
    return this->mCertificate->GetExpireTime()-this->mAuthDelta-time(NULL);
}
}

namespace WONAPI {


bool AuthPeerData::IsValid2() const
{
	return (this->mKeyBlock->IsValid() && this->mCertificate2->IsValid() && this->mPrivateKey.IsPrivate());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e1ea0
// (protected: virtual void * __thiscall WONAPI::AuthPubKeyBlock::`scalar deleting destructor'(unsigned int)    [??_GAuthPubKeyBlock@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006e1ec0
// (protected: virtual void * __thiscall WONAPI::AuthPeerData::`scalar deleting destructor'(unsigned int)    [??_GAuthPeerData@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e1df0
// (public: __thiscall std::map<unsigned long,unsigned long,struct std::less<unsigned long>,class std::allocator<struct std::pair<unsigned long const ,uns)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e1dd0
// (public: virtual void * __thiscall WONAPI::AuthCertificateBase::`scalar deleting destructor'(unsigned int)    [??_GAuthCertificateBase@WONAPI@@UAEPAXI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9850
// (public: __thiscall std::list<class WONAPI::ElGamal,class std::allocator<class WONAPI::ElGamal> >::~list<class WONAPI::ElGamal,class std::allocator<cla)

// Compiler-generated and header-inline functions this unit emits:
