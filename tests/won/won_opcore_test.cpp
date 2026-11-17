// RustServerOp's own host calls and the AsyncOp machinery behind them (ServerOp.cpp's HostCall: the op's status,
// mode, time left, timeout and Finish; ServerOp's members: the length field, the connect, send and receive limits,
// the auth type and context, the client key, its socket; the tracker: Track, KillAll, the base's cleanup, whether
// it is empty; the timers and repeating receives whose completions call the op back; a completion called, an
// object's references; another Rust op's numbers and bytes; a server context's addresses; an HTTP session's AddOp;
// the HTTP get's modify time; a new peer authentication op's members), over real sockets on 127.0.0.1, one line per
// event, compared with rust/crates/ee_won_net/src/golden_opcore.txt (recorded over ServerOp.cpp's HostCall, AsyncOp.cpp
// and AsyncOpWithTracker.cpp before these moved into Rust, net round 24). Every call goes through an op's own call
// path (ee_won_hostop_call), so the lines are the C++ side's answers before the move and the Rust op's after it.
// Running parents are HTTP get ops connected to a listener that never answers, so they stay pending until a call
// finishes them. Waits are on events (an op pumped until it is no longer pending), never on elapsed time, but for
// the timers' own time-outs. Objects are named by the call that made them. `--print` prints the lines instead,
// `--trace` each line as it is logged. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static bool sTrace;
static void Log(const string& inLine)
{
    sLines.push_back(inLine);
    if (sTrace) fprintf(stderr, "%s\n", inLine.c_str());
}
static string N(long long inValue) { return std::to_string(inValue); }
static string Hex(const string& inBytes, size_t inMax = 48)
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
static long long Num(int inCode, void* inObj0 = NULL, long long inNum0 = 0, long long inNum1 = 0)
{
    return Call(inCode, inObj0, NULL, inNum0, inNum1).mCall.out_num;
}
static string Bytes(const Answer& a) { return a.mHasBytes ? Hex(a.mBytes) : string("none"); }
static void* R(RefCount* inObj) { return static_cast<void*>(inObj); }
static long long Refs(void* inObj) { return inObj == NULL ? -1 : (long long)static_cast<RefCount*>(inObj)->GetRefCount(); }

// objects named in the order they were made (never by address: a freed object's address comes back)
static std::vector<std::pair<void*, string> > sObjects;
static string Fresh(void* inObj)
{
    if (inObj == NULL) return "null";
    for (size_t i = 0; i < sObjects.size(); i++)
        if (sObjects[i].first == inObj) sObjects[i].first = NULL;
    string aName = "o" + N((long long)sObjects.size());
    sObjects.push_back(std::make_pair(inObj, aName));
    return aName;
}
static string Name(void* inObj)
{
    if (inObj == NULL) return "null";
    for (size_t i = 0; i < sObjects.size(); i++)
        if (sObjects[i].first == inObj) return sObjects[i].second;
    return "unknown";
}
static WONAPICore* sCore;
// pumps until the op is no longer pending, at most about ten seconds
static void PumpUntilDone(AsyncOp* inOp)
{
    for (int i = 0; i < 1000 && (inOp->GetStatus() == WS_Pending || inOp->IsCompletionPending()); i++)
        sCore->Pump(10);
}
static void PumpUntil(bool (*inDone)(), int inMax = 1000)
{
    for (int i = 0; i < inMax && !inDone(); i++) sCore->Pump(10);
}
// the op's own values, each its own statement
static void Own(const string& inWhat, bool inTimed = true)
{
    long long aStatus = Num(EE_WON_HOST_STATUS);
    long long aMode = Num(EE_WON_HOST_MODE);
    long long aLeft = Num(EE_WON_HOST_TIME_LEFT);
    long long aTimeout = Num(EE_WON_HOST_TIMEOUT);
    long long aLfs = Num(EE_WON_HOST_LFS);
    long long aSend = Num(EE_WON_HOST_SEND_TIME);
    long long aRecv = Num(EE_WON_HOST_RECV_TIME);
    long long aConnect = Num(EE_WON_HOST_CONNECT_TIME);
    long long anAuth = Num(EE_WON_HOST_AUTH_TYPE);
    long long aKey = Num(EE_WON_HOST_CLIENT_KEY);
    void* aSocket = Call(EE_WON_HOST_OWN_SOCKET).mCall.out_obj;
    long long anEmpty = Num(EE_WON_HOST_TRACKER_EMPTY);
    // (a blocking op's time left counts from its start, 0 for an op never run: not logged)
    if (!inTimed) aLeft = aConnect = -2;
    Log(inWhat + ": status " + N(aStatus) + " mode " + N(aMode) + " left " + N(aLeft) + " timeout " + N(aTimeout)
        + " lfs " + N(aLfs) + " send " + N(aSend) + " recv " + N(aRecv) + " connect " + N(aConnect) + " auth "
        + N(anAuth) + " key " + N(aKey) + " socket " + Name(aSocket) + " tracker empty " + N(anEmpty));
}

// a completion of the test's own: the op it was called with, by name, and its references then
static std::vector<string> sCompleted;
static void TestCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inTag)
{
    RefCount* anOp = inOp.get();
    sCompleted.push_back(Name(R(anOp)) + " refs " + N(Refs(R(anOp))) + " tag " + Name(R(inTag.get())));
}

static SmartPtr<AsyncSocket> sListener;
static unsigned short sPort;
// a running HTTP get to the silent listener (pending: connected, its request sent, waiting for the status line)
static RustServerOp* Running(const string& inPath)
{
    char aHost[64];
    snprintf(aHost, sizeof(aHost), "127.0.0.1:%u", (unsigned)sPort);
    RustServerOp* anOp = RustServerOp::NewHttpGet(aHost, inPath);
    anOp->AddRef();
    anOp->RunAsync(20000);
    return anOp;
}
static RustServerOp* sWaitOp;
// the running op waiting for the status line (its request sent: the line's receive is the one op tracked), or no
// longer pending
static bool Connected()
{
    if (sWaitOp->GetStatus() != WS_Pending) return true;
    if (sWaitOp->GetHttpSocket() == NULL || sWaitOp->mTracker.GetNumTrack() != 1) return false;
    return dynamic_cast<RecvCRMsgOp*>(sWaitOp->mTracker.GetTrackedOp(0)) != NULL;
}
static void* sWaitTimer;
static bool TimerDone() { return static_cast<AsyncOp*>(static_cast<RefCount*>(sWaitTimer))->GetStatus() != WS_Pending; }
// the timer thread gave the timer back (its entry's reference, released after it finished the timer, on its thread)
static bool TimerAlone() { return Refs(sWaitTimer) <= 1; }
static void Release(RustServerOp* inOp)
{
    if (inOp->GetStatus() == WS_Pending) inOp->Kill(true);
    sCore->Pump(10);
    inOp->Release();
}

int main(int argc, char** argv)
{
    sTrace = argc > 1 && strcmp(argv[argc - 1], "--trace") == 0;
    string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    string aDir = (aSlash == string::npos ? string(".") : aExe.substr(0, aSlash)) + "/opcore_files/";
    PlatCreateDirectory(aDir.c_str());
    string aFile = aDir + "local.bin";
    {
        FILE* f = fopen(aFile.c_str(), "wb");
        if (f != NULL) {
            fputs("local", f);
            fclose(f);
        }
#ifdef _WIN32
        struct _utimbuf aTime;
        aTime.actime = aTime.modtime = 1000000000;
        _utime(aFile.c_str(), &aTime);
#else
        struct utimbuf aTime;
        aTime.actime = aTime.modtime = 1000000000;
        utime(aFile.c_str(), &aTime);
#endif
    }
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(aDir.c_str());
        aCore.Startup();
        sCore = &aCore;

        // the silent listener every running op connects to
        sListener = new BlockingSocket(AsyncSocket::TCP);
        sListener->Fn2B0D90(0, true);
        sListener->Listen(1000);
        sPort = sListener->GetLocalPort();
        Log("listener port nonzero " + N(sPort != 0));

        // ---- the op's own values: each kind of op as made, then after the setters
        SmartPtr<RustServerOp> aGet = RustServerOp::NewHttpGet("127.0.0.1", "/own");
        sOp = aGet.get();
        Log("http get " + Fresh(R(aGet.get())));
        Own("http get");
        SmartPtr<ServerContext> aContext = new ServerContext;
        aContext->SetShuffleAddresses(false);
        aContext->AddAddress(IPAddr("127.0.0.1", 15101));
        aContext->AddAddress(IPAddr("127.0.0.1", 15102));
        aContext->AddAddress(IPAddr("127.0.0.1", 15103));
        Log("server context " + Fresh(R(aContext.get())));
        std::wstring aPath = L"/TitanServers";
        SmartPtr<RustServerOp> aDirOp = RustServerOp::NewGetDir(aContext.get(), aPath, 0x10);
        sOp = aDirOp.get();
        Log("get dir " + Fresh(R(aDirOp.get())));
        Own("get dir");
        SmartPtr<AuthContext> aCtx = new AuthContext;
        Log("auth context " + Fresh(R(aCtx.get())));
        SmartPtr<RustServerOp> anAccount = RustServerOp::NewGetAccount(aContext.get(), aCtx.get());
        sOp = anAccount.get();
        Log("get account " + Fresh(R(anAccount.get())));
        Own("get account");
        {
            const void* aCore0 = Call(EE_WON_HOST_AUTH_CORE).mCall.out_obj;
            const void* aCore1 = Call(EE_WON_HOST_AUTH_CORE, R(aCtx.get())).mCall.out_obj;
            Log("auth core own " + N(aCore0 != NULL) + " same as ctx " + N(aCore0 == aCore1) + " is ctx's "
                + N(aCore1 == static_cast<const void*>(aCtx->Core())));
        }
        SmartPtr<BlockingSocket> aPeerSocket = new BlockingSocket(AsyncSocket::TCP);
        Log("peer socket " + Fresh(R(aPeerSocket.get())));
        SmartPtr<PeerAuthOp> aPeer = new PeerAuthOp(IPAddr("127.0.0.1", 15104), aCtx.get(), AUTH_TYPE_PERSISTENT,
                                                    aPeerSocket.get(), true);
        sOp = aPeer.get();
        Log("peer auth " + Fresh(R(aPeer.get())) + " socket refs " + N(Refs(R(aPeerSocket.get()))));
        Own("peer auth");
        Log("peer auth cert status " + N(Num(EE_WON_HOST_PEER_AUTH_CERT, R(aPeer.get()))));
        SmartPtr<PeerAuthServerOp> aPeerServer = new PeerAuthServerOp(aCtx.get(), aPeerSocket.get());
        sOp = aPeerServer.get();
        Log("peer auth server " + Fresh(R(aPeerServer.get())));
        Own("peer auth server");
        SmartPtr<GetCertOp> aCert = new GetCertOp(aCtx.get());
        sOp = aCert.get();
        Log("get cert " + Fresh(R(aCert.get())));
        Own("get cert");
        SmartPtr<DetectFirewallOp> aFirewall = new DetectFirewallOp(aContext.get());
        sOp = aFirewall.get();
        Log("detect firewall " + Fresh(R(aFirewall.get())));
        Own("detect firewall");
        {
            const void* aCore0 = Call(EE_WON_HOST_AUTH_CORE).mCall.out_obj;
            Log("auth core none " + N(aCore0 != NULL));
        }

        // the setters
        sOp = aGet.get();
        aGet->SetMaxTimes(1000, 2000, 3000);
        Own("max times 1000 2000 3000");
        aGet->SetLengthFieldSize(2);
        Own("length field 2");
        aGet->SetAuthType(AUTH_TYPE_SESSION, aCtx.get());
        Own("auth session");
        Log("ctx refs " + N(Refs(R(aCtx.get()))));
        aGet->SetClientKeySize(16, true);
        Own("client key 16 on");
        aGet->SetClientKeySize(24, false);
        Own("client key 24 off");
        aGet->SetTimeout(5000);
        Own("timeout 5000");
        aGet->SetMode(OP_MODE_BLOCK);
        aGet->SetTimeout(0xffffffff);
        Own("blocking forever", false);
        aGet->SetMaxTimes(-1, 7, 0);
        Own("max times -1 7 0", false);
        aGet->SetMode(OP_MODE_ASYNC);
        Own("async again");
        {
            const void* aCore0 = Call(EE_WON_HOST_AUTH_CORE).mCall.out_obj;
            Log("auth core set " + N(aCore0 == static_cast<const void*>(aCtx->Core())));
        }
        aGet->SetAuthType(AUTH_TYPE_NONE);
        Own("auth none");
        Log("ctx refs " + N(Refs(R(aCtx.get()))));
        aGet->SetAuthType(AUTH_TYPE_PERSISTENT_NOCRYPT, aCtx.get());

        // a new peer authentication op made by an op: its members from the op (auth context and type, times)
        {
            aGet->SetMaxTimes(1100, 2200, 3300);
            IPAddr aTo("127.0.0.1", 15105);
            Answer a = Call(EE_WON_HOST_NEW_PEER_AUTH, R(aPeerSocket.get()), NULL, 0, 0, string(), &aTo);
            void* p = a.mCall.out_obj;
            Log("new peer auth " + Fresh(p) + " refs " + N(Refs(p)) + " socket refs " + N(Refs(R(aPeerSocket.get()))));
            RustServerOp* aNew = static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(p)));
            sOp = aNew;
            Own("new peer auth");
            const void* aCore0 = Call(EE_WON_HOST_AUTH_CORE).mCall.out_obj;
            Log("new peer auth core is ctx's " + N(aCore0 == static_cast<const void*>(aCtx->Core())) + " cert status "
                + N(Num(EE_WON_HOST_PEER_AUTH_CERT, p)));
            sOp = aGet.get();
            a = Call(EE_WON_HOST_NEW_PEER_AUTH, NULL, NULL, 1, 0, string(), &aTo);
            void* q = a.mCall.out_obj;
            Log("new peer auth no socket " + Fresh(q) + " refs " + N(Refs(q)));
            sOp = static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(q)));
            Own("new peer auth no socket");
            sOp = aGet.get();
            Call(EE_WON_HOST_RELEASE, p);
            Call(EE_WON_HOST_RELEASE, q);
            Log("socket refs after release " + N(Refs(R(aPeerSocket.get()))));
        }

        // references and a completion called
        {
            void* s = R(aPeerSocket.get());
            Log("refs " + N(Refs(s)));
            Call(EE_WON_HOST_ADD_REF, s);
            Log("add ref " + N(Refs(s)));
            Call(EE_WON_HOST_ADD_REF, s);
            Log("add ref " + N(Refs(s)));
            Call(EE_WON_HOST_RELEASE, s);
            Log("release " + N(Refs(s)));
            Call(EE_WON_HOST_RELEASE, s);
            Log("release " + N(Refs(s)));
            SmartPtr<CompletionBase<AsyncOpPtr> > aCompletion =
                new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(TestCallback, aContext.get());
            Log("completion " + Fresh(R(aCompletion.get())) + " refs " + N(Refs(R(aCompletion.get()))));
            Call(EE_WON_HOST_COMPLETE, R(aCompletion.get()));
            Log("completed " + (sCompleted.empty() ? string("none") : sCompleted.back()) + " refs after "
                + N(Refs(R(aGet.get()))) + " completion refs " + N(Refs(R(aCompletion.get()))));
            sOp = aDirOp.get();
            Call(EE_WON_HOST_COMPLETE, R(aCompletion.get()));
            Log("completed " + (sCompleted.empty() ? string("none") : sCompleted.back()));
            sOp = aGet.get();
        }

        // another Rust op's numbers, bytes and settings, a peer op's certificate status, the HTTP modify time
        {
            SmartPtr<RustServerOp> anOther = RustServerOp::NewHttpGet("example.invalid", "/other/path");
            void* o = R(anOther.get());
            Log("other " + Fresh(o));
            Log("other host " + Bytes(Call(EE_WON_HOST_HOSTOP_BYTES, o, NULL, EE_WON_HTTPGET_HOST)));
            Log("other remote path " + Bytes(Call(EE_WON_HOST_HOSTOP_BYTES, o, NULL, EE_WON_HTTPGET_REMOTE_PATH)));
            Log("other local path " + Bytes(Call(EE_WON_HOST_HOSTOP_BYTES, o, NULL, EE_WON_HTTPGET_LOCAL_PATH)));
            Log("other last modified " + N(Num(EE_WON_HOST_HOSTOP_NUM, o, EE_WON_HTTPGET_LAST_MODIFIED)));
            Log("other modified " + N(Num(EE_WON_HOST_HTTP_MODIFIED, o)));
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_LOCAL_PATH, 0, aFile);
            Log("other local path set " + N(anOther->GetBytes(EE_WON_HTTPGET_LOCAL_PATH) == aFile));
            Log("other modified " + N(Num(EE_WON_HOST_HTTP_MODIFIED, o)));
            Log("other last modified " + N(Num(EE_WON_HOST_HOSTOP_NUM, o, EE_WON_HTTPGET_LAST_MODIFIED)));
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_LAST_MODIFIED, 0);
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_LOCAL_PATH, 0, aDir + "missing.bin");
            Log("other modified missing " + N(Num(EE_WON_HOST_HTTP_MODIFIED, o)));
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_LAST_MODIFIED, 0);
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_BINARY, 1);
            Log("other binary " + N(Num(EE_WON_HOST_HOSTOP_NUM, o, EE_WON_HTTPGET_BINARY)));
            Call(EE_WON_HOST_HOSTOP_SET, o, NULL, EE_WON_HTTPGET_CLOSE, 1);
            Log("other close " + N(Num(EE_WON_HOST_HOSTOP_NUM, o, EE_WON_HTTPGET_CLOSE)));
            Log("other modified zero " + N(Num(EE_WON_HOST_HTTP_MODIFIED, o)));
            Log("peer cert status " + N(Num(EE_WON_HOST_PEER_AUTH_CERT, R(aPeer.get()))));
        }

        // a server context's addresses and a failed one
        {
            void* c = R(aContext.get());
            Log("ctx addrs " + Bytes(Call(EE_WON_HOST_CTX_ADDRS, c)));
            IPAddr aFailed("127.0.0.1", 15101);
            Call(EE_WON_HOST_CTX_FAILED, c, NULL, 0, 0, string(), &aFailed);
            Log("ctx addrs after failed " + Bytes(Call(EE_WON_HOST_CTX_ADDRS, c)));
            IPAddr anOther("127.0.0.1", 15999);
            Call(EE_WON_HOST_CTX_FAILED, c, NULL, 0, 0, string(), &anOther);
            Log("ctx addrs after unknown " + Bytes(Call(EE_WON_HOST_CTX_ADDRS, c)));
            SmartPtr<ServerContext> anEmpty = new ServerContext;
            Log("empty ctx addrs " + Bytes(Call(EE_WON_HOST_CTX_ADDRS, R(anEmpty.get()))));
        }

        // ---- running ops: the tracker, the timers and their completions, Finish
        {
            // a running op tracks its connect, send and receive; a timer tracked under 5 times out and its
            // completion (the tracker's) calls the op back, which finishes with the timer's status
            RustServerOp* p = Running("/tracked");
            sOp = p;
            Log("running " + Fresh(R(p)));
            sWaitOp = p;
            PumpUntil(Connected);
            Own("running");
            Answer a = Call(EE_WON_HOST_NEW_TIMER, NULL, NULL, 7);
            void* t = a.mCall.out_obj;
            Log("timer " + Fresh(t) + " refs " + N(Refs(t)) + " op refs " + N(Refs(R(p))));
            Call(EE_WON_HOST_TRACK, t, NULL, 5);
            Log("tracked refs " + N(Refs(t)) + " op refs " + N(Refs(R(p))) + " tracker "
                + N(p->mTracker.GetNumTrack()));
            Call(EE_WON_HOST_RUN_ASYNC, t, NULL, 30);
            Log("timer run status " + N(Num(EE_WON_HOST_OP_STATUS, t)));
            sWaitTimer = t;
            PumpUntil(TimerDone);
            PumpUntilDone(p);
            sWaitTimer = t;
            PumpUntil(TimerAlone);
            Log("timer done " + N(Num(EE_WON_HOST_OP_STATUS, t)) + " refs " + N(Refs(t)));
            Own("after tracked timer");
            Log("op refs " + N(Refs(R(p))) + " tracker " + N(p->mTracker.GetNumTrack()));
            Call(EE_WON_HOST_RELEASE, t);
            Release(p);
        }
        {
            // a timer not tracked: its own completion calls the op back with its id
            RustServerOp* p = Running("/timer");
            sOp = p;
            Log("running " + Fresh(R(p)));
            sWaitOp = p;
            PumpUntil(Connected);
            void* t = Call(EE_WON_HOST_NEW_TIMER, NULL, NULL, 3).mCall.out_obj;
            Log("timer " + Fresh(t) + " refs " + N(Refs(t)) + " op refs " + N(Refs(R(p))));
            Call(EE_WON_HOST_RUN_ASYNC, t, NULL, 30);
            sWaitTimer = t;
            PumpUntil(TimerDone);
            PumpUntilDone(p);
            sWaitTimer = t;
            PumpUntil(TimerAlone);
            Log("timer done " + N(Num(EE_WON_HOST_OP_STATUS, t)) + " refs " + N(Refs(t)));
            Own("after own timer");
            Call(EE_WON_HOST_RELEASE, t);
            Log("op refs " + N(Refs(R(p))));
            Release(p);
        }
        {
            // a killed timer: its completion's killed op kills the op
            RustServerOp* p = Running("/killed");
            sOp = p;
            Log("running " + Fresh(R(p)));
            sWaitOp = p;
            PumpUntil(Connected);
            void* t = Call(EE_WON_HOST_NEW_TIMER, NULL, NULL, 4).mCall.out_obj;
            Log("timer " + Fresh(t));
            Call(EE_WON_HOST_TRACK, t, NULL, 6);
            Call(EE_WON_HOST_RUN_ASYNC, t, NULL, 60000);
            Call(EE_WON_HOST_KILL, t, NULL, 1);
            PumpUntilDone(p);
            sWaitTimer = t;
            PumpUntil(TimerAlone);
            Log("killed timer " + N(Num(EE_WON_HOST_OP_STATUS, t)) + " refs " + N(Refs(t)));
            Own("after killed timer");
            Call(EE_WON_HOST_RELEASE, t);
            Release(p);
        }
        {
            // KillAll: the tracked ops leave the tracker first, so their completions do not call back; the op runs on
            RustServerOp* p = Running("/killall");
            sOp = p;
            Log("running " + Fresh(R(p)));
            sWaitOp = p;
            PumpUntil(Connected);
            void* t = Call(EE_WON_HOST_NEW_TIMER, NULL, NULL, 8).mCall.out_obj;
            Log("timer " + Fresh(t));
            Call(EE_WON_HOST_TRACK, t, NULL, 9);
            Call(EE_WON_HOST_RUN_ASYNC, t, NULL, 60000);
            Log("tracker " + N(p->mTracker.GetNumTrack()) + " empty " + N(Num(EE_WON_HOST_TRACKER_EMPTY)));
            Call(EE_WON_HOST_KILL_ALL);
            for (int i = 0; i < 5; i++) aCore.Pump(10);
            sWaitTimer = t;
            PumpUntil(TimerAlone);
            Log("killed all: timer " + N(Num(EE_WON_HOST_OP_STATUS, t)) + " refs " + N(Refs(t)) + " tracker "
                + N(p->mTracker.GetNumTrack()));
            Own("after kill all");
            // Track again and the base's cleanup (the tracker's KillAll)
            Call(EE_WON_HOST_TRACK, t, NULL, 10);
            Log("tracked again " + N(p->mTracker.GetNumTrack()) + " refs " + N(Refs(t)));
            Call(EE_WON_HOST_TRACK, t, NULL, 11);
            Log("tracked twice " + N(p->mTracker.GetNumTrack()) + " refs " + N(Refs(t)));
            Call(EE_WON_HOST_BASE_CLEANUP);
            Log("base cleanup " + N(p->mTracker.GetNumTrack()) + " refs " + N(Refs(t)));
            Own("after base cleanup");
            // Finish: the op's completion, its cleanup
            SmartPtr<CompletionBase<AsyncOpPtr> > aCompletion =
                new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(TestCallback, NULL);
            p->SetCompletion(aCompletion.get());
            Call(EE_WON_HOST_FINISH, NULL, NULL, 777);
            Log("finished status " + N(p->GetStatus()) + " pending completion " + N(p->IsCompletionPending()));
            PumpUntil([]() { return !sCompleted.empty() && sCompleted.back().compare(0, 1, "o") == 0 &&
                                    sCompleted.back().find(" tag null") != string::npos; });
            Log("finish completed " + (sCompleted.empty() ? string("none") : sCompleted.back()));
            Own("after finish");
            Call(EE_WON_HOST_FINISH, NULL, NULL, 778);
            Log("finished again status " + N(p->GetStatus()));
            Call(EE_WON_HOST_RELEASE, t);
            Release(p);
        }

        // ---- a repeating receive whose completion calls the op back with its id
        {
            // a second listener for the client, so that the accepted socket is the client's
            SmartPtr<BlockingSocket> aListener2 = new BlockingSocket(AsyncSocket::TCP);
            aListener2->Fn2B0D90(0, true);
            aListener2->Listen(1000);
            IPAddr aListen("127.0.0.1", aListener2->GetLocalPort());
            SmartPtr<BlockingSocket> aClient = new BlockingSocket(AsyncSocket::TCP);
            RustServerOp* p = Running("/repeat");
            sOp = p;
            Log("running " + Fresh(R(p)));
            sWaitOp = p;
            PumpUntil(Connected);
            SmartPtr<AcceptOp> anAccept = new AcceptOp(aListener2.get());
            aListener2->QueueOp(anAccept.get(), 10000);
            Log("connect " + N(Call(EE_WON_HOST_CONNECT, R(aClient.get()), NULL, 5000, 0, string(), &aListen).mCall.out_num));
            PumpUntilDone(anAccept.get());
            SmartPtr<AsyncSocket> aServer = anAccept->GetAcceptedSocket();
            Log("accepted " + N(aServer.get() != NULL));
            if (aServer.get() == NULL) return 1;
            void* s = R(aServer.get());
            Log("server " + Fresh(s) + " refs " + N(Refs(s)));
            void* r = Call(EE_WON_HOST_NEW_OP, s, NULL, EE_WON_HOST_OP_RECV_MSG).mCall.out_obj;
            Log("recv " + Fresh(r) + " refs " + N(Refs(r)) + " op refs " + N(Refs(R(p))));
            Call(EE_WON_HOST_SET_REPEAT, s, r, 12);
            Log("repeat set: recv refs " + N(Refs(r)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, r)) + " op refs "
                + N(Refs(R(p))));
            string aMsg("\x09\x00\x00\x00hello", 9);
            Call(EE_WON_HOST_SEND_BYTES, R(aClient.get()), NULL, 2000, 0, aMsg);
            sWaitTimer = r;
            PumpUntil(TimerDone);
            for (int i = 0; i < 5; i++) aCore.Pump(10);
            Log("after message: op " + N(p->GetStatus()) + " recv " + N(Num(EE_WON_HOST_OP_STATUS, r)) + " refs "
                + N(Refs(r)));
            Call(EE_WON_HOST_CLOSE, R(aClient.get()));
            PumpUntilDone(p);
            Log("after close: op " + N(p->GetStatus()) + " recv " + N(Num(EE_WON_HOST_OP_STATUS, r)));
            Own("after repeat");
            Call(EE_WON_HOST_RELEASE, r);
            Call(EE_WON_HOST_SOCKET_KILL, s);
            aServer = NULL;
            Release(p);
            aListener2->Close();
        }

        // ---- an HTTP session's AddOp
        {
            SmartPtr<HTTPSession> aSession = new HTTPSession;
            char aHost[64];
            snprintf(aHost, sizeof(aHost), "127.0.0.1:%u", (unsigned)sPort);
            SmartPtr<RustServerOp> aQueued = RustServerOp::NewHttpGet(aHost, "/session");
            Log("session " + Fresh(R(aSession.get())) + " ops " + N(aSession->GetNumOps()) + " queued "
                + Fresh(R(aQueued.get())) + " refs " + N(Refs(R(aQueued.get()))));
            sOp = aGet.get();
            Call(EE_WON_HOST_SESSION_ADD, R(aSession.get()), R(aQueued.get()));
            Log("session ops " + N(aSession->GetNumOps()) + " queued status " + N(aQueued->GetStatus()) + " refs "
                + N(Refs(R(aQueued.get()))));
            aSession->KillAll();
            for (int i = 0; i < 5; i++) aCore.Pump(10);
            Log("session killed ops " + N(aSession->GetNumOps()) + " queued status " + N(aQueued->GetStatus()));
        }

        sOp = NULL;
        aFirewall = NULL;
        aCert = NULL;
        aPeerServer = NULL;
        aPeer = NULL;
        anAccount = NULL;
        aDirOp = NULL;
        aGet = NULL;
        Log("peer socket refs at end " + N(Refs(R(aPeerSocket.get()))) + " ctx refs " + N(Refs(R(aCtx.get()))));
        aPeerSocket = NULL;
        aCtx = NULL;
        sListener->Close();
        sListener = NULL;
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
    string aGoldenPath = __FILE__;
    aSlash = aGoldenPath.find_last_of("/\\");
    aGoldenPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aGoldenPath += "../../rust/crates/ee_won_net/src/golden_opcore.txt";
    FILE* aGolden = fopen(aGoldenPath.c_str(), "rb");
    if (aGolden == NULL) {
        printf("FAIL: cannot open %s\n", aGoldenPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aGolden)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aGolden);
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
    printf("won_opcore_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
