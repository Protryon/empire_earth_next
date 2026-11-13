// WONAuth's peer authentication and session (PeerAuthClient's and PeerAuthServer's Start, HandleRecvMsg and every
// handler; AuthSession's Encrypt and Decrypt): the original's statements (copied from PeerAuthClient.cpp,
// PeerAuthServer.cpp and AuthSession.cpp before the port) against won_peerauth_msg.h (include/ee/wonapi/
// WONPeerAuthMsg.h until round 14) over ee_won's peerauth module, and against the Rust AuthSession object.
//
// The handshake runs over stand-ins whose verdicts follow their bytes (a key's Decrypt, a certificate's validity,
// expiry and verification, a secret's Create) and that log every call, so the original's statements and the template
// meet identical, deterministic environments: generated messages, whole, cut at every length and mutated, in
// sequences of up to four per side. The trace, the statuses, the out messages and the members must agree. The
// session runs over the real Blowfish (ee_won's), on generated messages and sequence numbers.
//
// `--print N` prints N scenarios per side in the golden's format (rust/crates/ee_won/src/golden_peerauth.txt; the
// Rust replay is `peerauth::tests::golden`). The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "won_peerauth_msg.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 5151;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const string& s)
{
    static const char kHex[] = "0123456789abcdef";
    string r;
    for (size_t i = 0; i < s.size(); i++) { r += kHex[(unsigned char)s[i] >> 4]; r += kHex[s[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string Bytes(const void* p, unsigned long n) { return n ? string(static_cast<const char*>(p), n) : string(); }
static string RandBytes(int n)
{
    string s;
    for (int i = 0; i < n; i++) s += (char)Next();
    return s;
}

// ---- the stand-ins: verdicts follow the bytes, every call logged into the owner's trace

// a key: Decrypt fails on an empty input or one whose first byte is 0, else drops that byte and unmasks the rest;
// Encrypt fails for a length of 3 modulo 7, else puts a 1 in front and masks the rest
struct SKey {
    string* mLog;
    ByteBufferPtr Decrypt(const void* p, unsigned long n) const
    {
        string s = Bytes(p, n);
        *mLog += "dec(" + Hex(s) + ")";
        if (s.empty() || s[0] == 0) return NULL;
        string r;
        for (size_t i = 1; i < s.size(); i++) r += (char)(s[i] ^ 0x33);
        return new ByteBuffer(r.data(), r.size(), false);
    }
    ByteBufferPtr Encrypt(const void* p, unsigned long n) const
    {
        string s = Bytes(p, n);
        *mLog += "enc(" + Hex(s) + ")";
        if (n % 7 == 3) return NULL;
        string r(1, '\1');
        for (size_t i = 0; i < s.size(); i++) r += (char)(s[i] ^ 0x33);
        return new ByteBuffer(r.data(), r.size(), false);
    }
};

// a certificate: valid when non-empty and not starting 0xff; expired when its last byte is 0xee; verified when its
// length is not a multiple of 5
struct SCert : public RefCount {
    string mRaw;
    bool mTwo;
    SKey mKey;
    SCert(string* inLog, bool inTwo, const void* p, unsigned short n) : mRaw(Bytes(p, n)), mTwo(inTwo)
    {
        this->mKey.mLog = inLog;
        *inLog += string(inTwo ? "cert2(" : "cert1(") + Hex(this->mRaw) + ")";
    }
    bool IsValid() const { return !this->mRaw.empty() && (unsigned char)this->mRaw[0] != 0xff; }
    bool IsExpired(time_t inDelta) const { return !this->mRaw.empty() && (unsigned char)this->mRaw[this->mRaw.size() - 1] == 0xee; }
    const SKey& GetPubKey() const { return this->mKey; }
    const void* GetRaw() const { return this->mRaw.empty() ? NULL : this->mRaw.data(); }
    unsigned long GetRawLen() const { return (unsigned long)this->mRaw.size(); }
    ByteBufferPtr GetRawBuf() const { return this->mRaw.empty() ? NULL : new ByteBuffer(this->mRaw.data(), this->mRaw.size(), false); }
};
typedef SmartPtr<SCert> SCertPtr;

struct SPeerData {
    string* mLog;
    SKey mPrivate;
    SCertPtr mCert, mCert2;
    const SKey& GetPrivateKey() const { return this->mPrivate; }
    bool Verify(const SCert* c) const
    {
        *mLog += "verify;";
        return c->mRaw.size() % 5 != 0;
    }
    const SCert* GetCertificate() const { return this->mCert.get(); }
    const SCert* GetCertificate2() const { return this->mCert2.get(); }
    time_t GetAuthDelta() const { return 0; }
};

// a secret: Create makes a deterministic key from a counter (a length outside 5..56 fails and keeps the key);
// SetKey takes 5..56 bytes
struct SSecret {
    string* mLog;
    string mKey;
    int mCount;
    char mName;
    bool Create(int n)
    {
        char b[32];
        snprintf(b, sizeof(b), "create%c(%d)", this->mName, n);
        *mLog += b;
        if (n < 5 || n > 56) return false;
        this->mKey.clear();
        for (int i = 0; i < n; i++) this->mKey += (char)(i * 31 + this->mCount * 7 + this->mName);
        this->mCount++;
        return true;
    }
    bool SetKey(const void* p, int n)
    {
        *mLog += string("setkey") + this->mName + "(" + Hex(Bytes(p, n)) + ")";
        if (n < 5 || n > 56) return false;
        this->mKey = Bytes(p, n);
        return true;
    }
    const void* GetKey() const { return this->mKey.data(); }
    int GetKeyLen() const { return (int)this->mKey.size(); }
};

struct SSession : public RefCount {
    SSession(string* inLog, AuthType t, unsigned short id, const SSecret& k, unsigned char lfs)
    {
        char b[64];
        snprintf(b, sizeof(b), "session(%d,%u,%c,%u)", (int)t, (unsigned)id, k.mName, (unsigned)lfs);
        *inLog += b;
    }
};

// the members of PeerAuthClient and PeerAuthServer over the stand-ins
struct SPeer {
    enum { STATE_NOT_STARTED = 0, STATE_AWAITING_CHALLENGE = 1, STATE_AWAITING_COMPLETE = 2 };
    enum { STATE_AWAITING_REQUEST = 1, STATE_AWAITING_CHALLENGE2 = 2 };
    string mLog;
    SPeerData* mPeerData;
    SPeerData mData;
    AuthType mAuthType;
    SSecret mSecretA, mSecretB;
    unsigned char mLengthFieldSize;
    bool mUseClientSecret;
    unsigned char mClientKeySize;
    bool mUseAuth2;
    SCertPtr mServerCertificate;  // the client's; the server's mClientCertificate
    SmartPtr<SSession> mSession;
    int mState;

    void Init()
    {
        this->mData.mLog = &this->mLog;
        this->mData.mPrivate.mLog = &this->mLog;
        this->mSecretA.mLog = this->mSecretB.mLog = &this->mLog;
        this->mPeerData = &this->mData;
    }
    SPeer() { this->Init(); }
    SPeer(const SPeer& o) { *this = o; }
    SPeer& operator=(const SPeer& o)
    {
        this->mLog = o.mLog;
        this->mData.mCert = o.mData.mCert.get() ? new SCert(&this->mLog, o.mData.mCert->mTwo, o.mData.mCert->mRaw.data(), (unsigned short)o.mData.mCert->mRaw.size()) : NULL;
        this->mData.mCert2 = o.mData.mCert2.get() ? new SCert(&this->mLog, true, o.mData.mCert2->mRaw.data(), (unsigned short)o.mData.mCert2->mRaw.size()) : NULL;
        this->mLog = o.mLog;
        this->mAuthType = o.mAuthType;
        this->mSecretA = o.mSecretA;
        this->mSecretB = o.mSecretB;
        this->mLengthFieldSize = o.mLengthFieldSize;
        this->mUseClientSecret = o.mUseClientSecret;
        this->mClientKeySize = o.mClientKeySize;
        this->mUseAuth2 = o.mUseAuth2;
        this->mServerCertificate = o.mServerCertificate.get() ? new SCert(&this->mLog, o.mServerCertificate->mTwo, o.mServerCertificate->mRaw.data(), (unsigned short)o.mServerCertificate->mRaw.size()) : NULL;
        this->mSession = o.mSession;
        this->mState = o.mState;
        this->mLog = o.mLog;
        this->Init();
        return *this;
    }
    string Dump() const
    {
        char b[96];
        snprintf(b, sizeof(b), "%d %d %u %d %d %u ", this->mState, (int)this->mAuthType, (unsigned)this->mLengthFieldSize,
                 this->mUseAuth2, this->mUseClientSecret, (unsigned)this->mClientKeySize);
        return b + Hex(this->mSecretA.mKey) + " " + Hex(this->mSecretB.mKey) + " " +
               (this->mServerCertificate.get() ? Hex(this->mServerCertificate->mRaw) : string("*")) + " " + this->mLog;
    }

    // the golden's form: the members, the other side's certificate (* for none) and the trace, in hex
    string Compact() const
    {
        char b[96];
        snprintf(b, sizeof(b), "%d,%d,%u,%d,%d,%u,", this->mState, (int)this->mAuthType, (unsigned)this->mLengthFieldSize,
                 this->mUseAuth2, this->mUseClientSecret, (unsigned)this->mClientKeySize);
        return b + Hex(this->mSecretA.mKey) + "," + Hex(this->mSecretB.mKey) + "," +
               (this->mServerCertificate.get() ? Hex(this->mServerCertificate->mRaw) : string("*")) + "," + Hex(this->mLog);
    }

    // the template's hooks
    bool PaPrivateDecrypt(const void* p, unsigned long n, string& out)
    {
        ByteBufferPtr b = this->mPeerData->GetPrivateKey().Decrypt(p, n);
        if (b.get() == NULL) return false;
        out.assign(b->data(), b->length());
        return true;
    }
    bool PaMakeCert(bool auth2, const void* p, unsigned long n)
    {
        this->mServerCertificate = new SCert(&this->mLog, auth2, p, (unsigned short)n);
        return this->mServerCertificate->IsValid();
    }
    bool PaVerifyCert() { return this->mPeerData->Verify(this->mServerCertificate.get()); }
    bool PaCertExpired() { return this->mServerCertificate->IsExpired(this->mPeerData->GetAuthDelta()); }
    bool PaCertEncrypt(const void* p, unsigned long n, string& out)
    {
        ByteBufferPtr b = this->mServerCertificate->GetPubKey().Encrypt(p, n);
        if (b.get() == NULL) return false;
        out.assign(b->data(), b->length());
        return true;
    }
    bool PaHasOwnCert2() { return this->mPeerData->GetCertificate2() != NULL; }
    void PaOwnCertRaw(bool auth2, string& out)
    {
        const SCert* c = auth2 ? this->mPeerData->GetCertificate2() : this->mPeerData->GetCertificate();
        out = c->mRaw;
    }
    void PaSecretCreate(int which, int n) { (which ? this->mSecretB : this->mSecretA).Create(n); }
    bool PaSecretSetKey(int which, const void* p, int n) { return (which ? this->mSecretB : this->mSecretA).SetKey(p, n); }
    void PaSecretKey(int which, string& out)
    {
        const SSecret& s = which ? this->mSecretB : this->mSecretA;
        out.assign(static_cast<const char*>(s.GetKey()), s.GetKeyLen());
    }
    void PaNewSession(int type, unsigned short id, int which, unsigned char lfs)
    {
        this->mSession = new SSession(&this->mLog, (AuthType)type, id, which ? this->mSecretB : this->mSecretA, lfs);
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
        this->mState = s.state;
        this->mAuthType = (AuthType)s.auth_type;
        this->mLengthFieldSize = s.length_field_size;
        this->mUseAuth2 = s.use_auth2;
        this->mUseClientSecret = s.use_client_secret;
        this->mClientKeySize = s.client_key_size;
    }
};

// ---- the original's statements, over the stand-ins (`o.` for `this->`)
namespace OrigClient {
static WONStatus GetChallenge2(SPeer& o, ByteBufferPtr& challenge2)
{
    o.mState = SPeer::STATE_AWAITING_COMPLETE;
    WriteBuffer aMsg(o.mLengthFieldSize, true);
    aMsg.AppendLong(203);
    aMsg.AppendLong(52);
    WriteBuffer anEncryptBuf(0, true);
    anEncryptBuf.AppendShort(o.mSecretB.GetKeyLen());
    anEncryptBuf.AppendBytes(o.mSecretB.GetKey(), o.mSecretB.GetKeyLen());
    anEncryptBuf.AppendBytes(o.mSecretA.GetKey(), o.mSecretA.GetKeyLen());
    ByteBufferPtr anEncrypt = o.mServerCertificate->GetPubKey().Encrypt(anEncryptBuf.data(), anEncryptBuf.length());
    if (anEncrypt.get() == NULL)
        return WS_PeerAuthClient_Challenge2EncryptFailure;
    aMsg.AppendShort(anEncrypt->length());
    aMsg.AppendBytes(anEncrypt->data(), anEncrypt->length());
    challenge2 = aMsg.ToByteBuffer(true);
    return WS_Success;
}
// sUB: the decrypted secret was shorter than its length bytes (the original read past it)
static bool sUB;
static WONStatus HandleChallenge1(SPeer& o, ReadBuffer& theChallenge, ByteBufferPtr& challenge2)
{
    unsigned short aSecretLenWithLen = theChallenge.ReadShort();
    ByteBufferPtr aSecretBuf = o.mPeerData->GetPrivateKey().Decrypt(theChallenge.ReadBytes(aSecretLenWithLen), aSecretLenWithLen);
    if (aSecretBuf.get() == NULL)
        return WS_PeerAuthClient_Challenge1DecryptFailure;
    if (aSecretBuf->length() < 2) { sUB = true; return 0; } // the original read past the buffer here
    unsigned short aSecretLen = aSecretBuf->data()[0] | (aSecretBuf->data()[1] << 8);
    unsigned long aMaxLen = aSecretBuf->length() - 2;
    if (aSecretLen > aMaxLen)
        return WS_PeerAuthClient_Challenge1InvalidSecretLen;
    o.mSecretA.Create(o.mClientKeySize);
    const void* aKey = aSecretBuf->data() + 2;
    if (!o.mSecretB.SetKey(aKey, aSecretLen))
        return WS_PeerAuthClient_Challenge1InvalidSecretKey;
    unsigned short aCertLen = theChallenge.ReadShort();
    if (o.mUseAuth2)
        o.mServerCertificate = new SCert(&o.mLog, true, theChallenge.ReadBytes(aCertLen), aCertLen);
    else
        o.mServerCertificate = new SCert(&o.mLog, false, theChallenge.ReadBytes(aCertLen), aCertLen);
    if (!o.mServerCertificate->IsValid())
        return WS_PeerAuthClient_Challenge1CertificateUnpackFailure;
    if (!o.mPeerData->Verify(o.mServerCertificate.get()))
        return WS_PeerAuthClient_Challenge1CertificateVerifyFailure;
    return GetChallenge2(o, challenge2);
}
static WONStatus HandleComplete(SPeer& o, ReadBuffer& theComplete)
{
    short aStatus = theComplete.ReadShort();
    if (aStatus < 0) {
        unsigned short aNumErrors = theComplete.ReadShort();
        for (int i = 0; i < aNumErrors; i++) {
            std::string anError;
            theComplete.ReadString(anError);
        }
        return (WONStatus)aStatus;
    }
    unsigned short aLen = theComplete.ReadShort();
    ByteBufferPtr aDecrypt = o.mPeerData->GetPrivateKey().Decrypt(theComplete.ReadBytes(aLen), aLen);
    if (aDecrypt.get() == NULL)
        return WS_PeerAuthClient_CompleteDecryptFailure;
    if (aDecrypt->length() < 2)
        return WS_PeerAuthClient_CompleteInvalidSecretLen;
    aLen = (aDecrypt->data()[0] | (aDecrypt->data()[1] << 8));
    if (aLen > aDecrypt->length() - 2 || aLen != o.mSecretA.GetKeyLen() || memcmp(o.mSecretA.GetKey(), aDecrypt->data() + 2, aLen) != 0)
        return WS_PeerAuthClient_CompleteInvalidSecretKey;
    unsigned short aSessionId = 0;
    if (o.mAuthType == AUTH_TYPE_SESSION)
        aSessionId = theComplete.ReadShort();
    if (o.mAuthType != AUTH_TYPE_PERSISTENT_NOCRYPT)
        o.mSession = new SSession(&o.mLog, o.mAuthType, aSessionId, o.mUseClientSecret ? o.mSecretA : o.mSecretB, o.mLengthFieldSize);
    return WS_Success;
}
static ByteBufferPtr Start(SPeer& o, AuthType theType, unsigned char theLengthFieldSize)
{
    o.mState = SPeer::STATE_AWAITING_CHALLENGE;
    o.mAuthType = theType;
    o.mLengthFieldSize = theLengthFieldSize;
    bool sessioned = (o.mAuthType == AUTH_TYPE_SESSION);
    bool encrypted = (sessioned || o.mAuthType == AUTH_TYPE_PERSISTENT);
    WriteBuffer aMsg(o.mLengthFieldSize, true);
    aMsg.AppendLong(203);
    aMsg.AppendLong(50);
    aMsg.AppendByte(sessioned ? 2 : 1);
    aMsg.AppendByte(encrypted ? 1 : 0);
    int anEncryptFlags = 0;
    if (!sessioned) anEncryptFlags |= 0x0001;
    if (o.mUseClientSecret) anEncryptFlags |= 0x8000;
    aMsg.AppendShort(anEncryptFlags);
    const SCert* aCert = o.mPeerData->GetCertificate2();
    if (aCert == NULL || !o.mUseAuth2) {
        o.mUseAuth2 = false;
        aCert = o.mPeerData->GetCertificate();
    }
    unsigned short aCertLen = aCert->GetRawLen();
    aMsg.AppendShort(aCertLen);
    aMsg.AppendBytes(aCert->GetRaw(), aCertLen);
    return aMsg.ToByteBuffer(true);
}
static WONStatus HandleRecvMsg(SPeer& o, const void* inMsg, unsigned long inMsgLen, ByteBufferPtr& outMsg)
{
    outMsg = NULL;
    try {
        ReadBuffer aMsg(inMsg, inMsgLen);
        DWORD aServiceType = aMsg.ReadLong();
        DWORD aMessageType = aMsg.ReadLong();
        if (aServiceType != 203)
            return WS_PeerAuthClient_InvalidServiceType;
        if (aMessageType == 53)
            return HandleComplete(o, aMsg);
        else if (aMessageType == 51) {
            if (o.mState != SPeer::STATE_AWAITING_CHALLENGE)
                return WS_PeerAuthClient_UnexpectedChallenge;
            return HandleChallenge1(o, aMsg, outMsg);
        } else
            return WS_PeerAuthClient_InvalidMessage;
    } catch (ReadBufferException&) {
        return WS_PeerAuthClient_MsgUnpackFailure;
    }
}
} // namespace OrigClient

namespace OrigServer {
static void Start(SPeer& o, unsigned char lengthFieldSize)
{
    o.mLengthFieldSize = lengthFieldSize;
    o.mState = SPeer::STATE_AWAITING_REQUEST;
}
static WONStatus GetChallenge1(SPeer& o, ByteBufferPtr& challenge)
{
    WriteBuffer msg(o.mLengthFieldSize, true);
    msg.AppendLong(203);
    msg.AppendLong(51);
    o.mSecretB.Create(8);
    WriteBuffer secret(0, true);
    secret.AppendShort(o.mSecretB.GetKeyLen());
    secret.AppendBytes(o.mSecretB.GetKey(), o.mSecretB.GetKeyLen());
    ByteBufferPtr encrypted = o.mServerCertificate->GetPubKey().Encrypt(secret.data(), secret.length());
    if (encrypted.get() == NULL)
        return WS_PeerAuthServer_FailedToEncryptWithClientPubKey;
    msg.AppendShort(encrypted->length());
    msg.AppendBytes(encrypted->data(), encrypted->length());
    if (o.mUseAuth2)
        msg.AppendBuffer(o.mPeerData->GetCertificate2()->GetRawBuf().get(), 2);
    else
        msg.AppendBuffer(o.mPeerData->GetCertificate()->GetRawBuf().get(), 2);
    challenge = msg.ToByteBuffer(true);
    return WS_Success;
}
static WONStatus HandleRequest(SPeer& o, ReadBuffer& request, ByteBufferPtr& challenge)
{
    unsigned char authMode = request.ReadByte();
    unsigned char encryptType = request.ReadByte();
    request.ReadShort();
    if (authMode != 1)
        return WS_PeerAuthServer_InvalidAuthMode;
    if (encryptType != 0 && encryptType != 1)
        return WS_PeerAuthServer_InvalidEncryptType;
    bool encrypted = encryptType == 1;
    o.mAuthType = encrypted ? AUTH_TYPE_PERSISTENT : AUTH_TYPE_PERSISTENT_NOCRYPT;
    unsigned short len = request.ReadShort();
    if (o.mUseAuth2)
        o.mServerCertificate = new SCert(&o.mLog, true, request.ReadBytes(len), len);
    else
        o.mServerCertificate = new SCert(&o.mLog, false, request.ReadBytes(len), len);
    if (!o.mServerCertificate->IsValid())
        return WS_PeerAuthServer_InvalidClientCertificate;
    if (o.mServerCertificate->IsExpired(o.mPeerData->GetAuthDelta()))
        return WS_PeerAuthServer_ExpiredClientCertificate;
    if (!o.mPeerData->Verify(o.mServerCertificate.get()))
        return WS_PeerAuthServer_FailedToVerifyClientCertificate;
    return GetChallenge1(o, challenge);
}
static WONStatus GetComplete(SPeer& o, WONStatus status, ByteBufferPtr& complete)
{
    o.mState = 0;
    WriteBuffer msg(o.mLengthFieldSize, true);
    msg.AppendLong(203);
    msg.AppendLong(53);
    if (status != 0) {
        msg.AppendShort(-4);
        msg.AppendShort(1);
        msg.AppendString(WONStatusToString(status));
    } else {
        msg.AppendShort(0);
        WriteBuffer secret(0, true);
        secret.AppendShort(o.mSecretA.GetKeyLen());
        secret.AppendBytes(o.mSecretA.GetKey(), o.mSecretA.GetKeyLen());
        ByteBufferPtr encrypted = o.mServerCertificate->GetPubKey().Encrypt(secret.data(), secret.length());
        if (encrypted.get() == NULL)
            return WS_PeerAuthServer_FailedToEncryptWithClientPubKey;
        msg.AppendShort(encrypted->length());
        msg.AppendBytes(encrypted->data(), encrypted->length());
        o.mSession = new SSession(&o.mLog, o.mAuthType, 0, o.mSecretB, o.mLengthFieldSize);
    }
    complete = msg.ToByteBuffer(true);
    return WS_Success;
}
static WONStatus HandleChallenge2(SPeer& o, ReadBuffer& challenge, ByteBufferPtr& complete)
{
    unsigned short encryptedLen = challenge.ReadShort();
    ByteBufferPtr decrypted = o.mPeerData->GetPrivateKey().Decrypt(challenge.ReadBytes(encryptedLen), encryptedLen);
    if (decrypted.get() == NULL)
        return WS_PeerAuthServer_FailedToDecryptWithPrivateKey;
    ReadBuffer msg(decrypted->data(), decrypted->length());
    unsigned short secretBLen = msg.ReadShort();
    if (secretBLen != o.mSecretB.GetKeyLen() || memcmp(o.mSecretB.GetKey(), msg.ReadBytes(secretBLen), secretBLen) != 0)
        return WS_PeerAuthServer_InvalidSecretB;
    if (!o.mSecretA.SetKey(msg.data() + msg.pos(), msg.length() - msg.pos()))
        return WS_PeerAuthServer_InvalidSecretA;
    return GetComplete(o, 0, complete);
}
static WONStatus HandleRecvMsg(SPeer& o, const void* data, unsigned long len, ByteBufferPtr& output)
{
    output = NULL;
    if (o.mState == 0)
        return WS_PeerAuthServer_NotStarted;
    WONStatus status;
    try {
        ReadBuffer msg(data, len);
        DWORD serviceType = msg.ReadLong();
        DWORD messageType = msg.ReadLong();
        if (serviceType != 203)
            return WS_PeerAuthServer_InvalidServiceType;
        if (messageType == 50) {
            if (o.mState != SPeer::STATE_AWAITING_REQUEST)
                return WS_PeerAuthServer_UnexpectedRequest;
            status = HandleRequest(o, msg, output);
            o.mState = SPeer::STATE_AWAITING_CHALLENGE2;
        } else if (messageType == 52) {
            if (o.mState != SPeer::STATE_AWAITING_CHALLENGE2)
                return WS_PeerAuthServer_UnexpectedChallenge;
            status = HandleChallenge2(o, msg, output);
        } else
            return WS_PeerAuthServer_InvalidMessage;
    } catch (ReadBufferException&) {
        status = 100043;
    }
    if (status != 0)
        GetComplete(o, status, output);
    return status;
}
} // namespace OrigServer

// ---- generated messages

static string Short(int v) { return string(1, (char)(v & 255)) + (char)((v >> 8) & 255); }
static string Long(unsigned v) { return Short(v & 0xffff) + Short(v >> 16); }
static string Mask(const string& s)
{
    string r(1, '\1');
    for (size_t i = 0; i < s.size(); i++) r += (char)(s[i] ^ 0x33);
    return r;
}
static string Header(int type)
{
    unsigned service = Pick(12) == 0 ? (unsigned)Pick(300) : 203u;
    return Long(service) + Long(Pick(15) == 0 ? (unsigned)(50 + Pick(6)) : (unsigned)type);
}
static string Cert()
{
    switch (Pick(6)) {
    case 0: return "";
    case 1: return string(1, '\xff') + RandBytes(Pick(8));
    case 2: return RandBytes(4) + "\xee";
    default: return RandBytes(1 + Pick(14));
    }
}
static string Secret(const SPeer& o, bool inSelf)
{
    // a key echoed from the peer's own, or another
    if (inSelf && Pick(3)) return o.mSecretA.mKey;
    return RandBytes(Pick(5) == 0 ? Pick(5) : 5 + Pick(12));
}
static string Field(const string& inPlain)
{
    string m = Pick(10) == 0 ? RandBytes(Pick(6)) : Mask(inPlain);
    if (Pick(12) == 0 && !m.empty()) m[0] = 0;
    return Short((int)m.size() + (Pick(15) == 0 ? Pick(3) - 1 : 0)) + m;
}
static string ClientMsg(const SPeer& o)
{
    if (o.mState == 2 ? Pick(4) != 0 : Pick(4) == 0) {
        // complete
        string m = Header(53);
        if (Pick(4) == 0) {
            m += Short(-1 - Pick(30));
            int n = Pick(3);
            m += Short(n);
            for (int i = 0; i < n; i++) {
                string e = RandBytes(Pick(6));
                m += Short((int)e.size()) + e;
            }
            return m;
        }
        m += Short(Pick(3));
        string s = Secret(o, true);
        int len = Pick(10) == 0 ? (int)s.size() + Pick(3) - 1 : (int)s.size();
        m += Field(Pick(12) == 0 ? RandBytes(Pick(2)) : Short(len) + s);
        if (Pick(2)) m += Short(Pick(65536));
        return m;
    }
    string m = Header(51);
    string s = RandBytes(Pick(4) == 0 ? Pick(5) : 5 + Pick(40));
    int len = Pick(8) == 0 ? (int)s.size() + 1 + Pick(4) : (int)s.size() - (Pick(6) == 0 ? Pick(3) : 0);
    m += Field(Pick(15) == 0 ? RandBytes(Pick(2)) : Short(len) + s);
    string c = Cert();
    m += Short((int)c.size()) + c;
    return m;
}
static string ServerMsg(const SPeer& o)
{
    if (o.mState == 1 ? Pick(5) != 0 : Pick(5) == 0) {
        string m = Header(50);
        m += (char)(Pick(6) == 0 ? Pick(4) : 1);
        m += (char)(Pick(6) == 0 ? Pick(4) : Pick(2));
        m += Short(Pick(65536));
        string c = Cert();
        m += Short((int)c.size()) + c;
        return m;
    }
    string m = Header(52);
    string b = Pick(4) ? o.mSecretB.mKey : RandBytes(Pick(10));
    int blen = Pick(10) == 0 ? (int)b.size() + Pick(3) - 1 : (int)b.size();
    string a = RandBytes(Pick(5) == 0 ? Pick(60) : 5 + Pick(20));
    m += Field(Short(blen) + b + a);
    return m;
}
static string Mutate(const string& m)
{
    string r = m;
    switch (Pick(8)) {
    case 0: r = r.substr(0, Pick((int)r.size() + 1)); break;
    case 1: if (!r.empty()) r[Pick((int)r.size())] ^= (char)(1 << Pick(8)); break;
    case 2: r += RandBytes(Pick(4)); break;
    default: break;
    }
    return r;
}

static void RandPeer(SPeer& o)
{
    o.mLog.clear();
    o.mAuthType = (AuthType)Pick(4);
    o.mSecretA.mKey = Pick(3) ? "" : RandBytes(8);
    o.mSecretB.mKey = Pick(3) ? "" : RandBytes(8);
    o.mSecretA.mCount = Pick(5);
    o.mSecretB.mCount = Pick(5);
    o.mSecretA.mName = 'A';
    o.mSecretB.mName = 'B';
    static const unsigned char kLfs[] = {0, 1, 2, 4};
    o.mLengthFieldSize = kLfs[Pick(4)];
    o.mUseClientSecret = Pick(2) != 0;
    o.mClientKeySize = (unsigned char)(Pick(5) == 0 ? Pick(70) : 8);
    o.mUseAuth2 = Pick(4) != 0;
    o.mSession = NULL;
    o.mState = Pick(3);
    // a peer past its first message holds the other side's certificate (the original's precondition)
    o.mServerCertificate = (o.mState != 2 && Pick(3)) ? NULL : new SCert(&o.mLog, true, "\x05\x06\x07", 3);
    string c1 = RandBytes(Pick(8)), c2 = RandBytes(1 + Pick(8));
    o.mData.mCert = new SCert(&o.mLog, false, c1.data(), (unsigned short)c1.size());
    o.mData.mCert2 = Pick(3) ? new SCert(&o.mLog, true, c2.data(), (unsigned short)c2.size()) : NULL;
    o.mLog.clear();
}

static int sUBRuns;

static void ClientScenario(bool inPrint)
{
    SPeer a;
    RandPeer(a);
    SPeer b = a;
    string line = "client " + a.Compact() + "," + Hex(a.mData.mCert->mRaw) + "," +
                  (a.mData.mCert2.get() ? Hex(a.mData.mCert2->mRaw) : string("*")) + "," +
                  std::to_string(a.mSecretA.mCount) + "," + std::to_string(a.mSecretB.mCount);
    if (Pick(3)) {
        AuthType t = (AuthType)Pick(4);
        static const unsigned char kLfs[] = {0, 1, 2, 4};
        unsigned char lfs = kLfs[Pick(4)];
        ByteBufferPtr ra = OrigClient::Start(a, t, lfs);
        string rb;
        PeerAuthMsg<SPeer>::ClientStart(b, t, lfs, PeerAuthStringSink, &rb);
        CHECK(Bytes(ra->data(), ra->length()) == rb);
        CHECK(a.Dump() == b.Dump());
        line += " start:" + std::to_string((int)t) + ":" + std::to_string((unsigned)lfs) + ":" + Hex(rb);
    }
    int n = 1 + Pick(4);
    for (int i = 0; i < n; i++) {
        string m = Mutate(ClientMsg(a));
        OrigClient::sUB = false;
        ByteBufferPtr oa;
        long sa = OrigClient::HandleRecvMsg(a, m.data(), (unsigned long)m.size(), oa);
        if (OrigClient::sUB) {
            sUBRuns++;
            return;
        }
        string ob;
        long sb = PeerAuthMsg<SPeer>::ClientRecv(b, m.data(), (unsigned long)m.size(), PeerAuthStringSink, &ob);
        bool hasA = oa.get() != NULL;
        CHECK(sa == sb);
        CHECK(hasA == !ob.empty() || (hasA && oa->length() == 0));
        CHECK(!hasA || Bytes(oa->data(), oa->length()) == ob);
        CHECK(a.Dump() == b.Dump());
        if (a.Dump() != b.Dump() && sFailures < 6) printf("client:\n%s\n%s\n", a.Dump().c_str(), b.Dump().c_str());
        line += " msg:" + Hex(m) + ":" + std::to_string(sa) + ":" + (hasA ? Hex(ob) : string("*"));
    }
    line += " end:" + a.Compact();
    if (inPrint) printf("%s\n", line.c_str());
}

static void ServerScenario(bool inPrint)
{
    SPeer a;
    RandPeer(a);
    if (a.mUseAuth2 && a.mData.mCert2.get() == NULL) {
        string c2 = RandBytes(1 + Pick(8));
        a.mData.mCert2 = new SCert(&a.mLog, true, c2.data(), (unsigned short)c2.size());
        a.mLog.clear();
    }
    SPeer b = a;
    string line = "server " + a.Compact() + "," + Hex(a.mData.mCert->mRaw) + "," +
                  (a.mData.mCert2.get() ? Hex(a.mData.mCert2->mRaw) : string("*")) + "," +
                  std::to_string(a.mSecretA.mCount) + "," + std::to_string(a.mSecretB.mCount);
    if (a.mState == 0 ? Pick(8) != 0 : Pick(4) == 0) {
        static const unsigned char kLfs[] = {0, 1, 2, 4};
        unsigned char lfs = kLfs[Pick(4)];
        OrigServer::Start(a, lfs);
        PeerAuthMsg<SPeer>::ServerStart(b, lfs);
        CHECK(a.Dump() == b.Dump());
        line += " start:0:" + std::to_string((unsigned)lfs) + ":-";
    }
    int n = 1 + Pick(4);
    for (int i = 0; i < n; i++) {
        string m = Mutate(ServerMsg(a));
        ByteBufferPtr oa;
        long sa = OrigServer::HandleRecvMsg(a, m.data(), (unsigned long)m.size(), oa);
        string ob;
        long sb = PeerAuthMsg<SPeer>::ServerRecv(b, m.data(), (unsigned long)m.size(), PeerAuthStringSink, &ob);
        bool hasA = oa.get() != NULL;
        CHECK(sa == sb);
        CHECK(!hasA || Bytes(oa->data(), oa->length()) == ob);
        CHECK(hasA || ob.empty());
        CHECK(a.Dump() == b.Dump());
        if (a.Dump() != b.Dump() && sFailures < 6) printf("server:\n%s\n%s\n", a.Dump().c_str(), b.Dump().c_str());
        line += " msg:" + Hex(m) + ":" + std::to_string(sa) + ":" + (hasA ? Hex(ob) : string("*"));
    }
    line += " end:" + a.Compact();
    if (inPrint) printf("%s\n", line.c_str());
}

// ---- AuthSession: the original's statements over the real Blowfish against the Rust object

namespace OrigSession {
struct S {
    AuthType mAuthType;
    unsigned short mId, mInSeq, mOutSeq;
    Blowfish mKey;
    unsigned char mLengthFieldSize;
};
static WONStatus Encrypt(S& o, ByteBufferPtr& theMsg)
{
    if (o.mAuthType == AUTH_TYPE_NONE || o.mAuthType == AUTH_TYPE_PERSISTENT_NOCRYPT)
        return WS_Success;
    WriteBuffer aMsg(o.mLengthFieldSize, true);
    aMsg.AppendByte(12);
    int anOffset = o.mLengthFieldSize;
    if (anOffset == 2 && theMsg->length() >= 0xffff)
        anOffset = 6;
    WriteBuffer aSeqBuf(0, true);
    const char* aBuf = theMsg->data() + anOffset;
    unsigned long aLen = theMsg->length() - anOffset;
    if (o.mAuthType == AUTH_TYPE_SESSION) {
        aMsg.AppendShort(o.mId);
        aSeqBuf.AppendShort(++o.mOutSeq);
        aSeqBuf.AppendBytes(aBuf, aLen);
        aBuf = aSeqBuf.data();
        aLen = aSeqBuf.length();
    }
    ByteBufferPtr anEncrypt = o.mKey.Encrypt(aBuf, aLen);
    if (anEncrypt.get() == NULL)
        return WS_AuthSession_EncryptFailure;
    aMsg.AppendBytes(anEncrypt->data(), anEncrypt->length());
    theMsg = aMsg.ToByteBuffer(true);
    return WS_Success;
}
static WONStatus Decrypt(S& o, ByteBufferPtr& theMsg)
{
    try {
        if (o.mAuthType == AUTH_TYPE_NONE || o.mAuthType == AUTH_TYPE_PERSISTENT_NOCRYPT)
            return WS_Success;
        ReadBuffer anIn(theMsg->data(), theMsg->length());
        WriteBuffer anOut(0, true);
        unsigned char headerType = anIn.ReadByte();
        switch (headerType) {
        case 2: break;
        case 4: anOut.AppendByte(3); break;
        case 6: anOut.AppendByte(5); break;
        case 8: anOut.AppendByte(7); break;
        case 12: break;
        default: return WS_Success;
        }
        bool sessioned = o.mAuthType == AUTH_TYPE_SESSION;
        if (sessioned) {
            unsigned short aSessionId = anIn.ReadShort();
            if (aSessionId != o.mId)
                return WS_AuthSession_DecryptSessionIdMismatch;
        }
        ByteBufferPtr aDecrypt = o.mKey.Decrypt(anIn.data() + anIn.pos(), anIn.length() - anIn.pos());
        if (aDecrypt.get() == NULL)
            return WS_AuthSession_DecryptFailure;
        if (sessioned) {
            if (aDecrypt->length() < 2)
                return WS_AuthSession_DecryptBadLen;
            if (++o.mInSeq != *(const unsigned short*)aDecrypt->data())
                return WS_AuthSession_DecryptInvalidSequenceNum;
            anOut.AppendBytes(aDecrypt->data() + 2, aDecrypt->length() - 2);
        } else
            anOut.AppendBytes(aDecrypt->data(), aDecrypt->length());
        theMsg = anOut.ToByteBuffer(true);
        return WS_Success;
    } catch (ReadBufferException&) {
        return WS_AuthSession_DecryptUnpackFailure;
    }
}
} // namespace OrigSession

static void SessionScenario()
{
    OrigSession::S o;
    o.mAuthType = (AuthType)Pick(4);
    o.mId = (unsigned short)(Pick(3) ? Pick(4) : Next());
    o.mInSeq = o.mOutSeq = 0;
    static const unsigned char kLfs[] = {0, 1, 2, 4};
    o.mLengthFieldSize = kLfs[Pick(4)];
    string k = RandBytes(Pick(8) == 0 ? Pick(5) : 5 + Pick(20));
    if (!k.empty()) o.mKey.SetKey(k.data(), (int)k.size());
    EeWonAuthSession* r = ee_won_auth_session_new(o.mAuthType, o.mId,
                                                  reinterpret_cast<const EeWonBlowfish*>(&o.mKey), o.mLengthFieldSize, 7);
    int n = 1 + Pick(8);
    std::vector<string> sent;
    for (int i = 0; i < n; i++) {
        bool decrypt = Pick(2) != 0;
        string m;
        if (decrypt && !sent.empty() && Pick(3)) {
            m = sent[Pick((int)sent.size())];
            m = m.substr(std::min<size_t>(m.size(), o.mLengthFieldSize == 2 && m.size() >= 0xffff ? 6 : o.mLengthFieldSize));
            if (Pick(3) == 0 && !m.empty()) m[Pick((int)m.size())] ^= 1;
            if (Pick(3) == 0) m = m.substr(0, Pick((int)m.size() + 1));
        } else if (decrypt) {
            m = string(1, (char)(Pick(3) ? 2 * (1 + Pick(6)) : Pick(16))) + RandBytes(Pick(30));
        } else {
            size_t lfs = o.mLengthFieldSize;
            m = RandBytes((int)lfs + Pick(40));
            if (Pick(40) == 0) m = RandBytes(0x10000 + Pick(4));
        }
        ByteBufferPtr ma = new ByteBuffer(m.data(), m.size(), false);
        WONStatus sa = decrypt ? OrigSession::Decrypt(o, ma) : OrigSession::Encrypt(o, ma);
        string out;
        bool replaced = false;
        int32_t sb = ee_won_auth_session_crypt(r, decrypt, reinterpret_cast<const uint8_t*>(m.data()), m.size(), 9,
                                               PeerAuthStringSink, &out, &replaced);
        string after = Bytes(ma->data(), ma->length());
        CHECK(sa == sb);
        CHECK(replaced ? after == out : after == m);
        CHECK(ee_won_auth_session_get(r, 4) == o.mInSeq && ee_won_auth_session_get(r, 5) == o.mOutSeq);
        CHECK(ee_won_auth_session_get(r, 2) == 9);
        if (!decrypt && sa == 0) sent.push_back(after);
    }
    ee_won_auth_session_free(r);
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        for (int i = 0; i < aPrint; i++) ClientScenario(true);
        for (int i = 0; i < aPrint; i++) ServerScenario(true);
        return 0;
    }
    for (int i = 0; i < 150000; i++) ClientScenario(false);
    for (int i = 0; i < 150000; i++) ServerScenario(false);
    for (int i = 0; i < 40000; i++) SessionScenario();
    printf("won_peerauth_test: %lld checks, %d failures (%d client runs where the original read past a short secret)\n",
           sChecks, sFailures, sUBRuns);
    return sFailures;
}
