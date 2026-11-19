//! `extern "C"` for the main game form's cursor (`cursor.rs`). The host is `ee_screenscpp_cursor`
//! (`User Interface Thread/EEUIFMainGame.cpp`): `ctx` is its read state for one `UpdateCursorKind`.

use core::ffi::c_void;

use crate::screens::cursor::{cursor_kind, CursorHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_cursor(ctx: *mut c_void, op: u32, a: u32, b: u32, f: *mut f32) -> i32;
    }
}

struct Host(*mut c_void);

impl CursorHost for Host {
    fn op(&mut self, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> i32 {
        // SAFETY: the host's live read state (its layout asserted in EEUIFMainGame.cpp)
        if let Some(r) = unsafe { crate::screens::hud_direct::cursor(self.0.cast(), op, a, b, f) } {
            return r;
        }
        unsafe { sys::ee_screenscpp_cursor(self.0, op, a, b, f.as_mut_ptr()) }
    }
}

/// `EEUIFMainGame::UpdateCursorKind`'s cursor kind, read through the host with `ctx`.
#[no_mangle]
pub extern "C" fn ee_screens_cursor_kind(ctx: *mut c_void) -> i32 {
    cursor_kind(&mut Host(ctx))
}
