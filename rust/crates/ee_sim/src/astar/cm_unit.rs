//! The continent manager's queries over a unit's class (the paths lane's round 1; was EEAIContinentManager.cpp and
//! EEComplexUnit.cpp): `CanGetTo`, `AreConnectedForUnit` with their CNTQ records (kinds 6 and 13) and
//! `EEComplexUnit::GetFootprintContinent`. The queries they make inside are the manager's own entry points
//! (`ee_astar_cmq_*`, recorded only when outermost, as the C++ calls were), the footprint the unit utilities'
//! `EEGetFootprintTiles` (its UUNT note). What stays C++ is the terrain mesh's space-map flag and the planet id an
//! aircraft without one is given (`ee_astar_cm_unit_host`).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_long;

/// `EEUnitClass::mDomains`' bits
pub const MOVEMENT_AIR: u8 = 1;
pub const MOVEMENT_SEA: u8 = 4;
pub const MOVEMENT_SEA_OR_SPACE: u8 = 4 | 8;
/// `kUnitCategoryPlanetSpaceship`, `kUnitTypeAirport`
pub const CATEGORY_PLANET_SPACESHIP: u64 = 0x22;
pub const TYPE_AIRPORT: u64 = 0x42;
/// `kAIPathNonSpace`: the region id kind of the land and water tiles
pub const PATH_NON_SPACE: c_long = 4;
/// the space terrain (`CanMoveOnTerrain(unit, 0xc)`)
pub const TERRAIN_SPACE: i32 = 0xc;

/// What `CanGetTo` reads of the unit and its complex unit.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Reach {
    /// the unit's class domains and whether it can move on space tiles
    pub domains: u8,
    pub can_space: bool,
    /// the complex unit's class: category, domains, type (None: no complex unit)
    pub complex: Option<(u64, u8, u64)>,
}

/// The world `CanGetTo` asks, in the original's order.
pub trait ReachWorld {
    /// whether both tiles are on the manager's grid
    fn on_grid(&mut self, from: (c_long, c_long), to: (c_long, c_long)) -> bool;
    /// `GetContinentAt(unit)`, `GetContinentAt(x, y)`
    fn continent_of_unit(&mut self) -> u64;
    fn continent_at(&mut self, x: c_long, y: c_long) -> u64;
    /// the terrain mesh's `GetIsSpaceMap()`
    fn space_map(&mut self) -> bool;
    /// the aircraft's `mPlanetID` (given one when 0)
    fn planet(&mut self) -> u16;
    /// `GetContinent(complex unit, kind)`, `GetContinent(tile, kind)`
    fn region_of_complex(&mut self, kind: c_long) -> u16;
    fn region_at(&mut self, x: c_long, y: c_long, kind: c_long) -> u16;
    /// `GetFootprintContinent(kind)` of the complex unit
    fn footprint_region(&mut self, kind: c_long) -> u16;
}

/// `EEAIContinentManager::CanGetTo(unit, x, y)`'s body (`tile` the unit's).
pub fn can_get_to(w: &mut dyn ReachWorld, r: Reach, tile: (c_long, c_long), x: c_long, y: c_long) -> bool {
    if !w.on_grid(tile, (x, y)) {
        return false;
    }
    let from = w.continent_of_unit();
    let to = w.continent_at(x, y);
    let blocked = !(r.domains & MOVEMENT_AIR == 0 || r.can_space);
    if from == to && !blocked {
        return true;
    }
    let Some((category, domains, unit_type)) = r.complex else { return false };
    if category == CATEGORY_PLANET_SPACESHIP {
        return true;
    }
    if domains & MOVEMENT_SEA_OR_SPACE == 0 {
        if domains & MOVEMENT_AIR == 0 && unit_type != TYPE_AIRPORT {
            return false;
        }
        // a map without space tiles needs no planet check
        if !w.space_map() || !blocked {
            return true;
        }
        let id = if domains & MOVEMENT_AIR != 0 { w.planet() } else { w.region_of_complex(PATH_NON_SPACE) };
        id != 0 && id == w.region_at(x, y, PATH_NON_SPACE)
    } else {
        let kind = if domains & MOVEMENT_SEA != 0 { 1 } else { 3 };
        let id = w.footprint_region(kind);
        id != 0 && id == w.region_at(x, y, kind)
    }
}

/// `GetFootprintContinent(kind)`'s choice: the first non-zero region of the footprint's tiles.
pub fn first_region(regions: impl IntoIterator<Item = u16>) -> u16 {
    regions.into_iter().find(|&r| r != 0).unwrap_or(0)
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::astar::cm_capture::Query;
    use crate::astar::cm_queries::{ee_astar_cmq_are_connected, ee_astar_cmq_region_id};
    use crate::astar::world_view::cm;
    use crate::world::{UnitView, WorldRead};

    extern "C" {
        /// 0: the terrain mesh's `GetIsSpaceMap()`; 1: complex unit `unit`'s `mPlanetID`, given its land continent
        /// (`EEGetLandContinentOf`, with the original's trace) when 0 (EEAIContinentManager.cpp)
        fn ee_astar_cm_unit_host(what: i32, unit: u64) -> u32;
    }

    fn token() -> WorldRead {
        // SAFETY: the simulation thread's reads of the live world
        unsafe { WorldRead::begin() }
    }

    fn region(x: c_long, y: c_long, kind: c_long) -> u16 {
        match cm() {
            // SAFETY: the registered live record (its own CNTQ scope)
            Some(r) => unsafe { ee_astar_cmq_region_id(r, x, y, kind) },
            None => 0,
        }
    }

    fn continent_at(x: c_long, y: c_long) -> u64 {
        let Some(r) = cm() else { return 0 };
        if (x as std::ffi::c_ulong) >= r.width || (y as std::ffi::c_ulong) >= r.height {
            return 0;
        }
        match crate::astar::cm_io::continent_id_at(r, x, y) {
            0 => 0,
            id => r.tables().tables[crate::astar::cm::EeAStarCmTable::Continents as usize]
                .get(id as usize)
                .copied()
                .unwrap_or(0),
        }
    }

    /// `GetFootprintContinent(kind)` of complex unit `unit`.
    pub fn footprint_region(unit: u64, kind: c_long) -> u16 {
        let mut tiles = Vec::new();
        crate::units::uu_ffi::unit_tiles(0, unit, &mut tiles);
        let regions: Vec<u16> = tiles.iter().map(|t| region(t[0] as c_long, t[1] as c_long, kind)).collect();
        first_region(regions)
    }

    struct World {
        unit: u64,
        complex: u64,
        tile: (c_long, c_long),
    }

    impl ReachWorld for World {
        fn on_grid(&mut self, from: (c_long, c_long), to: (c_long, c_long)) -> bool {
            let Some(r) = cm() else { return false };
            let on =
                |t: (c_long, c_long)| (t.0 as std::ffi::c_ulong) < r.width && (t.1 as std::ffi::c_ulong) < r.height;
            on(from) && on(to)
        }
        fn continent_of_unit(&mut self) -> u64 {
            continent_at(self.tile.0, self.tile.1)
        }
        fn continent_at(&mut self, x: c_long, y: c_long) -> u64 {
            continent_at(x, y)
        }
        fn space_map(&mut self) -> bool {
            // SAFETY: the C++ host's read of the live map
            unsafe { ee_astar_cm_unit_host(0, 0) != 0 }
        }
        fn planet(&mut self) -> u16 {
            // SAFETY: a live complex unit
            unsafe { ee_astar_cm_unit_host(1, self.complex) as u16 }
        }
        fn region_of_complex(&mut self, kind: c_long) -> u16 {
            let t = token();
            // SAFETY: a live complex unit
            let (x, y) = unsafe { UnitView::of(&t, self.complex) }.map_or((0, 0), |u| u.tile_xy());
            region(x as c_long, y as c_long, kind)
        }
        fn region_at(&mut self, x: c_long, y: c_long, kind: c_long) -> u16 {
            region(x, y, kind)
        }
        fn footprint_region(&mut self, kind: c_long) -> u16 {
            let _ = self.unit;
            footprint_region(self.complex, kind)
        }
    }

    /// `CanGetTo(unit, x, y)` with its CNTQ record.
    ///
    /// # Safety
    /// `unit` a live unit.
    pub unsafe fn can_get_to_unit(unit: u64, x: c_long, y: c_long) -> bool {
        let t = token();
        // SAFETY: the caller's contract
        let u = unsafe { UnitView::of(&t, unit) }.expect("a live unit");
        let q = Query::new(6);
        q.i(u.id() as i64);
        q.i(x as i64);
        q.i(y as i64);
        let can_space = |v: &UnitView<'_>| {
            let Some(c) = v.class() else { return false };
            let stats = v.stats().map(|s| s.fields());
            // SAFETY: live records of the unit's class and stats
            unsafe {
                crate::units::cls_answers::can_move_on_terrain(
                    &*c.fields().ptr(),
                    stats.map(|s| &*s.ptr()),
                    TERRAIN_SPACE,
                )
            }
        };
        let domains = u.class().map_or(0, |c| crate::world_field!(c.fields(), mDomains));
        let complex = u.complex_unit();
        let r = Reach {
            domains,
            can_space: can_space(&u),
            complex: complex
                .and_then(|c| c.class())
                .map(|c| (c.category(), crate::world_field!(c.fields(), mDomains), c.unit_type())),
        };
        let (tx, ty) = u.tile_xy();
        let mut w = World { unit, complex: u.complex_unit_handle(), tile: (tx as c_long, ty as c_long) };
        let result = can_get_to(&mut w, r, (tx as c_long, ty as c_long), x, y);
        q.i(result as i64);
        result
    }

    /// `EEAIContinentManager::UpdateWallUnit(unit, now)` with its CNTQ record (kind 24, the unit's id, `now`, the
    /// manager's state after): a finished blocking unit's or gate's footprint tiles queued for a sector rebuild and
    /// the path manager told (`SetRegionUpdates(true, now)`); nothing while the manager clears, before it has areas or
    /// without a map, nor for a foundation.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn update_wall_unit(unit: u64, now: bool) {
        let q = Query::new(24);
        let id = {
            let t = token();
            // SAFETY: the caller's contract
            unsafe { UnitView::of(&t, unit) }.map_or(-1, |u| u.id() as i64)
        };
        q.i(id);
        q.i(now as i64);
        // SAFETY: the caller's contract
        unsafe { update_wall(unit, now) };
        if q.on() {
            // SAFETY: the registered live record
            unsafe { crate::astar::cm_queries::ee_astar_cntq_state(crate::astar::world_view::cm_ptr()) };
        }
    }

    unsafe fn update_wall(unit: u64, now: bool) {
        let Some(r) = cm() else { return };
        let (has_map, foundation, blocks, category) = {
            let t = token();
            let map = crate::world::WorldView::global(&t).and_then(|w| w.map()).is_some();
            // SAFETY: the caller's contract
            let u = unsafe { UnitView::of(&t, unit) }.expect("a live unit");
            let foundation = u.is_complex() && crate::world_field!(u.complex(), mIsFoundation);
            let c = u.class();
            (map, foundation, c.is_some_and(|c| c.blocks_tile()), c.map_or(0, |c| c.category()))
        };
        if r.tables().areas.is_empty() || r.clearing || !has_map {
            return;
        }
        if foundation || (!blocks && category != crate::astar::world_view::CATEGORY_GATE) {
            return;
        }
        let mut tiles = Vec::new();
        crate::units::uu_ffi::unit_tiles(0, unit, &mut tiles);
        let points: Vec<crate::astar::hull::EeHullPoint> =
            tiles.iter().map(|t| crate::astar::hull::EeHullPoint { x: t[0] as c_long, y: t[1] as c_long }).collect();
        // SAFETY: the registered live record; the points live for the call
        unsafe {
            crate::astar::cm_passes::ee_astar_cm_add_sector_tiles(
                crate::astar::world_view::cm_ptr(),
                points.as_ptr(),
                points.len(),
            )
        };
        // gPathManager.SetRegionUpdates(true, now)
        let mut io = crate::astar::pm::EePathIo::default();
        io.i[0] = 1;
        io.i[1] = i64::from(now);
        // SAFETY: the game's live manager
        unsafe { crate::astar::pm::ee_path_manager_call(crate::astar::pm::global(), 6, &mut io) };
    }

    /// `AreConnectedForUnit(unit, first, second)` with its CNTQ record (the unit's passable flags; 0 without one).
    ///
    /// # Safety
    /// `unit` 0 or a live unit; the continents live areas (or 0).
    pub unsafe fn connected_for_unit(unit: u64, first: u64, second: u64) -> bool {
        let t = token();
        // SAFETY: the caller's contract
        let u = unsafe { UnitView::of(&t, unit) };
        let q = Query::new(13);
        q.i(u.map_or(-1, |u| u.id() as i64));
        q.area(first);
        q.area(second);
        let flags = match u.and_then(|u| u.class().map(|c| (c, u.stats()))) {
            // SAFETY: live records of the unit's class and stats
            Some((c, s)) => unsafe {
                crate::units::cls_answers::passable_flags(&*c.fields().ptr(), s.map(|s| &*s.fields().ptr()))
            },
            None => 0,
        };
        // SAFETY: as above
        let result = unsafe { ee_astar_cmq_are_connected(first, second, flags as c_long) };
        q.i(result as i64);
        result
    }
}

#[cfg(not(test))]
pub use game::*;

/// `EEAIContinentManager::CanGetTo(unit, x, y)`.
///
/// # Safety
/// `unit` a live unit.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_can_get_to(unit: u64, x: c_long, y: c_long) -> bool {
    // SAFETY: the caller's contract
    unsafe { can_get_to_unit(unit, x, y) }
}

#[cfg(test)]
#[path = "cm_unit_tests.rs"]
mod tests;
