// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 and 17): the goals whose class moved whole into Rust. The object is
// the EEUnitGoal the queues, the unit's mGoalState and the save hold (its base fields, its vtable); its own state and
// bodies are Rust's (rust/crates/ee_units/src/rust_goal.rs dispatches by mKind: patrol.rs for kGoalPatrol,
// calamity.rs for kGoalCastCalamity, explore.rs for kGoalExplore, unload.rs for kGoalUnloadTransport), every
// virtual one call. Replaces EEUGPatrol (0x227de8..0x2283bc; EEPlayer's SetPatrolGroup 0x563659 with it),
// EEUGCastCalamity (0x220986..0x2213dd), EEUGExplore (0x2213dd..0x221d98) and EEUGUnloadTransport
// (0x2283bc..0x228c4c): their classes and files are deleted.
#ifndef EE_EEUGRUST_H
#define EE_EEUGRUST_H

#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class EEPPatrolGroup;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;
struct EeUnitsGoalIo;

class EEUGRust : public EEUnitGoal {
public:
    // a goal of the kind with no state yet (the bodies' copies make theirs; the New functions below for the rest)
    EEUGRust(unsigned char inKind, bool inArg);

    // a patrol (kGoalPatrol): a new empty patrol group, no point (EEUGPatrol(inArg)); or an existing group and the
    // point to start at, both referenced (EEUGPatrol(inGroup, inTarget, inArg))
    static EEUGRust* NewPatrol(bool inArg);
    static EEUGRust* NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg);
    // the patrol's group, its current point, the group moved to inGroup (the references moved)
    EEPPatrolGroup* GetPatrolGroup();
    EEComplexUnit* GetPatrolTarget();
    void SetPatrolGroup(EEPPatrolGroup* inGroup);

    // a cast (kGoalCastCalamity): the source casts the calamity of technology inCalamityType on the target unit or
    // at the tile, both units referenced (EEUGCastCalamity(...)); or the load's empty goal (EEUGCastCalamity(inArg))
    static EEUGRust* NewCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                     long inCalamityType, bool inArg);
    static EEUGRust* NewCastCalamity(bool inArg);
    // the cast's technology (its tile and target unit when asked); where the calamity goes (the target, the source
    // for two technologies, else the tile; at the flying source's height)
    long GetCalamityType(long* outTileX = NULL, long* outTileY = NULL, EEUnit** outTarget = NULL);
    void GetCalamityTargetLocation(GE3DPoint& outLocation);

    // an explore goal (kGoalExplore): no tile searched yet (EEUGExplore(inArg)); the tile being explored's centre
    // (x and y only: EEUGExplore's GetTileCenter), the tile set
    static EEUGRust* NewExplore(bool inArg);
    void GetExploreTileCenter(GE3DPoint& outLocation);
    void SetExploreTile(long inX, long inY);

    // an unload (kGoalUnloadTransport): the transport (referenced) unloads at the tile, its unload tiles found when
    // it carries cargo (EEUGUnloadTransport(...)); or the load's empty goal (EEUGUnloadTransport(inArg))
    static EEUGRust* NewUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg);
    static EEUGRust* NewUnloadTransport(bool inArg);
    // the transport (its unload tile when asked); another transport (the reference moved, its range taken); whether
    // the transport can unload at the tile (its class's test, else a free tile)
    EEComplexUnit* GetUnloadTransport(long* outX = NULL, long* outY = NULL);
    void SetUnloadTransport(EEComplexUnit* inTransport);
    bool CanUnloadAtDestination();
    // a new unload tile, then the unload tiles found for it (the AI's search for a place to unload)
    void SetUnloadDestination(long inX, long inY);
    // the transport dropped without its release (the air bomber's unused goal keeps the plane's reference)
    void ForgetUnloadTransport();

    // the combat lane's goals (ee_combat goal_objects.rs, goal_attack.rs; EEUGRustCombat.cpp): an attack on the
    // ground at inTarget (kGoalAttackGround; EEUGAttackGround(inTarget, inArg), the load's at 0, 0, 0) and its point
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
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);
    virtual EEUnitGoal* Clone();
    virtual void DumpOutOfSynch(FILE* inFile);
    virtual long GetOutOfSynchChecksum();
    virtual void GetDebugLine(UWideString& ioText);
    virtual void GetDebugText(UWideString& ioText);
    virtual void GetGoalLocation(GE3DPoint& outLocation);
    virtual long GetMaxPathAttempts();
    virtual long GetStartAction();
    virtual void SetOwner(EEComplexUnit* inUnit);
    virtual void Step(EEComplexUnit* inUnit);
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);
    virtual void Start(EEComplexUnit* inUnit);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);
    virtual bool Suits(EEUnit* inUnit);
    // the base goal's, then the kind's (the attack unit's: 0 also clears the sidestep count)
    virtual void SetPathAttempts(long inValue);
};

// one body of inGoal through the kind dispatch (ee_units_rust_goal over EEUGRust.cpp's host), for the accessors
// defined outside EEUGRust.cpp (EEUGRustCombat.cpp)
int64_t EEUGRustGoal(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0,
                     EeUnitsGoalIo* ioArgs = NULL, FSFile* inFile = NULL);
// the combat goals' host operations (EE_COMBAT_G_*, from 1000; EEUGRustCombat.cpp), the host's other cases
int64_t EECombatGoalHostOp(FSFile* inFile, int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsGoalIo* ioArgs);

// the goal of kind inKind (the old EEGoalCast<EEUGPatrol>, <EEUGCastCalamity>, <EEUGExplore>,
// <EEUGUnloadTransport>), NULL for another or none
inline EEUGRust* EERustGoalCast(EEUnitGoal* inGoal, unsigned char inKind)
{
    return inGoal != NULL && inGoal->mKind == inKind ? static_cast<EEUGRust*>(inGoal) : NULL;
}
inline EEUGRust* EEPatrolGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalPatrol);
}
inline EEUGRust* EECastCalamityGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalCastCalamity);
}
inline EEUGRust* EEExploreGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalExplore);
}
inline EEUGRust* EEUnloadGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalUnloadTransport);
}
// the attack-ground and attack-unit goals (the old EEGoalCast<EEUGAttackGround>, <EEUGAttackUnit>)
inline EEUGRust* EEAttackGroundCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalAttackGround);
}
inline EEUGRust* EEAttackUnitCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalAttackUnit);
}

#endif
