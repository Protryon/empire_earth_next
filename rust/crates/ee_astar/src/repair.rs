//! The continent manager's path area repair (`EEAIContinentManager::RepairPathArea`, 0x42372b) and its region id
//! lookup (`GetAdjacentAreaID`, 0x421a18), line by line, over a snapshot of a tile's eight neighbours (the map tile,
//! its terrain, its path area's id, kind, continent, unit, size and region ids). `RepairPathArea`'s choice is a
//! plan the C++ applies: the largest same-kind neighbouring area the tile joins (and the others merged into it, in
//! the C++'s order), the single neighbour's area, or a new area; the objects, their tile lists and the sectors stay
//! C++.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): no arithmetic, the C++'s tests and loop orders, ties broken as
//! the C++ broke them (the sorted matches walked from the highest id down, the first largest kept), and its `theOK`
//! carried from one neighbour to the next as the C++'s was.

use crate::area_goals::{
    KIND_CLIFF, KIND_FOREST, KIND_GAIA_OBSTACLE, KIND_GATE, KIND_MAP_EDGE, KIND_UNIT_OBSTACLE, KIND_WALL, KIND_WATER,
};
use crate::goals::{TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::TERRAIN_SHALLOW_WATER;
use crate::path_area_kind::{PATH_LAND_MASS, PATH_NON_SPACE, PATH_SPACE_REGION, PATH_WATER_BODY};

/// The neighbours' offsets in the C++'s push order (`x - 1`, then `x`, then `x + 1`; `y - 1` before `y + 1`).
pub const NEIGHBOURS: [(i32, i32); 8] = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)];

/// One tile as the repair reads it: the map tile, then its path area (`GetPathAreaAt`; none without a map tile).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAreaTile {
    pub tile: bool,
    pub terrain: i32,
    pub area: bool,
    pub id: u16,
    pub continent: u16,
    pub kind: i32,
    pub unit_id: i32,
    /// `mTiles.Size()` (a `long`)
    pub size: i32,
    pub land_mass: u16,
    pub water_body: u16,
    pub space_region: u16,
    pub surface_region: u16,
}

/// The index in `NEIGHBOURS` of offset `(dx, dy)`.
fn index_of(dx: i32, dy: i32) -> usize {
    NEIGHBOURS.iter().position(|&o| o == (dx, dy)).expect("a neighbour offset")
}

/// 0x421a18 `GetAdjacentAreaID`: the `kind` region id of a fitting path area next to a tile (`centre`), 0 if none.
/// The land mass skips water, cliff..forest and map-edge areas unless of kind `area_kind`; the others take the
/// first neighbour of the region's terrain, from the last offset back.
pub fn adjacent_area_id(kind: i32, area_kind: i32, centre: &EeAreaTile, n: &[EeAreaTile; 8]) -> u16 {
    if kind == PATH_LAND_MASS {
        for t in n.iter().rev() {
            if !t.tile {
                continue;
            }
            if t.area
                && t.kind != KIND_WATER
                && (t.kind < KIND_CLIFF || (t.kind > KIND_FOREST && t.kind != KIND_MAP_EDGE) || area_kind == t.kind)
            {
                return t.land_mass;
            }
        }
        return 0;
    }
    let fits = |terrain: i32| {
        if kind == PATH_WATER_BODY {
            terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER
        } else {
            (kind == PATH_SPACE_REGION && terrain == TERRAIN_SPACE)
                || (kind == PATH_NON_SPACE && terrain != TERRAIN_SPACE)
        }
    };
    if !centre.tile || !fits(centre.terrain) {
        return 0;
    }
    for t in n.iter().rev() {
        if !t.tile || !fits(t.terrain) || !t.area {
            continue;
        }
        return if kind == PATH_WATER_BODY {
            t.water_body
        } else if kind == PATH_SPACE_REGION {
            t.space_region
        } else {
            t.surface_region
        };
    }
    0
}

/// What `RepairPathArea` does with the tile, as indices into `NEIGHBOURS`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeRepairPlan {
    /// `REPAIR_*`
    pub action: i32,
    /// the joined area's neighbour (`REPAIR_JOIN`, `REPAIR_SINGLE`)
    pub target: i32,
    /// the areas merged into the joined one, in the C++'s order (`REPAIR_JOIN`)
    pub merged: [i32; 8],
    pub merged_count: i32,
}

/// The tile joins the largest matching area, the others merge into it.
pub const REPAIR_JOIN: i32 = 1;
/// The tile joins its one neighbour's area.
pub const REPAIR_SINGLE: i32 = 2;
/// The tile gets a new area.
pub const REPAIR_NEW: i32 = 3;

/// 0x42372b's choice for a map tile whose kind changed to `kind`: `area_continent` its old area's continent,
/// `tile_continent` the tile's, `wall_id` its `EEAIGetTileWallID` (read only for an obstacle), `n` its neighbours.
pub fn repair_plan(
    kind: i32,
    area_continent: u16,
    tile_continent: u16,
    wall_id: i32,
    n: &[EeAreaTile; 8],
) -> EeRepairPlan {
    let neighbours: Vec<usize> = (0..8).filter(|&i| n[i].area).collect();
    let mut matches: Vec<usize> = Vec::new();
    let mut single = None;
    if neighbours.len() > 1 {
        let mut ok = true;
        for &i in neighbours.iter().rev() {
            let t = &n[i];
            if t.kind != kind || t.continent != area_continent {
                continue;
            }
            if kind == KIND_UNIT_OBSTACLE {
                ok = t.unit_id == wall_id;
            }
            if !ok {
                continue;
            }
            let (dx, dy) = NEIGHBOURS[i];
            ok = if dx != 0 && dy != 0 {
                match kind {
                    KIND_CLIFF | KIND_GAIA_OBSTACLE | KIND_MAP_EDGE | KIND_UNIT_OBSTACLE | KIND_WALL | KIND_GATE => {
                        true
                    }
                    _ => {
                        // the two orthogonal steps beside the diagonal (EETurnDirection by one either way)
                        let a = &n[index_of(0, dy)];
                        let b = &n[index_of(dx, 0)];
                        a.area && a.kind == kind && b.area && b.kind == kind
                    }
                }
            } else {
                true
            };
            if ok {
                matches.push(i);
            }
        }
    } else if neighbours.len() == 1 {
        let t = &n[neighbours[0]];
        if kind == t.kind && tile_continent == t.continent {
            single = Some(neighbours[0]);
        }
    }
    // std::sort by id, std::unique by id: one entry per area (which one does not matter: only the area is used)
    matches.sort_by_key(|&i| n[i].id);
    matches.dedup_by_key(|i| n[*i].id);
    let mut plan = EeRepairPlan::default();
    if !matches.is_empty() {
        let mut best: Option<usize> = None;
        for &i in matches.iter().rev() {
            if best.is_none_or(|b| n[i].size > n[b].size) {
                best = Some(i);
            }
        }
        let best = best.expect("a match");
        plan.action = REPAIR_JOIN;
        plan.target = best as i32;
        for &i in matches.iter().rev() {
            if i == best {
                continue;
            }
            plan.merged[plan.merged_count as usize] = i as i32;
            plan.merged_count += 1;
        }
    } else if let Some(i) = single {
        plan.action = REPAIR_SINGLE;
        plan.target = i as i32;
    } else {
        plan.action = REPAIR_NEW;
    }
    plan
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::area_goals::KIND_LAND;

    fn area(id: u16, kind: i32, size: i32) -> EeAreaTile {
        EeAreaTile { tile: true, area: true, id, kind, continent: 1, size, unit_id: -1, ..Default::default() }
    }

    fn empty() -> [EeAreaTile; 8] {
        [EeAreaTile { tile: true, ..Default::default() }; 8]
    }

    #[test]
    fn offsets() {
        assert_eq!(index_of(-1, -1), 0);
        assert_eq!(index_of(0, 1), 4);
        assert_eq!(index_of(1, 1), 7);
    }

    #[test]
    fn joins_the_largest_and_merges_the_rest() {
        let mut n = empty();
        n[1] = area(5, KIND_LAND, 10);
        n[3] = area(2, KIND_LAND, 30);
        n[4] = area(9, KIND_LAND, 30);
        n[6] = area(5, KIND_LAND, 10);
        n[7] = area(4, KIND_WATER, 99);
        let plan = repair_plan(KIND_LAND, 1, 1, -1, &n);
        assert_eq!(plan.action, REPAIR_JOIN);
        // ties: the highest id of the largest
        assert_eq!(n[plan.target as usize].id, 9);
        let merged: Vec<u16> = plan.merged[..plan.merged_count as usize].iter().map(|&i| n[i as usize].id).collect();
        assert_eq!(merged, vec![5, 2]);
    }

    #[test]
    fn diagonals_need_both_steps() {
        let mut n = empty();
        n[0] = area(3, KIND_LAND, 5);
        n[7] = area(4, KIND_LAND, 5);
        n[6] = area(6, KIND_LAND, 1);
        n[4] = area(6, KIND_LAND, 1);
        // (1, 1) has both steps (1, 0) and (0, 1) of the kind; (-1, -1) has neither
        let plan = repair_plan(KIND_LAND, 1, 1, -1, &n);
        let ids: Vec<u16> = std::iter::once(plan.target)
            .chain(plan.merged[..plan.merged_count as usize].iter().copied())
            .map(|i| n[i as usize].id)
            .collect();
        assert_eq!(n[plan.target as usize].id, 4);
        assert!(!ids.contains(&3));
        // obstacles join diagonally without the steps
        let mut n = empty();
        n[0] = area(3, KIND_CLIFF, 5);
        n[7] = area(4, KIND_CLIFF, 2);
        let plan = repair_plan(KIND_CLIFF, 1, 1, -1, &n);
        assert_eq!((n[plan.target as usize].id, plan.merged_count), (3, 1));
    }

    #[test]
    fn a_failed_neighbour_stops_the_rest() {
        // walked from the last: (1, 1) fails its diagonal test, and theOK stays false for (-1, 0)
        let mut n = empty();
        n[1] = area(3, KIND_LAND, 5);
        n[7] = area(4, KIND_LAND, 5);
        let plan = repair_plan(KIND_LAND, 1, 1, -1, &n);
        assert_eq!(plan.action, REPAIR_NEW);
        // the other way round the orthogonal one is found first
        let mut n = empty();
        n[0] = area(3, KIND_LAND, 5);
        n[6] = area(4, KIND_LAND, 5);
        let plan = repair_plan(KIND_LAND, 1, 1, -1, &n);
        assert_eq!((plan.action, n[plan.target as usize].id, plan.merged_count), (REPAIR_JOIN, 4, 0));
    }

    #[test]
    fn obstacles_match_the_wall() {
        let mut n = empty();
        n[1] = EeAreaTile { unit_id: 8, ..area(3, KIND_UNIT_OBSTACLE, 5) };
        n[6] = EeAreaTile { unit_id: 7, ..area(4, KIND_UNIT_OBSTACLE, 9) };
        // an obstacle's test is made again at every neighbour: (1, 0) fails it, (-1, 0) passes
        let plan = repair_plan(KIND_UNIT_OBSTACLE, 1, 1, 8, &n);
        assert_eq!((plan.action, n[plan.target as usize].id, plan.merged_count), (REPAIR_JOIN, 3, 0));
        let plan = repair_plan(KIND_UNIT_OBSTACLE, 1, 1, 7, &n);
        assert_eq!((n[plan.target as usize].id, plan.merged_count), (4, 0));
    }

    #[test]
    fn single_neighbours() {
        let mut n = empty();
        n[2] = area(3, KIND_LAND, 5);
        assert_eq!(
            repair_plan(KIND_LAND, 1, 1, -1, &n),
            EeRepairPlan { action: REPAIR_SINGLE, target: 2, ..Default::default() }
        );
        // the single neighbour is checked against the tile's continent, not the old area's
        assert_eq!(repair_plan(KIND_LAND, 1, 2, -1, &n).action, REPAIR_NEW);
        assert_eq!(repair_plan(KIND_LAND, 2, 1, -1, &n).action, REPAIR_SINGLE);
        assert_eq!(repair_plan(KIND_WATER, 1, 1, -1, &n).action, REPAIR_NEW);
        assert_eq!(repair_plan(KIND_LAND, 1, 1, -1, &empty()).action, REPAIR_NEW);
    }

    #[test]
    fn adjacent_ids() {
        let mut n = empty();
        let centre = EeAreaTile { tile: true, terrain: TERRAIN_DEEP_WATER, ..Default::default() };
        n[0] =
            EeAreaTile { terrain: TERRAIN_SHALLOW_WATER, water_body: 4, surface_region: 6, ..area(1, KIND_WATER, 1) };
        n[5] = EeAreaTile { terrain: 0, water_body: 5, surface_region: 7, land_mass: 8, ..area(2, KIND_LAND, 1) };
        n[7] = EeAreaTile { tile: false, ..n[5] };
        assert_eq!(adjacent_area_id(PATH_WATER_BODY, KIND_WATER, &centre, &n), 4);
        assert_eq!(adjacent_area_id(PATH_NON_SPACE, KIND_WATER, &centre, &n), 7);
        assert_eq!(adjacent_area_id(PATH_SPACE_REGION, KIND_WATER, &centre, &n), 0);
        assert_eq!(adjacent_area_id(PATH_LAND_MASS, KIND_WATER, &centre, &n), 8);
        // land mass: cliff..forest and the map edge only of the asked kind; water never
        n[5].kind = KIND_FOREST;
        assert_eq!(adjacent_area_id(PATH_LAND_MASS, KIND_WATER, &centre, &n), 0);
        assert_eq!(adjacent_area_id(PATH_LAND_MASS, KIND_FOREST, &centre, &n), 8);
        n[5].kind = KIND_WATER;
        assert_eq!(adjacent_area_id(PATH_LAND_MASS, KIND_WATER, &centre, &n), 0);
        n[5].kind = KIND_GATE;
        assert_eq!(adjacent_area_id(PATH_LAND_MASS, KIND_LAND, &centre, &n), 8);
        let off = EeAreaTile { tile: false, ..centre };
        assert_eq!(adjacent_area_id(PATH_WATER_BODY, KIND_WATER, &off, &n), 0);
        let space = EeAreaTile { terrain: TERRAIN_SPACE, ..centre };
        assert_eq!(adjacent_area_id(PATH_NON_SPACE, KIND_WATER, &space, &n), 0);
    }
}
