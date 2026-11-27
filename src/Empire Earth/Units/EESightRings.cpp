// C:\EEX\Empire Earth\Units\EESightRings.cpp (file name inferred: no __FILE__, no RTTI). The line-of-sight
// rings: 0x20834c..0x208c37 (unit map [C], between EEGroup.cpp and EEOrbital's range 0x208c37; edge: the globals
// chain): the reveal/hide helpers walk the tile offsets at each ceiling distance (0..32) from a unit's centre for a
// unit's player and the current player's mini map and terrain reference counts (EEComplexUnit.cpp calls
// EEAddSightCircle, EERemoveSightCircle and EEMoveSightCircle). Unit flags: the exe's module set (provisional names).
// port (the object-storage plan's lane P): the tile sight is Rust (ee_units tile_sight.rs over the world views):
// the circles, the ring steps and their table, RTSTile's AddSight and RemoveSight and EEPlayer's shared and own sight
// of a tile, each inside its TEST ONLY TSGT scope. What stays here: the forwarders (the world calls Rust makes are the
// sight host's, EEComplexUnit.cpp, and EEComplexUnit's sight bodies call the Rust entries directly: units round 14).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDoppelgangerManager.h"
#include "rust/game/gamecpp.h"
#include "rust/sim/units.h"

// Drops one of the player's sight references on the tile; the last one leaves it explored
void RTSTile::RemoveSight(EEPlayer* inPlayer) { ee_units_tile_sight_tile((uintptr_t)&gWorld, kSightTileRemove, 0, (uintptr_t)this, (uintptr_t)inPlayer); }

// Adds a sight reference of the player to the tile; an unexplored tile becomes seen
void RTSTile::AddSight(EEPlayer* inPlayer) { ee_units_tile_sight_tile((uintptr_t)&gWorld, kSightTileAdd, 1, (uintptr_t)this, (uintptr_t)inPlayer); }

// Removes the player's sight from a tile, and its allies' sight when line of sight is shared
void EEPlayer::RemoveSharedSight(RTSTile* inTile) { ee_units_tile_sight_player((uintptr_t)&gWorld, kSightPlayerRemoveShared, 0, 1, (uintptr_t)this, (uintptr_t)inTile, -1, -1); }

// Removes the player's and its sight-sharing allies' sight from the tile at a location
void EEPlayer::RemoveSharedSightAt(const EETileLocation& inLocation) { this->RemoveSharedSight(gWorld.mMap->GetTileAtLocation(inLocation)); }

// Removes only the player's own sight from a tile
void EEPlayer::RemoveOwnSight(RTSTile* inTile) { ee_units_tile_sight_player((uintptr_t)&gWorld, kSightPlayerRemoveOwn, 0, 0, (uintptr_t)this, (uintptr_t)inTile, -1, -1); }

// Removes only the player's own sight from the tile at a location
void EEPlayer::RemoveOwnSightAt(const EETileLocation& inLocation) { this->RemoveOwnSight(gWorld.mMap->GetTileAtLocation(inLocation)); }

// Reveals a tile to the player and, with shared line of sight, to every ally that shares it too
void EEPlayer::AddSharedSight(EETile* inTile, long inX, long inY) { ee_units_tile_sight_player((uintptr_t)&gWorld, kSightPlayerAddShared, 1, 1, (uintptr_t)this, (uintptr_t)inTile, inX, inY); }

// Reveals the tile at a map location to the player and its sight-sharing allies
void EEPlayer::AddSharedSightAt(const EETileLocation& inLocation) { this->AddSharedSight(gWorld.mMap->GetTileAtLocation(inLocation), inLocation.GetX(), inLocation.GetY()); }

// Reveals a tile to the player only (a first sighting adds to statistics series 0x18)
void EEPlayer::AddOwnSight(EETile* inTile, long inX, long inY) { ee_units_tile_sight_player((uintptr_t)&gWorld, kSightPlayerAddOwn, 1, 0, (uintptr_t)this, (uintptr_t)inTile, inX, inY); }

// Reveals the tile at a map location to the player only
void EEPlayer::AddOwnSightAt(const EETileLocation& inLocation) { this->AddOwnSight(gWorld.mMap->GetTileAtLocation(inLocation), inLocation.GetX(), inLocation.GetY()); }
