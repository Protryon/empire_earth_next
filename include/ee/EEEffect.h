// C:\EEX\Empire Earth\Graphics Thread\EEEffect.h (RECONSTRUCTION; file name inferred). The graphics thread's effects
// (RTTI EEEffect > UReferenceCounter, vtable 0x43f9fc; EEEffect.cpp 0x10251d..0x10428c, EEEffect2D.cpp, EEEffect3D.cpp
// and their sixty leaves): since the graphics + LLE cluster's second round Rust objects (ee_gfx effect_obj.rs: the
// lifetime and references, Load, Save, RepairPointers, the regions, the render queues, the 2D parts and every kind's
// operations over effect_rising.rs and effect_2d.rs). This class is the view of an effect's head (EeGfxEffectHead)
// that the C++ answers of EEEffectHost.cpp and the game's other code read; the manager (ee/EEEffectManager.h) makes
// and finds them.
#ifndef EE_EEEFFECT_H
#define EE_EEEFFECT_H

#include <stdint.h>
#include "lle/GE3DPoint.h"
#include "rust/render/gfx.h"            // effect_obj.rs, effect_state.rs
#include "rust/sim/world.h"             // EEEffectHandle (unit_fields.rs EeUnitsEffectHandle)

class EEUnit;
class EEGraphicEffect;
class EEGraphicObject;
class FSFile;
class GEModel;
class GETransformation;
class SESMInstanceNode;
template <class T> class U2DPoint;
template <class T> class U2DRectangle;

// An effect's handle (provisional name). A struct, not an integer: the "no effect" constant is a real .rdata object
// in every unit that uses it, compared and copied through memory.
// New (units round 12): the struct is the Rust record EeUnitsEffectHandle (rust/sim/world.h, a cbindgen rename: a
// unit's handles are fields of its record); its one member is mValue (EECalamity Load/Save use FSFile Read/Write
// on it; 32 bits).
inline bool operator==(const EEEffectHandle& inA, const EEEffectHandle& inB) { return inA.mValue == inB.mValue; }
inline bool operator!=(const EEEffectHandle& inA, const EEEffectHandle& inB) { return inA.mValue != inB.mValue; }

static const EEEffectHandle kNoEffectHandle = {0};

// An FSFile as ee_gfx effect_state.rs's typed reads and writes (EEEffectHost.cpp)
EeGfxEffectFile EEEffectFile(FSFile& inFile);
// 0x1487a3 (cdecl): the graphic's sound for the animation frame inFrame at inLocation (inStart: the frame changed);
// takes a reference to inSound, returns a referenced node (provisional name)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart);

class EEEffect {
public:
    EEEffect() = delete;                    // made by Rust only (ee_gfx_effect_new)
    EEEffect(const EEEffect&) = delete;
    EEEffect& operator=(const EEEffect&) = delete;

    void Reference() { ee_gfx_effect_reference(this->Head()); }
    void Release() { ee_gfx_effect_release(this->Head()); }   // the last reference destroys it
    long GetKind() const { return this->mRecord->kind; }
    EEEffectHandle GetHandle() const { return EEEffectHandle{this->mRecord->handle}; }
    // shown only to inPlayer, not saved (a player's confirmations, flares and goal markers; gamecpp fx_world.rs)
    void SetLocalTo(long inPlayer);
    EEUnit* GetUnit() const { return this->mUnit; }
    EEGraphicEffect* GetGraphicEffect() const { return this->mGraphicEffect; }
    // 0x102f96: the graphic effect's object (a random one of its kind without one; 0 without a graphic effect)
    // the 3D kinds' transformation and the 2D kinds' position (the record's)
    GETransformation& Transformation() { return *reinterpret_cast<GETransformation*>(&this->mRecord->transformation); }
    void SetTransformation(const GETransformation& inTransformation)
        { ee_gfx_effect_set_transformation(this->Head(), *reinterpret_cast<const EeGeTransformation*>(&inTransformation)); }
    // the weather storms' (EEEMStorm's): the tiles moved one step towards a new centre, the tiles' bounds, the
    // centre, the number of tiles (EEEffectHost.cpp)
    void Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds);
    void GetBounds(U2DRectangle<long>& outBounds);
    U2DPoint<long> GetCenterPoint() const;
    unsigned long GetTileCount() const { return ee_gfx_storm_tile_count(this->mRecord); }

    // the head (ee_gfx effect_obj.rs EeGfxEffectHead, in its order)
    EeGfxEffectRecord* mRecord;             // the plain state (effect_state.rs)
    EEUnit* mUnit;                          // referenced (its saved id while mUnitIsSavedID)
    EEGraphicEffect* mGraphicEffect;        // referenced and loaded
    GEModel* mModel;                        // the 2D kinds' shared plane model (referenced)
    EEGraphicObject* mGraphicObject;        // the kept graphic object (its media loaded)
    EEGraphicObject* mUnitModel;            // the additive pass's: its unit's model, loaded while it lives
    SESMInstanceNode* mSound;               // the animation sound; the rain's loop
    SESMInstanceNode* mRumble;              // the rain's thunder
    EEUnit* mTarget;                        // the tracer beam's target unit (referenced)
    GEModel* mVolcanoModel;                 // the volcano's model (referenced by its build)
    EEUnit* mFromUnit;                      // the marching line's end units (saved as their ids)
    EEUnit* mToUnit;
    uint32_t mReferences;
    bool mUnitIsSavedID;                    // the unit pointers hold Load's saved ids until RepairPointers
    bool mIs2D;

private:
    EeGfxEffectHead* Head() { return reinterpret_cast<EeGfxEffectHead*>(this); }
};

static_assert(sizeof(EEEffect) == sizeof(EeGfxEffectHead), "ee_gfx effect_obj.rs EeGfxEffectHead");
static_assert(sizeof(EEEffectHandle) == sizeof(uint32_t), "an effect handle is the record's u32");

#endif
