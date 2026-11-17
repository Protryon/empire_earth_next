// The peer sessions a peer authentication makes and the encryption they put on a socket (AuthSession,
// CryptTransform, AuthContext's session cache), over real sockets on 127.0.0.1, one line per event, compared with
// rust/crates/ee_won_net/src/golden_authsession.txt (recorded over the C++ AuthSession and ServerOp.cpp's HostCall
// before the session became a Rust handle). A real PeerAuthOp and PeerAuthServerOp authenticate over a socket pair
// with real auth contexts and signed certificates (won_peerop_test's keys); every session call goes through an
// op's own call path (ee_won_hostop_call): the ops' sessions and keys, the transforms the ops and the calls add
// (ADD_CRYPT), messages both ways through them, a raw read of what a transform wrote (sessions made with a known
// key: PEER_NEW_SESSION), a sequence number skipped, the auth context's cache (PUT_SESSION, GET_SESSION) and a
// cached session reused on a new connection, sessions released while their transforms still use them. Objects
// are named by the answer that returned them; random keys are logged by their length and equality only.
// `--print` prints the lines instead. The exit code is the number of failures.
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
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "rust/ee_won.h"
#include "won_blocking.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static bool sTrace; // --trace: each line to stderr as it is logged
static void Log(const string& inLine)
{
    sLines.push_back(inLine);
    if (sTrace) fprintf(stderr, "%s\n", inLine.c_str());
}
static string N(long long inValue) { return std::to_string(inValue); }
static string Hex(const string& inBytes, size_t inMax = 64)
{
    static const char kDigits[] = "0123456789abcdef";
    string s = N((long long)inBytes.size()) + ":";
    for (size_t i = 0; i < inBytes.size() && i < inMax; i++) {
        unsigned char c = static_cast<unsigned char>(inBytes[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}

// ---- key material and certificates (won_peerop_test's keys: 0 the auth server, 2 the peers)

static const char* const kPriv[3] = {
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02184bb17e485a32d0a3f02eb0bc5d2b9a09f3cb369e38ff200602080c931814d44a5e5b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02183dda7c54fd793a4e534f60facc1301e37953f4df16f49861020808e58d3749117c1b",
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
static const long long kFuture = 0x7f000000;
static const int kCommunity = 42;
static string Signed(const string& data, int signer)
{
    string key = Der(signer, true);
    ElGamal k(key.data(), (int)key.size(), true);
    ByteBufferPtr sig = k.Sign(data.data(), (int)data.size());
    return data + string(sig->data(), sig->length());
}
static string Cert1(int key, long long user)
{
    W w;
    w.S(1); w.L(1000); w.L(kFuture); w.L(user); w.L(kCommunity); w.S(5);
    w.Str(Der(key, false));
    return Signed(w.d, 0);
}
static string Cert2(int key, long long user)
{
    W w;
    w.S(2); w.L(1000); w.L(kFuture); w.S(1);
    W r;
    r.L(user); r.Str(Der(key, false)); r.WStr("Player"); r.B(1); r.L(kCommunity); r.S(5);
    w.S(0); w.S((int)r.d.size()); w.d += r.d;
    return Signed(w.d, 0);
}
static string Block(int id)
{
    W b;
    b.S(3); b.L(1000); b.L(kFuture); b.S(id); b.S(1); b.Str(Der(0, false));
    return Signed(b.d, 0);
}

// ---- one call through an op's own path

struct Answer {
    bool mHasBytes = false;
    string mBytes;
    EeWonHostCall mCall;
};
static void Sink(void* inContext, const uint8_t* inData, size_t inLength)
{
    Answer* a = static_cast<Answer*>(inContext);
    a->mHasBytes = true;
    a->mBytes.assign(reinterpret_cast<const char*>(inData), inLength);
}
static RustServerOp* sOp;
static Answer Call(int inCode, void* inObj0 = NULL, void* inObj1 = NULL, long long inNum0 = 0, long long inNum1 = 0,
                   const string& inData = string(), const IPAddr* inAddr = NULL)
{
    Answer a;
    memset(&a.mCall, 0, sizeof(a.mCall));
    a.mCall.code = inCode;
    a.mCall.obj[0] = inObj0;
    a.mCall.obj[1] = inObj1;
    a.mCall.num[0] = inNum0;
    a.mCall.num[1] = inNum1;
    a.mCall.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    a.mCall.len = inData.size();
    if (inAddr != NULL) memcpy(a.mCall.addr, inAddr->GetSixByte(), 6);
    a.mCall.out_sink = Sink;
    a.mCall.out_context = &a;
    ee_won_hostop_call(sOp->GetRustOp(), &a.mCall);
    return a;
}
static void* R(RefCount* inObj) { return inObj; }

// the sessions the calls answered, named by the order they first came out
static std::vector<void*> sSessions;
static string Name(void* inSession)
{
    if (inSession == NULL) return "null";
    for (size_t i = 0; i < sSessions.size(); i++)
        if (sSessions[i] == inSession) return "s" + N((long long)i);
    sSessions.push_back(inSession);
    return "s" + N((long long)sSessions.size() - 1) + "(new)";
}

static void Send(const string& inWhat, BlockingSocket* inSocket, const string& inMsg)
{
    // SendMsg takes the message with the socket's length field in front (the length with the field)
    unsigned int aField = inSocket->GetLengthFieldSize(), aLength = (unsigned int)inMsg.size() + aField;
    string aMsg;
    for (unsigned int i = 0; i < aField; i++) aMsg += (char)((aLength >> (8 * i)) & 0xff);
    aMsg += inMsg;
    Log(inWhat + " send " + N(Call(EE_WON_HOST_SEND_MSG, R(inSocket), NULL, 2000, 0, aMsg).mCall.out_num));
}
static void Recv(const string& inWhat, BlockingSocket* inSocket)
{
    Answer a = Call(EE_WON_HOST_RECV_MSG, R(inSocket), NULL, 2000);
    Log(inWhat + " recv " + N(a.mCall.out_num) + " " + (a.mHasBytes ? Hex(a.mBytes) : string("none")));
}
// a message encrypted with a random key: its length, its first byte and whether it differs from the plain text
static void RecvLength(const string& inWhat, BlockingSocket* inSocket, const string& inPlain)
{
    Answer a = Call(EE_WON_HOST_RECV_MSG, R(inSocket), NULL, 2000);
    Log(inWhat + " recv " + N(a.mCall.out_num) + " length " + N((long long)a.mBytes.size()) + " first "
        + Hex(a.mBytes.substr(0, 1)) + " plain " + N(a.mBytes.find(inPlain) != string::npos));
}

// a connected pair of stream sockets: the client and the accepted one
struct Pair {
    SmartPtr<BlockingSocket> mClient;
    SmartPtr<AsyncSocket> mServer;
    BlockingSocket* Server() { return static_cast<BlockingSocket*>(this->mServer.get()); }
};
static SmartPtr<BlockingSocket> sListener;
static IPAddr sListenAddr;
static bool Accept(Pair& p, WONAPICore& inCore)
{
    for (int i = 0; i < 200 && p.mServer.get() == NULL; i++) {
        sListener->WaitForAccept(20);
        if (sListener->Accept(p.mServer) != 0) p.mServer = NULL;
        inCore.Pump(5);
    }
    return p.mServer.get() != NULL;
}
static bool Connect(Pair& p, WONAPICore& inCore)
{
    p.mClient = new BlockingSocket(0);
    if (WonBlocking::Connect(p.mClient, sListenAddr, 2000) != WS_Success) return false;
    return Accept(p, inCore);
}

// an auth context with peer data for a user (the peers' key 2, certificates signed by key 0)
static SmartPtr<AuthContext> NewContext(long long inUser)
{
    SmartPtr<AuthContext> aCtx = new AuthContext;
    SmartPtr<GetCertOp> aCertOp = new GetCertOp(aCtx.get());
    sOp = aCertOp.get();
    void* c1 = Call(EE_WON_HOST_CERT_NEW, NULL, NULL, 1, 0, Cert1(2, inUser)).mCall.out_obj;
    void* c2 = Call(EE_WON_HOST_CERT_NEW, NULL, NULL, 2, 0, Cert2(2, inUser)).mCall.out_obj;
    void* kb = Call(EE_WON_HOST_CERT_NEW_BLOCK, NULL, NULL, 0, 0, Block(17)).mCall.out_obj;
    void* k = Call(EE_WON_HOST_CERT_PRIV_NEW).mCall.out_obj;
    Call(EE_WON_HOST_CERT_PRIV_SET, k, NULL, 0, 0, Der(2, true));
    {
        Answer q;
        memset(&q.mCall, 0, sizeof(q.mCall));
        q.mCall.code = EE_WON_HOST_CERT_NEW_PEER;
        q.mCall.obj[0] = c1;
        q.mCall.obj[1] = kb;
        q.mCall.obj[2] = c2;
        q.mCall.num[0] = (long long)(intptr_t)k;
        q.mCall.num[1] = 0;
        q.mCall.out_sink = Sink;
        q.mCall.out_context = &q;
        ee_won_hostop_call(sOp->GetRustOp(), &q.mCall);
        Call(EE_WON_HOST_CERT_SET_PEER, R(aCtx.get()), q.mCall.out_obj);
        Call(EE_WON_HOST_RELEASE, q.mCall.out_obj);
    }
    Call(EE_WON_HOST_CERT_PRIV_FREE, k);
    void* owned[] = {c1, c2, kb};
    for (void* o : owned) Call(EE_WON_HOST_RELEASE, o);
    sOp = NULL;
    ConstSmartPtr<AuthPeerData> aPeer = aCtx->GetPeerData();
    Log("context " + N(inUser) + " peer valid " + N(aPeer->IsValid()) + " user " + N((long long)aPeer->GetUserId())
        + " expired " + N(aPeer->CertificateExpired(30)) + "/" + N(aPeer->Certificate2Expired()));
    return aCtx;
}

// a client and a server peer authentication of a type over a new pair; the ops are kept for their sessions
struct Auth {
    Pair mPair;
    SmartPtr<PeerAuthOp> mClientOp;
    SmartPtr<PeerAuthServerOp> mServerOp;
};
static void Authenticate(Auth& a, AuthType inType, AuthContext* inClient, AuthContext* inServer, WONAPICore& inCore)
{
    a.mPair.mClient = new BlockingSocket(0);
    a.mClientOp = new PeerAuthOp(sListenAddr, inClient, inType, a.mPair.mClient.get(), true);
    a.mClientOp->RunAsync(10000);
    Log("accept " + N(Accept(a.mPair, inCore)));
    if (a.mPair.mServer.get() == NULL) return;
    a.mServerOp = new PeerAuthServerOp(inServer, a.mPair.Server());
    a.mServerOp->RunAsync(10000);
    for (int i = 0; i < 500 && (a.mClientOp->GetStatus() == WS_Pending || a.mServerOp->GetStatus() == WS_Pending); i++)
        inCore.Pump(10);
    Log("type " + N(inType) + " client status " + N(a.mClientOp->GetStatus()) + " server status "
        + N(a.mServerOp->GetStatus()) + " lfs " + N(a.mPair.mClient->GetLengthFieldSize()) + "/"
        + N(a.mPair.Server()->GetLengthFieldSize()));
}
// the op's session through the call path (a reference the caller releases) and its key
static void* Session(const string& inWhat, RustServerOp* inOp)
{
    if (inOp == NULL) {
        Log(inWhat + " no op");
        return NULL;
    }
    void* s = Call(EE_WON_HOST_PEER_AUTH_SESSION, R(inOp)).mCall.out_obj;
    Log(inWhat + " session " + Name(s));
    return s;
}
static string Key(RustServerOp* inOp)
{
    if (inOp == NULL) return "(no op)";
    Answer a = Call(EE_WON_HOST_PEER_AUTH_KEY, R(inOp));
    return a.mHasBytes ? a.mBytes : string("(none)");
}
// a session made with a known key, through the call path (PEER_NEW_SESSION adopts it)
static void* NewSession(int inType, unsigned short inId, const string& inKey, unsigned char inLfs)
{
    Blowfish aKey(inKey.data(), (int)inKey.size());
    EeWonAuthSession* s = ee_won_auth_session_new(inType, inId, reinterpret_cast<const EeWonBlowfish*>(&aKey), inLfs,
                                                  (int64_t)time(NULL));
    void* o = Call(EE_WON_HOST_PEER_NEW_SESSION, NULL, NULL, (long long)(intptr_t)s).mCall.out_obj;
    Log("new session type " + N(inType) + " id " + N(inId) + " lfs " + N(inLfs) + " " + Name(o));
    return o;
}

int main(int argc, char** argv)
{
    sTrace = argc > 1 && strcmp(argv[argc - 1], "--trace") == 0;
    string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    string aDir = (aSlash == string::npos ? string(".") : aExe.substr(0, aSlash)) + "/authsession_files/";
    PlatCreateDirectory(aDir.c_str());
    remove((aDir + "_wonlogin.ks").c_str());
    {
        string aPub = Der(0, false);
        FILE* f = fopen((aDir + "_wonkver.pub").c_str(), "wb");
        if (f == NULL) {
            printf("FAIL: cannot write the verifier key\n");
            return 1;
        }
        fwrite(aPub.data(), 1, aPub.size(), f);
        fclose(f);
    }
    AsyncSocket::StartWinsock();
    ee_won_random_test_seed(5151);
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(aDir.c_str());
        aCore.Startup();
        SmartPtr<AuthContext> aClientCtx = NewContext(1001);
        SmartPtr<AuthContext> aServerCtx = NewContext(2002);
        sListener = new BlockingSocket(0);
        WONStatus aBind = sListener->Bind(IPAddr("127.0.0.1", 0), true);
        WONStatus aListen = sListener->Listen(5);
        Log("listen " + N(aBind) + " " + N(aListen));
        sListenAddr = IPAddr("127.0.0.1", sListener->GetLocalPort());

        // a persistent authentication (the peer server's one mode; the LAN game's direct connect): both ops'
        // sessions and keys, messages both ways through their transforms
        Auth a;
        Authenticate(a, AUTH_TYPE_PERSISTENT, aClientCtx.get(), aServerCtx.get(), aCore);
        sOp = a.mClientOp.get();
        void* sc = Session("client", a.mClientOp.get());
        void* sc2 = Session("client again", a.mClientOp.get());
        void* ss = Session("server", a.mServerOp.get());
        string kc = Key(a.mClientOp.get()), ks = Key(a.mServerOp.get());
        Log("keys " + N((long long)kc.size()) + " " + N((long long)ks.size()) + " same " + N(kc == ks));
        if (a.mPair.mServer.get() != NULL) {
            Send("client", a.mPair.mClient.get(), "hello server");
            Recv("server", a.mPair.Server());
            Send("server", a.mPair.Server(), "hello client");
            Recv("client", a.mPair.mClient.get());
            Send("client", a.mPair.mClient.get(), "");
            Recv("server", a.mPair.Server());
            Send("client", a.mPair.mClient.get(), string(300, 'x'));
            Recv("server", a.mPair.Server());
            Send("server", a.mPair.Server(), "second");
            Recv("client", a.mPair.mClient.get());
        }

        // the auth context's cache: the client's session kept for the server's address, taken back once
        Log("get none " + Name(Call(EE_WON_HOST_GET_SESSION, NULL, NULL, 0, 0, string(), &sListenAddr).mCall.out_obj));
        Call(EE_WON_HOST_PUT_SESSION, sc, NULL, 0, 0, string(), &sListenAddr);
        void* got = Call(EE_WON_HOST_GET_SESSION, NULL, NULL, 0, 0, string(), &sListenAddr).mCall.out_obj;
        Log("get " + Name(got));
        Log("get again " + Name(Call(EE_WON_HOST_GET_SESSION, NULL, NULL, 0, 0, string(), &sListenAddr).mCall.out_obj));

        // the cached session reused on a new connection (a request's TryConnect), the server's on its socket
        Pair b;
        Log("connect " + N(Connect(b, aCore)));
        if (b.mServer.get() != NULL && got != NULL && ss != NULL) {
            Call(EE_WON_HOST_ADD_CRYPT, R(b.mClient.get()), got);
            Call(EE_WON_HOST_ADD_CRYPT, R(b.Server()), ss);
            Send("reused client", b.mClient.get(), "on a new socket");
            Recv("reused server", b.Server());
            Send("reused server", b.Server(), "back");
            Recv("reused client", b.mClient.get());
            // the server reads one message raw, then through its session again
            b.Server()->RemoveAllMsgTransforms();
            Send("reused client", b.mClient.get(), "read raw");
            RecvLength("raw server", b.Server(), "read raw");
            Call(EE_WON_HOST_ADD_CRYPT, R(b.Server()), ss);
            Send("reused client", b.mClient.get(), "after the raw one");
            Recv("reused server", b.Server());
        }

        // sessions with a known key: what the transform writes, read raw, and read back through a transform
        Pair c;
        Log("connect " + N(Connect(c, aCore)));
        if (c.mServer.get() != NULL) {
            string aKey = "0123456789abcdef";
            void* k1 = NewSession(AUTH_TYPE_SESSION, 0x1234, aKey, 4);
            void* k2 = NewSession(AUTH_TYPE_SESSION, 0x1234, aKey, 4);
            void* k3 = NewSession(AUTH_TYPE_PERSISTENT, 77, aKey, 4);
            void* k4 = NewSession(AUTH_TYPE_PERSISTENT, 78, aKey, 4);
            void* k5 = NewSession(AUTH_TYPE_SESSION, 0x1235, aKey, 4);
            void* k6 = NewSession(AUTH_TYPE_PERSISTENT_NOCRYPT, 5, aKey, 4);
            void* k0 = NewSession(AUTH_TYPE_SESSION, 0x1234, aKey, 4);
            // what the transform writes: the same text twice (the sequence number differs), then the session on the
            // client replaced by a new one, the server's from its first message on
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k0);
            Send("known client", c.mClient.get(), "known text");
            Recv("raw server", c.Server());
            Send("known client", c.mClient.get(), "known text");
            Recv("raw server", c.Server());
            c.mClient->RemoveAllMsgTransforms();
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k1);
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k2);
            Send("known client", c.mClient.get(), "third");
            Recv("known server", c.Server());
            Send("known client", c.mClient.get(), "fourth");
            Recv("known server", c.Server());
            // the server reads one message raw: the next one's sequence number is one ahead of its session's
            c.Server()->RemoveAllMsgTransforms();
            Send("known client", c.mClient.get(), "skipped");
            Recv("raw server", c.Server());
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k2);
            Send("known client", c.mClient.get(), "after a skip");
            Recv("skip server", c.Server());
            Send("known client", c.mClient.get(), "after the skip again");
            Recv("skip server", c.Server());
            // the server's session id differs: its transform answers a mismatch
            c.Server()->RemoveAllMsgTransforms();
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k5);
            Send("known client", c.mClient.get(), "wrong id");
            Recv("mismatch server", c.Server());
            // a persistent session: no id, no sequence number
            c.mClient->RemoveAllMsgTransforms();
            c.Server()->RemoveAllMsgTransforms();
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k3);
            Send("persistent client", c.mClient.get(), "persistent");
            Recv("raw server", c.Server());
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k4);
            Send("persistent client", c.mClient.get(), "persistent two");
            Recv("persistent server", c.Server());
            Send("persistent server", c.Server(), "persistent back");
            Recv("persistent client", c.mClient.get());
            // a session that does not encrypt
            c.mClient->RemoveAllMsgTransforms();
            c.Server()->RemoveAllMsgTransforms();
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k6);
            Send("nocrypt client", c.mClient.get(), "plain");
            Recv("raw server", c.Server());
            // two transforms of the same session on one socket, the other end with two sessions in the same order
            c.mClient->RemoveAllMsgTransforms();
            void* k7 = NewSession(AUTH_TYPE_PERSISTENT, 9, aKey, 4);
            void* k8 = NewSession(AUTH_TYPE_PERSISTENT, 9, aKey, 4);
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k7);
            Call(EE_WON_HOST_ADD_CRYPT, R(c.mClient.get()), k7);
            Send("twice client", c.mClient.get(), "twice");
            Recv("raw server", c.Server());
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k8);
            Call(EE_WON_HOST_ADD_CRYPT, R(c.Server()), k8);
            Send("twice client", c.mClient.get(), "twice again");
            Recv("twice server", c.Server());
            // the sessions released while the transforms hold them: they go on working
            void* known[] = {k0, k1, k2, k3, k4, k5, k6, k7, k8};
            for (void* o : known) Call(EE_WON_HOST_RELEASE, o);
            Send("released client", c.mClient.get(), "still");
            Recv("released server", c.Server());
            c.mClient->RemoveAllMsgTransforms();
            c.Server()->RemoveAllMsgTransforms();
        }

        // a session authentication: the peer server refuses the mode, no session
        Auth p;
        Authenticate(p, AUTH_TYPE_SESSION, aClientCtx.get(), aServerCtx.get(), aCore);
        sOp = p.mClientOp.get();
        void* pc = Session("session client", p.mClientOp.get());
        void* ps = Session("session server", p.mServerOp.get());
        Log("keys same " + N(Key(p.mClientOp.get()) == Key(p.mServerOp.get())));
        if (p.mPair.mServer.get() != NULL) {
            Send("client", p.mPair.mClient.get(), "unencrypted hello");
            Recv("server", p.mPair.Server());
        }
        // the ops' sessions released, then the ops: the transforms keep the sessions
        sOp = a.mClientOp.get();
        void* mine[] = {sc, sc2, ss, got, pc, ps};
        for (void* o : mine)
            if (o != NULL) Call(EE_WON_HOST_RELEASE, o);
        sOp = NULL;
        a.mClientOp = NULL;
        a.mServerOp = NULL;
        p.mClientOp = NULL;
        p.mServerOp = NULL;
        aCore.Pump(20);
        if (a.mPair.mServer.get() != NULL) {
            SmartPtr<GetCertOp> aPlain = new GetCertOp(aClientCtx.get());
            sOp = aPlain.get();
            Send("client after release", a.mPair.mClient.get(), "after the op");
            Recv("server after release", a.mPair.Server());
            Send("server after release", a.mPair.Server(), "back after the op");
            Recv("client after release", a.mPair.mClient.get());
            sOp = NULL;
        }
        // an op without a session
        SmartPtr<PeerAuthOp> aNone = new PeerAuthOp(sListenAddr, aClientCtx.get(), AUTH_TYPE_SESSION, NULL, true);
        sOp = aNone.get();
        Session("unrun op", aNone.get());
        Log("unrun key " + Hex(Key(aNone.get())));
        sOp = NULL;
        aNone = NULL;

        a.mPair = Pair();
        b = Pair();
        c = Pair();
        p.mPair = Pair();
        sListener = NULL;
        aClientCtx = NULL;
        aServerCtx = NULL;
        aCore.Pump(20);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    Log("done");
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    string aPath = __FILE__;
    aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aPath += "../../rust/crates/ee_won_net/src/golden_authsession.txt";
    FILE* aFile = fopen(aPath.c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", aPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_authsession_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
