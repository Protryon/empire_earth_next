// C:\EEX\Empire Earth\Player\EEPlayer.cpp (__FILE__ 0x165cfa). The player: 0x161825..0x16d346 (the class static keys'
// init chains first, then EEWUnitsOffMapInfo, EEPlayer in definition order, the selected-group and functor helpers it
// defines, and its template COMDATs). Unit flags: the exe's module set. Data: the vtable 0x4429e0.
#include <windows.h>
#include "platform/PlatSim.h"
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUnitKind.h"
#include "ee/EETileLocation.h"
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEGameLog.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEButtonObject.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/UResourceFile.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitClassImprovement.h"
#include "lle/NEMessage.h"
#include "ee/EESavedGroup.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEHerd.h"
#include "ee/EEVectorIO.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEGameVariant.h"           // the wonder powers' hit point factors (EEPlayerHost)
#include "ee/EEUnitUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "lle/SESoundManager.h"
#include "ee/EEUGRust.h"                // the towns' patrol goals (EEPlayerHost)
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"   // ee_screens_hot_key_unit_selection: the docks' hot keys (ee_screens hot_keys.rs)
#include "rust/sim/units.h"
#include "lle/U2DRectangle.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAnimal.h"
#include "ee/EEStringIDs.h"
#include "lle/ISKeyCode.h"

// the interval of the healing units' hit-point bonus (0x4429b8, just before kTributeFee; ComputeLoadDistance)
// (provisional name)
static const float kPlayerHealInterval = EE_UNITS_PLAYER_HEAL_INTERVAL;   // (ee_units player_tick.rs)

// TEST ONLY: the player capture (tools/sim_capture.py): PLST, each player's state at every UpdatePlayer (ee_sim app
// player_capture.rs); PLEC, the economy and improvement decisions (ee_units player_econ.rs)
#include "ee/EESimCapture.h"
#include "ee/EEUnitStore.h"
#include <cstring>

// the decisions' record measures a reason text (ee_units player_econ.rs)
extern "C" uint32_t ee_units_player_wide_length(const void* inText) { return static_cast<const UWideString*>(inText)->GetLength(); }

// a unit as the Rust player's operations take it: its RTSUnit address (0: none)
static int64_t EEPlayerUnitArg(RTSUnit* inUnit)
{
    return (int64_t)reinterpret_cast<uintptr_t>(inUnit);
}

static EEComplexUnit* EEPlayerUnitAt(int64_t inUnit)
{
    return static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(inUnit));
}

// the hosts' objects (player_rest.rs, player_split_more.rs): a town, a herd, a point packed (x high)
static EETown* EEPlayerTownAt(uint64_t inTown) { return reinterpret_cast<EETown*>(static_cast<uintptr_t>(inTown)); }
static EEHerd* EEPlayerHerdAt(uint64_t inHerd) { return reinterpret_cast<EEHerd*>(static_cast<uintptr_t>(inHerd)); }
static int64_t EEPlayerPackPoint(const U2DPoint<long>& inPoint) { return (int64_t)((uint64_t)(uint32_t)inPoint.mX << 32 | (uint32_t)inPoint.mY); }

template <class T> static T* EEPlayerPtr(int64_t inObject) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inObject)); }
static GETransformation EEPlayerMovedLocation(EEComplexUnit* inUnit, const float* inPoint)
{
    GETransformation theLocation(false);
    theLocation = inUnit->mPhysicalModel.GetLocation();
    theLocation.SetPosition(inPoint[0], inPoint[1], inPoint[2]);
    return theLocation;
}

// the player's lists' host (ee_units player_lists.rs): the units as RTSUnit addresses
extern "C" uint64_t ee_units_lists_host(int32_t inOp, uint64_t inValue)
{
    RTSUnit* theUnit = reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inValue));
    switch (inOp) {
    case EE_UNITS_LISTS_HOST_REFERENCE: theUnit->Reference();
        return 0;
    case EE_UNITS_LISTS_HOST_DEREFERENCE: theUnit->Dereference();
        return 0;
    case EE_UNITS_LISTS_HOST_UNIT_ID: return theUnit->GetUnitID();
    case EE_UNITS_LISTS_HOST_UNIT_BY_ID: return (uint64_t)reinterpret_cast<uintptr_t>(gWorld.GetUnit((unsigned long)inValue));
    }
    return 0;
}

static int64_t EEPlayerHost(void* inPlayer, int32_t inOp, EeUnitsPlayerIo* io);

// the Rust player's view of a player (ee_units player.rs)
EEPlayerRefs::EEPlayerRefs(const EEPlayer* inPlayer)
    : EeUnitsPlayerRefs{const_cast<EEPlayer*>(inPlayer), &EEPlayerHost, const_cast<EEPlayer*>(inPlayer), const_cast<EEPlayer*>(inPlayer), inPlayer->mSavedGroups.mStats}
{
}

// port (the player lane, round 1): the economy and the improvements are Rust (ee_units player.rs) over this host; the
// player's members after the saved record are the Rust record EeUnitsPlayerFields, the queued improvements its
// queue (mQueued)
static int64_t EEPlayerHost(void* inPlayer, int32_t inOp, EeUnitsPlayerIo* io)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    int64_t* a = io->i;
    EETechTreeNode* theNode = reinterpret_cast<EETechTreeNode*>(a[0]);
    EEUnit* theUnit = static_cast<EEUnit*>(reinterpret_cast<RTSUnit*>(a[0]));   // the units' operations: an RTSUnit address
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(a[0]));
    EEPlayer* theOther = a[0] != 0 ? reinterpret_cast<EEPlayer*>(a[0]) : thePlayer;   // the diplomacy's: 0 this player
    switch (inOp) {
    case EE_UNITS_PLAYER_NODE_COST: return theNode->GetCost((long)a[1], (long)a[2], 1.0f);
    case EE_UNITS_PLAYER_METALLURGY: return gCivPowersManager.HasPower(thePlayer, kCivPowerMetallurgy);
    case EE_UNITS_PLAYER_ADD: thePlayer->AddResource((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_UNITS_PLAYER_HERO_COUNT: return thePlayer->GetHeroCount(a[0] != 0, a[1] != 0);
    case EE_UNITS_PLAYER_SCENARIO: return gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_PLAYER_CAN_AFFORD: return thePlayer->CanAfford(theNode, (long)a[1], (long)a[2]);
    case EE_UNITS_PLAYER_STATS_LOWER: thePlayer->mSavedGroups.Lower((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_UPDATE_JIT: gUserInterface->UpdateJITManufacturing();
        return 0;
    // the unit bookkeeping and the wonders (ee_units player_units.rs): the units as RTSUnit addresses
    case EE_UNITS_PLAYER_STATE: return (gWorld.mLoadingSavedGame ? 1 : 0) | (gWorld.mGameScreenActive ? 2 : 0) | (gWorld.mClearing ? 4 : 0) | (UShutdownInProgress() ? 8 : 0) | (gWorld.mVictoryEnabled ? 16 : 0) | (gUserInterface != NULL ? 32 : 0);
    case EE_UNITS_PLAYER_RTS_ADD_UNIT: ee_rts_player_add_unit(thePlayer, theUnit, a[1] != 0);
        return 0;
    case EE_UNITS_PLAYER_ACTIVATE: gWorld.ActivateUnit(theComplex);
        return 0;
    case EE_UNITS_PLAYER_UNIT_PLACED: thePlayer->UnitPlaced(theComplex);
        return 0;
    case EE_UNITS_PLAYER_SWAP_KIND: return ee_physics_unit_swap_kind(gPhysics, theComplex);
    case EE_UNITS_PLAYER_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theComplex);
    case EE_UNITS_PLAYER_STATS_RESET: thePlayer->mSavedGroups.Reset((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_FOUNDATION: return theComplex->IsFoundation();
    case EE_UNITS_PLAYER_ADD_TO_TOWN: return ee_units_player_areas(EEPlayerRefs(thePlayer), 0, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_ADD_TO_HERD: return ee_units_player_areas(EEPlayerRefs(thePlayer), 1, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_CIV_POWER: return gCivPowersManager.HasPower(thePlayer, (long)a[0]);
    case EE_UNITS_PLAYER_LOG_LOST: ee_game_log_unit_lost((uint64_t)(uintptr_t)thePlayer, (uint64_t)(uintptr_t)theComplex);   // port: the game log (ee/EEGameLog.h)
        return 0;
    case EE_UNITS_PLAYER_REMOVE_FROM_TOWN: ee_units_player_areas(EEPlayerRefs(thePlayer), 2, static_cast<RTSUnit*>(theComplex));
        return 0;
    case EE_UNITS_PLAYER_REMOVE_FROM_HERD: ee_units_player_areas(EEPlayerRefs(thePlayer), 3, static_cast<RTSUnit*>(theComplex));
        return 0;
    case EE_UNITS_PLAYER_RECOMPUTE_AREAS: thePlayer->SuggestRecomputeAllAreaEffects();
        return 0;
    case EE_UNITS_PLAYER_OWNER_ADD_WONDER: theComplex->mPlayer->AddWonder(theComplex, false);
        return 0;
    case EE_UNITS_PLAYER_SET_CONTAINS: return gUnitSetManager.GetSet((long)a[0]) != NULL && gUnitSetManager.GetSet((long)a[0])->Contains((long)a[1]);
    case EE_UNITS_PLAYER_UPGRADE_HIT_POINTS: EEUpgradeHitPoints(thePlayer->mUnitClassUpgrades[(size_t)a[0]], thePlayer->mUnitClassUpgrades[(size_t)a[0]]->mUnitClass->GetUnitType(), io->f[0], thePlayer);
        return 0;
    case EE_UNITS_PLAYER_CHANGE_LEVEL: thePlayer->mUnitClassUpgrades[(size_t)a[0]]->ChangeLevel((long)a[1], (long)a[2]);
        return 0;
    case EE_UNITS_PLAYER_GENERATE: io->f[0] = gPhysics->mRandom.Generate();
        return 0;
    case EE_UNITS_PLAYER_PRODUCE: return ee_units_uc_produce(theUnit->GetUnitClass(), theUnit->GetComplexUnit(), gUnitClassTable[(size_t)a[1]], false);
    case EE_UNITS_PLAYER_SLAVERY_SOUND: gSoundManager->Play(EESoundObject::Get(kSoundSlavery)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_PLAYER_HEALED_SET: thePlayer->mHealedUnitSet = a[1] != 0 ? gUnitSetManager.GetSet((long)a[0]) : NULL;
        return 0;
    // the diplomacy and the shared sight (ee_units player_diplomacy.rs): the players as EEPlayer addresses, 0 this one
    case EE_UNITS_PLAYER_REFRESH_TERRAIN: gWorld.GetCurrentPlayer()->RefreshTerrainVisibility();
        return 0;
    // the compound operations' statements (ee_units player_split.rs): a[0] a unit or a player (0 this one)
    case EE_UNITS_PLAYER_X_SHARE_REMOVE: gWorld.RemoveShareBonus();
        return 0;
    case EE_UNITS_PLAYER_X_SHARE_APPLY: gWorld.ApplyShareBonus();
        return 0;
    case EE_UNITS_PLAYER_X_SHARER_ADD: theOther->AddSightSharer(thePlayer);
        return 0;
    case EE_UNITS_PLAYER_X_SHARER_REMOVE: theOther->RemoveSightSharer(thePlayer);
        return 0;
    case EE_UNITS_PLAYER_X_COMPLEX_SIGHT_ON: theUnit->GetComplexUnit()->SightOnHere();
        return 0;
    case EE_UNITS_PLAYER_X_COMPLEX_SIGHT_OFF: theUnit->GetComplexUnit()->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_X_SET_FLIGHT_TIME: theComplex->SetFlightTime((long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_SET_POWER: theComplex->SetPower((long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_SET_HIT_POINTS: theUnit->SetHitPoints((long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_EVENTS_SHOW: theOther->mEvents.ShowEvents(theOther);
        return 0;
    case EE_UNITS_PLAYER_X_EVENTS_HIDE: theOther->mEvents.HideEvents(theOther);
        return 0;
    case EE_UNITS_PLAYER_X_MINIMAP: return gMiniMap != NULL;
    case EE_UNITS_PLAYER_X_MINIMAP_REDRAW: ee_gamecpp_minimap_redraw(gMiniMap);
        return 0;
    case EE_UNITS_PLAYER_X_STATS_LOWER: thePlayer->mSavedGroups.Lower((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_SIGHT_ON: theUnit->SightOnHere();
        return 0;
    case EE_UNITS_PLAYER_X_SIGHT_OFF: theUnit->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_X_FOREST_REMOVE: gContinentManager.RemoveForestTile(theUnit);
        return 0;
    case EE_UNITS_PLAYER_X_UPGRADE_SLOTS: return (int64_t)thePlayer->mUnitClassUpgrades.size();
    case EE_UNITS_PLAYER_X_UPGRADE_AT: return (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mUnitClassUpgrades[(size_t)a[0]]);
    case EE_UNITS_PLAYER_X_STRING: gResource->GetString((long)a[0], *reinterpret_cast<UWideString*>(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2: return (long)gWorldTable.GetLongValue2(EEWorldImprovementTime(a[0]));
    case EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1: return (long)gWorldTable.GetLongValue(EEWorldImprovementTime(a[0]));
    // the towns and herds (ee_units player_areas.rs): list a[0] (0 the towns, 1 the herds), the areas and units by address
    case EE_UNITS_PLAYER_TOWN_UNIT: return ee_cp_town_is_town_unit((uint64_t)(uintptr_t)theUnit);
    case EE_UNITS_PLAYER_HERD_ANIMAL: return EEHerd::IsAnimalOnMap(theUnit);
    case EE_UNITS_PLAYER_AREA_CAN_TAKE:
        return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->CanTake(EEPlayerUnitAt(a[2])) : reinterpret_cast<EEHerd*>(a[1])->CanTake(EEPlayerUnitAt(a[2]), true);
    case EE_UNITS_PLAYER_HERD_HAS: return reinterpret_cast<EEHerd*>(a[0])->HasUnit(EEPlayerUnitAt(a[1]));
    case EE_UNITS_PLAYER_AREA_ADD:
        return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->AddUnit(EEPlayerUnitAt(a[2])) : reinterpret_cast<EEHerd*>(a[1])->AddUnit(EEPlayerUnitAt(a[2]));
    case EE_UNITS_PLAYER_HOLDER: return EEPlayerUnitArg(theComplex->mHolder);
    case EE_UNITS_PLAYER_RANDOM: return gPhysics->GetRandom((long)a[0], (long)a[1]);
    case EE_UNITS_PLAYER_AREA_TILES: return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->GetNumTiles() : reinterpret_cast<EEHerd*>(a[1])->GetNumTiles();
    case EE_UNITS_PLAYER_TOWN_CONTAINS: return reinterpret_cast<EETown*>(a[0])->Contains(U2DPoint<long>((long)a[1], (long)a[2]));
    // the world lane, round 2: the compound operations' statements (ee_units player_split_more.rs)
    case EE_UNITS_PLAYER_X_CLASS_POPULATION: return EEPlayerPtr<EEUnitClass>(a[0])->GetPopulationCost();
    case EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS: return (int64_t)(uintptr_t)EEPlayerPtr<EEUnitClass>(a[0])->mImprovements;
    case EE_UNITS_PLAYER_X_UP_CAN_UPGRADE: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mImprovement.CanUpgrade(EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mUnitClass, (long)a[1]) == true;
    case EE_UNITS_PLAYER_X_UP_COST: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mImprovement.GetCost((long)a[1]);
    case EE_UNITS_PLAYER_X_UP_TOTAL: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mImprovement.mTotalCost;
    case EE_UNITS_PLAYER_X_UP_LEVEL: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mImprovement.GetLevel((long)a[1]);
    case EE_UNITS_PLAYER_X_POINTS_MAX: return gWorldTable.GetLongValue(kWorldUnitImprovementPointsMax);
    case EE_UNITS_PLAYER_X_PAY_IMPROVEMENT: thePlayer->PayImprovement(EEPlayerPtr<EEUnitClass>(a[0]), (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_PULSE_SIGHT: thePlayer->PulseSight(true, io->f[0], theUnit->mUnitID);
        return 0;
    case EE_UNITS_PLAYER_X_VARIANT_FACTOR: io->f[0] = gWorld.GetGameVariant()->GetFactor((long)a[0]);
        return 0;
    case EE_UNITS_PLAYER_X_WONDER_HIT_POINT_BONUS: io->f[0] = gWorldTable.GetFloatValue(kWorldWonderHitPointBonus);
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_NEW: return (int64_t)(uintptr_t)new EETown(thePlayer, ++gWorld.mTownIDCounter);
    case EE_UNITS_PLAYER_X_HERD_NEW: return (int64_t)(uintptr_t)new EEHerd(thePlayer, theComplex->mUnitClass->mAnimalID, ++gWorld.mHerdIDCounter);
    case EE_UNITS_PLAYER_X_TOWN_SERIAL: EEPlayerTownAt(a[0])->SetSerial(gContinentManager.NewSerial());
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_ADD_AREA: gContinentManager.AddArea(EEPlayerTownAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA: gContinentManager.RemoveArea(EEPlayerTownAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_DELETE: delete EEPlayerTownAt(a[0]);
        return 0;
    case EE_UNITS_PLAYER_X_HERD_SERIAL: EEPlayerHerdAt(a[0])->SetSerial(gContinentManager.NewSerial());
        return 0;
    case EE_UNITS_PLAYER_X_HERD_ADD_AREA: gContinentManager.AddArea(EEPlayerHerdAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_REMOVE_AREA: gContinentManager.RemoveArea(EEPlayerHerdAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_DELETE: delete EEPlayerHerdAt(a[0]);
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_REMOVE_UNIT: EEPlayerTownAt(a[0])->RemoveUnit(EEPlayerUnitAt(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_REMOVE_UNIT: EEPlayerHerdAt(a[0])->RemoveUnit(EEPlayerUnitAt(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_MERGE: EEPlayerTownAt(a[0])->Merge(EEPlayerTownAt(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_TOWN_PATROL_OF: return (int64_t)(uintptr_t)EEPlayerTownAt(a[0])->mPatrolGroup;
    case EE_UNITS_PLAYER_X_TOWN_REFRESH_PATROL: EEPlayerTownAt(a[0])->RefreshPatrolGroup();
        return 0;
    case EE_UNITS_PLAYER_X_POINT_UNPATROL: EEPlayerPtr<EEComplexUnit>(a[0])->SetPatrolGroup(NULL);
        return 0;
    case EE_UNITS_PLAYER_X_POINT_KILL: EEPlayerPtr<EEComplexUnit>(a[0])->Kill(true, false, false);
        return 0;
    case EE_UNITS_PLAYER_X_POINT_KILL_UNIT: EEPlayerPtr<EEComplexUnit>(a[0])->mPlayer->KillUnit(EEPlayerPtr<EEComplexUnit>(a[0]), false);
        return 0;
    case EE_UNITS_PLAYER_X_PATROL_GOAL: return (int64_t)(uintptr_t)EEPatrolGoalCast(theComplex->mGoalState);
    case EE_UNITS_PLAYER_X_GOAL_PATROL_GROUP: return (int64_t)(uintptr_t)EEPlayerPtr<EEUGRust>(a[0])->GetPatrolGroup();
    case EE_UNITS_PLAYER_X_GOAL_SET_PATROL: EEPlayerPtr<EEUGRust>(a[0])->SetPatrolGroup(EEPlayerPtr<EEPPatrolGroup>(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_FIND_AREA_TILE: return EEPlayerPackPoint(theComplex->FindAreaTile(true, 1));
    case EE_UNITS_PLAYER_X_LAST_FLOOR_X: return UMFloor(theComplex->mLastMapPosition.GetX());
    case EE_UNITS_PLAYER_X_LAST_FLOOR_Y: return UMFloor(theComplex->mLastMapPosition.GetY());
    }
    return 0;
}

// Replaces each swapped old unit by its new one in all units, air traffic, the selection, control groups, wonders
// Called when the physics replaced units (EEPhysics 0x1534d1); a new unit of another player is not added to
// the groups, and an unbuilt wall becomes a construction site
void EEPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    ee_units_player_units_swapped(this, reinterpret_cast<void* const (*)[2]>(ioSwaps.data()), ioSwaps.size(), &ioSwaps);   // player_body.rs
}

// Sets the player's game result; a new victory or defeat of another player is announced in the chat log
// (the chat log's line of a player's victory or defeat: player_rest.rs announce_result). Out of line: the harness
// hooks its entry (tools/build.py HARNESS_HOOKS, tools/harness/no_human_defeat.py)
void EEPlayer::SetGameResult(long inState)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_SET_GAME_RESULT, 0, inState, 0);   // player_rest.rs
}

// the unit bookkeeping and the wonders (ee_units player_units.rs ee_units_player_units)
long EEPlayerUnits(const EEPlayer* inPlayer, uint32_t inWhat, void* inUnit, bool inFlag, bool inFlag2)
{
    return ee_units_player_units(EEPlayerRefs(inPlayer), inWhat, inUnit, inFlag, inFlag2);
}

// EEWorld::ApplyShareBonus (inRemove false) and RemoveShareBonus over every player's records (ee_units player_units.rs)
void EEPlayerShareBonus(bool inRemove)
{
    std::vector<EeUnitsPlayerRefs> theRefs(gWorld.mNumPlayers);   // (no player: zeros)
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++)
        if (gWorld.mPlayers[i] != NULL)
            theRefs[i] = EEPlayerRefs(gWorld.mPlayers[i]);
    if (!theRefs.empty())
        ee_units_player_share_bonus(&theRefs[0], (uint32_t)theRefs.size(), inRemove, gWorldTable.GetFloatValue(kWorldWonderPopulationBonus),
                                    gWorldTable.GetFloatValue2(kWorldWonderPopulationBonus), gWorld.mPlayerPopulationLimit);
}

// the improvement decisions (ee_units player.rs ee_units_player_improvement)
static long EEPlayerImprovement(const EEPlayer* inPlayer, uint32_t inWhat, unsigned long inUnitType, long inImprovement, bool inFlag = false,
                                bool inFlag2 = false, float inFraction = 0.0f, UWideString* outReason = NULL)
{
    return ee_units_player_improvement(EEPlayerRefs(inPlayer), inWhat, inUnitType, inImprovement, inFlag, inFlag2, inFraction, outReason);
}

// Records a unit leaving the map with its tile and the current game time
void EEPlayer::AddOffMapUnit(RTSUnit* inUnit)
{
    if (inUnit != NULL)
        ee_units_off_map(this->mLists, 0, EEPlayerUnitArg(inUnit), inUnit->mTileX, inUnit->mTileY, gPhysics != NULL ? gPhysics->mGameTime : 0);
}

// Detaches a unit from the computer player's control
void EEPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
}

// the class forwarders' upgrade of a unit class: NULL past the classes or without one
static EEUnitStats* EEPlayerClassStats(EEPlayer* inPlayer, unsigned long inUnitClass)
{
    return inUnitClass < EEUnitClassCount() ? inPlayer->mUnitClassUpgrades[inUnitClass] : NULL;
}
#define EE_PLAYER_CLASS_SET(inCall) if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass)) theStats->inCall
// the class media setters (EEEvent's, the civilization's): the player's upgrade of the unit class, if it has one
void EEPlayer::SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex) { EE_PLAYER_CLASS_SET(SetAcknowledgeSound(inSound, inIndex)); }
void EEPlayer::SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound) { EE_PLAYER_CLASS_SET(SetCreationSound(inSound)); }
void EEPlayer::SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound) { EE_PLAYER_CLASS_SET(SetDeathSound(inSound)); }
void EEPlayer::SetClassModel(unsigned long inUnitClass, EEModel* inModel) { EE_PLAYER_CLASS_SET(SetModel(inModel)); }
void EEPlayer::SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton) { EE_PLAYER_CLASS_SET(SetButton(inButton)); }
void EEPlayer::SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex) { EE_PLAYER_CLASS_SET(SetSelectionSound(inSound, inIndex)); }
void EEPlayer::LoadUnitClassButtons(long inUnitClass) { EE_PLAYER_CLASS_SET(LoadButtons(this)); }
void EEPlayer::SetUnitClassMedia(long inUnitClass, long inMediaFlags) { EE_PLAYER_CLASS_SET(LoadMedia(this, inMediaFlags)); }
void EEPlayer::UnloadUnitClassButtons(long inUnitClass) { EE_PLAYER_CLASS_SET(UnloadButtons(this)); }
void EEPlayer::UnloadUnitClassMedia(long inUnitClass) { EE_PLAYER_CLASS_SET(UnloadMedia(this)); }

// Marks the player as having met another player when it first sees a unit of a non-gaia player
void EEPlayer::NoticeForeignUnit(EEUnit* inUnit)
{
    if (this->mCombatState == 0 && inUnit->mPlayer->mPlayerID != 0)
        this->mCombatState = 1;
}

// Returns the town nearest to a tile, optionally only on the tile's continent; NULL when none
EEAIArea* EEPlayer::GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent)
{
    return static_cast<EEAIArea*>(reinterpret_cast<EETown*>(ee_units_player_rest(this, EE_UNITS_PLAYER_REST_NEAREST_AREA, inSameContinent, inTile.GetX(), inTile.GetY())));   // player_rest.rs
}

// Returns the player's population limit: the game's limit plus the player's bonuses
long EEPlayer::GetPopulationLimit(long inArg)
{
    return this->mPopulationLimitBonus + this->mShareBonus + gWorld.mPlayerPopulationLimit;
}

// The context of EEPlayer::Load and Save's operations (EEPlayerSaveOperation) and of LoadUnit's and SaveUnit's
// (EEPlayerUnitOperation), made in the call's argument (Self: the address the Rust passes back)
struct EEPlayerSaveContext {
    EEPlayer* mPlayer;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
    RTSUnit* mUnit;                         // SaveUnit's unit
    EEUnitClassUpgrade* mDummyUpgrade;      // the load's stand-in upgrade (UPGRADE_DUMMY_NEW .. DELETE)
    void* Self() { return this; }
};

static int32_t EEPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);
static int32_t RTSPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t, EeUnitsPlayerSaveIo* io);

// Loads the player's state from a saved game (the mirror of Save); returns the read status, some reads' errors ignored.
// port (R5b.4): the body is Rust (ee_units player_save.rs `load`: the original's four bodies, one per saved-game era,
// as one sequence with the eras' returns and assertion lines) over the record this class derives from
// (EeUnitsPlayerSaved) and EEPlayer::SaveOperation's parts
long EEPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_units_player_load(EEPlayerSaveContext{this, &inFile, inMajorVersion, inMinorVersion, NULL, NULL}.Self(), &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// The C++ parts of the player's Load and Save (ee_units player_save.rs): the file's primitives with the overload the
// original called (io holds the old value in, the read value out; a bool as 0 or 1) and the player's containers and
// sub-objects by operation
// The Rust saves' file primitives (ee/EEPlayer.h): the overload the original called; a read's io holds the old value
// in and the read value out (a bool as 0 or 1)
int32_t EESavePrimitive(FSFile& theFile, int32_t inOp, EeUnitsPlayerSaveIo* io)
{
    return ee_units_save_primitive(&theFile, inOp, io);   // ee_units save_prim.rs
}

int32_t EESaveFilePrimitive(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    return EESavePrimitive(*static_cast<FSFile*>(inFile), inOp, io);
}

static int32_t EEPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerSaveContext& theContext = *static_cast<EEPlayerSaveContext*>(inContext);
    EEPlayer* thePlayer = theContext.mPlayer;
    FSFile& theFile = *theContext.mFile;
    unsigned long theMajor = theContext.mMajorVersion;
    unsigned long theMinor = theContext.mMinorVersion;
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(theFile, inOp, io);
    switch (inOp) {
    // the load
    case EE_UNITS_PLAYER_SAVE_X_DUMMY_DELETE: delete theContext.mDummyUpgrade;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_RTS_LOAD: ee_units_rts_player_load(&theContext, &RTSPlayerSaveOperation, thePlayer, (uint32_t)theMajor, (uint32_t)theMinor);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_DELETE: delete thePlayer->mTechnologies;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD: thePlayer->mSavedGroups.Load(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_AIR_LOAD:
        return EEATCLoad(thePlayer->mAirTraffic[inIndex], &theFile);
    case EE_UNITS_PLAYER_SAVE_ACTIVE_LOAD:
        return thePlayer->mActiveGroup->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIV_LOAD:
        return thePlayer->mCivilization.Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_GROUP_LOAD:
        return thePlayer->mGroups[inIndex]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIVFILE_LOAD:
        return thePlayer->mCivilizationFile.Load(theFile);
    case EE_UNITS_PLAYER_SAVE_X_HERD_NEW: io->i = (int64_t)(uintptr_t)new EEHerd;   // (the Rust stores it: player_save.rs)
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_HERD_LOAD: return EEPlayerHerdAt(io->i)->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_LOAD:
        return thePlayer->mEvents.Load(theMajor, theMinor, theFile);
    // the save
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS: io->i = (int64_t)thePlayer->mUnitClassUpgrades.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT:
        return thePlayer->mUnitClassUpgrades[inIndex] != NULL;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE:
        return thePlayer->mUnitClassUpgrades[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_RTS_SAVE: ee_units_rts_player_save(&theContext, &RTSPlayerSaveOperation, thePlayer);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_SAVE:
        return ee_units_pg_save(reinterpret_cast<EEPPatrolGroup*>(io->i), (uint32_t)theMajor, (uint32_t)theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_SAVE: thePlayer->mSavedGroups.Save(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_AIR_SAVE:
        return EEATCSave(thePlayer->mAirTraffic[inIndex], &theFile);
    case EE_UNITS_PLAYER_SAVE_ACTIVE_SAVE:
        return thePlayer->mActiveGroup->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIV_SAVE:
        return thePlayer->mCivilization.Save(theFile);
    case EE_UNITS_PLAYER_SAVE_GROUP_SAVE:
        return thePlayer->mGroups[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIVFILE_SAVE:
        return thePlayer->mCivilizationFile.Save(theFile);
    case EE_UNITS_PLAYER_SAVE_HERD_SAVE:
        return EEPlayerHerdAt(io->i)->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_SAVE:
        return thePlayer->mEvents.Save(theMajor, theMinor, theFile);
    // the compound operations' statements (ee_units player_save_split.rs)
    case EE_UNITS_PLAYER_SAVE_X_DUMMY_CLEAR: theContext.mDummyUpgrade = NULL;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_DUMMY_NEW: theContext.mDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[inIndex]);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_DUMMY_VERIFY: ESVERIFY_AT(theContext.mDummyUpgrade != NULL, inIndex);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT: return (unsigned long)io->i < EEUnitClassCount() && thePlayer->mUnitClassUpgrades[(size_t)io->i] != NULL;
    case EE_UNITS_PLAYER_SAVE_X_UPGRADE_LOAD: return thePlayer->mUnitClassUpgrades[(size_t)io->i]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_X_DUMMY_LOAD: return theContext.mDummyUpgrade->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_X_TECH_NEW: thePlayer->mTechnologies = new EETechTree;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_COPY: *thePlayer->mTechnologies = gTechTree;
        return kErrorOK;
        return kErrorOK;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_LOAD: return thePlayer->mTechnologies->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD: return ee_units_pg_load(reinterpret_cast<EEPPatrolGroup*>(io->i), (uint32_t)theMajor, (uint32_t)theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_X_TECH_SAVE: return thePlayer->mTechnologies->Save(theMajor, theMinor, theFile);
    }
    ESVERIFY_AT(false, inOp);
    return kErrorOK;
}

// LoadUnit's and SaveUnit's C++ parts (ee_units player_save.rs load_unit, save_unit): the file's primitives, the
// class's fit, the loads, the saved unit's kind and its Save

static int32_t EEPlayerUnitOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerSaveContext& c = *static_cast<EEPlayerSaveContext*>(inContext);
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(*c.mFile, inOp, io);
    EEUnitClass* theClass = (uint64_t)io->i < gUnitClassTable.size() ? gUnitClassTable[(size_t)io->i] : NULL;
    EEUnit* theUnit = static_cast<EEUnit*>(c.mUnit);
    EEUnit* theIt = static_cast<EEUnit*>(EEPlayerUnitAt(io->i));   // a unit the Rust holds (player_save_split.rs)
    switch (inOp) {
    case EE_UNITS_PLAYER_SAVE_UNIT_SAVE: return theUnit->Save(c.mMajorVersion, c.mMinorVersion, *c.mFile);
    // the compound operations' statements (ee_units player_save_split.rs)
    case EE_UNITS_PLAYER_SAVE_X_UNIT_CREATE: io->i = EEPlayerUnitArg(theClass->CreateUnitAtTile(c.mPlayer, -10000, -10000, NULL, 3.1415927f));
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD: return theIt->Load(c.mMajorVersion, c.mMinorVersion, *c.mFile);
    case EE_UNITS_PLAYER_SAVE_X_UNIT_KILL: c.mPlayer->KillUnit(theIt, true);
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_COMPLEX_NEW: io->i = EEPlayerUnitArg(EEComplexUnit::Create());
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_NEW: io->i = EEPlayerUnitArg(EEUnit::Create());
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_REPAIR: theIt->RepairPointers(c.mMajorVersion, c.mMinorVersion);
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_CLEAR: theIt->Clear();
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_RELEASE: theIt->Dereference();
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_SAVED_TYPE: return (int32_t)theUnit->mUnitClass->mUnitType;
    case EE_UNITS_PLAYER_SAVE_X_SAVED_COMPLEX: return theUnit->mComplexUnit != NULL;
    }
    return 0;
}

// Reads one unit from a saved game and creates it on the player (ee_units player_save.rs load_unit)
long EEPlayer::LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_units_player_unit(EEPlayerSaveContext{this, &inFile, inMajorVersion, inMinorVersion, NULL, NULL}.Self(), &EEPlayerUnitOperation, this, false, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Makes every town of the player recheck whether it is near water
void EEPlayer::RestartTowns()
{
    for (long i = this->GetNumTowns() - 1; i >= 0; i--) {
        if (this->GetTown(i) != NULL)
            this->GetTown(i)->UpdateNearWater();
    }
}

// port (save version 0.260): the towns' ids and order. A load rebuilds every town from its units (RepairPointers),
// numbering them on from the saved counter (++mTownIDCounter) in the load's order, so the towns of the loaded game had
// other ids than the saved game's, which the saved AI state names its towns by (unit requests, defense packages,
// attack sub groups: GetTownByID), and stood in another order in mTowns, which the AI walks. Saved: the id counter
// and per player each town's id, hull points, counters and flags in mTowns' order; a rebuilt town whose hull equals a
// saved one takes its id and its counters, and when every town is matched they take the saved order. From 0.268 the
// towns are made from their saved areas instead. The save, the load, the pending towns and the matching are Rust
// (ee_units town_save.rs) over EETownSaveHost.

struct EETownSaveContext {
    FSFile* mFile;                      // the save's and the load's
    EEPlayer* mPlayer;                  // RepairPointers' player
    std::vector<unsigned char> mArea;   // EE_UNITS_TOWN_AREA's bytes
};

// the towns' save, load and repair (ee_units town_save.rs over town_split.rs: the compound operations are Rust, the
// town objects' reads and writes the player's body host's)
static int32_t EETownSaveHost(void* inContext, int32_t inOp, uint32_t inPlayer, uint32_t inIndex, EeUnitsTownIo* io)
{
    EETownSaveContext& theContext = *static_cast<EETownSaveContext*>(inContext);
    switch (inOp) {
    case EE_UNITS_TOWN_COUNTER: io->prim.i = gWorld.mTownIDCounter; return 0;
    case EE_UNITS_TOWN_SET_COUNTER: gWorld.mTownIDCounter = (long)io->prim.i; return 0;
    case EE_UNITS_TOWN_X_CONTEXT_PLAYER: io->prim.i = (int64_t)(uintptr_t)theContext.mPlayer; return 0;
    case EE_UNITS_TOWN_X_AREA_CLEAR: theContext.mArea.clear(); return 0;
    case EE_UNITS_TOWN_X_AREA_WRITE: EEWriteAreaBytes(*EEPlayerTownAt(io->prim.i), theContext.mArea); return 0;
    case EE_UNITS_TOWN_X_AREA_DATA: io->prim.i = (int64_t)(uintptr_t)theContext.mArea.data(); return 0;
    case EE_UNITS_TOWN_X_AREA_SIZE: io->prim.i = (int64_t)theContext.mArea.size(); return 0;
    case EE_UNITS_TOWN_X_FILE: io->prim.i = (int64_t)(uintptr_t)theContext.mFile; return 0;
    case EE_UNITS_TOWN_DEBUG: ESDebugOut("%.*s", (int)io->len, reinterpret_cast<const char*>(io->bytes)); return 0;
    }
    return 0;
}

long EESaveTowns(FSFile& outFile)
{
    EETownSaveContext theContext = {&outFile, NULL};
    return ee_units_towns_save(&theContext, &EETownSaveHost);
}

long EELoadTowns(FSFile& inFile, bool inAreas)
{
    EETownSaveContext theContext = {&inFile, NULL};
    uint8_t thePending = 0;
    long theResult = ee_units_towns_load(&theContext, &EETownSaveHost, inAreas, &thePending);
    if (thePending != 0)
        EEAfterLoad([]() { ee_units_towns_clear(); });   // the loaded game's own towns from here on
    return theResult;
}

// port (0.268): the player's towns made from the save, else false (rebuilt from the buildings); port (0.260): the
// rebuilt towns take their saved ids and order (ee_units town_save.rs make, apply)
static bool EEMakeSavedTowns(EEPlayer* inPlayer)
{
    EETownSaveContext theContext = {NULL, inPlayer};
    return ee_units_towns_repair(&theContext, &EETownSaveHost, (uint32_t)inPlayer->mPlayerID, false);
}

static void EEApplySavedTowns(EEPlayer* inPlayer)
{
    EETownSaveContext theContext = {NULL, inPlayer};
    ee_units_towns_repair(&theContext, &EETownSaveHost, (uint32_t)inPlayer->mPlayerID, true);
}

#include "lle/FSMemoryFile.h"

// Writes the player's state to a saved game; returns the write status (some writes' errors are ignored).
// port (R5b.4): the body is Rust (ee_units player_save.rs `save`) over the record this class derives from and
// EEPlayerSaveOperation's parts
long EEPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_units_player_save(EEPlayerSaveContext{this, &outFile, inMajorVersion, inMinorVersion, NULL, NULL}.Self(), &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Writes one unit's type, kind and data to a saved game (ee_units player_save.rs save_unit; path points saved as units
// from 0.254)
long EEPlayer::SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_units_player_unit(EEPlayerSaveContext{this, &outFile, inMajorVersion, inMinorVersion, inUnit, NULL}.Self(), &EEPlayerUnitOperation, this, true, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// VirtualBC's functor (RTTI EEPSuggestRecomputeAllAreaEffects; vtable 0x442b08: the shared ??_G 0x16b555, its
// Virtual04 0x16b543, the base's empty Virtual08): every area effect of each complex unit recomputed
class EEPSuggestRecomputeAllAreaEffects : public EEUUUnitFunctor {
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAreaEffects.MarkDirty(); }
};

// Marks the area effects of every unit of the player for recomputation
void EEPlayer::SuggestRecomputeAllAreaEffects()
{
    EEPSuggestRecomputeAllAreaEffects theFunctor;
    EEForEachUnitOf(this, &theFunctor, false);
}

#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "rust/game/game_log.h"
#include "rust/game/gamecpp.h"
#include "rust/game/physics.h"
#include "rust/sim/cp.h"
#include "rust/sim/tech.h"
#include "rust/game/rts.h"
#include "rust/game/tech_tree.h"

// The context of UpdatePlayer's operations: the player
struct EEPlayerTickContext {
    EEPlayer* mPlayer;
};

// The C++ parts of the player's tick (ee_units player_tick.rs): the groups, herds, air traffic, events, the queued
// improvements' map and the units, by operation
static int32_t EEPlayerTickOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerTickContext& theContext = *static_cast<EEPlayerTickContext*>(inContext);
    EEPlayer* thePlayer = theContext.mPlayer;
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(io->i));
    EEHerd* theHerd = EEPlayerHerdAt(ee_units_lists_at(thePlayer->mLists, EE_UNITS_LISTS_HERDS, inIndex));   // herd inIndex's
    switch (inOp) {
    case EE_UNITS_PLAYER_TICK_HERD_DUE:
        io->i = (int64_t)theHerd->mNextUpdateTime;
        io->b = theHerd->mIsActive ? 1 : 0;
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_UPDATE: theHerd->Update((unsigned long)io->i);
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_STATE:
        return (int32_t)theHerd->mState;
    case EE_UNITS_PLAYER_TICK_TICK_TIME: io->f = gPhysics->GetTickTime();
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_APPLY: thePlayer->ApplyImprovement((long)io->i, (long)inIndex, false);
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE:
        return thePlayer->AdvanceImprovement((unsigned long)io->i) ? 1 : 0;
    case EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS: theUnit->SetHitPoints((long)(int32_t)inIndex);
        return 0;
    // the compound operations' statements (ee_units player_tick.rs split)
    case EE_UNITS_PLAYER_TICK_X_VIEWED_ALONE: return thePlayer->mViewedUnit != NULL && thePlayer->mViewedUnit->mReferenceCount == 1;
    case EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE: return thePlayer->mSelectedViewedUnit != NULL && thePlayer->mSelectedViewedUnit->mReferenceCount == 1;
    case EE_UNITS_PLAYER_TICK_X_EVENTS: thePlayer->mEvents.TickEvents(thePlayer, gPhysics->mTickTime);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_AMOUNT: io->i = gWorldTable.GetLongValue(kWorldWonderHealAmount);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_STEP: io->i = gWorldTable.GetLongValue(kWorldWonderHealEpochStep);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR: io->f = gWorldTable.GetFloatValue(kWorldWonderHealEpochFactor);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEALED: return thePlayer->mHealedUnitSet->Contains(theUnit->mUnitClass->mUnitType);
    }
    return 0;
}

// Updates the player each tick: validates groups, herds, air traffic, events, improvement timers and wonder healing
// port (R5b.4): the body is Rust (ee_units player_tick.rs: the herds' and timers' decisions, the heal) over the
// record this class derives from and EEPlayerTickOperation's parts
void EEPlayer::UpdatePlayer()
{
    ee_app_capture_player_state((uint64_t)(uintptr_t)this, gPhysics != NULL ? (uint32_t)gPhysics->mGameTime : 0, this->mSavedGroups.mStats, this->mSavedGroups.mEndTime, this->mEvents.mList);   // TEST ONLY (PLST)
    EEPlayerTickContext theContext = {this};
    ee_units_player_update(&theContext, &EEPlayerTickOperation, static_cast<EeUnitsPlayerSaved*>(this), this);
}

// The C++ of EEPlayer's Rust bodies (ee_units player_body.rs; EE_UNITS_PLAYER_BODY_*): one statement of the original
// bodies each, in their order; inA an object or index, inB and inC values, inSwaps UnitsSwapped's vector
extern "C" int64_t ee_units_player_body_host(int32_t inOp, void* inPlayer, uint64_t inA, int64_t inB, int64_t inC, void* inSwaps)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    RTSUnit* theUnit = reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inA));
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    EEMedia* theMedia = reinterpret_cast<EEMedia*>(static_cast<uintptr_t>(inA));
    EEGroup* theGroup = static_cast<EEGroup*>(reinterpret_cast<EeUnitsGroup*>(static_cast<uintptr_t>(inA)));
    std::vector<EEPUnitSwap>* theSwaps = static_cast<std::vector<EEPUnitSwap>*>(inSwaps);
    switch (inOp) {
    case EE_UNITS_PLAYER_BODY_UNIT_REFERENCE: theUnit->Reference();
        return 0;
    case EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE: theUnit->Dereference();
        return 0;
    case EE_UNITS_PLAYER_BODY_EXECUTING_LOCAL: gWorld.mExecutingLocalSelection = inB != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_EXECUTE: reinterpret_cast<NEMessage*>(static_cast<uintptr_t>(inA))->Execute();
        return 0;
    case EE_UNITS_PLAYER_BODY_REPLACE_UNITS: ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_UNITS, theComplex, (int64_t)(uintptr_t)&(*theSwaps), 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_REPLACE: EEATCReplaceUnits(thePlayer->mAirTraffic[inB], *theSwaps);
        return 0;
    case EE_UNITS_PLAYER_BODY_UNBUILT_WALL: return theComplex->mUnitClass->mIsWall && !theComplex->mIsBuilt;
    case EE_UNITS_PLAYER_BODY_DOPPELGANGERS: ee_units_dm_body(EE_UNITS_DM_DO_UNREGISTER_PLAYER, thePlayer, 0, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_EVENTS_CLEAR: thePlayer->mEvents.Clear(inB != 0 ? thePlayer : NULL);
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_CLEAR: thePlayer->mSavedGroups.Clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_NAME: thePlayer->mSavedGroups.SetName(thePlayer->GetName());
        return 0;
    case EE_UNITS_PLAYER_BODY_UPGRADES_RESIZE: thePlayer->mUnitClassUpgrades.resize((size_t)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_UPGRADE_NEW: thePlayer->mUnitClassUpgrades[inB] = gUnitClassTable[inB] != NULL ? new EEUnitClassUpgrade(gUnitClassTable[inB]) : NULL;
        return 0;
    case EE_UNITS_PLAYER_BODY_UPGRADE_DELETE: delete thePlayer->mUnitClassUpgrades.begin()[inB];
        return 0;
    case EE_UNITS_PLAYER_BODY_UPGRADES_CLEAR: thePlayer->mUnitClassUpgrades.clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_NEW: thePlayer->mTechnologies = new EETechTree;
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_COPY: *thePlayer->mTechnologies = gTechTree;
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_DELETE: delete thePlayer->mTechnologies;
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_NEW: return (int64_t)(uintptr_t)EEATCNew();
    case EE_UNITS_PLAYER_BODY_WORLD_LONG: return gWorldTable.GetLongValue((EEWorldConstant)inB);
    case EE_UNITS_PLAYER_BODY_EFFECT: return (int64_t)(uintptr_t)static_cast<EEMedia*>(EEGraphicEffect::Get((unsigned long)inB));
    case EE_UNITS_PLAYER_BODY_MEDIA_LOAD: theMedia->LoadMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_MEDIA_UNLOAD: theMedia->UnloadMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_CIV_REAPPLY: thePlayer->mCivilization.Reapply(thePlayer, (unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_RTS_REPAIR: ee_rts_player_repair_pointers(thePlayer, (unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_MAKE_SAVED_TOWNS: return EEMakeSavedTowns(thePlayer);
    case EE_UNITS_PLAYER_BODY_LIVE_STATE: return EESaveHasLiveState((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_PLAYER_BODY_ADD_TO_TOWN: return ee_units_player_areas(EEPlayerRefs(thePlayer), 0, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_BODY_CIV_POINTS: return !thePlayer->mCivilization.GetNoPointsAdded() && thePlayer->mCivilization.GetAvailablePoints() != 0;
    case EE_UNITS_PLAYER_BODY_CIV_BLINK: return ee_screens_cb_set_blinking(true);
    case EE_UNITS_PLAYER_BODY_RECOUNT_POPULATION: thePlayer->RecountPopulation();
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_REPAIR: EEPlayerHerdAt(inA)->RepairPointers((unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_REGISTER_HERDS: ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_REGISTER, 0, 1, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_EVENTS_REPAIR: return thePlayer->mEvents.RepairEvents(thePlayer, (unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_PLAYER_BODY_REQUEUE: EEPlayerImprovement(thePlayer, 10, 0, 0);   // the queued improvements queued again (ee_units player.rs requeue)
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_OBSOLETE: return ee_gamecpp_tech_obj_obsolete(thePlayer->GetTechTree()->Get((long)inB));
    case EE_UNITS_PLAYER_BODY_TECH_ALLOW: thePlayer->GetTechTree()->Get((long)inB)->mIsAllowed = inC != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_APPLY_SAVED_TOWNS: EEApplySavedTowns(thePlayer);   // port (0.260): the rebuilt towns take their saved ids and order
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_REFRESH: EEPlayerTownAt(inA)->RefreshPatrolGroup();
        return 0;
    case EE_UNITS_PLAYER_BODY_ADD_OFF_MAP: thePlayer->AddOffMapUnit(theUnit);
        return 0;
    case EE_UNITS_PLAYER_BODY_RECOUNT_HEROES: EEPlayerUnits(thePlayer, 5);
        return 0;
    case EE_UNITS_PLAYER_BODY_NEAREST_TOWN: return (int64_t)(uintptr_t)theComplex->FindNearestTown();
    case EE_UNITS_PLAYER_BODY_TOWN_PATROL: return (int64_t)(uintptr_t)reinterpret_cast<EETown*>(static_cast<uintptr_t>(inA))->mPatrolGroup;
    case EE_UNITS_PLAYER_BODY_PATROL_NEAREST: return (int64_t)(uintptr_t)ee_units_pg_nearest(reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(inA)), (int32_t)inB, (int32_t)inC);
    case EE_UNITS_PLAYER_BODY_PATROL_OF: return (int64_t)(uintptr_t)theComplex->mPatrolGroup;
    case EE_UNITS_PLAYER_BODY_NEW_PATROL_GOAL: return (int64_t)(uintptr_t)static_cast<EEUnitGoal*>(EEUGRust::NewPatrol(reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(inB)), theComplex, true));
    case EE_UNITS_PLAYER_BODY_KEEP_MOBILE: static_cast<EEActiveGroup*>(theGroup)->KeepMobileUnits();
        return 0;
    case EE_UNITS_PLAYER_BODY_PLAN: static_cast<EEActiveGroup*>(theGroup)->Plan();
        return 0;
    case EE_UNITS_PLAYER_BODY_KILL: static_cast<EEUnit*>(theUnit)->Kill(0, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_KILL_UNIT: thePlayer->KillUnit(theUnit, false);
        return 0;
    // the second batch (player_body.rs)
    case EE_UNITS_PLAYER_BODY_LINE_OF_SIGHT: return (int32_t)UFloatBits(static_cast<EEUnit*>(theUnit)->GetStats()->GetLineOfSight());
    case EE_UNITS_PLAYER_BODY_COMPLEX_SIGHT_OFF: static_cast<EEUnit*>(theUnit)->GetComplexUnit()->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_BODY_SET_SIGHT: static_cast<EEUnit*>(theUnit)->GetComplexUnit()->SetSight(UBitsFloat((int32_t)inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_SIGHT_ON: theUnit->SightOnHere();
        return 0;
    case EE_UNITS_PLAYER_BODY_HOT_KEY_REMOVE: gUIManager->GetMainGameForm()->RemoveHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_WATERLESS: return (gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
    case EE_UNITS_PLAYER_BODY_NOT_SPACE: return !gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_PLAYER_BODY_HOT_KEY_ADD: gUIManager->GetMainGameForm()->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection((int32_t)inB, false, false, false)), NULL, NULL, NULL, 0, false);
        return 0;
    case EE_UNITS_PLAYER_BODY_HAS_CAMOUFLAGE: return gCivPowersManager.HasPower(thePlayer, kCivPowerCamouflage);
    case EE_UNITS_PLAYER_BODY_CAMOUFLAGE_SET: return (int64_t)(uintptr_t)gUnitSetManager.GetSet((reinterpret_cast<EECivPower*>(ee_tech_civ_power_record((int32_t)(long)(kCivPowerCamouflage))))->mUnitSet);
    case EE_UNITS_PLAYER_BODY_SET_HAS: return reinterpret_cast<EEUnitSet*>(static_cast<uintptr_t>(inA))->Contains((long)inB);
    case EE_UNITS_PLAYER_BODY_COMMAND_TYPE: return theComplex->mCommand ? theComplex->mCommand->mType : 0;
    case EE_UNITS_PLAYER_BODY_CLOAKABLE_OFF: theComplex->SetCloakableChain(false);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLOAKED_OFF: theComplex->SetCloakedChain(false);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLASS_STATS: return (int64_t)(uintptr_t)EEPlayerClassStats(thePlayer, (unsigned long)inB);
    case EE_UNITS_PLAYER_BODY_TERRAIN_MASK: return reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mTerrainMask;
    case EE_UNITS_PLAYER_BODY_TEAM_COUNT: return (int64_t)ee_app_teams_get(SIZE_MAX, NULL, 0);
    case EE_UNITS_PLAYER_BODY_TEAM_HAS: return ee_app_teams_contains((size_t)inB, (uint32_t)inC);
    case EE_UNITS_PLAYER_BODY_BUTTON_LOADS: return reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mButtonLoadCount;
    case EE_UNITS_PLAYER_BODY_IS_HUMAN: return thePlayer->IsHuman() == true;
    case EE_UNITS_PLAYER_BODY_BUTTON_RELEASE: reinterpret_cast<EEButtonObject*>(static_cast<uintptr_t>(inA))->Release();
        return 0;
    case EE_UNITS_PLAYER_BODY_BUTTON_ACQUIRE: reinterpret_cast<EEButtonObject*>(static_cast<uintptr_t>(inA))->Acquire();
        return 0;
    case EE_UNITS_PLAYER_BODY_MAP_SIZE: return (int64_t)gWorld.mMap->mSize;
    case EE_UNITS_PLAYER_BODY_TILE_PRESENT: return gWorld.mMap->GetTile((unsigned long)inB, (unsigned long)inC) != NULL;
    case EE_UNITS_PLAYER_BODY_TILE_VISIBILITY: gWorld.mMap->mTerrainMesh.SetTileReferenceCount((unsigned long)inB, (unsigned long)inC, gWorld.mMap->GetTile((unsigned long)inB, (unsigned long)inC)->GetVisibility(thePlayer->mPlayerID));
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_SET_INDEX: thePlayer->mTechnologies->SetPlayerIndex((long)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER: thePlayer->mTechnologies->SetPlayer(thePlayer);
        return 0;
    // the world lane, round 2 (player_rest.rs)
    case EE_UNITS_PLAYER_BODY_CONTINENT_ID: return gContinentManager.GetContinentID(inB, inC);
    case EE_UNITS_PLAYER_BODY_TOWN_CLOSEST: return EEPlayerPackPoint(EEPlayerTownAt(inA)->GetClosestPoint(U2DPoint<long>((long)inB, (long)inC)));
    case EE_UNITS_PLAYER_BODY_RTS_CHECKSUM: ee_rts_player_checksum(thePlayer, reinterpret_cast<ESChecksum*>(static_cast<uintptr_t>(inA)));
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_CHECKSUM: thePlayer->mTechnologies->GetChecksum(*reinterpret_cast<ESChecksum*>(static_cast<uintptr_t>(inA)));
        return 0;
    case EE_UNITS_PLAYER_BODY_ADD_CHECKSUM: reinterpret_cast<ESChecksum*>(static_cast<uintptr_t>(inA))->AddChecksum(UString("EEPlayer"), (unsigned long)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_ANIMAL_OF_TYPE: return (int64_t)(uintptr_t)EEAnimal::Find((long)inB);
    case EE_UNITS_PLAYER_BODY_ANIMAL_ID: return reinterpret_cast<EEAnimal*>(static_cast<uintptr_t>(inA))->mAnimalID;
    case EE_UNITS_PLAYER_BODY_HERD_ANIMAL: return EEPlayerHerdAt(inA)->mAnimalID;
    case EE_UNITS_PLAYER_BODY_HERDS_PUSH: reinterpret_cast<std::vector<EEHerd*>*>(static_cast<uintptr_t>(inA))->push_back(reinterpret_cast<EEHerd*>(inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_SERIAL: EEPlayerTownAt(inA)->SetSerial(gContinentManager.NewSerial());
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA: gContinentManager.AddArea(EEPlayerTownAt(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_SERIAL: EEPlayerHerdAt(inA)->SetSerial(gContinentManager.NewSerial());
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_ADD_AREA: gContinentManager.AddArea(EEPlayerHerdAt(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_GOAL_QUEUES: return (int64_t)theComplex->GetNumGoalQueues();
    case EE_UNITS_PLAYER_BODY_CLEAR_GOAL_QUEUE: ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, theComplex, (unsigned long)inB, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_FINISH_LOAD: static_cast<EEUnit*>(theUnit)->FinishLoad((long)inB, (long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLASS_UNLOAD_MEDIA: gUnitClassTable[(size_t)inB]->UnloadClassMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT: return (int64_t)reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->GetMediaCount();
    case EE_UNITS_PLAYER_BODY_STATS_LOAD_MEDIA: reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mUnitClass->LoadClassMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_TYPE: return (int64_t)reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mUnitClass->mUnitType;
    case EE_UNITS_PLAYER_BODY_GOALS0_SIZE: return (int64_t)theComplex->mGoalQueues[0].mGoals.size();
    case EE_UNITS_PLAYER_BODY_BUILD_QUEUE_SIZE: return (int64_t)theComplex->mCitizenBuildQueue.size();
    case EE_UNITS_PLAYER_BODY_ACTIVE_GROUP: return (int64_t)(uintptr_t)static_cast<EeUnitsGroup*>(theComplex->mActiveGroup);
    case EE_UNITS_PLAYER_BODY_GROUP_REMOVE: theGroup->RemoveUnit(EEPlayerUnitAt(inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_REMOVE_FROM_TILE: static_cast<EEUnit*>(theUnit)->mUnitClass->RemoveFromTile(static_cast<EEUnit*>(theUnit));
        return 0;
    case EE_UNITS_PLAYER_BODY_REMOVE_FROM_REGION: static_cast<EEUnit*>(theUnit)->mUnitClass->RemoveFromRegion(static_cast<EEUnit*>(theUnit));
        return 0;
    case EE_UNITS_PLAYER_BODY_INFLUENCE_REMOVE: gWorld.mInfluenceTable->RemoveUnit(static_cast<EEUnit*>(theUnit), (long)inB, (long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_NO_HUMAN_DEFEAT: return gPlatSimNoHumanDefeat;
    case EE_UNITS_PLAYER_BODY_NO_DEFEAT_HIT: gPlatSimNoHumanDefeatHits++;
        return 0;
    case EE_UNITS_PLAYER_BODY_TEXT_NEW: return (int64_t)(intptr_t)new UWideString;
    case EE_UNITS_PLAYER_BODY_TEXT_DELETE: delete EEPlayerPtr<UWideString>(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_TEXT_RESOURCE: return gResource->GetString((unsigned long)inB, *EEPlayerPtr<UWideString>(inA));
    case EE_UNITS_PLAYER_BODY_TEXT_FORMAT: sprintf(*EEPlayerPtr<UWideString>(inA), *EEPlayerPtr<UWideString>(inB), (const char*)*EEPlayerPtr<UWideString>(inC));
        return 0;
    case EE_UNITS_PLAYER_BODY_TEXT_NAME: *EEPlayerPtr<UWideString>(inA) = thePlayer->mName;
        return 0;
    case EE_UNITS_PLAYER_BODY_TEXT_CIV_NAME: sprintf(*EEPlayerPtr<UWideString>(inA), "%s - (%s)", (const char*)thePlayer->mCivilization.mName, (const char*)thePlayer->mName);
        return 0;
    case EE_UNITS_PLAYER_BODY_CHAT_RESULT: gUserInterface->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), *EEPlayerPtr<UWideString>(inA), *EEPlayerPtr<UWideString>(inB), gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_HULL_COUNT: return (int64_t)EEPlayerTownAt(inA)->GetHullPointCount();
    case EE_UNITS_PLAYER_BODY_HULL_POINT: return EEPlayerPackPoint(EEPlayerTownAt(inA)->GetHullPoint((long)inB));
    case EE_UNITS_PLAYER_BODY_TOWN_SET_PATROL: EEPlayerTownAt(inA)->mPatrolGroup = reinterpret_cast<EEPPatrolGroup*>(inB);
        return 0;
    // ClearFootprint's and ReloadTechTree's (player_rest.rs)
    case EE_UNITS_PLAYER_BODY_UNITS_NEW: return (int64_t)(uintptr_t)new std::vector<EEUnit*>;
    case EE_UNITS_PLAYER_BODY_TILES_NEW: return (int64_t)(uintptr_t)new std::vector<EETileCoordinate>;
    case EE_UNITS_PLAYER_BODY_UNITS_DELETE: delete EEPlayerPtr<std::vector<EEUnit*> >(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_TILES_DELETE: delete EEPlayerPtr<std::vector<EETileCoordinate> >(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_UNITS_SIZE: return (int64_t)EEPlayerPtr<std::vector<EEUnit*> >(inA)->size();
    case EE_UNITS_PLAYER_BODY_TILES_SIZE: return (int64_t)EEPlayerPtr<std::vector<EETileCoordinate> >(inA)->size();
    case EE_UNITS_PLAYER_BODY_UNITS_AT: return EEPlayerUnitArg((*EEPlayerPtr<std::vector<EEUnit*> >(inA))[(size_t)inB]);
    case EE_UNITS_PLAYER_BODY_TILES_AT: return EEPlayerPackPoint((*EEPlayerPtr<std::vector<EETileCoordinate> >(inA))[(size_t)inB]);
    case EE_UNITS_PLAYER_BODY_SET_MANAGER_CONTAINS: return gUnitSetManager.Contains((long)inB, (long)inC);
    case EE_UNITS_PLAYER_BODY_WAYPOINTS: return (int64_t)theComplex->mWaypoints.size();
    case EE_UNITS_PLAYER_BODY_NEXT_WAYPOINT: return theComplex->NextWaypoint();
    case EE_UNITS_PLAYER_BODY_PATH_START: return theComplex->GetPathStart(*EEPlayerPtr<GE3DPoint>(inB), *EEPlayerPtr<float>(inC));
    case EE_UNITS_PLAYER_BODY_MOVE_TO: theComplex->SetTransformation(EEPlayerMovedLocation(theComplex, EEPlayerPtr<float>(inB)), true);
        return 0;
    case EE_UNITS_PLAYER_BODY_SORT_TILES: std::sort(EEPlayerPtr<std::vector<EETileCoordinate> >(inA)->begin(), EEPlayerPtr<std::vector<EETileCoordinate> >(inA)->end(), EETileDistanceLess((long)inB, (long)inC));
        return 0;
    case EE_UNITS_PLAYER_BODY_TILE_EMPTY: return gWorld.mMap->GetTile((long)inB, (long)inC) != NULL && gWorld.mMap->GetTile((long)inB, (long)inC)->mUnits == NULL;
    case EE_UNITS_PLAYER_BODY_MESH_VALID: return gWorld.mMap->mTerrainMesh.GetIsValidTile((long)inB, (long)inC);
    case EE_UNITS_PLAYER_BODY_MESH_Z: return (uint32_t)UFloatBits(gWorld.mMap->mTerrainMesh.GetMeshZ(EEPlayerPtr<float>(inA)[0], EEPlayerPtr<float>(inA)[1]));
    case EE_UNITS_PLAYER_BODY_DROP_PATH: ee_units_waypoints_drop((uint64_t)(uintptr_t)theComplex);
        return 0;
    case EE_UNITS_PLAYER_BODY_START_IDLE: theComplex->StartAction(kActionIdle);
        return 0;
    case EE_UNITS_PLAYER_BODY_ACTION_CHANGED: theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    case EE_UNITS_PLAYER_BODY_MEMORY_FILE_NEW: return (int64_t)(uintptr_t)new FSMemoryFile(1000, true, 1);
    case EE_UNITS_PLAYER_BODY_MEMORY_FILE_DELETE: delete EEPlayerPtr<FSMemoryFile>(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_SAVE: thePlayer->GetTechTree()->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, *EEPlayerPtr<FSMemoryFile>(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_SHARE_REMOVE: gWorld.RemoveShareBonus();
        return 0;
    case EE_UNITS_PLAYER_BODY_MEMORY_FILE_REWIND: EEPlayerPtr<FSMemoryFile>(inA)->SetPosition(0, kFileOriginBegin, false);
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_LOAD: return thePlayer->GetTechTree()->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, *EEPlayerPtr<FSMemoryFile>(inA));
    case EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE: thePlayer->GetTechTree()->ResetEpochRange((long)inB, (long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED: thePlayer->GetTechTree()->ApplySavedState(true, inB != 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_UNLOAD_CLASS_MEDIA: thePlayer->UnloadUnitClassMedia((long)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLASS_NAME: return (int64_t)(uintptr_t)(const char*)gUnitClassTable[(size_t)inB]->mName;
    case EE_UNITS_PLAYER_BODY_HERD_DELETE: delete EEPlayerHerdAt(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_UNITS: EEPlayerHerdAt(inA)->GetUnits(*EEPlayerPtr<std::vector<EEComplexUnit*> >(inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_COMPLEXES_NEW: return (int64_t)(uintptr_t)new std::vector<EEComplexUnit*>;
    case EE_UNITS_PLAYER_BODY_COMPLEXES_DELETE: delete EEPlayerPtr<std::vector<EEComplexUnit*> >(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_COMPLEXES_SIZE: return (int64_t)EEPlayerPtr<std::vector<EEComplexUnit*> >(inA)->size();
    case EE_UNITS_PLAYER_BODY_COMPLEXES_AT: return (int64_t)(uintptr_t)(*EEPlayerPtr<std::vector<EEComplexUnit*> >(inA))[(size_t)inB];
    case EE_UNITS_PLAYER_BODY_KILL_KILLED: EEPlayerPtr<EEComplexUnit>(inA)->Kill(inB != 0, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_DUMP_HEADER: fprintf(*EEPlayerPtr<FSFileSpec>(inA), "Media Information For Player %ld, The Players Name is %s\n\n", thePlayer->mPlayerID, (const char*)thePlayer->mName);
        return 0;
    case EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS: return (int64_t)thePlayer->mUnitClassUpgrades.size();
    case EE_UNITS_PLAYER_BODY_UPGRADE_AT: return (int64_t)(uintptr_t)thePlayer->mUnitClassUpgrades[(size_t)inB];
    case EE_UNITS_PLAYER_BODY_DUMP_COUNT: EEPlayerPtr<EEUnitStats>(inB)->DumpMediaCount(*EEPlayerPtr<FSFileSpec>(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_DUMP_TEXT: fprintf(*EEPlayerPtr<FSFileSpec>(inA), "%s", EEPlayerPtr<const char>(inB));
        return 0;
    // the towns' save host's (town_split.rs)
    case EE_UNITS_PLAYER_BODY_TOWN_NEW: return (int64_t)(uintptr_t)new EETown(thePlayer, (long)inB);
    case EE_UNITS_PLAYER_BODY_TOWN_DELETE: delete EEPlayerTownAt(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_READ_AREA: return EEReadAreaBytes(std::vector<unsigned char>(EEPlayerPtr<unsigned char>(inB), EEPlayerPtr<unsigned char>(inB) + inC), *EEPlayerTownAt(inA));
    }
    return 0;
}

// ---- the base player's (RTSPlayer's, folded into EEPlayer) save: its vision flags, ids, color (deriving the shade
// color), name, units and observer mode (ee_units rts_player_save.rs over EeUnitsRtsPlayerFields and these parts);
// units.rs's shims
static int32_t RTSPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t, EeUnitsPlayerSaveIo* io)
{
    EEPlayerSaveContext& theContext = *static_cast<EEPlayerSaveContext*>(inContext);
    EEPlayer* thePlayer = theContext.mPlayer;
    FSFile& theFile = *theContext.mFile;
    switch (inOp) {
    case EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD: return thePlayer->mColor.Load(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_SHADE:
        thePlayer->mShadeColor.SetRed(thePlayer->mColor.GetRed() * 0.6f);
        thePlayer->mShadeColor.SetGreen(thePlayer->mColor.GetGreen() * 0.6f);
        thePlayer->mShadeColor.SetBlue(thePlayer->mColor.GetBlue() * 0.6f);
        return 0;
    case EE_UNITS_RTS_PLAYER_SAVE_NAME_LOAD: return thePlayer->mName.Load(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_LOAD_UNIT:
        return thePlayer->LoadUnit(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING: thePlayer->AddPendingUnits(true, false); return 0;
    case EE_UNITS_RTS_PLAYER_SAVE_COLOR_SAVE: return thePlayer->mColor.Save(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE: return thePlayer->mName.Save(theFile);
    case EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS:
        return ee_rts_player_save_units(thePlayer, theContext.mMajorVersion, theContext.mMinorVersion, &theFile);
    }
    return EESavePrimitive(theFile, inOp, io);
}

// The shims units.rs calls (UnitsCpp: the player's side)
extern "C" {
EeUnitsRtsPlayerFields* ee_rtspcpp_player_fields(RTSPlayer* inPlayer) { return inPlayer; }
EeRtsUnitBucket* ee_rtspcpp_buckets(RTSPlayer* inPlayer) { return inPlayer->mUnitBuckets; }
EEUnit** ee_rtspcpp_first_slot(RTSPlayer* inPlayer) { return &inPlayer->mFirstUnit; }
EEUnit** ee_rtspcpp_last_slot(RTSPlayer* inPlayer) { return &inPlayer->mLastUnit; }
size_t ee_rtspcpp_pending_size(RTSPlayer* inPlayer) { return inPlayer->mPendingUnits.size(); }
RTSUnit* ee_rtspcpp_pending_at(RTSPlayer* inPlayer, size_t inIndex) { return inPlayer->mPendingUnits[inIndex]; }
void ee_rtspcpp_pending_clear(RTSPlayer* inPlayer) { inPlayer->mPendingUnits.clear(); }
size_t ee_rtspcpp_killed_size(RTSPlayer* inPlayer) { return inPlayer->mKilledUnits.size(); }
RTSUnit* ee_rtspcpp_killed_at(RTSPlayer* inPlayer, size_t inIndex) { return inPlayer->mKilledUnits[inIndex]; }
void ee_rtspcpp_killed_clear(RTSPlayer* inPlayer) { inPlayer->mKilledUnits.clear(); }
void ee_rtspcpp_killed_push(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->mKilledUnits.push_back(inUnit); }
void ee_rtspcpp_add_unit(RTSPlayer* inPlayer, RTSUnit* inUnit, bool inKeepID) { inPlayer->AddUnit(inUnit, inKeepID); }
void ee_rtspcpp_release_pending(RTSPlayer* inPlayer) { inPlayer->ReleasePendingUnits(); }
void ee_rtspcpp_removing_unit(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->RemovingUnit(inUnit); }
void ee_rtspcpp_unit_removed(RTSPlayer* inPlayer, RTSUnit* inUnit) { inPlayer->UnitRemoved(inUnit); }
void ee_rtspcpp_removals_done(RTSPlayer* inPlayer) { inPlayer->RemovalsDone(); }
long ee_rtspcpp_save_unit(RTSPlayer* inPlayer, RTSUnit* inUnit, unsigned long inMajor, unsigned long inMinor, FSFile* inFile) { return inPlayer->SaveUnit(inUnit, inMajor, inMinor, *inFile); }
void ee_rtspcpp_name_checksum(RTSPlayer* inPlayer, ESChecksum* ioChecksum) { inPlayer->mName.GetChecksum(*ioChecksum); }
void ee_rtspcpp_checksum_add(ESChecksum* ioChecksum, const char* inName, unsigned long inValue) { ioChecksum->AddChecksum(inName, inValue); }
void ee_rtsucpp_store_insert(RTSUnit* inUnit, unsigned long inOwner, RTSPlayer* inPlayer) { ee_units_store_insert_object((uint64_t)(uintptr_t)inUnit, (uint32_t)inOwner, (uint64_t)(uintptr_t)static_cast<const RTSPlayer*>(inPlayer)); }
}

// ---- EEPPlayerEventList (EEPPlayerEventList.cpp, vtable 0x442b54)
// port (the player lane, round 1): the events and every decision on them are Rust (ee_units player_events.rs, mList);
// this is their world: the sight at an event's location and the local view of it, the local player, the alert sounds
// (loaded with the list), the mini-map ping and the chat text

// the alert sounds (the event's kind - 1)
static const unsigned long kEEPlayerEventSounds[5] = {kSoundUnderAttack, kSoundGarrison, kSoundFlare, 0x7d0, 0x7d0};

// the list's world, one statement an operation (the compound ones are Rust: player_events_split.rs)
int32_t EEPPlayerEventList::Host(void* inList, void* inPlayer, int32_t inOp, const EeUnitsPlayerEvent* inEvent)
{
    EEPPlayerEventList* theList = static_cast<EEPPlayerEventList*>(inList);
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    switch (inOp) {
    case EE_UNITS_EVENTS_IS_LOCAL: return thePlayer == gWorld.GetCurrentPlayer();
    case EE_UNITS_EVENTS_PLAYING: return theList->mSound != NULL && gSoundManager->GetIsPlaying(theList->mSound);
    case EE_UNITS_EVENTS_TEXT:
        if (inEvent->text != -1) {
            UWideString theText;
            gResource->GetString(inEvent->text, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
        }
        return 0;
    case EE_UNITS_EVENTS_STOP_PING: gMiniMap->mB12185 = false;
        return 0;
    case EE_UNITS_EVENTS_X_SIGHT_ON: thePlayer->AddSharedSightAt(EETileLocation(inEvent->x, inEvent->y));
        return 0;
    case EE_UNITS_EVENTS_X_SIGHT_OFF: thePlayer->RemoveSharedSightAt(EETileLocation(inEvent->x, inEvent->y));
        return 0;
    case EE_UNITS_EVENTS_X_CELL_ON: ee_gamecpp_minimap_count_cell(gMiniMap, inEvent->x, inEvent->y, true);
        return 0;
    case EE_UNITS_EVENTS_X_CELL_OFF: ee_gamecpp_minimap_count_cell(gMiniMap, inEvent->x, inEvent->y, false);
        return 0;
    case EE_UNITS_EVENTS_X_TILE_ON: gWorld.mMap->mTerrainMesh.ReferenceTile(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_TILE_OFF: gWorld.mMap->mTerrainMesh.DereferenceTile(inEvent->x, inEvent->y);
        return 0;
    case EE_UNITS_EVENTS_X_HAS_SOUND: return theList->mSounds[inEvent->kind - 1] != NULL;
    case EE_UNITS_EVENTS_X_PLAY: theList->mPlayed = gSoundManager->PlayAndReturnReferencedNode(theList->mSounds[inEvent->kind - 1]->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_EVENTS_X_HAS_CURRENT: return theList->mSound != NULL;
    case EE_UNITS_EVENTS_X_CURRENT_RELEASE: theList->mSound->Dereference();
        return 0;
    case EE_UNITS_EVENTS_X_CURRENT_SET: theList->mSound = theList->mPlayed;
        return 0;
    case EE_UNITS_EVENTS_X_HAS_PLAYED: return theList->mPlayed != NULL;
    case EE_UNITS_EVENTS_X_PLAYED_REFERENCE: theList->mPlayed->Reference();
        return 0;
    case EE_UNITS_EVENTS_X_PING_X: gMiniMap->mU1216C.SetX(inEvent->source_x);
        return 0;
    case EE_UNITS_EVENTS_X_PING_Y: gMiniMap->mU1216C.SetY(inEvent->source_y);
        return 0;
    case EE_UNITS_EVENTS_X_PING_ON: gMiniMap->mB12185 = true;
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_GET: theList->mSounds[inEvent->kind - 1] = EESoundObject::Get(kEEPlayerEventSounds[inEvent->kind - 1]);
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_LOAD: theList->mSounds[inEvent->kind - 1]->LoadMedia();
        return 0;
    case EE_UNITS_EVENTS_X_SOUND_UNLOAD: theList->mSounds[inEvent->kind - 1]->UnloadMedia();
        return 0;
    }
    return 0;
}

void EEPPlayerEventList::AddEvent(const EETileLocation& inTile, long inType, EEPlayer* inPlayer, long inTextID, bool inForceNew)
{
    this->AddPlayerEvent(inTile, EETileLocation(-10000, -10000), inType, inPlayer, inTextID, inForceNew);
}

void EEPPlayerEventList::AddPlayerEvent(const EETileLocation& inTile, const EETileLocation& inFrom, long inType, EEPlayer* inPlayer,
                                        long inTextID, bool inForceNew)
{
    ee_units_events_add(this->mList, this, inPlayer, &EEPPlayerEventList::Host, inTile.GetX(), inTile.GetY(), inFrom.GetX(),
                        inFrom.GetY(), inType, inTextID, inForceNew);
}

bool EEPPlayerEventList::GetNextEventSource(EETileLocation& outSource)
{
    return ee_units_events_next(this->mList, &outSource.mX, &outSource.mY);   // (0, 0) without one
}

// the statistics (EEPlayerStatistics, ee/EEPlayer.h; the series are Rust's: ee_units player_stats.rs)
// Set's gate (a shutdown, a saved game loading, the scenario editor outside the game screen) and the game time, then
// the series' op (0 Set, 1 raised, 2 lowered)
void EEPlayerStatistics::Record(uint32_t inOp, long inSeries, long inValue)
{
    ee_units_stats_set(this->mStats, inOp, inSeries, inValue, gPhysics != NULL ? gPhysics->mGameTime : 0,
                       UShutdownInProgress() ? EE_UNITS_STATS_SHUTDOWN : gWorld.mLoadingSavedGame ? EE_UNITS_STATS_LOADING
                       : gUIManager->mInScenarioEditor && !gWorld.mGameScreenActive ? EE_UNITS_STATS_EDITOR : 0);
}

// out of line from ee/EEPlayer.h (its Rust function headers stay out of it)

EEPPlayerEventList::EEPPlayerEventList() : mList(ee_units_events_new()), mSound(NULL), mPlayed(NULL)
{ ee_units_events_sounds(this, &EEPPlayerEventList::Host, true); }

EEPPlayerEventList::~EEPPlayerEventList() { ee_units_events_destroy(this->mList, this, &EEPPlayerEventList::Host); }

long EEPPlayerEventList::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{ return ee_units_events_file(this->mList, &inFile, &EESaveFilePrimitive, false); }

long EEPPlayerEventList::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{ return ee_units_events_file(this->mList, &outFile, &EESaveFilePrimitive, true); }

long EEPPlayerEventList::RepairEvents(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->Call(inPlayer, EE_UNITS_EVENTS_REPAIR, 0.0f);
    return 0;
}

void EEPPlayerEventList::TickEvents(EEPlayer* inPlayer, float inElapsed)
{ this->Call(inPlayer, EE_UNITS_EVENTS_TICK, inElapsed); }

void EEPPlayerEventList::ShowEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_SHOW_ALL, 0.0f); }
void EEPPlayerEventList::HideEvents(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_HIDE_ALL, 0.0f); }
void EEPPlayerEventList::Clear(EEPlayer* inPlayer) { this->Call(inPlayer, EE_UNITS_EVENTS_CLEAR, 0.0f); }

void EEPPlayerEventList::Call(EEPlayer* inPlayer, uint32_t inWhat, float inElapsed)
{ ee_units_events_call(this->mList, this, inPlayer, &EEPPlayerEventList::Host, inWhat, inElapsed); }

void EEGroup::Add(EEComplexUnit* inUnit) { ee_units_group_do(this, 0, inUnit); }
bool EEGroup::Contains(EEComplexUnit* inUnit) { return ee_units_group_contains(this, inUnit); }

EEComplexUnit* EEGroup::GetUnit(long inIndex)
{ return static_cast<EEComplexUnit*>(ee_units_group_unit(this, inIndex)); }

unsigned long EEGroup::GetNumUnits() { return ee_units_group_len(this); }

long EEGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{ return ee_units_group_file(this, false, &inFile, &EESaveFilePrimitive); }

void EEGroup::Clear() { ee_units_group_do(this, 2, NULL); }
void EEGroup::RemoveUnit(EEComplexUnit* inUnit) { ee_units_group_do(this, 1, inUnit); }

long EEGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{ return ee_units_group_file(this, true, &outFile, &EESaveFilePrimitive); }

void EEGroup::GetBounds(GERectangle& outBounds)
{ ee_units_group_bounds(this, reinterpret_cast<float(*)[4]>(&outBounds)); }

void EEGroup::Assign(EEGroup* inGroup) { ee_units_group_with(this, 3, inGroup); }

EEComplexUnit* EEGroup::FindUnitOfCategory(long inCategory)
{ return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 3, inCategory))); }

EEComplexUnit* EEGroup::FindUnitWithFlags(unsigned long inFlags)
{ return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_group_get(this, 4, (int64_t)inFlags))); }

bool EEGroup::HasUnitType(long inKey) { return ee_units_group_get(this, 1, inKey) != 0; }

EESelectedGroup& EESelectedGroup::CopySelection(EESelectedGroup* inGroup)
{ ee_units_group_with(this, 2, inGroup); return *this; }

void EESelectedGroup::AddUnit(EEComplexUnit* inUnit, bool inNeedsSort)
{ ee_units_group_do(this, inNeedsSort ? 11 : 12, inUnit); }

unsigned short EESelectedGroup::GetContinent() { return (unsigned short)ee_units_group_get(this, 5, 0); }

EEActiveGroup* EEActiveGroup::New(EEPlayer* inPlayer)
{ return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(ee_units_group_new(2, inPlayer, 0))); }

void EEActiveGroup::Finish() { ee_units_group_do(this, 15, NULL); }

bool EEActiveGroup::IsReserved(const GE3DPoint& inLocation)
{ return ee_units_ag_reserved(H(this), (const float(*)[3])&inLocation.mX, 0) != 0; }

void EEActiveGroup::SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget)
{ ee_units_ag_split_calamity_casters(H(this), H(inCalamity), H(inNode), H(inTarget)); }

void EEActiveGroup::SetGoal(EEUnitGoal* inGoal, bool inFlag) { ee_units_group_set_goal(this, inGoal, inFlag); }
void EEActiveGroup::Plan() { ee_units_ag_plan(H(this)); }

void EEActiveGroup::SpreadAroundTile(long inTileX, long inTileY)
{ ee_units_ag_spread_around_tile(H(this), (int32_t)inTileX, (int32_t)inTileY, 0); }

void EEActiveGroup::SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY)
{ ee_units_ag_spread_around_unit(H(this), H(inTarget), (int32_t)inTileX, (int32_t)inTileY); }

void EEActiveGroup::KeepMobileUnits() { ee_units_ag_keep_mobile(H(this)); }

void EEActiveGroup::TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{ ee_units_group_tell(this, inUnit, (const float(*)[3])&inLocation.mX); }

EEStatisticsSeries::EEStatisticsSeries() : mPoints(1, EeUnitsStatPoint{0, 0}), mMaxValue(0), mKeepHistory(false) {}
EEPlayerStatistics::EEPlayerStatistics() : mStats(ee_units_stats_new()), mEndTime(0) {}

EEPlayerStatistics::EEPlayerStatistics(const EEPlayerStatistics& inOther) : mName(inOther.mName), mStats(ee_units_stats_new()), mEndTime(inOther.mEndTime)
{
    ee_units_stats_copy(this->mStats, inOther.mStats);
}

EEPlayerStatistics::~EEPlayerStatistics() { ee_units_stats_free(this->mStats); }

long EEPlayerStatistics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    return theResult != 0 ? theResult : ee_units_stats_load(this->mStats, &inFile, &EESaveFilePrimitive, &this->mEndTime);
}

long EEPlayerStatistics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    return theResult != 0 ? theResult : ee_units_stats_save(this->mStats, &outFile, &EESaveFilePrimitive, this->mEndTime);
}

unsigned long EEPlayerStatistics::GetValue(unsigned long inSeries)
{ return ee_units_stats_value(this->mStats, inSeries, 0); }

unsigned long EEPlayerStatistics::GetLastValue(unsigned long inSeries)
{ return ee_units_stats_value(this->mStats, inSeries, 1); }

unsigned long EEPlayerStatistics::CountIncreases(unsigned long inSeries)
{ return ee_units_stats_value(this->mStats, inSeries, 2); }

void EEPlayerStatistics::Clear()
{ this->mName = UWideString(); this->mEndTime = 0; ee_units_stats_clear(this->mStats); }

EEPlayerStatistics& EEPlayerStatistics::operator=(const EEPlayerStatistics& inStatistics)
{
    this->mName = inStatistics.mName;
    this->mEndTime = inStatistics.mEndTime;
    ee_units_stats_copy(this->mStats, inStatistics.mStats);
    return *this;
}

EEStatisticsSeries EEPlayerStatistics::GetSeries(unsigned long inSeries)
{
    EEStatisticsSeries theSeries;
    theSeries.mPoints.resize(ee_units_stats_value(this->mStats, inSeries, 3));
    if (!theSeries.mPoints.empty())
        ee_units_stats_points(this->mStats, inSeries, theSeries.mPoints.data(), theSeries.mPoints.size());
    theSeries.mMaxValue = ee_units_stats_value(this->mStats, inSeries, 0);
    theSeries.mKeepHistory = ee_units_stats_value(this->mStats, inSeries, 4) != 0;
    return theSeries;
}

EEPlayer::EEPlayer()
{ ee_rts_player_reset(this); ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 0, 0, 0.0f); }

EEPlayer::EEPlayer(const UWideString& inName, unsigned long inPlayerID)
{
    ee_rts_player_reset(this);
    this->mName = inName;
    this->mPlayerID = inPlayerID;
    this->mSaveVisMap = inPlayerID != 0;
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 1, (int64_t)inPlayerID, 0.0f);
}

EEPlayer::~EEPlayer()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_DESTROY, 0, 0, 0, 0.0f);
    ee_units_store_clear_player_object((uint64_t)(uintptr_t)this);
}

void EEPlayer::Clear() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_CLEAR, 0, 0, 0, 0.0f); }
void EEPlayer::RemovalsDone() { ee_units_groups_player(this, 3); }

long EEPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{ return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_REPAIR_POINTERS, inFirst, (int64_t)inSecond, 0, 0.0f); }

void EEPlayer::AddPendingUnits(bool inKeepID, bool inReleasePending)
{ ee_rts_eeplayer_add_pending_units(this, this->mGroupLists, inKeepID, inReleasePending); }

void EEPlayer::ReleasePendingUnits() { ee_rts_eeplayer_release_pending_units(this, this->mGroupLists); }

void EEPlayer::GetChecksum(ESChecksum& ioChecksum)
{ ee_units_player_rest(this, EE_UNITS_PLAYER_REST_CHECKSUM, (uint64_t)(uintptr_t)&ioChecksum, 0, 0); }

void EEPlayer::UnitRemoved(RTSUnit* inUnit)
{ ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_REMOVED, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }

void EEPlayer::RemovingUnit(RTSUnit* inUnit)
{ ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REMOVING_UNIT, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }

void EEPlayer::RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REFUND_TECHNOLOGY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL); }

long EEPlayer::GetHeroCount(bool inByClass, bool inRequests)
{ return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_NUM_HEROES, NULL, inByClass, inRequests, 0, 0, 0, 0.0f, NULL); }

bool EEPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_PRODUCE, NULL, inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, 0.0f, outReason) != 0; }

bool EEPlayer::CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD_REPAIR, inUnit, (int64_t)reinterpret_cast<uintptr_t>(inOther), inValue, 0, 0, 0, 0.0f, NULL) != 0; }

EEUnit* EEPlayer::GetNearestConstructionSite(EEComplexUnit* inUnit)
{ return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(ee_units_player_body(this, EE_UNITS_PLAYER_DO_NEAREST_SITE, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0.0f))); }

bool EEPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD, inTechnology, inUnitType, inCount, 0, 0, 0, 0.0f, NULL) != 0; }

void EEPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_DAMAGE, 0, inPlayer, inAmount, 0.0f); }

void EEPlayer::PlayAcknowledgement(EEComplexUnit* inUnit)
{ ee_units_selection(this, 9, inUnit, 0, NULL, 0, false, false); }

void EEPlayer::RefreshTownGroups() { ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REFRESH_TOWN_GROUPS, 0, 0, 0); }

long EEPlayer::FinishUnitsLoad(long inMajorVersion, long inMinorVersion)
{ return (long)ee_units_player_rest(this, EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD, 0, inMajorVersion, inMinorVersion); }

void EEPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY_IMPROVEMENT, inClass, inImprovement, 0, 0, 0, 0, 0.0f, NULL); }

void EEPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL); }

void EEPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{ ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_GOAL_DONE, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0); }

bool EEPlayer::AdvanceImprovement(unsigned long inUnitType)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_ADVANCE_IMPROVEMENT, NULL, (int32_t)inUnitType, 0, 0, 0, 0, 0.0f, NULL) != 0; }

void EEPlayer::DeselectUnit(EEUnit* inUnit) { ee_units_selection(this, 7, inUnit, 0, NULL, 0, false, false); }
void EEPlayer::EnableCamouflagePower() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 1, 0, 0.0f); }

unsigned long EEPlayer::GetNumTowns() const
{ return (unsigned long)ee_units_lists_len(this->mLists, EE_UNITS_LISTS_TOWNS); }

EETown* EEPlayer::GetTown(unsigned long inIndex) const
{ return reinterpret_cast<EETown*>(static_cast<uintptr_t>(ee_units_lists_at(this->mLists, EE_UNITS_LISTS_TOWNS, inIndex))); }

void EEPlayer::ApplyImprovement(long inUnitType, long inImprovement, bool inFlag)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_APPLY_IMPROVEMENT, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL); }

EEWUnitsOffMapInfo* EEPlayer::GetOffMapInfo(EEComplexUnit* inUnit)
{ return ee_units_off_map_info(this->mLists, (int64_t)(uintptr_t)(void*)inUnit); }

void EEPlayer::UnselectAllUnits() { ee_units_player_body(this, EE_UNITS_PLAYER_DO_UNSELECT_ALL, 0, 0, 0, 0.0f); }

EEPPatrolGroup* EEPlayer::GetPatrolGroup(unsigned long inID)
{ return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(ee_units_player_body(this, EE_UNITS_PLAYER_DO_PATROL_GROUP, inID, 0, 0, 0.0f))); }

void EEPlayer::AddPatrolGroup(EEPPatrolGroup* inGroup)
{ ee_units_lists_add(this->mLists, EE_UNITS_LISTS_PATROLS, (uint64_t)(uintptr_t)inGroup); }

long EEPlayer::CountQueuedImprovements(unsigned long inUnitType, long inArg) const
{ return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_COUNT_QUEUED, NULL, (int32_t)inUnitType, inArg, 0, 0, 0, 0.0f, NULL); }

void EEPlayer::RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REMOVE_QUEUED, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL); }

bool EEPlayer::CanBuy(long inUnitType, long inImprovement, UWideString* outText)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_BUY, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outText) != 0; }

void EEPlayer::QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid)
{ ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_QUEUE_IMPROVEMENT, NULL, (int32_t)inUnitType, (int32_t)inImprovement, inFront, inAlreadyPaid, 0, 0.0f, NULL); }

bool EEPlayer::CanImprove(long inUnitType, long inImprovement, UWideString* outReason)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_IMPROVE, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outReason) != 0; }

void EEPlayer::PulseSight(bool inByID, float inSight, long inID)
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_PULSE_SIGHT, (uint64_t)(int64_t)inID, inByID, 0, inSight); }

unsigned long EEPlayer::GetTeamIndex()
{ return (unsigned long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_TEAM_INDEX, 0, 0, 0, 0.0f); }

void EEPlayer::EraseQueuedImprovements(unsigned long inUnitType)
{ ee_units_queued_op(this->mQueued, 0, inUnitType, 0); }

void EEPlayer::MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo)
{ ee_units_queued_op(this->mQueued, 1, inFrom, inTo); }

long EEPlayer::GetResearchedImprovement(unsigned long inUnitType)
{ return (long)ee_units_queued_op(this->mQueued, 2, inUnitType, 0); }

float EEPlayer::GetImprovementProgress(unsigned long inUnitType)
{ return ee_units_queued_timer(this->mQueued, inUnitType); }

void EEPlayer::AssignHerds() { ee_units_player_areas(EEPlayerRefs(this), 5, NULL); }
void EEPlayer::AssignTowns() { ee_units_player_areas(EEPlayerRefs(this), 4, NULL); }

void EEPlayer::SetSelectedViewedUnit(RTSUnit* inUnit)
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED, (uint64_t)(int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0.0f); }

void EEPlayer::ExecuteSelectionMessage(NEMessage* inMessage)
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_EXECUTE_SELECTION, (uint64_t)(uintptr_t)inMessage, 0, 0, 0.0f); }

void EEPlayer::AddSightSharer(EEPlayer* inPlayer)
{ ee_units_player_diplomacy(EEPlayerRefs(this), 1, inPlayer, 0, false); }

void EEPlayer::RemoveSightSharer(EEPlayer* inPlayer)
{ ee_units_player_diplomacy(EEPlayerRefs(this), 2, inPlayer, 0, false); }

void EEPlayer::AddStrandedUnit(EEUnit* inUnit)
{ ee_units_lists_add(this->mLists, EE_UNITS_LISTS_STRANDED, (int64_t)(uintptr_t)(void*)inUnit); }

void EEPlayer::AddConstructionSite(EEComplexUnit* inUnit)
{
    if (!ee_units_lists_has(this->mLists, EE_UNITS_LISTS_SITES, (uint64_t)(uintptr_t)(void*)inUnit))
        ee_units_lists_add(this->mLists, EE_UNITS_LISTS_SITES, (uint64_t)(uintptr_t)(void*)inUnit);
}

void EEPlayer::RemoveOffMapUnit(RTSUnit* inUnit)
{ ee_units_off_map(this->mLists, 1, (int64_t)(uintptr_t)(void*)inUnit, 0, 0, 0); }

void EEPlayer::SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap)
{ ee_units_player_diplomacy(EEPlayerRefs(this), 0, inOther, (unsigned long)inStance, inMap != NULL); }

void EEPlayer::RefreshTerrainVisibility()
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_REFRESH_TERRAIN, 0, 0, 0, 0.0f); }

void EEPlayer::SetSharedLOS(bool inFlag, EEMap* inArg)
{ ee_units_player_diplomacy(EEPlayerRefs(this), 3, NULL, 0, inFlag); }

bool EEPlayer::HasLoadedUnitID(unsigned long inUnitID)
{ return ee_units_lists_has(this->mLists, EE_UNITS_LISTS_LOADED_IDS, inUnitID); }

long EEPlayer::GetCitizenPassability()
{ return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_CITIZEN_PASSABILITY, 0, 0, 0, 0.0f); }

long EEPlayer::GetTotalPopulation(bool inFirst, bool inSecond)
{ return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_TOTAL_POPULATION, NULL, inFirst, inSecond, 0, 0, 0, 0.0f, NULL); }

long EEPlayer::GetImprovementLevel(unsigned long inUnitType, long inImprovement) const
{ return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IMPROVEMENT_LEVEL, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL); }

bool EEPlayer::IsImprovementMaxed(unsigned long inUnitType, long inImprovement)
{ return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IS_IMPROVEMENT_MAXED, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL) != 0; }

void EEPlayer::AddLoadedMedia(EEMedia* inMedia)
{ ee_units_lists_add(this->mLists, EE_UNITS_LISTS_MEDIA, (uint64_t)(uintptr_t)inMedia); }

long EEPlayer::GetWonderTime(long inIndex) const
{ return (long)ee_units_player_rest(const_cast<EEPlayer*>(this), EE_UNITS_PLAYER_REST_WONDER_TIME, 0, inIndex, 0); }

unsigned long EEPlayer::GetNumWonderUnits() const
{ return (unsigned long)ee_units_lists_len(this->mLists, EE_UNITS_LISTS_WONDERS); }

void EEPlayer::SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond)
{ ee_units_player_body(this, EE_UNITS_PLAYER_DO_SWAP_BUTTONS, (uint64_t)(uintptr_t)inFirst, (int64_t)inUnitClass, (int64_t)(uintptr_t)inSecond, 0.0f); }

RTSUnit* EEPlayer::GetUnit(unsigned long inUnitID) { return static_cast<RTSUnit*>(ee_rts_player_unit(this, inUnitID)); }

void EEPlayer::SetColor(const GEColor& inColor)
{
    ee_units_player_set_color(reinterpret_cast<float(*)[3]>(&this->mColor), reinterpret_cast<float(*)[3]>(&this->mShadeColor), reinterpret_cast<const float(*)[3]>(&inColor));
}

void EEPlayer::KillUnit(RTSUnit* inUnit, bool inForce) { ee_rts_player_kill_unit(this, inUnit, inForce); }

unsigned long EEPlayer::GetNumRunningGroups()
{ return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING); }

EEGroup* EEPlayer::GetRunningGroup(unsigned long inIndex)
{ return static_cast<EEGroup*>(ee_units_player_groups_at(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_RUNNING, inIndex)); }

unsigned long EEPlayer::GetNumPendingGroups()
{ return ee_units_player_groups_len(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING); }

void EEPlayer::AddPendingGroup(EEGroup* inGroup)
{ ee_units_player_groups_push(this->mGroupLists, EE_UNITS_PLAYER_GROUPS_PENDING, inGroup); }


bool EESaveIsPrimitive(int32_t inOp)
{
    return (inOp >= EE_UNITS_PLAYER_SAVE_READ_ULONG && inOp <= EE_UNITS_PLAYER_SAVE_WRITE_BOOL) ||
           inOp == EE_UNITS_PLAYER_SAVE_READ_USHORT || inOp == EE_UNITS_PLAYER_SAVE_WRITE_USHORT;
}
