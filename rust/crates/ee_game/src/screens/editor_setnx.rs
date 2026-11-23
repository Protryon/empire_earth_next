//! The terrain tab's host ops that were C++ statement sequences (`ee_screenscpp_setn`, Scenario Editor/
//! EEUIFSETerrain.cpp; the scenario editor lane's round 4): the terrain tables in the editor, a terrain's values,
//! the paint type and brush radio buttons, the terrain type and colour lists, the paint control, the preview
//! bitmap, its texture and tint, the selections, the class button, the load thread's posts, the paint action;
//! composed here of single C++ statements (`EE_SCREENS_SETNX_*`, `ee_screenscpp_setnx`) in the C++'s order.
//! [`compose`] answers an `EE_SCREENS_SETN_OP_*` op or `None` for the C++'s.

use std::os::raw::c_ulong;

use ee_base::math::types::GeRectangle;

use crate::screens::editor_brush::EE_SCREENS_SETN_OP_BRUSH;
use crate::screens::editor_terrain::*;

// ---- the statements (p0 the made object's out slot, else as named; p1, p2 the operands)
/// The terrain type tables: their count, table i0 in the editor, table i0.
pub const EE_SCREENS_SETNX_TT_COUNT: u32 = 0;
pub const EE_SCREENS_SETNX_TT_IN_EDITOR: u32 = 1;
pub const EE_SCREENS_SETNX_TT_GET: u32 = 2;
/// The terrain tables: the host's list cleared, the tables' count, table i0 in the editor, pushed, the list's size,
/// sorted by name (`EESETerrainNameLess`), its entry i0.
pub const EE_SCREENS_SETNX_T_CLEAR: u32 = 3;
pub const EE_SCREENS_SETNX_T_COUNT: u32 = 4;
pub const EE_SCREENS_SETNX_T_IN_EDITOR: u32 = 5;
pub const EE_SCREENS_SETNX_T_PUSH: u32 = 6;
pub const EE_SCREENS_SETNX_T_SIZE: u32 = 7;
pub const EE_SCREENS_SETNX_T_SORT: u32 = 8;
pub const EE_SCREENS_SETNX_T_AT: u32 = 9;
/// Terrain p1's `mID`, `mColorType`, `mIsAmbientsOnly`, its unit classes' count, its preview texture into *p2,
/// its first unit class.
pub const EE_SCREENS_SETNX_T_ID: u32 = 10;
pub const EE_SCREENS_SETNX_T_COLOR_TYPE: u32 = 11;
pub const EE_SCREENS_SETNX_T_AMBIENTS: u32 = 12;
pub const EE_SCREENS_SETNX_T_CLASS_COUNT: u32 = 13;
pub const EE_SCREENS_SETNX_T_PREVIEW: u32 = 14;
pub const EE_SCREENS_SETNX_T_CLASS: u32 = 15;
/// String i0 into the host's text; radio p1's label's background 0.4, its text transparent, coloured f0, f1, f2
/// (`SetnYellow`); radio p1 as a control.
pub const EE_SCREENS_SETNX_STRING: u32 = 16;
pub const EE_SCREENS_SETNX_LABEL_BG: u32 = 18;
pub const EE_SCREENS_SETNX_LABEL_TRANSPARENT: u32 = 19;
pub const EE_SCREENS_SETNX_LABEL_YELLOW: u32 = 20;
pub const EE_SCREENS_SETNX_RADIO_CONTROL: u32 = 21;
/// The type list's lines: i0 new lines, line i0's name of terrain p1[i0], its colour set i1; the list (index i0,
/// at f0, f1, width f2, form p1); the lines cleared; its event (group p1); list p1's trigger (event p2), text
/// transparent, yellow, selected background clear, background 0.4; added to form p1 (p2); as a control.
pub const EE_SCREENS_SETNX_LINES_NEW: u32 = 22;
pub const EE_SCREENS_SETNX_LINE_NAME: u32 = 23;
pub const EE_SCREENS_SETNX_LINE_COLOR_SET: u32 = 24;
pub const EE_SCREENS_SETNX_TYPE_LIST_NEW: u32 = 25;
pub const EE_SCREENS_SETNX_LINES_CLEAR: u32 = 26;
pub const EE_SCREENS_SETNX_TYPE_EVENT: u32 = 27;
pub const EE_SCREENS_SETNX_TYPE_TRIGGER: u32 = 28;
pub const EE_SCREENS_SETNX_EVENT_RELEASE: u32 = 29;
pub const EE_SCREENS_SETNX_TYPE_TEXT_TRANSPARENT: u32 = 30;
pub const EE_SCREENS_SETNX_TYPE_TEXT_YELLOW: u32 = 31;
pub const EE_SCREENS_SETNX_TYPE_SELECTED_CLEAR: u32 = 32;
pub const EE_SCREENS_SETNX_TYPE_BG: u32 = 33;
pub const EE_SCREENS_SETNX_TYPE_ADD: u32 = 34;
pub const EE_SCREENS_SETNX_TYPE_CONTROL: u32 = 35;
/// The colour list's lines: i0 new lines, type table p1[i0]'s first and last colour, colour i1 pushed on line i0;
/// the list (index i0, at f0, f1, width f2, form p1); the lines cleared; its event (group p1); list p1's trigger
/// (event p2), `ChangeSelectionIndex(i0, false, true)`, selected background clear, background 0.4; added to form p1
/// (p2); as a control.
pub const EE_SCREENS_SETNX_COLORS_NEW: u32 = 36;
pub const EE_SCREENS_SETNX_TYPE_FIRST_COLOR: u32 = 37;
pub const EE_SCREENS_SETNX_TYPE_LAST_COLOR: u32 = 38;
pub const EE_SCREENS_SETNX_COLORS_PUSH: u32 = 39;
pub const EE_SCREENS_SETNX_COLOR_LIST_NEW: u32 = 40;
pub const EE_SCREENS_SETNX_COLORS_CLEAR: u32 = 41;
pub const EE_SCREENS_SETNX_COLOR_EVENT: u32 = 42;
pub const EE_SCREENS_SETNX_COLOR_TRIGGER: u32 = 43;
pub const EE_SCREENS_SETNX_COLOR_SELECT: u32 = 44;
pub const EE_SCREENS_SETNX_COLOR_SELECTED_CLEAR: u32 = 45;
pub const EE_SCREENS_SETNX_COLOR_BG: u32 = 46;
pub const EE_SCREENS_SETNX_COLOR_ADD: u32 = 47;
pub const EE_SCREENS_SETNX_COLOR_CONTROL: u32 = 48;
/// The paint control (the handles p2, index i0, form p1); control p2 added to form p1 (layer 3).
pub const EE_SCREENS_SETNX_PAINT_NEW: u32 = 49;
pub const EE_SCREENS_SETNX_ADD_CONTROL: u32 = 50;
/// `new GEBitmap`; the preview `UIBitmap` (bitmap p2, index i0, at f0, f1, form p1); bitmap p1's `Dereference()`;
/// the `UIBitmap` p2 added to form p1; as a control.
pub const EE_SCREENS_SETNX_NEW_BITMAP: u32 = 51;
pub const EE_SCREENS_SETNX_UIBITMAP_NEW: u32 = 52;
pub const EE_SCREENS_SETNX_BITMAP_RELEASE: u32 = 53;
pub const EE_SCREENS_SETNX_UIBITMAP_ADD: u32 = 54;
pub const EE_SCREENS_SETNX_UIBITMAP_CONTROL: u32 = 55;
/// `EELoadTerrainPreviews()`, `EEReleaseTerrainPreviews()`.
pub const EE_SCREENS_SETNX_LOAD_PREVIEWS: u32 = 56;
pub const EE_SCREENS_SETNX_RELEASE_PREVIEWS: u32 = 57;
/// The colour list's and the type list's `GetCurrentSelection(*p2)` (list p1, p2 an `unsigned long`): the error.
pub const EE_SCREENS_SETNX_COLORS_SELECTION: u32 = 58;
pub const EE_SCREENS_SETNX_TYPES_SELECTION: u32 = 59;
/// The type list p1's line i0's texture.
pub const EE_SCREENS_SETNX_LINE_TEXTURE: u32 = 60;
/// `gUnitClassTable[i0] != NULL`, its button's texture.
pub const EE_SCREENS_SETNX_CLASS_EXISTS: u32 = 61;
pub const EE_SCREENS_SETNX_CLASS_BUTTON_TEXTURE: u32 = 62;
/// The preview p1's bitmap's `SetTexture(p2, list p0's screen)`; texture p1's width, height.
pub const EE_SCREENS_SETNX_PREVIEW_TEXTURE: u32 = 63;
pub const EE_SCREENS_SETNX_TEXTURE_WIDTH: u32 = 64;
pub const EE_SCREENS_SETNX_TEXTURE_HEIGHT: u32 = 65;
/// Form p1's load thread's `Post408(i0)`, `Post409(i0)`.
pub const EE_SCREENS_SETNX_POST_408: u32 = 68;
pub const EE_SCREENS_SETNX_POST_409: u32 = 69;
/// The paint control p1's `SKOP_PAINT_SET_CENTERED(i0 != 0)`; `new EESEAPaintRust(kSEAPaintTerrain, i0, i1)`,
/// `(kSEAPaintBulldozer)`.
pub const EE_SCREENS_SETNX_PAINT_CENTERED: u32 = 70;
pub const EE_SCREENS_SETNX_PAINT_TERRAIN: u32 = 71;
pub const EE_SCREENS_SETNX_PAINT_BULLDOZER: u32 = 72;

// round 6 (4000..): the shell radio of either kind, its rectangle's inputs, the bitmaps' Rust objects
/// The screen font's height; the form p1's radio-on bitmap's height into f0; `new EEUIShellRadio(kind i0, the
/// arguments at p2 (`[count, ...]`), id i1, the host's text, the screen font, the rectangle f (left, top, right,
/// bottom), form p1, index i2, border 0, checked i3)`.
pub const EE_SCREENS_SETNX_FONT_HEIGHT: u32 = 4000;
pub const EE_SCREENS_SETNX_RADIO_ON_HEIGHT: u32 = 4001;
pub const EE_SCREENS_SETNX_NEW_RADIO: u32 = 4002;
/// The preview p1's bitmap's Rust object (`EeGeBitmap`); its rectangle into *p2; control p1's screen; the colour
/// list p1's line i0's colours (a `GEColor` array); the type list p1's bitmap's Rust object.
pub const EE_SCREENS_SETNX_PREVIEW_BITMAP: u32 = 4003;
pub const EE_SCREENS_SETNX_PREVIEW_RECT: u32 = 4004;
pub const EE_SCREENS_SETNX_SCREEN: u32 = 4005;
pub const EE_SCREENS_SETNX_LINE_COLORS: u32 = 4006;
pub const EE_SCREENS_SETNX_T_BITMAP: u32 = 4007;
/// The host's text's bytes (NUL-terminated) into the out slot (after `EE_SCREENS_SETNX_STRING`).
pub const EE_SCREENS_SETNX_TEXT: u32 = 4008;
/// Rust's own statements (`ge_op`, never the C++): the bitmap p0's `ComputeStretchedCoordsAndScale` of the three
/// rectangles at p1 (texture, source, destination) on screen i0; the bitmap p0's `SetColorPerVertex` of the
/// colours i0..i3 of the `GEColor` array p1.
pub const EE_SCREENS_SETNX_GE_STRETCH: u32 = 4010;
pub const EE_SCREENS_SETNX_GE_COLORS: u32 = 4011;
/// The type list p1's destructor's parts: its bitmap released, its text deleted, its copy text deleted.
pub const EE_SCREENS_SETNX_T_BITMAP_RELEASE: u32 = 4012;
pub const EE_SCREENS_SETNX_T_TEXT_DELETE: u32 = 4013;
pub const EE_SCREENS_SETNX_T_COPY_TEXT_DELETE: u32 = 4014;

/// The shell kinds the radios are (`EE_SCREENS_SK_PAINT_TYPE`, `EE_SCREENS_SK_BRUSH`).
const SETNX_KIND_PAINT_TYPE: i32 = crate::screens::shells_editor::EE_SCREENS_SK_PAINT_TYPE as i32;
const SETNX_KIND_BRUSH: i32 = crate::screens::shells::EE_SCREENS_SK_BRUSH as i32;
/// The labels' and the type list's text colour (0x445... `SetnYellow`).
const SETNX_YELLOW: [f32; 4] = [0.99607843, 0.96862745, 0.1254902, 0.0];

/// The colour list p1 (from 100): its base `CopyData()`, `SetCopiedHasFocus(true)`, its copied lines' count, line
/// i0's colour count, its copy text rectangle into *p2 (a `GERectangle`), its line height; `new GERhombus`, rhombus
/// p1's `SetRectangle(*p2)`, rhombus p2's colour of the list p1's line i0's colour i1, the list p1's draw of
/// rhombus p2, rhombus p1 deleted; the list's base `DrawPolygons()`.
pub const EE_SCREENS_SETNX_C_BASE_COPY: u32 = 100;
pub const EE_SCREENS_SETNX_C_FOCUS_ON: u32 = 101;
pub const EE_SCREENS_SETNX_C_COPY_COUNT: u32 = 102;
pub const EE_SCREENS_SETNX_C_LINE_SIZE: u32 = 103;
pub const EE_SCREENS_SETNX_C_COPY_RECT: u32 = 104;
pub const EE_SCREENS_SETNX_C_LINE_HEIGHT: u32 = 105;
pub const EE_SCREENS_SETNX_RHOMBUS_NEW: u32 = 106;
pub const EE_SCREENS_SETNX_RHOMBUS_RECT: u32 = 107;
pub const EE_SCREENS_SETNX_C_RHOMBUS_COLOR: u32 = 108;
pub const EE_SCREENS_SETNX_C_DRAW_RHOMBUS: u32 = 109;
pub const EE_SCREENS_SETNX_RHOMBUS_DELETE: u32 = 110;
pub const EE_SCREENS_SETNX_C_BASE_DRAW: u32 = 111;
/// The type list p1: its copied lines' count, copied line i0 has a texture, its `Dereference()`, cleared,
/// `Reference()`; its base `CopyData()`, `*mCopyText = *mText`, the copied selection, `SetCopiedHasFocus(false)`,
/// the frame flag set to i0, the copied focus colour from the focus colour, the frame flag; `new GELine` of its
/// copied focus colour; its base `DrawPolygons()`; copied line i0's texture; its bitmap's `SetTexture(p2)`; its copy
/// text rectangle into *p2, its line height; its bitmap stretched over *p2 from f (left, top, right, bottom), at
/// *p2's top left, drawn; its copied selection's rectangle into *p2; line p1's start and end (f0, f1); the list's
/// draw of line p2; line p1 deleted; `DrawScrollBarText()`, `UIControl::DrawTextA()`, `GetDrawCount()`, its copy
/// text at (f0, f1), its text copied line i0's name, `DrawClippedText(*p2, its copy text, the list)`; its line i0
/// has a texture, `Dereference()`, set to p2, `Reference()`.
pub const EE_SCREENS_SETNX_T_COPY_COUNT: u32 = 120;
pub const EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE: u32 = 121;
pub const EE_SCREENS_SETNX_T_COPY_RELEASE: u32 = 122;
pub const EE_SCREENS_SETNX_T_COPY_CLEAR: u32 = 123;
pub const EE_SCREENS_SETNX_T_COPY_REFERENCE: u32 = 124;
pub const EE_SCREENS_SETNX_T_BASE_COPY: u32 = 125;
pub const EE_SCREENS_SETNX_T_COPY_TEXT: u32 = 126;
pub const EE_SCREENS_SETNX_T_HAS_SELECTION: u32 = 127;
pub const EE_SCREENS_SETNX_T_FOCUS_OFF: u32 = 128;
pub const EE_SCREENS_SETNX_T_SET_FRAMED: u32 = 129;
pub const EE_SCREENS_SETNX_T_FOCUS_COLOR: u32 = 130;
pub const EE_SCREENS_SETNX_T_FRAMED: u32 = 131;
pub const EE_SCREENS_SETNX_LINE_NEW: u32 = 132;
pub const EE_SCREENS_SETNX_T_BASE_DRAW: u32 = 133;
pub const EE_SCREENS_SETNX_T_COPY_TEXTURE: u32 = 134;
pub const EE_SCREENS_SETNX_T_BITMAP_TEXTURE: u32 = 135;
pub const EE_SCREENS_SETNX_T_COPY_RECT: u32 = 137;
pub const EE_SCREENS_SETNX_T_LINE_HEIGHT: u32 = 138;
pub const EE_SCREENS_SETNX_T_BITMAP_AT: u32 = 140;
pub const EE_SCREENS_SETNX_T_DRAW_BITMAP: u32 = 141;
pub const EE_SCREENS_SETNX_T_SELECTED_RECT: u32 = 142;
pub const EE_SCREENS_SETNX_LINE_START: u32 = 143;
pub const EE_SCREENS_SETNX_LINE_END: u32 = 144;
pub const EE_SCREENS_SETNX_T_DRAW_LINE: u32 = 145;
pub const EE_SCREENS_SETNX_LINE_DELETE: u32 = 146;
pub const EE_SCREENS_SETNX_T_SCROLL_TEXT: u32 = 147;
pub const EE_SCREENS_SETNX_T_CONTROL_TEXT: u32 = 148;
pub const EE_SCREENS_SETNX_T_DRAW_COUNT: u32 = 149;
pub const EE_SCREENS_SETNX_T_TEXT_AT: u32 = 150;
pub const EE_SCREENS_SETNX_T_TEXT_NAME: u32 = 151;
pub const EE_SCREENS_SETNX_T_DRAW_TEXT: u32 = 152;
pub const EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE: u32 = 153;
pub const EE_SCREENS_SETNX_T_DATA_RELEASE: u32 = 154;
pub const EE_SCREENS_SETNX_T_DATA_SET: u32 = 155;
pub const EE_SCREENS_SETNX_T_DATA_REFERENCE: u32 = 156;

/// The list boxes' members composed here (`ee_screens_setlb_call`): the colour list's `CopyData` and
/// `DrawPolygons`, the type list's `CopyData`, `DrawPolygons`, `DrawTextA`.
pub const EE_SCREENS_SETLB_COLOR_COPY: u32 = 1;
pub const EE_SCREENS_SETLB_COLOR_DRAW: u32 = 2;
pub const EE_SCREENS_SETLB_TYPE_COPY: u32 = 3;
pub const EE_SCREENS_SETLB_TYPE_DRAW: u32 = 4;
pub const EE_SCREENS_SETLB_TYPE_TEXT: u32 = 5;
/// The type list's destructor (`~EESEUITerrainTypeLB`, 0x1ac40d).
pub const EE_SCREENS_SETLB_TYPE_FREE: u32 = 4000;

/// `kErrorOK`.
const ERROR_OK: i32 = 0;
/// The load thread's terrain post that is 0x408 (the other 0x409).
const POST_408: i32 = 0x408;

/// One statement: `x(op, p, i, f)` with p the three handles; the answer, i and f as the C++ leaves them.
pub type Setnx<'a> = dyn FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 + 'a;

/// A statement on operands p0, p1, p2 with integers a and floats g.
fn raw(x: &mut Setnx, op: u32, p: [usize; 3], a: [i32; 4], g: [f32; 4]) -> i32 {
    let (mut a, mut g) = (a, g);
    x(op, p, &mut a, &mut g)
}

/// A statement on p1 (p2) with one integer.
fn on(x: &mut Setnx, op: u32, p1: usize, p2: usize, v: i32) -> i32 {
    raw(x, op, [0, p1, p2], [v, 0, 0, 0], [0.0; 4])
}

/// A making statement: the object the C++ writes into the out slot p0.
fn mk(x: &mut Setnx, op: u32, p1: usize, p2: usize, a: [i32; 4], g: [f32; 4]) -> usize {
    let mut out: usize = 0;
    raw(x, op, [&mut out as *mut usize as usize, p1, p2], a, g);
    out
}

/// The op's out array's entry `k`.
///
/// # Safety
/// `p0` is an array of at least `k + 1` pointers.
unsafe fn put(p0: usize, k: usize, v: usize) {
    unsafe { *(p0 as *mut usize).add(k) = v };
}

/// Entry `k` of a C++ array of pointers.
///
/// # Safety
/// `a` is an array of at least `k + 1` pointers.
unsafe fn at(a: usize, k: usize) -> usize {
    unsafe { *(a as *const usize).add(k) }
}

/// Radio p1's label: background 0.4, its text transparent and yellow (`SetnYellow`).
fn label(x: &mut Setnx, radio: usize, background: bool) {
    if background {
        on(x, EE_SCREENS_SETNX_LABEL_BG, radio, 0, 0);
    }
    on(x, EE_SCREENS_SETNX_LABEL_TRANSPARENT, radio, 0, 0);
    raw(x, EE_SCREENS_SETNX_LABEL_YELLOW, [0, radio, 0], [0; 4], SETNX_YELLOW);
}

/// `GERectangle(GE2DPoint(x, y), width, height)` as left, top, right, bottom (its constructor's sums).
fn sized(x: f32, y: f32, width: f32, height: f32) -> [f32; 4] {
    [x, y, x + (width - 1.0), y + (height - 1.0)]
}

/// A shell radio of `kind` with `args`, id, index and checked, at `rect`, on form p1 (the host's text).
fn radio(x: &mut Setnx, form: usize, kind: i32, args: &[i64], ints: [i32; 3], rect: [f32; 4]) -> usize {
    let mut block = vec![args.len() as i64];
    block.extend_from_slice(args);
    let i = [kind, ints[0], ints[1], ints[2]];
    mk(x, EE_SCREENS_SETNX_NEW_RADIO, form, block.as_ptr() as usize, i, rect)
}

/// The rectangle as the four floats a statement's f takes (left, top, right, bottom).
fn floats(r: &GeRectangle) -> [f32; 4] {
    [r.left, r.top, r.right, r.bottom]
}

/// The bitmap `bitmap` stretched: the texture rectangle `texture` (also the source), `destination`, on `screen`.
fn stretch(x: &mut Setnx, bitmap: usize, texture: GeRectangle, destination: GeRectangle, screen: i32) {
    let rects = [texture, texture, destination];
    raw(x, EE_SCREENS_SETNX_GE_STRETCH, [bitmap, rects.as_ptr() as usize, 0], [screen, 0, 0, 0], [0.0; 4]);
}

/// The terrain names' order (`EESETerrainNameLess`, 0x1aee71): string `first` before string `second`, compared
/// without case (`PlatMbsICmp`).
pub fn terrain_less(x: &mut Setnx, first: i32, second: i32) -> bool {
    let mut text = |id: i32| -> Vec<u8> {
        on(x, EE_SCREENS_SETNX_STRING, 0, 0, id);
        let bytes = mk(x, EE_SCREENS_SETNX_TEXT, 0, 0, [0; 4], [0.0; 4]);
        if bytes == 0 {
            return vec![0];
        }
        // SAFETY: the host's text, NUL-terminated, lives until the next string
        let mut v =
            unsafe { core::ffi::CStr::from_ptr(bytes as *const core::ffi::c_char) }.to_bytes_with_nul().to_vec();
        v.shrink_to_fit();
        v
    };
    let (a, b) = (text(first), text(second));
    // SAFETY: both NUL-terminated
    unsafe { ee_base::plat_core::string::PlatMbsICmp(a.as_ptr(), b.as_ptr()) < 0 }
}

/// Rust's own statements (`EE_SCREENS_SETNX_GE_*` on the bitmaps' Rust objects), `None` for the C++'s.
///
/// # Safety
/// The operands are the live objects the statement names.
pub unsafe fn ge_op(op: u32, p: [usize; 3], i: &[i32; 4]) -> Option<i32> {
    use ee_render::ge::bmpobj::{ee_ge_bmp_set_colors, ee_ge_bmp_stretch, EeGeBitmap};
    let bitmap = p[0] as *mut EeGeBitmap;
    match op {
        EE_SCREENS_SETNX_GE_STRETCH => {
            let r = p[1] as *const GeRectangle;
            unsafe { ee_ge_bmp_stretch(bitmap, r, r.add(1), r.add(2), i[0] as u32 as c_ulong) };
        }
        EE_SCREENS_SETNX_GE_COLORS => {
            // a GEColor is three floats
            let color = |k: i32| (p[1] as *mut [f32; 3]).wrapping_add(k as usize).cast::<core::ffi::c_void>();
            let quad = [color(i[0]), color(i[1]), color(i[2]), color(i[3])];
            unsafe { ee_ge_bmp_set_colors(bitmap, &quad) };
        }
        _ => return None,
    }
    Some(0)
}

/// The op composed, or `None` for the C++ host's.
///
/// # Safety
/// `p` are the op's handles as `ee_screenscpp_setn` takes them (its out slots and arrays live).
pub unsafe fn compose(x: &mut Setnx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let z = [0.0f32; 4];
    Some(match op {
        EE_SCREENS_SETN_OP_TABLES => {
            let out = p[0];
            let cap = i[1] as c_ulong;
            if i[0] == 0 {
                let mut count: c_ulong = 0;
                let n = on(x, EE_SCREENS_SETNX_TT_COUNT, 0, 0, 0) as u32 as c_ulong;
                for k in 0..n {
                    if on(x, EE_SCREENS_SETNX_TT_IN_EDITOR, 0, 0, k as i32) == 0 {
                        continue;
                    }
                    if out != 0 && count < cap {
                        let t = mk(x, EE_SCREENS_SETNX_TT_GET, 0, 0, [k as i32, 0, 0, 0], z);
                        // SAFETY: p0 the caller's array of i1 entries
                        unsafe { put(out, count as usize, t) };
                    }
                    count += 1;
                }
                return Some(count as i32);
            }
            on(x, EE_SCREENS_SETNX_T_CLEAR, 0, 0, 0);
            let n = on(x, EE_SCREENS_SETNX_T_COUNT, 0, 0, 0) as u32 as c_ulong;
            for k in 0..n {
                if on(x, EE_SCREENS_SETNX_T_IN_EDITOR, 0, 0, k as i32) != 0 {
                    on(x, EE_SCREENS_SETNX_T_PUSH, 0, 0, k as i32);
                }
            }
            if out == 0 {
                return Some(on(x, EE_SCREENS_SETNX_T_SIZE, 0, 0, 0));
            }
            on(x, EE_SCREENS_SETNX_T_SORT, 0, 0, 0);
            let size = on(x, EE_SCREENS_SETNX_T_SIZE, 0, 0, 0);
            let mut k: c_ulong = 0;
            while k < size as u32 as c_ulong && k < cap {
                let t = mk(x, EE_SCREENS_SETNX_T_AT, 0, 0, [k as i32, 0, 0, 0], z);
                // SAFETY: p0 the caller's array of i1 entries
                unsafe { put(out, k as usize, t) };
                k += 1;
            }
            size
        }
        EE_SCREENS_SETN_OP_TERRAIN => {
            let t = p[0];
            i[0] = on(x, EE_SCREENS_SETNX_T_ID, t, 0, 0);
            i[1] = on(x, EE_SCREENS_SETNX_T_COLOR_TYPE, t, 0, 0);
            i[2] = on(x, EE_SCREENS_SETNX_T_AMBIENTS, t, 0, 0);
            i[3] = on(x, EE_SCREENS_SETNX_T_CLASS_COUNT, t, 0, 0);
            if p[1] != 0 {
                on(x, EE_SCREENS_SETNX_T_PREVIEW, t, p[1], 0);
            }
            if on(x, EE_SCREENS_SETNX_T_CLASS_COUNT, t, 0, 0) != 0 {
                on(x, EE_SCREENS_SETNX_T_CLASS, t, 0, 0)
            } else {
                -1
            }
        }
        EE_SCREENS_SETN_OP_RADIO => {
            on(x, EE_SCREENS_SETNX_STRING, 0, 0, i[1]);
            let font = on(x, EE_SCREENS_SETNX_FONT_HEIGHT, 0, 0, 0) as f32;
            let on_height = {
                let mut g = [0.0f32; 4];
                x(EE_SCREENS_SETNX_RADIO_ON_HEIGHT, [0, p[1], 0], &mut [0; 4], &mut g);
                g[0]
            };
            let height = if font < on_height { on_height } else { font };
            let rect = sized(f[0], f[1], 125.0, height);
            let radio = radio(x, p[1], SETNX_KIND_PAINT_TYPE, &[], [i[0], i[2], i32::from(i[3] != 0)], rect);
            label(x, radio, true);
            let c = mk(x, EE_SCREENS_SETNX_RADIO_CONTROL, radio, 0, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, c) };
            0
        }
        EE_SCREENS_SETN_OP_TYPE_LIST => {
            let q = p[2];
            // SAFETY: q the op's three handles: the group, the terrains, their colour sets (i0 each)
            let (group, terrains, sets) = unsafe { (at(q, 0), at(q, 1), at(q, 2)) };
            on(x, EE_SCREENS_SETNX_LINES_NEW, 0, 0, i[0]);
            for k in 0..i[0].max(0) {
                on(x, EE_SCREENS_SETNX_LINE_NAME, terrains, 0, k);
                // SAFETY: the colour sets, i0 of them
                let set = unsafe { *(sets as *const i32).add(k as usize) };
                raw(x, EE_SCREENS_SETNX_LINE_COLOR_SET, [0; 3], [k, set, 0, 0], z);
            }
            let list = mk(x, EE_SCREENS_SETNX_TYPE_LIST_NEW, p[1], 0, [i[1], 0, 0, 0], sized(f[0], f[1], f[2], 85.0));
            let event = mk(x, EE_SCREENS_SETNX_TYPE_EVENT, group, 0, [0; 4], z);
            on(x, EE_SCREENS_SETNX_TYPE_TRIGGER, list, event, 0);
            on(x, EE_SCREENS_SETNX_EVENT_RELEASE, event, 0, 0);
            on(x, EE_SCREENS_SETNX_TYPE_TEXT_TRANSPARENT, list, 0, 0);
            raw(x, EE_SCREENS_SETNX_TYPE_TEXT_YELLOW, [0, list, 0], [0; 4], SETNX_YELLOW);
            on(x, EE_SCREENS_SETNX_TYPE_SELECTED_CLEAR, list, 0, 0);
            on(x, EE_SCREENS_SETNX_TYPE_BG, list, 0, 0);
            on(x, EE_SCREENS_SETNX_TYPE_ADD, p[1], list, 0);
            let c = mk(x, EE_SCREENS_SETNX_TYPE_CONTROL, list, 0, [0; 4], z);
            on(x, EE_SCREENS_SETNX_LINES_CLEAR, 0, 0, 0);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, c) };
            0
        }
        EE_SCREENS_SETN_OP_COLOR_LIST => {
            let q = p[2];
            // SAFETY: q the op's handles: the group, the colour type tables (i0 of them)
            let (group, tables) = unsafe { (at(q, 0), at(q, 1)) };
            on(x, EE_SCREENS_SETNX_COLORS_NEW, 0, 0, i[0]);
            for k in 0..i[0].max(0) {
                let first = on(x, EE_SCREENS_SETNX_TYPE_FIRST_COLOR, tables, 0, k);
                let mut color = first;
                while color <= on(x, EE_SCREENS_SETNX_TYPE_LAST_COLOR, tables, 0, k) {
                    raw(x, EE_SCREENS_SETNX_COLORS_PUSH, [0; 3], [k, color, 0, 0], z);
                    color += 1;
                }
            }
            let list = mk(x, EE_SCREENS_SETNX_COLOR_LIST_NEW, p[1], 0, [i[1], 0, 0, 0], sized(f[0], f[1], f[2], 85.0));
            let event = mk(x, EE_SCREENS_SETNX_COLOR_EVENT, group, 0, [0; 4], z);
            on(x, EE_SCREENS_SETNX_COLOR_TRIGGER, list, event, 0);
            on(x, EE_SCREENS_SETNX_EVENT_RELEASE, event, 0, 0);
            on(x, EE_SCREENS_SETNX_COLOR_SELECT, list, 0, i[2]);
            on(x, EE_SCREENS_SETNX_COLOR_SELECTED_CLEAR, list, 0, 0);
            on(x, EE_SCREENS_SETNX_COLOR_BG, list, 0, 0);
            on(x, EE_SCREENS_SETNX_COLOR_ADD, p[1], list, 0);
            let c = mk(x, EE_SCREENS_SETNX_COLOR_CONTROL, list, 0, [0; 4], z);
            on(x, EE_SCREENS_SETNX_COLORS_CLEAR, 0, 0, 0);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, c) };
            0
        }
        EE_SCREENS_SETN_OP_PAINT => {
            // SAFETY: p2 the op's three handles, the shell's arguments
            let args = unsafe { [3, at(p[2], 0) as i64, at(p[2], 1) as i64, at(p[2], 2) as i64] };
            let rect = sized(0.0, 0.0, 1024.0, 606.0);
            let t = mk(x, EE_SCREENS_SETNX_PAINT_NEW, p[1], args.as_ptr() as usize, [i[0], 0, 0, 0], rect);
            on(x, EE_SCREENS_SETNX_ADD_CONTROL, p[1], t, 0);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            0
        }
        EE_SCREENS_SETN_OP_BITMAP => {
            let bitmap = mk(x, EE_SCREENS_SETNX_NEW_BITMAP, 0, 0, [0; 4], z);
            let t = mk(x, EE_SCREENS_SETNX_UIBITMAP_NEW, p[1], bitmap, [i[0], 0, 0, 0], sized(f[0], f[1], 64.0, 64.0));
            on(x, EE_SCREENS_SETNX_BITMAP_RELEASE, bitmap, 0, 0);
            on(x, EE_SCREENS_SETNX_UIBITMAP_ADD, p[1], t, 0);
            let c = mk(x, EE_SCREENS_SETNX_UIBITMAP_CONTROL, t, 0, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, c) };
            0
        }
        EE_SCREENS_SETN_OP_PREVIEWS => {
            let s = if i[0] != 0 { EE_SCREENS_SETNX_LOAD_PREVIEWS } else { EE_SCREENS_SETNX_RELEASE_PREVIEWS };
            on(x, s, 0, 0, 0);
            0
        }
        EE_SCREENS_SETN_OP_SELECTION => {
            let mut s: c_ulong = 0;
            let which = if i[0] != 0 { EE_SCREENS_SETNX_COLORS_SELECTION } else { EE_SCREENS_SETNX_TYPES_SELECTION };
            if on(x, which, p[0], &mut s as *mut c_ulong as usize, 0) != ERROR_OK {
                return Some(-1);
            }
            s as i32
        }
        EE_SCREENS_SETN_OP_CLASS_BUTTON => {
            if on(x, EE_SCREENS_SETNX_CLASS_EXISTS, 0, 0, i[0]) == 0 {
                return Some(0);
            }
            let t = mk(x, EE_SCREENS_SETNX_CLASS_BUTTON_TEXTURE, 0, 0, [i[0], 0, 0, 0], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, t) };
            1
        }
        EE_SCREENS_SETN_OP_SHOW_PREVIEW => {
            let texture = mk(x, EE_SCREENS_SETNX_LINE_TEXTURE, p[0], 0, [i[0], 0, 0, 0], z);
            raw(x, EE_SCREENS_SETNX_PREVIEW_TEXTURE, [p[0], p[1], texture], [0; 4], z);
            let w = on(x, EE_SCREENS_SETNX_TEXTURE_WIDTH, texture, 0, 0) as u32 as f32;
            let h = on(x, EE_SCREENS_SETNX_TEXTURE_HEIGHT, texture, 0, 0) as u32 as f32;
            let bitmap = mk(x, EE_SCREENS_SETNX_PREVIEW_BITMAP, p[1], 0, [0; 4], z);
            let preview = rect_of(x, EE_SCREENS_SETNX_PREVIEW_RECT, p[1]);
            let screen = on(x, EE_SCREENS_SETNX_SCREEN, p[0], 0, 0);
            stretch(x, bitmap, preview, GeRectangle { left: 0.0, top: 0.0, right: w, bottom: h }, screen);
            0
        }
        EE_SCREENS_SETN_OP_SET_TEXTURE => {
            let (list, texture, line) = (p[0], p[1], i[0]);
            if on(x, EE_SCREENS_SETNX_T_DATA_HAS_TEXTURE, list, 0, line) != 0 {
                on(x, EE_SCREENS_SETNX_T_DATA_RELEASE, list, 0, line);
            }
            on(x, EE_SCREENS_SETNX_T_DATA_SET, list, texture, line);
            if texture != 0 {
                on(x, EE_SCREENS_SETNX_T_DATA_REFERENCE, list, 0, line);
            }
            0
        }
        EE_SCREENS_SETN_OP_TINT => {
            let colors = mk(x, EE_SCREENS_SETNX_LINE_COLORS, p[0], 0, [i[0], 0, 0, 0], z);
            let bitmap = mk(x, EE_SCREENS_SETNX_PREVIEW_BITMAP, p[1], 0, [0; 4], z);
            raw(x, EE_SCREENS_SETNX_GE_COLORS, [bitmap, colors, 0], [0, i[1], i[2], i[3]], z);
            0
        }
        EE_SCREENS_SETN_OP_POST => {
            let s = if i[0] == POST_408 { EE_SCREENS_SETNX_POST_408 } else { EE_SCREENS_SETNX_POST_409 };
            on(x, s, p[0], 0, i[1]);
            0
        }
        EE_SCREENS_SETN_OP_PAINT_ACTION => {
            if i[3] >= 0 {
                on(x, EE_SCREENS_SETNX_PAINT_CENTERED, p[0], 0, (i[3] != 0) as i32);
            }
            let action = match i[0] {
                0 => mk(x, EE_SCREENS_SETNX_PAINT_TERRAIN, 0, 0, [i[1], i[2], 0, 0], z),
                1 => mk(x, EE_SCREENS_SETNX_PAINT_BULLDOZER, 0, 0, [0; 4], z),
                _ => 0,
            };
            // SAFETY: p1 the op's out slot
            unsafe { put(p[1], 0, action) };
            0
        }
        EE_SCREENS_SETN_OP_BRUSH => {
            on(x, EE_SCREENS_SETNX_STRING, 0, 0, i[2]);
            let args = [p[2] as i64, i64::from(i[0])];
            let rect = sized(f[0], f[1], 100.0, 20.0);
            let radio = radio(x, p[1], SETNX_KIND_BRUSH, &args, [i[1], i[3], i32::from(i[0] == 0)], rect);
            label(x, radio, false);
            let c = mk(x, EE_SCREENS_SETNX_RADIO_CONTROL, radio, 0, [0; 4], z);
            // SAFETY: p0 the op's out slot
            unsafe { put(p[0], 0, c) };
            0
        }
        _ => return None,
    })
}

/// A rectangle the C++ writes into (`*p2`).
fn rect_of(x: &mut Setnx, op: u32, list: usize) -> GeRectangle {
    let mut r = GeRectangle::default();
    raw(x, op, [0, list, &mut r as *mut GeRectangle as usize], [0; 4], [0.0; 4]);
    r
}

/// A statement on the list p1 with the rectangle `r` at p2.
fn with_rect(x: &mut Setnx, op: u32, list: usize, r: &GeRectangle, g: [f32; 4]) -> i32 {
    raw(x, op, [0, list, r as *const GeRectangle as usize], [0; 4], g)
}

/// `EESEUITerrainColorLB::DrawPolygons`: each copied line as a row of colour swatches clipped to the list, then
/// the list itself.
fn color_draw(x: &mut Setnx, list: usize) -> i32 {
    let z = [0.0f32; 4];
    let rhombus = mk(x, EE_SCREENS_SETNX_RHOMBUS_NEW, 0, 0, [0; 4], z);
    let mut line = GeRectangle::default();
    let lines = on(x, EE_SCREENS_SETNX_C_COPY_COUNT, list, 0, 0) as u32 as c_ulong;
    for i in 0..lines {
        let copy = rect_of(x, EE_SCREENS_SETNX_C_COPY_RECT, list);
        let height = on(x, EE_SCREENS_SETNX_C_LINE_HEIGHT, list, 0, 0) as u32 as c_ulong;
        line.top = copy.top + height.wrapping_mul(i) as f32;
        line.bottom = line.top + height as f32 - 1.0;
        let colors = on(x, EE_SCREENS_SETNX_C_LINE_SIZE, list, 0, i as i32) as u32 as c_ulong;
        let width = ((copy.right - copy.left) / colors as f32) as c_ulong;
        let mut offset: c_ulong = 0;
        for j in 0..colors {
            line.left = copy.left + offset as f32;
            line.right = line.left + width as f32 - 1.0;
            let Some(mut swatch) = line.and(&copy) else {
                break;
            };
            swatch.right += 1.0;
            swatch.bottom += 1.0;
            raw(x, EE_SCREENS_SETNX_RHOMBUS_RECT, [0, rhombus, &swatch as *const GeRectangle as usize], [0; 4], z);
            raw(x, EE_SCREENS_SETNX_C_RHOMBUS_COLOR, [0, list, rhombus], [i as i32, j as i32, 0, 0], z);
            on(x, EE_SCREENS_SETNX_C_DRAW_RHOMBUS, list, rhombus, 0);
            offset = offset.wrapping_add(width);
        }
    }
    let r = on(x, EE_SCREENS_SETNX_C_BASE_DRAW, list, 0, 0);
    on(x, EE_SCREENS_SETNX_RHOMBUS_DELETE, rhombus, 0, 0);
    r
}

/// `EESEUITerrainTypeLB::CopyData`: the copied lines' textures released, the base copy, the text copied, the
/// selection framed (the focus colour copied) or not, the new copy's textures referenced.
fn type_copy(x: &mut Setnx, list: usize) -> i32 {
    let n = on(x, EE_SCREENS_SETNX_T_COPY_COUNT, list, 0, 0);
    for k in 0..n {
        if on(x, EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE, list, 0, k) != 0 {
            on(x, EE_SCREENS_SETNX_T_COPY_RELEASE, list, 0, k);
            on(x, EE_SCREENS_SETNX_T_COPY_CLEAR, list, 0, k);
        }
    }
    let r = on(x, EE_SCREENS_SETNX_T_BASE_COPY, list, 0, 0);
    on(x, EE_SCREENS_SETNX_T_COPY_TEXT, list, 0, 0);
    if on(x, EE_SCREENS_SETNX_T_HAS_SELECTION, list, 0, 0) != 0 {
        on(x, EE_SCREENS_SETNX_T_FOCUS_OFF, list, 0, 0);
        on(x, EE_SCREENS_SETNX_T_SET_FRAMED, list, 0, 1);
        on(x, EE_SCREENS_SETNX_T_FOCUS_COLOR, list, 0, 0);
    } else {
        on(x, EE_SCREENS_SETNX_T_SET_FRAMED, list, 0, 0);
    }
    let n = on(x, EE_SCREENS_SETNX_T_COPY_COUNT, list, 0, 0);
    for k in 0..n {
        if on(x, EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE, list, 0, k) != 0 {
            on(x, EE_SCREENS_SETNX_T_COPY_REFERENCE, list, 0, k);
        }
    }
    r
}

/// `EESEUITerrainTypeLB::DrawPolygons`: the list, each copied line's 16x16 preview clipped to it, the frame of
/// the selected line.
fn type_draw(x: &mut Setnx, list: usize) -> i32 {
    let z = [0.0f32; 4];
    let line = mk(x, EE_SCREENS_SETNX_LINE_NEW, list, 0, [0; 4], z);
    let mut r = on(x, EE_SCREENS_SETNX_T_BASE_DRAW, list, 0, 0);
    let mut dest = GeRectangle::default();
    let n = on(x, EE_SCREENS_SETNX_T_COPY_COUNT, list, 0, 0) as u32 as c_ulong;
    for i in 0..n {
        let texture = mk(x, EE_SCREENS_SETNX_T_COPY_TEXTURE, list, 0, [i as i32, 0, 0, 0], z);
        if texture == 0 {
            continue;
        }
        on(x, EE_SCREENS_SETNX_T_BITMAP_TEXTURE, list, texture, 0);
        let w = on(x, EE_SCREENS_SETNX_TEXTURE_WIDTH, texture, 0, 0) as u32 as f32;
        let h = on(x, EE_SCREENS_SETNX_TEXTURE_HEIGHT, texture, 0, 0) as u32 as f32;
        let copy = rect_of(x, EE_SCREENS_SETNX_T_COPY_RECT, list);
        dest.left = copy.left;
        let height = on(x, EE_SCREENS_SETNX_T_LINE_HEIGHT, list, 0, 0) as u32 as c_ulong;
        dest.top = copy.top + height.wrapping_mul(i) as f32;
        dest.right = dest.left + 16.0 - 1.0;
        dest.bottom = dest.top + 16.0 - 1.0;
        if !dest.and_in_place(&copy) {
            continue;
        }
        let bitmap = mk(x, EE_SCREENS_SETNX_T_BITMAP, list, 0, [0; 4], z);
        let screen = on(x, EE_SCREENS_SETNX_SCREEN, list, 0, 0);
        stretch(x, bitmap, dest, GeRectangle { left: 0.0, top: 0.0, right: w, bottom: h }, screen);
        with_rect(x, EE_SCREENS_SETNX_T_BITMAP_AT, list, &dest, z);
        r = on(x, EE_SCREENS_SETNX_T_DRAW_BITMAP, list, 0, 0);
    }
    if on(x, EE_SCREENS_SETNX_T_FRAMED, list, 0, 0) != 0 {
        dest = rect_of(x, EE_SCREENS_SETNX_T_SELECTED_RECT, list);
        let copy = rect_of(x, EE_SCREENS_SETNX_T_COPY_RECT, list);
        if dest.and_in_place(&copy) {
            let (tl, tr) = ([dest.left, dest.top], [dest.right, dest.top]);
            let (bl, br) = ([dest.left, dest.bottom], [dest.right, dest.bottom]);
            let point = |x: &mut Setnx, op: u32, q: [f32; 2]| {
                raw(x, op, [0, line, 0], [0; 4], [q[0], q[1], 0.0, 0.0]);
            };
            point(x, EE_SCREENS_SETNX_LINE_START, tl);
            point(x, EE_SCREENS_SETNX_LINE_END, tr);
            on(x, EE_SCREENS_SETNX_T_DRAW_LINE, list, line, 0);
            point(x, EE_SCREENS_SETNX_LINE_END, bl);
            on(x, EE_SCREENS_SETNX_T_DRAW_LINE, list, line, 0);
            point(x, EE_SCREENS_SETNX_LINE_START, br);
            on(x, EE_SCREENS_SETNX_T_DRAW_LINE, list, line, 0);
            point(x, EE_SCREENS_SETNX_LINE_END, tr);
            r = on(x, EE_SCREENS_SETNX_T_DRAW_LINE, list, line, 0);
        }
    }
    on(x, EE_SCREENS_SETNX_LINE_DELETE, line, 0, 0);
    r
}

/// `EESEUITerrainTypeLB::DrawTextA`: the visible names right of their previews; the last draw's result.
fn type_text(x: &mut Setnx, list: usize) -> i32 {
    let height = on(x, EE_SCREENS_SETNX_T_LINE_HEIGHT, list, 0, 0) as u32 as c_ulong;
    on(x, EE_SCREENS_SETNX_T_SCROLL_TEXT, list, 0, 0);
    on(x, EE_SCREENS_SETNX_T_CONTROL_TEXT, list, 0, 0);
    let mut rect = rect_of(x, EE_SCREENS_SETNX_T_COPY_RECT, list);
    rect.right += 1.0;
    rect.left += 16.0;
    let mut r = 0;
    let mut offset: c_ulong = 0;
    let mut i: c_ulong = 0;
    while i < on(x, EE_SCREENS_SETNX_T_DRAW_COUNT, list, 0, 0) as u32 as c_ulong {
        let copy = rect_of(x, EE_SCREENS_SETNX_T_COPY_RECT, list);
        rect.top = copy.top + offset as f32;
        rect.bottom = rect.top + height as f32;
        raw(x, EE_SCREENS_SETNX_T_TEXT_AT, [0, list, 0], [0; 4], [rect.left, rect.top, 0.0, 0.0]);
        on(x, EE_SCREENS_SETNX_T_TEXT_NAME, list, 0, i as i32);
        r = raw(x, EE_SCREENS_SETNX_T_DRAW_TEXT, [0, list, 0], [0; 4], floats(&rect));
        offset = offset.wrapping_add(height);
        i += 1;
    }
    r
}

/// A list box member composed (`EE_SCREENS_SETLB_*` on the list `list`): its answer.
pub fn compose_list(x: &mut Setnx, which: u32, list: usize) -> i32 {
    match which {
        EE_SCREENS_SETLB_COLOR_COPY => {
            let r = on(x, EE_SCREENS_SETNX_C_BASE_COPY, list, 0, 0);
            on(x, EE_SCREENS_SETNX_C_FOCUS_ON, list, 0, 0);
            r
        }
        EE_SCREENS_SETLB_COLOR_DRAW => color_draw(x, list),
        EE_SCREENS_SETLB_TYPE_COPY => type_copy(x, list),
        EE_SCREENS_SETLB_TYPE_DRAW => type_draw(x, list),
        EE_SCREENS_SETLB_TYPE_TEXT => type_text(x, list),
        EE_SCREENS_SETLB_TYPE_FREE => {
            // the copied lines (the original walks +0xb0, mCopyLineArray), then the bitmap and the texts
            let n = on(x, EE_SCREENS_SETNX_T_COPY_COUNT, list, 0, 0) as u32;
            for k in 0..n {
                if on(x, EE_SCREENS_SETNX_T_COPY_HAS_TEXTURE, list, 0, k as i32) != 0 {
                    on(x, EE_SCREENS_SETNX_T_COPY_RELEASE, list, 0, k as i32);
                }
            }
            on(x, EE_SCREENS_SETNX_T_BITMAP_RELEASE, list, 0, 0);
            on(x, EE_SCREENS_SETNX_T_TEXT_DELETE, list, 0, 0);
            on(x, EE_SCREENS_SETNX_T_COPY_TEXT_DELETE, list, 0, 0);
            0
        }
        _ => 0,
    }
}

#[cfg(test)]
#[path = "editor_setnx_tests.rs"]
mod tests;
