//! The air classes' host's statements answered over the world views (the units' lane, round 14):
//! EEUnitClassAir.cpp's reads and stores of plain members, its closest tile (this thread's cell, its address passed
//! in `a`) and the carrier's fighters (their technology ids passed in `c`). Asked by
//! [`kind_view`](super::uc_kind_view::kind_view) for the air kind after its plain reads.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::cell::Cell;
use std::os::raw::{c_long, c_ulong, c_void};

use crate::units::uc_body::game::ee_units_uc_host;
use crate::units::uc_view::{complex_mut, Ctx};
use crate::units::uca_air::*;
use crate::world::unit_fields::EeUnitsComplexUnitFields;
use crate::world_field;

thread_local! {
    /// GetGroundHeight's closest tile's x and y (not read)
    static TILE: Cell<[c_long; 2]> = const { Cell::new([0; 2]) };
}

/// the carrier's fighters, best first: `kTechInf15Watchman`, `kTechInf14Guardian`, `kTechInf13Sentinel`
/// (rust/sim/tech_tree_ids.h)
const FIGHTERS: [i64; 3] = [15027, 14101, 13012];

fn host(x: &Ctx<'_>, op: i32, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
    let p = |h: u64| h as usize as *mut c_void;
    // SAFETY: the C++ host's statement with the live class, unit and objects the body handed over
    unsafe { ee_units_uc_host(op, p(x.class), p(x.unit), a, b, c, f) }
}

fn set_f(f: &mut [f32; 4], v: f32) -> i64 {
    f[0] = v;
    0
}

/// The unit's record, for a store.
fn rec(x: &Ctx<'_>) -> Option<*mut EeUnitsComplexUnitFields> {
    Some(complex_mut(&x.u()?))
}

/// The air kind's statement `op` when it is one of these.
pub(crate) fn air_view(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let u = || x.u();
    Some(match op {
        EE_UNITS_UCA_HAS_PROJECTILE => {
            i64::from(!world_field!(crate::units::uc_view::links(&u()?.class()?), mProjectileClass).is_null())
        }
        EE_UNITS_UCA_LOCATION_OF => {
            let at = x.o()?.location();
            f[..3].copy_from_slice(&at);
            0
        }
        EE_UNITS_UCA_CRUISE => set_f(f, world_field!(x.cls()?.fields(), mCruiseAltitude)),
        EE_UNITS_UCA_CLOSEST_TILE => host(x, op, TILE.with(|t| t.as_ptr() as usize as u64), b, c, f),
        EE_UNITS_UCA_SET_COMMANDABLE => {
            let p = rec(x)?;
            // SAFETY: the unit's member
            unsafe { (*p).mIsCommandable = b != 0 };
            0
        }
        EE_UNITS_UCA_SET_HEIGHT => {
            let p = rec(x)?;
            // SAFETY: the unit's member
            unsafe { (*p).mHeight = f[0] };
            0
        }
        EE_UNITS_UCA_SET_STATS_FLIGHT_TIME => {
            let time: c_long = world_field!(u()?.stats()?.fields(), mFlightTime);
            let p = rec(x)?;
            // SAFETY: the unit's member
            unsafe { (*p).mFlightTime = time };
            0
        }
        EE_UNITS_UCA_SET_AMMUNITION => {
            let ammunition: c_long = world_field!(x.cls()?.fields(), mAmmunition);
            let p = rec(x)?;
            // SAFETY: the unit's member
            unsafe { (*p).mAmmunition = ammunition as c_ulong };
            0
        }
        EE_UNITS_UCA_RANGE => set_f(f, u()?.stats()?.range()),
        EE_UNITS_UCA_LAUNCH_SPEED => set_f(f, world_field!(u()?.stats()?.fields(), mLaunchSpeed)),
        EE_UNITS_UCA_AREA_EFFECT => set_f(f, world_field!(u()?.stats()?.fields(), mAreaEffect)),
        EE_UNITS_UCA_SET_FLIGHT_TIME_OF => {
            let o = x.o()?;
            // SAFETY: the unit a's member
            unsafe { (*complex_mut(&o)).mFlightTime = b as c_long };
            0
        }
        EE_UNITS_UCA_TILE_HEIGHT => set_f(f, u()?.tile()?.height()),
        EE_UNITS_UCA_FIGHTER_USABLE | EE_UNITS_UCA_FIGHTER_CLASS => host(x, op, x.a, b, FIGHTERS[c as usize], f),
        _ => return None,
    })
}
