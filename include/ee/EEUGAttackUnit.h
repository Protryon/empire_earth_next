// EEUGAttackUnit.h (RECONSTRUCTION; file name inferred: one header per goal class like ee/EEUGExplore.h; split out of
// ee/EEUnitGoal.h, no __FILE__ evidence either way). Requires lle/GE3DPoint.h (the member).
#ifndef EE_EEUGATTACKUNIT_H
#define EE_EEUGATTACKUNIT_H

#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;

// Attacking a unit (RTTI EEUGAttackUnit > EEUnitGoal; vtable 0x447380; sizeof 0x44; EEUGAttackUnit.cpp
// 0x21d2f4..0x21e0c1, __FILE__ 0x4c9338). Member names from its out-of-synch dump 0x21d5de and SetTarget's asserts
// (mAttacker); others (provisional name)
class EEUGAttackUnit : public EEUnitGoal {
public:
    // inline (COMDAT 0x23c5b2 after its first user, EEUnitGoal::CreateGoal)
    EEUGAttackUnit(bool inArg) : EEUnitGoal(kGoalAttackUnit, inArg), mTarget(NULL), mSidestepCount(0) {}
    // 0x21d337: the target referenced (or the carrier EEGetAttackPart gives); the attacker's player counts its attacks
    EEUGAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert);
    virtual ~EEUGAttackUnit();              // +0x00 0x21d4ab (??_G 0x21d48f)
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);  // +0x08 0x21d539
    virtual EEUnitGoal* Clone();            // +0x10 0x21d67e
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x21d5de
    virtual long GetOutOfSynchChecksum();   // +0x18 0x21d6b4
    virtual void GetDebugLine(UWideString& ioText);  // +0x1c 0x21d703
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x21d763
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 (ICF-folded 0x21d0b4: EEUGAttackGround's)
    virtual long GetMaxPathAttempts();      // +0x2c 0x21d8d7 (mL18 + 1)
    virtual long GetStartAction();          // +0x30 0x21d460 (0x7dd; EEUGAttackGround's folds into it)
    virtual void SetOwner(EEComplexUnit* inAttacker);    // +0x3c 0x21d9d3 (not referenced)
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 0x21d8dc
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x21d9dd
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);  // +0x50 0x21da63 (repairs the pointers)
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 (ICF-folded 0x21d1db: EEUGAttackGround's)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x21db0d
    virtual void SetPathAttempts(long inValue);  // +0x5c 0x21d47e (0 also clears mL3C)
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x21dc35
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c 0x21e01e
    virtual bool Suits(EEUnit* inUnit);       // +0x70 0x21e058
    virtual GE3DPoint* GetLocation();       // +0x78 0x21d45c (new slot) (provisional name)
    virtual void SetLocation(const GE3DPoint& inLocation);   // +0x7c 0x21d466 (new slot) (provisional name)
    void UpdateTargetTile();                // 0x21d5a3: the target's tile (provisional name)
    void RetargetGatePart(EEComplexUnit* inUnit);  // 0x21d5b3: a gate target becomes the unit EEGetAttackPart gives
    void SetTarget(EEUnit* inTarget);       // 0x21dbaa (asserts the target is targetable)
    bool IsGateClosed(EEComplexUnit* inUnit);  // 0x21d7e2 (Virtual64's 0x807)
    bool IsStillReachable(EEComplexUnit* inUnit);  // 0x21d857 (Virtual64's 0x1396)

    GE3DPoint mLocation;                    // +0x20 "The Actual Destination" (the target's tile centre)
    EEComplexUnit* mAttacker;               // +0x2c "The attacking unit" (not referenced; saved by id)
    EEUnit* GetTarget() const { return this->mTarget; }
    EEUnit* mTarget;                        // +0x30 "The target unit" (referenced; saved by id while alive)
    long mTargetTileX;                      // +0x34 "The last known target X" (saved)
    long mTargetTileY;                      // +0x38 (saved)
    unsigned long mSidestepCount;           // +0x3c (= 0; unsigned: 0x636b69 `cmp [edi+0x3c], 1; setb`)
    bool mIsConvert;                        // +0x40 "Is this a convert goal?" (saved from 0.238)

    static const UString sData;             // "EEUGAttackUnit-Data" (init chain 0x21d2f4)
};

#endif
