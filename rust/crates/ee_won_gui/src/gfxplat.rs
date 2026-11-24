//! Graphics on its platform surface (R6 WON GUI round 35).
//!
//! The original's Graphics called virtual hooks its one subclass (MSGraphics; the port's PlatGraphics) answered on
//! the surface. With the two classes one, the hooks of [`crate::gfxobj`] are answered here: each drawing hook is
//! `PlatGraphics`' statement, the [`crate::platgfx`] hook on the graphics' surface at its pixel scale in its colour
//! (read when the hook runs, as `this->mColor` was). The image draws and the text stay the C++ host's (the image and
//! font objects' calls).
use crate::gfxobj::{EeWonGfx, EeWonGfxCall, EeWonGfxHook as H, EeWonGfxHost, EeWonGfxState, Gfx};
use crate::platfont::{ee_won_plat_font_draw, EeWonPlatFont};
use crate::platgfx::{ee_won_plat_gfx, EeWonPlatGfx as P};
use crate::surface::EeWonSurface;
use std::ffi::c_void;

/// The surface's answer to a Graphics hook; `None` for the hooks that are not the surface's (the images, the text).
///
/// `DrawDottedRectHook` was Graphics' own (every other pixel of the four edges through `SetPixelHook`, the colour
/// read at each pixel); `SetColorHook` did nothing.
///
/// # Safety
/// `s` is null or a live surface no one else uses during the call; `st` is live.
pub unsafe fn surface_hook(
    s: *mut EeWonSurface,
    scale: f32,
    st: *const EeWonGfxState,
    h: H,
    a: &[i32; 7],
) -> Option<u32> {
    // SAFETY: live (the contract); read at each hook
    let color = || unsafe { (*st).color };
    // SAFETY: the contract
    let plat =
        |op: P, v: [i32; 6]| unsafe { ee_won_plat_gfx(s, scale, color(), op, v[0], v[1], v[2], v[3], v[4], v[5]) };
    let four = [a[0], a[1], a[2], a[3], 0, 0];
    Some(match h {
        H::ClearClipRect => plat(P::ClearClip, [0; 6]),
        H::SetClipRect => plat(P::SetClip, four),
        H::SetPixel => plat(P::SetPixel, [a[0], a[1], a[2], 0, 0, 0]),
        H::SetColor => 0,
        H::DrawLine => plat(P::Line, four),
        H::DrawRect => plat(P::Rect, four),
        H::FillRect => plat(P::FillRect, four),
        H::DrawDottedRect => {
            let (x, y, w, hh) = (a[0], a[1], a[2], a[3]);
            let mut i = 0;
            while i < w {
                plat(P::SetPixel, [x.wrapping_add(i), y, color() as i32, 0, 0, 0]);
                plat(P::SetPixel, [x.wrapping_add(i), y.wrapping_add(hh).wrapping_sub(1), color() as i32, 0, 0, 0]);
                i += 2;
            }
            let mut i = 0;
            while i < hh {
                plat(P::SetPixel, [x, y.wrapping_add(i), color() as i32, 0, 0, 0]);
                plat(P::SetPixel, [x.wrapping_add(w).wrapping_sub(1), y.wrapping_add(i), color() as i32, 0, 0, 0]);
                i += 2;
            }
            0
        }
        H::DrawEllipse => plat(P::Ellipse, four),
        H::FillEllipse => plat(P::FillEllipse, four),
        H::DrawArc => plat(P::Arc, [a[0], a[1], a[2], a[3], a[4], a[5]]),
        H::FillRectBlend => plat(P::FillRectBlend, [a[0], a[1], a[2], a[3], a[4], 0]),
        H::ImageEntire | H::ImagePart | H::String => return None,
    })
}

/// One Graphics method on the C++ object `g` (as `ee_won_gfx_call`), its drawing hooks answered on the surface `s`
/// (null: the graphics is attached to none) at pixel scale `scale`; the host gets the image and text hooks only.
///
/// # Safety
/// As `ee_won_gfx_call`; `s` is null or a live surface no one else uses during a drawing hook.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gfx_plat_call(
    g: *mut c_void,
    host: EeWonGfxHost,
    o: *mut EeWonGfx,
    st: *mut EeWonGfxState,
    scheme: *const u32,
    s: *mut EeWonSurface,
    scale: f32,
    call: EeWonGfxCall,
    a: *const i32,
    p: *const c_void,
) -> u32 {
    // SAFETY: the contract (the scheme copied, as `ee_won_gfx_call` does)
    let (args, scheme) =
        unsafe { (*(a as *const [i32; 7]), (!scheme.is_null()).then(|| *(scheme as *const [u32; 17]))) };
    let mut gfx = Gfx {
        st,
        obj: o,
        scheme,
        // SAFETY: the surface's and the host's contracts
        hook: |h: H, x: &[i32; 7], q: *const c_void| unsafe {
            match surface_hook(s, scale, st, h, x) {
                Some(v) => v,
                None => host(g, h, x.as_ptr(), q),
            }
        },
    };
    gfx.call(call, &args, p)
}

/// Where a Graphics draws (PlatGraphics' members): the surface of the display context it is attached to (borrowed:
/// the image that owns the context keeps it; null while detached or when the context has none) and the context's
/// pixel scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeWonGfxPlat {
    pub surface: *mut EeWonSurface,
    pub scale: f32,
}

/// A new Graphics': no surface, scale 1.
#[no_mangle]
pub extern "C" fn ee_won_gfx_plat_none() -> EeWonGfxPlat {
    EeWonGfxPlat { surface: std::ptr::null_mut(), scale: 1.0 }
}

/// `PlatGraphics::Attach`'s: the context's surface and its pixel scale (1 when it is not positive).
#[no_mangle]
pub extern "C" fn ee_won_gfx_plat_of(surface: *mut EeWonSurface, scale: f32) -> EeWonGfxPlat {
    EeWonGfxPlat { surface, scale: if scale > 0.0 { scale } else { 1.0 } }
}

/// `PlatGUIFont::DrawString(context, text, offset, length, x, y)` with a context: the text into the context's
/// surface `s` (null: none) at the context's pixel scale in its Graphics' colour without the top byte (black
/// without a Graphics); an underlined font's line through that Graphics' `DrawLineHook` (its surface, scale and
/// whole colour). `st` and `plat` are the context's Graphics' state and surface, both null without one.
///
/// # Safety
/// `f` is a live font; `s` and the Graphics' surface are null or live surfaces; `st` and `plat` are both null or
/// both live; `text` holds `len` units.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gfx_font_draw(
    f: *mut EeWonPlatFont,
    s: *mut EeWonSurface,
    scale: f32,
    st: *const EeWonGfxState,
    plat: *const EeWonGfxPlat,
    text: *const c_void,
    len: u16,
    x: i32,
    y: i32,
) {
    let graphics = !st.is_null() && !plat.is_null();
    // SAFETY: live when not null (the contract)
    let color = if graphics { unsafe { (*st).color } } else { 0 };
    let mut line = [0i32; 3];
    // SAFETY: the contract; three ints
    if unsafe { ee_won_plat_font_draw(f, s, scale, graphics, color & 0x00ff_ffff, text, len, x, y, line.as_mut_ptr()) }
    {
        // SAFETY: `graphics` (the font's answer is false without one); the Graphics' surface per the contract
        unsafe {
            ee_won_plat_gfx((*plat).surface, (*plat).scale, color, P::Line, line[0], line[1], line[2], line[1], 0, 0)
        };
    }
}

#[cfg(test)]
#[path = "gfxplat_tests.rs"]
mod tests;
