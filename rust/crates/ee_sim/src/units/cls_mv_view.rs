//! The class bodies' plain statements of `EEUnitClassMoveOp` (EEUnitClass.cpp; the units' lane, round 13): each
//! `EE_UNITS_CLS_MV_*`, `_CR_*`, `_LV_*`, `_ST_*` and `_R_*` statement that reads or stores one member of a Rust
//! record or layout entry (the unit's, its goal's, its player's air traffic, its tile's, the world's), or calls a
//! Rust function, answered over the world views before [`ClsGame::call`](super::cls_host::ClsGame) asks the C++
//! host; their cases left the C++ switch. `a` is the unit, `b` an object (a goal, a unit, a tile).

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cls_carrier_host::*;
use crate::units::cls_host::*;
use crate::units::cls_level_host::*;
use crate::units::cls_move_host::*;
use crate::units::cls_status_host::*;
use crate::units::uc_view::air_traffic;
use crate::world::layout::layout;
use crate::world::unit_fields::EeUnitsComplexUnitFields;
use crate::world::view::{UnitView, WorldRead, WorldView};
use crate::world_field;
use std::os::raw::c_long;

/// `kResourceWood`
const RESOURCE_WOOD: u64 = 3;

/// The statement `op` when it is a plain one; None for the C++ host's.
pub(crate) fn cls_mv_view(op: i32, a: u64, b: u64, io: &mut EeUnitsClsIo) -> Option<i64> {
    // SAFETY: the class bodies run inside the C++ caller's hold of the world; their handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above (`a` a complex unit)
    let u = || unsafe { UnitView::of(&t, a) };
    // SAFETY: as above (`b` a unit)
    let o = || unsafe { UnitView::of(&t, b) };
    Some(match op {
        EE_UNITS_CLS_MV_GOAL | EE_UNITS_CLS_CR_GOAL => u()?.goal_handle() as i64,
        EE_UNITS_CLS_MV_ACTIVE_GOAL => u()?.active_goal_handle() as i64,
        EE_UNITS_CLS_MV_BLOCKING_UNIT => world_field!(u()?.complex(), mBlockingUnit) as usize as i64,
        EE_UNITS_CLS_MV_LAST_ATTACKER => u()?.last_attacker_handle() as i64,
        EE_UNITS_CLS_MV_CLEAR_LAST_ATTACKER => {
            let p = u()?.complex().ptr() as *mut EeUnitsComplexUnitFields;
            // SAFETY: the unit's member
            unsafe { (*p).mLastAttacker = std::ptr::null_mut() };
            0
        }
        EE_UNITS_CLS_MV_GOAL_KIND => {
            if b == 0 {
                return None;
            }
            // SAFETY: a live goal's kind byte
            i64::from(unsafe { std::ptr::read((b as usize + layout().goal_kind as usize) as *const u8) })
        }
        EE_UNITS_CLS_MV_RESOURCE_IS_WOOD => i64::from(o()?.resource_type() == RESOURCE_WOOD),
        EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC => i64::from(air_traffic(&u()?)? != 0),
        EE_UNITS_CLS_R_ATC => air_traffic(&u()?)? as i64,
        EE_UNITS_CLS_CR_ATC_SLOTS => {
            let atc = air_traffic(&u()?)?;
            // SAFETY: the player's live air traffic state
            unsafe { crate::units::atc::ee_units_atc_slot_count(atc as usize as *const _) as u64 as i64 }
        }
        EE_UNITS_CLS_ST_CINEMATIC_INTERVAL => i64::from(WorldView::global(&t)?.in_cinematic()),
        EE_UNITS_CLS_ST_CLOAKED_ENEMY => {
            let w = WorldView::global(&t)?;
            let current = w.player(w.current_player_index() as usize).map_or(0, |p| p.handle());
            i64::from(crate::units::uu_ffi::ee_units_uu_cloaked_enemy(a, current))
        }
        EE_UNITS_CLS_R_HIT_POINTS_OF => o()?.hit_points(),
        EE_UNITS_CLS_R_COMPLEX_OF => o()?.complex_unit_handle() as i64,
        EE_UNITS_CLS_R_LOCATION_OF => {
            io.p = o()?.location();
            0
        }
        EE_UNITS_CLS_R_TILE_CENTER_OF => {
            let o = o()?;
            let (x, y) = o.tile_xy();
            io.p = [x as c_long as f32 + 0.5, y as c_long as f32 + 0.5, o.tile()?.height()];
            0
        }
        EE_UNITS_CLS_LV_SET_TILE_HEIGHT => {
            if a == 0 {
                return None;
            }
            // SAFETY: the map tile `a`'s height member
            unsafe { std::ptr::write((a as usize + layout().tile_height as usize) as *mut f32, io.f) };
            0
        }
        _ => return None,
    })
}
