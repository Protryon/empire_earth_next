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

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDoppelgangerManager.h"

static uint64_t EESightWorld() { return (uint64_t)(uintptr_t)&gWorld; }

static uint64_t EESightObject(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }

// the Rust sight calls inside their TSGT capture scope (TEST ONLY): a circle (inOn 0 add, 1 remove, 4 move), a tile's
// sight (inOn 1 add, 0 remove) and a player's own or shared sight of a tile (a removal names no coordinates: -1 to the
// capture, 0 to Rust, as the original's calls)
static void EESightCircle(unsigned int inKind, int inOn, EEPlayer* inPlayer, float inX0, float inY0, float inX1, float inY1, float inRadius)
{
    EESightScope theCapture(inKind, inPlayer, inX0, inY0, inX1, inY1, inRadius);
    ee_units_tile_sight_circle(EESightWorld(), (uint8_t)inOn, inX0, inY0, inX1, inY1, inRadius, EESightObject(inPlayer));
}

static void EESightTile(unsigned int inKind, int inOn, RTSTile* inTile, EEPlayer* inPlayer)
{
    EESightScope theCapture(inKind, inPlayer, inTile);
    ee_units_tile_sight_tile(EESightWorld(), (uint8_t)inOn, EESightObject(inTile), EESightObject(inPlayer));
}

static void EESightPlayer(unsigned int inKind, int inOn, int inShared, EEPlayer* inPlayer, RTSTile* inTile, long inX, long inY)
{
    EESightScope theCapture(inKind, inPlayer, inTile, inX, inY);
    ee_units_tile_sight_player(EESightWorld(), (uint8_t)inOn, (uint8_t)inShared, EESightObject(inPlayer), EESightObject(inTile),
                               inOn ? inX : 0, inOn ? inY : 0);
}

// The world calls of the Rust tile sight (ee_units tile_sight_host.rs)
extern "C" int64_t ee_units_tile_sight_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD)
{
    switch (inOp) {
    case EE_UNITS_TILE_SIGHT_SHOWN: gDoppelgangerManager.TileShown(reinterpret_cast<EETile*>((uintptr_t)inA), (long)inC, (long)inD, reinterpret_cast<EEPlayer*>((uintptr_t)inB)); return 0;
    case EE_UNITS_TILE_SIGHT_HIDDEN: gDoppelgangerManager.TileHidden(reinterpret_cast<EETile*>((uintptr_t)inA), reinterpret_cast<EEPlayer*>((uintptr_t)inB)); return 0;
    case EE_UNITS_TILE_SIGHT_FIRST: reinterpret_cast<EEPlayer*>((uintptr_t)inA)->mSavedGroups.Reset(0x18, 1); return 0;
    case EE_UNITS_TILE_SIGHT_CELL_ON: gMiniMap->CountCellOn((long)inC, (long)inD); return 0;
    case EE_UNITS_TILE_SIGHT_REFERENCE: gWorld.mMap->mTerrainMesh.ReferenceTile((unsigned long)inC, (unsigned long)inD); return 0;
    case EE_UNITS_TILE_SIGHT_CELL_OFF: gMiniMap->CountCellOff((long)inC, (long)inD); return 0;
    case EE_UNITS_TILE_SIGHT_DEREFERENCE: gWorld.mMap->mTerrainMesh.DereferenceTile((unsigned long)inC, (unsigned long)inD); return 0;
    }
    return 0;
}

// Adds a player's sight to every tile within a radius (at most 32) of a point, showing them if the current player
// is that player or an ally both sharing line of sight
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer) { EESightCircle(kSightCircleAdd, 0, inPlayer, inX, inY, 0.0f, 0.0f, inRadius); }

// Removes a player's sight from every tile within a radius (at most 32) of a point
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer) { EESightCircle(kSightCircleRemove, 1, inPlayer, inX, inY, 0.0f, 0.0f, inRadius); }

// Moves a unit's sight: a step of at most one tile updates only the outer rings, a longer one redoes the whole circle
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer)
{
    EESightCircle(kSightCircleMove, 4, inPlayer, inOldX, inOldY, inNewX, inNewY, inRadius);
}

// Drops one of the player's sight references on the tile; the last one leaves it explored
void RTSTile::RemoveSight(EEPlayer* inPlayer) { EESightTile(kSightTileRemove, 0, this, inPlayer); }

// Adds a sight reference of the player to the tile; an unexplored tile becomes seen
void RTSTile::AddSight(EEPlayer* inPlayer) { EESightTile(kSightTileAdd, 1, this, inPlayer); }

// Removes the player's sight from a tile, and its allies' sight when line of sight is shared
void EEPlayer::RemoveSharedSight(RTSTile* inTile) { EESightPlayer(kSightPlayerRemoveShared, 0, 1, this, inTile, -1, -1); }

// Removes the player's and its sight-sharing allies' sight from the tile at a location
void EEPlayer::RemoveSharedSightAt(const EETileLocation& inLocation) { this->RemoveSharedSight(gWorld.mMap->GetTileAtLocation(inLocation)); }

// Removes only the player's own sight from a tile
void EEPlayer::RemoveOwnSight(RTSTile* inTile) { EESightPlayer(kSightPlayerRemoveOwn, 0, 0, this, inTile, -1, -1); }

// Removes only the player's own sight from the tile at a location
void EEPlayer::RemoveOwnSightAt(const EETileLocation& inLocation) { this->RemoveOwnSight(gWorld.mMap->GetTileAtLocation(inLocation)); }

// Reveals a tile to the player and, with shared line of sight, to every ally that shares it too
void EEPlayer::AddSharedSight(EETile* inTile, long inX, long inY) { EESightPlayer(kSightPlayerAddShared, 1, 1, this, inTile, inX, inY); }

// Reveals the tile at a map location to the player and its sight-sharing allies
void EEPlayer::AddSharedSightAt(const EETileLocation& inLocation) { this->AddSharedSight(gWorld.mMap->GetTileAtLocation(inLocation), inLocation.GetX(), inLocation.GetY()); }

// Reveals a tile to the player only (a first sighting adds to statistics series 0x18)
void EEPlayer::AddOwnSight(EETile* inTile, long inX, long inY) { EESightPlayer(kSightPlayerAddOwn, 1, 0, this, inTile, inX, inY); }

// Reveals the tile at a map location to the player only
void EEPlayer::AddOwnSightAt(const EETileLocation& inLocation) { this->AddOwnSight(gWorld.mMap->GetTileAtLocation(inLocation), inLocation.GetX(), inLocation.GetY()); }
