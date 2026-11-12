// GetCertOp: the original's statements (copied from GetCertOp.cpp before the port, `this->` an op stand-in)
// against the Rust op (ee_won getcert.rs) through its hooks, both over the same stand-ins for
// the auth classes (certificates, key blocks, the ElGamal and Blowfish keys, the peer data, the auth context),
// whose verdicts follow the bytes they are made from and whose every call is logged. The three requests over
// generated members; every reply generated in every op state, whole, cut at every length and mutated: the trace,
// the members, the return or the throw compared; and 100,000 sequences of the op's steps (RunHook, GetNextRequest,
// CheckResponse, Reset, CleanupHook, the setters) on one op. The exit code is the number of failed checks.
//
// `--print-ops N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_getcert.txt,
// recorded before the port; `getcert_tests.rs` replays it).
//
// `--print N` prints N replies in the golden's format (rust/crates/ee_won/src/golden_cert.txt): the state, the
// session key, the reply and what the original read of it with every check passing (`cert_golden` replays it).
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7171;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

// ---- the stand-ins

static std::string sTrace;
static int sSerial;
// golden mode: every check passes, the decryption is the identity
static bool sAllPass;
static void Log(const char* inFormat, ...)
{
    char aBuffer[256];
    va_list ap;
    va_start(ap, inFormat);
    vsnprintf(aBuffer, sizeof(aBuffer), inFormat, ap);
    va_end(ap);
    sTrace += aBuffer;
    sTrace += ';';
}
static std::string Hex(const void* inData, size_t inLength)
{
    static const char kHex[] = "0123456789abcdef";
    std::string s;
    const unsigned char* p = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inLength; i++) { s += kHex[p[i] >> 4]; s += kHex[p[i] & 15]; }
    return s;
}
static unsigned char At(const void* inData, unsigned long inLength, unsigned long i)
{
    return i < inLength ? static_cast<const unsigned char*>(inData)[i] : 0;
}

struct SKey {   // ElGamal: the key block's current key (Encrypt), the private key (SetPrivateKey), the verifier
    int mFail;
    bool mPublic;
    SKey() : mFail(0), mPublic(true) {}
    bool IsPublic() const { Log("public"); return this->mPublic; }
    bool SetPrivateKey(const void* inData, int inLength)
    {
        Log("priv %s", Hex(inData, inLength).c_str());
        return sAllPass || (inLength > 0 && At(inData, inLength, 0) != 0);
    }
    ByteBufferPtr Encrypt(const void* inData, int inLength) const
    {
        Log("elg %s", Hex(inData, inLength).c_str());
        if (this->mFail) return NULL;
        std::string s(static_cast<const char*>(inData), inLength);
        for (size_t i = 0; i < s.size(); i++) s[i] ^= 0x5a;
        s += "E";
        return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
    }
};

struct SCert : public RefCount {    // AuthCertificate / Auth2Certificate
    int mSerial, mIssue, mExpire;
    bool mValid, mSigned;
    SCert(const char* inKind, const void* inData, unsigned long inLength)
    {
        this->mSerial = ++sSerial;
        this->mValid = sAllPass || (At(inData, inLength, 0) & 1);
        this->mSigned = sAllPass || (At(inData, inLength, 0) & 2);
        this->mIssue = At(inData, inLength, 1) * 1000;
        this->mExpire = this->mIssue + At(inData, inLength, 2) * 10;
        Log("%s#%d %s", inKind, this->mSerial, Hex(inData, inLength).c_str());
    }
    bool IsValid() const { Log("valid#%d", this->mSerial); return this->mValid; }
    time_t ComputeAuthDelta() const { Log("delta#%d", this->mSerial); return this->mIssue - 500; }
    time_t GetIssueTime() const { return this->mIssue; }
    time_t GetExpireTime() const { return this->mExpire; }
};
struct SCert1 : public SCert { SCert1(const void* d = NULL, unsigned long n = 0) : SCert("cert1", d, n) {} };
struct SCert2 : public SCert { SCert2(const void* d = NULL, unsigned long n = 0) : SCert("cert2", d, n) {} };

struct SKeyBlock : public RefCount {    // AuthPubKeyBlock
    int mSerial;
    unsigned short mId;
    bool mValid, mVerified;
    SKey mKey;
    SKeyBlock(const void* inData = NULL, unsigned long inLength = 0)
    {
        this->mSerial = ++sSerial;
        this->mId = (unsigned short)(At(inData, inLength, 1) | At(inData, inLength, 2) << 8);
        this->mValid = inLength > 0;
        this->mVerified = sAllPass || (At(inData, inLength, 0) & 1);
        this->mKey.mFail = !sAllPass && (At(inData, inLength, 0) & 4);
        Log("block#%d %s", this->mSerial, Hex(inData, inLength).c_str());
    }
    bool IsValid() const { Log("isvalid#%d", this->mSerial); return this->mValid; }
    unsigned short GetId() const { return this->mId; }
    const SKey& GetCurrentKey() const { return this->mKey; }
    bool IsValidAndVerified(const SKey&) const { Log("verified#%d", this->mSerial); return this->mVerified; }
    bool Verify(const SCert* inCert) const
    {
        Log("verify#%d#%d", this->mSerial, inCert->mSerial);
        return inCert->mSigned;
    }
};

struct SPeerData : public RefCount {    // AuthPeerData
    int mSerial;
    ConstSmartPtr<SKeyBlock> mKeyBlock;
    time_t mDelta;
    SPeerData(const SCert1* c1, const SKeyBlock* b, const SKey&, time_t d, const SCert2* c2) : mKeyBlock(b), mDelta(d)
    {
        this->mSerial = ++sSerial;
        Log("peer#%d c%d b%d d%d c%d", this->mSerial, c1 ? c1->mSerial : 0, b ? b->mSerial : 0, (int)d, c2 ? c2->mSerial : 0);
    }
    // AuthPeerData(const AuthPubKeyBlock*): an empty key block when none
    explicit SPeerData(const SKeyBlock* b) : mKeyBlock(b), mDelta(0)
    {
        if (this->mKeyBlock.get() == NULL) this->mKeyBlock = new SKeyBlock();
        this->mSerial = ++sSerial;
        Log("peer#%d b%d", this->mSerial, this->mKeyBlock->mSerial);
    }
    const SKeyBlock* GetPubKeyBlock() const { return this->mKeyBlock.get(); }
    time_t GetAuthDelta() const { return this->mDelta; }
};

struct SSessionKey {    // Blowfish
    unsigned char mKey[8];
    int mLength;
    SSessionKey() : mLength(8) { for (int i = 0; i < 8; i++) this->mKey[i] = (unsigned char)(0x10 + i); }
    bool Create(int n)
    {
        Log("create %d", n);
        for (int i = 0; i < 8; i++) this->mKey[i] = (unsigned char)(this->mKey[i] * 7 + 3);
        this->mLength = n;
        return true;
    }
    const void* GetKey() const { return this->mKey; }
    int GetKeyLen() const { return this->mLength; }
    ByteBufferPtr Encrypt(const void* inData, int inLength) const
    {
        Log("bf %s", Hex(inData, inLength).c_str());
        std::string s(static_cast<const char*>(inData), inLength);
        for (size_t i = 0; i < s.size(); i++) s[i] ^= 0x33;
        return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
    }
    ByteBufferPtr Decrypt(const void* inData, int inLength) const
    {
        Log("unbf %d", inLength);
        if (!sAllPass && inLength > 0 && At(inData, inLength, 0) == 0xdd) return NULL;
        return new ByteBuffer(inData, inLength, false);
    }
};

struct SAuthContext : public RefCount {   // a RefCount: the Rust op holds a reference (the tests hold one too)
    SKey mVerifier;
    std::wstring mUser, mPassword;
    std::string mCommunity;
    ConstSmartPtr<SPeerData> mPeer;
    const SKey& GetVerifierKey() const { return this->mVerifier; }
    ConstSmartPtr<SPeerData> GetPeerData() const { Log("getpeer#%d", this->mPeer->mSerial); return this->mPeer; }
    void LoadSecretList() { Log("load"); }
    const std::wstring& GetUserName() const { return this->mUser; }
    const std::wstring& GetPassword() const { return this->mPassword; }
    void RemoveSecret(const void* p) { Log("remove %s", Hex(p, 16).c_str()); }
    void AddSecret(const void* p) { Log("add %s", Hex(p, 16).c_str()); }
    void CommitSecrets(time_t a, time_t b) { Log("commit %d %d", (int)a, (int)b); }
    void SetPeerData(const SPeerData* p) { Log("setpeer#%d", p ? p->mSerial : 0); this->mPeer = p; }
    void SetPassword(const std::wstring& p) { Log("password %d", (int)p.size()); }
    void ClearOneTimeData() { Log("clear"); }
    void AppendCommunityData(WriteBuffer& w) { Log("community"); w.AppendBytes(this->mCommunity.data(), (unsigned long)this->mCommunity.size()); }
    void AppendHashes(WriteBuffer& w, const RawBuffer& s) { Log("hashes"); w.AppendBytes(s.data(), (unsigned long)s.size()); w.AppendByte('H'); }
};

struct SOp {
    struct COMMUNITY_STATUS {
        short mStatus;
        unsigned long mCommunityID;
        std::wstring mCommunityName;
    };
    enum State { GETTING_PUB_KEYS = 1, GETTING_CHALLENGE = 2, GETTING_CERTIFICATE = 3 };
    SAuthContext* mAuthContext;
    SSessionKey mSessionKey;
    ConstSmartPtr<SKeyBlock> mKeyBlock;
    ConstSmartPtr<SPeerData> mPeerData;
    RawBuffer mChallengeSeed;
    std::wstring mNewPassword;
    bool mCreateAccount;
    State mState;
    std::vector<std::string> mErrorList;
    std::list<COMMUNITY_STATUS> mCommunityStatusList;
    ByteBufferPtr mRequest, mResponse;
    time_t mAuthDelta;
    int mStatus;
    SOp() : mAuthDelta(0), mStatus(0) {}
    void SetLastServerError(WONStatus s) { Log("lasterror %d", (int)s); }
    WONStatus InvalidReplyHeader() { Log("invalid"); return WS_ServerReq_TryNextServer; }
    // AsyncOp's and ServerRequestOp's parts the op calls
    void Finish(int s) { Log("finish %d", s); }
    int GetStatus() const { Log("status %d", this->mStatus); return this->mStatus; }
    void BaseRunHook() { Log("baserun"); }
    void BaseCleanupHook() { Log("baseclean"); }
    std::string Dump() const
    {
        std::string s = "state " + std::to_string((int)this->mState) + " delta " +
                        std::to_string((long long)this->mAuthDelta) + " block " +
                        std::to_string(this->mKeyBlock.get() ? this->mKeyBlock->mSerial : 0) + " peer " +
                        std::to_string(this->mPeerData.get() ? this->mPeerData->mSerial : 0) + " seed " +
                        Hex(this->mChallengeSeed.data(), this->mChallengeSeed.size()) + " key " +
                        Hex(this->mSessionKey.mKey, 8) + " req " +
                        (this->mRequest.get() ? Hex(this->mRequest->data(), this->mRequest->length()) : "-");
        for (size_t i = 0; i < this->mErrorList.size(); i++) s += " err " + Hex(this->mErrorList[i].data(), this->mErrorList[i].size());
        for (std::list<COMMUNITY_STATUS>::const_iterator it = this->mCommunityStatusList.begin(); it != this->mCommunityStatusList.end(); ++it) {
            std::string n;
            WideAppendUtf16LE(n, it->mCommunityName);
            s += " cs " + std::to_string(it->mStatus) + " " + std::to_string(it->mCommunityID) + " " + Hex(n.data(), n.size());
        }
        return s;
    }
};

// ---- the Rust op (ee_won getcert.rs) over the same stand-ins, through its hooks

struct NOp {
    SAuthContext* mAuthContext;
    SSessionKey mSessionKey;
    ByteBufferPtr mRequest, mResponse;
    int mStatus;
    EeWonGetCert* mCert;
    NOp() : mAuthContext(NULL), mStatus(0), mCert(NULL) {}
    ~NOp();
    EeWonGetCertHooks Hooks();
    int GetStatus() const { Log("status %d", this->mStatus); return this->mStatus; }
    void RunHook()
    {
        EeWonGetCertHooks h = this->Hooks();
        long s = 0;
        if (ee_won_getcert_run_hook(this->mCert, &h, &s)) Log("finish %d", (int)s);
        else Log("baserun");
    }
    void CleanupHook()
    {
        EeWonGetCertHooks h = this->Hooks();
        ee_won_getcert_cleanup_hook(this->mCert, &h);
        Log("baseclean");
    }
    WONStatus GetNextRequest()
    {
        EeWonGetCertHooks h = this->Hooks();
        return (WONStatus)ee_won_getcert_get_next_request(this->mCert, &h);
    }
    WONStatus CheckResponse()
    {
        EeWonGetCertHooks h = this->Hooks();
        bool cut = false;
        long s = ee_won_getcert_check_response(this->mCert, &h, &cut);
        if (cut) throw ReadBufferException("Attempt to read past end of buffer.");
        return (WONStatus)s;
    }
    void SetNewPassword(const std::wstring& inPassword)
    {
        std::string b;
        WideAppendUtf16LE(b, inPassword);
        ee_won_getcert_set_new_password(this->mCert, reinterpret_cast<const uint8_t*>(b.data()), b.size());
    }
    std::string Dump() const;
};

namespace NH {
static RefCount* R(void* p) { return static_cast<RefCount*>(p); }
static void* V(const RefCount* p) { return const_cast<RefCount*>(p); }
static NOp* O(void* c) { return static_cast<NOp*>(c); }
static SAuthContext* C(void* p) { return static_cast<SAuthContext*>(R(p)); }
static const SKeyBlock* B(void* p) { return static_cast<const SKeyBlock*>(R(p)); }
static const SPeerData* P(void* p) { return static_cast<const SPeerData*>(R(p)); }
static const SCert* Cert(void* p) { return static_cast<const SCert*>(R(p)); }
static void Out(EeWonByteSink s, void* c, const void* d, size_t n) { s(c, static_cast<const uint8_t*>(d), n); }
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<std::string*>(c)->append(reinterpret_cast<const char*>(d), n); }
static void AddRef(void* p) { R(p)->AddRef(); }
static void Release(void* p) { R(p)->Release(); }
static bool VerifierPublic(void*, void* c) { return C(c)->GetVerifierKey().IsPublic(); }
static void* CtxPeer(void*, void* c)
{
    ConstSmartPtr<SPeerData> p = C(c)->GetPeerData();
    if (p.get() != NULL) const_cast<SPeerData*>(p.get())->AddRef();
    return V(p.get());
}
static void* PeerBlock(void*, void* p) { return V(P(p)->GetPubKeyBlock()); }
static int64_t PeerDelta(void*, void* p) { return P(p)->GetAuthDelta(); }
static void LoadSecrets(void*, void* c) { C(c)->LoadSecretList(); }
static long OpStatus(void* o) { return O(o)->GetStatus(); }
static bool BlockValid(void*, void* b) { return B(b)->IsValid(); }
static void SetPeerFromBlock(void*, void* c, void* b) { C(c)->SetPeerData(new SPeerData(B(b))); }
static uint16_t BlockId(void*, void* b) { return B(b)->GetId(); }
static bool BlockEncrypt(void*, void* b, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
{
    ByteBufferPtr e = B(b)->GetCurrentKey().Encrypt(d, (int)n);
    if (e.get() == NULL) return false;
    Out(s, sc, e->data(), e->length());
    return true;
}
static void KeyCreate(void* o, int32_t n) { O(o)->mSessionKey.Create(n); }
static void Key(void* o, EeWonByteSink s, void* sc) { Out(s, sc, O(o)->mSessionKey.GetKey(), O(o)->mSessionKey.GetKeyLen()); }
static bool KeyEncrypt(void* o, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
{
    ByteBufferPtr e = O(o)->mSessionKey.Encrypt(d, (int)n);
    if (e.get() == NULL) return false;
    Out(s, sc, e->data(), e->length());
    return true;
}
static bool KeyDecrypt(void* o, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
{
    ByteBufferPtr e = O(o)->mSessionKey.Decrypt(d, (int)n);
    if (e.get() == NULL) return false;
    Out(s, sc, e->data(), e->length());
    return true;
}
static void User(void*, void* c, EeWonByteSink s, void* sc)
{
    std::string b;
    WideAppendUtf16LE(b, C(c)->GetUserName());
    Out(s, sc, b.data(), b.size());
}
static void Password(void*, void* c, EeWonByteSink s, void* sc)
{
    std::string b;
    WideAppendUtf16LE(b, C(c)->GetPassword());
    Out(s, sc, b.data(), b.size());
}
static void Community(void*, void* c, EeWonByteSink s, void* sc)
{
    WriteBuffer w(0, true);
    C(c)->AppendCommunityData(w);
    Out(s, sc, w.data(), w.length());
}
static void Hashes(void*, void* c, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
{
    WriteBuffer w(0, true);
    C(c)->AppendHashes(w, RawBuffer(d, n));
    Out(s, sc, w.data(), w.length());
}
static void SetRequest(void* o, const uint8_t* d, size_t n) { O(o)->mRequest = new ByteBuffer(d, (unsigned long)n, false); }
static void Response(void* o, EeWonByteSink s, void* sc) { Out(s, sc, O(o)->mResponse->data(), O(o)->mResponse->length()); }
static void SetLastError(void*, long s) { Log("lasterror %d", (int)s); }
static long InvalidHeader(void*) { Log("invalid"); return WS_ServerReq_TryNextServer; }
static void AddSecret(void*, void* c, const uint8_t* d, size_t) { C(c)->AddSecret(d); }
static void RemoveSecret(void*, void* c, const uint8_t* d, size_t) { C(c)->RemoveSecret(d); }
static void* NewCert(void*, int32_t k, const uint8_t* d, size_t n)
{
    RefCount* c = k == 1 ? static_cast<RefCount*>(new SCert1(d, (unsigned long)n)) : static_cast<RefCount*>(new SCert2(d, (unsigned long)n));
    c->AddRef();
    return c;
}
static bool CertValid(void*, int32_t, void* c) { return Cert(c)->IsValid(); }
static bool BlockVerify(void*, void* b, int32_t, void* c) { return B(b)->Verify(Cert(c)); }
static int64_t CertDelta(void*, int32_t, void* c) { return Cert(c)->ComputeAuthDelta(); }
static void CertTimes(void*, int32_t, void* c, int64_t* i, int64_t* e)
{
    *i = Cert(c)->GetIssueTime();
    *e = Cert(c)->GetExpireTime();
}
static void* NewBlock(void*, const uint8_t* d, size_t n)
{
    SKeyBlock* b = new SKeyBlock(d, (unsigned long)n);
    b->AddRef();
    return V(b);
}
static bool BlockVerified(void*, void* b, void* c) { return B(b)->IsValidAndVerified(C(c)->GetVerifierKey()); }
static void Commit(void*, void* c, int64_t t, int64_t d) { C(c)->CommitSecrets((time_t)t, (time_t)d); }
static void* PrivNew(void*) { return new SKey; }
static bool PrivSet(void*, void* k, const uint8_t* d, size_t n) { return static_cast<SKey*>(k)->SetPrivateKey(d, (int)n); }
static void PrivFree(void*, void* k) { delete static_cast<SKey*>(k); }
static void* NewPeer(void*, void* c1, void* b, void* k, int64_t d, void* c2)
{
    SPeerData* p = new SPeerData(static_cast<const SCert1*>(Cert(c1)), B(b), *static_cast<SKey*>(k), (time_t)d,
                                 static_cast<const SCert2*>(Cert(c2)));
    p->AddRef();
    return V(p);
}
static void SetPassword(void*, void* c, const uint8_t* d, size_t n)
{
    std::wstring w;
    WideAssignUtf16LE(w, d, (unsigned long)(n / 2));
    C(c)->SetPassword(w);
}
static void SetPeer(void*, void* c, void* p) { C(c)->SetPeerData(P(p)); }
static void ClearOneTime(void*, void* c) { C(c)->ClearOneTimeData(); }
} // namespace NH

EeWonGetCertHooks NOp::Hooks()
{
    EeWonGetCertHooks h;
    h.context = this;
    h.add_ref = NH::AddRef;
    h.release = NH::Release;
    h.verifier_public = NH::VerifierPublic;
    h.ctx_peer = NH::CtxPeer;
    h.peer_block = NH::PeerBlock;
    h.peer_delta = NH::PeerDelta;
    h.load_secrets = NH::LoadSecrets;
    h.op_status = NH::OpStatus;
    h.block_valid = NH::BlockValid;
    h.set_peer_from_block = NH::SetPeerFromBlock;
    h.block_id = NH::BlockId;
    h.block_encrypt = NH::BlockEncrypt;
    h.key_create = NH::KeyCreate;
    h.key = NH::Key;
    h.key_encrypt = NH::KeyEncrypt;
    h.key_decrypt = NH::KeyDecrypt;
    h.user = NH::User;
    h.password = NH::Password;
    h.community = NH::Community;
    h.hashes = NH::Hashes;
    h.set_request = NH::SetRequest;
    h.response = NH::Response;
    h.set_last_error = NH::SetLastError;
    h.invalid_header = NH::InvalidHeader;
    h.add_secret = NH::AddSecret;
    h.remove_secret = NH::RemoveSecret;
    h.new_cert = NH::NewCert;
    h.cert_valid = NH::CertValid;
    h.block_verify = NH::BlockVerify;
    h.cert_delta = NH::CertDelta;
    h.cert_times = NH::CertTimes;
    h.new_block = NH::NewBlock;
    h.block_verified = NH::BlockVerified;
    h.commit = NH::Commit;
    h.priv_new = NH::PrivNew;
    h.priv_set = NH::PrivSet;
    h.priv_free = NH::PrivFree;
    h.new_peer = NH::NewPeer;
    h.set_password = NH::SetPassword;
    h.set_peer = NH::SetPeer;
    h.clear_one_time = NH::ClearOneTime;
    return h;
}

NOp::~NOp()
{
    if (this->mCert != NULL) {
        EeWonGetCertHooks h = this->Hooks();
        ee_won_getcert_free(this->mCert, &h);
    }
}

std::string NOp::Dump() const
{
    const SKeyBlock* b = NH::B(ee_won_getcert_key_block(this->mCert));
    const SPeerData* p = NH::P(ee_won_getcert_peer_data(this->mCert));
    std::string seed;
    ee_won_getcert_seed(this->mCert, NH::StrSink, &seed);
    std::string s = "state " + std::to_string(ee_won_getcert_state(this->mCert)) + " delta " +
                    std::to_string((long long)ee_won_getcert_auth_delta(this->mCert)) + " block " +
                    std::to_string(b ? b->mSerial : 0) + " peer " + std::to_string(p ? p->mSerial : 0) + " seed " +
                    Hex(seed.data(), seed.size()) + " key " + Hex(this->mSessionKey.mKey, 8) + " req " +
                    (this->mRequest.get() ? Hex(this->mRequest->data(), this->mRequest->length()) : "-");
    for (size_t i = 0; i < ee_won_getcert_error_count(this->mCert); i++) {
        std::string e;
        ee_won_getcert_error(this->mCert, i, NH::StrSink, &e);
        s += " err " + Hex(e.data(), e.size());
    }
    for (size_t i = 0; i < ee_won_getcert_community_count(this->mCert); i++) {
        short aStatus = 0;
        uint32_t anId = 0;
        std::string n;
        ee_won_getcert_community(this->mCert, i, &aStatus, &anId, NH::StrSink, &n);
        s += " cs " + std::to_string(aStatus) + " " + std::to_string(anId) + " " + Hex(n.data(), n.size());
    }
    return s;
}

// the Rust op with the members an original-statements stand-in op was set up with
static void ToRust(const SOp& o, NOp& n)
{
    n.mAuthContext = o.mAuthContext;
    EeWonGetCertHooks h = n.Hooks();
    n.mCert = ee_won_getcert_new(&h, NH::V(o.mAuthContext));
    ee_won_getcert_setup(n.mCert, &h, (int)o.mState, o.mChallengeSeed.data(), o.mChallengeSeed.size(),
                         NH::V(o.mKeyBlock.get()));
    n.SetNewPassword(o.mNewPassword);
    ee_won_getcert_set_create_account(n.mCert, o.mCreateAccount);
    sTrace.clear();
}

// the original thinks it dereferences NULL here (a certificate's times when the other one came)
struct OrigCrash {};

// ---- the original's statements (GetCertOp.cpp before the port)

namespace Orig {
enum {
    WS_GetCert_DecryptFailure = 100074,
    WS_GetCert_InvalidCertificate = 100072,
    WS_GetCert_InvalidPrivateKey = 100075,
    WS_GetCert_InvalidPubKeyBlock = 100073,
    WS_GetCert_InvalidSecretConfirm = 100076,
    WS_GetCert_MissingCertificate = 100077,
    WS_GetCert_UnexpectedLoginChallenge = 100069,
    WS_GetCert_PubKeyEncryptFailure = 100065,
    WS_ServerReq_Send = 100058,
    WS_ServerReq_TryNextServer = 100057,
    WS_ServerReq_Recv = 100059,
    WS_Success = 0
};
typedef SCert1 AuthCertificate;
typedef SCert2 Auth2Certificate;
typedef SKeyBlock AuthPubKeyBlock;
typedef SPeerData AuthPeerData;
typedef SKey ElGamal;

WONStatus SendPubKeyRequest(SOp& o)
{
    o.mState = SOp::GETTING_PUB_KEYS;
    WriteBuffer msg(4, true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(1);
    o.mRequest = msg.ToByteBuffer(true);
    return (WONStatus)WS_ServerReq_Recv;
}

WONStatus SendLoginRequest(SOp& o)
{
    WriteBuffer msg(4, true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(3);
    msg.AppendShort(o.mKeyBlock->GetId());

    const ElGamal& pubKey = o.mKeyBlock->GetCurrentKey();
    o.mSessionKey.Create(8);
    ByteBufferPtr encryptedSessionKey = pubKey.Encrypt(o.mSessionKey.GetKey(), o.mSessionKey.GetKeyLen());
    if (encryptedSessionKey.get() == NULL)
        return (WONStatus)WS_GetCert_PubKeyEncryptFailure;

    msg.AppendShort(encryptedSessionKey->length());
    msg.AppendBytes(encryptedSessionKey->data(), encryptedSessionKey->length());

    WriteBuffer loginData(0, true);
    loginData.AppendBytes(o.mSessionKey.GetKey(), 2);
    loginData.AppendShort(o.mKeyBlock->GetId());
    int flags = 0x0002 | 0x0004 | 0x0008;
    if (o.mCreateAccount)
        flags |= 0x0001;
    loginData.AppendLong(flags);
    loginData.AppendWString(o.mAuthContext->GetUserName(), 2);
    loginData.AppendWString(o.mAuthContext->GetPassword(), 2);
    loginData.AppendWString(o.mNewPassword, 2);
    o.mAuthContext->AppendCommunityData(loginData);

    ByteBufferPtr encryptedLoginData = o.mSessionKey.Encrypt(loginData.data(), loginData.length());
    msg.AppendShort(encryptedLoginData->length());
    msg.AppendBytes(encryptedLoginData->data(), encryptedLoginData->length());
    o.mRequest = msg.ToByteBuffer(true);
    o.mState = SOp::GETTING_CHALLENGE;
    return (WONStatus)WS_ServerReq_Recv;
}

WONStatus SendLoginConfirm(SOp& o)
{
    WriteBuffer msg(4, true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(5);

    WriteBuffer dataBlock(0, true);
    o.mAuthContext->AppendHashes(dataBlock, o.mChallengeSeed);
    ByteBufferPtr encrypted = o.mSessionKey.Encrypt(dataBlock.data(), dataBlock.length());
    msg.AppendShort(encrypted->length());
    msg.AppendBytes(encrypted->data(), encrypted->length());
    o.mRequest = msg.ToByteBuffer(true);
    return (WONStatus)WS_ServerReq_Recv;
}

// the golden's read log: what the original read (offsets in the message, or in the decrypted part)
static std::string sRead;
static void Read(const char* inFormat, ...)
{
    char aBuffer[128];
    va_list ap;
    va_start(ap, inFormat);
    vsnprintf(aBuffer, sizeof(aBuffer), inFormat, ap);
    va_end(ap);
    if (!sRead.empty()) sRead += ' ';
    sRead += aBuffer;
}

WONStatus HandleLoginReply(SOp& o, ReadBuffer& theMsg)
{
    ConstSmartPtr<AuthCertificate> aCert1;
    ConstSmartPtr<Auth2Certificate> aCert2;
    ElGamal aPrivKey;
    time_t anAuthDelta = 0;

    WONStatus aStatus = (WONStatus)theMsg.ReadShort();
    Read("status %d", (int)aStatus);
    if (aStatus < 0) // Failure status
    {
        if (!o.mChallengeSeed.empty())
            o.mAuthContext->RemoveSecret(o.mChallengeSeed.data());

        if (aStatus == -1) // unknown auth error
        {
            o.SetLastServerError(aStatus);
            return (WONStatus)WS_ServerReq_TryNextServer;
        }
    }

    unsigned char anErrorCount = theMsg.ReadByte();
    unsigned long i;
    const char* data;
    for (i = 0; i < anErrorCount; i++) {
        std::string anErrorString;
        theMsg.ReadString(anErrorString);
        o.mErrorList.push_back(anErrorString);
        Read("error %d %d", theMsg.pos() - (int)anErrorString.size(), (int)anErrorString.size());
    }

    unsigned char aNumClearEntries = theMsg.ReadByte();

    for (i = 0; i < aNumClearEntries; i++) {
        unsigned char aType = theMsg.ReadByte();
        unsigned short aLen = theMsg.ReadShort();

        data = static_cast<const char*>(theMsg.ReadBytes(aLen));
        if (aType != 7) Read("entry %d %d %d", aType, (int)(data - theMsg.data()), aLen);
        switch (aType) {
        case 1: {
            aCert1 = new AuthCertificate(data, aLen);
            if (!aCert1->IsValid() || !o.mKeyBlock->Verify(aCert1)) {
                o.SetLastServerError((WONStatus)WS_GetCert_InvalidCertificate);
                return (WONStatus)WS_ServerReq_TryNextServer;
            } else
                anAuthDelta = aCert1->ComputeAuthDelta();
        } break;

        case 3: {
            ConstSmartPtr<AuthPubKeyBlock> aKeyBlock = new AuthPubKeyBlock(data, aLen);
            if (!aKeyBlock->IsValidAndVerified(o.mAuthContext->GetVerifierKey())) {
                o.SetLastServerError((WONStatus)WS_GetCert_InvalidPubKeyBlock);
                return (WONStatus)WS_ServerReq_TryNextServer;
            } else
                o.mKeyBlock = aKeyBlock;
        } break;

        case 6: {
            aCert2 = new Auth2Certificate(data, aLen);
            if (!aCert2->IsValid() || !o.mKeyBlock->Verify(aCert2)) {
                o.SetLastServerError((WONStatus)WS_GetCert_InvalidCertificate);
                return (WONStatus)WS_ServerReq_TryNextServer;
            } else
                anAuthDelta = aCert2->ComputeAuthDelta();
        } break;

        case 7: {
            ReadBuffer aSubMsg(data, aLen);
            o.mCommunityStatusList.push_back(SOp::COMMUNITY_STATUS());
            SOp::COMMUNITY_STATUS& aCommunityStatus = o.mCommunityStatusList.back();

            int aFields = 0;
            try {
                aCommunityStatus.mStatus = aSubMsg.ReadShort();
                aFields = 1;
                aCommunityStatus.mCommunityID = aSubMsg.ReadLong();
                aFields = 2;
                aSubMsg.ReadWString(aCommunityStatus.mCommunityName, 2);
                aFields = 3;
            } catch (ReadBufferException&) {
                Read("community %d %d %u", aFields, aCommunityStatus.mStatus, (unsigned)aCommunityStatus.mCommunityID);
                throw;
            }
            Read("community 3 %d %u %d %d", aCommunityStatus.mStatus, (unsigned)aCommunityStatus.mCommunityID,
                 (int)(data - theMsg.data()) + aSubMsg.pos() - (int)aCommunityStatus.mCommunityName.size() * 2,
                 (int)aCommunityStatus.mCommunityName.size());
        } break;
        }
    }

    Read("rest %d %d", theMsg.pos(), (int)(theMsg.length() - theMsg.pos()));
    if (aStatus < 0)
        return aStatus;

    // Commit secrets and clean up secret file using auth server clock (the duration initialized: the original
    // passed an indeterminate one when neither certificate came)
    time_t anAuthTime = 0, aCertDuration = 0;
    if (aCert2.get() != NULL) {
        if (aCert1.get() == NULL) throw OrigCrash();
        anAuthTime = aCert1->GetIssueTime();
        aCertDuration = aCert1->GetExpireTime() - aCert1->GetIssueTime();
    } else if (aCert1.get() != NULL) {
        throw OrigCrash();  // anAuthTime = aCert2->GetIssueTime(): aCert2 is NULL here
    }

    o.mAuthContext->CommitSecrets(anAuthTime, aCertDuration);

    int aNumLeft = theMsg.length() - theMsg.pos();

    ByteBufferPtr aDecryptBuf = o.mSessionKey.Decrypt(theMsg.ReadBytes(aNumLeft), aNumLeft);
    if (aDecryptBuf.get() == NULL) {
        o.SetLastServerError((WONStatus)WS_GetCert_DecryptFailure);
        return (WONStatus)WS_ServerReq_TryNextServer;
    }

    ReadBuffer aDecrypt(aDecryptBuf->data(), aDecryptBuf->length());
    unsigned char aNumCryptEntries = aDecrypt.ReadByte();

    for (i = 0; i < aNumCryptEntries; i++) {
        unsigned char aType = aDecrypt.ReadByte();
        unsigned short aLen = aDecrypt.ReadShort();
        data = static_cast<const char*>(aDecrypt.ReadBytes(aLen));
        Read("crypt %d %d %d", aType, (int)(data - aDecrypt.data()), aLen);
        switch (aType) {
        case 2:
            if (!aPrivKey.SetPrivateKey(data, aLen)) {
                o.SetLastServerError((WONStatus)WS_GetCert_InvalidPrivateKey);
                return (WONStatus)WS_ServerReq_TryNextServer;
            }
            break;

        case 4:
            if ((aLen != o.mSessionKey.GetKeyLen() + 2) || memcmp(data + 2, o.mSessionKey.GetKey(), aLen - 2) != 0) {
                o.SetLastServerError((WONStatus)WS_GetCert_InvalidSecretConfirm);
                return (WONStatus)WS_ServerReq_TryNextServer;
            }
            break;

        case 5:
            break;
        }
    }

    if (aCert1.get() == NULL || aCert2.get() == NULL) {
        o.SetLastServerError((WONStatus)WS_GetCert_MissingCertificate);
        return (WONStatus)WS_ServerReq_TryNextServer;
    }

    o.mPeerData = new AuthPeerData(aCert1, o.mKeyBlock, aPrivKey, anAuthDelta, aCert2);
    if (!o.mNewPassword.empty())
        o.mAuthContext->SetPassword(o.mNewPassword);

    o.mAuthContext->SetPeerData(o.mPeerData.get());
    o.mAuthContext->ClearOneTimeData();
    return (WONStatus)WS_Success;
}

WONStatus HandleLoginChallenge(SOp& o, ReadBuffer& theMsg)
{
    unsigned short aLen = theMsg.ReadShort();
    const void* aBytes = theMsg.ReadBytes(aLen);
    Read("challenge %d %d", (int)(static_cast<const char*>(aBytes) - theMsg.data()), aLen);
    ByteBufferPtr aDecrypt = o.mSessionKey.Decrypt(aBytes, aLen);
    if (aDecrypt.get() == NULL) {
        o.SetLastServerError((WONStatus)WS_GetCert_DecryptFailure);
        return (WONStatus)WS_ServerReq_TryNextServer;
    }

    ReadBuffer aBuf(aDecrypt->data(), aDecrypt->length());
    o.mChallengeSeed.assign(static_cast<const unsigned char*>(aBuf.ReadBytes(16)), 16);
    o.mAuthContext->AddSecret(o.mChallengeSeed.data());
    Read("seed");
    return (WONStatus)WS_ServerReq_Send;
}

WONStatus CheckResponse(SOp& o)
{
    ReadBuffer aMsg(o.mResponse->data(), o.mResponse->length());
    unsigned char aHeaderType = aMsg.ReadByte();
    unsigned short aServiceType = aMsg.ReadShort();
    unsigned short aMessageType = aMsg.ReadShort();

    if (aHeaderType != 5 || aServiceType != 12) {
        Read("invalid");
        return o.InvalidReplyHeader();
    }

    if (o.mState == SOp::GETTING_PUB_KEYS) {
        if (aMessageType != 2) { // GetPubKeysReply
            Read("invalid");
            return o.InvalidReplyHeader();
        }

        WONStatus aStatus = (WONStatus)aMsg.ReadShort();
        if (aStatus < 0) { // shouldn't get this
            Read("server %d", (int)aStatus);
            o.SetLastServerError(aStatus);
            return (WONStatus)WS_ServerReq_TryNextServer;
        }

        unsigned short aLen = aMsg.ReadShort();
        const void* aBytes = aMsg.ReadBytes(aLen);
        Read("block %d %d", (int)(static_cast<const char*>(aBytes) - aMsg.data()), aLen);
        ConstSmartPtr<AuthPubKeyBlock> aKeyBlock = new AuthPubKeyBlock(aBytes, aLen);
        if (!aKeyBlock->IsValidAndVerified(o.mAuthContext->GetVerifierKey())) {
            o.SetLastServerError((WONStatus)WS_GetCert_InvalidPubKeyBlock);
            return (WONStatus)WS_ServerReq_TryNextServer;
        } else {
            o.mKeyBlock = aKeyBlock;
            return (WONStatus)WS_ServerReq_Send;
        }
    }

    if (aMessageType == 6)
        return HandleLoginReply(o, aMsg);

    if (aMessageType != 4) {
        Read("invalid");
        return o.InvalidReplyHeader();
    }

    if (o.mState != SOp::GETTING_CHALLENGE) {
        Read("unexpected");
        o.SetLastServerError((WONStatus)WS_GetCert_UnexpectedLoginChallenge);
        return (WONStatus)WS_ServerReq_TryNextServer;
    }

    return HandleLoginChallenge(o, aMsg);
}

// the op's other steps (GetCertOp.cpp before the port; the base class's calls are the stand-in's)
void RunHook(SOp& o)
{
    if (!o.mAuthContext->GetVerifierKey().IsPublic()) {
        o.Finish(100070);   // WS_GetCert_NeedVerifierKey
        return;
    }

    ConstSmartPtr<AuthPeerData> peerData = o.mAuthContext->GetPeerData();
    o.mKeyBlock = peerData->GetPubKeyBlock();
    o.mAuthDelta = peerData->GetAuthDelta();
    o.mAuthContext->LoadSecretList();
    o.BaseRunHook();
}

void CleanupHook(SOp& o)
{
    if (o.GetStatus() != WS_Success && o.mKeyBlock->IsValid() && !o.mAuthContext->GetPeerData()->GetPubKeyBlock()->IsValid())
        o.mAuthContext->SetPeerData(new AuthPeerData(o.mKeyBlock));

    o.BaseCleanupHook();
}

void Reset(SOp& o)
{
    o.mState = SOp::GETTING_PUB_KEYS;
    o.mChallengeSeed.erase();
    o.mErrorList.clear();
    o.mCommunityStatusList.clear();
}

WONStatus GetNextRequest(SOp& o)
{
    if (!o.mKeyBlock->IsValid())
        return SendPubKeyRequest(o);
    else if (o.mState == SOp::GETTING_CHALLENGE)
        return SendLoginConfirm(o);
    else
        return SendLoginRequest(o);
}
} // namespace Orig

// ---- the scenarios

static std::wstring AWString()
{
    static const wchar_t* const kTexts[] = { L"", L"bob", L"p\x263a", L"\x00e9t\x00e9" };
    std::wstring s = kTexts[Pick(4)];
    if (Pick(10) == 0) s += std::wstring(Pick(200), L'x');
    return s;
}
static std::string ABytes(int n)
{
    std::string s(n, '\0');
    for (int i = 0; i < n; i++) s[i] = (char)Pick(256);
    return s;
}
static void Short(std::string& s, int v) { s += (char)(v & 0xff); s += (char)((v >> 8) & 0xff); }
static void Long(std::string& s, unsigned v) { Short(s, v & 0xffff); Short(s, v >> 16); }

struct Setup {
    int mState;
    bool mSeed, mBlock, mNewPassword, mCreate, mEncryptFail;
    std::string mBlockBytes;
    std::wstring mUser, mPassword, mNew;
    std::string mCommunity;
    void Generate()
    {
        this->mState = 1 + Pick(3);
        this->mSeed = Pick(2) != 0;
        this->mBlock = Pick(5) != 0;
        this->mBlockBytes = ABytes(3);
        this->mEncryptFail = Pick(6) == 0;
        this->mNewPassword = Pick(3) == 0;
        this->mCreate = Pick(2) != 0;
        this->mUser = AWString();
        this->mPassword = AWString();
        this->mNew = this->mNewPassword ? AWString() : std::wstring();
        this->mCommunity = ABytes(Pick(6));
    }
    void Apply(SOp& o, SAuthContext& c) const
    {
        sSerial = 0;
        c.mUser = this->mUser;
        c.mPassword = this->mPassword;
        c.mCommunity = this->mCommunity;
        o.mAuthContext = &c;
        o.mState = (SOp::State)this->mState;
        if (this->mBlock) {
            o.mKeyBlock = new SKeyBlock(this->mBlockBytes.data(), (unsigned long)this->mBlockBytes.size());
            const_cast<SKeyBlock*>(o.mKeyBlock.get())->mKey.mFail = this->mEncryptFail;
        } else {
            o.mKeyBlock = new SKeyBlock();
        }
        if (this->mSeed) {
            unsigned char aSeed[16];
            for (int i = 0; i < 16; i++) aSeed[i] = (unsigned char)(0x80 + i);
            o.mChallengeSeed.assign(aSeed, 16);
        }
        o.mNewPassword = this->mNew;
        o.mCreateAccount = this->mCreate;
        sTrace.clear();
    }
};

// the session key a reply's secret confirmation must carry (the stand-in's key before any Create)
static std::string SessionKey()
{
    SSessionKey k;
    return std::string(reinterpret_cast<const char*>(k.mKey), 8);
}

static std::string CertEntry(int inType)
{
    std::string e;
    e += (char)inType;
    std::string d;
    if (inType == 7) {
        Short(d, Pick(3) ? Pick(5) : (int)Next());
        Long(d, Next());
        std::wstring n = AWString();
        std::string b;
        WideAppendUtf16LE(b, n);
        Short(d, Pick(12) ? (int)(b.size() / 2) : (int)Next());
        d += b;
        if (Pick(8) == 0) d.resize(Pick((int)d.size() + 1));
    } else {
        d = ABytes(Pick(4) ? 3 + Pick(4) : Pick(40));
        if (!d.empty() && Pick(3)) d[0] = (char)(Pick(4) ? 3 : Pick(256));   // mostly valid and signed
    }
    Short(e, (int)d.size());
    return e + d;
}

static std::string CryptPart()
{
    std::string c;
    int n = Pick(5);
    c += (char)n;
    for (int i = 0; i < n; i++) {
        static const int kCrypt[] = {2, 4, 5};
        int t = Pick(4) ? kCrypt[Pick(3)] : Pick(10);
        c += (char)t;
        std::string d;
        if (t == 4 && Pick(4)) d = ABytes(2) + SessionKey();
        else d = ABytes(Pick(12));
        if (t == 2 && !d.empty() && Pick(3)) d[0] = 1;
        Short(c, (int)d.size());
        c += d;
    }
    if (Pick(8) == 0) c = (char)0xdd + c;
    return c;
}

static std::string GenReply(int inState)
{
    std::string m;
    m += (char)(Pick(12) ? 5 : Pick(8));
    Short(m, Pick(12) ? 12 : Pick(20));
    int aType;
    if (inState == 1) aType = Pick(8) ? 2 : Pick(8);
    else aType = Pick(8) ? (Pick(3) ? 6 : 4) : Pick(8);
    Short(m, aType);
    if (inState == 1 && aType == 2) {
        Short(m, Pick(6) ? Pick(3) : -Pick(5) - 1);
        std::string b = ABytes(Pick(5) ? 3 : Pick(20));
        Short(m, (int)b.size());
        m += b;
    } else if (aType == 4) {
        std::string c = ABytes(Pick(5) ? 16 + Pick(5) : Pick(20));
        if (!c.empty() && Pick(10) == 0) c[0] = (char)0xdd;
        Short(m, (int)c.size());
        m += c;
    } else if (aType == 6) {
        static const int kStatus[] = {-1, -5, 3};
        int aStatus = Pick(5) ? 0 : kStatus[Pick(3)];
        Short(m, aStatus);
        int ne = Pick(3) ? 0 : Pick(4);
        m += (char)ne;
        for (int i = 0; i < ne; i++) {
            std::string s = ABytes(Pick(6));
            Short(m, (int)s.size());
            m += s;
        }
        int nc = Pick(6);
        std::string entries;
        if (Pick(3)) { entries += CertEntry(1); entries += CertEntry(6); nc = 2 + Pick(3); }
        m += (char)nc;
        m += entries;
        for (int i = entries.empty() ? 0 : 2; i < nc; i++) {
            static const int kTypes[] = {1, 3, 6, 7, 7, 0, 9};
            m += CertEntry(kTypes[Pick(7)]);
        }
        m += CryptPart();
    } else {
        m += ABytes(Pick(10));
    }
    return m;
}

struct Outcome {
    bool threw, crashed;
    int status;
    std::string trace, dump, read;
    bool operator==(const Outcome& o) const
    {
        return threw == o.threw && status == o.status && trace == o.trace && dump == o.dump;
    }
};

static Outcome RunReply(const Setup& s, const std::string& m, bool inOrig)
{
    SAuthContext c;
    c.AddRef();
    SOp o;
    s.Apply(o, c);
    NOp n;
    if (!inOrig) ToRust(o, n);
    o.mResponse = new ByteBuffer(m.data(), (unsigned long)m.size(), false);
    n.mResponse = o.mResponse;
    Outcome r;
    r.threw = r.crashed = false;
    r.status = 0;
    Orig::sRead.clear();
    try {
        r.status = inOrig ? Orig::CheckResponse(o) : n.CheckResponse();
    } catch (ReadBufferException&) {
        r.threw = true;
    } catch (OrigCrash&) {
        r.crashed = true;
    }
    r.trace = sTrace;
    r.dump = inOrig ? o.Dump() : n.Dump();
    r.read = Orig::sRead;
    return r;
}

// the original crashed: the new code's trace starts with the original's and goes on to the documented times
static void TestReplyOn(const Setup& s, const std::string& m)
{
    Outcome a = RunReply(s, m, true);
    Outcome b = RunReply(s, m, false);
    if (a.crashed) {
        CHECK(b.trace.compare(0, a.trace.size(), a.trace) == 0);
        CHECK(b.trace.find("commit ", a.trace.size()) == a.trace.size());
        return;
    }
    CHECK(a == b);
    if (!(a == b) && sFailures < 6)
        printf("state %d reply %s\n  orig %d/%d %s | %s\n  new  %d/%d %s | %s\n", s.mState, Hex(m.data(), m.size()).c_str(),
               a.threw, a.status, a.trace.c_str(), a.dump.c_str(), b.threw, b.status, b.trace.c_str(), b.dump.c_str());
}

static long sCrashes;
static void TestReplies()
{
    for (int i = 0; i < 40000; i++) {
        Setup s;
        s.Generate();
        sAllPass = Pick(4) == 0;
        std::string m = GenReply(s.mState);
        TestReplyOn(s, m);
        for (size_t n = 0; n < m.size(); n++) TestReplyOn(s, m.substr(0, n));
        for (int j = 0; j < 4 && !m.empty(); j++) {
            std::string x = m;
            x[Pick((int)x.size())] = (char)Pick(256);
            TestReplyOn(s, x);
        }
        if (RunReply(s, m, true).crashed) sCrashes++;
    }
    sAllPass = false;
}

// a request through GetNextRequest's choice: the key block request (an invalid block), the login confirmation
// (the challenge state) or the login request
static Outcome RunRequest(const Setup& s, int inKind, bool inOrig)
{
    SAuthContext c;
    c.AddRef();
    SOp o;
    s.Apply(o, c);
    if (inKind == 0) o.mKeyBlock = new SKeyBlock();
    else if (inKind == 1) o.mState = SOp::GETTING_PUB_KEYS;
    else o.mState = SOp::GETTING_CHALLENGE;
    NOp n;
    if (!inOrig) ToRust(o, n);
    sTrace.clear();
    Outcome r;
    r.threw = r.crashed = false;
    r.status = inOrig ? Orig::GetNextRequest(o) : n.GetNextRequest();
    r.trace = sTrace;
    r.dump = inOrig ? o.Dump() : n.Dump();
    return r;
}

static void TestRequests()
{
    for (int i = 0; i < 30000; i++) {
        Setup s;
        s.Generate();
        s.mBlock = true;
        int k = i % 3;
        Outcome a = RunRequest(s, k, true);
        Outcome b = RunRequest(s, k, false);
        CHECK(a == b);
        if (!(a == b) && sFailures < 6)
            printf("request %d\n  orig %d %s | %s\n  new  %d %s | %s\n", k, a.status, a.trace.c_str(), a.dump.c_str(),
                   b.status, b.trace.c_str(), b.dump.c_str());
    }
}

// ---- the whole op: a setup and a sequence of steps on one op (RunHook, GetNextRequest, CheckResponse, Reset,
// CleanupHook, the setters), the trace, the return or throw and the members after each step

struct Step {
    int mKind;          // 0 RunHook, 1 GetNextRequest, 2 CheckResponse, 3 Reset, 4 CleanupHook, 5 the setters
    std::string mReply; // 2: the reply
    int mStatus;        // 4: the op's status
    std::wstring mNew;  // 5: SetNewPassword
    bool mCreate;       // 5: SetCreateAccount
};
struct Seq {
    Setup mSetup;
    bool mAllPass, mPublic, mPeerBlock;
    std::string mPeerBlockBytes;
    int mPeerDelta;
    std::vector<Step> mSteps;
    void Generate()
    {
        this->mSetup.Generate();
        this->mAllPass = Pick(4) == 0;
        this->mPublic = Pick(8) != 0;
        this->mPeerBlock = Pick(3) != 0;
        this->mPeerBlockBytes = ABytes(3);
        this->mPeerDelta = Pick(3) ? (int)Next() % 100000 - 50000 : 0;
        int n = 1 + Pick(9);
        for (int i = 0; i < n; i++) {
            Step s;
            static const int kKinds[] = {0, 1, 1, 2, 2, 2, 3, 4, 5};
            s.mKind = i == 0 && Pick(2) ? 0 : kKinds[Pick(9)];
            s.mStatus = 0;
            s.mCreate = false;
            if (s.mKind == 2) {
                s.mReply = GenReply(1 + Pick(3));
                if (Pick(4) == 0 && !s.mReply.empty()) s.mReply.resize(Pick((int)s.mReply.size()));
            } else if (s.mKind == 4) {
                static const int kStatus[] = {0, 0, 100057, -5, 100070};
                s.mStatus = kStatus[Pick(5)];
            } else if (s.mKind == 5) {
                s.mNew = Pick(3) ? AWString() : std::wstring();
                s.mCreate = Pick(2) != 0;
            }
            this->mSteps.push_back(s);
        }
    }
    // the stand-ins' setup, then the trace cleared
    void Apply(SOp& o, SAuthContext& c) const
    {
        this->mSetup.Apply(o, c);
        c.mVerifier.mPublic = this->mPublic;
        SKeyBlock* b = NULL;
        if (this->mPeerBlock) {
            b = new SKeyBlock(this->mPeerBlockBytes.data(), 3);
            b->mKey.mFail = this->mSetup.mEncryptFail;
        }
        SPeerData* p = new SPeerData(b);
        p->mDelta = this->mPeerDelta;
        c.mPeer = p;
        sTrace.clear();
    }
    std::string Line() const
    {
        const Setup& s = this->mSetup;
        std::string u, p, n;
        WideAppendUtf16LE(u, s.mUser);
        WideAppendUtf16LE(p, s.mPassword);
        WideAppendUtf16LE(n, s.mNew);
        char aBuffer[128];
        snprintf(aBuffer, sizeof(aBuffer), "S %d %d %d %d %d %d %d %d %d", s.mState, s.mSeed, s.mBlock, s.mEncryptFail,
                 s.mCreate, this->mAllPass, this->mPublic, this->mPeerBlock, this->mPeerDelta);
        return std::string(aBuffer) + " " + Hex(s.mBlockBytes.data(), 3) + " " + Hex(this->mPeerBlockBytes.data(), 3) +
               " u" + Hex(u.data(), u.size()) + " p" + Hex(p.data(), p.size()) + " n" + Hex(n.data(), n.size()) +
               " c" + Hex(s.mCommunity.data(), s.mCommunity.size());
    }
};

static std::string StepLine(const Step& s)
{
    std::string a;
    if (s.mKind == 2) a = s.mReply.empty() ? "-" : Hex(s.mReply.data(), s.mReply.size());
    else if (s.mKind == 4) a = std::to_string(s.mStatus);
    else if (s.mKind == 5) {
        std::string n;
        WideAppendUtf16LE(n, s.mNew);
        a = (s.mCreate ? "1n" : "0n") + Hex(n.data(), n.size());
    } else a = "-";
    return "T " + std::to_string(s.mKind) + " " + a;
}

// one step on the original's statements: "threw status|trace|dump"
static std::string OrigStep(SOp& o, const Step& s)
{
    sTrace.clear();
    bool threw = false;
    int status = 0;
    try {
        switch (s.mKind) {
        case 0: Orig::RunHook(o); break;
        case 1: status = Orig::GetNextRequest(o); break;
        case 2:
            o.mResponse = new ByteBuffer(s.mReply.data(), (unsigned long)s.mReply.size(), false);
            status = Orig::CheckResponse(o);
            break;
        case 3: Orig::Reset(o); break;
        case 4: o.mStatus = s.mStatus; Orig::CleanupHook(o); break;
        case 5: o.mNewPassword = s.mNew; o.mCreateAccount = s.mCreate; break;
        }
    } catch (ReadBufferException&) {
        threw = true;
    } catch (OrigCrash&) {
        return "crash|" + sTrace;
    }
    return std::to_string((int)threw) + " " + std::to_string(status) + "|" + sTrace + "|" + o.Dump() + " ctxpeer " +
           std::to_string(o.mAuthContext->mPeer->mSerial);
}

// the Rust op's step
static std::string RustStep(NOp& n, const Step& s)
{
    sTrace.clear();
    bool threw = false;
    int status = 0;
    try {
        switch (s.mKind) {
        case 0: n.RunHook(); break;
        case 1: status = n.GetNextRequest(); break;
        case 2:
            n.mResponse = new ByteBuffer(s.mReply.data(), (unsigned long)s.mReply.size(), false);
            status = n.CheckResponse();
            break;
        case 3: ee_won_getcert_reset(n.mCert); break;
        case 4: n.mStatus = s.mStatus; n.CleanupHook(); break;
        case 5: n.SetNewPassword(s.mNew); ee_won_getcert_set_create_account(n.mCert, s.mCreate); break;
        }
    } catch (ReadBufferException&) {
        threw = true;
    }
    return std::to_string((int)threw) + " " + std::to_string(status) + "|" + sTrace + "|" + n.Dump() + " ctxpeer " +
           std::to_string(n.mAuthContext->mPeer->mSerial);
}

// the original's run of a sequence, a line per step; it stops at a crash
static std::vector<std::string> OrigSequence(const Seq& q)
{
    std::vector<std::string> out;
    SAuthContext c;
    c.AddRef();
    SOp o;
    sAllPass = q.mAllPass;
    q.Apply(o, c);
    for (size_t i = 0; i < q.mSteps.size(); i++) {
        out.push_back(OrigStep(o, q.mSteps[i]));
        if (out.back().compare(0, 6, "crash|") == 0) break;
    }
    sAllPass = false;
    return out;
}

// the Rust op's run of the same sequence (as many steps as the original's run)
static std::vector<std::string> RustSequence(const Seq& q, size_t inSteps)
{
    std::vector<std::string> out;
    SAuthContext c;
    c.AddRef();
    SOp o;
    sAllPass = q.mAllPass;
    q.Apply(o, c);
    NOp n;
    ToRust(o, n);
    for (size_t i = 0; i < inSteps; i++) out.push_back(RustStep(n, q.mSteps[i]));
    sAllPass = false;
    return out;
}

static long sSequenceSteps;
static void TestSequences()
{
    for (int i = 0; i < 100000; i++) {
        Seq q;
        q.Generate();
        std::vector<std::string> a = OrigSequence(q);
        std::vector<std::string> b = RustSequence(q, a.size());
        for (size_t j = 0; j < a.size(); j++) {
            sSequenceSteps++;
            if (a[j].compare(0, 6, "crash|") == 0) {
                // the original dereferenced NULL: the new trace starts with its trace and goes on to the commit
                std::string t = a[j].substr(6);
                size_t at = b[j].find('|') + 1;
                CHECK(b[j].compare(at, t.size(), t) == 0);
                CHECK(b[j].compare(at + t.size(), 7, "commit ") == 0);
                break;
            }
            CHECK(a[j] == b[j]);
            if (a[j] != b[j] && sFailures < 6)
                printf("%s\n%s\n  orig %s\n  new  %s\n", q.Line().c_str(), StepLine(q.mSteps[j]).c_str(), a[j].c_str(),
                       b[j].c_str());
            if (a[j] != b[j]) break;
        }
    }
}

// the golden of the whole op: a setup line, then each step and what the original did
static void PrintOps(int inCount)
{
    for (int i = 0; i < inCount; i++) {
        Seq q;
        q.Generate();
        std::vector<std::string> r = OrigSequence(q);
        printf("%s\n", q.Line().c_str());
        for (size_t j = 0; j < r.size(); j++) printf("%s => %s\n", StepLine(q.mSteps[j]).c_str(), r[j].c_str());
    }
}

// the golden: state, the reply, what the original read with every check passing ("cut" after a throw)
static void Print(int inCount)
{
    sAllPass = true;
    for (int i = 0; i < inCount; i++) {
        Setup s;
        s.Generate();
        std::string m = GenReply(s.mState);
        std::vector<std::string> v;
        v.push_back(m);
        for (int j = 0; j < 3 && !m.empty(); j++) v.push_back(m.substr(0, Pick((int)m.size())));
        if (!m.empty()) { std::string x = m; x[Pick((int)x.size())] = (char)Pick(256); v.push_back(x); }
        for (size_t j = 0; j < v.size(); j++) {
            Outcome a = RunReply(s, v[j], true);
            printf("%d %s |%s%s%s\n", s.mState, v[j].empty() ? "-" : Hex(v[j].data(), v[j].size()).c_str(),
                   a.read.empty() ? "" : " ", a.read.c_str(), a.threw ? " cut" : a.crashed ? " crash" : "");
        }
    }
}

int main(int argc, char** argv)
{
    int aPrint = 0, aPrintOps = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--print-ops") && i + 1 < argc) aPrintOps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrintOps) {
        PrintOps(aPrintOps);
        return 0;
    }
    if (aPrint) {
        Print(aPrint);
        return 0;
    }
    TestRequests();
    TestReplies();
    TestSequences();
    printf("won_cert_test: %lld checks, %d failures (%ld whole replies where the original dereferenced NULL; %ld "
           "steps of 100000 sequences)\n", sChecks, sFailures, sCrashes, sSequenceSteps);
    return sFailures;
}
