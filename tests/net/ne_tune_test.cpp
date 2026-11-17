// The server's lock-step tuning and booting in Rust (LLE Network Engine round 3, ee_net tune.rs): the original members,
// unmodified (orig_s/NESynchronousServerTune.inc: TuneServer, TuneForSlowNetwork, TuneForSlowCPU, BootAllLaggers,
// BootTheSlowestResponder, BootTheSlowestClientCPU, GetSlowestClientCPU), compiled into a stand-in server whose
// message manager, events, messages and game callbacks are world lines with answers from the world's random stream,
// against ee_net's EeNetTune whose host answers with the same world (as NESynchronousServer.cpp's TuneHost does with
// the game's). Each sequence changes the server's fields (send intervals near their limits, the 0xffffffff of a
// server that has not sent, rates, the cost factor, the game state) and calls the members; both sides print every
// effect and their state (the tuning's own fields included) after each call; the traces must be the same.
//
// The Rust side's host also records each op with its answer: the first sequences are
// rust/crates/ee_net/src/net/golden_tune.txt, which cargo replays against the Rust alone (`--print FILE` writes it).
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include "rust/net/net.h"
#include "ne_tcp_host.h"

// ---- the world

struct TWorld {
    std::string mTrace;
    std::string mOps;
    bool mRecord = false;
    uint32_t mRandom = 1;
    unsigned long mNumClients = 3;
    unsigned long mWhoLeft = 0;
    bool mLagging = false;        // most ticks late   // the laggers WhoHasNotRespondedNext may still name (at most the clients)
    int mNextMessage = 1;
    void Line(const char* inFormat, ...)
    {
        char theLine[512];
        va_list theArgs;
        va_start(theArgs, inFormat);
        vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
        va_end(theArgs);
        this->mTrace += std::string(theLine) + "\n";
    }
    void Op(const std::string& inLine)
    {
        if (this->mRecord)
            this->mOps += inLine + "\n";
    }
    uint32_t Next()
    {
        this->mRandom = this->mRandom * 1103515245u + 12345u;
        return this->mRandom >> 8;
    }
};
static TWorld* gW;

enum { kSlotServer = 0xfe, kSlotNone = 0xff };
enum NEServerGameState { kServerStateOpen = 0, kServerStateClosed = 1, kServerStateRunning = 3, kServerStatePaused = 4 };

class TSEvent {
public:
    explicit TSEvent(const char* inName) : mName(inName) {}
    long Set()
    {
        gW->Line("event set %s", this->mName);
        return 0;
    }
    const char* mName;
};

class NEMessage {
public:
    virtual ~NEMessage() {}
    int mID;
};
class NEMRemovePlayer : public NEMessage {
public:
    NEMRemovePlayer(unsigned char inSlot, bool inA, bool inB, bool inC)
    {
        this->mID = gW->mNextMessage++;
        gW->Line("msg removeplayer m%d %d %d %d %d", this->mID, inSlot, (int)inA, (int)inB, (int)inC);
    }
};

// the message manager's answers
static uint8_t WMmNumClients(const EeNetMsgMgr*)
{
    gW->Line("mm numclients = %lu", gW->mNumClients);
    gW->mWhoLeft = gW->mNumClients;
    return (uint8_t)gW->mNumClients;
}
static uint8_t WMmWho(const EeNetMsgMgr*, uint32_t inID)
{
    uint8_t s = gW->Next() % 3 == 0 || gW->mWhoLeft == 0 ? kSlotNone : (uint8_t)(gW->Next() % 8);
    if (s != kSlotNone)
        gW->mWhoLeft--;
    gW->Line("mm who %u = %d", inID, s);
    return s;
}
static uint8_t WMmWhoNext(const EeNetMsgMgr*, uint8_t inSlot, uint32_t inID)
{
    uint8_t s = gW->Next() % 2 == 0 || gW->mWhoLeft == 0 ? kSlotNone : (uint8_t)(gW->Next() % 8);
    if (s != kSlotNone)
        gW->mWhoLeft--;
    gW->Line("mm whonext %d %u = %d", inSlot, inID, s);
    return s;
}
static uint8_t WMmOutstanding(const EeNetMsgMgr*, uint32_t inID)
{
    // (a lagging phase: nearly every tick late, so the resends run out and laggers are booted)
    uint8_t n = (gW->mLagging ? gW->Next() % 10 != 0 : gW->Next() % 3 == 0) ? (uint8_t)(1 + gW->Next() % 2) : 0;
    gW->Line("mm outstanding %u = %d", inID, n);
    return n;
}
static uint32_t WMmHighestResponseTime(const EeNetMsgMgr*)
{
    static const uint32_t kTimes[] = {0, 100, 349, 350, 351, 500, 999, 1000, 2000, 4000};
    uint32_t t = gW->Next() % 2 == 0 ? kTimes[gW->Next() % 10] : gW->Next() % 3000;
    gW->Line("mm highestresponse = %u", t);
    return t;
}
static uint16_t WMmHighestCpu(const EeNetMsgMgr*)
{
    uint16_t c = gW->Next() % 6 == 0 ? 0 : (uint16_t)(gW->Next() % 700);
    gW->Line("mm highestcpu = %d", c);
    return c;
}
static uint32_t WMmLowestCt(const EeNetMsgMgr*)
{
    uint32_t c = gW->Next() % 100;
    gW->Line("mm lowestct = %u", c);
    return c;
}
static void WMmResend(const EeNetMsgMgr*, uint32_t inID) { gW->Line("mm resend %u", inID); }
static uint8_t WMmSlowestResponder(const EeNetMsgMgr*)
{
    uint8_t s = gW->Next() % 3 == 0 ? kSlotNone : (uint8_t)(gW->Next() % 8);
    gW->Line("mm slowestresponder = %d", s);
    return s;
}
static uint8_t WMmSlowestClientCpu(const EeNetMsgMgr*)
{
    static const uint8_t kSlots[] = {kSlotNone, kSlotServer, 0, 1, 2, 5};
    uint8_t s = kSlots[gW->Next() % 6];
    gW->Line("mm slowestcpu = %d", s);
    return s;
}
static void WMmSetForceCt(const EeNetMsgMgr*, bool inOn) { gW->Line("mm forcect %d", (int)inOn); }

#define ee_net_mm_num_clients WMmNumClients
#define ee_net_mm_who_has_not_responded WMmWho
#define ee_net_mm_who_has_not_responded_next WMmWhoNext
#define ee_net_mm_outstanding WMmOutstanding
#define ee_net_mm_highest_response_time WMmHighestResponseTime
#define ee_net_mm_highest_cpu WMmHighestCpu
#define ee_net_mm_lowest_ct WMmLowestCt
#define ee_net_mm_resend WMmResend
#define ee_net_mm_slowest_responder WMmSlowestResponder
#define ee_net_mm_slowest_client_cpu WMmSlowestClientCpu
#define ee_net_mm_set_force_ct WMmSetForceCt

// ---- the stand-in server: the fields the members use, the game's callbacks as world lines

class NESynchronousServer {
public:
    NESynchronousServer()
        : mChangeSendIntervalEvent(new TSEvent("changeinterval")), mPauseGameEvent(new TSEvent("pause")),
          mUnpauseGameEvent(new TSEvent("unpause"))
    {
    }
    ~NESynchronousServer()
    {
        delete this->mChangeSendIntervalEvent;
        delete this->mPauseGameEvent;
        delete this->mUnpauseGameEvent;
    }
    unsigned long GetCurrentCommunicationsTick() const
    {
        gW->Line("getct = %lu", this->mCurrentCommunicationsTick);
        return this->mCurrentCommunicationsTick;
    }
    void BeforeRemovingPlayers() { gW->Line("v beforeremoving"); }
    void BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting)
    {
        gW->Line("v bootplayer %d %d %d", inSlot, (int)inWasBooted, (int)inGameIsExiting);
    }
    void ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag)
    {
        std::string theList;
        for (unsigned char s : inSlots)
            theList += " " + std::to_string(s);
        gW->Line("v reportlaggers %d%s", (int)inFlag, theList.c_str());
    }
    long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        gW->Line("v processlocal m%d %d", inMessage->mID, inPlayer);
        delete inMessage;
        return 0;
    }
    void UnregisterSlot(unsigned char inSlot)
    {
        if (inSlot != kSlotServer && inSlot != kSlotNone)
            gW->Line("mm unregister %d", inSlot);
    }

    void BootAllLaggers(unsigned long inTime);
    void BootTheSlowestClientCPU();
    void BootTheSlowestResponder();
    unsigned char GetSlowestClientCPU();
    bool TuneForSlowCPU();
    bool TuneForSlowNetwork();
    bool TuneServer();

    EeNetMsgMgr* mMessageManager = NULL;
    TSEvent* mChangeSendIntervalEvent;
    TSEvent* mPauseGameEvent;
    TSEvent* mUnpauseGameEvent;
    std::vector<unsigned char> mSlotsD0;
    float mPhysicsTickCostFactor = 1.0f;
    unsigned char mGameState = 0;
    unsigned long mCurrentCommunicationsTick = 0;
    unsigned long mSendInterval = 0xffffffff;
    unsigned long mDefaultSendInterval = 350;
    unsigned long mLastCTMessageID = 0xffffffff;
    unsigned long mMaxPhysicsTicksPerSecond = 40;
    unsigned long mMinPhysicsTicksPerSecond = 10;
    unsigned long mSlowCPUCount = 0;
    unsigned long mMaxSendInterval = 3000;
    unsigned long mSendIntervalIncrement = 100;
    unsigned long mSendIntervalDecrement = 50;
    unsigned long mNetTuneState = 0;
    unsigned long mNetConsecutiveLate = 0;
    unsigned long mNetResendCount = 0;
    unsigned long mNetLateEpisodes = 0;
    unsigned long mNetAtMaxCount = 0;
    unsigned long mHighestResponseTime = 0;
    unsigned long mNetLateCount = 0;
    unsigned char mPTsPerCT = 14;
    unsigned char mPTsPerSecond = 40;
    bool mLagReportActive = false;
    bool mLagWarning = false;
    bool mIsMultiplayerSession = true;
};

#include "orig_s/NESynchronousServerTune.inc"

// ---- the Rust side: its host over a second stand-in server (the fields and the game's callbacks)

static NESynchronousServer* sRustServer;
static EeNetTune* sRust;

static std::string Hex(const uint8_t* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}

static ptrdiff_t RustTuneHost(void*, EeNetTuOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* s = sRustServer;
    std::string theArgs = std::to_string((int)inOp);
    ptrdiff_t r = 0;
    switch (inOp) {
    case EeNetTuOp::Get:
        theArgs += " " + std::to_string(a);
        switch ((EeNetTuField)a) {
        case EeNetTuField::SendInterval: r = (uint32_t)s->mSendInterval; break;
        case EeNetTuField::SendIntervalIncrement: r = (uint32_t)s->mSendIntervalIncrement; break;
        case EeNetTuField::MaxSendInterval: r = (uint32_t)s->mMaxSendInterval; break;
        case EeNetTuField::PTsPerSecond: r = s->mPTsPerSecond; break;
        case EeNetTuField::MaxPTsPerSecond: r = (uint32_t)s->mMaxPhysicsTicksPerSecond; break;
        case EeNetTuField::MinPTsPerSecond: r = (uint32_t)s->mMinPhysicsTicksPerSecond; break;
        case EeNetTuField::CostFactor: {
            uint32_t theBits;
            memcpy(&theBits, &s->mPhysicsTickCostFactor, 4);
            r = theBits;
            break;
        }
        case EeNetTuField::DefaultSendInterval: r = (uint32_t)s->mDefaultSendInterval; break;
        case EeNetTuField::LastCtMessageId: r = (uint32_t)s->mLastCTMessageID; break;
        case EeNetTuField::GameState: r = s->mGameState; break;
        case EeNetTuField::IsMultiplayerSession: r = s->mIsMultiplayerSession; break;
        case EeNetTuField::CurrentCommunicationsTick: r = (uint32_t)s->GetCurrentCommunicationsTick(); break;
        case EeNetTuField::PTsPerCt: r = s->mPTsPerCT; break;
        }
        break;
    case EeNetTuOp::Set:
        theArgs += " " + std::to_string(a) + " " + std::to_string(b);
        if ((EeNetTuField)a == EeNetTuField::SendInterval)
            s->mSendInterval = (uint32_t)b;
        else if ((EeNetTuField)a == EeNetTuField::PTsPerSecond)
            s->mPTsPerSecond = (unsigned char)b;
        else if ((EeNetTuField)a == EeNetTuField::PTsPerCt)
            s->mPTsPerCT = (unsigned char)b;
        break;
    case EeNetTuOp::EventSet:
        theArgs += " " + std::to_string(a);
        (a == 0 ? s->mChangeSendIntervalEvent : a == 1 ? s->mPauseGameEvent : s->mUnpauseGameEvent)->Set();
        break;
    case EeNetTuOp::MmOutstanding:
        theArgs += " " + std::to_string(a);
        r = WMmOutstanding(NULL, (uint32_t)a);
        break;
    case EeNetTuOp::MmHighestResponseTime:
        r = WMmHighestResponseTime(NULL);
        break;
    case EeNetTuOp::MmHighestCpu:
        r = WMmHighestCpu(NULL);
        break;
    case EeNetTuOp::MmLowestCt:
        r = WMmLowestCt(NULL);
        break;
    case EeNetTuOp::MmResend:
        theArgs += " " + std::to_string(a);
        WMmResend(NULL, (uint32_t)a);
        break;
    case EeNetTuOp::MmWho:
        theArgs += " " + std::to_string(a);
        r = WMmWho(NULL, (uint32_t)a);
        break;
    case EeNetTuOp::MmWhoNext:
        theArgs += " " + std::to_string(a) + " " + std::to_string(b);
        r = WMmWhoNext(NULL, (uint8_t)a, (uint32_t)b);
        break;
    case EeNetTuOp::MmNumClients:
        r = WMmNumClients(NULL);
        break;
    case EeNetTuOp::MmSlowestResponder:
        r = WMmSlowestResponder(NULL);
        break;
    case EeNetTuOp::MmSlowestClientCpu:
        r = WMmSlowestClientCpu(NULL);
        break;
    case EeNetTuOp::MmSetForceCt:
        theArgs += " " + std::to_string(a);
        WMmSetForceCt(NULL, a != 0);
        break;
    case EeNetTuOp::UnregisterSlot:
        theArgs += " " + std::to_string(a);
        s->UnregisterSlot((unsigned char)a);
        break;
    case EeNetTuOp::BeforeRemovingPlayers:
        s->BeforeRemovingPlayers();
        break;
    case EeNetTuOp::BootPlayer:
        theArgs += " " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c);
        s->BootPlayer((unsigned char)a, b != 0, c != 0);
        break;
    case EeNetTuOp::ReportLaggers: {
        theArgs += " " + Hex((const uint8_t*)a, b) + " " + std::to_string(c);
        std::vector<unsigned char> theSlots((const uint8_t*)a, (const uint8_t*)a + b);
        s->ReportLaggers(theSlots, c != 0);
        break;
    }
    case EeNetTuOp::RemovePlayerInput: {
        theArgs += " " + std::to_string(a);
        NEMessage* theMessage = new NEMRemovePlayer((unsigned char)a, true, false, false);
        if (theMessage != NULL)
            s->ProcessLocalUserInput(theMessage, kSlotNone);
        break;
    }
    }
    gW->Op("op " + theArgs);
    gW->Op("= " + std::to_string((long long)r));
    return r;
}

// ---- one sequence on one side

struct Rng {
    uint32_t mState;
    uint32_t Next()
    {
        this->mState = this->mState * 1664525u + 1013904223u;
        return this->mState >> 8;
    }
    uint32_t Below(uint32_t inN) { return this->Next() % inN; }
};

static void State(bool inRust, NESynchronousServer* s)
{
    unsigned long t[12];
    if (inRust) {
        uint32_t theState[12];
        ee_net_tu_state(sRust, theState);
        for (int i = 0; i < 12; i++)
            t[i] = theState[i];
    } else {
        t[0] = s->mNetTuneState;
        t[1] = s->mNetConsecutiveLate;
        t[2] = s->mNetResendCount;
        t[3] = s->mNetLateEpisodes;
        t[4] = s->mNetAtMaxCount;
        t[5] = s->mHighestResponseTime;
        t[6] = s->mNetLateCount;
        t[7] = s->mSlowCPUCount;
        t[8] = s->mSendIntervalDecrement;
        t[9] = s->mLagWarning;
        t[10] = s->mLagReportActive;
        t[11] = (unsigned long)s->mSlotsD0.size();
    }
    gW->Line("state interval=%lu pts/s=%d pts/ct=%d tune=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
             s->mSendInterval, s->mPTsPerSecond, s->mPTsPerCT, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8],
             t[9], t[10], t[11]);
    if (inRust)
        gW->Op("state " + std::to_string(t[0]) + " " + std::to_string(t[8]) + " " + std::to_string(t[11]));
}

static std::string RunSide(bool inRust, uint32_t inSeed, int inSteps, bool inRecord, std::string& outOps)
{
    TWorld theWorld;
    theWorld.mRandom = inSeed * 2654435761u + 7;
    theWorld.mRecord = inRecord;
    gW = &theWorld;
    Rng r = {inSeed};
    NESynchronousServer* s = new NESynchronousServer;
    static const unsigned long kIntervals[] = {100, 340, 350, 360, 400, 450, 499, 500, 600, 999, 1000, 1500, 3000, 0xffffffff};
    static const float kFactors[] = {0.5f, 1.0f, 1.5f, 2.75f, 4.0f, 0.1f};
    s->mSendIntervalDecrement = 10 + r.Below(100);
    if (inRust) {
        sRustServer = s;
        sRust = ee_net_tu_new(NULL, RustTuneHost, (uint32_t)s->mSendIntervalDecrement);
        gW->Op("new " + std::to_string(s->mSendIntervalDecrement));
    }
    for (int i = 0; i < inSteps; i++) {
        uint32_t w = r.Below(100);
        if (w < 30) {
            uint32_t f = r.Below(11);
            unsigned long v = 0;
            switch (f) {
            case 0: v = s->mSendInterval = kIntervals[r.Below(14)]; break;
            case 1: v = s->mSendIntervalIncrement = r.Below(4) == 0 ? 0 : 10 + r.Below(300); break;
            case 2: v = s->mMaxSendInterval = kIntervals[r.Below(14)]; break;
            case 3: v = s->mPTsPerSecond = (unsigned char)r.Below(256); break;
            case 4: v = s->mMaxPhysicsTicksPerSecond = r.Below(80); break;
            case 5: v = s->mMinPhysicsTicksPerSecond = r.Below(40); break;
            case 6: s->mPhysicsTickCostFactor = kFactors[r.Below(6)]; v = (unsigned long)(s->mPhysicsTickCostFactor * 100); break;
            case 7: v = s->mDefaultSendInterval = kIntervals[r.Below(14)]; break;
            case 8: v = s->mLastCTMessageID = r.Below(200); break;
            case 9: {
                static const unsigned char kStates[] = {0, 1, 2, 3, 3, 4};
                v = s->mGameState = kStates[r.Below(6)];
                break;
            }
            default: v = s->mIsMultiplayerSession = r.Below(4) != 0; break;
            }
            gW->Line("set %u %lu", f, v);
            gW->mNumClients = 1 + r.Below(6);
            gW->mLagging = r.Below(3) == 0;
            s->mCurrentCommunicationsTick += r.Below(3);
            continue;
        }
        long theResult = 0;
        std::string theCall;
        uint32_t k = w < 75 ? 0 : w < 82 ? 1 : w < 88 ? 2 : w < 92 ? 3 : w < 95 ? 4 : w < 98 ? 5 : 6;
        unsigned long theID = r.Below(200);
        theCall = "call " + std::to_string(k) + (k == 3 ? " " + std::to_string(theID) : std::string());
        gW->Line("%s", theCall.c_str());
        gW->Op(theCall);
        gW->mWhoLeft = gW->mNumClients;   // (BootAllLaggers' array holds the clients)
        if (inRust) {
            switch (k) {
            case 0: theResult = ee_net_tu_tune_server(sRust); break;
            case 1: theResult = ee_net_tu_tune_for_slow_network(sRust); break;
            case 2: theResult = ee_net_tu_tune_for_slow_cpu(sRust); break;
            case 3: ee_net_tu_boot_all_laggers(sRust, (uint32_t)theID); break;
            case 4: ee_net_tu_boot_the_slowest_responder(sRust); break;
            case 5: ee_net_tu_boot_the_slowest_client_cpu(sRust); break;
            default: theResult = ee_net_tu_slowest_client_cpu(sRust); break;
            }
        } else {
            switch (k) {
            case 0: theResult = s->TuneServer(); break;
            case 1: theResult = s->TuneForSlowNetwork(); break;
            case 2: theResult = s->TuneForSlowCPU(); break;
            case 3: s->BootAllLaggers(theID); break;
            case 4: s->BootTheSlowestResponder(); break;
            case 5: s->BootTheSlowestClientCPU(); break;
            default: theResult = s->GetSlowestClientCPU(); break;
            }
        }
        gW->Line("= %ld", theResult);
        gW->Op("ret " + std::to_string(theResult));
        State(inRust, s);
    }
    if (inRust)
        ee_net_tu_free(sRust);
    delete s;
    outOps = theWorld.mOps;
    gW = NULL;
    return theWorld.mTrace;
}

int main(int argc, char** argv)
{
    int theCount = 20000, theSteps = 120, theRecorded = 40;
    const char* thePrint = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            theCount = atoi(argv[++i]);
        else if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
    }
    int theDiffering = 0;
    size_t theLines = 0;
    std::map<std::string, int> theKinds;
    std::string theGolden;
    for (int s = 0; s < theCount; s++) {
        std::string theOps;
        std::string theOrig = RunSide(false, (uint32_t)s + 1, theSteps, false, theOps);
        bool theRecord = thePrint != NULL && s < theRecorded;
        std::string theRust = RunSide(true, (uint32_t)s + 1, theSteps, theRecord, theOps);
        if (theRecord)
            theGolden += "seq " + std::to_string(s + 1) + "\n" + theOps;
        for (size_t i = 0, theEnd; i < theOrig.size(); i = theEnd + 1) {
            theEnd = theOrig.find('\n', i);
            std::string theLine = theOrig.substr(i, theEnd - i);
            size_t theSecond = theLine.find(' ', theLine.find(' ') + 1);
            theKinds[theLine.substr(0, theSecond)]++;
            theLines++;
        }
        if (theOrig != theRust && theDiffering++ < 3) {
            size_t i = 0;
            while (i < theOrig.size() && i < theRust.size() && theOrig[i] == theRust[i])
                i++;
            size_t theStart = theOrig.rfind('\n', i == 0 ? 0 : i - 1);
            theStart = theStart == std::string::npos ? 0 : theStart + 1;
            printf("sequence %d differs:\n  orig: %.300s\n  rust: %.300s\n", s + 1, theOrig.c_str() + theStart,
                   theRust.c_str() + theStart);
        }
    }
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(theGolden.data(), 1, theGolden.size(), theFile);
        fclose(theFile);
    }
    for (auto& k : theKinds)
        if (k.first.compare(0, 6, "state ") != 0 && k.first.compare(0, 4, "set ") != 0 && k.first.compare(0, 2, "= ") != 0)
            printf("  %-28s %d\n", k.first.c_str(), k.second);
    printf("ne_tune_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
