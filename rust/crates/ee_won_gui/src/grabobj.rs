//! WONGUI's BackgroundGrabber as a whole Rust object (R6 WON round 37): what a translucent or background-grabbing
//! component (a lobby popup, a tool tip, a skinned button over the lobby's picture) keeps of the screen under it.
//! The original's BackgroundGrabber.cpp: the captured image's place, the clip it was captured in, whether it is
//! captured and must be put back, the translucent colour and strength, the capture's order, with every method:
//! the capture (the offscreen image copied into the background image, a debug window showing it), its tint (one
//! blended fill, or each pixel scaled to n/8 plus the colour's (8-n)/8), putting it back before a repaint.
//!
//! The C++ `BackgroundGrabber` stays the type the components' extra data and the root container hold; it keeps its
//! C++-object members (the background image, the debug window) and its methods are one-line forwarders. The Rust
//! object makes the original's calls on the component, the Graphics, the images and the debug window through the
//! host's `Grab` op (`EeWonGrab`); the root container reads its state with `ee_won_grab_read`.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::rootobj::EeWonRootGrab;

/// `ComponentFlag_DebugBG` and `ComponentFlag_Translucent` (WONComponentFlags.h; the C++ asserts them).
pub const EE_WON_GRAB_FLAG_DEBUG_BG: isize = 0x20000;
pub const EE_WON_GRAB_FLAG_TRANSLUCENT: isize = 0x200;

/// The inward calls (the C++ shell's methods; `a` the component, `b` the Graphics where they take them).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGrabCall {
    /// `Clear()`.
    Clear = 1,
    /// `PrePaint((Component*)a, *(Graphics*)b)`.
    PrePaint,
    /// `Paint((Component*)a, *(Graphics*)b)`.
    Paint,
    /// `SetTranslucentColor(a, b)`.
    SetTranslucentColor,
    /// `Invalidate((Component*)a)`.
    Invalidate,
    /// `SizeChanged((Component*)a)` (nothing).
    SizeChanged,
    /// `NeedDrawBG()` (0 or 1).
    NeedDrawBG,
}

/// The host's calls (`EeWonGuiOp::Grab`; the receiver the C++ `BackgroundGrabber*`; `b`, `c`, `d` the arguments;
/// R6 WON round 34: each one statement of the original's, the runs of them composed here).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGrab {
    /// `((Component*)b)->LocalToRoot(p[0], p[1])` with `p` the two ints at `c`.
    LocalToRoot = 1,
    /// `((Graphics*)b)->DrawImage(mBGImage, c, d)`.
    DrawImage,
    /// `((Graphics*)b)->mClipRect` into the `EeWonRect4` at `c`.
    Clip,
    /// `((Component*)b)->Width()` and `Height()` into the two ints at `c`.
    CompSize,
    /// `mBGImage.get() != NULL` (0 or 1).
    HasImage,
    /// `mBGImage->GetWidth()`.
    ImageWidth,
    /// `mBGImage->GetHeight()`.
    ImageHeight,
    /// `mBGImage = ((Component*)b)->GetRoot()->CreateImage(c, d)`.
    NewImage,
    /// `((Component*)b)->ComponentFlagSet(c)` (0 or 1).
    FlagSet,
    /// `mDebugWindow.get() != NULL` (0 or 1).
    DebugExists,
    /// `mDebugWindow = CreateNativeWindow()`.
    DebugNew,
    /// `mDebugWindow->SetParent(((Component*)b)->GetRoot())`.
    DebugParent,
    /// `mDebugWindow->SetStyle(0x40)`.
    DebugStyle,
    /// `mDebugWindow->Create(WONRectangle(0, 0, c, d))`.
    DebugCreateAt,
    /// `mDebugWindow->mRoot->RemoveAllChildren()`.
    DebugRemoveAll,
    /// `mDebugWindow->AddComponent(new ImageComponent(mBGImage))`.
    DebugAddImage,
    /// `((Component*)b)->GetRoot()->GetRect(r, false)` into the `EeWonRect4` at `c`.
    ScreenRect,
    /// `mDebugWindow->SetPosSize(c, d, -1, -1)`.
    DebugPos,
    /// `mDebugWindow->mRoot->Invalidate()`.
    DebugInvalidate,
    /// `((Component*)b)->GetRoot()->GetOffscreenImage()` (a pointer).
    Offscreen,
    /// `((Component*)b)->RootToChild(p[0], p[1])` with `p` the two ints at `c`.
    RootToChild,
    /// `mBGImage->GetGraphics().DrawImage((NativeImage*)c, p[0], p[1])` with `p` the two ints at `d`.
    CaptureDraw,
    /// `((Graphics*)b)->BlendSupported()` (0 or 1).
    BlendSupported,
    /// `((Graphics*)b)->SetColor(c)`.
    SetColor,
    /// `((Graphics*)b)->FillRectBlend(0, 0, w, h, alpha)` with `w`, `h`, `alpha` the three ints at `d`.
    FillBlend,
    /// `mBGImage->GetRaw()`, a reference held (a `RawImage32*`).
    RawNew,
    /// `((RawImage32*)c)->GetImageData()`.
    RawPixels,
    /// `((RawImage32*)c)->GetWidth()`.
    RawWidth,
    /// `((RawImage32*)c)->GetHeight()`.
    RawHeight,
    /// `((Graphics*)b)->DrawImage((RawImage32*)c, 0, 0)`.
    DrawRaw,
    /// `((RawImage32*)c)->Release()`.
    RawRelease,
}

#[derive(Clone, Copy, Debug, Default)]
struct State {
    image_x: i32,
    image_y: i32,
    clip: [i32; 4],
    need_put: bool,
    have: bool,
    color: i32,
    level: i32,
    order: u64,
}

thread_local! {
    /// `mImageOrderCounter`: the order of the captures, over every grabber (the original's was one static; the
    /// lobby's one GUI thread made every capture, so one counter per thread is the same order).
    static ORDER: Cell<u64> = const { Cell::new(0) };
}

/// The Rust BackgroundGrabber.
pub struct EeWonGrabber {
    me: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

/// `COLOR_n_8` of a pixel (its channels scaled to n/8, alpha's too, as the original's macros did on a `DWORD`).
fn scale8(c: u32, n: i32) -> u32 {
    let one = (c >> 3) & 0xff1f1f1f;
    let two = (c >> 2) & 0xff3f3f3f;
    let four = (c >> 1) & 0xff7f7f7f;
    match n {
        1 => one,
        2 => two,
        3 => one.wrapping_add(two),
        4 => four,
        5 => four.wrapping_add(one),
        6 => four.wrapping_add(two),
        7 => four.wrapping_add(one.wrapping_add(two)),
        _ => 0,
    }
}

/// `GetColor8th(color, n)`: the same of an `int` colour (its shifts arithmetic, as the original's on an `int`); 0
/// outside 1..7.
pub fn color_8th(color: i32, n: i32) -> i32 {
    let one = ((color >> 3) as u32) & 0xff1f1f1f;
    let two = ((color >> 2) as u32) & 0xff3f3f3f;
    let four = ((color >> 1) as u32) & 0xff7f7f7f;
    (match n {
        1 => one,
        2 => two,
        3 => one.wrapping_add(two),
        4 => four,
        5 => four.wrapping_add(one),
        6 => four.wrapping_add(two),
        7 => four.wrapping_add(one.wrapping_add(two)),
        _ => 0,
    }) as i32
}

/// The translucent pass over the background's pixels: each scaled to `level`/8 plus `color` (levels 1..7; any
/// other leaves them). The original's do-while never ended over 0 pixels; this returns.
pub fn tint(pixels: &mut [u32], level: i32, color: i32) {
    if !(1..=7).contains(&level) {
        return;
    }
    for p in pixels.iter_mut() {
        *p = scale8(*p, level).wrapping_add(color as u32);
    }
}

impl EeWonGrabber {
    /// The constructor's state: nothing captured, black at half strength.
    pub fn new(me: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonGrabber { me, host, st: RefCell::new(State { level: 4, ..State::default() }) }
    }

    fn op(&self, w: EeWonGrab, b: isize, c: isize, d: isize) -> isize {
        if w == EeWonGrab::RawRelease {
            if let Some(r) = crate::gamewin::rf(c, false) {
                return r;
            }
        }
        if crate::compobj::is_game() {
            if let Some(r) = crate::gfxrun::grab_op(w, b, c, d) {
                return r;
            }
            if let Some(r) = self.game_op(w, b, c, d) {
                return r;
            }
        }
        match self.host {
            // SAFETY: the host was given with this grabber; no borrow of the state is live here
            Some(h) => unsafe { h(self.me, Op::Grab, w as isize, b, c, d) },
            None => 0,
        }
    }
    /// The game's answers (R6 WON GUI round 39): the component's members and calls, and the background image (the C++
    /// grabber's `mBGImage`, its first member: an image handle) read in place: its presence and size, its draw on
    /// the Graphics `b` at (c, d), a capture `c` drawn into it at the two ints at `d`, its raw copy (one reference)
    /// and that copy's pixels and size; the new image (the root window's CreateImage) and the root window's offscreen
    /// image (R6 WON GUI round 40). None for the others (the debug window, the root's rectangle).
    fn game_op(&self, w: EeWonGrab, b: isize, c: isize, d: isize) -> Option<isize> {
        use crate::compobj::EeWonCompCall as K;
        use crate::imgobj::{ee_won_image_call, ee_won_image_get, image_of, EeWonImageCall as I, EeWonImageField as F};
        use EeWonGrab as G;
        // SAFETY: the game's live component `b` (each call re-enters through Component's entry)
        let kc = |k: K, p: isize, q: isize| unsafe {
            crate::compobj::ee_won_comp_call(b as *mut c_void, crate::compobj::host(), k, p, q, 0)
        };
        // SAFETY: the game's live grabber: its background image's pointer first
        let bg = unsafe { *(self.me as *const isize) };
        let draw = |g: isize, image: isize, x: i32, y: i32| {
            // SAFETY: the game's live Graphics and image
            unsafe {
                crate::gfxrun::ee_won_graphics_call(
                    g as *mut crate::gfxrun::EeWonGraphics,
                    crate::gfxobj::EeWonGfxCall::DrawImage,
                    x,
                    y,
                    0,
                    0,
                    0,
                    0,
                    0,
                    image as *const c_void,
                )
            };
        };
        // SAFETY: the grabber's or the call's live images (the ops' contracts), each read once
        unsafe {
            Some(match w {
                G::LocalToRoot => {
                    kc(K::LocalToRoot, c, c + 4);
                    0
                }
                G::RootToChild => {
                    kc(K::RootToChild, c, c + 4);
                    0
                }
                G::CompSize => {
                    let r = crate::compmem::bounds(b);
                    *(c as *mut [i32; 2]) = [r.width, r.height];
                    0
                }
                G::FlagSet => isize::from(crate::compmem::bits(b) & c as i32 != 0),
                G::HasImage => isize::from(bg != 0),
                G::ImageWidth => image_of(bg)?.width() as u16 as isize,
                G::ImageHeight => image_of(bg)?.height() as u16 as isize,
                G::DrawImage => {
                    draw(b, bg, c as i32, d as i32);
                    0
                }
                G::CaptureDraw => {
                    let xy = *(d as *const [i32; 2]);
                    draw(image_of(bg)?.graphics(), c, xy[0], xy[1]);
                    0
                }
                G::RawNew => ee_won_image_call(image_of(bg)?, I::Raw, 0, 0, 0, 0),
                G::RawPixels => ee_won_image_get(image_of(c)?, F::Pixels),
                G::RawWidth => image_of(c)?.width() as u16 as isize,
                G::RawHeight => image_of(c)?.height() as u16 as isize,
                G::NewImage => {
                    let old = std::mem::replace(&mut *(self.me as *mut isize), crate::gamewin::new_image(c, d));
                    if old != 0 {
                        crate::gamewin::rf(old, false);
                    }
                    0
                }
                G::Offscreen => crate::gamewin::offscreen(kc(K::Root, 0, 0)),
                _ => return None,
            })
        }
    }
    fn local_to_root(&self, comp: isize, p: &mut [i32; 2]) {
        self.op(EeWonGrab::LocalToRoot, comp, p.as_mut_ptr() as isize, 0);
    }
    fn comp_size(&self, comp: isize) -> (i32, i32) {
        let mut wh = [0i32; 2];
        self.op(EeWonGrab::CompSize, comp, wh.as_mut_ptr() as isize, 0);
        (wh[0], wh[1])
    }
    fn flag(&self, comp: isize, f: isize) -> bool {
        self.op(EeWonGrab::FlagSet, comp, f, 0) != 0
    }
    fn debug_exists(&self) -> bool {
        self.op(EeWonGrab::DebugExists, 0, 0, 0) != 0
    }

    pub fn clear(&self) {
        let mut s = self.st.borrow_mut();
        s.have = false;
        s.need_put = false;
    }

    pub fn need_draw_bg(&self) -> bool {
        let s = self.st.borrow();
        s.have && s.need_put
    }

    /// `PrePaint`: the captured background put back under the component when a repaint is pending, then forgotten.
    pub fn pre_paint(&self, comp: isize, g: isize) {
        if !self.need_draw_bg() {
            return;
        }
        let mut p = [0i32; 2];
        self.local_to_root(comp, &mut p);
        let (x, y) = {
            let s = self.st.borrow();
            (s.image_x.wrapping_sub(p[0]), s.image_y.wrapping_sub(p[1]))
        };
        self.op(EeWonGrab::DrawImage, g, x as isize, y as isize);
        self.clear();
    }

    pub fn set_translucent_color(&self, color: i32, level: i32) {
        let mut s = self.st.borrow_mut();
        s.level = level;
        s.color = color;
    }

    /// `Paint`: the screen under the component captured once (a new background image when the size changed, the
    /// debug window shown when asked), then, for a translucent component, tinted.
    pub fn paint(&self, comp: isize, g: isize) {
        if self.st.borrow().have {
            return;
        }
        let mut clip = [0i32; 4];
        self.op(EeWonGrab::Clip, g, clip.as_mut_ptr() as isize, 0);
        {
            let mut s = self.st.borrow_mut();
            s.clip = clip;
            s.order = ORDER.with(|o| {
                let v = o.get();
                o.set(v.wrapping_add(1));
                v
            });
        }
        let (w, h) = self.comp_size(comp);
        let has = self.op(EeWonGrab::HasImage, 0, 0, 0) != 0;
        if !has
            || w != self.op(EeWonGrab::ImageWidth, 0, 0, 0) as i32
            || h != self.op(EeWonGrab::ImageHeight, 0, 0, 0) as i32
        {
            self.op(EeWonGrab::NewImage, comp, w as isize, h as isize);
            if self.flag(comp, EE_WON_GRAB_FLAG_DEBUG_BG) && !self.debug_exists() {
                self.debug_create(comp);
            }
            if self.debug_exists() {
                self.debug_show(comp);
            }
        }
        // the screen under the component: the root's offscreen image drawn into the background image
        let screen = self.op(EeWonGrab::Offscreen, comp, 0, 0);
        let mut p = [0i32; 2];
        self.op(EeWonGrab::RootToChild, comp, p.as_mut_ptr() as isize, 0);
        self.op(EeWonGrab::CaptureDraw, 0, screen, p.as_ptr() as isize);
        let mut p = [0i32; 2];
        self.local_to_root(comp, &mut p);
        {
            let mut s = self.st.borrow_mut();
            s.image_x = p[0];
            s.image_y = p[1];
        }
        if self.debug_exists() {
            self.op(EeWonGrab::DebugInvalidate, 0, 0, 0);
        }
        self.st.borrow_mut().have = true;
        if !self.flag(comp, EE_WON_GRAB_FLAG_TRANSLUCENT) {
            return;
        }
        let (color, level) = {
            let s = self.st.borrow();
            (s.color, s.level)
        };
        let tinted = color_8th(color, 8i32.wrapping_sub(level));
        if self.op(EeWonGrab::BlendSupported, g, 0, 0) != 0 {
            let (w, h) = self.comp_size(comp);
            let a = [w, h, 8i32.wrapping_sub(level).wrapping_mul(255) / 8];
            self.op(EeWonGrab::SetColor, g, color as isize, 0);
            self.op(EeWonGrab::FillBlend, g, 0, a.as_ptr() as isize);
            return;
        }
        // each pixel scaled to level/8 plus the colour's (8 - level)/8, then drawn
        let raw = self.op(EeWonGrab::RawNew, 0, 0, 0);
        let pixels = self.op(EeWonGrab::RawPixels, 0, raw, 0) as *mut u32;
        let n = (self.op(EeWonGrab::RawWidth, 0, raw, 0) as i32 as usize).wrapping_mul(self.op(
            EeWonGrab::RawHeight,
            0,
            raw,
            0,
        ) as i32 as usize);
        if n != 0 && !pixels.is_null() {
            // SAFETY: the raw image's pixels, held by the reference RawNew took
            tint(unsafe { std::slice::from_raw_parts_mut(pixels, n) }, level, tinted);
        }
        self.op(EeWonGrab::DrawRaw, g, raw, 0);
        self.op(EeWonGrab::RawRelease, 0, raw, 0);
    }

    /// The debug window made (parent the component's root, style 0x40, created at the background image's size).
    fn debug_create(&self, comp: isize) {
        let (w, h) = (self.op(EeWonGrab::ImageWidth, 0, 0, 0), self.op(EeWonGrab::ImageHeight, 0, 0, 0));
        self.op(EeWonGrab::DebugNew, 0, 0, 0);
        self.op(EeWonGrab::DebugParent, comp, 0, 0);
        self.op(EeWonGrab::DebugStyle, 0, 0, 0);
        self.op(EeWonGrab::DebugCreateAt, 0, w, h);
    }

    /// The debug window shows the background image at the component's place on the screen (its root emptied, a
    /// new ImageComponent of the image added, moved to the component's root point plus its window's screen corner).
    fn debug_show(&self, comp: isize) {
        self.op(EeWonGrab::DebugRemoveAll, 0, 0, 0);
        self.op(EeWonGrab::DebugAddImage, 0, 0, 0);
        let mut p = [0i32; 2];
        self.local_to_root(comp, &mut p);
        let mut r = [0i32; 4];
        self.op(EeWonGrab::ScreenRect, comp, r.as_mut_ptr() as isize, 0);
        self.op(EeWonGrab::DebugPos, 0, p[0].wrapping_add(r[0]) as isize, p[1].wrapping_add(r[1]) as isize);
    }

    pub fn invalidate(&self) {
        let mut s = self.st.borrow_mut();
        if s.have {
            s.need_put = true;
        }
    }

    /// The root container's read (`RootGrabberRead`; the image is the C++ member's, set by the caller).
    pub fn read(&self, q: &mut EeWonRootGrab) {
        let s = self.st.borrow();
        q.need = (s.have && s.need_put) as i32;
        q.order = s.order as i32;
        q.x = s.image_x;
        q.y = s.image_y;
        q.clip.x = s.clip[0];
        q.clip.y = s.clip[1];
        q.clip.w = s.clip[2];
        q.clip.h = s.clip[3];
    }

    pub fn call(&self, c: EeWonGrabCall, a: isize, b: isize) -> isize {
        use EeWonGrabCall as C;
        match c {
            C::Clear => self.clear(),
            C::PrePaint => self.pre_paint(a, b),
            C::Paint => self.paint(a, b),
            C::SetTranslucentColor => self.set_translucent_color(a as i32, b as i32),
            C::Invalidate => self.invalidate(),
            C::SizeChanged => {}
            C::NeedDrawBG => return self.need_draw_bg() as isize,
        }
        0
    }
}

/// A new grabber for the C++ `BackgroundGrabber` `me`.
#[no_mangle]
pub extern "C" fn ee_won_grab_new(me: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonGrabber {
    Box::into_raw(Box::new(EeWonGrabber::new(me, host)))
}

/// # Safety
/// `g` is null or from `ee_won_grab_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_grab_free(g: *mut EeWonGrabber) {
    if !g.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(g) });
    }
}

/// One of the shell's methods (`EeWonGrabCall`).
///
/// # Safety
/// `g` is a live grabber.
#[no_mangle]
pub unsafe extern "C" fn ee_won_grab_call(g: *const EeWonGrabber, c: EeWonGrabCall, a: isize, b: isize) -> isize {
    // SAFETY: the contract
    unsafe { (*g).call(c, a, b) }
}

/// The root container's calls on a game grabber `g` (R6 WON GUI round 39; RootContainer.cpp's cases went): its
/// state with its background image into `*(EeWonRootGrab*)q`, its Clear, and its Clear with the image given back.
/// The C++ grabber is its image's pointer, its debug window's and its Rust grabber's (checked in
/// BackgroundGrabber.cpp).
pub(crate) fn root_grabber_op(op: crate::guihost::EeWonGuiOp, g: isize, q: isize) -> isize {
    use crate::guihost::EeWonGuiOp as Op;
    let p = g as *mut isize;
    // SAFETY: the game's live grabber (the ops' contract): the image's pointer first, the Rust grabber third
    unsafe {
        let rust = *p.add(2) as *const EeWonGrabber;
        match op {
            Op::RootGrabberRead => {
                (*rust).read(&mut *(q as *mut EeWonRootGrab));
                (*(q as *mut EeWonRootGrab)).image = *p;
            }
            Op::RootGrabberClear => (*rust).clear(),
            _ => {
                (*rust).clear();
                let image = std::mem::replace(&mut *p, 0);
                if image != 0 {
                    crate::gamewin::rf(image, false);
                }
            }
        }
    }
    0
}

/// The root container's read of the grabber's state into `q` (its `image` left as it was).
///
/// # Safety
/// `g` is a live grabber, `q` points at an `EeWonRootGrab`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_grab_read(g: *const EeWonGrabber, q: *mut EeWonRootGrab) {
    // SAFETY: the contract
    unsafe { (*g).read(&mut *q) }
}

/// The translucent pass over `n` pixels at `pixels` (`tint`).
///
/// # Safety
/// `pixels` points at `n` pixels (or `n` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_grab_tint(pixels: *mut u32, n: usize, level: i32, color: i32) {
    if n == 0 || pixels.is_null() {
        return;
    }
    // SAFETY: the contract
    tint(unsafe { std::slice::from_raw_parts_mut(pixels, n) }, level, color);
}

#[cfg(test)]
#[path = "grabobj_tests.rs"]
mod tests;
