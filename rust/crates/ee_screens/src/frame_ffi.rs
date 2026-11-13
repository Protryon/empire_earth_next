//! `extern "C"` for the framed background (`frame.rs`): `EEUIBGenericBackground::DrawBackground` calls it; the host
//! is `ee_screenscpp_frame_fill` and `_piece` (`User Interface Thread/EEUIBGenericBackground.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};

use ee_ge::types::GeRectangle;

use crate::frame::{EeScreensFramePiece, Frame, FrameHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_long, c_void};

    use crate::frame::EeScreensFramePiece;

    extern "C" {
        pub fn ee_screenscpp_frame_fill(background: *mut c_void, inner: *const f32) -> c_long;
        pub fn ee_screenscpp_frame_piece(background: *mut c_void, piece: *const EeScreensFramePiece);
    }
}

/// The background (`EEUIBGenericBackground*`).
struct Background(*mut c_void);

impl FrameHost for Background {
    fn fill(&mut self, inner: [f32; 4]) -> c_long {
        unsafe { sys::ee_screenscpp_frame_fill(self.0, inner.as_ptr()) }
    }
    fn piece(&mut self, piece: &EeScreensFramePiece) {
        unsafe { sys::ee_screenscpp_frame_piece(self.0, piece) }
    }
}

/// `EEUIBGenericBackground::DrawBackground`: the background (its `GERectangle`, frame scale and flags) drawn within
/// `clip` (a `GERectangle`); the panel's draw result.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_frame_draw(
    background: *mut c_void,
    rectangle: *const c_void,
    scale_x: f32,
    scale_y: f32,
    flags: u32,
    clip: *const c_void,
) -> c_long {
    // (GERectangle's layout is GeRectangle's: ee_ge types.rs)
    let rectangle = unsafe { *(rectangle as *const GeRectangle) };
    let clip = unsafe { *(clip as *const GeRectangle) };
    Frame { rectangle, scale_x, scale_y, flags }.draw(&mut Background(background), &clip)
}
