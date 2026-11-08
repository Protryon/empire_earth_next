// Certificate storage and virtual methods. The constructors are in the original only;
// this file implements unpack/check methods and destruction
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
    ElGamal mPubKey; //20..1c8, alignment as in the original
public:
    AuthCertificateBase(const void* data = NULL, unsigned long len = 0)
        :AuthBase(data,len),mUserId(0) {} // inline: AuthPeerData 0x6e1b60 expands it; 0x6e5760 is a refused copy
    const ElGamal& GetPubKey() const { return this->mPubKey; } //+20, borrowed
    unsigned long GetUserId() const { return this->mUserId; } //+1c
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const; //slot 8; the base fallback is external
    // Implicit destructor 2dd3d0: six BigInteger+Random members, then AuthBase
};
class AuthCertificate : public AuthCertificateBase {
    unsigned long mCommunityId; //1c8
    unsigned short mTrustLevel; //1cc; natural extent1d0
protected:
    virtual ~AuthCertificate(); //2dcfb0, stores its own vptr, then member/base cleanup
    virtual bool UnpackHook(ReadBuffer&); //2dd120
public:
    AuthCertificate(const void* data = NULL, unsigned long len = 0)
        :AuthCertificateBase(data,len),mCommunityId(0),mTrustLevel(0) { this->Unpack(); } //inline, 2e8214..2e8241
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); } //inline; the original's GetCert reply shares this issue-time minus current-time block
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const; //2dd190 discoverygap
};
typedef std::map<DWORD, unsigned short> CommunityTrustMap;
typedef std::map<std::wstring, std::wstring> NicknameMap;
typedef std::map<DWORD, ByteBufferPtr> UserDataMap;
typedef std::map<DWORD, DWORD> KeyIdMap;
class Auth2Certificate : public AuthCertificateBase {
    std::wstring mUserName; //1c8..1d8
    CommunityTrustMap mCommunityTrustMap; //1d8..1e4
    NicknameMap mNicknameMap; //1e4..1f0
    UserDataMap mUserDataMap; //1f0..1fc
    KeyIdMap mKeyIdMap; //1fc..208, size 208
protected:
    virtual ~Auth2Certificate(); //2dd1d0, member cleanup
    virtual bool UnpackHook(ReadBuffer&); //2dd6b0 discoverygap; code918, table16 excluded
public:
    Auth2Certificate(const void* data = NULL, unsigned long len = 0)
        :AuthCertificateBase(data,len) { this->Unpack(); } //inline, 2e8171..2e81e8
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); } //inline
    DWORD GetFirstCommunity() const; //2ddb50/25, cached count/head/first-key getter
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
