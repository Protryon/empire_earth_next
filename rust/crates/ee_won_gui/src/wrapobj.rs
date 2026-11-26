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

/// The kind of a Rust-made RefCount that is a game WrappedText (guistrobj.rs's texts are 0, wmobj.rs's events 1).
pub const EE_WON_RC_WRAP: u32 = 2;

/// The game's WrappedText as a whole Rust object (R6 WON GUI round 41: the C++ class's members, constructors,
/// CalcWrap and Paint went; the class is this object's handle): a C++ RefCount without a vtable
/// (`RefCount::Release` gives its last reference to `ee_won_gstr_free`, which frees it by its kind), its text (a held
/// text: the field's address is a `GUIString*`), its font (a held `Font*`) and the wrap.
#[repr(C)]
pub struct EeWonWrapRc {
    vtable: *const c_void,
    refs: i32,
    kind: u32,
    text: *const crate::guistrobj::EeWonGstr,
    font: isize,
    obj: EeWonWrapText,
}

/// Where the classes that hold one keep their wrapped text, and `Font::mRust` (offsets in a `Label`, an
/// `MSButtonBase` and a `Font`; GuiHost.cpp registers them; R6 WON GUI rounds 40-41).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonWrapAt {
    /// `Label::mWrappedText`, `MSButtonBase::mText` (SmartPtrs)
    pub label_text: usize,
    pub msbtn_text: usize,
    /// `Font::mRust`
    pub font_rust: usize,
}
static WRAP_AT: std::sync::Mutex<Option<EeWonWrapAt>> = std::sync::Mutex::new(None);

/// Registers the holders' offsets (GuiHost.cpp at static initialisation).
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

/// `new WrappedText` (no reference yet): the empty text, no font, wrapping, not centred.
#[no_mangle]
pub extern "C" fn ee_won_wrapt_new() -> *mut c_void {
    let text = crate::guistrobj::ee_won_gstr_empty();
    // SAFETY: the shared empty text; the object's reference
    unsafe { crate::guistrobj::gstr_ref(text) };
    let w = EeWonWrapRc {
        vtable: std::ptr::null(),
        refs: 0,
        kind: EE_WON_RC_WRAP,
        text,
        font: 0,
        obj: EeWonWrapText::new(),
    };
    Box::into_raw(Box::new(w)) as *mut c_void
}

/// A game wrapped text freed at its last reference: its text and font given back.
///
/// # Safety
/// `w` came from `ee_won_wrapt_new` and is not used again.
pub(crate) unsafe fn free_rc(w: *mut c_void) {
    // SAFETY: the contract: made by Box
    let b = unsafe { Box::from_raw(w as *mut EeWonWrapRc) };
    // SAFETY: the reference the object held
    unsafe { crate::guistrobj::gstr_release(b.text) };
    if b.font != 0 {
        crate::gamewin::rf(b.font, false);
    }
}

/// One of WrappedText's methods on the game's wrapped text `w` (`which`; `c` the argument): `SetText(*(const
/// GUIString*)c)`, the setters that may rewrap, the size, `GetFont()` and `&GetText()`.
///
/// # Safety
/// `w` is a live object of `ee_won_wrapt_new`; `c` as the method needs it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrapt_call(w: *mut c_void, which: crate::guihost::EeWonWbText, c: isize) -> isize {
    // SAFETY: the contract
    unsafe { wrap_call(w as *mut EeWonWrapRc, which, c) }.unwrap_or(0)
}

/// The text's font's platform font (NULL without one).
fn plat_font(w: &EeWonWrapRc) -> *const EeWonPlatFont {
    let at = WRAP_AT.lock().ok().and_then(|a| *a).unwrap_or_default();
    if w.font == 0 {
        std::ptr::null()
    } else {
        // SAFETY: the text's live font (its reference holds it): its Rust font at the registered offset
        unsafe { word(w.font, at.font_rust) as *const EeWonPlatFont }
    }
}

/// The text's units' pointer and length.
fn units_of(w: &EeWonWrapRc) -> (*const c_void, i32) {
    // SAFETY: the held text: its block at its offset
    let b = unsafe {
        &*((w.text as *const u8).add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf)
    };
    (b.text as *const c_void, i32::from(b.length))
}

/// `CalcWrap()`: the breaks and size for the font and maximum width.
fn calc(w: &mut EeWonWrapRc) {
    let font = plat_font(w);
    let (p, n) = units_of(w);
    // SAFETY: the live Rust object, font (or NULL) and text
    unsafe { ee_won_wrap_calc_font(&mut w.obj, font, p, n) };
}

/// The methods (`ee_won_wrapt_call`). None for `New`, `Hold` and `Release`.
///
/// # Safety
/// As `ee_won_wrapt_call`.
unsafe fn wrap_call(w: *mut EeWonWrapRc, which: crate::guihost::EeWonWbText, c: isize) -> Option<isize> {
    use crate::guihost::EeWonWbText as T;
    // SAFETY: the contract; no other reference to it is live during a method (the reference calls do not re-enter)
    let w = unsafe { &mut *w };
    Some(match which {
        T::SetText => {
            // SAFETY: `c` a live GUIString; the field holds its text's reference (GUIString's assignment)
            unsafe {
                let new = *(c as *const *const crate::guistrobj::EeWonGstr);
                crate::guistrobj::gstr_ref(new);
                let old = std::mem::replace(&mut w.text, new);
                crate::guistrobj::gstr_release(old);
            }
            calc(w);
            0
        }
        T::SetMaxWidth => {
            if w.obj.set_max_width(c as i32) {
                calc(w);
            }
            0
        }
        T::SetWrap => {
            // SAFETY: the live Rust object
            if unsafe { ee_won_wrap_set_wrap(&mut w.obj, c != 0) } {
                calc(w);
            }
            0
        }
        T::SetCentered => {
            w.obj.centered = c != 0;
            0
        }
        T::SetFont => {
            if c != 0 {
                crate::gamewin::rf(c, true)?;
            }
            let old = std::mem::replace(&mut w.font, c);
            if old != 0 {
                crate::gamewin::rf(old, false)?;
            }
            calc(w);
            0
        }
        T::Width => w.obj.width as isize,
        T::Height => {
            if w.obj.height != 0 {
                w.obj.height as isize
            } else {
                // SAFETY: the text's live font (GetHeight's `mFont->GetHeight()`)
                (unsafe { ee_won_plat_font_metric(plat_font(w), 0) }) as isize
            }
        }
        T::Font => w.font,
        T::Text => &w.text as *const _ as isize,
        _ => return None,
    })
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
/// The game label `label`'s constructor's `mWrappedText = new WrappedText` (its slot empty before; R6 WON GUI round
/// 41); None in a driver's world.
pub(crate) fn label_new_text(label: isize) -> Option<isize> {
    let at = wrap_at().filter(|_| label != 0)?;
    let w = ee_won_wrapt_new() as isize;
    crate::gamewin::rf(w, true)?;
    // SAFETY: the game's live label: its SmartPtr slot (empty in the constructor)
    unsafe { *((label as usize + at.label_text) as *mut isize) = w };
    Some(0)
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

/// One of the game's WrappedText `w`'s methods in Rust (R6 WON GUI round 40; the object Rust's since round 41).
/// `c` the argument. None in a driver's world and for `Hold` and `Release`; `New` answers a new text with one
/// reference.
pub(crate) fn game_wrap_op(w: isize, which: crate::guihost::EeWonWbText, c: isize) -> Option<isize> {
    wrap_at()?;
    if which == crate::guihost::EeWonWbText::New {
        let n = ee_won_wrapt_new() as isize;
        crate::gamewin::rf(n, true)?;
        return Some(n);
    }
    // SAFETY: the game's live wrapped text (the callers')
    unsafe { wrap_call(w as *mut EeWonWrapRc, which, c) }
}

/// `WrappedText::Paint(g, x, y)` on the game's wrapped text `w` (R6 WON GUI round 41: the C++ method and the
/// Graphics host's Text case went): each wrapped line (centred if set) from `x`, `y`, nothing without a font; the
/// plan's font and string slots the text's own. None in a driver's world.
///
/// # Safety
/// `g` is the game's live Graphics, `w` a live wrapped text.
pub(crate) unsafe fn game_paint(g: *mut crate::gfxrun::EeWonGraphics, w: isize, x: i32, y: i32) -> Option<()> {
    wrap_at()?;
    // SAFETY: the contract
    let w = unsafe { &*(w as *const EeWonWrapRc) };
    if w.font == 0 {
        return Some(());
    }
    let font = plat_font(w);
    let (p, n) = units_of(w);
    // SAFETY: the live Rust object and font; the plan asked for its length, then filled
    let ops = unsafe {
        let count = ee_won_wrap_paint_font(&w.obj, font, p, n, x, y, std::ptr::null_mut(), 0);
        let mut v = vec![EeWonDrawOp::default(); count];
        let count = ee_won_wrap_paint_font(&w.obj, font, p, n, x, y, v.as_mut_ptr(), v.len());
        v.truncate(count);
        v
    };
    let slots = crate::gfxrun::EeWonDrawSlots {
        font: w.font as *mut c_void,
        string: &w.text as *const _ as *const c_void,
        ..Default::default()
    };
    // SAFETY: the contract; the slots' objects are the live text's
    unsafe { crate::gfxrun::ee_won_graphics_draw(g, ops.as_ptr(), ops.len(), &slots) };
    Some(())
}

#[cfg(test)]
#[path = "wrapobj_tests.rs"]
mod tests;
