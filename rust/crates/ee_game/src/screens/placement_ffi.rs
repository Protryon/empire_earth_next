//! The wall and gate placements' start and end (`EEUIBuildWall::Initialize` 0x256456, `Terminate` 0x2564a2;
//! `EEUIBuildGate::ClearGhostUnits` 0x2561c9, `Initialize` 0x2561ff, `Terminate` 0x256245) over their records
//! (`EeScreensBuildWallFields`, `EeScreensBuildGateFields`: the members from `mState` / `mUnitClass` on) and the
//! world's one-statement shims (EEUIBuildWall.cpp, EEUIBuildGate.cpp).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::screens::hud_ctl::EeScreensPoint;
use crate::screens::mouse_modes::{EeScreensBuildGateFields, EeScreensBuildWallFields};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_pl_wall_load_media(player: i32, technology: i32);
        pub fn ee_screenscpp_pl_wall_unload_media(player: i32, technology: i32);
        pub fn ee_screenscpp_pl_unlock_cursor();
        pub fn ee_screenscpp_pl_gate_highlight(player: i32, wall_type: i32);
        pub fn ee_screenscpp_pl_gate_unhighlight(player: i32, wall_type: i32);
        pub fn ee_screenscpp_pl_ghosts_enter();
        pub fn ee_screenscpp_pl_ghosts_leave();
        pub fn ee_screenscpp_pl_ghosts_clear();
        pub fn ee_screenscpp_pl_gate_load_media(player: i32, unit_class: *mut c_void);
        pub fn ee_screenscpp_pl_gate_unload_media(player: i32, unit_class: *mut c_void);
    }
}

/// No point yet (-99, -99).
const NO_POINT: EeScreensPoint = EeScreensPoint { x: -99.0, y: -99.0 };

/// `EEUIBuildWall::Initialize`: starts wall placement for a player: no points yet, the wall class's media loaded.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_wall_initialize(wall: *mut EeScreensBuildWallFields, player: i32) {
    // SAFETY: the placement's live record
    unsafe {
        let w = &mut *wall;
        w.start = NO_POINT;
        w.end = NO_POINT;
        w.state = 0;
        sys::ee_screenscpp_pl_wall_load_media(player, w.technology);
        (*wall).player = player;
    }
}

/// `EEUIBuildWall::Terminate`: ends wall placement: unlocks the cursor and unloads the wall class's media.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_wall_terminate(wall: *mut EeScreensBuildWallFields) {
    // SAFETY: the placement's live record
    unsafe {
        sys::ee_screenscpp_pl_unlock_cursor();
        sys::ee_screenscpp_pl_wall_unload_media((*wall).player, (*wall).technology);
    }
}

/// `EEUIBuildGate::ClearGhostUnits`: restores the player's wall colours and removes the gate ghost.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_gate_clear_ghosts(gate: *mut EeScreensBuildGateFields) {
    // SAFETY: the placement's live record
    unsafe {
        sys::ee_screenscpp_pl_gate_unhighlight((*gate).player, (*gate).wall_type);
        sys::ee_screenscpp_pl_ghosts_enter();
        sys::ee_screenscpp_pl_ghosts_clear();
        sys::ee_screenscpp_pl_ghosts_leave();
    }
}

/// `EEUIBuildGate::Initialize`: starts gate placement for a player: highlights the walls a gate can go on and loads
/// the gate's media.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_gate_initialize(gate: *mut EeScreensBuildGateFields, player: i32) {
    // SAFETY: the placement's live record
    unsafe {
        sys::ee_screenscpp_pl_gate_highlight(player, (*gate).wall_type);
        let g = &mut *gate;
        g.tile_x = -99;
        g.tile_y = -99;
        g.direction = 2;
        g.player = player;
        sys::ee_screenscpp_pl_gate_load_media(player, g.unit_class);
    }
}

/// `EEUIBuildGate::Terminate`: ends gate placement: removes highlights and ghosts, unlocks the cursor and unloads the
/// gate's media.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_gate_terminate(gate: *mut EeScreensBuildGateFields) {
    // SAFETY: the placement's live record
    unsafe {
        ee_screens_gate_clear_ghosts(gate);
        sys::ee_screenscpp_pl_unlock_cursor();
        sys::ee_screenscpp_pl_gate_unload_media((*gate).player, (*gate).unit_class);
    }
}
