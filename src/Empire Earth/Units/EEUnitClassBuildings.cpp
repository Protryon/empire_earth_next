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


// 0x447004..0x447018: the unit's named float constants (one .rdata block per unit) (provisional names)
// 0x447004/0x447008 (1.0 open, 0.0 closed) are ee_units cls_condition.rs GATE_OPEN, GATE_CLOSED



bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits);


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



