// The original's certificate classes, peer data and peer authentication objects, copied from WONAuth's
// AuthBase.cpp, AuthCertificate.cpp, AuthCertificateBase.cpp, Auth2Certificate.cpp, AuthPubKeyBlock.cpp,
// AuthPeerData.cpp, PeerAuthClient.cpp, PeerAuthServer.cpp and their headers (WONAuthBase.h, WONAuthCertificate.h,
// WONAuthPubKeyBlock.h, WONAuthPeerData.h, WONRouting.h's PeerAuthClient and PeerAuthServer) before round 14 of
// the port moved them into ee_won (peerobj.rs), in namespace orig: the original half of
// tests/won/won_peerobj_test.cpp. The handshake is won_peerauth_msg.h's template over ee_won's peerauth module, as
// the classes ran it; the unpacking is ee_won's auth module, as it was.
#pragma once
#include <time.h>
#include <list>
#include <string>
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONPeerAuthMsg.h"

namespace orig {
using namespace WONAPI;

class AuthBase : public RefCount {
    ByteBufferPtr mRawBuf;
    unsigned short mDataLen;
    unsigned short mFamily;
    time_t mIssueTime;
    time_t mExpireTime;
    bool mIsValid;
    EeWonAuth* mParsed;
protected:
    virtual ~AuthBase() { ee_won_auth_free(this->mParsed); }
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_BASE; }
    virtual void UnpackHook() {}
    bool Unpack()
    {
        ee_won_auth_free(this->mParsed);
        this->mParsed = NULL;
        this->mHeader = EeWonAuthHeader();
        if (this->mRawBuf.get() != NULL) {
            this->mParsed = ee_won_auth_unpack(this->UnpackKind(), reinterpret_cast<const uint8_t*>(this->mRawBuf->data()),
                                               this->mRawBuf->length());
            ee_won_auth_header(this->mParsed, &this->mHeader);
        }
        this->mFamily = this->mHeader.family;
        this->mIssueTime = this->mHeader.issue_time;
        this->mExpireTime = this->mHeader.expire_time;
        this->mDataLen = this->mHeader.data_len;
        this->mIsValid = this->mHeader.valid;
        if (this->mParsed != NULL)
            this->UnpackHook();
        return this->mIsValid;
    }
    const EeWonAuthHeader& GetHeader() const { return this->mHeader; }
    EeWonAuthHeader mHeader;
public:
    AuthBase(const void* data = NULL, unsigned long length = 0)
        : mDataLen(0), mFamily(0), mIssueTime(0), mExpireTime(0), mIsValid(false), mParsed(NULL), mHeader()
    {
        if (data != NULL && length != 0)
            this->mRawBuf = new ByteBuffer(data, length, false);
    }
    ByteBufferPtr GetRawBuf() const { return this->mRawBuf; }
    const void* GetRaw() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; }
    unsigned long GetRawLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length() : 0; }
    bool IsValid() const { return this->mIsValid; }
    const void* GetData() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; }
    unsigned long GetDataLen() const { return this->mRawBuf.get()!=NULL ? this->mDataLen : 0; }
    const void* GetSig() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data()+this->mDataLen : NULL; }
    unsigned long GetSigLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length()-this->mDataLen : 0; }
    time_t GetIssueTime() const { return this->mIssueTime; }
    time_t GetExpireTime() const { return this->mExpireTime; }
    unsigned short GetFamily() const { return this->mFamily; }
    bool IsExpired(time_t extra = 0) const
    {
        if (!this->mIsValid || this->mParsed == NULL)
            return true;
        return ee_won_auth_is_expired(this->mParsed, static_cast<int64_t>(time(NULL)), static_cast<int64_t>(extra));
    }
    bool Verify(const ElGamal& key) const
    {
        return key.Verify(this->GetData(), this->GetDataLen(), this->GetSig(), this->GetSigLen());
    }
    const EeWonAuth* GetParsed() const { return this->mParsed; }
};

class AuthCertificateBase : public AuthBase {
protected:
    unsigned long mUserId;
    ElGamal mPubKey;
    virtual void UnpackHook()
    {
        this->mUserId = this->GetHeader().user_id;
        this->mPubKey.Adopt(ee_won_auth_key(this->GetParsed(), 0));
    }
public:
    AuthCertificateBase(const void* data = NULL, unsigned long len = 0) : AuthBase(data, len), mUserId(0) {}
    const ElGamal& GetPubKey() const { return this->mPubKey; }
    unsigned long GetUserId() const { return this->mUserId; }
    virtual bool VerifyPermissions(DWORD, DWORD, unsigned short) const { return false; }
};

class AuthCertificate : public AuthCertificateBase {
protected:
    virtual ~AuthCertificate() {}
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_CERTIFICATE; }
public:
    AuthCertificate(const void* data = NULL, unsigned long len = 0) : AuthCertificateBase(data, len) { this->Unpack(); }
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); }
    virtual bool VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
    {
        return this->GetParsed() != NULL && ee_won_auth_verify_permissions(this->GetParsed(), userId, community, minTrust);
    }
};

class Auth2Certificate : public AuthCertificateBase {
protected:
    virtual ~Auth2Certificate() {}
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_CERTIFICATE2; }
public:
    Auth2Certificate(const void* data = NULL, unsigned long len = 0) : AuthCertificateBase(data, len) { this->Unpack(); }
    time_t ComputeAuthDelta() const { return this->GetIssueTime() - time(NULL); }
    DWORD GetFirstCommunity() const
    {
        return this->GetParsed() != NULL ? ee_won_auth_cert2_query(this->GetParsed(), EeWonCert2Query::FirstCommunity, 0) : 0;
    }
    DWORD GetKeyId(DWORD theCommunityId) const
    {
        return this->GetParsed()!=NULL ? ee_won_auth_cert2_query(this->GetParsed(),EeWonCert2Query::KeyId,theCommunityId) : 0;
    }
    unsigned short GetFirstCommunityTrust() const
    {
        return this->GetParsed()!=NULL
            ? static_cast<unsigned short>(ee_won_auth_cert2_query(this->GetParsed(),EeWonCert2Query::FirstCommunityTrust,0)) : 0;
    }
    virtual bool VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
    {
        return this->GetParsed() != NULL && ee_won_auth_verify_permissions(this->GetParsed(), userId, community, minTrust);
    }
};

class AuthPubKeyBlock : public AuthBase {
    typedef std::list<ElGamal> KeyBlock;
    KeyBlock mKeyBlock;
    unsigned short mKeyBlockId;
protected:
    virtual ~AuthPubKeyBlock() {}
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_PUB_KEY_BLOCK; }
    virtual void UnpackHook()
    {
        this->mKeyBlockId=this->GetHeader().key_block_id;
        this->mKeyBlock.clear();
        size_t count=ee_won_auth_key_count(this->GetParsed());
        for(size_t i=0;i<count;++i) {
            ElGamal key;
            key.Adopt(ee_won_auth_key(this->GetParsed(),i));
            this->mKeyBlock.push_back(key);
        }
    }
public:
    AuthPubKeyBlock(const void* data = NULL,unsigned long len = 0) : AuthBase(data,len),mKeyBlockId(0) { this->Unpack(); }
    unsigned short GetId() const { return this->mKeyBlockId; }
    bool HasKey() const { return !this->mKeyBlock.empty(); } // the driver's guard of GetCurrentKey
    const ElGamal& GetCurrentKey() const { return this->mKeyBlock.front(); }
    bool Verify(const AuthBase* base) const
    {
        if(this->GetParsed()==NULL || base==NULL || base->GetParsed()==NULL) return false;
        return ee_won_auth_verify_with_block(this->GetParsed(),base->GetParsed());
    }
    bool IsValidAndVerified(const ElGamal& verifier) const
    {
        return this->IsValid() && verifier.Verify(this->GetData(),this->GetDataLen(),this->GetSig(),this->GetSigLen());
    }
};

class AuthPeerData : public RefCount {
    ConstSmartPtr<AuthCertificate> mCertificate;
    ConstSmartPtr<Auth2Certificate> mCertificate2;
    ConstSmartPtr<AuthPubKeyBlock> mKeyBlock;
    ElGamal mPrivateKey;
    time_t mAuthDelta;
protected:
    virtual ~AuthPeerData() {}
public:
    AuthPeerData(const AuthPubKeyBlock* keyBlock = NULL)
        :mCertificate(new AuthCertificate),mCertificate2(new Auth2Certificate),mKeyBlock(keyBlock),mAuthDelta(0)
    {
        if(this->mKeyBlock.get()==NULL) this->mKeyBlock=new AuthPubKeyBlock;
    }
    AuthPeerData(const AuthCertificate* certificate,const AuthPubKeyBlock* keyBlock,
        const ElGamal& key,time_t delta,const Auth2Certificate* certificate2 = NULL)
        :mCertificate(certificate),mCertificate2(certificate2),mKeyBlock(keyBlock),mPrivateKey(key),mAuthDelta(delta)
    {
    }
    const AuthPubKeyBlock* GetPubKeyBlock() const { return this->mKeyBlock.get(); }
    time_t GetAuthDelta() const { return this->mAuthDelta; }
    const AuthCertificate* GetCertificate() const { return this->mCertificate.get(); }
    const ElGamal& GetPrivateKey() const { return this->mPrivateKey; }
    const Auth2Certificate* GetCertificate2() const { return this->mCertificate2.get(); }
    bool Verify(const AuthBase* base) const { return this->mKeyBlock->Verify(base); }
    bool CertificateExpired(time_t extra = 0) const { return this->mCertificate->IsExpired(extra+this->mAuthDelta); }
    bool Certificate2Expired(time_t extra = 0) const { return this->mCertificate2->IsExpired(extra+this->mAuthDelta); }
    bool IsValid() const
    {
        return this->mKeyBlock->IsValid() && this->mCertificate->IsValid() && this->mPrivateKey.IsPrivate();
    }
    bool IsValid2() const
    {
        return (this->mKeyBlock->IsValid() && this->mCertificate2->IsValid() && this->mPrivateKey.IsPrivate());
    }
    time_t GetExpireDelta() const
    {
        if(this->CertificateExpired()) return 0;
        return this->mCertificate->GetExpireTime()-this->mAuthDelta-time(NULL);
    }
};

class PeerAuthClient {
    ConstSmartPtr<AuthPeerData> mPeerData;
    AuthType mAuthType;
    Blowfish mSecretA;
    Blowfish mSecretB;
    unsigned char mLengthFieldSize;
    bool mUseClientSecret;
    unsigned char mClientKeySize;
    bool mUseAuth2;
    SmartPtr<AuthCertificateBase> mServerCertificate;
    SmartPtr<AuthSession> mSession;
public:
    enum PeerAuthClientState { STATE_NOT_STARTED=0, STATE_AWAITING_CHALLENGE=1, STATE_AWAITING_COMPLETE=2 } mState;
    template <class P> friend struct WONAPI::PeerAuthMsg;
    bool PaPrivateDecrypt(const void* theData, unsigned long theLen, std::string& out)
    {
        ByteBufferPtr aBuf = this->mPeerData->GetPrivateKey().Decrypt(theData, theLen);
        if(aBuf.get()==NULL)
            return false;
        out.assign(aBuf->data(), aBuf->length());
        return true;
    }
    bool PaMakeCert(bool auth2, const void* theData, unsigned long theLen)
    {
        if(auth2)
            this->mServerCertificate = new Auth2Certificate(theData, theLen);
        else
            this->mServerCertificate = new AuthCertificate(theData, theLen);
        return this->mServerCertificate->IsValid();
    }
    bool PaVerifyCert() { return this->mPeerData->Verify(this->mServerCertificate.get()); }
    bool PaCertExpired() { return this->mServerCertificate->IsExpired(this->mPeerData->GetAuthDelta()); }
    bool PaCertEncrypt(const void* theData, unsigned long theLen, std::string& out)
    {
        ByteBufferPtr aBuf = this->mServerCertificate->GetPubKey().Encrypt(theData, theLen);
        if(aBuf.get()==NULL)
            return false;
        out.assign(aBuf->data(), aBuf->length());
        return true;
    }
    bool PaHasOwnCert2() { return this->mPeerData->GetCertificate2()!=NULL; }
    void PaOwnCertRaw(bool auth2, std::string& out)
    {
        const AuthCertificateBase* aCert = auth2 ? static_cast<const AuthCertificateBase*>(this->mPeerData->GetCertificate2())
                                                 : static_cast<const AuthCertificateBase*>(this->mPeerData->GetCertificate());
        if(aCert->GetRawLen()>0)
            out.assign(static_cast<const char*>(aCert->GetRaw()), aCert->GetRawLen());
    }
    void PaSecretCreate(int which, int theLen) { (which ? this->mSecretB : this->mSecretA).Create(theLen); }
    bool PaSecretSetKey(int which, const void* theKey, int theLen)
    {
        return (which ? this->mSecretB : this->mSecretA).SetKey(theKey, theLen);
    }
    void PaSecretKey(int which, std::string& out)
    {
        const Blowfish& aKey = which ? this->mSecretB : this->mSecretA;
        out.assign(static_cast<const char*>(aKey.GetKey()), aKey.GetKeyLen());
    }
    void PaNewSession(int theType, unsigned short theId, int which, unsigned char theLengthFieldSize)
    {
        this->mSession = new AuthSession((AuthType)theType, theId, which ? this->mSecretB : this->mSecretA, theLengthFieldSize);
    }
    void PaGetState(EeWonPeerState& s)
    {
        s.state = this->mState;
        s.auth_type = this->mAuthType;
        s.length_field_size = this->mLengthFieldSize;
        s.use_auth2 = this->mUseAuth2;
        s.use_client_secret = this->mUseClientSecret;
        s.client_key_size = this->mClientKeySize;
    }
    void PaSetState(const EeWonPeerState& s)
    {
        this->mState = (PeerAuthClientState)s.state;
        this->mAuthType = (AuthType)s.auth_type;
        this->mLengthFieldSize = s.length_field_size;
        this->mUseAuth2 = s.use_auth2;
        this->mUseClientSecret = s.use_client_secret;
        this->mClientKeySize = s.client_key_size;
    }
public:
    PeerAuthClient()
    {
        this->Reset();
        this->mUseAuth2=true;
        this->mUseClientSecret=false;
        this->mClientKeySize=8;
    }
    void Reset() { this->mState=STATE_NOT_STARTED; }
    ByteBufferPtr Start(const AuthPeerData *theData, AuthType theType, unsigned char theLengthFieldSize)
    {
        this->mPeerData = theData;
        ByteBufferPtr aMsg;
        PeerAuthMsg<PeerAuthClient>::ClientStart(*this, theType, theLengthFieldSize, WonByteBufferSink, &aMsg);
        return aMsg;
    }
    WONStatus HandleRecvMsg(const void *inMsg, unsigned long inMsgLen, ByteBufferPtr &outMsg)
    {
        outMsg = NULL;
        return PeerAuthMsg<PeerAuthClient>::ClientRecv(*this, inMsg, inMsgLen, WonByteBufferSink, &outMsg);
    }
    AuthSession* GetSession() { return this->mSession.get(); }
    const AuthCertificateBase* GetServerCertificate() { return this->mServerCertificate.get(); }
    PeerAuthClientState GetState() { return this->mState; }
    void SetUseAuth2(bool value) { this->mUseAuth2=value; }
    void SetClientKeySize(unsigned char size,bool useSecret=true) { this->mClientKeySize=size;this->mUseClientSecret=useSecret; }
};

class PeerAuthServer {
    ConstSmartPtr<AuthPeerData> mPeerData;
    AuthType mAuthType;
    Blowfish mSecretA;
    Blowfish mSecretB;
    unsigned char mLengthFieldSize;
    SmartPtr<AuthCertificateBase> mClientCertificate;
    SmartPtr<AuthSession> mSession;
    bool mUseAuth2;
public:
    enum PeerAuthServerState { STATE_NOT_STARTED=0,STATE_AWAITING_REQUEST=1,STATE_AWAITING_CHALLENGE2=2 } mState;
    template <class P> friend struct WONAPI::PeerAuthMsg;
    bool PaPrivateDecrypt(const void* data,unsigned long len,std::string& out)
    {
        ByteBufferPtr buf=this->mPeerData->GetPrivateKey().Decrypt(data,len);
        if(buf.get()==NULL)
            return false;
        out.assign(buf->data(),buf->length());
        return true;
    }
    bool PaMakeCert(bool auth2,const void* data,unsigned long len)
    {
        if(auth2)
            this->mClientCertificate=new Auth2Certificate(data,len);
        else
            this->mClientCertificate=new AuthCertificate(data,len);
        return this->mClientCertificate->IsValid();
    }
    bool PaVerifyCert() { return this->mPeerData->Verify(this->mClientCertificate.get()); }
    bool PaCertExpired() { return this->mClientCertificate->IsExpired(this->mPeerData->GetAuthDelta()); }
    bool PaCertEncrypt(const void* data,unsigned long len,std::string& out)
    {
        ByteBufferPtr buf=this->mClientCertificate->GetPubKey().Encrypt(data,len);
        if(buf.get()==NULL)
            return false;
        out.assign(buf->data(),buf->length());
        return true;
    }
    bool PaHasOwnCert2() { return this->mPeerData->GetCertificate2()!=NULL; }
    void PaOwnCertRaw(bool auth2,std::string& out)
    {
        ByteBufferPtr raw=auth2 ? this->mPeerData->GetCertificate2()->GetRawBuf() : this->mPeerData->GetCertificate()->GetRawBuf();
        if(raw.get()!=NULL && raw->length()>0)
            out.assign(raw->data(),raw->length());
    }
    void PaSecretCreate(int which,int len) { (which ? this->mSecretB : this->mSecretA).Create(len); }
    bool PaSecretSetKey(int which,const void* key,int len)
    {
        return (which ? this->mSecretB : this->mSecretA).SetKey(key,len);
    }
    void PaSecretKey(int which,std::string& out)
    {
        const Blowfish& key=which ? this->mSecretB : this->mSecretA;
        out.assign(static_cast<const char*>(key.GetKey()),key.GetKeyLen());
    }
    void PaNewSession(int type,unsigned short id,int which,unsigned char lengthFieldSize)
    {
        this->mSession=new AuthSession((AuthType)type,id,which ? this->mSecretB : this->mSecretA,lengthFieldSize);
    }
    void PaGetState(EeWonPeerState& s)
    {
        s.state=this->mState;
        s.auth_type=this->mAuthType;
        s.length_field_size=this->mLengthFieldSize;
        s.use_auth2=this->mUseAuth2;
        s.use_client_secret=false;
        s.client_key_size=0;
    }
    void PaSetState(const EeWonPeerState& s)
    {
        this->mState=(PeerAuthServerState)s.state;
        this->mAuthType=(AuthType)s.auth_type;
        this->mLengthFieldSize=s.length_field_size;
        this->mUseAuth2=s.use_auth2;
    }
public:
    PeerAuthServer()
    {
        this->mState=STATE_NOT_STARTED;
        this->mUseAuth2=true;
    }
    void Start(const AuthPeerData* data,unsigned char lengthFieldSize)
    {
        this->mPeerData=data;
        PeerAuthMsg<PeerAuthServer>::ServerStart(*this,lengthFieldSize);
    }
    WONStatus HandleRecvMsg(const void* data,unsigned long len,ByteBufferPtr& output)
    {
        output=NULL;
        return PeerAuthMsg<PeerAuthServer>::ServerRecv(*this,data,len,WonByteBufferSink,&output);
    }
    const AuthCertificateBase* GetClientCertificate() { return this->mClientCertificate.get(); }
    AuthSession* GetSession() { return this->mSession.get(); }
    PeerAuthServerState GetState() { return this->mState; }
    void SetUseAuth2(bool value) { this->mUseAuth2=value; }
};

} // namespace orig
