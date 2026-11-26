// EEUnitClassBuildings.cpp (new, R5b.4 round 32): the buildings' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUIManager.h"
#include "ee/EEUnitUtilities.h"

// C:\EEX\Empire Earth\Units\EEUCBuilding.cpp (__FILE__ 0x4c8f28: the volcano assert, line 261). The buildings' unit
// class: 0x20e58d..0x21005e (after EEUCBomb.cpp; EEUCBuildingPriest.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (EEUnitClass.cpp's factory). Order: Virtual04, VirtualE0, EruptVolcano (the volcano erupts),
// CreateUnitAt, CreateUnit, the wall/tower helpers (0x20ea01..0x20ed72: EEGetFacingQuarter = the map quarter a tile faces),
// Virtual44, Virtual74, Virtual78, Virtual84, ReleaseUnit, VirtualA8, Virtual98, VirtualA0, VirtualB4, VirtualBC,
// VirtualB8, UpdateArmed, then EECalamityManager::GetCalamityClass<EECCVolcano>.

// 0x446ff0/0x446ff4: the unit's named constants (one .rdata copy per unit)

// 0x446ff4 (-2.0, below it a building is gone) is ee_units cls_condition.rs BUILDING_SUNK

// Returns the map quarter facing the most space terrain within 6 tiles of a tile (half weight to the neighbouring
// sides; its terrain run 0x60ea01 and the side tables 0x4c8ef8/0x4c8f08: ee_units cls_building.rs, R5b.4 round 33)
long EEGetDockFacing(unsigned long inX, unsigned long inY) { return ee_units_ucn_facing(0, (int32_t)inX, (int32_t)inY); }

// Returns a navy yard's facing at a tile: its map quarter if 5 deep-water tiles follow that way, else the next side
// that has them, else the side with the longest run (cls_building.rs)
static long GetNavyYardFacing(unsigned long inX, unsigned long inY) { return ee_units_ucn_facing(1, (int32_t)inX, (int32_t)inY); }

// Returns the map quarter (0..3) a tile faces from the middle of its continent (space maps) or of the map
// (cls_building.rs)
long EEGetFacingQuarter(long inX, long inY) { return ee_units_ucn_facing(2, (int32_t)inX, (int32_t)inY); }

// Returns the map quarter the unit's tile faces
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit) { return EEGetFacingQuarter(inUnit->GetTileX(), inUnit->GetTileY()); }

// Returns the model part for a wall piece kind (0..8); outFlag is set for the corner kinds 0, 2, 6 and 8
// (inferred meaning)
long EEGetWallPiecePart(long inIndex, bool* outFlag) { return ee_units_ucb_wall_piece_part((int32_t)inIndex, outFlag); }  // ucb_release.rs

// ---- EEUCGate.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The wall gate's unit class:
// 0x21048f..0x212a6f (after EEUCFortress.cpp; EEUCHelicopter.cpp follows). One unit per EEUC* class in alphabetical
// order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). The unit opens with the
// `$E` chains of its two file-static colours, then Virtual20, EEBuildGate (EEMBuildGate's work), the slots and the
// gate helpers in address order; EEMap's corner-height inlines and the sort/unique COMDATs over the tile vector
// close it. A gate is five linked pieces (+0x1cc/+0x1d0): piece 2 is the gate, 0/4 the posts, 1/3 the doors.

// The highlight of a wall that can take a gate, and the normal colour (`$E` chains 0x21048f / 0x2104bf; within one
// unit later globals get lower addresses: 0x5634f0 / 0x5634e0) (provisional names)
static GEColor sGateHighlight(0.0f, 1.0f, 0.0f);

static GEColor sGateNormal(1.0f, 1.0f, 1.0f);

// 0x447004..0x447018: the unit's named float constants (one .rdata block per unit) (provisional names)
// 0x447004/0x447008 (1.0 open, 0.0 closed) are ee_units cls_condition.rs GATE_OPEN, GATE_CLOSED

long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex);

void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles);

bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits);

bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// 0x24c627 (inTiles' units of inUnitType): ee/EEUnitUtilities.h

// Turns the five wall pieces across a tile into a closed gate of the technology's class for a player (EEMBuildGate's
// work): weakest wall's health, posts on the lower ground; adds the energy wall once space gates are available
void EEBuildGate(EETileCoordinate& inTile, long inPlayer, long inTechnology, long inDirection)
{
    ee_units_ucb_gate(EE_UNITS_UCB_DO_BUILD_GATE, NULL, NULL, (uint64_t)inPlayer, (int64_t)(uint32_t)inTile.mX | (int64_t)((uint64_t)(uint32_t)inTile.mY << 32),
                      (int64_t)(uint32_t)inTechnology | (int64_t)((uint64_t)(uint32_t)inDirection << 32), 0.0f);   // ucb_gate.rs
}

// The sort-and-unique of a vector (cdecl template 0x211ebc and its range half 0x211f07, here one body) (provisional
// names)
template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    ioValues.erase(std::unique(ioValues.begin(), ioValues.end(), inEqual), ioValues.end());
}

// Returns whether a gate fits across a wall unit's tile in a direction (1..4), giving the five walls of the type
// (EECPWallManager.cpp declares it the same way)
bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    return EEDoesWallGateFit(EETileCoordinate(inUnit->mTileX, inUnit->mTileY), inSide, inUnitType, outUnits);
}

// Returns whether a gate fits across a tile in a direction: five built, living walls of the type on level tiles
// (else outUnits is emptied)
bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits)
{
    return ee_units_ucn_wall_gate_fit((int32_t)inTile.mX, (int32_t)inTile.mY, (int32_t)inDirection, (uint32_t)inUnitType, EEObj(&outUnits));
}

// Returns the first piece of a unit's linked chain (+0x1cc)
EEComplexUnit* EEGetFirstPart(EEComplexUnit* inUnit) { return (EEComplexUnit*)(uintptr_t)ee_units_uc_rest(EE_UNITS_UC_DO_R_FIRST_PART, NULL, NULL, EEObj(inUnit), 0, 0, NULL); }  // uc_rest.rs

// Turns gate piece inIndex for a gate in direction 1..4; returns its animation: 9 a post, 10 a diagonal post, 11 else
// (the yaws: cls_building.rs)
long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex)
{
    return ee_units_ucn_turn_gate_part(GERust(ioTransformation), (int32_t)inDirection, (int32_t)inIndex);   // ucn_noted.rs (UCBD note 24)
}

// Returns piece inIndex (0..4) of a gate, found along its linked chain (2 is the gate piece itself)
// (GetPart: ucn_noted.rs, UCBD note 29; inline in ee/EEUnit.h)

// Lists the five tiles of a gate line through a tile: direction 1 along x, 2 along y, 3 and 4 the diagonals
// (cls_building.rs)
void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_ucn_wall_gate_tiles((int32_t)inTile.mX, (int32_t)inTile.mY, (int32_t)inDirection, EEObj(&outTiles));
}

// Returns whether a tile holds a highlighted living wall that can take a gate, giving its gate direction
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection)
{
    return ee_units_ucb_gate(EE_UNITS_UCB_DO_FIND_GATE_WALL, NULL, NULL, EEObj(&ioDirection), (int64_t)(uint32_t)inTile.mX | (int64_t)((uint64_t)(uint32_t)inTile.mY << 32), 0, 0.0f) != 0;   // ucb_gate.rs (sets the direction when found)
}

// Restores the normal colour of a player's walls of a type and clears their gate direction (after gate placement)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType) { ee_units_ucb_gate(EE_UNITS_UCB_DO_CLEAR_GATE_HIGHLIGHTS, NULL, NULL, EEObj(inPlayer), inUnitType, 0, 0.0f); }  // ucb_gate.rs

// Highlights the middle wall of a gate line through each of a player's walls of a type when one or three of the
// four directions fit (an XOR of the four tests); the first fitting direction is used
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType) { ee_units_ucb_gate(EE_UNITS_UCB_DO_HIGHLIGHT_GATE_WALLS, NULL, NULL, EEObj(inPlayer), inUnitType, 0, 0.0f); }  // ucb_gate.rs

// Returns whether all tiles are on the map, flat or ramps (0 or 2 raised corners), and differ in height by at most
// 0.6 (0x447014; cls_building.rs) (EECPWallManager.cpp declares it the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles) { return ee_units_ucn_tiles_level(EEObj(&inTiles)); }

template <class T> static T* EEUcbObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// the gate bodies' vectors (ucb_gate.rs: one per local of the originals) and EEFindGateWall's direction
static std::vector<EEUnit*> sUcbUnits[8];
static std::vector<U2DPoint<long> > sUcbTiles[3];

// ReleaseUnit's yaw is the orientation's first float
static_assert(offsetof(GEOrientation, mYaw) == 0, "a GEOrientation starts with its yaw");

// The C++ of the buildings' Rust bodies (ee_units ucb_release.rs; EE_UNITS_UCB_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucb_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcbObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCB_R_VOLCANO_CLASS: return USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(9, *EEUcbObject<EECalamityClass*>(inA));   // uc_rest.rs EruptVolcano
    case EE_UNITS_UCB_R_VOLCANO_VERIFY: ESVerify(__FILE__, 261, "EECalamityManager::Instance()->FindCalamityClass(keectVolcano, theVolcanoClass)"); return 0; // the original assert text (provisional names here)
    case EE_UNITS_UCB_R_RANDOM_LOCATION: EEUcbObject<EECalamityClass>(inA)->GetRandomLocation(GE3DPoint(f[0], f[1], f[2]), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_R_ATTACK_GROUND: theUnit->SetGoal(EEUGRust::NewAttackGround(*reinterpret_cast<GE3DPoint*>(f), false), 0, 0); return 0;
    // the gates' (ucb_gate.rs)
    case EE_UNITS_UCB_UNITS_VEC: return EEObj(&sUcbUnits[inB]);
    case EE_UNITS_UCB_TILES_VEC: return EEObj(&sUcbTiles[inB]);
    case EE_UNITS_UCB_UNITS_SIZE: return EEUcbObject<std::vector<EEUnit*> >(inA)->size();
    case EE_UNITS_UCB_UNITS_AT: return EEObj((*EEUcbObject<std::vector<EEUnit*> >(inA))[(size_t)inB]);
    case EE_UNITS_UCB_UNITS_CLEAR: EEUcbObject<std::vector<EEUnit*> >(inA)->clear(); return 0;
    case EE_UNITS_UCB_GATE_TILES_CLEAR: EEUcbObject<std::vector<U2DPoint<long> > >(inA)->clear(); return 0;
    case EE_UNITS_UCB_TILES_SIZE: return EEUcbObject<std::vector<U2DPoint<long> > >(inA)->size();
    case EE_UNITS_UCB_TILE_X: return (*EEUcbObject<std::vector<U2DPoint<long> > >(inA))[(size_t)inB].mX;
    case EE_UNITS_UCB_TILE_Y: return (*EEUcbObject<std::vector<U2DPoint<long> > >(inA))[(size_t)inB].mY;
    case EE_UNITS_UCB_WALL_GATE_TILES: EEGetWallGateTiles(U2DPoint<long>((long)(int32_t)inB, (long)(int32_t)(inB >> 32)), (long)inC, *EEUcbObject<std::vector<U2DPoint<long> > >(inA)); return 0;
    case EE_UNITS_UCB_UNITS_ON: ee_units_uu_units_on(0, EEUcbObject<void>(inA), EEUcbObject<void>((uint64_t)inB)); return 0;
    case EE_UNITS_UCB_WALL_GATE_FIT:
        return EEDoesWallGateFit(EETileCoordinate((long)(int32_t)inB, (long)(int32_t)(inB >> 32)), (long)(int32_t)inC, (unsigned long)(uint32_t)(inC >> 32), *EEUcbObject<std::vector<EEUnit*> >(inA));
    case EE_UNITS_UCB_TECH_CLASS: return EEObj(gUnitClassTable[EEUcbObject<EEPlayer>(inA)->mTechnologies->Get((long)inB)->mData->mUnitClass]);
    case EE_UNITS_UCB_REPLACE_UNIT: return EEObj(EEReplaceUnit(theOther, (long)inB, EEUcbObject<EEUnitClass>((uint64_t)inC)));
    case EE_UNITS_UCB_TURN_GATE_PART: return EETurnGatePart(*EEUcbObject<GETransformation>(inA), (long)inB, (long)inC);
    case EE_UNITS_UCB_SET_GATE_DIRECTION_OF: theOther->SetGateDirection((unsigned long)inB); return 0;
    case EE_UNITS_UCB_POSITION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_MESH_Z: f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_UNITS_UCB_SET_LOCATION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->SetLocation(*reinterpret_cast<GE3DPoint*>(f), true); return 0;
    case EE_UNITS_UCB_LINK_PART: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->LinkPart(EEUcbObject<EEUnit>((uint64_t)inB)->GetComplexUnit()); return 0;
    case EE_UNITS_UCB_START_ACTION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->StartAction((long)inB); return 0;
    case EE_UNITS_UCB_SET_HEALTH: EEUcbObject<EEUnit>(inA)->SetHitPoints(UMFloatToLong(EEUcbObject<EEUnit>(inA)->GetStats()->mMaxHitPoints * f[0])); return 0;
    case EE_UNITS_UCB_IN_EDITOR: return gUIManager->mInScenarioEditor && !gWorld.mGameScreenActive;
    case EE_UNITS_UCB_FLUSH_PLACEMENTS: ::gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_UCB_ADD_PENDING: EEUcbObject<EEPlayer>(inA)->AddPendingUnits(0, true); return 0;
    case EE_UNITS_UCB_REMOVE_KILLED: EEUcbObject<EEPlayer>(inA)->RemoveKilledUnits(); return 0;
    case EE_UNITS_UCB_SPACE_GATES:
        return EEUcbObject<EEPlayer>(inA)->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && EEUcbObject<EEUnitClass>((uint64_t)inB)->GetUnitType() != kUnitTypeGatePalisades;
    case EE_UNITS_UCB_ENERGY_WALL: EECreateSpaceGateEnergyWall(EEUnitCast<EEComplexUnit>(EEUcbObject<EEUnit>(inA))); return 0;
    case EE_UNITS_UCB_LOCATION_MATRIX_OF: *EEUcbObject<GETransformation>((uint64_t)inB) = theOther->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_UCB_TILES_LEVEL: return EEAreTilesLevel(*EEUcbObject<std::vector<U2DPoint<long> > >(inA));
    case EE_UNITS_UCB_UNITS_OF_TYPE_ON_TILES: ee_units_uu_units_of_type_on_tiles(EEUcbObject<void>(inA), (int32_t)inB, EEUcbObject<void>((uint64_t)inC)); return 0;
    case EE_UNITS_UCB_PLAYER_UNITS_OF_TYPE: ee_units_uu_player_units_of_type((int32_t)inB, (int32_t)inC, EEUcbObject<void>(inA)); return 0;
    case EE_UNITS_UCB_SET_COLOR: theOther->SetColor(inB != 0 ? sGateHighlight : sGateNormal); return 0;
    case EE_UNITS_UCB_SET_TINTED: theOther->SetIsTinted(inB != 0); return 0;
    case EE_UNITS_UCB_FIT_ON_UNIT: return EEDoesWallGateFitOnUnit(theOther, (long)(int32_t)inB, (long)(int32_t)(inB >> 32), *EEUcbObject<std::vector<EEUnit*> >((uint64_t)inC));
    case EE_UNITS_UCB_MAP_TILE: return EEObj(::gWorld.GetMap()->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCB_RING: ee_units_uu_square_ring_tiles((int32_t)inB, (int32_t)(inB >> 32), (int32_t)inC, EEUcbObject<void>(inA)); return 0;
    case EE_UNITS_UCB_TARGET_BUILDING:
        return EEUcbObject<EEUnit>(inA)->mUnitClass->IsLandBuilding() && EEUcbObject<EEUnit>(inA)->mHitPoints > 0 && ee_units_uu_targetable(0, EEObj(EEUcbObject<EEUnit>(inA)->GetUnitClass()), 0);
    case EE_UNITS_UCB_SORT_UNIQUE: EESortUnique(*EEUcbObject<std::vector<U2DPoint<long> > >(inA), std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >()); return 0;
    case EE_UNITS_UCB_NEXT_WAYPOINT: return theUnit->GetNextWaypointPoint(*reinterpret_cast<GE3DPoint*>(f));
    case EE_UNITS_UCB_LOCATION_YAW: theUnit->mPhysicalModel.GetLocation().ConvertToOrientation(*reinterpret_cast<GEOrientation*>(f)); return 0;
    case EE_UNITS_UCB_HUMAN_OF: return theOther->GetPlayer()->IsHuman();
    case EE_UNITS_UCB_QUEUE_SHIFT_MODE: return theUnit->mGoalQueues[(size_t)inB].GetUnit()->mPlayer->mShiftMode;
    case EE_UNITS_UCB_SET_EXITING: theUnit->SetExitingUnitID((unsigned long)inB); return 0;
    case EE_UNITS_UCB_CLEAR_QUEUE0: theOther->mGoalQueues[0].Clear(); return 0;
    case EE_UNITS_UCB_QUEUE_LEN_OF: return theOther->GetQueueGoals((unsigned long)inB).size();
    case EE_UNITS_UCB_QUEUE_FRONT_OF: return EEObj(theOther->mGoalQueues[(size_t)inB].GetFront());
    case EE_UNITS_UCB_EXPLORE_CAST: return EEObj(EEExploreGoalCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_ATTACK_CAST: return EEObj(EEAttackUnitCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_HAS_LOCATION: return EEGoalHasLocation(EEUcbObject<EEUnitGoal>(inA));
    case EE_UNITS_UCB_PLAYER_HUMAN_OF: return theOther->mPlayer->IsHuman();
    case EE_UNITS_UCB_TAGS_EMPTY: return EEUcbObject<EETagPointList>(inA)->mPoints.empty();
    case EE_UNITS_UCB_TAGS_SCALE_STATS: EEUcbObject<EETagPointList>(inA)->Scale(*EEUcbObject<GETransformation>((uint64_t)inB), theUnit->mStats->GetScale()); return 0;
    case EE_UNITS_UCB_TAG0_POINT: EEUcbObject<EETagPointList>(inA)->mPoints[0]->GetPoint(EEUcbObject<EEComplexUnit>((uint64_t)inB)->mPhysicalModel.GetFrame(), *EEUcbObject<GETransformation>((uint64_t)inC), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_SET_CARGO_SCALE: theOther->mPhysicalModel.SetScale(theOther->mStats->GetScale() * ((f[0] * 0.5f) / theOther->mUnitClass->GetCruiseAltitude() + 0.5f)); return 0;
    case EE_UNITS_UCB_QUEUE_GOAL_OF: return EEObj(theOther->GetQueueGoals((unsigned long)inB)[(size_t)inC]);
    case EE_UNITS_UCB_ATC_REMOVE_TAKEOFF: ee_units_atc_remove_from_takeoff_queues(theOther->mPlayer->mAirTraffic[theOther->mUnitClass->mAirTrafficType - 1], theOther); return 0;
    case EE_UNITS_UCB_UNIT_QUEUE_LEN: return theUnit->mGoalQueues[(size_t)inB].mGoals.size();
    case EE_UNITS_UCB_UNIT_QUEUE_GOAL: return EEObj(theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]);
    case EE_UNITS_UCB_TILE_FREE: return EEIsTileFree(theOther, (unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCB_SET_RESOURCE_TYPE: theUnit->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCB_SET_RESOURCE_AMOUNT: theUnit->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCB_FACING_QUARTER: return EEGetUnitFacingQuarter(theUnit);
    case EE_UNITS_UCB_SET_FACING: theUnit->SetFacing((unsigned long)inB); return 0;
    case EE_UNITS_UCB_NAVY_YARD_FACING: return GetNavyYardFacing(theUnit->GetTileX(), theUnit->GetTileY());
    case EE_UNITS_UCB_SPACE_DOCK_YAW: f[0] = UBitsFloat(ee_units_ck_body(EE_UNITS_CK_DO_SPACE_DOCK_YAW, 0, 0, theUnit->GetTileX() + 1, theUnit->GetTileY() + 1)); return 0;
    case EE_UNITS_UCB_UPDATE_TURRETS: theUnit->UpdateTurrets(); return 0;
    case EE_UNITS_UCB_GOAL_WORK: theUnit->mGoalState->Work(theUnit); return 0;
    case EE_UNITS_UCB_SET_MINE: EEUcbObject<EEUnit>(inA)->mComplexUnit->SetMine(EEUcbObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCB_FACING_QUARTER_AT: return EEGetFacingQuarter((long)inB, (long)inC);
    case EE_UNITS_UCB_TILES_CLEAR: EEUcbObject<std::vector<EETileCoordinate>>(inA)->clear(); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
