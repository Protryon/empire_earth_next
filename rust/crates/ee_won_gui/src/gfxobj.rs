//! WONGUI's Graphics as Rust (R6 WONGUI B round 1): the drawing context every lobby paint goes through. The
//! original's Graphics.cpp: the translated drawing (every primitive moves its point by the origin, then calls its
//! hook), the colour (a scheme reference 0x80000000 + 0..16 resolved through the colour scheme, the hook told only
//! of a change), the scheme's colours, the clip stack (pushed relative to the origin, the clip the intersection of
//! every rectangle on it, none with an empty stack) and the base hooks' loops (a rectangle as four lines, a dotted
//! rectangle as every other pixel, a filled rectangle as a line per row).
//!
//! The C++ `Graphics` keeps its vtable (the hooks: PlatGraphics and the drawing stand-ins override them) and its
//! C++-object members (the display context, the font, the borrowed colour scheme); its translation, colour and clip
//! rectangle are one block of members the Rust edits in place (`EeWonGfxState`: PlatGraphics, ProgramImage and
//! BackgroundGrabber read them inline), and the clip stack is this module's object (`EeWonGfx`). The Rust calls the
//! hooks, the images' draws and the font's draw through one host function the C++ hands over.

use std::ffi::c_void;

use crate::guilayout::{intersect, EeWonRect};

/// The C++ Graphics' translation, colour and clip rectangle (`mTransX`, `mTransY`, `mColor`, `mClipRect`, in that
/// order).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonGfxState {
    pub trans_x: i32,
    pub trans_y: i32,
    pub color: u32,
    pub clip: EeWonRect,
}

/// The clip stack (`mClipStack`).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EeWonGfx {
    pub stack: Vec<EeWonRect>,
}

/// A virtual the Rust calls on the C++ object (`args` its ints, `p` its object).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGfxHook {
    ClearClipRect,
    /// (x, y, width, height)
    SetClipRect,
    /// (x, y, colour)
    SetPixel,
    SetColor,
    /// (x1, y1, x2, y2)
    DrawLine,
    /// (x, y, width, height): the virtual (the base's is `GfxCall::BaseRect`)
    DrawRect,
    FillRect,
    DrawDottedRect,
    DrawEllipse,
    FillEllipse,
    /// (x, y, width, height, start, arc)
    DrawArc,
    /// (x, y, width, height, amount)
    FillRectBlend,
    /// `p` the Image: `DrawEntire(context, x, y)`
    ImageEntire,
    /// `p` the Image: `Draw(context, x, y, left, top, width, height)`
    ImagePart,
    /// `p` the GUIString: the font's `DrawString(context, text, offset, length, x, y)`
    String,
}

/// The host: the hook on the C++ Graphics `g` with up to seven ints at `args` and an object `p`; its answer.
pub type EeWonGfxHost =
    unsafe extern "C" fn(g: *mut c_void, hook: EeWonGfxHook, args: *const i32, p: *const c_void) -> u32;

/// The method `ee_won_gfx_call` runs (`a` its ints, `p` its object).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGfxCall {
    /// `SetColor(a0)`
    SetColor,
    /// `GetStandardColor(a0)`: the scheme's colour, 0 without a scheme
    StandardColor,
    /// `SetSysColor(a0)`
    SetSysColor,
    /// `Translate(a0, a1)`
    Translate,
    /// `SetPixel(x, y, colour)`
    SetPixel,
    /// `DrawImage(p, x, y)`
    DrawImage,
    /// `DrawImage(p, x, y, left, top, width, height)`
    DrawImagePart,
    DrawLine,
    DrawRect,
    FillRect,
    /// `FillRectBlend(x, y, width, height, amount)`
    FillRectBlend,
    DrawDottedRect,
    DrawEllipse,
    FillEllipse,
    DrawArc,
    /// `DrawString(p, x, y)` with `a2` the text's length (the whole text)
    DrawString,
    /// `DrawString(p, offset, length, x, y)`
    DrawStringPart,
    PerformClip,
    /// `PushClipRect(x, y, width, height)`
    PushClip,
    PopClip,
    /// Graphics' own `DrawRectHook`: four lines to the far edges
    BaseRect,
    /// Graphics' own `DrawDottedRectHook`: every other pixel of the four edges in the current colour
    BaseDottedRect,
    /// Graphics' own `FillRectHook`: a line per row
    BaseFillRect,
}

/// One Graphics call's context: the C++ object's members and clip stack (raw: a hook may call back into this
/// object's Graphics, so no reference to them lives across a hook), its scheme's 17 colours (or none) and its hooks.
pub struct Gfx<H: FnMut(EeWonGfxHook, &[i32; 7], *const c_void) -> u32> {
    pub st: *mut EeWonGfxState,
    pub obj: *mut EeWonGfx,
    pub scheme: Option<[u32; 17]>,
    pub hook: H,
}

const NO_CLIP: EeWonRect = EeWonRect { x: -10_000_000, y: -10_000_000, width: 20_000_001, height: 20_000_001 };

impl<H: FnMut(EeWonGfxHook, &[i32; 7], *const c_void) -> u32> Gfx<H> {
    fn st(&self) -> EeWonGfxState {
        // SAFETY: live (the caller's contract); read, not held
        unsafe { *self.st }
    }

    fn set_st(&mut self, f: impl FnOnce(&mut EeWonGfxState)) {
        // SAFETY: live; the reference ends before any hook
        f(unsafe { &mut *self.st })
    }

    fn stack(&mut self, f: impl FnOnce(&mut Vec<EeWonRect>)) {
        // SAFETY: live; the reference ends before any hook
        f(unsafe { &mut (*self.obj).stack })
    }

    fn hook(&mut self, h: EeWonGfxHook, a: &[i32], p: *const c_void) -> u32 {
        let mut x = [0i32; 7];
        x[..a.len()].copy_from_slice(a);
        (self.hook)(h, &x, p)
    }

    fn translate(&self, x: i32, y: i32) -> (i32, i32) {
        let st = self.st();
        (x.wrapping_add(st.trans_x), y.wrapping_add(st.trans_y))
    }

    /// `SetColor`: a scheme reference resolved through the scheme (when there is one; this way, not through
    /// `ApplyColorSchemeColor`, so a scheme colour referring to itself does not loop), the hook told of a change.
    pub fn set_color(&mut self, color: u32) {
        let resolved = match &self.scheme {
            Some(s) if (0x8000_0000..0x8000_0000 + 17).contains(&color) => s[(color - 0x8000_0000) as usize],
            _ => color,
        };
        if resolved != self.st().color {
            self.set_st(|s| s.color = resolved);
            self.hook(EeWonGfxHook::SetColor, &[], std::ptr::null());
        }
    }

    pub fn standard_color(&self, i: i32) -> u32 {
        match &self.scheme {
            Some(s) => s.get(i as usize).copied().unwrap_or(0),
            None => 0,
        }
    }

    /// `PerformClip`: the clip cleared, then the intersection of the stack (none when empty).
    pub fn perform_clip(&mut self) {
        self.hook(EeWonGfxHook::ClearClipRect, &[], std::ptr::null());
        self.set_st(|s| s.clip = NO_CLIP);
        let mut clip = None;
        self.stack(|v| {
            if let Some((&first, rest)) = v.split_first() {
                clip = Some(rest.iter().fold(first, |r, &q| intersect(r, q)));
            }
        });
        let Some(r) = clip else {
            return;
        };
        self.hook(EeWonGfxHook::SetClipRect, &[r.x, r.y, r.width, r.height], std::ptr::null());
        self.set_st(|s| s.clip = r);
    }

    /// One method (`a` its ints, `p` its object); the answer of `StandardColor`, else 0.
    pub fn call(&mut self, call: EeWonGfxCall, a: &[i32; 7], p: *const c_void) -> u32 {
        use EeWonGfxCall as C;
        use EeWonGfxHook as H;
        // the primitives: the point translated, the other numbers as given
        let at = |g: &Self, rest: &[i32]| -> Vec<i32> {
            let (x, y) = g.translate(a[0], a[1]);
            let mut v = vec![x, y];
            v.extend_from_slice(rest);
            v
        };
        match call {
            C::SetColor => self.set_color(a[0] as u32),
            C::StandardColor => return self.standard_color(a[0]),
            C::SetSysColor => {
                if self.scheme.is_some() {
                    let c = self.standard_color(a[0]);
                    self.set_color(c);
                }
            }
            C::Translate => self.set_st(|s| {
                s.trans_x = s.trans_x.wrapping_add(a[0]);
                s.trans_y = s.trans_y.wrapping_add(a[1]);
            }),
            C::SetPixel => {
                let v = at(self, &[a[2]]);
                self.hook(H::SetPixel, &v, p);
            }
            C::DrawImage => {
                let v = at(self, &[]);
                self.hook(H::ImageEntire, &v, p);
            }
            C::DrawImagePart => {
                let v = at(self, &a[2..6]);
                self.hook(H::ImagePart, &v, p);
            }
            C::DrawLine => {
                let (x1, y1) = self.translate(a[0], a[1]);
                let (x2, y2) = self.translate(a[2], a[3]);
                self.hook(H::DrawLine, &[x1, y1, x2, y2], p);
            }
            C::DrawRect | C::FillRect | C::DrawDottedRect | C::DrawEllipse | C::FillEllipse => {
                let h = match call {
                    C::DrawRect => H::DrawRect,
                    C::FillRect => H::FillRect,
                    C::DrawDottedRect => H::DrawDottedRect,
                    C::DrawEllipse => H::DrawEllipse,
                    _ => H::FillEllipse,
                };
                let v = at(self, &a[2..4]);
                self.hook(h, &v, p);
            }
            C::FillRectBlend => {
                let v = at(self, &a[2..5]);
                self.hook(H::FillRectBlend, &v, p);
            }
            C::DrawArc => {
                let v = at(self, &a[2..6]);
                self.hook(H::DrawArc, &v, p);
            }
            C::DrawString => {
                // (offset 0, the text's length, then the point)
                let (x, y) = self.translate(a[0], a[1]);
                self.hook(H::String, &[0, a[2], x, y], p);
            }
            C::DrawStringPart => {
                let (x, y) = self.translate(a[2], a[3]);
                self.hook(H::String, &[a[0], a[1], x, y], p);
            }
            C::PerformClip => self.perform_clip(),
            C::PushClip => {
                let (x, y) = self.translate(a[0], a[1]);
                self.stack(|v| v.push(EeWonRect { x, y, width: a[2], height: a[3] }));
                self.perform_clip();
            }
            C::PopClip => {
                self.stack(|v| {
                    v.pop();
                });
                self.perform_clip();
            }
            C::BaseRect => {
                let (x, y, w, h) = (a[0], a[1], a[2], a[3]);
                let (r, b) = (x.wrapping_add(w), y.wrapping_add(h));
                self.hook(H::DrawLine, &[x, y, r, y], p);
                self.hook(H::DrawLine, &[r, y, r, b], p);
                self.hook(H::DrawLine, &[r, b, x, b], p);
                self.hook(H::DrawLine, &[x, b, x, y], p);
            }
            C::BaseDottedRect => {
                let (x, y, w, h) = (a[0], a[1], a[2], a[3]);
                // (the colour read at each pixel, as the original's `this->mColor` was)
                let mut i = 0;
                while i < w {
                    let c = self.st().color as i32;
                    self.hook(H::SetPixel, &[x.wrapping_add(i), y, c], p);
                    let c = self.st().color as i32;
                    self.hook(H::SetPixel, &[x.wrapping_add(i), y.wrapping_add(h).wrapping_sub(1), c], p);
                    i += 2;
                }
                let mut i = 0;
                while i < h {
                    let c = self.st().color as i32;
                    self.hook(H::SetPixel, &[x, y.wrapping_add(i), c], p);
                    let c = self.st().color as i32;
                    self.hook(H::SetPixel, &[x.wrapping_add(w).wrapping_sub(1), y.wrapping_add(i), c], p);
                    i += 2;
                }
            }
            C::BaseFillRect => {
                let (x, y, w, h) = (a[0], a[1], a[2], a[3]);
                for i in 0..h.max(0) {
                    let row = y.wrapping_add(i);
                    self.hook(H::DrawLine, &[x, row, x.wrapping_add(w), row], p);
                }
            }
        }
        0
    }
}

/// A new Graphics' clip stack (empty).
#[no_mangle]
pub extern "C" fn ee_won_gfx_new() -> *mut EeWonGfx {
    Box::into_raw(Box::default())
}

/// # Safety
/// `o` is null or from `ee_won_gfx_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gfx_free(o: *mut EeWonGfx) {
    if !o.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(o) });
    }
}

/// One Graphics method on the C++ object `g` (its members `st`, its clip stack `o`, its scheme's 17 colours at
/// `scheme` or NULL): `a` holds seven ints, `p` the image or text; the host calls `g`'s hooks.
///
/// # Safety
/// `g` is live for the host; `o`, `st` and `a` are live (seven ints); `scheme` is NULL or 17 colours. The host may
/// call back into Graphics on another object only.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gfx_call(
    g: *mut c_void,
    host: EeWonGfxHost,
    o: *mut EeWonGfx,
    st: *mut EeWonGfxState,
    scheme: *const u32,
    call: EeWonGfxCall,
    a: *const i32,
    p: *const c_void,
) -> u32 {
    // SAFETY: the contract (copied: the scheme is the C++'s, read once per call as the original read it per call)
    let (args, scheme) =
        unsafe { (*(a as *const [i32; 7]), (!scheme.is_null()).then(|| *(scheme as *const [u32; 17]))) };
    let mut gfx = Gfx {
        st,
        obj: o,
        scheme,
        // SAFETY: the host's contract
        hook: |h: EeWonGfxHook, x: &[i32; 7], q: *const c_void| unsafe { host(g, h, x.as_ptr(), q) },
    };
    gfx.call(call, &args, p)
}

#[cfg(test)]
#[path = "gfxobj_tests.rs"]
mod tests;
