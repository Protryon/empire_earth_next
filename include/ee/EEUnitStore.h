// New (port): the unit store, stage one (docs/RUST_PLAN.md R5b.4 leaf 5). Each unit's plain state is a Rust record
// (rust/crates/ee_sim/src/units/store.rs), readable by id without a call into C++, while the unit is in a player's list:
// RTSPlayer::InsertUnit inserts it (EEUnitStoreInsert: seeded whole from the object), RTSPlayer::RemoveKilledUnits
// removes it, ~RTSPlayer drops the player's records.
//
// The plain members it mirrors are the Rust records the unit classes derive from (R5b.4, round 26: ee_sim world
// unit_fields.rs EeUnitsRtsUnitFields, EeUnitsUnitFields, EeUnitsComplexUnitFields): a record keeps its object's record
// addresses and every read of the store takes the values from them (store.rs refresh). The pointer members it mirrors
// (EEUnit::mUnitClass, mComplexUnit; EEComplexUnit::mActiveGroup, mContainer, mCommand, mGoalState, mHerd) are read the
// same way since round 27, at their offsets in the object (the world's layout, ee/EEWorldLayout.h, registered once):
// what the store keeps of them is derived: the class's category, type, domains, flags (its record,
// EeUnitsUnitClassFields), the goal's kind and id, the complex flag, set or not for the group, container, herd, the
// command's type. Only the location is still reported (EEUnitStoreModel).
//
// TEST ONLY: EEUnitStoreCheck compares every unit of every player's list with its record (the simulation capture's
// USCK record; a mismatch stops EE-SIM, exit code 3).
#ifndef EE_EEUNITSTORE_H
#define EE_EEUNITSTORE_H

#include <stddef.h>
#include <stdint.h>

class RTSUnit;
class RTSPlayer;

// the location's report (store.rs EE_UNIT_FIELD_LOCATION_*; EEUnitStoreModel)
enum EEUnitStoreFieldID {
    kUnitFieldLocation = 17     // EEComplexUnit::mPhysicalModel's location (EEUnitStoreModel; the record's x, y, z)
};

// the location was written (EEUnitStore.cpp: the owner from the member's offset)
void EEUnitStoreWrote(unsigned int inField, const void* inMember);

// the unit enters inPlayer's list (its id inOwner; after the unit's mPlayer and id are final) / leaves it (also at
// RTSUnit::FreeNode, a no-op unless a teardown dropped the list's reference first)
void EEUnitStoreInsert(RTSUnit* inUnit, unsigned long inOwner, const RTSPlayer* inPlayer);
void EEUnitStoreRemove(RTSUnit* inUnit);
// a player object goes (~RTSPlayer): the records of its list go, by the object (temporary players may share an id), and
// its diplomacy table (ee_units players.rs)
void EEUnitStoreClearPlayer(const RTSPlayer* inPlayer);
class EEPlayer;
// the player's mEnemies (and id) were written: its diplomacy table in the store (ee_units players.rs; EEPlayer::Reset,
// the loads, SetDiplomaticStance; USCK compares the world's players with the tables)
void EEUnitStorePlayerTable(const EEPlayer* inPlayer);
// the unit was linked into or unlinked from the world's unit chain (RTSUnit::Link, RTSUnit::Unlink): the record's
// on-the-map flag (store.rs EE_UNIT_FIELD_ON_MAP), the domain of the out-of-sync checksum's unit walk
void EEUnitStoreLinked(RTSUnit* inUnit);
// New (R5b.4, the fifteenth round): the goal queues as goal ids (ee_units goals.rs). A queue container's deque
// changed (EEGoalDeque's writes): its goals' ids to the mirror; its destructor drops it.
class EEUnitVectorItem;
void EEGoalQueueReport(EEUnitVectorItem* inQueue);
void EEGoalQueueDropped(EEUnitVectorItem* inQueue);
// New (R5b.4, the fourteenth round): a unit of id 0xffffffff in no list (a shot: CreateUnit with no list): its record
// in the store's unlisted table by object until a list takes it or its destructor. The store keeps every unit of id
// 0xffffffff there, listed or not (a scenario's path points are listed, all with that id): records by id cannot hold
// them. The combat reads' by-object lookup, the out-of-sync checksum's flags and the on-map check read both tables;
// holders write through to it by object.
void EEUnitStoreInsertUnlisted(RTSUnit* inUnit);

// the units stepped every tick (EEWorld::mUpdateUnits; ee_units order.rs, the same ids in the same order):
// EEUnitOrderReset copies the deque whole (after a load's reorder, EEPhysics::Tick's after-load fixups). USCK
// compares the two.
void EEUnitOrderReset();

class EEPlayer;
// the box selection's pick (EEPlayer::SelectUnits): the ids of inIDs (inCount) the selection takes, in order (an
// invalid unit at each of its ids: the group's Add skips it); outIDs has room for inCount; the count
size_t EESelectPick(EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount, uint32_t* outIDs);
// (the pick's and the Fill test's USPK and USFL records and sweeps: ee_sim units/pick_sweep.rs; the Fill test:
// ee_units_fill_members)

// TEST ONLY: the step leaves' record (USTM, R5b.4 leaf 6; ee_units step.rs): ee_units_note_step adds one call of a leaf
// (its kind, the unit's id, its inputs and outputs as 32-bit words) to the tick's digest; ee_units_note_step_commit (the end of
// EEPhysics::UpdateUnits) writes the tick's record: the tick, the calls, their FNV-1a. Words: reload / target search
// [timer, tick ms, written, timer after]; heal search the same with float bits (the tick time, not ms); power
// [regeneration, maximum, cinematic, land building, built, power, timer, tick ms, interval, written, power set, power
// after, timer after]. Nothing is noted unless the capture runs.
enum EEUnitStepKind {
    kUnitStepReload = 1,        // EEComplexUnit::UpdateReloadTimer
    kUnitStepTargetSearch = 2,  // EEComplexUnit::UpdateTargetSearchTimer
    kUnitStepHealSearch = 3,    // EEComplexUnit::UpdateHealSearchTimer
    kUnitStepPower = 4,         // EEUnitClass::RegeneratePower
    // the eleventh round's: rotting [time, tick ms, time after]; resources rotting [time, amount, tick ms, time after,
    // amount after]; camouflage timers [timer, second timer (mU32C), no vital building, tick ms, then each event of
    // the decision in order (ee_units step.rs EE_UNITS_CAMO_*: a mini map move with 0x100 when it was made, a value
    // write followed by the value), timer after, second timer after] (noted only when a timer runs); cloak fade
    // [opacity, cloaked, tick time, capitol pulse, game time, opacity after] (float bits)
    kUnitStepRotting = 5,       // EEComplexUnit::UpdateRottingAwayTime (the classes' kActionUnitRotting countdowns)
    kUnitStepResourcesRotting = 6,   // EEComplexUnit::UpdateResourcesRotting (kActionResourcesRotting)
    kUnitStepCamouflage = 7,    // EEUnitClass::UpdateCamouflageTimer's timers
    kUnitStepCloakFade = 8      // EEUnitClass::UpdateCamouflageTimer's cloak opacity
};
// TEST ONLY: the unit step's state record (USST, R5b.4 leaf 6, the eleventh round): ee_units_note_state adds one call
// (its kind, the unit's id, its words) to the tick's second digest, written by ee_units_note_step_commit as a USST record (the
// tick, the calls, their FNV-1a). Words: AIStep [command type, next trigger time, game time, path (1 the gather goal
// restarted its action, 2 no command, 3 an idle unit waiting, 4 evaluated), the gather's action, the first and the
// second evaluation's results (0xffffffff: none), command type after, next trigger time after]; a trigger's
// condition [trigger id, the class's answer]; the roll's decay [roll, tick seconds, roll after]; the collision push
// [push x, push y, collision period, maximum velocity, tick seconds, location x, y, moving, past the move distance, dx,
// dy, x, y] (floats as bits)
enum EEUnitStateKind {
    kUnitStateAIStep = 1,       // EEComplexUnit::AIStep
    kUnitStateCondition = 2,    // EEUnitTrigger::Evaluate's EvaluateCondition calls
    kUnitStateRoll = 3,         // EEPhysicsModel::AfterStep's roll decay
    kUnitStatePush = 4,         // EEPhysicsModel::Move's push arithmetic (and the subclasses' copies: Helicopter,
                                // Naval, Spaceship, SubmergedNaval; the helicopter's speed is its velocity)
    // the twelfth round's: the physics models' step arithmetic (float bits; ee_units physics.rs's inputs in its
    // structs' order, then the outputs). Turn [tick seconds, ticks per game second, angular acceleration, maximum, its
    // inverse, yaw rate, heading, target, use facing, broadside, facing offset, angle left, rate, heading after];
    // spin down and steady turn [tick seconds, acceleration, maximum, rate, heading, (steady: physics order, command,)
    // rate after, heading after]; Accelerate [tick seconds, speed, top speed, acceleration, waypoints, distance, mode,
    // speed after]; SpeedMode [helicopter, tick seconds, ticks per game second, speed, top speed, acceleration, its
    // inverse, yaw rate, slope penalty (0: the helicopter's), waypoints, the remaining distance, the terrain pitch (0:
    // not read), distance, turn, mode]; PitchMode [naval, tick seconds, ticks per game second, acceleration, its
    // inverse (0: naval), rate, angle, mode]; ApproachPitch [naval, tick seconds, acceleration, maximum, rate, pitch,
    // target, mode, rate after, pitch after]
    kUnitStateTurn = 5,         // EEPhysicsModel::TurnTowardAngle (with YawMode)
    kUnitStateSpinDown = 6,     // EEPhysicsModel::SpinDown
    kUnitStateSteadyTurn = 7,   // EEPhysicsModel::SteadyTurn
    kUnitStateAccelerate = 8,   // EEPhysicsModel::Accelerate, EEPMSeaAnimal::Accelerate
    kUnitStateSpeedMode = 9,    // EEPhysicsModel::SpeedMode, EEPMHelicopter::SpeedMode
    kUnitStatePitchMode = 10,   // EEPhysicsModel::PitchMode, EEPMNaval::PitchMode, EEPMSpaceship::PitchMode
    kUnitStateApproachPitch = 11,  // EEPhysicsModel::ApproachPitch, EEPMNaval::ApproachPitch
    // the thirteenth round's: a class Update's leaf calls in order, three words each [operation (ee_units update.rs
    // EE_UNITS_UPD_*), its two arguments], noted by the Update host (EEUnitUpdate.cpp) and written once the unit's
    // step is done (ee_units_note_update_commit)
    kUnitStateUpdate = 12,
    // a class ActionChanged's operations in order, three words each [operation (ee_units actions.rs
    // EE_UNITS_ACT_*), its two arguments], the nested calls (another part's, the idle restart) with them, written once
    // the outermost call returns (ee_units_note_action_begin/end)
    kUnitStateActionChanged = 13
};
// (the notes and their records: ee_sim units/notes.rs, ee_units_note_*)

// TEST ONLY: every unit of every player's list against its record, the world's unit chain against the records'
// on-the-map flags, the out-of-sync checksum's flags 1 and 2 over the chain against the store's, and the stepped units'
// deque against the Rust list (EEUnitOrder*); the count compared.
// inWhere names the point (0: after a physics tick, else the executed message's type). A mismatch prints the unit and
// field and exits 3.
unsigned long EEUnitStoreCheck(unsigned long inWhere);

#endif
