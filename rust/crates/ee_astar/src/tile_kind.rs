//! The continent manager's tile classification (`EEAIGetTileKind`, 0x41ed2c) and a tile's wall id
//! (`EEAIGetTileWallID`, 0x41eea9), line by line, over what the C++ reads of the tile: its continent's category,
//! its terrain, its AI value (`GetTileFlag`) and its units in the tile's list order.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): no arithmetic, the C++'s tests in order; the unit records are
//! pure member reads, so reading every unit up front (the C++ stops at the first hit) answers the same.

use crate::area_goals::{
    KIND_CLIFF, KIND_FOREST, KIND_GATE, KIND_LAND, KIND_MAP_EDGE, KIND_NEAR_SHORE, KIND_SHALLOWS,
    KIND_SHALLOWS_NEAR_SHORE, KIND_SHALLOWS_SHORE, KIND_SHORE, KIND_SPACE, KIND_UNIT_OBSTACLE, KIND_WALL, KIND_WATER,
};
use crate::goals::{
    CATEGORY_CLIFF, CATEGORY_LAND, CATEGORY_MAP_EDGE, CATEGORY_SPACE, CATEGORY_WATER, TERRAIN_DEEP_WATER,
};
use crate::modifiers::TERRAIN_SHALLOW_WATER;

/// One unit of a tile's list as the classification reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTileUnit {
    /// the class's resource type is wood
    pub wood: bool,
    /// `mResourceAmount > 0`
    pub has_amount: bool,
    /// the class's `mBlocksTile`
    pub blocks: bool,
    /// the class's category is the gate's
    pub gate: bool,
    /// the class's `mIsWall`
    pub wall: bool,
    /// `IsFoundation()`
    pub foundation: bool,
    /// `mHitPoints > 0`
    pub alive: bool,
    /// a dock or navy yard (`CreatePathAreaKind`'s water body test)
    pub dock: bool,
    /// a space dock, space turret or orbital space station (its space region test)
    pub space_dock: bool,
    /// `mUnitID`
    pub id: i32,
}

/// 0x29ced `EEAIHasResourceUnit`, as a test: a wood unit on a tile not of deep water.
pub fn has_resource_unit(terrain: i32, units: &[EeTileUnit]) -> bool {
    terrain != TERRAIN_DEEP_WATER && units.iter().any(|u| u.wood)
}

/// 0x41ed2c `EEAIGetTileKind` of a map tile: `category` its continent's, `terrain` its type, `flag` its AI value,
/// `units` its unit list. (Off the map, kind 0, stays the C++'s test.)
pub fn tile_kind(category: u8, terrain: i32, flag: i32, units: &[EeTileUnit]) -> i32 {
    let by_flag = |shore: i32, near: i32, flat: i32| {
        if flag < 0 {
            shore
        } else if flag > 0 {
            near
        } else {
            flat
        }
    };
    match category {
        CATEGORY_LAND => {
            let mut result = if terrain == TERRAIN_SHALLOW_WATER {
                by_flag(KIND_SHALLOWS_SHORE, KIND_SHALLOWS_NEAR_SHORE, KIND_SHALLOWS)
            } else {
                KIND_LAND
            };
            if has_resource_unit(terrain, units) {
                if units.iter().any(|u| u.wood && u.has_amount) {
                    result = KIND_FOREST;
                }
            } else if let Some(u) = units
                .iter()
                .find(|u| !((!u.blocks && !u.gate) || u.foundation || !u.alive || (u.wood && !u.has_amount)))
            {
                result = if u.gate {
                    KIND_GATE
                } else if u.wall {
                    KIND_WALL
                } else {
                    KIND_UNIT_OBSTACLE
                };
            }
            result
        }
        CATEGORY_WATER => by_flag(KIND_SHORE, KIND_NEAR_SHORE, KIND_WATER),
        CATEGORY_SPACE => by_flag(KIND_SHORE, KIND_NEAR_SHORE, KIND_SPACE),
        CATEGORY_MAP_EDGE => KIND_MAP_EDGE,
        CATEGORY_CLIFF => KIND_CLIFF,
        _ => 0,
    }
}

/// 0x41eea9 `EEAIGetTileWallID` of a map tile: the id of its first live, finished blocking unit or gate (wood only
/// while not used up), -1 if none.
pub fn tile_wall_id(units: &[EeTileUnit]) -> i32 {
    units
        .iter()
        .find(|u| (u.blocks || u.gate) && !u.foundation && u.alive && (!u.wood || u.has_amount))
        .map_or(-1, |u| u.id)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn unit() -> EeTileUnit {
        EeTileUnit { alive: true, id: 7, ..Default::default() }
    }

    #[test]
    fn categories_and_flags() {
        assert_eq!(tile_kind(CATEGORY_WATER, 1, -3, &[]), KIND_SHORE);
        assert_eq!(tile_kind(CATEGORY_WATER, 1, 5, &[]), KIND_NEAR_SHORE);
        assert_eq!(tile_kind(CATEGORY_WATER, 1, 0, &[]), KIND_WATER);
        assert_eq!(tile_kind(CATEGORY_SPACE, 12, 0, &[]), KIND_SPACE);
        assert_eq!(tile_kind(CATEGORY_SPACE, 12, -1, &[]), KIND_SHORE);
        assert_eq!(tile_kind(CATEGORY_MAP_EDGE, 0, 0, &[unit()]), KIND_MAP_EDGE);
        assert_eq!(tile_kind(CATEGORY_CLIFF, 9, 0, &[]), KIND_CLIFF);
        assert_eq!(tile_kind(0, 0, 0, &[]), 0);
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 9, &[]), KIND_LAND);
        assert_eq!(tile_kind(CATEGORY_LAND, 10, -1, &[]), KIND_SHALLOWS_SHORE);
        assert_eq!(tile_kind(CATEGORY_LAND, 10, 1, &[]), KIND_SHALLOWS_NEAR_SHORE);
        assert_eq!(tile_kind(CATEGORY_LAND, 10, 0, &[]), KIND_SHALLOWS);
    }

    #[test]
    fn land_units() {
        let tree = EeTileUnit { wood: true, has_amount: true, blocks: true, ..unit() };
        let stump = EeTileUnit { has_amount: false, ..tree };
        let wall = EeTileUnit { blocks: true, wall: true, id: 9, ..unit() };
        let gate = EeTileUnit { gate: true, id: 11, ..unit() };
        // a live tree makes forest; a used-up one leaves the terrain's kind and hides the walls behind it
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 0, &[stump, tree]), KIND_FOREST);
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 0, &[stump, wall]), KIND_LAND);
        // on deep water a wood unit is no resource: the blocker test runs and skips the stump
        assert_eq!(tile_kind(CATEGORY_LAND, 1, 0, &[stump, wall]), KIND_WALL);
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 0, &[gate, wall]), KIND_GATE);
        let block = EeTileUnit { blocks: true, ..unit() };
        assert_eq!(tile_kind(CATEGORY_LAND, 10, 1, &[block]), KIND_UNIT_OBSTACLE);
        // foundations, dead units and non-blockers are skipped
        let skipped = [
            EeTileUnit { foundation: true, ..wall },
            EeTileUnit { alive: false, ..wall },
            EeTileUnit { blocks: false, ..wall },
        ];
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 0, &skipped), KIND_LAND);
        assert_eq!(tile_kind(CATEGORY_LAND, 0, 0, &[skipped[0], skipped[1], skipped[2], gate]), KIND_GATE);
    }

    #[test]
    fn wall_ids() {
        let wall = EeTileUnit { blocks: true, id: 3, ..unit() };
        let tree = EeTileUnit { wood: true, has_amount: true, blocks: true, id: 4, ..unit() };
        let stump = EeTileUnit { has_amount: false, id: 5, ..tree };
        assert_eq!(tile_wall_id(&[]), -1);
        assert_eq!(tile_wall_id(&[EeTileUnit { foundation: true, ..wall }, stump, tree, wall]), 4);
        assert_eq!(tile_wall_id(&[EeTileUnit { alive: false, ..wall }, unit()]), -1);
        assert_eq!(tile_wall_id(&[EeTileUnit { gate: true, id: 8, ..unit() }]), 8);
    }
}
