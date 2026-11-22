//! The target searches' world (`search.rs`) over the world views: the unit regions, the unit store's tiles, the
//! candidates' own values and the plain tests; the other tests and the searching unit's calls through
//! [`SearchCallbacks`] (the unit utilities' search bodies, units/uu_search.rs, over the C++'s statements). Every handle
//! a C++ pointer; nothing is kept after a search returns. A unit's tile is the unit store's (`store.rs`).

use crate::combat::search::*;
use crate::combat::store;
use crate::combat::walks::EeCombatHandle;

/// The calls of a search that are not reads of the views (see `SearchHost`).
pub trait SearchCallbacks {
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle;
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool;
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool;
    fn hit_distance_squared(&mut self, unit: EeCombatHandle, x: f32, y: f32) -> f32;
    fn attack_factor(&mut self, unit: EeCombatHandle) -> f32;
    fn line_blocked(&mut self, unit: EeCombatHandle) -> bool;
    fn reachable(&mut self, unit: EeCombatHandle) -> bool;
    fn ignore(&mut self, unit: EeCombatHandle);
    fn footprint_tile(&mut self, unit: EeCombatHandle) -> (i32, i32);
    fn trace(&mut self, unit: EeCombatHandle, code: i32, score: i32);
}

/// The searches' world: the map's unit regions (`gWorld.mMap->mUnitRegions`, regions.rs, live for the search) and
/// the callbacks.
pub struct SearchWorld<'c, C: SearchCallbacks + ?Sized> {
    pub unit_regions: *const crate::combat::regions::EeCombatUnitRegions,
    pub calls: &'c mut C,
}

/// `f` of the view of a candidate (a handle the C++ search gave: a live unit while the search runs).
fn with_unit<R>(unit: EeCombatHandle, f: impl FnOnce(crate::world::UnitView<'_>) -> R) -> R {
    // SAFETY: a search runs inside the C++ call that holds the world
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above; the handle is a live unit
    f(unsafe { crate::world::UnitView::of(&t, unit) }.expect("a candidate"))
}

impl<C: SearchCallbacks + ?Sized> SearchWorld<'_, C> {
    fn table(&self) -> &crate::combat::regions::EeCombatUnitRegions {
        // SAFETY: the table's contract: the regions are live for the call that handed them over
        unsafe { &*self.unit_regions }
    }
}

impl<C: SearchCallbacks + ?Sized> SearchHost for SearchWorld<'_, C> {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        self.table().rect(left, top, right, bottom)
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        self.table().region(x as u32, y as u32).map_or(-1, |r| r.len() as i32)
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> EeCombatHandle {
        self.table().region(x as u32, y as u32).map_or(0, |r| r[index as usize])
    }
    fn unit_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        // the unit store's record (ee_combat store.rs: every handle here a live complex unit)
        store::unit_of(unit).map_or((0, 0), |r| (r.tile_x as i32, r.tile_y as i32))
    }
    fn unit_position(&mut self, unit: EeCombatHandle) -> (f32, f32) {
        // the candidate's complex unit's location (a region's unit is its own complex unit)
        with_unit(unit, |u| {
            let p = u.complex_unit().expect("a candidate with a complex unit").location();
            (p[0], p[1])
        })
    }
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle {
        self.calls.list_next(unit, prev)
    }
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool {
        self.calls.dummy_head(unit)
    }
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool {
        // the plain reads, through the views; the others the C++'s
        match test {
            EE_COMBAT_SEARCH_TEST_NATURE => with_unit(unit, |u| u.player().expect("an owner").id() == 0),
            EE_COMBAT_SEARCH_TEST_WALL => with_unit(unit, |u| {
                let c = u.class().expect("a candidate's class");
                let tower: bool = crate::world_field!(c.fields(), mIsTower);
                c.wall() && !tower
            }),
            EE_COMBAT_SEARCH_TEST_ALIVE => with_unit(unit, |u| u.hit_points() > 0),
            _ => self.calls.test(test, unit),
        }
    }
    fn target(&mut self, unit: EeCombatHandle) -> EeCombatSearchTarget {
        // its class's size and type, its stats' attack and maximum velocity
        with_unit(unit, |u| {
            let class = u.class().expect("a candidate's class");
            let size: std::os::raw::c_long = crate::world_field!(class.fields(), mTileSize);
            let stats = u.stats().expect("a candidate's stats");
            EeCombatSearchTarget {
                multiple: i32::from(size > 1),
                unit_type: class.unit_type() as i32,
                attack: crate::world_field!(stats.fields(), mAttack),
                max_velocity: crate::world_field!(stats.fields(), mMaxVelocity),
            }
        })
    }
    fn hit_distance_squared(&mut self, unit: EeCombatHandle, x: f32, y: f32) -> f32 {
        self.calls.hit_distance_squared(unit, x, y)
    }
    fn attack_factor(&mut self, unit: EeCombatHandle) -> f32 {
        self.calls.attack_factor(unit)
    }
    fn line_blocked(&mut self, unit: EeCombatHandle) -> bool {
        self.calls.line_blocked(unit)
    }
    fn reachable(&mut self, unit: EeCombatHandle) -> bool {
        self.calls.reachable(unit)
    }
    fn ignore(&mut self, unit: EeCombatHandle) {
        self.calls.ignore(unit)
    }
    fn footprint_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        self.calls.footprint_tile(unit)
    }
    fn trace(&mut self, unit: EeCombatHandle, code: i32, score: i32) {
        self.calls.trace(unit, code, score)
    }
}
