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
#include "rust/sim/ai.h"   // ee_ai_new_game, ee_ai_out_of_synch_checksum (next_trace.rs)

class EEComputerPlayer;

extern bool gCPTraceHashOn;                 // EE_AI_TRACE set: the sim hash lines too (EEArtificialIntelligence)
extern bool gCPTraceOn;                     // EE_AI_TRACE set (read at the first EECPTraceInit)

void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
#if defined(__clang__) || defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

#define EE_CP_TRACE(player, event, ...) \
    do { if (gCPTraceOn) EECPTraceOut((player), (event), __VA_ARGS__); } while (0)

// TEST ONLY (EE-SIM's state dumps): its parts per player and the computer players' own timers, one line each
void EECPOutOfSynchDump(FILE* inFile);

// the computer player's personality (aipersonalities.yaml; ee/EERules.h). TEST ONLY: EE_AI_PERSONALITY in the
// environment names one for every computer player, or a comma list they take in turn by player id
const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer);
// TEST ONLY: EE_AI_EXP names experiments, "a,b" for every computer player or "set/set/..." in turn from player 1
// (as EE_AI_CHEATS): code behind EECPExperiment(player, "a") is tried head to head before it becomes data
bool EECPExperiment(const EEComputerPlayer* inPlayer, const char* inName);
// the personality's switches (military.militia, economy.forage_patches, ...) with their experiments, the claim range
// (kEECPClaimRange 25), the cheats that stay on (the difficulty's ai_cheats narrowed by EE_AI_CHEATS): ee_ai
// personality.rs, which the Rust bodies call
const char* EECPCheatNames(const EEComputerPlayer* inPlayer);               // the cheats that are on, for the trace ("cost,reveal,..." or "none")

#endif
