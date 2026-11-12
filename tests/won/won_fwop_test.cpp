// DetectFirewallOp as a whole op: the original's statements (copied from DetectFirewallOp.cpp and its messages
// before the port, `this->` an op stand-in) against the Rust op (ee_won fwop.rs) through its hooks, both over the
// same stand-ins for the listening socket, the datagram receive ops and the op's base (Finish, Track, the base
// hooks, the mode, the time left, the current server), whose answers come from a generated script and whose every
// call is logged. 100,000 sequences of the op's steps (RunHook, GetNextRequest, CheckResponse, CallbackHook,
// CleanupHook, RecvServerDatagram, the setters) on one op: the trace, the return and the members after each step.
// The exit code is the number of failed checks.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_fwop.txt, recorded
// before the port; `fwop::tests::golden` replays it).
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 9191;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string sTrace;
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
static string Hex(const void* inData, size_t inLength)
{
    static const char kHex[] = "0123456789abcdef";
    string s;
    const unsigned char* p = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inLength; i++) { s += kHex[p[i] >> 4]; s += kHex[p[i] & 15]; }
    return s.empty() ? "-" : s;
}
static string Unhex(const string& h)
{
    string s;
    if (h == "-") return s;
    for (size_t i = 0; i + 1 < h.size(); i += 2) s += (char)strtol(h.substr(i, 2).c_str(), NULL, 16);
    return s;
}

// ---- the script: what the environment answers

struct Script {
    int mMode;              // GetMode(): 1 async, 2 blocking
    unsigned long mTimeLeft;
    int mCurHost;           // the current server's host
    int mBind, mListen;     // their statuses
    unsigned short mLocalPort;
    bool mBaseCallback;     // ServerRequestOp::CallbackHook's return
    std::vector<int> mHosts;    // each new receive op's sender host, in order (then the current host)
    std::vector<int> mRuns;     // each blocking Run's status, in order (then WS_Timeout)
    size_t mHost, mRun;
};
static Script sScript;
static int sSerial;

struct SSocket : public RefCount {     // BlockingSocket
    int mSerial;
    explicit SSocket(int inType) { this->mSerial = ++sSerial; Log("sock#%d u%d", this->mSerial, inType); }
    long Bind(unsigned short inPort) { Log("bind#%d %u", this->mSerial, (unsigned)inPort); return sScript.mBind; }
    long Listen(int inBacklog) { Log("listen#%d %d", this->mSerial, inBacklog); return sScript.mListen; }
    unsigned short GetLocalPort() { Log("port#%d", this->mSerial); return sScript.mLocalPort; }
    void Close() { Log("close#%d", this->mSerial); }
};

struct SRecv : public RefCount {       // RecvBytesFromOp
    int mSerial, mHost;
    SRecv(int inMax, const SSocket* inSocket)
    {
        this->mSerial = ++sSerial;
        this->mHost = sScript.mHost < sScript.mHosts.size() ? sScript.mHosts[sScript.mHost++] : sScript.mCurHost;
        Log("recv#%d %d s%d", this->mSerial, inMax, inSocket ? inSocket->mSerial : 0);
    }
    void RunAsync(unsigned long inWait) { Log("async#%d %lu", this->mSerial, inWait); }
    long Run(int inMode, unsigned long inWait)
    {
        Log("run#%d %d %lu", this->mSerial, inMode, inWait);
        return sScript.mRun < sScript.mRuns.size() ? sScript.mRuns[sScript.mRun++] : (long)WS_TimedOut;
    }
    // GetAddr().GetHost() != GetCurAddr().GetHost()
    bool HostDiffers() const { Log("host#%d", this->mSerial); return this->mHost != sScript.mCurHost; }
};

// ---- the original's statements (DetectFirewallOp.cpp and WONMiscOpsMsg.h's FirewallMsg before the port)

struct SOp {
    unsigned char mLengthFieldSize;
    unsigned long mMaxRecvTime;
    unsigned short mListenPort;
    unsigned long mMaxConnectWaitTime;
    SmartPtr<SSocket> mListenSocket;
    bool mDoListen, mWaitForConnect, mUseUDP;
    SmartPtr<SRecv> mRecvBytesFromOp;
    string mPublicAddr;
    ByteBufferPtr mRequest, mResponse;
    // the base's parts
    void Finish(long s) { Log("finish %ld", s); }
    void Track(const SRecv* r, int p) { Log("track#%d %d", r->mSerial, p); }
    void BaseRunHook() { Log("baserun"); }
    void BaseCleanupHook() { Log("baseclean"); }
    bool BaseCallbackHook(int p) { Log("basecb %d", p); return sScript.mBaseCallback; }
    int GetMode() { Log("mode"); return sScript.mMode; }
    unsigned long TimeLeft() { Log("timeleft"); return sScript.mTimeLeft; }
    long InvalidReplyHeader() { Log("invalid"); return WS_ServerReq_TryNextServer; }
    void SetPublic(long h) { char b[32]; snprintf(b, sizeof(b), "%ld", h); this->mPublicAddr = b; Log("public %ld", h); }
    string Dump() const
    {
        char b[200];
        snprintf(b, sizeof(b), "port %u wait %lu lfs %u sock %d listen %d waitc %d udp %d recv %d pub %s req ",
                 (unsigned)this->mListenPort, this->mMaxConnectWaitTime, (unsigned)this->mLengthFieldSize,
                 this->mListenSocket.get() ? this->mListenSocket->mSerial : 0, this->mDoListen, this->mWaitForConnect,
                 this->mUseUDP, this->mRecvBytesFromOp.get() ? this->mRecvBytesFromOp->mSerial : 0,
                 this->mPublicAddr.empty() ? "-" : this->mPublicAddr.c_str());
        return string(b) + (this->mRequest.get() ? Hex(this->mRequest->data(), this->mRequest->length()) : "-");
    }
};

namespace Orig {
static void Init(SOp& o)
{
    o.mLengthFieldSize = 2;
    o.mListenPort = 0;
    o.mMaxConnectWaitTime = 0;
    o.mDoListen = true;
    o.mUseUDP = false;
    o.mWaitForConnect = true;
    o.mMaxRecvTime = 0xffffffff;
}
static long RecvServerDatagram(SOp& o);
static unsigned long Wait(unsigned long inTimeLeft, unsigned long mMaxConnectWaitTime, bool inAsync)
{
    unsigned long wait = inTimeLeft;
    if (mMaxConnectWaitTime > 0 && mMaxConnectWaitTime < wait)
        wait = mMaxConnectWaitTime;
    if (inAsync && wait == 0) wait = 0xffffffff;
    return wait;
}
static void RunHook(SOp& o)
{
    if (o.mDoListen || o.mUseUDP) {
        o.mListenSocket = new SSocket(o.mUseUDP ? 1 : 0);
        long status = o.mListenSocket->Bind(o.mListenPort);
        if (status != 0) {
            o.Finish(status);
            return;
        }
        if (!o.mUseUDP) {
            status = o.mListenSocket->Listen(1000);
            if (status != 0) {
                o.Finish(status);
                return;
            }
        }
        o.mListenPort = o.mListenSocket->GetLocalPort();
    }
    o.BaseRunHook();
}
static bool CallbackHook(SOp& o, int inParam, long inStatus)
{
    if (inParam != 1000) return o.BaseCallbackHook(inParam);
    if (inStatus == WS_Success && o.mRecvBytesFromOp->HostDiffers()) {
        RecvServerDatagram(o);
        return true;
    }
    o.Finish(inStatus);
    return true;
}
static void CleanupHook(SOp& o)
{
    o.BaseCleanupHook();
    if (o.mListenSocket.get() != NULL) {
        o.mListenSocket->Close();
        o.mListenSocket = NULL;
    }
}
static long GetNextRequest(SOp& o)
{
    WriteBuffer request(o.mLengthFieldSize, true);
    request.AppendByte(5);
    request.AppendShort(3);
    request.AppendShort(o.mUseUDP ? 3 : 1);
    request.AppendShort(o.mListenPort);
    if (!o.mUseUDP) {
        request.AppendBool(o.mWaitForConnect);
        request.AppendLong(o.mMaxConnectWaitTime);
        request.AppendBool(false);
    }
    o.mRequest = request.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}
static long CheckResponse(SOp& o)
{
    ReadBuffer reply(o.mResponse->data(), o.mResponse->length());
    unsigned char header = reply.ReadByte();
    unsigned short service = reply.ReadShort();
    unsigned short message = reply.ReadShort();
    if (header != 5 || service != 3 || message != 2) return o.InvalidReplyHeader();
    short status = reply.ReadShort();
    if (reply.Available() >= 4) {
        long host = reply.ReadLong();
        if (host != 0) o.SetPublic(host);
    }
    if (o.mUseUDP) return RecvServerDatagram(o);
    return status;
}
static long RecvServerDatagram(SOp& o)
{
    if (o.GetMode() == 1) {
        o.mRecvBytesFromOp = new SRecv(255, o.mListenSocket.get());
        unsigned long wait = Wait(o.TimeLeft(), o.mMaxConnectWaitTime, true);
        o.Track(o.mRecvBytesFromOp.get(), 1000);
        o.mRecvBytesFromOp->RunAsync(wait);
        return WS_ServerReq_ExitCommunicationLoop;
    }
    do {
        o.mRecvBytesFromOp = new SRecv(255, o.mListenSocket.get());
        unsigned long wait = Wait(o.TimeLeft(), o.mMaxConnectWaitTime, false);
        long status = o.mRecvBytesFromOp->Run(2, wait);
        if (status != WS_Success) return status;
    } while (o.mRecvBytesFromOp->HostDiffers());
    return WS_Success;
}
} // namespace Orig

// ---- the Rust op over the same stand-ins

struct NOp {
    unsigned char mLengthFieldSize;
    ByteBufferPtr mRequest, mResponse;
    EeWonFirewallOp* mFw;
    NOp() : mLengthFieldSize(2), mFw(NULL) {}
    ~NOp();
    EeWonFirewallHooks Hooks();
    void Finish(long s) { Log("finish %ld", s); }
    void RunHook()
    {
        EeWonFirewallHooks h = this->Hooks();
        long s = 0;
        if (ee_won_fwop_run_hook(this->mFw, &h, &s)) this->Finish(s);
        else Log("baserun");
    }
    bool CallbackHook(int inParam, long inStatus)
    {
        EeWonFirewallHooks h = this->Hooks();
        long s = 0;
        switch (ee_won_fwop_callback_hook(this->mFw, &h, inParam, inStatus, &s)) {
        case 0: Log("basecb %d", inParam); return sScript.mBaseCallback;
        case 1: this->RecvServerDatagram(); return true;
        default: this->Finish(s); return true;
        }
    }
    void CleanupHook()
    {
        Log("baseclean");
        EeWonFirewallHooks h = this->Hooks();
        ee_won_fwop_cleanup_hook(this->mFw, &h);
    }
    long GetNextRequest()
    {
        EeWonFirewallHooks h = this->Hooks();
        return ee_won_fwop_get_next_request(this->mFw, &h, this->mLengthFieldSize);
    }
    long CheckResponse()
    {
        EeWonFirewallHooks h = this->Hooks();
        int aKind = 0;
        long s = ee_won_fwop_check_response(this->mFw, &h, &aKind);
        if (aKind < 0) throw ReadBufferException("Attempt to read past end of buffer.");
        if (aKind == 1) return this->RecvServerDatagram();
        return s;
    }
    long RecvServerDatagram()
    {
        EeWonFirewallHooks h = this->Hooks();
        uint32_t aWait = 0;
        long s = 0;
        if (ee_won_fwop_recv_datagram(this->mFw, &h, &aWait, &s)) {
            const SRecv* r = static_cast<const SRecv*>(static_cast<RefCount*>(ee_won_fwop_recv_op(this->mFw)));
            Log("track#%d %d", r->mSerial, 1000);
            const_cast<SRecv*>(r)->RunAsync(aWait);
            return WS_ServerReq_ExitCommunicationLoop;
        }
        return s;
    }
    string Dump() const
    {
        const SSocket* s = static_cast<const SSocket*>(static_cast<RefCount*>(ee_won_fwop_listen_socket(this->mFw)));
        const SRecv* r = static_cast<const SRecv*>(static_cast<RefCount*>(ee_won_fwop_recv_op(this->mFw)));
        int32_t aHost = 0;
        char aPub[32] = "-";
        if (ee_won_fwop_public_host(this->mFw, &aHost)) snprintf(aPub, sizeof(aPub), "%ld", (long)aHost);
        char b[200];
        snprintf(b, sizeof(b), "port %u wait %lu lfs %u sock %d listen %d waitc %d udp %d recv %d pub %s req ",
                 (unsigned)ee_won_fwop_listen_port(this->mFw), (unsigned long)ee_won_fwop_max_wait(this->mFw),
                 (unsigned)this->mLengthFieldSize, s ? s->mSerial : 0, ee_won_fwop_do_listen(this->mFw),
                 ee_won_fwop_wait_for_connect(this->mFw), ee_won_fwop_use_udp(this->mFw), r ? r->mSerial : 0, aPub);
        return string(b) + (this->mRequest.get() ? Hex(this->mRequest->data(), this->mRequest->length()) : "-");
    }
};

namespace NH {
static RefCount* R(void* p) { return static_cast<RefCount*>(p); }
static NOp* O(void* c) { return static_cast<NOp*>(c); }
static SSocket* S(void* p) { return static_cast<SSocket*>(R(p)); }
static SRecv* Rv(void* p) { return static_cast<SRecv*>(R(p)); }
static void Release(void* p) { R(p)->Release(); }
static void* NewSocket(void*, bool udp) { SSocket* s = new SSocket(udp ? 1 : 0); s->AddRef(); return static_cast<RefCount*>(s); }
static long Bind(void*, void* s, uint16_t port) { return S(s)->Bind(port); }
static long Listen(void*, void* s) { return S(s)->Listen(1000); }
static uint16_t LocalPort(void*, void* s) { return S(s)->GetLocalPort(); }
static void Close(void*, void* s) { S(s)->Close(); }
static void* NewRecv(void*, void* s)
{
    SRecv* r = new SRecv(255, s ? S(s) : NULL);
    r->AddRef();
    return static_cast<RefCount*>(r);
}
static long RecvRun(void*, void* r, uint32_t wait) { return Rv(r)->Run(2, wait); }
static bool RecvFromCurrent(void*, void* r) { return !Rv(r)->HostDiffers(); }
static uint32_t TimeLeft(void*) { Log("timeleft"); return (uint32_t)sScript.mTimeLeft; }
static int32_t Mode(void*) { Log("mode"); return sScript.mMode; }
static void SetRequest(void* o, const uint8_t* d, size_t n) { WonByteBufferSink(&O(o)->mRequest, d, n); }
static void Response(void* o, EeWonByteSink s, void* c)
{
    s(c, reinterpret_cast<const uint8_t*>(O(o)->mResponse->data()), O(o)->mResponse->length());
}
static long InvalidHeader(void*) { Log("invalid"); return WS_ServerReq_TryNextServer; }
static void SetPublic(void*, int32_t h) { Log("public %ld", (long)h); }
}

EeWonFirewallHooks NOp::Hooks()
{
    EeWonFirewallHooks h;
    h.context = this;
    h.release = NH::Release;
    h.new_socket = NH::NewSocket;
    h.bind = NH::Bind;
    h.listen = NH::Listen;
    h.local_port = NH::LocalPort;
    h.close = NH::Close;
    h.new_recv = NH::NewRecv;
    h.recv_run = NH::RecvRun;
    h.recv_from_current = NH::RecvFromCurrent;
    h.time_left = NH::TimeLeft;
    h.mode = NH::Mode;
    h.set_request = NH::SetRequest;
    h.response = NH::Response;
    h.invalid_header = NH::InvalidHeader;
    h.set_public = NH::SetPublic;
    return h;
}
NOp::~NOp()
{
    if (this->mFw != NULL) {
        EeWonFirewallHooks h = this->Hooks();
        ee_won_fwop_free(this->mFw, &h);
    }
}

// ---- the sequences

struct Step {
    int mKind;      // 0 RunHook, 1 GetNextRequest, 2 CheckResponse, 3 CallbackHook, 4 CleanupHook,
                    // 5 RecvServerDatagram, 6 the setters
    string mReply;  // 2
    int mParam;     // 3: the param; 6: the port
    long mStatus;   // 3: the completed op's status
    bool mA, mB;    // 6: SetDoListen, SetWaitForConnect
};
struct Seq {
    Script mScript;
    bool mUdp, mInit;
    unsigned char mLfs;
    unsigned long mMaxWait;
    std::vector<Step> mSteps;
    void Generate()
    {
        Script& s = this->mScript;
        s.mMode = Pick(2) ? 1 : 2;
        s.mTimeLeft = Pick(4) ? Pick(300) : Next() << 8;
        s.mCurHost = Pick(3);
        static const int kStatus[] = {0, 0, 0, -5, 100012};
        s.mBind = kStatus[Pick(5)];
        s.mListen = kStatus[Pick(5)];
        s.mLocalPort = (unsigned short)Next();
        s.mBaseCallback = Pick(2) != 0;
        for (int i = Pick(5); i > 0; i--) s.mHosts.push_back(Pick(3));
        for (int i = Pick(5); i > 0; i--) s.mRuns.push_back(kStatus[Pick(5)]);
        s.mHost = s.mRun = 0;
        this->mUdp = Pick(2) != 0;
        this->mInit = Pick(4) != 0;
        static const unsigned char kLfs[] = {0, 1, 2, 4};
        this->mLfs = kLfs[Pick(4)];
        this->mMaxWait = Pick(3) ? Pick(200) : Next() << 8;
        for (int i = 1 + Pick(10); i > 0; i--) {
            Step t;
            static const int kKinds[] = {0, 0, 1, 2, 2, 3, 3, 4, 5, 6};
            t.mKind = kKinds[Pick(10)];
            t.mParam = t.mKind == 3 ? (Pick(4) ? 1000 : Pick(3)) : (int)(unsigned short)Next();
            t.mStatus = kStatus[Pick(5)];
            t.mA = Pick(2) != 0;
            t.mB = Pick(2) != 0;
            if (t.mKind == 2) {
                string r = string(1, (char)(Pick(8) ? 5 : Pick(8))) + "\x03" + string(1, '\0') +
                           string(1, (char)(Pick(8) ? 2 : 3)) + string(1, '\0');
                for (int k = 0; k < 2; k++) r += (char)Next();
                if (Pick(2)) for (int k = 0; k < 4; k++) r += Pick(3) ? (char)Next() : '\0';
                for (int k = Pick(3); k > 0; k--) r += (char)Next();
                if (Pick(3) == 0) r = r.substr(0, Pick((int)r.size() + 1));
                t.mReply = r;
            }
            this->mSteps.push_back(t);
        }
    }
    string Line() const
    {
        const Script& s = this->mScript;
        char b[200];
        snprintf(b, sizeof(b), "S %d %lu %d %d %d %u %d %d %d %u %lu", s.mMode, s.mTimeLeft, s.mCurHost, s.mBind,
                 s.mListen, (unsigned)s.mLocalPort, s.mBaseCallback, this->mUdp, this->mInit, (unsigned)this->mLfs,
                 this->mMaxWait);
        string l = b;
        l += " h";
        for (size_t i = 0; i < s.mHosts.size(); i++) l += (i ? "," : "") + std::to_string(s.mHosts[i]);
        l += " r";
        for (size_t i = 0; i < s.mRuns.size(); i++) l += (i ? "," : "") + std::to_string(s.mRuns[i]);
        return l;
    }
};
static string StepLine(const Step& t)
{
    char b[64];
    snprintf(b, sizeof(b), "T %d %d %ld %d %d ", t.mKind, t.mParam, t.mStatus, t.mA, t.mB);
    return b + Hex(t.mReply.data(), t.mReply.size());
}

static string OrigStep(SOp& o, const Step& t)
{
    sTrace.clear();
    long r = 0;
    bool threw = false;
    try {
        switch (t.mKind) {
        case 0: Orig::RunHook(o); break;
        case 1: r = Orig::GetNextRequest(o); break;
        case 2:
            o.mResponse = new ByteBuffer(t.mReply.data(), (unsigned long)t.mReply.size(), false);
            r = Orig::CheckResponse(o);
            break;
        case 3:
            if (t.mParam == 1000 && t.mStatus == 0 && o.mRecvBytesFromOp.get() == NULL) return "skip";
            r = Orig::CallbackHook(o, t.mParam, t.mStatus);
            break;
        case 4: Orig::CleanupHook(o); break;
        case 5: r = Orig::RecvServerDatagram(o); break;
        case 6: o.mListenPort = (unsigned short)t.mParam; o.mDoListen = t.mA; o.mWaitForConnect = t.mB; break;
        }
    } catch (ReadBufferException&) {
        threw = true;
    }
    return std::to_string((int)threw) + " " + std::to_string(r) + "|" + sTrace + "|" + o.Dump();
}

static string RustStep(NOp& n, const Step& t)
{
    sTrace.clear();
    long r = 0;
    bool threw = false;
    try {
        switch (t.mKind) {
        case 0: n.RunHook(); break;
        case 1: r = n.GetNextRequest(); break;
        case 2:
            n.mResponse = new ByteBuffer(t.mReply.data(), (unsigned long)t.mReply.size(), false);
            r = n.CheckResponse();
            break;
        case 3:
            if (t.mParam == 1000 && t.mStatus == 0 && ee_won_fwop_recv_op(n.mFw) == NULL) return "skip";
            r = n.CallbackHook(t.mParam, t.mStatus);
            break;
        case 4: n.CleanupHook(); break;
        case 5: r = n.RecvServerDatagram(); break;
        case 6:
            ee_won_fwop_set_listen_port(n.mFw, (uint16_t)t.mParam);
            ee_won_fwop_set_do_listen(n.mFw, t.mA);
            ee_won_fwop_set_wait_for_connect(n.mFw, t.mB);
            break;
        }
    } catch (ReadBufferException&) {
        threw = true;
    }
    return std::to_string((int)threw) + " " + std::to_string(r) + "|" + sTrace + "|" + n.Dump();
}

static void Setup(const Seq& q, SOp& o)
{
    sScript = q.mScript;
    sSerial = 0;
    Orig::Init(o);
    if (q.mInit) return;
    o.mUseUDP = q.mUdp;
    o.mLengthFieldSize = q.mLfs;
    o.mMaxConnectWaitTime = q.mMaxWait;
}
static void Setup(const Seq& q, NOp& n)
{
    sScript = q.mScript;
    sSerial = 0;
    n.mFw = ee_won_fwop_new();
    if (q.mInit) return;
    ee_won_fwop_setup(n.mFw, q.mUdp, (uint32_t)q.mMaxWait);
    n.mLengthFieldSize = q.mLfs;
}

static std::vector<string> OrigSequence(const Seq& q)
{
    std::vector<string> out;
    SOp o;
    Setup(q, o);
    for (size_t i = 0; i < q.mSteps.size(); i++) out.push_back(OrigStep(o, q.mSteps[i]));
    return out;
}
static std::vector<string> RustSequence(const Seq& q)
{
    std::vector<string> out;
    NOp n;
    Setup(q, n);
    for (size_t i = 0; i < q.mSteps.size(); i++) out.push_back(RustStep(n, q.mSteps[i]));
    return out;
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        for (int i = 0; i < aPrint; i++) {
            Seq q;
            q.Generate();
            std::vector<string> a = OrigSequence(q);
            printf("%s\n", q.Line().c_str());
            for (size_t j = 0; j < a.size(); j++) printf("%s => %s\n", StepLine(q.mSteps[j]).c_str(), a[j].c_str());
        }
        return 0;
    }
    long aSteps = 0;
    for (int i = 0; i < 100000; i++) {
        Seq q;
        q.Generate();
        std::vector<string> a = OrigSequence(q);
        std::vector<string> b = RustSequence(q);
        for (size_t j = 0; j < a.size(); j++) {
            aSteps++;
            CHECK(a[j] == b[j]);
            if (a[j] != b[j]) {
                if (sFailures < 6)
                    printf("%s\n%s\n  orig %s\n  new  %s\n", q.Line().c_str(), StepLine(q.mSteps[j]).c_str(),
                           a[j].c_str(), b[j].c_str());
                break;
            }
        }
    }
    printf("won_fwop_test: %lld checks, %d failures (%ld steps of 100000 sequences)\n", sChecks, sFailures, aSteps);
    return sFailures;
}
