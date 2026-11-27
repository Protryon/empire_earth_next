// New (port), TEST ONLY: TSGT, the tile sight's capture record (tools/sim_capture.py; the object-storage plan's
// lane P). Every sight function opens a scope; when the outermost one on the thread returns, one record holds its
// kind, the player, its arguments, the tile range it can reach and an FNV-1a of that range after the call (each
// tile's visibility words, the terrain mesh tile's reference count). Nested sight calls record nothing: the outer
// record's range covers what they changed. Off (no EE_SIM_CAPTURE), a scope is a thread-local count.
#ifndef EE_EESIGHTCAPTURE_H
#define EE_EESIGHTCAPTURE_H

class EEPlayer;
class RTSTile;

#include "rust/game/sight_capture.h"   // ee_app_sight_* (ee_game sight_capture.rs)

enum {
    kSightTileAdd = 1,          // RTSTile::AddSight(player)
    kSightTileRemove,           // RTSTile::RemoveSight(player)
    kSightTileSet,              // RTSTile::SetVisibility(value, player)
    kSightPlayerAddShared,      // EEPlayer::AddSharedSight(tile, x, y)
    kSightPlayerRemoveShared,   // EEPlayer::RemoveSharedSight(tile)
    kSightPlayerAddOwn,         // EEPlayer::AddOwnSight(tile, x, y)
    kSightPlayerRemoveOwn,      // EEPlayer::RemoveOwnSight(tile)
    kSightMapAdd,               // EEMap::AddSight(player, x, y, radius, shared)
    kSightMapRemove,            // EEMap::RemoveSight(player, x, y, radius, shared)
    kSightCircleAdd,            // EEAddSightCircle(x, y, radius, player)
    kSightCircleRemove,         // EERemoveSightCircle(x, y, radius, player)
    kSightRingAdd,              // EEAddSightRingStep(old x, old y, new x, new y, radius, player)
    kSightRingRemove,           // EERemoveSightRingStep(old x, old y, new x, new y, radius, player)
    kSightCircleMove,           // EEMoveSightCircle(old x, old y, new x, new y, radius, player)
    kSightFogSeen,              // RTSMap::FogSeen(player): the whole map
    kSightExploreUnseen,        // RTSMap::ExploreUnseen(player): the whole map
    kSightPrepareSave,          // EEMap::PrepareSave(): the whole map
    kSightRevealMap,            // EEWorld's reveal of the map at a game's start: the whole map
    kSightSnapshot,             // EE-SIM's tick hook every game minute: the whole map (no call)
};

// The scope is inline over ee_game sight_capture.rs (the depth, the open scopes' records, the rectangle, the hash and
// the record, with the world's map, its mesh and the tile's index in the map's tile array: gamecpp map.rs)
class EESightScope {
public:
    // a tile's call (inX, inY: the coordinates the call takes, -1 when it takes none; inValue SetVisibility's)
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inX = -1, long inY = -1,
                 unsigned long inValue = 0)
        { ee_app_sight_open_tile(inKind, inPlayer, inTile, inX, inY, inValue); }
    // a circle around (inX0, inY0), or a step from it to (inX1, inY1); inValue the map sight's shared flag
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, float inX0, float inY0, float inX1, float inY1,
                 float inRadius, unsigned long inValue = 0)
        { ee_app_sight_open_circle(inKind, inPlayer, inX0, inY0, inX1, inY1, inRadius, inValue); }
    // the whole map
    EESightScope(unsigned int inKind, EEPlayer* inPlayer) { ee_app_sight_open_whole(inKind, inPlayer); }
    ~EESightScope() { ee_app_sight_close(); }
    EESightScope(const EESightScope&) = delete;
};

#endif
