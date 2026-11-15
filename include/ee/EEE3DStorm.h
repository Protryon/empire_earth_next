// C:\EEX\Empire Earth\Graphics Thread\EEE3DStorm.h (RECONSTRUCTION; file name inferred).
// The abstract weather effect over a list of tiles, the base of EEE3DRain and EEE3DSnow (vtable 0x440e44; RTTI
// EEE3DStorm > EEEffect3D; EEE3DStorm.cpp 0xfb088..0xfb9f6: a map's init chain, the ctors .. Save, the map's tree
// COMDATs; no key chain). Names are provisional. Requires ee/EEEffect3D.h, lle/U2DRectangle.h,
// ee/EETileCoordinate.h, lle/U2DPoint.h, <vector>.
// The storm's state, its tiles and its drops or flakes are the effect's record (ee_gfx effect_storm.rs: the tiles,
// the centre, the move, the bounds, the set-up, the drops' spawn and fall, the thunder timer, the respawn after a
// load, Load and Save); the rain and the snow keep their sounds, their models and their draws.
#ifndef EE_EEE3DSTORM_H
#define EE_EEE3DSTORM_H

#include <vector>
#include "rust/ee_gfx.h"

class GEModel;
template <class T> class U2DRectangle;

// Free helpers in another unit (0x241c2d.. range; provisional names): the 8-way direction from (inX1, inY1) towards
// (inX2, inY2) (UMArcTan2) and the tile inDistance steps from inTile in that direction.
long EEGetTileDirection(long inX1, long inY1, long inX2, long inY2);  // 0x2451df
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance);  // 0x248ecc

class EEE3DStorm : public EEEffect3D {
public:
    // 0xfb0c8 (no unit: the effect covers inTiles)
    EEE3DStorm(long inKind, const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect);
    EEE3DStorm(long inKind);                // 0xfb191
    virtual ~EEE3DStorm();                  // +0x00 0xfb217 (??_G 0xfb175)
    // +0x24 (the rain's 0xf76da, the snow's 0xf9e42): copies the drops into the draw list, render queue 4
    virtual long Queue(EEEffectQueue* inQueues);
    // +0x30 (the snow's 0xfa291, the rain's folded into it): centre, transformation, radius
    virtual long SetUp();
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xfb756
    // +0x3c (the rain's 0xf7d0f, the snow's 0xfa5f3): the drops saved as a count are respawned
    virtual long RepairPointers();
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xfb83f
    // +0x4c (the snow's 0xfa70d; the rain's 0xf7e3f after its sounds): the drops, the age (7: over)
    virtual long Step();
    // +0x58 0xfb252: move the tiles towards a new centre (each one tile step towards its new place when that tile
    // exists), then the new mean centre, the transformation there, the bounds and the radius (provisional name)
    // outBounds: the storm's mArea (EEEMStorm +0x28 passed straight at 0x12ebfe)
    virtual void Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds);
    virtual void GetBounds(U2DRectangle<long>& outBounds);   // +0x60 0xfb6b4: the tiles' bounding box (EEEMStorm mArea, 0x12e364)
    virtual void Virtual64();               // +0x64 0xfb72e: the graphic effect's parameters

    void GetCenter();                       // 0xfb500: mCenter = the tiles' mean; the transformation there
    U2DPoint<long> GetCenterPoint() const   // mCenter
        { return U2DPoint<long>(this->mRecord->storm.center_x, this->mRecord->storm.center_y); }
    unsigned long GetTileCount() const { return ee_gfx_storm_tile_count(this->mRecord); }   // mTiles.size()
    void BeginDraw();                       // the draw passes' render state (alpha reference, blending, fog)

    std::vector<EeGfxStormDraw> mDraws;     // the drops to draw this frame (Queue)
    GEModel* mModel;                        // +0x90 (dereferenced by the dtor)
    float& mFallSpeed = this->mRecord->storm.fall_speed;       // +0x94 (the effect's mF28)
    float& mSpawnHeight = this->mRecord->storm.spawn_height;   // +0x98 (the effect's mF2C)
    float& mDensity = this->mRecord->storm.density;            // +0x9c (the effect's mF30)
    float& mIntensity = this->mRecord->storm.intensity;        // +0xa0 (1; saved)
    uint32_t& mDuration = this->mRecord->storm.duration;       // +0xa4 (5000; the effect's duration)
    float& mElapsedTime = this->mRecord->storm.elapsed;        // +0xa8 (saved)
    bool& mIsRestored = this->mRecord->storm.restored;         // +0xac
    bool& mIsSustained = this->mRecord->storm.sustained;       // +0xad (saved)
};

#endif
