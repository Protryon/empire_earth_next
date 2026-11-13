// PeerAuthOp and PeerAuthServerOp (WONAuth/PeerAuthOp.cpp, PeerAuthServerOp.cpp): the original's statements (copied
// before round 14 of the port; `this->` an op stand-in, every C++ object it touches a numbered stand-in, the
// handshake the original's PeerAuthClient / PeerAuthServer of won_auth_orig.h) against the Rust ops on the op base
// (ee_won peerop.rs over peerobj.rs) through the base's one C call, both over the same stand-in: one set of
// functions answering every call from a generated stream and logging it with its answer.
//
// The other end of the socket is a real handshake (ee_won's peerobj object, the same for both halves): every message
// the op sends reaches it, its answers wait to be received; the stand-in also fails connects, sends and receives,
// hands over garbage, NULL or mutated messages, refreshes the certificate or not. Asynchronous runs complete the
// queued ops and the certificate refresh in order and call the op back. Every key, secret and encryption draws
// from one fixed seed (ee_won_random_test_seed), so both halves must draw in the same order.
//
// `--print N` prints N sequences with the original's trace, the answers in it (rust/crates/ee_won/src/
// golden_peerop.txt; `peerop::tests::golden` replays it); `--count N` runs N. The exit code is the number of failed
// checks.
#include <windows.h>
#include <stdarg.h>
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

static unsigned int sSeed = 1515;
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

// ---- key material and certificates (won_peerobj_test's keys: 0 the auth server, 1 a wrong signer, 2 the
// client, 3 the server)

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
static string Der(int k, bool priv)
{
    string d = Unhex(kPriv[k]);
    if (priv) return d;
    d.resize(d.size() - 10);
    d[1] = (char)(d.size() - 2);
    return d;
}
struct W {
    string d;
    void B(int v) { d += (char)(v & 0xff); }
    void S(int v) { B(v); B(v >> 8); }
    void L(long long v) { S((int)(v & 0xffff)); S((int)((v >> 16) & 0xffff)); }
    void Str(const string& s) { S((int)s.size()); d += s; }
    void WStr(const char* ascii) { S((int)strlen(ascii)); for (const char* c = ascii; *c; c++) S(*c); }
};
static const long long kFuture = 0x7f000000, kPast = 1000;
static const int kCommunity = 42;
static string Signed(const string& data, int signer)
{
    string key = Der(signer, true);
    ElGamal k(key.data(), (int)key.size(), true);
    ByteBufferPtr sig = k.Sign(data.data(), (int)data.size());
    return data + string(sig->data(), sig->length());
}
static string Cert1(int key, long long user, bool expired, int signer)
{
    W w;
    w.S(1); w.L(1000); w.L(expired ? kPast : kFuture); w.L(user); w.L(kCommunity); w.S(5);
    w.Str(Der(key, false));
    return Signed(w.d, signer);
}
static string Cert2(int key, long long user, bool expired, int signer)
{
    W w;
    w.S(2); w.L(1000); w.L(expired ? kPast : kFuture); w.S(1);
    W r;
    r.L(user); r.Str(Der(key, false)); r.WStr("Player"); r.B(1); r.L(kCommunity); r.S(5);
    w.S(0); w.S((int)r.d.size()); w.d += r.d;
    return Signed(w.d, signer);
}

// a peer data's inputs: empty (AuthPeerData(block)) or full
struct PeerSpec {
    bool full;
    int priv;
    string c1, c2; // c2 empty: NULL
    long long delta;
};
static PeerSpec MakePeer(int key, long long user, bool expired, bool full)
{
    PeerSpec p;
    p.full = full;
    p.priv = key;
    int signer = Pick(16) == 0 ? 1 : 0;
    p.c1 = Cert1(key, user, expired, signer);
    p.c2 = Pick(5) != 0 ? Cert2(key, user + 1000, expired, signer) : string();
    p.delta = Pick(10) == 0 ? 400000000 : 0;
    return p;
}

struct Scenario {
    int run;            // the random seed of the keys and secrets
    unsigned int script; // the stand-in's answers
    bool server, async, useAuth2;
    int type, lfs, clientKey;
    string block;
    PeerSpec ctx, refreshed, other; // the auth context's peer data before and after a refresh, the other end's
    bool otherAuth2;
    int otherType, otherKey;
};

static Scenario Generate(int index)
{
    Scenario sc;
    ee_won_random_test_seed(200000 + index); // the signatures
    sc.run = 9000 + index;
    sc.script = 77 + index * 7919u;
    sc.server = Pick(2) != 0;
    sc.async = Pick(2) != 0;
    sc.useAuth2 = Pick(3) != 0;
    static const int kTypes[] = { 2, 2, 2, 3, 3, 1, 0 };
    sc.type = kTypes[Pick(7)];
    sc.lfs = Pick(2) ? 4 : 2;
    sc.clientKey = Pick(3) == 0 ? (5 + Pick(10)) | (Pick(2) << 8) : 8;
    W b;
    b.S(3); b.L(1000); b.L(kFuture); b.S(17); b.S(1); b.Str(Der(0, false));
    sc.block = Signed(b.d, 0);
    int own = sc.server ? 3 : 2, them = sc.server ? 2 : 3;
    sc.ctx = MakePeer(own, sc.server ? 1001 : 1000, Pick(5) == 0, Pick(12) != 0);
    sc.refreshed = MakePeer(own, sc.server ? 1001 : 1000, Pick(4) == 0, true);
    sc.other = MakePeer(them, sc.server ? 1000 : 1001, Pick(10) == 0, Pick(16) != 0);
    // the server op's PeerAuthServer keeps its version 2 default
    sc.otherAuth2 = Pick(5) != 0 ? (sc.server || sc.useAuth2) : !(sc.server || sc.useAuth2);
    if (sc.ctx.c2.empty() && sc.ctx.full && sc.server) sc.otherAuth2 = false; // the server read its NULL certificate
    if (sc.other.c2.empty() && sc.other.full && !sc.server) sc.otherAuth2 = false;
    sc.otherType = sc.server ? kTypes[Pick(6)] : 0; // a client at the other end: rarely an invalid mode
    sc.otherKey = Pick(3) == 0 ? (5 + Pick(10)) | (Pick(2) << 8) : 8;
    return sc;
}

// ---- the stand-in: every call answered from the script's stream and logged with its answer

struct Standin {
    unsigned int mSeed;
    int mSerial, mStatus, mLfs, mKey, mType;
    bool mAsync;
    string mTrace;
    std::list<string> mInbox;            // the other end's messages waiting (with their length field)
    std::list<int> mPending;             // queued ops and refreshes, in order
    std::map<int, int> mKinds;           // object -> kind (0 connect, 1 send, 2 recv, 3 light, 4 session)
    std::map<int, int> mTrack;           // object -> its tracking id
    std::map<int, int> mDone;            // a completed op's status
    std::map<int, string> mBytes;        // a completed receive's bytes ("" with mNull: NULL)
    std::set<int> mNull;
    std::map<int, EeWonAuthSession*> mSessions; // the Rust half's sessions by object
    EeWonPeerAuth* mOther;               // the other end
    int mLightPeer;                      // the peer data a refresh gives (1 the context's, 2 the refreshed)
    int mCtxPeer;                        // the auth context's current peer data
    int mCalls;
};
static Standin S;

static unsigned int ANext()
{
    S.mSeed = S.mSeed * 1103515245u + 12345u;
    return (S.mSeed >> 8) & 0xffffff;
}
static int APick(int n) { return (int)(ANext() % (unsigned)n); }
static void Log(const string& s) { S.mTrace += s; S.mTrace += ';'; S.mCalls++; }
static string O(int o) { return "o" + Num(o); }

static void OtherSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
// a message the op sends reaches the other end; its answer waits in the inbox
static void Feed(const string& m)
{
    string body = m.substr(m.size() < (size_t)S.mLfs ? m.size() : (size_t)S.mLfs);
    string out;
    bool has = false;
    ee_won_peerauth_recv(S.mOther, reinterpret_cast<const uint8_t*>(body.data()), body.size(), OtherSink, &out, &has);
    EeWonAuthSession* s = ee_won_peerauth_take_session(S.mOther);
    if (s) ee_won_auth_session_free(s);
    if (has) S.mInbox.push_back(out);
}
// what a receive gets: a failure, NULL, garbage, a mutation, or the next waiting message (without its length field)
static int RecvAnswer(string& outBytes, bool& outNull)
{
    outNull = false;
    int k = APick(24);
    if (k == 0 || S.mCalls > 200) return 100001;
    if (k == 1) { outNull = true; return 0; }
    if (k == 2) { int n = APick(12); for (int i = 0; i < n; i++) outBytes += (char)ANext(); return 0; }
    if (S.mInbox.empty()) return 0;
    string m = S.mInbox.front();
    S.mInbox.pop_front();
    outBytes = m.substr(m.size() < (size_t)S.mLfs ? m.size() : (size_t)S.mLfs);
    if (k == 3 && !outBytes.empty()) outBytes[APick((int)outBytes.size())] ^= (char)(1 + APick(255));
    return 0;
}
static string BytesText(const string& b, bool isNull) { return isNull ? string("N") : Hex(b); }

// the calls, each logged with its answer
static int HConnect(int s, unsigned long t)
{
    int st = (APick(12) == 0 || S.mCalls > 200) ? 100001 : 0;
    Log("connect(" + O(s) + "," + Num(t) + ")=" + Num(st));
    return st;
}
static int HSend(int s, const string& m, unsigned long t)
{
    int st = (APick(16) == 0 || S.mCalls > 200) ? 100002 : 0;
    Log("send(" + O(s) + "," + Hex(m) + "," + Num(t) + ")=" + Num(st));
    if (st == 0) Feed(m);
    return st;
}
static int HRecv(int s, unsigned long t, string& outBytes, bool& outNull)
{
    int st = RecvAnswer(outBytes, outNull);
    Log("recv(" + O(s) + "," + Num(t) + ")=" + Num(st) + ":" + BytesText(outBytes, outNull || st != 0));
    return st;
}
static int HNewOp(int kind, int s, const string& m)
{
    int o = ++S.mSerial;
    S.mKinds[o] = kind;
    Log("newop(" + Num(kind) + "," + O(s) + "," + Hex(m) + ")=" + O(o));
    if (kind == 1) Feed(m); // the send's message is on its way
    return o;
}
static void HTrack(int o, int id) { S.mTrack[o] = id; Log("track(" + O(o) + "," + Num(id) + ")"); }
static void HQueue(int s, int o, unsigned long t)
{
    Log("queue(" + O(s) + "," + O(o) + "," + Num(t) + ")");
    S.mPending.push_back(o);
}
static void HFinish(int st)
{
    Log("finish(" + Num(st) + ")");
    if (S.mStatus == WS_Pending) S.mStatus = st;
}
static int HCtxPeer() { Log("ctxpeer=p" + Num(S.mCtxPeer)); return S.mCtxPeer; }
static int HRefreshBlock(unsigned long t)
{
    int st = APick(4) == 0 ? 100011 : 0;
    Log("refresh(" + Num(t) + ")=" + Num(st));
    if (st == 0) S.mCtxPeer = 2;
    return st;
}
static int HNewLight()
{
    int o = ++S.mSerial;
    S.mKinds[o] = 3;
    Log("light=" + O(o));
    return o;
}
static void HRefreshAsync(int o) { Log("refreshasync(" + O(o) + ")"); S.mPending.push_back(o); }
static int HOpStatus(int o) { int st = S.mDone[o]; Log("opstatus(" + O(o) + ")=" + Num(st)); return st; }
static bool HIsRecv(int o) { bool r = S.mKinds[o] == 2; Log("isrecv(" + O(o) + ")=" + Num(r)); return r; }
static string HOpBytes(int o, bool& outNull)
{
    outNull = S.mNull.count(o) != 0;
    string b = S.mBytes[o];
    Log("opbytes(" + O(o) + ")=" + BytesText(b, outNull));
    return b;
}
static int HLightData(int o) { Log("lightdata(" + O(o) + ")=p" + Num(S.mLightPeer)); return S.mLightPeer; }
static int HNewSession(int type)
{
    int o = ++S.mSerial;
    S.mKinds[o] = 4;
    Log("newsession(" + Num(type) + ")=" + O(o));
    return o;
}
static void HCrypt(int s, int session) { Log("crypt(" + O(s) + "," + O(session) + ")"); }
static void HCleanup() { Log("cleanup"); }

// a queued op completes: its status (and a receive's bytes); a refresh: its status and peer data
static void Complete(int o)
{
    int kind = S.mKinds[o];
    int st = 0;
    if (kind == 0) st = (APick(12) == 0 || S.mCalls > 200) ? 100001 : 0;
    else if (kind == 1) st = (APick(16) == 0 || S.mCalls > 200) ? 100002 : 0;
    else if (kind == 2) {
        string b;
        bool isNull = false;
        st = RecvAnswer(b, isNull);
        S.mBytes[o] = b;
        if (isNull || st != 0) S.mNull.insert(o);
    } else if (kind == 3) {
        st = APick(4) == 0 ? 100011 : 0;
        S.mLightPeer = st == 0 ? 2 : (APick(2) ? 1 : 0);
        if (st == 0) S.mCtxPeer = 2;
    }
    S.mDone[o] = st;
    Log("done(" + O(o) + ")=" + Num(st));
}

static const int kSocket = 1000;
static const unsigned long kConnectTime = 60001, kSendTime = 60002, kRecvTime = 60003, kTimeLeft = 50000;

// ---- the original half: the statements of PeerAuthOp.cpp and PeerAuthServerOp.cpp over the stand-in, the
// handshake orig::PeerAuthClient / orig::PeerAuthServer over orig::AuthPeerData

struct OrigObjs {
    ConstSmartPtr<orig::AuthPeerData> peers[3]; // p1, p2 (index - 1); NULL for p0
    const orig::AuthPeerData* Peer(int i) const { return i >= 1 && i <= 2 ? peers[i - 1].get() : NULL; }
};

template <class C> static string Session(C& c, AuthSession*& ioLast)
{
    AuthSession* s = c.GetSession();
    if (s == ioLast) return string();
    ioLast = s;
    return "made";
}

struct OrigClientOp {
    const OrigObjs& mObjs;
    WONStatus mGetCertStatus;
    const orig::AuthPeerData* mPeerData;
    orig::PeerAuthClient mPeerAuthClient;
    unsigned char mLengthFieldSize;
    AuthType mAuthType;
    AuthSession* mLast;
    int mSession;
    explicit OrigClientOp(const OrigObjs& o, bool useAuth2, AuthType t)
        : mObjs(o), mGetCertStatus(0), mPeerData(NULL), mAuthType(t), mLast(NULL), mSession(0)
    {
        this->mPeerAuthClient.SetUseAuth2(useAuth2);
    }
    WONStatus Handle(const string& m, bool isNull, ByteBufferPtr& out)
    {
        WONStatus st = this->mPeerAuthClient.HandleRecvMsg(isNull ? NULL : m.data(), (unsigned long)m.size(), out);
        if (!Session(this->mPeerAuthClient, this->mLast).empty())
            this->mSession = HNewSession(this->mLast->GetAuthType());
        return st;
    }
    bool CheckStatus(WONStatus status)
    {
        if (status == 0) return false;
        HFinish(status);
        return true;
    }
    void Success()
    {
        if (this->mAuthType == AUTH_TYPE_SESSION || this->mAuthType == AUTH_TYPE_PERSISTENT) HCrypt(kSocket, this->mSession);
        HFinish(WS_Success);
    }
    void AsyncSend(const ByteBuffer* theMsg)
    {
        int o = HNewOp(1, kSocket, string(theMsg->data(), theMsg->length()));
        HTrack(o, 1);
        HQueue(kSocket, o, kSendTime);
        o = HNewOp(2, kSocket, string());
        HTrack(o, 1);
        HQueue(kSocket, o, kRecvTime);
    }
    void AsyncConnect()
    {
        int o = HNewOp(0, kSocket, string());
        HTrack(o, 1);
        HQueue(kSocket, o, kConnectTime);
    }
    void AsyncRefreshCert()
    {
        this->mGetCertStatus = WS_Pending;
        int l = HNewLight();
        HTrack(l, 2);
        HRefreshAsync(l);
    }
    void AsyncRetry()
    {
        this->mGetCertStatus = (-4);
        this->AsyncConnect();
        this->AsyncSend(this->mPeerAuthClient.Start(this->mPeerData, this->mAuthType, this->mLengthFieldSize).get());
    }
    void AsyncRun()
    {
        this->mGetCertStatus = WS_None;
        if (this->mAuthType != AUTH_TYPE_NONE) {
            if (this->mPeerData->CertificateExpired(120))
                this->AsyncRefreshCert();
            if (this->mPeerData->CertificateExpired(30))
                return;
        }
        this->AsyncConnect();
        if (this->mAuthType != AUTH_TYPE_NONE)
            this->AsyncSend(this->mPeerAuthClient.Start(this->mPeerData, this->mAuthType, this->mLengthFieldSize).get());
    }
    void RunHook(bool async)
    {
        this->mLengthFieldSize = (unsigned char)S.mLfs;
        this->mPeerAuthClient.SetClientKeySize((unsigned char)(S.mKey & 0xff), (S.mKey >> 8) != 0);
        if (this->mAuthType != AUTH_TYPE_NONE)
            this->mPeerData = mObjs.Peer(HCtxPeer());
        if (async) {
            this->AsyncRun();
            return;
        }
        if (this->mAuthType != AUTH_TYPE_NONE && this->mPeerData->CertificateExpired(30)) {
            this->mGetCertStatus = HRefreshBlock(kTimeLeft);
            if (this->mGetCertStatus != WS_Success) {
                HFinish(WS_PeerAuth_GetCertFailure);
                return;
            }
            this->mPeerData = mObjs.Peer(HCtxPeer());
        }
        for (int i = 0; i < 2; i++) {
            WONStatus aStatus;
            aStatus = HConnect(kSocket, kConnectTime);
            if (this->CheckStatus(aStatus))
                return;
            if (this->mAuthType == AUTH_TYPE_NONE) {
                HFinish(WS_Success);
                return;
            }
            ByteBufferPtr aSendMsg = this->mPeerAuthClient.Start(this->mPeerData, this->mAuthType, this->mLengthFieldSize);
            aStatus = HSend(kSocket, string(aSendMsg->data(), aSendMsg->length()), kSendTime);
            if (this->CheckStatus(aStatus))
                return;
            string aRecv;
            bool aNull = false;
            aStatus = HRecv(kSocket, kRecvTime, aRecv, aNull);
            if (this->CheckStatus(aStatus))
                return;
            aStatus = this->Handle(aRecv, aNull, aSendMsg);
            if (i == 0 && (aStatus == (-4) || aStatus == (-19))) {
                this->mPeerAuthClient.Reset();
                this->mGetCertStatus = HRefreshBlock(kTimeLeft);
                if (this->mGetCertStatus != WS_Success) {
                    HFinish(WS_PeerAuth_GetCertFailure);
                    return;
                }
                this->mPeerData = mObjs.Peer(HCtxPeer());
                continue;
            } else if (this->CheckStatus(aStatus))
                return;
            // a success with no message to send sent NULL (the port: an empty message)
            aStatus = HSend(kSocket, aSendMsg.get() ? string(aSendMsg->data(), aSendMsg->length()) : string(), kSendTime);
            if (this->CheckStatus(aStatus))
                return;
            aNull = false;
            aRecv.clear();
            aStatus = HRecv(kSocket, kRecvTime, aRecv, aNull);
            if (this->CheckStatus(aStatus))
                return;
            aStatus = this->Handle(aRecv, aNull, aSendMsg);
            if (this->CheckStatus(aStatus))
                return;
            this->Success();
            return;
        }
    }
    bool CallbackHook(int theOp, int theParam)
    {
        switch (theParam) {
        case 1: {
            int st = HOpStatus(theOp);
            if (st != WS_Success) {
                HFinish(st);
                break;
            }
            if (this->mAuthType == AUTH_TYPE_NONE) {
                HFinish(WS_Success);
                break;
            }
            if (!HIsRecv(theOp))
                break;
            ByteBufferPtr aSendMsg;
            bool aNull = false;
            string aRecvMsg = HOpBytes(theOp, aNull);
            WONStatus aStatus = this->Handle(aRecvMsg, aNull, aSendMsg);
            if (aStatus == (-4) || aStatus == (-19)) {
                this->mPeerAuthClient.Reset();
                switch (this->mGetCertStatus) {
                case WS_None: this->AsyncRefreshCert(); break;
                case WS_Pending: break;
                case WS_Success: this->AsyncRetry(); break;
                default: HFinish(WS_PeerAuth_GetCertFailure);
                }
                break;
            } else if (this->CheckStatus(aStatus))
                break;
            if (aSendMsg.get() == NULL)
                this->Success();
            else
                this->AsyncSend(aSendMsg.get());
        } break;
        case 2: {
            this->mGetCertStatus = HOpStatus(theOp);
            this->mPeerData = mObjs.Peer(HLightData(theOp));
            if (this->mPeerAuthClient.GetState() == orig::PeerAuthClient::STATE_NOT_STARTED) {
                if (this->mGetCertStatus != WS_Success) {
                    HFinish(WS_PeerAuth_GetCertFailure);
                    break;
                } else
                    this->AsyncRetry();
            }
        } break;
        default:
            return false;
        }
        return true;
    }
};

struct OrigServerOp {
    const OrigObjs& mObjs;
    orig::PeerAuthServer mPeerAuthServer;
    AuthSession* mLast;
    int mSession;
    explicit OrigServerOp(const OrigObjs& o) : mObjs(o), mLast(NULL), mSession(0) {}
    WONStatus Handle(const string& m, bool isNull, ByteBufferPtr& out)
    {
        WONStatus st = this->mPeerAuthServer.HandleRecvMsg(isNull ? NULL : m.data(), (unsigned long)m.size(), out);
        if (!Session(this->mPeerAuthServer, this->mLast).empty())
            this->mSession = HNewSession(this->mLast->GetAuthType());
        return st;
    }
    bool CheckStatus(WONStatus status)
    {
        if (status == 0) return false;
        HFinish(status);
        return true;
    }
    void Success()
    {
        AuthSession* session = this->mPeerAuthServer.GetSession();
        if (session != NULL) { // the original read a NULL session
            AuthType type = session->GetAuthType();
            if (type == AUTH_TYPE_SESSION || type == AUTH_TYPE_PERSISTENT) HCrypt(kSocket, this->mSession);
        }
        HFinish(WS_Success);
    }
    void AsyncRun()
    {
        int o = HNewOp(2, kSocket, string());
        HTrack(o, 1);
        HQueue(kSocket, o, kRecvTime);
    }
    void RunHook(bool async)
    {
        this->mPeerAuthServer.Start(mObjs.Peer(HCtxPeer()), (unsigned char)S.mLfs);
        if (async) {
            this->AsyncRun();
            return;
        }
        string recvMsg;
        bool isNull = false;
        WONStatus status = HRecv(kSocket, kRecvTime, recvMsg, isNull);
        if (this->CheckStatus(status))
            return;
        ByteBufferPtr sendMsg;
        status = this->Handle(recvMsg, isNull, sendMsg);
        this->CheckStatus(status);
        if (sendMsg.get() == NULL)
            return;
        status = HSend(kSocket, string(sendMsg->data(), sendMsg->length()), kSendTime);
        this->CheckStatus(status);
        if (S.mStatus != WS_Pending)
            return;
        recvMsg.clear();
        isNull = false;
        status = HRecv(kSocket, kRecvTime, recvMsg, isNull);
        if (this->CheckStatus(status))
            return;
        status = this->Handle(recvMsg, isNull, sendMsg);
        this->CheckStatus(status);
        if (sendMsg.get() == NULL)
            return;
        status = HSend(kSocket, string(sendMsg->data(), sendMsg->length()), kSendTime);
        if (status == 0)
            this->Success();
        else
            HFinish(status);
    }
    bool CallbackHook(int op, int type)
    {
        int st = HOpStatus(op);
        if (st != WS_Success) {
            HFinish(st);
            return true;
        }
        if (type == 1) {
            if (!HIsRecv(op))
                return true;
            ByteBufferPtr sendMsg;
            bool isNull = false;
            string recvMsg = HOpBytes(op, isNull);
            WONStatus status = this->Handle(recvMsg, isNull, sendMsg);
            int sendType = 0;
            if (status == WS_Success && this->mPeerAuthServer.GetState() == orig::PeerAuthServer::STATE_NOT_STARTED)
                sendType = 2;
            if (sendMsg.get() != NULL) {
                int o = HNewOp(1, kSocket, string(sendMsg->data(), sendMsg->length()));
                HTrack(o, sendType);
                HQueue(kSocket, o, kSendTime);
            }
            if (status != WS_Success)
                HFinish(status);
            else if (sendType != 2) {
                int o = HNewOp(2, kSocket, string());
                HTrack(o, 1);
                HQueue(kSocket, o, kRecvTime);
            }
        } else if (type == 2)
            this->Success();
        return true;
    }
};

// ---- the scenario's objects, the run and its driving

static orig::AuthPeerData* OrigPeer(const PeerSpec& p, const orig::AuthPubKeyBlock* block)
{
    if (!p.full) return new orig::AuthPeerData(block);
    SmartPtr<orig::AuthCertificate> c1 = new orig::AuthCertificate(p.c1.data(), (unsigned long)p.c1.size());
    SmartPtr<orig::Auth2Certificate> c2 =
        p.c2.empty() ? NULL : new orig::Auth2Certificate(p.c2.data(), (unsigned long)p.c2.size());
    string pk = Der(p.priv, true);
    ElGamal key(pk.data(), (int)pk.size(), true);
    return new orig::AuthPeerData(c1.get(), block, key, (time_t)p.delta, c2.get());
}
static AuthPeerData* NewPeer(const PeerSpec& p, const AuthBase* block)
{
    if (!p.full) return new AuthPeerData(block);
    SmartPtr<AuthBase> c1 = new AuthBase(EE_WON_AUTH_CERTIFICATE, p.c1.data(), (unsigned long)p.c1.size());
    SmartPtr<AuthBase> c2 =
        p.c2.empty() ? NULL : new AuthBase(EE_WON_AUTH_CERTIFICATE2, p.c2.data(), (unsigned long)p.c2.size());
    string pk = Der(p.priv, true);
    ElGamal key(pk.data(), (int)pk.size(), true);
    return new AuthPeerData(c1.get(), block, key, (time_t)p.delta, c2.get());
}

// the stand-in's start: the other end (the game's objects, the same in both halves), then the run's seed
static ConstSmartPtr<AuthPeerData> sOtherPeer;
static SmartPtr<AuthBase> sOtherBlock;
static void Begin(const Scenario& sc)
{
    ee_won_random_test_seed(sc.run);
    S = Standin();
    S.mSeed = sc.script;
    S.mStatus = WS_Pending;
    S.mLfs = sc.lfs;
    S.mKey = sc.clientKey;
    S.mType = sc.type;
    S.mAsync = sc.async;
    S.mCtxPeer = 1;
    sOtherBlock = new AuthBase(EE_WON_AUTH_PUB_KEY_BLOCK, sc.block.data(), (unsigned long)sc.block.size());
    sOtherPeer = NewPeer(sc.other, sOtherBlock.get());
    S.mOther = ee_won_peerauth_new(!sc.server);
    ee_won_peerauth_set(S.mOther, EE_WON_PEERAUTH_USE_AUTH2, sc.otherAuth2);
    if (sc.server) {
        // the other end is a client: its request waits
        ee_won_peerauth_set(S.mOther, EE_WON_PEERAUTH_CLIENT_KEY, sc.otherKey);
        string req;
        ee_won_peerauth_start(S.mOther, sOtherPeer->Impl(), sc.otherType, (unsigned char)sc.lfs, OtherSink, &req);
        S.mInbox.push_back(req);
    } else
        ee_won_peerauth_start(S.mOther, sOtherPeer->Impl(), 0, (unsigned char)sc.lfs, OtherSink, NULL);
}
static void End()
{
    ee_won_peerauth_free(S.mOther);
    for (std::map<int, EeWonAuthSession*>::iterator i = S.mSessions.begin(); i != S.mSessions.end(); ++i)
        ee_won_auth_session_free(i->second);
    sOtherPeer = NULL;
    sOtherBlock = NULL;
}
// asynchronous: complete the waiting ops in order while the op runs
template <class F> static void Drive(F callback)
{
    for (int n = 0; n < 40 && !S.mPending.empty() && S.mStatus == WS_Pending; n++) {
        int o = S.mPending.front();
        S.mPending.pop_front();
        Complete(o);
        Log("cb(" + Num(S.mTrack[o]) + "," + O(o) + ")");
        callback(o, S.mTrack[o]);
    }
}

static string RunOrig(const Scenario& sc)
{
    Begin(sc);
    OrigObjs objs;
    SmartPtr<orig::AuthPubKeyBlock> block = new orig::AuthPubKeyBlock(sc.block.data(), (unsigned long)sc.block.size());
    objs.peers[0] = OrigPeer(sc.ctx, block.get());
    objs.peers[1] = OrigPeer(sc.refreshed, block.get());
    string result;
    if (sc.server) {
        OrigServerOp op(objs);
        op.RunHook(sc.async);
        if (sc.async) Drive([&](int o, int id) { op.CallbackHook(o, id); });
        HCleanup();
        result = " session " + Num(op.mPeerAuthServer.GetSession() != NULL) + " permits " +
                 Num(op.mPeerAuthServer.GetClientCertificate() != NULL &&
                     op.mPeerAuthServer.GetClientCertificate()->VerifyPermissions(0, kCommunity, 0));
    } else {
        OrigClientOp op(objs, sc.useAuth2, (AuthType)sc.type);
        op.RunHook(sc.async);
        if (sc.async) Drive([&](int o, int id) { op.CallbackHook(o, id); });
        HCleanup();
        result = " session " + Num(op.mPeerAuthClient.GetSession() != NULL) + " gcs " + Num(op.mGetCertStatus);
    }
    string t = S.mTrace + "|status " + Num(S.mStatus) + result;
    End();
    return t;
}

// ---- the Rust half: the op through the base's C call, answered by the same stand-in

struct RustObjs {
    ConstSmartPtr<AuthPeerData> peers[2];
    const AuthPeerData* Peer(int i) const { return i >= 1 && i <= 2 ? peers[i - 1].get() : NULL; }
};
static RustObjs* sRust;
static void* P(int o) { return reinterpret_cast<void*>((intptr_t)o); }
static int I(void* p) { return (int)(intptr_t)p; }
static void RustHost(void*, EeWonHostCall* c)
{
    int o0 = I(c->obj[0]);
    long long n0 = c->num[0];
    string data = c->len ? string(reinterpret_cast<const char*>(c->data), c->len) : string();
    switch (c->code) {
    case EE_WON_HOST_STATUS: c->out_num = S.mStatus; break;
    case EE_WON_HOST_MODE: c->out_num = S.mAsync ? 1 : 2; break;
    case EE_WON_HOST_TIME_LEFT: c->out_num = kTimeLeft; break;
    case EE_WON_HOST_SEND_TIME: c->out_num = kSendTime; break;
    case EE_WON_HOST_RECV_TIME: c->out_num = kRecvTime; break;
    case EE_WON_HOST_CONNECT_TIME: c->out_num = kConnectTime; break;
    case EE_WON_HOST_AUTH_TYPE: c->out_num = S.mType; break;
    case EE_WON_HOST_OWN_SOCKET: c->out_obj = P(kSocket); break;
    case EE_WON_HOST_SOCKET_LFS: c->out_num = S.mLfs; break;
    case EE_WON_HOST_CLIENT_KEY: c->out_num = S.mKey; break;
    case EE_WON_HOST_CONNECT: c->out_num = HConnect(o0, (unsigned long)n0); break;
    case EE_WON_HOST_SEND_MSG: c->out_num = HSend(o0, data, (unsigned long)n0); break;
    case EE_WON_HOST_RECV_MSG: {
        string b;
        bool isNull = false;
        c->out_num = HRecv(o0, (unsigned long)n0, b, isNull);
        if (!isNull && c->out_num == 0) c->out_sink(c->out_context, reinterpret_cast<const uint8_t*>(b.data()), b.size());
        break;
    }
    case EE_WON_HOST_NEW_OP: {
        int kind = n0 == EE_WON_HOST_OP_CONNECT ? 0 : n0 == EE_WON_HOST_OP_SEND_MSG ? 1 : 2;
        c->out_obj = P(HNewOp(kind, o0, data));
        break;
    }
    case EE_WON_HOST_TRACK: HTrack(o0, (int)n0); break;
    case EE_WON_HOST_QUEUE: HQueue(o0, I(c->obj[1]), (unsigned long)n0); break;
    case EE_WON_HOST_FINISH: HFinish((int)n0); break;
    case EE_WON_HOST_RELEASE: case EE_WON_HOST_ADD_REF: break;
    case EE_WON_HOST_PEER_CTX_DATA: {
        const AuthPeerData* p = sRust->Peer(HCtxPeer());
        c->out_num = p ? (long long)(intptr_t)ee_won_authobj_clone(p->Impl()) : 0;
        break;
    }
    case EE_WON_HOST_PEER_REFRESH_BLOCK: c->out_num = HRefreshBlock((unsigned long)n0); break;
    case EE_WON_HOST_PEER_NEW_LIGHT: c->out_obj = P(HNewLight()); break;
    case EE_WON_HOST_PEER_REFRESH_ASYNC: HRefreshAsync(o0); break;
    case EE_WON_HOST_PEER_LIGHT_DATA: {
        const AuthPeerData* p = sRust->Peer(HLightData(o0));
        c->out_num = p ? (long long)(intptr_t)ee_won_authobj_clone(p->Impl()) : 0;
        break;
    }
    case EE_WON_HOST_OP_STATUS: c->out_num = HOpStatus(o0); break;
    case EE_WON_HOST_OP_IS_RECV_MSG: c->out_num = HIsRecv(o0); break;
    case EE_WON_HOST_OP_BYTES: {
        bool isNull = false;
        string b = HOpBytes(o0, isNull);
        if (!isNull) c->out_sink(c->out_context, reinterpret_cast<const uint8_t*>(b.data()), b.size());
        break;
    }
    case EE_WON_HOST_PEER_NEW_SESSION: {
        EeWonAuthSession* s = reinterpret_cast<EeWonAuthSession*>((intptr_t)n0);
        int o = HNewSession((int)ee_won_auth_session_get(s, 0));
        S.mSessions[o] = s;
        c->out_obj = P(o);
        break;
    }
    case EE_WON_HOST_ADD_CRYPT: HCrypt(o0, I(c->obj[1])); break;
    case EE_WON_HOST_BASE_CLEANUP: HCleanup(); break;
    default:
        Log("unexpected(" + Num(c->code) + ")");
        break;
    }
}

static string RunRust(const Scenario& sc)
{
    Begin(sc);
    RustObjs objs;
    sRust = &objs;
    SmartPtr<AuthBase> block = new AuthBase(EE_WON_AUTH_PUB_KEY_BLOCK, sc.block.data(), (unsigned long)sc.block.size());
    objs.peers[0] = NewPeer(sc.ctx, block.get());
    objs.peers[1] = NewPeer(sc.refreshed, block.get());
    const uint8_t kAddr[6] = { 1, 2, 3, 4, 5, 6 };
    EeWonHostOp* op = sc.server ? ee_won_hostop_peerauth_server() : ee_won_hostop_peerauth(kAddr, sc.useAuth2);
    ee_won_hostop_bind(op, RustHost, NULL);
    ee_won_hostop_run(op);
    if (sc.async) Drive([&](int o, int id) { ee_won_hostop_callback(op, id, P(o)); });
    ee_won_hostop_cleanup(op);
    string result;
    if (sc.server)
        result = " session " + Num(ee_won_hostop_num(op, EE_WON_PEEROP_SESSION, 0) != 0) + " permits " +
                 Num(ee_won_hostop_num(op, EE_WON_PEEROP_CLIENT_PERMITS, kCommunity));
    else
        result = " session " + Num(ee_won_hostop_num(op, EE_WON_PEEROP_SESSION, 0) != 0) + " gcs " +
                 Num(ee_won_hostop_num(op, EE_WON_PEEROP_GET_CERT_STATUS, 0));
    ee_won_hostop_free(op);
    string t = S.mTrace + "|status " + Num(S.mStatus) + result;
    End();
    sRust = NULL;
    return t;
}

// ---- the golden's line: the scenario, then the original's trace

static string Spec(const PeerSpec& p)
{
    return Num(p.full) + " " + Num(p.priv) + " " + Hex(p.c1) + " " + (p.c2.empty() ? string("N") : Hex(p.c2)) + " " +
           Num(p.delta);
}
static string Line(const Scenario& sc, const string& t)
{
    return "O " + Num(sc.run) + " " + Num(sc.server) + " " + Num(sc.async) + " " + Num(sc.useAuth2) + " " + Num(sc.type) +
           " " + Num(sc.lfs) + " " + Num(sc.clientKey) + " " + Hex(sc.block) + " " + Spec(sc.ctx) + " " + Spec(sc.refreshed) +
           " " + Spec(sc.other) + " " + Num(sc.otherAuth2) + " " + Num(sc.otherType) + " " + Num(sc.otherKey) + " => " + t;
}

int main(int argc, char** argv)
{
    int print = 0, count = 2000;
    bool origOnly = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) print = atoi(argv[i + 1]);
        if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atoi(argv[i + 1]);
        if (!strcmp(argv[i], "--original")) origOnly = true;
    }
    if (print) {
        for (int i = 0; i < print; i++) {
            Scenario sc = Generate(i);
            printf("%s\n", Line(sc, RunOrig(sc)).c_str());
        }
        return 0;
    }
    std::map<string, int> outcomes;
    for (int i = 0; i < count; i++) {
        Scenario sc = Generate(i);
        string a = RunOrig(sc);
        string b = origOnly ? RunOrig(sc) : RunRust(sc);
        CHECK(a == b);
        if (a != b && sFailures < 4)
            printf("sequence %d\n  original %s\n  rust     %s\n", i, a.c_str(), b.c_str());
        outcomes[(sc.server ? "server " : "client ") + a.substr(a.find("|status"))]++;
    }
    for (std::map<string, int>::iterator i = outcomes.begin(); i != outcomes.end(); ++i)
        if (i->second >= 5) printf("  %5d %s\n", i->second, i->first.c_str());
    printf("won_peerop_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
