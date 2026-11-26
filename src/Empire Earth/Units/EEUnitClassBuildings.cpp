// EEUnitClassBuildings.cpp (new, R5b.4 round 32): the buildings' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>
#include <functional>

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
#include "rust/game/rts.h"
#include "rust/sim/units.h"

// C:\EEX\Empire Earth\Units\EEUCBuilding.cpp (__FILE__ 0x4c8f28: the volcano assert, line 261). The buildings' unit
// class: 0x20e58d..0x21005e (after EEUCBomb.cpp; EEUCBuildingPriest.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (EEUnitClass.cpp's factory). Order: Virtual04, VirtualE0, EruptVolcano (the volcano erupts),
// CreateUnitAt, CreateUnit, the wall/tower helpers (0x20ea01..0x20ed72: EEGetFacingQuarter = the map quarter a tile faces),
// Virtual44, Virtual74, Virtual78, Virtual84, ReleaseUnit, VirtualA8, Virtual98, VirtualA0, VirtualB4, VirtualBC,
// VirtualB8, UpdateArmed, then EECalamityManager::GetCalamityClass<EECCVolcano>.

// 0x446ff0/0x446ff4: the unit's named constants (one .rdata copy per unit)

// 0x446ff4 (-2.0, below it a building is gone) is ee_units cls_condition.rs BUILDING_SUNK

// 0x446dc0: the yaw of each map quarter (EEGetFacingQuarter's 0..3) (provisional name)


// Returns the map quarter the unit's tile faces
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit) { return (long)ee_units_ucn_facing(2, (int32_t)(inUnit->GetTileX()), (int32_t)(inUnit->GetTileY())); }


// Returns the first living targetable building found in rings 1..19 around a unit's tile
// Lists the tiles around a building; an airport's include its runway (four times its size toward its facing)
// Sets up a new building (built, its resource stock, regeneration); an airport turns to its map quarter, a navy yard
// picks its facing and a space dock its yaw
// Launches the exiting unit: places it at the exit (runway, water or tag point), gives it the building's rally
// goals or its own (else loads it back) and starts its takeoff from this home base
// Runs an armed building's tick (physics, turrets, timers, animation, sound) and fires on its attack frame; skips
// disabled ones and standing foundations
// ---- EEUCBuildingPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). A building that
// converts (the temple): 0x21005e..0x21013b (after EEUCBuilding.cpp; EEUCFortress.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory); the slots pick the building's or the priest's handling
// by command/event. RTTI gives EEUCBuildingPriest no EEUCBasicPriest base, so the priest half is the class's own copy
// of EEUCBasicPriest's bodies: identical source compiled to identical code, which LINK's ICF folded into
// EEUCBasicPriest's (0x20d59f, 0x20d7f2, 0x20d998, 0x20dd0a, 0x20dd2d, 0x20dee4, 0x20df05, 0x20e16a, 0x20e274). With
// one class the temple runs the basic priest's bodies themselves (R5b.4 round 32); only the update's copy is left
// (PriestVirtualB8: the basic priest's update is a Rust sequence the temple does not run).

// The temple's copy of EEUCBasicPriest::Update (0x20df05)
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

// 0x24f8f9: after a gate is built for the current player (the new gate's piece 2) (provisional name)

// Replaces the other four pieces of a gate whose gate piece was replaced (new owner and class), turning and
// relinking them; the posts go idle and the doors take the new gate piece's action
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

// Collects the footprint tiles of every gate piece not left out (doors and gate piece while open), sorted and unique
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

// Returns the middle unit of a list (the lower of the two middles for an even count)

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

// Returns whether a living enemy unit of unit set 0x62 is within reach of the gate or its next two pieces
// (the squared distance is compared with the unsquared reach; cls_building.rs)
// Returns whether a unit that may pass is within reach of the gate or its next two pieces: a mobile land unit
// the owner is not hostile to, or an enemy of unit set 0x62 (cls_building.rs)
// Returns whether a tile holds a highlighted living wall that can take a gate, giving its gate direction
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection)
{
    return ee_units_ucb_gate(EE_UNITS_UCB_DO_FIND_GATE_WALL, NULL, NULL, EEObj(&ioDirection), (int64_t)(uint32_t)inTile.mX | (int64_t)((uint64_t)(uint32_t)inTile.mY << 32), 0, 0.0f) != 0;   // ucb_gate.rs (sets the direction when found)
}

// Returns the piece before the last one of a unit's linked chain
// Returns gate piece 1 (the piece before piece 3); NULL when the chain is too short (a gate created as a single
// piece, e.g. by a scenario effect)
// Returns the last piece of a unit's linked chain
// Returns gate piece 3 (two pieces before the end of the chain); NULL when the chain is too short (a gate created
// as a single piece, e.g. by a scenario effect)
// Restores the normal colour of a player's walls of a type and clears their gate direction (after gate placement)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType) { ee_units_ucb_gate(EE_UNITS_UCB_DO_CLEAR_GATE_HIGHLIGHTS, NULL, NULL, EEObj(inPlayer), inUnitType, 0, 0.0f); }  // ucb_gate.rs

// Sets up a new gate piece the building's way; the chain's first piece starts closed
// Highlights the middle wall of a gate line through each of a player's walls of a type when one or three of the
// four directions fit (an XOR of the four tests); the first fitting direction is used
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType) { ee_units_ucb_gate(EE_UNITS_UCB_DO_HIGHLIGHT_GATE_WALLS, NULL, NULL, EEObj(inPlayer), inUnitType, 0, 0.0f); }  // ucb_gate.rs

// Returns whether all tiles are on the map, flat or ramps (0 or 2 raised corners), and differ in height by at most
// 0.6 (0x447014; cls_building.rs) (EECPWallManager.cpp declares it the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles) { return ee_units_ucn_tiles_level(EEObj(&inTiles)); }

// ---- EEUCMiningBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The mining
// building's unit class (a mine built on a resource unit): 0x217085..0x217337 (after EEUCLandTransport.cpp;
// EEUCMissile.cpp follows). Unit flags: the exe's module set. Its ctor is inline (vtable 0x44843c).

// Creates a mine at a placement and ties it to the first resource under its footprint that it can gather
// Creates a mine on a tile, at the tile's height (0 off the map)
// ---- EEUCNavalBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval building's
// unit class (a dock): 0x218158..0x218490 (after EEUCNaval.cpp; EEUCNavalCitizen.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline. Virtual74/78/88 are ICF-folded into EEUCSpaceBuilding.cpp's copies; VirtualA0/E0
// here are also EEUCSpaceBuilding's (ICF).

// Picks the first fully healed docked ship that should leave (a human's ship whose own or the dock's rally goal is an
// attack, move or explore goal; any ship with a goal for a computer player); returns whether one was picked

// ---- EEUCSpaceBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The space
// building's unit class: 0x219fe3..0x21a219 (after EEUCSeaAnimal.cpp; EEUCSpaceship.cpp follows). Unit flags: the
// exe's module set. Its ctor is inline. Virtual74/78/88 are also EEUCNavalBuilding's (ICF: these copies are kept);
// VirtualA0/E0 are EEUCNavalBuilding.cpp's (ICF).

// Returns the tile under a location along the camera line, at the cruise altitude
// Creates a space building at a placement, at the cruise altitude
// Creates a space building on a tile, at the cruise altitude
// Sets up a new space building as a building, already built
// Returns whether the space building may stop: already sinking, a slow request, or at its next waypoint's tile
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
    case EE_UNITS_UCB_UNIT_CLASS_OF: return EEObj(EEUcbObject<EEUnit>(inA)->GetUnitClass());
    case EE_UNITS_UCB_CLASS_IS_WALL: return EEUcbObject<EEUnitClass>(inA)->mIsWall;
    case EE_UNITS_UCB_CLASS_TYPE: return EEUcbObject<EEUnitClass>(inA)->GetUnitType();
    case EE_UNITS_UCB_WALL_GATE_FIT:
        return EEDoesWallGateFit(EETileCoordinate((long)(int32_t)inB, (long)(int32_t)(inB >> 32)), (long)(int32_t)inC, (unsigned long)(uint32_t)(inC >> 32), *EEUcbObject<std::vector<EEUnit*> >(inA));
    case EE_UNITS_UCB_PLAYER_AT: return EEObj(::gWorld.mPlayers[inB]);
    case EE_UNITS_UCB_TECH_CLASS: return EEObj(gUnitClassTable[EEUcbObject<EEPlayer>(inA)->mTechnologies->Get((long)inB)->mData->mUnitClass]);
    case EE_UNITS_UCB_HIT_RATIO: f[0] = (float)EEUcbObject<EEUnit>(inA)->mHitPoints / EEUcbObject<EEUnit>(inA)->mStats->mMaxHitPoints; return 0;
    case EE_UNITS_UCB_PLAYER_ID: return EEUcbObject<EEPlayer>(inA)->GetPlayerID();
    case EE_UNITS_UCB_REPLACE_UNIT: return EEObj(EEReplaceUnit(theOther, (long)inB, EEUcbObject<EEUnitClass>((uint64_t)inC)));
    case EE_UNITS_UCB_TURN_GATE_PART: return EETurnGatePart(*EEUcbObject<GETransformation>(inA), (long)inB, (long)inC);
    case EE_UNITS_UCB_SET_TRANSFORMATION_OF: theOther->SetTransformation(*EEUcbObject<GETransformation>((uint64_t)inB), true); return 0;
    case EE_UNITS_UCB_SET_GATE_DIRECTION_OF: theOther->SetGateDirection((unsigned long)inB); return 0;
    case EE_UNITS_UCB_POSITION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_MESH_Z: f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_UNITS_UCB_SET_LOCATION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->SetLocation(*reinterpret_cast<GE3DPoint*>(f), true); return 0;
    case EE_UNITS_UCB_LINK_PART: ee_units_cu_body(EE_UNITS_CU_DO_LINK_PART, EEUcbObject<EEUnit>(inA)->GetComplexUnit(), (int64_t)(uintptr_t)(EEUcbObject<EEUnit>((uint64_t)inB)->GetComplexUnit()), 0, 0); return 0;
    case EE_UNITS_UCB_START_ACTION_OF: EEUcbObject<EEUnit>(inA)->GetComplexUnit()->StartAction((long)inB); return 0;
    case EE_UNITS_UCB_SET_HEALTH: EEUcbObject<EEUnit>(inA)->SetHitPoints(UMFloatToLong(EEUcbObject<EEUnit>(inA)->GetStats()->mMaxHitPoints * f[0])); return 0;
    case EE_UNITS_UCB_IN_EDITOR: return gUIManager->mInScenarioEditor && !gWorld.mGameScreenActive;
    case EE_UNITS_UCB_FLUSH_PLACEMENTS: ::gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_UCB_ADD_PENDING: EEUcbObject<EEPlayer>(inA)->AddPendingUnits(0, true); return 0;
    case EE_UNITS_UCB_REMOVE_KILLED: ee_rts_player_remove_killed_units(EEUcbObject<EEPlayer>(inA)); return 0;
    case EE_UNITS_UCB_SPACE_GATES:
        return EEUcbObject<EEPlayer>(inA)->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && EEUcbObject<EEUnitClass>((uint64_t)inB)->GetUnitType() != kUnitTypeGatePalisades;
    case EE_UNITS_UCB_ENERGY_WALL: EECreateSpaceGateEnergyWall(EEUnitCast<EEComplexUnit>(EEUcbObject<EEUnit>(inA))); return 0;
    case EE_UNITS_UCB_COMMAND_TYPE_OF: return EEUcbObject<EEUnit>(inA)->GetComplexUnit()->GetCommandType();
    case EE_UNITS_UCB_GATE_DIRECTION_OF: return EEUcbObject<EEUnit>(inA)->GetComplexUnit()->mGateDirection;
    case EE_UNITS_UCB_GET_PART: return EEObj(theClass->GetPart(theOther, (long)inB));
    case EE_UNITS_UCB_UNIT_PLAYER_ID: return EEUcbObject<EEUnit>(inA)->GetPlayer()->GetPlayerID();
    case EE_UNITS_UCB_COMPLEX_OF: return EEObj(EEUcbObject<EEUnit>(inA)->GetComplexUnit());
    case EE_UNITS_UCB_LOCATION_MATRIX_OF: *EEUcbObject<GETransformation>((uint64_t)inB) = theOther->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_UCB_SET_ANIMATION_OF: theOther->SetAnimation((long)inB, -1, 0); return 0;
    case EE_UNITS_UCB_BUILT_ALIVE: return EEUcbObject<EEUnit>(inA)->mComplexUnit->mIsBuilt && EEUcbObject<EEUnit>(inA)->mHitPoints > 0;
    case EE_UNITS_UCB_TILES_LEVEL: return EEAreTilesLevel(*EEUcbObject<std::vector<U2DPoint<long> > >(inA));
    case EE_UNITS_UCB_UNITS_OF_TYPE_ON_TILES: ee_units_uu_units_of_type_on_tiles(EEUcbObject<void>(inA), (int32_t)inB, EEUcbObject<void>((uint64_t)inC)); return 0;
    case EE_UNITS_UCB_PLAYER_UNITS_OF_TYPE: ee_units_uu_player_units_of_type((int32_t)inB, (int32_t)inC, EEUcbObject<void>(inA)); return 0;
    case EE_UNITS_UCB_COMPLEX_FIELD: return EEObj(EEUcbObject<EEUnit>(inA)->mComplexUnit);
    case EE_UNITS_UCB_SET_COLOR: theOther->SetColor(inB != 0 ? sGateHighlight : sGateNormal); return 0;
    case EE_UNITS_UCB_SET_TINTED: theOther->SetIsTinted(inB != 0); return 0;
    case EE_UNITS_UCB_IS_TINTED: return theOther->mIsTinted;
    case EE_UNITS_UCB_FIT_ON_UNIT: return EEDoesWallGateFitOnUnit(theOther, (long)(int32_t)inB, (long)(int32_t)(inB >> 32), *EEUcbObject<std::vector<EEUnit*> >((uint64_t)inC));
    case EE_UNITS_UCB_PLAYER_ID_FIELD: return EEUcbObject<EEPlayer>(inA)->mPlayerID;
    case EE_UNITS_UCB_MAP_TILE: return EEObj(::gWorld.GetMap()->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCB_TILE_NODES: return EEObj(EEUcbObject<EEMapTile>(inA)->mUnits);
    case EE_UNITS_UCB_GATE_NODE_UNIT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mUnit);
    case EE_UNITS_UCB_GATE_NODE_NEXT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mNext);
    case EE_UNITS_UCB_TINTED_WALL:
        return EEUcbObject<EEUnit>(inA)->mUnitClass->mIsWall && EEUcbObject<EEUnit>(inA)->mHitPoints > 0 && EEUcbObject<EEUnit>(inA)->mTileNode != NULL && EEUcbObject<EEUnit>(inA)->GetComplexUnit()->mIsTinted;
    case EE_UNITS_UCB_RING: ee_units_uu_square_ring_tiles((int32_t)inB, (int32_t)(inB >> 32), (int32_t)inC, EEUcbObject<void>(inA)); return 0;
    case EE_UNITS_UCB_UNIT_TILE_X: return EEUcbObject<EEUnit>(inA)->GetTileX();
    case EE_UNITS_UCB_UNIT_TILE_Y: return EEUcbObject<EEUnit>(inA)->GetTileY();
    case EE_UNITS_UCB_TARGET_BUILDING:
        return EEUcbObject<EEUnit>(inA)->mUnitClass->IsLandBuilding() && EEUcbObject<EEUnit>(inA)->mHitPoints > 0 && ee_units_uu_targetable(0, EEObj(EEUcbObject<EEUnit>(inA)->GetUnitClass()), 0);
    case EE_UNITS_UCB_PREVIOUS_PART: return EEObj(theOther->mPreviousPart);
    case EE_UNITS_UCB_NEXT_PART_OF: return EEObj(theOther->mNextPart);
    case EE_UNITS_UCB_PART_LEFT_OUT: return theClass->IsPartLeftOut(theOther);
    case EE_UNITS_UCB_GATE_BASE_SURROUNDING: theClass->BaseGetSurroundingTiles(theOther, *EEUcbObject<std::vector<EETileCoordinate> >((uint64_t)inB), (long)inC, true); return 0;
    case EE_UNITS_UCB_SORT_UNIQUE: EESortUnique(*EEUcbObject<std::vector<U2DPoint<long> > >(inA), std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >()); return 0;
    case EE_UNITS_UCB_COLLISION_STATE: return theUnit->mCollisionState;
    case EE_UNITS_UCB_NEXT_WAYPOINT: return theUnit->GetNextWaypointPoint(*reinterpret_cast<GE3DPoint*>(f));
    case EE_UNITS_UCB_SELF_TILE_X: return theUnit->GetTileX();
    case EE_UNITS_UCB_SELF_TILE_Y: return theUnit->GetTileY();
    case EE_UNITS_UCB_BUILDING_UNIT_CREATED: theClass->BuildingUnitCreated(theUnit); return 0;
    case EE_UNITS_UCB_CHAIN_HEAD: return theUnit->mPreviousPart == NULL && theUnit->mNextPart != NULL;
    case EE_UNITS_UCB_START_ACTION: theUnit->StartAction((long)inB); return 0;
    case EE_UNITS_UCB_ACTION_CHANGED: theClass->ActionChanged(theUnit); return 0;
    case EE_UNITS_UCB_SPACE_SET_BUILT: theUnit->SetBuilt(1, 0, 1); return 0;
    case EE_UNITS_UCB_LOCATION_YAW: theUnit->mPhysicalModel.GetLocation().ConvertToOrientation(*reinterpret_cast<GEOrientation*>(f)); return 0;
    case EE_UNITS_UCB_CARGO_SIZE: return theUnit->mCargo.size();
    case EE_UNITS_UCB_CARGO_INDEX: return EEObj(theUnit->mCargo[(size_t)inB]);
    case EE_UNITS_UCB_HIT_POINTS_OF: return theOther->GetHitPoints();
    case EE_UNITS_UCB_MAX_HIT_POINTS_OF: return theOther->GetStats()->mMaxHitPoints;
    case EE_UNITS_UCB_HUMAN_OF: return theOther->GetPlayer()->IsHuman();
    case EE_UNITS_UCB_QUEUE_SHIFT_MODE: return theUnit->mGoalQueues[(size_t)inB].GetUnit()->mPlayer->mShiftMode;
    case EE_UNITS_UCB_GOAL_STATE_OF: return EEObj(theOther->mGoalState);
    case EE_UNITS_UCB_UNIT_ID_OF: return theOther->mUnitID;
    case EE_UNITS_UCB_SET_EXITING: theUnit->SetExitingUnitID((unsigned long)inB); return 0;
    case EE_UNITS_UCB_DETACHED: return theOther->mIsDetachedFromAI;
    case EE_UNITS_UCB_CLEAR_QUEUE0: theOther->mGoalQueues[0].Clear(); return 0;
    case EE_UNITS_UCB_REFRESH_GOALS: theOther->RefreshGoals(); return 0;
    case EE_UNITS_UCB_REFRESH_ACTIVE: ee_units_unit_refresh_active_goal((uint64_t)(uintptr_t)theOther); return 0;
    case EE_UNITS_UCB_QUEUE_LEN_OF: return theOther->GetQueueGoals((unsigned long)inB).size();
    case EE_UNITS_UCB_QUEUE_FRONT_OF: return EEObj(theOther->mGoalQueues[(size_t)inB].GetFront());
    case EE_UNITS_UCB_EXPLORE_CAST: return EEObj(EEExploreGoalCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_ATTACK_CAST: return EEObj(EEAttackUnitCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_HAS_LOCATION: return EEGoalHasLocation(EEUcbObject<EEUnitGoal>(inA));
    case EE_UNITS_UCB_HIT_POINTS_FIELD_OF: return theOther->mHitPoints;
    case EE_UNITS_UCB_MAX_HIT_POINTS_FIELD_OF: return theOther->mStats->mMaxHitPoints;
    case EE_UNITS_UCB_PLAYER_HUMAN_OF: return theOther->mPlayer->IsHuman();
    case EE_UNITS_UCB_COMPUTER_PLAYER: return EEObj(theUnit->mPlayer->mComputerPlayer);
    case EE_UNITS_UCB_EXITING_ID: return theUnit->mExitingUnitID;
    case EE_UNITS_UCB_AIR_TRAFFIC_SELF: return theUnit->mUnitClass->mAirTrafficType;
    case EE_UNITS_UCB_TILE_SIZE_FIELD: return theClass->mTileSize;
    case EE_UNITS_UCB_FACING: return theUnit->mFacing;
    case EE_UNITS_UCB_FIND_PASSENGER: return EEObj(theUnit->FindPassenger((unsigned long)inB));
    case EE_UNITS_UCB_TAGS_EMPTY: return EEUcbObject<EETagPointList>(inA)->mPoints.empty();
    case EE_UNITS_UCB_TAGS_SCALE_STATS: EEUcbObject<EETagPointList>(inA)->Scale(*EEUcbObject<GETransformation>((uint64_t)inB), theUnit->mStats->GetScale()); return 0;
    case EE_UNITS_UCB_TAG0_POINT: EEUcbObject<EETagPointList>(inA)->mPoints[0]->GetPoint(EEUcbObject<EEComplexUnit>((uint64_t)inB)->mPhysicalModel.GetFrame(), *EEUcbObject<GETransformation>((uint64_t)inC), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_SET_CARGO_SCALE: theOther->mPhysicalModel.SetScale(theOther->mStats->GetScale() * ((f[0] * 0.5f) / theOther->mUnitClass->GetCruiseAltitude() + 0.5f)); return 0;
    case EE_UNITS_UCB_UNLOAD_BY_ID: return EEObj(theUnit->UnloadCargoByID((unsigned long)inB, *EEUcbObject<GETransformation>(inA)));
    case EE_UNITS_UCB_CATEGORY_SELF: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_UCB_QUEUE_GOAL_OF: return EEObj(theOther->GetQueueGoals((unsigned long)inB)[(size_t)inC]);
    case EE_UNITS_UCB_UNIT_HIT_POINTS: return EEUcbObject<EEUnit>(inA)->mHitPoints;
    case EE_UNITS_UCB_UNIT_TYPE_SELF: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_UCB_CATEGORY_OF: return theOther->mUnitClass->mCategory;
    case EE_UNITS_UCB_FAMILY_OF: return theOther->mUnitClass->mFamily;
    case EE_UNITS_UCB_PARATROOPER_OF: return theOther->mUnitClass->mIsParatrooperBomber;
    case EE_UNITS_UCB_GOAL_OF: return EEObj(theOther->GetGoal());
    case EE_UNITS_UCB_LOAD_CARGO: (long)ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, theUnit, (int64_t)(uintptr_t)theOther, false, 0); return 0;
    case EE_UNITS_UCB_ATC_REMOVE_TAKEOFF: ee_units_atc_remove_from_takeoff_queues(theOther->mPlayer->mAirTraffic[theOther->mUnitClass->mAirTrafficType - 1], theOther); return 0;
    case EE_UNITS_UCB_UNIT_QUEUE_LEN: return theUnit->mGoalQueues[(size_t)inB].mGoals.size();
    case EE_UNITS_UCB_UNIT_QUEUE_GOAL: return EEObj(theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]);
    case EE_UNITS_UCB_TILE_FREE: return EEIsTileFree(theOther, (unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCB_UNIT_TYPE_OF: return theOther->mUnitClass->mUnitType;
    case EE_UNITS_UCB_SET_HOME_BASE: ee_units_cu_body(EE_UNITS_CU_DO_SET_HOME_BASE, theOther, (int64_t)(uintptr_t)theUnit, 0, 0); return 0;
    case EE_UNITS_UCB_SET_BUILT: theUnit->SetBuilt((long)inB, (long)(inC & 1), (long)(inC >> 1)); return 0;
    case EE_UNITS_UCB_SET_RESOURCE_TYPE: theUnit->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCB_RESOURCE_TYPE: return theUnit->mResourceType;
    case EE_UNITS_UCB_SET_RESOURCE_AMOUNT: theUnit->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCB_FACING_QUARTER: return EEGetUnitFacingQuarter(theUnit);
    case EE_UNITS_UCB_SET_FACING: theUnit->SetFacing((unsigned long)inB); return 0;
    case EE_UNITS_UCB_NAVY_YARD_FACING: return (long)ee_units_ucn_facing(1, (int32_t)(unsigned long)(theUnit->GetTileX()), (int32_t)(unsigned long)(theUnit->GetTileY()));
    case EE_UNITS_UCB_SPACE_DOCK_YAW: f[0] = EEGetSpaceDockYaw(theUnit->GetTileX() + 1, theUnit->GetTileY() + 1, theUnit->mUnitClass->GetTileSize()); return 0;
    case EE_UNITS_UCB_DISABLED: return theUnit->mIsDisabled;
    case EE_UNITS_UCB_PHYSICS_ACTION: return theUnit->mCurrentPhysicsAction;
    case EE_UNITS_UCB_UPDATE_TURRETS: theUnit->UpdateTurrets(); return 0;
    case EE_UNITS_UCB_TILE_X_FIELD: return theUnit->mTileX;
    case EE_UNITS_UCB_UPDATE_HEAL_SEARCH: ee_units_ucp_call(EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER, NULL, theUnit, 0, 0); return 0;
    case EE_UNITS_UCB_IS_MEDIC: return theClass->mIsMedic;
    case EE_UNITS_UCB_GOAL_WORK: theUnit->mGoalState->Work(theUnit); return 0;
    case EE_UNITS_UCB_REGENERATION: return theUnit->mStats->mRegeneration;
    case EE_UNITS_UCB_REGENERATE_POWER: theClass->RegeneratePower(theUnit); return 0;
    case EE_UNITS_UCB_PRIEST_CONVERSION: ee_units_ucp_call(EE_UNITS_UCP_DO_UPDATE_CONVERSION, theClass, theUnit, 0, 0); return 0;
    case EE_UNITS_UCB_CONVERSION_START: return theUnit->mConversionStartTime;
    case EE_UNITS_UCB_CLEAR_CONVERSION_START: theUnit->mConversionStartTime = 0; return 0;
    case EE_UNITS_UCB_RELEASE_CONVERSION: theUnit->ReleaseConversionEffect(); return 0;
    case EE_UNITS_UCB_HIDE_CONVERSION: ee_units_ucp_call(EE_UNITS_UCP_DO_HIDE_CONVERSION, theClass, theUnit, 0, 0); return 0;
    case EE_UNITS_UCB_CRUISE_ALTITUDE: f[0] = theClass->mCruiseAltitude; return 0;
    case EE_UNITS_UCB_CAN_GATHER: return EEUcbObject<EEUnit>(inA)->CanGather(EEUcbObject<EEUnit>((uint64_t)inB), EEUcbObject<EEUnit>((uint64_t)inB)->GetTileX(), EEUcbObject<EEUnit>((uint64_t)inB)->GetTileY(), 0, 0);
    case EE_UNITS_UCB_TILE_UNITS: return EEObj(EEUcbObject<EETile>(inA)->mUnits);
    case EE_UNITS_UCB_NODE_UNIT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mUnit);
    case EE_UNITS_UCB_NODE_NEXT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mNext);
    case EE_UNITS_UCB_SET_MINE: ee_units_cu_body(EE_UNITS_CU_DO_SET_MINE, EEUcbObject<EEUnit>(inA)->mComplexUnit, (int64_t)(uintptr_t)(EEUcbObject<EEUnit>((uint64_t)inB)), 0, 0); return 0;
    case EE_UNITS_UCB_PREVIOUS_PART_OF: return EEObj(theOther->mPreviousPart);
    case EE_UNITS_UCB_TILE_SIZE_OF: return theOther->mUnitClass->GetTileSize();
    case EE_UNITS_UCB_FACING_QUARTER_AT: return (long)ee_units_ucn_facing(2, (int32_t)((long)inB), (int32_t)((long)inC));
    case EE_UNITS_UCB_TILES_CLEAR: EEUcbObject<std::vector<EETileCoordinate>>(inA)->clear(); return 0;
    case EE_UNITS_UCB_BASE_SURROUNDING: theClass->BaseGetSurroundingTiles(theUnit, *EEUcbObject<std::vector<EETileCoordinate>>(inA), (long)inB, inC != 0); return 0;
    case EE_UNITS_UCB_COMPLEX_FIELD_OF: return EEObj(theOther->mComplexUnit);
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
