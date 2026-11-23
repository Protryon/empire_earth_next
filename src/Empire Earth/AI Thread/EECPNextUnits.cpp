// The next computer player's per-unit Lua interface (ee/EECPNextUnits.h; docs/AI_LUA_UNITS.md, stage 1). New in this
// port (no original address).
//
// Rules every binding keeps (AI_LUA_UNITS §1):
// - integers only: positions in tiles (x, y) and centi-tiles (px, py), speeds and ranges in centi-tiles;
// - lists sorted by id, or by (distance, id) with `near`;
// - fog-honest: other players' units only while their tile IsSeenBy the player (the reveal cheat lifts it there);
//   resources only on explored tiles (IsVisibleTo);
// - no Lua error while a C++ object with a destructor lives: arguments are read into the player's scratch vectors
//   (members, not locals), bad arguments are refused with a result instead of raised, and only the budget charge
//   (ee_ai lua_host.rs charge) raises, before anything is built;
// - commands run the human's own messages (EEMessage.cpp's Execute) on the computer player's selection, at once
//   (AI_LUA_UNITS §3.2): the selection is filled with the units, the player answers to a borrowed slot id while the
//   message runs, and both are put back.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <utility>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/NEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUGRust.h"
#include "ee/EEPathManager.h"
#include "rust/sim/astar.h"   // the path goals' modifiers (modifier_obj.rs)
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EECPEngine.h"
#include "ee/EEMessage.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EETeam.h"
#include "ee/EECPTrace.h"
#include "ee/EEEvent.h"
#include "ee/EECPBehavior.h"
#include "ee/EECPNext.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPASGFlags.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_units_*: the save block's format (units_save.rs), the bindings' Lua helpers (units_lua.rs)
#include "rust/sim/units.h"
#include "ee/EEUnitRegions.h"

// The state is Rust's (ee_ai units_state.rs: the held set, the events, the forwarding, the enemy memory, the reflexes,
// the limits, the danger layers, the bindings' scratch, ai.wall_route's path requests; units_bind.rs, units_more.rs,
// units_world.rs: its hooks and the bindings): EEComputerPlayer::mUnitsState, made at the bindings' first call
void EECPUnitsDelete(EEComputerPlayer* inPlayer)
{
    ee_ai_units_state_free(std::exchange(inPlayer->mUnitsState, nullptr));
}

// ---------------------------------------------------------------------------------------------------------------
// Events

void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim)
{
    if (inPlayer->mUnitsState != NULL && inVictim != NULL)
        ee_ai_units_on_dealt(inPlayer->mUnitsState, inPlayer, inAmount, (uint64_t)(uintptr_t)static_cast<RTSUnit*>(inVictim));
}

void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState, inDefend ? 9 : 8, inPlayerID, 0, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// The host's helpers for the Rust bindings (ee_ai units_more.rs)

bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);   // EECPWallManager.cpp

// ---------------------------------------------------------------------------------------------------------------
// The registration and the micro step are Rust (ee_ai units_bind.rs ee_ai_units_open, next_game.rs
// ee_ai_next_micro_tick)

void* EECPUnitsRust(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL ? inPlayer->mUnitsState : NULL;
}

// ---------------------------------------------------------------------------------------------------------------
// The save: "EENU" (version 4: the militia and held reflexes; 3: the lost layer's trades; 2: the hurt spots and the
// marks). The state, its format and the save (with its TEST ONLY check) are Rust's (ee_ai units_state.rs,
// units_save.rs, units_bind.rs ee_ai_units_save).

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    ee_ai_units_save(const_cast<EEComputerPlayer*>(inPlayer), EECPUnitsRust(inPlayer), &outBytes);
}

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    return ee_ai_units_load(inPlayer, (const uint8_t*)inBytes.data(), inBytes.size());
}

// ---------------------------------------------------------------------------------------------------------------
// The host of the Rust bindings (ee_ai units_bind.rs EE_AI_UNITS_HOST_*): what the world views do not hold

// a command's message, as the human's: run as it is, or from the borrowed slot inSlot (0: none) (ee_ai units_cases.rs:
// its type, the borrowed slot's sequence)
static EEMRecordValue EECPUnitsValue(const EeAiUnitsMsgValue& inValue)
{
    if (inValue.kind == 2)
        return EEMRecordValue(GE2DPoint(inValue.x, inValue.y));
    if (inValue.kind == 1)
        return EEMRecordValue((double)inValue.x);
    return EEMRecordValue((long long)inValue.i);
}

static void EECPUnitsRun(EEMessage&& inMessage, unsigned char inSlot)
{
    if (inSlot != 0)
        inMessage.SetOriginatingSlot(inSlot);
    inMessage.Execute();
}

static void EECPUnitsExec(unsigned char inType, const EeAiUnitsMessage& m, unsigned char inSlot)
{
    const EeAiUnitsMsgValue* v = m.values;
    switch (m.count) {
    case 0: EECPUnitsRun(EEMRecord(inType), inSlot); break;
    case 1: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0])}), inSlot); break;
    case 2: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1])}), inSlot); break;
    case 3: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2])}), inSlot); break;
    case 4: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3])}), inSlot); break;
    case 5: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]), EECPUnitsValue(v[4])}), inSlot); break;
    case 7: EECPUnitsRun(EEMRecord(inType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]), EECPUnitsValue(v[4]), EECPUnitsValue(v[5]), EECPUnitsValue(v[6])}), inSlot); break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The continent manager and the walls for the Rust bindings (ee_ai units_more.rs): the only reads of the
// continent manager's objects the bindings make (lane C re-points these when the continents move)

// the classic wall manager's area utilities (EECPWallManager.cpp declares them the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// an event effect's kind, unit class and attribute (units_world.rs EE_AI_UNITS_HOST_EFFECT_FIELDS)
static int64_t EECPUnitsEffectBits(const EEEventEffect* inEffect)
{
    return inEffect == NULL ? -1 : (int64_t)(((uint64_t)(inEffect->mKind & 0xffff) << 48) | ((uint64_t)(uint32_t)inEffect->mUnitClass << 16) | (uint64_t)(inEffect->mAttribute & 0xffff));
}

// the units a gate's fit asks about (EE_AI_UNITS_HOST_GATE_FITS: emptied for each side), the tiles of a level test
static std::vector<EEUnit*> sGateUnits;
static std::vector<U2DPoint<long> > sLevelTiles;

// (the world's and the continent manager's reads are Rust's: units_bind.rs world_op)
extern "C" int64_t ee_ai_units_host_cpp(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    switch (inOp) {
    case EE_AI_UNITS_HOST_STATE:
        return (int64_t)(uintptr_t)(me->mUnitsState != NULL ? me->mUnitsState : (me->mUnitsState = ee_ai_units_state_new()));
    case EE_AI_UNITS_HOST_UNIT:
        return a > 0 ? (int64_t)(uintptr_t)static_cast<RTSUnit*>(gWorld.GetUnit((unsigned long)a)) : 0;
    case EE_AI_UNITS_HOST_DETACH:
        me->DetachUnitFromAI((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_UPGRADE_COUNT:
        return (int64_t)((EEPlayer*)(uintptr_t)a)->mUnitClassUpgrades.size();
    case EE_AI_UNITS_HOST_UPGRADE_AT:
        return (int64_t)(uintptr_t)((EEPlayer*)(uintptr_t)a)->mUnitClassUpgrades[(size_t)b];
    case EE_AI_UNITS_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_HUMAN:
        return ((EEPlayer*)(uintptr_t)a)->IsHuman() ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAMS:
        return (int64_t)gWorld.mTeams.size();
    case EE_AI_UNITS_HOST_TEAM_HAS:
        return gWorld.mTeams[(size_t)a].ContainsPlayer((unsigned long)b) ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAM_SIZE:
        return (int64_t)gWorld.mTeams[(size_t)a].GetNumPlayers();
    case EE_AI_UNITS_HOST_TEAM_PLAYER:
        return (int64_t)gWorld.mTeams[(size_t)a].GetPlayersAt((unsigned long)b);
    case EE_AI_UNITS_HOST_GATHER_GOAL:
        return (int64_t)(uintptr_t)EEGatherGoalCast(((EEComplexUnit*)(uintptr_t)a)->GetGoalState());
    case EE_AI_UNITS_HOST_GATHER_RESOURCE:
        return (int64_t)(uintptr_t)static_cast<RTSUnit*>(reinterpret_cast<EEUGRust*>((uintptr_t)a)->GetGatherResource());
    case EE_AI_UNITS_HOST_ACTIVE_GROUP:
        return (int64_t)(uintptr_t)me->mActiveGroup;
    case EE_AI_UNITS_HOST_GROUP_CLEAR: me->mActiveGroup->Clear();
        return 0;
    case EE_AI_UNITS_HOST_SELECT_ADD:
        me->mActiveGroup->Add((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_EXEC_RUN: EECPUnitsExec((unsigned char)a, *(const EeAiUnitsMessage*)outRecord, (unsigned char)b);
        return 0;
    case EE_AI_UNITS_HOST_SLOT:
        return me->mSlot;
    case EE_AI_UNITS_HOST_SET_SLOT: me->mSlot = (unsigned char)a;
        return 0;
    case EE_AI_UNITS_HOST_SLOT_IS_ME:
        return gWorld.GetPlayerBySlot((unsigned char)a) == me ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_PRODUCE:
        return me->EEPlayer::CanProduce((long)a, 0x20, 1, true, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_PLACE:
        return ee_units_uu_can_place_unit_on_tile((uint64_t)(uintptr_t)me, 0, (long)a, (long)b, (long)c, false, 1, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_LAND_BUILDING:
        return ((EEUnitClass*)(uintptr_t)a)->IsLandBuilding() ? 1 : 0;
    case EE_AI_UNITS_HOST_BEHAVIOR_OK:
        return a >= 0 && (unsigned long)a < EEUnitBehavior::sTable.size() && EEUnitBehavior::sTable[(unsigned long)a] != NULL;
    case EE_AI_UNITS_HOST_HAS_NODE:
        return me->mTechnologies->Get((long)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_SEES_ALL:
        return EECPEngineSeesAll(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_HAS_TREE:
        return me->mTechnologies != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_NODE_OF:
        return (int64_t)(uintptr_t)me->mTechnologies->Get((long)a);
    case EE_AI_UNITS_HOST_CAN_AFFORD:
        return me->CanAfford((EETechTreeNode*)(uintptr_t)a, 0x20, 1) ? 1 : 0;
    case EE_AI_UNITS_HOST_FLIES_OVER:
        return ((EEUnitClass*)(uintptr_t)a)->FliesOver() ? 1 : 0;
    case EE_AI_UNITS_HOST_POP_COST:
        return ((EEUnitClass*)(uintptr_t)a)->GetPopulationCost();
    case EE_AI_UNITS_HOST_DAMAGES:
        return EEDamageFactor((long)a, (long)b) != 0.0f ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAM_INDEX:
        return (int64_t)((EEPlayer*)(uintptr_t)a)->GetTeamIndex();
    case EE_AI_UNITS_HOST_WONDER_COUNT:
        return (int64_t)((EEPlayer*)(uintptr_t)a)->GetNumWonderUnits();
    case EE_AI_UNITS_HOST_NODE_COST:
        return me->ScaleCost(((EETechTreeNode*)(uintptr_t)a)->GetCost((long)b, (long)c, 1.0f));
    case EE_AI_UNITS_HOST_SCALE_COST:
        return me->ScaleCost((long)a);
    case EE_AI_UNITS_HOST_NODE_AVAILABLE:
        return ((EETechTreeNode*)(uintptr_t)a)->GetIsAvailable() ? 1 : 0;
    case EE_AI_UNITS_HOST_NODE_OBSOLETE:
        return ((EETechTreeNode*)(uintptr_t)a)->GetIsObsolete() ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_PRODUCE_AS:
        return me->CanProduce((long)a, (long)b, 1, c != 0, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_EPOCH_ID:
        return EEIsEpochID((long)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_EPOCH_COUNT:
        return EEEpochCount();
    case EE_AI_UNITS_HOST_NODE_HAS_DATA:
        return ((EETechTreeNode*)(uintptr_t)a)->mData != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_NODE_COST_AT:
        return ((EETechTreeNode*)(uintptr_t)a)->mData->mCosts[kResourceFood + (long)b];
    case EE_AI_UNITS_HOST_CLASS_IMPROVEMENTS:
        return (int64_t)(uintptr_t)gUnitClassTable[(long)a]->mImprovements;
    case EE_AI_UNITS_HOST_IMPROVE_MAX:
        return ((EEUnitClassImprovementTable*)(uintptr_t)a)->GetMaxLevel();
    case EE_AI_UNITS_HOST_IMPROVE_FACTOR: *(float*)outRecord = ((EEUnitClassImprovementTable*)(uintptr_t)a)->mEntries[(long)b].mCostFactor;
        return 0;
    case EE_AI_UNITS_HOST_IMPROVE_LEVEL:
        return me->GetImprovementLevel((unsigned long)a, (long)b);
    case EE_AI_UNITS_HOST_COST_SCALE: *(float*)outRecord = ((EEUnitClassUpgrade*)(uintptr_t)a)->mCostScale;
        return 0;
    case EE_AI_UNITS_HOST_CAN_BUY:
        return me->CanBuy((long)a, (long)b, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_GATE_CLASS:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>((EEUnitClass*)(uintptr_t)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_STANCE:
        gWorld.SetDiplomaticStance(me->mPlayerID, (long)a, (long)b);
        return 0;
    case EE_AI_UNITS_HOST_ADD_GARRISON:
        return ((EEComplexUnit*)(uintptr_t)a)->AddGarrison((long)b, (long)c) ? 1 : 0;
    case EE_AI_UNITS_HOST_FILLS_GARRISON:
        return EECPEngineFillsGarrison(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_GOAL_LOCATION: reinterpret_cast<EEUGRust*>((uintptr_t)a)->GetGoalLocation(*(GE3DPoint*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_MOVES:
        return !((EEComplexUnit*)(uintptr_t)a)->mUnitClass->IsBuilding() && ((EEComplexUnit*)(uintptr_t)a)->mUnitClass->GetMaxVelocity() > 0.0f ? 1 : 0;
    case EE_AI_UNITS_HOST_BEHAVIORS:
        return (int64_t)EEUnitBehavior::sTable.size();
    case EE_AI_UNITS_HOST_BEHAVIOR_AT:
        return (int64_t)(uintptr_t)EEUnitBehavior::sTable[(size_t)a];
    case EE_AI_UNITS_HOST_GATE_FITS:
        return EEDoesWallGateFitOnUnit((EEComplexUnit*)(uintptr_t)a, (long)b, (long)c, sGateUnits = std::vector<EEUnit*>()) ? 1 : 0;
    case EE_AI_UNITS_HOST_GATE_WALL:
        return ((EEUnit*)(uintptr_t)a)->GetUnitClass()->GetIsWall() && ((EEUnit*)(uintptr_t)a)->GetHitPoints() > 0 && ((EEUnit*)(uintptr_t)a)->mPlayer == me && ((EEUnit*)(uintptr_t)a)->GetComplexUnit() != NULL && ((EEUnit*)(uintptr_t)a)->GetComplexUnit()->GetIsBuilt() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_DEATHMATCH:
        return me->IsDeathmatch() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_RESOURCES:
        return gWorld.mGameOptions.mResources;
    case EE_AI_UNITS_HOST_T_EPOCH_START:
        return gWorld.mGameOptions.mEpochStart;
    case EE_AI_UNITS_HOST_T_EPOCH_END:
        return gWorld.mGameOptions.mEpochEnd;
    case EE_AI_UNITS_HOST_T_SPACE:
        return gWorld.mMap != NULL && gWorld.mMap->mTerrainMesh.GetIsSpaceMap() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_POP_LIMIT:
        return me->EEPlayer::GetPopulationLimit(0);
    case EE_AI_UNITS_HOST_T_BEHAVIORS:
        return (int64_t)(uintptr_t)gCPBehaviors;
    case EE_AI_UNITS_HOST_TILES_LEVEL:
        return EEAreTilesLevel(sLevelTiles = std::vector<U2DPoint<long> >((const U2DPoint<long>*)((EeAiUnitsTiles*)outRecord)->xy, (const U2DPoint<long>*)((EeAiUnitsTiles*)outRecord)->xy + ((EeAiUnitsTiles*)outRecord)->len)) ? 1 : 0;
    case EE_AI_UNITS_HOST_NODE_EVENT:
        return ((EETechTreeNode*)(uintptr_t)a)->mData->mResearchedEvent;
    case EE_AI_UNITS_HOST_EVENT_EFFECTS:
        return EEEvent::Get((long)a) != NULL ? (int64_t)EEEvent::Get((long)a)->GetNumEffects() : -1;
    case EE_AI_UNITS_HOST_EVENT_EFFECT:
        return EEEvent::Get((long)a)->GetEffect((unsigned long)b);
    case EE_AI_UNITS_HOST_EFFECT_FIELDS:
        return EECPUnitsEffectBits(EEEvent::GetEventEffect((unsigned long)a));
    case EE_AI_UNITS_HOST_TAKE_RESOURCE: ((EEPlayer*)(uintptr_t)a)->TakeResource((long)b, (long)c);
        return 0;
    case EE_AI_UNITS_HOST_ADD_RESOURCE: ((EEPlayer*)(uintptr_t)a)->AddResource((long)b, (long)c);
        return 0;
    case EE_AI_UNITS_HOST_ADD_TRIBUTE: ((EEPlayer*)(uintptr_t)a)->AddTribute((long)(b >> 32), (long)(int32_t)b, (long)c);
        return 0;
    default:
        return 0;
    }
}
