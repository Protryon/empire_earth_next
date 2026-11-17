//! WONGUI's font on the platform layer as a whole Rust object (R6 WON round 40): the C++ `PlatGUIFont`
//! (WONLobbyPlatform.h), MSFont's replacement, over the platform layer's fonts (ee_plat_ui's `PlatFont`). The face,
//! size and style of the font's descriptor, the font made from them (the point size as GDI's character height at
//! 96 dpi, small sizes without smoothing as GDI drew them at DEFAULT_QUALITY), its metrics, the copy rasterised at
//! a surface's pixel scale (smoothed: there are pixels enough for it; the last one is kept), the string and
//! character widths and the drawing of part of a string into a surface within its clip.
//!
//! An underlined font's underline was drawn by MSFont as a line just above the descent through the context's
//! Graphics: the Rust answers the line and the C++ shell draws it with the Graphics' DrawLineHook. The text is
//! the GUI's wide characters, made UTF-8 for the platform fonts (decision D6) by the platform layer's own
//! conversion.

use std::ffi::{c_char, c_void};

use ee_base::plat_core::string::{PlatWideToUtf8, WChar};
use ee_base::plat_ui::font::{
    PlatFont, PlatFontCreate, PlatFontDestroy, PlatFontDraw, PlatFontGetMetrics, PlatFontMeasure, PlatFontMetrics,
    PlatFontRect, PlatFontTarget,
};

use crate::platgfx::pixels;
use crate::surface::{EeWonSurface, EeWonSurfaceField as F};

/// `FontStyle_Bold`, `FontStyle_Italic`, `FontStyle_Underline`.
const BOLD: i32 = 1;
const ITALIC: i32 = 2;
const UNDERLINE: i32 = 4;

/// The lobby's text as GDI drew it at DEFAULT_QUALITY: small sizes without smoothing.
const ANTIALIASED: i32 = 0;

/// MulDiv: `a * b / c` rounded half away from zero.
pub fn mul_div_round(a: i32, b: i32, c: i32) -> i32 {
    let p = a as i64 * b as i64;
    let half = (c / 2) as i64;
    ((if p >= 0 { p + half } else { p - half }) / c as i64) as i32
}

/// Wide text as UTF-8 (`PlatGUIToUtf8`).
pub fn to_utf8(text: &[WChar]) -> Vec<u8> {
    if text.is_empty() {
        return Vec::new();
    }
    // SAFETY: `text` holds its units; a null output asks for the length
    let n = unsafe { PlatWideToUtf8(text.as_ptr(), text.len() as u32, std::ptr::null_mut(), 0) };
    let mut out = vec![0u8; n as usize];
    if n != 0 {
        // SAFETY: `out` holds n bytes
        unsafe { PlatWideToUtf8(text.as_ptr(), text.len() as u32, out.as_mut_ptr() as *mut c_char, n) };
    }
    out
}

/// The Rust PlatGUIFont.
pub struct EeWonPlatFont {
    /// The descriptor's face, NUL-terminated.
    face: Vec<u8>,
    size: i32,
    style: i32,
    font: *mut PlatFont,
    scaled: *mut PlatFont,
    scaled_scale: f32,
    height: i32,
    ascent: i32,
    descent: i32,
}

fn create(face: &[u8], height: i32, style: i32, antialiased: i32) -> *mut PlatFont {
    // SAFETY: `face` is NUL-terminated
    unsafe {
        PlatFontCreate(
            face.as_ptr() as *const c_char,
            height,
            i32::from(style & BOLD != 0),
            i32::from(style & ITALIC != 0),
            0,
            antialiased,
        )
    }
}

impl EeWonPlatFont {
    /// The constructor: the font of the descriptor and its metrics (zero without a font).
    pub fn new(face: &[u8], size: i32, style: i32) -> Self {
        let end = face.iter().position(|&b| b == 0).unwrap_or(face.len());
        let mut f = face[..end].to_vec();
        f.push(0);
        let font = create(&f, -mul_div_round(size, 96, 72), style, ANTIALIASED);
        let mut m = PlatFontMetrics::default();
        if !font.is_null() {
            // SAFETY: a live font
            unsafe { PlatFontGetMetrics(font, &mut m) };
        }
        EeWonPlatFont {
            face: f,
            size,
            style,
            font,
            scaled: std::ptr::null_mut(),
            scaled_scale: 0.0,
            height: m.height,
            ascent: m.ascent,
            descent: m.descent,
        }
    }

    pub fn height(&self) -> i32 {
        self.height
    }
    pub fn ascent(&self) -> i32 {
        self.ascent
    }
    pub fn descent(&self) -> i32 {
        self.descent
    }

    /// `FontForScale`: the logical font at 1, else the same request with the character height times the scale,
    /// smoothed (the last one is kept; the logical font when it cannot be made).
    pub fn for_scale(&mut self, scale: f32) -> *const PlatFont {
        if scale == 1.0 || self.font.is_null() {
            return self.font;
        }
        if !self.scaled.is_null() && self.scaled_scale == scale {
            return self.scaled;
        }
        if !self.scaled.is_null() {
            // SAFETY: ours
            unsafe { PlatFontDestroy(self.scaled) };
        }
        let h = -((mul_div_round(self.size, 96, 72) as f32 * scale + 0.5).floor() as i32);
        self.scaled = create(&self.face, h, self.style, 1);
        self.scaled_scale = scale;
        if self.scaled.is_null() {
            self.font
        } else {
            self.scaled
        }
    }

    /// `GetStringWidth`: the pixel width of the text (0 when empty).
    pub fn width(&self, text: &[WChar]) -> i32 {
        if text.is_empty() || self.font.is_null() {
            return 0;
        }
        let b = to_utf8(text);
        // SAFETY: a live font and the bytes
        unsafe { PlatFontMeasure(self.font, b.as_ptr() as *const c_char, b.len() as u32) }
    }

    /// `DrawString`'s text: the cell's top-left at the logical point's pixel, in `color`, within the surface's clip.
    pub fn draw(&mut self, s: &mut EeWonSurface, scale: f32, color: u32, text: &[WChar], x: i32, y: i32) {
        if s.pixels().is_null() || self.font.is_null() || text.is_empty() {
            return;
        }
        let bytes = to_utf8(text);
        let target = PlatFontTarget {
            pixels: s.pixels(),
            pitch: s.field(F::BytesPerRow),
            width: s.field(F::Width),
            height: s.field(F::Height),
            flags: 0,
            scale: 0.0,
        };
        let clip = PlatFontRect {
            left: s.field(F::ClipX1),
            top: s.field(F::ClipY1),
            right: s.field(F::ClipX2).wrapping_add(1),
            bottom: s.field(F::ClipY2).wrapping_add(1),
        };
        if clip.right > clip.left && clip.bottom > clip.top {
            let font = self.for_scale(scale);
            // SAFETY: a live font, the bytes, the surface's pixels as the target
            unsafe {
                PlatFontDraw(
                    font,
                    bytes.as_ptr() as *const c_char,
                    bytes.len() as u32,
                    pixels(x, scale),
                    pixels(y, scale),
                    color,
                    0,
                    0,
                    &clip,
                    &target,
                )
            };
        }
    }

    /// The underline MSFont drew for an underlined font (a line just above the descent, as wide as the text):
    /// (x1, y, x2), or None.
    pub fn underline(&self, text: &[WChar], x: i32, y: i32) -> Option<(i32, i32, i32)> {
        if self.style & UNDERLINE == 0 {
            return None;
        }
        let d = self.descent;
        let m = if d < 3 {
            0
        } else if d == 3 {
            1
        } else {
            d >> 1
        };
        let ly = y.wrapping_add(self.height).wrapping_sub(m).wrapping_sub(1);
        Some((x, ly, x.wrapping_add(self.width(text))))
    }
}

impl Drop for EeWonPlatFont {
    fn drop(&mut self) {
        // SAFETY: ours (null is a no-op)
        unsafe {
            PlatFontDestroy(self.scaled);
            PlatFontDestroy(self.font);
        }
    }
}

/// The text at `text` (`len` wide characters; null with 0).
///
/// # Safety
/// `text` holds `len` units.
unsafe fn units<'a>(text: *const c_void, len: u16) -> &'a [WChar] {
    if text.is_null() || len == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(text as *const WChar, len as usize) }
    }
}

/// `new PlatGUIFont(context, descriptor)`'s font: `face` the descriptor's face (NUL-terminated).
///
/// # Safety
/// `face` is NUL-terminated (or null: no face).
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_font_new(face: *const c_char, size: i32, style: i32) -> *mut EeWonPlatFont {
    let f: &[u8] = if face.is_null() {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::ffi::CStr::from_ptr(face) }.to_bytes()
    };
    Box::into_raw(Box::new(EeWonPlatFont::new(f, size, style)))
}

/// The font's destruction (its platform fonts with it).
///
/// # Safety
/// `f` is null or from `ee_won_plat_font_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_font_free(f: *mut EeWonPlatFont) {
    if !f.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(f) });
    }
}

/// `GetHeight()` (0), `GetAscent()` (1), `GetDescent()` (2).
///
/// # Safety
/// `f` is a live font.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_font_metric(f: *const EeWonPlatFont, which: i32) -> i32 {
    // SAFETY: the contract
    let f = unsafe { &*f };
    match which {
        0 => f.height(),
        1 => f.ascent(),
        _ => f.descent(),
    }
}

/// `GetStringWidth(text, offset, length)` over the `len` units at `text` (the offset applied by the caller).
///
/// # Safety
/// `f` is a live font; `text` holds `len` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_font_width(f: *const EeWonPlatFont, text: *const c_void, len: u16) -> i32 {
    // SAFETY: the contract
    unsafe { (*f).width(units(text, len)) }
}

/// `DrawString(context, text, offset, length, x, y)` once the context is known: the text drawn into the context's
/// surface `s` (null: none) at its pixel scale in `color` (the Graphics' colour without its top byte, 0 without
/// one); the underline to draw into `line` (x1, y, x2) and true when the font is underlined and `graphics`.
///
/// # Safety
/// `f` is a live font; `s` null or a live surface; `text` holds `len` units; `line` holds three ints.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_plat_font_draw(
    f: *mut EeWonPlatFont,
    s: *mut EeWonSurface,
    scale: f32,
    graphics: bool,
    color: u32,
    text: *const c_void,
    len: u16,
    x: i32,
    y: i32,
    line: *mut i32,
) -> bool {
    // SAFETY: the contract
    let (f, t) = unsafe { (&mut *f, units(text, len)) };
    let k = if scale > 0.0 { scale } else { 1.0 };
    if !s.is_null() {
        // SAFETY: the contract
        f.draw(unsafe { &mut *s }, k, color, t, x, y);
    }
    if !graphics {
        return false;
    }
    match f.underline(t, x, y) {
        Some((x1, ly, x2)) => {
            // SAFETY: three ints (the contract)
            unsafe {
                *line = x1;
                *line.add(1) = ly;
                *line.add(2) = x2;
            }
            true
        }
        None => false,
    }
}

#[cfg(test)]
#[path = "platfont_tests.rs"]
mod tests;
