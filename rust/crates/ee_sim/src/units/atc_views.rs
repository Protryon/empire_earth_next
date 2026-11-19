//! The air traffic control's plain reads over the world views (the units' state lane, round 1): every
//! `EE_UNITS_ATC_*` read that is a member of the unit, its class or the units its members name is answered here in
//! place; the C++ host (Physics Thread/EEAirTrafficControl.cpp) keeps the reference counts, the setters and the
//! world calls (kill, reroute, the exiting unit, the id lookup, the dump's lines).

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::atc::*;
use crate::world::view::{UnitView, WorldRead};
use crate::world_field;

/// `kUnitCategoryAirplane`, `kUnitCategoryPlanetSpaceship` (ee/EEUnitClassIDs.h)
const CATEGORY_AIRPLANE: u64 = 3;
const CATEGORY_PLANET_SPACESHIP: u64 = 0x22;

/// The read `op` of `unit` (with `other` for `IN_CARGO`); None for one the C++ host answers, or before the layout
/// is registered (the tests' mock host).
pub(crate) fn view_call(op: i32, unit: Unit, other: Unit, io: &mut EeUnitsAtcIo) -> Option<i64> {
    crate::world::layout::get()?;
    // SAFETY: the control runs on the thread that holds the world; its units are live complex units
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = unsafe { UnitView::of(&t, unit as usize as u64) }?;
    if !u.is_complex() {
        return None;
    }
    let class = || u.class().map(|c| c.fields());
    Some(match op {
        EE_UNITS_ATC_WEIGHT => world_field!(class()?, mWeight) as i64,
        EE_UNITS_ATC_TRANSPORT_CAPACITY => world_field!(class()?, mTransportCapacity) as i64,
        EE_UNITS_ATC_CLEARANCE => i64::from(world_field!(u.complex(), mIsClearanceGranted)),
        EE_UNITS_ATC_HIT_POINTS => u.hit_points(),
        EE_UNITS_ATC_CARGO_ROOM => world_field!(u.complex(), mCargoRoom) as i64,
        EE_UNITS_ATC_IN_CARGO => {
            let o = other as usize as u64;
            i64::from((0..u.cargo_len()).any(|i| u.cargo_handle(i) == o))
        }
        EE_UNITS_ATC_FAMILY => u.class()?.family() as i64,
        EE_UNITS_ATC_COMMAND_TYPE => i64::from(u.command_type()),
        EE_UNITS_ATC_TILE => {
            let (x, y) = u.tile_xy();
            io.a = x;
            io.b = y;
            0
        }
        EE_UNITS_ATC_LOCATION => {
            io.p = u.location();
            0
        }
        EE_UNITS_ATC_AIR_TRAFFIC_TYPE => world_field!(class()?, mAirTrafficType) as i64,
        EE_UNITS_ATC_CRUISE_ALTITUDE => {
            io.f = world_field!(class()?, mCruiseAltitude);
            0
        }
        EE_UNITS_ATC_IN_CONTAINER => i64::from(u.container_handle() != 0),
        EE_UNITS_ATC_IS_AIRCRAFT => {
            let c = u.class()?.category();
            i64::from(c == CATEGORY_AIRPLANE || c == CATEGORY_PLANET_SPACESHIP)
        }
        EE_UNITS_ATC_HOME_BASE => u.home_base_handle() as i64,
        EE_UNITS_ATC_UNIT_ID => world_field!(u.rts(), mUnitID) as i64,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn no_layout_no_answer() {
        // the unit tests register no layout: the mock hosts answer
        if crate::world::layout::get().is_none() {
            let mut io = EeUnitsAtcIo::default();
            assert_eq!(view_call(EE_UNITS_ATC_WEIGHT, std::ptr::null_mut(), std::ptr::null_mut(), &mut io), None);
        }
    }

    use std::sync::atomic::{AtomicI64, Ordering};
    static REFS: AtomicI64 = AtomicI64::new(0);

    unsafe extern "C" fn host(_: *mut std::ffi::c_void, op: i32, _: Unit, _: Unit, _: *mut EeUnitsAtcIo) -> i64 {
        match op {
            EE_UNITS_ATC_REFERENCE => REFS.fetch_add(1, Ordering::SeqCst),
            EE_UNITS_ATC_DEREFERENCE => REFS.fetch_sub(1, Ordering::SeqCst),
            _ => 0,
        };
        0
    }

    #[test]
    fn the_entry_points_call_the_registered_host() {
        let airport = 0x1000 as Unit;
        let p = ee_units_atc_new(host);
        // SAFETY: a state just made; the airport is never read by the mock host
        unsafe {
            ee_units_atc_add_airport(p, airport);
            assert_eq!(ee_units_atc_slot_count(p), 1);
            assert_eq!(ee_units_atc_slot_airport(p, 0), airport);
            assert_eq!(ee_units_atc_queue_length(p, airport), 0);
            assert_eq!(ee_units_atc_cargo(p, 1), 0);
            ee_units_atc_remove_airport(p, airport);
            assert_eq!(ee_units_atc_slot_count(p), 0);
            ee_units_atc_delete(p);
        }
        // the airport's reference was taken and given back
        assert_eq!(REFS.load(Ordering::SeqCst), 0);
    }
}
