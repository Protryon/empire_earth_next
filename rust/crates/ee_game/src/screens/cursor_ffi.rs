//! `extern "C"` for the main game form's cursor (`cursor.rs`). The host is `ee_screenscpp_cursor`
//! (`User Interface Thread/EEUIFMainGame.cpp`): `ctx` is its read state for one `UpdateCursorKind`.

use core::ffi::c_void;

use crate::screens::cursor::{cursor_kind, CursorHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_cur_local_player() -> *mut c_void;
        pub fn ee_screenscpp_cur_selected_group(player: *mut c_void) -> *mut c_void;
        pub fn ee_screenscpp_cur_group_first(group: *mut c_void) -> *mut c_void;
        pub fn ee_screenscpp_cur_set_pointer(kind: i32);
    }
}

struct Host(*mut c_void);

impl CursorHost for Host {
    fn op(&mut self, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> i32 {
        // SAFETY: the host's live read state (its layout asserted in EEUIFMainGame.cpp)
        if let Some(r) = unsafe { crate::screens::hud_direct::cursor(self.0.cast(), op, a, b, f) } {
            return r;
        }
        crate::screens::cursor_calls::call(&mut crate::screens::game_cpp::Ffi, self.0 as usize, op, a, b, f)
            .unwrap_or(0)
    }
}

/// `EEUIFMainGame::UpdateCursorKind`'s cursor kind, read through the host with `ctx`.
#[no_mangle]
pub extern "C" fn ee_screens_cursor_kind(ctx: *mut c_void) -> i32 {
    cursor_kind(&mut Host(ctx))
}

/// `EEUIFMainGame::UpdateCursorKind` (0x266f8b): picks the mouse cursor for the unit under it from what the local
/// player's selection can do to it (`cursor_kind` over one read state of `form`).
///
/// # Safety
/// `form` is the live main game form; the world has its local player.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_update_cursor_kind(form: *mut c_void) {
    // SAFETY: the contract
    unsafe {
        let player = sys::ee_screenscpp_cur_local_player();
        let group = sys::ee_screenscpp_cur_selected_group(player);
        let mut read = crate::screens::hud_direct::EeScreensCursorRead::new(
            form,
            player,
            group,
            sys::ee_screenscpp_cur_group_first(group),
        );
        sys::ee_screenscpp_cur_set_pointer(ee_screens_cursor_kind(
            (&mut read as *mut crate::screens::hud_direct::EeScreensCursorRead).cast(),
        ));
    }
}
