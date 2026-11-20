// C:\EEX\Empire Earth\Player\EEWUnitsOffMapInfo.h (RECONSTRUCTION; file name inferred). A unit kept off the map:
// it holds a reference on the unit; a saved one keeps the unit id in mUnit until RepairPointers looks it up.
// port (the game core lane, round 5): the record is Rust's (ee_units player_lists.rs EeUnitsOffMapInfo: mLocation,
// mUnit, mTime, mReason, their C++ names and types), owned by the player's lists (EEPlayer::mLists) with its Load,
// Save and RepairPointers; the callers set mReason through GetOffMapInfo
#ifndef EE_EEWUNITSOFFMAPINFO_H
#define EE_EEWUNITSOFFMAPINFO_H

#include "rust/sim/units.h"

typedef EeUnitsOffMapInfo EEWUnitsOffMapInfo;

#endif
