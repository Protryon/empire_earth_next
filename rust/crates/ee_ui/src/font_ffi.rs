//! `extern "C"` for the UI font table (`User Interface/UIFontTable.cpp`): the shared fonts stay the C++ `sFont`
//! list, asked through `ee_uicpp_font_*` with the query (`query`: the C++ face name, style, quality and size) on each
//! step.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::font::{self, DbFont, FontEntry, FontHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_uicpp_font_count() -> usize;
        pub fn ee_uicpp_font_matches(query: *const c_void, i: usize) -> bool;
        pub fn ee_uicpp_font_take(i: usize);
        pub fn ee_uicpp_font_create(query: *const c_void) -> usize;
    }
}

struct Fonts(*const c_void);

impl FontHost for Fonts {
    fn count(&self) -> usize {
        unsafe { sys::ee_uicpp_font_count() }
    }
    fn matches(&mut self, i: usize) -> bool {
        unsafe { sys::ee_uicpp_font_matches(self.0, i) }
    }
    fn take(&mut self, i: usize) -> usize {
        unsafe { sys::ee_uicpp_font_take(i) };
        i
    }
    fn create(&mut self) -> usize {
        unsafe { sys::ee_uicpp_font_create(self.0) }
    }
}

/// `UIFontTable::Create`'s reading of one font record.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_font_entry(record: *const DbFont) -> FontEntry {
    font::entry(&*record)
}

/// `UIFontTable::CreateClosestFont`'s point size for a pixel height.
#[no_mangle]
pub extern "C" fn ee_ui_font_closest_size(pixel_height: c_ulong) -> c_long {
    font::closest_size(pixel_height)
}

/// `UIFontTable::FindOrCreateFont` -> the index in `sFont` of the font handed out (referenced).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_font_find_or_create(query: *const c_void) -> usize {
    font::find_or_create(&mut Fonts(query))
}
