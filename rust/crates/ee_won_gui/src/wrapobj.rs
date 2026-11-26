//! WONGUI's WrappedText as a Rust object (R6 WON round 38): every label's and skinned button's text layout. The
//! original's WrappedText.cpp: the line breaks for the font and maximum width (`guiwrap`'s rule), the text's width
//! and height, the wrap and centre settings, the rewrap rule of `SetMaxWidth`, and the paint's plan (`guidraw`'s
//! `wrapped_text` over the object's own breaks).
//!
//! The C++ `WrappedText` stays the reference-counted type the labels and buttons hold; it keeps its C++-object
//! members (the text and the font) and passes them to `ee_won_wrap_calc` (the text's units and a measure of the
//! font) when the original rewrapped. The setters that may rewrap answer whether to.

use std::ffi::c_void;

use ee_base::plat_core::string::WChar;

use crate::guidraw::{wrapped_text, EeWonDrawOp, EeWonLineBreak, EeWonTextMeasure};
use crate::guiwrap::{max_width_change, wrap};
use crate::platfont::{ee_won_plat_font_char_width, ee_won_plat_font_metric, ee_won_plat_font_width, EeWonPlatFont};

/// The Rust WrappedText (the original's members but the text and font).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EeWonWrapText {
    /// `mBreakList` (each field the original's `unsigned short`).
    pub breaks: Vec<EeWonLineBreak>,
    pub width: i32,
    pub height: i32,
    pub max_width: i32,
    pub wrap: bool,
    pub centered: bool,
}

impl EeWonWrapText {
    /// `Init()`: wrapping, not centred, no size and no maximum width.
    pub fn new() -> Self {
        EeWonWrapText { wrap: true, ..Default::default() }
    }

    /// `CalcWrap()` over the text's units, `measure` the font's (`string_width(0, len)` and `char_width(i)`) and
    /// its height; nothing without a font.
    pub fn calc(&mut self, has_font: bool, units: &[u32], m: &EeWonTextMeasure, font_height: i32) {
        if !has_font {
            return;
        }
        let len = units.len() as i32;
        if !self.wrap {
            self.width = m.string_width_at(0, len);
            self.height = font_height;
            return;
        }
        if self.max_width == 0 {
            return;
        }
        self.breaks.clear();
        // the original passed the length as an unsigned short to the wrap
        let n = usize::from(len as u16);
        let widths: Vec<i32> = (0..len).map(|i| m.char_width_at(i)).collect();
        let (b, width) = wrap(&units[..n], &widths[..n], self.max_width);
        self.breaks.extend(b.iter().map(|x| EeWonLineBreak {
            break_pos: x.pos as u16,
            next_start: x.next as u16,
            width: x.width as u16,
        }));
        self.width = width;
        // (size + 1) * height in the original's size_t, kept as an int
        self.height = ((self.breaks.len() as u64 + 1).wrapping_mul(font_height as i64 as u64)) as i32;
    }

    /// `SetWrap`: whether to rewrap (the setting changed).
    pub fn set_wrap(&mut self, wrap: bool) -> bool {
        if wrap != self.wrap {
            self.wrap = wrap;
            return true;
        }
        false
    }

    /// `SetMaxWidth`: whether to rewrap (`guiwrap`'s rule).
    pub fn set_max_width(&mut self, width: i32) -> bool {
        match max_width_change(width, self.max_width, self.wrap, !self.breaks.is_empty()) {
            Some((w, rewrap)) => {
                self.max_width = w;
                rewrap
            }
            None => false,
        }
    }

    /// `Paint`'s plan (with a font: the C++ returns before it without one).
    pub fn paint(&self, length: i32, font_height: i32, x: i32, y: i32, m: &EeWonTextMeasure) -> Vec<EeWonDrawOp> {
        wrapped_text(true, length, self.centered, self.width, font_height, &self.breaks, x, y, m)
    }
}

/// A new WrappedText's state (`Init()`).
#[no_mangle]
pub extern "C" fn ee_won_wrap_new() -> *mut EeWonWrapText {
    Box::into_raw(Box::new(EeWonWrapText::new()))
}

/// # Safety
/// `o` is null or from `ee_won_wrap_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_free(o: *mut EeWonWrapText) {
    if !o.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(o) });
    }
}

/// `CalcWrap()`: `text` holds `len` units of `wchar_size` bytes (or is NULL with 0), `m` measures them in the font.
///
/// # Safety
/// `o` is live; `text` and `m` as described.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_calc(
    o: *mut EeWonWrapText,
    has_font: bool,
    text: *const c_void,
    len: i32,
    wchar_size: u32,
    m: *const EeWonTextMeasure,
    font_height: i32,
) {
    let n = len.max(0) as usize;
    let units: Vec<u32> = if n == 0 || text.is_null() {
        Vec::new()
    } else if wchar_size == 2 {
        // SAFETY: `len` units (the contract)
        unsafe { std::slice::from_raw_parts(text.cast::<u16>(), n) }.iter().map(|&u| u32::from(u)).collect()
    } else {
        // SAFETY: `len` units (the contract)
        unsafe { std::slice::from_raw_parts(text.cast::<u32>(), n) }.to_vec()
    };
    // SAFETY: the contract
    unsafe { (*o).calc(has_font, &units, &*m, font_height) }
}

/// `SetWrap(wrap)`: true when the C++ must rewrap.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_wrap(o: *mut EeWonWrapText, wrap: bool) -> bool {
    // SAFETY: the contract
    unsafe { (*o).set_wrap(wrap) }
}

/// `SetMaxWidth(width)`: true when the C++ must rewrap.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_max_width(o: *mut EeWonWrapText, width: i32) -> bool {
    // SAFETY: the contract
    unsafe { (*o).set_max_width(width) }
}

/// `SetCentered(centered)`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_centered(o: *mut EeWonWrapText, centered: bool) {
    // SAFETY: the contract
    unsafe { (*o).centered = centered }
}

/// `mWidth`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_width(o: *const EeWonWrapText) -> i32 {
    // SAFETY: the contract
    unsafe { (*o).width }
}

/// `mHeight` (the C++ `GetHeight` answers the font's height when it is 0).
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_height(o: *const EeWonWrapText) -> i32 {
    // SAFETY: the contract
    unsafe { (*o).height }
}

/// The break list (`mBreakList`): the first `cap` into `out`, their count returned.
///
/// # Safety
/// `o` is live; `out` has room for `cap` breaks (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_breaks(o: *const EeWonWrapText, out: *mut EeWonLineBreak, cap: usize) -> usize {
    // SAFETY: the contract
    let b = unsafe { &(*o).breaks };
    if !out.is_null() {
        for (i, x) in b.iter().take(cap).enumerate() {
            // SAFETY: room for `cap` (the contract)
            unsafe { *out.add(i) = *x };
        }
    }
    b.len()
}

/// `Paint`'s plan for a text of `length` units in a font `font_height` high at (`x`, `y`): the first `cap`
/// operations into `out`, their count returned.
///
/// # Safety
/// `o` and `m` are live; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_paint(
    o: *const EeWonWrapText,
    length: i32,
    font_height: i32,
    x: i32,
    y: i32,
    m: *const EeWonTextMeasure,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    // SAFETY: the contract
    let ops = unsafe { (*o).paint(length, font_height, x, y, &*m) };
    if !out.is_null() {
        for (i, op) in ops.iter().take(cap).enumerate() {
            // SAFETY: room for `cap` (the contract)
            unsafe { *out.add(i) = *op };
        }
    }
    ops.len()
}

/// A text in a font: what the measure of `CalcWrap` and `Paint` reads (the C++ WONDrawMeasure until R6 WON GUI
/// round 36: `Font::GetStringWidth` over the text's units, the offset and the length as the original's unsigned
/// shorts and 0 for no units; `GetCharacterWidth` of one unit, or of 'M' for a negative index).
struct FontText {
    font: *const EeWonPlatFont,
    text: *const WChar,
}

unsafe extern "C" fn font_string_width(ctx: *mut c_void, offset: i32, length: i32) -> i32 {
    // SAFETY: `measure_of`'s record, live for the call
    let ft = unsafe { &*(ctx as *const FontText) };
    let (o, n) = (offset as u16, length as u16);
    if n == 0 || ft.font.is_null() {
        return 0;
    }
    // SAFETY: a live font; the caller's text holds the units asked for
    unsafe { ee_won_plat_font_width(ft.font, ft.text.add(o as usize) as *const c_void, n) }
}

unsafe extern "C" fn font_char_width(ctx: *mut c_void, index: i32) -> i32 {
    // SAFETY: `measure_of`'s record, live for the call
    let ft = unsafe { &*(ctx as *const FontText) };
    if ft.font.is_null() {
        return 0;
    }
    // SAFETY: a live font; the caller's text holds the unit asked for
    unsafe {
        ee_won_plat_font_char_width(ft.font, if index < 0 { 'M' as i32 } else { *ft.text.add(index as usize) as i32 })
    }
}

fn measure_of(ft: &FontText) -> EeWonTextMeasure {
    EeWonTextMeasure {
        ctx: ft as *const FontText as *mut c_void,
        string_width: Some(font_string_width),
        char_width: Some(font_char_width),
    }
}

/// `WrappedText::CalcWrap()` whole: the `len` units at `text` wrapped for the font `font` (null: none) by its
/// widths and height.
///
/// # Safety
/// `o` is live; `font` null or a live font; `text` holds `len` units (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_calc_font(
    o: *mut EeWonWrapText,
    font: *const EeWonPlatFont,
    text: *const c_void,
    len: i32,
) {
    let ft = FontText { font, text: text as *const WChar };
    // SAFETY: a live font
    let height = if font.is_null() { 0 } else { unsafe { ee_won_plat_font_metric(font, 0) } };
    // SAFETY: the contract; the measure lives for the call
    unsafe {
        ee_won_wrap_calc(o, !font.is_null(), text, len, std::mem::size_of::<WChar>() as u32, &measure_of(&ft), height)
    }
}

/// `WrappedText::Paint`'s plan for the `length` units at `text` in the font `font` at (`x`, `y`): the first `cap`
/// operations into `out`, their count returned.
///
/// # Safety
/// `o` and `font` are live; `text` holds `length` units; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_paint_font(
    o: *const EeWonWrapText,
    font: *const EeWonPlatFont,
    text: *const c_void,
    length: i32,
    x: i32,
    y: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let ft = FontText { font, text: text as *const WChar };
    // SAFETY: the contract; the measure lives for the call
    unsafe { ee_won_wrap_paint(o, length, ee_won_plat_font_metric(font, 0), x, y, &measure_of(&ft), out, cap) }
}

/// Where the game's WrappedText keeps its members and the classes that hold one keep it (offsets in a
/// `WrappedText`, a `Label`, an `MSButtonBase` and a `Font`; GuiHost.cpp registers them; R6 WON GUI round 40).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonWrapAt {
    /// `WrappedText::mText` (a GUIString: its text's pointer), `mFont` (a SmartPtr), `mObj`
    pub text: usize,
    pub font: usize,
    pub obj: usize,
    /// `Label::mWrappedText`, `MSButtonBase::mText` (SmartPtrs)
    pub label_text: usize,
    pub msbtn_text: usize,
    /// `Font::mRust`
    pub font_rust: usize,
}
static WRAP_AT: std::sync::Mutex<Option<EeWonWrapAt>> = std::sync::Mutex::new(None);

/// Registers the wrapped texts' member offsets (GuiHost.cpp at static initialisation).
///
/// # Safety
/// `at` points at the offsets.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_register(at: *const EeWonWrapAt) {
    if let Ok(mut s) = WRAP_AT.lock() {
        // SAFETY: the contract
        *s = Some(unsafe { *at });
    }
}

fn wrap_at() -> Option<EeWonWrapAt> {
    if !crate::compobj::is_game() {
        return None;
    }
    *WRAP_AT.lock().ok()?
}

/// The word at `p + off` (a pointer or SmartPtr member).
///
/// # Safety
/// `p + off` holds a pointer-sized member of a live object.
unsafe fn word(p: isize, off: usize) -> isize {
    // SAFETY: the contract
    unsafe { *((p as usize + off) as *const isize) }
}

/// The wrapped text a game label holds (`mWrappedText`); None in a driver's world.
pub(crate) fn label_text(label: isize) -> Option<isize> {
    let at = wrap_at().filter(|_| label != 0)?;
    // SAFETY: the game's live label
    Some(unsafe { word(label, at.label_text) })
}
/// The wrapped text a game MS button holds (`mText`); None in a driver's world.
pub(crate) fn msbtn_text(button: isize) -> Option<isize> {
    let at = wrap_at().filter(|_| button != 0)?;
    // SAFETY: the game's live MS button
    Some(unsafe { word(button, at.msbtn_text) })
}

/// A label's or MS button's host op on its wrapped text `w` (None: not the game's, or no wrapped text op).
pub(crate) fn wrap_host_op(w: Option<isize>, op: crate::guihost::EeWonGuiOp, a: isize) -> Option<isize> {
    use crate::guihost::{EeWonGuiOp as Op, EeWonWbText as T};
    let which = match op {
        Op::WrapSetMaxWidth => T::SetMaxWidth,
        Op::WrapSetCentered => T::SetCentered,
        Op::WrapSetWrap => T::SetWrap,
        Op::WrapWidth => T::Width,
        Op::WrapHeight => T::Height,
        _ => return None,
    };
    game_wrap_op(w?, which, a)
}

/// One of the game's WrappedText `w`'s methods in Rust (R6 WON GUI round 40): its text and font held in place, its
/// Rust object rewrapped when the original's `CalcWrap` ran. `c` the argument. None in a driver's world and for
/// `New`, `Hold` and `Release`.
pub(crate) fn game_wrap_op(w: isize, which: crate::guihost::EeWonWbText, c: isize) -> Option<isize> {
    use crate::guihost::EeWonWbText as T;
    let at = wrap_at()?;
    let slot = |off: usize| (w as usize + off) as *mut isize;
    // SAFETY: the game's live wrapped text (the callers'): its Rust object at the registered offset
    let obj = unsafe { *slot(at.obj) } as *mut EeWonWrapText;
    let calc = || {
        // SAFETY: the text's live font (its SmartPtr holds it) and its text (a guistrobj.rs block)
        unsafe {
            let f = *slot(at.font);
            let font = if f == 0 { std::ptr::null() } else { word(f, at.font_rust) as *const EeWonPlatFont };
            let t = *slot(at.text) as *const u8;
            let (p, n) = if t.is_null() {
                (std::ptr::null(), 0)
            } else {
                let b = &*(t.add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf);
                (b.text as *const c_void, i32::from(b.length))
            };
            ee_won_wrap_calc_font(obj, font, p, n);
        }
    };
    // SAFETY (the calls below): the live Rust object
    Some(match which {
        T::SetText => {
            // SAFETY: `c` a live GUIString; the slot holds its text's reference (GUIString's assignment)
            unsafe {
                let new = *(c as *const *const crate::guistrobj::EeWonGstr);
                if !new.is_null() {
                    crate::guistrobj::gstr_ref(new);
                }
                let old = std::mem::replace(&mut *(slot(at.text) as *mut *const crate::guistrobj::EeWonGstr), new);
                if !old.is_null() {
                    crate::guistrobj::gstr_release(old);
                }
            }
            calc();
            0
        }
        T::SetMaxWidth => {
            // SAFETY: as above
            if unsafe { ee_won_wrap_set_max_width(obj, c as i32) } {
                calc();
            }
            0
        }
        T::SetWrap => {
            // SAFETY: as above
            if unsafe { ee_won_wrap_set_wrap(obj, c != 0) } {
                calc();
            }
            0
        }
        T::SetCentered => {
            // SAFETY: as above
            unsafe { ee_won_wrap_set_centered(obj, c != 0) };
            0
        }
        T::SetFont => {
            if c != 0 {
                crate::gamewin::rf(c, true)?;
            }
            // SAFETY: the text's font slot (a SmartPtr)
            let old = unsafe { std::mem::replace(&mut *slot(at.font), c) };
            if old != 0 {
                crate::gamewin::rf(old, false)?;
            }
            calc();
            0
        }
        // SAFETY: as above
        T::Width => (unsafe { ee_won_wrap_width(obj) }) as isize,
        T::Height => {
            // SAFETY: as above
            let h = unsafe { ee_won_wrap_height(obj) };
            if h != 0 {
                h as isize
            } else {
                // SAFETY: the text's live font (GetHeight's `mFont->GetHeight()`)
                (unsafe { ee_won_plat_font_metric(word(*slot(at.font), at.font_rust) as *const EeWonPlatFont, 0) })
                    as isize
            }
        }
        // SAFETY: as above
        T::Font => unsafe { *slot(at.font) },
        T::Text => slot(at.text) as isize,
        _ => return None,
    })
}

#[cfg(test)]
#[path = "wrapobj_tests.rs"]
mod tests;
