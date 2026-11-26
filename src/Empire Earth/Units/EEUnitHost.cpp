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
#include "ee/EECombatGoals.h"

static EEUnit* UnitOf(uint32_t inID) { return EEUnitHandleResolved<EEUnit>(inID); }
static EEComplexUnit* ComplexOf(uint32_t inID) { return EEUnitHandleResolved<EEComplexUnit>(inID); }

// ---- the host's calls ----

// a group as Rust holds it: its EEGroup base (the address group_ids reads)
static EEActiveGroup* ActiveOf(void* inGroup)
{
    return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(inGroup));
}

extern "C" int32_t ee_units_host_random(int32_t inLow, int32_t inHigh)
{
    return (int32_t)gPhysics->GetRandom((long)inLow, (long)inHigh);
}

extern "C" void ee_units_host_play_acknowledgement(void* inPlayer, void* inGroup, int32_t inIndex)
{
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ActiveOf(inGroup)->GetUnit(inIndex));
}

extern "C" void ee_units_host_send_group_command(void* inGroup, uint32_t inTarget, void* inPlayer, int32_t inCommand, int32_t inQueue)
{
    EESendGroupCommand(static_cast<EEGroup*>(inGroup), inTarget != 0xffffffffu ? UnitOf(inTarget) : NULL, static_cast<EEPlayer*>(inPlayer)->mPlayerID, inCommand, inQueue);
}

extern "C" void ee_units_host_acknowledge_unit(void* inPlayer, uint32_t inUnit)
{
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ComplexOf(inUnit));
}

extern "C" void ee_units_host_wall_build(void* inGroup, void* inPieces)
{
    ee_units_uu_build_wall((uint64_t)(uintptr_t)ActiveOf(inGroup), inPieces);   // uu_world.rs build_wall
}

extern "C" void ee_units_host_unload_warning(void* inPlayer, int64_t inX, int64_t inY)
{
    static_cast<EEPlayer*>(inPlayer)->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, static_cast<EEPlayer*>(inPlayer), 0x7c3b, false);
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

extern "C" uint8_t ee_units_host_can_produce(void* inPlayer, int32_t inTechnology)
{
    return static_cast<EEPlayer*>(inPlayer)->CanProduce(inTechnology, 0x20, 1, true, 3, NULL) ? 1 : 0;
}

extern "C" void ee_units_host_unit_add_farm_target(uint32_t inUnit, uint32_t inTarget)
{
    ComplexOf(inUnit)->AddBuildTarget(UnitOf(inTarget));
}

extern "C" void ee_units_host_unit_order_build(uint32_t inUnit, uint32_t inTarget)
{
    ComplexOf(inUnit)->OrderRepair(UnitOf(inTarget));
}

extern "C" void ee_units_host_unit_drop_gathered(uint32_t inUnit)
{
    ComplexOf(inUnit)->SetGatheredResource(NULL);
}

extern "C" void ee_units_host_work_farm(uint32_t inFarm, uint32_t inUnit)
{
    EEWorkFarm(ComplexOf(inFarm), ComplexOf(inUnit));
}

// a read
extern "C" uint8_t ee_units_host_technology_contains(void* inPlayer, int32_t inTechnology, int64_t inUnitType)
{
    return NodeOf(inPlayer, inTechnology)->Contains((long)inUnitType) ? 1 : 0;
}

// a read
extern "C" int64_t ee_units_host_calamity_cost(void* inClass)
{
    return (int64_t)static_cast<EECalamityClass*>(inClass)->GetCost();
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
    return EECombatIsSeenBy(UnitOf(inUnit), static_cast<EEPlayer*>(inPlayer), true) ? 1 : 0;
}

extern "C" uint8_t ee_units_host_unit_can_attack_class(uint32_t inUnit, uint32_t inTarget)
{
    return ComplexOf(inUnit)->CanAttackClass(UnitOf(inTarget)->mUnitClass) ? 1 : 0;
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

extern "C" void ee_units_host_unit_clear_build_targets(uint32_t inUnit)
{
    ComplexOf(inUnit)->ClearBuildTargets();
}

extern "C" void ee_units_host_unit_action_changed(uint32_t inUnit)
{
    ComplexOf(inUnit)->mUnitClass->ActionChanged(ComplexOf(inUnit));
}

extern "C" void ee_units_host_unit_halt(uint32_t inUnit)
{
    ComplexOf(inUnit)->Halt();
}

extern "C" void ee_units_host_player_event(void* inPlayer, int64_t inX, int64_t inY, int32_t inCode)
{
    static_cast<EEPlayer*>(inPlayer)->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, static_cast<EEPlayer*>(inPlayer), inCode, false);
}

extern "C" uint8_t ee_units_host_scenario_game()
{
    return gWorld.mGameOptions.mIsScenarioGame ? 1 : 0;
}

extern "C" void ee_units_host_unit_queue_research(uint32_t inUnit, int32_t inTechnology, uint8_t inFront)
{
    ComplexOf(inUnit)->QueueResearch(inTechnology, inFront != 0);
}

extern "C" float ee_units_host_player_class_scale(int64_t inPlayerID, int32_t inUnitType)
{
    return gWorld.mPlayers[inPlayerID]->mUnitClassUpgrades[gUnitClassTable[inUnitType]->mUnitType]->mScale;
}

// ---- the bodies' statements (the units' lane, round 8: ee_units uh_body.rs, the bodies of the calls above that
// were more than one statement; the shared locals below stand for their locals, each set or cleared where it was
// declared)
static GE3DPoint sUhPoint;
static GE3DPoint sUhGround;
static long sUhXY[2];
static long sUhRing;
static U2DPoint<long> sUhTile;
static std::vector<EEComplexUnit*> sUhUnits;
static std::vector<EEComplexUnit*> sUhRejected;
static EECivPower sUhPower;
static GETransformation sUhTransformation;

template <class T> static T* UhObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

static uint32_t UhID(uint64_t inUnit) { return (uint32_t)inUnit; }

static RTSTileUnitNode* UhNode(uint64_t inNode) { return UhObject<RTSTileUnitNode>(inNode); }

static EEUGRust* UhGoal(uint64_t inGoal) { return static_cast<EEUGRust*>(UhObject<EEUnitGoal>(inGoal)); }

extern "C" int64_t ee_units_uh_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float* f)
{
    switch (inOp) {
    case EE_UNITS_UH_MAP_HEIGHT: f[0] = gWorld.GetMap()->GetHeight(GE3DPoint(f[0], f[1], 0.0f)); return 0;
    case EE_UNITS_UH_NEAREST_TOWN: return EEObj(ComplexOf(UhID(inA))->FindNearestTown());
    case EE_UNITS_UH_TOWN_PATROL_GROUP: return EEObj(UhObject<EETown>(inB)->mPatrolGroup);
    case EE_UNITS_UH_ADD_BUILD_TARGET: UnitOf(UhID(inA))->mComplexUnit->AddBuildTarget(UnitOf(UhID(inB))); return 0;
    case EE_UNITS_UH_ORDER_REPAIR: UnitOf(UhID(inA))->mComplexUnit->OrderRepair(UnitOf(UhID(inB))); return 0;
    case EE_UNITS_UH_START_ACTION: UnitOf(UhID(inA))->mComplexUnit->StartAction((long)inC); return 0;
    case EE_UNITS_UH_ACTION_CHANGED: UnitOf(UhID(inA))->mComplexUnit->mUnitClass->ActionChanged(UnitOf(UhID(inA))->mComplexUnit); return 0;
    case EE_UNITS_UH_C_START_ACTION: ComplexOf(UhID(inA))->StartAction((long)inC); return 0;
    case EE_UNITS_UH_C_ACTION_CHANGED: ComplexOf(UhID(inA))->mUnitClass->ActionChanged(ComplexOf(UhID(inA))); return 0;
    case EE_UNITS_UH_ADD_PATROL_GROUP: UhObject<EEPlayer>(inA)->AddPatrolGroup(UhObject<EEPPatrolGroup>(inB)); return 0;
    case EE_UNITS_UH_PATROL_GOAL: return EEObj(static_cast<EEUnitGoal*>(EEPatrolGoalCast(ComplexOf(UhID(inA))->GetGoal())));
    case EE_UNITS_UH_GOAL_PATROL_GROUP: return EEObj(UhGoal(inB)->GetPatrolGroup());
    case EE_UNITS_UH_MAP_TILE: return EEObj(gWorld.GetMap()->GetTile((long)inC, (long)inD));
    case EE_UNITS_UH_CLOSEST_TILE: return EEObj(gWorld.GetMap()->GetClosestTile(&(sUhPoint = GE3DPoint(f[0], f[1], 0.0f)), &(sUhXY[0] = (long)inC), &(sUhXY[1] = (long)inD)));
    case EE_UNITS_UH_X: return sUhXY[0];
    case EE_UNITS_UH_Y: return sUhXY[1];
    case EE_UNITS_UH_TILE_UNITS: return EEObj(UhObject<EETile>(inB)->mUnits);
    case EE_UNITS_UH_NODE_NEXT: return EEObj(UhNode(inB)->mNext);
    case EE_UNITS_UH_NODE_BLOCKS: return UhNode(inB)->mUnit->mUnitClass->mTileSize > 1 && !UhNode(inB)->mUnit->IsPassableBy(ComplexOf(UhID(inA))->GetComplexUnit());
    case EE_UNITS_UH_NEAREST_FOOTPRINT: ComplexOf(UhID(inA))->GetNearestFootprintTile(UhNode(inB)->mUnit, &(sUhXY[0] = (long)inC), &(sUhXY[1] = (long)inD)); return 0;
    case EE_UNITS_UH_TILE_HEIGHT: f[0] = UhObject<EETile>(inB)->mHeight; return 0;
    case EE_UNITS_UH_NEW_PIECES: return EEObj(new std::vector<EEUnit*>());
    case EE_UNITS_UH_TECH_CLASS: return EEObj(gUnitClassTable[UhObject<EEPlayer>(inA)->mTechnologies->Get((long)inC)->mData->mUnitClass]);
    case EE_UNITS_UH_PIECES_SIZE: return (int64_t)UhObject<std::vector<EEUnit*> >(inB)->size();
    case EE_UNITS_UH_PIECE_ID: return (uint32_t)(*UhObject<std::vector<EEUnit*> >(inB))[(size_t)inC]->mUnitID;
    case EE_UNITS_UH_DELETE_PIECES: delete UhObject<std::vector<EEUnit*> >(inB); return 0;
    case EE_UNITS_UH_MAKE_BUILDING: return EEObj(EEMakeBuilding(UhObject<EEPlayer>(inA), (long)(int64_t)inB, (long)inC, (long)inD));
    case EE_UNITS_UH_RESET_RING: sUhRing = 1; return 0;
    case EE_UNITS_UH_FIND_FREE_FARM: return EEObj(EEFindFreeFarm(UhObject<EEComplexUnit>(inB), 1, &sUhRing));
    case EE_UNITS_UH_UNITS_CLEAR: sUhUnits.clear(); return 0;
    case EE_UNITS_UH_UNITS_PUSH: sUhUnits.push_back(ComplexOf(UhID(inA))); return 0;
    case EE_UNITS_UH_UNITS_SIZE: return (int64_t)sUhUnits.size();
    case EE_UNITS_UH_UNITS_ID: return (uint32_t)sUhUnits[(size_t)inC]->mUnitID;
    case EE_UNITS_UH_SORT_FARMS: EESortFarms(sUhUnits, (long)inC, (long)inD, inB != 0); return 0;
    case EE_UNITS_UH_LOAD_GATHER_GOAL: return EEObj(static_cast<EEUnitGoal*>(EEGatherGoalCast(ComplexOf(UhID(inA))->GetLoadGoal())));
    case EE_UNITS_UH_GATHER_RESOURCE: return EEObj(UhGoal(inB)->GetGatherResource());
    case EE_UNITS_UH_CAN_TAKE: return ComplexOf(UhID(inA))->CanTake(ComplexOf(UhID(inB)), ComplexOf(UhID(inB))->mResourceType);
    case EE_UNITS_UH_TECH_NODE: return EEObj(NodeOf(UhObject<void>(inA), (int32_t)inC));
    case EE_UNITS_UH_NODE_CALAMITY_CLASS: return EEObj(EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)UhObject<EETechTreeNode>(inB)->mData->mCalamity));
    case EE_UNITS_UH_CLASS_ENUM: return (int32_t)UhObject<EECalamityClass>(inB)->mClassEnum;
    case EE_UNITS_UH_CAST_BY_ALL: return UhObject<EECalamityClass>(inB)->mIsCastByAllSelected;
    case EE_UNITS_UH_CAN_CAST_AT: return (int64_t)UhObject<EECalamityClass>(inB)->CanCastAt(ComplexOf(UhID(inA)), sUhPoint, (unsigned long)(int32_t)inC, (unsigned long)(int32_t)(inC >> 32), true, (uint32_t)inD != 0xffffffffu ? UnitOf((uint32_t)inD) : NULL);
    case EE_UNITS_UH_RESET_POINT: sUhPoint = GE3DPoint(); return 0;
    case EE_UNITS_UH_SPLIT_CASTERS: ActiveOf(UhObject<void>(inA))->SplitCalamityCasters(UhObject<EECalamityClass>(inB), UhObject<EETechTreeNode>((uint64_t)inC), (uint32_t)inD != 0xffffffffu ? UnitOf((uint32_t)inD) : NULL); return 0;
    case EE_UNITS_UH_TILE_DOPPELGANGERS: return (int64_t)UhObject<EETile>(inB)->GetNumDoppelgangers();
    case EE_UNITS_UH_TILE_DOPPELGANGER_UNIT: return EEObj(UhObject<EETile>(inB)->mDoppelgangers[(size_t)inC]->mUnit);
    case EE_UNITS_UH_ATTACKABLE: return UnitOf(UhID(inA))->mUnitClass->CanBeAttacked() && EEDamageFactor(UnitOf(UhID(inA))->mUnitClass->mFamily, (long)inC) > 0.0f;
    case EE_UNITS_UH_CLEAR_AIR_TRAFFIC: ee_units_atc_clear_airport_queue(ComplexOf(UhID(inA))->GetPlayer()->mAirTraffic[ComplexOf(UhID(inA))->GetUnitClass()->mAirTrafficType - 1], ComplexOf(UhID(inA)), 0); return 0;
    case EE_UNITS_UH_NEW_GARRISON_LOAD: return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewLoad(kGoalLoadGarrison, ComplexOf(UhID(inA)), sUhUnits, true)));
    case EE_UNITS_UH_LOAD_VALID: return UhGoal(inB)->IsLoadGarrisonValid();
    case EE_UNITS_UH_DELETE_GOAL: delete UhGoal(inB); return 0;
    case EE_UNITS_UH_SET_GOAL: ComplexOf(UhID(inA))->SetGoal(UhGoal(inB), 0, 0); return 0;
    case EE_UNITS_UH_GOAL_CLONE: return EEObj(static_cast<EEUnitGoal*>(UhGoal(inB)->Clone()));
    case EE_UNITS_UH_LOAD_GOAL: return EEObj(ComplexOf(UhID(inA))->GetLoadGoal());
    case EE_UNITS_UH_ADD_LOAD_UNITS: UhGoal(inB)->AddLoadUnits(sUhUnits); return 0;
    case EE_UNITS_UH_NEW_LOAD: return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewLoad((unsigned char)inC, ComplexOf(UhID(inA)), sUhUnits, inD != 0)));
    case EE_UNITS_UH_LOAD_UNIT_TILE: sUhTile = UhGoal(inB)->GetLoadUnitTile(); return 0;
    case EE_UNITS_UH_TILE_POINT_X: return sUhTile.GetX();
    case EE_UNITS_UH_TILE_POINT_Y: return sUhTile.GetY();
    case EE_UNITS_UH_REJECTED_CLEAR: sUhRejected.clear(); return 0;
    case EE_UNITS_UH_FILTER: EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ComplexOf(UhID(inA))->mUnitClass)->FilterLoadable(&sUhUnits, ComplexOf(UhID(inA)), &sUhRejected); return 0;
    case EE_UNITS_UH_REJECTED_SIZE: return (int64_t)sUhRejected.size();
    case EE_UNITS_UH_REJECTED_ID: return (uint32_t)sUhRejected[(size_t)inC]->mUnitID;
    case EE_UNITS_UH_TREE_NODE: return EEObj(UhObject<EEPlayer>(inA)->GetTechTree()->Get((long)inC));
    case EE_UNITS_UH_NODE_KIND: return UhObject<EETechTreeNode>(inB)->mData->mKind;
    case EE_UNITS_UH_NODE_HERO: return UhObject<EETechTreeNode>(inB)->mData->mUnitClass > 0 && gUnitClassTable[UhObject<EETechTreeNode>(inB)->mData->mUnitClass] && gUnitClassTable[UhObject<EETechTreeNode>(inB)->mData->mUnitClass]->mAIKind == kAIKindHero;
    case EE_UNITS_UH_NODE_BUNDESWEHR: return gCivPowersManager.GetPower(kCivPowerBundeswehr, sUhPower = EECivPower()) && sUhPower.mTechnology == UhObject<EETechTreeNode>(inB)->mData->mPrerequisite1;
    case EE_UNITS_UH_NODE_MAKES_UNIT: return UhObject<EETechTreeNode>(inB)->mData->mMakesUnit;
    case EE_UNITS_UH_IS_EPOCH: return EEIsEpochID((long)inC);
    case EE_UNITS_UH_GROUND_POINT: gWorld.GetMap()->GetGroundPoint(GE3DPoint(f[0], f[1], 0.0f), sUhGround = GE3DPoint()); return 0;
    case EE_UNITS_UH_RESET_TRANSFORMATION: sUhTransformation = GETransformation(); return 0;
    case EE_UNITS_UH_FACE_PI: sUhTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f); return 0;
    case EE_UNITS_UH_POSITION_AT_GROUND: sUhTransformation.SetPosition(sUhGround.GetX(), sUhGround.GetY(), sUhGround.GetZ()); return 0;
    case EE_UNITS_UH_CREATE_AT: return EEObj(gUnitClassTable[inC]->CreateUnitAt(sUhTransformation, (long)inD, false, NULL));
    case EE_UNITS_UH_CLEAR_RELOAD: UhObject<EEUnit>(inB)->mComplexUnit->SetReloadTimer(0); return 0;
    case EE_UNITS_UH_FULL_POWER: UhObject<EEUnit>(inB)->mComplexUnit->SetPower(UhObject<EEUnit>(inB)->mComplexUnit->mStats->mPower); return 0;
    case EE_UNITS_UH_UPGRADE_CREATION_SOUND: return EEObj(gWorld.mPlayers[inD]->mUnitClassUpgrades[gUnitClassTable[inC]->mUnitType]->mCreationSound);
    case EE_UNITS_UH_PLAY_SOUND: gSoundManager->Play(UhObject<EESoundObject>(inB)->mSampleID, UhObject<EESoundObject>(inB)->mVolume, 0.0f, 1.0f, false, NULL); return 0;
    case EE_UNITS_UH_CREATE_BUILDING: return EEObj(gUnitClassTable[(long)inA]->CreateUnit((long)inC, (long)inD, (long)(int64_t)inB, false, NULL));
    case EE_UNITS_UH_SET_BUILT: UhObject<EEUnit>(inB)->mComplexUnit->SetBuilt(1, 0, 1); return 0;
    case EE_UNITS_UH_IS_TRANSPORT: return ComplexOf(UhID(inA))->mUnitClass->IsTransport();
    default: return 0;
    }
}

// TEST ONLY: the group command sweep (USGC records; ee_sim units/group_sweep.rs), its towns' hull reads inside a
// geometry capture scope (they are the sweep's, not the game's: the HULL/AREA records skip them)
extern "C" void ee_units_group_command_sweep();

void EEGroupCommandSweep()
{
    EEGeomCaptureScope theScope;
    ee_units_group_command_sweep();
}
