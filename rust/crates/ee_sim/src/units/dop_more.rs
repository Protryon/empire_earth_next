//! More of the complex doppelganger's and the complex unit's load bodies over their C++ statements (the units' lane,
//! round 8; were EEComplexDoppelganger.cpp): `EEComplexDoppelganger::Reset`, `Clear` and `RepairPointers`,
//! `EEComplexUnit::FinishLoad`. Every C++ statement is one `EE_UNITS_DOPX_*` case of `ee_units_dop_host` (from
//! 450), asked in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::units::dop_repair::DopHost;

// ---- the statements (`op`; `a` an object, `b`, `c` values)
/// the doppelganger's `mScale = 0`, `mModel = NULL`, `mPackedHitPoints = 0`, `mFrame = 0`, `mAnimation = 0`,
/// `mTileX = -10000`, `mTileY = -10000`, `mIsFoundation = false`
pub const EE_UNITS_DOPX_ZERO_SCALE: i32 = 450;
pub const EE_UNITS_DOPX_CLEAR_MODEL: i32 = 451;
pub const EE_UNITS_DOPX_ZERO_HIT_POINTS: i32 = 452;
pub const EE_UNITS_DOPX_ZERO_FRAME: i32 = 453;
pub const EE_UNITS_DOPX_ZERO_ANIMATION: i32 = 454;
pub const EE_UNITS_DOPX_TILE_X_OFF: i32 = 455;
pub const EE_UNITS_DOPX_TILE_Y_OFF: i32 = 456;
pub const EE_UNITS_DOPX_NOT_FOUNDATION: i32 = 457;
/// `EEDoppelganger::Clear()`
pub const EE_UNITS_DOPX_BASE_CLEAR: i32 = 458;
/// the doppelganger's `mModel != NULL`; its `mModel->UnloadMedia()`
pub const EE_UNITS_DOPX_HAS_MODEL: i32 = 459;
pub const EE_UNITS_DOPX_UNLOAD_MODEL: i32 = 460;
/// `USavedID(mModel)`
pub const EE_UNITS_DOPX_MODEL_SAVED_ID: i32 = 461;
/// `EEDoppelganger::RepairPointers(b, c)`
pub const EE_UNITS_DOPX_BASE_REPAIR: i32 = 462;
/// `(EEModel*)EEMedia::Get(b)`
pub const EE_UNITS_DOPX_MEDIA: i32 = 463;
/// model `a`'s `LoadMedia()`
pub const EE_UNITS_DOPX_LOAD_MEDIA: i32 = 464;
/// `mModel = ` model `a`
pub const EE_UNITS_DOPX_SET_MODEL: i32 = 465;
/// `mFrame = min(mFrame, ` model `a`'s frame count `- 1)`
pub const EE_UNITS_DOPX_CLAMP_FRAME: i32 = 466;
/// `gDoppelgangerManager.AddToTiles(this, mTileX, mTileY, ` an airport's `)`
pub const EE_UNITS_DOPX_ADD_TO_TILES: i32 = 467;
/// `gDoppelgangerManager.Free(this)`
pub const EE_UNITS_DOPX_FREE: i32 = 468;
/// `ee_units_unit_finish_fractions(unit, ` its maxima (`mStats->mMaxHitPoints`, `mStats->mFlightTime`, its class's
/// `mGarrisonCapacity`, `mStats->mPower`) `)`
pub const EE_UNITS_DOPX_FINISH_FRACTIONS: i32 = 469;
/// `mIsFoundation = ` an unbuilt land building (not a farm) at 1 hit point; the result
pub const EE_UNITS_DOPX_OLD_FOUNDATION: i32 = 473;
/// `GetPlayer()->UnloadUnitClassMedia(` its type `)`, `gDataPreLoading->PostLoadUnitClass(` the player, its type `)`
pub const EE_UNITS_DOPX_UNLOAD_CLASS_MEDIA: i32 = 474;
pub const EE_UNITS_DOPX_POST_LOAD_CLASS: i32 = 475;
/// `EEUnit::FinishLoad(b, c)`
pub const EE_UNITS_DOPX_BASE_FINISH_LOAD: i32 = 476;

// ---- the bodies (`ee_units_dop_more`'s `what`)
pub const EE_UNITS_DOPX_DO_RESET: i32 = 1;
pub const EE_UNITS_DOPX_DO_CLEAR: i32 = 2;
pub const EE_UNITS_DOPX_DO_REPAIR_POINTERS: i32 = 3;
pub const EE_UNITS_DOPX_DO_FINISH_LOAD: i32 = 4;

/// `kErrorOK`
const ERROR_OK: i64 = 0;

fn q<H: DopHost + ?Sized>(h: &mut H, op: i32, unit: u64) -> i64 {
    h.dop(op, unit, 0, 0, 0)
}

/// `EEComplexDoppelganger::Reset()`: no model, hit points, frame, animation or scale; off the map.
pub fn reset<H: DopHost + ?Sized>(h: &mut H, dop: u64) {
    for op in EE_UNITS_DOPX_ZERO_SCALE..=EE_UNITS_DOPX_NOT_FOUNDATION {
        q(h, op, dop);
    }
}

/// `EEComplexDoppelganger::Clear()`: the unit and the model's media released, then reset.
pub fn clear<H: DopHost + ?Sized>(h: &mut H, dop: u64) {
    q(h, EE_UNITS_DOPX_BASE_CLEAR, dop);
    if q(h, EE_UNITS_DOPX_HAS_MODEL, dop) != 0 {
        q(h, EE_UNITS_DOPX_UNLOAD_MODEL, dop);
    }
    reset(h, dop);
}

/// `EEComplexDoppelganger::RepairPointers(first, second)`: the loaded model index back into its media (the frame
/// clamped) and the doppelganger onto its tile; freed when the unit or model is gone. Returns 0.
pub fn repair_pointers<H: DopHost + ?Sized>(h: &mut H, dop: u64, first: i64, second: i64) -> i64 {
    let model = q(h, EE_UNITS_DOPX_MODEL_SAVED_ID, dop);
    q(h, EE_UNITS_DOPX_CLEAR_MODEL, dop);
    if h.dop(EE_UNITS_DOPX_BASE_REPAIR, dop, 0, first, second) == ERROR_OK {
        let media = h.dop(EE_UNITS_DOPX_MEDIA, dop, 0, model, 0) as u64;
        if media != 0 && h.dop(EE_UNITS_DOPX_LOAD_MEDIA, dop, media, 0, 0) == ERROR_OK {
            h.dop(EE_UNITS_DOPX_SET_MODEL, dop, media, 0, 0);
            h.dop(EE_UNITS_DOPX_CLAMP_FRAME, dop, media, 0, 0);
            q(h, EE_UNITS_DOPX_ADD_TO_TILES, dop);
            return ERROR_OK;
        }
    }
    q(h, EE_UNITS_DOPX_FREE, dop);
    ERROR_OK
}

/// `EEComplexUnit::FinishLoad(major, minor)`: the saved fractions back into amounts; an old save's (0.200 and before)
/// unbuilt land building at 1 hit point marked a foundation, its media reloaded; then `EEUnit::FinishLoad`.
pub fn finish_load<H: DopHost + ?Sized>(h: &mut H, unit: u64, major: c_long, minor: c_long) -> i64 {
    q(h, EE_UNITS_DOPX_FINISH_FRACTIONS, unit);
    if major == 0 && minor as u32 <= 200 && q(h, EE_UNITS_DOPX_OLD_FOUNDATION, unit) != 0 {
        q(h, EE_UNITS_DOPX_UNLOAD_CLASS_MEDIA, unit);
        q(h, EE_UNITS_DOPX_POST_LOAD_CLASS, unit);
    }
    h.dop(EE_UNITS_DOPX_BASE_FINISH_LOAD, unit, 0, i64::from(major), i64::from(minor))
}

/// Runs body `what` on `object`.
pub fn run<H: DopHost + ?Sized>(h: &mut H, what: i32, object: u64, b: i64, c: i64) -> i64 {
    match what {
        EE_UNITS_DOPX_DO_RESET => reset(h, object),
        EE_UNITS_DOPX_DO_CLEAR => clear(h, object),
        EE_UNITS_DOPX_DO_REPAIR_POINTERS => return repair_pointers(h, object, b, c),
        EE_UNITS_DOPX_DO_FINISH_LOAD => return finish_load(h, object, b as c_long, c as c_long),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::dop_repair::game::Game;
    use std::os::raw::c_void;

    /// The complex doppelganger's or complex unit's body `what` (`EE_UNITS_DOPX_DO_*`) on `object`.
    ///
    /// # Safety
    /// `object` is the live object the body names, on the thread that holds the world.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_dop_more(what: i32, object: *mut c_void, b: i64, c: i64) -> i64 {
        run(&mut Game, what, object as usize as u64, b, c)
    }
}

#[cfg(test)]
#[path = "dop_more_tests.rs"]
mod tests;
