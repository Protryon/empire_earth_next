// The next engine's C++ side (ee/EECPNext.h; docs/AI_LUA_PLAN.md §2, stage S1): the computer player's own bookkeeping
// before the engine's tick, the class and tech names, the shims of the engine's entry points, and the host of its
// Rust body (ee_ai next_game.rs ee_ai_next_host: the reads the world views do not hold). The engine, its state, the
// view, the army's memory, the strategy step and the save are Rust (ee_ai next_state.rs, next_value.rs,
// next_game.rs). New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
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
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPReflexes.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEGameLog.h"
#include "ee/EELuaApi.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESaveGameFiles.h"
#include "EEDataText.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_*: the engine (next_game.rs), the name tables (names.rs)

// the facts of the Lua build the Rust runtime (ee_ai lua_api.rs: mlua-sys, lua54) assumes; lua_host.rs checks its side
static_assert(LUA_VERSION_NUM == 504, "Lua 5.4");
static_assert(sizeof(lua_Integer) == 8 && sizeof(lua_Number) == 8, "64-bit integers and doubles");
static_assert(LUAI_MAXSTACK == 1000000 && LUA_REGISTRYINDEX == -1001000, "the stack size and the registry index");
static_assert(LUAL_BUFFERSIZE == 16 * sizeof(void*) * sizeof(lua_Number), "luaL_Buffer's initial size");
static_assert(LUA_EXTRASPACE == sizeof(void*) && LUA_IDSIZE == 60, "the extra space and the chunk id size");
static_assert(LUA_MASKCOUNT == 8 && LUA_NOREF == -2 && LUA_REFNIL == -1, "the count hook's mask, the references");
static_assert(sizeof(luaL_Buffer) == 4 * sizeof(void*) + LUAL_BUFFERSIZE, "luaL_Buffer's layout");

const char* EECPNextCodeName(long inCode)
{
    return ee_ai_code_name((int64_t)inCode);
}

const char* EECPNextReasonName(long inReason)
{
    return ee_ai_reason_name((int64_t)inReason);
}

// ---------------------------------------------------------------------------------------------------------------
// The state (Rust's record)

EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer)
{
    if (inPlayer->mNextState == NULL)
        inPlayer->mNextState = (EECPNextState*)ee_ai_next_state_new();
    return inPlayer->mNextState;
}

void EECPNextDeleteState(EEComputerPlayer* inPlayer)
{
    ee_ai_next_state_free(inPlayer->mNextState);
    inPlayer->mNextState = NULL;
}

std::string EECPNextAI(const EEComputerPlayer* inPlayer)
{
    if (inPlayer == NULL || inPlayer->mNextState == NULL)
        return std::string();
    size_t theLength = 0;
    const uint8_t* theAI = ee_ai_next_state_ai(inPlayer->mNextState, &theLength);
    return std::string((const char*)theAI, theLength);
}

void EECPNextSetAI(EEComputerPlayer* inPlayer, const std::string& inAI)
{
    ee_ai_next_state_set_ai(EECPNextGetState(inPlayer), (const uint8_t*)inAI.data(), inAI.size());
}

// EEComputerPlayer::CanAfford: the plain check (EEPlayer::CanAfford), an epoch free with the epochs cheat (stage 6: the
// strategy's reserve is the Lua verbs' own check, data/ai/lib/verbs.lua)
bool EECPNextCanAfford(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, long inCount)
{
    bool theEpoch = inNode != NULL && inNode->mData != NULL && EEIsEpochID(inNode->mData->mID);
    if (theEpoch && inPlayer->mTechnologies != NULL && EECPCheatOn(inPlayer, kAICheatEpochs))
        return true;
    return inPlayer->EEPlayer::CanAfford(inNode, inUnitType, inCount);
}

void EECPNextIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount)
{
    if (inPlayer->mNextState != NULL)
        ee_ai_next_income(inPlayer->mNextState, inResource - kResourceFood, inAmount);
}

void EECPNextEventAdd(const EEComputerPlayer* inPlayer, long inKind, long inType, long inCount)
{
    if (inPlayer->mNextState != NULL)
        ee_ai_next_event_add(inPlayer->mNextState, inKind, inType, inCount);
}

// the next engine's tick: the bookkeeping the classic tick does first (EEComputerPlayer::Update): new units into
// resource centres, the combat state's decay and the population cap (WorkOutPopulation; GetPopulationLimit reads it).
// EnableManagers does not run: its flags gate only classic decisions. Then the engine (ee_ai_next_update): the
// reflexes' danger zones, the army's scan, one strategy step on a fresh view.
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime)
{
    EECPNextGetState(inPlayer);
    if (inPlayer->mPlacement == NULL || inPlayer->mTechnologies == NULL)
        return;
    if (inPlayer->mRegroupPending) {
        inPlayer->UpdateGroups();
        inPlayer->mRegroupPending = false;
    }
    if (inPlayer->mCombatState == 2 && inPlayer->mNumAttacks == 0 && inTime - inPlayer->mLastDamagedTime > 60000) {
        inPlayer->SetCombatState(1);
        inPlayer->mBuildingDamaged = false;
        inPlayer->mCitizenDamaged = false;
    }
    if (inTime >= (unsigned long)inPlayer->mNextPopulationTime) {
        inPlayer->WorkOutPopulation();
        inPlayer->mNextPopulationTime = inTime + 60000;
    }
    ee_ai_next_update(inPlayer, inTime);
}

bool EECPNextPrepare(EEComputerPlayer* inPlayer)
{
    return ee_ai_next_prepare(inPlayer);
}

bool EECPNextSaveState(EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    return ee_ai_next_save(inPlayer, &outBytes);
}

bool EECPNextLoadState(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    return ee_ai_next_load(inPlayer, (const uint8_t*)inBytes.data(), inBytes.size());
}

extern "C" void ee_ai_next_string_append(void* inString, const uint8_t* inData, size_t inLength)
{
    ((std::string*)inString)->append((const char*)inData, inLength);
}

// ---------------------------------------------------------------------------------------------------------------
// The gather safety and the losses trace (ee_ai next_game.rs)

static uint64_t EECPNextHandle(const RTSUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

long EECPNextAssigned(EEComputerPlayer* inPlayer, EEUnit* inResource)
{
    return ee_ai_next_assigned(inPlayer, EECPNextHandle(inResource));
}

bool EECPNextHasRoom(EEComputerPlayer* inPlayer, EEUnit* inResource)
{
    return ee_ai_next_has_room(inPlayer, EECPNextHandle(inResource));
}

bool EECPNextDropReady(EEComputerPlayer* inPlayer, long inX, long inY, bool inFood, long inSlack)
{
    return ee_ai_next_drop_ready(inPlayer, 0, inX, inY, inFood, inSlack);
}

bool EECPNextUnitDropReady(EEComputerPlayer* inPlayer, EEUnit* inResource)
{
    return inResource != NULL && ee_ai_next_drop_ready(inPlayer, EECPNextHandle(inResource), 0, 0, false, 0);
}

long EECPNextBuildUnsafe(EEComputerPlayer* inPlayer, long inX, long inY)
{
    return ee_ai_next_build_unsafe(inPlayer, inX, inY);
}

long EECPNextSiteThreat(EEComputerPlayer* inPlayer, long inX, long inY, bool inNow, long* outKind)
{
    return ee_ai_next_site_threat(inPlayer, inX, inY, inNow, outKind);
}

long EECPNextSiteBlocked(EEComputerPlayer* inPlayer, EEUnit* inUnit, long* outCause)
{
    return ee_ai_next_site_blocked(inPlayer, EECPNextHandle(inUnit), outCause);
}

void EECPNextTraceHit(EEComputerPlayer* inPlayer, long inAttacker, EEUnit* inUnit)
{
    ee_ai_next_trace_hit(inPlayer, inAttacker, EECPNextHandle(inUnit));
}

void EECPNextTraceDeath(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    ee_ai_next_trace_death(inPlayer, EECPNextHandle(inUnit));
}

// the reflexes (ee/EECPReflexes.h; ee_ai next_reflex.rs)
void EECPRMNoteDanger(EEComputerPlayer* inPlayer, EEUnit* inCitizen, unsigned long inAttacker)
{
    ee_ai_next_note_danger(inPlayer, EECPNextHandle(inCitizen), inAttacker);
}

void EECPRMNavalFlee(EEComputerPlayer* inPlayer, EEUnit* inVictim, EEComplexUnit* inAttacker)
{
    ee_ai_next_naval_flee(inPlayer, EECPNextHandle(inVictim), EECPNextHandle(inAttacker));
}

void EECPRMFleeRaids(EEComputerPlayer* inPlayer)
{
    ee_ai_next_flee_raids(inPlayer);
}

// ---------------------------------------------------------------------------------------------------------------
// Names: classes and techs by their data ids (dbobjects / dbtechtree "id": "citizen", "b_barracks", "inf01_clubman")

// the table of a db table's row ids (EEDbRowIds; none when it has none) with the prefix of a key without an id
// ("dbobjects_": the name of such a key, as tools/eedata ids.py): ee_ai names.rs
static EeAiNames* EECPNextNamesTable(const char* inTable, const char* inPrefix)
{
    std::map<long, std::string> theIds;
    EEDbRowIds(inTable, theIds);
    std::vector<int64_t> theKeys;
    std::vector<EeAiBytes> theNames;
    for (std::map<long, std::string>::iterator it = theIds.begin(); it != theIds.end(); ++it) {
        EeAiBytes theName = {(const uint8_t*)it->second.data(), it->second.size()};
        theKeys.push_back((int64_t)it->first);
        theNames.push_back(theName);
    }
    EeAiBytes thePrefix = {(const uint8_t*)inPrefix, strlen(inPrefix)};
    return ee_ai_names_new(theKeys.data(), theNames.data(), theKeys.size(), thePrefix);
}

static EeAiNames* EECPNextNamesOf(const char* inTable)
{
    static EeAiNames* sClasses = NULL;
    static EeAiNames* sTechs = NULL;
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        sClasses = EECPNextNamesTable("dbobjects", "dbobjects_");
        sTechs = EECPNextNamesTable("dbtechtree", "dbtechtree_");
    }
    return strcmp(inTable, "dbobjects") == 0 ? sClasses : sTechs;
}

static std::string EECPNextNameOf(EeAiNames* inNames, long inKey)
{
    uint8_t theBuffer[48];
    size_t theLength = 0;
    const uint8_t* theName = ee_ai_names_name_of(inNames, (int64_t)inKey, theBuffer, sizeof(theBuffer), &theLength);
    return std::string((const char*)theName, theLength);
}

// "<table>_<key>" names a row without an id in the data (or the binary tree's names)
static long EECPNextKeyOf(EeAiNames* inNames, const std::string& inName)
{
    EeAiBytes theName = {(const uint8_t*)inName.data(), inName.size()};
    return (long)ee_ai_names_key_of(inNames, theName);
}

long EECPNextClassByName(const std::string& inName)
{
    long theClass = EECPNextKeyOf(EECPNextNamesOf("dbobjects"), inName);
    return theClass >= 0 && (unsigned long)theClass < EEUnitClassCount() && gUnitClassTable[theClass] != NULL ? theClass : -1;
}

long EECPNextTechByName(const std::string& inName)
{
    return EECPNextKeyOf(EECPNextNamesOf("dbtechtree"), inName);
}

std::string EECPNextClassName(long inClass)
{
    return EECPNextNameOf(EECPNextNamesOf("dbobjects"), inClass);
}

std::string EECPNextTechName(long inTech)
{
    return EECPNextNameOf(EECPNextNamesOf("dbtechtree"), inTech);
}

// ---------------------------------------------------------------------------------------------------------------
// The host of the engine (ee_ai next_game.rs EE_AI_NEXT_HOST_*): the computer player's own objects (the placement,
// the resource centres, the danger zones), a goal's target, the continent manager's areas and forests (lane C
// re-points these), the tech nodes' methods with bodies, the trace, the game log and the files

static void EECPNextAppend(void* outBytes, const std::string& inText)
{
    ee_ai_next_append(outBytes, (const uint8_t*)inText.data(), inText.size());
}

static bool EECPNextReadFile(const char* inPath, void* outBytes)
{
    FILE* f = PlatFOpen(inPath, "rb");
    if (f == NULL)
        return false;
    char theBuffer[4096];
    size_t n;
    while ((n = fread(theBuffer, 1, sizeof(theBuffer), f)) > 0)
        ee_ai_next_append(outBytes, (const uint8_t*)theBuffer, n);
    bool theOK = !ferror(f);
    fclose(f);
    return theOK;
}

extern "C" int64_t ee_ai_next_host(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    int64_t* out = (int64_t*)outRecord;
    switch (inOp) {
    case EE_AI_NEXT_HOST_NOW:
        return (int64_t)gPhysics->mGameTime;
    case EE_AI_NEXT_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_NEXT_HOST_TRACE:
        if (a != 0) {
            EECPTraceScope theScope((const char*)(uintptr_t)a);
            EECPTraceOut(me, (const char*)(uintptr_t)b, "%s", (const char*)(uintptr_t)c);
        } else
            EECPTraceOut(me, (const char*)(uintptr_t)b, "%s", (const char*)(uintptr_t)c);
        return 0;
    case EE_AI_NEXT_HOST_GAME_LOG_ON:
        return EEGameLogOn() ? 1 : 0;
    case EE_AI_NEXT_HOST_GAME_LOG:
        EEGameLogLine((const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_SIM_MODE:
        return gPlatSimMode ? 1 : 0;
    case EE_AI_NEXT_HOST_STDERR:
        fprintf(stderr, "%s", (const char*)(uintptr_t)a);
        fflush(stderr);
        return 0;
    case EE_AI_NEXT_HOST_ENGINE_LOG:
        EECPEngineLog("%s", (const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_GAME_SEED:
        return (int64_t)gUserInterface->mRandomSeed;
    case EE_AI_NEXT_HOST_READ:
        return EECPNextReadFile((const char*)(uintptr_t)a, outRecord) ? 1 : 0;
    case EE_AI_NEXT_HOST_EXISTS: {
        FILE* f = PlatFOpen((const char*)(uintptr_t)a, "rb");
        if (f == NULL)
            return 0;
        fclose(f);
        return 1;
    }
    case EE_AI_NEXT_HOST_CLASS_NAME:
        EECPNextAppend(outRecord, EECPNextClassName((long)a));
        return 0;
    case EE_AI_NEXT_HOST_CLASSIC_AI:
        if (EECPClassicAI() == NULL)
            return 0;
        EECPNextAppend(outRecord, EECPClassicAI()->mID);
        return 1;
    case EE_AI_NEXT_HOST_AI_SCRIPT: {
        const EECPAIEntry* theAI = EECPAIByID((const char*)(uintptr_t)a);
        if (theAI == NULL)
            return 0;
        EECPNextAppend(outRecord, theAI->mScript);
        return 1;
    }
    case EE_AI_NEXT_HOST_UNITS_STATE:
        return (int64_t)(uintptr_t)EECPUnitsRust(me);
    case EE_AI_NEXT_HOST_TOWN:
        if (me->mPlacement == NULL)
            return 0;
        out[0] = me->mPlacement->mTownCenterTile.mX;
        out[1] = me->mPlacement->mTownCenterTile.mY;
        return 1;
    case EE_AI_NEXT_HOST_POPULATION:
        switch (a) {
        case 0: return me->GetTotalPopulation(true, false);
        case 1: return me->GetPopulationLimit(0);
        case 2: return me->GetTotalPopulation(true, true);
        case 3: return me->EEPlayer::GetPopulationLimit(0);
        case 4: return me->mPopulationCap;
        default: return me->mDropOffDistance;
        }
    case EE_AI_NEXT_HOST_PENDING_EPOCH:
        return EECPPendingEpoch(me);
    case EE_AI_NEXT_HOST_HAS_COMBAT:
        return me->mCombatManager != NULL ? 1 : 0;
    case EE_AI_NEXT_HOST_CAN_GATHER:
        return me->mCanGather ? 1 : 0;
    case EE_AI_NEXT_HOST_RC_COUNT:
        return (int64_t)me->GetNumResourceCenters();
    case EE_AI_NEXT_HOST_RC: {
        EECPResourceCenter* theCenter = me->GetResourceCenter((unsigned long)a);
        out[0] = (int64_t)EECPNextHandle((EEUnit*)theCenter->mUnit.Get());
        out[1] = theCenter->mIsActive ? 1 : 0;
        out[2] = (int64_t)theCenter->mUnits.size();
        return 0;
    }
    case EE_AI_NEXT_HOST_RC_UNIT:
        return (int64_t)EECPNextHandle(me->GetResourceCenter((unsigned long)a)->mUnits[(size_t)b].Get());
    case EE_AI_NEXT_HOST_RC_CENTER: {
        EETileCoordinate theTile;
        me->GetResourceCenter((unsigned long)a)->GetCenter(theTile);
        out[0] = theTile.mX;
        out[1] = theTile.mY;
        return 0;
    }
    case EE_AI_NEXT_HOST_RESOURCE_USERS:
        return (int64_t)gWorld.GetResourceUsers((EEUnit*)(uintptr_t)a);
    case EE_AI_NEXT_HOST_GOAL_UNIT: {
        EEUGRust* theGoal = static_cast<EEUGRust*>((EEUnitGoal*)(uintptr_t)a);
        EEUnit* theUnit = b == 0 ? theGoal->GetGatherResource() : b == 1 ? theGoal->GetAttackTarget() : theGoal->GetBuildTarget();
        return (int64_t)EECPNextHandle(theUnit);
    }
    case EE_AI_NEXT_HOST_CONTINENT_OF:
        return (int64_t)gContinentManager.GetContinent((EEUnit*)(uintptr_t)a, kAIPathLandMass);
    case EE_AI_NEXT_HOST_AREA_HULL: {
        EETileCoordinate theTile;
        theTile.mX = (long)b;
        theTile.mY = (long)c;
        EEAIArea* theArea = ((EEPlayer*)(uintptr_t)a)->GetNearestArea(theTile, true);
        if (theArea == NULL)
            return 0;
        U2DPoint<long> theClosest = theArea->GetClosestHullPoint(U2DPoint<long>((long)b, (long)c));
        out[0] = theClosest.mX;
        out[1] = theClosest.mY;
        return 1;
    }
    case EE_AI_NEXT_HOST_FORESTS:
        return (int64_t)gContinentManager.GetForests().size();
    case EE_AI_NEXT_HOST_FOREST: {
        EEAIForest* theForest = gContinentManager.GetForests()[(unsigned short)a];
        return theForest != NULL ? (int64_t)theForest->GetID() : -1;
    }
    case EE_AI_NEXT_HOST_FOREST_POINT: {
        EEAIForest* theForest = gContinentManager.GetForests()[(unsigned short)a];
        U2DPoint<long> theFrom((long)b, (long)c);
        U2DPoint<long> thePoint = out[2] != 0 ? theForest->GetClosestHullPoint(theFrom) : theForest->GetClosestPoint(theFrom);
        out[0] = thePoint.mX;
        out[1] = thePoint.mY;
        return 0;
    }
    case EE_AI_NEXT_HOST_UNIT:
        return (int64_t)EECPNextHandle(gWorld.GetUnit((unsigned long)a));
    case EE_AI_NEXT_HOST_NODE_COST:
        return me->ScaleCost(((EETechTreeNode*)(uintptr_t)a)->GetCost((long)b, (long)c, 1.0f));
    case EE_AI_NEXT_HOST_NODE_TEST:
        return (b != 0 ? ((EETechTreeNode*)(uintptr_t)a)->GetIsObsolete() : ((EETechTreeNode*)(uintptr_t)a)->GetIsAvailable()) ? 1 : 0;
    case EE_AI_NEXT_HOST_EPOCH_COUNT:
        return EEEpochCount();
    case EE_AI_NEXT_HOST_EPOCH_END:
        return gWorld.mGameOptions.mEpochEnd;
    case EE_AI_NEXT_HOST_EXPERIMENT:
        return EECPExperiment(me, (const char*)(uintptr_t)a) ? 1 : 0;
    case EE_AI_NEXT_HOST_LAST_ATTACKER:
        return (int64_t)EECPNextHandle(((EEComplexUnit*)(uintptr_t)a)->mLastAttacker);
    case EE_AI_NEXT_HOST_STATE:
        return (int64_t)(uintptr_t)EECPNextGetState(me);
    case EE_AI_NEXT_HOST_MICRO_PLAYER: {
        EEPlayer* thePlayer = gWorld.mPlayers[(unsigned long)a];
        EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
        if (theCP == NULL || theCP->mUnitsState == NULL || theCP->mNextState == NULL || !theCP->mActive)
            return 0;
        return (int64_t)(uintptr_t)theCP;
    }
    case EE_AI_NEXT_HOST_STATE_OF:
        return (int64_t)(uintptr_t)me->mNextState;
    case EE_AI_NEXT_HOST_DEBUG_OUT:
        ESDebugOut("%s", (const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_SCOPE: {
        const char* theSaved = gCPTraceManager;
        gCPTraceManager = (const char*)(uintptr_t)a;
        return (int64_t)(uintptr_t)theSaved;
    }
    case EE_AI_NEXT_HOST_PLAYER_ID:
        return (int64_t)me->mPlayerID;
    case EE_AI_NEXT_HOST_DANGER_COUNT:
        return (int64_t)me->mDangerZones.size();
    case EE_AI_NEXT_HOST_DANGER:
        out[0] = me->mDangerZones[(size_t)a].first.mX;
        out[1] = me->mDangerZones[(size_t)a].first.mY;
        out[2] = (int64_t)me->mDangerZones[(size_t)a].second;
        return 0;
    case EE_AI_NEXT_HOST_DANGER_ADD:
        me->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(U2DPoint<long>((long)a, (long)b), (unsigned long)c));
        return 0;
    case EE_AI_NEXT_HOST_DANGER_ERASE:
        me->mDangerZones.erase(me->mDangerZones.begin() + (ptrdiff_t)a);
        return 0;
    case EE_AI_NEXT_HOST_ORDER: {
        // the reflexes' orders (ee_ai next_reflex.rs): attack a unit, or go to a tile
        EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)a;
        if (out[4] != 0)
            theUnit->Reference();
        theUnit->ClearGoalQueue(0);
        if (out[0] == 0)
            theUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit, (EEComplexUnit*)(uintptr_t)out[1], false, false), 0, 0);
        else
            theUnit->SetGoal(EEUGRust::NewLocation(GE2DPoint((float)out[2], (float)out[3]), true, true, false, 0.0f, false, false), 0, 0);
        theUnit->StartAction(kActionPrepareToMove);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        if (out[4] != 0)
            theUnit->Dereference();
        return 0;
    }
    case EE_AI_NEXT_HOST_UNITS_OF_TYPE: {
        std::vector<EEUnit*> theUnits;
        EEGetPlayerUnitsOfType((long)a, (long)b, theUnits);
        for (size_t k = 0; k < theUnits.size(); k++)
            ee_ai_next_push_handle(outRecord, EECPNextHandle(theUnits[k]));
        return 0;
    }
    case EE_AI_NEXT_HOST_CITIZEN_QUEUE:
        return (int64_t)((EEComplexUnit*)(uintptr_t)a)->mCitizenBuildQueue.size();
    case EE_AI_NEXT_HOST_IS_HUMAN:
        return ((EEPlayer*)(uintptr_t)a)->IsHuman() ? 1 : 0;
    case EE_AI_NEXT_HOST_SWITCH:
        return (a == 0 ? EECPMilitiaOn(me) : EECPMilitiaTowersOn(me)) ? 1 : 0;
    case EE_AI_NEXT_HOST_DISTANCE2: {
        float theDistance = ((EEComplexUnit*)(uintptr_t)a)->DistanceSquaredToUnit((EEUnit*)(uintptr_t)b);
        uint32_t theBits;
        memcpy(&theBits, &theDistance, sizeof(theBits));
        return (int64_t)theBits;
    }
    case EE_AI_NEXT_HOST_COMPUTER:
        return (int64_t)(uintptr_t)((EEPlayer*)(uintptr_t)a)->mComputerPlayer;
    case EE_AI_NEXT_HOST_AREAS: {
        // the continent manager's areas (serial/kind), its serial counter and its lists' sizes (EE-SIM's dumps)
        char theLine[256];
        snprintf(theLine, sizeof(theLine),
                 "AREAS digest=%08lx last=%lu continents=%zu hills=%zu pathareas=%zu sectors=%zu subsectors=%zu chokepoints=%zu forests=%zu areas=%zu:",
                 EEContinentRegionsDigest(), (unsigned long)gContinentManager.GetLastSerial(), gContinentManager.GetContinents().size(),
                 gContinentManager.GetHills().size(), gContinentManager.GetPathAreas().size(), gContinentManager.GetPathSectors().size(),
                 gContinentManager.GetPathSubSectors().size(), gContinentManager.GetChokePoints().size(), gContinentManager.GetForests().size(),
                 gContinentManager.GetAreas().size());
        ee_ai_next_append(outRecord, (const uint8_t*)theLine, strlen(theLine));
        EEAIListView<EeAStarCmArea> theAreas = gContinentManager.GetAreas();
        for (const EeAStarCmArea* theIt = theAreas.begin(); theIt != theAreas.end(); ++theIt) {
            EEAIArea* theArea = (EEAIArea*)(uintptr_t)theIt->area;
            snprintf(theLine, sizeof(theLine), " %lu/%ld", (unsigned long)theIt->serial,
                     theArea != NULL ? (long)theArea->GetAreaType() : -1L);
            ee_ai_next_append(outRecord, (const uint8_t*)theLine, strlen(theLine));
        }
        ee_ai_next_append(outRecord, (const uint8_t*)"\n", 1);
        return 0;
    }
    case EE_AI_NEXT_HOST_STARTED:
        return me->mIsStarted ? 1 : 0;
    case EE_AI_NEXT_HOST_SIM_HASH: {
        unsigned long theUnits = 0;
        unsigned long theCount = 0;
        for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
            theUnits += static_cast<EEUnit*>(theUnit)->GetChecksum(3);   // hit points, tiles, positions, commands
            theCount++;
        }
        out[0] = (int64_t)(unsigned long)gPhysics->mCurrentTick;
        out[1] = (int64_t)theCount;
        out[2] = (int64_t)theUnits;
        out[3] = (int64_t)(unsigned long)gPhysics->mRandom.GetSeed();
        out[4] = (int64_t)gPhysics->mInputMessagesProcessed;
        return 0;
    }
    case EE_AI_NEXT_HOST_FORMAT_F2: {
        double theValue;
        memcpy(&theValue, &a, sizeof(theValue));
        char theText[64];
        snprintf(theText, sizeof(theText), "%.2f", theValue);
        ee_ai_next_append(outRecord, (const uint8_t*)theText, strlen(theText));
        return 0;
    }
    default:
        return 0;
    }
}
