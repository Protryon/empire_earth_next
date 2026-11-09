// The auth certificates. Their fields and permission checks are ee_won's (rust/crates/ee_won/src/auth.rs); the
// classes keep what their inline getters return.
#ifndef EE_WONAPI_WONAUTHCERTIFICATE_H
#define EE_WONAPI_WONAUTHCERTIFICATE_H
#include <map>
#include <string>
#include "ee/wonapi/WONAuthBase.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
class AuthCertificateBase : public AuthBase {
protected:
    unsigned long mUserId; //1c
    ElGamal mPubKey; //20
    virtual void UnpackHook(); // the user id and the public key
public:
    AuthCertificateBase(const void* data = NULL, unsigned long len = 0)
        :AuthBase(data,len),mUserId(0) {} // inline: AuthPeerData 0x6e1b60 expands it; 0x6e5760 is a refused copy
    const ElGamal& GetPubKey() const { return this->mPubKey; } //+20, borrowed
    unsigned long GetUserId() const { return this->mUserId; } //+1c
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const; //slot 8; the base fallback is external
};
class AuthCertificate : public AuthCertificateBase {
protected:
    virtual ~AuthCertificate(); //2dcfb0
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_CERTIFICATE; }
public:
    AuthCertificate(const void* data = NULL, unsigned long len = 0)
        :AuthCertificateBase(data,len) { this->Unpack(); } //inline, 2e8214..2e8241
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); } //inline
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const; //2dd190 discoverygap
};
class Auth2Certificate : public AuthCertificateBase {
protected:
    virtual ~Auth2Certificate(); //2dd1d0
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_CERTIFICATE2; }
public:
    Auth2Certificate(const void* data = NULL, unsigned long len = 0)
        :AuthCertificateBase(data,len) { this->Unpack(); } //inline, 2e8171..2e81e8
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); } //inline
    DWORD GetFirstCommunity() const; //2ddb50/25
    std::wstring GetNickname(const std::wstring&) const;
    ByteBufferPtr GetUserData(DWORD) const;
    DWORD GetKeyId(DWORD) const;
    std::wstring GetFirstNickname() const;
    unsigned short GetFirstCommunityTrust() const;
    ByteBufferPtr GetFirstUserData() const;
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const; //2dda60
};
}
#endif
