//! `extern "C"` for the framed background (`frame.rs`): `EEUIBGenericBackground::DrawBackground` calls it; its fill
//! and pieces are `backgrounds_calls.rs`' (over `User Interface Thread/EEUIBGenericBackground.cpp`'s shims).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};

use ee_base::math::types::GeRectangle;

use crate::screens::frame::{EeScreensFramePiece, Frame, FrameHost};

/// The background (`EEUIBGenericBackground*`).
struct Background(*mut c_void);

impl FrameHost for Background {
    fn fill(&mut self, inner: [f32; 4]) -> c_long {
        crate::screens::backgrounds_calls::frame_fill(self.0 as usize, inner)
    }
    fn piece(&mut self, piece: &EeScreensFramePiece) {
        crate::screens::backgrounds_calls::frame_piece(self.0 as usize, piece)
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
