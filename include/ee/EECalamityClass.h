// C:\EEX\Empire Earth\Calamities\EECalamityClass.h (RECONSTRUCTION; file name inferred). The behaviour of one
// kind of calamity (a row of db\dbcalamity.dat), shared by every active EECalamity of that kind. EECalamityClass.cpp
// is 0xadcd9.. (ctor 0xadcd9, vtable 0x43c6c4, 19 slots). Also declares the dbcalamity.dat record as the classes read
// it (EECalamityClass::Initialize 0xadd1c and the subclasses' Initialize).
// Virtual names are provisional (by behaviour; unknown ones by slot offset). Fields proven by Initialize 0xadd1c.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYCLASS_H
#define EE_EECALAMITYCLASS_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include "rust/ee_tech.h"   // EeCalamityClassFields, EeCalamityClassMedia (ee_tech calamity_class.rs)

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

// One record of db\dbcalamity.dat (provisional name). Gap-free, from the file itself (36 records of 0xec bytes):
// the name, then floats, ids and flags. Field names
// are provisional unless a reader names them.
// what EECalamityData +0x80 / EECalamityClass +0x18 hold (an enum: see mL18; values not decoded, provisional name)
enum EECalamityKind {
    kCalamityKindNone = -1
};

struct EECalamityData {
    char mLabel[100];                       // +0x00 ("Undefined", "Global Warming", ... "Special Location")
    float mFloatParam1;                     // +0x64 (line of sight: the bonus)
    float mFloatParam2;                     // +0x68 (plague: functor argument)
    float mQuakeMinimumStrength;            // +0x6c (earthquake Initialize 0xb0765)
    float mQuakeStrength;                   // +0x70 (earthquake Initialize)
    float mUnused1;                         // +0x74 (0 in every record)
    float mRange;                           // +0x78 -> EECalamityClass +0x1c (rounded)
    int32_t mUnusedDatabaseID;              // +0x7c (128..: the record's id)
    EECalamityKind mID;                     // +0x80 -> +0x18 (an enum, as the class member it fills)
    int32_t mClassEnum;                     // +0x84 -> +0x08
    int32_t mUnusedTargetKind;              // +0x88
    int32_t mLifeTime;                      // +0x8c -> +0x04 (as float)
    int32_t mUnused2;                       // +0x90 (1 in every record)
    int32_t mGraphicEffectID1;              // +0x94 EEGraphicEffect table index (area effects: +0x28)
    int32_t mGraphicEffectID2;              // +0x98 (area effects: +0x2c)
    int32_t mGraphicEffectID3;              // +0x9c (mind control: +0x30)
    int32_t mGraphicEffectID4;              // +0xa0 (time warp: +0x34)
    int32_t mUnused3;                       // +0xa4 (0 in every record)
    int32_t mUnused4;                       // +0xa8 (0)
    int32_t mUnused5;                       // +0xac (0)
    int32_t mUnitClassIndex;                // +0xb0 (drop object: the unit class to drop)
    int32_t mIntParam1;                     // +0xb4 (area effects: +0x44)
    int32_t mIntParam2;                     // +0xb8 (area effects: +0x3c)
    int32_t mIntParam3;                     // +0xbc (area effects: functor argument)
    int32_t mIntParam4;                     // +0xc0 (fire: functor argument)
    int32_t mIntParam5;                     // +0xc4 (earthquake Initialize)
    int32_t mIntParam6;                     // +0xc8 (hurricane: the map area its location needs)
    bool mUnused6;                          // +0xcc (1 in 31 of 36 records)
    bool mBoolParam1;                       // +0xcd (extra damage: functor argument)
    bool mUnused7;                          // +0xce (0 in every record)
    bool mUnused8;                          // +0xcf (0)
    bool mIsCastByAllSelected;              // +0xd0 -> +0x26
    bool mIsBlockedByImmunity;              // +0xd1 -> +0x25
    int32_t mSoundID;                       // +0xd4 EESoundObject table index (area effects: +0x34)
    int32_t mSecondarySoundID;              // +0xd8 (plague: its second sound)
    int32_t mCost;                          // +0xdc -> +0x20
    uint32_t mUnitSet;                      // +0xe0 -> +0x10
    int32_t mNameID;                        // +0xe4 -> +0x14
    bool mUnusedClassFlag;                  // +0xe8 -> +0x24
};

static_assert(sizeof(EECalamityData) == 236, "on-disk record: 236 bytes on every target");

// New (port): the class's effects, sounds and numbers in one place for every class (ee_tech calamity_class.rs). The
// numbers are the Rust record EeCalamityClassFields the class derives from (the base members, then the union of the
// twenty subclasses' own). The subclasses are gone (calamities lane, round 2): the manager makes one of the three
// bases by the kind's target (ee_tech calamity_class.rs target_kind), each per-class virtual below is the base's, run
// by tables keyed by mClassEnum, and the classes' world steps are the host's (EECalamityUpdate.cpp).
class EECalamityClass : public EeCalamityClassFields {
public:
    EECalamityClass();                      // 0xadcd9 (every number 0, no effect, sound or functor)
    virtual ~EECalamityClass();             // +0x00 (0xadd15): the class's own releases (its media steps)
    virtual bool Start(EECalamity* inCalamity);              // +0x04 (Rust: ee/EECalamityUpdate.h)
    virtual void Stop(EECalamity* inCalamity);               // +0x08
    virtual EEContagionFunctor* GetContagionFunctor();       // +0x0c (base 0x1ad67e: none)
    virtual long GetCost();                 // +0x10 (0xadceb: mCost)
    virtual long GetTargetKind() = 0;       // +0x14
    virtual long GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool);  // +0x18 (0xadcef: -1)
    virtual unsigned long GetUnitSet();     // +0x1c (0xadcf5: mUnitSet)
    virtual void Initialize(EECalamityData* inData);         // +0x20 (body 0xadd1c, then the class's own)
    virtual void Reset(EECalamity* inCalamity);              // +0x24
    // +0x28: EECalamity's ctor calls it with (the caster's player or player 0, true); the classes load their effects,
    // sounds and unit class media (the bool goes to EEGraphicEffect::Load)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);
    virtual void PlaySound(EECalamity* inCalamity);   // +0x2c (EECalamity::PlaySound calls it)
    // +0x30 (0x2907a3: empty): the volcano's (0xb5137) makes units step aside from its site
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY);
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x34 (~EECalamity calls it)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);  // +0x38 (Rust: ee/EECalamityUpdate.h)
    // +0x3c (0xadec7): 1 = refused, 2 = protected by an area effect, 0 = fine (tile (inX, inY), when inCheck)
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY, bool inCheck,
                           EEUnit* inTargetUnit);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CanCastAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                   bool inCheck, EEUnit* inTargetUnit)
        { return CanCastAt(inUnit, const_cast<GE3DPoint&>(inLocation), inX, inY, inCheck, inTargetUnit); }
#endif
    // +0x40 (0xade97): Virtual3C(0, GE3DPoint(), inX, inY, inCheck, inTargetUnit)
    virtual long CanCast(EEUnit* inTargetUnit, unsigned long inX, unsigned long inY, bool inCheck);
    // +0x44 (0xadd8a): advances inCalamity's clock; 7 once its life time is over, else 0. Ticks the contagion helper.
    virtual long UpdateTimeAlive(EECalamity* inCalamity, float inElapsedTime, long inHelperID);
    // +0x48 (0xadde9): volume/pan/pitch of one of the calamity's two sounds from its location
    // (inVolume -1: the computed volume, else inVolume clamped to 0..1; inFalloff: EEComputeSoundParameters's)
    virtual void UpdateSound(EECalamity* inCalamity, EESoundObject* inSound, float inVolume, float inFalloff,
                             bool inPrimary);

    // New (port): what the Rust tables read of the class's effects and sounds (EeCalamityClassMedia)
    void GetMedia(EeCalamityClassMedia& outMedia);

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
