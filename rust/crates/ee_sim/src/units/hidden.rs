//! `EEUnit::UpdateHidden` and `EEComplexUnit::UpdateHidden` whole (the units' round 1, branch rust-units1): the
//! hidden flags (`EEUnit::mHiddenFlags`: bit 4 moved, bit 5 hidden, bits 0-1 the check's phase) of the obstacles
//! behind a unit and of a unit behind them, read and written through the world views. The renderer's opacity
//! (ee_render fill.rs) and `EEUnit::MoveTo` call it; the C++ virtuals are one call of [`ee_units_update_hidden`].
//!
//! The walk is over [`HiddenHost`] (the views in the game, a mock in the tests): the same tiles in the same order,
//! each flag written as the original wrote it (a later read of the same unit sees it).

use crate::world::view::{UnitView, WorldRead, WorldView};

/// `kHiddenFlagPhaseMask`, `kHiddenFlagMoved`, `kHiddenFlagHidden` (ee/EEUnitClassIDs.h)
pub const EE_UNITS_HIDDEN_PHASE_MASK: u8 = 3;
pub const EE_UNITS_HIDDEN_MOVED: u8 = 0x10;
pub const EE_UNITS_HIDDEN_HIDDEN: u8 = 0x20;

const OFF_MAP: i64 = -10000;
/// `kResourceFood`
const RESOURCE_FOOD: u64 = 2;
/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const CATEGORY_LAND_CITIZEN: u64 = 6;
const CATEGORY_NAVAL_CITIZEN: u64 = 0x16;

/// What the walk reads of a unit on a tile (an `EEUnit*` of a tile node).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct HiddenFacts {
    /// `mComplexUnit` (0: none)
    pub complex: u64,
    /// its class's `mIsObstacle`, `mIsProjectile`, `mMaxVelocity`, `mCategory`
    pub obstacle: bool,
    pub projectile: bool,
    pub max_velocity: f32,
    pub category: u64,
    pub resource_type: u64,
    pub resource_amount: i64,
    pub hit_points: i64,
    /// its player's id (0 for none)
    pub owner: u32,
    /// `mCanBeHidden` (a complex unit's)
    pub can_be_hidden: bool,
    /// `mNextPart` (a complex unit's; 0: none)
    pub next_part: u64,
}

/// What the walk reads of the unit it updates.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct HiddenSelf {
    pub complex: bool,
    pub tile: (i64, i64),
    /// its class's `mTileSize`
    pub tile_size: i64,
    /// (complex only) `mCanBeHidden`, `mIsCloakable && mIsCloaked`, `mIsFoundation`
    pub can_be_hidden: bool,
    pub cloaked: bool,
    pub foundation: bool,
    /// (complex only) the local player is its owner, or its owner is not the local player's enemy
    pub seen_by_local: bool,
    /// `mNextPart` (complex only; 0: none)
    pub next_part: u64,
}

/// The world the walk reads and writes.
pub trait HiddenHost {
    /// `gWorld.mInCinematic`, `gPhysics->mGameTime`
    fn in_cinematic(&self) -> bool;
    fn game_time(&self) -> i32;
    /// the units of the map's tile (x, y) in node order (None off the map; a node without a unit is left out)
    fn tile_units(&self, x: i64, y: i64) -> Option<Vec<u64>>;
    fn facts(&self, unit: u64) -> HiddenFacts;
    fn flags(&self, unit: u64) -> u8;
    fn set_flags(&mut self, unit: u64, flags: u8);
}

/// The hidden bit set and the phase put into the low two bits (the original's xor; and 3; xor).
fn mark(host: &mut dyn HiddenHost, unit: u64, phase: u8) {
    let f = host.flags(unit) | EE_UNITS_HIDDEN_HIDDEN;
    host.set_flags(unit, f ^ ((f ^ phase) & EE_UNITS_HIDDEN_PHASE_MASK));
}

fn clear(host: &mut dyn HiddenHost, unit: u64, bits: u8) {
    let f = host.flags(unit) & !bits;
    host.set_flags(unit, f);
}

/// `((gPhysics->mGameTime + 2) / 1000) % 3`, the phase a found unit is marked with
fn next_phase(host: &dyn HiddenHost) -> u8 {
    (host.game_time().wrapping_add(2) / 1000 % 3) as u8
}

/// the hidden flag set and its phase due: `(signed char)(flags & 3) <= (signed char)((mGameTime / 1000) % 3)`
fn due(host: &dyn HiddenHost, flags: u8) -> bool {
    flags & EE_UNITS_HIDDEN_HIDDEN == EE_UNITS_HIDDEN_HIDDEN
        && ((flags & EE_UNITS_HIDDEN_PHASE_MASK) as i8) <= ((host.game_time() / 1000 % 3) as i8)
}

/// The tiles a unit is checked against, in the order the walk reads them (from the last): a large unit's own
/// tile, then its west column from the far row up and its far row from the east; a one-tile unit's own tile,
/// south, south-west and west.
fn check_tiles(x: i64, y: i64, size: i64) -> Vec<(i64, i64)> {
    let mut tiles = Vec::new();
    if size > 1 {
        tiles.push((x, y));
        let mut ty = y + size;
        while ty >= y {
            tiles.push((x - 1, ty));
            ty -= 1;
        }
        let mut tx = x - 1 + size;
        while tx >= x {
            tiles.push((tx, y + size));
            tx -= 1;
        }
    } else {
        tiles.extend([(x, y), (x, y + 1), (x - 1, y + 1), (x - 1, y)]);
    }
    tiles
}

/// Whether a unit on a checked tile hides the unit: a complex one that can be hidden and is alive (or an
/// unowned food resource that is no citizen), else a moving object that is no obstacle or projectile.
fn blocks(host: &dyn HiddenHost, node_unit: u64) -> bool {
    let f = host.facts(node_unit);
    if f.complex != 0 {
        let c = host.facts(f.complex);
        let resource = c.resource_type == RESOURCE_FOOD
            && c.resource_amount > 0
            && c.complex != 0
            && c.category != CATEGORY_LAND_CITIZEN
            && c.category != CATEGORY_NAVAL_CITIZEN
            && c.owner == 0;
        c.can_be_hidden && (c.hit_points > 0 || resource)
    } else {
        !f.obstacle && !f.projectile && f.max_velocity > 0.0
    }
}

/// The check of a unit against the tiles around it (`chain`: its parts are marked with it).
fn check(host: &mut dyn HiddenHost, unit: u64, me: &HiddenSelf, chain: bool) {
    clear(host, unit, EE_UNITS_HIDDEN_HIDDEN | EE_UNITS_HIDDEN_PHASE_MASK);
    let phase = next_phase(host);
    let tiles = check_tiles(me.tile.0, me.tile.1, me.tile_size);
    for &(x, y) in tiles.iter().rev() {
        let Some(units) = host.tile_units(x, y) else { continue };
        if let Some(_found) = units.into_iter().find(|&u| blocks(host, u)) {
            mark(host, unit, phase);
            if chain {
                let mut part = me.next_part;
                while part != 0 {
                    mark(host, part, phase);
                    part = host.facts(part).next_part;
                }
            }
            return;
        }
    }
}

/// `EEComplexUnit::UpdateHidden` (`me.complex`) or `EEUnit::UpdateHidden`: returns whether the unit is hidden
/// and no foundation.
pub fn update_hidden(host: &mut dyn HiddenHost, unit: u64, me: &HiddenSelf) -> bool {
    if me.complex {
        if me.can_be_hidden && (!me.cloaked || me.seen_by_local) && me.tile.0 != OFF_MAP {
            if !host.in_cinematic() {
                if host.flags(unit) & EE_UNITS_HIDDEN_MOVED == EE_UNITS_HIDDEN_MOVED {
                    // the obstacles on its tile and north, north-east and east of it are hidden behind it
                    let phase = next_phase(host);
                    for (dx, dy) in [(0, 0), (0, -1), (1, -1), (1, 0)] {
                        let Some(units) = host.tile_units(me.tile.0 + dx, me.tile.1 + dy) else { continue };
                        for u in units {
                            if u == unit || !host.facts(u).obstacle {
                                continue;
                            }
                            mark(host, u, phase);
                            let mut part = host.facts(u).complex;
                            if part == 0 {
                                continue;
                            }
                            loop {
                                part = host.facts(part).next_part;
                                if part == 0 {
                                    break;
                                }
                                mark(host, part, phase);
                            }
                        }
                    }
                    clear(host, unit, EE_UNITS_HIDDEN_MOVED);
                } else {
                    clear(host, unit, EE_UNITS_HIDDEN_HIDDEN);
                }
            }
        } else if due(host, host.flags(unit)) {
            if !host.in_cinematic() {
                check(host, unit, me, true);
            } else {
                clear(host, unit, EE_UNITS_HIDDEN_HIDDEN);
            }
        }
    } else if !host.in_cinematic() {
        if due(host, host.flags(unit)) {
            check(host, unit, me, false);
        }
    } else {
        clear(host, unit, EE_UNITS_HIDDEN_HIDDEN);
    }
    host.flags(unit) & EE_UNITS_HIDDEN_HIDDEN == EE_UNITS_HIDDEN_HIDDEN && !me.foundation
}

// ---- over the world views

/// The views as the walk's world (the caller's token; the writes go to the units' `EEUnit` records in place).
pub struct ViewHidden<'t> {
    t: &'t WorldRead,
    world: Option<WorldView<'t>>,
}

impl<'t> ViewHidden<'t> {
    pub fn new(t: &'t WorldRead) -> Self {
        ViewHidden { t, world: WorldView::global(t) }
    }
    fn unit(&self, unit: u64) -> Option<UnitView<'t>> {
        // SAFETY: the walk's handles are live units: the caller's, the tiles' nodes' and their pointer members
        unsafe { UnitView::of(self.t, unit) }
    }
    /// What the walk reads of `unit` itself.
    pub fn me(&self, unit: u64) -> Option<HiddenSelf> {
        let u = self.unit(unit)?;
        let class = u.class()?;
        let mut me = HiddenSelf {
            complex: u.is_complex(),
            tile: u.tile_xy(),
            tile_size: class.tile_size(),
            ..HiddenSelf::default()
        };
        if me.complex {
            let c = u.complex();
            me.can_be_hidden = crate::world_field!(c, mCanBeHidden);
            me.cloaked = u.cloakable() && u.cloaked();
            me.foundation = crate::world_field!(c, mIsFoundation);
            me.next_part = u.next_part_handle();
            let local = self.world.and_then(|w| w.current_player());
            me.seen_by_local = match (local, u.player()) {
                (Some(l), Some(p)) => l.handle() == p.handle() || p.enemy(l.id() as usize) == 0,
                (None, _) => true,
                (Some(_), None) => false,
            };
        }
        Some(me)
    }
}

impl HiddenHost for ViewHidden<'_> {
    fn in_cinematic(&self) -> bool {
        self.world.is_some_and(|w| w.in_cinematic())
    }
    fn game_time(&self) -> i32 {
        self.world.map_or(0, |w| w.game_time() as i32)
    }
    fn tile_units(&self, x: i64, y: i64) -> Option<Vec<u64>> {
        let tile = self.world?.map()?.tile(x, y)?;
        Some(tile.units().flatten().map(|u| u.handle()).collect())
    }
    fn facts(&self, unit: u64) -> HiddenFacts {
        let Some(u) = self.unit(unit) else { return HiddenFacts::default() };
        let mut f = HiddenFacts {
            resource_type: u.resource_type(),
            resource_amount: u.resource_amount(),
            hit_points: u.hit_points(),
            owner: u.player().map_or(0, |p| p.id()),
            ..HiddenFacts::default()
        };
        if u.is_unit() {
            f.complex = u.complex_unit_handle();
            if let Some(c) = u.class() {
                f.obstacle = crate::world_field!(c.fields(), mIsObstacle);
                f.projectile = crate::world_field!(c.fields(), mIsProjectile);
                f.max_velocity = crate::world_field!(c.fields(), mMaxVelocity);
                f.category = c.category();
            }
        }
        if u.is_complex() {
            f.can_be_hidden = crate::world_field!(u.complex(), mCanBeHidden);
            f.next_part = u.next_part_handle();
        }
        f
    }
    fn flags(&self, unit: u64) -> u8 {
        self.unit(unit).map_or(0, |u| u.hidden_flags())
    }
    fn set_flags(&mut self, unit: u64, flags: u8) {
        if let Some(u) = self.unit(unit) {
            let at = u.fields().ptr() as *mut crate::world::unit_fields::EeUnitsUnitFields;
            // SAFETY: a live unit's own record member, written on the thread that holds the world (the caller's
            // token): the original's write of `mHiddenFlags`
            unsafe { std::ptr::addr_of_mut!((*at).mHiddenFlags).write(flags) };
        }
    }
}

/// The unit's `UpdateHidden()` (an `EEUnit*` or `EEComplexUnit*`, by its object kind) over the world views, with
/// the caller's token.
pub fn update_hidden_view(t: &WorldRead, unit: u64) -> bool {
    let mut host = ViewHidden::new(t);
    match host.me(unit) {
        Some(me) => update_hidden(&mut host, unit, &me),
        None => false,
    }
}

/// `EEUnit::UpdateHidden()` and `EEComplexUnit::UpdateHidden()` of `unit` (an `EEUnit*`; its object kind picks
/// the body).
///
/// # Safety
/// `unit` is a live unit; the caller holds the world (the simulation thread, or the renderer's read).
#[no_mangle]
pub unsafe extern "C" fn ee_units_update_hidden(unit: u64) -> bool {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    update_hidden_view(&t, unit)
}

#[cfg(test)]
#[path = "hidden_tests.rs"]
mod tests;
