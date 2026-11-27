//! The placement ghosts' host's plain statements (the units' lane, round 13, second leg): each `EE_UNITS_GHOST_*`
//! statement of EEGhostUnit.cpp's `ee_units_ghost_host` that reads or stores one member of the ghost's record
//! (`EeUnitsGhostFields`, the first base of `EEGhostUnit` after its vtable pointer: EEGhostUnit.cpp checks the
//! offset at start-up), of a unit's, a class's or a tile node's, answered here before the C++ host is asked
//! (ghost_body.rs `Game`); their cases left the C++. `ghost` the ghost, `a` an object, `b` and `c` values.
#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::ghost_body::*;
use crate::units::ghost_make::*;
use crate::units::ghost_more::*;
use crate::units::obj_fields::EeUnitsGhostFields;
use crate::units::uc_view::word;
use crate::world::layout::layout;
use crate::world::view::{ClassView, UnitView, WorldRead, WorldView};
use crate::world_field;
use std::os::raw::c_long;

/// The ghost's record (after its vtable pointer).
fn fields(ghost: u64) -> Option<*mut EeUnitsGhostFields> {
    (ghost != 0).then(|| (ghost as usize + std::mem::size_of::<usize>()) as *mut EeUnitsGhostFields)
}

/// The ghost host's statement `op` when it is a plain one (None for the C++ host's).
pub(crate) fn ghost_view(op: i32, ghost: u64, a: u64, b: i64, c: i64) -> Option<i64> {
    // SAFETY: the ghosts are the world's, under its ghost lock or the caller's own; the handles live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let o = || unsafe { UnitView::of(&t, a) };
    // SAFETY: as above (a class)
    let oc = || unsafe { ClassView::of(&t, a) };
    let g = || fields(ghost);
    // SAFETY: (every read and store below) the live ghost's record
    let gc = || unsafe { ClassView::of(&t, (*g()?).mUnitClass as usize as u64) };
    Some(match op {
        EE_UNITS_GHOST_UNIT_TYPE => gc()?.unit_type() as c_long as i64,
        EE_UNITS_GHOST_CATEGORY => gc()?.category() as i64,
        EE_UNITS_GHOST_TILE_SIZE => gc()?.tile_size(),
        EE_UNITS_GHOST_IS_COMPLEX => world_field!(gc()?.fields(), mIsComplex) as i64,
        // SAFETY: as above
        EE_UNITS_GHOST_TILE_X => unsafe { (*g()?).mTileX as i64 },
        // SAFETY: as above
        EE_UNITS_GHOST_TILE_Y => unsafe { (*g()?).mTileY as i64 },
        EE_UNITS_GHOST_SET_TILE_X => {
            // SAFETY: as above
            unsafe { (*g()?).mTileX = b as c_long };
            0
        }
        EE_UNITS_GHOST_SET_TILE_Y => {
            // SAFETY: as above
            unsafe { (*g()?).mTileY = b as c_long };
            0
        }
        EE_UNITS_GHOST_SET_HEIGHT => {
            // SAFETY: as above (b below 144)
            unsafe { (*g()?).mHeights[b as usize] = f32::from_bits(c as u32) };
            0
        }
        EE_UNITS_GHOST_SET_FOOTPRINT => {
            // SAFETY: as above (b, c below 12)
            unsafe { (*g()?).mFootprint[b as usize][c as usize] = a != 0 };
            0
        }
        EE_UNITS_GHOST_SET_CAN_PLACE => {
            // SAFETY: as above
            unsafe { (*g()?).mCanPlace = b != 0 };
            0
        }
        // SAFETY: as above (the bool's byte)
        EE_UNITS_GHOST_CAN_PLACE => {
            i64::from(unsafe { std::ptr::read(std::ptr::addr_of!((*g()?).mCanPlace).cast::<u8>()) } != 0)
        }
        EE_UNITS_GHOST_FOOTPRINT_FROM_CAN_PLACE => {
            let r = g()?;
            // SAFETY: as above
            unsafe { (*r).mFootprint[0][0] = std::ptr::read(std::ptr::addr_of!((*r).mCanPlace).cast::<u8>()) != 0 };
            0
        }
        EE_UNITS_GHOST_M_SET_PLAYER => {
            // SAFETY: as above
            unsafe { (*g()?).mPlayer = b as usize as *mut _ };
            0
        }
        EE_UNITS_GHOST_M_SET_CLASS => {
            // SAFETY: as above
            unsafe { (*g()?).mUnitClass = b as usize as *mut _ };
            0
        }
        EE_UNITS_GHOST_M_SET_NO_MEDIA => {
            // SAFETY: as above
            unsafe { (*g()?).mNoMedia = b != 0 };
            0
        }
        // SAFETY: as above (the bool's byte)
        EE_UNITS_GHOST_M_NO_MEDIA => {
            i64::from(unsafe { std::ptr::read(std::ptr::addr_of!((*g()?).mNoMedia).cast::<u8>()) } != 0)
        }
        EE_UNITS_GHOST_M_SET_FRAME => {
            // SAFETY: as above
            unsafe { (*g()?).mFrame = b as c_long };
            0
        }
        EE_UNITS_GHOST_M_SET_ANIMATION_INDEX => {
            // SAFETY: as above
            unsafe { (*g()?).mAnimation = b as c_long };
            0
        }
        EE_UNITS_GHOST_M_CLEAR_FOOTPRINT => {
            // SAFETY: as above
            unsafe { (*g()?).mFootprint = [[false; 12]; 12] };
            0
        }
        EE_UNITS_GHOST_M_CLEAR_HEIGHTS => {
            // SAFETY: as above
            unsafe { (*g()?).mHeights = [0.0; 144] };
            0
        }
        EE_UNITS_GHOST_SET_OVERLAP_SET => {
            let set = world_field!(oc()?.fields(), mOverlapUnitSet);
            // SAFETY: as above
            unsafe { (*g()?).mUnitSet = set };
            0
        }
        // the units', classes' and tile nodes'
        EE_UNITS_GHOST_UNIT_IS_WALL => i64::from(o()?.class()?.wall()),
        EE_UNITS_GHOST_UNIT_TILE => {
            let (x, y) = o()?.tile_xy();
            if b == 0 {
                x
            } else {
                y
            }
        }
        // SAFETY: a live tile node's members
        EE_UNITS_GHOST_NODE_UNIT => unsafe { word(a, layout().unit_node_unit) as i64 },
        // SAFETY: as above
        EE_UNITS_GHOST_NODE_NEXT => unsafe { word(a, layout().unit_node_next) as i64 },
        EE_UNITS_GHOST_UNIT_TINTED => {
            let c = o()?.complex_unit();
            i64::from(c.is_some_and(|c| world_field!(c.complex(), mIsTinted)))
        }
        EE_UNITS_GHOST_UNIT_GATE_DIRECTION => world_field!(o()?.complex_unit()?.complex(), mGateDirection) as i64,
        EE_UNITS_GHOST_CLASS_AT => ClassView::by_index(&t, b as usize).map_or(0, |k| k.handle() as i64),
        EE_UNITS_GHOST_CLASS_FIXED => i64::from(world_field!(oc()?.fields(), mMaxAngularVelocity) == 0.0),
        EE_UNITS_GHOST_CLASS_TYPE => oc()?.unit_type() as c_long as i64,
        EE_UNITS_GHOST_CLASS_TILE_SIZE => oc()?.tile_size(),
        EE_UNITS_GHOST_CLASS_CATEGORY => oc()?.category() as i64,
        EE_UNITS_GHOST_TILE_EXISTS => {
            let map = WorldView::global(&t)?.map()?;
            i64::from(map.tile(b as c_long as i64, c as c_long as i64).is_some())
        }
        EE_UNITS_GHOST_TILE_FIRST_NODE => {
            let map = WorldView::global(&t)?.map()?;
            // SAFETY: a live tile's node list
            map.tile(b as c_long as i64, c as c_long as i64)
                .map_or(0, |k| unsafe { word(k.handle(), layout().tile_units) } as i64)
        }
        _ => return None,
    })
}
