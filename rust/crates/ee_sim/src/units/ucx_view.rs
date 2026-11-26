//! The transports' host's statements answered over the world views (the units' lane, round 14): EEUnitClassTransports.cpp's
//! stores of plain members, its boarding tiles (this thread's cell, filled and searched by the unit utilities'
//! Rust) and its map tile statements, which are the ground kinds' (`EE_UNITS_UCG_TILE_ADD`/`_REMOVE`, the same
//! C++). Asked by [`kind_view`](super::uc_kind_view::kind_view) for the transports' kind after its plain reads.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::cell::RefCell;
use std::os::raw::{c_long, c_ulong, c_void};

use crate::units::uc_body::game::ee_units_uc_host;
use crate::units::uc_view::Ctx;
use crate::units::ucg_ground::{EE_UNITS_UCG_TILE_ADD, EE_UNITS_UCG_TILE_REMOVE};
use crate::units::ucx_transports::*;
use crate::units::uu_tiles::Tile;
use crate::world::unit_fields::EeUnitsUnitFields;
use crate::world_field;

thread_local! {
    /// the boarding location's tiles (`EEGetTilesAroundUnit`'s)
    static TILES: RefCell<Vec<Tile>> = const { RefCell::new(Vec::new()) };
}

fn host(x: &Ctx<'_>, op: i32, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
    let p = |h: u64| h as usize as *mut c_void;
    // SAFETY: the C++ host's statement with the live class, unit and objects the body handed over
    unsafe { ee_units_uc_host(op, p(x.class), p(x.unit), a, b, c, f) }
}

/// The transports' statement `op` when it is one of these.
pub(crate) fn transports_view(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    Some(match op {
        EE_UNITS_UCX_TILES_CLEAR => {
            TILES.with(|t| t.borrow_mut().clear());
            0
        }
        // `EEGetTilesAroundUnit(theUnit, a, 1, 0, false)` into the tiles
        EE_UNITS_UCX_TILES_AROUND => {
            let tiles = crate::units::uu_ffi::tiles_around(x.unit, x.a, 1, 0, false);
            TILES.with(|t| *t.borrow_mut() = tiles);
            0
        }
        // `EEPickTileNear(the tiles, theUnit, a, false, b)` (b a location's three floats)
        EE_UNITS_UCX_PICK_TILE_NEAR => {
            // SAFETY: the body's location buffer
            let io = unsafe { &mut *(b as usize as *mut [f32; 3]) };
            let tiles = TILES.with(|t| t.borrow().clone());
            i64::from(crate::units::uu_ffi::pick_tile_near(&tiles, x.unit, x.a, false, io))
        }
        EE_UNITS_UCX_SET_POINT => {
            // SAFETY: the body's point buffer (three floats)
            unsafe { std::ptr::write(b as usize as *mut [f32; 3], [f[0], f[1], f[2]]) };
            0
        }
        // `a->SetResourceType(theClass->mResourceType)`, `a->SetResourceAmount(theClass->mResourceAmount)`
        EE_UNITS_UCX_SET_RESOURCE_TYPE | EE_UNITS_UCX_SET_RESOURCE_AMOUNT => {
            let class = x.cls()?.fields();
            let (ty, amount): (c_long, c_long) = (world_field!(class, mResourceType), world_field!(class, mResourceAmount));
            let to = x.o()?.fields().ptr() as *mut EeUnitsUnitFields;
            // SAFETY: the unit's members
            unsafe {
                if op == EE_UNITS_UCX_SET_RESOURCE_TYPE {
                    (*to).mResourceType = ty as c_ulong;
                } else {
                    (*to).mResourceAmount = amount;
                }
            }
            0
        }
        EE_UNITS_UCX_COMPLEX_FIELD_OF => x.o()?.complex_unit_handle() as i64,
        EE_UNITS_UCX_CARGO_OF => x.o()?.cargo_handle(b as c_ulong as usize) as i64,
        EE_UNITS_UCX_TILE_ADD => host(x, EE_UNITS_UCG_TILE_ADD, x.a, b, c, f),
        EE_UNITS_UCX_TILE_REMOVE => host(x, EE_UNITS_UCG_TILE_REMOVE, x.a, b, c, f),
        _ => return None,
    })
}
