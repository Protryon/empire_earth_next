// The computer-player decision trace and the cheat switches (ee/EECPTrace.h; docs/AI_RESEARCH.md §5 stage 0).
// New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lle/LowLevelEngine.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPTrace.h"
#include "rust/sim/ai.h"   // ee_ai_switch*: the TEST ONLY switches (switches.rs)

bool gCPTraceOn = false;
bool gCPTraceHashOn = false;

// the Rust accessors' first read (EE_AI_TRACE, and the TEST ONLY switches EE_AI_PERSONALITY, EE_AI_DEMAND,
// EE_AI_CHEATS, EE_AI_EXP: ee_ai personality.rs, switches.rs, read once), and the lobby's difficulty
extern "C" void ee_ai_host_trace_init()
{
    ee_ai_trace_init(&gCPTraceOn, &gCPTraceHashOn);
}

extern "C" uint32_t ee_ai_host_difficulty()
{
    return (uint32_t)gWorld.mLobbyOptions.mDifficulty;
}

// The personality, the experiments and the cheats are Rust's (ee_ai personality.rs: every switch of the rules)
bool EECPExperiment(const EEComputerPlayer* inPlayer, const char* inName)
{
    return ee_ai_experiment(inPlayer->mPlayerID, inName);
}

const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer)
{
    return EEGetRules().mAIPersonalities.mPersonalities[(size_t)ee_ai_personality_index(inPlayer->mPlayerID)];
}

const char* EECPCheatNames(const EEComputerPlayer* inPlayer)
{
    return ee_ai_cheat_names_text(inPlayer != NULL ? inPlayer->mPlayerID : 0);
}

// the line is Rust's (ee_ai trace_line.rs: the manager, the resources, the debug output and the game log)
void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
{
    char theText[640];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    ee_ai_trace_out(inPlayer, inEvent, theText);
}

// ---------------------------------------------------------------------------------------------------------------
// The trace lines beyond the decisions, the out-of-synch checksum and its dump: Rust's (ee_ai next_trace.rs)

void EECPOutOfSynchDump(FILE* inFile)
{
    std::string theText;
    ee_ai_out_of_synch_dump(&theText);
    fputs(theText.c_str(), inFile);
}
