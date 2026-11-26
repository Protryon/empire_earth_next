//! The placement ghosts' host's scratch values, kept here (the units' lane, round 14): EEGhostUnit.cpp's file
//! statics that one statement filled and the next read back (the fill square's and the preview's points, the picked
//! tile and its x and y, the wall piece's flag, the load's and save's file and value) are this thread's cells. The
//! statements that only stored or read them are answered here; the ones that still call C++ go to
//! `ee_units_ghost_host` with the cell's address in an operand the original left unused. ghost_body.rs's `Game`
//! asks [`ghost_scratch_view`] before [`ghost_view`](super::ghost_view::ghost_view).

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::cell::Cell;
use std::os::raw::{c_long, c_ulong, c_void};

use crate::units::ghost_body::game::ee_units_ghost_host;
use crate::units::ghost_body::*;
use crate::units::ghost_make::*;
use crate::units::ghost_more::*;

thread_local! {
    /// the fill square's position (`GE3DPoint(theGhost->mTransformation)`)
    static FILL: Cell<[f32; 3]> = const { Cell::new([0.0; 3]) };
    /// the preview's position
    static POSITION: Cell<[f32; 3]> = const { Cell::new([0.0; 3]) };
    /// the picked tile (an `EETile*`, not read) and the picked x and y
    static TILE: Cell<usize> = const { Cell::new(0) };
    static XY: Cell<[c_long; 2]> = const { Cell::new([0; 2]) };
    /// `EEGetWallPiecePart`'s flag
    static PIECE: Cell<bool> = const { Cell::new(false) };
    /// the load's and save's file (an `FSFile*`) and the value read
    static FILE: Cell<u64> = const { Cell::new(0) };
    static VALUE: Cell<c_ulong> = const { Cell::new(0) };
}

fn host(op: i32, ghost: u64, a: u64, b: i64, c: i64) -> i64 {
    // SAFETY: the C++ host's statement on the live ghost, with the cells' addresses
    unsafe { ee_units_ghost_host(op, ghost as usize as *mut c_void, a, b, c) }
}

fn addr<T>(cell: &'static std::thread::LocalKey<Cell<T>>) -> u64 {
    cell.with(|c| c.as_ptr() as usize as u64)
}

/// The statement `op` when it is one of the scratch values'.
pub(crate) fn ghost_scratch_view(op: i32, ghost: u64, a: u64, b: i64, c: i64) -> Option<i64> {
    Some(match op {
        EE_UNITS_GHOST_POSITION_FROM_TRANSFORM | EE_UNITS_GHOST_CAN_PLACE_AT => host(op, ghost, addr(&FILL), b, c),
        EE_UNITS_GHOST_POSITION_RESET => {
            POSITION.set([0.0; 3]);
            0
        }
        EE_UNITS_GHOST_POINT_UNIT_OR_TILE => host(op, ghost, a, addr(&TILE) as i64, addr(&POSITION) as i64),
        EE_UNITS_GHOST_POSITION => i64::from(POSITION.get()[b as usize].to_bits()),
        EE_UNITS_GHOST_SET_POSITION_PART => {
            let mut p = POSITION.get();
            p[b as usize] = f32::from_bits(c as u32);
            POSITION.set(p);
            0
        }
        EE_UNITS_GHOST_SET_POSITION | EE_UNITS_GHOST_LOCATION_TO_TILE => host(op, ghost, addr(&POSITION), b, c),
        EE_UNITS_GHOST_POINT_TILE_XY => host(op, ghost, a, addr(&TILE) as i64, addr(&XY) as i64),
        EE_UNITS_GHOST_SCRATCH_XY => XY.get()[b as usize] as i64,
        EE_UNITS_GHOST_WALL_PIECE_PART => host(op, ghost, addr(&PIECE), b, c),
        EE_UNITS_GHOST_PIECE_FLAG => i64::from(PIECE.get()),
        EE_UNITS_GHOST_M_SET_FILE => {
            FILE.set(a);
            0
        }
        EE_UNITS_GHOST_M_READ_ULONG => host(op, ghost, FILE.get(), addr(&VALUE) as i64, c),
        EE_UNITS_GHOST_M_VALUE => VALUE.get() as i64,
        EE_UNITS_GHOST_M_WRITE_ULONG | EE_UNITS_GHOST_M_WRITE_PLAYER_ID => host(op, ghost, FILE.get(), b, c),
        _ => return None,
    })
}
