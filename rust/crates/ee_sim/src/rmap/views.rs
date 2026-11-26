//! The random map's reads of the map it makes, its tiles, the unit classes, the players and the units it places,
//! answered by the world views (game core lane, round 17: the single-statement reads of `ee_rmap_cpp`,
//! EERandomMap.cpp, gone). Each is the C++'s expression: `GetTile`'s unsigned compare, a NULL member 0.

// c_long/c_ulong are 32 bits on Windows
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::world::view::{ClassView, MapView, PlayerView, TileView, UnitView, WorldRead};
use crate::world_field as get;

/// `f` over a read token.
fn read<R>(f: impl FnOnce(&WorldRead) -> R) -> R {
    // SAFETY: the random map runs on the thread that makes the map, inside the C++ call that holds it
    let t = unsafe { WorldRead::begin() };
    f(&t)
}

/// The map `map`'s `GetTile((unsigned long)x, (unsigned long)y)` (0: off the map).
pub fn tile(map: usize, x: i64, y: i64) -> usize {
    // SAFETY: the map the random map makes, live through the call
    read(|t| unsafe { MapView::of(t, map as u64) }.and_then(|m| m.tile(x, y)).map_or(0, |k| k.handle() as usize))
}

/// The map's `mL195610`, `mL195614` (its rows and columns of tiles) and `mSize`.
pub fn dimensions(map: usize) -> (i64, i64, i64) {
    // SAFETY: as [`tile`]
    read(|t| {
        unsafe { MapView::of(t, map as u64) }
            .map_or((0, 0, 0), |m| (get!(m.rts(), mL195610) as i64, get!(m.rts(), mL195614) as i64, m.size()))
    })
}

/// The tile `tile`'s `mTerrainType`, `GetHeight()`, and its first unit (`mUnits ? mUnits->mUnit : 0`; None: no
/// unit node).
pub fn tile_state(tile: usize) -> (i32, f32, Option<usize>) {
    // SAFETY: a tile of the live map
    read(|t| {
        unsafe { TileView::of(t, tile as u64) }.map_or((0, 0.0, None), |k| {
            let first = k.units().next().map(|u| u.map_or(0, |u| u.handle() as usize));
            (k.terrain_type() as i32, k.height(), first)
        })
    })
}

/// `EEUnitClassCount()`.
pub fn class_count() -> u32 {
    read(|t| ClassView::count(t) as u32)
}

/// `gUnitClassTable[index]` (0: none).
pub fn class(index: i64) -> usize {
    read(|t| if index < 0 { None } else { ClassView::by_index(t, index as usize) }.map_or(0, |k| k.handle() as usize))
}

/// A class's members the random map reads.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RmapClassReads {
    pub enabled: bool,
    /// `mTileSize` (`GetTileSize()` is the member)
    pub tile_size: i32,
    pub is_wall: bool,
    pub ai_kind: i32,
    pub terrain_types: [bool; 17],
}

/// The class `class`'s members.
pub fn class_of(class: usize) -> RmapClassReads {
    // SAFETY: a class of the table
    read(|t| {
        unsafe { ClassView::of(t, class as u64) }.map_or(RmapClassReads::default(), |k| RmapClassReads {
            enabled: get!(k.fields(), mIsEnabled),
            tile_size: k.tile_size() as i32,
            is_wall: k.wall(),
            ai_kind: k.ai_kind() as i32,
            terrain_types: get!(k.fields(), mTerrainTypes),
        })
    })
}

/// A unit's members the random map reads.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RmapUnitReads {
    /// `mComplexUnit` (0: none)
    pub complex: usize,
    pub player: usize,
    pub class: usize,
    pub resource_type: i32,
    pub resource_amount: i64,
    /// the complex unit's `mIsPaidFor`
    pub paid_for: bool,
}

/// The unit `unit`'s members.
pub fn unit(unit: usize) -> RmapUnitReads {
    // SAFETY: a unit the random map placed, live
    read(|t| {
        unsafe { UnitView::of(t, unit as u64) }.map_or(RmapUnitReads::default(), |u| {
            let complex = u.complex_unit();
            RmapUnitReads {
                complex: complex.map_or(0, |c| c.handle() as usize),
                player: u.player().map_or(0, |p| p.handle() as usize),
                class: u.class().map_or(0, |k| k.handle() as usize),
                resource_type: u.resource_type() as c_long as i32,
                resource_amount: u.resource_amount(),
                paid_for: complex.is_some_and(|c| get!(c.complex(), mIsPaidFor)),
            }
        })
    })
}

/// The player `player`'s `mPlayerID` and `mEnemies[id]`.
pub fn player_id(player: usize) -> i64 {
    // SAFETY: a player of the game
    read(|t| unsafe { PlayerView::of(t, player as u64) }.map_or(0, |p| p.id() as i64))
}

pub fn player_enemy(player: usize, id: i64) -> i64 {
    // SAFETY: as above
    read(|t| unsafe { PlayerView::of(t, player as u64) }.map_or(0, |p| p.enemy(id as usize) as i64))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::world::layout::with_test_layout;
    use crate::world::view_tests::{addr, fake_layout, fixture};

    #[test]
    fn the_map_tiles_classes_units_and_players_from_the_views() {
        let f = fixture();
        let _guard = with_test_layout(fake_layout());
        let map = addr(&*f.map);
        assert_eq!(dimensions(map), (7, 8, 2));
        assert_eq!(tile(map, 1, 0), 0);
        assert_eq!(tile(map, -1, 0), 0);
        let t0 = tile(map, 0, 0);
        assert_eq!(t0, f.tile_array[0]);
        assert_eq!(tile_state(t0), (1, 0.0, Some(addr(&*f.tree))));
        assert_eq!(tile_state(tile(map, 1, 1)), (3, 1.0, None));
        let k = addr(&*f.class);
        let c = class_of(k);
        assert_eq!((c.is_wall, c.tile_size, c.enabled), (true, 0, false));
        let soldier = unit(addr(&*f.soldier));
        assert_eq!((soldier.complex, soldier.class, soldier.player), (addr(&*f.soldier), k, addr(&*f.player)));
        let tree = unit(addr(&*f.tree));
        assert_eq!((tree.complex, tree.resource_type, tree.resource_amount, tree.paid_for), (0, 2, 300, false));
        let p = addr(&*f.player);
        assert_eq!((player_id(p), player_enemy(p, 2), player_enemy(p, 3)), (1, 1, 0));
        assert_eq!((class_count(), class(0)), (0, 0));
    }
}
