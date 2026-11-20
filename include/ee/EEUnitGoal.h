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
#include "rust/sim/units.h"
#include "rust/sim/combat.h"

class EEComplexUnit;
class EEPlayer;
class EETechTreeNode;
class EEUnitClass;
class EEUnit;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class FSFile;
class GE2DPoint;
class GE3DPoint;
class UWideString;
struct EeUnitsGoalIo;

class EEUnitGoal {
public:
    // New (units behaviour, round 3): a goal is a Rust object (ee_units goal_obj.rs) whose head is these members:
    // made by Make (0x23c375: active, idle, kind inKind, the next goal id, stamped with the game time; its own state
    // not made yet: the New functions below make it), deleted by delete (the destructor runs the kind's destroy, the
    // memory is Rust's). The bodies that write no file or text are one ee_units_goal_call each.
    static EEUnitGoal* Make(unsigned char inKind, bool inArg)
    {
        return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new(inKind, inArg));
    }
    static void* operator new(size_t) = delete;
    static void operator delete(void* inGoal) { ee_units_goal_dealloc(static_cast<EeUnitsGoalBase*>(inGoal)); }
    // 0x23c3e6: an empty goal of kind inKind for loading (NULL for an unknown kind); it hands the counter value back
    static EEUnitGoal* CreateGoal(unsigned char inKind);
    ~EEUnitGoal() { this->Call(EE_UNITS_GOAL_DESTROY); }

    // the original's virtuals (vtable 0x43cc64, by slot), each one call of the kind's body
    void AddCreatedUnit(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_ADD_CREATED_UNIT, H(inUnit)); }   // +0x04
    void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);   // +0x08 (the base: told to the group)
    // +0x0c (the gather goal reports 5000 while not active)
    bool IsActive(unsigned long* outValue)
    {
        if (this->mKind == kGoalGatherKind && !this->mIsActive)
            *outValue = 5000;
        return this->mIsActive;
    }
    EEUnitGoal* Clone()                                        // +0x10
    {
        return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(this->Call(EE_UNITS_GOAL_CLONE, 0, this->mB1C)));
    }
    void DumpOutOfSynch(FILE* inFile);                         // +0x14
    long GetOutOfSynchChecksum() { return (long)this->Call(EE_UNITS_GOAL_CHECKSUM); }   // +0x18
    void GetDebugLine(UWideString& ioText);                    // +0x1c
    // +0x24 (kGoalLoadTransport, kGoalLoadGarrison)
    bool IsLoadGoal() { return this->mKind == 7 || this->mKind == 12; }
    void GetGoalLocation(GE3DPoint& outLocation);              // +0x28
    long GetMaxPathAttempts() { return (long)this->Call(EE_UNITS_GOAL_MAX_PATH_ATTEMPTS); }   // +0x2c (the base: 3)
    long GetStartAction() { return (long)this->Call(EE_UNITS_GOAL_START_ACTION); }            // +0x30
    long GetStartTime() { return this->mStartTime; }           // +0x34
    unsigned long GetCreationTime() { return this->mCreationTime; }   // +0x38
    void SetOwner(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_SET_OWNER, H(inUnit)); }   // +0x3c
    void Step(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_STEP, H(inUnit)); }            // +0x40
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44
    void PathTaken(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_PATH_TAKEN, H(inUnit)); }   // +0x48
    void Abandon(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_ABANDON, H(inUnit)); }        // +0x4c
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)         // +0x50
    {
        return (long)this->Call(EE_UNITS_GOAL_REPAIR, inMajorVersion, inMinorVersion);
    }
    void Start(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_START, H(inUnit)); }          // +0x54
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x58
    // +0x5c: the base goal's count, then the kind's (the attack unit's: 0 also clears the sidestep count)
    void SetPathAttempts(long inValue)
    {
        this->mPathAttempts = inValue;
        this->Call(EE_UNITS_GOAL_SET_PATH_ATTEMPTS, (uint64_t)(int64_t)inValue);
    }
    void SetStartTime(long inValue) { this->mStartTime = inValue; }   // +0x60
    bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)   // +0x64
    {
        return this->Call(EE_UNITS_GOAL_EVALUATE, H(inUnit), inEvent) != 0;
    }
    void RemovedFromQueue(EEComplexUnit*) {}                   // +0x68 (empty in every class)
    void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit* inUnit)   // +0x6c
    {
        EeUnitsGoalIo theArgs = {{(int64_t)H(inUnit)}};
        this->Call(EE_UNITS_GOAL_REPLACE_UNIT, H(inOld), H(inNew), &theArgs);
    }
    bool Suits(EEUnit* inUnit) { return this->Call(EE_UNITS_GOAL_SUITS, H(inUnit)) != 0; }    // +0x70
    void Work(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_WORK, H(inUnit)); }           // +0x74

    // one body of the goal with the registered world host (ee_units goal_obj.rs), an object's handle
    int64_t Call(int32_t inOperation, uint64_t inA = 0, uint64_t inB = 0, EeUnitsGoalIo* ioArgs = NULL) const
    {
        return ee_units_goal_call(inOperation, reinterpret_cast<const EeUnitsGoalBase*>(this), inA, inB, ioArgs);
    }
    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
    // the object of a handle a body answers
    template <class T> static T* Of(int64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }
    // a new goal of a kind made by its body's operation (the New functions)
    static EEUnitGoal* New(unsigned char inKind, bool inArg, int32_t inOperation, uint64_t inA = 0, uint64_t inB = 0,
                           EeUnitsGoalIo* ioArgs = NULL)
    {
        EEUnitGoal* theGoal = Make(inKind, inArg);
        theGoal->Call(inOperation, inA, inB, ioArgs);
        return theGoal;
    }
    // one field of a body's answer block
    int64_t Field(int32_t inOperation, int inIndex) const
    {
        EeUnitsGoalIo theArgs = {};
        this->Call(inOperation, 0, 0, &theArgs);
        return theArgs.i[inIndex];
    }
    EeUnitsLocationGoal Location() const
    {
        EeUnitsLocationGoal theRecord;
        ee_units_location_goal_get(H(this), &theRecord);
        return theRecord;
    }
    EeCombatAttackGoal Attack() const
    {
        EeCombatAttackGoal theRecord;
        ee_combat_attack_get(H(this), &theRecord);
        return theRecord;
    }
    enum { kGoalGatherKind = 6 };

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
    float GetYaw() const { return this->Location().yaw; }
    bool GetIsForcedMove() const { return this->Location().forced_move; }
    bool GetShouldFace() const { return this->Location().should_face; }
    bool GetIsAutoWaypoint() const { return this->Location().auto_waypoint; }
    bool GetUseGroupMovement() const { return this->Location().group_movement; }
    void SetUseGroupMovement(bool inValue)
    {
        EeUnitsLocationGoal theRecord = this->Location();
        theRecord.group_movement = inValue;
        ee_units_location_goal_set(H(this), &theRecord);
    }
    unsigned char GetGroupPathState() const { return this->Location().group_path_state; }
    void SetGroupPathState(unsigned char inValue)
    {
        EeUnitsLocationGoal theRecord = this->Location();
        theRecord.group_path_state = inValue;
        ee_units_location_goal_set(H(this), &theRecord);
    }
    // whether inUnit faces the goal's yaw (within a degree); if so it is turned to it exactly (EEUGLocation::FaceYaw)
    bool FaceYaw(EEComplexUnit* inUnit) { return this->Call(EE_UNITS_GOAL_FACE_YAW, H(inUnit)) != 0; }

    // a patrol (kGoalPatrol): a new empty patrol group, no point (EEUGPatrol(inArg)); or an existing group and the
    // point to start at, both referenced (EEUGPatrol(inGroup, inTarget, inArg))
    static EEUnitGoal* NewPatrol(bool inArg) { return New(9, inArg, EE_UNITS_GOAL_PATROL_NEW_EMPTY); }
    static EEUnitGoal* NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg)
    {
        return New(9, inArg, EE_UNITS_GOAL_NEW_PATROL, H(inGroup), H(inTarget));
    }
    // the patrol's group, its current point, the group moved to inGroup (the references moved)
    EEPPatrolGroup* GetPatrolGroup() { return Of<EEPPatrolGroup>(this->Call(EE_UNITS_GOAL_PATROL_GROUP)); }
    EEComplexUnit* GetPatrolTarget() { return Of<EEComplexUnit>(this->Call(EE_UNITS_GOAL_PATROL_TARGET)); }
    void SetPatrolGroup(EEPPatrolGroup* inGroup) { this->Call(EE_UNITS_GOAL_PATROL_SET_GROUP, H(inGroup)); }

    // a cast (kGoalCastCalamity): the source casts the calamity of technology inCalamityType on the target unit or
    // at the tile, both units referenced (EEUGCastCalamity(...)); or the load's empty goal (EEUGCastCalamity(inArg))
    static EEUnitGoal* NewCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                       long inCalamityType, bool inArg)
    {
        EeUnitsGoalIo theArgs = {{inTileX, inTileY, inCalamityType}};
        return New(11, inArg, EE_UNITS_GOAL_NEW_CALAMITY, H(inSource), H(inTarget), &theArgs);
    }
    static EEUnitGoal* NewCastCalamity(bool inArg) { return New(11, inArg, EE_UNITS_GOAL_CALAMITY_NEW_EMPTY); }
    // the cast's technology (its tile and target unit when asked); where the calamity goes (the target, the source
    // for two technologies, else the tile; at the flying source's height)
    long GetCalamityType(long* outTileX = NULL, long* outTileY = NULL, EEUnit** outTarget = NULL)
    {
        EeUnitsGoalIo theArgs = {};
        EEUnit* theTarget = Of<EEUnit>(this->Call(EE_UNITS_GOAL_CALAMITY_FIELDS, 0, 0, &theArgs));
        if (outTileX != NULL)
            *outTileX = (long)theArgs.i[1];
        if (outTileY != NULL)
            *outTileY = (long)theArgs.i[2];
        if (outTarget != NULL)
            *outTarget = theTarget;
        return (long)theArgs.i[0];
    }
    void GetCalamityTargetLocation(GE3DPoint& outLocation);

    // an explore goal (kGoalExplore): no tile searched yet (EEUGExplore(inArg)); the tile being explored's centre
    // (x and y only: EEUGExplore's GetTileCenter), the tile set
    static EEUnitGoal* NewExplore(bool inArg) { return New(10, inArg, EE_UNITS_GOAL_NEW_EXPLORE); }
    void GetExploreTileCenter(GE3DPoint& outLocation);
    void SetExploreTile(long inX, long inY)
    {
        EeUnitsGoalIo theArgs = {{inX, inY}};
        this->Call(EE_UNITS_GOAL_EXPLORE_SET_TILE, 0, 0, &theArgs);
    }

    // an unload (kGoalUnloadTransport): the transport (referenced) unloads at the tile, its unload tiles found when
    // it carries cargo (EEUGUnloadTransport(...)); or the load's empty goal (EEUGUnloadTransport(inArg))
    static EEUnitGoal* NewUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg)
    {
        EeUnitsGoalIo theArgs = {{inX, inY}};
        return New(8, inArg, EE_UNITS_GOAL_NEW_UNLOAD, H(inTransport), 0, &theArgs);
    }
    static EEUnitGoal* NewUnloadTransport(bool inArg) { return New(8, inArg, EE_UNITS_GOAL_UNLOAD_NEW_EMPTY); }
    // the transport (its unload tile when asked); another transport (the reference moved, its range taken); whether
    // the transport can unload at the tile (its class's test, else a free tile)
    EEComplexUnit* GetUnloadTransport(long* outX = NULL, long* outY = NULL)
    {
        EeUnitsGoalIo theArgs = {};
        EEComplexUnit* theTransport = Of<EEComplexUnit>(this->Call(EE_UNITS_GOAL_UNLOAD_FIELDS, 0, 0, &theArgs));
        if (outX != NULL)
            *outX = (long)theArgs.i[0];
        if (outY != NULL)
            *outY = (long)theArgs.i[1];
        return theTransport;
    }
    void SetUnloadTransport(EEComplexUnit* inTransport) { this->Call(EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT, H(inTransport)); }
    bool CanUnloadAtDestination() { return this->Call(EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD) != 0; }

    // the combat lane's goals (ee_combat goal_objects.rs, goal_attack.rs; EEUGRustCombat.cpp): an attack on the
    // ground at inTarget (kGoalAttackGround; EEUGAttackGround(inTarget, inArg), the load's at 0, 0, 0) and its point
    static EEUnitGoal* NewAttackGround(const GE3DPoint& inTarget, bool inArg);
    GE3DPoint GetGroundTarget();
    // an attack on a unit (kGoalAttackUnit; EEUGAttackUnit(inAttacker, inTarget, inArg3, inIsConvert): a multi-part
    // target's attack part, the target referenced, the attack counts; the load's EEUGAttackUnit(inArg) empty), its
    // target, attacker, last known target tile, convert flag, destination and sidestep count
    static EEUnitGoal* NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert)
    {
        EeUnitsGoalIo theArgs = {{inIsConvert ? 1 : 0}};
        return New(2, inArg3, EE_COMBAT_GOAL_NEW_ATTACK, H(inAttacker), H(inTarget), &theArgs);
    }
    static EEUnitGoal* NewAttackUnitEmpty(bool inArg)
    {
        EeUnitsGoalIo theArgs = {{2}};
        return New(2, inArg, EE_COMBAT_GOAL_NEW_ATTACK, 0, 0, &theArgs);
    }
    EEUnit* GetAttackTarget() const { return Of<EEUnit>(this->Attack().target); }
    EEComplexUnit* GetAttacker() const { return Of<EEComplexUnit>(this->Attack().attacker); }
    void SetAttacker(EEComplexUnit* inAttacker)
    {
        EeCombatAttackGoal theRecord = this->Attack();
        theRecord.attacker = H(inAttacker);
        ee_combat_attack_set(H(this), &theRecord);
    }
    // the old SetTarget (the reference moved, the tile)
    void SetAttackTarget(EEUnit* inTarget) { this->Call(EE_COMBAT_GOAL_SET_TARGET, H(inTarget)); }
    // a gate target becomes the part EEGetAttackPart gives
    void RetargetGatePart(EEComplexUnit*) { this->Call(EE_COMBAT_GOAL_RETARGET_GATE); }
    long GetAttackTileX() const { return this->Attack().tile[0]; }
    long GetAttackTileY() const { return this->Attack().tile[1]; }
    bool IsConvertAttack() const { return this->Attack().convert != 0; }
    unsigned long GetSidestepCount() const { return this->Attack().sidestep; }
    void SetSidestepCount(unsigned long inCount)
    {
        EeCombatAttackGoal theRecord = this->Attack();
        theRecord.sidestep = (uint32_t)inCount;
        ee_combat_attack_set(H(this), &theRecord);
    }
    // a heal (kGoalHealUnit; EEUGHealUnit(inTarget, inArg): the location goal at the healee's position, the healee
    // referenced; the load's EEUGHealUnit(inArg): no healee, at 0, 0, 0), its healee, and the location it was made at
    // (the heal goal derived from EEUGLocation until it moved: EELocationGoalPoint reads it)
    static EEUnitGoal* NewHealUnit(EEComplexUnit* inTarget, bool inArg) { return New(15, inArg, EE_COMBAT_GOAL_NEW_HEAL, H(inTarget)); }
    static EEUnitGoal* NewHealUnit(bool inArg)
    {
        EeUnitsGoalIo theArgs = {{2}};
        return New(15, inArg, EE_COMBAT_GOAL_NEW_HEAL, 0, 0, &theArgs);
    }
    EEComplexUnit* GetHealTarget() const
    {
        EeCombatHealGoal theRecord;
        ee_combat_heal_get(H(this), &theRecord);
        return Of<EEComplexUnit>(theRecord.target);
    }
    GE3DPoint GetHealLocation() const;
    void SetHealLocation(const GE3DPoint& inLocation);
    // a load goal (kGoalLoadTransport, kGoalLoadGarrison): the container and the units, all referenced; inUnits is
    // sorted by id and loses its duplicates, as the original constructors did to the caller's list
    // (EEUGLoadTransport/EEUGLoadGarrison(inContainer, inUnits, inArg)); or the load's empty goal
    static EEUnitGoal* NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                               bool inArg);
    static EEUnitGoal* NewLoad(unsigned char inKind, bool inArg) { return New(inKind, inArg, EE_UNITS_GOAL_LOAD_NEW_EMPTY); }
    // the container, its and the units' rendezvous tiles, the garrison's valid flag (a building it can upgrade or a
    // capitol); the units to load (each added unless listed)
    EEComplexUnit* GetLoadContainer() { return Of<EEComplexUnit>(this->Call(EE_UNITS_GOAL_LOAD_FIELDS)); }
    U2DPoint<long> GetLoadContainerTile() { return U2DPoint<long>((long)this->Field(EE_UNITS_GOAL_LOAD_FIELDS, 0), (long)this->Field(EE_UNITS_GOAL_LOAD_FIELDS, 1)); }
    U2DPoint<long> GetLoadUnitTile() { return U2DPoint<long>((long)this->Field(EE_UNITS_GOAL_LOAD_FIELDS, 2), (long)this->Field(EE_UNITS_GOAL_LOAD_FIELDS, 3)); }
    bool IsLoadGarrisonValid() { return this->Field(EE_UNITS_GOAL_LOAD_FIELDS, 4) != 0; }
    unsigned long GetNumLoadUnits() { return (unsigned long)this->Call(EE_UNITS_GOAL_LOAD_UNIT_COUNT); }
    bool HasLoadUnit(EEComplexUnit* inUnit) { return this->Call(EE_UNITS_GOAL_LOAD_HAS_UNIT, H(inUnit)) != 0; }
    void AddLoadUnit(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_LOAD_ADD_UNIT, H(inUnit)); }
    void AddLoadUnits(const std::vector<EEComplexUnit*>& inUnits)
    {
        for (size_t i = 0; i < inUnits.size(); i++)
            this->AddLoadUnit(inUnits[i]);
    }

    // a build/repair goal (kGoalBuildRepair): the player's goal on the building's first part, referenced
    // (EEUGBuildRepair(inPlayer, inTarget, inArg)); or the load's empty goal (EEUGBuildRepair(inArg))
    static EEUnitGoal* NewBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg)
    {
        return New(4, inArg, EE_UNITS_GOAL_NEW_BUILD, H(inPlayer), H(inTarget));
    }
    static EEUnitGoal* NewBuildRepair(bool inArg) { return New(4, inArg, EE_UNITS_GOAL_BUILD_NEW_EMPTY); }
    // the building (NULL once dropped), its class's type, technology, tech tree node and class as the goal took
    // them; whether units other than the building block its footprint
    EEUnit* GetBuildTarget() { return Of<EEUnit>(this->Call(EE_UNITS_GOAL_BUILD_FIELDS)); }
    long GetBuildTargetType() { return (long)this->Field(EE_UNITS_GOAL_BUILD_FIELDS, 0); }
    long GetBuildTechTreeID() { return (long)this->Field(EE_UNITS_GOAL_BUILD_FIELDS, 1); }
    EEUnitClass* GetBuildTargetClass() { return Of<EEUnitClass>(this->Field(EE_UNITS_GOAL_BUILD_FIELDS, 3)); }
    bool IsBuildSiteTaken() { return this->Call(EE_UNITS_GOAL_BUILD_SITE_TAKEN) != 0; }

    // a gather goal (kGoalGather): the resource and the storage, both referenced (EEUGGather(inResource, inStorage,
    // inArg)); or the load's empty goal (EEUGGather(inArg))
    static EEUnitGoal* NewGather(EEUnit* inResource, EEUnit* inStorage, bool inArg)
    {
        return New(6, inArg, EE_UNITS_GOAL_NEW_GATHER, H(inResource), H(inStorage));
    }
    static EEUnitGoal* NewGather(bool inArg) { return New(6, inArg, EE_UNITS_GOAL_GATHER_NEW_EMPTY); }
    // the resource, the storage, the gathering and waiting flags, the destination; a new resource or storage (the
    // references moved)
    EEUnit* GetGatherResource() { return Of<EEUnit>(this->Call(EE_UNITS_GOAL_GATHER_FIELDS)); }
    EEUnit* GetGatherStorage() { return Of<EEUnit>(this->Field(EE_UNITS_GOAL_GATHER_FIELDS, 0)); }
    bool IsGathering() { return this->Field(EE_UNITS_GOAL_GATHER_FIELDS, 1) != 0; }
    bool IsGatherWaiting() { return this->Field(EE_UNITS_GOAL_GATHER_FIELDS, 2) != 0; }
    void SetGatherStorage(EEUnit* inStorage) { this->Call(EE_UNITS_GOAL_GATHER_SET_STORAGE, H(inStorage)); }
    // a waiting area by the resource (true when found); an unfinished living settlement within twice the
    // gatherer's line of sight (NULL if none)
    bool FindGatherSpot(EEComplexUnit* inUnit) { return this->Call(EE_UNITS_GOAL_GATHER_FIND_SPOT, H(inUnit)) != 0; }

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
