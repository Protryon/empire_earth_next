// EEUGAttackGround.h (RECONSTRUCTION; file name inferred: one header per goal class like ee/EEUGExplore.h; split out of
// ee/EEUnitGoal.h, no __FILE__ evidence either way). Requires lle/GE3DPoint.h (the member), ee/EEUnit.h (the inline
// Virtual70).
#ifndef EE_EEUGATTACKGROUND_H
#define EE_EEUGATTACKGROUND_H

#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class FSFile;
class GE3DPoint;
class UWideString;

// Attacking the ground at a location (RTTI EEUGAttackGround > EEUnitGoal; vtable 0x43cbe8; sizeof 0x2c). Its ctor is
// inline (out-of-line COMDAT 0xb2a4b in EECCMeteorStorm.cpp, its first user); its out-of-line slots are around
// 0x21ceaf (EEUGAttackGround.cpp). Names are provisional.
class EEUGAttackGround : public EEUnitGoal {
public:
    EEUGAttackGround(const GE3DPoint& inTarget, bool inArg) : EEUnitGoal(kGoalAttackGround, inArg) { this->mTarget = inTarget; }
    // inline (COMDAT 0x23c589 after its first user, EEUnitGoal::CreateGoal)
    EEUGAttackGround(bool inArg) : EEUnitGoal(kGoalAttackGround, inArg) {}
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);  // +0x08 0x21ceaf
    virtual EEUnitGoal* Clone();            // +0x10 0x21cf93
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x21cfc1
    virtual long GetOutOfSynchChecksum();   // +0x18 0x21cfee
    virtual void GetDebugLine(UWideString& ioText);  // +0x1c 0x21d016
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x21d065
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 0x21d0b4
    virtual long GetStartAction();          // +0x30 0x21d460
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 0x21d0cc
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x21d1b2
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 0x21d1db
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x21d1f9
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x21d222
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c (ICF-folded 0x2907a3: empty)
    // +0x70 (inline 0xb2aa7): inUnit's class has +0xf8 set
    virtual bool Suits(EEUnit* inUnit);

    GE3DPoint mTarget;                      // +0x20

    static const UString sData;             // "EEUGAttackGround-Data" (init chain 0x21ce6c)
};

#endif
