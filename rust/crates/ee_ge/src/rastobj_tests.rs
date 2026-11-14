//! `rastobj.rs`'s tests: unit tests of each method over a mock host, and the golden driver's replay.
//!
//! The golden (`golden_grast.txt`) is the digest of each of the first sequences of the scratch driver over the
//! original GERasterizer.cpp (R4.3 part 12's census commit, unmodified) and traced fakes: random rasterizers,
//! activations that add screens, the base's setters, the plug-in's writes through the screen records, viewports, font
//! bitmaps, the pixel sizes and the two visibility tests, the rasterizer list and the active rasterizer, each step
//! followed by every live rasterizer's state through the accessors. `golden` below is that driver ported onto the
//! Rust object's C ABI: the mock host is the fakes, drawing the same random stream and writing the same lines.

// the driver's random draws keep its `R() % n` shapes
#![allow(clippy::manual_is_multiple_of)]

use super::*;
use crate::types::{GeRectangle, GeTransformation};
use crate::visible_ffi::ee_ge_viewport_init;
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

// ---- the random stream and the trace (the driver's)
struct World {
    seed: u64,
    hash: u64,
    next_serial: i32,
    lines: Option<Vec<String>>,
    /// The live rasterizers' owners, by construction.
    rasts: Vec<*mut Rast>,
    rast_serial: BTreeMap<usize, i32>,
}

thread_local! {
    static WORLD: RefCell<World> = const { RefCell::new(World {
        seed: 0,
        hash: 0,
        next_serial: 0,
        lines: None,
        rasts: Vec::new(),
        rast_serial: BTreeMap::new(),
    }) };
}

fn w<T>(f: impl FnOnce(&mut World) -> T) -> T {
    WORLD.with(|x| f(&mut x.borrow_mut()))
}

fn trace(line: String) {
    w(|w| {
        for b in line.bytes().chain(std::iter::once(10u8)) {
            w.hash = (w.hash ^ b as u64).wrapping_mul(0x100000001b3);
        }
        if let Some(lines) = &mut w.lines {
            lines.push(line);
        }
    })
}

fn r() -> u32 {
    w(|w| {
        w.seed = w.seed.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (w.seed >> 33) as u32
    })
}

fn f() -> f32 {
    const SPECIAL: [u32; 10] = [
        0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7149f2ca,
        0x3a83126f,
    ];
    let k = r() % 16;
    if k < 10 && r() % 4 == 0 {
        return f32::from_bits(SPECIAL[k as usize]);
    }
    ((r() % 2000001) as i32 - 1000000) as f32 / 1000.0
}

fn range(low: i32, high: i32) -> f32 {
    (low * 100 + (r() % ((high - low) * 100 + 1) as u32) as i32) as f32 / 100.0
}

fn bits(f: f32) -> u32 {
    f.to_bits()
}

// ---- the fakes
#[derive(Default)]
struct Fake {
    kind: &'static str,
    serial: i32,
    refs: i64,
    dead: bool,
    font: usize,
    manager: usize,
    center: Ge3DPoint,
    radius: [f32; 4],
    graphics_radius: f32,
    bbox: GeBoundingBox,
    internal: [GeTransformation; 4],
    model: usize,
    frame: c_ulong,
    scale: f32,
    location: GeTransformation,
}

fn fake(kind: &'static str) -> *mut Fake {
    let serial = w(|w| {
        w.next_serial += 1;
        w.next_serial
    });
    Box::into_raw(Box::new(Fake { kind, serial, refs: 1, scale: 1.0, ..Fake::default() }))
}

fn fk<'a>(p: usize) -> &'a mut Fake {
    // SAFETY: every fake is leaked for the test's life
    unsafe { &mut *(p as *mut Fake) }
}

fn serial(p: usize) -> i32 {
    if p == 0 {
        0
    } else {
        fk(p).serial
    }
}

fn release(p: usize) {
    let x = fk(p);
    if x.dead {
        trace(format!("deref dead {} {}", x.kind, x.serial));
        return;
    }
    trace(format!("deref {} {} -> {}", x.kind, x.serial, x.refs - 1));
    x.refs -= 1;
    if x.refs == 0 {
        x.dead = true;
    }
}

fn reference(p: usize) {
    let x = fk(p);
    x.refs += 1;
    trace(format!("ref {} {} -> {}", x.kind, x.serial, x.refs));
}

/// A rasterizer of the driver (the C++ `FakeRast`): its owner token is this record's address.
struct Rast {
    serial: i32,
    rust: *mut EeGeRasterizer,
}

fn rast_serial(owner: usize) -> i32 {
    if owner == 0 {
        return 0;
    }
    w(|w| w.rast_serial.get(&owner).copied().unwrap_or(-1))
}

fn color(c: *const EeGeRastColor) -> String {
    // SAFETY: the host's contract
    let c = unsafe { *c };
    format!("{:08x} {:08x} {:08x}", bits(c.red), bits(c.green), bits(c.blue))
}

unsafe extern "C" fn host(owner: *mut c_void, op: EeGeRastOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let obj = obj as usize;
    let fbits = |v: f32| bits(v) as isize;
    match op {
        EeGeRastOp::NameNew => {
            // SAFETY: a is the driver's name
            let name = unsafe { (*(a as *const String)).clone() };
            trace(format!("UString copy {name}"));
            Box::into_raw(Box::new(name)) as isize
        }
        EeGeRastOp::NameDelete => {
            // SAFETY: made by NameNew
            let name = unsafe { Box::from_raw(obj as *mut String) };
            trace(format!("UString dtor {name}"));
            0
        }
        EeGeRastOp::LockCreate => {
            let cs = fake("cs");
            let result = if r() % 8 == 0 { 9 } else { 0 };
            trace(format!("cs create {} {}", serial(cs as usize), result));
            // SAFETY: a points at the lock's slot
            unsafe { *(a as *mut usize) = cs as usize };
            result
        }
        EeGeRastOp::LockDestroy => {
            trace(format!("cs destroy {}", serial(obj)));
            0
        }
        EeGeRastOp::Verify => {
            if a == 0 {
                trace(format!("verify failed {b}"));
            }
            0
        }
        EeGeRastOp::FontNew => {
            let font = fake("font") as usize;
            trace(format!("font new {} Arial Bold 9 0 2", serial(font)));
            trace("UString dtor Arial Bold".into());
            font as isize
        }
        EeGeRastOp::FontDereference => {
            release(obj);
            0
        }
        EeGeRastOp::TextNew => {
            let text = fake("text") as usize;
            fk(text).font = a as usize;
            trace(format!("text new {} font {}", serial(text), serial(a as usize)));
            reference(a as usize);
            text as isize
        }
        EeGeRastOp::TextDelete => {
            trace(format!("text delete {}", serial(obj)));
            fk(obj).dead = true;
            release(fk(obj).font);
            0
        }
        EeGeRastOp::TextSetAlignment => {
            trace(format!("text {} align {}", serial(obj), a));
            0
        }
        EeGeRastOp::TextSetFgColor => {
            trace(format!("text {} fg {}", serial(obj), color(a as *const EeGeRastColor)));
            0
        }
        EeGeRastOp::TextSetBgColor => {
            trace(format!("text {} bg {}", serial(obj), color(a as *const EeGeRastColor)));
            0
        }
        EeGeRastOp::TextSetFont => {
            trace(format!("text {} font {}", serial(obj), serial(a as usize)));
            release(fk(obj).font);
            fk(obj).font = a as usize;
            reference(a as usize);
            0
        }
        EeGeRastOp::TextSetIsTransparent => {
            trace(format!("text {} transparent {}", serial(obj), (a != 0) as i32));
            0
        }
        EeGeRastOp::TextSetLocation => {
            trace(format!("text {} location {:08x} {:08x}", serial(obj), a as u32, b as u32));
            0
        }
        EeGeRastOp::ActivateRasterizer => {
            // SAFETY: the owner is a live Rast
            let rast = unsafe { &*(owner as *const Rast) };
            let count = r() % 3;
            trace(format!("activate R{} adds {}", rast.serial, count));
            for _ in 0..count {
                let screen = fake("screen") as usize;
                if r() % 2 != 0 {
                    fk(screen).manager = fake("manager") as usize;
                }
                // SAFETY: the rasterizer is live
                unsafe { ee_ge_rast_add_screen(rast.rust, screen as *mut c_void) };
            }
            if r() % 2 != 0 {
                ee_ge_rast_set_active(owner);
            }
            if r() % 4 == 0 {
                9
            } else {
                0
            }
        }
        EeGeRastOp::FontBitmapNew => {
            let x = fake("fontbitmap") as usize;
            trace(format!(
                "fontbitmap new {} rast {} font {} screen {}",
                serial(x),
                rast_serial(owner as usize),
                serial(a as usize),
                b
            ));
            0
        }
        EeGeRastOp::SurfaceDereference | EeGeRastOp::TextureDereference => {
            release(obj);
            0
        }
        EeGeRastOp::ModelCenter => {
            // SAFETY: a points at a point
            unsafe { *(a as *mut Ge3DPoint) = fk(obj).center };
            0
        }
        EeGeRastOp::ModelBoundingRadius => fbits(fk(obj).radius[a as usize % 4]),
        EeGeRastOp::ModelGraphicsRadius => fbits(fk(obj).graphics_radius),
        EeGeRastOp::ModelGraphicsBox => {
            // SAFETY: a points at a box
            unsafe { *(a as *mut GeBoundingBox) = fk(obj).bbox };
            0
        }
        EeGeRastOp::ModelSetClip => {
            trace(format!("model {} clip {} {}", serial(obj), a, (b != 0) as i32));
            0
        }
        EeGeRastOp::ModelInternalTransform => {
            // SAFETY: b points at a transformation
            unsafe { *(b as *mut GeTransformation) = fk(obj).internal[a as usize % 4] };
            0
        }
        EeGeRastOp::PlacedScale => fbits(fk(obj).scale),
        EeGeRastOp::PlacedFrame => fk(obj).frame as isize,
        EeGeRastOp::PlacedModel => fk(obj).model as isize,
        EeGeRastOp::PlacedLocation => {
            // SAFETY: a points at a transformation
            unsafe { *(a as *mut GeTransformation) = fk(obj).location };
            0
        }
    }
}

// ---- the dump
fn dump() {
    let n = ee_ge_rast_list(2, std::ptr::null_mut(), 0) as usize;
    trace(format!("list {} active R{}", n, rast_serial(ee_ge_rast_active() as usize)));
    for i in 0..n {
        trace(format!(" list {} R{}", i, rast_serial(ee_ge_rast_list(3, std::ptr::null_mut(), i) as usize)));
    }
    let rasts = w(|w| w.rasts.clone());
    for owner in rasts {
        // SAFETY: the rasterizers in the list are live
        unsafe {
            let x = &*owner;
            let p = x.rust;
            let get = |v| ee_ge_rast_get(p, v);
            let mut ripple = [0.0f32; 3];
            ee_ge_rast_ripple(p, &mut ripple, false);
            let name = &*(get(EeGeRastVal::Name) as *const String);
            trace(format!(
                "R{} act {} cull {} prof {} id {} screens {} name {} lock {} int {:08x} ripple {:08x} {:08x} {:08x}",
                x.serial,
                get(EeGeRastVal::IsActivated),
                get(EeGeRastVal::Culling) as c_ulong,
                get(EeGeRastVal::IsPolyAreaProfiling),
                get(EeGeRastVal::ScreenId) as c_ulong,
                get(EeGeRastVal::ScreenCount),
                name,
                serial(get(EeGeRastVal::DrawingAccess) as usize),
                get(EeGeRastVal::AmbientIntensity) as u32,
                bits(ripple[0]),
                bits(ripple[1]),
                bits(ripple[2])
            ));
            trace(format!(" ambient {}", color(ee_ge_rast_color(p, false))));
            trace(format!(" computed {}", color(ee_ge_rast_color(p, true))));
            for s in 0..get(EeGeRastVal::ScreenCount) as c_ulong {
                let t = &*ee_ge_rast_screen(p, s);
                trace(format!(
                    " s{} scr {} prim {} sec {} rt {} tex {} win {} mgr {} act {} vsync {}",
                    s,
                    serial(t.screen as usize),
                    serial(t.primary_surface as usize),
                    serial(t.secondary_surface as usize),
                    serial(t.render_target as usize),
                    serial(t.texture as usize),
                    serial(t.window as usize),
                    serial(fk(t.screen as usize).manager),
                    t.is_screen_activated as i32,
                    t.wait_for_vsync as i32
                ));
                trace(format!(
                    "  at {} atc {} atr {:08x} aa {} bl {} {} ck {} di {} ds {} fog {} li {} pm {} sh {} scs {} sp {} zb {} zc {}",
                    t.alpha_testing,
                    t.alpha_testing_compare_state,
                    bits(t.alpha_testing_ref_value),
                    t.anti_aliasing,
                    t.blend_source,
                    t.blend_destination,
                    t.color_keying,
                    t.dithering,
                    t.draw_style,
                    t.fog_state,
                    t.lighting,
                    t.projection_mode,
                    t.shading,
                    t.shading_color_source,
                    t.specular,
                    t.z_buffer,
                    t.z_compare
                ));
                let fog = t.fog;
                trace(format!(
                    "  fogattr {} {:08x} {} {:08x} {:08x} {:08x}",
                    fog.kind,
                    bits(fog.density),
                    color(&fog.color),
                    bits(fog.start),
                    bits(fog.end),
                    bits(fog.scale)
                ));
                trace(format!(
                    "  stats {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} dpc {} c {} {} {} {} has {}",
                    bits(t.drawn_poly_area),
                    bits(t.drawn_poly_area_per_frame),
                    bits(t.drawn_poly_area_per_second),
                    bits(t.drawn_polys_per_frame),
                    bits(t.drawn_polys_per_second),
                    bits(t.frames_per_second),
                    bits(t.scene_polys_per_frame),
                    bits(t.scene_polys_per_second),
                    bits(t.total_texture_changes_per_frame),
                    bits(t.alpha_testing_ref_value),
                    t.drawn_poly_count,
                    t.counters[0],
                    t.counters[1],
                    t.counters[2],
                    t.counters[3],
                    t.has_texture as i32
                ));
                let bytes = std::slice::from_raw_parts(
                    &t.viewport as *const GeViewport as *const u8,
                    std::mem::size_of::<GeViewport>(),
                );
                let mut h: u64 = 0xcbf29ce484222325;
                for &b in bytes {
                    h = (h ^ b as u64).wrapping_mul(0x100000001b3);
                }
                trace(format!("  viewport {h:016x}"));
            }
        }
    }
}

// ---- the objects the steps use
fn rect() -> GeRectangle {
    let (l, t, wd, ht) = (range(-500, 500), range(-500, 500), range(0, 900), range(0, 900));
    GeRectangle { left: l, top: t, right: l + wd, bottom: t + ht }
}

fn transform() -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    if r() % 4 == 0 {
        for i in 0..4 {
            for j in 0..4 {
                t.m[i][j] = if i == 3 && j < 3 { 0.0 } else { range(-2, 2) };
            }
        }
    }
    t.m[0][3] = range(-60, 60);
    t.m[1][3] = range(-60, 600);
    t.m[2][3] = range(-60, 60);
    if r() % 8 == 0 {
        t.m[0][3] = f();
    }
    t
}

fn viewport() -> GeViewport {
    let (view, ortho) = (rect(), rect());
    let mut camera = GeTransformation::IDENTITY;
    if r() % 2 != 0 {
        camera.m[0][3] = range(-20, 20);
        camera.m[1][3] = range(-200, 20);
        camera.m[2][3] = range(-20, 20);
    }
    let (front, depth, fov) = (range(0, 20), range(0, 1000), range(0, 3));
    let mut v = GeViewport::default();
    // SAFETY: valid pointers
    unsafe { ee_ge_viewport_init(&mut v, &view, &ortho, &camera, front, front + depth, fov) };
    v
}

fn model() -> usize {
    let m = fake("model") as usize;
    let (cx, cy, cz) = (range(-50, 50), range(-50, 600), range(-50, 50));
    fk(m).center = Ge3DPoint { x: cx, y: cy, z: cz };
    for i in 0..4 {
        fk(m).radius[i] = if r() % 10 == 0 { f() } else { range(0, 60) };
        fk(m).internal[i] = transform();
    }
    fk(m).graphics_radius = if r() % 10 == 0 { f() } else { range(0, 60) };
    let (lx, ly, lz) = (range(-80, 80), range(-80, 600), range(-80, 80));
    let (dx, dy, dz) = (range(0, 100), range(0, 100), range(0, 100));
    fk(m).bbox =
        GeBoundingBox::new(Ge3DPoint { x: lx, y: ly, z: lz }, Ge3DPoint { x: lx + dx, y: ly + dy, z: lz + dz });
    m
}

fn rgb() -> EeGeRastColor {
    let (red, green, blue) = (f(), f(), f());
    EeGeRastColor { red, green, blue }
}

/// The driver's `Sequence(index)`: its digest.
fn sequence(index: u32, verbose: bool) -> (u64, Vec<String>) {
    w(|w| {
        w.seed = 0x9e3779b97f4a7c15u64 ^ (index as u64).wrapping_mul(0x2545f4914f6cdd1d);
        w.hash = 0xcbf29ce484222325;
        w.next_serial = 0;
        w.lines = verbose.then(Vec::new);
    });
    trace(format!("sequence {index}"));
    let steps = 10 + r() % 60;
    for _ in 0..steps {
        let k = r() % 24;
        let rasts = w(|w| w.rasts.clone());
        let pick = if rasts.is_empty() { None } else { Some(rasts[(r() % rasts.len() as u32) as usize]) };
        // SAFETY: every rasterizer in the list is live; the pointers handed over are the fakes'
        unsafe {
            let n = pick.map_or(0, |x| ee_ge_rast_get((*x).rust, EeGeRastVal::ScreenCount) as c_ulong);
            let s: c_ulong = if n == 0 { 0 } else { r() as c_ulong % n };
            if k == 0 || pick.is_none() {
                let serial = w(|w| {
                    w.next_serial += 1;
                    w.next_serial
                });
                let name = format!("R{serial}");
                trace(format!("do new {name}"));
                let owner = Box::into_raw(Box::new(Rast { serial, rust: std::ptr::null_mut() }));
                w(|w| w.rast_serial.insert(owner as usize, serial));
                (*owner).rust = ee_ge_rast_new(host, owner as *mut c_void, &name as *const String as *const c_void);
                trace(format!("UString dtor {name}"));
                w(|w| w.rasts.push(owner));
                dump();
                continue;
            }
            let x = pick.unwrap_or(std::ptr::null_mut());
            let rs = (*x).serial;
            let p = (*x).rust;
            if k == 1 && rasts.len() > 1 {
                trace(format!("do delete R{rs}"));
                w(|w| w.rasts.retain(|&y| y != x));
                ee_ge_rast_destroy(p);
            } else if k == 2 || k == 3 {
                trace(format!("do activate R{rs}"));
                let result = ee_ge_rast_activate(p);
                trace(format!("activate {result}"));
            } else if k == 4 {
                trace(format!("do addscreen R{rs}"));
                let screen = fake("screen");
                ee_ge_rast_add_screen(p, screen as *mut c_void);
                trace("add 0".into());
            } else if k == 5 && r() % 3 == 0 {
                trace(format!("do deactivate R{rs}"));
                ee_ge_rast_deactivate(p);
                trace("deactivate 0".into());
            } else if k == 6 && n > 0 {
                let which = r() % 22;
                let v: c_ulong = if r() % 4 == 0 { r() as c_ulong } else { (r() % 5) as c_ulong };
                let fl = f();
                trace(format!("do set R{rs} s{s} {which} {v} {:08x}", bits(fl)));
                let set = |state, value: isize| ee_ge_rast_screen_set(p, s, state, value);
                match which {
                    0 => set(EeGeRastState::AlphaTesting, v as isize),
                    1 => set(EeGeRastState::AlphaTestingCompareState, v as isize),
                    2 => set(EeGeRastState::AlphaTestingRefValue, bits(fl) as isize),
                    3 => set(EeGeRastState::AntiAliasing, v as isize),
                    4 => ee_ge_rast_set_blending(p, s, v, v + 1),
                    5 => set(EeGeRastState::ColorKeying, v as isize),
                    6 | 7 => {}
                    8 => set(EeGeRastState::Dithering, v as isize),
                    9 => set(EeGeRastState::DrawStyle, v as isize),
                    10 => set(EeGeRastState::FogState, v as isize),
                    11 => {
                        let c = rgb();
                        ee_ge_rast_set_fog_color(p, s, &c);
                    }
                    12 => {
                        let end = f();
                        ee_ge_rast_set_fog_range(p, s, fl, end);
                    }
                    13 => set(EeGeRastState::FogType, v as isize),
                    14 => set(EeGeRastState::FogDensity, bits(fl) as isize),
                    15 => set(EeGeRastState::Lighting, v as isize),
                    16 => set(EeGeRastState::ProjectionMode, v as isize),
                    17 => set(EeGeRastState::Shading, v as isize),
                    18 => set(EeGeRastState::ShadingColorSource, v as isize),
                    19 => set(EeGeRastState::Specular, v as isize),
                    20 => {
                        set(EeGeRastState::ZBuffer, v as isize);
                        set(EeGeRastState::ZCompare, (v + 2) as isize);
                    }
                    _ => set(EeGeRastState::WaitForVSync, (v & 1) as isize),
                }
            } else if k == 7 && n > 0 {
                trace(format!("do viewport R{rs} s{s}"));
                let v = viewport();
                ee_ge_rast_set_viewport(p, s, &v);
            } else if k == 8 && n > 0 {
                let t = &mut *ee_ge_rast_screen(p, s);
                let which = r() % 6;
                trace(format!("do record R{rs} s{s} {which}"));
                match which {
                    0 => t.primary_surface = fake("surface") as *mut c_void,
                    1 => t.secondary_surface = fake("surface") as *mut c_void,
                    2 => t.render_target = fake("surface") as *mut c_void,
                    3 => {
                        t.texture = fake("texture") as *mut c_void;
                        t.has_texture = true;
                    }
                    4 => {
                        t.window = fake("window") as *mut c_void;
                        t.is_screen_activated = true;
                    }
                    _ => {
                        let i = (r() % 4) as usize;
                        t.counters[i] += (r() % 100) as c_ulong;
                        t.drawn_poly_count += (r() % 50) as c_ulong;
                        t.drawn_poly_area += range(0, 10);
                        t.frames_per_second = range(0, 100);
                    }
                }
            } else if k == 9 {
                let font = fake("font") as usize;
                trace(format!("font new {} Face 1 2 3", serial(font)));
                trace("UString dtor Face".into());
                trace(format!("do fontbitmap R{rs} F{} s{s}", serial(font)));
                ee_ge_rast_create_font_bitmap(p, font as *mut c_void, s);
                trace("fontbitmap 0".into());
            } else if k == 10 || k == 11 {
                let m = model();
                let v = if n > 0 && r() % 2 != 0 { (*ee_ge_rast_screen(p, s)).viewport } else { viewport() };
                let t = transform();
                let scale = if r() % 3 == 0 { range(0, 4) } else { 1.0 };
                let ortho = r() % 2 != 0;
                trace(format!("do pixelsize R{rs} M{} {}", serial(m), ortho as i32));
                let size = ee_ge_rast_pixel_size(p, m as *mut c_void, &v, &t, scale, ortho);
                trace(format!("pixelsize {:08x}", bits(size)));
            } else if k == 12 {
                let placed = fake("placed") as usize;
                fk(placed).model = model();
                fk(placed).frame = (r() % 4) as c_ulong;
                fk(placed).scale = if r() % 2 != 0 {
                    1.0
                } else if r() % 8 == 0 {
                    f()
                } else {
                    range(0, 4)
                };
                fk(placed).location = transform();
                let v = viewport();
                let ortho = r() % 2 != 0;
                trace(format!("do placedsize R{rs} P{} {}", serial(placed), ortho as i32));
                let size = ee_ge_rast_placed_pixel_size(p, placed as *const c_void, &v, ortho);
                trace(format!("placedsize {:08x}", bits(size)));
            } else if (13..=16).contains(&k) {
                let m = model();
                let v = if n > 0 && r() % 2 != 0 { (*ee_ge_rast_screen(p, s)).viewport } else { viewport() };
                let t = transform();
                let scale = if r() % 3 == 0 { range(0, 4) } else { 1.0 };
                let (mut visible, mut clipped) = (r() % 2 != 0, r() % 2 != 0);
                if k <= 14 {
                    trace(format!("do perspective R{rs} M{}", serial(m)));
                    ee_ge_rast_model_visible(p, m as *mut c_void, scale, &v, &t, false, &mut visible, &mut clipped);
                } else {
                    trace(format!("do orthographic R{rs} M{}", serial(m)));
                    ee_ge_rast_model_visible(p, m as *mut c_void, 1.0, &v, &t, true, &mut visible, &mut clipped);
                }
                trace(format!("visible {} clipped {}", visible as i32, clipped as i32));
            } else if k == 17 {
                let c = rgb();
                trace(format!("do ambient R{rs}"));
                ee_ge_rast_set_ambient_color(p, &c);
            } else if k == 18 {
                let fl = if r() % 2 != 0 { f() } else { range(0, 1) };
                trace(format!("do intensity R{rs} {:08x}", bits(fl)));
                ee_ge_rast_set_ambient_intensity(p, fl);
            } else if k == 19 {
                let which = r() % 4;
                let v = (r() % 4) as isize;
                trace(format!("do misc R{rs} {which} {v}"));
                match which {
                    0 => ee_ge_rast_set(p, EeGeRastVal::IsPolyAreaProfiling, v & 1),
                    1 => ee_ge_rast_set(p, EeGeRastVal::ScreenId, v),
                    2 => ee_ge_rast_set(p, EeGeRastVal::Culling, v),
                    _ => {
                        let mut ripple = [f(), f(), f()];
                        ee_ge_rast_ripple(p, &mut ripple, true);
                    }
                }
            } else if k == 20 || k == 21 {
                let op = if r() % 4 == 0 { 0 } else { 1 };
                trace(format!("do list {op} R{rs}"));
                ee_ge_rast_list(op, x as *mut c_void, 0);
            } else if k == 22 {
                trace(format!("do setactive R{rs}"));
                ee_ge_rast_set_active(if r() % 3 == 0 { std::ptr::null_mut() } else { x as *mut c_void });
            } else {
                trace("do none".into());
            }
        }
        dump();
    }
    let rasts = w(|w| std::mem::take(&mut w.rasts));
    for x in rasts {
        // SAFETY: live until destroyed here
        unsafe {
            if r() % 2 != 0 {
                trace(format!("end deactivate R{}", (*x).serial));
                ee_ge_rast_deactivate((*x).rust);
            }
            trace(format!("end delete R{}", (*x).serial));
            ee_ge_rast_destroy((*x).rust);
        }
    }
    w(|w| w.rast_serial.clear());
    ee_ge_rast_list(0, std::ptr::null_mut(), 0);
    ee_ge_rast_set_active(std::ptr::null_mut());
    w(|w| (w.hash, w.lines.take().unwrap_or_default()))
}

/// The statics are process-wide: the tests that touch them take this lock.
static STATICS: Mutex<()> = Mutex::new(());

#[test]
fn golden_sequences_replay_with_the_same_digests() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    let golden = include_str!("golden_grast.txt");
    let mut count = 0;
    for line in golden.lines() {
        let (index, digest) = line.split_once(' ').expect("index digest");
        let index: u32 = index.parse().expect("index");
        let digest = u64::from_str_radix(digest, 16).expect("digest");
        let (got, _) = sequence(index, false);
        if got != digest {
            let (_, lines) = sequence(index, true);
            panic!("sequence {index}: digest {got:016x}, golden {digest:016x}; its trace:\n{}", lines.join("\n"));
        }
        count += 1;
    }
    assert!(count >= 40, "{count} golden sequences");
}

// ---- unit tests over a small mock host
thread_local! {
    static LOG: RefCell<Vec<(EeGeRastOp, isize, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn log_host(_owner: *mut c_void, op: EeGeRastOp, _obj: *mut c_void, a: isize, b: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b)));
    match op {
        EeGeRastOp::FontNew => 0x100,
        EeGeRastOp::TextNew => 0x200,
        EeGeRastOp::NameNew => 0x300,
        EeGeRastOp::ModelBoundingRadius | EeGeRastOp::ModelGraphicsRadius => 2.0f32.to_bits() as isize,
        EeGeRastOp::ModelGraphicsBox => {
            // SAFETY: a points at a box
            unsafe {
                *(a as *mut GeBoundingBox) =
                    GeBoundingBox::new(Ge3DPoint { x: -1.0, y: 4.0, z: -1.0 }, Ge3DPoint { x: 1.0, y: 6.0, z: 1.0 })
            };
            0
        }
        EeGeRastOp::ModelCenter => {
            // SAFETY: a points at a point
            unsafe { *(a as *mut Ge3DPoint) = Ge3DPoint { x: 0.0, y: 5.0, z: 0.0 } };
            0
        }
        _ => 0,
    }
}

fn ops() -> Vec<(EeGeRastOp, isize, isize)> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

#[test]
fn the_constructor_makes_the_debug_text_and_the_destructor_releases_it_in_order() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the mock host answers every operation
    unsafe {
        let p = ee_ge_rast_new(log_host, 0x10 as *mut c_void, 0x20 as *const c_void);
        let made: Vec<EeGeRastOp> = ops().into_iter().map(|o| o.0).collect();
        use EeGeRastOp::*;
        assert_eq!(
            made,
            [
                NameNew,
                LockCreate,
                Verify,
                FontNew,
                TextNew,
                TextSetAlignment,
                TextSetFgColor,
                TextSetBgColor,
                TextSetFont,
                TextSetIsTransparent,
                TextSetLocation
            ]
        );
        assert_eq!(ee_ge_rast_get(p, EeGeRastVal::Culling), 2);
        assert_eq!(ee_ge_rast_get(p, EeGeRastVal::ScreenId), 1);
        assert_eq!(ee_ge_rast_get(p, EeGeRastVal::IsActivated), 0);
        assert_eq!(*ee_ge_rast_color(p, false), WHITE);
        ee_ge_rast_destroy(p);
        let gone: Vec<EeGeRastOp> = ops().into_iter().map(|o| o.0).collect();
        assert_eq!(gone, [FontDereference, TextDelete, LockDestroy, NameDelete]);
    }
}

#[test]
fn a_screen_record_starts_as_the_original_and_deactivate_releases_its_surfaces() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the mock host answers every operation
    unsafe {
        let p = ee_ge_rast_new(log_host, 0x10 as *mut c_void, 0x20 as *const c_void);
        ee_ge_rast_add_screen(p, 0x40 as *mut c_void);
        let s = &mut *ee_ge_rast_screen(p, 0);
        assert!(ee_ge_rast_screen(p, 1).is_null());
        assert_eq!(s.screen as usize, 0x40);
        assert!(s.wait_for_vsync && !s.is_screen_activated && !s.has_texture);
        assert_eq!((s.fog.density, s.fog.start, s.fog.end, s.fog.scale), (1.0, 0.0, 1.0, 1.0));
        assert_eq!(s.viewport, GeViewport::default());
        s.primary_surface = 0x51 as *mut c_void;
        s.texture = 0x52 as *mut c_void;
        ops();
        ee_ge_rast_deactivate(p);
        let released: Vec<(EeGeRastOp, isize, isize)> = ops();
        assert_eq!(released.len(), 2);
        assert_eq!(released[0].0, EeGeRastOp::SurfaceDereference);
        assert_eq!(released[1].0, EeGeRastOp::TextureDereference);
        assert_eq!(ee_ge_rast_get(p, EeGeRastVal::ScreenCount), 0);
        ee_ge_rast_destroy(p);
    }
}

#[test]
fn the_setters_record_each_state_and_the_fog_range_its_scale() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the mock host answers every operation
    unsafe {
        let p = ee_ge_rast_new(log_host, 0x10 as *mut c_void, 0x20 as *const c_void);
        ee_ge_rast_add_screen(p, 0x40 as *mut c_void);
        ee_ge_rast_screen_set(p, 0, EeGeRastState::ZCompare, 7);
        ee_ge_rast_screen_set(p, 0, EeGeRastState::FogDensity, 0.25f32.to_bits() as isize);
        ee_ge_rast_screen_set(p, 0, EeGeRastState::WaitForVSync, 0);
        ee_ge_rast_set_blending(p, 0, 5, 6);
        ee_ge_rast_set_fog_range(p, 0, 2.0, 6.0);
        let s = &*ee_ge_rast_screen(p, 0);
        assert_eq!((s.z_compare, s.blend_source, s.blend_destination), (7, 5, 6));
        assert_eq!((s.fog.density, s.fog.start, s.fog.end, s.fog.scale), (0.25, 2.0, 6.0, 0.25));
        assert!(!s.wait_for_vsync);
        ee_ge_rast_set_ambient_color(p, &EeGeRastColor { red: 1.0, green: 0.5, blue: 0.25 });
        ee_ge_rast_set_ambient_intensity(p, 2.0);
        assert_eq!(*ee_ge_rast_color(p, true), EeGeRastColor { red: 2.0, green: 1.0, blue: 0.5 });
        ee_ge_rast_set_ambient_color(p, &EeGeRastColor { red: 0.5, green: 0.5, blue: 0.5 });
        assert_eq!(*ee_ge_rast_color(p, true), EeGeRastColor { red: 1.0, green: 1.0, blue: 1.0 });
        ee_ge_rast_destroy(p);
    }
}

#[test]
fn a_failed_activation_clears_the_active_rasterizer() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    unsafe extern "C" fn failing(_o: *mut c_void, op: EeGeRastOp, _x: *mut c_void, _a: isize, _b: isize) -> isize {
        if op == EeGeRastOp::ActivateRasterizer {
            9
        } else {
            0
        }
    }
    // SAFETY: the host answers every operation
    unsafe {
        let p = ee_ge_rast_new(failing, 0x10 as *mut c_void, 0x20 as *const c_void);
        ee_ge_rast_set_active(0x99 as *mut c_void);
        assert_eq!(ee_ge_rast_activate(p), 9);
        assert!(ee_ge_rast_active().is_null());
        assert_eq!(ee_ge_rast_get(p, EeGeRastVal::IsActivated), 0);
        let q = ee_ge_rast_new(log_host, 0x11 as *mut c_void, 0x20 as *const c_void);
        assert_eq!(ee_ge_rast_activate(q), 0);
        assert_eq!(ee_ge_rast_active() as usize, 0x11);
        ee_ge_rast_deactivate(q);
        assert!(ee_ge_rast_active().is_null());
        ee_ge_rast_destroy(p);
        ee_ge_rast_destroy(q);
        ops();
    }
}

#[test]
fn the_rasterizer_list_appends_answers_and_clears() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    ee_ge_rast_list(0, std::ptr::null_mut(), 0);
    ee_ge_rast_list(1, 0x10 as *mut c_void, 0);
    ee_ge_rast_list(1, 0x20 as *mut c_void, 0);
    assert_eq!(ee_ge_rast_list(2, std::ptr::null_mut(), 0) as usize, 2);
    assert_eq!(ee_ge_rast_list(3, std::ptr::null_mut(), 1) as usize, 0x20);
    assert!(ee_ge_rast_list(3, std::ptr::null_mut(), 2).is_null());
    ee_ge_rast_list(0, std::ptr::null_mut(), 0);
    assert_eq!(ee_ge_rast_list(2, std::ptr::null_mut(), 0) as usize, 0);
}

#[test]
fn a_sphere_inside_the_view_is_visible_and_one_across_the_front_clips_it() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the mock host answers every operation
    unsafe {
        let p = ee_ge_rast_new(log_host, 0x10 as *mut c_void, 0x20 as *const c_void);
        let view = GeRectangle { left: 0.0, top: 0.0, right: 640.0, bottom: 480.0 };
        let mut v = GeViewport::default();
        ee_ge_viewport_init(&mut v, &view, &view, &GeTransformation::IDENTITY, 1.0, 100.0, 1.5);
        ops();
        let (mut visible, mut clipped) = (false, true);
        ee_ge_rast_model_visible(
            p,
            0x60 as *mut c_void,
            1.0,
            &v,
            &GeTransformation::IDENTITY,
            false,
            &mut visible,
            &mut clipped,
        );
        assert!(visible && !clipped);
        let clips: Vec<(isize, isize)> =
            ops().into_iter().filter(|o| o.0 == EeGeRastOp::ModelSetClip).map(|o| (o.1, o.2)).collect();
        assert_eq!(clips, [(0, 0), (1, 0), (4, 0), (5, 0), (2, 0), (3, 0)]);
        // radius 2 * 3 = 6 around y 5 crosses the front plane at 1
        ee_ge_rast_model_visible(
            p,
            0x60 as *mut c_void,
            3.0,
            &v,
            &GeTransformation::IDENTITY,
            false,
            &mut visible,
            &mut clipped,
        );
        assert!(visible && clipped);
        let front = ops().into_iter().find(|o| o.0 == EeGeRastOp::ModelSetClip).map(|o| (o.1, o.2));
        assert_eq!(front, Some((0, 1)));
        ee_ge_rast_destroy(p);
        ops();
    }
}

#[test]
fn an_orthographic_box_below_the_rectangle_is_hidden_after_its_first_planes() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the mock host answers every operation
    unsafe {
        let p = ee_ge_rast_new(log_host, 0x10 as *mut c_void, 0x20 as *const c_void);
        let view = GeRectangle { left: 0.0, top: 0.0, right: 640.0, bottom: 480.0 };
        let ortho = GeRectangle { left: -10.0, top: 10.0, right: 10.0, bottom: -10.0 };
        let mut v = GeViewport::default();
        ee_ge_viewport_init(&mut v, &view, &ortho, &GeTransformation::IDENTITY, 1.0, 100.0, 1.5);
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 50.0;
        ops();
        let (mut visible, mut clipped) = (true, true);
        ee_ge_rast_model_visible(p, 0x60 as *mut c_void, 1.0, &v, &t, true, &mut visible, &mut clipped);
        assert!(!visible && !clipped);
        let clips: Vec<(isize, isize)> =
            ops().into_iter().filter(|o| o.0 == EeGeRastOp::ModelSetClip).map(|o| (o.1, o.2)).collect();
        // front and rear pass; the box's x (49..51) is right of the rectangle: the left plane is set, then hidden
        assert_eq!(clips, [(0, 0), (1, 0), (4, 0)]);
        ee_ge_rast_destroy(p);
        ops();
    }
}

#[test]
fn the_pixel_size_of_a_sphere_scales_by_the_half_width_and_a_negative_width_is_the_whole_view() {
    let _lock = STATICS.lock().unwrap_or_else(|e| e.into_inner());
    let v = GeViewport { perspective_scale: 2.0, orthographic_scale: 0.5, half_width: 100.0, ..GeViewport::default() };
    let camera = Ge3DPoint { x: 0.0, y: 10.0, z: 0.0 };
    // perspective: (x+r)*2/10 - (x-r)*2/10 = 0.4 r
    assert_eq!(pixel_width(&v, camera, 5.0, false), 0.4 * 5.0 * 100.0);
    // a camera point at y 0 has no width
    assert_eq!(pixel_width(&v, Ge3DPoint { y: 0.0, ..camera }, 5.0, false), 0.0);
    // orthographic: 2r * 0.5
    assert_eq!(pixel_width(&v, camera, 5.0, true), 5.0 * 100.0);
    // a negative radius gives a negative width: the whole view
    assert_eq!(pixel_width(&v, camera, -5.0, true), 200.0);
}
