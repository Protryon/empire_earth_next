// GetCertOp: logs in to the auth server and fetches the certificates, key block and private key. The op's members
// and every step are ee_won's (rust/crates/ee_won/src/getcert.rs, a Rust object); this unit is the ServerRequestOp
// shim and the hooks for the C++ objects the op holds or makes (the auth context, key blocks, certificates, peer
// data, the private key). The base class's RunHook, CleanupHook and Finish are called here, after Rust returns.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONAuthCertificate.h"
#include "ee/wonapi/WONAuthPubKeyBlock.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONStatus.h"

namespace WONAPI {

// the hooks (a friend: the request and response are ServerRequestOp's protected members). Every object crosses as
// its RefCount*.
struct GetCertOpHooks {
    static RefCount* R(void* p) { return static_cast<RefCount*>(p); }
    static void* V(const RefCount* p) { return const_cast<RefCount*>(p); }
    static GetCertOp* Op(void* c) { return static_cast<GetCertOp*>(c); }
    static AuthContext* Ctx(void* p) { return static_cast<AuthContext*>(R(p)); }
    static const AuthPubKeyBlock* Block(void* p) { return static_cast<const AuthPubKeyBlock*>(R(p)); }
    static const AuthPeerData* Peer(void* p) { return static_cast<const AuthPeerData*>(R(p)); }
    static const AuthCertificate* Cert1(void* p) { return static_cast<const AuthCertificate*>(R(p)); }
    static const Auth2Certificate* Cert2(void* p) { return static_cast<const Auth2Certificate*>(R(p)); }
    static const AuthCertificateBase* Cert(int32_t k, void* p)
    {
        return k == 1 ? static_cast<const AuthCertificateBase*>(Cert1(p)) : static_cast<const AuthCertificateBase*>(Cert2(p));
    }
    static bool Out(EeWonByteSink s, void* c, const ByteBufferPtr& b)
    {
        if (b.get() == NULL)
            return false;
        s(c, reinterpret_cast<const uint8_t*>(b->data()), b->length());
        return true;
    }
    static void OutText(EeWonByteSink s, void* c, const std::wstring& t)
    {
        std::string b;
        WideAppendUtf16LE(b, t);
        s(c, reinterpret_cast<const uint8_t*>(b.data()), b.size());
    }

    static void AddRef(void* p) { R(p)->AddRef(); }
    static void Release(void* p) { R(p)->Release(); }
    static bool VerifierPublic(void*, void*) { return AuthContext::GetVerifierKey().IsPublic(); }
    static void* CtxPeer(void*, void* c)
    {
        ConstSmartPtr<AuthPeerData> p = Ctx(c)->GetPeerData();
        if (p.get() != NULL)
            const_cast<AuthPeerData*>(p.get())->AddRef();
        return V(p.get());
    }
    static void* PeerBlock(void*, void* p) { return V(Peer(p)->GetPubKeyBlock()); }
    static int64_t PeerDelta(void*, void* p) { return Peer(p)->GetAuthDelta(); }
    static void LoadSecrets(void*, void* c) { Ctx(c)->LoadSecretList(); }
    static long OpStatus(void* o) { return Op(o)->GetStatus(); }
    static bool BlockValid(void*, void* b) { return Block(b)->IsValid(); }
    static void SetPeerFromBlock(void*, void* c, void* b) { Ctx(c)->SetPeerData(new AuthPeerData(Block(b))); }
    static uint16_t BlockId(void*, void* b) { return Block(b)->GetId(); }
    static bool BlockEncrypt(void*, void* b, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
    {
        return Out(s, sc, Block(b)->GetCurrentKey().Encrypt(d, (int)n));
    }
    static void User(void*, void* c, EeWonByteSink s, void* sc) { OutText(s, sc, Ctx(c)->GetUserName()); }
    static void Password(void*, void* c, EeWonByteSink s, void* sc) { OutText(s, sc, Ctx(c)->GetPassword()); }
    static void Community(void*, void* c, EeWonByteSink s, void* sc)
    {
        WriteBuffer w(0, true);
        Ctx(c)->AppendCommunityData(w);
        s(sc, reinterpret_cast<const uint8_t*>(w.data()), w.length());
    }
    static void Hashes(void*, void* c, const uint8_t* d, size_t n, EeWonByteSink s, void* sc)
    {
        WriteBuffer w(0, true);
        Ctx(c)->AppendHashes(w, RawBuffer(d, n));
        s(sc, reinterpret_cast<const uint8_t*>(w.data()), w.length());
    }
    static void SetRequest(void* o, const uint8_t* d, size_t n)
    {
        ByteBufferPtr aBuffer;
        WonByteBufferSink(&aBuffer, d, n);
        Op(o)->mRequest = aBuffer;
    }
    static void Response(void* o, EeWonByteSink s, void* sc) { Out(s, sc, Op(o)->mResponse); }
    static void SetLastError(void* o, long s) { Op(o)->SetLastServerError((WONStatus)s); }
    static long InvalidHeader(void* o) { return Op(o)->InvalidReplyHeader(); }
    static void AddSecret(void*, void* c, const uint8_t* d, size_t) { Ctx(c)->AddSecret(d); }
    static void RemoveSecret(void*, void* c, const uint8_t* d, size_t) { Ctx(c)->RemoveSecret(d); }
    static void* NewCert(void*, int32_t k, const uint8_t* d, size_t n)
    {
        RefCount* c = k == 1 ? static_cast<RefCount*>(new AuthCertificate(d, (unsigned long)n))
                             : static_cast<RefCount*>(new Auth2Certificate(d, (unsigned long)n));
        c->AddRef();
        return c;
    }
    static bool CertValid(void*, int32_t k, void* c) { return Cert(k, c)->IsValid(); }
    static bool BlockVerify(void*, void* b, int32_t k, void* c)
    {
        return k == 1 ? Block(b)->Verify(Cert1(c)) : Block(b)->Verify(Cert2(c));
    }
    static int64_t CertDelta(void*, int32_t k, void* c)
    {
        return k == 1 ? Cert1(c)->ComputeAuthDelta() : Cert2(c)->ComputeAuthDelta();
    }
    static void CertTimes(void*, int32_t k, void* c, int64_t* i, int64_t* e)
    {
        *i = Cert(k, c)->GetIssueTime();
        *e = Cert(k, c)->GetExpireTime();
    }
    static void* NewBlock(void*, const uint8_t* d, size_t n)
    {
        AuthPubKeyBlock* b = new AuthPubKeyBlock(d, (unsigned long)n);
        b->AddRef();
        return V(b);
    }
    static bool BlockVerified(void*, void* b, void*) { return Block(b)->IsValidAndVerified(AuthContext::GetVerifierKey()); }
    static void Commit(void*, void* c, int64_t t, int64_t d) { Ctx(c)->CommitSecrets((time_t)t, (time_t)d); }
    static void* PrivNew(void*) { return new ElGamal; }
    static bool PrivSet(void*, void* k, const uint8_t* d, size_t n) { return static_cast<ElGamal*>(k)->SetPrivateKey(d, (int)n); }
    static void PrivFree(void*, void* k) { delete static_cast<ElGamal*>(k); }
    static void* NewPeer(void*, void* c1, void* b, void* k, int64_t d, void* c2)
    {
        AuthPeerData* p = new AuthPeerData(Cert1(c1), Block(b), *static_cast<ElGamal*>(k), (time_t)d, Cert2(c2));
        p->AddRef();
        return V(p);
    }
    static void SetPassword(void*, void* c, const uint8_t* d, size_t n)
    {
        std::wstring w;
        WideAssignUtf16LE(w, d, (unsigned long)(n / 2));
        Ctx(c)->SetPassword(w);
    }
    static void SetPeer(void*, void* c, void* p) { Ctx(c)->SetPeerData(Peer(p)); }
    static void ClearOneTime(void*, void* c) { Ctx(c)->ClearOneTimeData(); }
    static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<std::string*>(c)->append(reinterpret_cast<const char*>(d), n); }
};

EeWonGetCertHooks GetCertOp::Hooks()
{
    typedef GetCertOpHooks H;
    EeWonGetCertHooks h;
    h.context = this;
    h.add_ref = H::AddRef;
    h.release = H::Release;
    h.verifier_public = H::VerifierPublic;
    h.ctx_peer = H::CtxPeer;
    h.peer_block = H::PeerBlock;
    h.peer_delta = H::PeerDelta;
    h.load_secrets = H::LoadSecrets;
    h.op_status = H::OpStatus;
    h.block_valid = H::BlockValid;
    h.set_peer_from_block = H::SetPeerFromBlock;
    h.block_id = H::BlockId;
    h.block_encrypt = H::BlockEncrypt;
    h.key_create = NULL;     // the session key is the Rust object's Blowfish
    h.key = NULL;
    h.key_encrypt = NULL;
    h.key_decrypt = NULL;
    h.user = H::User;
    h.password = H::Password;
    h.community = H::Community;
    h.hashes = H::Hashes;
    h.set_request = H::SetRequest;
    h.response = H::Response;
    h.set_last_error = H::SetLastError;
    h.invalid_header = H::InvalidHeader;
    h.add_secret = H::AddSecret;
    h.remove_secret = H::RemoveSecret;
    h.new_cert = H::NewCert;
    h.cert_valid = H::CertValid;
    h.block_verify = H::BlockVerify;
    h.cert_delta = H::CertDelta;
    h.cert_times = H::CertTimes;
    h.new_block = H::NewBlock;
    h.block_verified = H::BlockVerified;
    h.commit = H::Commit;
    h.priv_new = H::PrivNew;
    h.priv_set = H::PrivSet;
    h.priv_free = H::PrivFree;
    h.new_peer = H::NewPeer;
    h.set_password = H::SetPassword;
    h.set_peer = H::SetPeer;
    h.clear_one_time = H::ClearOneTime;
    return h;
}

// FUNCTION: WON 0x006e3c30
// Creates a certificate request on the auth context's server list
GetCertOp::GetCertOp(AuthContext* context)
    :ServerRequestOp(context->GetServerContext().get())
{
    EeWonGetCertHooks h = this->Hooks();
    this->mCert = ee_won_getcert_new(&h, GetCertOpHooks::V(context));
}
// FUNCTION: WON 0x006e3d90
// Destroys the request: the Rust object lets its references go
GetCertOp::~GetCertOp()
{
    EeWonGetCertHooks h = this->Hooks();
    ee_won_getcert_free(this->mCert, &h);
}

// FUNCTION: WON 0x006e3f40
// Starts the request with the current key block and saved secrets; fails at once without a signing key
void GetCertOp::RunHook()
{
    EeWonGetCertHooks h = this->Hooks();
    long aStatus = 0;
    if (ee_won_getcert_run_hook(this->mCert, &h, &aStatus)) {
        this->Finish((WONStatus)aStatus);
        return;
    }
    ServerRequestOp::RunHook();
}

// FUNCTION: WON 0x006e4050
// On failure hands a valid key block to the auth context when it has none, then cleans up the request
void GetCertOp::CleanupHook()
{
    EeWonGetCertHooks h = this->Hooks();
    ee_won_getcert_cleanup_hook(this->mCert, &h);
    ServerRequestOp::CleanupHook();
}

// FUNCTION: WON 0x006e4130
// Returns to the key-fetch state and clears the challenge, errors and community status list
void GetCertOp::Reset()
{
    ee_won_getcert_reset(this->mCert);
}

// FUNCTION: WON 0x006e4850
// Picks the next message: key block request, login request or login confirmation (0x006e4270, 0x006e4380,
// 0x006e46a0)
WONStatus GetCertOp::GetNextRequest()
{
    EeWonGetCertHooks h = this->Hooks();
    return (WONStatus)ee_won_getcert_get_next_request(this->mCert, &h);
}

// FUNCTION: WON 0x006e5980
// Checks the auth server reply and handles the key block, the login challenge (0x006e5800) or the login reply
// (0x006e4880); a reply cut short throws as the original's reads did
WONStatus GetCertOp::CheckResponse()
{
    EeWonGetCertHooks h = this->Hooks();
    bool aCut = false;
    long aStatus = ee_won_getcert_check_response(this->mCert, &h, &aCut);
    if (aCut)
        throw ReadBufferException("Attempt to read past end of buffer.");
    return (WONStatus)aStatus;
}

const AuthPeerData* GetCertOp::GetPeerData() const
{
    return GetCertOpHooks::Peer(ee_won_getcert_peer_data(this->mCert));
}
void GetCertOp::SetCreateAccount(bool value)
{
    ee_won_getcert_set_create_account(this->mCert, value);
}
void GetCertOp::SetNewPassword(const std::wstring& password)
{
    std::string b;
    WideAppendUtf16LE(b, password);
    ee_won_getcert_set_new_password(this->mCert, reinterpret_cast<const uint8_t*>(b.data()), b.size());
}
void GetCertOp::SetAuthContext(AuthContext* context)
{
    EeWonGetCertHooks h = this->Hooks();
    ee_won_getcert_set_context(this->mCert, &h, GetCertOpHooks::V(context));
}
// the C++ Blowfish is one EeWonBlowfish member: the Rust object's key is one
const Blowfish& GetCertOp::GetSessionKey() const
{
    return *reinterpret_cast<const Blowfish*>(ee_won_getcert_session_key(this->mCert));
}
GetCertOp::ERROR_LIST GetCertOp::GetErrorList() const
{
    ERROR_LIST aList;
    for (size_t i = 0; i < ee_won_getcert_error_count(this->mCert); i++) {
        aList.push_back(std::string());
        ee_won_getcert_error(this->mCert, i, GetCertOpHooks::StrSink, &aList.back());
    }
    return aList;
}
GetCertOp::COMMUNITY_STATUS_LIST GetCertOp::GetCommunityStatusList() const
{
    COMMUNITY_STATUS_LIST aList;
    for (size_t i = 0; i < ee_won_getcert_community_count(this->mCert); i++) {
        short aStatus = 0;
        uint32_t anId = 0;
        std::string aName;
        ee_won_getcert_community(this->mCert, i, &aStatus, &anId, GetCertOpHooks::StrSink, &aName);
        aList.push_back(COMMUNITY_STATUS());
        aList.back().mStatus = aStatus;
        aList.back().mCommunityID = anId;
        WideAssignUtf16LE(aList.back().mCommunityName, aName.data(), (unsigned long)(aName.size() / 2));
    }
    return aList;
}

}
