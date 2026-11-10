//! The continent manager's region fills' start tests (`EEAIContinentManager::CreatePathAreaKind`, 0x4204ea, and
//! `CreateContinents`' corner test, 0x41f8ae), line by line: which map tile starts a water body, land mass, space
//! region or surface region fill (by its terrain, its place against the map's diagonal edges, its path area's kind
//! and region ids and, for an obstacle area, a live dock or space dock on it), and whether a fill checks corners.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): wrapping `long` arithmetic as `i32`, the C++'s tests in order
//! (its `theFlag` carries from one tile to the next, as the C++'s did, though only kinds 1..4 come here).

use crate::area_goals::{KIND_FOREST, KIND_MAP_EDGE, KIND_SPACE, KIND_UNIT_OBSTACLE, KIND_WATER};
use crate::goals::{TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::TERRAIN_SHALLOW_WATER;
use crate::tile_kind::EeTileUnit;

/// `kAIPathWaterBody`, `kAIPathLandMass`, `kAIPathSpaceRegion`, `kAIPathNonSpace` (ee/EEAIKindIDs.h).
pub const PATH_WATER_BODY: i32 = 1;
pub const PATH_LAND_MASS: i32 = 2;
pub const PATH_SPACE_REGION: i32 = 3;
pub const PATH_NON_SPACE: i32 = 4;

/// The map's diagonal edge lines (`EEMap::mL195608`, `mL195610`, `mL19560C`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeMapEdges {
    pub sum_low: i32,
    pub half: i32,
    pub sum_high: i32,
}

impl EeMapEdges {
    /// Whether `(x, y)` lies on none of the four diagonal edge lines.
    pub fn off_edges(&self, x: i32, y: i32) -> bool {
        x.wrapping_add(y) != self.sum_low
            && y.wrapping_sub(x) != self.half
            && x.wrapping_add(y) != self.sum_high
            && x.wrapping_sub(y) != self.half
    }
}

/// A fill's `mCheckCorners` at a start tile: not of terrain `no_corners` (`kTerrainCliff` in `CreateContinents`;
/// the literal 3 in `CreatePathAreaKind`, kept) and off the diagonal edges.
pub fn check_corners(terrain: i32, no_corners: i32, x: i32, y: i32, edges: &EeMapEdges) -> bool {
    terrain != no_corners && edges.off_edges(x, y)
}

/// The tile's path area as the start test reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePathAreaIDs {
    /// the area exists
    pub present: bool,
    pub kind: i32,
    pub land_mass: u16,
    pub water_body: u16,
    pub space_region: u16,
    pub surface_region: u16,
}

/// One tile of `CreatePathAreaKind`'s loop: whether a fill of `kind` starts there. `flag` is the C++'s `theFlag`,
/// carried from the previous tile and updated here.
#[allow(clippy::too_many_arguments)]
pub fn starts_fill(
    kind: i32,
    terrain: i32,
    x: i32,
    y: i32,
    edges: &EeMapEdges,
    area: &EePathAreaIDs,
    units: &[EeTileUnit],
    flag: &mut bool,
) -> bool {
    if kind == PATH_WATER_BODY {
        if terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SHALLOW_WATER {
            return false;
        }
        *flag = true;
    } else if kind == PATH_SPACE_REGION {
        *flag = terrain == TERRAIN_SPACE;
        if !*flag {
            return false;
        }
    } else if kind == PATH_LAND_MASS {
        if terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SPACE {
            return false;
        }
        *flag = true;
    } else {
        if kind == PATH_NON_SPACE {
            *flag = terrain != TERRAIN_SPACE && edges.off_edges(x, y);
        }
        if !*flag {
            return false;
        }
    }
    if !area.present {
        return false;
    }
    let live = |u: &&EeTileUnit| u.alive && !u.foundation;
    match kind {
        PATH_WATER_BODY => {
            if area.kind == KIND_FOREST || area.water_body != 0 {
                false
            } else if area.kind == KIND_WATER {
                true
            } else if area.kind <= KIND_MAP_EDGE {
                false
            } else if area.kind < KIND_UNIT_OBSTACLE {
                true
            } else if area.kind == KIND_UNIT_OBSTACLE {
                units.iter().filter(live).any(|u| u.dock)
            } else {
                false
            }
        }
        PATH_LAND_MASS => area.kind != KIND_FOREST && area.land_mass == 0,
        PATH_SPACE_REGION => {
            if area.kind == KIND_FOREST || area.space_region != 0 {
                false
            } else if area.kind == KIND_UNIT_OBSTACLE {
                units.iter().filter(live).any(|u| u.space_dock)
            } else {
                area.kind == KIND_SPACE
            }
        }
        PATH_NON_SPACE => area.surface_region == 0,
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::area_goals::{KIND_CLIFF, KIND_LAND, KIND_NEAR_SHORE, KIND_SHORE, KIND_WALL};

    const EDGES: EeMapEdges = EeMapEdges { sum_low: 10, half: 10, sum_high: 30 };

    fn area(kind: i32) -> EePathAreaIDs {
        EePathAreaIDs { present: true, kind, ..Default::default() }
    }

    fn run(kind: i32, terrain: i32, area: &EePathAreaIDs, units: &[EeTileUnit]) -> bool {
        let mut flag = false;
        starts_fill(kind, terrain, 8, 9, &EDGES, area, units, &mut flag)
    }

    #[test]
    fn edges_and_corners() {
        assert!(EDGES.off_edges(8, 9));
        assert!(!EDGES.off_edges(4, 6));
        assert!(!EDGES.off_edges(15, 15));
        assert!(!EDGES.off_edges(2, 12));
        assert!(!EDGES.off_edges(12, 2));
        assert!(check_corners(0, 9, 8, 9, &EDGES));
        assert!(!check_corners(9, 9, 8, 9, &EDGES));
        assert!(!check_corners(0, 9, 12, 2, &EDGES));
        // the wrapping sums never trap
        assert!(!EeMapEdges { sum_low: i32::MIN, half: 0, sum_high: 0 }.off_edges(i32::MAX, 1));
    }

    #[test]
    fn water_bodies() {
        assert!(run(PATH_WATER_BODY, 1, &area(KIND_WATER), &[]));
        assert!(run(PATH_WATER_BODY, 10, &area(KIND_SHORE), &[]));
        assert!(run(PATH_WATER_BODY, 10, &area(KIND_NEAR_SHORE), &[]));
        assert!(!run(PATH_WATER_BODY, 0, &area(KIND_WATER), &[]));
        assert!(!run(PATH_WATER_BODY, 1, &area(KIND_FOREST), &[]));
        assert!(!run(PATH_WATER_BODY, 1, &area(KIND_CLIFF), &[]));
        assert!(!run(PATH_WATER_BODY, 1, &area(KIND_WALL), &[]));
        assert!(!run(PATH_WATER_BODY, 1, &EePathAreaIDs { water_body: 3, ..area(KIND_WATER) }, &[]));
        assert!(!run(PATH_WATER_BODY, 1, &EePathAreaIDs { present: false, ..area(KIND_WATER) }, &[]));
        let dock = EeTileUnit { dock: true, alive: true, ..Default::default() };
        assert!(run(PATH_WATER_BODY, 10, &area(KIND_UNIT_OBSTACLE), &[dock]));
        assert!(!run(PATH_WATER_BODY, 10, &area(KIND_UNIT_OBSTACLE), &[EeTileUnit { foundation: true, ..dock }]));
        assert!(!run(PATH_WATER_BODY, 10, &area(KIND_UNIT_OBSTACLE), &[EeTileUnit { alive: false, ..dock }]));
    }

    #[test]
    fn land_space_and_surface() {
        assert!(run(PATH_LAND_MASS, 0, &area(KIND_LAND), &[]));
        assert!(run(PATH_LAND_MASS, 10, &area(KIND_CLIFF), &[]));
        assert!(!run(PATH_LAND_MASS, 1, &area(KIND_LAND), &[]));
        assert!(!run(PATH_LAND_MASS, 12, &area(KIND_LAND), &[]));
        assert!(!run(PATH_LAND_MASS, 0, &area(KIND_FOREST), &[]));
        assert!(!run(PATH_LAND_MASS, 0, &EePathAreaIDs { land_mass: 1, ..area(KIND_LAND) }, &[]));
        assert!(run(PATH_SPACE_REGION, 12, &area(KIND_SPACE), &[]));
        assert!(!run(PATH_SPACE_REGION, 12, &area(KIND_SHORE), &[]));
        assert!(!run(PATH_SPACE_REGION, 0, &area(KIND_SPACE), &[]));
        let station = EeTileUnit { space_dock: true, alive: true, ..Default::default() };
        assert!(run(PATH_SPACE_REGION, 12, &area(KIND_UNIT_OBSTACLE), &[EeTileUnit::default(), station]));
        assert!(run(PATH_NON_SPACE, 0, &area(KIND_FOREST), &[]));
        assert!(!run(PATH_NON_SPACE, 12, &area(KIND_LAND), &[]));
        assert!(!run(PATH_NON_SPACE, 0, &EePathAreaIDs { surface_region: 2, ..area(KIND_LAND) }, &[]));
        let mut flag = true;
        assert!(!starts_fill(PATH_NON_SPACE, 0, 4, 6, &EDGES, &area(KIND_LAND), &[], &mut flag));
        assert!(!flag);
    }

    #[test]
    fn the_flag_carries() {
        // a kind outside 1..4 keeps the previous tile's flag
        let mut flag = false;
        assert!(!starts_fill(5, 0, 8, 9, &EDGES, &area(KIND_LAND), &[], &mut flag));
        flag = true;
        assert!(!starts_fill(5, 0, 8, 9, &EDGES, &area(KIND_LAND), &[], &mut flag));
        assert!(flag);
    }
}
