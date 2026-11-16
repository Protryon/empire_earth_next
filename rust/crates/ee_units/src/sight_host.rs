//! `EEComplexUnit::SightOn`/`SightOff`'s host in the game (`ee_units_sight_host`, EEComplexUnit.cpp) and the entry
//! point (sight.rs). Absent from a test binary.

#![cfg(not(test))]

use crate::sight::{sight, EeUnitsSightUnit, SightHost};

/// unit `a` into `unit`
pub const EE_UNITS_SIGHT_UNIT: i32 = 1;
/// `gSightUpdatesEnabled`
pub const EE_UNITS_SIGHT_ENABLED: i32 = 2;
/// unit `a`'s `IsFoundation()`
pub const EE_UNITS_SIGHT_FOUNDATION: i32 = 3;
/// unit `a`'s `AddCenterOffset(f[0], f[1])`
pub const EE_UNITS_SIGHT_CENTER: i32 = 4;
/// `gWorld.GetCurrentPlayer()`
pub const EE_UNITS_SIGHT_CURRENT: i32 = 5;
/// player `a`'s `mSharedLOS`
pub const EE_UNITS_SIGHT_SHARED_LOS: i32 = 6;
/// player `a`'s `IsAlliedWith(b)`
pub const EE_UNITS_SIGHT_ALLIED: i32 = 7;
/// player `a`'s `mSightSharers.size()`
pub const EE_UNITS_SIGHT_SHARERS: i32 = 8;
/// player `a`'s `mSightSharers[c]`
pub const EE_UNITS_SIGHT_SHARER: i32 = 9;
/// `EEAddSightCircle` (`on`) / `EERemoveSightCircle(f[0], f[1], f[2], player a)`
pub const EE_UNITS_SIGHT_CIRCLE: i32 = 10;
/// `gWorld.mMap->GetTile(c, d)`
pub const EE_UNITS_SIGHT_TILE: i32 = 11;
/// tile `a`'s `AddSight` (`on`) / `RemoveSight(player b)`
pub const EE_UNITS_SIGHT_TILE_SIGHT: i32 = 12;
/// player `a`'s `AddSharedSight(tile b, c, d)` (`on`) / `RemoveSharedSight(tile b)`
pub const EE_UNITS_SIGHT_SHARED: i32 = 13;
/// `gMiniMap->CountCellOn/Off(c, d)`, the terrain mesh's `ReferenceTile/DereferenceTile(c, d)`
pub const EE_UNITS_SIGHT_SHOW: i32 = 14;
/// `gWorld.mMap->AddSight/RemoveSight(player a, f[0], f[1], the wonder sight radius, false)`
pub const EE_UNITS_SIGHT_WONDER: i32 = 15;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_SIGHT_NUM_PLAYERS: i32 = 16;
/// `gWorld.mPlayers[c]`
pub const EE_UNITS_SIGHT_PLAYER: i32 = 17;

extern "C" {
    fn ee_units_sight_host(
        op: i32,
        on: u8,
        a: u64,
        b: u64,
        c: i64,
        d: i64,
        f: *mut [f32; 3],
        unit: *mut EeUnitsSightUnit,
    ) -> i64;
}

struct Game;

impl Game {
    #[allow(clippy::too_many_arguments)] // the switch's operands
    fn op(&mut self, op: i32, on: bool, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 3]) -> i64 {
        let mut unit = EeUnitsSightUnit::default();
        // SAFETY: the C++ switch reads live objects the body hands it
        unsafe { ee_units_sight_host(op, on as u8, a, b, c, d, f, &mut unit) }
    }
    fn q(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        self.op(op, false, a, b, c, d, &mut [0.0; 3])
    }
}

impl SightHost for Game {
    fn unit(&mut self, u: u64) -> EeUnitsSightUnit {
        let mut unit = EeUnitsSightUnit::default();
        // SAFETY: as `op`
        unsafe { ee_units_sight_host(EE_UNITS_SIGHT_UNIT, 0, u, 0, 0, 0, &mut [0.0; 3], &mut unit) };
        unit
    }
    fn updates_enabled(&mut self) -> bool {
        self.q(EE_UNITS_SIGHT_ENABLED, 0, 0, 0, 0) != 0
    }
    fn is_foundation(&mut self, u: u64) -> bool {
        self.q(EE_UNITS_SIGHT_FOUNDATION, u, 0, 0, 0) != 0
    }
    fn center_offset(&mut self, u: u64, x: f32, y: f32) -> (f32, f32) {
        let mut f = [x, y, 0.0];
        self.op(EE_UNITS_SIGHT_CENTER, false, u, 0, 0, 0, &mut f);
        (f[0], f[1])
    }
    fn current_player(&mut self) -> u64 {
        self.q(EE_UNITS_SIGHT_CURRENT, 0, 0, 0, 0) as u64
    }
    fn shared_los(&mut self, player: u64) -> bool {
        self.q(EE_UNITS_SIGHT_SHARED_LOS, player, 0, 0, 0) != 0
    }
    fn allied(&mut self, a: u64, b: u64) -> bool {
        self.q(EE_UNITS_SIGHT_ALLIED, a, b, 0, 0) != 0
    }
    fn sharers(&mut self, player: u64) -> Vec<u64> {
        let n = self.q(EE_UNITS_SIGHT_SHARERS, player, 0, 0, 0);
        (0..n).map(|i| self.q(EE_UNITS_SIGHT_SHARER, player, 0, i, 0) as u64).collect()
    }
    fn circle(&mut self, on: bool, x: f32, y: f32, radius: f32, player: u64) {
        self.op(EE_UNITS_SIGHT_CIRCLE, on, player, 0, 0, 0, &mut [x, y, radius]);
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        self.q(EE_UNITS_SIGHT_TILE, 0, 0, x as i64, y as i64) as u64
    }
    fn tile_sight(&mut self, on: bool, tile: u64, player: u64) {
        self.op(EE_UNITS_SIGHT_TILE_SIGHT, on, tile, player, 0, 0, &mut [0.0; 3]);
    }
    fn shared_sight(&mut self, on: bool, player: u64, tile: u64, x: i32, y: i32) {
        self.op(EE_UNITS_SIGHT_SHARED, on, player, tile, x as i64, y as i64, &mut [0.0; 3]);
    }
    fn show(&mut self, on: bool, x: i32, y: i32) {
        self.op(EE_UNITS_SIGHT_SHOW, on, 0, 0, x as i64, y as i64, &mut [0.0; 3]);
    }
    fn wonder_sight(&mut self, on: bool, player: u64, x: f32, y: f32) {
        self.op(EE_UNITS_SIGHT_WONDER, on, player, 0, 0, 0, &mut [x, y, 0.0]);
    }
    fn num_players(&mut self) -> u32 {
        self.q(EE_UNITS_SIGHT_NUM_PLAYERS, 0, 0, 0, 0) as u32
    }
    fn player_at(&mut self, i: u32) -> u64 {
        self.q(EE_UNITS_SIGHT_PLAYER, 0, 0, i as i64, 0) as u64
    }
}

/// `EEComplexUnit::SightOn(x, y)` (`on` 1) and `SightOff(x, y)` (0) of unit `unit`.
#[no_mangle]
pub extern "C" fn ee_units_sight(unit: u64, x: u32, y: u32, on: u8) {
    sight(&mut Game, unit, x, y, on != 0)
}
