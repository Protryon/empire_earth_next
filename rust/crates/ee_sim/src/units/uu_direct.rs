//! The unit utilities for the other Rust modules' game hosts (round 4): the goals' and the AI's hosts call the noted
//! entry points (uu_ffi.rs) here instead of a C++ host case that called a C++ forwarder that called them, so each
//! call is noted (UUTL) as before. The game's only: a unit test's fake hosts never reach these (they panic there).

#![allow(clippy::missing_panics_doc)]
#![cfg_attr(test, allow(unused_variables, unused_mut, unreachable_code))]

use crate::units::uu_tiles::Tile;

#[cfg(not(test))]
use crate::units::uu_ffi as ffi;

/// Runs `game` in the game; a unit test never calls it.
macro_rules! game {
    ($e:expr) => {{
        #[cfg(not(test))]
        {
            $e
        }
        #[cfg(test)]
        {
            unreachable!("the unit utilities' game entry points")
        }
    }};
}

/// `EECanStandOn`.
pub fn can_stand_on(x: i32, y: i32, unit: u64) -> bool {
    game!(ffi::ee_units_uu_can_stand_on(x, y, unit) != 0)
}

/// `EEIsLineBlocked` with its outputs dropped.
pub fn line_blocked(x1: i32, y1: i32, x2: i32, y2: i32) -> bool {
    game!({
        let (mut wall, mut written, mut last) = (0u64, 0u8, [0i32; 2]);
        // SAFETY: three live locals
        unsafe { ffi::ee_units_uu_line_blocked(x1, y1, x2, y2, &mut wall, &mut written, &mut last) != 0 }
    })
}

/// `EEGetAttackPart`.
pub fn attack_part(unit: u64, target: u64) -> u64 {
    game!(ffi::ee_units_uu_attack_part(unit, target))
}

/// `EEDepositLoad`.
pub fn deposit_load(unit: u64, container: u64) {
    game!(ffi::ee_units_uu_deposit_load(unit, container))
}

/// `EEFindResourceSpot` (`which` 1), `EEFindNextFreeTile` (2): `io` in and out.
pub fn spot(which: i32, unit: u64, target: u64, io: &mut [f32; 3]) -> bool {
    // SAFETY: a live point
    game!(unsafe { ffi::ee_units_uu_spot(which, unit, target, io) != 0 })
}

/// `EEIsWithinReachOf`.
pub fn within_reach_of(point: [f32; 3], unit: u64, target: u64) -> bool {
    // SAFETY: a live point
    game!(unsafe { ffi::ee_units_uu_within_reach(unit, target, &point) != 0 })
}

/// `EEGetPathLengthTo(unit, tile, limit, walls)` (the tile's update dropped).
pub fn path_length_to(unit: u64, tile: Tile, limit: u32, walls: bool) -> f32 {
    game!({
        let mut t = tile;
        // SAFETY: a live tile
        unsafe { ffi::ee_units_uu_path_length_to(unit, &mut t, limit, u8::from(walls)) }
    })
}

/// `EEGetTileUnitStanding`.
pub fn tile_unit_standing(unit: u64, player: u64, unit_type: i32, ignore_owner: bool) -> i32 {
    game!(ffi::ee_units_uu_tile_unit_standing(unit, player, unit_type, u8::from(ignore_owner)))
}

/// `EEGetBlockingUnits` (`standing` 2) and `EEGetMovableTileUnits` (1) on tile `tile` (0 off the map).
pub fn tile_units_standing(player: u64, unit_type: i32, tile: u64, standing: i32) -> Vec<u64> {
    game!(ffi::tile_units_standing(player, unit_type, tile, false, standing).1)
}

/// `EEGetTilesAroundUnit`.
pub fn tiles_around(unit: u64, target: u64, range: i32, corners: i32, virtual_call: bool) -> Vec<Tile> {
    game!(ffi::tiles_around(unit, target, range, corners, virtual_call))
}

/// `EEPickTileNear`.
pub fn pick_tile_near(tiles: &[Tile], unit: u64, container: u64, ignore_blocked_line: bool, io: &mut [f32; 3]) -> bool {
    game!(ffi::pick_tile_near(tiles, unit, container, ignore_blocked_line, io))
}

/// `EEGetFootprintTiles` (into an empty list).
pub fn footprint_tiles(unit: u64) -> Vec<Tile> {
    game!({
        let mut tiles = Vec::new();
        ffi::unit_tiles(0, unit, &mut tiles);
        tiles
    })
}

/// `EEGetUnitsOn` (into an empty list).
pub fn units_on(tiles: &[Tile]) -> Vec<u64> {
    game!({
        let mut units = Vec::new();
        ffi::units_on(tiles, &mut units);
        units
    })
}

/// `EEGetPlayerUnitsOfType` (into an empty list).
pub fn player_units_of_type(unit_type: i32, index: i32) -> Vec<u64> {
    game!({
        let mut units = Vec::new();
        ffi::player_units_of_type(unit_type, index, &mut units);
        units
    })
}

/// `EEGetUnitsInRadius`.
pub fn units_in_radius(x: f32, y: f32, radius: f32) -> Vec<u64> {
    game!(ffi::units_in_radius(x, y, radius))
}

/// The world map's tile (x, y) (`GetTile`, 0 off the map).
pub fn map_tile(x: i32, y: i32) -> u64 {
    game!({
        // SAFETY: the game host's caller holds the world
        let t = unsafe { crate::world::WorldRead::begin() };
        crate::world::WorldView::global(&t)
            .and_then(|w| w.map())
            .and_then(|m| m.tile(x.into(), y.into()))
            .map_or(0, |t| t.handle())
    })
}

/// `EEStepAsideFromSite(unit, its class, its player, its tile, false)`: the units on a unit's site make way.
pub fn clear_site(unit: u64) {
    game!({
        // SAFETY: the game host's caller holds the world; a live unit
        let t = unsafe { crate::world::WorldRead::begin() };
        // SAFETY: as above
        let u = unsafe { crate::world::UnitView::of(&t, unit) }.expect("a unit");
        let (x, y) = u.tile_xy();
        let class = u.class().map_or(0, |c| c.handle());
        let player = u.player().map_or(0, |p| p.handle());
        ffi::ee_units_uu_step_aside(unit, class, player, x as i32, y as i32, 0)
    })
}
