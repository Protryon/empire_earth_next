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
class EEPlayer;
typedef EEPlayer RTSPlayer;

// EEComplexUnit::mPhysicalModel's location was written (EEUnitStoreModel): the record's x, y, z (store.rs
// EE_UNIT_FIELD_LOCATION_*; EEUnitStore.cpp: the owner from the member's offset)
void EEUnitStoreMoved(const void* inModel);

// the unit enters inPlayer's list (its id inOwner; after the unit's mPlayer and id are final) / leaves it (also at
// RTSUnit::FreeNode, a no-op unless a teardown dropped the list's reference first)
// a player object goes (~RTSPlayer): the records of its list go, by the object (temporary players may share an id), and
// its diplomacy table (ee_units players.rs)
class EEPlayer;


// (the box selection's pick and the Fill test: ee_sim units/pick_sweep.rs; the step and state notes' kinds (USTM,
// USST): ee_sim units/notes.rs, ee_units_note_*)


#endif
