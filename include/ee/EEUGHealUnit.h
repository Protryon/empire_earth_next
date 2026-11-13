// EEUGHealUnit.h (RECONSTRUCTION; file name inferred: one header per goal class like ee/EEUGExplore.h; split out of
// ee/EEUnitGoal.h, no __FILE__ evidence either way).
#ifndef EE_EEUGHEALUNIT_H
#define EE_EEUGHEALUNIT_H

#include "ee/EEUGLocation.h"

class EEComplexUnit;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;

// Healing a unit (RTTI EEUGHealUnit > EEUGLocation; vtable 0x447630; ctor 0x22531b: EEUGLocation at inTarget's
// position, kind 0xf, keeps a reference; sizeof 0x40: `new 0x40` in EEMHealUnit::Execute). Names are provisional
class EEUGHealUnit : public EEUGLocation {
public:
    EEUGHealUnit(EEComplexUnit* inTarget, bool inArg);   // 0x22531b
    EEUGHealUnit(bool inArg);               // 0x2252d0 (EEUnitGoal::CreateGoal)
    virtual ~EEUGHealUnit();                // +0x00 0x225378 (??_G 0x2252ff)
    virtual EEUnitGoal* Clone();            // +0x10 0x2253a5
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x2253d2
    virtual long GetOutOfSynchChecksum();   // +0x18 0x225415
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x225432
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 0x2254b1 (the healee's position)
    virtual long GetStartAction();          // +0x30 0x2252f9 (0x3e82)
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 0x225534
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x225595
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);  // +0x50 0x2255e3 (repairs the healee)
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 (ICF-folded 0x157269: empty)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x22561e
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x22569f
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c 0x225828
    virtual bool Suits(EEUnit* inUnit);       // +0x70 0x22583d
    virtual void Work(EEComplexUnit* inUnit);        // +0x74 0x225789: heals the healee every second
    void SetTarget(EEComplexUnit* inTarget);   // 0x22567a (provisional name)
    // out of line, ICF-folded into 0x12c734 (EECCAreaEffect::GetContagionFunctor, `mov eax,[ecx+0x38]`): mTarget; the
    // helicopter's Virtual10 0x558da4 calls it (provisional name)
    EEComplexUnit* GetTarget();

    EEComplexUnit* mTarget;                 // +0x38 "Healee unit" (referenced; its id between Load and Virtual50)
    unsigned long mElapsedTime;             // +0x3c "Elasped Time in Milliseconds" (= 0)

    static const UString sData;             // "EEUGHealUnit-StringID" (init chain 0x22528d)
};

#endif
