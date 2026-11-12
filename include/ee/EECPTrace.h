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
// ee/EERules.h EEAICheat; none by default, the original kept every one: a cheat test needs a gameoptions.yaml whose
// difficulty lists them). TEST ONLY: EE_AI_CHEATS in the environment narrows
// that further to the cheats it names, comma separated ("none": every one off; unset or "all": no change); sets
// separated by '/' are taken in turn by player id from player 1 (the human slot), as EE_AI_PERSONALITY: with two
// computer players (p2, p3), "none/all" makes p2 cheat and p3 honest. An
// environment variable is per machine, so peers of a multiplayer game with different values would desync.
#ifndef EE_EECPTRACE_H
#define EE_EECPTRACE_H

#include <stdio.h>
#include "ee/EERules.h"

class EEComputerPlayer;

extern bool gCPTraceHashOn;                 // EE_AI_TRACE set: the sim hash lines too (EEArtificialIntelligence)
extern bool gCPTraceOn;                     // EE_AI_TRACE set (read at the first EECPTraceInit)
extern const char* gCPTraceManager;         // the manager running now (EEComputerPlayer::Update sets it)

void EECPTraceInit();                       // reads the environment once; every accessor calls it
void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
#if defined(__clang__) || defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
void EECPTraceStats(EEComputerPlayer* inPlayer, unsigned long inTime);   // every 30 game s per player
void EECPTraceIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount);   // counted into the next income line
// TEST ONLY (EE_AI_EXP spending, docs/AI_RESEARCH.md §8.51): what a payment bought (kinds: 0 citizens, 1 soldiers,
// 2 buildings, 3 techs, 4 epochs, 5 boats, 6 improvements, 7 other), counted into the next "spend" line
void EECPTraceSpend(EEComputerPlayer* inPlayer, long inKind, const long inCost[5]);
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
// TEST ONLY (EE-SIM's state dumps): its parts per player and the computer players' own timers, one line each
void EECPOutOfSynchDump(FILE* inFile);

// the computer player's personality (aipersonalities.yaml; ee/EERules.h). TEST ONLY: EE_AI_PERSONALITY in the
// environment names one for every computer player, or a comma list they take in turn by player id
const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer);
// TEST ONLY: EE_AI_EXP names experiments, "a,b" for every computer player or "set/set/..." in turn from player 1
// (as EE_AI_CHEATS): code behind EECPExperiment(player, "a") is tried head to head before it becomes data
bool EECPExperiment(const EEComputerPlayer* inPlayer, const char* inName);
const char* EECPExperimentNames(const EEComputerPlayer* inPlayer);   // ",a,b," or "-" (the start line's exp=)
bool EECPDemandSplitOn(const EEComputerPlayer* inPlayer);
bool EECPMilitiaOn(const EEComputerPlayer* inPlayer);
bool EECPCoastFishingOn(const EEComputerPlayer* inPlayer);  // economy.coast_fishing; TEST ONLY: EE_AI_EXP nofish
bool EECPMilitiaTowersOn(const EEComputerPlayer* inPlayer); // military.militia_towers; TEST ONLY: EE_AI_EXP nomtower
long EECPPrehistoricArmy(const EEComputerPlayer* inPlayer); // military.prehistoric_army (0: none); TEST ONLY: EE_AI_EXP noprearmy
// the §8.27 rules; TEST ONLY: EE_AI_EXP turns each on (foodeco, goldfix, navalflee, prepeace, stone22/stone30) or off
// (nofoodeco, nogoldfix, nonavalflee, noprepeace, nostone)
bool EECPForagePatchesOn(const EEComputerPlayer* inPlayer);   // economy.forage_patches
bool EECPClaimNeededOn(const EEComputerPlayer* inPlayer);     // economy.claim_needed
bool EECPNavalFleeOn(const EEComputerPlayer* inPlayer);       // military.naval_flee
bool EECPPrehistoricPeaceOn(const EEComputerPlayer* inPlayer);   // not military.prehistoric_attacks
float EECPStoneSpan(const EEComputerPlayer* inPlayer);        // epoch_pressure.stone_span in minutes (0: the timer)
// the §8.29 rules; TEST ONLY: EE_AI_EXP turns each on (noupgrade, fewbarracks, patchexpand, safedash, stonepeace) or
// off (nonoupgrade, manybarracks, nopatchexpand, nosafedash, nostonepeace)
long EECPEarlyTownCenters(const EEComputerPlayer* inPlayer);  // economy.early_town_centers (0: the original's timers)
long EECPEarlyTrainers(const EEComputerPlayer* inPlayer);     // economy.early_trainers (0: no cap)
bool EECPPatchExpandOn(const EEComputerPlayer* inPlayer);     // economy.patch_expand
float EECPSafeBonus(const EEComputerPlayer* inPlayer);        // epoch_pressure.safe_bonus
bool EECPStonePeaceOn(const EEComputerPlayer* inPlayer);      // not military.stone_attacks
// economy.forage_patches: the land the AI claims and builds its drop sites on, in tiles from its town centre (the
// owner: "stop claiming land it cannot defend"; docs/AI_RESEARCH.md §8.28): kEECPClaimRange, a step more each time a
// farm, granary or settlement finds no room ("if no room for stuff like a farm, then expand"), kEECPClaimRangeMax at most
const long kEECPClaimRange = 25;
const long kEECPClaimRangeStep = 5;
const long kEECPClaimRangeMax = 45;
long EECPClaimRange(const EEComputerPlayer* inPlayer);
void EECPClaimExpand(const EEComputerPlayer* inPlayer);   // one step (at most one per 2 game minutes)
// ... and a worked forage patch has a drop site within this many tiles ("every patch should have 0 or 1 tile
// distance to a settlement")
const long kEECPPatchDropRange = 3;
// economy.forage_patches with the claim range, the settlements on the patches and the drop site rule (TEST ONLY:
// EE_AI_EXP oldpatches keeps forage_patches without them, as in §8.27)
bool EECPClaimRangeOn(const EEComputerPlayer* inPlayer);       // the personality's military.militia; TEST ONLY: EE_AI_EXP nomilitia   // the personality's economy.demand_split; TEST ONLY: EE_AI_DEMAND
                                           // overrides it (0 or 1, or per player in turn from player 1, "0/1")

bool EECPCheatOn(const EEComputerPlayer* inPlayer, long inCheat);          // whether that cheat stays on (the difficulty's ai_cheats, EE_AI_CHEATS)
const char* EECPCheatNames(const EEComputerPlayer* inPlayer);               // the cheats that are on, for the trace ("cost,reveal,..." or "none")

#endif
