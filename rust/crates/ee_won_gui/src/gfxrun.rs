//! The game's Graphics as a Rust record, and the draw plans run on it (R6 WON GUI round 38).
//!
//! The C++ `Graphics` begins with this record's members (`EeWonGraphics`: the translation, colour and clip of
//! [`EeWonGfxState`], the clip stack, the borrowed colour scheme and the surface; checked in Graphics.cpp), so a
//! `Graphics*` is the record. Its methods are [`ee_won_graphics_call`] over gfxobj.rs and gfxplat.rs, the scheme's 17
//! colours read in place from the C++ `ColorScheme` (after its RefCount, at the offset the game registers: 16 under
//! the MSVC ABI, 12 under the Itanium one). A draw plan's
//! operations (guidraw.rs, mspaint.rs) run here ([`ee_won_graphics_draw`]: WONGUIDraw.h's WONDrawOne until this
//! round); only an image's draw and size, the font and the wrapped text go to the game's one host function (the C++
//! objects' calls, Graphics.cpp), registered once.
//!
//! In the game the GUI objects' plain draws and the scheme's save and restore on a Graphics are answered here
//! ([`game_op`], through gamewin.rs common_op): the C++ hosts' cases went. The drivers' worlds have Graphics of
//! their own and keep their ops.
use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::gfxobj::{EeWonGfx, EeWonGfxCall, EeWonGfxHook as H, EeWonGfxHost, EeWonGfxState};
use crate::gfxplat::{ee_won_gfx_plat_call, EeWonGfxPlat};
use crate::guidraw::{
    EeWonDrawOp, EE_WON_DRAW_DOTTED_LINE, EE_WON_DRAW_DOTTED_RECT, EE_WON_DRAW_FILL_RECT, EE_WON_DRAW_IMAGE,
    EE_WON_DRAW_IMAGE_SIZE, EE_WON_DRAW_LINE, EE_WON_DRAW_PIXEL, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP,
    EE_WON_DRAW_RECT, EE_WON_DRAW_SET_COLOR, EE_WON_DRAW_SET_FONT, EE_WON_DRAW_SET_SYS_COLOR, EE_WON_DRAW_STRING,
    EE_WON_DRAW_STRING_ALL, EE_WON_DRAW_TEXT, EE_WON_DRAW_TRANSLATE,
};
use crate::mspaint::{
    ee_won_mspaint_plan, EeWonMsPaint, EE_WON_DRAW_ARC, EE_WON_DRAW_FILL_ELLIPSE, EE_WON_DRAW_SYS_PIXEL,
};

/// The game's Graphics (the C++ `Graphics` is this record since R6 WON GUI round 39, made, attached and freed here:
/// [`new_graphics`], [`free_graphics`]): the translation, colour and clip, the clip stack, the borrowed colour
/// scheme, the surface, and the display context and font (each held, as the original's SmartPtr members held them).
#[repr(C)]
#[derive(Debug)]
pub struct EeWonGraphics {
    pub st: EeWonGfxState,
    /// the clip stack (owned)
    pub gfx: *mut EeWonGfx,
    /// the `ColorScheme*` (borrowed; NULL: none)
    pub scheme: *const c_void,
    pub plat: EeWonGfxPlat,
    /// the `DisplayContext*` (held; 0: none)
    pub context: isize,
    /// the `Font*` (held; 0: none)
    pub font: isize,
}

/// What a plan's operations name (WONGUIDraw.h's WONDrawSlots): four `Image*`, the `Font*`, the `GUIString*` and the
/// `WrappedText*`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonDrawSlots {
    pub images: [*mut c_void; 4],
    pub font: *mut c_void,
    pub string: *const c_void,
    pub text: *mut c_void,
}

impl Default for EeWonDrawSlots {
    fn default() -> Self {
        EeWonDrawSlots {
            images: [std::ptr::null_mut(); 4],
            font: std::ptr::null_mut(),
            string: std::ptr::null(),
            text: std::ptr::null_mut(),
        }
    }
}

static HOST: AtomicUsize = AtomicUsize::new(0);
static SCHEME_COLORS: AtomicUsize = AtomicUsize::new(0);

/// The game's Graphics host (the image draws and size, the text, the font: `EeWonGfxHook::ImageEntire` ..) and
/// `offsetof(ColorScheme, mColors)`.
#[no_mangle]
pub extern "C" fn ee_won_graphics_register(host: EeWonGfxHost, scheme_colors: usize) {
    SCHEME_COLORS.store(scheme_colors, Ordering::Release);
    HOST.store(host as usize, Ordering::Release);
}

unsafe extern "C" fn no_host(_: *mut c_void, _: H, _: *const i32, _: *const c_void) -> u32 {
    0
}
fn registered() -> EeWonGfxHost {
    match HOST.load(Ordering::Acquire) {
        0 => no_host,
        // SAFETY: a value ee_won_graphics_register stored from a host function
        f => unsafe { std::mem::transmute::<usize, EeWonGfxHost>(f) },
    }
}
fn host() -> EeWonGfxHost {
    match HOST.load(Ordering::Acquire) {
        0 => no_host,
        _ => game_hook,
    }
}

/// The Graphics hooks: in the game the font set and the image draws and size are answered here (R6 WON GUI round
/// 39: the record's context and font, imgobj.rs's images); the text and the wrapped text, and every hook in a
/// test's world, go to the registered host.
unsafe extern "C" fn game_hook(gp: *mut c_void, h: H, a: *const i32, p: *const c_void) -> u32 {
    if !crate::compobj::is_game() || !matches!(h, H::SetFont | H::ImageEntire | H::ImagePart | H::ImageSize) {
        // SAFETY: the hook's contract, passed on
        return unsafe { registered()(gp, h, a, p) };
    }
    let g = gp as *mut EeWonGraphics;
    if h == H::SetFont {
        set_font(g, p as isize);
        return 0;
    }
    // SAFETY: the hook's contract: seven ints at `a`, `p` the game's live image handle (or NULL), `g` live
    unsafe {
        let a = std::slice::from_raw_parts(a, 7);
        let Some(i) = crate::imgobj::image_of(p as isize) else { return 0 };
        let ctx = (*g).context;
        match h {
            H::ImageEntire => {
                let (w, h) = (i.width() as u16 as i32, i.height() as u16 as i32);
                crate::imgobj::ee_won_image_draw(i, ctx, a[0], a[1], 0, 0, w, h)
            }
            H::ImagePart => crate::imgobj::ee_won_image_draw(i, ctx, a[0], a[1], a[2], a[3], a[4], a[5]),
            _ => {
                let (w, h) = (a[0] as isize, a[1] as isize);
                crate::imgobj::ee_won_image_call(i, crate::imgobj::EeWonImageCall::SetSize, w, h, 0, 0);
            }
        }
    }
    0
}

/// A new Graphics' members: no translation, no colour scheme, an empty clip stack, no surface, context or font
/// (the colour is set when it attaches; the clip starts empty).
fn init() -> EeWonGraphics {
    EeWonGraphics {
        st: EeWonGfxState::default(),
        gfx: crate::gfxobj::ee_won_gfx_new(),
        scheme: std::ptr::null(),
        plat: crate::gfxplat::ee_won_gfx_plat_none(),
        context: 0,
        font: 0,
    }
}

/// `held = p` (a SmartPtr member's assignment: the new reference first).
fn set_held(held: &mut isize, p: isize) {
    if p != 0 {
        crate::gamewin::rf(p, true);
    }
    let old = std::mem::replace(held, p);
    if old != 0 {
        crate::gamewin::rf(old, false);
    }
}

/// `new Graphics` attached to the display context `ctx` (PlatNativeImage.cpp's NewGraphics until R6 WON GUI round
/// 39): the context held and linked back to the Graphics, its surface drawn into at its pixel scale, the colour
/// black.
pub(crate) fn new_graphics(ctx: isize) -> isize {
    let g = Box::into_raw(Box::new(init()));
    // SAFETY: the new record; `ctx` the game's live display context (NewGraphics' contract)
    unsafe {
        set_held(&mut (*g).context, ctx);
        if let Some(r) = crate::imgobj::context_rec_mut(ctx) {
            r.graphics = g as isize;
            (*g).plat = crate::gfxplat::ee_won_gfx_plat_of(r.surface, r.pixel_scale);
        }
        (*g).st.color = 0;
    }
    g as isize
}

/// `delete` a Graphics: detached (the surface and the context left), the clip stack freed, the font and the context
/// given back.
pub(crate) fn free_graphics(g: isize) {
    // SAFETY: a record new_graphics made, freed once (FreeGraphics' contract)
    let mut g = unsafe { Box::from_raw(g as *mut EeWonGraphics) };
    if !g.plat.surface.is_null() {
        g.plat.surface = std::ptr::null_mut();
        set_held(&mut g.context, 0);
    }
    // SAFETY: the record's own clip stack
    unsafe { crate::gfxobj::ee_won_gfx_free(g.gfx) };
    set_held(&mut g.font, 0);
    set_held(&mut g.context, 0);
}

/// `SetFont(f)`: the font held when it differs from the current one (selecting it into the context did nothing).
fn set_font(g: *mut EeWonGraphics, f: isize) {
    // SAFETY: the game's live Graphics
    unsafe {
        if f != (*g).font {
            set_held(&mut (*g).font, f);
        }
    }
}

/// `SetFont(f)` on the game's Graphics `g`.
///
/// # Safety
/// `g` is a live game Graphics; `f` a live Font or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_graphics_set_font(g: *mut EeWonGraphics, f: *mut c_void) {
    set_font(g, f as isize);
}

/// The image host's Graphics ops in the game (imgobj.rs; PlatNativeImage.cpp's cases went, R6 WON GUI round 39): a
/// new and a freed Graphics, the translation into the two ints at `b`, the context, a plan's `c` operations at `b`
/// with the four images at `d`, the image `b` drawn at the two ints at `c`. None in a driver's world.
pub(crate) fn image_host_op(
    op: crate::imgobj::EeWonImageHost,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> Option<isize> {
    use crate::imgobj::EeWonImageHost as I;
    if !crate::compobj::is_game() {
        return None;
    }
    let g = a as *mut EeWonGraphics;
    // SAFETY: the game's live Graphics and the ops' arguments (EeWonImageHost's contracts)
    unsafe {
        Some(match op {
            I::NewGraphics => new_graphics(a),
            I::FreeGraphics => {
                free_graphics(a);
                0
            }
            I::GfxTrans => {
                *(b as *mut [i32; 2]) = [(*g).st.trans_x, (*g).st.trans_y];
                0
            }
            I::GfxContext => (*g).context,
            I::GfxRun => {
                let s = EeWonDrawSlots { images: *(d as *const [*mut c_void; 4]), ..EeWonDrawSlots::default() };
                ee_won_graphics_draw(g, b as *const EeWonDrawOp, c as usize, &s);
                0
            }
            I::GfxDrawImage => {
                let xy = *(c as *const [i32; 2]);
                gc(g, EeWonGfxCall::DrawImage, [xy[0], xy[1], 0, 0], b as *const c_void);
                0
            }
            _ => return None,
        })
    }
}

/// The scheme's colours (NULL without one).
fn colors(scheme: *const c_void) -> *const u32 {
    if scheme.is_null() {
        std::ptr::null()
    } else {
        // SAFETY: a live ColorScheme (the record's contract): its colours at their offset
        unsafe { (scheme as *const u8).add(SCHEME_COLORS.load(Ordering::Acquire)) as *const u32 }
    }
}

/// One Graphics method on the game's Graphics `g` with seven ints and the image or text `p` (Graphics' GCall).
///
/// # Safety
/// `g` is a live C++ Graphics; `p` what the call names.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_graphics_call(
    g: *mut EeWonGraphics,
    call: EeWonGfxCall,
    a: i32,
    b: i32,
    c: i32,
    d: i32,
    e: i32,
    f: i32,
    h: i32,
    p: *const c_void,
) -> u32 {
    let args = [a, b, c, d, e, f, h];
    // SAFETY: the contract; the members read once here, as the C++ passed them (a hook may re-enter the Graphics:
    // only raw pointers to it are held)
    unsafe {
        let (gfx, scheme, plat) = ((*g).gfx, (*g).scheme, (*g).plat);
        ee_won_gfx_plat_call(
            g as *mut c_void,
            host(),
            gfx,
            std::ptr::addr_of_mut!((*g).st),
            colors(scheme),
            plat.surface,
            plat.scale,
            call,
            args.as_ptr(),
            p,
        )
    }
}

unsafe fn gc(g: *mut EeWonGraphics, call: EeWonGfxCall, a: [i32; 4], p: *const c_void) -> u32 {
    // SAFETY: the caller's
    unsafe { ee_won_graphics_call(g, call, a[0], a[1], a[2], a[3], 0, 0, 0, p) }
}
unsafe fn hook(g: *mut EeWonGraphics, h: H, a: [i32; 2], p: *const c_void) {
    let x = [a[0], a[1], 0, 0, 0, 0, 0];
    // SAFETY: the caller's; the host's contract
    unsafe { host()(g as *mut c_void, h, x.as_ptr(), p) };
}

/// A `GUIString*`'s length (`length()`).
unsafe fn string_len(s: *const c_void) -> i32 {
    // SAFETY: a live GUIString: its data's block at its offset (guistrobj.rs)
    unsafe {
        let data = *(s as *const *const u8);
        (*(data.add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf)).length as i32
    }
}

/// Whether the operation is the Graphics' alone (no slot, no caller's call).
pub fn pure(kind: i32) -> bool {
    matches!(
        kind,
        EE_WON_DRAW_SET_COLOR
            | EE_WON_DRAW_SET_SYS_COLOR
            | EE_WON_DRAW_FILL_RECT
            | EE_WON_DRAW_RECT
            | EE_WON_DRAW_DOTTED_RECT
            | EE_WON_DRAW_LINE
            | EE_WON_DRAW_PUSH_CLIP
            | EE_WON_DRAW_POP_CLIP
            | EE_WON_DRAW_TRANSLATE
            | EE_WON_DRAW_DOTTED_LINE
            | EE_WON_DRAW_PIXEL
            | EE_WON_DRAW_SYS_PIXEL
            | EE_WON_DRAW_FILL_ELLIPSE
            | EE_WON_DRAW_ARC
    )
}

/// One operation on the Graphics (WONDrawOne's): the calls (`EE_WON_DRAW_CALL`) and the stretch filter's kinds are
/// the caller's, so nothing here.
unsafe fn one(g: *mut EeWonGraphics, o: &EeWonDrawOp, s: &EeWonDrawSlots) {
    use EeWonGfxCall as C;
    let four = [o.a, o.b, o.c, o.d];
    let none = std::ptr::null();
    let image = || s.images.get(o.a as usize).copied().unwrap_or(std::ptr::null_mut()) as *const c_void;
    // SAFETY: the caller's (each call re-enters the Graphics through its record)
    unsafe {
        match o.kind {
            EE_WON_DRAW_SET_COLOR => _ = gc(g, C::SetColor, [o.a, 0, 0, 0], none),
            EE_WON_DRAW_SET_SYS_COLOR => _ = gc(g, C::SetSysColor, [o.a, 0, 0, 0], none),
            EE_WON_DRAW_FILL_RECT => _ = gc(g, C::FillRect, four, none),
            EE_WON_DRAW_RECT => _ = gc(g, C::DrawRect, four, none),
            EE_WON_DRAW_DOTTED_RECT => _ = gc(g, C::DrawDottedRect, four, none),
            EE_WON_DRAW_LINE => _ = gc(g, C::DrawLine, four, none),
            EE_WON_DRAW_IMAGE => _ = gc(g, C::DrawImage, [o.b, o.c, 0, 0], image()),
            EE_WON_DRAW_IMAGE_SIZE => hook(g, H::ImageSize, [o.b, o.c], image()),
            EE_WON_DRAW_PUSH_CLIP => _ = gc(g, C::PushClip, four, none),
            EE_WON_DRAW_POP_CLIP => _ = gc(g, C::PopClip, [0; 4], none),
            EE_WON_DRAW_TRANSLATE => _ = gc(g, C::Translate, [o.a, o.b, 0, 0], none),
            EE_WON_DRAW_SET_FONT => hook(g, H::SetFont, [0, 0], s.font),
            // (the offset and length as `unsigned short`)
            EE_WON_DRAW_STRING => {
                _ = gc(g, C::DrawStringPart, [o.b as u16 as i32, o.c as u16 as i32, o.d, o.e], s.string)
            }
            EE_WON_DRAW_STRING_ALL => _ = gc(g, C::DrawString, [o.b, o.c, string_len(s.string), 0], s.string),
            EE_WON_DRAW_TEXT => hook(g, H::Text, [o.b, o.c], s.text),
            // every other pixel in the current colour (TreeItem's dotted lines)
            EE_WON_DRAW_DOTTED_LINE => {
                let color = (*g).st.color as i32;
                let mut i = if (if o.d != 0 { o.b } else { o.a }) & 1 != 0 { 1 } else { 0 };
                while i < o.c {
                    let (x, y) = if o.d != 0 { (o.a, o.b.wrapping_add(i)) } else { (o.a.wrapping_add(i), o.b) };
                    gc(g, C::SetPixel, [x, y, color, 0], none);
                    i += 2;
                }
            }
            EE_WON_DRAW_PIXEL => _ = gc(g, C::SetPixel, [o.a, o.b, o.c, 0], none),
            EE_WON_DRAW_SYS_PIXEL => {
                let c = gc(g, C::StandardColor, [o.c, 0, 0, 0], none);
                gc(g, C::SetPixel, [o.a, o.b, c as i32, 0], none);
            }
            EE_WON_DRAW_FILL_ELLIPSE => _ = gc(g, C::FillEllipse, four, none),
            EE_WON_DRAW_ARC => _ = ee_won_graphics_call(g, C::DrawArc, o.a, o.b, o.c, o.d, o.e, o.f, 0, none),
            _ => {}
        }
    }
}

/// The `n` operations at `ops` run on the game's Graphics `g` in order, their slots `slots` (NULL: none).
///
/// # Safety
/// `g` is a live C++ Graphics; `ops` holds `n` operations; `slots` is NULL or live, its objects live for the kinds
/// that name them.
#[no_mangle]
pub unsafe extern "C" fn ee_won_graphics_draw(
    g: *mut EeWonGraphics,
    ops: *const EeWonDrawOp,
    n: usize,
    slots: *const EeWonDrawSlots,
) {
    let s = if slots.is_null() {
        EeWonDrawSlots::default()
    } else {
        // SAFETY: the contract
        unsafe { *slots }
    };
    for i in 0..n {
        // SAFETY: the contract (copied: an operation may run C++ that plans again)
        let o = unsafe { *ops.add(i) };
        // SAFETY: the contract
        unsafe { one(g, &o, &s) };
    }
}

/// The Windows skin's paint `p` (mspaint.rs's plan) run on the game's Graphics `g` (WONMsPaint; no caller asks for
/// the plan's text call).
///
/// # Safety
/// `g` is a live C++ Graphics; `p` live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_graphics_paint(g: *mut EeWonGraphics, p: *const EeWonMsPaint) {
    // SAFETY: the contract (the plan is pure: asked for its length, then filled)
    unsafe {
        let n = ee_won_mspaint_plan(p, std::ptr::null_mut(), 0);
        let mut ops = vec![EeWonDrawOp::default(); n];
        let n = ee_won_mspaint_plan(p, ops.as_mut_ptr(), n);
        ee_won_graphics_draw(g, ops.as_ptr(), n.min(ops.len()), std::ptr::null());
    }
}

/// A GUI object's Graphics op in the game (gamewin.rs common_op): a plain draw operation (`Draw` of a kind that is
/// the Graphics' alone: the others name the asking host's slots or calls), the scheme saved and set (`CtScheme`,
/// `CtSetScheme`). `g` is the game's Graphics.
pub(crate) fn game_op(op: crate::guihost::EeWonGuiOp, g: isize, b: isize) -> Option<isize> {
    use crate::guihost::EeWonGuiOp as Op;
    let g = g as *mut EeWonGraphics;
    match op {
        Op::Draw => {
            // SAFETY: `b` is the operation (Draw's contract)
            let o = unsafe { *(b as *const EeWonDrawOp) };
            if !pure(o.kind) {
                return None;
            }
            // SAFETY: the game's live Graphics
            unsafe { one(g, &o, &EeWonDrawSlots::default()) };
            Some(0)
        }
        // SAFETY: the game's live Graphics
        Op::CtScheme => Some(unsafe { (*g).scheme } as isize),
        Op::CtSetScheme => {
            // SAFETY: the game's live Graphics; `b` a live ColorScheme or NULL (borrowed)
            unsafe { (*g).scheme = b as *const c_void };
            Some(0)
        }
        _ => None,
    }
}

/// `DrawImage(image, x, y, left, top, width, height)` on the game's Graphics `g`, the six ints at `q` (the root's
/// background draw: RootContainer.cpp's case went).
pub(crate) fn draw_image_part(g: isize, image: isize, q: isize) -> isize {
    // SAFETY: the game's live Graphics and image; six ints at `q` (RootDrawImage's contract)
    unsafe {
        let q = *(q as *const [i32; 6]);
        let g = g as *mut EeWonGraphics;
        ee_won_graphics_call(g, EeWonGfxCall::DrawImagePart, q[0], q[1], q[2], q[3], q[4], q[5], 0, image as _);
    }
    0
}

/// The background grabber's Graphics calls in the game (grabobj.rs; BackgroundGrabber.cpp's cases went): the clip
/// into the four ints at `c`, the blend (always supported), the raw image `c` drawn at the origin, the colour `c`, the
/// blend fill of the three ints at `d`.
pub(crate) fn grab_op(w: crate::grabobj::EeWonGrab, g: isize, c: isize, d: isize) -> Option<isize> {
    use crate::grabobj::EeWonGrab as W;
    use EeWonGfxCall as C;
    let g = g as *mut EeWonGraphics;
    // SAFETY: the game's live Graphics; `c` and `d` as the calls name them (grabobj.rs's contract)
    unsafe {
        match w {
            W::Clip => {
                let r = (*g).st.clip;
                *(c as *mut [i32; 4]) = [r.x, r.y, r.width, r.height];
            }
            W::BlendSupported => return Some(1),
            W::DrawRaw => _ = gc(g, C::DrawImage, [0; 4], c as *const c_void),
            W::SetColor => _ = gc(g, C::SetColor, [c as i32, 0, 0, 0], std::ptr::null()),
            W::FillBlend => {
                let a = *(d as *const [i32; 3]);
                ee_won_graphics_call(g, C::FillRectBlend, 0, 0, a[0], a[1], a[2], 0, 0, std::ptr::null());
            }
            _ => return None,
        }
    }
    Some(0)
}

/// A game Graphics record for the unit tests (attached to no surface).
#[cfg(test)]
pub(crate) fn tests_graphics() -> EeWonGraphics {
    // (its clip stack leaks with the test)
    init()
}

#[cfg(test)]
#[path = "gfxrun_tests.rs"]
mod tests;
