//! gfxplat: the folded Graphics against the two classes it replaces. The reference is `ee_won_gfx_call` under a host
//! written as Graphics.cpp's `GfxHost` and PlatGraphics.cpp's hooks were at 71c097464 (each virtual its PlatGraphics
//! statement; `DrawDottedRectHook` the base's, through `BaseDottedRect`); both run the same random calls on two
//! surfaces and must leave the same pixels, clip, state, answers and host calls.
use super::*;
use crate::gfxobj::{ee_won_gfx_call, ee_won_gfx_free, ee_won_gfx_new};
use crate::surface::{EeWonSurfaceField as F, EE_WON_SURFACE_32};

struct World {
    s: EeWonSurface,
    scale: f32,
    st: EeWonGfxState,
    obj: *mut EeWonGfx,
    scheme: Option<[u32; 17]>,
    /// the image and text hooks the host got: (hook, ints)
    log: Vec<(u32, [i32; 7])>,
}

impl World {
    fn new(w: i32, h: i32, scale: f32, scheme: Option<[u32; 17]>) -> Box<World> {
        let mut s = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
        s.create(w, h);
        Box::new(World { s, scale, st: EeWonGfxState::default(), obj: ee_won_gfx_new(), scheme, log: Vec::new() })
    }
    fn scheme_ptr(&self) -> *const u32 {
        self.scheme.as_ref().map_or(std::ptr::null(), |s| s.as_ptr())
    }
}

impl Drop for World {
    fn drop(&mut self) {
        // SAFETY: from ee_won_gfx_new
        unsafe { ee_won_gfx_free(self.obj) };
    }
}

/// The folded class's host: only the image and text hooks may arrive.
unsafe extern "C" fn new_host(g: *mut c_void, h: H, a: *const i32, _p: *const c_void) -> u32 {
    // SAFETY: the test's world and seven ints
    let (w, a) = unsafe { (&mut *(g as *mut World), *(a as *const [i32; 7])) };
    assert!(matches!(h, H::ImageEntire | H::ImagePart | H::String), "a drawing hook reached the host: {h:?}");
    w.log.push((h as u32, a));
    0
}

/// The two classes as they were: `GfxHost`'s cases over PlatGraphics' hooks.
unsafe extern "C" fn old_host(g: *mut c_void, h: H, a: *const i32, p: *const c_void) -> u32 {
    // SAFETY: the test's world and seven ints
    let (w, a) = unsafe { (&mut *(g as *mut World), *(a as *const [i32; 7])) };
    let (s, k, c) = (&mut w.s as *mut EeWonSurface, w.scale, w.st.color);
    // SAFETY: the world's surface
    let hook = |op: P, v: &[i32]| unsafe {
        let mut x = [0i32; 6];
        x[..v.len()].copy_from_slice(v);
        ee_won_plat_gfx(s, k, c, op, x[0], x[1], x[2], x[3], x[4], x[5])
    };
    match h {
        H::ClearClipRect => hook(P::ClearClip, &[]),
        H::SetClipRect => hook(P::SetClip, &a[..4]),
        H::SetPixel => hook(P::SetPixel, &a[..3]),
        H::SetColor => 0,
        H::DrawLine => hook(P::Line, &a[..4]),
        H::DrawRect => hook(P::Rect, &a[..4]),
        H::FillRect => hook(P::FillRect, &a[..4]),
        // PlatGraphics::DrawDottedRectHook: Graphics::DrawDottedRectHook, GCall(C::BaseDottedRect, x, y, w, h)
        H::DrawDottedRect => {
            let args = [a[0], a[1], a[2], a[3], 0, 0, 0];
            let (obj, st, scheme) = (w.obj, &mut w.st as *mut EeWonGfxState, w.scheme_ptr());
            // SAFETY: the world's members
            unsafe { ee_won_gfx_call(g, old_host, obj, st, scheme, EeWonGfxCall::BaseDottedRect, args.as_ptr(), p) }
        }
        H::DrawEllipse => hook(P::Ellipse, &a[..4]),
        H::FillEllipse => hook(P::FillEllipse, &a[..4]),
        H::DrawArc => hook(P::Arc, &a[..6]),
        H::FillRectBlend => hook(P::FillRectBlend, &a[..5]),
        H::ImageEntire | H::ImagePart | H::String | H::SetFont | H::Text | H::ImageSize => {
            w.log.push((h as u32, a));
            0
        }
    }
}

struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn range(&mut self, lo: i32, hi: i32) -> i32 {
        lo + (self.next() % (hi - lo + 1) as u32) as i32
    }
}

const CALLS: [EeWonGfxCall; 23] = [
    EeWonGfxCall::SetColor,
    EeWonGfxCall::StandardColor,
    EeWonGfxCall::SetSysColor,
    EeWonGfxCall::Translate,
    EeWonGfxCall::SetPixel,
    EeWonGfxCall::DrawImage,
    EeWonGfxCall::DrawImagePart,
    EeWonGfxCall::DrawLine,
    EeWonGfxCall::DrawRect,
    EeWonGfxCall::FillRect,
    EeWonGfxCall::FillRectBlend,
    EeWonGfxCall::DrawDottedRect,
    EeWonGfxCall::DrawEllipse,
    EeWonGfxCall::FillEllipse,
    EeWonGfxCall::DrawArc,
    EeWonGfxCall::DrawString,
    EeWonGfxCall::DrawStringPart,
    EeWonGfxCall::PerformClip,
    EeWonGfxCall::PushClip,
    EeWonGfxCall::PopClip,
    EeWonGfxCall::BaseRect,
    EeWonGfxCall::BaseDottedRect,
    EeWonGfxCall::BaseFillRect,
];

fn args(r: &mut Rng, call: EeWonGfxCall) -> [i32; 7] {
    let mut a = [0i32; 7];
    match call {
        EeWonGfxCall::SetColor => {
            a[0] = if r.next().is_multiple_of(3) { (0x8000_0000u32 + r.next() % 19) as i32 } else { r.next() as i32 }
        }
        EeWonGfxCall::StandardColor | EeWonGfxCall::SetSysColor => a[0] = r.range(-1, 18),
        EeWonGfxCall::Translate => {
            a[0] = r.range(-3, 3);
            a[1] = r.range(-3, 3);
        }
        EeWonGfxCall::SetPixel => {
            a[0] = r.range(-4, 40);
            a[1] = r.range(-4, 30);
            a[2] = r.next() as i32;
        }
        EeWonGfxCall::DrawArc => {
            a[0] = r.range(-4, 30);
            a[1] = r.range(-4, 20);
            a[2] = r.range(-2, 20);
            a[3] = r.range(-2, 20);
            a[4] = r.range(-400, 400);
            a[5] = r.range(-400, 400);
        }
        EeWonGfxCall::FillRectBlend => {
            a[0] = r.range(-4, 30);
            a[1] = r.range(-4, 20);
            a[2] = r.range(-2, 20);
            a[3] = r.range(-2, 20);
            a[4] = r.range(-10, 270);
        }
        _ => {
            for v in a.iter_mut().take(6) {
                *v = r.range(-4, 34);
            }
        }
    }
    a
}

fn run(seed: u64, scale: f32, scheme: Option<[u32; 17]>) -> usize {
    let mut r = Rng(seed);
    let (mut old, mut new) = (World::new(40, 30, scale, scheme), World::new(40, 30, scale, scheme));
    let mut steps = 0;
    for step in 0..400 {
        let call = CALLS[(r.next() % CALLS.len() as u32) as usize];
        let a = args(&mut r, call);
        let (po, pn) = (&mut *old as *mut World, &mut *new as *mut World);
        // SAFETY: the worlds' members; the hosts read their world only
        let (x, y) = unsafe {
            (
                ee_won_gfx_call(
                    po as *mut c_void,
                    old_host,
                    (*po).obj,
                    &mut (*po).st,
                    (*po).scheme_ptr(),
                    call,
                    a.as_ptr(),
                    std::ptr::null(),
                ),
                ee_won_gfx_plat_call(
                    pn as *mut c_void,
                    new_host,
                    (*pn).obj,
                    &mut (*pn).st,
                    (*pn).scheme_ptr(),
                    &mut (*pn).s,
                    (*pn).scale,
                    call,
                    a.as_ptr(),
                    std::ptr::null(),
                ),
            )
        };
        assert_eq!(x, y, "seed {seed} step {step} {call:?} {a:?}: answer");
        assert_eq!(old.st, new.st, "seed {seed} step {step} {call:?} {a:?}: state");
        // SAFETY: live
        assert_eq!(unsafe { &(*old.obj).stack }, unsafe { &(*new.obj).stack }, "seed {seed} step {step}: clip stack");
        assert_eq!(old.log, new.log, "seed {seed} step {step} {call:?}: host calls");
        assert_eq!(old.s.bytes(), new.s.bytes(), "seed {seed} step {step} {call:?} {a:?}: pixels");
        for f in [F::ClipX1, F::ClipY1, F::ClipX2, F::ClipY2, F::Color] {
            assert_eq!(old.s.field(f), new.s.field(f), "seed {seed} step {step} {call:?}: surface clip");
        }
        steps += 1;
    }
    steps
}

#[test]
fn the_folded_graphics_draws_as_the_two_classes_did() {
    let scheme: [u32; 17] = std::array::from_fn(|i| 0x0010_2030u32.wrapping_mul(i as u32 + 1));
    let mut steps = 0;
    for seed in 1..=60u64 {
        let scale = [1.0f32, 1.25, 1.5, 2.0][(seed % 4) as usize];
        steps += run(seed, scale, if seed % 3 == 0 { None } else { Some(scheme) });
    }
    assert_eq!(steps, 60 * 400);
}

#[test]
fn a_graphics_without_a_surface_draws_nothing_and_still_reaches_the_host() {
    let mut w = World::new(4, 4, 1.0, None);
    let p = &mut *w as *mut World;
    let before = w.s.bytes().to_vec();
    for call in CALLS {
        let a = [1, 1, 2, 2, 1, 1, 0];
        // SAFETY: the world's members; a null surface
        unsafe {
            ee_won_gfx_plat_call(
                p as *mut c_void,
                new_host,
                (*p).obj,
                &mut (*p).st,
                std::ptr::null(),
                std::ptr::null_mut(),
                1.0,
                call,
                a.as_ptr(),
                std::ptr::null(),
            );
        }
    }
    assert_eq!(w.s.bytes(), &before[..]);
    assert_eq!(w.log.len(), 4); // DrawImage, DrawImagePart, DrawString, DrawStringPart
}

#[test]
fn the_dotted_rectangle_reads_the_colour_per_pixel_and_set_color_is_silent() {
    let mut s = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
    s.create(8, 8);
    let st = EeWonGfxState { color: 0x00ff_ffff, ..Default::default() };
    // SAFETY: the surface and the state are live
    unsafe {
        assert_eq!(surface_hook(&mut s, 1.0, &st, H::SetColor, &[0; 7]), Some(0));
        assert_eq!(surface_hook(&mut s, 1.0, &st, H::DrawDottedRect, &[1, 1, 5, 4, 0, 0, 0]), Some(0));
        assert_eq!(surface_hook(&mut s, 1.0, &st, H::String, &[0; 7]), None);
    }
    assert_ne!(s.pixel(1, 1), 0);
    assert_eq!(s.pixel(2, 1), 0);
    assert_ne!(s.pixel(3, 1), 0);
    assert_ne!(s.pixel(5, 3), 0); // the right edge, x + w - 1
    assert_ne!(s.pixel(1, 4), 0); // the bottom edge, y + h - 1
    assert_eq!(s.pixel(0, 0), 0);
}

// ---- the font's draw with its underline (PlatGUIFont::DrawString at 71c097464: the Rust font's draw, then the
// context's Graphics' DrawLineHook with the line it answers)

const FONT: &[u8] = include_bytes!("../../ee_base/src/plat_ui/golden_font.ttf");

fn register() {
    // SAFETY: a static face's data
    unsafe {
        ee_base::plat_ui::font::PlatFontRegister(
            c"Synth".as_ptr(),
            0,
            0,
            FONT.as_ptr() as *const c_void,
            FONT.len() as u32,
        )
    };
}

fn text_surface(seed: u32) -> EeWonSurface {
    let mut s = EeWonSurface::new(EE_WON_SURFACE_32).unwrap();
    s.create(90, 40);
    let mut v = seed;
    for p in s.words_mut().unwrap() {
        v = v.wrapping_mul(1664525).wrapping_add(1013904223);
        *p = v & 0x00ff_ffff;
    }
    s
}

/// The C++ statement as it was: `if (ee_won_plat_font_draw(.., theLine)) theGraphics->DrawLineHook(theLine[0],
/// theLine[1], theLine[2], theLine[1])`, the hook PlatGraphics' on its own surface and scale in its whole colour.
#[allow(clippy::too_many_arguments)]
unsafe fn old_draw(
    f: *mut EeWonPlatFont,
    ctx: *mut EeWonSurface,
    ctx_scale: f32,
    graphics: Option<(u32, *mut EeWonSurface, f32)>,
    text: &[u16],
    x: i32,
    y: i32,
) {
    let mut line = [0i32; 3];
    let color = graphics.map_or(0, |g| g.0 & 0x00ff_ffff);
    // SAFETY: the caller's
    unsafe {
        if ee_won_plat_font_draw(
            f,
            ctx,
            ctx_scale,
            graphics.is_some(),
            color,
            text.as_ptr() as *const c_void,
            text.len() as u16,
            x,
            y,
            line.as_mut_ptr(),
        ) {
            let (c, s, k) = graphics.unwrap();
            ee_won_plat_gfx(s, k, c, P::Line, line[0], line[1], line[2], line[1], 0, 0);
        }
    }
}

#[test]
fn the_font_draws_its_text_and_underline_as_the_class_did() {
    if std::mem::size_of::<ee_base::plat_core::string::WChar>() != 2 {
        return; // the texts below are UTF-16 units
    }
    register();
    let text: Vec<u16> = "Wave, link".encode_utf16().collect();
    let mut cases = 0;
    for style in 0..8 {
        for (i, &scale) in [1.0f32, 2.0, 0.0, 1.5].iter().enumerate() {
            for with_graphics in [true, false] {
                for own_surface in [true, false] {
                    let (mut a, mut b) = (text_surface(7), text_surface(7));
                    // a Graphics attached to another surface than the context's draws its line there
                    let (mut ga, mut gb) = (text_surface(9), text_surface(9));
                    let color = 0xff00_0000u32 | (0x0040_80c0u32.wrapping_mul(style as u32 + 1) & 0x00ff_ffff);
                    // SAFETY: live fonts and surfaces
                    unsafe {
                        let fa = crate::platfont::ee_won_plat_font_new(c"Synth".as_ptr(), 11 + i as i32, style);
                        let fb = crate::platfont::ee_won_plat_font_new(c"Synth".as_ptr(), 11 + i as i32, style);
                        let (sa, sb): (*mut EeWonSurface, *mut EeWonSurface) =
                            if own_surface { (&mut ga, &mut gb) } else { (&mut a, &mut b) };
                        let plat_scale = if scale > 0.0 { scale } else { 1.0 };
                        old_draw(fa, &mut a, scale, with_graphics.then_some((color, sa, plat_scale)), &text, 3, 4);
                        let st = EeWonGfxState { color, ..Default::default() };
                        let plat = ee_won_gfx_plat_of(sb, scale);
                        assert_eq!(plat.scale, plat_scale);
                        ee_won_gfx_font_draw(
                            fb,
                            &mut b,
                            scale,
                            if with_graphics { &st } else { std::ptr::null() },
                            if with_graphics { &plat } else { std::ptr::null() },
                            text.as_ptr() as *const c_void,
                            text.len() as u16,
                            3,
                            4,
                        );
                        crate::platfont::ee_won_plat_font_free(fa);
                        crate::platfont::ee_won_plat_font_free(fb);
                    }
                    assert_eq!(a.bytes(), b.bytes(), "style {style} scale {scale} graphics {with_graphics}");
                    assert_eq!(ga.bytes(), gb.bytes(), "style {style} scale {scale}: the Graphics' surface");
                    assert_ne!(a.bytes(), text_surface(7).bytes(), "nothing drawn");
                    cases += 1;
                }
            }
        }
    }
    assert_eq!(cases, 8 * 4 * 2 * 2);
}

#[test]
fn an_underlined_font_draws_a_line_only_with_a_graphics() {
    if std::mem::size_of::<ee_base::plat_core::string::WChar>() != 2 {
        return;
    }
    register();
    let text: Vec<u16> = "abc".encode_utf16().collect();
    let run = |with: bool| {
        let (mut s, mut line) = (text_surface(1), text_surface(2));
        let st = EeWonGfxState { color: 0x00ff_ffff, ..Default::default() };
        let plat = ee_won_gfx_plat_of(&mut line, 1.0);
        // SAFETY: live font and surfaces
        unsafe {
            let f = crate::platfont::ee_won_plat_font_new(c"Synth".as_ptr(), 12, 4); // FontStyle_Underline
            ee_won_gfx_font_draw(
                f,
                &mut s,
                1.0,
                if with { &st } else { std::ptr::null() },
                if with { &plat } else { std::ptr::null() },
                text.as_ptr() as *const c_void,
                3,
                2,
                2,
            );
            crate::platfont::ee_won_plat_font_free(f);
        }
        line.bytes() != text_surface(2).bytes()
    };
    assert!(run(true));
    assert!(!run(false));
    assert_eq!(ee_won_gfx_plat_none().surface, std::ptr::null_mut());
    assert_eq!(ee_won_gfx_plat_none().scale, 1.0);
}
