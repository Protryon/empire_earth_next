// New (port; docs/RUST_PLAN.md R5b.2, the fifteenth round): the map's unit regions are a Rust table
// (rust/crates/ee_sim/src/combat regions.rs; was RTSRegionTable<EEComplexUnit*>, EEMap::mUnitRegions). The table's C++
// interface is rust/sim/combat.h (ee_combat_regions_*); the collector's walk is ee/EEUnitCollector.h's.
#ifndef EE_EEUNITREGIONS_H
#define EE_EEUNITREGIONS_H

#include <stdint.h>
#include <vector>


#endif
