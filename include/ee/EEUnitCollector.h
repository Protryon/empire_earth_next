// EEUnitCollector.h (RECONSTRUCTION; file name inferred). The map unit-region collector template
// shared by EEAIArea.cpp (0xdcbe/0xde7f) and EEHerd.cpp (0xa509a), moved out of EEHerd.cpp. Includers provide gWorld,
// EEMap, RTSRegionTable and EEUnit (ee/EEWorld.h, ee/EEMap.h, ee/RTSRegionTable.h, ee/EEUnit.h).
#ifndef EE_EEUNITCOLLECTOR_H
#define EE_EEUNITCOLLECTOR_H

#include <vector>
#include <functional>
#include <algorithm>
#include <stdint.h>
#include "lle/U2DRectangle.h"
#include "rust/sim/combat.h"             // the map's unit regions (ee_combat regions.rs)
#include "rust/sim/units.h"              // the collector (ee_sim units uu_ffi.rs)

// (Units by id, the collector's sort-and-unique (0x23dd96 / 0x23ddab, 0xe0b9 / 0xe104) and the walk of the map's unit
// regions: ee_sim units uu_reads.rs units_in_rectangle)

// the collector's predicate as the Rust walk calls it (inPredicate: the caller's Pred)
template <class Pred> uint8_t EECollectorKeep(const void* inPredicate, uint64_t inUnit)
{
    return (*static_cast<const Pred*>(inPredicate))(reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inUnit)));
}

// cdecl template 0xa509a (EEAIArea.cpp's 0xdcbe/0xde7f with another predicate): the units on the map's unit regions
// under inRectangle that inPredicate holds for, appended to outUnits (a std::vector of unit pointers); sorted and made
// unique when inUnique and a region held a unit of a class bigger than one region (+0x28c > 1). Returns their number.
// The walk (column by column, each region from its last unit), the test of the unit's tile and the sort are
// uu_ffi.rs ee_units_uu_collect_units
template <class Pred, class V>
long EEGetUnitsInRectangle(const U2DRectangle<long>& inRectangle, V& outUnits, const Pred& inPredicate, bool inUnique)
{
    return ee_units_uu_collect_units((int32_t)inRectangle.mLeft, (int32_t)inRectangle.mTop, (int32_t)inRectangle.mRight, (int32_t)inRectangle.mBottom, &EECollectorKeep<Pred>, &inPredicate, &outUnits, inUnique);
}

#endif
