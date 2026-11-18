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
//   (EECPLuaState::Charge) raises, before anything is built;
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
#include "lle/NEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMapDimensions.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUGRust.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
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
#include "ee/EECPCombatManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEMessageID.h"
#include "ee/EECPTrace.h"
#include "ee/EEGameLog.h"
#include "ee/EECPLua.h"
#include "ee/EEEvent.h"
#include "ee/EEUGRust.h"
#include "ee/EECPBehavior.h"
#include "ee/EERandomMapIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EELuaApi.h"
#include "ee/EECPNext.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_units_*: the save block's format (units_save.rs), the bindings' Lua helpers (units_lua.rs)
#include "rust/sim/units.h"
#include "ee/EEUnitRegions.h"

// ---------------------------------------------------------------------------------------------------------------
// The state

namespace {

const long kMicroMs = 250;           // the micro step's period (game ms)
const int64_t kMicroBudget = 300000;
const unsigned char kBorrowedSlot = 0xfd;   // the slot id a computer player answers to while its order runs

}  // namespace

// The state is Rust's (ee_ai units_state.rs: the held set, the events, the forwarding, the enemy memory, the reflexes,
// the limits, the danger layers, the bindings' scratch; units_bind.rs, units_more.rs: its hooks and the bindings).
// The C++ keeps ai.wall_route's path requests (C++ objects, not saved).
struct EECPUnitsState {
    void* mRust = ee_ai_units_state_new();
    // ai.wall_route's path requests (the path manager's, held until read; not saved: a loaded game forgets them)
    std::map<long, EEAutoPathRequest*> mRoutes;
    long mNextRoute = 1;
    ~EECPUnitsState()
    {
        for (std::map<long, EEAutoPathRequest*>::iterator it = mRoutes.begin(); it != mRoutes.end(); ++it)
            if (it->second != NULL)
                it->second->Dereference();
        ee_ai_units_state_free(mRust);
    }
};

static uint64_t EECPUnitsHandle(const RTSUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

static EECPUnitsState* EECPUnitsGet(EEComputerPlayer* inPlayer)
{
    if (inPlayer->mUnitsState == NULL)
        inPlayer->mUnitsState = new EECPUnitsState();
    return inPlayer->mUnitsState;
}

void EECPUnitsDelete(EEComputerPlayer* inPlayer)
{
    delete inPlayer->mUnitsState;
    inPlayer->mUnitsState = NULL;
}

// ---------------------------------------------------------------------------------------------------------------
// The held set and the reflexes

bool EECPUnitsHoldsID(const EEComputerPlayer* inPlayer, long inID)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && ee_ai_units_holds(inPlayer->mUnitsState->mRust, inID);
}

bool EECPUnitsHolds(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit)
{
    return inUnit != NULL && EECPUnitsHoldsID(inPlayer, (long)inUnit->mUnitID);
}

void EECPUnitsHeld(const EEComputerPlayer* inPlayer, std::vector<long>& outIDs)
{
    outIDs.clear();
    if (inPlayer == NULL || inPlayer->mUnitsState == NULL)
        return;
    void* s = inPlayer->mUnitsState->mRust;
    outIDs.resize(ee_ai_units_held(s, NULL, 0));
    ee_ai_units_held(s, outIDs.data(), outIDs.size());
}

bool EECPUnitsReflexOn(const EEComputerPlayer* inPlayer, const char* inName)
{
    if (inPlayer == NULL || inPlayer->mUnitsState == NULL)
        return strcmp(inName, "held") != 0;
    return ee_ai_units_reflex_on(inPlayer->mUnitsState->mRust, inName);
}

// ---------------------------------------------------------------------------------------------------------------
// Events

void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim)
{
    if (inPlayer->mUnitsState != NULL && inVictim != NULL)
        ee_ai_units_on_dealt(inPlayer->mUnitsState->mRust, inPlayer, inAmount, EECPUnitsHandle(inVictim));
}

void EECPUnitsOnDamage(EEComputerPlayer* inPlayer, long inAttackerPlayer, long inAmount, EEUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_damage(inPlayer->mUnitsState->mRust, inPlayer, inAttackerPlayer, inAmount, EECPUnitsHandle(inUnit));
}

void EECPUnitsOnPlaced(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 3, EECPUnitsHandle(inUnit));   // created
}

void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState->mRust, inDefend ? 9 : 8, inPlayerID, 0, 0);
}

void EECPUnitsOnFlare(EEComputerPlayer* inPlayer, long inPlayerID, long inX, long inY)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState->mRust, 10, inPlayerID, inX, inY);
}

void EECPUnitsOnCompleted(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 4, EECPUnitsHandle(inUnit));   // built
}

void EECPUnitsOnTaken(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit, bool inSwapped)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL && !inSwapped)
        ee_ai_units_on_taken(inPlayer->mUnitsState->mRust, inPlayer, EECPUnitsHandle(inUnit));
}

void EECPUnitsOnGoalDone(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 5, EECPUnitsHandle(inUnit));   // idle
}

void EECPUnitsOnSwapped(EEComputerPlayer* inPlayer, std::vector<EEPUnitSwap>& ioSwaps)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    long theNow = (long)gPhysics->mGameTime;
    for (unsigned long k = 0; k < ioSwaps.size(); k++) {
        if (ioSwaps[k].mOld == NULL || ioSwaps[k].mNew == NULL)
            continue;
        if (ioSwaps[k].mOld->mPlayer != inPlayer && ioSwaps[k].mNew->mPlayer != inPlayer)
            continue;
        ee_ai_units_on_swap(s->mRust, (long)ioSwaps[k].mOld->mUnitID, (long)ioSwaps[k].mNew->mUnitID,
            ioSwaps[k].mNew->mPlayer == inPlayer, ioSwaps[k].mNew->mUnitClass->mUnitType, theNow);
    }
    ee_ai_units_forward_expire(s->mRust, theNow);
}

// ---------------------------------------------------------------------------------------------------------------
// The bindings (ee_ai units_bind.rs, units_more.rs)

bool EECPUnitsUsed(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && ee_ai_units_used(inPlayer->mUnitsState->mRust);
}

// ---------------------------------------------------------------------------------------------------------------
// The host's helpers for the Rust bindings (ee_ai units_more.rs)

// the classic research manager's citizen test (EECPResearchManager::ChooseResearch): a tech whose researched event
// improves a citizen's gathering (effect kind 2 on the citizen classes, attributes 0x1a-0x1d, 0x1f-0x21)
static bool EECPUnitsCitizenUpgrade(EETechTreeNode* inNode)
{
    EEEvent* theEvent = EEEvent::Get(inNode->mData->mResearchedEvent);
    if (theEvent == NULL)
        return false;
    for (unsigned long e = 0; e < theEvent->GetNumEffects(); e++) {
        EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(e));
        if (theEffect != NULL && theEffect->mKind == 2 && (theEffect->mUnitClass == 0x20 || theEffect->mUnitClass == 0x1e0)
            && (theEffect->mAttribute == 0x1a || theEffect->mAttribute == 0x1b || theEffect->mAttribute == 0x1c
                || theEffect->mAttribute == 0x1d || theEffect->mAttribute == 0x20 || theEffect->mAttribute == 0x21
                || theEffect->mAttribute == 0x1f))
            return true;
    }
    return false;
}

bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);   // EECPWallManager.cpp

// the next level's cost of a class's improvement, as the improve verb pays it (ai.improve_cost): false for no node
// now or no such improvement
static bool EECPUnitsImproveCost(EEComputerPlayer* inPlayer, long inType, long inImprovement, EeAiUnitsImproveCost& outCost)
{
    if (inType < 0 || (unsigned long)inType >= EEUnitClassCount() || gUnitClassTable[inType] == NULL
        || inPlayer->mTechnologies == NULL)
        return false;
    EETechTreeNode* theNode = inPlayer->mTechnologies->Get(gUnitClassTable[inType]->mTechnology);
    if (theNode == NULL || theNode->mData == NULL || !theNode->GetIsAvailable() || theNode->GetIsObsolete())
        return false;
    EEUnitClass* theClass = gUnitClassTable[inType];
    if (inImprovement < 0 || inImprovement >= 16 || theClass->mImprovements == NULL)
        return false;
    EEUnitClassUpgrade* theUpgrade = inPlayer->mUnitClassUpgrades[inType];
    outCost.level = inPlayer->GetImprovementLevel(inType, inImprovement);
    outCost.max = theClass->mImprovements->GetMaxLevel();
    float theFactor = theClass->mImprovements->mEntries[inImprovement].mCostFactor * (float)outCost.level;
    for (long i = 0; i < 5; i++)
        outCost.cost[i] = inPlayer->ScaleCost(UMFloatToLong((float)theNode->mData->mCosts[kResourceFood + i] * theUpgrade->mCostScale * theFactor));
    return true;
}

// the side (1..4) a gate fits on over the player's finished wall at the tile, as the classic wall manager picks it;
// 0: none
static long EECPUnitsGateSide(EEComputerPlayer* me, long x, long y)
{
    std::vector<EEUnit*> theUnits;
    EEGetUnitsOnTile(x, y, theUnits);
    EEUnit* theWall = NULL;
    for (unsigned long k = 0; k < theUnits.size() && theWall == NULL; k++)
        if (theUnits[k]->GetUnitClass()->GetIsWall() && theUnits[k]->GetHitPoints() > 0 && theUnits[k]->mPlayer == me
            && theUnits[k]->GetComplexUnit() != NULL && theUnits[k]->GetComplexUnit()->GetIsBuilt())
            theWall = theUnits[k];
    if (theWall == NULL)
        return 0;
    long theWallType = theWall->GetUnitClass()->GetUnitType();
    for (long side = 1; side <= 4; side++) {
        theUnits.clear();
        if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), side, theWallType, theUnits))
            return side;
    }
    return 0;
}

// the classic build manager's HasEnemyNear through the fog (the manifest's "sees_all"): every armed unit not the
// player's in the influence regions round the tile (one tile on small island and planet maps), idle searchers left
// out -> their count
static long EECPUnitsEnemyNearAll(EEComputerPlayer* me, long inX, long inY)
{
    std::vector<EEComplexUnit*> theUnits;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    EEMap* theMap = gWorld.mMap;
    U2DRectangle<long> theRect;
    if (theTable == NULL)
        return 0;
    if (((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap
            || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)
        && theMap->mL195610 <= 0x2a) {
        theRect.mLeft = inX - 1;
        theRect.mRight = inX + 1;
        theRect.mTop = inY - 1;
        theRect.mBottom = inY + 1;
    } else {
        long theSize = theTable->mRegionSize;
        theRect.mLeft = inX - theSize;
        theRect.mRight = inX + theSize;
        theRect.mTop = inY - theSize;
        theRect.mBottom = theSize + inY;
    }
    long theCount = theTable->SumEnemyValue(me->mPlayerID, theTable->GetRegions(theRect), true);
    if (theCount <= 0)
        return theCount;
    EECollectRegionUnits(theMap->mUnitRegions, theRect, theUnits);
    theCount = 0;
    for (size_t i = 0; i < theUnits.size(); i++) {
        EEComplexUnit* u = theUnits[i];
        if (u->mPlayer != me && u->mStats->mAttack != 0.0f && !(u->mTargetSearchTimer > 60000))
            theCount++;
    }
    return theCount;
}

// ai.tuning's values beyond the computer player's record: the game's options, the map, the continents, the
// manifest's switches, the cheats, the personality with the TEST ONLY experiments' switches, the behaviour table
static void EECPUnitsTuning(EEComputerPlayer* me, EeAiUnitsTuning& t)
{
    t.deathmatch = me->IsDeathmatch();
    t.all_managers = gWorld.mGameOptions.mResources == 5;
    t.epoch_start = gWorld.mGameOptions.mEpochStart;
    t.epoch_end = gWorld.mGameOptions.mEpochEnd;
    t.space = gWorld.mMap != NULL && gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    t.water_tiles = gContinentManager.GetNumWaterTiles();
    t.land_tiles = gContinentManager.GetNumLandTiles();
    t.space_tiles = gContinentManager.GetNumSpaceTiles();
    t.largest_sea = gContinentManager.GetLargestContinent(kContinentCategoryWater);
    t.fills_garrison = EECPEngineFillsGarrison(me);
    t.sees_all = EECPEngineSeesAll(me);
    t.population_limit = me->EEPlayer::GetPopulationLimit(0);
    for (long k = 0; k < 7; k++)
        t.cheats[k] = EECPCheatOn(me, 1L << k);
    const EEAIPersonalityRule& p = EECPPersonality(me);
    snprintf(t.id, sizeof(t.id), "%s", p.mID.c_str());
    t.epoch_pressure_start = p.mEpochPressureStart;
    t.epoch_pressure_full = p.mEpochPressureFull;
    t.stone_span = EECPStoneSpan(me);
    t.safe_bonus = EECPSafeBonus(me);
    t.surplus = p.mSurplus;
    t.surplus_cut = p.mSurplusCut;
    t.low_stock = p.mLowStock;
    for (long k = 0; k < 5; k++)
        t.floor[k] = p.mFloor[k];
    t.max_citizen_share = p.mMaxCitizenShare;
    t.wood_slots = p.mWoodSlots;
    t.moves_per_cycle = p.mMovesPerCycle;
    t.demand_split = EECPDemandSplitOn(me);
    t.coast_fishing = EECPCoastFishingOn(me);
    t.forage_patches = EECPForagePatchesOn(me);
    t.claim_range = EECPClaimRangeOn(me);
    t.claim_needed = EECPClaimNeededOn(me);
    t.early_town_centers = EECPEarlyTownCenters(me);
    t.early_trainers = EECPEarlyTrainers(me);
    t.patch_expand = EECPPatchExpandOn(me);
    t.standing_army = p.mStandingArmy;
    t.army_floor = p.mArmyFloor;
    t.militia = EECPMilitiaOn(me);
    t.militia_towers = EECPMilitiaTowersOn(me);
    t.prehistoric_army = EECPPrehistoricArmy(me);
    t.prehistoric_peace = EECPPrehistoricPeaceOn(me);
    t.stone_peace = EECPStonePeaceOn(me);
    t.naval_flee = EECPNavalFleeOn(me);
    t.epoch_advance_time_scale = gCPBehaviors[kCPBehaviorEpochAdvanceTimeScale].mFloatValue;
    const EECPBehavior& f = gCPBehaviors[kCPBehaviorCarrierFightersToMaintain];
    const EECPBehavior& d = gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM];
    long theFighters[6] = {f.mIntValue1, f.mIntValue2, (long)f.mFloatValue, d.mIntValue1, d.mIntValue2, (long)d.mFloatValue};
    for (long k = 0; k < 6; k++)
        t.carrier_fighters[k] = theFighters[k];
    t.population_cap_own_weight = gCPBehaviors[kCPBehaviorPopulationCapOwnWeight].mFloatValue;
    t.population_cap_other_weight = gCPBehaviors[kCPBehaviorPopulationCapOtherWeight].mFloatValue;
}

// ---------------------------------------------------------------------------------------------------------------
// Opening, the micro step

static const luaL_Reg kUnitsFunctions[] = {
    {"me", (lua_CFunction)ee_ai_units_lua_me}, {"time", (lua_CFunction)ee_ai_units_lua_time}, {"players", (lua_CFunction)ee_ai_units_lua_players}, {"map", (lua_CFunction)ee_ai_units_lua_map},
    {"units", (lua_CFunction)ee_ai_units_lua_units}, {"unit", (lua_CFunction)ee_ai_units_lua_unit}, {"get", (lua_CFunction)ee_ai_units_lua_get}, {"seen", (lua_CFunction)ee_ai_units_lua_seen}, {"memory", (lua_CFunction)ee_ai_units_lua_memory},
    {"resources", (lua_CFunction)ee_ai_units_lua_resources}, {"tile", (lua_CFunction)ee_ai_units_lua_tile}, {"class", (lua_CFunction)ee_ai_units_lua_class}, {"can_hurt", (lua_CFunction)ee_ai_units_lua_can_hurt}, {"wonders", (lua_CFunction)ee_ai_units_lua_wonders}, {"events", (lua_CFunction)ee_ai_units_lua_events},
    {"can_place", (lua_CFunction)ee_ai_units_lua_can_place}, {"reach", (lua_CFunction)ee_ai_units_lua_reach}, {"strength", (lua_CFunction)ee_ai_units_lua_strength},
    {"move", (lua_CFunction)ee_ai_units_lua_move}, {"attack", (lua_CFunction)ee_ai_units_lua_attack}, {"attack_ground", (lua_CFunction)ee_ai_units_lua_attack_ground}, {"gather", (lua_CFunction)ee_ai_units_lua_gather},
    {"build", (lua_CFunction)ee_ai_units_lua_build}, {"repair", (lua_CFunction)ee_ai_units_lua_repair}, {"garrison", (lua_CFunction)ee_ai_units_lua_garrison}, {"load", (lua_CFunction)ee_ai_units_lua_load}, {"unload", (lua_CFunction)ee_ai_units_lua_unload},
    {"train", (lua_CFunction)ee_ai_units_lua_train}, {"research", (lua_CFunction)ee_ai_units_lua_research}, {"stance", (lua_CFunction)ee_ai_units_lua_stance}, {"stop", (lua_CFunction)ee_ai_units_lua_stop},
    {"explore", (lua_CFunction)ee_ai_units_lua_explore}, {"delete", (lua_CFunction)ee_ai_units_lua_delete}, {"take", (lua_CFunction)ee_ai_units_lua_take}, {"release", (lua_CFunction)ee_ai_units_lua_release},
    {"reflex", (lua_CFunction)ee_ai_units_lua_reflex}, {"test_spawn", (lua_CFunction)ee_ai_units_lua_test_spawn}, {"test_town", (lua_CFunction)ee_ai_units_lua_test_town},
    {"danger", (lua_CFunction)ee_ai_units_lua_danger}, {"danger_box", (lua_CFunction)ee_ai_units_lua_danger_box}, {"mark", (lua_CFunction)ee_ai_units_lua_mark}, {"route", (lua_CFunction)ee_ai_units_lua_route},
    {"find_site", (lua_CFunction)ee_ai_units_lua_find_site}, {"heal", (lua_CFunction)ee_ai_units_lua_heal}, {"rally", (lua_CFunction)ee_ai_units_lua_rally}, {"cancel", (lua_CFunction)ee_ai_units_lua_cancel},
    {"improve", (lua_CFunction)ee_ai_units_lua_improve}, {"improve_cost", (lua_CFunction)ee_ai_units_lua_improve_cost}, {"formation", (lua_CFunction)ee_ai_units_lua_formation}, {"pack", (lua_CFunction)ee_ai_units_lua_pack}, {"dropoff", (lua_CFunction)ee_ai_units_lua_dropoff},
    {"farm", (lua_CFunction)ee_ai_units_lua_farm}, {"cast", (lua_CFunction)ee_ai_units_lua_cast}, {"replant", (lua_CFunction)ee_ai_units_lua_replant},
    {"path_len", (lua_CFunction)ee_ai_units_lua_path_len}, {"drop_site", (lua_CFunction)ee_ai_units_lua_drop_site}, {"shore", (lua_CFunction)ee_ai_units_lua_shore}, {"water_sites", (lua_CFunction)ee_ai_units_lua_water_sites}, {"area", (lua_CFunction)ee_ai_units_lua_area}, {"garrison_fill", (lua_CFunction)ee_ai_units_lua_garrison_fill}, {"enemy_force", (lua_CFunction)ee_ai_units_lua_enemy_force}, {"player_file", (lua_CFunction)ee_ai_units_lua_player_file}, {"area_point", (lua_CFunction)ee_ai_units_lua_area_point}, {"forest_point", (lua_CFunction)ee_ai_units_lua_forest_point}, {"hill_point", (lua_CFunction)ee_ai_units_lua_hill_point}, {"choke_point", (lua_CFunction)ee_ai_units_lua_choke_point}, {"wall_path", (lua_CFunction)ee_ai_units_lua_wall_path}, {"wall_route", (lua_CFunction)ee_ai_units_lua_wall_route}, {"wall_route_result", (lua_CFunction)ee_ai_units_lua_wall_route_result}, {"wall_line", (lua_CFunction)ee_ai_units_lua_wall_line}, {"wall_stretches", (lua_CFunction)ee_ai_units_lua_wall_stretches}, {"enemy_near", (lua_CFunction)ee_ai_units_lua_enemy_near}, {"unexplored", (lua_CFunction)ee_ai_units_lua_unexplored},
    {"config", (lua_CFunction)ee_ai_units_lua_config}, {"used", (lua_CFunction)ee_ai_units_lua_used},
    {"wall", (lua_CFunction)ee_ai_units_lua_wall}, {"gate", (lua_CFunction)ee_ai_units_lua_gate}, {"lock", (lua_CFunction)ee_ai_units_lua_lock}, {"tribute", (lua_CFunction)ee_ai_units_lua_tribute}, {"trade", (lua_CFunction)ee_ai_units_lua_trade},
    {"diplomacy", (lua_CFunction)ee_ai_units_lua_diplomacy}, {"patrol", (lua_CFunction)ee_ai_units_lua_patrol},
    {"tuning", (lua_CFunction)ee_ai_units_lua_tuning}, {"techs", (lua_CFunction)ee_ai_units_lua_techs}, {"epoch", (lua_CFunction)ee_ai_units_lua_epoch}, {"gatherers", (lua_CFunction)ee_ai_units_lua_gatherers},
    {"place", (lua_CFunction)ee_ai_units_lua_place}, {"site_blocked", (lua_CFunction)ee_ai_units_lua_site_blocked}, {"build_unsafe", (lua_CFunction)ee_ai_units_lua_build_unsafe},
    {NULL, NULL}};
static const size_t kUnitsFunctionCount = sizeof(kUnitsFunctions) / sizeof(kUnitsFunctions[0]) - 1;

// TEST ONLY (EE_AI_BINDING_TRACE, ee_ai binding_trace.rs): each binding's arguments and results, the port's golden
template <size_t K>
static int LuaTraced(lua_State* L)
{
    ee_ai_binding_trace_enter(L, kUnitsFunctions[K].name);
    int theResults = kUnitsFunctions[K].func(L);
    ee_ai_binding_trace_leave(L, kUnitsFunctions[K].name, theResults);
    return theResults;
}

template <size_t... K>
static void EECPUnitsTracedTable(lua_CFunction* outTable, std::index_sequence<K...>)
{
    ((outTable[K] = LuaTraced<K>), ...);
}

static void EECPUnitsTraceOut(const char* inText)
{
    ESDebugOut("%s", inText);
}

void EECPUnitsOpen(lua_State* L, void* inPlayer)
{
    EEComputerPlayer* thePlayer = (EEComputerPlayer*)inPlayer;
    EECPUnitsGet(thePlayer);
    // the host records as this build's header has them (a stale rust/sim/ai.h is refused)
    if (ee_ai_units_records_size(NULL, NULL, NULL, NULL, NULL, NULL) != sizeof(EeAiUnitsImproveCost) + sizeof(EeAiUnitsPlace)
            + sizeof(EeAiUnitsTuning) + sizeof(EeAiUnitsArea) + sizeof(EeAiUnitsAreaPick) + sizeof(EeAiUnitsTiles))
        abort();
    static lua_CFunction sTraced[kUnitsFunctionCount] = {};
    bool theTrace = ee_ai_binding_trace_on();
    if (theTrace && sTraced[0] == NULL) {
        ee_ai_binding_trace_set_out(EECPUnitsTraceOut);
        EECPUnitsTracedTable(sTraced, std::make_index_sequence<kUnitsFunctionCount>());
    }
    for (size_t k = 0; k < kUnitsFunctionCount; k++) {
        lua_pushlightuserdata(L, inPlayer);
        lua_pushcclosure(L, theTrace ? sTraced[k] : kUnitsFunctions[k].func, 1);
        lua_setfield(L, -2, kUnitsFunctions[k].name);
    }
}

static void EECPUnitsMicroError(EEComputerPlayer* inPlayer, EECPLuaState* inLua)
{
    std::string theStatus = EECPLuaStatusName(inLua->LastStatus());
    std::replace(theStatus.begin(), theStatus.end(), ' ', '_');
    std::string theMessage = inLua->LastError();
    std::replace(theMessage.begin(), theMessage.end(), '\n', '|');
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.error", "in=micro status=%s msg=%s", theStatus.c_str(), theMessage.c_str());
    }
    if (gPlatSimMode) {
        fprintf(stderr, "SIM next.error p%lu t=%lu in=micro status=%s msg=%s\n", inPlayer->mPlayerID, gPhysics->mGameTime,
            theStatus.c_str(), theMessage.c_str());
        fflush(stderr);
    }
}

void EECPUnitsMicroTick(unsigned long inTime)
{
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
        if (theCP == NULL || theCP->mUnitsState == NULL || theCP->mNextState == NULL || !theCP->mActive)
            continue;
        void* s = theCP->mUnitsState->mRust;
        ee_ai_units_second(s, theCP, (long)inTime);   // the enemy memory and the danger layers, once a second
        EECPNextState* theNext = theCP->mNextState;
        long theMicro = ee_ai_units_micro_next(s);
        if (theNext->mLua == NULL || !theNext->mStarted || theNext->mFailed || theMicro == 0x7fffffff)
            continue;
        if (theMicro < 0)
            theMicro = (long)inTime + kMicroMs;
        if ((long)inTime < theMicro) {
            ee_ai_units_set_micro_next(s, theMicro);
            continue;
        }
        while (theMicro <= (long)inTime)
            theMicro += kMicroMs;
        ee_ai_units_set_micro_next(s, theMicro);
        EECPLuaValue theArg = EECPLuaValue::NewMap(), theResult;
        theArg.Set("time", EECPLuaValue::Int((int64_t)inTime));
        bool theFound = false;
        if (!theNext->mLua->CallOptional("micro", theArg, theResult, kMicroBudget, theFound))
            EECPUnitsMicroError(theCP, theNext->mLua);
        else if (!theFound)
            ee_ai_units_set_micro_next(s, 0x7fffffff);   // the script has no micro
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The save: "EENU" (version 4: the militia and held reflexes; 3: the lost layer's trades; 2: the hurt spots and the
// marks). The state and its format are Rust's (ee_ai units_state.rs, units_save.rs).

// TEST ONLY (EE_AI_SAVE_CHECK=1, as EECPNextSaveCheck): the bytes decode into a fresh state that encodes to the same
// bytes; `next.savecheck units=same|DIFFERENT` with the counts
static void EECPUnitsSaveCheck(const EEComputerPlayer* inPlayer, const EECPUnitsState* s, const std::string& inBytes)
{
    static int sOn = -1;
    if (sOn < 0) {
        const char* theValue = getenv("EE_AI_SAVE_CHECK");
        sOn = theValue != NULL && theValue[0] == '1' ? 1 : 0;
    }
    if (!sOn)
        return;
    size_t theCounts[4];
    bool theSame = ee_ai_units_state_save_check(s->mRust, (const uint8_t*)inBytes.data(), inBytes.size(), theCounts);
    EECPTraceScope theScope("next");
    EE_CP_TRACE(const_cast<EEComputerPlayer*>(inPlayer), "next.savecheck", "units=%s bytes=%lu held=%lu events=%lu memory=%lu trades=%lu",
        theSame ? "same" : "DIFFERENT", (unsigned long)inBytes.size(), (unsigned long)theCounts[0],
        (unsigned long)theCounts[1], (unsigned long)theCounts[2], (unsigned long)theCounts[3]);
}

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    const EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    outBytes.resize(ee_ai_units_state_save(s->mRust, NULL, 0));
    ee_ai_units_state_save(s->mRust, (uint8_t*)&outBytes[0], outBytes.size());
    EECPUnitsSaveCheck(inPlayer, s, outBytes);
}

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    if (inBytes.empty())
        return true;
    return ee_ai_units_state_load(EECPUnitsGet(inPlayer)->mRust, (const uint8_t*)inBytes.data(), inBytes.size());
}

// ---------------------------------------------------------------------------------------------------------------
// The host of the Rust bindings (ee_ai units_bind.rs EE_AI_UNITS_HOST_*): what the world views do not hold

// a command's message (ee_ai units_bind.rs EE_AI_UNITS_MSG_*), as the human's: on the selection under the borrowed
// slot (mode 0, EECPUnitsRun's), or run as it is (mode 1)
static const unsigned char kUnitsMessages[] = {0, kMessagePathTo, kMessageAttackUnit, kMessageAttackGround,
    kMessageGatherResource, kMessageBuildUnit, kMessageRepairUnit, kMessageLoadGarrison, kMessageLoadTransport,
    kMessageUnloadTransport, kMessageResearchTechnology, kMessageSetUnitBehavior, kMessageStopActiveGroup, kMessageExplore,
    kMessageKillUnit, kMessageHealUnit, kMessageSetRallyGoal, kMessageRemoveTechnology, kMessageChangeFormation,
    kMessagePackUnpack, kMessageDropoffResource, kMessageReplantFarms, kMessageSmartFarmers, kMessageCastCalamity,
    kMessageAddPatrolPoint, kMessageLockUnlock, kMessageBuildWall, kMessageImproveUnit, kMessageHackANewUnit, kMessageBuildGate,
    kMessageExchangeResources};

static EEMRecordValue EECPUnitsValue(const EeAiUnitsMsgValue& inValue)
{
    if (inValue.kind == 2)
        return EEMRecordValue(GE2DPoint(inValue.x, inValue.y));
    if (inValue.kind == 1)
        return EEMRecordValue((double)inValue.x);
    return EEMRecordValue((long long)inValue.i);
}

static void EECPUnitsExecute(EEComputerPlayer* me, EEMessage& inMessage, long inMode)
{
    if (inMode == 1) {
        inMessage.Execute();
        return;
    }
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    if (inMode == 2 || gWorld.GetPlayerBySlot(kBorrowedSlot) == me) {   // 2: the borrowed slot, unchecked
        inMessage.SetOriginatingSlot(kBorrowedSlot);
        inMessage.Execute();
    }
    me->mSlot = theSlot;
    if (inMode == 0)   // 2, 3: the selection is not the order's
        me->mActiveGroup->Clear();
}

static void EECPUnitsExec(EEComputerPlayer* me, const EeAiUnitsMessage& m)
{
    static_assert(sizeof(kUnitsMessages) == EE_AI_UNITS_MSG_EXCHANGE + 1, "ee_ai units_bind.rs EE_AI_UNITS_MSG_*");
    unsigned char theType = kUnitsMessages[m.message];
    const EeAiUnitsMsgValue* v = m.values;
    switch (m.count) {
    case 0: { EEMRecord theMessage(theType); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 1: { EEMRecord theMessage(theType, {EECPUnitsValue(v[0])}); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 2: { EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1])}); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 3: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 4: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 5: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]),
            EECPUnitsValue(v[4])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 7: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]),
            EECPUnitsValue(v[4]), EECPUnitsValue(v[5]), EECPUnitsValue(v[6])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The continent manager and the walls for the Rust bindings (ee_ai units_more.rs): the only reads of the
// continent manager's objects the bindings make (lane C re-points these when the continents move)

// the last list handed out, for a second read into a larger buffer (EE_AI_UNITS_HOST_TILES_AGAIN): a list is never
// computed twice (the shore's GetTilesNextTo and FindPath are captured)
static std::vector<U2DPoint<long> > sUnitsLastTiles;

static void EECPUnitsTilesOut(EeAiUnitsTiles& ioTiles, const std::vector<U2DPoint<long> >& inTiles)
{
    for (size_t k = 0; k < inTiles.size() && k < ioTiles.cap; k++) {
        ioTiles.xy[2 * k] = inTiles[k].mX;
        ioTiles.xy[2 * k + 1] = inTiles[k].mY;
    }
    if (inTiles.size() > ioTiles.cap)
        sUnitsLastTiles = inTiles;
}

static void EECPUnitsTilesIn(const EeAiUnitsTiles& inTiles, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.clear();
    for (size_t k = 0; k < inTiles.len; k++)
        outTiles.push_back(U2DPoint<long>(inTiles.xy[2 * k], inTiles.xy[2 * k + 1]));
}

// a forest's, a hill's or a choke point's random point (ai.forest_point, ai.hill_point, ai.choke_point)
template <class T>
static void EECPUnitsPickOf(std::vector<T*>& inFound, EeAiUnitsAreaPick& p)
{
    p.found = (long)inFound.size();
    if (inFound.empty())
        return;
    long thePick = (long)(p.pick % (int64_t)inFound.size());
    if (thePick < 0)
        thePick += (long)inFound.size();
    T* theArea = inFound[thePick];
    if (!(theArea->GetNumTiles() > 0))
        return;
    URandom theRandom((unsigned long)p.seed);
    U2DPoint<long> thePoint = theArea->GetTileSet().GetRandomPoint(theRandom);
    p.ok = true;
    p.px = thePoint.mX;
    p.py = thePoint.mY;
}

static void EECPUnitsAreaPick(EeAiUnitsAreaPick& p)
{
    EEAIContinent* theLand = (EEAIContinent*)(uintptr_t)p.land;
    U2DPoint<long> theFrom(p.x, p.y);
    long r = p.r;
    if (p.kind == 0) {
        std::vector<EEAIForest*> theForests;
        for (size_t i = 0; i < theLand->GetForestIDs().size(); i++) {
            unsigned short theID = theLand->GetForestIDs()[i];
            EEAIForest* theForest = theID < gContinentManager.GetForests().size() ? gContinentManager.GetForests()[theID] : NULL;
            if (theForest != NULL && (theForest->HullContains(theFrom)
                    || theForest->GetClosestHullPoint(theFrom).DistanceToSquared(theFrom) <= r * r))
                theForests.push_back(theForest);
        }
        p.total = (long)theLand->GetForestIDs().size();
        EECPUnitsPickOf(theForests, p);
    } else if (p.kind == 1) {
        std::vector<EEAIHill*> theHills;
        for (size_t i = 0; i < theLand->GetHillIDs().size(); i++) {
            unsigned short theID = theLand->GetHillIDs()[i];
            EEAIHill* theHill = theID < gContinentManager.GetHills().size() ? gContinentManager.GetHills()[theID] : NULL;
            if (theHill != NULL && (theHill->HullContains(theFrom)
                    || theHill->GetClosestHullPoint(theFrom).DistanceToSquared(theFrom) <= r * r))
                theHills.push_back(theHill);
        }
        p.total = (long)theLand->GetHillIDs().size();
        EECPUnitsPickOf(theHills, p);
    } else {
        // the choke points of the land; of the `r` nearest (by their closest point) when r is above 0
        std::vector<EEAIChokePoint*> theChokePoints;
        gContinentManager.GetChokePoints(theChokePoints, theLand->GetID(), false);
        p.total = (long)theChokePoints.size();
        if (r > 0 && !theChokePoints.empty()) {
            std::multimap<long, EEAIChokePoint*> theMap;
            while (!theChokePoints.empty()) {
                U2DPoint<long> theClosest = theChokePoints.back()->GetClosestPoint(theFrom);
                theMap.insert(std::make_pair((long)theClosest.DistanceToSquared(theFrom), theChokePoints.back()));
                theChokePoints.pop_back();
            }
            long theCount = 0;
            for (std::multimap<long, EEAIChokePoint*>::iterator it = theMap.begin(); it != theMap.end() && theCount < r;
                 ++it, ++theCount)
                theChokePoints.push_back(it->second);
        }
        EECPUnitsPickOf(theChokePoints, p);
    }
}

// the largest water body's shore tiles next to the land mass at the tile (EECPBuildManager::CollectSiteTiles(2)); -1
// when that water body does not touch it
static int64_t EECPUnitsWaterShore(EeAiUnitsTiles& t)
{
    unsigned short theWaterID = (unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryWater);
    EEAIContinent* theWater = theWaterID != 0 ? gContinentManager.GetContinentFromID(theWaterID) : NULL;
    EEAIContinent* theLand = gContinentManager.GetContinentAt(t.from[0], t.from[1]);
    if (theWater == NULL || theLand == NULL
        || !gContinentManager.AreAdjacent(theWaterID, theLand->GetID()))
        return -1;
    std::vector<U2DPoint<long> > theShore;
    theWater->GetShoreNextTo(theLand->GetID(), theShore);
    EECPUnitsTilesOut(t, theShore);
    return (int64_t)theShore.size();
}

// the classic wall manager's area utilities (EECPWallManager.cpp declares them the same way)
void EEGetWallLine(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo, std::vector<U2DPoint<long> >& outTiles);
void EEGetSquareRingTiles(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles);
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// IsPathBlocked (EECPWallManager::PlanWall): a land building stands on the line or within 2 tiles of it, the
// player's own wall pieces on the line excepted
static bool EECPUnitsWallBlocked(EEComputerPlayer* me, const EeAiUnitsTiles& t)
{
    std::vector<U2DPoint<long> > theLine, theAll, theAround;
    EECPUnitsTilesIn(t, theLine);
    for (size_t k = 0; k < theLine.size(); k++) {
        theAll.push_back(theLine[k]);
        for (long r = 1; r <= 2; r++) {
            EEGetSquareRingTiles(theLine[k].mX, theLine[k].mY, r, theAround);
            theAll.insert(theAll.end(), theAround.begin(), theAround.end());
        }
    }
    std::sort(theAll.begin(), theAll.end());
    theAll.erase(std::unique(theAll.begin(), theAll.end()), theAll.end());
    std::vector<EEUnit*> theUnits;
    EEGetUnitsOn(theAll, theUnits);
    bool theBlocked = false;
    for (size_t k = 0; k < theUnits.size() && !theBlocked; k++) {
        EEUnit* u = theUnits[k];
        if (!u->mUnitClass->IsLandBuilding())
            continue;
        theBlocked = true;
        if (u->mPlayer == me && u->mUnitClass->mIsWall && u->mUnitClass->mCategory != kUnitCategoryGate
            && std::find(theLine.begin(), theLine.end(), U2DPoint<long>(u->mTileX, u->mTileY)) != theLine.end())
            theBlocked = false;
    }
    return theBlocked;
}

static int64_t EECPUnitsWallsHost(EEComputerPlayer* me, int32_t inOp, int64_t a, int64_t b, void* outRecord)
{
    EECPUnitsState* s = inOp >= EE_AI_UNITS_HOST_ROUTE_REQUEST ? EECPUnitsGet(me) : NULL;   // the routes' (me given)
    switch (inOp) {
    case EE_AI_UNITS_HOST_AREA_AT:
        return (int64_t)(uintptr_t)gContinentManager.GetContinentAt((long)a, (long)b);
    case EE_AI_UNITS_HOST_AREA_BY_ID:
        if (a <= 0 || (size_t)a >= gContinentManager.GetContinents().size())
            return 0;
        return (int64_t)(uintptr_t)gContinentManager.GetContinentByID((unsigned short)a);
    case EE_AI_UNITS_HOST_AREA_INFO: {
        EEAIContinent* theArea = (EEAIContinent*)(uintptr_t)a;
        EeAiUnitsArea& r = *(EeAiUnitsArea*)outRecord;
        r.id = (long)theArea->GetID();
        r.category = (long)theArea->GetCategory();
        r.tiles = (long)theArea->GetNumTiles();
        return 0;
    }
    case EE_AI_UNITS_HOST_AREA_POINT: {
        EEAIContinent* theArea = (EEAIContinent*)(uintptr_t)a;
        if (!(theArea->GetNumTiles() > 0))
            return 0;
        URandom theRandom((unsigned long)b);
        U2DPoint<long> thePoint = theArea->GetTileSet().GetRandomPoint(theRandom);
        ((EeAiUnitsArea*)outRecord)->x = thePoint.mX;
        ((EeAiUnitsArea*)outRecord)->y = thePoint.mY;
        return 1;
    }
    case EE_AI_UNITS_HOST_AREA_PICK:
        EECPUnitsAreaPick(*(EeAiUnitsAreaPick*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_TILES_AGAIN: {
        std::vector<U2DPoint<long> > theTiles;
        theTiles.swap(sUnitsLastTiles);
        EECPUnitsTilesOut(*(EeAiUnitsTiles*)outRecord, theTiles);
        return (int64_t)theTiles.size();
    }
    case EE_AI_UNITS_HOST_WATER_SHORE:
        return EECPUnitsWaterShore(*(EeAiUnitsTiles*)outRecord);
    case EE_AI_UNITS_HOST_WALL_LINE: {
        EeAiUnitsTiles& t = *(EeAiUnitsTiles*)outRecord;
        std::vector<U2DPoint<long> > theTiles;
        EEGetWallLine(U2DPoint<long>(t.from[0], t.from[1]), U2DPoint<long>(t.to[0], t.to[1]), theTiles);
        EECPUnitsTilesOut(t, theTiles);
        return (int64_t)theTiles.size();
    }
    case EE_AI_UNITS_HOST_WALL_BLOCKED:
        return EECPUnitsWallBlocked(me, *(EeAiUnitsTiles*)outRecord) ? 1 : 0;
    case EE_AI_UNITS_HOST_FOREST_AT:
        return gContinentManager.GetForestAt(U2DPoint<long>((long)a, (long)b)) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_TILES_LEVEL: {
        std::vector<U2DPoint<long> > theFive;
        EECPUnitsTilesIn(*(EeAiUnitsTiles*)outRecord, theFive);
        return EEAreTilesLevel(theFive) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_WALL_PATH: {
        EeAiUnitsTiles& t = *(EeAiUnitsTiles*)outRecord;
        U2DPoint<long> theFrom(t.from[0], t.from[1]), theTo(t.to[0], t.to[1]);
        EEAIAStarBounds theBounds;
        theBounds.Set(theFrom);
        theBounds.Extend(theTo);
        std::vector<U2DPoint<long> > theSegment;
        EEAIGPGModifierComputerPlayerWallPather* thePather = new EEAIGPGModifierComputerPlayerWallPather;
        gPathManager.FindPath((EEComplexUnit*)(uintptr_t)a, gWorld.mMap, theFrom, theTo, theBounds, theSegment, 1, 0x32,
            thePather, NULL);
        thePather->Dereference();
        EECPUnitsTilesOut(t, theSegment);
        return (int64_t)theSegment.size();
    }
    case EE_AI_UNITS_HOST_ROUTE_REQUEST: {
        // one request a player at a time: a new one drops the old
        EeAiUnitsTiles& t = *(EeAiUnitsTiles*)outRecord;
        for (std::map<long, EEAutoPathRequest*>::iterator it = s->mRoutes.begin(); it != s->mRoutes.end(); ++it)
            if (it->second != NULL)
                it->second->Dereference();
        s->mRoutes.clear();
        EECPASGFlags theFlags;
        gUnitClassTable[kUnitTypeCitizen]->GetTerrainFlags(NULL, theFlags);
        EEAIGPGModifierComputerPlayerWallPlanner* theModifier = new EEAIGPGModifierComputerPlayerWallPlanner((long)a, 0);
        EETileCoordinate theFrom, theTo;
        theFrom.mX = t.from[0];
        theFrom.mY = t.from[1];
        theTo.mX = t.to[0];
        theTo.mY = t.to[1];
        EEAutoPathRequest* theRequest = gPathManager.RequestPath((long)a, theFrom, theTo, &theFlags, 2, -1, theModifier, 1);
        theModifier->Dereference();
        if (theRequest == NULL)
            return 0;
        long theHandle = s->mNextRoute++;
        s->mRoutes[theHandle] = theRequest;
        return theHandle;
    }
    case EE_AI_UNITS_HOST_ROUTE_STATE: {
        std::map<long, EEAutoPathRequest*>::iterator it = s->mRoutes.find((long)a);
        if (it == s->mRoutes.end() || it->second == NULL)
            return -2;
        return it->second->mIsDone ? (int64_t)it->second->mPath.size() : -1;
    }
    case EE_AI_UNITS_HOST_ROUTE_TAKE: {
        std::map<long, EEAutoPathRequest*>::iterator it = s->mRoutes.find((long)a);
        if (it == s->mRoutes.end() || it->second == NULL)
            return 0;
        EeAiUnitsTiles& t = *(EeAiUnitsTiles*)outRecord;
        EEAutoPathRequest* theRequest = it->second;
        for (size_t k = 0; k < theRequest->mPath.size() && k < t.cap; k++) {
            t.xy[2 * k] = theRequest->mPath[k].mX;
            t.xy[2 * k + 1] = theRequest->mPath[k].mY;
        }
        int64_t n = (int64_t)theRequest->mPath.size();
        theRequest->Dereference();
        s->mRoutes.erase(it);
        return n;
    }
    default:
        return 0;
    }
}

extern "C" int64_t ee_ai_units_host(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    switch (inOp) {
    case EE_AI_UNITS_HOST_STATE:
        return (int64_t)(uintptr_t)EECPUnitsGet(me)->mRust;
    case EE_AI_UNITS_HOST_NOW:
        return (int64_t)gPhysics->mGameTime;
    case EE_AI_UNITS_HOST_UNIT:
        return a > 0 ? (int64_t)EECPUnitsHandle(gWorld.GetUnit((unsigned long)a)) : 0;
    case EE_AI_UNITS_HOST_MAP_SIZE: {
        // the map's size in tiles (ee/EEMapDimensions.h)
        long w = gWorld.mMap != NULL ? (long)gMapWidth : 0, h = gWorld.mMap != NULL ? (long)gMapHeight : 0;
        return (int64_t)(((uint64_t)(uint32_t)h << 32) | (uint32_t)w);
    }
    case EE_AI_UNITS_HOST_CLASS_BY_NAME:
        return EECPNextClassByName((const char*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_CLASS_NAME: {
        std::string theName = EECPNextClassName((long)a);
        size_t n = theName.size() < 1023 ? theName.size() : 1023;
        memcpy(outRecord, theName.data(), n);
        ((char*)outRecord)[n] = 0;
        return (int64_t)n;
    }
    case EE_AI_UNITS_HOST_DETACH:
        me->DetachUnitFromAI((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_ATTACH: {
        EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)a;
        if (theUnit->mIsDetachedFromAI)
            theUnit->mIsDetachedFromAI = false;
        return 0;
    }
    case EE_AI_UNITS_HOST_LAST_ATTACKER:
        return (int64_t)EECPUnitsHandle(((EEComplexUnit*)(uintptr_t)a)->mLastAttacker);
    case EE_AI_UNITS_HOST_UPGRADE: {
        EEPlayer* theOwner = (EEPlayer*)(uintptr_t)a;
        if (b < 0 || (unsigned long)b >= theOwner->mUnitClassUpgrades.size())
            return 0;
        return (int64_t)(uintptr_t)theOwner->mUnitClassUpgrades[(unsigned long)b];
    }
    case EE_AI_UNITS_HOST_CONTINENT:
        return (int64_t)gContinentManager.GetContinent(U2DPoint<long>((long)a, (long)b), c != 0 ? kAIPathWaterBody : kAIPathLandMass);
    case EE_AI_UNITS_HOST_TRACE:
        if (gCPTraceOn) {
            EECPTraceScope theScope("next");
            EECPTraceOut(me, (const char*)(uintptr_t)a, "%s", (const char*)(uintptr_t)b);
        }
        return 0;
    case EE_AI_UNITS_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_UNITS_HOST_FACTS: {
        EEComplexUnit* c = (EEComplexUnit*)(uintptr_t)a;
        EeAiUnitsFacts& f = *(EeAiUnitsFacts*)outRecord;
        ee_ai_units_facts_reset(&f);
        EEUnitGoal* g = c->GetGoalState();
        if (g != NULL) {
            f.has_goal = true;
            GE3DPoint thePlace;
            g->GetGoalLocation(thePlace);
            f.tx = UMFloor(thePlace.GetX());
            f.ty = UMFloor(thePlace.GetY());
            // the goal's target unit (AI_LUA_UNITS §2.3 `target`): the resource gathered, the building built or
            // repaired, the unit attacked; and a gatherer's resource kind
            EEUnit* theTarget = NULL;
            if (EEUGRust* theGather = EEGatherGoalCast(g)) {
                theTarget = theGather->GetGatherResource();
                long r = (long)c->mResourceType - kResourceFood;
                if (r >= 0 && r < 5)
                    f.gathers = r;
                // waiting for room at its resource (the classic resource manager counts these gatherers as idle)
                f.waiting = theGather->IsGatherWaiting() && c->mCurrentPhysicsAction != 0;
            } else if (EEUGRust* theBuild = EEBuildRepairGoalCast(g)) {
                theTarget = theBuild->GetBuildTarget();
                // a builder waiting for its foundation's site to clear while another unit holds it (the classic's
                // FindStalledBuilder: CancelStalledBuilds cancels these)
                f.stalled = (c->mCommand ? c->mCommand->mType : 0) == kActionWaitForBuildSiteToClear && theTarget != NULL
                    && theTarget->mComplexUnit != NULL && !theTarget->mComplexUnit->mIsBuilt && theBuild->IsBuildSiteTaken();
            } else if (EEUGRust* theAttack = EEAttackUnitCast(g))
                theTarget = theAttack->GetAttackTarget();
            if (theTarget != NULL && theTarget->mIsValid)
                f.target = (long)theTarget->mUnitID;
        }
        // an own aircraft's home base (the airport or carrier it returns to; the classic air sub-group's reach)
        if (c->mHomeBase != NULL && c->mHomeBase->mIsValid)
            f.home_base = (long)c->mHomeBase->mUnitID;
        if (!c->mUnitClass->IsBuilding() && c->mUnitClass->GetMaxVelocity() > 0.0f) {
            // the terrain the unit can cross (EEUnitClass::GetTerrainFlags: the classic's sub-group common move flags)
            f.moves = true;
            EECPASGFlags theFlags;
            memset(theFlags.mFlags, 0, sizeof(theFlags.mFlags));
            theFlags.mPlayerID = 0;
            c->mUnitClass->GetTerrainFlags(c, theFlags);
            f.crosses_forest = theFlags.mFlags[kAITileForest] != 0;
            f.crosses_cliff = theFlags.mFlags[kAITileCliff] != 0;
        }
        f.garrisoned = c->mGarrisonCount;   // citizens inside a town centre, capitol or settlement
        for (unsigned long k = 0; k < EEUnitBehavior::sTable.size(); k++)
            if (EEUnitBehavior::sTable[k] == c->mBehavior)
                f.stance = (long)k;
        return 0;
    }
    case EE_AI_UNITS_HOST_ASSIGNED:
        return EECPNextAssigned(me, (EEUnit*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_HAS_ROOM:
        return EECPNextHasRoom(me, (EEUnit*)(uintptr_t)a) ? 1 : 0;
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
    case EE_AI_UNITS_HOST_GATHER_TARGET: {
        EEUGRust* theGather = EEGatherGoalCast(((EEComplexUnit*)(uintptr_t)a)->GetGoalState());
        if (theGather != NULL && theGather->GetGatherResource() != NULL && theGather->GetGatherResource()->mIsValid)
            return (int64_t)theGather->GetGatherResource()->mUnitID;
        return 0;
    }
    case EE_AI_UNITS_HOST_SELECT_BEGIN:
        if (me->mActiveGroup == NULL)
            return 0;
        me->mActiveGroup->Clear();
        return 1;
    case EE_AI_UNITS_HOST_SELECT_ADD:
        me->mActiveGroup->Add((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_EXEC:
        EECPUnitsExec(me, *(const EeAiUnitsMessage*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_TECH_BY_NAME:
        return EECPNextTechByName((const char*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_CAN_PRODUCE:
        return me->EEPlayer::CanProduce((long)a, 0x20, 1, true, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_PLACE:
        return EECanPlaceOnTile(me, (long)a, (long)b, (long)c, false, 1) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_TRANSPORT:
        return ((EEUnitClass*)(uintptr_t)a)->IsTransport() ? 1 : 0;
    case EE_AI_UNITS_HOST_LAND_BUILDING:
        return ((EEUnitClass*)(uintptr_t)a)->IsLandBuilding() ? 1 : 0;
    case EE_AI_UNITS_HOST_BEHAVIOR_OK:
        return a >= 0 && (unsigned long)a < EEUnitBehavior::sTable.size() && EEUnitBehavior::sTable[(unsigned long)a] != NULL;
    case EE_AI_UNITS_HOST_HAS_NODE:
        return me->mTechnologies->Get((long)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_SEES_ALL:
        return EECPEngineSeesAll(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_BLOCK_TAKES: {
        long theSize = *(const long*)outRecord;
        return EEPlayerBlockTakes(me, (long)a, (long)b - 1, (long)c - 1, theSize + 2, theSize + 2, 0) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_HAS_TREE:
        return me->mTechnologies != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_WALL_CLASS:
        return EECPWallClass(me);
    case EE_AI_UNITS_HOST_NODE_AFFORD: {
        EETechTreeNode* theNode = me->mTechnologies->Get((long)a);
        if (theNode == NULL)
            return -1;
        return me->CanAfford(theNode, 0x20, 1) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_FLIES_OVER:
        return ((EEUnitClass*)(uintptr_t)a)->FliesOver() ? 1 : 0;
    case EE_AI_UNITS_HOST_POP_COST:
        return ((EEUnitClass*)(uintptr_t)a)->GetPopulationCost();
    case EE_AI_UNITS_HOST_DAMAGES:
        return gDamageTable != NULL && gDamageTable[(long)a][(long)b] != 0.0f ? 1 : 0;
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
    case EE_AI_UNITS_HOST_TECH_NAME: {
        std::string theName = EECPNextTechName((long)a);
        size_t n = theName.size() < 1023 ? theName.size() : 1023;
        memcpy(outRecord, theName.data(), n);
        ((char*)outRecord)[n] = 0;
        return (int64_t)n;
    }
    case EE_AI_UNITS_HOST_CAN_PRODUCE_AS:
        return me->CanProduce((long)a, (long)b, 1, c != 0, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_CITIZEN_UPGRADE:
        return EECPUnitsCitizenUpgrade((EETechTreeNode*)(uintptr_t)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_EPOCH_ID:
        return EEIsEpochID((long)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_EPOCH_COUNT:
        return EEEpochCount();
    case EE_AI_UNITS_HOST_IMPROVE_COST:
        return EECPUnitsImproveCost(me, (long)a, (long)b, *(EeAiUnitsImproveCost*)outRecord) ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_BUY:
        return me->CanBuy((long)a, (long)b, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_GATE_CLASS:
        return EECPGateClass(me);
    case EE_AI_UNITS_HOST_IS_GATE_CLASS:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>((EEUnitClass*)(uintptr_t)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_GATE_SIDE:
        return EECPUnitsGateSide(me, (long)a, (long)b);
    case EE_AI_UNITS_HOST_TRIBUTE: {
        EEPlayer* theTo = gWorld.mPlayers[(long)a];
        me->TakeResource((long)b, (long)c);
        me->AddTribute(0, (long)b, (long)c);
        theTo->AddResource((long)b, (long)c);
        theTo->AddTribute(1, (long)b, (long)c);
        return 0;
    }
    case EE_AI_UNITS_HOST_STANCE:
        gWorld.SetDiplomaticStance(me->mPlayerID, (long)a, (long)b);
        return 0;
    case EE_AI_UNITS_HOST_GARRISON_FILL: {
        EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)a;
        return theUnit->AddGarrison(theUnit->mGarrisonCount + (long)b, theUnit->mGarrisonPopulation) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_SITE_BLOCKED:
        return EECPNextSiteBlocked(me, (EEUnit*)(uintptr_t)a, NULL);
    case EE_AI_UNITS_HOST_BUILD_UNSAFE:
        return EECPNextBuildUnsafe(me, (long)a, (long)b);
    case EE_AI_UNITS_HOST_SITE_THREAT:
        return EECPNextSiteThreat(me, (long)a, (long)b, false, NULL);
    case EE_AI_UNITS_HOST_ENEMY_NEAR_ALL:
        return EECPUnitsEnemyNearAll(me, (long)a, (long)b);
    case EE_AI_UNITS_HOST_PLACE: {
        EeAiUnitsPlace& p = *(EeAiUnitsPlace*)outRecord;
        std::vector<long> theUnits(p.units, p.units + p.units_len);
        return EECPNextPlaceSearch(me, p.unit_type, p.mode, p.x, p.y, p.first, p.last, theUnits, p.site, p.out_x, p.out_y,
            p.out_gap) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_FILLS_GARRISON:
        return EECPEngineFillsGarrison(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_TUNING:
        EECPUnitsTuning(me, *(EeAiUnitsTuning*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_AREA_AT: case EE_AI_UNITS_HOST_AREA_BY_ID: case EE_AI_UNITS_HOST_AREA_INFO:
    case EE_AI_UNITS_HOST_AREA_POINT: case EE_AI_UNITS_HOST_AREA_PICK: case EE_AI_UNITS_HOST_WATER_SHORE:
    case EE_AI_UNITS_HOST_WALL_LINE: case EE_AI_UNITS_HOST_WALL_BLOCKED: case EE_AI_UNITS_HOST_FOREST_AT:
    case EE_AI_UNITS_HOST_TILES_LEVEL: case EE_AI_UNITS_HOST_WALL_PATH: case EE_AI_UNITS_HOST_ROUTE_REQUEST:
    case EE_AI_UNITS_HOST_ROUTE_STATE: case EE_AI_UNITS_HOST_ROUTE_TAKE: case EE_AI_UNITS_HOST_TILES_AGAIN:
        return EECPUnitsWallsHost(me, inOp, a, b, outRecord);
    default:
        return 0;
    }
}
