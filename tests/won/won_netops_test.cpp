// The lobby hosts' network cases R6 WONLobby round 28 answered in Rust (ee_won_lobby netops.rs: LanLogicHost.cpp's,
// EELobbyGameHost.cpp's transfer and RoutingLogic.cpp's ops on AsyncOps, socket ops, queue sockets, byte buffers and
// routing ops) against the deleted cases as they were (won_netops_orig.h), over the real C++ classes on 127.0.0.1.
// Two worlds are made alike (a listener, a client and the socket accepted by an accept op, a message and a datagram
// received by their ops, a timed-out receive, a killed op, an idle timer, two routing ops, a byte buffer); each step
// of a seeded random sequence runs the original case on the first world and the Rust on the second. Compared after
// each step: the result (an object as its name), the bytes a take op appended, an address as the socket it names,
// and a census of both worlds (every object's references, an op's status and kill flag, a socket's open and
// connected flags); the sends are compared by what the peer receives. `--seeds N` (default 6), `--steps N`
// (default 300). The exit code is the number of mismatches.
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
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatSocket.h"
#include "platform/PlatTime.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/netops.h"
#include "won_blocking.h"
#include "won_netops_orig.h"

using namespace WONAPI;

static std::string N(long long v) { return std::to_string(v); }
static WONAPICore* sCore;
static void Pump(int inMs)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + inMs;
    do sCore->Pump(5); while (PlatTimeMilliseconds64() < anEnd);
}
// the stream messages carry their 4-byte length (the sender's buffer is the whole message, as the lobby's)
static const char kHello[] = "\x09\0\0\0hello";
static const char kLanSend[] = "\x0c\0\0\0lan send";
static const char kBuf[] = "\x10\0\0\0buffer bytes";
static ByteBufferPtr Bytes(const char* s) { return new ByteBuffer(s, (unsigned long)strlen(s), false); }

// one world's objects, by index
enum { W_L, W_C, W_S, W_U1, W_U2, W_ACC, W_MSG, W_DG, W_FAIL, W_DONE, W_KILLED, W_TIMER, W_ROP, W_ROP2, W_BUF, W_COUNT };
static const char* const kNames[W_COUNT] = {"L", "C", "S", "U1", "U2", "acc", "msg", "dg", "fail", "done", "killed", "timer",
                                          "rop", "rop2", "buf"};
static bool IsSocket(int i) { return i <= W_U2; }
static bool IsOp(int i) { return i >= W_ACC && i <= W_ROP2; }
struct World {
    RefCount* mObj[W_COUNT];
    std::vector<RefCount*> mNew; // ops the steps made (AsyncOpNew)
    void Make(const char* inTag)
    {
        BlockingSocket* l = new BlockingSocket(0);
        BlockingSocket* c = new BlockingSocket(0);
        QueueSocket* u1 = new QueueSocket(1);
        QueueSocket* u2 = new QueueSocket(1);
        l->AddRef(); // (each object one reference of the world's, before an op holds and drops one)
        c->AddRef();
        u1->AddRef();
        u2->AddRef();
        l->Bind(IPAddr("127.0.0.1", 0), true);
        l->Listen(5);
        WonBlocking::Connect(c, IPAddr("127.0.0.1", l->GetLocalPort()), 5000);
        SocketOp* acc = new SocketOp(EeWonCoreSocketOp::Accept, l);
        acc->AddRef();
        acc->RunBlock(5000);
        AsyncSocket* s = acc->GetAcceptedSocket();
        s->AddRef();
        ByteBufferPtr aTag = new ByteBuffer(inTag, 9, false);
        SmartPtr<SocketOp> send = new SocketOp(EeWonCoreSocketOp::SendMsg, c, aTag.get());
        send->RunBlock(5000);
        SocketOp* msg = new SocketOp(EeWonCoreSocketOp::RecvMsg, s);
        msg->AddRef();
        msg->RunBlock(5000);
        u1->Bind(IPAddr("127.0.0.1", 0), true);
        u2->Bind(IPAddr("127.0.0.1", 0), true);
        IPAddr aTo("127.0.0.1", u2->GetLocalPort());
        SmartPtr<SocketOp> dgsend = new SocketOp(EeWonCoreSocketOp::SendTo, u1, Bytes("datagram").get(), aTo.GetValue());
        dgsend->RunBlock(5000);
        SocketOp* done = dgsend.get(); // succeeded, then killed without finishing
        done->AddRef();
        done->Kill(false);
        SocketOp* dg = new SocketOp(EeWonCoreSocketOp::RecvFrom, u2, NULL, NULL, 512);
        dg->AddRef();
        dg->RunBlock(5000);
        SocketOp* fail = new SocketOp(EeWonCoreSocketOp::RecvBytes, s, NULL, NULL, 4);
        fail->AddRef();
        fail->RunBlock(0);
        AsyncOp* killed = new AsyncOp;
        killed->AddRef();
        killed->Kill(true);
        AsyncOp* timer = new AsyncOp;
        timer->AddRef();
        RoutingOp* rop = new RoutingOp(NULL, 5);
        rop->AddRef();
        rop->SetNum(EE_WON_RTF_GROUP_ID, 12);
        rop->SetNum(EE_WON_RTF_CLIENT_ID, 34);
        const uint16_t anIds[] = {7, 9};
        ee_won_routeop_set_ids(rop->mRust, anIds, 2);
        ee_won_routeop_set_data_type(rop->mRust, reinterpret_cast<const uint8_t*>("G"), 1);
        RoutingOp* rop2 = new RoutingOp(NULL, 6);
        rop2->AddRef();
        ee_won_routeop_set_data_type(rop2->mRust, reinterpret_cast<const uint8_t*>("GX"), 2);
        ByteBuffer* buf = new ByteBuffer(kBuf, 16, false);
        buf->AddRef();
        RefCount* o[W_COUNT] = {l, c, s, u1, u2, acc, msg, dg, fail, done, killed, timer, rop, rop2, buf};
        for (int i = 0; i < W_COUNT; i++) this->mObj[i] = o[i];
    }
    void* P(int i) { return i < 0 ? NULL : static_cast<void*>(this->mObj[i]); }
    std::string Name(intptr_t p)
    {
        if (p == 0) return "0";
        for (int i = 0; i < W_COUNT; i++)
            if ((void*)p == (void*)this->mObj[i]) return kNames[i];
        for (size_t i = 0; i < this->mNew.size(); i++)
            if ((void*)p == (void*)this->mNew[i]) return "new" + N(i);
        for (int i = W_ACC; i <= W_DONE; i++) { // a socket op's message or bytes
            SocketOp* op = static_cast<SocketOp*>(static_cast<AsyncOp*>(this->mObj[i]));
            if ((void*)p == (void*)op->GetMsg().get()) return std::string(kNames[i]) + ".msg";
            if ((void*)p == (void*)op->GetBytes().get()) return std::string(kNames[i]) + ".bytes";
        }
        return "?";
    }
    std::string PortName(unsigned short inPort)
    {
        for (int i = 0; i <= W_U2; i++)
            if (inPort == static_cast<AsyncSocket*>(this->mObj[i])->GetLocalPort()) return kNames[i];
        return N(inPort);
    }
    std::string Census()
    {
        std::string s;
        for (int i = 0; i < W_COUNT; i++) {
            s += std::string(kNames[i]) + ":" + N(this->mObj[i]->GetRefCount());
            if (IsOp(i)) {
                AsyncOp* op = static_cast<AsyncOp*>(this->mObj[i]);
                s += "/" + N(op->GetStatus()) + "/" + N(op->IsKilled());
            }
            if (i == W_ROP || i == W_ROP2)
                for (int f = 0; f < 6; f++) s += "/" + N(static_cast<RoutingOp*>(static_cast<AsyncOp*>(this->mObj[i]))->Num(f));
            if (IsSocket(i)) {
                AsyncSocket* k = static_cast<AsyncSocket*>(this->mObj[i]);
                s += "/" + N(k->IsOpen()) + N(k->IsConnected());
            }
            s += " ";
        }
        for (size_t i = 0; i < this->mNew.size(); i++) {
            AsyncOp* op = static_cast<AsyncOp*>(this->mNew[i]);
            s += "new" + N(i) + ":" + N(op->GetRefCount()) + "/" + N(op->GetStatus()) + " ";
        }
        return s;
    }
};

static int sMismatches, sChecks;
static void Same(const std::string& inWhat, const std::string& a, const std::string& b)
{
    sChecks++;
    if (a != b && sMismatches++ < 20) printf("MISMATCH %s\n  orig %s\n  rust %s\n", inWhat.c_str(), a.c_str(), b.c_str());
}

// the peer's next message (a stream) or datagram, after a send step
static std::string Received(World& w, bool inDatagram)
{
    Pump(30);
    SmartPtr<SocketOp> r = inDatagram
        ? new SocketOp(EeWonCoreSocketOp::RecvFrom, static_cast<AsyncSocket*>(w.mObj[W_U2]), NULL, NULL, 512)
        : new SocketOp(EeWonCoreSocketOp::RecvMsg, static_cast<AsyncSocket*>(w.mObj[W_S]));
    r->RunBlock(3000);
    ByteBufferPtr b = inDatagram ? r->GetBytes() : r->GetMsg();
    std::string s = N(r->GetStatus()) + " " + (b.get() ? std::string(b->data(), b->length()) : std::string("(none)"));
    if (inDatagram) s += " from " + w.PortName(r->GetAddr().GetPort());
    return s;
}

// one step: a table (0 lobby, 1 transfer, 2 routing), an op, its objects (an index; -1 null, -2 a sink, -3 an
// address) and numbers (a routing step's b is a number)
struct Step {
    int mTable, mOp, mA, mB;
    intptr_t mC, mD;
    const char* mText; // the bytes a send step's c, d name
};
static std::string Run(World& w, const Step& s, bool inRust)
{
    typedef EeWonRouteLogicOp RO;
    void* aSink = ee_won_netops_sink_new(); // a Rust Vec, as the logics' take ops pass
    EeWonIpAddr* anAddr = ee_won_ipaddr_new();
    void* a = w.P(s.mA);
    void* b = s.mB == -2 ? aSink : s.mB == -3 ? (void*)anAddr : s.mTable == 2 ? NULL : w.P(s.mB);
    intptr_t c = s.mC, d = s.mD;
    if (s.mText != NULL) {
        c = (intptr_t)s.mText;
        d = s.mD != 0 ? s.mD : (intptr_t)strlen(s.mText);
    }
    IPAddr aTo;
    if (s.mTable == 0 && (EeWonLobbyOp)s.mOp == EeWonLobbyOp::LanSendTo) {
        aTo = IPAddr("127.0.0.1", static_cast<AsyncSocket*>(w.mObj[W_U2])->GetLocalPort());
        b = (void*)aTo.GetValue();
    }
    bool aHandled = false;
    intptr_t r = 0;
    switch (s.mTable) {
    case 0:
        r = inRust ? ee_won_netops_lobby((EeWonLobbyOp)s.mOp, a, b, c, d, &aHandled)
                   : WonNetopsOrig::Lan((EeWonLobbyOp)s.mOp, a, b, c, d, &aHandled);
        break;
    case 1:
        r = inRust ? ee_won_netops_xfer((EeWonEeXferOp)s.mOp, (intptr_t)a, &aHandled)
                   : WonNetopsOrig::Xfer((EeWonEeXferOp)s.mOp, (intptr_t)a, &aHandled);
        break;
    default:
        r = inRust ? ee_won_netops_route((RO)s.mOp, (intptr_t)a, s.mB, c, &aHandled)
                   : WonNetopsOrig::Route((RO)s.mOp, (intptr_t)a, s.mB, c, &aHandled);
        break;
    }
    std::string out = "handled " + N(aHandled);
    bool isNew = s.mTable == 2 && (RO)s.mOp == RO::AsyncOpNew;
    bool isNow = s.mTable == 2 && (RO)s.mOp == RO::Now;
    bool isTick = s.mTable == 2 && (RO)s.mOp == RO::Tick;
    if (isNew && r != 0) w.mNew.push_back(reinterpret_cast<RefCount*>(r));
    if (isNow || isTick) {
        intptr_t want = isNow ? (intptr_t)time(NULL) : (intptr_t)WONTickCount();
        out += " clock " + N(llabs((long long)(want - r)) <= (isNow ? 1 : 200));
    } else
        out += " result " + w.Name(r) + (w.Name(r) == "?" ? "=" + N(r) : "");
    std::string aBytes(ee_won_netops_sink_bytes(aSink, NULL, 0), '\0');
    if (!aBytes.empty()) {
        ee_won_netops_sink_bytes(aSink, reinterpret_cast<uint8_t*>(&aBytes[0]), aBytes.size());
        out += " sink " + aBytes;
    }
    ee_won_netops_sink_free(aSink);
    if (s.mB == -3) out += " addr " + w.PortName(IPAddr::FromValue(ee_won_ipaddr_clone(anAddr)).GetPort());
    ee_won_ipaddr_free(anAddr);
    return out;
}

static unsigned sRand;
static int R(int n)
{
    sRand = sRand * 1103515245u + 12345u;
    return (int)((sRand >> 16) % (unsigned)n);
}
static int AnyOp() { return W_ACC + R(W_ROP2 - W_ACC + 1); }
static int AnySocketOp() { return W_ACC + R(W_DONE - W_ACC + 1); }
static int AnyObj() { return R(W_COUNT); }

// a random sequence over the ops that leave the worlds usable (the destructive ones end each run)
static std::vector<Step> Steps(int inCount)
{
    typedef EeWonLobbyOp LO;
    typedef EeWonEeXferOp XO;
    typedef EeWonRouteLogicOp RO;
    std::vector<Step> v;
    bool aTimerRun = false;
    while ((int)v.size() < inCount) {
        Step s = {0, 0, -1, -1, 0, 0, NULL};
        switch (R(23)) {
        case 0: { int o = AnyObj(); v.push_back({0, (int)LO::RefHold, o, -1, 0, 0, NULL}); s = {0, (int)LO::RefRelease, o, -1, 0, 0, NULL}; break; }
        case 1: { int o = AnyObj(); v.push_back({0, (int)LO::ClientHold, o, -1, 0, 0, NULL}); s = {0, (int)LO::ClientRelease, o, -1, 0, 0, NULL}; break; }
        case 2: s = {0, (int)LO::LanOpSocket, AnySocketOp(), -1, 0, 0, NULL}; break;
        case 3: s = {0, (int)LO::LanRecvOk, AnyOp(), -1, 0, 0, NULL}; break;
        case 4: s = {0, (int)(R(2) ? LO::LanRecvMsgTake : LO::LanDiscoveryTake), AnySocketOp(), -2, 0, 0, NULL}; break;
        case 5: s = {0, (int)LO::LanAcceptedSocket, R(2) ? W_ACC : AnySocketOp(), -1, 0, 0, NULL}; break;
        case 6: s = {0, (int)LO::LanOpOnSocket, AnySocketOp(), R(W_U2 + 1), 0, 0, NULL}; break;
        case 7: s = {0, (int)LO::LanRecvFromAddr, AnySocketOp(), -3, 0, 0, NULL}; break;
        case 8: s = R(2) ? Step{0, (int)LO::LanSocketSend, W_BUF, W_C, 0, 0, NULL} : Step{0, (int)LO::LanSocketSend, -1, W_C, 0, 12, kLanSend}; break;
        case 9: s = {0, (int)LO::LanSendTo, W_U1, -1, 0, 0, R(3) ? "to u2" : NULL}; break;
        case 10:
            if (aTimerRun) continue;
            aTimerRun = true;
            s = {0, (int)LO::LanRunTimer, W_TIMER, -1, 60000, 0, NULL};
            break;
        case 11: { int o = AnyObj(); v.push_back({1, (int)XO::AddRef, o, -1, 0, 0, NULL}); s = {1, (int)XO::Release, o, -1, 0, 0, NULL}; break; }
        case 12: s = R(2) ? Step{1, (int)XO::Succeeded, AnyOp(), -1, 0, 0, NULL} : Step{1, (int)XO::OpSocket, AnySocketOp(), -1, 0, 0, NULL}; break;
        case 13: s = {1, (int)XO::AcceptedSocket, AnySocketOp(), -1, 0, 0, NULL}; break;
        case 14: { int o = AnyObj(); v.push_back({2, (int)RO::AddRef, o, -1, 0, 0, NULL}); s = {2, (int)RO::Release, o, -1, 0, 0, NULL}; break; }
        case 15: s = {2, (int)(R(2) ? RO::OpStatus : RO::OpKilled), AnyOp(), -1, 0, 0, NULL}; break;
        case 16: s = {2, (int)RO::RNum, R(2) ? W_ROP : W_ROP2, R(6), 0, 0, NULL}; break;
        case 17: s = {2, (int)(R(2) ? RO::RRecipient : RO::RIsGameData), R(2) ? W_ROP : W_ROP2, -1, 0, 0, NULL}; break;
        case 18: s = {2, (int)(R(3) == 0 ? RO::AsyncOpNew : R(2) ? RO::Now : RO::Tick), -1, -1, 0, 0, NULL}; break;
        // R6 WON lobby and network round 40's: a routing op's number set, a socket's connected flag, a user status
        // op's map (none of these ops has one)
        case 19: s = {2, (int)RO::RSetNum, R(2) ? W_ROP : W_ROP2, R(6), R(3) == 0 ? -1 - R(100) : R(70000), 0, NULL}; break;
        case 20: s = {2, (int)RO::ConnIsConnected, R(W_S + 1), -1, 0, 0, NULL}; break;
        case 21: s = {2, (int)RO::StatusOpRust, AnyOp(), -1, 0, 0, NULL}; break;
        default: s = {2, (int)RO::OpSocket, AnySocketOp(), -1, 0, 0, NULL}; break;
        }
        v.push_back(s);
    }
    return v;
}

int main(int argc, char** argv)
{
    int aSeeds = 6, aCount = 300;
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--seeds") == 0) aSeeds = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--steps") == 0) aCount = atoi(argv[i + 1]);
    }
    AsyncSocket::StartWinsock();
    typedef EeWonLobbyOp LO;
    typedef EeWonEeXferOp XO;
    typedef EeWonRouteLogicOp RO;
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        for (int seed = 1; seed <= aSeeds; seed++) {
            sRand = (unsigned)seed * 7919u;
            World a, b;
            if (getenv("WON_NETOPS_TRACE") != NULL) fprintf(stderr, "make\n");
            a.Make(kHello);
            b.Make(kHello);
            Same("seed " + N(seed) + " made", a.Census(), b.Census());
            if (getenv("WON_NETOPS_TRACE") != NULL) fprintf(stderr, "made %s\n", a.Census().c_str());
            std::vector<Step> v = Steps(aCount);
            // the ends: an op killed, the timer finished and killed, a listen, the client closed through its
            // queue (and a NULL socket's close), the datagram sockets killed
            v.push_back({2, (int)RO::OpKill, W_TIMER, -1, 0, 0, NULL});
            v.push_back({2, (int)RO::OpKill, W_FAIL, -1, 0, 0, NULL});
            v.push_back({2, (int)RO::OpFinish, W_TIMER, -1, 0, 0, NULL});
            v.push_back({0, (int)LO::LanTimerKill, W_TIMER, -1, 0, 0, NULL});
            v.push_back({1, (int)XO::Listen, W_U1, -1, 0, 0, NULL});
            v.push_back({0, (int)LO::LanSocketClose, W_C, -1, 1000, 0, NULL});
            v.push_back({0, (int)LO::LanSocketClose, -1, -1, 0, 0, NULL});
            v.push_back({0, (int)LO::LanSocketKill, W_U1, -1, 0, 0, NULL});
            v.push_back({1, (int)XO::Kill, W_U2, -1, 0, 0, NULL});
            for (size_t i = 0; i < v.size(); i++) {
                const Step& s = v[i];
                std::string what = "seed " + N(seed) + " step " + N(i) + " table " + N(s.mTable) + " op "
                    + N(s.mOp) + " a " + N(s.mA) + " b " + N(s.mB);
                static const bool sTrace = getenv("WON_NETOPS_TRACE") != NULL;
                if (sTrace) fprintf(stderr, "%s\n", what.c_str());
                std::string ra = Run(a, s, false), rb = Run(b, s, true);
                Same(what, ra, rb);
                if (s.mTable == 0 && (s.mOp == (int)LO::LanSocketSend || s.mOp == (int)LO::LanSendTo)) {
                    bool dg = s.mOp == (int)LO::LanSendTo;
                    std::string ga = Received(a, dg), gb = Received(b, dg);
                    if (getenv("WON_NETOPS_TRACE") != NULL) fprintf(stderr, "  received %s\n", ga.c_str());
                    Same(what + " received", ga, gb);
                }
                if ((s.mTable == 0 && s.mOp == (int)LO::LanSocketClose) || (s.mTable == 2 && s.mOp == (int)RO::OpFinish))
                    Pump(50);
                Same(what + " census", a.Census(), b.Census());
            }
            for (size_t i = 0; i < a.mNew.size(); i++) a.mNew[i]->Release();
            for (size_t i = 0; i < b.mNew.size(); i++) b.mNew[i]->Release();
            Pump(20);
        }
        aCore.Pump(50);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    printf("won_netops_test: %d checks, %d mismatches (%d seeds of %d steps)\n", sChecks, sMismatches, aSeeds, aCount);
    return sMismatches;
}
