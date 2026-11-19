// New (port): the world calls a Rust decision makes (rust/crates/ee_sim/src/units/host.rs; docs/RUST_PLAN.md R5b.4
// leaf 5): a group's units as ids, goals built from a plain record (units by id, resolved through the registry) and
// handed back as opaque handles. Every call runs on the thread that called the decision (the physics thread, under
// the world's write lock). TEST ONLY: the dry host (ee_sim units/dry.rs) answers the same calls without acting.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <algorithm>
#include <map>
#include <initializer_list>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEPPatrolGroup.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAirTrafficControl.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEUnitStore.h"
#include "ee/EEUnitHost.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EECivPowersManager.h"
#include "ee/EETechTreeTechID.h"
#include "lle/SESoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "rust/sim/units.h"

static EEUnit* UnitOf(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

static EEComplexUnit* ComplexOf(uint32_t inID)
{
    EEComplexUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

// ---- the host's calls ----

extern "C" size_t ee_units_host_group_ids(void* inGroup, uint32_t* outIDs, size_t inCapacity)
{
    EEGroup* theGroup = static_cast<EEGroup*>(inGroup);
    size_t theCount = theGroup->mUnits.size();
    for (size_t i = 0; i < theCount && i < inCapacity; i++) {
        EEComplexUnit* theUnit = theGroup->mUnits[i];
        outIDs[i] = theUnit != NULL ? static_cast<uint32_t>(theUnit->mUnitID) : 0xffffffffu;
    }
    return theCount;
}

extern "C" void* ee_units_host_goal_new(const EeGoalSpec* inSpec)
{
    bool theFlagA = inSpec->flag_a != 0;
    bool theFlagB = inSpec->flag_b != 0;
    switch (inSpec->ctor) {
    case EE_GOAL_CTOR_EXPLORE:
        return static_cast<EEUnitGoal*>(EEUGRust::NewExplore(theFlagA));
    case EE_GOAL_CTOR_ATTACK_GROUND:
        return static_cast<EEUnitGoal*>(EEUGRust::NewAttackGround(GE3DPoint(inSpec->point[0], inSpec->point[1], inSpec->point[2]), theFlagA));
    case EE_GOAL_CTOR_ATTACK_UNIT: {
        EEUnit* theTarget = UnitOf(inSpec->unit_b);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(EEUGRust::NewAttackUnit(ComplexOf(inSpec->unit_a), theTarget, theFlagA, theFlagB));
    }
    case EE_GOAL_CTOR_GATHER:
        return static_cast<EEUnitGoal*>(EEUGRust::NewGather(UnitOf(inSpec->unit_a), UnitOf(inSpec->unit_b), theFlagA));
    case EE_GOAL_CTOR_HEAL_UNIT: {
        EEComplexUnit* theTarget = ComplexOf(inSpec->unit_a);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(EEUGRust::NewHealUnit(theTarget, theFlagA));
    }
    case EE_GOAL_CTOR_PATROL:
        return static_cast<EEUnitGoal*>(EEUGRust::NewPatrol(theFlagA));
    case EE_GOAL_CTOR_PATROL_ROUTE:
        return static_cast<EEUnitGoal*>(EEUGRust::NewPatrol(static_cast<EEPPatrolGroup*>(inSpec->object), ComplexOf(inSpec->unit_a), theFlagA));
    case EE_GOAL_CTOR_LOCATION: {   // EEMPathTo's: EEUGRust::NewLocation(&point, true, forced, rotating, facing, false, 0)
        GE3DPoint theLocation(inSpec->point[0], inSpec->point[1], inSpec->point[2]);
        EEUGRust* theGoal = EEUGRust::NewLocation(&theLocation, true, theFlagA, theFlagB, inSpec->value, false, 0);
        theGoal->SetUseGroupMovement(inSpec->flag_c != 0);
        return static_cast<EEUnitGoal*>(theGoal);
    }
    case EE_GOAL_CTOR_UNLOAD_TRANSPORT: {   // EEMUnloadTransport's: the first unit's complex unit, the tile
        EEComplexUnit* theUnit = ComplexOf(inSpec->unit_a);
        if (theUnit == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(EEUGRust::NewUnloadTransport(theUnit->GetComplexUnit(), (long)inSpec->point[0],
                                                                (long)inSpec->point[1], theFlagA));
    }
    case EE_GOAL_CTOR_LOCATION_XY:   // EEMUnloadTransport's move
        return static_cast<EEUnitGoal*>(EEUGRust::NewLocation(inSpec->point[0], inSpec->point[1], 0.0f, false, true, false,
                                                         0.0f, false, 0));
    case EE_GOAL_CTOR_LOCATION_2D:   // EEMSmartFarmers' move
        return static_cast<EEUnitGoal*>(EEUGRust::NewLocation(GE2DPoint(inSpec->point[0], inSpec->point[1]), true, true, false,
                                                         0.0f, false, 0));
    case EE_GOAL_CTOR_BUILD_REPAIR: {   // EEMSetRallyGoal's: the farm or damaged building's owner, the building
        EEUnit* theTarget = UnitOf(inSpec->unit_a);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(EEUGRust::NewBuildRepair(theTarget->mPlayer, theTarget, theFlagA));
    }
    case EE_GOAL_CTOR_CAST_CALAMITY: {   // EEMCastCalamity's: the caster, the target or none, the tile, the technology
        EEComplexUnit* theUnit = ComplexOf(inSpec->unit_a);
        if (theUnit == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(EEUGRust::NewCastCalamity(theUnit, UnitOf(inSpec->unit_b), (long)inSpec->ints[0],
                                                                  (long)inSpec->ints[1], (long)inSpec->ints[2],
                                                                  theFlagA));
    }
    default:
        return NULL;
    }
}

extern "C" uint8_t ee_units_host_goal_kind(void* inGoal)
{
    return static_cast<EEUnitGoal*>(inGoal)->mKind;
}

extern "C" void ee_units_host_goal_release(void* inGoal)
{
    delete static_cast<EEUnitGoal*>(inGoal);
}

// a group as Rust holds it: its EEGroup base (the address group_ids reads)
static EEActiveGroup* ActiveOf(void* inGroup)
{
    return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(inGroup));
}

extern "C" void* ee_units_host_active_group_new(void* inPlayer)
{
    return static_cast<EEGroup*>(new EEActiveGroup(static_cast<EEPlayer*>(inPlayer)));
}

extern "C" void ee_units_host_group_fill(void* inGroup, void* inSelection, int32_t inSort, int32_t inSlot)
{
    ActiveOf(inGroup)->Fill(static_cast<EESelectedGroup*>(static_cast<EEGroup*>(inSelection)), inSort, inSlot);
}

extern "C" void ee_units_host_group_release(void* inGroup)
{
    ActiveOf(inGroup)->Dereference();
}

extern "C" void ee_units_host_pending_push(void* inPlayer, void* inGroup)
{
    static_cast<EEPlayer*>(inPlayer)->mPendingGroups.push_back(static_cast<EEGroup*>(inGroup));
}

extern "C" void ee_units_host_group_keep_mobile(void* inGroup)
{
    ActiveOf(inGroup)->KeepMobileUnits();
}

extern "C" void ee_units_host_group_clear_goal(void* inGroup)
{
    ActiveOf(inGroup)->ClearGoal();
}

extern "C" void ee_units_host_group_set_goal(void* inGroup, void* inGoal, uint8_t inQueued)
{
    ActiveOf(inGroup)->SetGoal(static_cast<EEUnitGoal*>(inGoal), inQueued != 0);
}

extern "C" void ee_units_host_group_set_action(void* inGroup, int32_t inAction)
{
    ActiveOf(inGroup)->mAction = inAction;
}

extern "C" void ee_units_host_group_plan(void* inGroup)
{
    ActiveOf(inGroup)->Plan();
}

extern "C" int32_t ee_units_host_random(int32_t inLow, int32_t inHigh)
{
    return (int32_t)gPhysics->GetRandom((long)inLow, (long)inHigh);
}

extern "C" void ee_units_host_play_acknowledgement(void* inPlayer, void* inGroup, int32_t inIndex)
{
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ActiveOf(inGroup)->GetUnit(inIndex));
}

extern "C" float ee_units_host_map_height(float inX, float inY)
{
    GE3DPoint theTarget;
    theTarget.SetX(inX);
    theTarget.SetY(inY);
    float theHeight = gWorld.GetMap()->GetHeight(theTarget);
    return theHeight;
}

extern "C" void ee_units_host_group_keep_ground_attackers(void* inGroup)
{
    ActiveOf(inGroup)->KeepGroundAttackers();
}

extern "C" void ee_units_host_send_group_command(void* inGroup, uint32_t inTarget, void* inPlayer, int32_t inCommand, int32_t inQueue)
{
    EEUnit* theTarget = inTarget != 0xffffffffu ? UnitOf(inTarget) : NULL;
    EESendGroupCommand(static_cast<EEGroup*>(inGroup), theTarget, static_cast<EEPlayer*>(inPlayer)->mPlayerID, inCommand, inQueue);
}

extern "C" void ee_units_host_acknowledge_unit(void* inPlayer, uint32_t inUnit)
{
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ComplexOf(inUnit));
}

// EEMPatrol's target -1: the unit's nearest town (FindNearestTown) and, when it has a patrol group, that group's point
// nearest to the unit's tile (GetNearest; NULL when it has none). Reads only.
extern "C" uint32_t ee_units_host_town_patrol_point(uint32_t inUnit, uint8_t* outFound)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EETown* theSite = theUnit->FindNearestTown();
    uint32_t thePoint = 0xffffffffu;
    *outFound = 0;
    if (theSite && theSite->mPatrolGroup) {
        *outFound = 1;
        EEComplexUnit* theNearest = static_cast<EEComplexUnit*>(ee_units_pg_nearest(theSite->mPatrolGroup, theUnit->GetTileX(), theUnit->GetTileY()));
        thePoint = theNearest != NULL ? static_cast<uint32_t>(theNearest->mUnitID) : 0xffffffffu;
    }
    return thePoint;
}

extern "C" void ee_units_host_patrol_add_unit_tile(uint32_t inUnit, uint32_t inTarget, void* inPlayer)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EEComplexUnit* theTarget = ComplexOf(inTarget);
    ee_units_pg_add_point(theUnit->mPatrolGroup, (float)theTarget->mTileX, (float)theTarget->mTileY,
                          (uint32_t)static_cast<EEPlayer*>(inPlayer)->mPlayerID);
}

extern "C" void* ee_units_host_unit_patrol_group(uint32_t inUnit)
{
    EEPPatrolGroup* theRoute = ComplexOf(inUnit)->mPatrolGroup;
    return theRoute;
}

extern "C" void ee_units_host_group_set_queued(void* inGroup, uint8_t inQueued)
{
    ActiveOf(inGroup)->mIsQueued = inQueued != 0;
}

extern "C" void ee_units_host_group_spread_around_resource(void* inGroup, uint32_t inTarget)
{
    ActiveOf(inGroup)->SpreadAroundResource(UnitOf(inTarget));
}

extern "C" void ee_units_host_group_spread_around_tile(void* inGroup, int64_t inX, int64_t inY)
{
    ActiveOf(inGroup)->SpreadAroundTile((long)inX, (long)inY);
}

extern "C" void ee_units_host_unit_add_build_target(uint32_t inUnit, uint32_t inTarget)
{
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    theUnit->AddBuildTarget(UnitOf(inTarget));
    // Port: no smart task after a shift-queued repair (EEUGBuildRepair::Step)
    theUnit->mHasShiftQueuedBuild = true;
}

extern "C" void ee_units_host_unit_order_repair(uint32_t inUnit, uint32_t inTarget)
{
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    theUnit->OrderRepair(UnitOf(inTarget));
    theUnit->StartAction(kActionIdle);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

extern "C" void ee_units_host_group_add_member(void* inGroup, uint32_t inUnit)
{
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    ActiveOf(inGroup)->AddMember(theUnit->mComplexUnit, true);
}

extern "C" void ee_units_host_group_keep_non_citizens(void* inGroup)
{
    ActiveOf(inGroup)->KeepNonCitizens();
}

// EEMAddPatrolPoint with nothing selected: a lone patrol group with the point, registered with the player
extern "C" void ee_units_host_patrol_new_lone(void* inPlayer, float inX, float inY)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    EEPPatrolGroup* thePatrolGroup = ee_units_pg_new();
    ee_units_pg_add_point(thePatrolGroup, inX, inY, (uint32_t)thePlayer->mPlayerID);
    thePlayer->AddPatrolGroup(thePatrolGroup);
    ee_units_pg_dereference(thePatrolGroup);
}

// a selected path point: the point into its route after it
extern "C" void ee_units_host_patrol_insert_point_at(uint32_t inUnit, float inX, float inY, void* inPlayer)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    ee_units_pg_insert_at(theUnit->mPatrolGroup, inX, inY, (uint32_t)static_cast<EEPlayer*>(inPlayer)->mPlayerID, theUnit);
}

// the route of the unit's current goal when it is a patrol (EEGoalCast<EEUGPatrol>(GetGoal())), else NULL; a read
extern "C" void* ee_units_host_unit_goal_patrol_route(uint32_t inUnit)
{
    EEUGRust* theGoal = EEPatrolGoalCast(ComplexOf(inUnit)->GetGoal());
    EEPPatrolGroup* theRoute = theGoal != NULL ? theGoal->GetPatrolGroup() : NULL;
    return theRoute;
}

extern "C" void ee_units_host_patrol_add_point(void* inRoute, float inX, float inY, void* inPlayer)
{
    ee_units_pg_add_point(static_cast<EEPPatrolGroup*>(inRoute), inX, inY, (uint32_t)static_cast<EEPlayer*>(inPlayer)->mPlayerID);
}

// a new patrol goal's route: the point added, the route registered with the player
extern "C" void ee_units_host_goal_patrol_add_point(void* inGoal, float inX, float inY, void* inPlayer)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    EEUGRust* theGoal = static_cast<EEUGRust*>(static_cast<EEUnitGoal*>(inGoal));
    ee_units_pg_add_point(theGoal->GetPatrolGroup(), inX, inY, (uint32_t)thePlayer->mPlayerID);
    thePlayer->AddPatrolGroup(theGoal->GetPatrolGroup());
}

extern "C" void ee_units_host_group_spread_around_unit(void* inGroup, uint32_t inTarget, int64_t inX, int64_t inY)
{
    ActiveOf(inGroup)->SpreadAroundUnit(UnitOf(inTarget), (long)inX, (long)inY);
}

// EEMPathTo's point (its statements before the loop, as they were): a point off the map goes to the closest tile's
// centre; a tile under a building the first selected unit cannot pass goes to the building's nearest footprint tile;
// the height is the final tile's (at least 0). *ioX, *ioY: the message's point in, the point the body uses out (the
// message keeps it). A read
extern "C" float ee_units_host_path_to_point(uint32_t inFirst, float* ioX, float* ioY)
{
    EEComplexUnit* theFirstUnit = ComplexOf(inFirst);
    float theInX = *ioX, theInY = *ioY;
    long theX = (long)*ioX;
    long theY = (long)*ioY;
    EETile* theTile = gWorld.GetMap()->GetTile(theX, theY);
    if (theTile == NULL) {
        GE3DPoint thePoint((float)theX + 0.5f, (float)theY + 0.5f, 0.0f);
        theTile = gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        *ioX = (float)theX + 0.5f;
        *ioY = (float)theY + 0.5f;
    }
    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        if (theNode->mUnit->mUnitClass->mTileSize > 1 && !theNode->mUnit->IsPassableBy(theFirstUnit->GetComplexUnit())) {
            theFirstUnit->GetNearestFootprintTile(theNode->mUnit, &theX, &theY);
            theTile = gWorld.GetMap()->GetTile(theX, theY);
            if (theTile != NULL) {
                *ioX = (float)theX + 0.5f;
                *ioY = (float)theY + 0.5f;
                break;
            }
            theX = (long)*ioX;
            theY = (long)*ioY;
            theTile = gWorld.GetMap()->GetTile(theX, theY);
        }
    }
    float theZ = theTile->mHeight >= 0.0f ? theTile->mHeight : 0.0f;
    return theZ;
}

extern "C" void ee_units_host_group_reset_paths(void* inGroup)
{
    ActiveOf(inGroup)->ResetUnitsPaths();
}

extern "C" void ee_units_host_group_keep_citizens(void* inGroup)
{
    ActiveOf(inGroup)->KeepCitizens();
}

// EEMBuildWall's plan: the wall of the technology's unit class from start to end as paid foundations (EEPlanWall(..,
// true, false, pieces)); the new pieces in a list the caller releases
extern "C" void* ee_units_host_wall_plan(void* inPlayer, int32_t inTechnology, float inStartX, float inStartY, float inEndX,
                                         float inEndY, uintptr_t* outCount)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    std::vector<EEUnit*>* thePieces = new std::vector<EEUnit*>();
    EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    EEPlanWall(GE2DPoint(inStartX, inStartY), GE2DPoint(inEndX, inEndY), theClass, thePlayer, 1, 0, *thePieces);
    *outCount = thePieces->size();
    return thePieces;
}

extern "C" void ee_units_host_wall_build(void* inGroup, void* inPieces)
{
    EEBuildWall(ActiveOf(inGroup), *static_cast<std::vector<EEUnit*>*>(inPieces));
}

extern "C" void ee_units_host_group_spread_for_unload(void* inGroup, int64_t inX, int64_t inY)
{
    ActiveOf(inGroup)->SpreadForUnload((long)inX, (long)inY);
}

// a transport (or paratrooper bomber) with cargo; a read
extern "C" uint8_t ee_units_host_unit_can_unload(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    bool theResult = (theUnit->GetUnitClass()->IsTransport() || theUnit->GetUnitClass()->mIsParatrooperBomber)
        && theUnit->mComplexUnit->GetNumCargoUnits() > 0;
    return theResult ? 1 : 0;
}

extern "C" void ee_units_host_goal_set_transport(void* inGoal, uint32_t inUnit)
{
    static_cast<EEUGRust*>(static_cast<EEUnitGoal*>(inGoal))->SetUnloadTransport(ComplexOf(inUnit)->mComplexUnit);
}

extern "C" uint8_t ee_units_host_goal_can_unload(void* inGoal)
{
    return static_cast<EEUGRust*>(static_cast<EEUnitGoal*>(inGoal))->CanUnloadAtDestination() ? 1 : 0;
}

extern "C" void ee_units_host_unload_warning(void* inPlayer, int64_t inX, int64_t inY)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    U2DPoint<long> theTile((long)inX, (long)inY);
    thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, 0x7c3b, false);
}

extern "C" void ee_units_host_wall_release(void* inPieces)
{
    delete static_cast<std::vector<EEUnit*>*>(inPieces);
}

// ---- EEMBuildUnit, EEMSmartFarmers, EEMCastCalamity (ee_units commands.rs) ----

// the technology's node of the player's tree (NULL: none)
static EETechTreeNode* NodeOf(void* inPlayer, int32_t inTechnology)
{
    return static_cast<EEPlayer*>(inPlayer)->mTechnologies->Get(inTechnology);
}

// a read
extern "C" int64_t ee_units_host_technology_unit_class(void* inPlayer, int32_t inTechnology, uint8_t* outFound)
{
    EETechTreeNode* theNode = NodeOf(inPlayer, inTechnology);
    *outFound = theNode != NULL ? 1 : 0;
    int64_t theClass = theNode != NULL ? (int64_t)theNode->mData->mUnitClass : -1;
    return theClass;
}

extern "C" uint8_t ee_units_host_can_produce(void* inPlayer, int32_t inTechnology)
{
    return static_cast<EEPlayer*>(inPlayer)->CanProduce(inTechnology, 0x20, 1, true, 3, NULL) ? 1 : 0;
}

extern "C" uint8_t ee_units_host_can_place_on_tile(void* inPlayer, int64_t inUnitType, int64_t inX, int64_t inY)
{
    bool theResult = EECanPlaceOnTile(static_cast<EEPlayer*>(inPlayer), (long)inUnitType, (long)inX, (long)inY, 0, 1);
    return theResult ? 1 : 0;
}

extern "C" uint32_t ee_units_host_make_building(void* inPlayer, int64_t inUnitType, int64_t inX, int64_t inY)
{
    EEUnit* theBuilding = EEMakeBuilding(static_cast<EEPlayer*>(inPlayer), (long)inUnitType, (long)inX, (long)inY);
    return theBuilding != NULL ? static_cast<uint32_t>(theBuilding->mUnitID) : 0xffffffffu;
}

// the last list a two-call read computed (its first call, capacity 0); the second call copies it
static std::vector<uint32_t> sHostList;

static size_t CopyList(uint32_t* outIDs, size_t inCapacity)
{
    for (size_t i = 0; i < sHostList.size() && i < inCapacity; i++)
        outIDs[i] = sHostList[i];
    return sHostList.size();
}

// EEFindFreeFarm from ring 1 until it finds none (the bodies' loops); a read (none for a unit no id resolves)
extern "C" size_t ee_units_host_free_farms(uint32_t inUnit, uint32_t* outIDs, size_t inCapacity)
{
    if (inCapacity > 0)
        return CopyList(outIDs, inCapacity);
    sHostList.clear();
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    if (theUnit != NULL) {
        long theRing = 1;
        EEComplexUnit* theFarm;
        do {
            theFarm = EEFindFreeFarm(theUnit, 1, &theRing);
            if (theFarm != NULL)
                sHostList.push_back(static_cast<uint32_t>(theFarm->mUnitID));
        } while (theFarm != NULL);
    }
    return sHostList.size();
}

// the bodies' std::sorts over the farms (EESortFarms, EEMessage.cpp: their comparators' unit); a permutation (none
// when a farm is not known: then the order stays)
extern "C" void ee_units_host_sort_farms(uint32_t* ioIDs, size_t inCount, int64_t inX, int64_t inY, uint8_t inGreater)
{
    std::vector<EEComplexUnit*> theFarms;
    for (size_t i = 0; i < inCount; i++) {
        EEComplexUnit* theFarm = ComplexOf(ioIDs[i]);
        if (theFarm == NULL)
            break;
        theFarms.push_back(theFarm);
    }
    if (theFarms.size() == inCount) {
        EESortFarms(theFarms, (long)inX, (long)inY, inGreater != 0);
        for (size_t i = 0; i < inCount; i++)
            ioIDs[i] = static_cast<uint32_t>(theFarms[i]->mUnitID);
    }
}

extern "C" void ee_units_host_unit_add_farm_target(uint32_t inUnit, uint32_t inTarget)
{
    ComplexOf(inUnit)->AddBuildTarget(UnitOf(inTarget));
}

extern "C" void ee_units_host_unit_order_build(uint32_t inUnit, uint32_t inTarget)
{
    ComplexOf(inUnit)->OrderRepair(UnitOf(inTarget));
}

extern "C" void ee_units_host_unit_start_action(uint32_t inUnit, int32_t inAction)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    theUnit->StartAction(inAction);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

// a read
extern "C" uint32_t ee_units_host_unit_load_farm(uint32_t inUnit)
{
    uint32_t theFarm = 0xffffffffu;
    EEUGRust* theGather = EEGatherGoalCast(ComplexOf(inUnit)->GetLoadGoal());
    if (theGather != NULL && theGather->GetGatherResource() != NULL && theGather->GetGatherResource()->mUnitClass->mUnitType == kUnitTypeFarm) {
        EEComplexUnit* theComplex = theGather->GetGatherResource()->mComplexUnit;
        if (theComplex != NULL)
            theFarm = static_cast<uint32_t>(theComplex->mUnitID);
    }
    return theFarm;
}

// EEGetFarmDropSites; a read
extern "C" size_t ee_units_host_farm_drop_sites(uint32_t inFarm, uint32_t* outIDs, size_t inCapacity, uint8_t* outFound)
{
    static uint8_t sFound = 0;
    if (inCapacity > 0) {
        *outFound = sFound;
        return CopyList(outIDs, inCapacity);
    }
    std::vector<EEUnit*> theSites;
    sFound = EEGetFarmDropSites(ComplexOf(inFarm), theSites) ? 1 : 0;
    sHostList.clear();
    for (size_t i = 0; i < theSites.size(); i++)
        sHostList.push_back(static_cast<uint32_t>(theSites[i]->mUnitID));
    *outFound = sFound;
    return sHostList.size();
}

// a read
extern "C" uint8_t ee_units_host_can_take(uint32_t inTarget, uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    bool theResult = ComplexOf(inTarget)->CanTake(theUnit, theUnit->mResourceType);
    return theResult ? 1 : 0;
}

extern "C" void ee_units_host_unit_drop_gathered(uint32_t inUnit)
{
    ComplexOf(inUnit)->SetGatheredResource(NULL);
}

extern "C" void ee_units_host_work_farm(uint32_t inFarm, uint32_t inUnit)
{
    EEWorkFarm(ComplexOf(inFarm), ComplexOf(inUnit));
}

extern "C" void ee_units_host_unit_set_goal(uint32_t inUnit, void* inGoal)
{
    ComplexOf(inUnit)->SetGoal(static_cast<EEUnitGoal*>(inGoal), 0, 0);
}

extern "C" void ee_units_host_group_finish(void* inGroup)
{
    ActiveOf(inGroup)->Finish();
}

// the technology's calamity class (EEMCastCalamity: the node, then the class of its calamity); a read
extern "C" void* ee_units_host_calamity_of(void* inPlayer, int32_t inTechnology, int32_t* outClassEnum, uint8_t* outCastByAll)
{
    EETechTreeNode* theNode = NodeOf(inPlayer, inTechnology);
    EECalamityClass* theClass = NULL;
    if (theNode != NULL)
        theClass = EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
    *outClassEnum = theClass != NULL ? (int32_t)theClass->mClassEnum : 0;
    *outCastByAll = theClass != NULL && theClass->mIsCastByAllSelected ? 1 : 0;
    return theClass;
}

// CanCastAt with the check (the location is its out-value, unused)
extern "C" int64_t ee_units_host_calamity_can_cast_at(void* inClass, uint32_t inUnit, int32_t inX, int32_t inY, uint32_t inTarget)
{
    GE3DPoint theLocation;
    EEUnit* theTarget = inTarget != 0xffffffffu ? UnitOf(inTarget) : NULL;
    return (int64_t)static_cast<EECalamityClass*>(inClass)->CanCastAt(ComplexOf(inUnit), theLocation, (unsigned long)inX,
                                                                      (unsigned long)inY, true, theTarget);
}

// a read
extern "C" uint8_t ee_units_host_technology_contains(void* inPlayer, int32_t inTechnology, int64_t inUnitType)
{
    bool theResult = NodeOf(inPlayer, inTechnology)->Contains((long)inUnitType);
    return theResult ? 1 : 0;
}

// a read
extern "C" int64_t ee_units_host_calamity_cost(void* inClass)
{
    int64_t theCost = (int64_t)static_cast<EECalamityClass*>(inClass)->GetCost();
    return theCost;
}

// a read
extern "C" void* ee_units_host_unit_player(uint32_t inUnit)
{
    EEPlayer* thePlayer = ComplexOf(inUnit)->mPlayer;
    return thePlayer;
}

// SplitCalamityCasters
extern "C" void ee_units_host_group_split_calamity_casters(void* inGroup, void* inClass, void* inPlayer, int32_t inTechnology,
                                                           uint32_t inTarget)
{
    EEUnit* theTarget = inTarget != 0xffffffffu ? UnitOf(inTarget) : NULL;
    ActiveOf(inGroup)->SplitCalamityCasters(static_cast<EECalamityClass*>(inClass), NodeOf(inPlayer, inTechnology), theTarget);
}

extern "C" void* ee_units_host_goal_clone(void* inGoal)
{
    return static_cast<EEUnitGoal*>(inGoal)->Clone();
}

extern "C" void ee_units_host_group_remove_unit(void* inGroup, uint32_t inUnit)
{
    ActiveOf(inGroup)->RemoveUnit(UnitOf(inUnit)->GetComplexUnit());
}

extern "C" void ee_units_host_unit_clear_goals(uint32_t inUnit)
{
    ComplexOf(inUnit)->ClearGoals(0);
}

// ---- EEMSetAirportCommand's and EEMSetRallyGoal's calls (rally.rs) ----

extern "C" void* ee_units_host_player_selection(void* inPlayer)
{
    return static_cast<EEGroup*>(static_cast<EEPlayer*>(inPlayer)->mActiveGroup);
}

// the reads below are made on the objects
extern "C" uint8_t ee_units_host_unit_seen_by(uint32_t inUnit, void* inPlayer)
{
    return EEIsSeenBy(UnitOf(inUnit), static_cast<EEPlayer*>(inPlayer), true) ? 1 : 0;
}

extern "C" uint8_t ee_units_host_unit_ghost_seen(uint32_t inUnit)
{
    EEUnit* theUnit = UnitOf(inUnit);
    // a unit off the tiles (inside a carrier, a transport or a garrison) has no doppelganger: the original read its
    // NULL tile (EEMSetRallyGoal at such a unit crashed; the USGC sweep met one in a carrier scenario)
    EETile* theTile = theUnit != NULL ? static_cast<EETile*>(theUnit->mTile) : NULL;
    if (theTile == NULL)
        return 0;
    for (unsigned long i = 0; i < theTile->GetNumDoppelgangers(); i++)
        if (theTile->mDoppelgangers[i]->mUnit == theUnit)
            return 1;
    return 0;
}

extern "C" uint8_t ee_units_host_unit_attackable(uint32_t inUnit, int64_t inDamageIndex)
{
    EEUnit* theUnit = UnitOf(inUnit);
    unsigned long theClass = theUnit->mUnitClass->mFamily;
    return theUnit->mUnitClass->CanBeAttacked() && EEDamageFactor(theClass, (long)inDamageIndex) > 0.0f ? 1 : 0;
}

extern "C" uint8_t ee_units_host_unit_can_attack_class(uint32_t inUnit, uint32_t inTarget)
{
    return ComplexOf(inUnit)->CanAttackClass(UnitOf(inTarget)->mUnitClass) ? 1 : 0;
}

extern "C" uint8_t ee_units_host_unit_shares_rally_point(uint32_t inUnit, int64_t inUnitType)
{
    return ComplexOf(inUnit)->mUnitClass->SharesRallyPoint((unsigned long)inUnitType) ? 1 : 0;
}

extern "C" int64_t ee_units_host_unit_num_goal_queues(uint32_t inUnit)
{
    return (int64_t)ComplexOf(inUnit)->GetNumGoalQueues();
}

extern "C" void ee_units_host_unit_clear_goal_queue(uint32_t inUnit, int64_t inQueue)
{
    ComplexOf(inUnit)->ClearGoalQueue((unsigned long)inQueue);
}

extern "C" void ee_units_host_unit_clear_all_goal_queues(uint32_t inUnit)
{
    ComplexOf(inUnit)->ClearAllGoalQueues();
}

extern "C" void ee_units_host_unit_queue_goal(uint32_t inUnit, void* inGoal, int64_t inQueue)
{
    ComplexOf(inUnit)->QueueGoal(static_cast<EEUnitGoal*>(inGoal), (long)inQueue);
}

extern "C" void ee_units_host_unit_queue_timer(uint32_t inUnit, int64_t inQueue, int64_t inMs)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    theUnit->mGoalQueues[inQueue].ResetTimer();
    theUnit->mGoalQueues[inQueue].AddTime((unsigned long)inMs);
}

extern "C" void* ee_units_host_unit_goal_clone(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    return theUnit->mGoalState->Clone();
}

extern "C" void ee_units_host_unit_queue_clear(uint32_t inUnit, int64_t inQueue)
{
    ComplexOf(inUnit)->mGoalQueues[inQueue].Clear();
}

extern "C" void ee_units_host_unit_clear_air_traffic(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EEUnitClass* theClass = theUnit->GetUnitClass();
    ee_units_atc_clear_airport_queue(theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1], theUnit, 0);
}

extern "C" void ee_units_host_unit_refresh_goals(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    theUnit->RefreshGoals();
    theUnit->RefreshActiveGoal();
}

extern "C" void ee_units_host_unit_clear_build_targets(uint32_t inUnit)
{
    ComplexOf(inUnit)->ClearBuildTargets();
}

extern "C" void ee_units_host_unit_action_changed(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

extern "C" void ee_units_host_unit_halt(uint32_t inUnit)
{
    ComplexOf(inUnit)->Halt();
}

// EEMSetRallyGoal's garrison (EEMessage.cpp's block, unchanged): a new load goal the garrison takes when it is valid
extern "C" void* ee_units_host_garrison_rally_load(uint32_t inGarrison)
{
    EEComplexUnit* theGarrison = ComplexOf(inGarrison);
    std::vector<EEComplexUnit*> theGarrisonUnits;
    EEUGRust* theLoad = EEUGRust::NewLoad(kGoalLoadGarrison, theGarrison, theGarrisonUnits, true);
    if (!theLoad->IsLoadGarrisonValid()) {
        delete theLoad;
        return NULL;
    }
    theGarrison->SetGoal(theLoad, 0, 0);
    if (theGarrison->mStats->mMaxVelocity > 0.0f)
        theGarrison->StartAction(kActionPrepareToMove);
    else
        theGarrison->StartAction(kActionPickedUpLoadableUnits);
    theGarrison->mUnitClass->ActionChanged(theGarrison);
    return static_cast<EEUnitGoal*>(theLoad->Clone());
}

// ---- EEMLoadGarrison's and EEMLoadTransport's calls (boarding.rs) ----

// a read
extern "C" uint8_t ee_units_host_unit_load_traits(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    return (theUnit->mUnitClass->mCanGarrison ? EE_UNITS_LOAD_CAN_GARRISON : 0) | (theUnit->mUnitClass->IsTransport() ? EE_UNITS_LOAD_TRANSPORT : 0)
        | (theUnit->mStats->mMaxVelocity > 0.0f ? EE_UNITS_LOAD_MOVES : 0);
}

// a garrison's mGoalState, a transport's GetLoadGoal(), when it is of the kind
extern "C" void* ee_units_host_unit_load_goal(uint32_t inUnit, uint8_t inKind)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EEUnitGoal* theGoal = inKind == kGoalLoadGarrison ? theUnit->mGoalState : theUnit->GetLoadGoal();
    if (theGoal != NULL && theGoal->mKind != inKind)
        theGoal = NULL;
    return theGoal;
}

static std::vector<EEComplexUnit*> ComplexUnits(const uint32_t* inIDs, size_t inCount)
{
    std::vector<EEComplexUnit*> theUnits;
    for (size_t i = 0; i < inCount; i++)
        theUnits.push_back(ComplexOf(inIDs[i]));
    return theUnits;
}

extern "C" void ee_units_host_load_goal_add_units(void* inGoal, const uint32_t* inIDs, size_t inCount)
{
    static_cast<EEUGRust*>(static_cast<EEUnitGoal*>(inGoal))->AddLoadUnits(ComplexUnits(inIDs, inCount));
}

// EEUGRust::NewLoad (the ids come sorted, none twice): the goal is the carrier's once it takes it
extern "C" void* ee_units_host_load_goal_new(uint8_t inKind, uint32_t inCarrier, const uint32_t* inIDs, size_t inCount, uint8_t inFlag)
{
    std::vector<EEComplexUnit*> theUnits = ComplexUnits(inIDs, inCount);
    return static_cast<EEUnitGoal*>(EEUGRust::NewLoad(inKind, ComplexOf(inCarrier), theUnits, inFlag != 0));
}

// IsLoadGarrisonValid, GetLoadUnitTile
extern "C" uint8_t ee_units_host_load_goal_fields(void* inGoal, int64_t* outX, int64_t* outY)
{
    EEUGRust* theGoal = static_cast<EEUGRust*>(static_cast<EEUnitGoal*>(inGoal));
    U2DPoint<long> theTile = theGoal->GetLoadUnitTile();
    *outX = theTile.GetX();
    *outY = theTile.GetY();
    return theGoal->IsLoadGarrisonValid() ? 1 : 0;
}

extern "C" void ee_units_host_player_event(void* inPlayer, int64_t inX, int64_t inY, int32_t inCode)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    U2DPoint<long> theTile((long)inX, (long)inY);
    thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, inCode, false);
}

extern "C" void ee_units_host_group_add_unit(void* inGroup, uint32_t inUnit)
{
    ActiveOf(inGroup)->AddMember(ComplexOf(inUnit), true);
}

// EEUCTransport::FilterLoadable (a read)
extern "C" size_t ee_units_host_transport_filter(uint32_t inTransport, uint32_t* ioIDs, size_t inCount, uint32_t* outRejected)
{
    EEComplexUnit* theTransport = ComplexOf(inTransport);
    std::vector<EEComplexUnit*> theUnits = ComplexUnits(ioIDs, inCount);
    std::vector<EEComplexUnit*> theRejected;
    EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theTransport->mUnitClass)->FilterLoadable(&theUnits, theTransport, &theRejected);
    for (size_t i = 0; i < theUnits.size(); i++)
        ioIDs[i] = static_cast<uint32_t>(theUnits[i]->mUnitID);
    for (size_t i = 0; i < theRejected.size(); i++)
        outRejected[i] = static_cast<uint32_t>(theRejected[i]->mUnitID);
    return theUnits.size();
}

// ---- EEMResearchTechnology's calls (research.rs) ----

// the node's reads: its kind; found, a hero's technology, the Bundeswehr power's, makes a unit,
// an epoch
extern "C" uint8_t ee_units_host_research_node(void* inPlayer, int32_t inTechnology, int32_t* outKind)
{
    EETechTreeNode* theNode = static_cast<EEPlayer*>(inPlayer)->GetTechTree()->Get(inTechnology);
    *outKind = 0;
    if (theNode == NULL)
        return 0;
    EECivPower thePower;
    uint8_t theBits = EE_UNITS_RESEARCH_FOUND;
    *outKind = theNode->mData->mKind;
    if (theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass]
        && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero)
        theBits |= EE_UNITS_RESEARCH_HERO;
    if (gCivPowersManager.GetPower(kCivPowerBundeswehr, thePower) && thePower.mTechnology == theNode->mData->mPrerequisite1)
        theBits |= EE_UNITS_RESEARCH_BUNDESWEHR;
    if (theNode->mData->mMakesUnit)
        theBits |= EE_UNITS_RESEARCH_MAKES_UNIT;
    if (EEIsEpochID(inTechnology))
        theBits |= EE_UNITS_RESEARCH_EPOCH;
    return theBits;
}

extern "C" uint8_t ee_units_host_scenario_game()
{
    return gWorld.mGameOptions.mIsScenarioGame ? 1 : 0;
}

extern "C" uint8_t ee_units_host_unit_shares_research(uint32_t inUnit, int64_t inUnitType)
{
    return ComplexOf(inUnit)->mUnitClass->SharesResearch((long)inUnitType) ? 1 : 0;
}

extern "C" int64_t ee_units_host_unit_research_state(uint32_t inUnit, int64_t* outCurrent, uint8_t* outDisabled)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    *outCurrent = theUnit->mCurrentResearch;
    *outDisabled = theUnit->mIsDisabled ? 1 : 0;
    return (int64_t)theUnit->GetBuildQueueSize();
}

extern "C" void ee_units_host_unit_queue_research(uint32_t inUnit, int32_t inTechnology, uint8_t inFront)
{
    ComplexOf(inUnit)->QueueResearch(inTechnology, inFront != 0);
}

// ---- EEMHackANewUnit's calls (hack.rs) ----

extern "C" float ee_units_host_player_class_scale(int64_t inPlayerID, int32_t inUnitType)
{
    return gWorld.mPlayers[inPlayerID]->mUnitClassUpgrades[gUnitClassTable[inUnitType]->mUnitType]->mScale;
}

// the cheat's unit (EEMessage.cpp's statements, unchanged): at the point's ground facing pi, its reload cleared and
// its power full; the creation sound for the current player
extern "C" void ee_units_host_hack_create_unit(int32_t inUnitType, float inX, float inY, int64_t inPlayerID)
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    GE3DPoint theGround;
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayerID];
    thePoint.SetX(inX);
    thePoint.SetY(inY);
    gWorld.GetMap()->GetGroundPoint(thePoint, theGround);
    theTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
    theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
    EEUnit* theUnit = theClass->CreateUnitAt(theTransformation, (long)inPlayerID, false, NULL);
    if (theUnit->mTileNode != NULL) {
        theUnit->mComplexUnit->SetReloadTimer(0);
        theUnit->mComplexUnit->SetPower(theUnit->mComplexUnit->mStats->mPower);
    }
    EESoundObject* theSound = thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mCreationSound;
    if (inPlayerID == gWorld.GetCurrentPlayer()->mPlayerID && theSound != NULL)
        gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
}

// the cheat's building (EEMessage.cpp's statements, unchanged): at its lower left corner, built; its creation sound
// for the current player
extern "C" void ee_units_host_hack_create_building(int32_t inUnitType, int64_t inX, int64_t inY, int64_t inPlayerID)
{
    EEUnit* theUnit = gUnitClassTable[inUnitType]->CreateUnit((long)inX, (long)inY, (long)inPlayerID, false, NULL);
    if (theUnit->mTileNode != NULL)
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
    if (inPlayerID == gWorld.GetCurrentPlayer()->mPlayerID) {
        EESoundObject* theSound = theUnit->mStats->mCreationSound;
        if (theSound != NULL)
            gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
    }
}
