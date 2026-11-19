// EEPPatrolGroup (RECONSTRUCTION; file name inferred): the patrol points a group of units walks (RTTI EEPPatrolGroup >
// UReferenceCounter; ctor 0x16066d). Held by a patrolling EEUGPatrol, by EEComplexUnit::mPatrolGroup, by EETown and
// by EEPlayer's list.
// port (the units' state lane, round 1): the class is gone. A patrol group is Rust's object (ee_units
// patrol_group.rs: its reference count, points, id and flags, every member's body); EEPPatrolGroup is its name here
// and the holders call the ee_units_pg_* entry points (Reference, Dereference, GetNearest, ...). EEPPatrolGroup.cpp
// is the C++ the bodies call.
#ifndef EE_EEPPATROLGROUP_H
#define EE_EEPPATROLGROUP_H

#include <vector>

#include "rust/sim/units.h"

typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
template <class T> class U2DPoint;

// SetPatrolPoints (0x160da9; EETown): the route from inPoints for player inPlayerID
void EEPGSetPatrolPoints(EEPPatrolGroup* inGroup, const std::vector<U2DPoint<long> >& inPoints, unsigned long inPlayerID);

#endif
