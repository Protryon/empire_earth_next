// The lobby's ping op over 127.0.0.1 (R6 WON lobby and network round 36): the original PingGameOp, a C++ subclass of
// UDPManagerSendRecvOp (PingLogic.cpp at 71c097464, copied below over a stand-in game), against the op as it is
// now, a UDPManagerSendRecvOp with a Rust body (logics.rs ee_won_pingop_body, udpops.rs EeWonUdpBody; the host's
// cases as PingLogic.cpp answers them). Both run every case over a real UDPManager and a game host stand-in (a
// datagram socket that logs each request and sends the scripted replies); the two traces must be equal line for
// line: the request's bytes, the replies refused (another kind, another id, a reply cut short, an empty datagram),
// the reply's flags and rest, the ping time, no reply, a blocking run, a kill, and the game's and the op's
// references at every step. A LAN game does not ping (probe, round 36), so this driver is the move's check.
// EE_W36_MUTATE=n changes the new side in one way (1..6); the exit code is the number of mismatching lines.
#include <windows.h>
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
#include "ee/wonapi/WONRouting.h"
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONAPICore.h"
#include "rust/won_lobby/logics.h"
#include "won_blocking.h"

using namespace WONAPI;

static int sMutate;
static bool Mutate(int n) { return sMutate == n; }
static std::vector<std::string> sLines;
static void Log(const std::string& inLine) { sLines.push_back(inLine); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static std::string Hex(const ByteBuffer* inBuffer)
{
    if (inBuffer == NULL)
        return "(null)";
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N(inBuffer->length()) + ":";
    for (unsigned long i = 0; i < inBuffer->length() && i < 64; i++) {
        unsigned char c = static_cast<unsigned char>(inBuffer->data()[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static ByteBufferPtr Bytes(const std::string& inData) { return new ByteBuffer(inData.data(), (unsigned long)inData.size(), false); }

struct Done : public CompletionBase<AsyncOpPtr> {
    int mCount = 0;
    virtual void Complete(AsyncOpPtr) { this->mCount++; }
};

static WONAPICoreEx* sCore;
static void PumpFor(Done* inDone, int inCount, unsigned inMs = 8000)
{
    uint32_t aStart = WONTickCount();
    while (inDone->mCount < inCount && WONTickCount() - aStart < inMs)
        sCore->Pump(10);
}
static void PumpMs(unsigned inMs)
{
    uint32_t aStart = WONTickCount();
    while (WONTickCount() - aStart < inMs)
        sCore->Pump(10);
}

// the stand-in game: what the op reads of a LobbyGame
struct Game : public RefCount {
    IPAddr mAddr;
    int mType = 0;
    IPAddr GetAddress() const { return this->mAddr; }
};
static Game* G(intptr_t a) { return reinterpret_cast<Game*>(a); }

static bool sNew; // which side runs
static intptr_t Host(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

// ---- the original (PingLogic.cpp at 71c097464)
class OrigPingGameOp : public UDPManagerSendRecvOp {
public:
    OrigPingGameOp(Game* inGame, unsigned short inRequestId, unsigned char inFlags)
        : mGame(inGame), mRust(ee_won_pingop_new(this, Host, inRequestId, inFlags))
    {
        this->SetDestAddr(this->mGame->GetAddress());
    }
    virtual ~OrigPingGameOp() { ee_won_pingop_free(this->mRust); }
    virtual void RunHook() { ee_won_pingop_call(this->mRust, EeWonPingOpCall::RunHook, (intptr_t)this->mGame.get()); }
    virtual WONStatus CheckResponse(ReadBuffer& inBuffer)
    {
        int64_t aStatus = ee_won_pingop_call(this->mRust, EeWonPingOpCall::CheckResponse, (intptr_t)&inBuffer.mState);
        if (aStatus == EE_WON_LOGIC_THREW)
            throw ReadBufferException("Attempt to read past end of buffer.");
        return (WONStatus)aStatus;
    }
    SmartPtr<Game> mGame; // 0x88
    EeWonPingOp* mRust; // the request id, the ping, the flags, the retries, the reply
};

// ---- the new side's helpers (PingLogic.cpp)
static EeWonPingOp* Rec(AsyncOp* inOp) { return static_cast<EeWonPingOp*>(ee_won_udp_body_of(static_cast<RefCount*>(inOp), EE_WON_UDP_BODY_PING)); }
static UDPManagerSendRecvOp* PingOp(void* inObj) { return static_cast<UDPManagerSendRecvOp*>(static_cast<RefCount*>(inObj)); }
static intptr_t Held(AsyncOp* inOp)
{
    if (!Mutate(4)) inOp->AddRef();
    return (intptr_t)inOp;
}

// the host: the ping op's cases as PingLogicHost answers them, the original's for the original
static intptr_t Host(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    switch (inOp) {
    case O::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::GameType: return (intptr_t)G(a)->mType;
    // (the new side's RequestSend and Elapsed are the Rust body's own since R6 WON round 40: mutations 1, 2 unreached)
    case O::RequestSend:
        if (sNew) {
            if (!Mutate(1)) ee_won_udpsendrecvop_set_outgoing_bytes(PingOp(inObj)->GetUdpObject(), reinterpret_cast<const uint8_t*>(a), (size_t)b);
            return 0;
        } else {
            OrigPingGameOp* anOp = static_cast<OrigPingGameOp*>(inObj);
            WriteBuffer aBuffer(0, true);
            aBuffer.AppendBytes(reinterpret_cast<const void*>(a), (unsigned long)b);
            anOp->SetOutgoing(aBuffer.ToByteBuffer(true));
            anOp->UDPManagerSendRecvOp::RunHook();
            return 0;
        }
    case O::Elapsed:
        if (sNew) return Mutate(2) ? 0 : (intptr_t)(unsigned long)(WONTickCount() - PingOp(inObj)->GetStartTime());
        else {
            unsigned long aStart = static_cast<OrigPingGameOp*>(inObj)->GetStartTime();
            return (intptr_t)(unsigned long)(WONTickCount() - aStart);
        }
    case O::OpGame:
        if (sNew) return (intptr_t)ee_won_pingop_call(Rec(reinterpret_cast<AsyncOp*>(a)), EeWonPingOpCall::Field, 5);
        return (intptr_t)static_cast<OrigPingGameOp*>(reinterpret_cast<AsyncOp*>(a))->mGame.get();
    case O::OpRecord:
        if (sNew) return (intptr_t)Rec(reinterpret_cast<AsyncOp*>(a));
        return (intptr_t)static_cast<OrigPingGameOp*>(reinterpret_cast<AsyncOp*>(a))->mRust;
    case O::NewPingOp:
        if (sNew)
            return Held(new UDPManagerSendRecvOp(ee_won_pingop_body(a, Host, (unsigned short)b, (unsigned char)(Mutate(3) ? c + 1 : c)),
                                                 Mutate(5) ? IPAddr() : G(a)->GetAddress(), Mutate(6) ? NULL : reinterpret_cast<UDPManager*>(d)));
        else {
            OrigPingGameOp* anOp = new OrigPingGameOp(G(a), (unsigned short)b, (unsigned char)c);
            anOp->AddRef();
            anOp->SetUDPManager(reinterpret_cast<UDPManager*>(d));
            return (intptr_t)static_cast<AsyncOp*>(anOp);
        }
    default: return 0;
    }
}

static SmartPtr<UDPManager> sManager;
static SmartPtr<BlockingSocket> sServer;
static IPAddr sServerAddr;

static std::string Fields(AsyncOp* inOp)
{
    EeWonPingOp* r = reinterpret_cast<EeWonPingOp*>(Host(NULL, EeWonLogicOp::OpRecord, (intptr_t)inOp, 0, 0, 0));
    long long aPing = ee_won_pingop_call(r, EeWonPingOpCall::Field, 1);
    return "id " + N(ee_won_pingop_call(r, EeWonPingOpCall::Field, 0)) + " ping " +
           (aPing == 0xffff ? "none" : aPing == 0 ? "zero" : aPing >= 60 && aPing < 3000 ? "timed" : "other") + " flags " +
           N(ee_won_pingop_call(r, EeWonPingOpCall::Field, 2)) + " retries " + N(ee_won_pingop_call(r, EeWonPingOpCall::Field, 3)) +
           " reply " + N(ee_won_pingop_call(r, EeWonPingOpCall::Field, 4));
}

enum Mode { ASYNC, BLOCK, KILL, NO_REPLY };
// one ping: the op made as the logic makes it, run; the server answers its request with the scripted replies
static void Ping(const char* inName, unsigned short inId, unsigned char inFlags, int inType, const std::vector<std::string>& inReplies,
                 Mode inMode = ASYNC)
{
    Log(std::string("== ") + inName);
    Game* aGame = new Game;
    aGame->AddRef();
    aGame->mAddr = sServerAddr;
    aGame->mType = inType;
    AsyncOp* aRaw = reinterpret_cast<AsyncOp*>(Host(NULL, EeWonLogicOp::NewPingOp, (intptr_t)aGame, inId, inFlags, (intptr_t)sManager.get()));
    UDPManagerSendRecvOp* anOp = static_cast<UDPManagerSendRecvOp*>(aRaw);
    Log("made game refs " + N(aGame->GetRefCount()) + " op refs " + N(anOp->GetRefCount()) + " " + Fields(anOp) + " game " +
        N(G(Host(NULL, EeWonLogicOp::OpGame, (intptr_t)aRaw, 0, 0, 0)) == aGame) + " dest " + anOp->GetDestAddr().GetHostAndPortString() + " manager " +
        N(ee_won_udpsendrecvop_manager(anOp->GetUdpObject()) == static_cast<RefCount*>(sManager.get())) + " retransmits " +
        N(anOp->GetNumRetransmits()) + " timeout " + N((long long)anOp->GetMsgTimeout()));
    if (Mutate(4)) anOp->AddRef(); // (the mutation's missing reference, so the run can go on)
    if (inMode == NO_REPLY)
        anOp->SetMsgTimeout(300);
    SmartPtr<Done> aDone = new Done;
    anOp->SetCompletion(aDone);
    if (inMode == BLOCK) {
        Log("block run " + N(anOp->RunBlock(1000)) + " status " + St(anOp->GetStatus()) + " outgoing " + Hex(anOp->GetOutgoing()) + " " + Fields(anOp));
    } else {
        anOp->RunAsync(10000);
        sCore->Pump(20); // RunAsync only queues: the pump starts the op, which sends
        ByteBufferPtr aGot;
        IPAddr aFrom;
        WONStatus aStatus = WonBlocking::RecvBytesFrom(sServer, aGot, 512, aFrom, 2000);
        Log("server got " + St(aStatus) + " " + Hex(aGot.get()) + " " + Fields(anOp));
        PumpMs(100); // the ping time: at least this long
        if (inMode == KILL) {
            anOp->Kill(true);
            sCore->Pump(20);
            Log("killed done " + N(aDone->mCount) + " status " + St(anOp->GetStatus()));
        }
        for (size_t r = 0; aStatus == WS_Success && r < inReplies.size(); r++) {
            WonBlocking::SendBytesTo(sServer, Bytes(inReplies[r]), aFrom, 2000);
            if (r + 1 < inReplies.size() || inMode != ASYNC) { // a reply the op refuses: still waiting after it was read
                PumpMs(200);
                Log("after reply " + N((long long)r) + " done " + N(aDone->mCount) + " status " + St(anOp->GetStatus()) + " " + Fields(anOp));
            }
        }
        PumpFor(aDone, 1, inMode == NO_REPLY ? 3000 : 8000);
        sCore->Pump(20);
        Log("done " + N(aDone->mCount) + " status " + St(anOp->GetStatus()) + " " + Fields(anOp) + " outgoing " + Hex(anOp->GetOutgoing()));
    }
    anOp->SetCompletion(NULL);
    sCore->Pump(20);
    Log("before release game refs " + N(aGame->GetRefCount()) + " op refs " + N(anOp->GetRefCount()));
    anOp->Release();
    sCore->Pump(20);
    Log("after release game refs " + N(aGame->GetRefCount()));
    aGame->Release();
}

static std::string R(std::initializer_list<int> inBytes)
{
    std::string s;
    for (int b : inBytes)
        s += (char)b;
    return s;
}

static std::vector<std::string> Side(bool inNew)
{
    sNew = inNew;
    sLines.clear();
    Ping("plain reply", 0x1234, 0, 2, {R({8, 0x34, 0x12})});
    Ping("flags and rest", 7, 3, 3, {R({8, 7, 0, 5, 1, 2, 3, 4})});
    Ping("flags alone", 8, 1, 3, {R({8, 8, 0, 2})});
    Ping("another kind, then right", 9, 0, 1, {R({9, 9, 0}), R({8, 9, 0})});
    Ping("another id, then right", 10, 2, 1, {R({8, 11, 0, 1}), R({8, 10, 0, 6, 9})});
    Ping("cut short, then right", 11, 0, 1, {R({8, 11}), R({8, 11, 0})});
    Ping("empty, then right", 12, 0, 1, {std::string(), R({8, 12, 0})});
    Ping("an id from 0x8000 never matches", 0x8001, 0, 1, {R({8, 0x01, 0x80})}, NO_REPLY);
    Ping("no reply", 13, 0, 1, {}, NO_REPLY);
    Ping("blocking", 14, 2, 3, {}, BLOCK);
    Ping("killed", 15, 0, 1, {R({8, 15, 0})}, KILL);
    return sLines;
}

int main(int argc, char** argv)
{
    sMutate = getenv("EE_W36_MUTATE") ? atoi(getenv("EE_W36_MUTATE")) : 0;
    std::vector<std::string> aOrig, aNew;
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        sManager = new UDPManager;
        sManager->Bind(0, false);
        sServer = new BlockingSocket(1);
        sServer->Bind(IPAddr("127.0.0.1", 0), false);
        sServerAddr = IPAddr("127.0.0.1", sServer->GetLocalPort());
        aOrig = Side(false);
        aNew = Side(true);
        sServer = NULL;
        sManager->Close();
        sManager = NULL;
        aCore.Pump(50);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    size_t aCount = aOrig.size() > aNew.size() ? aOrig.size() : aNew.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < aNew.size() ? aNew[i].c_str() : "(none)";
        const char* aWant = i < aOrig.size() ? aOrig[i].c_str() : "(none)";
        if (aPrint)
            printf("%s\n", aWant);
        if (strcmp(aGot, aWant) != 0 && aFailures++ < 20)
            printf("MISMATCH line %d\n  new  %s\n  orig %s\n", (int)i + 1, aGot, aWant);
    }
    if (sMutate)
        printf("won_ping36_test (mutation %d): %d lines, %d mismatches\n", sMutate, (int)aOrig.size(), aFailures);
    else
        printf("won_ping36_test: %d lines, %d mismatches\n", (int)aOrig.size(), aFailures);
    return sMutate ? 0 : aFailures;
}
