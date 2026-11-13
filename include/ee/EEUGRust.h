// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 and 17): the goals whose class moved whole into Rust. The object is
// the EEUnitGoal the queues, the unit's mGoalState and the save hold (its base fields, its vtable); its own state and
// bodies are Rust's (rust/crates/ee_units/src/rust_goal.rs dispatches by mKind: patrol.rs for kGoalPatrol,
// calamity.rs for kGoalCastCalamity, explore.rs for kGoalExplore, unload.rs for kGoalUnloadTransport, load.rs for
// kGoalLoadTransport and kGoalLoadGarrison), every virtual one call. Replaces EEUGPatrol (0x227de8..0x2283bc;
// EEPlayer's SetPatrolGroup 0x563659 with it), EEUGCastCalamity (0x220986..0x2213dd), EEUGExplore
// (0x2213dd..0x221d98), EEUGUnloadTransport (0x2283bc..0x228c4c) and the load goals EEUGLoadContainer
// (0x22585a..0x226a43), EEUGLoadGarrison (0x226a43..0x2271c7) and EEUGLoadTransport (0x2271c7..0x227621): their
// classes and files are deleted.
#ifndef EE_EEUGRUST_H
#define EE_EEUGRUST_H

#include <vector>

#include "lle/U2DPoint.h"
#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class EEPPatrolGroup;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;

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

    // a load goal (kGoalLoadTransport, kGoalLoadGarrison): the container and the units, all referenced; inUnits is
    // sorted by id and loses its duplicates, as the original constructors did to the caller's list
    // (EEUGLoadTransport/EEUGLoadGarrison(inContainer, inUnits, inArg)); or the load's empty goal
    static EEUGRust* NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                             bool inArg);
    static EEUGRust* NewLoad(unsigned char inKind, bool inArg);
    // the container, its and the units' rendezvous tiles, the garrison's valid flag (a building it can upgrade or a
    // capitol); the units to load (each added unless listed)
    EEComplexUnit* GetLoadContainer();
    U2DPoint<long> GetLoadContainerTile();
    U2DPoint<long> GetLoadUnitTile();
    bool IsLoadGarrisonValid();
    unsigned long GetNumLoadUnits();
    EEComplexUnit* GetLoadUnitAt(unsigned long inIndex);
    bool HasLoadUnit(EEComplexUnit* inUnit);
    void AddLoadUnit(EEComplexUnit* inUnit);
    void AddLoadUnits(const std::vector<EEComplexUnit*>& inUnits);

    virtual ~EEUGRust();
    virtual void AddCreatedUnit(EEComplexUnit* inUnit);
    virtual bool IsLoadGoal();
    virtual void Abandon(EEComplexUnit* inUnit);
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
};

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
// the old dynamic_cast<EEUGLoadTransport*>, <EEUGLoadGarrison*> and <EEUGLoadContainer*> (either)
inline EEUGRust* EELoadTransportGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalLoadTransport);
}
inline EEUGRust* EELoadGarrisonGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalLoadGarrison);
}
inline EEUGRust* EELoadGoalCast(EEUnitGoal* inGoal)
{
    EEUGRust* theGoal = EELoadTransportGoalCast(inGoal);
    return theGoal != NULL ? theGoal : EELoadGarrisonGoalCast(inGoal);
}

#endif
