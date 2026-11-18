// C:\EEX\Empire Earth\Graphics Thread\EEEffect.h (RECONSTRUCTION; file name inferred). The graphics thread's effects
// (RTTI EEEffect > UReferenceCounter, vtable 0x43f9fc; EEEffect.cpp 0x10251d..0x10428c), since R4.4's twenty-fifth
// round one class for every kind: the original's bases EEEffect, EEEffect2D (vtable 0x43f9a4, EEEffect2D.cpp
// 0x10428c..0x1050fb) and EEEffect3D (vtable 0x440334, EEEffect3D.cpp 0x1050fb..0x1054ec) and their leaves (since
// moved: EEE3DRust and EEE2DRust, all deleted) are this class over its Rust record (ee_gfx effect_state.rs), each
// operation a call into Rust by the record's kind (ee_gfx effect_rising.rs for the 3D kinds, effect_2d.rs for the 2D
// ones) answered through EeGfxEffect3DHost or EeGfxEffect2DHost (EEEffectHost.cpp) and drawn through the raster table
// gEERustRaster (EERenderObject.cpp). Requires lle/UReferenceCounter.h.
#ifndef EE_EEEFFECT_H
#define EE_EEEFFECT_H

#include <map>
#include <deque>
#include <vector>
#include <string.h>
#include <new>
#include <stdint.h>
#include "lle/GE3DPoint.h"
#include "rust/render/gfx.h"            // effect_state.rs: the effects' records (RUST_PLAN R4.4)

class EEUnit;
class EEGraphicEffect;
class EEGraphicObject;
class EETile;
typedef EETile EEMapTile;
class FSFile;
class GE3DLine;
class GEVector;
class GETransformation;
class GETexture;
class GEModel;
class SESMInstanceNode;
template <class T> class U2DPoint;
template <class T> class U2DRectangle;

// An effect's handle (provisional name). A struct, not an integer: the "no effect" constant is a real .rdata object
// in every unit that uses it (EECCAEExtraDamage 0x43c728, EECCAEMindControl 0x43c78c, EEEffect.cpp 0x4411a8),
// compared and copied through memory (an integral const would fold to an immediate).
struct EEEffectHandle {
    bool operator==(const EEEffectHandle& inHandle) const { return this->mValue == inHandle.mValue; }
    bool operator!=(const EEEffectHandle& inHandle) const { return this->mValue != inHandle.mValue; }

    uint32_t mValue;                        // (EECalamity Load/Save use FSFile Read/Write on it; 32 bits, the record's)
};

static const EEEffectHandle kNoEffectHandle = {0};

class EEEffect;

// The effects' render queues, one map per pass: Queue's argument is the 5-element global array 0x4e9418
// (EERenderObject::QueueEffects 0x10bff5 passes it to every visible effect), keyed by the effect's texture
typedef std::map<GETexture*, std::deque<EEEffect*> > EEEffectQueue;

// TEST ONLY (tools/sim_capture.py EFST): the effects' random generator's seed
unsigned long EEEffectRandomSeed();

// An FSFile as ee_gfx effect_state.rs's typed reads and writes (EEEffect.cpp)
EeGfxEffectFile EEEffectFile(FSFile& inFile);
// The terrain mesh as the particles' move asks it (EEEffect.cpp)
EeGfxParticleGround EEParticleGround();
// The particles' move reads: the clock, the wind, the graphic effect's parameters, the effect's position
EeGfxParticleTick EEParticleTick(EEGraphicEffect* inEffect, const GE3DPoint& inPosition);
// the facts of an effect's graphic effect, unit and tick the Rust effects read (EEEffectHost.cpp)
EeGfxGraphicFacts EEEffectFacts(EEGraphicEffect* inEffect, EEUnit* inUnit);
// 0x1487a3 (cdecl): the graphic's sound for the animation frame inFrame at inLocation (inStart: the frame changed);
// takes a reference to inSound, returns a referenced node (provisional name)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart);
// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

// One sprite of a 2D effect (EEEffect2D::mParts' 0x24-byte element: at a tag point of the unit, or the effect's
// position). Names are provisional.
struct EEEffect2DPart {
    GE3DPoint mPosition;                    // +0x00 (saved)
    EEGraphicObject* mGraphicObject;        // +0x0c (LoadMedia when made, UnloadMedia by the effect)
    unsigned long mTag;                     // +0x10 (the tag point's index, 10000 for the effect's own position; saved)
    unsigned long mFrame;                   // +0x14 the animation frame (saved)
    unsigned long mDrawFrame;               // +0x18 (the queue's copy of mFrame)
    float mTime;                            // +0x1c the time into the frame (saved)
    bool mIsDone;                           // +0x20 (saved)
};

class EEEffect : public UReferenceCounter {
public:
    // New (port, R4.4): effects are allocated zeroed. The original's constructors leave members to SetUp, to the
    // first step or to a tag point that may not exist, and a save or the EFST capture wrote whatever the heap held;
    // zero-filled storage makes those values the same in every run.
    static void* operator new(size_t inSize) { return memset(::operator new(inSize), 0, inSize); }
    static void operator delete(void* inMemory) { ::operator delete(inMemory); }
    static const UString sData;             // "EEEffect-Data" (chain 0x10251d)

    // The constructors of the manager's entry points (each the original leaf's of inKind): on a unit (inKeepsUnit:
    // the light glow's; inTagPoint: the particle emitter's; inGoalIndex: the command confirmation's goal queue)
    EEEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit = false, long inTagPoint = 0,
             unsigned long inGoalIndex = 0);
    EEEffect(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);     // at a point
    // from a unit to a point (the shots, the beams, the command confirmation at a point)
    EEEffect(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind,
             unsigned long inGoalIndex);
    EEEffect(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind);     // between two units
    EEEffect(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind);
    EEEffect(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind);          // along a line
    // over a copy of the tiles, centred on them (the rain, the snow)
    EEEffect(const std::vector<U2DPoint<long> >& inTiles, EEGraphicEffect* inGraphicEffect, long inKind);
    EEEffect(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect, long inKind);   // the ripple
    explicit EEEffect(long inKind);         // for loading
    EEEffect(const EEEffect&) = delete;
    EEEffect& operator=(const EEEffect&) = delete;
    virtual ~EEEffect();                    // 0x102600 and the leaves': unloads, leaves its regions, dereferences

    long DrawPass2() { return this->Call(EeGfxEffect3DOp::DrawPass2); }
    long DrawPass3() { return this->Call(EeGfxEffect3DOp::DrawPass3); }
    long DrawPass4() { return this->Call(EeGfxEffect3DOp::DrawPass4); }
    long DrawPass5() { return this->Call(EeGfxEffect3DOp::DrawPass5); }
    long DrawPass6() { return this->Call(EeGfxEffect3DOp::DrawPass6); }
    long Queue(EEEffectQueue* inQueues) { return this->Call(EeGfxEffect3DOp::Queue, inQueues); }
    long SetUp() { return this->Call(EeGfxEffect3DOp::SetUp); }
    long Step();
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long RepairPointers();
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    // releases what the destructor releases without destroying the effect (0x102e61 and the leaves')
    void ReleaseResources();
    // the effect on inOldUnit moves to inNewUnit unless it keeps its unit (0x104250; the tracer beam's target, the
    // marching line's ends; the farm's is empty)
    void ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);
    // the farm's: rebuilt when the changed terrain rectangle overlaps its field (EEE3DFarm::ElevationChanged)
    void ElevationChanged(long inLeft, long inBottom, long inRight, long inTop);
    // to the map region under the translation or the point (0x104175, 0x10409b; the marching line's regions along it)
    void MoveToTransformation(const GETransformation& inTransformation);
    void MoveToPoint(const GE3DPoint& inLocation);
    void MoveToRegion(float inX, float inY);
    void Release() { this->Dereference(); }
    void SetIsLocalOnly(bool inValue) { this->mIsLocalOnly = inValue; }
    void SetPlayer(long inValue) { this->mPlayer = inValue; }
    void SetIsSaved(bool inValue) { this->mIsSaved = inValue; }
    EEUnit* GetUnit() const { return this->mUnit; }
    EEGraphicEffect* GetGraphicEffect() const { return this->mGraphicEffect; }
    EEGraphicObject* GetGraphicObject();    // 0x102f96: the graphic effect's object (0 without one)
    // 0x102759: the unit's location (its complex unit's position, else its tile centre at its tile's height, else 0)
    const GE3DPoint& GetUnitLocation();
    // 0x102f8b: the effects' random number; a value around inCenter, up to half of inSpread either way
    static float GetRandom();

    // the 2D kinds' position and the 3D kinds' transformation, each then its region
    void SetPosition(const GE3DPoint& inPosition);
    void SetTransformation(const GETransformation& inTransformation);
    // the 3D kinds' tile test (0x105429: the current player sees it; the energy wall's and the farm's own)
    bool IsTileVisible(EEMapTile* inTile);
    // the base queues (EEEffect::Queue 0x102807: another player's local effect; with inInView EEEffect2D::Queue
    // 0x104332 or EEEffect3D::Queue 0x10513e: off the map, unseen or out of view): 10 to skip, else 0
    long BaseQueue(bool inInView);
    // the 2D kinds' parts: one per tag point of the graphic effect's kind on the unit's model (or one at the unit),
    // raised above the terrain by inOffset; the location and the parts' radius + height (0x104623)
    void CreateParts(GE3DPoint& outPosition, float& outHeight, float inOffset);
    // the icon's and goal's (EEE3DIcon::SetLocation, 0xf2637): a fixed one moves to inLocation; others ignore it
    void SetLocation(const GE3DPoint& inLocation);
    // the marching line's ends, set unless the end follows a unit; its effect regions along it from inFrom to inTo but
    // its ends' and its own, left again
    bool SetFrom(const GE3DPoint& inPoint);
    bool SetTo(const GE3DPoint& inPoint);
    void AddRegions(const GE3DPoint& inFrom, const GE3DPoint& inTo);
    void RemoveRegions();
    // the weather storms' (EEEMStorm's): the tiles moved one step towards a new centre (EEE3DStorm::Move, 0xfb252),
    // the tiles' bounds (0xfb6b4), the centre, the number of tiles
    void Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds);
    void GetBounds(U2DRectangle<long>& outBounds);
    U2DPoint<long> GetCenterPoint() const;
    unsigned long GetTileCount() const { return ee_gfx_storm_tile_count(this->mRecord); }

    // inArg: a constructor's unit, point or line (the multi-effect's linked effects are made through it)
    long Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues = NULL, const EeGfxEffectFile* inFile = NULL, long inBase = 0,
              unsigned long inMajorVersion = 0, unsigned long inMinorVersion = 0, void* inArg = NULL);

    // The effect's plain state is a Rust record (ee_gfx effect_state.rs), made here and freed by the destructor; the
    // members below are references into it.
    EeGfxEffectRecord* mRecord = ee_gfx_effect_record_new(0);
    EEUnit* mUnit = NULL;                   // +0x08 (referenced; saved as its id)
    EEGraphicEffect* mGraphicEffect = NULL; // +0x0c (referenced; Load/Unload)
    // New (port): set while the effect's unit pointers hold Load's saved unit ids (mUnit, the marching line's ends),
    // cleared by RepairPointers. A load that fails before the repair (a corrupt save, a scenario this build cannot
    // read) tears its effects down with the ids in place: the destructor and ReleaseResources then drop the ids
    // instead of dereferencing them (the original's destructor dereferenced the id and faulted)
    bool mUnitIsSavedID = false;
    int32_t& mKind = mRecord->kind;         // +0x18
    EEEffectHandle& mHandle = *reinterpret_cast<EEEffectHandle*>(&mRecord->handle);   // +0x1c
    int32_t& mPlayer = mRecord->player;     // +0x20 (compared with the current player)
    int32_t& mTileX = mRecord->tile_x;      // +0x24 (the region: -1 on none)
    int32_t& mTileY = mRecord->tile_y;      // +0x28
    bool& mKeepsUnit = mRecord->keeps_unit; // +0x2c (set: ReplaceUnit keeps the unit)
    bool& mIsLocalOnly = mRecord->local_only;   // +0x2d
    bool& mIsSaved = mRecord->saved;        // +0x2e
    GETransformation& mTransformation = *reinterpret_cast<GETransformation*>(&this->mRecord->transformation);  // 3D
    GE3DPoint& mPosition = *reinterpret_cast<GE3DPoint*>(&this->mRecord->position);   // 2D
    float& mBoundingRadius = this->mRecord->bounding_radius;
    bool mIs2D = false;                     // a 2D kind (ee_gfx effect_kinds.rs)
    std::vector<EEEffect2DPart> mParts;     // the 2D kinds' sprites
    GEModel* mModel = NULL;                 // the 2D kinds' shared plane model (referenced)
    EEGraphicObject* mGraphicObject = NULL; // the kept graphic object (loaded by SetUp, unloaded by the destructor)
    EEGraphicObject* mUnitModel = NULL;     // the additive pass's: its unit's model, loaded while it lives
    SESMInstanceNode* mSound = NULL;        // the animation sound; the rain's loop
    SESMInstanceNode* mRumble = NULL;       // the rain's thunder
    EEUnit* mTarget = NULL;                 // the tracer beam's target unit (referenced; -1 from Load to RepairPointers)
    GEModel* mVolcanoModel = NULL;          // the volcano's model (referenced by its build)
    EEUnit* mFromUnit = NULL;               // the marching line's end units (saved as their ids; none in practice)
    EEUnit* mToUnit = NULL;
    std::vector<U2DPoint<long> > mRegions;  // the marching line's effect regions

private:
    // the original base constructor's: the unit and graphic effect referenced, the kind, the media load; then a 2D
    // kind's shared plane model referenced
    void Attach(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit);
    void Begin(long inKind);
    // the point constructor's part after Attach (the destruction smoke's on a unit too)
    void PlaceAt(const GE3DPoint& inLocation);
    // the base's part of the destructor and ReleaseResources (the graphic effect, the region, the unit)
    void ReleaseBase();
    // the 2D particle's (0x2e) start at a point, moving along inVelocity when given
    void StartParticle(const GE3DPoint& inPosition, const GEVector* inVelocity);
};

static_assert(sizeof(EEEffectHandle) == sizeof(uint32_t), "an effect handle is the record's u32");

#endif
