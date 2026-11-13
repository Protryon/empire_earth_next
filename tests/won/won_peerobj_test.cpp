// The certificate classes, the peer data and the peer authentication objects (WONAuth's AuthBase, AuthCertificate,
// Auth2Certificate, AuthPubKeyBlock, AuthPeerData, PeerAuthClient and PeerAuthServer): the original's statements
// (tests/won/won_auth_orig.h, copied before round 14 of the port) against the game's classes over ee_won's objects
// (peerobj.rs), on the same generated scenarios.
//
// A scenario is real key material (small ElGamal keys over one group), certificates and key blocks signed by an
// auth server's key or a wrong one, cut, mutated or unsigned, each side's peer data (empty, or certificates, key
// block, private key and clock delta), the handshake's options, and a plan of mutations of the messages in flight.
// Each half builds its objects, reads every accessor and runs the handshake between a client and a server, from
// the same random seed (ee_won_random_test_seed: the secrets and the encryption draw from it, so both halves must
// draw in the same order): the transcripts must be equal. Times are chosen so the answers do not change for years
// (expiries far in the past or in 2037, clock deltas that cross them or not).
//
// `--print N` prints N scenarios with the original's transcript (rust/crates/ee_won/src/golden_peerobj.txt; the Rust
// replay is `peerobj::tests::golden`). The exit code is the number of failed checks.
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
#include "won_auth_orig.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 1414;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const void* p, size_t n)
{
    static const char kHex[] = "0123456789abcdef";
    const unsigned char* b = static_cast<const unsigned char*>(p);
    string r;
    for (size_t i = 0; i < n; i++) { r += kHex[b[i] >> 4]; r += kHex[b[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string Hex(const string& s) { return Hex(s.data(), s.size()); }
static string Num(long long v)
{
    char b[32];
    sprintf(b, "%lld", v);
    return b;
}

// ---- key material: four ElGamal keys over one 192-bit group (made once with ee_won's bigint): 0 the auth server's
// (it signs), 1 a wrong signer, 2 the client's, 3 the server's

static const char* const kPriv[4] = {
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02184bb17e485a32d0a3f02eb0bc5d2b9a09f3cb369e38ff200602080c931814d44a5e5b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02183dda7c54fd793a4e534f60facc1301e37953f4df16f49861020808e58d3749117c1b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a0218467548d5e50f1f981f842a63ceebe356665d1ec15fcdc85d02080f47ec32536bc8f1",
};
static string Unhex(const char* h)
{
    string r;
    for (; h[0] && h[1]; h += 2) {
        int v = 0;
        for (int i = 0; i < 2; i++) v = v * 16 + (h[i] <= '9' ? h[i] - '0' : h[i] - 'a' + 10);
        r += (char)v;
    }
    return r;
}
// key k's DER, private (p, q, g, y, x) or public (the sequence without x: its length less x's ten bytes)
static string Der(unsigned long long k, bool priv)
{
    string d = Unhex(kPriv[k]);
    if (priv) return d;
    d.resize(d.size() - 10);
    d[1] = (char)(d.size() - 2);
    return d;
}

// ---- little-endian writing (WON's wire)
struct W {
    string d;
    void B(int v) { d += (char)(v & 0xff); }
    void S(int v) { B(v); B(v >> 8); }
    void L(long long v) { S((int)(v & 0xffff)); S((int)((v >> 16) & 0xffff)); }
    void Str(const string& s) { S((int)s.size()); d += s; }
    void WStr(const char* ascii) { S((int)strlen(ascii)); for (const char* c = ascii; *c; c++) S(*c); }
};

static const long long kFuture = 0x7f000000; // 2037
static const long long kPast = 1000;
static const int kCommunity = 42;

// a signed block: the bytes then the signature of the signer's private key (C++ ElGamal::Sign)
static string Signed(const string& data, unsigned long long signer, int how)
{
    // how: 0 signed by the signer, 1 unsigned, 2 cut, 3 a data byte flipped after signing
    if (how == 1) return data;
    string key = Der(signer, true);
    ElGamal k(key.data(), (int)key.size(), true);
    ByteBufferPtr sig = k.Sign(data.data(), (int)data.size());
    string raw = data + (sig.get() ? string(sig->data(), sig->length()) : string());
    if (how == 2) raw.resize(Pick((int)raw.size()));
    if (how == 3 && !raw.empty()) raw[Pick((int)data.size())] ^= (char)(1 + Pick(255));
    return raw;
}

struct Side {
    unsigned long long priv; // the private key (kPriv's index)
    string c1, c2;            // the certificates' bytes
    bool hasC2;               // false: NULL (full peer data only)
    bool full;                // the full constructor (else AuthPeerData(block))
    long long delta;
    bool useAuth2;
};
struct Scenario {
    int run;
    unsigned long long signer;
    string block;
    Side c, s;
    bool setKey, useSecret;
    int keySize, type, lfs;
    std::vector<int> muts; // per message: kind, a, b
};

static string Cert1(unsigned long long key, long long user, int how, unsigned long long signer)
{
    W w;
    w.S(1);
    w.L(1000);
    w.L(how == 4 ? kPast : kFuture);
    w.L(user);
    w.L(Pick(4) == 0 ? 7 : kCommunity);
    w.S(Pick(10));
    string k = Der(key, false);
    w.Str(k);
    return Signed(w.d, signer, how == 4 ? 0 : how);
}
static string Cert2(unsigned long long key, long long user, int how, unsigned long long signer)
{
    W w;
    w.S(2);
    w.L(1000);
    w.L(how == 4 ? kPast : kFuture);
    int extra = Pick(3);
    w.S(1 + extra);
    W r;
    r.L(user);
    r.Str(Der(key, false));
    r.WStr("Player");
    int n = Pick(3);
    r.B(n);
    for (int i = 0; i < n; i++) { r.L(i == 0 ? kCommunity : 100 + i); r.S(Pick(10)); }
    w.S(0); w.S((int)r.d.size()); w.d += r.d;
    for (int i = 0; i < extra; i++) {
        W x;
        int kind = 1 + Pick(3);
        if (kind == 1) { x.L(kCommunity); x.Str("udata"); }
        else if (kind == 2) { x.WStr("k1"); x.WStr("Nick"); }
        else { x.L(kCommunity); x.L(99); }
        w.S(kind); w.S((int)x.d.size()); w.d += x.d;
    }
    return Signed(w.d, signer, how == 4 ? 0 : how);
}
// 0 signed, 1 unsigned, 2 cut, 3 a byte flipped, 4 expired (mostly signed)
static int How() { static const int k[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4 }; return k[Pick(17)]; }
static unsigned long long Signer(unsigned long long good) { return Pick(16) == 0 ? 1 : good; }

static Scenario Generate(int index)
{
    Scenario sc;
    ee_won_random_test_seed(100000 + index); // the signatures
    sc.run = 7000 + index;
    sc.signer = 0;
    W b;
    b.S(3); b.L(1000); b.L(Pick(20) == 0 ? kPast : kFuture); b.S(17);
    int keys = Pick(6) == 0 ? 2 : 1;
    b.S(keys);
    for (int i = 0; i < keys; i++) b.Str(Der(i == 0 ? sc.signer : 1, false));
    int bh = Pick(40);
    sc.block = Signed(b.d, sc.signer, bh < 37 ? 0 : bh - 36);
    unsigned long long ck = 2, sk = 3;
    Side* sides[2] = { &sc.c, &sc.s };
    unsigned long long own[2] = { ck, sk };
    for (int i = 0; i < 2; i++) {
        Side& d = *sides[i];
        d.full = Pick(12) != 0;
        d.priv = Pick(10) == 0 ? (own[i] ^ 1) : own[i]; // the other side's key at times
        d.c1 = Cert1(own[i], 1000 + i, How(), Signer(sc.signer));
        d.c2 = Cert2(own[i], 2000 + i, How(), Signer(sc.signer));
        d.hasC2 = !d.full || Pick(5) != 0;
        static const long long kDeltas[] = { 0, 0, 0, 0, 0, 0, 3600, -3600, 400000000, -2000000000 };
        d.delta = kDeltas[Pick(10)];
        d.useAuth2 = Pick(3) != 0;
    }
    if (Pick(8) != 0) sc.s.useAuth2 = sc.c.useAuth2 && (sc.c.hasC2 || Pick(2)); // both sides' kinds agree mostly
    if (!sc.s.hasC2) sc.s.useAuth2 = false; // the original's server read the NULL certificate
    sc.setKey = Pick(2) != 0;
    sc.keySize = 5 + Pick(10);
    sc.useSecret = Pick(2) != 0;
    static const int kTypes[] = { 2, 2, 2, 2, 3, 3, 3, 1, 0 };
    sc.type = kTypes[Pick(9)];
    sc.lfs = Pick(2) ? 4 : 2;
    for (int i = 0; i < 6; i++) {
        int k = Pick(16);
        int kind = k < 14 ? 0 : k - 13;
        sc.muts.push_back(kind);
        sc.muts.push_back(Pick(400));
        sc.muts.push_back(1 + Pick(255));
    }
    return sc;
}

// a message as the other side's RecvMsg gives it (without its length field), mutated by the plan
static string Mutate(const ByteBuffer* inMsg, const Scenario& sc, int step, string& log)
{
    string m(inMsg->data(), inMsg->length());
    m.erase(0, m.size() < (size_t)sc.lfs ? m.size() : (size_t)sc.lfs);
    int kind = sc.muts[step * 3], a = sc.muts[step * 3 + 1], b = sc.muts[step * 3 + 2];
    if (kind == 0 || m.empty()) return m;
    string r = m;
    if (kind == 1) r[a % r.size()] ^= (char)b;
    else r.resize(a % r.size());
    log += " mut" + Num(kind);
    return r;
}

// ---- one half: H's types and object makers

static string B(bool v) { return v ? "1" : "0"; }

template <class H> struct Half {
    typedef typename H::Cert1 C1;
    typedef typename H::Cert2 C2;
    typedef typename H::Block K;
    typedef typename H::Peer P;

    static string QCert(const typename H::CertBase* c, const K* block, long long delta, bool v2)
    {
        string t = "v" + B(c->IsValid()) + " i" + Num(c->GetIssueTime()) + " e" + Num(c->GetExpireTime()) +
                   " u" + Num(H::UserId(c)) + " x" + B(c->IsExpired(0)) + B(c->IsExpired(delta)) +
                   " p" + B(c->VerifyPermissions(0, 0, 0)) + B(c->VerifyPermissions(1000, 0, 0)) +
                   B(c->VerifyPermissions(0, kCommunity, 5)) + " b" + B(block->Verify(c));
        if (v2) {
            const C2* c2 = static_cast<const C2*>(c);
            t += " f" + Num(c2->GetFirstCommunity()) + " t" + Num(c2->GetFirstCommunityTrust()) + " k" +
                 Num(c2->GetKeyId(c2->GetFirstCommunity()));
        }
        return t;
    }
    static string QPeer(const P* p)
    {
        // IsValid2 and Certificate2Expired dereferenced a NULL second certificate
        bool has2 = p->GetCertificate2() != NULL;
        return "V" + B(p->IsValid()) + " W" + (has2 ? B(p->IsValid2()) : "N") + " X" + B(p->CertificateExpired()) +
               B(p->CertificateExpired(120)) + " Z" + (has2 ? B(p->Certificate2Expired()) : "N") +
               " D" + B(p->GetExpireDelta() > 0) + " A" + Num(p->GetAuthDelta()) + " P" + B(H::Private(p));
    }

    static string Run(const Scenario& sc)
    {
        ee_won_random_test_seed(sc.run);
        string t;
        SmartPtr<K> block = H::NewBlock(sc.block);
        string sig = Der(sc.signer, false);
        ElGamal signer(sig.data(), (int)sig.size(), false);
        t += "B v" + B(block->IsValid()) + " id" + Num(block->GetId()) + " vv" + B(block->IsValidAndVerified(signer)) +
             " k" + B(H::HasKey(block.get())) + " i" + Num(block->GetIssueTime()) + " e" + Num(block->GetExpireTime()) +
             " x" + B(block->IsExpired(0));
        ConstSmartPtr<P> peers[2];
        const Side* sides[2] = { &sc.c, &sc.s };
        for (int i = 0; i < 2; i++) {
            const Side& d = *sides[i];
            if (!d.full) {
                peers[i] = H::NewPeer(block.get());
                t += i ? " |S " : " |C ";
                t += QPeer(peers[i].get());
                continue;
            }
            SmartPtr<C1> c1 = H::NewCert1(d.c1);
            SmartPtr<C2> c2 = d.hasC2 ? H::NewCert2(d.c2) : SmartPtr<C2>();
            string pk = Der(d.priv, true);
            ElGamal key(pk.data(), (int)pk.size(), true);
            peers[i] = H::NewPeer(c1.get(), block.get(), key, d.delta, c2.get());
            t += i ? " |S " : " |C ";
            t += "c1 " + QCert(c1.get(), block.get(), d.delta, false);
            if (c2.get()) t += " c2 " + QCert(c2.get(), block.get(), d.delta, true);
            t += " " + QPeer(peers[i].get());
        }
        // the handshake
        typename H::Client client;
        typename H::Server server;
        client.SetUseAuth2(sc.c.useAuth2);
        if (sc.setKey) client.SetClientKeySize((unsigned char)sc.keySize, sc.useSecret);
        server.SetUseAuth2(sc.s.useAuth2);
        server.Start(peers[1].get(), (unsigned char)sc.lfs);
        ByteBufferPtr msg = client.Start(peers[0].get(), (AuthType)sc.type, (unsigned char)sc.lfs);
        t += " |H c>" + Hex(msg->data(), msg->length());
        int step = 0;
        for (int round = 0; round < 3 && msg.get() != NULL; round++) {
            string m = Mutate(msg.get(), sc, step++, t);
            ByteBufferPtr out;
            WONStatus st = server.HandleRecvMsg(m.data(), (unsigned long)m.size(), out);
            t += " s" + Num(st) + "." + Num(server.GetState()) + ">" + (out.get() ? Hex(out->data(), out->length()) : "N");
            if (out.get() == NULL) break;
            m = Mutate(out.get(), sc, step++, t);
            st = client.HandleRecvMsg(m.data(), (unsigned long)m.size(), msg);
            t += " c" + Num(st) + "." + Num(client.GetState()) + ">" + (msg.get() ? Hex(msg->data(), msg->length()) : "N");
        }
        AuthSession* cs = client.GetSession();
        AuthSession* ss = server.GetSession();
        t += " |cs" + (cs ? Num(cs->GetAuthType()) + "." + Num(cs->GetSessionId()) + "." +
                                Hex(cs->GetKey().GetKey(), cs->GetKey().GetKeyLen())
                          : string("N"));
        t += " ss" + (ss ? Num(ss->GetAuthType()) + "." + Num(ss->GetSessionId()) + "." +
                                Hex(ss->GetKey().GetKey(), ss->GetKey().GetKeyLen())
                          : string("N"));
        t += " sc" + H::OtherCert(client) + " cc" + H::OtherCert(server);
        return t;
    }
};

struct OrigHalf {
    typedef orig::AuthCertificateBase CertBase;
    typedef orig::AuthCertificate Cert1;
    typedef orig::Auth2Certificate Cert2;
    typedef orig::AuthPubKeyBlock Block;
    typedef orig::AuthPeerData Peer;
    typedef orig::PeerAuthClient Client;
    typedef orig::PeerAuthServer Server;
    static SmartPtr<Block> NewBlock(const string& b) { return new Block(b.data(), (unsigned long)b.size()); }
    static SmartPtr<Cert1> NewCert1(const string& b) { return new Cert1(b.data(), (unsigned long)b.size()); }
    static SmartPtr<Cert2> NewCert2(const string& b) { return new Cert2(b.data(), (unsigned long)b.size()); }
    static Peer* NewPeer(const Block* k) { return new Peer(k); }
    static Peer* NewPeer(const Cert1* c, const Block* k, const ElGamal& key, time_t d, const Cert2* c2)
    {
        return new Peer(c, k, key, d, c2);
    }
    static long long UserId(const CertBase* c) { return c->GetUserId(); }
    static bool HasKey(const Block* k) { return k->HasKey(); }
    static bool Private(const Peer* p) { return p->GetPrivateKey().IsPrivate(); }
    static string OtherCert(const CertBase* c)
    {
        return c ? B(c->IsValid()) + "." + Num(c->GetUserId()) : string("N");
    }
    static string OtherCert(Client& c) { return OtherCert(c.GetServerCertificate()); }
    static string OtherCert(Server& s) { return OtherCert(s.GetClientCertificate()); }
};

// ---- the game's classes: AuthBase and AuthPeerData over ee_won's objects (peerobj.rs), read through their numbers
// as the original's accessors

// PeerAuthClient / PeerAuthServer as the game's shims over ee_won's handshake object were in round 14's first
// step (the ops own the object since; ee_won_peerauth_* stays for this driver and the ops' driver)
class PeerAuth {
    EeWonPeerAuth* mImpl;
    SmartPtr<AuthSession> mSession;
    PeerAuth(const PeerAuth&);
    PeerAuth& operator=(const PeerAuth&);
protected:
    explicit PeerAuth(bool inServer) : mImpl(ee_won_peerauth_new(inServer)) {}
    EeWonPeerAuth* Impl() const { return this->mImpl; }
    void TakeSession()
    {
        if (EeWonAuthSession* aSession = ee_won_peerauth_take_session(this->mImpl))
            this->mSession = new AuthSession(aSession);
    }
public:
    ~PeerAuth() { ee_won_peerauth_free(this->mImpl); }
    WONStatus HandleRecvMsg(const void* inMsg, unsigned long inLength, ByteBufferPtr& outMsg)
    {
        outMsg = NULL;
        bool aHasOut = false;
        WONStatus aStatus = ee_won_peerauth_recv(this->mImpl, static_cast<const uint8_t*>(inMsg), inLength, WonByteBufferSink,
                                                 &outMsg, &aHasOut);
        this->TakeSession();
        return aStatus;
    }
    AuthSession* GetSession() { return this->mSession.get(); }
    int GetState() const { return (int)ee_won_peerauth_num(this->mImpl, EE_WON_PEERAUTH_STATE, 0, 0, 0); }
    void SetUseAuth2(bool inValue) { ee_won_peerauth_set(this->mImpl, EE_WON_PEERAUTH_USE_AUTH2, inValue); }
    // the other side's certificate (EE_WON_PEERAUTH_OTHER, _OTHER_USER, _OTHER_PERMITS of a, b, c)
    long long Other(int inWhich, long long inA = 0, long long inB = 0, long long inC = 0) const
    {
        return ee_won_peerauth_num(this->mImpl, inWhich, inA, inB, inC);
    }
    bool OtherPermits(DWORD inUser, DWORD inCommunity, unsigned short inTrust) const
    {
        return this->Other(EE_WON_PEERAUTH_OTHER_PERMITS, inUser, inCommunity, inTrust) != 0;
    }
};
class PeerAuthClient : public PeerAuth {
public:
    enum PeerAuthClientState { STATE_NOT_STARTED=0, STATE_AWAITING_CHALLENGE=1, STATE_AWAITING_COMPLETE=2 };
    PeerAuthClient() : PeerAuth(false) {} //2e5c30
    ByteBufferPtr Start(const AuthPeerData* inData, AuthType inType, unsigned char inLengthFieldSize) //2e6730
    {
        ByteBufferPtr aMsg;
        ee_won_peerauth_start(this->Impl(), inData != NULL ? inData->Impl() : NULL, inType, inLengthFieldSize,
                              WonByteBufferSink, &aMsg);
        return aMsg;
    }
    void Reset() { ee_won_peerauth_set(this->Impl(), EE_WON_PEERAUTH_RESET, 0); } //2e5cb0
    void SetClientKeySize(unsigned char inSize, bool inUseSecret = true)
    {
        ee_won_peerauth_set(this->Impl(), EE_WON_PEERAUTH_CLIENT_KEY, inSize | (inUseSecret ? 0x100 : 0));
    }
};
class PeerAuthServer : public PeerAuth {
public:
    enum PeerAuthServerState { STATE_NOT_STARTED=0, STATE_AWAITING_REQUEST=1, STATE_AWAITING_CHALLENGE2=2 };
    PeerAuthServer() : PeerAuth(true) {} //2e7cc0
    void Start(const AuthPeerData* inData, unsigned char inLengthFieldSize) //2e7d30
    {
        ee_won_peerauth_start(this->Impl(), inData != NULL ? inData->Impl() : NULL, 0, inLengthFieldSize, WonByteBufferSink,
                              NULL);
    }
};

struct NCert : AuthBase {
    NCert(uint32_t inKind, const string& b) : AuthBase(inKind, b.data(), (unsigned long)b.size()) {}
    bool IsExpired(time_t e) const { return this->Num(EE_WON_AUTHOBJ_EXPIRED, e) != 0; }
    DWORD GetId() const { return (DWORD)this->Num(EE_WON_AUTHOBJ_BLOCK_ID); }
    DWORD GetFirstCommunity() const { return (DWORD)this->Num(EE_WON_AUTHOBJ_FIRST_COMMUNITY); }
    unsigned short GetFirstCommunityTrust() const { return (unsigned short)this->Num(EE_WON_AUTHOBJ_FIRST_TRUST); }
    DWORD GetKeyId(DWORD c) const { return (DWORD)this->Num(EE_WON_AUTHOBJ_KEY_ID, c); }
};
struct NPeer : AuthPeerData {
    explicit NPeer(const AuthBase* k) : AuthPeerData(k) {}
    NPeer(const AuthBase* c, const AuthBase* k, const ElGamal& key, time_t d, const AuthBase* c2)
        : AuthPeerData(c, k, key, d, c2) {}
    bool IsValid2() const { return this->Num(EE_WON_AUTHOBJ_PEER_VALID2) != 0; }
    const void* GetCertificate2() const { return this->Num(EE_WON_AUTHOBJ_PEER_HAS_CERT2) ? this : NULL; }
};
struct NewHalf {
    typedef NCert CertBase;
    typedef NCert Cert1;
    typedef NCert Cert2;
    typedef NCert Block;
    typedef NPeer Peer;
    typedef PeerAuthClient Client;
    typedef PeerAuthServer Server;
    static SmartPtr<Block> NewBlock(const string& b) { return new NCert(EE_WON_AUTH_PUB_KEY_BLOCK, b); }
    static SmartPtr<Cert1> NewCert1(const string& b) { return new NCert(EE_WON_AUTH_CERTIFICATE, b); }
    static SmartPtr<Cert2> NewCert2(const string& b) { return new NCert(EE_WON_AUTH_CERTIFICATE2, b); }
    static Peer* NewPeer(const Block* k) { return new Peer(k); }
    static Peer* NewPeer(const Cert1* c, const Block* k, const ElGamal& key, time_t d, const Cert2* c2)
    {
        return new Peer(c, k, key, d, c2);
    }
    static long long UserId(const CertBase* c) { return c->Num(EE_WON_AUTHOBJ_USER_ID); }
    static bool HasKey(const Block* k) { return k->Num(EE_WON_AUTHOBJ_HAS_KEY) != 0; }
    static bool Private(const Peer* p) { return p->Num(EE_WON_AUTHOBJ_PEER_PRIVATE) != 0; }
    static string OtherCert(const PeerAuth& a)
    {
        long long k = a.Other(EE_WON_PEERAUTH_OTHER);
        return k ? B(k == 2) + "." + Num(a.Other(EE_WON_PEERAUTH_OTHER_USER)) : string("N");
    }
};

// ---- the golden's line: the scenario, then the original's transcript

static string Line(const Scenario& sc, const string& t)
{
    string l = "P " + Num(sc.run) + " " + Num(sc.signer) + " " + Hex(sc.block);
    const Side* sides[2] = { &sc.c, &sc.s };
    for (int i = 0; i < 2; i++) {
        const Side& d = *sides[i];
        l += " " + B(d.full) + " " + Num((long long)d.priv) + " " + Hex(d.c1) + " " + (d.hasC2 ? Hex(d.c2) : string("N")) +
             " " + Num(d.delta) + " " + B(d.useAuth2);
    }
    l += " " + B(sc.setKey) + " " + Num(sc.keySize) + " " + B(sc.useSecret) + " " + Num(sc.type) + " " + Num(sc.lfs) + " ";
    for (size_t i = 0; i < sc.muts.size(); i++) l += (i ? "," : "") + Num(sc.muts[i]);
    return l + " => " + t;
}

int main(int argc, char** argv)
{
    int print = 0, count = 3000;
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--print")) print = atoi(argv[i + 1]);
        if (!strcmp(argv[i], "--count")) count = atoi(argv[i + 1]);
    }
    if (print) {
        for (int i = 0; i < print; i++) {
            Scenario sc = Generate(i);
            printf("%s\n", Line(sc, Half<OrigHalf>::Run(sc)).c_str());
        }
        return 0;
    }
    std::map<string, int> outcomes;
    for (int i = 0; i < count; i++) {
        Scenario sc = Generate(i);
        string a = Half<OrigHalf>::Run(sc);
        string b = Half<NewHalf>::Run(sc);
        CHECK(a == b);
        if (a != b && sFailures < 4)
            printf("scenario %d\n  original %s\n  game     %s\n", i, a.c_str(), b.c_str());
        size_t h = a.find("|cs");
        outcomes[a.substr(h, 5)]++;
    }
    for (std::map<string, int>::iterator i = outcomes.begin(); i != outcomes.end(); ++i)
        printf("  %s %d\n", i->first.c_str(), i->second);
    printf("won_peerobj_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
