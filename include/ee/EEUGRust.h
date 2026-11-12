// New (port; docs/RUST_PLAN.md R5b.4, the sixteenth round): a goal whose class moved whole into Rust. The object is
// the EEUnitGoal the queues, the unit's mGoalState and the save hold (its base fields, its vtable); its own state and
// bodies are Rust's (rust/crates/ee_units/src/patrol.rs for kGoalPatrol), every virtual one call. Replaces
// EEUGPatrol (0x227de8..0x2283bc: its class and file are deleted; EEPlayer's SetPatrolGroup 0x563659 with it).
#ifndef EE_EEUGRUST_H
#define EE_EEUGRUST_H

#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class EEPPatrolGroup;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;

class EEUGRust : public EEUnitGoal {
public:
    // a patrol (kGoalPatrol): a new empty patrol group, no point (EEUGPatrol(inArg)); or an existing group and the
    // point to start at, both referenced (EEUGPatrol(inGroup, inTarget, inArg))
    static EEUGRust* NewPatrol(bool inArg);
    static EEUGRust* NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg);
    // the patrol's group, its current point, the group moved to inGroup (the references moved)
    EEPPatrolGroup* GetPatrolGroup();
    EEComplexUnit* GetPatrolTarget();
    void SetPatrolGroup(EEPPatrolGroup* inGroup);

    virtual ~EEUGRust();
    virtual EEUnitGoal* Clone();
    virtual void DumpOutOfSynch(FILE* inFile);
    virtual long GetOutOfSynchChecksum();
    virtual void GetDebugLine(UWideString& ioText);
    virtual void GetDebugText(UWideString& ioText);
    virtual void GetGoalLocation(GE3DPoint& outLocation);
    virtual long GetStartAction();
    virtual void Step(EEComplexUnit* inUnit);
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);
    virtual void Start(EEComplexUnit* inUnit);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);
    virtual bool Suits(EEUnit* inUnit);

private:
    EEUGRust(unsigned char inKind, bool inArg);
};

// the patrol goal of inGoal (the old EEGoalCast<EEUGPatrol>), NULL when it is another goal or none
inline EEUGRust* EEPatrolGoalCast(EEUnitGoal* inGoal)
{
    return inGoal != NULL && inGoal->mKind == kGoalPatrol ? static_cast<EEUGRust*>(inGoal) : NULL;
}

#endif
