// The computer-player decision trace and the cheat switches (docs/AI_RESEARCH.md §5, stage 0). New in this port
// (no original address).
//
// Trace: EE_AI_TRACE=1 in the environment turns it on. Each AI decision is one debug string (ESDebugOut):
//   EE-AI <game ms> p<player> <manager> <event> key=value ... | f=<food> w=<wood> s=<stone> g=<gold> i=<iron>
// The lines hold no pointers and no wall-clock values, so two runs of the same seeded game must print the same
// lines (tools/harness/ai_trace.py compare). The AI's real-time cost goes into separate "EE-AIPROF" lines, which
// the comparison skips. Off, every trace site costs one bool test.
//
// Cheats: EE_AI_CHEATS names the cheats that stay on, comma separated; unset (or "all") keeps every one, as the
// original. "none" turns every one off. Names: cost, reveal, epochs, extra, popband, catchup, retire.
// TEST ONLY until stage 1 moves them into the rules data: an environment variable is per machine, so peers of a
// multiplayer game with different values would desync.
#ifndef EE_EECPTRACE_H
#define EE_EECPTRACE_H

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
void EECPTraceProfile(EEComputerPlayer* inPlayer, unsigned long inTime, long long inMicroseconds);

#define EE_CP_TRACE(player, event, ...) \
    do { if (gCPTraceOn) EECPTraceOut((player), (event), __VA_ARGS__); } while (0)

// The scope sets gCPTraceManager for one manager call and restores the previous name.
struct EECPTraceScope {
    explicit EECPTraceScope(const char* inName) : mSaved(gCPTraceManager) { gCPTraceManager = inName; }
    ~EECPTraceScope() { gCPTraceManager = this->mSaved; }
    const char* mSaved;
};

enum EECPCheat {
    kCPCheatCost = 1 << 0,      // ScaleCost: the difficulty's unit cost modifier (dbcpbehavior rows 67/96)
    kCPCheatReveal = 1 << 1,    // ExploreUnseen on the first update: the whole map explored
    kCPCheatEpochs = 1 << 2,    // epoch advances are free and always affordable
    kCPCheatExtra = 1 << 3,     // computer_player_extra_resources (row 101) gold and iron at the start
    kCPCheatPopBand = 1 << 4,   // WorkOutPopulation blends the pop cap toward the largest human population
    kCPCheatCatchUp = 1 << 5,   // CatchUpEpoch pulls the next epoch advance forward when another player is ahead
    kCPCheatRetire = 1 << 6,    // RetireObsoleteUnits deletes obsolete units no human sees
    kCPCheatAll = (1 << 7) - 1,
};

bool EECPCheatOn(long inCheat);             // whether that cheat stays on (EE_AI_CHEATS)
const char* EECPCheatNames();               // the cheats that are on, for the trace ("cost,reveal,..." or "none")

#endif
