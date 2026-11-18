//! `EEInfluenceTable` (RTTI EEInfluenceTable > RTSRegionTable<std::pair<unsigned long, long> >, 0xa5589..0xa5c31)
//! whole in Rust: per map region (9 x 9 tiles) and player, the summed influence (`EEUnitClass::mInfluence`, +0x244)
//! of the units there, and the total. The C++ class is a shim over this object (ee/EEInfluenceTable.h) that keeps
//! `mRegionSize` for the readers that divide tile coordinates by it.
//!
//! The original held a `std::vector<std::pair<player, value>>` per region with `players + 1` entries (`Init` pushed
//! `(i, 0)` for each player, then the total); the pairs' first members were the indices, so the values are one flat
//! array here, `players + 1` per region, regions in the original's order (`x * regions_y + y`).
//!
//! Kept: the region count per side is `UMCeilUnsigned(size / 9.0f)`; a region outside the grid reads 0 (the
//! coordinates as unsigned: a negative one is off the grid); `GetValue` of a player past the entries reads 0 (the
//! route planner passes -1); the removals clamp at 0; `SumValue`'s rectangle is not clipped, `SumOthersValue`'s and
//! `SumEnemyValue`'s start corner is clipped at 0. The enemies are read through the world's views: the players at
//! war with the given one (`mEnemies[id] != 0`), player 0 only when asked.

#![allow(clippy::unnecessary_cast)]

use ee_base::random::float_to_long;

use crate::world::{PlayerView, UnitView, WorldRead, WorldView};

/// The table (the C++ shim's `mTable`).
pub struct EeCpInfluence {
    regions_x: u32,
    regions_y: u32,
    region_size: i32,
    players: u32,
    values: Vec<i32>,
}

/// A rectangle of regions or tiles, edges included (`U2DRectangle<long>`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCpRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// `UMCeilUnsigned`: rounded to nearest, then up when below.
fn ceil_unsigned(v: f32) -> u32 {
    let mut r = float_to_long(v) as u32;
    if (r as f32) < v {
        r = r.wrapping_add(1);
    }
    r
}

impl EeCpInfluence {
    /// `EEInfluenceTable(map, players)`: `RTSRegionTable(map, region_size)` and `Init` (every entry 0).
    pub fn new(map_size: u32, region_size: i32, players: u32) -> EeCpInfluence {
        let count = ceil_unsigned(map_size as f32 / region_size as f32);
        let regions = count as usize * count as usize;
        EeCpInfluence {
            regions_x: count,
            regions_y: count,
            region_size,
            players,
            values: vec![0; regions * (players as usize + 1)],
        }
    }

    pub fn region_size(&self) -> i32 {
        self.region_size
    }

    /// The first entry of region (x, y) (`GetRegion`: None off the grid).
    fn region(&self, x: u32, y: u32) -> Option<usize> {
        (x < self.regions_x && y < self.regions_y)
            .then(|| (x as usize * self.regions_y as usize + y as usize) * (self.players as usize + 1))
    }

    /// `GetValue`: `player`'s value in region (x, y); 0 off the grid or past the entries.
    pub fn value(&self, player: u32, x: u32, y: u32) -> i32 {
        match self.region(x, y) {
            Some(at) if player <= self.players => self.values[at + player as usize],
            _ => 0,
        }
    }

    /// `GetOthersValue`: the total minus `player`'s; 0 off the grid.
    pub fn others_value(&self, player: u32, x: u32, y: u32) -> i32 {
        match self.region(x, y) {
            Some(at) => {
                let own = if player <= self.players { self.values[at + player as usize] } else { 0 };
                self.values[at + self.players as usize].wrapping_sub(own)
            }
            None => 0,
        }
    }

    /// `SumValue`: `player`'s values over the regions of `r` (not clipped).
    pub fn sum_value(&self, player: u32, r: EeCpRect) -> i32 {
        let mut total = 0i32;
        for x in r.left..=r.right {
            for y in r.top..=r.bottom {
                total = total.wrapping_add(self.value(player, x as u32, y as u32));
            }
        }
        total
    }

    /// `SumOthersValue`: the others' values over the regions of `r`, its start corner clipped at 0.
    pub fn sum_others_value(&self, player: u32, r: EeCpRect) -> i32 {
        let mut total = 0i32;
        for x in r.left.max(0)..=r.right {
            for y in r.top.max(0)..=r.bottom {
                total = total.wrapping_add(self.others_value(player, x as u32, y as u32));
            }
        }
        total
    }

    /// `AddUnit`: `influence` added to `player`'s and the total entry of the region of tile (x, y).
    pub fn add(&mut self, player: u32, influence: i32, x: i32, y: i32) {
        if let Some(at) = self.region((x / self.region_size) as u32, (y / self.region_size) as u32) {
            let own = at + player as usize;
            self.values[own] = self.values[own].wrapping_add(influence);
            let total = at + self.players as usize;
            self.values[total] = self.values[total].wrapping_add(influence);
        }
    }

    /// `RemoveUnit`: the same subtracted, each clamped at 0.
    pub fn remove(&mut self, player: u32, influence: i32, x: i32, y: i32) {
        if let Some(at) = self.region((x / self.region_size) as u32, (y / self.region_size) as u32) {
            for i in [at + player as usize, at + self.players as usize] {
                let v = self.values[i].wrapping_sub(influence);
                self.values[i] = v.max(0);
            }
        }
    }

    /// `GetEnemyValue`: the values in region (x, y) of the players `enemies` lists ([`enemy_indices`]).
    pub fn enemy_value(&self, enemies: &[usize], x: i32, y: i32) -> i32 {
        let Some(at) = self.region(x as u32, y as u32) else {
            return 0;
        };
        let mut total = 0i32;
        for &i in enemies {
            total = total.wrapping_add(self.values[at + i]);
        }
        total
    }

    /// `SumEnemyValue`: [`enemy_value`](Self::enemy_value) over the regions of `r`, its start corner clipped at 0.
    pub fn sum_enemy_value(&self, enemies: &[usize], r: EeCpRect) -> i32 {
        let mut total = 0i32;
        for x in r.left.max(0)..=r.right {
            for y in r.top.max(0)..=r.bottom {
                for &i in enemies {
                    total = total.wrapping_add(self.value(i as u32, x as u32, y as u32));
                }
            }
        }
        total
    }

    pub fn players(&self) -> u32 {
        self.players
    }
}

/// The enemies' loop of `GetEnemyValue` and `SumEnemyValue`: the slots from `!with_first` (player 0 only when asked)
/// to the table's `players` whose player (`slot(i)`: its handle and id) is not `me` and with which `me` is at war
/// (`at_war(id)`: `mEnemies[id] != 0`). None when `me` has no player (the original's `thePlayer != NULL`).
pub fn enemy_indices(
    players: u32,
    with_first: bool,
    me: Option<u64>,
    slot: impl Fn(usize) -> Option<(u64, u32)>,
    at_war: impl Fn(u32) -> bool,
) -> Option<Vec<usize>> {
    let me = me?;
    let mut out = Vec::new();
    for i in usize::from(!with_first)..players as usize {
        if let Some((handle, id)) = slot(i) {
            if handle != me && at_war(id) {
                out.push(i);
            }
        }
    }
    Some(out)
}

/// [`enemy_indices`] over the world's views: `player`'s slot of `world`.
///
/// # Safety
/// `world` is the live world (`&gWorld`) while the caller holds it.
pub unsafe fn world_enemies(world: u64, player: u32, players: u32, with_first: bool) -> Option<Vec<usize>> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let w = unsafe { WorldView::of(&t, world) }?;
    let me: Option<PlayerView<'_>> = w.player(player as usize);
    enemy_indices(
        players,
        with_first,
        me.as_ref().map(|p| p.handle()),
        |i| w.player(i).map(|p| (p.handle(), p.id())),
        |id| me.as_ref().is_some_and(|p| p.enemy(id as usize) != 0),
    )
}

/// A unit's owner id and class influence (`mPlayer->mPlayerID`, `mUnitClass->mInfluence`) through the views.
///
/// # Safety
/// `unit` is a live unit's handle while the caller holds the world.
pub unsafe fn unit_influence(unit: u64) -> Option<(u32, i32)> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let u = unsafe { UnitView::of(&t, unit) }?;
    let influence = crate::world_field!(u.class()?.fields(), mInfluence) as i32;
    Some((u.player()?.id(), influence))
}

#[cfg(test)]
#[path = "influence_tests.rs"]
mod tests;
