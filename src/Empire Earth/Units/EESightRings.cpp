// C:\EEX\Empire Earth\Units\EESightRings.cpp (file name inferred: no __FILE__, no RTTI). The line-of-sight
// rings: 0x20834c..0x208c37 (unit map [C], between EEGroup.cpp and EEOrbital's range 0x208c37; edge: the globals
// chain): the reveal/hide helpers walk the tile offsets at each ceiling distance (0..32) from a unit's centre for a
// unit's player and the current player's mini map and terrain reference counts (EEComplexUnit.cpp calls
// EEAddSightCircle, EERemoveSightCircle and EEMoveSightCircle). Unit flags: the exe's module set (provisional names).
// port (the object-storage plan's lane P): the tile sight is Rust (ee_units tile_sight.rs over the world views):
// the circles, the ring steps and their table, RTSTile's AddSight and RemoveSight and EEPlayer's shared and own sight
// of a tile. What stays here: the forwarders (with the TEST ONLY TSGT scope) and the world calls Rust makes through
// ee_units_tile_sight_host.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EETileLocation.h"
#include "ee/EEDoppelgangerManager.h"
#include "rust/sim/units.h"

static uint64_t EESightWorld()
{
    return (uint64_t)(uintptr_t)&gWorld;
}

static uint64_t EESightObject(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

// The world calls of the Rust tile sight (ee_units tile_sight_host.rs)
extern "C" int64_t ee_units_tile_sight_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD)
{
    switch (inOp) {
    case EE_UNITS_TILE_SIGHT_SHOWN:
        gDoppelgangerManager.TileShown(reinterpret_cast<EETile*>((uintptr_t)inA), (long)inC, (long)inD,
                                       reinterpret_cast<EEPlayer*>((uintptr_t)inB));
        return 0;
    case EE_UNITS_TILE_SIGHT_HIDDEN:
        gDoppelgangerManager.TileHidden(reinterpret_cast<EETile*>((uintptr_t)inA), reinterpret_cast<EEPlayer*>((uintptr_t)inB));
        return 0;
    case EE_UNITS_TILE_SIGHT_FIRST:
        reinterpret_cast<EEPlayer*>((uintptr_t)inA)->mSavedGroups.Reset(0x18, 1);
        return 0;
    case EE_UNITS_TILE_SIGHT_SHOW:
        if (inA != 0) {
            gMiniMap->CountCellOn((long)inC, (long)inD);
            gWorld.mMap->mTerrainMesh.ReferenceTile((unsigned long)inC, (unsigned long)inD);
        } else {
            gMiniMap->CountCellOff((long)inC, (long)inD);
            gWorld.mMap->mTerrainMesh.DereferenceTile((unsigned long)inC, (unsigned long)inD);
        }
        return 0;
    }
    return 0;
}

// Adds a player's sight to every tile within a radius (at most 32) of a point, showing them if the current player
// is that player or an ally both sharing line of sight
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightCircleAdd, inPlayer, inX, inY, 0.0f, 0.0f, inRadius);   // TSGT (TEST ONLY)
    ee_units_tile_sight_circle(EESightWorld(), 0, inX, inY, 0.0f, 0.0f, inRadius, EESightObject(inPlayer));
}

// Removes a player's sight from every tile within a radius (at most 32) of a point
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightCircleRemove, inPlayer, inX, inY, 0.0f, 0.0f, inRadius);   // TSGT (TEST ONLY)
    ee_units_tile_sight_circle(EESightWorld(), 1, inX, inY, 0.0f, 0.0f, inRadius, EESightObject(inPlayer));
}

// Moves a unit's sight: a step of at most one tile updates only the outer rings, a longer one redoes the whole circle
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightCircleMove, inPlayer, inOldX, inOldY, inNewX, inNewY, inRadius);   // TSGT (TEST ONLY)
    ee_units_tile_sight_circle(EESightWorld(), 4, inOldX, inOldY, inNewX, inNewY, inRadius, EESightObject(inPlayer));
}

// Drops one of the player's sight references on the tile; the last one leaves it explored
void RTSTile::RemoveSight(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightTileRemove, inPlayer, this);   // TSGT (TEST ONLY)
    ee_units_tile_sight_tile(EESightWorld(), 0, EESightObject(this), EESightObject(inPlayer));
}

// Adds a sight reference of the player to the tile; an unexplored tile becomes seen
void RTSTile::AddSight(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightTileAdd, inPlayer, this);   // TSGT (TEST ONLY)
    ee_units_tile_sight_tile(EESightWorld(), 1, EESightObject(this), EESightObject(inPlayer));
}

// Removes the player's sight from a tile, and its allies' sight when line of sight is shared
void EEPlayer::RemoveSharedSight(RTSTile* inTile)
{
    EESightScope theCapture(kSightPlayerRemoveShared, this, inTile);   // TSGT (TEST ONLY)
    ee_units_tile_sight_player(EESightWorld(), 0, 1, EESightObject(this), EESightObject(inTile), 0, 0);
}

// Removes the player's and its sight-sharing allies' sight from the tile at a location
void EEPlayer::RemoveSharedSightAt(const EETileLocation& inLocation)
{
    this->RemoveSharedSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// Removes only the player's own sight from a tile
void EEPlayer::RemoveOwnSight(RTSTile* inTile)
{
    EESightScope theCapture(kSightPlayerRemoveOwn, this, inTile);   // TSGT (TEST ONLY)
    ee_units_tile_sight_player(EESightWorld(), 0, 0, EESightObject(this), EESightObject(inTile), 0, 0);
}

// Removes only the player's own sight from the tile at a location
void EEPlayer::RemoveOwnSightAt(const EETileLocation& inLocation)
{
    this->RemoveOwnSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// Reveals a tile to the player and, with shared line of sight, to every ally that shares it too
void EEPlayer::AddSharedSight(EETile* inTile, long inX, long inY)
{
    EESightScope theCapture(kSightPlayerAddShared, this, inTile, inX, inY);   // TSGT (TEST ONLY)
    ee_units_tile_sight_player(EESightWorld(), 1, 1, EESightObject(this), EESightObject(inTile), inX, inY);
}

// Reveals the tile at a map location to the player and its sight-sharing allies
void EEPlayer::AddSharedSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddSharedSight(theTile, inLocation.GetX(), inLocation.GetY());
}

// Reveals a tile to the player only (a first sighting adds to statistics series 0x18)
void EEPlayer::AddOwnSight(EETile* inTile, long inX, long inY)
{
    EESightScope theCapture(kSightPlayerAddOwn, this, inTile, inX, inY);   // TSGT (TEST ONLY)
    ee_units_tile_sight_player(EESightWorld(), 1, 0, EESightObject(this), EESightObject(inTile), inX, inY);
}

// Reveals the tile at a map location to the player only
void EEPlayer::AddOwnSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddOwnSight(theTile, inLocation.GetX(), inLocation.GetY());
}
