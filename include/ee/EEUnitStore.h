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
// New (units round 11): the entry points are inline calls of the Rust bodies (us_fill.rs).
//
// TEST ONLY: EEUnitStoreCheck compares every unit of every player's list with its record (the simulation capture's
// USCK record; a mismatch stops EE-SIM, exit code 3).
#ifndef EE_EEUNITSTORE_H
#define EE_EEUNITSTORE_H

#include <stddef.h>
#include <stdint.h>

#include "rust/sim/units.h"             // the entry points (ee_sim units us_fill.rs, us_check.rs)

class RTSUnit;
class RTSPlayer;

// EEComplexUnit::mPhysicalModel's location was written (EEUnitStoreModel): the record's x, y, z (store.rs
// EE_UNIT_FIELD_LOCATION_*; EEUnitStore.cpp: the owner from the member's offset)
void EEUnitStoreMoved(const void* inModel);

// the unit enters inPlayer's list (its id inOwner; after the unit's mPlayer and id are final) / leaves it (also at
// RTSUnit::FreeNode, a no-op unless a teardown dropped the list's reference first)
inline void EEUnitStoreInsert(RTSUnit* inUnit, unsigned long inOwner, const RTSPlayer* inPlayer) { ee_units_store_insert_object((uint64_t)(uintptr_t)inUnit, (uint32_t)inOwner, (uint64_t)(uintptr_t)inPlayer); }
inline void EEUnitStoreRemove(RTSUnit* inUnit) { ee_units_store_remove_object((uint64_t)(uintptr_t)inUnit); }
// a player object goes (~RTSPlayer): the records of its list go, by the object (temporary players may share an id), and
// its diplomacy table (ee_units players.rs)
inline void EEUnitStoreClearPlayer(const RTSPlayer* inPlayer) { ee_units_store_clear_player_object((uint64_t)(uintptr_t)inPlayer); }
class EEPlayer;
// the player's mEnemies (and id) were written: its diplomacy table in the store (ee_units players.rs; EEPlayer::Reset,
// the loads, SetDiplomaticStance; USCK compares the world's players with the tables)
inline void EEUnitStorePlayerTable(const EEPlayer* inPlayer) { ee_units_store_player_table((uint64_t)(uintptr_t)inPlayer); }
// the unit was linked into or unlinked from the world's unit chain (RTSUnit::Link, RTSUnit::Unlink): the record's
// on-the-map flag (store.rs EE_UNIT_FIELD_ON_MAP), the domain of the out-of-sync checksum's unit walk
inline void EEUnitStoreLinked(RTSUnit* inUnit) { ee_units_store_linked((uint64_t)(uintptr_t)inUnit); }
// New (R5b.4, the fourteenth round): a unit of id 0xffffffff in no list (a shot: CreateUnit with no list): its record
// in the store's unlisted table by object until a list takes it or its destructor. The store keeps every unit of id
// 0xffffffff there, listed or not (a scenario's path points are listed, all with that id): records by id cannot hold
// them. The combat reads' by-object lookup, the out-of-sync checksum's flags and the on-map check read both tables;
// holders write through to it by object.
inline void EEUnitStoreInsertUnlisted(RTSUnit* inUnit) { ee_units_store_insert_unlisted_object((uint64_t)(uintptr_t)inUnit); }

// the units stepped every tick (EEWorld::mUpdateUnits; ee_units order.rs, the same ids in the same order):
// EEUnitOrderReset copies the deque whole (after a load's reorder, EEPhysics::Tick's after-load fixups). USCK
// compares the two.
inline void EEUnitOrderReset() { ee_units_order_reset_world(); }

// (the box selection's pick and the Fill test: ee_sim units/pick_sweep.rs; the step and state notes' kinds (USTM,
// USST): ee_sim units/notes.rs, ee_units_note_*)

// TEST ONLY: every unit of every player's list against its record, the world's unit chain against the records'
// on-the-map flags, the out-of-sync checksum's flags 1 and 2 over the chain against the store's, and the stepped units'
// deque against the Rust list (EEUnitOrder*); the count compared.
// inWhere names the point (0: after a physics tick, else the executed message's type). A mismatch prints the unit and
// field and exits 3.
inline unsigned long EEUnitStoreCheck(unsigned long inWhere) { return (unsigned long)ee_units_us_check(inWhere); }

#endif
