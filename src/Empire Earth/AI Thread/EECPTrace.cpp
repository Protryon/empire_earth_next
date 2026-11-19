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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitAction.h"
#include "ee/EECPTrace.h"
#include "rust/sim/ai.h"   // ee_ai_switch*: the TEST ONLY switches (switches.rs)
#include "ee/EECPExperimentTimers.h"
#include "ee/EECPNext.h"
#include "ee/EEGameLog.h"
#include "lle/U2DPoint.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include <math.h>

bool gCPTraceOn = false;
bool gCPTraceHashOn = false;
const char* gCPTraceManager = "-";

static bool sInitialized = false;
static char sCheatNames[128];

// a new game (EEWorld, before its players are made): the computer players' per-game statics, now Rust's (the
// trace's counters and next hash line, the loss incidents, the reflexes' cool-downs), cleared as at the start
void EECPNewGame()
{
    ee_ai_new_game();
}

// a personality's position in the rules (-1: none), and a line of the debug output: the switches' callbacks
static long EECPTraceFindPersonality(const char* inName)
{
    return EEGetRules().mAIPersonalities.Find(inName);
}

static void EECPTraceDebugOut(const char* inLine)
{
    ESDebugOut("%s", inLine);
}

// EE_AI_TRACE, and the TEST ONLY switches (EE_AI_PERSONALITY, EE_AI_DEMAND, EE_AI_CHEATS, EE_AI_EXP): Rust's
// (ee_ai switches.rs), read once
void EECPTraceInit()
{
    if (sInitialized)
        return;
    sInitialized = true;
    const char* theTrace = getenv("EE_AI_TRACE");
    gCPTraceOn = theTrace != NULL && theTrace[0] != '\0' && strcmp(theTrace, "0") != 0;
    gCPTraceHashOn = gCPTraceOn;   // the sim hash only with EE_AI_TRACE (the game log turns gCPTraceOn on alone)
    ee_ai_switches_init(EECPTraceFindPersonality, EECPTraceDebugOut);
}

bool EECPExperiment(const EEComputerPlayer* inPlayer, const char* inName)
{
    EECPTraceInit();
    return ee_ai_switch_experiment(inPlayer->mPlayerID, inName);
}

const char* EECPExperimentNames(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    return ee_ai_switch_experiment_names(inPlayer->mPlayerID);
}

bool EECPMilitiaOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mMilitia && !EECPExperiment(inPlayer, "nomilitia");
}

bool EECPCoastFishingOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mCoastFishing && !EECPExperiment(inPlayer, "nofish");
}

bool EECPMilitiaTowersOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mMilitiaTowers && !EECPExperiment(inPlayer, "nomtower");
}

long EECPPrehistoricArmy(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "prearmy4"))
        return 4;   // TEST ONLY: the owner's "maybe 4 clubmen before stone"
    return EECPExperiment(inPlayer, "noprearmy") ? 0 : EECPPersonality(inPlayer).mPrehistoricArmy;
}

static bool EECPRuleOn(const EEComputerPlayer* inPlayer, bool inRule, const char* inOn, const char* inOff)
{
    if (EECPExperiment(inPlayer, inOn))
        return true;
    return inRule && !EECPExperiment(inPlayer, inOff);
}

bool EECPForagePatchesOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mForagePatches, "foodeco", "nofoodeco");
}

bool EECPClaimNeededOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mClaimNeeded, "goldfix", "nogoldfix");
}

bool EECPNavalFleeOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mNavalFlee, "navalflee", "nonavalflee");
}

long EECPEarlyTownCenters(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "noupgrade"))
        return 2;
    return EECPExperiment(inPlayer, "nonoupgrade") ? 0 : EECPPersonality(inPlayer).mEarlyTownCenters;
}

long EECPEarlyTrainers(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "fewbarracks"))
        return 2;
    return EECPExperiment(inPlayer, "manybarracks") ? 0 : EECPPersonality(inPlayer).mEarlyTrainers;
}

bool EECPPatchExpandOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mPatchExpand, "patchexpand", "nopatchexpand");
}

float EECPSafeBonus(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "safedash"))
        return 0.25f;
    return EECPExperiment(inPlayer, "nosafedash") ? 0.0f : EECPPersonality(inPlayer).mSafeBonus;
}

bool EECPStonePeaceOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, !EECPPersonality(inPlayer).mStoneAttacks, "stonepeace", "nostonepeace");
}

bool EECPPrehistoricPeaceOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, !EECPPersonality(inPlayer).mPrehistoricAttacks, "prepeace", "noprepeace");
}

long EECPClaimRange(const EEComputerPlayer* inPlayer)
{
    return kEECPClaimRange;   // (stage 6: the strategy's claim range is the Lua verbs', data/ai/lib/verbs.lua)
}

bool EECPClaimRangeOn(const EEComputerPlayer* inPlayer)
{
    return EECPForagePatchesOn(inPlayer) && !EECPExperiment(inPlayer, "oldpatches");
}

float EECPStoneSpan(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "stone22"))
        return 22.0f;
    if (EECPExperiment(inPlayer, "stone30"))
        return 30.0f;
    return EECPExperiment(inPlayer, "nostone") ? 0.0f : EECPPersonality(inPlayer).mStoneSpan;
}

bool EECPDemandSplitOn(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    int32_t theDemand = ee_ai_switch_demand(inPlayer->mPlayerID);
    return theDemand < 0 ? EECPPersonality(inPlayer).mDemandSplit : theDemand != 0;
}

const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    const EEAIPersonalityRules& theRules = EEGetRules().mAIPersonalities;
    long k = ee_ai_switch_personality(inPlayer->mPlayerID);
    if (k >= 0 && (size_t)k < theRules.mPersonalities.size())
        return theRules.mPersonalities[k];
    return theRules.ForPlayer(inPlayer->mPlayerID);
}

// The game's difficulty's cheats (gameoptions.yaml ai_cheats), narrowed by EE_AI_CHEATS (the player's set)
static long EECPCheats(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    long theMask = ee_ai_switch_cheats(inPlayer != NULL ? inPlayer->mPlayerID : 1);
    return EEDifficultyAICheats(gWorld.mLobbyOptions.mDifficulty) & theMask;
}

bool EECPCheatOn(const EEComputerPlayer* inPlayer, long inCheat)
{
    return (EECPCheats(inPlayer) & inCheat) != 0;
}

const char* EECPCheatNames(const EEComputerPlayer* inPlayer)
{
    ee_ai_switch_cheat_names(EECPCheats(inPlayer), sCheatNames, sizeof(sCheatNames));
    return sCheatNames;
}

void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
{
    char theText[640];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    const long* theResources = inPlayer->mResourceCaps;
    char theLine[800];
    snprintf(theLine, sizeof(theLine), "EE-AI %lu p%lu %s %s %s | f=%ld w=%ld s=%ld g=%ld i=%ld\n", gPhysics->mGameTime,
        inPlayer->mPlayerID, gCPTraceManager, inEvent, theText, theResources[kResourceFood], theResources[kResourceWood],
        theResources[kResourceStone], theResources[kResourceGold], theResources[kResourceIron]);
    ESDebugOut("%s", theLine);
    if (EEGameLogOn())
        EEGameLogLine(theLine);
}

// ---------------------------------------------------------------------------------------------------------------
// The trace lines beyond the decisions, the out-of-synch checksum and its dump: Rust's (ee_ai next_trace.rs)

// What a payment bought (TEST ONLY, EE_AI_EXP spending), counted into the next "spend" line
void EECPTraceSpend(EEComputerPlayer* inPlayer, long inKind, const long inCost[5])
{
    if (!gCPTraceOn || inPlayer->mPlayerID >= 64 || inKind < 0 || inKind > 7 || !EECPExperiment(inPlayer, "spending"))
        return;
    ee_ai_trace_spend(inPlayer->mPlayerID, inKind, inCost);
}

void EECPTraceIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount)
{
    ee_ai_trace_income(inPlayer->mPlayerID, inResource, inAmount);
}

void EECPTraceUnitLost(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    ee_ai_trace_unit_lost(inPlayer, (uint64_t)(uintptr_t)static_cast<RTSUnit*>(inUnit));
}

void EECPTraceStats(EEComputerPlayer* inPlayer, unsigned long inTime)
{
    ee_ai_trace_stats(inPlayer, inTime);
}

void EECPTraceProfile(EEComputerPlayer* inPlayer, unsigned long inTime, long long inMicroseconds)
{
    ee_ai_trace_profile(inPlayer->mPlayerID, inTime, (int64_t)inMicroseconds);
}

void EECPOutOfSynchDump(FILE* inFile)
{
    std::string theText;
    ee_ai_out_of_synch_dump(&theText);
    fputs(theText.c_str(), inFile);
}

unsigned long EECPOutOfSynchChecksum()
{
    return ee_ai_out_of_synch_checksum();
}

void EECPTraceHash(unsigned long inTime)
{
    ee_ai_trace_hash(inTime);
}
