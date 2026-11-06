// The computer-player decision trace and the cheat switches (docs/AI_RESEARCH.md §5, stage 0). New in this port
// (no original address).
//
// Trace: EE_AI_TRACE=1 in the environment turns it on. Each AI decision is one debug string (ESDebugOut):
//   EE-AI <game ms> p<player> <manager> <event> key=value ... | f=<food> w=<wood> s=<stone> g=<gold> i=<iron>
// The lines hold no pointers and no wall-clock values, so two runs of the same seeded game must print the same
// lines (tools/harness/ai_trace.py compare). The AI's real-time cost goes into separate "EE-AIPROF" lines, which
// the comparison skips. Off, every trace site costs one bool test.
//
// Cheats: the game's difficulty sets which computer-player cheats stay on (gameoptions.yaml `ai_cheats`,
// ee/EERules.h EEAICheat; every one by default, as the original). TEST ONLY: EE_AI_CHEATS in the environment narrows
// that further to the cheats it names, comma separated ("none": every one off; unset or "all": no change); sets
// separated by '/' are taken in turn by player id from player 1 (the human slot), as EE_AI_PERSONALITY: with two
// computer players (p2, p3), "none/all" makes p2 cheat and p3 honest. An
// environment variable is per machine, so peers of a multiplayer game with different values would desync.
#ifndef EE_EECPTRACE_H
#define EE_EECPTRACE_H

#include "ee/EERules.h"

class EEComputerPlayer;

extern bool gCPTraceOn;                     // EE_AI_TRACE set (read at the first EECPTraceInit)
extern const char* gCPTraceManager;         // the manager running now (EEComputerPlayer::Update sets it)

void EECPTraceInit();                       // reads the environment once; every accessor calls it
void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
#if defined(__clang__) || defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
void EECPTraceStats(EEComputerPlayer* inPlayer, unsigned long inTime);   // every 30 game s per player
void EECPTraceUnitLost(EEComputerPlayer* inPlayer, EEUnit* inUnit);       // counted into the next stats line
void EECPTraceHash(unsigned long inTime);   // every 10 game s: the out-of-synch checksum's parts ("sim hash")
void EECPTraceProfile(EEComputerPlayer* inPlayer, unsigned long inTime, long long inMicroseconds);

#define EE_CP_TRACE(player, event, ...) \
    do { if (gCPTraceOn) EECPTraceOut((player), (event), __VA_ARGS__); } while (0)

// The scope sets gCPTraceManager for one manager call and restores the previous name.
struct EECPTraceScope {
    explicit EECPTraceScope(const char* inName) : mSaved(gCPTraceManager) { gCPTraceManager = inName; }
    ~EECPTraceScope() { gCPTraceManager = this->mSaved; }
    const char* mSaved;
};


// the computer players' share of the out-of-synch checksum (EEPhysics::GetOutOfSynchChecksum; not a trace: always on)
unsigned long EECPOutOfSynchChecksum();

// the computer player's personality (aipersonalities.yaml; ee/EERules.h). TEST ONLY: EE_AI_PERSONALITY in the
// environment names one for every computer player, or a comma list they take in turn by player id
const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer);
bool EECPDemandSplitOn(const EEComputerPlayer* inPlayer);   // TEST ONLY: EE_AI_DEMAND=1 (or per player in turn from player 1,
                                           // "0/1"): split the gatherers by the tiered demand (EECPResourceManager.cpp)

bool EECPCheatOn(const EEComputerPlayer* inPlayer, long inCheat);          // whether that cheat stays on (the difficulty's ai_cheats, EE_AI_CHEATS)
const char* EECPCheatNames(const EEComputerPlayer* inPlayer);               // the cheats that are on, for the trace ("cost,reveal,..." or "none")

#endif
