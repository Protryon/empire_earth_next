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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/LowLevelEngineCore.h"     // UFloatBits
#include "rust/sim/unit_ids.h"        // kGoalAttackGround
#include "rust/sim/units.h"
#include "rust/sim/combat.h"

class EEComplexUnit;
class EEPlayer;
class EEUnit;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class FSFile;
class GE2DPoint;
class GE3DPoint;
class UWideString;
struct EeUnitsGoalIo;

class EEUnitGoal : public EeUnitsGoalBase {
public:
    // New (units behaviour, round 3): a goal is a Rust object (ee_units goal_obj.rs) whose head is these members:
    // made by Make (0x23c375: active, idle, kind inKind, the next goal id, stamped with the game time; its own state
    // not made yet: the New functions below make it), deleted by delete (the destructor runs the kind's destroy, the
    // memory is Rust's). The bodies that write no file or text are one ee_units_goal_call each.
    static EEUnitGoal* Make(unsigned char inKind, bool inArg) { return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new(inKind, inArg)); }
    static void* operator new(size_t) = delete;
    static void operator delete(void* inGoal) { ee_units_goal_dealloc(static_cast<EeUnitsGoalBase*>(inGoal)); }
    ~EEUnitGoal() { this->Call(EE_UNITS_GOAL_DESTROY); }

    // the original's virtuals (vtable 0x43cc64, by slot), each one call of the kind's body
    void AddCreatedUnit(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_ADD_CREATED_UNIT, H(inUnit)); }   // +0x04
    void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)   // +0x08 (the base: told to the group)
        { ee_units_goal_point(EE_UNITS_GOAL_DESTINATION, this->Base(), H(inUnit), (float(*)[3])&outLocation.mX, true); }
    // +0x0c (the gather goal reports 5000 while not active)
    bool IsActive(unsigned long* outValue) { return ee_units_goal_is_active(this->Base(), outValue); }   // goal_calls.rs
    EEUnitGoal* Clone() { return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(this->Call(EE_UNITS_GOAL_CLONE, 0, this->mB1C))); }  // +0x10
    void DumpOutOfSynch(FILE* inFile);                         // +0x14
    long GetOutOfSynchChecksum() { return (long)this->Call(EE_UNITS_GOAL_CHECKSUM); }   // +0x18
    void GetDebugLine(UWideString& ioText);                    // +0x1c
    // +0x24 (kGoalLoadTransport, kGoalLoadGarrison)
    bool IsLoadGoal() { return this->mKind == 7 || this->mKind == 12; }
    void GetGoalLocation(GE3DPoint& outLocation)               // +0x28
        { ee_units_goal_point(EE_UNITS_GOAL_LOCATION, this->Base(), 0, (float(*)[3])&outLocation.mX, true); }
    long GetStartAction() { return (long)this->Call(EE_UNITS_GOAL_START_ACTION); }            // +0x30
    long GetStartTime() { return this->mStartTime; }           // +0x34
    unsigned long GetCreationTime() { return this->mCreationTime; }   // +0x38
    void SetOwner(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_SET_OWNER, H(inUnit)); }   // +0x3c
    void Step(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_STEP, H(inUnit)); }            // +0x40
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44
    void Abandon(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_ABANDON, H(inUnit)); }        // +0x4c
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion) { return (long)this->Call(EE_UNITS_GOAL_REPAIR, inMajorVersion, inMinorVersion); }  // +0x50
    void Start(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_START, H(inUnit)); }          // +0x54
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x58
    // +0x5c: the base goal's count, then the kind's (the attack unit's: 0 also clears the sidestep count)
    void SetPathAttempts(long inValue) { ee_units_goal_set_path_attempts(EE_UNITS_GOAL_SET_PATH_ATTEMPTS, reinterpret_cast<EeUnitsGoalBase*>(this), inValue); }
    void SetStartTime(long inValue) { this->mStartTime = inValue; }   // +0x60
    bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent) { return this->Call(EE_UNITS_GOAL_EVALUATE, H(inUnit), inEvent) != 0; }  // +0x64
    void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit* inUnit) { this->Call3(EE_UNITS_GOAL_REPLACE_UNIT, H(inOld), H(inNew), (int64_t)H(inUnit)); }  // +0x6c
    bool Suits(EEUnit* inUnit) { return this->Call(EE_UNITS_GOAL_SUITS, H(inUnit)) != 0; }    // +0x70
    void Work(EEComplexUnit* inUnit) { this->Call(EE_UNITS_GOAL_WORK, H(inUnit)); }           // +0x74

    // one body of the goal with the registered world host (ee_units goal_obj.rs), an object's handle
    int64_t Call(int32_t inOperation, uint64_t inA = 0, uint64_t inB = 0, EeUnitsGoalIo* ioArgs = NULL) const { return ee_units_goal_call(inOperation, this->Base(), inA, inB, ioArgs); }
    // the same with an answer block of three integers (the bodies' {{x, y, z}} blocks: ee_units goal_calls.rs)
    int64_t Call3(int32_t inOperation, uint64_t inA, uint64_t inB, int64_t inI0, int64_t inI1 = 0, int64_t inI2 = 0) const
    {
        return ee_units_goal_call3(inOperation, this->Base(), inA, inB, inI0, inI1, inI2);
    }
    const EeUnitsGoalBase* Base() const { return reinterpret_cast<const EeUnitsGoalBase*>(this); }
    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
    // a Rust point (ee_base math types.rs Ge3DPoint) as a GE3DPoint
    static GE3DPoint PointOf(const EeGe3DPoint& inPoint) { return GE3DPoint(inPoint.mX, inPoint.mY, inPoint.mZ); }
    // the object of a handle a body answers
    template <class T> static T* Of(int64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }
    // a new goal of a kind made by its body's operation (the New functions)
    static EEUnitGoal* New(unsigned char inKind, bool inArg, int32_t inOperation, uint64_t inA = 0, uint64_t inB = 0,
                           EeUnitsGoalIo* ioArgs = NULL)
    {
        return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new_io(inKind, inArg, inOperation, inA, inB, ioArgs));
    }
    // the same with an answer block of three integers
    static EEUnitGoal* NewCall(unsigned char inKind, bool inArg, int32_t inOperation, uint64_t inA, uint64_t inB, int64_t inI0,
                               int64_t inI1 = 0, int64_t inI2 = 0)
    {
        return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new_call(inKind, inArg, inOperation, inA, inB, inI0, inI1, inI2));
    }
    // one field of a body's answer block
    int64_t Field(int32_t inOperation, int inIndex) const { return ee_units_goal_field(inOperation, this->Base(), inIndex); }
    EeUnitsLocationGoal Location() const { return ee_units_location_goal_of(H(this)); }
    EeCombatAttackGoal Attack() const { return ee_combat_attack_of(H(this)); }

    // the copies pass it through a byte temporary, the other bools straight (provisional name)
    bool GetB1C() const { return this->mB1C; }
    unsigned char GetKind() const { return this->mKind; }

    // a location goal (kGoalLocation; ee_units location.rs; the four EEUGLocation constructors): at the point (x, y, z;
    // a map position at height 0; a 3D point) with the forced-move, facing (its yaw), auto-waypoint flags and the
    // group path state; or the load's empty goal (at 0, 0, 0, yaw 0, no facing). None uses group movement.
    static EEUnitGoal* NewLocation(float inX, float inY, float inZ, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
    {
        return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new_location(inArg, inX, inY, inZ, inIsForcedMove, inShouldFace, inYaw,
                                                                        inIsAutoWaypoint, inGroupPathState));
    }
    static EEUnitGoal* NewLocation(const GE2DPoint& inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
    {
        return NewLocation(inLocation.GetX(), inLocation.GetY(), 0.0f, inArg, inIsForcedMove, inShouldFace, inYaw,
                           inIsAutoWaypoint, inGroupPathState);
    }
    static EEUnitGoal* NewLocation(const GE3DPoint* inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                   float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
    {
        return NewLocation(inLocation->GetX(), inLocation->GetY(), inLocation->GetZ(), inArg, inIsForcedMove, inShouldFace,
                           inYaw, inIsAutoWaypoint, inGroupPathState);
    }
    static EEUnitGoal* NewLocation(bool inArg, bool inIsForcedMove, bool inIsAutoWaypoint, unsigned char inGroupPathState)
    {
        return NewLocation(0.0f, 0.0f, 0.0f, inArg, inIsForcedMove, false, 0.0f, inIsAutoWaypoint, inGroupPathState);
    }
    // the location goal's members (EEUGLocation's accessors; the record's fields, no host call): its point, the yaw
    // to face, the flags and the group path state
    GE3DPoint GetLocation() const { return PointOf(ee_units_location_goal_point(H(this))); }
    void SetLocation(const GE3DPoint& inLocation) { ee_units_location_goal_set_point(H(this), inLocation.GetX(), inLocation.GetY(), inLocation.GetZ()); }
    float GetYaw() const { return this->Location().yaw; }
    bool GetIsForcedMove() const { return this->Location().forced_move; }
    bool GetIsAutoWaypoint() const { return this->Location().auto_waypoint; }
    void SetUseGroupMovement(bool inValue) { ee_units_location_goal_set_group(H(this), 0, inValue); }
    unsigned char GetGroupPathState() const { return this->Location().group_path_state; }
    void SetGroupPathState(unsigned char inValue) { ee_units_location_goal_set_group(H(this), 1, inValue); }
    // whether inUnit faces the goal's yaw (within a degree); if so it is turned to it exactly (EEUGLocation::FaceYaw)
    bool FaceYaw(EEComplexUnit* inUnit) { return this->Call(EE_UNITS_GOAL_FACE_YAW, H(inUnit)) != 0; }

    // a patrol (kGoalPatrol): a new empty patrol group, no point (EEUGPatrol(inArg)); or an existing group and the
    // point to start at, both referenced (EEUGPatrol(inGroup, inTarget, inArg))
    static EEUnitGoal* NewPatrol(bool inArg) { return New(9, inArg, EE_UNITS_GOAL_PATROL_NEW_EMPTY); }
    static EEUnitGoal* NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg) { return New(9, inArg, EE_UNITS_GOAL_NEW_PATROL, H(inGroup), H(inTarget)); }
    // the patrol's group, its current point, the group moved to inGroup (the references moved)
    EEPPatrolGroup* GetPatrolGroup() { return Of<EEPPatrolGroup>(this->Call(EE_UNITS_GOAL_PATROL_GROUP)); }
    EEComplexUnit* GetPatrolTarget() { return Of<EEComplexUnit>(this->Call(EE_UNITS_GOAL_PATROL_TARGET)); }
    void SetPatrolGroup(EEPPatrolGroup* inGroup) { this->Call(EE_UNITS_GOAL_PATROL_SET_GROUP, H(inGroup)); }

    // a cast (kGoalCastCalamity): the source casts the calamity of technology inCalamityType on the target unit or
    // at the tile, both units referenced (EEUGCastCalamity(...)); or the load's empty goal (EEUGCastCalamity(inArg))
    static EEUnitGoal* NewCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                       long inCalamityType, bool inArg)
    {
        return NewCall(11, inArg, EE_UNITS_GOAL_NEW_CALAMITY, H(inSource), H(inTarget), inTileX, inTileY, inCalamityType);
    }
    static EEUnitGoal* NewCastCalamity(bool inArg) { return New(11, inArg, EE_UNITS_GOAL_CALAMITY_NEW_EMPTY); }
    // the cast's technology (its tile and target unit when asked); where the calamity goes (the target, the source
    // for two technologies, else the tile; at the flying source's height)
    long GetCalamityType(long* outTileX = NULL, long* outTileY = NULL, EEUnit** outTarget = NULL)
    {
        return ee_units_goal_calamity_type(EE_UNITS_GOAL_CALAMITY_FIELDS, this->Base(), outTileX, outTileY,
                                           reinterpret_cast<uint64_t*>(outTarget));
    }
    void GetCalamityTargetLocation(GE3DPoint& outLocation) { ee_units_goal_point(EE_UNITS_GOAL_CALAMITY_TARGET_LOCATION, this->Base(), 0, (float(*)[3])&outLocation.mX, false); }

    // an explore goal (kGoalExplore): no tile searched yet (EEUGExplore(inArg)); the tile being explored's centre
    // (x and y only: EEUGExplore's GetTileCenter), the tile set
    static EEUnitGoal* NewExplore(bool inArg) { return New(10, inArg, EE_UNITS_GOAL_NEW_EXPLORE); }
    void GetExploreTileCenter(GE3DPoint& outLocation) { ee_units_goal_explore_center(EE_UNITS_GOAL_EXPLORE_TILE, this->Base(), (float(*)[3])&outLocation.mX); }

    // an unload (kGoalUnloadTransport): the transport (referenced) unloads at the tile, its unload tiles found when
    // it carries cargo (EEUGUnloadTransport(...)); or the load's empty goal (EEUGUnloadTransport(inArg))
    static EEUnitGoal* NewUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg) { return NewCall(8, inArg, EE_UNITS_GOAL_NEW_UNLOAD, H(inTransport), 0, inX, inY); }
    static EEUnitGoal* NewUnloadTransport(bool inArg) { return New(8, inArg, EE_UNITS_GOAL_UNLOAD_NEW_EMPTY); }
    // the transport (its unload tile when asked); another transport (the reference moved, its range taken); whether
    // the transport can unload at the tile (its class's test, else a free tile)
    EEComplexUnit* GetUnloadTransport(long* outX = NULL, long* outY = NULL) { return Of<EEComplexUnit>(ee_units_goal_unload_transport(EE_UNITS_GOAL_UNLOAD_FIELDS, this->Base(), outX, outY)); }
    void SetUnloadTransport(EEComplexUnit* inTransport) { this->Call(EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT, H(inTransport)); }
    bool CanUnloadAtDestination() { return this->Call(EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD) != 0; }

    // the combat lane's goals (ee_combat goal_objects.rs, goal_attack.rs; EEUGRust.cpp's GoalOp): an attack on the
    // ground at inTarget (kGoalAttackGround; EEUGAttackGround(inTarget, inArg), the load's at 0, 0, 0) and its point
    static EEUnitGoal* NewAttackGround(const GE3DPoint& inTarget, bool inArg)
    {
        return reinterpret_cast<EEUnitGoal*>(ee_units_goal_new_point(kGoalAttackGround, inArg, EE_COMBAT_GOAL_NEW_GROUND,
                                                                     inTarget.GetX(), inTarget.GetY(), inTarget.GetZ()));
    }
    GE3DPoint GetGroundTarget() { return PointOf(ee_combat_ground_target_point(H(this))); }
    // an attack on a unit (kGoalAttackUnit; EEUGAttackUnit(inAttacker, inTarget, inArg3, inIsConvert): a multi-part
    // target's attack part, the target referenced, the attack counts; the load's EEUGAttackUnit(inArg) empty), its
    // target, attacker, last known target tile, convert flag, destination and sidestep count
    static EEUnitGoal* NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert)
    {
        return NewCall(2, inArg3, EE_COMBAT_GOAL_NEW_ATTACK, H(inAttacker), H(inTarget), inIsConvert ? 1 : 0);
    }
    EEUnit* GetAttackTarget() const { return Of<EEUnit>(this->Attack().target); }
    EEComplexUnit* GetAttacker() const { return Of<EEComplexUnit>(this->Attack().attacker); }
    void SetAttacker(EEComplexUnit* inAttacker) { ee_combat_attack_set_field(H(this), 0, H(inAttacker)); }
    // the old SetTarget (the reference moved, the tile)
    void SetAttackTarget(EEUnit* inTarget) { this->Call(EE_COMBAT_GOAL_SET_TARGET, H(inTarget)); }
    // a gate target becomes the part EEGetAttackPart gives
    void RetargetGatePart(EEComplexUnit*) { this->Call(EE_COMBAT_GOAL_RETARGET_GATE); }
    long GetAttackTileX() const { return this->Attack().tile[0]; }
    long GetAttackTileY() const { return this->Attack().tile[1]; }
    bool IsConvertAttack() const { return this->Attack().convert != 0; }
    unsigned long GetSidestepCount() const { return this->Attack().sidestep; }
    void SetSidestepCount(unsigned long inCount) { ee_combat_attack_set_field(H(this), 1, (uint32_t)inCount); }
    // a heal (kGoalHealUnit; EEUGHealUnit(inTarget, inArg): the location goal at the healee's position, the healee
    // referenced; the load's EEUGHealUnit(inArg): no healee, at 0, 0, 0), its healee, and the location it was made at
    // (the heal goal derived from EEUGLocation until it moved: EELocationGoalPoint reads it)
    static EEUnitGoal* NewHealUnit(EEComplexUnit* inTarget, bool inArg) { return New(15, inArg, EE_COMBAT_GOAL_NEW_HEAL, H(inTarget)); }
    static EEUnitGoal* NewHealUnit(bool inArg) { return NewCall(15, inArg, EE_COMBAT_GOAL_NEW_HEAL, 0, 0, 2); }
    EEComplexUnit* GetHealTarget() const { return Of<EEComplexUnit>(ee_combat_heal_target(H(this))); }
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
    void AddLoadUnits(const std::vector<EEComplexUnit*>& inUnits) { ee_units_goal_add_load_units(this->Base(), &inUnits); }   // goal_calls.rs

    // a build/repair goal (kGoalBuildRepair): the player's goal on the building's first part, referenced
    // (EEUGBuildRepair(inPlayer, inTarget, inArg)); or the load's empty goal (EEUGBuildRepair(inArg))
    static EEUnitGoal* NewBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg) { return New(4, inArg, EE_UNITS_GOAL_NEW_BUILD, H(inPlayer), H(inTarget)); }
    static EEUnitGoal* NewBuildRepair(bool inArg) { return New(4, inArg, EE_UNITS_GOAL_BUILD_NEW_EMPTY); }
    // the building (NULL once dropped), its class's type, technology, tech tree node and class as the goal took
    // them; whether units other than the building block its footprint
    EEUnit* GetBuildTarget() { return Of<EEUnit>(this->Call(EE_UNITS_GOAL_BUILD_FIELDS)); }
    long GetBuildTechTreeID() { return (long)this->Field(EE_UNITS_GOAL_BUILD_FIELDS, 1); }

    // a gather goal (kGoalGather): the resource and the storage, both referenced (EEUGGather(inResource, inStorage,
    // inArg)); or the load's empty goal (EEUGGather(inArg))
    static EEUnitGoal* NewGather(EEUnit* inResource, EEUnit* inStorage, bool inArg) { return New(6, inArg, EE_UNITS_GOAL_NEW_GATHER, H(inResource), H(inStorage)); }
    static EEUnitGoal* NewGather(bool inArg) { return New(6, inArg, EE_UNITS_GOAL_GATHER_NEW_EMPTY); }
    // the resource, the storage, the gathering and waiting flags, the destination; a new resource or storage (the
    // references moved)
    EEUnit* GetGatherResource() { return Of<EEUnit>(this->Call(EE_UNITS_GOAL_GATHER_FIELDS)); }
    EEUnit* GetGatherStorage() { return Of<EEUnit>(this->Field(EE_UNITS_GOAL_GATHER_FIELDS, 0)); }
    // a waiting area by the resource (true when found); an unfinished living settlement within twice the
    // gatherer's line of sight (NULL if none)
    bool FindGatherSpot(EEComplexUnit* inUnit) { return this->Call(EE_UNITS_GOAL_GATHER_FIND_SPOT, H(inUnit)) != 0; }

    // the members (mIsActive +0x04 of the original, mKind, mAction, mStartTime (saved from minor version 0x9d),
    // mCreationTime (from 0xa4), mID the goal's number (from 0x64), mPathAttempts, mB1C (from 0xf), mB1D (from 0x87)):
    // the Rust record EeUnitsGoalBase, the base above (ee_sim units goal_obj.rs)
};

// EEUnitGoal::mKind's kinds: Rust's enum (ee_sim unit_ids.rs, rust/sim/unit_ids.h).
#include "rust/sim/unit_ids.h"

#endif
