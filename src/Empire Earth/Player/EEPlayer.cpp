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
#include "ee/UReferenceContainer.h"
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
#include "ee/EETeam.h"
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
// EEUnitGoal.cpp's goal counter (0x964480; Save and Load write and read it: it is not file-static in the original)
// the main game form's blinking buttons (0x27fcf0, cdecl; EEPlayer::RepairPointers keeps its result, so not void as
// EEPhysics.cpp declares it; inferred)
// (EESetCivBuilderButtonBlinking: ee/EmpireEarth.h)


// std::for_each's functor in the dtor: dereferences each patrol group of mPatrolGroups (0x16d2f9) (provisional name)
struct EEPDereferencer {
    void operator()(EEPPatrolGroup* inGroup) { ee_units_pg_dereference(inGroup); }
};

// the interval of the healing units' hit-point bonus (0x4429b8, just before kTributeFee; ComputeLoadDistance)
// (provisional name)
static const float kPlayerHealInterval = EE_UNITS_PLAYER_HEAL_INTERVAL;   // (ee_units player_tick.rs)

// (the effects every player loads, 0x4429c0: ee_units player_body.rs PLAYER_EFFECTS)

// TEST ONLY: the player capture (tools/sim_capture.py): PLST, each player's state at every UpdatePlayer (ee_sim app
// player_capture.rs); PLEC, the economy and improvement decisions (ee_units player_econ.rs)
#include "ee/EESimCapture.h"
#include "ee/EETechCapture.h"
#include "ee/EEStatisticsHistory.h"
#include "ee/EEStatisticsRecord.h"
#include <cstring>

static int32_t EEPlayerFloatBits(float inValue)
{
    int32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static float EEPlayerBitsFloat(int32_t inBits)
{
    float theValue;
    memcpy(&theValue, &inBits, 4);
    return theValue;
}

// the decisions' record measures a reason text (ee_units player_econ.rs)
extern "C" uint32_t ee_units_player_wide_length(const void* inText) { return static_cast<const UWideString*>(inText)->GetLength(); }

// Creates a player with a name and ID: resets all state, binds its tech tree to it and creates
// its wonder and construction-site lists
EEPlayer::EEPlayer(const UWideString& inName, unsigned long inPlayerID)
    : RTSPlayer(inName, inPlayerID)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 1, (int64_t)inPlayerID, 0.0f);   // ee_units player_body.rs construct
}

// Creates an unnamed player: resets all state and creates its wonder and construction-site lists
EEPlayer::EEPlayer()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CONSTRUCT, 0, 0, 0, 0.0f);   // ee_units player_body.rs construct
}

// Destroys the player: clears and releases every unit group, its media, tech tree, air traffic, upgrades,
// towns, herds, off-map records, patrol groups and wonder/construction-site lists
EEPlayer::~EEPlayer()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_DESTROY, 0, 0, 0, 0.0f);   // ee_units player_body.rs destroy
}

// Destroys the civilization (members only)
EECivilization::~EECivilization()
{
}

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
struct EEPlayerRefs : EeUnitsPlayerRefs {
    EEPlayerRefs(const EEPlayer* inPlayer)
    {
        EEPlayer* thePlayer = const_cast<EEPlayer*>(inPlayer);
        this->player = thePlayer;
        this->host = &EEPlayerHost;
        this->saved = thePlayer;
        this->fields = thePlayer;
        this->stats = thePlayer->mSavedGroups.mStats;
    }
    operator const EeUnitsPlayerRefs*() const { return this; }
};

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
    case EE_UNITS_PLAYER_NODE: return (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mTechnologies->Get((long)a[0]));
    case EE_UNITS_PLAYER_NODE_AVAILABLE: return theNode->GetIsAvailable();
    case EE_UNITS_PLAYER_NODE_UNIT_CLASS: return theNode->GetUnitClass((unsigned long)a[1]);
    case EE_UNITS_PLAYER_NODE_COST: return theNode->GetCost((long)a[1], (long)a[2], 1.0f);
    case EE_UNITS_PLAYER_SCALE_COST: return thePlayer->ScaleCost((long)a[0]);
    case EE_UNITS_PLAYER_METALLURGY: return gCivPowersManager.HasPower(thePlayer, kCivPowerMetallurgy);
    case EE_UNITS_PLAYER_TAKE: thePlayer->TakeResource((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_ADD: thePlayer->AddResource((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_EXTRA_POPULATION: return thePlayer->GetExtraPopulation((long)a[0]);
    case EE_UNITS_PLAYER_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_UNITS_PLAYER_HERO_COUNT: return thePlayer->GetHeroCount(a[0] != 0, a[1] != 0);
    case EE_UNITS_PLAYER_SCENARIO: return gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_PLAYER_CAN_AFFORD: return thePlayer->CanAfford(theNode, (long)a[1], (long)a[2]);
    case EE_UNITS_PLAYER_STATS_LOWER: thePlayer->mSavedGroups.Lower((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_UPDATE_JIT: gUserInterface->UpdateJITManufacturing();
        return 0;
    // the unit bookkeeping and the wonders (ee_units player_units.rs): the units as RTSUnit addresses
    case EE_UNITS_PLAYER_NEXT_COMPLEX: return ee_units_player_body(thePlayer, EE_UNITS_PLAYER_DO_NEXT_COMPLEX, (uint64_t)EEPlayerUnitArg(theUnit), 0, 0, 0.0f);
    case EE_UNITS_PLAYER_STATE: return (gLoadingSavedGame ? 1 : 0) | (gGameScreenActive ? 2 : 0) | (gWorldClearing ? 4 : 0) | (UShutdownInProgress() ? 8 : 0) | (gWorld.mVictoryEnabled ? 16 : 0) | (gUserInterface != NULL ? 32 : 0);
    case EE_UNITS_PLAYER_RTS_ADD_UNIT: thePlayer->RTSPlayer::AddUnit(theUnit, a[1] != 0);
        return 0;
    case EE_UNITS_PLAYER_ACTIVATE: gWorld.ActivateUnit(theComplex);
        return 0;
    case EE_UNITS_PLAYER_NODE_IS_AVAILABLE: return theNode->mIsAvailable;
    case EE_UNITS_PLAYER_SET_NODE_AVAILABLE: thePlayer->mTechnologies->SetNodeAvailable((long)a[0], a[1] != 0);
        return 0;
    case EE_UNITS_PLAYER_RESEARCH: thePlayer->mTechnologies->Research((long)a[0], NULL);
        return 0;
    case EE_UNITS_PLAYER_UNIT_PLACED: thePlayer->UnitPlaced(theComplex);
        return 0;
    case EE_UNITS_PLAYER_CITIZEN_CONTAINS: return thePlayer->GetTechTree()->Get(kTechCitizen)->Contains((long)a[0]);
    case EE_UNITS_PLAYER_TECH_OBSOLETE: return thePlayer->mTechnologies->Get((long)a[0]) != NULL && thePlayer->mTechnologies->Get((long)a[0])->GetIsObsolete();
    case EE_UNITS_PLAYER_SWAP_KIND: return gPhysics->GetUnitSwapKind(theComplex);
    case EE_UNITS_PLAYER_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theComplex);
    case EE_UNITS_PLAYER_STATS_RESET: thePlayer->mSavedGroups.Reset((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_FOUNDATION: return theComplex->IsFoundation();
    case EE_UNITS_PLAYER_ADD_TO_TOWN: return ee_units_player_areas(EEPlayerRefs(thePlayer), 0, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_ADD_TO_HERD: return ee_units_player_areas(EEPlayerRefs(thePlayer), 1, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_CIV_POWER: return gCivPowersManager.HasPower(thePlayer, (long)a[0]);
    case EE_UNITS_PLAYER_LOG_LOST: EEGameLogUnitLost(thePlayer, theComplex);   // port: the game log (ee/EEGameLog.h)
        return 0;
    case EE_UNITS_PLAYER_UNIT_NODE: return (int64_t)reinterpret_cast<uintptr_t>(theUnit->GetTechTreeNode());
    case EE_UNITS_PLAYER_REMOVE_FROM_TOWN: ee_units_player_areas(EEPlayerRefs(thePlayer), 2, static_cast<RTSUnit*>(theComplex));
        return 0;
    case EE_UNITS_PLAYER_REMOVE_FROM_HERD: ee_units_player_areas(EEPlayerRefs(thePlayer), 3, static_cast<RTSUnit*>(theComplex));
        return 0;
    case EE_UNITS_PLAYER_REMOVE_SITE: thePlayer->mConstructionSites->Remove(theComplex);
        return 0;
    case EE_UNITS_PLAYER_RECOMPUTE_AREAS: thePlayer->SuggestRecomputeAllAreaEffects();
        return 0;
    case EE_UNITS_PLAYER_OWNER_ADD_WONDER: theComplex->mPlayer->AddWonder(theComplex, false);
        return 0;
    case EE_UNITS_PLAYER_CLEAR_FOOTPRINT: ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_CLEAR_FOOTPRINT, (uint64_t)EEPlayerUnitArg(theComplex), 0, 0);
        return 0;
    case EE_UNITS_PLAYER_SET_CONTAINS: return gUnitSetManager.GetSet((long)a[0]) != NULL && gUnitSetManager.GetSet((long)a[0])->Contains((long)a[1]);
    case EE_UNITS_PLAYER_UPGRADE_HIT_POINTS: EEUpgradeHitPoints(thePlayer->mUnitClassUpgrades[(size_t)a[0]], thePlayer->mUnitClassUpgrades[(size_t)a[0]]->mUnitClass->GetUnitType(), io->f[0], thePlayer);
        return 0;
    case EE_UNITS_PLAYER_CHANGE_LEVEL: thePlayer->mUnitClassUpgrades[(size_t)a[0]]->ChangeLevel((long)a[1], (long)a[2]);
        return 0;
    case EE_UNITS_PLAYER_GENERATE: io->f[0] = gPhysics->mRandom.Generate();
        return 0;
    case EE_UNITS_PLAYER_PRODUCE: return theUnit->GetUnitClass()->ProduceUnit(theUnit->GetComplexUnit(), gUnitClassTable[(size_t)a[1]], false);
    case EE_UNITS_PLAYER_SLAVERY_SOUND: gSoundManager->Play(EESoundObject::Get(kSoundSlavery)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_PLAYER_HEALED_SET: thePlayer->mHealedUnitSet = a[1] != 0 ? gUnitSetManager.GetSet((long)a[0]) : NULL;
        return 0;
    // the diplomacy and the shared sight (ee_units player_diplomacy.rs): the players as EEPlayer addresses, 0 this one
    case EE_UNITS_PLAYER_STORE_TABLE: EEUnitStorePlayerTable(thePlayer);   // New (R5b.4): the store's diplomacy table
        return 0;
    case EE_UNITS_PLAYER_RESET_WONDER_TIMERS: ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_RESET_WONDER_TIMERS, (uint64_t)(uintptr_t)theOther, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_UNVIEW: thePlayer->UpdateSelection(NULL, true);
        return 0;
    case EE_UNITS_PLAYER_REFRESH_TERRAIN: gWorld.GetCurrentPlayer()->RefreshTerrainVisibility();
        return 0;
    // the compound operations' statements (ee_units player_split.rs): a[0] a unit or a player (0 this one)
    case EE_UNITS_PLAYER_X_CAMOUFLAGE_ON: ee_units_player_body(thePlayer, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 1, 0, 0.0f);
        return 0;
    case EE_UNITS_PLAYER_X_CAMOUFLAGE_OFF: ee_units_player_body(thePlayer, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 0, 0, 0.0f);
        return 0;
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
    case EE_UNITS_PLAYER_X_ALLY_ADD: theOther->mAllies.push_back(a[1] != 0 ? reinterpret_cast<EEPlayer*>(a[1]) : thePlayer);
        return 0;
    case EE_UNITS_PLAYER_X_ALLY_REMOVE: theOther->RemoveAlly(a[1] != 0 ? reinterpret_cast<EEPlayer*>(a[1]) : thePlayer);
        return 0;
    case EE_UNITS_PLAYER_X_SHARERS_PUSH: thePlayer->mSightSharers.push_back(theOther);
        return 0;
    case EE_UNITS_PLAYER_X_SHARERS_HAS: return std::find(thePlayer->mSightSharers.begin(), thePlayer->mSightSharers.end(), theOther) != thePlayer->mSightSharers.end();
    case EE_UNITS_PLAYER_X_SHARERS_ERASE: thePlayer->mSightSharers.erase(std::find(thePlayer->mSightSharers.begin(), thePlayer->mSightSharers.end(), theOther));
        return 0;
    case EE_UNITS_PLAYER_X_MINIMAP: return gMiniMap != NULL;
    case EE_UNITS_PLAYER_X_MINIMAP_REDRAW: gMiniMap->Redraw();
        return 0;
    case EE_UNITS_PLAYER_X_WONDERS_ADD: thePlayer->mWonders->Add(theComplex);
        return 0;
    case EE_UNITS_PLAYER_X_WONDERS_REMOVE: thePlayer->mWonders->Remove(theComplex);
        return 0;
    case EE_UNITS_PLAYER_X_CLOAKED_NOW: theComplex->mCloaked = gPhysics->mGameTime;
        return 0;
    case EE_UNITS_PLAYER_X_CLOAKED_CLEAR: theComplex->mCloaked = 0;
        return 0;
    case EE_UNITS_PLAYER_X_STATS_LOWER: thePlayer->mSavedGroups.Lower((long)a[0], (long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_X_FIRST_UNIT: return EEPlayerUnitArg(theOther->mFirstUnit);
    case EE_UNITS_PLAYER_X_NEXT_UNIT: return EEPlayerUnitArg(theUnit->mNext);
    case EE_UNITS_PLAYER_X_LAND_BUILDING: return theUnit->mUnitClass->IsLandBuilding();
    case EE_UNITS_PLAYER_X_SIGHT_ON: theUnit->SightOnHere();
        return 0;
    case EE_UNITS_PLAYER_X_SIGHT_OFF: theUnit->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_X_IS_SELF: return gWorld.mPlayers[a[0]] == thePlayer;
    case EE_UNITS_PLAYER_X_WONDER_STARTED: gWorld.mPlayers[a[0]]->WonderStarted(EEPlayerUnitAt(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_RESOURCE: return theUnit->mResourceAmount;
    case EE_UNITS_PLAYER_X_RESOURCE_CLEAR: theUnit->mResourceAmount = 0;
        return 0;
    case EE_UNITS_PLAYER_X_FOREST_REMOVE: gContinentManager.RemoveForestTile(theUnit);
        return 0;
    case EE_UNITS_PLAYER_X_NODE: return (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mTechnologies->Get((long)a[0]));
    case EE_UNITS_PLAYER_X_NODE_ALLOW: theNode->mIsAllowed = a[1] != 0;
        return 0;
    case EE_UNITS_PLAYER_X_UPGRADE_SLOTS: return (int64_t)thePlayer->mUnitClassUpgrades.size();
    case EE_UNITS_PLAYER_X_UPGRADE_AT: return (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mUnitClassUpgrades[(size_t)a[0]]);
    case EE_UNITS_PLAYER_X_UPGRADE_UNIT_TYPE: return (int64_t)reinterpret_cast<EEUnitClassUpgrade*>(a[0])->mUnitClass->GetUnitType();
    case EE_UNITS_PLAYER_X_STRING: gResource->GetString((long)a[0], *reinterpret_cast<UWideString*>(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2: return (long)gWorldTable.GetLongValue2(EEWorldImprovementTime(a[0]));
    case EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1: return (long)gWorldTable.GetLongValue(EEWorldImprovementTime(a[0]));
    // the towns and herds (ee_units player_areas.rs): list a[0] (0 the towns, 1 the herds), the areas and units by address
    case EE_UNITS_PLAYER_TOWN_UNIT: return EETown::IsTownUnit(theUnit);
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
    case EE_UNITS_PLAYER_X_CLASS_COUNT: return (int64_t)gUnitClassTable.size();
    case EE_UNITS_PLAYER_X_CLASS_AT: return (int64_t)(uintptr_t)gUnitClassTable[(size_t)a[0]];
    case EE_UNITS_PLAYER_X_CLASS_TECH: return EEPlayerPtr<EEUnitClass>(a[0])->mTechnology;
    case EE_UNITS_PLAYER_X_CLASS_WEIGHT: return EEPlayerPtr<EEUnitClass>(a[0])->mWeight;
    case EE_UNITS_PLAYER_X_CLASS_POPULATION: return EEPlayerPtr<EEUnitClass>(a[0])->GetPopulationCost();
    case EE_UNITS_PLAYER_X_CLASS_TYPE: return (int64_t)EEPlayerPtr<EEUnitClass>(a[0])->mUnitType;
    case EE_UNITS_PLAYER_X_CLASS_AI_KIND: return (int64_t)EEPlayerPtr<EEUnitClass>(a[0])->mAIKind;
    case EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS: return (int64_t)(uintptr_t)EEPlayerPtr<EEUnitClass>(a[0])->mImprovements;
    case EE_UNITS_PLAYER_X_TABLE_STEP: io->f[0] = EEPlayerPtr<EeDataUpgradeFields>(a[0])->mFactors[a[1]].mOffset;
        return 0;
    case EE_UNITS_PLAYER_X_TABLE_COST_FACTOR: io->f[0] = EEPlayerPtr<EeDataUpgradeFields>(a[0])->mFactors[a[1]].mScale;
        return 0;
    case EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL: return EEPlayerPtr<EeDataUpgradeFields>(a[0])->mMaxLevel;
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
    case EE_UNITS_PLAYER_X_CLASS_FLIGHT_TIME: return EEPlayerPtr<EEUnitClass>(a[0])->mFlightTime;
    case EE_UNITS_PLAYER_X_UP_FLIGHT_TIME: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mFlightTime;
    case EE_UNITS_PLAYER_X_UP_POWER: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mPower;
    case EE_UNITS_PLAYER_X_UP_MAX_HIT_POINTS: return EEPlayerPtr<EEUnitClassUpgrade>(a[0])->mMaxHitPoints;
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
    case EE_UNITS_PLAYER_X_TOWNS_PUSH: thePlayer->mTowns.push_back(EEPlayerTownAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_SERIAL: EEPlayerHerdAt(a[0])->SetSerial(gContinentManager.NewSerial());
        return 0;
    case EE_UNITS_PLAYER_X_HERD_ADD_AREA: gContinentManager.AddArea(EEPlayerHerdAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_REMOVE_AREA: gContinentManager.RemoveArea(EEPlayerHerdAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_HERD_DELETE: delete EEPlayerHerdAt(a[0]);
        return 0;
    case EE_UNITS_PLAYER_X_HERDS_PUSH: thePlayer->mHerds.push_back(EEPlayerHerdAt(a[0]));
        return 0;
    case EE_UNITS_PLAYER_X_TOWNS_ERASE: thePlayer->mTowns.erase(thePlayer->mTowns.begin() + (size_t)a[0]);
        return 0;
    case EE_UNITS_PLAYER_X_HERDS_ERASE: thePlayer->mHerds.erase(thePlayer->mHerds.begin() + (size_t)a[0]);
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
    case EE_UNITS_PLAYER_X_COMPLEX: return EEPlayerUnitArg(theUnit->GetComplexUnit());
    case EE_UNITS_PLAYER_X_PATROL_GOAL: return (int64_t)(uintptr_t)EEPatrolGoalCast(theComplex->mGoalState);
    case EE_UNITS_PLAYER_X_GOAL_PATROL_GROUP: return (int64_t)(uintptr_t)EEPlayerPtr<EEUGRust>(a[0])->GetPatrolGroup();
    case EE_UNITS_PLAYER_X_GOAL_SET_PATROL: EEPlayerPtr<EEUGRust>(a[0])->SetPatrolGroup(EEPlayerPtr<EEPPatrolGroup>(a[1]));
        return 0;
    case EE_UNITS_PLAYER_X_FIND_AREA_TILE: return EEPlayerPackPoint(theComplex->FindAreaTile(true, 1));
    case EE_UNITS_PLAYER_X_TILE_X: return theUnit->mTileX;
    case EE_UNITS_PLAYER_X_TILE_Y: return theUnit->mTileY;
    case EE_UNITS_PLAYER_X_LAST_FLOOR_X: return UMFloor(theComplex->mLastMapPosition.GetX());
    case EE_UNITS_PLAYER_X_LAST_FLOOR_Y: return UMFloor(theComplex->mLastMapPosition.GetY());
    }
    return 0;
}


// the unit bookkeeping and the wonders (ee_units player_units.rs ee_units_player_units)
static long EEPlayerUnits(const EEPlayer* inPlayer, uint32_t inWhat, RTSUnit* inUnit = NULL, bool inFlag = false, bool inFlag2 = false)
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

// Queues an improvement (front or back) when it can be bought, paying unless told not to (ee_units player.rs)
void EEPlayer::QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_QUEUE_IMPROVEMENT, NULL, (int32_t)inUnitType, (int32_t)inImprovement, inFront, inAlreadyPaid, 0, 0.0f, NULL);
}

// Records a media item as loaded by the player (ignores NULL)
void EEPlayer::AddLoadedMedia(EEMedia* inMedia)
{
    ee_units_lists_add(this->mLists, EE_UNITS_LISTS_MEDIA, (uint64_t)(uintptr_t)inMedia);
}

// Starts every pending command group running, then adds the pending units as the base player does
void EEPlayer::AddPendingUnits(bool inKeepID, bool inReleasePending)
{
    ee_units_player_groups_do(this->mGroupLists, 0);   // the pending groups appended to the running ones
    RTSPlayer::AddPendingUnits(inKeepID, inReleasePending);
}

// Adds a patrol group to the player, keeping a reference to it
void EEPlayer::AddPatrolGroup(EEPPatrolGroup* inGroup)
{
    ee_units_pg_reference(inGroup);
    this->mPatrolGroups.push_back(inGroup);
}

// Starts sharing the player's sight with another player, refreshing its land buildings' sight around the change
void EEPlayer::AddSightSharer(EEPlayer* inPlayer)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 1, inPlayer, 0, false);
}

// Toggles units in the selection: adds built units not yet selected and removes the rest;
// plays the last added unit's selection sound during local selection
void EEPlayer::AddAndRemoveUnitsFromSelectedGroup(std::vector<long>& inUnitIDs)
{
    ee_units_selection(this, 5, NULL, 0, inUnitIDs.data(), inUnitIDs.size(), false, false);
}

// Refunds a fraction of a technology's cost in each resource (ee_units player.rs)
void EEPlayer::RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REFUND_TECHNOLOGY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL);
}

// Selects a unit by ID, starting a new selection when adding a unit of a player other than the selected one
void EEPlayer::AddUnitToSelectedGroup(unsigned long inUnitID, bool inFlag)
{
    ee_units_selection(this, 3, NULL, (int64_t)inUnitID, NULL, 0, inFlag, false);
}

// Adds the built units of a list of IDs to the selection; plays the last added unit's selection
// sound during local selection
void EEPlayer::AddUnitsToSelectedGroup(std::vector<long>& inUnitIDs)
{
    ee_units_selection(this, 4, NULL, 0, inUnitIDs.data(), inUnitIDs.size(), false, false);
}

// Pushes back the start time of every wonder's timer by an amount
void EEPlayer::DelayWonderTimers(long inAmount)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_DELAY_WONDER_TIMERS, 0, inAmount, 0);   // player_rest.rs
}

// Adds one of the player's own units to the repatrol list once, keeping a reference to it
void EEPlayer::AddUnitToRepatrol(EEComplexUnit* inUnit)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_ADD_TO_REPATROL, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0);   // player_rest.rs
}

// (gNumPlayers 0x530e0c, which EECPAttackCommander.cpp and EECPAttackSubGroup.cpp also declare, is
// gWorld.mNumPlayers, unsigned: `jbe`)

// Adds a unit to the player: counts its population, activates it in the world, marks its tech node (researched when
// built), counts vital units, unlocks the capitol technologies on the first capitol and records statistics
void EEPlayer::AddUnit(RTSUnit* inUnit, bool inKeepID)
{
    EEPlayerUnits(this, 0, inUnit, inKeepID);
}

// Records a unit leaving the map with its tile and the current game time
void EEPlayer::AddOffMapUnit(RTSUnit* inUnit)
{
    if (inUnit != NULL)
        ee_units_off_map(this->mLists, 0, EEPlayerUnitArg(inUnit), inUnit->mTileX, inUnit->mTileY, gPhysics != NULL ? gPhysics->mGameTime : 0);
}

// Adds a unit to the stranded list once, keeping a reference to it
void EEPlayer::AddStrandedUnit(EEUnit* inUnit)
{
    ee_units_lists_add(this->mLists, EE_UNITS_LISTS_STRANDED, EEPlayerUnitArg(inUnit));
}

// Selects a unit into the active group or as the single viewed unit; returns false for a foreign unit
// while own units are selected
bool EEPlayer::TrySelectUnit(EEUnit* inUnit, bool inFlag)
{
    return ee_units_selection(this, 1, inUnit, 0, NULL, 0, inFlag, false);
}

// Town merge is defined after its existing patrol-goal include below.

// (0x563659: EEUGPatrol::SetPatrolGroup, defined after ee/EEUGPatrol.h's include below)

// Detaches a unit from the computer player's control
void EEPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
}

// Adds a unit to the player's construction sites unless it is already there
void EEPlayer::AddConstructionSite(EEComplexUnit* inUnit)
{
    if (!this->mConstructionSites->Contains(inUnit))
        this->mConstructionSites->Add(inUnit);
}

// Records a wonder (starting its timer unless told not to); the first wonder of a type grants its wonder power
void EEPlayer::AddWonder(EEComplexUnit* inUnit, bool inIsLoading)
{
    EEPlayerUnits(this, 7, inUnit, inIsLoading);
}

// Recounts the population, the vital units that keep the player alive and the vital buildings
void EEPlayer::RecountPopulation()
{
    EEPlayerUnits(this, 4);
}

// Whether an improvement can be bought counting the queue (ee_units player.rs can_buy); else the reason text
bool EEPlayer::CanBuy(long inUnitType, long inImprovement, UWideString* outText)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_BUY, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outText) != 0;
}

// Whether a technology or unit can be produced (ee_units player.rs can_produce); sets the reason text when not
bool EEPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_PRODUCE, NULL, inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, 0.0f, outReason) != 0;
}

// Whether an improvement is allowed and within the limits, ignoring the queue (ee_units player.rs)
bool EEPlayer::CanImprove(long inUnitType, long inImprovement, UWideString* outReason)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_IMPROVE, NULL, inUnitType, inImprovement, 0, 0, 0, 0.0f, outReason) != 0;
}


// Does nothing (the base player ignores being targeted by another player)
// (ICF-kept empty `ret 8`, shared by the slots below; FlareReceived below (slot +0x98) is the same empty body,
// folded into it: the base player ignores flares)
void EEPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
}

void EEPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
}

// (slots sharing the empty body: EECPAttackFormation+0x14; EEHumanPlayer+0x58, +0x98; EECalamityClass+0x28;
// EECCConscription, EECCLineOfSight, EECCSpecialAttack, EECCSpecialLocation +0x28; EEE3DFarm+0x20;
// EEPlayer+0x58, +0x98; EESETSCTAlwaysTrue+0x4, +0xc; EESETSETNoEffect, EESETSETRemoveObject, EESETSETKillObject,
// EESETSETFreezeObject, EESETSETObjectCreate +0x8, +0x10; EESETSETPVisibility, EESETSETPCameraFadeOut,
// EESETSETPCameraFadeIn, EESETSETPlayerVictory +0x8; EEUnit+0x3c, +0x44, +0x68)

// the class forwarders' upgrade of a unit class: NULL past the classes or without one
static EEUnitStats* EEPlayerClassStats(EEPlayer* inPlayer, unsigned long inUnitClass)
{
    return inUnitClass < EEUnitClassCount() ? inPlayer->mUnitClassUpgrades[inUnitClass] : NULL;
}

// For a human player whose unit class has its button loaded, releases one button's media
// and acquires the other's
void EEPlayer::SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_SWAP_BUTTONS, (uint64_t)(uintptr_t)inFirst, (int64_t)inUnitClass, (int64_t)(uintptr_t)inSecond, 0.0f);
}

// Sets one of a unit class's acknowledgement sounds for the player
void EEPlayer::SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetAcknowledgeSound(inSound, inIndex);
}

// Sets a unit class's creation sound for the player
void EEPlayer::SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetCreationSound(inSound);
}

// Sets a unit class's death sound for the player
void EEPlayer::SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetDeathSound(inSound);
}

// Sets a unit class's model for the player
void EEPlayer::SetClassModel(unsigned long inUnitClass, EEModel* inModel)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetModel(inModel);
}

// Sets a unit class's button for the player
void EEPlayer::SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetButton(inButton);
}

// Sets one of a unit class's selection sounds for the player
void EEPlayer::SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetSelectionSound(inSound, inIndex);
}

// Drops every queued improvement of a unit type
void EEPlayer::EraseQueuedImprovements(unsigned long inUnitType)
{
    ee_units_queued_op(this->mQueued, 0, inUnitType, 0);
}

// Drops the pending command groups, then releases the pending units as the base player does
void EEPlayer::ReleasePendingUnits()
{
    ee_units_player_groups_do(this->mGroupLists, 1);   // the pending groups cleared
    RTSPlayer::ReleasePendingUnits();
}

// Empties a numbered control group and tells the user interface
void EEPlayer::ClearSavedGroup(long inGroup)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CLEAR_SAVED_GROUP, (uint64_t)inGroup, 0, 0, 0.0f);   // player_body.rs
}

// Handles a finished unit: registers wonders (telling the others when victory is on), counts vital
// buildings, applies the civ-power sight pulse, and updates area effects, construction sites, towns and walls
void EEPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 3, inUnit);
}

// Moves the improvement queue of one unit type to another
void EEPlayer::MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo)
{
    ee_units_queued_op(this->mQueued, 1, inFrom, inTo);
}

// Clears every goal queue of a complex unit leaving the player
void EEPlayer::UnitRemoved(RTSUnit* inUnit)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_REMOVED, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0);   // player_rest.rs
}

// Writes the player's per-class media counts and media load warnings to a text file
void EEPlayer::DumpMedia(FSFileSpec& inFile)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_DUMP_MEDIA, (uint64_t)(uintptr_t)&inFile, 0, 0);   // player_rest.rs
}

// Marks the player as having met another player when it first sees a unit of a non-gaia player
void EEPlayer::NoticeForeignUnit(EEUnit* inUnit)
{
    if (this->mCombatState == 0 && inUnit->mPlayer->mPlayerID != 0)
        this->mCombatState = 1;
}

// Puts every herd animal of the player that is in no herd into one
void EEPlayer::AssignHerds()
{
    ee_units_player_areas(EEPlayerRefs(this), 5, NULL);
}

// Adds every land building outside the player's towns to a town, then gives each town without
// a patrol group one
void EEPlayer::AssignTowns()
{
    ee_units_player_areas(EEPlayerRefs(this), 4, NULL);
}

// Executes a selection message against the player's network selection, keeping its local
// active selection and viewed unit
void EEPlayer::ExecuteSelectionMessage(NEMessage* inMessage)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_EXECUTE_SELECTION, (uint64_t)(uintptr_t)inMessage, 0, 0, 0.0f);   // player_body.rs
}

// Returns the town nearest to a tile, optionally only on the tile's continent; NULL when none
EEAIArea* EEPlayer::GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent)
{
    return static_cast<EEAIArea*>(reinterpret_cast<EETown*>(ee_units_player_rest(this, EE_UNITS_PLAYER_REST_NEAREST_AREA, inSameContinent, inTile.GetX(), inTile.GetY())));   // player_rest.rs
}

// Returns the construction site nearest to a unit within a squared distance of 100, or NULL
EEUnit* EEPlayer::GetNearestConstructionSite(EEComplexUnit* inUnit)
{
    return static_cast<EEUnit*>(EEPlayerUnitAt(ee_units_player_body(this, EE_UNITS_PLAYER_DO_NEAREST_SITE, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0, 0.0f)));
}

// Returns whether the player can afford to repair a number of a unit's hit points (cost relative to its maximum)
bool EEPlayer::CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD_REPAIR, inUnit, (int64_t)reinterpret_cast<uintptr_t>(inOther), inValue, 0, 0, 0, 0.0f, NULL) != 0;
}

// Adds the player's resources, population and tech tree to the sync checksum
void EEPlayer::GetChecksum(ESChecksum& ioChecksum)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_CHECKSUM, (uint64_t)(uintptr_t)&ioChecksum, 0, 0);   // player_rest.rs
}

// Returns whether the player still plays: not an observer, or an observer whose game has ended
bool EEPlayer::IsActive()
{
    return !this->mObserverMode || this->mGameResult != 0;
}

// Appends the player's herds of a unit type's animal to a list
void EEPlayer::GetHerdsOfType(std::vector<EEHerd*>& outHerds, long inUnitType)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_HERDS_OF_TYPE, (uint64_t)(uintptr_t)&outHerds, inUnitType, 0);   // player_rest.rs
}

// A unit type's level in an improvement counting the queued ones; -1 without an upgrade (ee_units player.rs)
long EEPlayer::GetImprovementLevel(unsigned long inUnitType, long inImprovement) const
{
    return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IMPROVEMENT_LEVEL, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL);
}

// A unit type's queued improvements of a kind past the first (ee_units player.rs)
long EEPlayer::CountQueuedImprovements(unsigned long inUnitType, long inImprovement) const
{
    return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_COUNT_QUEUED, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL);
}

// Whether an improvement has reached its limits counting the queue (ee_units player.rs)
bool EEPlayer::IsImprovementMaxed(unsigned long inUnitType, long inImprovement)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_IS_IMPROVEMENT_MAXED, NULL, (int32_t)inUnitType, inImprovement, 0, 0, 0, 0.0f, NULL) != 0;
}

// Returns the terrain kinds the player's citizens can cross, as passability flags 1/2/4/8
long EEPlayer::GetCitizenPassability()
{
    return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_CITIZEN_PASSABILITY, 0, 0, 0, 0.0f);   // player_body.rs
}

// Registers every herd of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterHerds()
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REGISTER, 0, 1, 0);   // player_rest.rs
}

// Registers every town of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterTowns()
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REGISTER, 0, 0, 0);   // player_rest.rs
}

// Returns the player's patrol group with an ID, or NULL
EEPPatrolGroup* EEPlayer::GetPatrolGroup(unsigned long inID)
{
    return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(ee_units_player_body(this, EE_UNITS_PLAYER_DO_PATROL_GROUP, inID, 0, 0, 0.0f)));
}

// Returns the population, optionally adding the pending units and the units inside category-0x1d containers
long EEPlayer::GetTotalPopulation(bool inFirst, bool inSecond)
{
    return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_TOTAL_POPULATION, NULL, inFirst, inSecond, 0, 0, 0, 0.0f, NULL);
}

// Returns the player's population limit: the game's limit plus the player's bonuses
long EEPlayer::GetPopulationLimit(long inArg)
{
    return this->mPopulationLimitBonus + this->mShareBonus + gWorld.mPlayerPopulationLimit;
}

// Returns the index of the team holding the player (0 when none)
unsigned long EEPlayer::GetTeamIndex()
{
    return (unsigned long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_TEAM_INDEX, 0, 0, 0, 0.0f);   // player_body.rs
}

// Returns the off-map record of a unit, or NULL
EEWUnitsOffMapInfo* EEPlayer::GetOffMapInfo(EEComplexUnit* inUnit)
{
    return ee_units_off_map_info(this->mLists, EEPlayerUnitArg(inUnit));
}

// Returns the timer start time of a wonder by index
long EEPlayer::GetWonderTime(long inIndex) const
{
    return (long)ee_units_player_rest(const_cast<EEPlayer*>(this), EE_UNITS_PLAYER_REST_WONDER_TIME, 0, inIndex, 0);   // player_rest.rs
}

// Returns the number of wonders the player holds
unsigned long EEPlayer::GetNumWonderUnits() const
{
    return this->mWonders->GetSize();
}

// Whether the player can afford a technology in all resources or else from a single one (ee_units player.rs)
bool EEPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_CAN_AFFORD, inTechnology, inUnitType, inCount, 0, 0, 0, 0.0f, NULL) != 0;
}

// Adds damage taken from a player to its tally and enters combat when that player is an enemy
void EEPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_DAMAGE, 0, inPlayer, inAmount, 0.0f);   // player_body.rs
}


// Under civ power 0x27, periodically makes a free unit (class 0x20 or 0x1e0) at a town center or capitol,
// playing a sound for the local player and lengthening the interval each time
void EEPlayer::UpdateFreeUnitPower()
{
    EEPlayerUnits(this, 12);
}

// Advances the player to the next epoch
void EEPlayer::IncrementEpic()
{
    this->mTechnologies->AdvanceEpoch();
}

// Resets the player to its starting state: new selection and control groups, upgrades, tech tree and
// air traffic, zeroed counters and default settings, then loads the player effects
void EEPlayer::Reset()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_RESET, 0, 0, 0, 0.0f);   // ee_units player_body.rs reset
}

// Kills every stranded unit, removes it from the player and empties the stranded list
void EEPlayer::KillStrandedUnits()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_KILL_STRANDED, 0, 0, 0, 0.0f);   // player_body.rs
}

// The context of EEPlayer::Load and Save's operations (EEPlayerSaveOperation)
struct EEPlayerSaveContext {
    EEPlayer* mPlayer;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
    EEUnitClassUpgrade* mDummyUpgrade;      // the load's stand-in upgrade (UPGRADE_DUMMY_NEW .. DELETE)
};

static int32_t EEPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);

// Loads the player's state from a saved game (the mirror of Save); returns the read status, some reads' errors ignored.
// port (R5b.4): the body is Rust (ee_units player_save.rs `load`: the original's four bodies, one per saved-game era,
// as one sequence with the eras' returns and assertion lines) over the record this class derives from
// (EeUnitsPlayerSaved) and EEPlayer::SaveOperation's parts
long EEPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEPlayerSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL};
    return ee_units_player_load(&theContext, &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
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

// the FSFile overloads the Rust primitives call (ee_units save_prim.rs)
static FSFile* EEPlayerFile(void* inFile) { return static_cast<FSFile*>(inFile); }
extern "C" long ee_units_file_read_u32(void* inFile, uint32_t* ioValue) { return EEPlayerFile(inFile)->Read(*ioValue); }
extern "C" long ee_units_file_read_i32(void* inFile, int32_t* ioValue) { return EEPlayerFile(inFile)->Read(*ioValue); }
extern "C" long ee_units_file_read_f32(void* inFile, float* ioValue) { return EEPlayerFile(inFile)->Read(*ioValue); }
extern "C" long ee_units_file_read_bool(void* inFile, uint8_t* ioValue) { return EEPlayerFile(inFile)->Read(*reinterpret_cast<bool*>(ioValue)); }
extern "C" long ee_units_file_read_u16(void* inFile, unsigned short* ioValue) { return EEPlayerFile(inFile)->Read(*ioValue); }
extern "C" long ee_units_file_read_bytes(void* inFile, uint8_t* outData, unsigned long inLength, unsigned long* outRead)
{
    return EEPlayerFile(inFile)->Read(reinterpret_cast<char*>(outData), inLength, *outRead);
}
extern "C" long ee_units_file_write_u32(void* inFile, uint32_t inValue) { return EEPlayerFile(inFile)->Write(inValue); }
extern "C" long ee_units_file_write_i32(void* inFile, int32_t inValue) { return EEPlayerFile(inFile)->Write(inValue); }
extern "C" long ee_units_file_write_f32(void* inFile, float inValue) { return EEPlayerFile(inFile)->Write(inValue); }
extern "C" long ee_units_file_write_bool(void* inFile, uint8_t inValue) { return EEPlayerFile(inFile)->Write(inValue != 0); }
extern "C" long ee_units_file_write_u16(void* inFile, unsigned short inValue) { return EEPlayerFile(inFile)->Write(inValue); }
extern "C" long ee_units_file_write_bytes(void* inFile, const uint8_t* inData, unsigned long inLength, unsigned long* outWritten)
{
    return EEPlayerFile(inFile)->Write(reinterpret_cast<const char*>(inData), inLength, *outWritten);
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
    case EE_UNITS_PLAYER_SAVE_RTS_LOAD: thePlayer->RTSPlayer::Load(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_DELETE: delete thePlayer->mTechnologies;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_NULL: thePlayer->mTechnologies = NULL;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_CLEAR: thePlayer->mPatrolGroups.clear();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD: thePlayer->mSavedGroups.Load(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE: EEUnitStorePlayerTable(thePlayer);   // New (R5b.4): the store's diplomacy table
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
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH: ee_units_lists_add(thePlayer->mLists, EE_UNITS_LISTS_SITE_IDS, (uint64_t)(long)io->i);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERDS_RESIZE: thePlayer->mHerds.resize((unsigned long)io->i, NULL);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_HERD_NEW: thePlayer->mHerds[inIndex] = new EEHerd;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_HERD_LOAD: return thePlayer->mHerds[inIndex]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_LOAD:
        return thePlayer->mEvents.Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE: return ee_units_off_map_file(thePlayer->mLists, 2, 0, &theFile, &EESaveFilePrimitive, io);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_LOAD: return ee_units_off_map_file(thePlayer->mLists, 3, inIndex, &theFile, &EESaveFilePrimitive, io);
    case EE_UNITS_PLAYER_SAVE_WONDER_PUSH: ee_units_lists_add(thePlayer->mLists, EE_UNITS_LISTS_WONDER_IDS, (uint64_t)(long)io->i);
        return kErrorOK;
    // the save
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS: io->i = (int64_t)thePlayer->mUnitClassUpgrades.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT:
        return thePlayer->mUnitClassUpgrades[inIndex] != NULL;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE:
        return thePlayer->mUnitClassUpgrades[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_RTS_SAVE: thePlayer->RTSPlayer::Save(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_TECH_PRESENT:
        return thePlayer->mTechnologies != NULL;
    case EE_UNITS_PLAYER_SAVE_PATROL_SLOTS: io->i = (int64_t)thePlayer->mPatrolGroups.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_SAVE:
        return ee_units_pg_save(thePlayer->mPatrolGroups[inIndex], (uint32_t)theMajor, (uint32_t)theMinor, &theFile);
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
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_COUNT: io->i = (int64_t)thePlayer->mConstructionSites->GetSize();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_ID: io->i = (int64_t)(unsigned long)thePlayer->mConstructionSites->mItems[inIndex]->mUnitID;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERD_COUNT: io->i = (int64_t)thePlayer->mHerds.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERD_SAVE:
        return thePlayer->mHerds[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_SAVE:
        return thePlayer->mEvents.Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_COUNT: return ee_units_off_map_file(thePlayer->mLists, 0, 0, &theFile, &EESaveFilePrimitive, io);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_SAVE: return ee_units_off_map_file(thePlayer->mLists, 1, inIndex, &theFile, &EESaveFilePrimitive, io);
    case EE_UNITS_PLAYER_SAVE_WONDER_COUNT: io->i = (int64_t)thePlayer->mWonders->GetSize();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_WONDER_ID: io->i = (int64_t)(unsigned long)thePlayer->mWonders->mItems[inIndex]->mUnitID;
        return kErrorOK;
    // the compound operations' statements (ee_units player_save_split.rs)
    case EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT: return (int32_t)EEUnitClassCount();
    case EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT: return gUnitClassTable[inIndex] != NULL;
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
    case EE_UNITS_PLAYER_SAVE_X_TECH_INDEX: thePlayer->mTechnologies->SetPlayerIndex(thePlayer->mPlayerID);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_PLAYER: thePlayer->mTechnologies->SetPlayer(thePlayer);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_LOAD: return thePlayer->mTechnologies->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_X_PATROL_NEW: io->i = (int64_t)(uintptr_t)ee_units_pg_new();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD: return ee_units_pg_load(reinterpret_cast<EEPPatrolGroup*>(io->i), (uint32_t)theMajor, (uint32_t)theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_X_PATROL_ADD: thePlayer->AddPatrolGroup(reinterpret_cast<EEPPatrolGroup*>(io->i));
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_PATROL_RELEASE: ee_units_pg_dereference(reinterpret_cast<EEPPatrolGroup*>(io->i));
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_WONDERS_CLEAR: thePlayer->mWonders->Clear();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_WONDERS_ITEMS_CLEAR: thePlayer->mWonders->mItems.clear();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_X_TECH_SAVE: return thePlayer->mTechnologies->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_X_PATROL_AT: io->i = (int64_t)(uintptr_t)thePlayer->mPatrolGroups[inIndex];
        return kErrorOK;
    }
    ESVERIFY_AT(false, inOp);
    return kErrorOK;
}

// Loads the class media of every unit class that has media and records those classes for a later unload
void EEPlayer::LoadAllClassMedia()
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_LOAD_ALL_CLASS_MEDIA, 0, 0, 0);   // player_rest.rs
}

// Loads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::LoadUnitClassButtons(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->LoadButtons(this);
}

// Loads the media of one unit class for the player, if the class index is valid
void EEPlayer::SetUnitClassMedia(long inUnitClass, long inMediaFlags)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->LoadMedia(this, inMediaFlags);
}

// LoadUnit's and SaveUnit's C++ parts (ee_units player_save.rs load_unit, save_unit): the file's primitives, the
// class's fit, the loads, the saved unit's kind and its Save
struct EEPlayerUnitContext {
    EEPlayer* mPlayer;
    FSFile* mFile;
    RTSUnit* mUnit;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
};

static int32_t EEPlayerUnitOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerUnitContext& c = *static_cast<EEPlayerUnitContext*>(inContext);
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(*c.mFile, inOp, io);
    EEUnitClass* theClass = (uint64_t)io->i < gUnitClassTable.size() ? gUnitClassTable[(size_t)io->i] : NULL;
    EEUnit* theUnit = static_cast<EEUnit*>(c.mUnit);
    EEUnit* theIt = static_cast<EEUnit*>(EEPlayerUnitAt(io->i));   // a unit the Rust holds (player_save_split.rs)
    switch (inOp) {
    case EE_UNITS_PLAYER_SAVE_UNIT_SAVE: return theUnit->Save(c.mMajorVersion, c.mMinorVersion, *c.mFile);
    // the compound operations' statements (ee_units player_save_split.rs)
    case EE_UNITS_PLAYER_SAVE_X_CLASS_THERE: return theClass != NULL;
    case EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE: return (int32_t)theClass->mUnitType;
    case EE_UNITS_PLAYER_SAVE_X_CLASS_COMPLEX: return theClass->mIsComplex;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_CREATE: io->i = EEPlayerUnitArg(theClass->CreateUnitAtTile(c.mPlayer, -10000, -10000, NULL, 3.1415927f));
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD: return theIt->Load(c.mMajorVersion, c.mMinorVersion, *c.mFile);
    case EE_UNITS_PLAYER_SAVE_X_UNIT_QUEUED_DELETE: return theIt->mIsQueuedForDelete;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_KILL: c.mPlayer->KillUnit(theIt, true);
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_COMPLEX_NEW: io->i = EEPlayerUnitArg(EEComplexUnit::Create());
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_UNIT_NEW: io->i = EEPlayerUnitArg(EEUnit::Create());
        return 0;
    case EE_UNITS_PLAYER_SAVE_X_LOADED_ID: ee_units_lists_add(c.mPlayer->mLists, EE_UNITS_LISTS_LOADED_IDS, theIt->GetUnitID());
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
    EEPlayerUnitContext theContext = {this, &inFile, NULL, inMajorVersion, inMinorVersion};
    return ee_units_player_unit(&theContext, &EEPlayerUnitOperation, false, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Returns the player's name
const UWideString& EEPlayer::GetName()
{
    return this->mName;
}

// Plays a unit's acknowledgement sound, at most once per delay for the same selection
// NEAR (71.9): registers only (the original caches TMGetTime in edi and the delay in ebx)
void EEPlayer::PlayAcknowledgement(EEComplexUnit* inUnit)
{
    ee_units_selection(this, 9, inUnit, 0, NULL, 0, false, false);
}

// Replaces each swapped old unit by its new one in all units, air traffic, the selection, control groups, wonders
// Called when the physics replaced units (EEPhysics 0x1534d1); a new unit of another player is not added to
// the groups, and an unbuilt wall becomes a construction site
void EEPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    ee_units_player_units_swapped(this, reinterpret_cast<void* const (*)[2]>(ioSwaps.data()), ioSwaps.size(), &ioSwaps);   // player_body.rs
}

// Makes every town of the player recheck whether it is near water
void EEPlayer::RestartTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (this->mTowns[i] != NULL)
            this->mTowns[i]->UpdateNearWater();
    }
}

// Empties the player: running and control groups, viewed unit, events, towns, herds, off-map units, patrols
void EEPlayer::Clear()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CLEAR, 0, 0, 0, 0.0f);   // ee_units player_body.rs clear
}

// Removes a player from the ally list
void EEPlayer::RemoveAlly(EEPlayer* inPlayer)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REMOVE_ALLY, (uint64_t)(uintptr_t)inPlayer, 0, 0);   // player_rest.rs
}

// Stops sharing the player's building sight with another player, refreshing every land building's sight
void EEPlayer::RemoveSightSharer(EEPlayer* inPlayer)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 2, inPlayer, 0, false);
}

// Finishes running groups left without units, then releases and drops every finished group
void EEPlayer::RemovalsDone()
{
    ee_units_groups_player(this, 3);   // ee_units group_obj.rs removals_done
}

// Cancels and refunds a queued improvement (inFlag: never the running one) (ee_units player.rs)
void EEPlayer::RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_REMOVE_QUEUED, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL);
}

// Removes a patrol group from the player and releases it
void EEPlayer::RemovePatrolGroup(EEPPatrolGroup* inGroup)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REMOVE_PATROL, (uint64_t)(uintptr_t)inGroup, 0, 0);   // player_rest.rs
}

// Deselects the unit with the given ID, if it exists
void EEPlayer::RemoveUnitFromSelectedGroup(long inUnitID)
{
    ee_units_selection(this, 8, NULL, inUnitID, NULL, 0, false, false);
}

// Takes a unit being removed off its tile, its region and the influence map
void EEPlayer::RemovingUnit(RTSUnit* inUnit)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REMOVING_UNIT, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0);   // player_rest.rs
}

// Deletes the off-map record of a unit, if it has one
void EEPlayer::RemoveOffMapUnit(RTSUnit* inUnit)
{
    ee_units_off_map(this->mLists, 1, EEPlayerUnitArg(inUnit), 0, 0, 0);
}

// Removes a unit from the selection, or clears the viewed unit when that is the unit
void EEPlayer::DeselectUnit(EEUnit* inUnit)
{
    ee_units_selection(this, 7, inUnit, 0, NULL, 0, false, false);
}

// Removes a wonder from the player; when the last of its type goes, its wonder power is taken away
void EEPlayer::RemoveWonder(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 8, inUnit);
}

// Lets every unit of the player finish loading; returns 0
long EEPlayer::FinishUnitsLoad(long inMajorVersion, long inMinorVersion)
{
    return (long)ee_units_player_rest(this, EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD, 0, inMajorVersion, inMinorVersion);   // player_rest.rs
}

// port (save version 0.260): the towns' ids and order. A load rebuilds every town from its units (RepairPointers),
// numbering them on from the saved counter (++mTownIDCounter) in the load's order, so the towns of the loaded game had
// other ids than the saved game's, which the saved AI state names its towns by (unit requests, defense packages,
// attack sub groups: GetTownByID), and stood in another order in mTowns, which the AI walks. Saved: the id counter
// and per player each town's id, hull points, counters and flags in mTowns' order; a rebuilt town whose hull equals a
// saved one takes its id and its counters, and when every town is matched they take the saved order. From 0.268 the
// towns are made from their saved areas instead. The save, the load, the pending towns and the matching are Rust
// (ee_units town_save.rs) over EETownSaveHost.

// the counters a town's units summed as they came and went (the rebuild counts them once each, where the running
// game counted a farm at its placement and again at its completion)
static long* EETownCounter(EETown* inTown, int64_t inIndex)
{
    switch (inIndex) {
    case 0: return (long*)&inTown->mValue;
    case 1: return &inTown->mNumWalls;
    case 2: return &inTown->mL80;
    case 3: return &inTown->mNumWonders;
    case 4: return &inTown->mNumDocks;
    case 5: return &inTown->mNumAirports;
    case 6: return &inTown->mNumUnits;
    case 7: return &inTown->mReach;
    }
    return NULL;
}

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
    case EE_UNITS_TOWN_PLAYERS: io->prim.i = gWorld.mNumPlayers; return 0;
    case EE_UNITS_TOWN_X_CONTEXT_PLAYER: io->prim.i = (int64_t)(uintptr_t)theContext.mPlayer; return 0;
    case EE_UNITS_TOWN_X_WORLD_PLAYER: io->prim.i = (int64_t)(uintptr_t)gWorld.mPlayers[io->prim.i]; return 0;
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

// Relinks the player after a load: towns, construction sites, wonders, groups, herds, events, queued improvements
// and off-map units are rebuilt from saved IDs; returns the status of the last step
// NEAR: inline budget (the original calls begin() out of line for mPatrolGroups[i] and mConstructionSiteIDs[j]), register allocation
// (the GetAvailablePoints test is != 0 here; the original compares unsigned, jbe)
long EEPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return (long)ee_units_player_body(this, EE_UNITS_PLAYER_DO_REPAIR_POINTERS, inFirst, (int64_t)inSecond, 0, 0.0f);   // player_body.rs
}

// When a human player reaches a valid epoch, lets every computer player catch up to it
void EEPlayer::EpochReached(long inEpoch)
{
    // port: the original pulled each computer player's epoch timer forward here (EEComputerPlayer::CatchUpEpoch, the
    // catch-up cheat); the timer went with the C++ classic's decisions (stage 5): a Lua AI reads the epochs itself
    (void)inEpoch;
}

// Starts the next queued improvement's timer; true when the queue is empty (ee_units player.rs advance)
bool EEPlayer::AdvanceImprovement(unsigned long inUnitType)
{
    return ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_ADVANCE_IMPROVEMENT, NULL, (int32_t)inUnitType, 0, 0, 0, 0, 0.0f, NULL) != 0;
}

#include "lle/FSMemoryFile.h"

// Rebuilds the player's tech tree and upgrades from scratch, keeping its state and epoch range, and unloads the
// unit media its nodes had loaded
void EEPlayer::ReloadTechTree(bool inFlag)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_RELOAD_TECH_TREE, 0, inFlag, 0);   // player_rest.rs
}

// Briefly reveals the area around one unit (by ID) or all units of a type, with the given sight range
void EEPlayer::PulseSight(bool inByID, float inSight, long inID)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_PULSE_SIGHT, (uint64_t)(int64_t)inID, inByID, 0, inSight);   // player_body.rs
}

// Writes the player's state to a saved game; returns the write status (some writes' errors are ignored).
// port (R5b.4): the body is Rust (ee_units player_save.rs `save`) over the record this class derives from and
// EEPlayerSaveOperation's parts
long EEPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEPlayerSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    return ee_units_player_save(&theContext, &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Writes one unit's type, kind and data to a saved game (ee_units player_save.rs save_unit; path points saved as units
// from 0.254)
long EEPlayer::SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEPlayerUnitContext theContext = {this, &outFile, inUnit, inMajorVersion, inMinorVersion};
    return ee_units_player_unit(&theContext, &EEPlayerUnitOperation, true, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Selects the unit with the given ID (optionally clearing the selection), playing its sound for own units
void EEPlayer::SelectUnit(unsigned long inUnitID, bool inArg1, bool inNewGroup)
{
    ee_units_selection(this, 2, NULL, (int64_t)inUnitID, NULL, 0, inArg1, inNewGroup);
}

// Replaces the selection with the built units of the given IDs and plays the last one's selection sound
void EEPlayer::SelectUnits(std::vector<long>& inUnitIDs)
{
    ee_units_selection(this, 6, NULL, 0, inUnitIDs.data(), inUnitIDs.size(), false, false);
}

// Sets every resource amount of the player to the same value
void EEPlayer::SetAllResources(long inValue)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_SET_ALL_RESOURCES, 0, inValue, 0);   // player_rest.rs
}

// Sets the player's civilization points (current and maximum) for an epoch
void EEPlayer::SetCivilizationPoints(long inEpoch)
{
    this->mCivilization.SetPointsAndMax(inEpoch);
}

// Changes the player's stance toward another player, forming or breaking the alliance and its shared sight
void EEPlayer::SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 0, inOther, (unsigned long)inStance, inMap != NULL);
}

// Sets the player's current epoch in its tech tree
void EEPlayer::SetEpoch(long inEpoch)
{
    this->mTechnologies->SetEpoch(inEpoch);
}

// Sets the viewed unit of the selection, moving the reference
void EEPlayer::SetSelectedViewedUnit(RTSUnit* inUnit)
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0, 0.0f);   // player_body.rs
}

// Sets the viewed unit, moving the reference; with a unit and inFlag the selection is cleared first
void EEPlayer::UpdateSelection(EEUnit* inUnit, bool inFlag)
{
    ee_units_selection(this, 0, inUnit, 0, NULL, 0, inFlag, false);
}

// Sets the player's override unit class
void EEPlayer::SetHackUnitClass(long inUnitClass)
{
    this->mHackUnitClass = inUnitClass;
}

// Turns the player's shared line of sight on or off, recomputing all its units' sight and events
void EEPlayer::SetSharedLOS(bool inFlag, EEMap* inArg)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 3, NULL, 0, inFlag);
}

// Copies the player's visibility of every tile into the terrain mesh
void EEPlayer::RefreshTerrainVisibility()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_REFRESH_TERRAIN, 0, 0, 0, 0.0f);   // player_body.rs
}

// Sets the player's game result; a new victory or defeat of another player is announced in the chat log
// the chat log's line of a player's victory or defeat (player_rest.rs set_game_result)
static void EEPlayerAnnounceResult(EEPlayer* inPlayer, bool inVictory)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theText;
    UWideString theReason;
    UWideString theTitle;
    UWideString thePlayerName;
    gResource->GetString(inVictory ? IDS_PLAYER_IS_VICTORIOUS : IDS_PLAYER_HAS_BEEN_DEFEATED, theReason);
    gResource->GetString(IDS_MSG_GAME, theTitle);
    if (inPlayer->IsHuman())
        sprintf(theText, theReason, (const char*)inPlayer->mName);
    else {
        sprintf(thePlayerName, "%s - (%s)", (const char*)inPlayer->mCivilization.mName, (const char*)inPlayer->mName);
        sprintf(theText, theReason, (const char*)thePlayerName);
    }
    gUserInterface->AddChatLogEntry(theColor, theTitle, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
}

void EEPlayer::SetGameResult(long inState)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_SET_GAME_RESULT, 0, inState, 0);   // player_rest.rs
}

// Subtracts an amount from one of the player's resources
void EEPlayer::TakeResource(long inIndex, long inAmount)
{
    this->mResourceCaps[inIndex] -= inAmount;
}

// Pays a technology's cost scaled by inFraction (ee_units player.rs pay)
void EEPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY, inTechnology, inUnitType, 0, 0, 0, 0, inFraction, NULL);
}

// Pays a unit class improvement at its queued level (ee_units player.rs pay_improvement)
void EEPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_PAY_IMPROVEMENT, inClass, inImprovement, 0, 0, 0, 0, 0.0f, NULL);
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

// Unloads the class media of every unit class recorded as loaded and empties the record
void EEPlayer::UnloadAllClassMedia()
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNLOAD_ALL_CLASS_MEDIA, 0, 0, 0);   // player_rest.rs
}

// Unloads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassButtons(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->UnloadButtons(this);
}

// Unloads the media of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassMedia(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->UnloadMedia(this);
}

// Raises (or with inFlag lowers) a unit class improvement and rescales existing units' power, hit points or sight
void EEPlayer::ApplyImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_APPLY_IMPROVEMENT, NULL, inUnitType, inImprovement, inFlag, 0, 0, 0.0f, NULL);
}

// Registers a unit placed in the game: wonder/teleporter counts, town, herd, sight power, walls
void EEPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 1, inUnit);
}

// Unregisters a unit leaving the player: counts, wonders, town, herd, construction site, heroes, capitol, walls/forest
// EEUnit::DepleteResource passes (this, 0); inArg is not read. Losing the last capitol re-enables the
// capitol technology and blocks the JIT-manufacturing ones
void EEPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    EEPlayerUnits(this, 2, inUnit, inSwapped);
}

#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"

// Reattaches a unit to the AI and drops it from its group when it has no further goals or builds
void EEPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_UNIT_GOAL_DONE, (uint64_t)EEPlayerUnitArg(inUnit), 0, 0);   // player_rest.rs
}

// Returns whether a unit ID is in the list of IDs loaded into discarded units
bool EEPlayer::HasLoadedUnitID(unsigned long inUnitID)
{
    return ee_units_lists_has(this->mLists, EE_UNITS_LISTS_LOADED_IDS, inUnitID);
}

// Clears the viewed unit and the selection
void EEPlayer::UnselectAllUnits()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_UNSELECT_ALL, 0, 0, 0, 0.0f);   // player_body.rs
}


// Sends each enabled unit waiting to repatrol to the nearest point of its nearest town's patrol, then clears the list
void EEPlayer::RestorePatrols()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_RESTORE_PATROLS, 0, 0, 0, 0.0f);   // player_body.rs
}

// Gives every town without a patrol group a new one
void EEPlayer::RefreshTownGroups()
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_REFRESH_TOWN_GROUPS, 0, 0, 0);   // player_rest.rs
}

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
    switch (inOp) {
    case EE_UNITS_PLAYER_TICK_HERD_COUNT:
        return (int32_t)thePlayer->mHerds.size();
    case EE_UNITS_PLAYER_TICK_GAME_TIME: io->i = (int64_t)gPhysics->mGameTime;
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_DUE:
        io->i = (int64_t)thePlayer->mHerds[inIndex]->mNextUpdateTime;
        io->b = thePlayer->mHerds[inIndex]->mIsActive ? 1 : 0;
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_UPDATE: thePlayer->mHerds[inIndex]->Update((unsigned long)io->i);
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_STATE:
        return (int32_t)thePlayer->mHerds[inIndex]->mState;
    case EE_UNITS_PLAYER_TICK_HERD_KILL: ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_KILL_HERD_UNITS, (uint64_t)(uintptr_t)thePlayer->mHerds[inIndex], 0, 0);
        return 0;
    case EE_UNITS_PLAYER_TICK_TICK_TIME: io->f = gPhysics->GetTickTime();
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_APPLY: thePlayer->ApplyImprovement((long)io->i, (long)inIndex, false);
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE:
        return thePlayer->AdvanceImprovement((unsigned long)io->i) ? 1 : 0;
    case EE_UNITS_PLAYER_TICK_TICK_MS: io->i = (int64_t)gPhysics->mTickTimeMs;
        return 0;
    case EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS: theUnit->SetHitPoints((long)(int32_t)inIndex);
        return 0;
    // the compound operations' statements (ee_units player_tick.rs split)
    case EE_UNITS_PLAYER_TICK_X_VIEWED_ALONE: return thePlayer->mViewedUnit != NULL && thePlayer->mViewedUnit->mReferenceCount == 1;
    case EE_UNITS_PLAYER_TICK_X_UNVIEW: thePlayer->UpdateSelection(NULL, true);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE: return thePlayer->mSelectedViewedUnit != NULL && thePlayer->mSelectedViewedUnit->mReferenceCount == 1;
    case EE_UNITS_PLAYER_TICK_X_SELECTED_UNVIEW: thePlayer->SetSelectedViewedUnit(NULL);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_VALIDATE_GROUPS: ee_units_groups_player(thePlayer, 4);   // (group_obj.rs validate_player)
        return 0;
    case EE_UNITS_PLAYER_TICK_X_AIR: ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_UPDATE_AIR_TRAFFIC, 0, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_EVENTS: thePlayer->mEvents.TickEvents(thePlayer, gPhysics->mTickTime);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_AMOUNT: io->i = gWorldTable.GetLongValue(kWorldWonderHealAmount);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_EPOCH: io->i = thePlayer->mTechnologies->mCurrentEpoch;
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_STEP: io->i = gWorldTable.GetLongValue(kWorldWonderHealEpochStep);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR: io->f = gWorldTable.GetFloatValue(kWorldWonderHealEpochFactor);
        return 0;
    case EE_UNITS_PLAYER_TICK_X_NEXT_UNIT: io->i = (int64_t)reinterpret_cast<uintptr_t>(theUnit == NULL ? thePlayer->mFirstUnit : static_cast<EEUnit*>(theUnit->mNext));
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

// Disallows the technologies a map forbids (by map flag; space units off space maps) and sets the selection hotkey
void EEPlayer::ApplyMapRestrictions()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_MAP_RESTRICTIONS, 0, 0, 0, 0.0f);   // player_body.rs
}

// Sets whether the unit and its following parts can cloak; turning it off also uncloaks them
void EEComplexUnit::SetCloakableChain(bool inValue)
{
    ee_units_cloak_chain((uint64_t)EEPlayerUnitArg(this), 0, inValue);   // ee_units player_rest.rs
}

// Cloaks or uncloaks the unit and its following parts
void EEComplexUnit::SetCloakedChain(bool inValue)
{
    ee_units_cloak_chain((uint64_t)EEPlayerUnitArg(this), 1, inValue);
}

// Records a media warning when a unit class's buttons were not preloaded for the player
void EEPlayer::CheckButtonsPreloaded(long inUnitClass)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_MEDIA_CHECK, (uint64_t)inUnitClass, 1, 0);   // player_rest.rs
}

// Records a media warning when a unit class's media was not preloaded for the player (loads nothing)
void EEPlayer::LoadUnitClassMedia(long inUnitClass)
{
    ee_units_player_rest(this, EE_UNITS_PLAYER_REST_MEDIA_CHECK, (uint64_t)inUnitClass, 0, 0);   // player_rest.rs
}

// EETechTreeNode 0x1cf989 / 0x1cfcc9 call these through slots +0x88 / +0xcc; both slots hold the shared empty
// `ret 4` 0x557269 (ICF), so each has an empty body (this also removes two unresolved virtuals)
void EEPlayer::EpochMediaLoaded(long inEpoch)
{
}

void EEPlayer::EpochMediaUnloaded(long inEpoch)
{
}

bool EEPlayer::AvoidsEnemies(class EEComplexUnit *)
{
    return false;
}

// Returns the number of living heroes and/or heroes in production
long EEPlayer::GetHeroCount(bool inFirst, bool inSecond)
{
    return (long)ee_units_player_econ(EEPlayerRefs(this), EE_UNITS_ECON_NUM_HEROES, NULL, inFirst, inSecond, 0, 0, 0, 0.0f, NULL);
}

bool EEPlayer::IsHuman()
{
    return false;
}

void EEPlayer::UnitStranded(class EEComplexUnit *)
{
}

void EEPlayer::Virtual60()
{
}

void EEPlayer::WonderStarted(class EEComplexUnit *)
{
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
    case EE_UNITS_PLAYER_BODY_SET_ACTIVE: thePlayer->mActiveGroup = static_cast<EESelectedGroup*>(theGroup);
        return 0;
    case EE_UNITS_PLAYER_BODY_SET_SELECTED: thePlayer->mSelectedGroup = static_cast<EESelectedGroup*>(theGroup);
        return 0;
    case EE_UNITS_PLAYER_BODY_SET_HOT_KEY: thePlayer->mGroups[inB] = theGroup;
        return 0;
    case EE_UNITS_PLAYER_BODY_EXECUTING_LOCAL: gWorld.mExecutingLocalSelection = inB != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_EXECUTE: reinterpret_cast<NEMessage*>(static_cast<uintptr_t>(inA))->Execute();
        return 0;
    case EE_UNITS_PLAYER_BODY_SELECTED_VIEWED: return EEPlayerUnitArg(thePlayer->mSelectedViewedUnit);
    case EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED: thePlayer->mSelectedViewedUnit = theUnit;
        return 0;
    case EE_UNITS_PLAYER_BODY_SET_VIEWED: thePlayer->mViewedUnit = theUnit;
        return 0;
    case EE_UNITS_PLAYER_BODY_NEXT_COMPLEX: return ee_units_player_body(thePlayer, EE_UNITS_PLAYER_DO_NEXT_COMPLEX, inA, 0, 0, 0.0f);
    case EE_UNITS_PLAYER_BODY_REPLACE_UNITS: theComplex->ReplaceUnits(*theSwaps);
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_REPLACE: EEATCReplaceUnits(thePlayer->mAirTraffic[inB], *theSwaps);
        return 0;
    case EE_UNITS_PLAYER_BODY_UNBUILT_WALL: return theComplex->mUnitClass->mIsWall && !theComplex->mIsBuilt;
    case EE_UNITS_PLAYER_BODY_ADD_SITE: thePlayer->AddConstructionSite(theComplex);
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDER_REPLACE: thePlayer->mWonders->Replace(theComplex, EEPlayerUnitAt(inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_DOPPELGANGERS: gDoppelgangerManager.UnregisterPlayer(thePlayer);
        return 0;
    case EE_UNITS_PLAYER_BODY_EVENTS_CLEAR: thePlayer->mEvents.Clear(inB != 0 ? thePlayer : NULL);
        return 0;
    case EE_UNITS_PLAYER_BODY_SITES_CLEAR: thePlayer->mConstructionSites->Clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_SITES_ITEMS_CLEAR: thePlayer->mConstructionSites->mItems.clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_CLEAR: thePlayer->mSavedGroups.Clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_NAME: thePlayer->mSavedGroups.SetName(thePlayer->GetName());
        return 0;
    case EE_UNITS_PLAYER_BODY_STORE_TABLE: EEUnitStorePlayerTable(thePlayer);   // New (R5b.4): the store's diplomacy table
        return 0;
    case EE_UNITS_PLAYER_BODY_CLASS_COUNT: return EEUnitClassCount();
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
    case EE_UNITS_PLAYER_BODY_TECH_NULL: thePlayer->mTechnologies = NULL;
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_NEW: return (int64_t)(uintptr_t)EEATCNew();
    case EE_UNITS_PLAYER_BODY_ATC_SET: thePlayer->mAirTraffic[inB] = reinterpret_cast<EeUnitsAtc*>(static_cast<uintptr_t>(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_DELETE: ee_units_atc_delete(thePlayer->mAirTraffic[inB]);
        return 0;
    case EE_UNITS_PLAYER_BODY_ATC_REPAIR: return ee_units_atc_repair_pointers(thePlayer->mAirTraffic[inB]);
    case EE_UNITS_PLAYER_BODY_WORLD_LONG: return gWorldTable.GetLongValue((EEWorldConstant)inB);
    case EE_UNITS_PLAYER_BODY_HEALED_CLEAR: thePlayer->mHealedUnitSet = NULL;
        return 0;
    case EE_UNITS_PLAYER_BODY_EFFECT: return (int64_t)(uintptr_t)static_cast<EEMedia*>(EEGraphicEffect::Get((unsigned long)inB));
    case EE_UNITS_PLAYER_BODY_MEDIA_LOAD: theMedia->LoadMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_MEDIA_UNLOAD: theMedia->UnloadMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_OFF_MAP: ee_units_off_map(thePlayer->mLists, (uint32_t)inB, 0, 0, 0, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_PATROLS_RELEASE: std::for_each(thePlayer->mPatrolGroups.begin(), thePlayer->mPatrolGroups.end(), EEPDereferencer());
        return 0;
    case EE_UNITS_PLAYER_BODY_PATROLS_CLEAR: thePlayer->mPatrolGroups.clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDERS_DELETE: delete thePlayer->mWonders;
        return 0;
    case EE_UNITS_PLAYER_BODY_SITES_DELETE: delete thePlayer->mConstructionSites;
        return 0;
    case EE_UNITS_PLAYER_BODY_QUEUED_FREE: ee_units_queued_free(thePlayer->mQueued);
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDER_STATE_FREE: ee_units_wonders_free(thePlayer->mWonderState);
        return 0;
    case EE_UNITS_PLAYER_BODY_LISTS_FREE: ee_units_lists_free(thePlayer->mLists);
        return 0;
    case EE_UNITS_PLAYER_BODY_GROUP_LISTS_FREE: ee_units_player_groups_free(thePlayer->mGroupLists);
        return 0;
    case EE_UNITS_PLAYER_BODY_RTS_CLEAR: thePlayer->RTSPlayer::Clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_CIV_REAPPLY: thePlayer->mCivilization.Reapply(thePlayer, (unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_RTS_REPAIR: thePlayer->RTSPlayer::RepairPointers((unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_PATROL_COUNT: return (int64_t)thePlayer->mPatrolGroups.size();
    case EE_UNITS_PLAYER_BODY_PATROL_REPAIR: ee_units_pg_repair(thePlayer->mPatrolGroups[inB]);
        return 0;
    case EE_UNITS_PLAYER_BODY_MAKE_SAVED_TOWNS: return EEMakeSavedTowns(thePlayer);
    case EE_UNITS_PLAYER_BODY_NEXT_UNIT: return EEPlayerUnitArg(theUnit == NULL ? thePlayer->mFirstUnit : theUnit->mNext);
    case EE_UNITS_PLAYER_BODY_LIVE_STATE: return EESaveHasLiveState((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_PLAYER_BODY_ADD_TO_TOWN: return ee_units_player_areas(EEPlayerRefs(thePlayer), 0, static_cast<RTSUnit*>(theComplex));
    case EE_UNITS_PLAYER_BODY_CIV_POINTS: return !thePlayer->mCivilization.GetNoPointsAdded() && thePlayer->mCivilization.GetAvailablePoints() != 0;
    case EE_UNITS_PLAYER_BODY_CIV_BLINK: return EESetCivBuilderButtonBlinking(true);
    case EE_UNITS_PLAYER_BODY_CONTAINER_ADD: (inB == 0 ? thePlayer->mConstructionSites : thePlayer->mWonders)->Add(theComplex);
        return 0;
    case EE_UNITS_PLAYER_BODY_RECOUNT_POPULATION: thePlayer->RecountPopulation();
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_COUNT: return (int64_t)thePlayer->mHerds.size();
    case EE_UNITS_PLAYER_BODY_HERD_REPAIR: thePlayer->mHerds[(size_t)inA]->RepairPointers((unsigned long)inB, (unsigned long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_REGISTER_HERDS: thePlayer->RegisterHerds();
        return 0;
    case EE_UNITS_PLAYER_BODY_EVENTS_REPAIR: return thePlayer->mEvents.RepairEvents(thePlayer, (unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_PLAYER_BODY_REQUEUE: EEPlayerImprovement(thePlayer, 10, 0, 0);   // the queued improvements queued again (ee_units player.rs requeue)
        return 0;
    case EE_UNITS_PLAYER_BODY_CAPITOLS: return thePlayer->mNumCapitols;
    case EE_UNITS_PLAYER_BODY_TECH_OBSOLETE: return thePlayer->GetTechTree()->Get((long)inB)->GetIsObsolete();
    case EE_UNITS_PLAYER_BODY_TECH_ALLOW: thePlayer->GetTechTree()->Get((long)inB)->mIsAllowed = inC != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_APPLY_SAVED_TOWNS: EEApplySavedTowns(thePlayer);   // port (0.260): the rebuilt towns take their saved ids and order
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_COUNT: return (int64_t)thePlayer->mTowns.size();
    case EE_UNITS_PLAYER_BODY_TOWN_UNPATROLLED: return thePlayer->mTowns[inB] != NULL && thePlayer->mTowns[inB]->mPatrolGroup == NULL;
    case EE_UNITS_PLAYER_BODY_TOWN_ADOPT: return ee_units_player_rest(thePlayer, EE_UNITS_PLAYER_REST_ADOPT_TOWN_PATROL, (uint64_t)(uintptr_t)thePlayer->mTowns[inB], 0, 0);
    case EE_UNITS_PLAYER_BODY_TOWN_REFRESH: thePlayer->mTowns[inB]->RefreshPatrolGroup();
        return 0;
    case EE_UNITS_PLAYER_BODY_TILE_X: return static_cast<EEUnit*>(theUnit)->GetTileX();
    case EE_UNITS_PLAYER_BODY_ADD_OFF_MAP: thePlayer->AddOffMapUnit(theUnit);
        return 0;
    case EE_UNITS_PLAYER_BODY_RECOUNT_HEROES: EEPlayerUnits(thePlayer, 5);
        return 0;
    case EE_UNITS_PLAYER_BODY_ADD_MEDIA: thePlayer->AddLoadedMedia(theMedia);
        return 0;
    case EE_UNITS_PLAYER_BODY_REMOVE_MEDIA: return ee_units_lists_remove(thePlayer->mLists, EE_UNITS_LISTS_MEDIA, (uint64_t)(uintptr_t)theMedia);
    case EE_UNITS_PLAYER_BODY_UNLOAD_ALL_MEDIA: ee_units_player_body(thePlayer, EE_UNITS_PLAYER_DO_UNLOAD_ALL_MEDIA, 0, 0, 0, 0.0f);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLEAR_REPATROL: ee_units_lists_clear(thePlayer->mLists, EE_UNITS_LISTS_REPATROL, true);
        return 0;
    case EE_UNITS_PLAYER_BODY_DISABLED: return theComplex->mIsDisabled;
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
    case EE_UNITS_PLAYER_BODY_LINE_OF_SIGHT: return EEPlayerFloatBits(static_cast<EEUnit*>(theUnit)->GetStats()->GetLineOfSight());
    case EE_UNITS_PLAYER_BODY_COMPLEX_SIGHT_OFF: static_cast<EEUnit*>(theUnit)->GetComplexUnit()->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_BODY_SET_SIGHT: static_cast<EEUnit*>(theUnit)->GetComplexUnit()->SetSight(EEPlayerBitsFloat((int32_t)inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_SIGHT_ON: theUnit->SightOnHere();
        return 0;
    case EE_UNITS_PLAYER_BODY_SIGHT_ZERO: static_cast<EEUnit*>(theUnit)->GetComplexUnit()->mSight = 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_IS_LOCAL: return gWorld.mPlayers[gWorld.mCurrentPlayer] == thePlayer;
    case EE_UNITS_PLAYER_BODY_HOT_KEY_REMOVE: gUIManager->GetMainGameForm()->RemoveHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, 0);
        return 0;
    case EE_UNITS_PLAYER_BODY_WATERLESS: return (gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
    case EE_UNITS_PLAYER_BODY_NOT_SPACE: return !gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_PLAYER_BODY_HOT_KEY_ADD: gUIManager->GetMainGameForm()->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection((int32_t)inB, false, false, false)), NULL, NULL, NULL, 0, false);
        return 0;
    case EE_UNITS_PLAYER_BODY_HAS_CAMOUFLAGE: return gCivPowersManager.HasPower(thePlayer, kCivPowerCamouflage);
    case EE_UNITS_PLAYER_BODY_CAMOUFLAGE_SET: return (int64_t)(uintptr_t)gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    case EE_UNITS_PLAYER_BODY_SET_HAS: return reinterpret_cast<EEUnitSet*>(static_cast<uintptr_t>(inA))->Contains((long)inB);
    case EE_UNITS_PLAYER_BODY_COMMAND_TYPE: return theComplex->mCommand ? theComplex->mCommand->mType : 0;
    case EE_UNITS_PLAYER_BODY_SET_CAMOUFLAGE_TIMER: theComplex->mCamouflageTimer = (long)inB;
        return 0;
    case EE_UNITS_PLAYER_BODY_CLOAKABLE_OFF: theComplex->SetCloakableChain(false);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLOAKED_OFF: theComplex->SetCloakedChain(false);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLEAR_U32C: theComplex->mU32C = 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_TILE_Y: return static_cast<EEUnit*>(theUnit)->GetTileY();
    case EE_UNITS_PLAYER_BODY_SITE_COUNT: return (int64_t)thePlayer->mConstructionSites->mItems.size();
    case EE_UNITS_PLAYER_BODY_SITE_AT: return EEPlayerUnitArg(thePlayer->mConstructionSites->mItems[(size_t)inB]);
    case EE_UNITS_PLAYER_BODY_CLASS_STATS: return (int64_t)(uintptr_t)EEPlayerClassStats(thePlayer, (unsigned long)inB);
    case EE_UNITS_PLAYER_BODY_TERRAIN_MASK: return reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mTerrainMask;
    case EE_UNITS_PLAYER_BODY_TEAM_COUNT: return (int64_t)gWorld.GetNumTeams();
    case EE_UNITS_PLAYER_BODY_TEAM_HAS: return gWorld.mTeams[inB].ContainsPlayer((unsigned long)inC);
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
    case EE_UNITS_PLAYER_BODY_PATROL_AT: return (int64_t)(uintptr_t)thePlayer->mPatrolGroups[(size_t)inB];
    case EE_UNITS_PLAYER_BODY_DAMAGE_ADD: thePlayer->mDamageFrom[inB] += (long)inC;
        return 0;
    case EE_UNITS_PLAYER_BODY_IS_SELF: return gWorld.mPlayers[inB] == thePlayer;
    case EE_UNITS_PLAYER_BODY_ENEMY_OF: return thePlayer->mEnemies[gWorld.mPlayers[inB]->mPlayerID];
    case EE_UNITS_PLAYER_BODY_SET_COMBAT: thePlayer->mCombatState = (long)inB;
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_SET_INDEX: thePlayer->mTechnologies->SetPlayerIndex((long)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER: thePlayer->mTechnologies->SetPlayer(thePlayer);
        return 0;
    case EE_UNITS_PLAYER_BODY_BB44_CLEAR: thePlayer->mBB44 = false;
        return 0;
    case EE_UNITS_PLAYER_BODY_POPULATION_BONUS_CLEAR: thePlayer->mPopulationLimitBonus = 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_TRIBUTE_RESET: thePlayer->mTributeFee = EE_UNITS_PLAYER_TRIBUTE_FEE;
        return 0;
    case EE_UNITS_PLAYER_BODY_LAST_UNIT: return EEPlayerUnitArg(thePlayer->mLastUnit);
    case EE_UNITS_PLAYER_BODY_COMPLEX_OF: return EEPlayerUnitArg(static_cast<EEUnit*>(theUnit)->GetComplexUnit());
    case EE_UNITS_PLAYER_BODY_QUEUED_NEW: thePlayer->mQueued = ee_units_queued_new();
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDER_STATE_NEW: thePlayer->mWonderState = ee_units_wonders_new();
        return 0;
    case EE_UNITS_PLAYER_BODY_LISTS_NEW: thePlayer->mLists = ee_units_lists_new();
        return 0;
    case EE_UNITS_PLAYER_BODY_GROUP_LISTS_NEW: thePlayer->mGroupLists = ee_units_player_groups_new();
        return 0;
    case EE_UNITS_PLAYER_BODY_COMPUTER_CLEAR: thePlayer->mComputerPlayer = NULL;
        return 0;
    case EE_UNITS_PLAYER_BODY_HUMAN_CLEAR: thePlayer->mHumanPlayer = NULL;
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDERS_NEW: thePlayer->mWonders = new UReferenceContainer<EEComplexUnit*>;
        return 0;
    case EE_UNITS_PLAYER_BODY_SITES_NEW: thePlayer->mConstructionSites = new UReferenceContainer<EEComplexUnit*>;
        return 0;
    // the world lane, round 2 (player_rest.rs)
    case EE_UNITS_PLAYER_BODY_CONTINENT_ID: return gContinentManager.GetContinentID(inB, inC);
    case EE_UNITS_PLAYER_BODY_TOWN_CONTINENT: return EEPlayerTownAt(inA)->mContinent;
    case EE_UNITS_PLAYER_BODY_TOWN_CLOSEST: return EEPlayerPackPoint(EEPlayerTownAt(inA)->GetClosestPoint(U2DPoint<long>((long)inB, (long)inC)));
    case EE_UNITS_PLAYER_BODY_RTS_CHECKSUM: thePlayer->RTSPlayer::GetChecksum(*reinterpret_cast<ESChecksum*>(static_cast<uintptr_t>(inA)));
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
    case EE_UNITS_PLAYER_BODY_CLEAR_GOAL_QUEUE: theComplex->ClearGoalQueue((unsigned long)inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_ALLY_ERASE: thePlayer->mAllies.erase(thePlayer->mAllies.begin() + inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_PATROL_ERASE: thePlayer->mPatrolGroups.erase(thePlayer->mPatrolGroups.begin() + inB);
        return 0;
    case EE_UNITS_PLAYER_BODY_FINISH_LOAD: static_cast<EEUnit*>(theUnit)->FinishLoad((long)inB, (long)inC);
        return 0;
    case EE_UNITS_PLAYER_BODY_CLASS_UNLOAD_MEDIA: gUnitClassTable[(size_t)inB]->UnloadClassMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT: return (int64_t)reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->GetMediaCount();
    case EE_UNITS_PLAYER_BODY_STATS_LOAD_MEDIA: reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mUnitClass->LoadClassMedia();
        return 0;
    case EE_UNITS_PLAYER_BODY_STATS_TYPE: return (int64_t)reinterpret_cast<EEUnitStats*>(static_cast<uintptr_t>(inA))->mUnitClass->mUnitType;
    case EE_UNITS_PLAYER_BODY_DETACHED: return theComplex->mIsDetachedFromAI;
    case EE_UNITS_PLAYER_BODY_SET_DETACHED: theComplex->mIsDetachedFromAI = inB != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_GOALS0_SIZE: return (int64_t)theComplex->mGoalQueues[0].mGoals.size();
    case EE_UNITS_PLAYER_BODY_BUILD_QUEUE_SIZE: return (int64_t)theComplex->mCitizenBuildQueue.size();
    case EE_UNITS_PLAYER_BODY_ACTIVE_GROUP: return (int64_t)(uintptr_t)static_cast<EeUnitsGroup*>(theComplex->mActiveGroup);
    case EE_UNITS_PLAYER_BODY_GROUP_REMOVE: theGroup->RemoveUnit(EEPlayerUnitAt(inB));
        return 0;
    case EE_UNITS_PLAYER_BODY_WONDER_COUNT: return (int64_t)thePlayer->mWonders->mItems.size();
    case EE_UNITS_PLAYER_BODY_WONDER_AT: return EEPlayerUnitArg(thePlayer->mWonders->mItems[(size_t)inB]);
    case EE_UNITS_PLAYER_BODY_SET_CLOAKED: theComplex->mCloaked = (unsigned long)inB;
        return 0;
    case EE_UNITS_PLAYER_BODY_GAME_TIME: return (int64_t)gPhysics->mGameTime;
    case EE_UNITS_PLAYER_BODY_CLOAKED: return (int64_t)theComplex->mCloaked;
    case EE_UNITS_PLAYER_BODY_ATC_UPDATE: ee_units_atc_update(thePlayer->mAirTraffic[inB]);
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
    case EE_UNITS_PLAYER_BODY_ANNOUNCE_RESULT: EEPlayerAnnounceResult(thePlayer, inB != 0);
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
    case EE_UNITS_PLAYER_BODY_MESH_Z: return (uint32_t)EEPlayerFloatBits(gWorld.mMap->mTerrainMesh.GetMeshZ(EEPlayerPtr<float>(inA)[0], EEPlayerPtr<float>(inA)[1]));
    case EE_UNITS_PLAYER_BODY_DROP_PATH: theComplex->DropPath();
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
    case EE_UNITS_PLAYER_BODY_MEDIA_WARNING: thePlayer->mMediaWarnings.push_back(UString(EEPlayerPtr<const char>(inA)));
        return 0;
    case EE_UNITS_PLAYER_BODY_HERD_AT: return (int64_t)(uintptr_t)thePlayer->mHerds[(size_t)inB];
    case EE_UNITS_PLAYER_BODY_HERD_DELETE: delete EEPlayerHerdAt(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_HERDS_CLEAR: thePlayer->mHerds.clear();
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
    case EE_UNITS_PLAYER_BODY_DUMP_TITLE: fprintf(*EEPlayerPtr<FSFileSpec>(inA), "\n\n\t\tMedia that might not have been loaded correctly:\n");
        return 0;
    case EE_UNITS_PLAYER_BODY_WARNINGS_SIZE: return (int64_t)thePlayer->mMediaWarnings.size();
    case EE_UNITS_PLAYER_BODY_DUMP_WARNING: fprintf(*EEPlayerPtr<FSFileSpec>(inA), "\t\t%s", (const char*)thePlayer->mMediaWarnings[(size_t)inB]);
        return 0;
    // the towns' save host's (town_split.rs)
    case EE_UNITS_PLAYER_BODY_TOWN_SLOTS: return (int64_t)thePlayer->mTowns.size();
    case EE_UNITS_PLAYER_BODY_TOWN_SLOT: return (int64_t)(uintptr_t)thePlayer->mTowns[(size_t)inB];
    case EE_UNITS_PLAYER_BODY_TOWN_ID: return EEPlayerTownAt(inA)->mTownID;
    case EE_UNITS_PLAYER_BODY_TOWN_SET_ID: EEPlayerTownAt(inA)->mTownID = (long)inB;
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_COUNTER: return *EETownCounter(EEPlayerTownAt(inA), inB);
    case EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER: *EETownCounter(EEPlayerTownAt(inA), inB) = (long)inC;
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER: return EEPlayerTownAt(inA)->mNearWater;
    case EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_WATER: EEPlayerTownAt(inA)->mNearWater = inB != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_NEAR_EDGE: return EEPlayerTownAt(inA)->mNearMapEdge;
    case EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_EDGE: EEPlayerTownAt(inA)->mNearMapEdge = inB != 0;
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT: EEPlayerTownAt(inA)->mContinent = (unsigned short)inB;
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_NEW: return (int64_t)(uintptr_t)new EETown(thePlayer, (long)inB);
    case EE_UNITS_PLAYER_BODY_TOWN_DELETE: delete EEPlayerTownAt(inA);
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWNS_PUSH: thePlayer->mTowns.push_back(EEPlayerTownAt(inA));
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWNS_CLEAR: thePlayer->mTowns.clear();
        return 0;
    case EE_UNITS_PLAYER_BODY_TOWN_READ_AREA: return EEReadAreaBytes(std::vector<unsigned char>(EEPlayerPtr<unsigned char>(inB), EEPlayerPtr<unsigned char>(inB) + inC), *EEPlayerTownAt(inA));
    }
    return 0;
}

// With the Camouflage civ power, sets the cloak timer of each idle unit (command 5000)
void EEPlayer::EnableCamouflagePower()
{
    ee_units_player_body(this, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 1, 0, 0.0f);   // player_body.rs
}
