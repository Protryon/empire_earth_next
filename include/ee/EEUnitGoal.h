// EEUnitGoal.h (RECONSTRUCTION; file name inferred). What a unit is doing (RTTI EEUnitGoal; vtable 0x43cc64)
// Layout of the original: EEUGAttackGround sizeof 0x2c = its new-site size, ctor 0x23c589 exact in EEUnitGoal.cpp;
// EEUGGather sizeof 0x44, ctor 0x23c63f; EEUGCastCalamity sizeof 0x34, ctor 0x2209c9; EEUGAttackUnit sizeof 0x44,
// ctor 0x23c5b2; EEUGHealUnit sizeof 0x40, ctor 0x2252d0. Member names are provisional.
//
// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 to 22; R5b.2 for the attack and heal goals): every goal class moved
// whole into Rust, so the base class and the one class left (EEUGRust, now a second name of this one: ee/EEUGRust.h)
// are one class with no vtable. The object is the goal the queues, the unit's mGoalState and the save hold: its base
// fields here, its own state and every body Rust's (rust/crates/ee_sim/src/units/rust_goal.rs dispatches by mKind:
// location.rs, patrol.rs, calamity.rs, explore.rs, unload.rs, load.rs, build.rs, gather.rs; the attack and heal
// kinds go to ee_combat's goal dispatch: goal_objects.rs, goal_attack.rs, goal_heal.rs). The base goal's own bodies
// (EEUnitGoal.cpp 0x23c332..0x23c83a: GetDestination, Start, EvaluateCondition, the base record's Save and Load,
// the trivial slots) are the dispatch's defaults; CreateGoal and the constructor are EEUGRust.cpp's. The goal classes
// it replaces: EEUGLocation (0x227621..0x227de8), EEUGPatrol (0x227de8..0x2283bc; EEPlayer's SetPatrolGroup 0x563659
// with it), EEUGCastCalamity (0x220986..0x2213dd), EEUGExplore (0x2213dd..0x221d98), EEUGUnloadTransport
// (0x2283bc..0x228c4c), EEUGLoadContainer (0x22585a..0x226a43), EEUGLoadGarrison (0x226a43..0x2271c7),
// EEUGLoadTransport (0x2271c7..0x227621), EEUGBuildRepair (0x21e0c1..0x220986), EEUGGather (0x221d98..0x22528d),
// EEUGAttackGround (0x21ce6c..0x21d2f4), EEUGAttackUnit (0x21d2f4..0x21e0c1), EEUGHealUnit (0x22528d..0x22585a).
#ifndef EE_EEUNITGOAL_H
#define EE_EEUNITGOAL_H

#include <stdio.h>                      // FILE (the out-of-synch dumps)
#include <stdint.h>
#include <vector>

#include "lle/U2DPoint.h"

class EEComplexUnit;
class EEPlayer;
class EETechTreeNode;
class EEUnitClass;
class EEUnit;
class EEPPatrolGroup;
class FSFile;
class GE2DPoint;
class GE3DPoint;
class UWideString;
struct EeUnitsGoalIo;

class EEUnitGoal {
public:
    // 0x23c375: active, idle, kind inKind, the next goal id (sGoalCounter), stamped with the game time; its own state
    // not made yet (the New functions below make it; the bodies' copies make theirs)
    EEUnitGoal(unsigned char inKind, bool inArg);
    // 0x23c3e6: an empty goal of kind inKind for loading (NULL for an unknown kind); it hands the counter value back
    static EEUnitGoal* CreateGoal(unsigned char inKind);
    ~EEUnitGoal();

    // the original's virtuals (vtable 0x43cc64, by slot), each one call of the kind's body
    void AddCreatedUnit(EEComplexUnit* inUnit);                // +0x04 (the load goals' container takes the unit)
    void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);   // +0x08 (the base: told to the group)
    bool IsActive(unsigned long* outValue);                    // +0x0c (the gather goal reports 5000 while not active)
    EEUnitGoal* Clone();                                       // +0x10
    void DumpOutOfSynch(FILE* inFile);                         // +0x14
    long GetOutOfSynchChecksum();                              // +0x18
    void GetDebugLine(UWideString& ioText);                    // +0x1c
    bool IsLoadGoal();                                         // +0x24
    void GetGoalLocation(GE3DPoint& outLocation);              // +0x28
    long GetMaxPathAttempts();                                 // +0x2c (the base: 3)
    long GetStartAction();                                     // +0x30
    long GetStartTime() { return this->mStartTime; }           // +0x34
    unsigned long GetCreationTime() { return this->mCreationTime; }   // +0x38
    void SetOwner(EEComplexUnit* inUnit);                      // +0x3c
    void Step(EEComplexUnit* inUnit);                          // +0x40
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44
    void PathTaken(EEComplexUnit* inUnit);                     // +0x48
    void Abandon(EEComplexUnit* inUnit);                       // +0x4c
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x50
    void Start(EEComplexUnit* inUnit);                         // +0x54
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x58
    // +0x5c: the base goal's count, then the kind's (the attack unit's: 0 also clears the sidestep count)
    void SetPathAttempts(long inValue);
    void SetStartTime(long inValue) { this->mStartTime = inValue; }   // +0x60
    bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);   // +0x64
    void RemovedFromQueue(EEComplexUnit*) {}                   // +0x68 (empty in every class)
    void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit* inUnit);   // +0x6c
    bool Suits(EEUnit* inUnit);                                // +0x70
    void Work(EEComplexUnit* inUnit);                          // +0x74

    // the copies pass it through a byte temporary, the other bools straight (provisional name)
    bool GetB1C() const { return this->mB1C; }
    unsigned char GetKind() const { return this->mKind; }

    // a location goal (kGoalLocation; ee_units location.rs; the four EEUGLocation constructors): at the point (x, y, z;
    // a map position at height 0; a 3D point) with the forced-move, facing (its yaw), auto-waypoint flags and the
    // group path state; or the load's empty goal (at 0, 0, 0, yaw 0, no facing). None uses group movement.
    static EEUnitGoal* NewLocation(float inX, float inY, float inZ, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState);
    static EEUnitGoal* NewLocation(const GE2DPoint& inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState);
    static EEUnitGoal* NewLocation(const GE3DPoint* inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState);
    static EEUnitGoal* NewLocation(bool inArg, bool inIsForcedMove, bool inIsAutoWaypoint,
                                   unsigned char inGroupPathState);
    // the location goal's members (EEUGLocation's accessors; the record's fields, no host call): its point, the yaw
    // to face, the flags and the group path state
    GE3DPoint GetLocation() const;
    void SetLocation(const GE3DPoint& inLocation);
    float GetYaw() const;
    bool GetIsForcedMove() const;
    bool GetShouldFace() const;
    bool GetIsAutoWaypoint() const;
    bool GetUseGroupMovement() const;
    void SetUseGroupMovement(bool inValue);
    unsigned char GetGroupPathState() const;
    void SetGroupPathState(unsigned char inValue);
    // whether inUnit faces the goal's yaw (within a degree); if so it is turned to it exactly (EEUGLocation::FaceYaw)
    bool FaceYaw(EEComplexUnit* inUnit);

    // a patrol (kGoalPatrol): a new empty patrol group, no point (EEUGPatrol(inArg)); or an existing group and the
    // point to start at, both referenced (EEUGPatrol(inGroup, inTarget, inArg))
    static EEUnitGoal* NewPatrol(bool inArg);
    static EEUnitGoal* NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg);
    // the patrol's group, its current point, the group moved to inGroup (the references moved)
    EEPPatrolGroup* GetPatrolGroup();
    EEComplexUnit* GetPatrolTarget();
    void SetPatrolGroup(EEPPatrolGroup* inGroup);

    // a cast (kGoalCastCalamity): the source casts the calamity of technology inCalamityType on the target unit or
    // at the tile, both units referenced (EEUGCastCalamity(...)); or the load's empty goal (EEUGCastCalamity(inArg))
    static EEUnitGoal* NewCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                       long inCalamityType, bool inArg);
    static EEUnitGoal* NewCastCalamity(bool inArg);
    // the cast's technology (its tile and target unit when asked); where the calamity goes (the target, the source
    // for two technologies, else the tile; at the flying source's height)
    long GetCalamityType(long* outTileX = NULL, long* outTileY = NULL, EEUnit** outTarget = NULL);
    void GetCalamityTargetLocation(GE3DPoint& outLocation);

    // an explore goal (kGoalExplore): no tile searched yet (EEUGExplore(inArg)); the tile being explored's centre
    // (x and y only: EEUGExplore's GetTileCenter), the tile set
    static EEUnitGoal* NewExplore(bool inArg);
    void GetExploreTileCenter(GE3DPoint& outLocation);
    void SetExploreTile(long inX, long inY);

    // an unload (kGoalUnloadTransport): the transport (referenced) unloads at the tile, its unload tiles found when
    // it carries cargo (EEUGUnloadTransport(...)); or the load's empty goal (EEUGUnloadTransport(inArg))
    static EEUnitGoal* NewUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg);
    static EEUnitGoal* NewUnloadTransport(bool inArg);
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
    static EEUnitGoal* NewAttackGround(const GE3DPoint& inTarget, bool inArg);
    GE3DPoint GetGroundTarget();
    // an attack on a unit (kGoalAttackUnit; EEUGAttackUnit(inAttacker, inTarget, inArg3, inIsConvert): a multi-part
    // target's attack part, the target referenced, the attack counts; the load's EEUGAttackUnit(inArg) empty), its
    // target, attacker, last known target tile, convert flag, destination and sidestep count
    static EEUnitGoal* NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert);
    static EEUnitGoal* NewAttackUnitEmpty(bool inArg);
    EEUnit* GetAttackTarget() const;
    EEComplexUnit* GetAttacker() const;
    void SetAttacker(EEComplexUnit* inAttacker);
    void SetAttackTarget(EEUnit* inTarget);           // the old SetTarget (the reference moved, the tile)
    void RetargetGatePart(EEComplexUnit* inUnit);     // a gate target becomes the part EEGetAttackPart gives
    long GetAttackTileX() const;
    long GetAttackTileY() const;
    bool IsConvertAttack() const;
    unsigned long GetSidestepCount() const;
    void SetSidestepCount(unsigned long inCount);
    // a heal (kGoalHealUnit; EEUGHealUnit(inTarget, inArg): the location goal at the healee's position, the healee
    // referenced; the load's EEUGHealUnit(inArg): no healee, at 0, 0, 0), its healee, and the location it was made at
    // (the heal goal derived from EEUGLocation until it moved: EELocationGoalPoint reads it)
    static EEUnitGoal* NewHealUnit(EEComplexUnit* inTarget, bool inArg);
    static EEUnitGoal* NewHealUnit(bool inArg);
    EEComplexUnit* GetHealTarget() const;
    GE3DPoint GetHealLocation() const;
    void SetHealLocation(const GE3DPoint& inLocation);
    // a load goal (kGoalLoadTransport, kGoalLoadGarrison): the container and the units, all referenced; inUnits is
    // sorted by id and loses its duplicates, as the original constructors did to the caller's list
    // (EEUGLoadTransport/EEUGLoadGarrison(inContainer, inUnits, inArg)); or the load's empty goal
    static EEUnitGoal* NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                               bool inArg);
    static EEUnitGoal* NewLoad(unsigned char inKind, bool inArg);
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

    // a build/repair goal (kGoalBuildRepair): the player's goal on the building's first part, referenced
    // (EEUGBuildRepair(inPlayer, inTarget, inArg)); or the load's empty goal (EEUGBuildRepair(inArg))
    static EEUnitGoal* NewBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg);
    static EEUnitGoal* NewBuildRepair(bool inArg);
    // the building (NULL once dropped), its class's type, technology, tech tree node and class as the goal took
    // them; whether units other than the building block its footprint
    EEUnit* GetBuildTarget();
    long GetBuildTargetType();
    long GetBuildTechTreeID();
    EETechTreeNode* GetBuildTechNode();
    EEUnitClass* GetBuildTargetClass();
    bool IsBuildSiteTaken();

    // a gather goal (kGoalGather): the resource and the storage, both referenced (EEUGGather(inResource, inStorage,
    // inArg)); or the load's empty goal (EEUGGather(inArg))
    static EEUnitGoal* NewGather(EEUnit* inResource, EEUnit* inStorage, bool inArg);
    static EEUnitGoal* NewGather(bool inArg);
    // the resource, the storage, the gathering and waiting flags, the destination; a new resource or storage (the
    // references moved)
    EEUnit* GetGatherResource();
    EEUnit* GetGatherStorage();
    bool IsGathering();
    bool IsGatherWaiting();
    void SetGatherResource(EEUnit* inResource);
    void SetGatherStorage(EEUnit* inStorage);
    // a waiting area by the resource (true when found); an unfinished living settlement within twice the
    // gatherer's line of sight (NULL if none)
    bool FindGatherSpot(EEComplexUnit* inUnit);
    EEUnit* FindUnfinishedFarm(EEComplexUnit* inUnit);

    bool mIsActive;                         // +0x04 (of the original: +0x00 was its vptr)
    unsigned char mKind;                    // +0x05
    unsigned long mAction;                  // +0x08 (FSFile Read/Write(unsigned long))
    unsigned long mStartTime;               // +0x0c (saved from minor version 0x9d)
    unsigned long mCreationTime;            // +0x10 (saved from 0xa4)
    unsigned long mID;                      // +0x14 the goal's number (saved from 0x64)
    long mPathAttempts;                     // +0x18
    bool mB1C;                              // +0x1c (saved from 0xf)
    bool mB1D;                              // +0x1d (saved from 0x87)
};

// EEUnitGoal::mKind: EEUnitGoal::CreateGoal 0x23c3e6 creates the goal class of each kind.
enum EEUnitGoalKind {
    kGoalLocation = 1,       // EEUnitGoal::CreateGoal 0x23c3e6 case 1: new EEUGLocation (RTTI)
    kGoalAttackUnit = 2,     // EEUnitGoal::CreateGoal 0x23c3e6 case 2: new EEUGAttackUnit (RTTI)
    kGoalAttackGround = 3,   // EEUnitGoal::CreateGoal 0x23c3e6 case 3: new EEUGAttackGround (RTTI)
    kGoalBuildRepair = 4,    // EEUnitGoal::CreateGoal 0x23c3e6 case 4: new EEUGBuildRepair (RTTI)
    kGoalGather = 6,         // EEUnitGoal::CreateGoal 0x23c3e6 case 6: new EEUGGather (RTTI)
    kGoalLoadTransport = 7,  // EEUnitGoal::CreateGoal 0x23c3e6 case 7: new EEUGLoadTransport (RTTI)
    kGoalUnloadTransport = 8,  // EEUnitGoal::CreateGoal 0x23c3e6 case 8: new EEUGUnloadTransport (RTTI)
    kGoalPatrol = 9,         // EEUnitGoal::CreateGoal 0x23c3e6 case 9: new EEUGPatrol (RTTI)
    kGoalExplore = 10,       // EEUnitGoal::CreateGoal 0x23c3e6 case 10: new EEUGExplore (RTTI)
    kGoalCastCalamity = 11,  // EEUnitGoal::CreateGoal 0x23c3e6 case 11: new EEUGCastCalamity (RTTI)
    kGoalLoadGarrison = 12,  // EEUnitGoal::CreateGoal 0x23c3e6 case 12: new EEUGLoadGarrison (RTTI)
    kGoalHealUnit = 15,      // EEUnitGoal::CreateGoal 0x23c3e6 case 15: new EEUGHealUnit (RTTI)
};

#endif
