//! `extern "C"` for `UIIMECandidate` (`User Interface/UIIMECandidate.cpp`): `SetCandidateList`'s string walk and lines
//! go back to the C++ (`ee_uicpp_ime_*`); the rest takes and returns values.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::ime::{self, CandidateFit, CandidateFrame, CandidateHost};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_ulong, c_void};

    extern "C" {
        pub fn ee_uicpp_ime_skip(list: *mut c_void);
        pub fn ee_uicpp_ime_line(list: *mut c_void, number: *const u8, length: c_ulong) -> f32;
    }
}

/// `SetCandidateList`'s walk (the C++ `UIIMECandidateRust::List`).
struct Raw(*mut c_void);

impl CandidateHost for Raw {
    fn skip(&mut self) {
        unsafe { sys::ee_uicpp_ime_skip(self.0) }
    }
    fn line(&mut self, number: &[u8]) -> f32 {
        unsafe { sys::ee_uicpp_ime_line(self.0, number.as_ptr(), number.len() as c_ulong) }
    }
}

/// `OpenCandidateWindow`'s rectangle.
#[no_mangle]
pub extern "C" fn ee_ui_ime_open_rect(screen: UiRect, control: UiRect, width_ratio: f32, height_ratio: f32) -> UiRect {
    ime::open_rect(screen, control, width_ratio, height_ratio)
}

/// `SetCandidateList`'s page, lines and width.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_ime_candidate_list(
    list: *mut c_void,
    height: f32,
    font_height: c_long,
    selection: c_ulong,
    count: c_ulong,
    window_width: c_ulong,
    left: f32,
) -> CandidateFit {
    ime::candidate_list(&mut Raw(list), height, font_height, selection, count, window_width, left)
}

/// `DrawPolygons`' selection bar and border.
#[no_mangle]
pub extern "C" fn ee_ui_ime_frame(rect: UiRect, selection: c_long, height: c_ulong) -> CandidateFrame {
    ime::frame(rect, selection, height)
}

/// `DrawTextA`'s row of line `index`.
#[no_mangle]
pub extern "C" fn ee_ui_ime_row(rect: UiRect, index: c_ulong, height: c_ulong) -> UiRect {
    ime::row(rect, index, height)
}
