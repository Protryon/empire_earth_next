//! The C ABI of the text (`font.rs`): `GEText::ComputeTextExtent`, `GEFontBitmap`'s cell layout, alpha pass and
//! draw plan. The C++ passes the platform's text functions in a `GeTextOps` table (`GERustText.h`) and its glyph
//! cells as they are (`GeFbCharacter` mirrors `GEFBCharacter`); every input is read before the call returns and
//! nothing calls back into C++ objects.

use std::ffi::{c_char, c_int, c_ulong, c_void};

use crate::font::{self, GeFontPacker, GeGlyphDraw, TextOps};
use crate::types::{Ge2DPoint, GeRectangle};

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

/// `GEFontBitmap`'s next glyph cell of the given extent: writes the cell, returns whether it starts a new texture.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_cell_place(
    packer: *mut GeFontPacker,
    width: f32,
    height: f32,
    out: *mut GeRectangle,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let (rect, new_texture) = (*packer).place(width, height);
        *out = rect;
        new_texture
    }
}

/// `GEFontBitmap::CreateSurfaceAlpha` on a locked surface's memory.
///
/// # Safety
/// `pixels` covers `height` rows of `bytes_per_row` bytes (the last row's padding may be missing).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_surface_alpha(
    pixels: *mut u16,
    width: c_ulong,
    height: c_ulong,
    bytes_per_row: c_ulong,
) {
    let n = font::surface_alpha_len(width, height, bytes_per_row);
    if n == 0 || pixels.is_null() {
        return;
    }
    // SAFETY: the function's contract
    unsafe { font::surface_alpha(std::slice::from_raw_parts_mut(pixels, n), width, height, bytes_per_row) };
}

/// `GEFBCharacter` (the C++ keeps it; only the rectangle is read here).
#[repr(C)]
pub struct GeFbCharacter {
    pub top_left: [f32; 2],
    pub bottom_right: [f32; 2],
    pub rect: GeRectangle,
    pub texture: *mut c_void,
    pub x_scale: f32,
    pub y_scale: f32,
}

/// `GEFontBitmap::Draw`'s glyphs (`clip` NULL: unclipped) into `out` (room for `capacity`; a text has at most one
/// glyph per byte) -> the count written.
///
/// # Safety
/// The pointers are valid (`cells` for `count`, `out` for `capacity`, `clip` may be NULL); `text` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_draw_plan(
    ops: *const GeTextOps,
    text: *const c_char,
    first_character: c_ulong,
    cells: *const GeFbCharacter,
    count: usize,
    location: *const Ge2DPoint,
    clip: *const GeRectangle,
    out: *mut GeGlyphDraw,
    capacity: usize,
) -> usize {
    // SAFETY: the function's contract
    unsafe {
        let rects: Vec<GeRectangle> = if count == 0 {
            Vec::new()
        } else {
            std::slice::from_raw_parts(cells, count).iter().map(|c| c.rect).collect()
        };
        let mut plan = Vec::new();
        font::draw_plan(terminated(text), first_character, &rects, *location, clip.as_ref(), &*ops, &mut plan);
        let n = plan.len().min(capacity);
        if n > 0 {
            std::ptr::copy_nonoverlapping(plan.as_ptr(), out, n);
        }
        n
    }
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

    #[test]
    fn the_plan_through_the_c_abi_fits_its_room() {
        let cell = GeFbCharacter {
            top_left: [0.0; 2],
            bottom_right: [0.0; 2],
            rect: GeRectangle { bottom: 9.0, left: 0.0, right: 4.0, top: 0.0 },
            texture: std::ptr::null_mut(),
            x_scale: 1.0,
            y_scale: 1.0,
        };
        let mut out = [GeGlyphDraw::default(); 2];
        let at = Ge2DPoint { x: 1.0, y: 2.0 };
        // SAFETY: valid pointers
        let n = unsafe {
            ee_ge_font_draw_plan(&ops(), c"aaa".as_ptr(), 0x61, &cell, 1, &at, std::ptr::null(), out.as_mut_ptr(), 2)
        };
        assert_eq!(n, 2);
        assert_eq!(out[1].location, Ge2DPoint { x: 5.0, y: 2.0 });
        // SAFETY: an empty text writes nothing
        let n = unsafe {
            ee_ge_font_draw_plan(&ops(), c"".as_ptr(), 0x61, &cell, 1, &at, std::ptr::null(), std::ptr::null_mut(), 0)
        };
        assert_eq!(n, 0);
        let mut px = [0x00F0u16; 4];
        // SAFETY: 2 x 2, 4 bytes a row
        unsafe { ee_ge_font_surface_alpha(px.as_mut_ptr(), 2, 2, 4) };
        assert_eq!(px, [0xFFFF; 4]);
        let mut p = GeFontPacker::default();
        let mut r = GeRectangle::default();
        // SAFETY: valid pointers
        assert!(unsafe { ee_ge_font_cell_place(&mut p, 5.0, 8.0, &mut r) });
        assert_eq!(p.x, 5.0);
    }
}
