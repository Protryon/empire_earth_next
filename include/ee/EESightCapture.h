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

// The scope is inline over ee_game app sight_capture.rs (the depth, the rectangle, the hash and the record);
// EESightRecord (EEMap.cpp) hands it the world's map, its mesh and the tile's index in the map's tile array.
void EESightRecord(unsigned int inShape, unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inTileX,
                   long inTileY, unsigned long inValue, const float* inArgs);

class EESightScope {
public:
    // a tile's call (inX, inY: the coordinates the call takes, -1 when it takes none; inValue SetVisibility's)
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inX = -1, long inY = -1,
                 unsigned long inValue = 0)
        : mOuter(ee_app_sight_enter()), mShape(EE_APP_SIGHT_TILE), mKind(inKind), mPlayer(inPlayer), mTile(inTile),
          mArgs{0, 0, 0, 0, 0}, mTileX(inX), mTileY(inY), mValue(inValue) {}
    // a circle around (inX0, inY0), or a step from it to (inX1, inY1); inValue the map sight's shared flag
    EESightScope(unsigned int inKind, EEPlayer* inPlayer, float inX0, float inY0, float inX1, float inY1,
                 float inRadius, unsigned long inValue = 0)
        : mOuter(ee_app_sight_enter()), mShape(EE_APP_SIGHT_CIRCLE), mKind(inKind), mPlayer(inPlayer), mTile(NULL),
          mArgs{inX0, inY0, inX1, inY1, inRadius}, mTileX(-1), mTileY(-1), mValue(inValue) {}
    // the whole map
    EESightScope(unsigned int inKind, EEPlayer* inPlayer)
        : mOuter(ee_app_sight_enter()), mShape(EE_APP_SIGHT_WHOLE), mKind(inKind), mPlayer(inPlayer), mTile(NULL),
          mArgs{0, 0, 0, 0, 0}, mTileX(-1), mTileY(-1), mValue(0) {}
    ~EESightScope()
    {
        if (ee_app_sight_leave(this->mOuter))
            EESightRecord(this->mShape, this->mKind, this->mPlayer, this->mTile, this->mTileX, this->mTileY,
                          this->mValue, this->mArgs);
    }

private:
    bool mOuter;
    unsigned int mShape;
    unsigned int mKind;
    EEPlayer* mPlayer;
    const RTSTile* mTile;
    float mArgs[5];
    long mTileX;
    long mTileY;
    unsigned long mValue;
};

// EE-SIM's tick hook, every game minute: a kSightSnapshot record of the whole map
inline void EESightCaptureSnapshot()
{
    EESightScope theScope(kSightSnapshot, NULL);
}

#endif
