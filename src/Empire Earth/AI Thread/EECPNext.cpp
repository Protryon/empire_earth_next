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
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEGameLog.h"
#include "ee/EESaveGameFiles.h"
#include "EEDataText.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_*: the engine (next_game.rs), the name tables (names.rs)
#include "rust/sim/units.h"

// ---------------------------------------------------------------------------------------------------------------
// The state (Rust's record)

EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer)
{
    return inPlayer->mNextState != NULL ? inPlayer->mNextState : (inPlayer->mNextState = (EECPNextState*)ee_ai_next_state_new());
}

void EECPNextDeleteState(EEComputerPlayer* inPlayer)
{
    ee_ai_next_state_free(std::exchange(inPlayer->mNextState, nullptr));
}

std::string EECPNextAI(const EEComputerPlayer* inPlayer)
{
    if (inPlayer == NULL || inPlayer->mNextState == NULL)
        return std::string();
    size_t theLength = 0;
    const uint8_t* theAI = ee_ai_next_state_ai(inPlayer->mNextState, &theLength);
    return std::string((const char*)theAI, theLength);
}

// The seats (ee/EECPEngine.h; the manifest and the choices are ee_game cp_engine.rs)
long EECPEngine(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mEngine == kEECPEngineNext ? kEECPEngineNext : kEECPEngineClassic;
}

// the seat's manifest entry (NULL: none; an empty id is in no manifest)
static const EECPAIEntry* EECPEngineEntry(const EEComputerPlayer* inPlayer)
{
    return EECPAIByID(EECPEngineAI(inPlayer));
}

bool EECPEngineSeesAll(const EEComputerPlayer* inPlayer)
{
    return EECPEngineEntry(inPlayer) != NULL && EECPEngineEntry(inPlayer)->sees_all;
}

bool EECPEngineFillsGarrison(const EEComputerPlayer* inPlayer)
{
    return EECPEngineEntry(inPlayer) != NULL && EECPEngineEntry(inPlayer)->fills_garrison;
}

void EECPEngineSet(EEComputerPlayer* inPlayer, unsigned long inChoice)
{
    // no AI (a manifest without a "classic" AI): the seat stands idle (ee_ai_next_prepare logs it)
    inPlayer->mEngine = kEECPEngineNext;
    EECPNextSetAI(inPlayer, EECPAIByChoice(inChoice) != NULL ? EECPAIID(EECPAIByChoice(inChoice)) : std::string("none"));
}

// the engine log's debug output (ee_game cp_engine.rs)
extern "C" void ee_cp_engine_debug_out(const char* inText)
{
    ESDebugOut("%s", inText);
}

bool EECPNextSaveState(EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    return ee_ai_next_save(inPlayer, &outBytes);
}

extern "C" void ee_ai_next_string_append(void* inString, const uint8_t* inData, size_t inLength)
{
    ((std::string*)inString)->append((const char*)inData, inLength);
}

// (the classes' and techs' names by their data ids: ee_ai names_db.rs)

// ---------------------------------------------------------------------------------------------------------------
// The host of the engine (ee_ai next_game.rs EE_AI_NEXT_HOST_*): the computer player's own objects (the placement,
// the resource centres, the danger zones), a goal's target, the continent manager's areas and forests (lane C
// re-points these), the tech nodes' methods with bodies, the trace, the game log and the files


extern "C" int64_t ee_ai_next_host(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    int64_t* out = (int64_t*)outRecord;
    switch (inOp) {
    case EE_AI_NEXT_HOST_NOW:
        return (int64_t)gPhysics->mGameTime;
    case EE_AI_NEXT_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_NEXT_HOST_SET_ATTACK: ((EEComplexUnit*)(uintptr_t)a)->SetGoal(EEUGRust::NewAttackUnit((EEComplexUnit*)(uintptr_t)a, (EEComplexUnit*)(uintptr_t)b, false, false), 0, 0);
        return 0;
    case EE_AI_NEXT_HOST_SET_LOCATION: ((EEComplexUnit*)(uintptr_t)a)->SetGoal(EEUGRust::NewLocation(GE2DPoint((float)b, (float)c), true, true, false, 0.0f, false, false), 0, 0);
        return 0;
    case EE_AI_NEXT_HOST_GAME_LOG_ON:
        return EEGameLogOn() ? 1 : 0;
    case EE_AI_NEXT_HOST_GAME_LOG:
        EEGameLogLine((const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_SIM_MODE:
        return gPlatSimMode ? 1 : 0;
    case EE_AI_NEXT_HOST_ENGINE_LOG:
        EECPEngineLog("%s", (const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_GAME_SEED:
        return (int64_t)gUserInterface->mRandomSeed;
    case EE_AI_NEXT_HOST_CLASSIC_AI:
        if (EECPClassicAI() == NULL)
            return 0;
        EECPNextAppend(outRecord, EECPAIID(EECPClassicAI()));
        return 1;
    case EE_AI_NEXT_HOST_AI_SCRIPT: {
        const EECPAIEntry* theAI = EECPAIByID((const char*)(uintptr_t)a);
        if (theAI == NULL)
            return 0;
        EECPNextAppend(outRecord, EECPAIText(theAI->script, theAI->script_len));
        return 1;
    }
    case EE_AI_NEXT_HOST_UNITS_STATE:
        return (int64_t)(uintptr_t)EECPUnitsRust(me);
    case EE_AI_NEXT_HOST_PENDING_EPOCH:
        return EECPPendingEpoch(me);
    case EE_AI_NEXT_HOST_CAN_GATHER:
        return me->mCanGather ? 1 : 0;
    case EE_AI_NEXT_HOST_RC_COUNT:   // the centres (ee_sim::cp lists.rs)
        return (int64_t)ee_cp_center_count(me);
    case EE_AI_NEXT_HOST_RESOURCE_USERS:
        return (int64_t)gWorld.GetResourceUsers((EEUnit*)(uintptr_t)a);
    case EE_AI_NEXT_HOST_CONTINENT_OF:
        return (int64_t)gContinentManager.GetContinent((EEUnit*)(uintptr_t)a, kAIPathLandMass);
    case EE_AI_NEXT_HOST_NEAREST_AREA:
        return (int64_t)(uintptr_t)((EEPlayer*)(uintptr_t)a)->GetNearestArea(EETileCoordinate((long)b, (long)c), true);
    case EE_AI_NEXT_HOST_UNIT:
        return (int64_t)(uintptr_t)static_cast<RTSUnit*>(gWorld.GetUnit((unsigned long)a));
    case EE_AI_NEXT_HOST_NODE_COST:
        return me->ScaleCost(((EETechTreeNode*)(uintptr_t)a)->GetCost((long)b, (long)c, 1.0f));
    case EE_AI_NEXT_HOST_EPOCH_END:
        return gWorld.mGameOptions.mEpochEnd;
    case EE_AI_NEXT_HOST_LAST_ATTACKER:
        return (int64_t)(uintptr_t)static_cast<RTSUnit*>(((EEComplexUnit*)(uintptr_t)a)->mLastAttacker);
    case EE_AI_NEXT_HOST_STATE:
        return (int64_t)(uintptr_t)EECPNextGetState(me);
    case EE_AI_NEXT_HOST_STATE_OF:
        return (int64_t)(uintptr_t)me->mNextState;
    case EE_AI_NEXT_HOST_DEBUG_OUT:
        ESDebugOut("%s", (const char*)(uintptr_t)a);
        return 0;
    case EE_AI_NEXT_HOST_PLAYER_ID:
        return (int64_t)me->mPlayerID;
    case EE_AI_NEXT_HOST_CITIZEN_QUEUE:
        return (int64_t)((EEComplexUnit*)(uintptr_t)a)->mCitizenBuildQueue.size();
    case EE_AI_NEXT_HOST_IS_HUMAN:
        return ((EEPlayer*)(uintptr_t)a)->IsHuman() ? 1 : 0;
    case EE_AI_NEXT_HOST_COMPUTER:
        return (int64_t)(uintptr_t)((EEPlayer*)(uintptr_t)a)->mComputerPlayer;
    case EE_AI_NEXT_HOST_GAME_LOG_TICK: EEGameLogTick((unsigned long)a);
        return 0;
    case EE_AI_NEXT_HOST_TRACE_HASH_ON:
        return gCPTraceHashOn ? 1 : 0;
    case EE_AI_NEXT_HOST_CP_UPDATE: me->Update((unsigned long)a);
        return 0;
    case EE_AI_NEXT_HOST_STARTED:
        return me->mIsStarted ? 1 : 0;
    case EE_AI_NEXT_HOST_SIM_FIRST_UNIT:
        return (int64_t)(uintptr_t)gWorld.mUnits.mFirst;
    case EE_AI_NEXT_HOST_SIM_NEXT_UNIT:
        return (int64_t)(uintptr_t)((RTSUnit*)(uintptr_t)a)->mTileNode->mNext;
    case EE_AI_NEXT_HOST_SIM_CHECKSUM:   // hit points, tiles, positions, commands
        return (int64_t)(unsigned long)static_cast<EEUnit*>((RTSUnit*)(uintptr_t)a)->GetChecksum(3);
    case EE_AI_NEXT_HOST_SIM_TICK:
        return (int64_t)(unsigned long)gPhysics->mCurrentTick;
    case EE_AI_NEXT_HOST_SIM_SEED:
        return (int64_t)(unsigned long)gPhysics->mRandom.GetSeed();
    case EE_AI_NEXT_HOST_SIM_INPUTS:
        return (int64_t)gPhysics->mInputMessagesProcessed;
    default:
        return 0;
    }
}
