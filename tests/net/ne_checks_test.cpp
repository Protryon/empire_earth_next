// The server's world checksums, out-of-sync slots and the removal of players from a running game in Rust (LLE Network
// Engine round 4, ee_net server.rs): the original members, unmodified (orig_s/NESynchronousServerChecks.inc:
// LogChecksumResponse with its sort order, RemovePlayerFromGameInProgress; orig_s/NESynchronousServerOOS.inc: the
// out-of-sync block of SendCommunicationsTick), compiled into a stand-in server whose game callbacks (IsPlayerPresent,
// GetPlayerSendToSlot, BeforeRemovingPlayers, ProcessLocalUserInput) and message manager (UnregisterSlot) are world
// lines with answers from the world's random stream, against ee_net's EeNetServer whose host answers with the same
// world (as NESynchronousServer.cpp's ServerHost does with the game's). Each sequence changes the human count and the
// player count, logs checksums of a few ticks from the server's and the clients' slots (mostly agreeing, sometimes
// not), runs the out-of-sync block and removes players; both sides print every effect and their state after each
// step; the traces must be the same.
//
// The Rust side's host also records each op with its answer: the first sequences are
// rust/crates/ee_net/src/golden_checks.txt, which cargo replays against the Rust alone (`--print FILE` writes it).
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "rust/ee_net.h"

// ---- the world

struct KWorld {
    std::string mTrace;
    std::string mOps;
    bool mRecord = false;
    uint32_t mRandom = 1;
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
static KWorld* gW;

enum { kSlotServer = 0xfe, kSlotNone = 0xff };

class TSCriticalSection {   // (one thread: nothing to do)
public:
    void Enter() {}
    void Leave() {}
};

class NEMessage {
public:
    virtual ~NEMessage() {}
    int mID;
};
class NEMRemovePlayer : public NEMessage {
public:
    NEMRemovePlayer(unsigned char inPlayer, bool inA, bool inB, bool inC)
    {
        this->mID = gW->mNextMessage++;
        gW->Line("msg removeplayer m%d %d %d %d %d", this->mID, inPlayer, (int)inA, (int)inB, (int)inC);
    }
};
class NEMWorldDatabaseChecksumResponse {
public:
    unsigned long GetCommunicationsTick() const { return this->mTick; }
    unsigned long GetWorldDatabaseChecksum() const { return this->mChecksum; }
    unsigned char GetOriginatingSlot() const { return this->mSlot; }
    unsigned long mTick, mChecksum;
    unsigned char mSlot;
};

// ---- the stand-in server: the fields the members use, the game's callbacks as world lines

class NESynchronousServer {
public:
    NESynchronousServer() : mChecksumResponsesLock(new TSCriticalSection) {}
    ~NESynchronousServer() { delete this->mChecksumResponsesLock; }
    bool IsPlayerPresent(unsigned char inPlayer)
    {
        bool b = gW->Next() % 4 != 0;
        gW->Line("v present %d = %d", inPlayer, (int)b);
        return b;
    }
    unsigned char GetPlayerSendToSlot(unsigned char inPlayer)
    {
        static const unsigned char kSlots[] = {0, 1, 2, 3, 4, 5, kSlotServer, kSlotNone};
        unsigned char s = gW->Next() % 3 == 0 ? inPlayer : kSlots[gW->Next() % 8];
        gW->Line("v sendtoslot %d = %d", inPlayer, s);
        return s;
    }
    void BeforeRemovingPlayers() { gW->Line("v beforeremoving"); }
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

    void LogChecksumResponse(NEMWorldDatabaseChecksumResponse* inMessage);
    void RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting);
    void OOSBlock();

    std::map<unsigned long, std::multimap<unsigned long, unsigned char> > mChecksumResponses;
    std::vector<unsigned char> mOOSSlots;
    TSCriticalSection* mChecksumResponsesLock;
    unsigned char mNumHumanPlayers = 2;
    unsigned char mMaxPlayers = 4;
    bool mIsOOS = false;
};

#include "orig_s/NESynchronousServerChecks.inc"

void NESynchronousServer::OOSBlock()
{
#include "orig_s/NESynchronousServerOOS.inc"
}

// ---- the Rust side: its host over a second stand-in server (the fields and the game's callbacks)

static NESynchronousServer* sRustServer;
static EeNetServer* sRust;

static ptrdiff_t RustServerHost(void*, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* s = sRustServer;
    std::string theArgs = std::to_string((int)inOp);
    ptrdiff_t r = 0;
    switch (inOp) {
    case EeNetSvOp::Get:
        theArgs += " " + std::to_string(a);
        r = (EeNetSvField)a == EeNetSvField::NumHumanPlayers ? s->mNumHumanPlayers : s->mMaxPlayers;
        break;
    case EeNetSvOp::Set:
        theArgs += " " + std::to_string(a) + " " + std::to_string(b);
        if ((EeNetSvField)a == EeNetSvField::NumHumanPlayers)
            s->mNumHumanPlayers = (unsigned char)b;
        break;
    case EeNetSvOp::IsPlayerPresent:
        theArgs += " " + std::to_string(a);
        r = s->IsPlayerPresent((unsigned char)a);
        break;
    case EeNetSvOp::PlayerSendToSlot:
        theArgs += " " + std::to_string(a);
        r = s->GetPlayerSendToSlot((unsigned char)a);
        break;
    case EeNetSvOp::UnregisterSlot:
        theArgs += " " + std::to_string(a);
        s->UnregisterSlot((unsigned char)a);
        break;
    case EeNetSvOp::BeforeRemovingPlayers:
        s->BeforeRemovingPlayers();
        break;
    case EeNetSvOp::RemovePlayerInput: {
        theArgs += " " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c) + " " + std::to_string(d);
        NEMessage* theMessage = new NEMRemovePlayer((unsigned char)a, b != 0, c != 0, d != 0);
        s->ProcessLocalUserInput(theMessage, kSlotNone);
        break;
    }
    default:
        theArgs += " ?";
        break;
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

// the checksums' state as the Rust's checks_state prints it
static std::string OrigChecks(NESynchronousServer* s)
{
    std::string t;
    for (auto& k : s->mChecksumResponses) {
        t += std::to_string(k.first) + ":";
        bool theFirst = true;
        for (auto& e : k.second) {
            t += (theFirst ? "" : ",") + std::to_string(e.first) + "/" + std::to_string(e.second);
            theFirst = false;
        }
        t += ";";
    }
    t += " oos";
    for (unsigned char o : s->mOOSSlots)
        t += " " + std::to_string(o);
    return t;
}

static void State(bool inRust, NESynchronousServer* s)
{
    std::string theChecks;
    bool theOOS;
    if (inRust) {
        char theBuffer[4096];
        ee_net_sv_checks_state(sRust, (uint8_t*)theBuffer, sizeof(theBuffer));
        theChecks = theBuffer;
        theOOS = ee_net_sv_is_oos(sRust);
    } else {
        theChecks = OrigChecks(s);
        theOOS = s->mIsOOS;
    }
    gW->Line("state humans=%d oos=%d %s", s->mNumHumanPlayers, (int)theOOS, theChecks.c_str());
    if (inRust)
        gW->Op("state " + std::to_string(s->mNumHumanPlayers) + " " + std::to_string((int)theOOS) + " " + theChecks);
}

static std::string RunSide(bool inRust, uint32_t inSeed, int inSteps, bool inRecord, std::string& outOps)
{
    KWorld theWorld;
    theWorld.mRandom = inSeed * 2654435761u + 7;
    theWorld.mRecord = inRecord;
    gW = &theWorld;
    Rng r = {inSeed};
    NESynchronousServer* s = new NESynchronousServer;
    if (inRust) {
        sRustServer = s;
        sRust = ee_net_sv_new(NULL, RustServerHost);
        gW->Op("new");
    }
    unsigned long theBaseTick = r.Below(3);
    static const unsigned long kChecksums[] = {1000, 1000, 1000, 1000, 2000, 2000, 3000, 0};
    static const unsigned char kSlots[] = {kSlotServer, kSlotServer, 0, 1, 2, 3, 4, 5};
    for (int i = 0; i < inSteps; i++) {
        uint32_t w = r.Below(100);
        if (w < 10) {   // the counts change
            static const unsigned char kHumans[] = {0, 1, 2, 2, 3, 3, 4, 5, 6};
            s->mNumHumanPlayers = kHumans[r.Below(9)];
            s->mMaxPlayers = (unsigned char)(1 + r.Below(9));
            if (r.Below(3) == 0)
                theBaseTick += 1 + r.Below(3);
            gW->Line("set humans=%d players=%d base=%lu", s->mNumHumanPlayers, s->mMaxPlayers, theBaseTick);
            continue;
        }
        std::string theCall;
        if (w < 75) {   // a checksum (mostly the tick being answered, one way)
            NEMWorldDatabaseChecksumResponse m;
            m.mTick = theBaseTick + (r.Below(4) == 0 ? r.Below(4) : 0);
            m.mChecksum = r.Below(3) == 0 ? kChecksums[r.Below(8)] : 1000 + 1000 * (theBaseTick % 2);
            m.mSlot = kSlots[r.Below(8)];
            theCall = "call 0 " + std::to_string(m.mTick) + " " + std::to_string(m.mChecksum) + " " + std::to_string(m.mSlot);
            gW->Line("%s", theCall.c_str());
            gW->Op(theCall);
            if (inRust)
                ee_net_sv_log_checksum_response(sRust, (uint32_t)m.mTick, (uint32_t)m.mChecksum, m.mSlot);
            else
                s->LogChecksumResponse(&m);
            if (r.Below(5) == 0)
                theBaseTick++;
        } else if (w < 88) {   // the tick's out-of-sync block
            theCall = "call 1";
            gW->Line("%s", theCall.c_str());
            gW->Op(theCall);
            if (inRust)
                ee_net_sv_remove_oos_players(sRust);
            else
                s->OOSBlock();
        } else {   // a player removed
            static const unsigned char kRemove[] = {kSlotServer, kSlotNone, 0, 1, 2, 3, 4, 5};
            unsigned char theSlot = kRemove[r.Below(8)];
            bool a = r.Below(2) != 0, b = r.Below(3) == 0, c = r.Below(4) == 0;
            theCall = "call 2 " + std::to_string(theSlot) + " " + std::to_string((int)a) + " " + std::to_string((int)b) +
                      " " + std::to_string((int)c);
            gW->Line("%s", theCall.c_str());
            gW->Op(theCall);
            if (inRust)
                ee_net_sv_remove_player_from_game_in_progress(sRust, theSlot, a, b, c);
            else
                s->RemovePlayerFromGameInProgress(theSlot, a, b, c);
        }
        gW->Op("ret 0");
        State(inRust, s);
    }
    if (inRust)
        ee_net_sv_free(sRust);
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
            std::string theKind = theLine.substr(0, theSecond);
            // (the out-of-sync states apart)
            if (theKind.compare(0, 6, "state ") == 0)
                theKind = theLine.find(" oos=1") != std::string::npos ? "state oos" : "state";
            theKinds[theKind]++;
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
        if (k.first.compare(0, 4, "set ") != 0 && k.first.compare(0, 5, "call ") != 0)
            printf("  %-28s %d\n", k.first.c_str(), k.second);
    printf("ne_checks_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
