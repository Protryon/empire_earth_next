//! `EEComplexUnit::SightOn`/`SightOff`'s host in the game (`ee_units_sight_host`, EEComplexUnit.cpp) and the entry
//! point (sight.rs). Absent from a test binary.

#![cfg(not(test))]

use crate::units::sight::{sight, EeUnitsSightUnit, SightHost};
use crate::world::view::{PlayerView, UnitView, WorldRead, WorldView};
use std::os::raw::c_long;

/// unit `a`'s `GetLineOfSight()` into `f[0]` (the reads of [`EeUnitsSightUnit`], one statement each)
pub const EE_UNITS_SIGHT_U_LINE_OF_SIGHT: i32 = 4000;
/// unit `a`'s `mPlayer`
pub const EE_UNITS_SIGHT_U_PLAYER: i32 = 4001;
/// unit `a`'s `mPlayer != NULL && mPlayer->mSaveVisMap`
pub const EE_UNITS_SIGHT_U_SAVE_VIS_MAP: i32 = 4002;
/// unit `a`'s `mHitPoints`
pub const EE_UNITS_SIGHT_U_HIT_POINTS: i32 = 4003;
/// unit `a`'s `mIsBuilt`
pub const EE_UNITS_SIGHT_U_IS_BUILT: i32 = 4004;
/// unit `a`'s `GetUnitClass()->IsLandBuilding()`
pub const EE_UNITS_SIGHT_U_LAND_BUILDING: i32 = 4005;
/// unit `a`'s `GetUnitClass()->mTileSize`
pub const EE_UNITS_SIGHT_U_TILE_SIZE: i32 = 4006;
/// unit `a`'s `GetUnitClass()->mWonderBonus`
pub const EE_UNITS_SIGHT_U_WONDER_BONUS: i32 = 4007;
/// unit `a`'s `GetTechTreeNode() != NULL ? GetTechTreeNode()->mData->mKind : -1`
pub const EE_UNITS_SIGHT_U_NODE_KIND: i32 = 4008;
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
/// `EERemoveSightCircle(f[0], f[1], f[2], player a)` (`EE_UNITS_SIGHT_CIRCLE` is the add)
pub const EE_UNITS_SIGHT_CIRCLE_OFF: i32 = 20;
/// tile `a`'s `RemoveSight(player b)` (`EE_UNITS_SIGHT_TILE_SIGHT` is the add)
pub const EE_UNITS_SIGHT_TILE_SIGHT_OFF: i32 = 21;
/// player `a`'s `RemoveSharedSight(tile b)` (`EE_UNITS_SIGHT_SHARED` is the add)
pub const EE_UNITS_SIGHT_SHARED_OFF: i32 = 22;
/// the terrain mesh's `ReferenceTile(c, d)` (`EE_UNITS_SIGHT_SHOW` is the minimap's `CountCellOn`)
pub const EE_UNITS_SIGHT_SHOW_TILE: i32 = 23;
/// `gMiniMap->CountCellOff(c, d)`
pub const EE_UNITS_SIGHT_SHOW_OFF: i32 = 24;
/// the terrain mesh's `DereferenceTile(c, d)`
pub const EE_UNITS_SIGHT_SHOW_TILE_OFF: i32 = 25;
/// `gWorld.mMap->RemoveSight(player a, f[0], f[1], the wonder sight radius, false)` (`EE_UNITS_SIGHT_WONDER` the add)
pub const EE_UNITS_SIGHT_WONDER_OFF: i32 = 26;

extern "C" {
    pub(crate) fn ee_units_sight_host(
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

/// `f` over the world's view (the sight's caller holds the world); its default without one.
fn with_world<R: Default>(f: impl FnOnce(&WorldView<'_>) -> R) -> R {
    // SAFETY: SightOn/SightOff run on the simulation thread inside the C++ caller's hold of the world
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).map_or_else(R::default, |w| f(&w))
}

/// `f` over the live player `player`'s view.
fn with_player<R: Default>(player: u64, f: impl FnOnce(&PlayerView<'_>) -> R) -> R {
    // SAFETY: as above; the body hands in live players
    let t = unsafe { WorldRead::begin() };
    unsafe { PlayerView::of(&t, player) }.map_or_else(R::default, |p| f(&p))
}

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
        // the records' members read here; the class's land building test and the tech node's kind are the host's
        // SAFETY: as with_world's; the body hands in a live complex unit
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        let view = unsafe { UnitView::of(&t, u) }.expect("ee_units::sight: a unit");
        let class = view.class().expect("ee_units::sight: the unit's class");
        let stats: f32 = view.stats().map_or(0.0, |s| crate::world_field!(s.fields(), mLineOfSight));
        let player = view.player();
        let hit_points: c_long = crate::world_field!(view.fields(), mHitPoints);
        let wonder_bonus: c_long = crate::world_field!(class.fields(), mWonderBonus);
        EeUnitsSightUnit {
            line_of_sight: stats + view.sight(),
            player: player.map_or(0, |p| p.handle()),
            save_vis_map: u8::from(player.is_some_and(|p| p.save_vis_map())),
            hit_points: hit_points as i32,
            is_built: u8::from(view.is_built()),
            land_building: self.q(EE_UNITS_SIGHT_U_LAND_BUILDING, u, 0, 0, 0) as u8,
            tile_size: class.tile_size() as i32,
            wonder_bonus: wonder_bonus as i32,
            node_kind: self.q(EE_UNITS_SIGHT_U_NODE_KIND, u, 0, 0, 0) as i32,
        }
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
        with_world(|w| w.current_player().map_or(0, |p| p.handle()))
    }
    fn shared_los(&mut self, player: u64) -> bool {
        with_player(player, |p| p.shared_los())
    }
    fn allied(&mut self, a: u64, b: u64) -> bool {
        self.q(EE_UNITS_SIGHT_ALLIED, a, b, 0, 0) != 0
    }
    fn sharers(&mut self, player: u64) -> Vec<u64> {
        with_player(player, |p| (0..p.sight_sharers_len()).map(|i| p.sight_sharer_handle(i)).collect())
    }
    fn circle(&mut self, on: bool, x: f32, y: f32, radius: f32, player: u64) {
        let op = if on { EE_UNITS_SIGHT_CIRCLE } else { EE_UNITS_SIGHT_CIRCLE_OFF };
        self.op(op, on, player, 0, 0, 0, &mut [x, y, radius]);
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        // `gWorld.mMap->GetTile(x, y)`
        with_world(|w| w.map().and_then(|m| m.tile(i64::from(x), i64::from(y))).map_or(0, |t| t.handle()))
    }
    fn tile_sight(&mut self, on: bool, tile: u64, player: u64) {
        let op = if on { EE_UNITS_SIGHT_TILE_SIGHT } else { EE_UNITS_SIGHT_TILE_SIGHT_OFF };
        self.op(op, on, tile, player, 0, 0, &mut [0.0; 3]);
    }
    fn shared_sight(&mut self, on: bool, player: u64, tile: u64, x: i32, y: i32) {
        let op = if on { EE_UNITS_SIGHT_SHARED } else { EE_UNITS_SIGHT_SHARED_OFF };
        self.op(op, on, player, tile, x as i64, y as i64, &mut [0.0; 3]);
    }
    fn show(&mut self, on: bool, x: i32, y: i32) {
        let (cell, mesh) = if on {
            (EE_UNITS_SIGHT_SHOW, EE_UNITS_SIGHT_SHOW_TILE)
        } else {
            (EE_UNITS_SIGHT_SHOW_OFF, EE_UNITS_SIGHT_SHOW_TILE_OFF)
        };
        self.op(cell, on, 0, 0, x as i64, y as i64, &mut [0.0; 3]);
        self.op(mesh, on, 0, 0, x as i64, y as i64, &mut [0.0; 3]);
    }
    fn wonder_sight(&mut self, on: bool, player: u64, x: f32, y: f32) {
        let op = if on { EE_UNITS_SIGHT_WONDER } else { EE_UNITS_SIGHT_WONDER_OFF };
        self.op(op, on, player, 0, 0, 0, &mut [x, y, 0.0]);
    }
    fn num_players(&mut self) -> u32 {
        with_world(|w| w.num_players() as u32)
    }
    fn player_at(&mut self, i: u32) -> u64 {
        with_world(|w| w.player(i as usize).map_or(0, |p| p.handle()))
    }
}

/// `EEComplexUnit::SightOn(x, y)` (`on` 1) and `SightOff(x, y)` (0) of unit `unit`.
#[no_mangle]
pub extern "C" fn ee_units_sight(unit: u64, x: u32, y: u32, on: u8) {
    sight(&mut Game, unit, x, y, on != 0)
}
