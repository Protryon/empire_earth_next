// The auth certificates (AuthCertificate, Auth2Certificate), the auth servers' key block (AuthPubKeyBlock) and the
// peer data (AuthPeerData, the original's 0x1c8 bytes: the certificates, the key block, the private key and the
// server clock delta) are ee_won's objects (rust/crates/ee_won/src/peerobj.rs: the bytes, the unpacked fields and
// keys, every check). These classes are RefCount shims holding one reference to one, so the ops' host calls and
// the SmartPtrs hold them as before; a certificate's kind is the EE_WON_AUTH_* it was made as.
#ifndef EE_WONAPI_WONAUTHPEERDATA_H
#define EE_WONAPI_WONAUTHPEERDATA_H
#include <time.h>
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
class AuthBase : public RefCount {
    EeWonAuthObj* mImpl;
    AuthBase(const AuthBase&);
    AuthBase& operator=(const AuthBase&);
protected:
    virtual ~AuthBase() { ee_won_authobj_free(this->mImpl); }
public:
    explicit AuthBase(EeWonAuthObj* inImpl) : mImpl(inImpl) {}
    // new AuthCertificate(data, len) (EE_WON_AUTH_CERTIFICATE), Auth2Certificate or AuthPubKeyBlock
    AuthBase(uint32_t inKind, const void* inData, unsigned long inLength)
        : mImpl(ee_won_cert_new(inKind, static_cast<const uint8_t*>(inData), inLength)) {}
    const EeWonAuthObj* Impl() const { return this->mImpl; }
    // a number (EE_WON_AUTHOBJ_*: IsValid, the times, the user id, IsExpired, VerifyPermissions, the key block's id,
    // the peer data's checks)
    long long Num(int inWhich, long long inA = 0, long long inB = 0, long long inC = 0) const
    {
        return ee_won_authobj_num(this->mImpl, inWhich, inA, inB, inC);
    }
    bool IsValid() const { return this->Num(EE_WON_AUTHOBJ_VALID) != 0; }
    time_t GetIssueTime() const { return (time_t)this->Num(EE_WON_AUTHOBJ_ISSUE); }
    time_t GetExpireTime() const { return (time_t)this->Num(EE_WON_AUTHOBJ_EXPIRE); }
    bool VerifyPermissions(DWORD inUser, DWORD inCommunity, unsigned short inTrust) const
    {
        return this->Num(EE_WON_AUTHOBJ_PERMITS, inUser, inCommunity, inTrust) != 0;
    }
    // a key block's (or peer data's) Verify(other)
    bool Verify(const AuthBase* inOther) const { return inOther != NULL && ee_won_authobj_verify(this->mImpl, inOther->mImpl); }
    bool IsValidAndVerified(const ElGamal& inKey) const { return ee_won_authobj_verified_by(this->mImpl, inKey.Raw()); }
    // the first key's Encrypt (a certificate's public key, a key block's current key); NULL when it cannot
    ByteBufferPtr Encrypt(const void* inData, int inLength) const
    {
        ByteBufferPtr anOut;
        ee_won_authobj_encrypt(this->mImpl, static_cast<const uint8_t*>(inData), inLength > 0 ? inLength : 0,
                               WonByteBufferSink, &anOut);
        return anOut;
    }
};

class AuthPeerData : public AuthBase {
    ConstSmartPtr<AuthBase> mKeyBlock; // the key block's shim (GetPubKeyBlock is borrowed)
    AuthPeerData(EeWonAuthObj* inImpl, int)
        : AuthBase(inImpl), mKeyBlock(KeyBlockOf(inImpl)) {}
    static AuthBase* KeyBlockOf(const EeWonAuthObj* inImpl)
    {
        EeWonAuthObj* aBlock = ee_won_authobj_get(inImpl, EE_WON_AUTHOBJ_PEER_BLOCK);
        return aBlock != NULL ? new AuthBase(aBlock) : NULL;
    }
protected:
    virtual ~AuthPeerData() {}
public:
    // AuthPeerData(keyBlock): empty certificates and private key (NULL: a new empty key block)
    explicit AuthPeerData(const AuthBase* inKeyBlock = NULL)
        : AuthBase(ee_won_peer_new_empty(inKeyBlock != NULL ? inKeyBlock->Impl() : NULL)),
          mKeyBlock(new AuthBase(ee_won_authobj_get(this->Impl(), EE_WON_AUTHOBJ_PEER_BLOCK))) {}
    AuthPeerData(const AuthBase* inCert, const AuthBase* inKeyBlock, const ElGamal& inKey, time_t inDelta,
                 const AuthBase* inCert2 = NULL)
        : AuthBase(ee_won_peer_new(inCert != NULL ? inCert->Impl() : NULL, inKeyBlock != NULL ? inKeyBlock->Impl() : NULL,
                                   inKey.Raw(), (int64_t)inDelta, inCert2 != NULL ? inCert2->Impl() : NULL)),
          mKeyBlock(inKeyBlock) {}
    // a shim adopting a reference to Rust peer data (NULL: none), as AuthContext::GetPeerData hands it out
    static AuthPeerData* Adopt(EeWonAuthObj* inImpl)
    {
        return inImpl != NULL ? new AuthPeerData(inImpl, 0) : NULL;
    }
    const AuthBase* GetPubKeyBlock() const { return this->mKeyBlock.get(); }
    time_t GetAuthDelta() const { return (time_t)this->Num(EE_WON_AUTHOBJ_PEER_DELTA); }
    bool CertificateExpired(time_t inExtra = 0) const { return this->Num(EE_WON_AUTHOBJ_PEER_EXPIRED, inExtra) != 0; }
    bool Certificate2Expired(time_t inExtra = 0) const { return this->Num(EE_WON_AUTHOBJ_PEER_EXPIRED2, inExtra) != 0; }
    time_t GetExpireDelta() const { return (time_t)this->Num(EE_WON_AUTHOBJ_PEER_EXPIRE_DELTA); }
    unsigned long GetUserId() const { return (unsigned long)this->Num(EE_WON_AUTHOBJ_USER_ID); } // the certificate's
    DWORD GetFirstCommunity() const { return (DWORD)this->Num(EE_WON_AUTHOBJ_FIRST_COMMUNITY); } // the second's
};
}
#endif
