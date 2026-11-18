// New (port), TEST ONLY: TSGT, the tile sight's capture record (tools/sim_capture.py; the object-storage plan's
// lane P). Every sight function opens a scope; when the outermost one on the thread returns, one record holds its
// kind, the player, its arguments, the tile range it can reach and an FNV-1a of that range after the call (each
// tile's visibility words, the terrain mesh tile's reference count). Nested sight calls record nothing: the outer
// record's range covers what they changed. Off (no EE_SIM_CAPTURE), a scope is a thread-local count.
#ifndef EE_EESIGHTCAPTURE_H
#define EE_EESIGHTCAPTURE_H

class EEPlayer;
class RTSTile;

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

class EESightScope {
public:
    // a tile's call (inX, inY: the coordinates the call takes, -1 when it takes none; inValue SetVisibility's)
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inX = -1, long inY = -1,
                 unsigned long inValue = 0);
    // a circle around (inX0, inY0), or a step from it to (inX1, inY1); inValue the map sight's shared flag
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, float inX0, float inY0, float inX1, float inY1,
                 float inRadius, unsigned long inValue = 0);
    // the whole map
    EESightScope(unsigned int inKind, EEPlayer* inPlayer);
    ~EESightScope();

private:
    void Begin();
    bool mOuter;
    unsigned int mKind;
    EEPlayer* mPlayer;
    const RTSTile* mTile;
    float mArgs[5];
    long mTileX;
    long mTileY;
    unsigned long mValue;
    long mRect[4];              // left, right, bottom, top (inclusive); left > right: the whole map
};

// EE-SIM's TEST ONLY end sweep of the app profile (--app-sweep, at the game's end): EEMap's AddSight and
// RemoveSight for every player at three points (MAPS, TSGT), ExploreUnseen and FogSeen of the last player (TSGT),
// then two full cycles of the out-of-synch checksum's windows (MAPK)
void EESightCaptureSweep();
// EE-SIM's tick hook, every game minute: a kSightSnapshot record of the whole map
void EESightCaptureSnapshot();

#endif
