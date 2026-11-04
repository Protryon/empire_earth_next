// C:\EEX\Empire Earth\Calamities\EECalamityClass.h (RECONSTRUCTION; file name inferred). The behaviour of one
// kind of calamity (a row of db\dbcalamity.dat), shared by every active EECalamity of that kind. EECalamityClass.cpp
// is 0xadcd9.. (ctor 0xadcd9, vtable 0x43c6c4, 19 slots). Also declares the dbcalamity.dat record as the classes read
// it (EECalamityClass::Initialize 0xadd1c and the subclasses' Initialize).
// Virtual names are provisional (by behaviour; unknown ones by slot offset). Fields proven by Initialize 0xadd1c.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYCLASS_H
#define EE_EECALAMITYCLASS_H

class EECalamity;
class EEContagionFunctor;
class EEPlayer;
class EESoundObject;
class EEComplexUnit;
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
    char mName[100];                        // +0x00 ("Undefined", "Global Warming", ... "Special Location")
    float mFloatParam1;                     // +0x64 (line of sight: the bonus)
    float mFloatParam2;                     // +0x68 (plague: functor argument)
    float mFloatParam3;                     // +0x6c (earthquake Initialize 0xb0765)
    float mFloatParam4;                     // +0x70 (earthquake Initialize)
    float mF74;                             // +0x74 (0 in every record)
    float mRange;                           // +0x78 -> EECalamityClass +0x1c (rounded)
    long mDatabaseID;                       // +0x7c (128..: the record's id)
    EECalamityKind mID;                     // +0x80 -> +0x18 (an enum, as the class member it fills)
    long mClassEnum;                        // +0x84 -> +0x08
    long mTargetKind;                       // +0x88
    long mLifeTime;                         // +0x8c -> +0x04 (as float)
    long mL90;                              // +0x90 (1 in every record)
    long mGraphicEffectID1;                 // +0x94 EEGraphicEffect table index (area effects: +0x28)
    long mGraphicEffectID2;                 // +0x98 (area effects: +0x2c)
    long mGraphicEffectID3;                 // +0x9c (mind control: +0x30)
    long mGraphicEffectID4;                 // +0xa0 (time warp: +0x34)
    long mLA4;                              // +0xa4 (0 in every record)
    long mLA8;                              // +0xa8 (0)
    long mLAC;                              // +0xac (0)
    long mUnitClassIndex;                   // +0xb0 (drop object: the unit class to drop)
    long mIntParam1;                        // +0xb4 (area effects: +0x44)
    long mIntParam2;                        // +0xb8 (area effects: +0x3c)
    long mIntParam3;                        // +0xbc (area effects: functor argument)
    long mIntParam4;                        // +0xc0 (fire: functor argument)
    long mIntParam5;                        // +0xc4 (earthquake Initialize)
    long mIntParam6;                        // +0xc8 (hurricane: the map area its location needs)
    bool mBCC;                              // +0xcc (1 in 31 of 36 records)
    bool mBoolParam1;                       // +0xcd (extra damage: functor argument)
    bool mBCE;                              // +0xce (0 in every record)
    bool mBCF;                              // +0xcf (0)
    bool mIsCastByAllSelected;              // +0xd0 -> +0x26
    bool mIsBlockedByImmunity;              // +0xd1 -> +0x25
    long mSoundID;                          // +0xd4 EESoundObject table index (area effects: +0x34)
    long mSecondarySoundID;                 // +0xd8 (plague: its second sound)
    long mCost;                             // +0xdc -> +0x20
    unsigned long mUnitSet;                 // +0xe0 -> +0x10
    long mNameID;                           // +0xe4 -> +0x14
    bool mBE8;                              // +0xe8 -> +0x24
};

class EECalamityClass {
public:
    EECalamityClass();                      // 0xadcd9
    virtual ~EECalamityClass();             // +0x00 (0xadd15)
    virtual bool Start(EECalamity* inCalamity) = 0;          // +0x04
    virtual void Stop(EECalamity* inCalamity) = 0;           // +0x08
    virtual EEContagionFunctor* GetContagionFunctor();       // +0x0c (base 0x1ad67e: none)
    virtual long GetCost();                 // +0x10 (0xadceb: mL20)
    virtual long GetTargetKind() = 0;       // +0x14
    virtual long GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool);  // +0x18 (0xadcef: -1)
    virtual unsigned long GetUnitSet();     // +0x1c (0xadcf5: mL10)
    virtual void Initialize(EECalamityData* inData) = 0;     // +0x20 (body 0xadd1c)
    virtual void Reset(EECalamity* inCalamity) = 0;          // +0x24
    // +0x28 (0x163d6f: empty): EECalamity's ctor calls it with (the caster's player or player 0, true); the area
    // effects load their media (the bool goes to EEGraphicEffect::Load)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);
    virtual void PlaySound(EECalamity* inCalamity);   // +0x2c (0x157269: empty; EECalamity::PlaySound calls it)
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY);       // (the volcano reads inUnit->mPlayer) +0x30 (0x2907a3: empty)
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x34 (0x157269: empty; ~EECalamity calls it)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime) = 0;  // +0x38
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

    float mLifeTime;                        // +0x04
    long mClassEnum;                        // +0x08
    unsigned long mClassFlags;              // +0x0c (0 here; each subclass ctor sets its own bit) (provisional name)
    unsigned long mUnitSet;                 // +0x10
    long mNameID;                           // +0x14
    EECalamityKind mID;                     // +0x18 (an enum: EECalamity::Save writes it as unsigned long and
                                            // EESETSETGameCalamity::Show 0x184db5 pushes it into a vector<long>, both
                                            // through a converted temporary)
    long mRange;                            // +0x1c
    long mCost;                             // +0x20
    bool mB24;                              // +0x24
    bool mIsBlockedByImmunity;              // +0x25 (area effects: functor mode 2 when set, else 3)
    bool mIsCastByAllSelected;              // +0x26
};

#endif
