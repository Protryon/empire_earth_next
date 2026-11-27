// C:\EEX\Empire Earth\Calamities\EECalamityClass.h (RECONSTRUCTION; file name inferred). The behaviour of one
// kind of calamity (a row of db\dbcalamity.dat), shared by every active EECalamity of that kind. EECalamityClass.cpp
// is 0xadcd9.. (ctor 0xadcd9, vtable 0x43c6c4, 19 slots). Also declares the dbcalamity.dat record as the classes read
// it (EECalamityClass::Initialize 0xadd1c and the subclasses' Initialize).
// Virtual names are provisional (by behaviour; unknown ones by slot offset). Fields proven by Initialize 0xadd1c.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYCLASS_H
#define EE_EECALAMITYCLASS_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include "rust/game/gamecpp.h"

class EECalamity;
class EEContagionFunctor;
class EEPlayer;
class EESoundObject;
class EEComplexUnit;
class EEGraphicEffect;
class EEUnit;
class GE3DPoint;

// The last argument of EEComputeSoundParameters for a calamity's sound (provisional name). A named const, not a
// literal: every unit that uses it has its own .rdata copy (EECCAEExtraDamage 0x43c72c, EECCAEMindControl 0x43c790)
// where a 4.0f literal would be one shared __real COMDAT.
static const float kCalamitySoundFalloff = 4.0f;

// One record of db\dbcalamity.dat: Rust's EeCalamityData (ee_sim tech calamity_class.rs; the evidence for each field
// is there)
typedef EeCalamityData EECalamityData;

// New (port): the class's effects, sounds and numbers in one place for every class (ee_tech calamity_class.rs). The
// numbers are the Rust record EeCalamityClassFields the class derives from (the base members, then the union of the
// twenty subclasses' own). The subclasses are gone (calamities lane, round 2), and the three bases they derived from
// (EEGlobalCalamityClass, EELocationCalamityClass, EEUnitCalamityClass; round 4) with them: what a base decided is
// the kind's target (ee_tech calamity_class.rs target_kind), each per-class member below runs by tables keyed by
// mClassEnum, and the classes' world steps are the host's (EECalamityUpdate.cpp).
class EECalamityClass : public EeCalamityClassFields {
public:
    // 0xadcd9 (every number 0, no effect, sound or functor)
    EECalamityClass() : EeCalamityClassFields(), mGraphicEffect1(NULL), mGraphicEffect2(NULL), mGraphicEffect3(NULL), mGraphicEffect4(NULL), mSound(NULL), mSecondarySound(NULL), mFunctor(NULL) {  }
    // +0x00 (0xadd15): the class's own releases (its media steps)
    virtual ~EECalamityClass() { ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_DESTROY, NULL, false); }
    // +0x04 (Rust: ee/EECalamityUpdate.h)
    virtual bool Start(EECalamity* inCalamity) { return ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_START, 0.0f) != 0; }
    // +0x08
    virtual void Stop(EECalamity* inCalamity) { ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_STOP, 0.0f); }
    // +0x0c (base 0x1ad67e: none)
    virtual EEContagionFunctor* GetContagionFunctor() { return this->mFunctor; }
    // +0x10 (0xadceb: mCost)
    virtual long GetCost() { return this->mCost; }
    // +0x14: what the class is cast at (EE_CALAMITY_TARGET_*: 1 nothing, 2 a location, 3 a unit; its base's)
    long GetTargetKind() { return (long)ee_calamity_class_target_kind((int32_t)this->mClassEnum); }
    // +0x18: a unit class's (0xb712a): the id of the unit under the mouse, -1 if none; -1 for the others (0xadcef)
    virtual long GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool) { return ee_gamecpp_calamity_class_default_target(this); }
    // +0x1c (0xadcf5: mUnitSet)
    virtual unsigned long GetUnitSet() { return this->mUnitSet; }
    // +0x20 (body 0xadd1c, then the class's own)
    virtual void Initialize(EECalamityData* inData) { ee_gamecpp_calamity_class_initialize(this, reinterpret_cast<const EeCalamityData*>(inData)); }
    // +0x24
    virtual void Reset(EECalamity* inCalamity) { ee_gamecpp_calamity_class_reset(this, inCalamity); }
    // +0x28: EECalamity's ctor calls it with (the caster's player or player 0, true); the classes load their effects,
    // sounds and unit class media (the bool goes to EEGraphicEffect::Load)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand) { ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_LOAD, inPlayer, inIsOnDemand); }
    // +0x2c (EECalamity::PlaySound calls it)
    virtual void PlaySound(EECalamity* inCalamity) { ee_gamecpp_calamity_class_play_sound(this, inCalamity); }
    // +0x30 (0x2907a3: empty): the volcano's (0xb5137) makes units step aside from its site
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY) { ee_gamecpp_calamity_class_place(this, inUnit, inX, inY); }
    // +0x34 (~EECalamity calls it)
    virtual void UnloadMedia(EEPlayer* inPlayer) { ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_UNLOAD, inPlayer, false); }
    // +0x38 (Rust: ee/EECalamityUpdate.h)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime) { return ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_UPDATE, inElapsedTime); }
    // +0x3c: 1 = refused, 2 = protected by an area effect, 0 = fine (tile (inX, inY), when inCheck): the rules of the
    // class's base (a location's, a unit's or the special attack's; the global classes': 0)
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY, bool inCheck, EEUnit* inTargetUnit) { return ee_gamecpp_calamity_class_can_cast(this, inUnit, (const float(*)[3])&inLocation.mX, inX, inY, inCheck, inTargetUnit); }
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CanCastAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                   bool inCheck, EEUnit* inTargetUnit)
        { return CanCastAt(inUnit, const_cast<GE3DPoint&>(inLocation), inX, inY, inCheck, inTargetUnit); }
#endif
    // +0x48 (0xadde9): volume/pan/pitch of one of the calamity's two sounds from its location
    // (inVolume -1: the computed volume, else inVolume clamped to 0..1; inFalloff: EEComputeSoundParameters's)
    virtual void UpdateSound(EECalamity* inCalamity, EESoundObject* inSound, float inVolume, float inFalloff, bool inPrimary) { ee_gamecpp_calamity_class_update_sound(inCalamity, inSound, inVolume, inFalloff, inPrimary); }

    // a location class's (+0x4c): the shared rule over the class's radius, water flag, map area, space terrain and
    // placed unit class
    bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY) { return ee_gamecpp_calamity_class_valid_location(this, inUnit, inX, inY); }
    // a unit class's (+0x4c): the fire's and the plague's functor decides (CanInfect), the others' the shared rule
    bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit) { return ee_gamecpp_calamity_class_valid_target(this, inUnit, inTargetUnit); }
    // the volcano's (0xb4e74; EEUCBuilding calls it): a random location of a map tile between mMinimumRadius and
    // mMaximumRadius around inCenter, at the tile's height
    void GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation) { ee_gamecpp_calamity_class_random_location(this, (const float(*)[3])&inCenter.mX, (float(*)[3])&outLocation.mX); }

    // the class's effects (record +0x94..+0xa0), sounds (+0xd4, +0xd8) and contagion functor (owned), each set as
    // its class's Initialize set it
    EEGraphicEffect* mGraphicEffect1;
    EEGraphicEffect* mGraphicEffect2;
    EEGraphicEffect* mGraphicEffect3;
    EEGraphicEffect* mGraphicEffect4;
    EESoundObject* mSound;
    EESoundObject* mSecondarySound;
    EEContagionFunctor* mFunctor;
};

#endif
