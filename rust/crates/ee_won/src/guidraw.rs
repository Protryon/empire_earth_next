//! WONGUI's drawing as plans the C++ executes: StretchImage's nine-slice placement (the filter's corner copies,
//! side and centre stretches and tiles) and its skip-centre clips, StretchCenterImage's centre, WONFrame's size and
//! place (the combo box's and the scroll container's frames, the scroll container's corner), Background's fill,
//! border, tiles and component, WONButton's and WONSimpleButton's state image and text place, WONHeaderButton's
//! sort arrow, Label's text place, WrappedText's lines, ListArea's and MultiListArea's row and column passes,
//! MultiListItem's and TextListItem's rows, InputBox's text, selection and caret, TextArea's lines, TextSegment's
//! and TextImage's selection.
//!
//! Each plan is the original's statements with every Graphics call (and every call into a component, image or
//! text object) replaced by an operation appended to a list, in the original's order. The C++ executes the list
//! (`WONGUIDraw.h`): the Surface, PlatGraphics, image and font calls stay C++. Where the original measured text
//! in the middle of its statements, the plan asks the C++ through a callback at the same point, so the font sees
//! the same calls. A plan's numbers are what the C++ read before the call; nothing the plan reads changes while
//! the list runs (the calls in between are draws and paints of other objects).
//!
//! Ints wrap (the game builds with -fwrapv); a division by zero is the original's fault (a tiled image of width
//! 0 loops without end in both).

use std::ffi::c_void;

/// One draw operation (`kind` and up to eight arguments; their meaning is the kind's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonDrawOp {
    pub kind: i32,
    pub a: i32,
    pub b: i32,
    pub c: i32,
    pub d: i32,
    pub e: i32,
    pub f: i32,
    pub g: i32,
    pub h: i32,
}

/// `Graphics::SetColor(a)` (the colour's bits).
pub const EE_WON_DRAW_SET_COLOR: i32 = 1;
/// `Graphics::SetSysColor(a)`.
pub const EE_WON_DRAW_SET_SYS_COLOR: i32 = 2;
/// `Graphics::FillRect(a, b, c, d)`.
pub const EE_WON_DRAW_FILL_RECT: i32 = 3;
/// `Graphics::DrawRect(a, b, c, d)`.
pub const EE_WON_DRAW_RECT: i32 = 4;
/// `Graphics::DrawDottedRect(a, b, c, d)`.
pub const EE_WON_DRAW_DOTTED_RECT: i32 = 5;
/// `Graphics::DrawLine(a, b, c, d)`.
pub const EE_WON_DRAW_LINE: i32 = 6;
/// `Graphics::DrawImage(image a, b, c)`.
pub const EE_WON_DRAW_IMAGE: i32 = 7;
/// `image a->SetSize(b, c)`.
pub const EE_WON_DRAW_IMAGE_SIZE: i32 = 8;
/// `Graphics::PushClipRect(a, b, c, d)`.
pub const EE_WON_DRAW_PUSH_CLIP: i32 = 9;
/// `Graphics::PopClipRect()`.
pub const EE_WON_DRAW_POP_CLIP: i32 = 10;
/// `Graphics::Translate(a, b)`.
pub const EE_WON_DRAW_TRANSLATE: i32 = 11;
/// `Graphics::SetFont(font a)`.
pub const EE_WON_DRAW_SET_FONT: i32 = 12;
/// `Graphics::DrawString(string a, b, c, d, e)` (offset, length, x, y; the offset and length as `unsigned short`).
pub const EE_WON_DRAW_STRING: i32 = 13;
/// `Graphics::DrawString(string a, b, c)` (the whole string at x, y).
pub const EE_WON_DRAW_STRING_ALL: i32 = 14;
/// `wrapped text a->Paint(g, b, c)`.
pub const EE_WON_DRAW_TEXT: i32 = 15;
/// A call the plan's caller makes (`a`: which, per plan; `b`..: its arguments).
pub const EE_WON_DRAW_CALL: i32 = 16;
/// The stretch filter's `CopyRawImage(a ? dst : src, dst, b, c, d, e, f, g)` (dest x, y, source x, y, width, height).
pub const EE_WON_DRAW_COPY_PIXELS: i32 = 20;
/// The stretch filter's `RawStretch(src, dst, a, b, c, d, e, f, g, h)` (x1, x2, y1, y2, tile left, top, width,
/// height).
pub const EE_WON_DRAW_STRETCH_PIXELS: i32 = 21;

/// `StandardColor_3DFace`, `_3DHilight`, `_GrayText` (WONLobbyGUI.h's enum).
const SYS_3D_FACE: i32 = 1;
const SYS_3D_HILIGHT: i32 = 2;
const SYS_GRAY_TEXT: i32 = 6;

/// Text measures the C++ answers (the original's font calls at the points the plan reaches them):
/// `string_width(ctx, offset, length)` is `Font::GetStringWidth(text, offset, length)`, `char_width(ctx, index)`
/// is `Font::GetCharacterWidth(text[index])`, or of 'M' for a negative index.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonTextMeasure {
    pub ctx: *mut c_void,
    pub string_width: Option<unsafe extern "C" fn(*mut c_void, i32, i32) -> i32>,
    pub char_width: Option<unsafe extern "C" fn(*mut c_void, i32) -> i32>,
}

impl EeWonTextMeasure {
    fn string(&self, offset: i32, length: i32) -> i32 {
        match self.string_width {
            // SAFETY: the C++ passes a function and its context valid for this call
            Some(f) => unsafe { f(self.ctx, offset, length) },
            None => 0,
        }
    }
    fn char(&self, index: i32) -> i32 {
        match self.char_width {
            // SAFETY: as above
            Some(f) => unsafe { f(self.ctx, index) },
            None => 0,
        }
    }
}

/// A selection's colours (`SelectionColor`: background, foreground and the foreground's mode).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonSelColor {
    pub background: u32,
    pub foreground: u32,
    pub mode: i32,
}

/// `SelectionColor::GetForegroundColor`: the original colour, its inverse, the set foreground, else white.
pub fn sel_foreground(sel: &EeWonSelColor, original: u32) -> u32 {
    match sel.mode {
        0 => original,
        1 => original ^ 0xffffff,
        2 => sel.foreground,
        _ => 0xffffff,
    }
}

#[derive(Default)]
struct Plan {
    ops: Vec<EeWonDrawOp>,
}

impl Plan {
    fn op(&mut self, kind: i32, args: &[i32]) {
        let mut o = EeWonDrawOp { kind, ..Default::default() };
        let slots = [&mut o.a, &mut o.b, &mut o.c, &mut o.d, &mut o.e, &mut o.f, &mut o.g, &mut o.h];
        for (s, v) in slots.into_iter().zip(args) {
            *s = *v;
        }
        self.ops.push(o);
    }
    fn color(&mut self, c: u32) {
        self.op(EE_WON_DRAW_SET_COLOR, &[c as i32]);
    }
    fn sys_color(&mut self, c: i32) {
        self.op(EE_WON_DRAW_SET_SYS_COLOR, &[c]);
    }
    fn fill(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.op(EE_WON_DRAW_FILL_RECT, &[x, y, w, h]);
    }
    fn line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        self.op(EE_WON_DRAW_LINE, &[x1, y1, x2, y2]);
    }
    fn image(&mut self, slot: i32, x: i32, y: i32) {
        self.op(EE_WON_DRAW_IMAGE, &[slot, x, y]);
    }
    fn image_size(&mut self, slot: i32, w: i32, h: i32) {
        self.op(EE_WON_DRAW_IMAGE_SIZE, &[slot, w, h]);
    }
    fn push_clip(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.op(EE_WON_DRAW_PUSH_CLIP, &[x, y, w, h]);
    }
    fn pop_clip(&mut self) {
        self.op(EE_WON_DRAW_POP_CLIP, &[]);
    }
    fn translate(&mut self, x: i32, y: i32) {
        self.op(EE_WON_DRAW_TRANSLATE, &[x, y]);
    }
    fn font(&mut self, slot: i32) {
        self.op(EE_WON_DRAW_SET_FONT, &[slot]);
    }
    fn string(&mut self, slot: i32, offset: i32, length: i32, x: i32, y: i32) {
        self.op(EE_WON_DRAW_STRING, &[slot, offset, length, x, y]);
    }
    fn string_all(&mut self, slot: i32, x: i32, y: i32) {
        self.op(EE_WON_DRAW_STRING_ALL, &[slot, x, y]);
    }
    fn text(&mut self, slot: i32, x: i32, y: i32) {
        self.op(EE_WON_DRAW_TEXT, &[slot, x, y]);
    }
    fn call(&mut self, args: &[i32]) {
        self.op(EE_WON_DRAW_CALL, args);
    }
}

/// Copies a plan into the caller's buffer (as much as fits) and returns its whole length: a caller whose buffer
/// was too small asks again with one of that length (the plans are pure).
fn deliver(ops: &[EeWonDrawOp], out: *mut EeWonDrawOp, cap: usize) -> usize {
    if !out.is_null() {
        let n = ops.len().min(cap);
        // SAFETY: the caller gives `cap` writable operations at `out`
        unsafe { std::ptr::copy_nonoverlapping(ops.as_ptr(), out, n) };
    }
    ops.len()
}

// ---- StretchImageFilter (StretchImage.cpp DoFilter): the nine pieces

/// The filter's inputs (`StretchImageFilter`'s members and the source image's size).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonStretchFilter {
    pub width: i32,
    pub height: i32,
    pub left: i32,
    pub right: i32,
    pub top: i32,
    pub bottom: i32,
    pub src_width: i32,
    pub src_height: i32,
    pub tile_horz: bool,
    pub tile_vert: bool,
    pub skip_center: bool,
}

#[allow(clippy::too_many_arguments)]
fn copy_px(p: &mut Plan, from_dst: bool, dx: i32, dy: i32, x: i32, y: i32, w: i32, h: i32) {
    p.op(EE_WON_DRAW_COPY_PIXELS, &[from_dst as i32, dx, dy, x, y, w, h]);
}

#[allow(clippy::too_many_arguments)]
fn stretch_px(p: &mut Plan, x1: i32, x2: i32, y1: i32, y2: i32, tl: i32, tt: i32, tw: i32, th: i32) {
    p.op(EE_WON_DRAW_STRETCH_PIXELS, &[x1, x2, y1, y2, tl, tt, tw, th]);
}

#[allow(clippy::too_many_arguments)]
fn tile_horz(p: &mut Plan, from_dst: bool, x1: i32, x2: i32, y: i32, tl: i32, tt: i32, tw: i32, th: i32) {
    let end_x = x2.wrapping_sub(tw);
    let mut x = x1;
    while x < end_x {
        copy_px(p, from_dst, x, y, tl, tt, tw, th);
        x = x.wrapping_add(tw);
    }
    let final_width = x2.wrapping_sub(x);
    if final_width > 0 {
        copy_px(p, from_dst, x, y, tl, tt, final_width, th);
    }
}

#[allow(clippy::too_many_arguments)]
fn tile_vert(p: &mut Plan, from_dst: bool, y1: i32, y2: i32, x: i32, tl: i32, tt: i32, tw: i32, th: i32) {
    let end_y = y2.wrapping_sub(th);
    let mut y = y1;
    while y < end_y {
        copy_px(p, from_dst, x, y, tl, tt, tw, th);
        y = y.wrapping_add(th);
    }
    let final_height = y2.wrapping_sub(y);
    if final_height > 0 {
        copy_px(p, from_dst, x, y, tl, tt, tw, final_height);
    }
}

#[allow(clippy::too_many_arguments)]
fn tile_horz_vert(p: &mut Plan, x1: i32, x2: i32, y1: i32, y2: i32, tl: i32, tt: i32, tw: i32, th: i32) {
    let end_y = y2.wrapping_sub(th);
    let mut y = y1;
    while y < end_y {
        tile_horz(p, false, x1, x2, y, tl, tt, tw, th);
        y = y.wrapping_add(th);
    }
    let final_height = y2.wrapping_sub(y);
    if final_height > 0 {
        tile_horz(p, false, x1, x2, y, tl, tt, tw, final_height);
    }
}

/// `DoFilter`: the new image's size and the pixel operations in the original's order (the centre, the four sides,
/// the four corners).
pub fn stretch_filter(f: &EeWonStretchFilter) -> (i32, i32, Vec<EeWonDrawOp>) {
    let mut p = Plan::default();
    let (lw, rw, th, bh) = (f.left, f.right, f.top, f.bottom);
    let mut width = f.width;
    let mut height = f.height;
    if bh.wrapping_add(th) > height {
        height = bh.wrapping_add(th);
    }
    if rw.wrapping_add(lw) > width {
        width = rw.wrapping_add(lw);
    }
    let (image_left, image_top) = (0i32, 0i32);
    let image_right = f.src_width.wrapping_sub(rw);
    let image_bottom = f.src_height.wrapping_sub(bh);
    let (left, top) = (0i32, 0i32);
    let right = left.wrapping_add(width).wrapping_sub(rw);
    let bottom = top.wrapping_add(height).wrapping_sub(bh);
    let center_width = width.wrapping_sub(rw).wrapping_sub(lw);
    let center_height = height.wrapping_sub(bh).wrapping_sub(th);
    let icw = f.src_width.wrapping_sub(rw).wrapping_sub(lw);
    let ich = f.src_height.wrapping_sub(bh).wrapping_sub(th);
    let min_cw = if center_width < icw { center_width } else { icw };
    let min_ch = if center_height < ich { center_height } else { ich };
    let (cl, ct) = (left.wrapping_add(lw), top.wrapping_add(th));
    let (il, it) = (image_left.wrapping_add(lw), image_top.wrapping_add(th));

    if !f.skip_center && ich > 0 && icw > 0 {
        if !f.tile_horz && !f.tile_vert {
            stretch_px(&mut p, cl, right, ct, bottom, il, it, icw, ich);
        } else if f.tile_horz && f.tile_vert {
            tile_horz_vert(&mut p, cl, right, ct, bottom, il, it, icw, ich);
        } else if f.tile_vert {
            stretch_px(&mut p, cl, right, ct, ct.wrapping_add(min_ch), il, it, icw, ich);
            tile_vert(&mut p, true, ct.wrapping_add(ich), bottom, cl, cl, ct, center_width, ich);
        } else {
            stretch_px(&mut p, cl, cl.wrapping_add(min_cw), ct, bottom, il, it, icw, ich);
            tile_horz(&mut p, true, cl.wrapping_add(icw), right, ct, cl, ct, icw, center_height);
        }
    }
    if lw > 0 && ich > 0 {
        if f.tile_vert {
            tile_vert(&mut p, false, ct, bottom, left, image_left, it, lw, ich);
        } else {
            stretch_px(&mut p, left, left.wrapping_add(lw), ct, bottom, image_left, it, lw, ich);
        }
    }
    if rw > 0 && ich > 0 {
        if f.tile_vert {
            tile_vert(&mut p, false, ct, bottom, right, image_right, it, rw, ich);
        } else {
            stretch_px(&mut p, right, right.wrapping_add(rw), ct, bottom, image_right, it, rw, ich);
        }
    }
    if th > 0 && icw > 0 {
        if f.tile_horz {
            tile_horz(&mut p, false, cl, right, top, il, image_top, icw, th);
        } else {
            stretch_px(&mut p, cl, right, top, top.wrapping_add(th), il, image_top, icw, th);
        }
    }
    if bh > 0 && icw > 0 {
        if f.tile_horz {
            tile_horz(&mut p, false, cl, right, bottom, il, image_bottom, icw, bh);
        } else {
            stretch_px(&mut p, cl, right, bottom, bottom.wrapping_add(bh), il, image_bottom, icw, bh);
        }
    }
    if lw > 0 && th > 0 {
        copy_px(&mut p, false, left, top, image_left, image_top, lw, th);
    }
    if rw > 0 && th > 0 {
        copy_px(&mut p, false, right, top, image_right, image_top, rw, th);
    }
    if lw > 0 && bh > 0 {
        copy_px(&mut p, false, left, bottom, image_left, image_bottom, lw, bh);
    }
    if rw > 0 && bh > 0 {
        copy_px(&mut p, false, right, bottom, image_right, image_bottom, rw, bh);
    }
    (width, height, p.ops)
}

/// `StretchImageFilter::Filter`'s placement: the new image's size, then the pixel operations.
///
/// # Safety
/// `f`, `out_width` and `out_height` are valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_stretch_filter(
    f: *const EeWonStretchFilter,
    out_width: *mut i32,
    out_height: *mut i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let (w, h, ops) = stretch_filter(&*f);
    *out_width = w;
    *out_height = h;
    deliver(&ops, out, cap)
}

// ---- StretchImage::Paint, StretchCenterImage::Paint, WONFrame::Paint

/// `StretchImage::Paint` after the cached image is current: the whole image (slot 0), or with skip-centre only the
/// four border strips, each through its clip.
pub fn stretch_paint(
    skip_center: bool,
    left: i32,
    right: i32,
    top: i32,
    bottom: i32,
    width: i32,
    height: i32,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if skip_center {
        if left > 0 {
            p.push_clip(0, 0, left, height);
            p.image(0, 0, 0);
            p.pop_clip();
        }
        if right > 0 {
            p.push_clip(width.wrapping_sub(right), 0, right, height);
            p.image(0, 0, 0);
            p.pop_clip();
        }
        if top > 0 {
            p.push_clip(0, 0, width, top);
            p.image(0, 0, 0);
            p.pop_clip();
        }
        if bottom > 0 {
            p.push_clip(0, height.wrapping_sub(bottom), width, bottom);
            p.image(0, 0, 0);
            p.pop_clip();
        }
    } else {
        p.image(0, 0, 0);
    }
    p.ops
}

/// # Safety
/// `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_stretch_paint(
    skip_center: bool,
    left: i32,
    right: i32,
    top: i32,
    bottom: i32,
    width: i32,
    height: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&stretch_paint(skip_center, left, right, top, bottom, width, height), out, cap)
}

/// `StretchCenterImage::Paint`: the stretch image (slot 0), then the centre image (slot 1) centred when it fits.
pub fn stretch_center_paint(width: i32, height: i32, center_width: i32, center_height: i32) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    p.image(0, 0, 0);
    if center_width <= width && center_height <= height {
        p.image(1, width.wrapping_sub(center_width) / 2, height.wrapping_sub(center_height) / 2);
    }
    p.ops
}

/// # Safety
/// `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_stretch_center(
    width: i32,
    height: i32,
    center_width: i32,
    center_height: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&stretch_center_paint(width, height, center_width, center_height), out, cap)
}

/// A frame to paint (`WONFrame::Paint`'s inputs): its image's size (slot `slot`), the component's size and the
/// paddings left out of the frame.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonFrame {
    pub has_image: bool,
    pub image_width: i32,
    pub image_height: i32,
    pub width: i32,
    pub height: i32,
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

fn frame_into(p: &mut Plan, slot: i32, f: &EeWonFrame) {
    if !f.has_image {
        return;
    }
    let w = f.width.wrapping_sub(f.left).wrapping_sub(f.right);
    let h = f.height.wrapping_sub(f.top).wrapping_sub(f.bottom);
    if f.image_width != w || f.image_height != h {
        p.image_size(slot, w, h);
    }
    p.image(slot, f.left, f.top);
}

/// `WONFrame::Paint` (both: the full size is the one with no paddings): the image (slot 0) sized to the component
/// less the paddings, drawn inside them.
pub fn frame_paint(f: &EeWonFrame) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    frame_into(&mut p, 0, f);
    p.ops
}

/// # Safety
/// `f` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_frame(f: *const EeWonFrame, out: *mut EeWonDrawOp, cap: usize) -> usize {
    deliver(&frame_paint(&*f), out, cap)
}

/// `WONComboBox::Paint` after `ComboBox::Paint`: no frame unless drawn; the whole frame when the button sits in it
/// or is hidden, else the frame less the button's width on the right. The frame's paddings are set here.
pub fn combo_frame(
    draw_frame: bool,
    button_in_frame: bool,
    button_visible: bool,
    button_width: i32,
    f: &EeWonFrame,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if !draw_frame {
        return p.ops;
    }
    let mut g = *f;
    g.left = 0;
    g.top = 0;
    g.bottom = 0;
    g.right = if button_in_frame || !button_visible { 0 } else { button_width };
    frame_into(&mut p, 0, &g);
    p.ops
}

/// # Safety
/// `f` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_combo_frame(
    draw_frame: bool,
    button_in_frame: bool,
    button_visible: bool,
    button_width: i32,
    f: *const EeWonFrame,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&combo_frame(draw_frame, button_in_frame, button_visible, button_width, &*f), out, cap)
}

/// `WONScrollContainer::Paint` after `Container::Paint`: the corner image (slot 1) under the vertical bar when both
/// bars show, the container is opaque and has one, then the frame (slot 0, full size).
#[allow(clippy::too_many_arguments)]
pub fn scroll_container(
    vert_visible: bool,
    horz_visible: bool,
    transparent: bool,
    has_corner: bool,
    vert_left: i32,
    vert_bottom: i32,
    f: &EeWonFrame,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if vert_visible && horz_visible && !transparent && has_corner {
        p.image(1, vert_left, vert_bottom);
    }
    let mut g = *f;
    g.left = 0;
    g.top = 0;
    g.right = 0;
    g.bottom = 0;
    frame_into(&mut p, 0, &g);
    p.ops
}

/// # Safety
/// `f` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_scroll_container(
    vert_visible: bool,
    horz_visible: bool,
    transparent: bool,
    has_corner: bool,
    vert_left: i32,
    vert_bottom: i32,
    f: *const EeWonFrame,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(
        &scroll_container(vert_visible, horz_visible, transparent, has_corner, vert_left, vert_bottom, &*f),
        out,
        cap,
    )
}

// ---- Background::Paint

/// A background to paint (`Background`'s members, its image's size and the rectangle and offsets).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonBackground {
    pub has_image: bool,
    pub stretch_image: bool,
    pub use_offsets: bool,
    pub has_component: bool,
    pub image_width: i32,
    pub image_height: i32,
    pub color: i32,
    pub border_color: i32,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub x_offset: i32,
    pub y_offset: i32,
}

/// `Background::Paint`: the stretched image (slot 0) alone, or the fill, the border, the image tiled from the
/// offsets inside a clip, and the component (call 0: its size, invalidation and paint), all translated.
pub fn background(b: &EeWonBackground) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if b.stretch_image && b.has_image {
        if b.image_width != b.width || b.image_height != b.height {
            p.image_size(0, b.width, b.height);
        }
        p.image(0, b.x, b.y);
        return p.ops;
    }
    p.translate(b.x, b.y);
    if b.color != -1 {
        p.color(b.color as u32);
        p.fill(0, 0, b.width, b.height);
    }
    if b.border_color != -1 {
        p.color(b.border_color as u32);
        p.op(EE_WON_DRAW_RECT, &[0, 0, b.width, b.height]);
    }
    if b.has_image {
        p.push_clip(0, 0, b.width, b.height);
        let start_x = if b.use_offsets { b.x_offset.wrapping_rem(b.image_width).wrapping_neg() } else { 0 };
        let start_y = if b.use_offsets { b.y_offset.wrapping_rem(b.image_height).wrapping_neg() } else { 0 };
        let mut y = start_y;
        while y < b.height {
            let mut x = start_x;
            while x < b.width {
                p.image(0, x, y);
                x = x.wrapping_add(b.image_width);
            }
            y = y.wrapping_add(b.image_height);
        }
        p.pop_clip();
    }
    if b.has_component {
        p.call(&[0, b.width, b.height]);
    }
    p.translate(b.x.wrapping_neg(), b.y.wrapping_neg());
    p.ops
}

/// # Safety
/// `b` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_background(
    b: *const EeWonBackground,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&background(&*b), out, cap)
}

// ---- WONButton::Paint and DrawButtonText, WONSimpleButton::Paint, WONHeaderButton's arrow

/// A WONButton to paint: its size, flags and members, and its state's image (slot 0) and text (slot 0).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonButtonPaint {
    pub width: i32,
    pub height: i32,
    pub transparent: bool,
    pub scale_image: bool,
    pub has_image: bool,
    pub has_text: bool,
    pub pressed: bool,
    pub disabled: bool,
    pub draw_gray_text: bool,
    /// the focus style is set, the button has the focus and its parent has used the dialog keys
    pub focus_shown: bool,
    pub h_align: i32,
    pub v_align: i32,
    pub image_width: i32,
    pub image_height: i32,
    pub text_width: i32,
    pub text_height: i32,
    pub color: i32,
    pub left_text_pad: i32,
    pub top_text_pad: i32,
    pub right_text_pad: i32,
    pub bottom_text_pad: i32,
    pub left_focus_pad: i32,
    pub top_focus_pad: i32,
    pub right_focus_pad: i32,
    pub bottom_focus_pad: i32,
    pub push_dx: i32,
    pub push_dy: i32,
}

fn button_text(p: &mut Plan, b: &EeWonButtonPaint) {
    if !b.has_text {
        return;
    }
    let (w, h) = (b.width, b.height);
    let mut x;
    let mut y;
    if b.scale_image {
        // an alignment outside the enum left x unset in the original; here it is 0
        x = match b.h_align {
            0 => b.left_text_pad,
            1 => (w.wrapping_sub(b.text_width).wrapping_sub(b.left_text_pad).wrapping_sub(b.right_text_pad) / 2)
                .wrapping_add(b.left_text_pad),
            2 => w.wrapping_sub(b.text_width).wrapping_sub(b.right_text_pad),
            _ => 0,
        };
        y = h.wrapping_sub(b.text_height) / 2;
        if b.pressed {
            x = x.wrapping_add(b.push_dx);
            y = y.wrapping_add(b.push_dy);
        }
    } else {
        x = b.left_text_pad;
        y = h.wrapping_sub(b.text_height) / 2;
    }
    if b.disabled && b.draw_gray_text {
        p.sys_color(SYS_3D_HILIGHT);
        p.text(0, x.wrapping_add(1), y.wrapping_add(1));
        p.sys_color(SYS_GRAY_TEXT);
        p.text(0, x, y);
    } else {
        p.color(b.color as u32);
        p.text(0, x, y);
        if b.focus_shown {
            if b.scale_image {
                p.op(
                    EE_WON_DRAW_DOTTED_RECT,
                    &[
                        b.left_focus_pad,
                        b.top_focus_pad,
                        w.wrapping_sub(b.left_focus_pad).wrapping_sub(b.right_focus_pad),
                        h.wrapping_sub(b.top_focus_pad).wrapping_sub(b.bottom_focus_pad),
                    ],
                );
            } else {
                p.op(
                    EE_WON_DRAW_DOTTED_RECT,
                    &[
                        b.left_text_pad.wrapping_sub(1),
                        y.wrapping_sub(1),
                        b.text_width.wrapping_add(2),
                        b.text_height.wrapping_add(2),
                    ],
                );
            }
        }
    }
}

/// `WONButton::Paint` after `Button::Paint`: the face unless transparent, the state image scaled to the button or
/// placed by the vertical alignment, then `DrawButtonText`.
pub fn button(b: &EeWonButtonPaint) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if !b.transparent {
        p.sys_color(SYS_3D_FACE);
        p.fill(0, 0, b.width, b.height);
    }
    if b.scale_image {
        if b.has_image {
            if b.image_width != b.width || b.image_height != b.height {
                p.image_size(0, b.width, b.height);
            }
            p.image(0, 0, 0);
        }
    } else if b.has_image {
        let mut y = 0i32;
        let rest = b.height.wrapping_sub(b.image_height).wrapping_sub(b.top_text_pad).wrapping_sub(b.bottom_text_pad);
        if b.v_align == 1 {
            y = rest / 2;
        } else if b.v_align == 2 {
            y = rest;
        }
        p.image(0, 0, y.wrapping_add(b.top_text_pad));
    }
    button_text(&mut p, b);
    p.ops
}

/// `WONButton::DrawButtonText` alone (the text, slot 0, of `b`).
pub fn button_text_paint(b: &EeWonButtonPaint) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    button_text(&mut p, b);
    p.ops
}

/// # Safety
/// `b` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_button_text(
    b: *const EeWonButtonPaint,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&button_text_paint(&*b), out, cap)
}

/// # Safety
/// `b` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_button(
    b: *const EeWonButtonPaint,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&button(&*b), out, cap)
}

/// `WONSimpleButton::Paint`'s image: disabled (2), pressed (1, by the mouse-down state when the button checks it),
/// mouse-over (3), the normal one (0) when that state has none.
pub fn simple_button_image(
    disabled: bool,
    check_mouse_down: bool,
    mouse_down_state: u8,
    pressed: bool,
    mouse_over: bool,
    has: [bool; 4],
) -> i32 {
    let mut slot = -1;
    if disabled {
        slot = 2;
    } else if (check_mouse_down && mouse_down_state != 0) || (!check_mouse_down && pressed) {
        slot = 1;
    } else if mouse_over {
        slot = 3;
    }
    if slot < 0 || !has[slot as usize] {
        slot = 0;
    }
    slot
}

/// A WONSimpleButton to paint (its four images' presence and sizes, slots 0..3).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonSimpleButtonPaint {
    pub disabled: bool,
    pub check_mouse_down: bool,
    pub pressed: bool,
    pub mouse_over: bool,
    pub scale_image: bool,
    pub mouse_down_state: u8,
    pub has_image: [bool; 4],
    pub image_width: [i32; 4],
    pub image_height: [i32; 4],
    pub width: i32,
    pub height: i32,
}

/// `WONSimpleButton::Paint` after `Button::Paint`: the state's image, sized to the button when scaled.
pub fn simple_button(b: &EeWonSimpleButtonPaint) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let s =
        simple_button_image(b.disabled, b.check_mouse_down, b.mouse_down_state, b.pressed, b.mouse_over, b.has_image);
    let k = s as usize;
    if b.scale_image && (b.image_width[k] != b.width || b.image_height[k] != b.height) {
        p.image_size(s, b.width, b.height);
    }
    p.image(s, 0, 0);
    p.ops
}

/// # Safety
/// `b` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_simple_button(
    b: *const EeWonSimpleButtonPaint,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&simple_button(&*b), out, cap)
}

/// `WONHeaderButton::Paint` after `WONButton::Paint`: the sort arrow (`sort`: ListSortType, 0 ascending,
/// 1 descending, else none) after the state's text, one pixel down and right while pressed.
#[allow(clippy::too_many_arguments)]
pub fn header_arrow(
    sort: i32,
    color: i32,
    has_text: bool,
    text_width: i32,
    left_text_pad: i32,
    width: i32,
    height: i32,
    pressed: bool,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if sort == 2 {
        return p.ops;
    }
    p.color(color as u32);
    let mut x = if !has_text || text_width == 0 {
        width.wrapping_sub(4) / 2
    } else {
        text_width.wrapping_add(10).wrapping_add(left_text_pad)
    };
    let mut y = (height / 2).wrapping_sub(2);
    let offset = pressed as i32;
    x = x.wrapping_add(offset);
    y = y.wrapping_add(offset);
    if sort == 0 {
        for i in 0..4 {
            p.line(x.wrapping_sub(i), y.wrapping_add(i), x.wrapping_add(i), y.wrapping_add(i));
        }
    } else if sort == 1 {
        for i in 0..4 {
            p.line(x.wrapping_sub(3 - i), y.wrapping_add(i), x.wrapping_add(3 - i), y.wrapping_add(i));
        }
    }
    p.ops
}

/// # Safety
/// `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_header_arrow(
    sort: i32,
    color: i32,
    has_text: bool,
    text_width: i32,
    left_text_pad: i32,
    width: i32,
    height: i32,
    pressed: bool,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&header_arrow(sort, color, has_text, text_width, left_text_pad, width, height, pressed), out, cap)
}

// ---- Label::Paint, WrappedText::Paint

/// `Label::Paint` after the background: the colour and the wrapped text (slot 0) aligned in the padded area.
#[allow(clippy::too_many_arguments)]
pub fn label(
    width: i32,
    height: i32,
    pads: [i32; 4],
    h_align: i32,
    v_align: i32,
    text_width: i32,
    text_height: i32,
    color: i32,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let [left, top, right, bottom] = pads;
    let w = width.wrapping_sub(right).wrapping_sub(left);
    let h = height.wrapping_sub(bottom).wrapping_sub(top);
    let mut x = match h_align {
        1 => w.wrapping_sub(text_width) / 2,
        2 => w.wrapping_sub(text_width),
        _ => 0,
    };
    let mut y = match v_align {
        1 => h.wrapping_sub(text_height) / 2,
        2 => h.wrapping_sub(text_height),
        _ => 0,
    };
    x = x.wrapping_add(left);
    y = y.wrapping_add(top);
    p.color(color as u32);
    p.text(0, x, y);
    p.ops
}

/// # Safety
/// `pads` holds four ints (left, top, right, bottom); `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_label(
    width: i32,
    height: i32,
    pads: *const i32,
    h_align: i32,
    v_align: i32,
    text_width: i32,
    text_height: i32,
    color: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let pads = [*pads, *pads.add(1), *pads.add(2), *pads.add(3)];
    deliver(&label(width, height, pads, h_align, v_align, text_width, text_height, color), out, cap)
}

/// One line break of a wrapped text (`WrappedText::BreakInfo`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonLineBreak {
    pub break_pos: u16,
    pub next_start: u16,
    pub width: u16,
}

/// `WrappedText::Paint`: the font (slot 0), each broken line (string 0) from the top, centred when set, then the
/// rest after the last break (centred by its measured width when there were breaks).
#[allow(clippy::too_many_arguments)]
pub fn wrapped_text(
    has_font: bool,
    length: i32,
    centered: bool,
    width: i32,
    font_height: i32,
    breaks: &[EeWonLineBreak],
    x: i32,
    y: i32,
    m: &EeWonTextMeasure,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if !has_font {
        return p.ops;
    }
    p.font(0);
    let mut ypos = y;
    let mut pos = 0i32;
    for b in breaks {
        if pos >= length {
            break;
        }
        let bp = b.break_pos as i32;
        let mut xpos = x;
        if centered {
            xpos = xpos.wrapping_add(width.wrapping_sub(b.width as i32) / 2);
        }
        p.string(0, pos, bp.wrapping_sub(pos), xpos, ypos);
        ypos = ypos.wrapping_add(font_height);
        pos = b.next_start as i32;
    }
    if pos >= length {
        return p.ops;
    }
    if centered && !breaks.is_empty() {
        let w = m.string(pos, length.wrapping_sub(pos));
        let xpos = x.wrapping_add(width.wrapping_sub(w) / 2);
        p.string(0, pos, length.wrapping_sub(pos), xpos, ypos);
    } else {
        p.string(0, pos, length.wrapping_sub(pos), x, ypos);
    }
    p.ops
}

/// # Safety
/// `breaks` holds `count` breaks; `m` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_wrapped_text(
    has_font: bool,
    length: i32,
    centered: bool,
    width: i32,
    font_height: i32,
    breaks: *const EeWonLineBreak,
    count: usize,
    x: i32,
    y: i32,
    m: *const EeWonTextMeasure,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let breaks = if count == 0 { &[][..] } else { std::slice::from_raw_parts(breaks, count) };
    deliver(&wrapped_text(has_font, length, centered, width, font_height, breaks, x, y, &*m), out, cap)
}

// ---- ListArea::ScrollPaint, MultiListArea::ScrollPaint, MultiListItem::Paint, TextListItem::Paint

/// A row's or line's place, as the list callback answers it (`top`, `height`; a text line's `x` too).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonRowPlace {
    pub x: i32,
    pub top: i32,
    pub height: i32,
}

/// Answers the place of row `index` (`ctx` is the C++ caller's).
pub type EeWonRowOf = Option<unsafe extern "C" fn(ctx: *const c_void, index: usize) -> EeWonRowPlace>;

#[allow(clippy::too_many_arguments)]
fn rows_paint(
    p: &mut Plan,
    first: usize,
    count: usize,
    offset: i32,
    view_height: i32,
    partial: bool,
    with_x: bool,
    row_of: &dyn Fn(usize) -> EeWonRowPlace,
) {
    let mut i = first;
    while i < count {
        let r = row_of(i);
        if !crate::guilist::ee_won_gui_list_visible(r.top, r.height, offset, view_height, partial) {
            break;
        }
        let x = if with_x { r.x } else { 0 };
        p.translate(x, r.top);
        p.call(&[0, i as i32]);
        p.translate(x.wrapping_neg(), r.top.wrapping_neg());
        i += 1;
    }
}

/// `ListArea::ScrollPaint`: the paint width to set while the rows paint (the width already set, else the wider of
/// the total and the view), then each visible row from the top line down, translated to its top (call 0: paint
/// row b). The C++ sets the paint width back to -1 after the list.
#[allow(clippy::too_many_arguments)]
pub fn list_paint(
    total_width: i32,
    view_width: i32,
    paint_width: i32,
    first: usize,
    count: usize,
    offset: i32,
    view_height: i32,
    partial: bool,
    row_of: &dyn Fn(usize) -> EeWonRowPlace,
) -> (i32, Vec<EeWonDrawOp>) {
    let mut p = Plan::default();
    let mut width = total_width;
    if view_width > width {
        width = view_width;
    }
    let pw = if paint_width == -1 { width } else { paint_width };
    rows_paint(&mut p, first, count, offset, view_height, partial, false, row_of);
    (pw, p.ops)
}

/// # Safety
/// `row_of` answers every index below `count` for `ctx`; `paint_width_out` is valid; `out` has room for `cap`
/// operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_list(
    total_width: i32,
    view_width: i32,
    paint_width: i32,
    first: usize,
    count: usize,
    offset: i32,
    view_height: i32,
    partial: bool,
    row_of: EeWonRowOf,
    ctx: *const c_void,
    paint_width_out: *mut i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let Some(f) = row_of else { return 0 };
    let (pw, ops) =
        list_paint(total_width, view_width, paint_width, first, count, offset, view_height, partial, &|i| f(ctx, i));
    *paint_width_out = pw;
    deliver(&ops, out, cap)
}

/// `TextArea::ScrollPaint`: each visible line from the top line down, translated to its place (call 0: paint the
/// segments of line b).
pub fn text_lines(
    first: usize,
    count: usize,
    offset: i32,
    view_height: i32,
    partial: bool,
    row_of: &dyn Fn(usize) -> EeWonRowPlace,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    rows_paint(&mut p, first, count, offset, view_height, partial, true, row_of);
    p.ops
}

/// # Safety
/// As `ee_won_gui_draw_list`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_text_lines(
    first: usize,
    count: usize,
    offset: i32,
    view_height: i32,
    partial: bool,
    row_of: EeWonRowOf,
    ctx: *const c_void,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let Some(f) = row_of else { return 0 };
    deliver(&text_lines(first, count, offset, view_height, partial, &|i| f(ctx, i)), out, cap)
}

/// `MultiListArea::ScrollPaint`: the rows once over all columns (call 0: column -1, paint width the total; call 1:
/// the list's pass), then once per column, clipped to it and translated to its x (call 0: column b, paint width
/// its width less the border).
pub fn multi_list(columns: &[i32], border: i32, total_width: i32, offset: i32, view_height: i32) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let mut x = 0i32;
    p.call(&[0, -1, total_width]);
    p.call(&[1]);
    for (i, w) in columns.iter().enumerate() {
        let pw = w.wrapping_sub(border);
        p.call(&[0, i as i32, pw]);
        p.push_clip(x, offset, pw, view_height);
        p.translate(x, 0);
        p.call(&[1]);
        p.translate(x.wrapping_neg(), 0);
        p.pop_clip();
        x = x.wrapping_add(*w);
    }
    p.ops
}

/// # Safety
/// `columns` holds `count` widths; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_multi_list(
    columns: *const i32,
    count: usize,
    border: i32,
    total_width: i32,
    offset: i32,
    view_height: i32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    let columns = if count == 0 { &[][..] } else { std::slice::from_raw_parts(columns, count) };
    deliver(&multi_list(columns, border, total_width, offset, view_height), out, cap)
}

/// A multi-column row to paint (`MultiListItem::Paint`'s inputs; `has_cell` and `cell_width` are the column's
/// cell when the column is one of the row's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonMultiRowPaint {
    pub column: i32,
    pub selected: bool,
    pub full_row_select: bool,
    pub has_cell: bool,
    pub cell_count: u32,
    pub selection_color: u32,
    pub total_width: i32,
    pub height: i32,
    pub align: u8,
    pub paint_width: i32,
    pub cell_width: i32,
}

/// `MultiListItem::Paint`: the selection bar in the all-columns pass (only with full-row selection), else the
/// column's cell (call 0: the cell's height b and selection c; call 1: paint the cell) at its alignment's offset.
pub fn multi_row(r: &EeWonMultiRowPaint) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if r.column < 0 && r.selected {
        if r.full_row_select {
            p.color(r.selection_color);
            p.fill(0, 0, r.total_width, r.height);
        }
        return p.ops;
    }
    if r.column as u32 >= r.cell_count || !r.has_cell {
        return p.ops;
    }
    let sel = r.selected && (r.full_row_select || r.column == 0);
    p.call(&[0, r.height, sel as i32]);
    let mut offset = 0i32;
    if r.align == 1 {
        offset = r.paint_width.wrapping_sub(r.cell_width) / 2;
    } else if r.align == 2 {
        offset = r.paint_width.wrapping_sub(r.cell_width);
    }
    if offset < 0 {
        offset = 0;
    }
    p.translate(offset, 0);
    p.call(&[1]);
    p.translate(offset.wrapping_neg(), 0);
    p.ops
}

/// # Safety
/// `r` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_multi_row(
    r: *const EeWonMultiRowPaint,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&multi_row(&*r), out, cap)
}

/// A text row to paint (`TextListItem::Paint`'s inputs; image slot 0, font slot 0, string slot 0).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonTextRowPaint {
    pub selected: bool,
    pub full_row_select: bool,
    pub no_image_select: bool,
    pub has_image: bool,
    pub sel: EeWonSelColor,
    pub color: u32,
    pub paint_width: i32,
    pub width: i32,
    pub height: i32,
    pub image_width: i32,
    pub image_height: i32,
    pub font_height: i32,
}

/// `TextListItem::Paint`: the selection behind a selected row (right of the image unless flag 0x20), the image
/// centred, the text after it centred on the row.
pub fn text_row(r: &EeWonTextRowPaint) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let mut color = r.color;
    if r.selected {
        p.color(r.sel.background);
        let mut w = r.paint_width;
        if !r.full_row_select {
            w = r.width;
        }
        let mut x = 0i32;
        if r.has_image && !r.no_image_select {
            x = r.image_width.wrapping_add(2);
            w = w.wrapping_sub(r.image_width.wrapping_add(2));
        }
        p.fill(x, 0, w, r.height);
        color = sel_foreground(&r.sel, r.color);
    }
    p.color(color);
    p.font(0);
    let mut text_x = 2i32;
    if r.has_image {
        let mut iy = r.height.wrapping_sub(r.image_height) / 2;
        if iy < 0 {
            iy = 0;
        }
        p.image(0, 2, iy);
        text_x = r.image_width.wrapping_add(4);
    }
    p.string_all(0, text_x, r.height.wrapping_sub(r.font_height) / 2);
    p.ops
}

/// # Safety
/// `r` is valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_text_row(
    r: *const EeWonTextRowPaint,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&text_row(&*r), out, cap)
}

// ---- InputBox::Paint

/// An input box to paint (its members; icon slot 0, font slot 0, string slot 0 the shown text).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonInputPaint {
    pub disabled_look: bool,
    pub has_icon: bool,
    pub has_focus: bool,
    pub cursor_on: bool,
    pub underline_cursor: bool,
    pub backwards_sel: bool,
    pub width: i32,
    pub height: i32,
    pub horz_offset: i32,
    pub text_color: i32,
    pub disabled_text_color: i32,
    pub icon_height: i32,
    pub text_x: i32,
    pub text_y: i32,
    pub text_width: i32,
    pub text_height: i32,
    pub sel_start: i32,
    pub sel_end: i32,
    pub length: i32,
    pub font_height: i32,
    pub sel: EeWonSelColor,
}

/// `InputBox::Paint` after `Component::Paint`: the background (call 0: b 1 for the disabled one, then the width,
/// height and offset), the icon, the text scrolled in its clipped rectangle, the selection over it in the selection
/// colours with the caret at its moving end, or the caret alone (a bar or an underline as wide as the character
/// under it, or an 'M').
pub fn input_box(b: &EeWonInputPaint, m: &EeWonTextMeasure) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let color = if b.disabled_look {
        p.call(&[0, 1, b.width, b.height, b.horz_offset]);
        b.disabled_text_color
    } else {
        p.call(&[0, 0, b.width, b.height, b.horz_offset]);
        b.text_color
    };
    if b.has_icon {
        p.image(0, 4, b.height.wrapping_sub(b.icon_height) / 2);
    }
    p.translate(b.text_x, b.text_y);
    p.push_clip(0, 0, b.text_width, b.text_height);
    p.translate(b.horz_offset.wrapping_neg(), 0);
    p.font(0);
    p.color(color as u32);
    p.string_all(0, 0, 0);
    let ch = b.font_height;
    if b.has_focus {
        if b.sel_start != b.sel_end {
            let x = m.string(0, b.sel_start);
            let w = m.string(b.sel_start, b.sel_end.wrapping_sub(b.sel_start));
            p.color(b.sel.background);
            p.fill(x, 0, w, ch);
            p.color(sel_foreground(&b.sel, color as u32));
            p.push_clip(x, 0, w, ch);
            p.string_all(0, 0, 0);
            p.pop_clip();
            if b.cursor_on && !b.underline_cursor {
                if b.backwards_sel {
                    p.line(x, 0, x, ch);
                } else {
                    let e = x.wrapping_add(w).wrapping_sub(1);
                    p.line(e, 0, e, ch);
                }
            }
        } else if b.cursor_on {
            let pos = b.sel_start;
            let xp = m.string(0, pos);
            if b.underline_cursor {
                let w = if pos < b.length { m.char(pos) } else { m.char(-1) };
                p.line(xp, ch.wrapping_sub(1), xp.wrapping_add(w).wrapping_sub(1), ch.wrapping_sub(1));
            } else {
                p.line(xp, 0, xp, ch);
            }
        }
    }
    p.translate(b.horz_offset, 0);
    p.pop_clip();
    p.translate(b.text_x.wrapping_neg(), b.text_y.wrapping_neg());
    p.ops
}

/// # Safety
/// `b` and `m` are valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_input_box(
    b: *const EeWonInputPaint,
    m: *const EeWonTextMeasure,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&input_box(&*b, &*m), out, cap)
}

// ---- TextSegment::Paint, TextImage::Paint

/// A text segment's range to paint (`TextSegment::Paint`'s inputs; font slot 0, string slot 0 the segment's text).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonSegmentPaint {
    pub area_sel_start: i32,
    pub area_sel_end: i32,
    pub abs_start: i32,
    pub start: i32,
    pub end: i32,
    pub x: i32,
    pub y: i32,
    pub line_height: i32,
    pub color: u32,
    pub sel: EeWonSelColor,
}

/// `TextSegment::Paint` (a range): the text unless the selection covers the whole range, then the selected part
/// over its background in the selection colours.
pub fn segment(s: &EeWonSegmentPaint, m: &EeWonTextMeasure) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    let mut ss = s.area_sel_start.wrapping_sub(s.abs_start);
    let mut se = s.area_sel_end.wrapping_sub(s.abs_start);
    if s.start > ss {
        ss = s.start;
    }
    if s.end < se {
        se = s.end;
    }
    p.color(s.color);
    p.font(0);
    let full = ss == s.start && se == s.end;
    if !full {
        p.string(0, s.start, s.end.wrapping_sub(s.start), s.x, s.y);
    }
    if se <= ss {
        return p.ops;
    }
    let x = s.x.wrapping_add(m.string(s.start, ss.wrapping_sub(s.start)));
    let w = m.string(ss, se.wrapping_sub(ss));
    p.color(s.sel.background);
    p.fill(x, 0, w, s.line_height);
    p.color(sel_foreground(&s.sel, s.color));
    p.string(0, ss, se.wrapping_sub(ss), x, s.y);
    p.ops
}

/// # Safety
/// `s` and `m` are valid; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_draw_segment(
    s: *const EeWonSegmentPaint,
    m: *const EeWonTextMeasure,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(&segment(&*s, &*m), out, cap)
}

/// `TextImage::Paint`: the selection background when the selection covers the image, then the image (slot 0).
#[allow(clippy::too_many_arguments)]
pub fn text_image(
    area_sel_start: i32,
    area_sel_end: i32,
    abs_start: i32,
    abs_end: i32,
    x: i32,
    y: i32,
    width: i32,
    line_height: i32,
    sel_background: u32,
) -> Vec<EeWonDrawOp> {
    let mut p = Plan::default();
    if area_sel_start <= abs_start && area_sel_end >= abs_end {
        p.color(sel_background);
        p.fill(x, 0, width, line_height);
    }
    p.image(0, x, y);
    p.ops
}

/// # Safety
/// `out` has room for `cap` operations (or is NULL).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_draw_text_image(
    area_sel_start: i32,
    area_sel_end: i32,
    abs_start: i32,
    abs_end: i32,
    x: i32,
    y: i32,
    width: i32,
    line_height: i32,
    sel_background: u32,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    deliver(
        &text_image(area_sel_start, area_sel_end, abs_start, abs_end, x, y, width, line_height, sel_background),
        out,
        cap,
    )
}

/// `SelectionColor::GetForegroundColor`.
#[no_mangle]
pub extern "C" fn ee_won_gui_sel_foreground(background: u32, foreground: u32, mode: i32, original: u32) -> u32 {
    sel_foreground(&EeWonSelColor { background, foreground, mode }, original)
}

#[cfg(test)]
mod tests;
