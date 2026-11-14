//! The C ABI of the text (`font.rs`): `GEText::ComputeTextExtent`, the platform's text functions as the C++ passes
//! them (a `GeTextOps` table, `GERustText.h`) and a font bitmap's glyph cell (`GeFbCharacter`, `fbobj.rs`). Every
//! input is read before the call returns and nothing calls back into C++ objects.

use std::ffi::{c_char, c_int, c_ulong, c_void};

use crate::font::{self, TextOps};
use crate::types::GeRectangle;

/// The platform's text functions and the font's measure (`PlatUtf8Decode`, `PlatMbsNInc`, `PlatCp1252ByteOf`,
/// `PlatFontMeasure`).
#[repr(C)]
pub struct GeTextOps {
    pub decode: unsafe extern "C" fn(text: *const c_char, out_length: *mut u32) -> u32,
    pub next_chars: unsafe extern "C" fn(text: *const u8, characters: usize) -> *mut u8,
    pub cp1252_byte_of: extern "C" fn(character: u32) -> c_int,
    pub measure: unsafe extern "C" fn(font: *const c_void, text: *const c_char, length: u32) -> i32,
}

impl TextOps for GeTextOps {
    fn decode(&self, text: &[u8]) -> (u32, usize) {
        let mut n = 0u32;
        // SAFETY: the slice holds its terminator (the FFI below builds it so)
        let c = unsafe { (self.decode)(text.as_ptr().cast(), &mut n) };
        (c, n as usize)
    }
    fn next_chars(&self, text: &[u8], characters: usize) -> usize {
        // SAFETY: as above; the step stops at the terminator
        unsafe { (self.next_chars)(text.as_ptr(), characters).offset_from(text.as_ptr()) as usize }
    }
    fn cp1252_byte_of(&self, character: u32) -> c_int {
        (self.cp1252_byte_of)(character)
    }
}

/// A NUL-terminated text as a slice that holds its terminator.
///
/// # Safety
/// `text` is a valid NUL-terminated string.
unsafe fn terminated<'a>(text: *const c_char) -> &'a [u8] {
    // SAFETY: the function's contract
    unsafe { std::ffi::CStr::from_ptr(text).to_bytes_with_nul() }
}

/// `GEText::ComputeTextExtent` with the font (`PlatFont*`, not NULL), its cell height and the text's character count.
///
/// # Safety
/// The pointers are valid; `text` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_extent(
    ops: *const GeTextOps,
    font: *const c_void,
    text: *const c_char,
    characters: c_ulong,
    height: i32,
    out: *mut GeRectangle,
) {
    // SAFETY: the function's contract
    unsafe {
        let ops = &*ops;
        let text = terminated(text);
        *out = font::text_extent(text, characters, height, ops, |s| {
            (ops.measure)(font, s.as_ptr().cast(), s.len() as u32)
        });
    }
}

/// `GEFBCharacter`, one glyph cell of a font bitmap (`fbobj.rs`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeFbCharacter {
    pub top_left: [f32; 2],
    pub bottom_right: [f32; 2],
    pub rect: GeRectangle,
    pub texture: *mut c_void,
    pub x_scale: f32,
    pub y_scale: f32,
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_plat_core::string::{PlatCp1252ByteOf, PlatMbsNInc, PlatUtf8Decode};

    unsafe extern "C" fn measure(_font: *const c_void, _text: *const c_char, length: u32) -> i32 {
        3 * length as i32
    }

    fn ops() -> GeTextOps {
        GeTextOps { decode: PlatUtf8Decode, next_chars: PlatMbsNInc, cp1252_byte_of: PlatCp1252ByteOf, measure }
    }

    #[test]
    fn the_extent_through_the_c_abi() {
        let mut r = GeRectangle::default();
        // SAFETY: valid pointers
        unsafe { ee_ge_text_extent(&ops(), std::ptr::null(), c"Zoë".as_ptr(), 3, 16, &mut r) };
        assert_eq!(r, GeRectangle { bottom: 16.0, left: 0.0, right: 12.0, top: 0.0 });
    }
}
