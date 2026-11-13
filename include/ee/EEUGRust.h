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

    // the combat lane's goals (EEUGRustCombat.cpp; ee_combat goal_objects.rs): an attack on the ground at inTarget
    // (kGoalAttackGround; EEUGAttackGround(inTarget, inArg), the load's EEUGAttackGround(inArg) at 0, 0, 0) and its
    // target point
    static EEUGRust* NewAttackGround(const GE3DPoint& inTarget, bool inArg);
    GE3DPoint GetGroundTarget();
    // an attack on a unit (kGoalAttackUnit; EEUGAttackUnit(inAttacker, inTarget, inArg3, inIsConvert): a multi-part
    // target's attack part, the target referenced, the attack counts; the load's EEUGAttackUnit(inArg) empty), its
    // target, attacker, last known target tile, convert flag, destination and sidestep count
    static EEUGRust* NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert);
    static EEUGRust* NewAttackUnitEmpty(bool inArg);
    EEUnit* GetAttackTarget() const;
    EEComplexUnit* GetAttacker() const;
    void SetAttacker(EEComplexUnit* inAttacker);
    void SetAttackTarget(EEUnit* inTarget);           // the old SetTarget (the reference moved, the tile)
    void RetargetGatePart(EEComplexUnit* inUnit);     // a gate target becomes the part EEGetAttackPart gives
    void UpdateTargetTile();
    long GetAttackTileX() const;
    long GetAttackTileY() const;
    bool IsConvertAttack() const;
    GE3DPoint GetAttackLocation() const;
    void SetAttackLocation(const GE3DPoint& inLocation);
    unsigned long GetSidestepCount() const;
    void SetSidestepCount(unsigned long inCount);

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
    // the base goal's for a patrol; the combat goals' (EEUGRustCombat.cpp)
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);
    virtual long GetMaxPathAttempts();
    virtual void SetOwner(EEComplexUnit* inUnit);
    virtual void SetPathAttempts(long inValue);
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);
    virtual void Work(EEComplexUnit* inUnit);

private:
    EEUGRust(unsigned char inKind, bool inArg);
    // the combat goals' bodies (EEUGRustCombat.cpp), one per virtual above, for a kind EEUGRustIsCombat answers
    void CombatDestroy();
    EEUnitGoal* CombatClone();
    void CombatDumpOutOfSynch(FILE* inFile);
    long CombatChecksum();
    void CombatDebugLine(UWideString& ioText);
    void CombatDebugText(UWideString& ioText);
    void CombatGoalLocation(GE3DPoint& outLocation);
    long CombatStartAction();
    void CombatStep(EEComplexUnit* inUnit);
    long CombatLoad(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long CombatRepairPointers(unsigned long inFirst, unsigned long inSecond);
    void CombatStart(EEComplexUnit* inUnit);
    long CombatSave(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void CombatReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew);
    bool CombatSuits(EEUnit* inUnit);
    void CombatDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);
    long CombatMaxPathAttempts();
    void CombatSetOwner(EEComplexUnit* inUnit);
    void CombatSetPathAttempts(long inValue);
    bool CombatEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);
    void CombatWork(EEComplexUnit* inUnit);
};

// whether goals of inKind are the combat lane's (EEUGRustCombat.cpp)
bool EEUGRustIsCombat(unsigned char inKind);

// the patrol goal of inGoal (the old EEGoalCast<EEUGPatrol>), NULL when it is another goal or none
inline EEUGRust* EEPatrolGoalCast(EEUnitGoal* inGoal)
{
    return inGoal != NULL && inGoal->mKind == kGoalPatrol ? static_cast<EEUGRust*>(inGoal) : NULL;
}

// the attack-unit goal of inGoal (the old EEGoalCast<EEUGAttackUnit>), NULL when it is another goal or none
inline EEUGRust* EEAttackUnitCast(EEUnitGoal* inGoal)
{
    return inGoal != NULL && inGoal->mKind == kGoalAttackUnit ? static_cast<EEUGRust*>(inGoal) : NULL;
}

// the attack-ground goal of inGoal (the old EEGoalCast<EEUGAttackGround>), NULL when it is another goal or none
inline EEUGRust* EEAttackGroundCast(EEUnitGoal* inGoal)
{
    return inGoal != NULL && inGoal->mKind == kGoalAttackGround ? static_cast<EEUGRust*>(inGoal) : NULL;
}

#endif
