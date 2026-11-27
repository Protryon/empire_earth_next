//! `EEGatherNextTree` over its C++ statements (the units' lane, round 7; was EEUGRust.cpp, GoalOp's NEXT_TREE): a
//! carried load of another type dropped; a tree without a free spot gives way to the first gatherable tree of its
//! forest on the tile line from a nearby drop site (or the gatherer) to it; another resource but an animal becomes
//! the gathered one. Every C++ statement is one operation of [`GtHost::gt`] (the game's: `ee_units_gt_host`,
//! EEUGRust.cpp), asked in the original's order.

#![allow(clippy::unnecessary_cast)]

/// unit `a`'s `mResourceAmount`
pub const EE_UNITS_GT_RESOURCE_AMOUNT: i32 = 1;
/// unit `a`'s `mResourceType`
pub const EE_UNITS_GT_RESOURCE_TYPE: i32 = 2;
/// unit `a`'s `mResourceAmount = 0`
pub const EE_UNITS_GT_CLEAR_RESOURCE_AMOUNT: i32 = 3;
/// `ee_units_uu_spot(1, unit a, resource b, the scratch point)` (a free gather spot on the tree)
pub const EE_UNITS_GT_FREE_SPOT: i32 = 4;
/// `gContinentManager.GetForestIDAt((c, d))`
pub const EE_UNITS_GT_FOREST_ID: i32 = 5;
/// `gContinentManager.GetForests()[c]`
pub const EE_UNITS_GT_FOREST: i32 = 6;
/// forest `a`'s `GetNumTiles()`
pub const EE_UNITS_GT_FOREST_TILES: i32 = 7;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_GT_TILE_X: i32 = 8;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_GT_TILE_Y: i32 = 9;
/// the bits of complex unit `a`'s `GetLineOfSight()`
pub const EE_UNITS_GT_LINE_OF_SIGHT: i32 = 10;
/// the unit list = `ee_units_uu_units_in_radius(f[0], f[1], f[2])`; its size
pub const EE_UNITS_GT_UNITS_IN_RADIUS: i32 = 11;
/// the unit list[c]
pub const EE_UNITS_GT_LIST_UNIT: i32 = 12;
/// unit `a`'s `GetPlayer()`
pub const EE_UNITS_GT_PLAYER: i32 = 13;
/// unit `a`'s class's `IsDropSite()`
pub const EE_UNITS_GT_IS_DROP_SITE: i32 = 14;
/// complex unit `a`'s `CanTake(complex b, 3)`
pub const EE_UNITS_GT_CAN_TAKE: i32 = 15;
/// the tile line = `(c, d)`'s `GetLineTiles(the target, 1.0f)` (the target set by `LINE_TARGET`); its size
pub const EE_UNITS_GT_LINE_TILES: i32 = 16;
/// the line's target tile = `(c, d)`
pub const EE_UNITS_GT_LINE_TARGET: i32 = 17;
/// the tile line's tile `c` (x, y packed: x low 32 bits)
pub const EE_UNITS_GT_LINE_TILE: i32 = 18;
/// `gWorld.GetMap()->GetTile(c, d)`'s first unit node (`mUnits`), 0 off the map... see `MAP_TILE`
pub const EE_UNITS_GT_MAP_TILE: i32 = 19;
/// tile `a`'s `mUnits`
pub const EE_UNITS_GT_FIRST_NODE: i32 = 20;
/// node `a`'s `mUnit`
pub const EE_UNITS_GT_NODE_UNIT: i32 = 21;
/// node `a`'s `mNext`
pub const EE_UNITS_GT_NEXT_NODE: i32 = 22;
/// complex unit `a`'s `CanGather(unit b, c, d, false, false)`
pub const EE_UNITS_GT_CAN_GATHER: i32 = 23;
/// complex unit `a`'s citizen class's `HasGatherPoint(a, unit b)` (its UCBD note)
pub const EE_UNITS_GT_HAS_GATHER_POINT: i32 = 24;
/// complex unit `a`'s `SetGatheredResource(unit b)`
pub const EE_UNITS_GT_SET_GATHERED: i32 = 25;
/// unit `a`'s class's `mFamily`
pub const EE_UNITS_GT_FAMILY: i32 = 26;

/// the tile line emptied (the original's was a new local)
pub const EE_UNITS_GT_LINE_CLEAR: i32 = 27;

const RESOURCE_WOOD: i64 = 3;
const FAMILY_ANIMAL: i64 = 0x11;

/// The C++ statements of the body.
pub trait GtHost {
    /// one statement (`EE_UNITS_GT_*`)
    fn gt(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64;
}

fn sa<H: GtHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.gt(op, a, 0, 0, 0, &mut [0.0; 4])
}
fn sab<H: GtHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64) -> i64 {
    h.gt(op, a, b, 0, 0, &mut [0.0; 4])
}
fn scd<H: GtHost + ?Sized>(h: &mut H, op: i32, c: i64, d: i64) -> i64 {
    h.gt(op, 0, 0, c, d, &mut [0.0; 4])
}

/// `EEGatherNextTree(unit, resource)`: the tree to gather next (0 for none).
pub fn gather_next_tree<H: GtHost + ?Sized>(h: &mut H, unit: u64, resource: u64) -> u64 {
    if sa(h, EE_UNITS_GT_RESOURCE_AMOUNT, unit) as i32 > 0
        && sa(h, EE_UNITS_GT_RESOURCE_TYPE, unit) as u32 != sa(h, EE_UNITS_GT_RESOURCE_TYPE, resource) as u32
    {
        sa(h, EE_UNITS_GT_CLEAR_RESOURCE_AMOUNT, unit);
    }
    if !(sa(h, EE_UNITS_GT_RESOURCE_TYPE, resource) == RESOURCE_WOOD
        && sab(h, EE_UNITS_GT_FREE_SPOT, unit, resource) == 0)
    {
        if sa(h, EE_UNITS_GT_FAMILY, resource) != FAMILY_ANIMAL {
            sab(h, EE_UNITS_GT_SET_GATHERED, unit, resource);
        }
        return 0;
    }
    let rx = sa(h, EE_UNITS_GT_TILE_X, resource);
    let ry = sa(h, EE_UNITS_GT_TILE_Y, resource);
    let forest_id = scd(h, EE_UNITS_GT_FOREST_ID, rx, ry) as u16;
    let forest = h.gt(EE_UNITS_GT_FOREST, 0, 0, i64::from(forest_id), 0, &mut [0.0; 4]) as u64;
    if forest == 0 || sa(h, EE_UNITS_GT_FOREST_TILES, forest) as u32 <= 1 {
        return 0;
    }
    let center = [sa(h, EE_UNITS_GT_TILE_X, resource) as i32 as f32, sa(h, EE_UNITS_GT_TILE_Y, resource) as i32 as f32];
    let radius = f32::from_bits(sa(h, EE_UNITS_GT_LINE_OF_SIGHT, unit) as u32);
    let n = h.gt(EE_UNITS_GT_UNITS_IN_RADIUS, 0, 0, 0, 0, &mut [center[0], center[1], radius, 0.0]);
    let units: Vec<u64> = (0..n).map(|k| scd(h, EE_UNITS_GT_LIST_UNIT, k, 0) as u64).collect();
    let mut other = 0u64;
    let player = sa(h, EE_UNITS_GT_PLAYER, unit);
    for &candidate in units.iter().rev() {
        if sa(h, EE_UNITS_GT_PLAYER, candidate) == player
            && sa(h, EE_UNITS_GT_IS_DROP_SITE, candidate) != 0
            && sab(h, EE_UNITS_GT_CAN_TAKE, candidate, unit) != 0
        {
            other = candidate;
            break;
        }
    }
    let from = if other != 0 { other } else { unit };
    let tile = (sa(h, EE_UNITS_GT_TILE_X, from), sa(h, EE_UNITS_GT_TILE_Y, from));
    let target = (sa(h, EE_UNITS_GT_TILE_X, resource), sa(h, EE_UNITS_GT_TILE_Y, resource));
    if tile == target {
        return 0;
    }
    sa(h, EE_UNITS_GT_LINE_CLEAR, 0);
    scd(h, EE_UNITS_GT_LINE_TARGET, target.0, target.1);
    let n = scd(h, EE_UNITS_GT_LINE_TILES, tile.0, tile.1);
    let line: Vec<(i64, i64)> = (0..n)
        .map(|k| {
            let packed = scd(h, EE_UNITS_GT_LINE_TILE, k, 0) as u64;
            (i64::from(packed as u32 as i32), i64::from((packed >> 32) as u32 as i32))
        })
        .collect();
    for (x, y) in line {
        let map_tile = scd(h, EE_UNITS_GT_MAP_TILE, x, y) as u64;
        if map_tile == 0 {
            continue;
        }
        let mut node = sa(h, EE_UNITS_GT_FIRST_NODE, map_tile) as u64;
        while node != 0 {
            let tree = sa(h, EE_UNITS_GT_NODE_UNIT, node) as u64;
            if tree != 0
                && sa(h, EE_UNITS_GT_RESOURCE_TYPE, tree) == RESOURCE_WOOD
                && h.gt(EE_UNITS_GT_CAN_GATHER, unit, tree, x, y, &mut [0.0; 4]) != 0
                && sab(h, EE_UNITS_GT_HAS_GATHER_POINT, unit, tree) != 0
                && scd(h, EE_UNITS_GT_FOREST_ID, x, y) as u16 == forest_id
            {
                return tree;
            }
            node = sa(h, EE_UNITS_GT_NEXT_NODE, node) as u64;
        }
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        fn ee_units_gt_host(op: i32, a: u64, b: u64, c: i64, d: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game;

    impl GtHost for Game {
        fn gt(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::goal_view::gt_view(op, a, b, c, d) {
                return v;
            }
            // SAFETY: the body runs inside the goal host that handed over its live units
            unsafe { ee_units_gt_host(op, a, b, c, d, f) }
        }
    }

    /// `EEGatherNextTree(unit, resource)`.
    ///
    /// # Safety
    /// Both are live units, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_gather_next_tree(unit: u64, resource: u64) -> u64 {
        gather_next_tree(&mut Game, unit, resource)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    #[derive(Default)]
    struct Mock {
        answers: BTreeMap<(i32, u64), i64>,
        by_cd: BTreeMap<(i32, i64, i64), i64>,
        log: Vec<(i32, u64, u64, i64, i64)>,
    }

    impl GtHost for Mock {
        fn gt(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, _f: &mut [f32; 4]) -> i64 {
            self.log.push((op, a, b, c, d));
            if let Some(v) = self.by_cd.get(&(op, c, d)) {
                return *v;
            }
            *self.answers.get(&(op, a)).unwrap_or(&0)
        }
    }

    const UNIT: u64 = 1;
    const TREE: u64 = 2;

    #[test]
    fn another_resource_but_an_animal_is_gathered_directly() {
        let mut h = Mock::default();
        h.answers.insert((EE_UNITS_GT_RESOURCE_TYPE, TREE), 1);
        assert_eq!(gather_next_tree(&mut h, UNIT, TREE), 0);
        assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GT_SET_GATHERED && e.2 == TREE));
        h.answers.insert((EE_UNITS_GT_FAMILY, TREE), FAMILY_ANIMAL);
        h.log.clear();
        gather_next_tree(&mut h, UNIT, TREE);
        assert!(!h.log.iter().any(|e| e.0 == EE_UNITS_GT_SET_GATHERED));
    }

    #[test]
    fn a_tree_with_a_free_spot_is_kept() {
        let mut h = Mock::default();
        h.answers.insert((EE_UNITS_GT_RESOURCE_TYPE, TREE), RESOURCE_WOOD);
        h.answers.insert((EE_UNITS_GT_FREE_SPOT, UNIT), 1);
        assert_eq!(gather_next_tree(&mut h, UNIT, TREE), 0);
        assert!(!h.log.iter().any(|e| e.0 == EE_UNITS_GT_FOREST_ID));
        assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GT_SET_GATHERED && e.2 == TREE));
    }

    #[test]
    fn a_full_tree_gives_way_to_one_on_the_line() {
        let mut h = Mock::default();
        h.answers.insert((EE_UNITS_GT_RESOURCE_TYPE, TREE), RESOURCE_WOOD);
        h.answers.insert((EE_UNITS_GT_RESOURCE_TYPE, 9), RESOURCE_WOOD);
        h.answers.insert((EE_UNITS_GT_TILE_X, TREE), 10);
        h.answers.insert((EE_UNITS_GT_TILE_Y, TREE), 10);
        h.answers.insert((EE_UNITS_GT_TILE_X, UNIT), 5);
        h.answers.insert((EE_UNITS_GT_TILE_Y, UNIT), 5);
        h.by_cd.insert((EE_UNITS_GT_FOREST, 4, 0), 50);
        h.answers.insert((EE_UNITS_GT_FOREST_TILES, 50), 3);
        h.by_cd.insert((EE_UNITS_GT_FOREST_ID, 10, 10), 4);
        h.by_cd.insert((EE_UNITS_GT_FOREST_ID, 7, 7), 4);
        h.by_cd.insert((EE_UNITS_GT_LINE_TILES, 5, 5), 2);
        h.by_cd.insert((EE_UNITS_GT_LINE_TILE, 0, 0), 6 | (6 << 32));
        h.by_cd.insert((EE_UNITS_GT_LINE_TILE, 1, 0), 7 | (7 << 32));
        h.by_cd.insert((EE_UNITS_GT_MAP_TILE, 7, 7), 70);
        h.answers.insert((EE_UNITS_GT_FIRST_NODE, 70), 71);
        h.answers.insert((EE_UNITS_GT_NODE_UNIT, 71), 9);
        h.answers.insert((EE_UNITS_GT_CAN_GATHER, UNIT), 1);
        h.answers.insert((EE_UNITS_GT_HAS_GATHER_POINT, UNIT), 1);
        assert_eq!(gather_next_tree(&mut h, UNIT, TREE), 9);
        let target = h.log.iter().find(|e| e.0 == EE_UNITS_GT_LINE_TARGET).unwrap();
        assert_eq!((target.3, target.4), (10, 10));
    }
}
