//! The cargo replay of the screen and surface golden driver (`build/ge14/gsc/driver_sc.cpp`, R4.3 part 14): the same
//! random sequences of screens, surfaces and windows, with the driver's traced fakes ported to Rust (the GX layer as a
//! [`Gx`] that answers and traces as the driver's `gx_*` functions under `GX.cpp` did, the C++ shells' host operations,
//! the reference counter that numbers the surfaces, the platform window), each step followed by every screen's and
//! surface's state through the objects. The digests of sequences 1..300 equal the C++ driver's over the unmodified units
//! (`golden_scrobj.txt`).

#![allow(clippy::manual_is_multiple_of, clippy::unnecessary_unwrap)] // the driver's `R() % n` spelled as the C++ spells it
use core::cell::RefCell;
use core::ffi::{c_long, c_ulong, c_void};
use std::collections::BTreeMap;

use crate::dx7::api::*;
use crate::dx7::screen::{self, VideoMode};
use crate::dx7::scrobj::{self, EeDx7Screen};
use crate::dx7::surface::LogicalMetrics;
use crate::dx7::surfobj::{EeDx7ObjOp, EeDx7Surface};

const GXERR_GENERIC: i32 = 0x8000_4005_u32 as i32;
const GXERR_OUTOFMEMORY: i32 = 0x8007_000e_u32 as i32;
const GXSCAPS_MIPMAP: u32 = 0x0040_0000;
const GXPF_RGB: u32 = 0x40;
const GXPF_ALPHAPIXELS: u32 = 1;

/// A GX object: the `GxObj` of the driver (a device's modes and caps, a surface's pixels).
#[derive(Default)]
struct GxObj {
    width: u32,
    height: u32,
    bits: u32,
    pixels: Vec<u8>,
    scale: f32,
    modes: Vec<SurfaceDesc>,
    display: SurfaceDesc,
    fx: u32,
}

/// A `DX7Surface`: its object and the `GESurface` base (the reference count, the logical members).
struct Surface {
    object: *mut EeDx7Surface,
    serial: i32,
    refs: u32,
    logical: (u32, u32, u32),
}

/// A `GEWindow`.
struct Window {
    serial: i32,
    plat: i32,
}

#[derive(Default)]
struct World {
    seed: u64,
    hash: u64,
    gx: Vec<GxObj>,
    current_dd: usize,
    surfaces: BTreeMap<i32, *mut Surface>,
    held: BTreeMap<i32, i32>,
    next_surface: i32,
    screen_serials: Vec<(usize, i32)>,
    next_screen: i32,
    next_window: i32,
    next_plat: i32,
    /// Every trace line, when `EE_REPLAY_DUMP` names a file (the C++ driver's `-v`).
    lines: Option<Vec<String>>,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

fn w<T>(f: impl FnOnce(&mut World) -> T) -> T {
    W.with(|w| f(&mut w.borrow_mut()))
}

fn r() -> u32 {
    w(|w| {
        w.seed = w.seed.wrapping_mul(6_364_136_223_846_793_005).wrapping_add(1_442_695_040_888_963_407);
        (w.seed >> 33) as u32
    })
}

fn range(low: i32, high: i32) -> f32 {
    (low * 100 + (r() % ((high - low) * 100 + 1) as u32) as i32) as f32 / 100.0
}

fn trace(line: String) {
    w(|w| {
        if let Some(lines) = w.lines.as_mut() {
            lines.push(line.clone());
        }
        for b in line.bytes().chain(core::iter::once(b'\n')) {
            w.hash = (w.hash ^ u64::from(b)).wrapping_mul(0x100_0000_01b3);
        }
    });
}

fn result(one_in: u32) -> i32 {
    if r() % one_in == 0 {
        GXERR_GENERIC
    } else {
        GX_OK
    }
}

fn busy() -> i32 {
    match r() % 16 {
        0 | 1 => GXERR_WASSTILLDRAWING,
        2 => GXERR_SURFACELOST,
        3 => GXERR_SURFACEBUSY,
        4 => GXERR_GENERIC,
        _ => GX_OK,
    }
}

/// A new GX object: its id (1 + its index) as the handle.
fn new_gx() -> H {
    let scale = if r() % 4 == 0 { 2.0 } else { 1.0 };
    w(|w| {
        w.gx.push(GxObj { scale, ..Default::default() });
        w.gx.len() as H
    })
}

fn g(h: H) -> usize {
    h as usize
}

fn set_pixels(h: H, width: u32, height: u32, bits: u32) {
    let n = (width as usize * 4 + 64) * (height as usize + 8);
    let mut pixels = Vec::with_capacity(n);
    for _ in 0..n {
        pixels.push((r() >> 8) as u8);
    }
    w(|w| {
        let o = &mut w.gx[g(h) - 1];
        o.width = width;
        o.height = height;
        o.bits = bits;
        o.pixels = pixels;
    });
}

fn mode(width: u32, height: u32, bits: u32, rate: u32, r555: bool) -> SurfaceDesc {
    let (rm, gm, bm) = match (bits, r555) {
        (32, _) => (0xff_0000, 0xff00, 0xff),
        (_, true) => (0x7c00, 0x3e0, 0x1f),
        _ => (0xf800, 0x7e0, 0x1f),
    };
    SurfaceDesc {
        dwWidth: width,
        dwHeight: height,
        dwRefreshRate: rate,
        format: PixelFormat {
            dwFlags: GXPF_RGB,
            dwRGBBitCount: bits,
            dwRBitMask: rm,
            dwGBitMask: gm,
            dwBBitMask: bm,
            ..Default::default()
        },
        ..Default::default()
    }
}

fn new_direct_draw() -> H {
    const SIZES: [[u32; 2]; 4] = [[640, 480], [1024, 768], [1280, 1024], [1600, 1200]];
    let dd = new_gx();
    let n = r() % 5;
    let at = r() % (n + 1);
    let mut modes = Vec::new();
    for i in 0..=n {
        if i == at {
            modes.push(mode(800, 600, 16, 60, false));
        } else {
            let width = SIZES[(r() % 4) as usize][0];
            let height = SIZES[(r() % 4) as usize][1];
            let bits = if r() % 2 != 0 { 32 } else { 16 };
            let rate = if r() % 2 != 0 { 60 } else { 75 };
            let r555 = r() % 4 == 0;
            modes.push(mode(width, height, bits, rate, r555));
        }
    }
    let display = if r() % 4 == 0 { mode(1920, 1080, 32, 60, false) } else { modes[(r() as usize) % modes.len()] };
    let mut fx = 0;
    for f in [0x20, 0x400, 0x1000, 0x4000, 0x1_0000] {
        if r() % 2 != 0 {
            fx |= f;
        }
    }
    w(|w| {
        let o = &mut w.gx[g(dd) - 1];
        o.modes = modes;
        o.display = display;
        o.fx = fx;
    });
    dd
}

/// The GX layer as the driver's `gx_*` fakes answered it through `GX.cpp` and `RawGx`.
struct ReplayGx;

fn rect_text(rect: Option<IRect>) -> String {
    match rect {
        Some(r) => format!("r {} {} {} {}", r.left, r.top, r.right, r.bottom),
        None => "- 0 0 0 0".into(),
    }
}

impl Gx for ReplayGx {
    fn surface_release(&mut self, s: H) {
        trace(format!("gx surface release {}", g(s)));
    }
    fn surface_add_attached(&mut self, s: H, o: H) -> i32 {
        let res = result(6);
        trace(format!("gx surface attach {} {} -> {:08x}", g(s), g(o), res as u32));
        res
    }
    fn surface_delete_attached(&mut self, s: H, o: H) -> i32 {
        let res = result(6);
        trace(format!("gx surface detach {} {} -> {:08x}", g(s), g(o), res as u32));
        res
    }
    fn surface_get_attached(&mut self, s: H, caps: u32) -> H {
        let o = new_gx();
        let mip = caps & GXSCAPS_MIPMAP != 0;
        let (width, height, bits) = w(|w| (w.gx[g(s) - 1].width, w.gx[g(s) - 1].height, w.gx[g(s) - 1].bits));
        let half = |v: u32| if v > 1 { v / 2 } else { 1 };
        set_pixels(o, if mip { half(width) } else { width }, if mip { half(height) } else { height }, bits);
        trace(format!("gx surface get attached {} {:08x} -> {}", g(s), caps, g(o)));
        o
    }
    fn surface_blt(
        &mut self,
        s: H,
        dst: Option<IRect>,
        src: H,
        src_rect: Option<IRect>,
        flags: u32,
        fill: Option<u32>,
    ) -> i32 {
        let res = busy();
        trace(format!(
            "gx surface blt {} [{}] {} [{}] {:08x} fill {} {:08x} -> {:08x}",
            g(s),
            rect_text(dst),
            g(src),
            rect_text(src_rect),
            flags,
            if fill.is_some() { "y" } else { "n" },
            fill.unwrap_or(0),
            res as u32
        ));
        res
    }
    fn surface_flip(&mut self, s: H, flags: u32) -> i32 {
        let res = busy();
        trace(format!("gx surface flip {} {:08x} -> {:08x}", g(s), flags, res as u32));
        res
    }
    fn surface_lock(&mut self, s: H) -> Locked {
        let mut res = busy();
        if res == GXERR_SURFACEBUSY {
            res = GX_OK;
        }
        let (memory, pitch) = if res == GX_OK {
            w(|w| {
                let o = &mut w.gx[g(s) - 1];
                (o.pixels.as_mut_ptr() as H, (o.width * (o.bits / 8)) as i32)
            })
        } else {
            (NULL, 0)
        };
        trace(format!("gx surface lock {} -> {:08x}", g(s), res as u32));
        Locked { result: res, memory, pitch }
    }
    fn surface_unlock(&mut self, s: H) -> i32 {
        let res = match r() % 8 {
            0 => GXERR_NOTLOCKED,
            1 => GXERR_GENERIC,
            _ => GX_OK,
        };
        trace(format!("gx surface unlock {} -> {:08x}", g(s), res as u32));
        res
    }
    fn surface_set_color_key(&mut self, s: H, color: u32) -> i32 {
        trace(format!("gx surface key {} {color:08x} {color:08x}", g(s)));
        GX_OK
    }
    fn surface_restore(&mut self, _s: H) -> i32 {
        GX_OK
    }
    fn vb_release(&mut self, _b: H) {
        unreachable!()
    }
    fn vb_lock(&mut self, _b: H, _flags: u32) -> (i32, *mut c_void) {
        unreachable!()
    }
    fn vb_unlock(&mut self, _b: H) -> i32 {
        unreachable!()
    }
    fn dd_create(&mut self) -> (i32, H) {
        unreachable!()
    }
    fn dd_release(&mut self, dd: H) {
        trace(format!("gx dd release {}", g(dd)));
    }
    fn dd_query_interface(&mut self, dd: H, iid: u32) -> (i32, H) {
        let res = result(6);
        let out = if res == GX_OK { new_gx() } else { NULL };
        trace(format!("gx dd query {} {iid} -> {:08x} {}", g(dd), res as u32, g(out)));
        (res, out)
    }
    fn dd_get_caps(&mut self, _dd: H) -> (i32, Caps) {
        let fx = w(|w| if w.current_dd != 0 { w.gx[w.current_dd - 1].fx } else { 0 });
        trace(format!("gx dd caps {fx:08x}"));
        (GX_OK, Caps { dwCaps: 0, dwFXCaps: fx })
    }
    fn dd_enum_display_modes(&mut self, dd: H, f: &mut dyn FnMut(&SurfaceDesc)) -> i32 {
        trace(format!("gx dd enum modes {}", g(dd)));
        let modes = w(|w| w.gx[g(dd) - 1].modes.clone());
        for m in &modes {
            f(m);
        }
        GX_OK
    }
    fn dd_get_display_mode(&mut self, dd: H) -> (i32, SurfaceDesc) {
        trace(format!("gx dd display mode {}", g(dd)));
        (GX_OK, w(|w| w.gx[g(dd) - 1].display))
    }
    fn dd_set_display_mode(&mut self, dd: H, width: u32, height: u32, bits: u32, _rate: u32, _flags: u32) -> i32 {
        let res = result(5);
        trace(format!("gx dd mode {} {width} {height} {bits} -> {:08x}", g(dd), res as u32));
        res
    }
    fn dd_set_cooperative_level(&mut self, dd: H, window: H, flags: u32) -> i32 {
        let res = result(8);
        trace(format!("gx dd coop {} window {} {flags:08x} -> {:08x}", g(dd), window as usize as i32, res as u32));
        res
    }
    fn dd_create_surface(&mut self, dd: H, d: &SurfaceDesc) -> (i32, H) {
        let p = d.format;
        trace(format!(
            "gx dd create surface {} flags {:08x} {} x {} bb {} mips {} caps {:08x} {:08x} pf {:08x} {} {:08x} {:08x} {:08x} {:08x}",
            g(dd),
            d.dwFlags,
            d.dwWidth,
            d.dwHeight,
            d.dwBackBufferCount,
            d.dwMipMapCount,
            d.ddsCaps,
            d.ddsCaps2,
            p.dwFlags,
            p.dwRGBBitCount,
            p.dwRBitMask,
            p.dwGBitMask,
            p.dwBBitMask,
            p.dwRGBAlphaBitMask
        ));
        if r() % 10 == 0 {
            trace("  fails".into());
            return (GXERR_OUTOFMEMORY, NULL);
        }
        let o = new_gx();
        let bits = if p.dwRGBBitCount != 0 { p.dwRGBBitCount } else { 16 };
        set_pixels(o, if d.dwWidth != 0 { d.dwWidth } else { 64 }, if d.dwHeight != 0 { d.dwHeight } else { 48 }, bits);
        trace(format!("  -> {}", g(o)));
        (GX_OK, o)
    }
    fn dd_get_available_vid_mem(&mut self, _dd: H, _caps: u32) -> (i32, u32, u32) {
        unreachable!()
    }
    fn dd_get_device_identifier(&mut self, _dd: H) -> (i32, Identifier) {
        unreachable!()
    }
    fn dd_test_cooperative_level(&mut self, _dd: H) -> i32 {
        GX_OK
    }
    fn dd_restore_all_surfaces(&mut self, _dd: H) -> i32 {
        GX_OK
    }
    fn dd_enum_devices(&mut self, _dd: H, _f: &mut dyn FnMut(DevCaps) -> bool) -> i32 {
        unreachable!()
    }
    fn dd_create_device(&mut self, _dd: H, _iid: u32, target: H) -> (i32, H) {
        let res = result(6);
        let out = if res == GX_OK { new_gx() } else { NULL };
        trace(format!("gx dd device {} -> {:08x} {}", g(target), res as u32, g(out)));
        (res, out)
    }
    fn dd_create_vertex_buffer(&mut self, _dd: H, _caps: u32, _fvf: u32, _count: u32) -> (i32, H) {
        unreachable!()
    }
    fn device_release(&mut self, d: H) {
        trace(format!("gx device release {}", g(d)));
    }
    fn device_get_caps(&mut self, _d: H) -> DevCaps {
        let depth = if r() % 2 != 0 { 0x100 } else { 0x400 };
        trace(format!("gx device caps {depth:08x}"));
        DevCaps { dwDevCaps: 0, dwDeviceRenderBitDepth: depth }
    }
    fn device_enum_texture_formats(&mut self, _d: H, f: &mut dyn FnMut(&PixelFormat) -> bool) -> i32 {
        trace("gx device enum formats".into());
        let n = r() % 4;
        for _ in 0..n {
            let p = match r() % 3 {
                0 => PixelFormat {
                    dwFlags: GXPF_RGB | GXPF_ALPHAPIXELS,
                    dwRGBBitCount: 32,
                    dwRBitMask: 0xff_0000,
                    dwGBitMask: 0xff00,
                    dwBBitMask: 0xff,
                    dwRGBAlphaBitMask: 0xff00_0000,
                    ..Default::default()
                },
                1 => PixelFormat {
                    dwFlags: GXPF_RGB,
                    dwRGBBitCount: 16,
                    dwRBitMask: 0xf800,
                    dwGBitMask: 0x7e0,
                    dwBBitMask: 0x1f,
                    ..Default::default()
                },
                _ => PixelFormat {
                    dwFlags: GXPF_RGB | GXPF_ALPHAPIXELS,
                    dwRGBBitCount: 16,
                    dwRBitMask: 0xf00,
                    dwGBitMask: 0xf0,
                    dwBBitMask: 0xf,
                    dwRGBAlphaBitMask: 0xf000,
                    ..Default::default()
                },
            };
            let cancel = f(&p);
            let res = if cancel { 0 } else { 1 };
            trace(format!("  format {} {:08x} -> {res}", p.dwRGBBitCount, p.dwRGBAlphaBitMask));
            if res == 0 {
                break;
            }
        }
        GX_OK
    }
    fn device_begin_scene(&mut self, _d: H) -> i32 {
        unreachable!()
    }
    fn device_end_scene(&mut self, _d: H) -> i32 {
        unreachable!()
    }
    fn device_set_render_target(&mut self, _d: H, _s: H) -> i32 {
        unreachable!()
    }
    fn device_set_viewport(&mut self, _d: H, _v: &Viewport) -> i32 {
        unreachable!()
    }
    fn device_clear(&mut self, _d: H, _rect: IRect, _flags: u32, _color: u32, _z: f32) -> i32 {
        unreachable!()
    }
    fn device_set_transform(&mut self, _d: H, _state: u32, _m: &[f32; 16]) -> i32 {
        unreachable!()
    }
    fn device_set_render_state(&mut self, _d: H, _state: u32, _value: u32) -> i32 {
        unreachable!()
    }
    fn device_set_stage_state(&mut self, _d: H, _stage: u32, _state: u32, _value: u32) -> i32 {
        unreachable!()
    }
    fn device_set_light(&mut self, _d: H, _index: u32, _light: &Light) -> i32 {
        unreachable!()
    }
    fn device_light_enable(&mut self, _d: H, _index: u32, _on: bool) -> i32 {
        unreachable!()
    }
    fn device_validate(&mut self, _d: H) -> (i32, u32) {
        unreachable!()
    }
}

/// `PlatSleep`, `ESRecordNativeError`.
struct ReplayHost;

impl Host for ReplayHost {
    fn sleep(&mut self, milliseconds: u32) {
        trace(format!("sleep {milliseconds}"));
    }
    fn record_native_error(&mut self, error: i32) {
        trace(format!("native error {:08x}", error as u32));
    }
}

fn surface(h: H) -> *mut Surface {
    h as *mut Surface
}

fn surface_serial(h: H) -> i32 {
    if h.is_null() {
        return 0;
    }
    w(|w| w.surfaces.values().find(|s| **s == surface(h)).map_or(-1, |s| unsafe { (**s).serial }))
}

fn screen_serial(h: H) -> i32 {
    if h.is_null() {
        return 0;
    }
    w(|w| w.screen_serials.iter().find(|(p, _)| *p == h as usize).map_or(-1, |(_, s)| *s))
}

fn reference(h: H) {
    let s = surface(h);
    unsafe { (*s).refs += 1 };
    trace(format!("surface {} ref -> {}", unsafe { (*s).serial }, unsafe { (*s).refs }));
}

fn dereference(h: H) {
    let s = surface(h);
    let serial = unsafe { (*s).serial };
    trace(format!("surface {serial} deref -> {}", unsafe { (*s).refs } - 1));
    unsafe { (*s).refs -= 1 };
    if unsafe { (*s).refs } == 0 {
        unsafe { EeDx7Surface::destroy((*s).object, &mut ReplayGx) };
        trace(format!("surface delete {serial}"));
        w(|w| {
            w.surfaces.remove(&serial);
            w.held.remove(&serial);
        });
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `DX7Screen::CreateAWindow` (the platform window under `gPlatSimMode` 0, a 1920 x 1080 display).
fn create_window(x: f32, y: f32, width: u32, height: u32, title: &str, style: u32) -> (i32, *mut Window) {
    let p = screen::window_params(false, style, width as i32, height as i32, 1920, 1080, x, y);
    let plat = if r() % 8 == 0 {
        trace("plat window fails".into());
        0
    } else {
        let next = w(|w| w.next_plat) + 1;
        trace(format!(
            "plat window {next} '{title}' {} {} {} {} {:08x} {}",
            if p.create_at { p.x } else { 0 },
            if p.create_at { p.y } else { 0 },
            p.width,
            p.height,
            p.flags,
            i32::from(p.create_at)
        ));
        w(|w| w.next_plat = next);
        next
    };
    if plat == 0 {
        trace("debugout CreateAWindow: no window\n".into());
        trace("native error 00000005".into());
        trace("get native error".into());
        return (K_NATIVE, core::ptr::null_mut());
    }
    trace("cursor 0".into());
    trace("dispatch events".into());
    (K_OK, new_window(plat, width, height, style, true))
}

fn new_window(plat: i32, width: u32, height: u32, depth: u32, full: bool) -> *mut Window {
    let serial = w(|w| {
        w.next_window += 1;
        w.next_window
    });
    trace(format!("window new {serial} plat {plat} {width} {height} {depth} {}", i32::from(full)));
    Box::into_raw(Box::new(Window { serial, plat }))
}

fn delete_window(p: *mut Window) {
    trace(format!("window delete {}", unsafe { (*p).serial }));
    drop(unsafe { Box::from_raw(p) });
}

unsafe extern "C" fn host(op: EeDx7ObjOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    match op {
        EeDx7ObjOp::NameNew => {
            let name = unsafe { &*(obj as *const String) }.clone();
            trace(format!("UString copy {name}"));
            Box::into_raw(Box::new(name)) as isize
        }
        EeDx7ObjOp::NameDelete => {
            let name = unsafe { Box::from_raw(obj as *mut String) };
            trace(format!("UString dtor {name}"));
            0
        }
        EeDx7ObjOp::SurfaceNew => {
            let desc = unsafe { *(a as *const SurfaceDescription) };
            let serial = w(|w| {
                w.next_surface += 1;
                w.next_surface
            });
            let s =
                Box::into_raw(Box::new(Surface { object: core::ptr::null_mut(), serial, refs: 1, logical: (0, 0, 0) }));
            unsafe { (*s).object = EeDx7Surface::create(host, s as H, obj, desc, b as H) };
            w(|w| w.surfaces.insert(serial, s));
            s as isize
        }
        EeDx7ObjOp::SurfaceObject => unsafe { (*surface(obj)).object as isize },
        EeDx7ObjOp::SurfaceReference => {
            reference(obj);
            0
        }
        EeDx7ObjOp::SurfaceDereference => {
            dereference(obj);
            0
        }
        EeDx7ObjOp::SurfaceAllocatedSize => unsafe { EeDx7Surface::allocated_size((*surface(obj)).object) as isize },
        EeDx7ObjOp::SurfaceMetrics => {
            let s = surface(obj);
            let d = unsafe { (*(*s).object).desc };
            let l = unsafe { (*s).logical };
            let m = LogicalMetrics {
                logical_width: l.0,
                logical_height: l.1,
                pixel_width: d.mWidth as u32,
                pixel_height: d.mHeight as u32,
                logical_shift: l.2,
            };
            unsafe { *(a as *mut LogicalMetrics) = m };
            0
        }
        EeDx7ObjOp::ProbeWindow => {
            let (res, window) = create_window(0.0, 0.0, 16, 16, "", 2);
            unsafe { *(a as *mut H) = window as H };
            trace("UString dtor ".into());
            trace("UString dtor TestWindowClass".into());
            res as isize
        }
        EeDx7ObjOp::WindowOs => unsafe { (*(obj as *mut Window)).plat as isize },
        EeDx7ObjOp::WindowDelete => {
            delete_window(obj as *mut Window);
            0
        }
        EeDx7ObjOp::DispatchEvents => {
            trace("dispatch events".into());
            0
        }
    }
}

fn obj(h: H) -> *mut EeDx7Surface {
    unsafe { (*surface(h)).object }
}

fn width_of(h: H, height: bool) -> c_ulong {
    let l = unsafe { (*surface(h)).logical };
    unsafe {
        EeDx7Surface::logical_size(obj(h), c_ulong::from(if height { l.1 } else { l.0 }), c_ulong::from(l.2), height)
    }
}

fn bits(f: f32) -> u32 {
    f.to_bits()
}

fn dump_screen(p: *mut EeDx7Screen, owner: H) {
    let s = unsafe { &*p };
    let n = s.modes.len();
    trace(format!(
        "screen {} name {} dd {} full {} attached {} shares {} mag {} min {} dev32 {} tex32 {} modes {n} current {}",
        screen_serial(owner),
        unsafe { &*(s.name as *const String) },
        g(s.dd),
        i32::from(s.full_screen),
        i32::from(s.attached_to_desktop),
        i32::from(s.shares_primary),
        i32::from(s.can_mag_filter),
        i32::from(s.can_min_filter),
        i32::from(s.device32),
        i32::from(s.texture32),
        s.current_mode
    ));
    for (i, m) in s.modes.iter().enumerate() {
        trace(format!("  mode {i}: {} {} {} {} {}", m.mWidth, m.mHeight, m.mRefreshRate, m.mBitCount, m.mPixelFormat));
    }
    if (s.current_mode as usize) < n {
        let m: VideoMode = s.modes[s.current_mode as usize];
        let r = unsafe { core::slice::from_raw_parts(EeDx7Screen::desktop_rectangle(p), 4) };
        trace(format!(
            "  current {} {} desktop {:08x} {:08x} {:08x} {:08x}",
            m.mWidth,
            m.mHeight,
            bits(r[1]),
            bits(r[3]),
            bits(r[2]),
            bits(r[0])
        ));
    }
}

fn memory_text(p: *mut c_void) -> String {
    if p.is_null() {
        return "null".into();
    }
    w(|w| {
        for (i, o) in w.gx.iter().enumerate() {
            let start = o.pixels.as_ptr() as usize;
            if !o.pixels.is_empty() && (p as usize) >= start && (p as usize) < start + o.pixels.len() {
                return format!("{}+{}", i + 1, p as usize - start);
            }
        }
        "?".into()
    })
}

fn dump_surface(h: H) {
    let o = unsafe { &*obj(h) };
    let d = o.desc;
    let scale = if o.gx.is_null() { 1.0 } else { w(|w| w.gx[g(o.gx) - 1].scale) };
    let size = unsafe { EeDx7Surface::allocated_size(obj(h)) };
    trace(format!(
        "surface {} refs {} type {} fmt {} vol {} depth {} w {} h {} desc {} {} {} {} 3d {} tr {} na {} bpr {} mem {} text {} \
         parent {} gx {} scale {:08x} color {:08x} {:08x} {:08x} size {size} attached {}",
        surface_serial(h),
        unsafe { (*surface(h)).refs },
        d.mType,
        d.mPixelFormat,
        d.mSurfaceVolatility,
        d.mPixelDepth,
        width_of(h, false),
        width_of(h, true),
        d.mWidth,
        d.mHeight,
        d.m14,
        d.mMipLevelCount,
        i32::from(d.mIs3DRenderable),
        i32::from(d.mIsTransparencyEnabled),
        i32::from(d.mNoAlpha),
        o.bytes_per_row,
        memory_text(o.memory),
        i32::from(o.text_locked),
        screen_serial(o.parent),
        g(o.gx),
        bits(scale),
        bits(o.transparency[0]),
        bits(o.transparency[1]),
        bits(o.transparency[2]),
        o.attached.len()
    ));
    for (i, a) in o.attached.clone().iter().enumerate() {
        trace(format!("  attached {i}: {}", surface_serial(*a)));
    }
}

/// The screens: (object, owner) in creation order.
type Screens = Vec<(*mut EeDx7Screen, H)>;

fn dump(screens: &Screens) {
    let n = scrobj::screen_count();
    trace(format!("screens {n}"));
    for i in 0..n {
        trace(format!("  list {i}: {}", screen_serial(scrobj::screen_at(i))));
    }
    for (p, owner) in screens {
        dump_screen(*p, *owner);
    }
    let surfaces: Vec<*mut Surface> = w(|w| w.surfaces.values().copied().collect());
    for s in surfaces {
        dump_surface(s as H);
    }
}

fn any_surface() -> H {
    let n = w(|w| w.surfaces.len());
    if n == 0 {
        return NULL;
    }
    let i = (r() as usize) % n;
    w(|w| *w.surfaces.values().nth(i).unwrap()) as H
}

fn held_surface() -> H {
    let n = w(|w| w.held.len());
    if n == 0 {
        return NULL;
    }
    let i = (r() as usize) % n;
    w(|w| {
        let serial = *w.held.keys().nth(i).unwrap();
        w.surfaces[&serial]
    }) as H
}

/// A `GERectangle` (bottom, left, right, top) inside the surface's logical size.
fn rect(h: H) -> [f32; 4] {
    let (wd, ht) = (width_of(h, false) as f32, width_of(h, true) as f32);
    let l = (r() as c_ulong % (if wd > 1.0 { wd } else { 1.0 }) as c_ulong) as f32;
    let t = (r() as c_ulong % (if ht > 1.0 { ht } else { 1.0 }) as c_ulong) as f32;
    let mut right = l + 1.0 + (r() as c_ulong % (if wd - l > 1.0 { wd - l } else { 1.0 }) as c_ulong) as f32;
    let mut bottom = t + 1.0 + (r() as c_ulong % (if ht - t > 1.0 { ht - t } else { 1.0 }) as c_ulong) as f32;
    if right > wd {
        right = wd;
    }
    if bottom > ht {
        bottom = ht;
    }
    [bottom, l, right, t]
}

fn colorable(h: H) -> bool {
    unsafe { (*obj(h)).desc.mPixelFormat < 8 }
}

fn sized(h: H) -> bool {
    let d = unsafe { (*obj(h)).desc };
    width_of(h, false) > 0 && width_of(h, true) > 0 && d.mWidth > 0 && d.mHeight > 0
}

fn pixel_hash(h: H) -> u64 {
    let gx = unsafe { (*obj(h)).gx };
    w(|w| {
        let mut x: u64 = 1_469_598_103_934_665_603; // the driver's constant (FNV-1a's basis without its last digit)
        for b in &w.gx[g(gx) - 1].pixels {
            x = (x ^ u64::from(*b)).wrapping_mul(0x100_0000_01b3);
        }
        x
    })
}

fn create_surface(p: *mut EeDx7Screen, owner: H) {
    const TYPES: [c_ulong; 13] = [0, 1, 1, 2, 3, 3, 4, 5, 5, 5, 6, 6, 7];
    const Z: [c_ulong; 7] = [9, 10, 11, 12, 13, 12, 13];
    let kind = TYPES[(r() % 13) as usize];
    let (format, depth) = if kind == 3 {
        let f = Z[(r() % 7) as usize];
        (f, if r() % 2 != 0 { 16 } else { 32 })
    } else {
        let f = if r() % 12 == 0 { 8 } else { c_ulong::from(r() % 8) };
        (f, if f < 2 { 32 } else { 16 })
    };
    let volatility = c_ulong::from(r() % 3);
    let width = if r() % 3 != 0 { 4 << (r() % 4) } else { 3 + r() % 30 };
    let height = if r() % 3 != 0 { 4 << (r() % 4) } else { 3 + r() % 30 };
    let back = c_ulong::from(r() % 3);
    let mips = 1 + (r() % 4) as c_long;
    let d3 = r() % 2 != 0;
    let transparent = r() % 2 != 0;
    let no_alpha = r() % 3 == 0;
    let mut d = SurfaceDescription {
        mType: kind,
        mPixelFormat: format,
        mSurfaceVolatility: volatility,
        mWidth: c_ulong::from(width),
        mHeight: c_ulong::from(height),
        m14: back,
        mMipLevelCount: mips,
        mPixelDepth: depth,
        mIs3DRenderable: d3,
        mIsTransparencyEnabled: transparent,
        mNoAlpha: no_alpha,
    };
    let mut out = NULL;
    let res = unsafe { EeDx7Screen::create_surface(p, &mut ReplayGx, &mut ReplayHost, &mut d, &mut out) };
    trace(format!(
        "create surface on {} -> {res} {} desc {} {} {} {} {} {} {} {}",
        screen_serial(owner),
        surface_serial(out),
        d.mType,
        d.mPixelFormat,
        d.mSurfaceVolatility,
        d.mWidth,
        d.mHeight,
        d.m14,
        d.mMipLevelCount,
        d.mPixelDepth
    ));
    if !out.is_null() {
        let serial = surface_serial(out);
        w(|w| w.held.insert(serial, 1));
    }
}

fn surface_op(s: H) {
    let (cr, cg, cb) = (range(0, 1), range(0, 1), range(0, 1));
    let c = [cr, cg, cb];
    let (gx, host) = (&mut ReplayGx, &mut ReplayHost);
    match r() % 15 {
        0 => {
            let o = any_surface();
            if o != s && unsafe { (*obj(o)).attached.is_empty() } {
                let res = unsafe { EeDx7Surface::attach(obj(s), gx, host, o) };
                trace(format!("attach {} {} -> {res}", surface_serial(s), surface_serial(o)));
            }
        }
        1 => {
            let n = unsafe { (*obj(s)).attached.len() };
            if n > 0 {
                let i = (r() as usize) % n;
                let res = unsafe { EeDx7Surface::detach(obj(s), gx, host, i) };
                trace(format!("detach {} {i} -> {res}", surface_serial(s)));
            }
        }
        2 => {
            if colorable(s) {
                let alpha = range(0, 1);
                let res = unsafe { EeDx7Surface::clear(obj(s), gx, host, core::ptr::null(), c, alpha) };
                trace(format!("clear {} -> {res}", surface_serial(s)));
            }
        }
        3 => {
            if colorable(s) {
                let a = rect(s);
                let alpha = range(0, 1);
                let res = unsafe { EeDx7Surface::clear(obj(s), gx, host, a.as_ptr(), c, alpha) };
                trace(format!("clear rect {} -> {res}", surface_serial(s)));
            }
        }
        4 => {
            let t = any_surface();
            let a = rect(s);
            let b = rect(t);
            let transparent = r() % 2 != 0;
            let res = unsafe { EeDx7Surface::copy_to(obj(s), gx, host, t, a.as_ptr(), b.as_ptr(), transparent) };
            trace(format!("copy {} {} -> {res}", surface_serial(s), surface_serial(t)));
        }
        5 => {
            let t = any_surface();
            let (ds, dt) = unsafe { ((*obj(s)).desc.mPixelDepth, (*obj(t)).desc.mPixelDepth) };
            if colorable(s) && colorable(t) && sized(s) && sized(t) && (ds == 16 || ds == 32) && ds == dt {
                let a = rect(s);
                let b = rect(t);
                let res = unsafe { EeDx7Surface::copy_with_filter(obj(s), gx, host, t, a.as_ptr(), b.as_ptr()) };
                trace(format!(
                    "filter {} {} -> {res} pixels {:016x}",
                    surface_serial(s),
                    surface_serial(t),
                    pixel_hash(t)
                ));
            }
        }
        6 => {
            let wait = r() % 2 != 0;
            let res = unsafe { EeDx7Surface::flip(obj(s), gx, host, wait) };
            trace(format!("flip {} -> {res}", surface_serial(s)));
        }
        7 => {
            let res = unsafe { EeDx7Surface::lock(obj(s), gx, host) };
            trace(format!("lock {} -> {res}", surface_serial(s)));
        }
        8 => {
            let res = unsafe { EeDx7Surface::unlock(obj(s), gx, host) };
            trace(format!("unlock {} -> {res}", surface_serial(s)));
        }
        9 => {
            let res = unsafe { EeDx7Surface::restore(obj(s), gx, host) };
            trace(format!("restore {} -> {res}", surface_serial(s)));
        }
        10 => {
            if colorable(s) {
                unsafe { EeDx7Surface::set_transparency_color(obj(s), gx, c) };
                trace(format!("transparency {}", surface_serial(s)));
            }
        }
        11 => {
            let res = unsafe { EeDx7Surface::text_lock(obj(s)) };
            trace(format!("text lock {} -> {res}", surface_serial(s)));
        }
        12 => {
            let res = unsafe { EeDx7Surface::text_unlock(obj(s)) };
            trace(format!("text unlock {} -> {res}", surface_serial(s)));
        }
        13 => {
            let l = unsafe { &mut (*surface(s)).logical };
            if r() % 2 != 0 {
                l.2 = r() % 3;
            } else if r() % 2 != 0 {
                let lw = if r() % 3 == 0 { 0 } else { 1 + r() % 40 };
                let lh = if r() % 3 == 0 { 0 } else { 1 + r() % 40 };
                (l.0, l.1) = (lw, lh);
            } else {
                (l.0, l.1) = (0, 0);
            }
            trace(format!("logical {}", surface_serial(s)));
        }
        _ => {
            let size = unsafe { EeDx7Surface::allocated_size(obj(s)) };
            trace(format!("size {} {size}", surface_serial(s)));
        }
    }
}

fn sequence(seed: u64) -> u64 {
    w(|w| {
        w.seed = seed.wrapping_mul(0x9e37_79b9_7f4a_7c15).wrapping_add(1);
        w.hash = 1_469_598_103_934_665_603; // the driver's constant (FNV-1a's basis without its last digit)
    });
    let mut screens: Screens = Vec::new();
    let steps = 10 + (r() % 30) as i32;
    let windows: Vec<*mut Window> = (0..2).map(|i| new_window(100 + i, 640, 480, 1, false)).collect();
    for _ in 0..steps {
        let pick = if screens.is_empty() { None } else { Some(screens[(r() as usize) % screens.len()]) };
        let k = r() % 12;
        if pick.is_none() || (k == 0 && screens.len() < 3) {
            let dd = new_direct_draw();
            w(|w| w.current_dd = g(dd));
            let serial = w(|w| w.next_screen) + 1;
            let name = format!("screen{serial}");
            let attached = r() % 3 != 0;
            let shares = r() % 2 != 0;
            trace(format!("new screen dd {} {name} {} {}", g(dd), i32::from(attached), i32::from(shares)));
            let owner = Box::into_raw(Box::new(0u8)) as H;
            let p =
                unsafe { EeDx7Screen::create(host, owner, NULL, dd, &name as *const String as H, attached, shares) };
            unsafe { EeDx7Screen::start(p, &mut ReplayGx, &mut ReplayHost) };
            trace(format!("UString dtor {name}"));
            screens.push((p, owner));
            w(|w| {
                w.next_screen = serial;
                w.screen_serials.push((owner as usize, serial));
            });
        } else {
            let (p, owner) = pick.unwrap();
            match k {
                1 => {
                    let window = windows[(r() % 2) as usize];
                    let exclusive = r() % 2 != 0;
                    let res =
                        unsafe { EeDx7Screen::capture(p, &mut ReplayGx, &mut ReplayHost, window as H, exclusive) };
                    trace(format!("capture {} -> {res}", screen_serial(owner)));
                }
                2 => {
                    let window = windows[(r() % 2) as usize];
                    let res = unsafe { EeDx7Screen::release(p, &mut ReplayGx, &mut ReplayHost, window as H) };
                    trace(format!("release {} -> {res}", screen_serial(owner)));
                }
                0 | 3..=5 => create_surface(p, owner),
                6..=8 => {
                    let s = any_surface();
                    if !s.is_null() {
                        surface_op(s);
                    }
                }
                9 => {
                    let n = unsafe { (*p).modes.len() };
                    match r() % 3 {
                        0 => {
                            if n > 0 {
                                let i = c_ulong::from(r()) % n as c_ulong;
                                let res = unsafe { EeDx7Screen::set_mode(p, &mut ReplayGx, &mut ReplayHost, i) };
                                trace(format!("set mode {} {i} -> {res}", screen_serial(owner)));
                            }
                        }
                        1 => {
                            let res = screen::restore_all(&mut ReplayGx, &mut ReplayHost, unsafe { (*p).dd });
                            trace(format!("restore all {} -> {res}", screen_serial(owner)));
                        }
                        _ => {
                            let ox = range(0, 100);
                            let oy = range(0, 100);
                            let width = 100 + r() % 900;
                            let height = 100 + r() % 900;
                            let style = r() % 3;
                            let (res, window) = create_window(ox, oy, width, height, "Title", style);
                            let serial = if window.is_null() { 0 } else { unsafe { (*window).serial } };
                            trace(format!("window {} -> {res} {serial}", screen_serial(owner)));
                            if !window.is_null() {
                                delete_window(window);
                            }
                            trace("UString dtor Title".into());
                            trace("UString dtor Class".into());
                        }
                    }
                }
                10 => {
                    let s = held_surface();
                    if !s.is_null() {
                        let serial = surface_serial(s);
                        w(|w| {
                            let n = w.held.get_mut(&serial).unwrap();
                            *n -= 1;
                            if *n == 0 {
                                w.held.remove(&serial);
                            }
                        });
                        dereference(s);
                    }
                }
                _ => {
                    let s = held_surface();
                    if !s.is_null() {
                        let serial = surface_serial(s);
                        w(|w| *w.held.get_mut(&serial).unwrap() += 1);
                        reference(s);
                    }
                }
            }
        }
        dump(&screens);
    }
    // the end: every reference the driver holds, then the screens and windows
    loop {
        let next = w(|w| w.held.iter().next().map(|(s, n)| (*s, *n)));
        let Some((serial, n)) = next else { break };
        let s = w(|w| w.surfaces[&serial]) as H;
        w(|w| {
            if n - 1 == 0 {
                w.held.remove(&serial);
            } else {
                w.held.insert(serial, n - 1);
            }
        });
        dereference(s);
    }
    trace(format!("left surfaces {}", w(|w| w.surfaces.len())));
    for (p, owner) in screens.drain(..) {
        unsafe { EeDx7Screen::destroy(p, &mut ReplayGx) };
        drop(unsafe { Box::from_raw(owner as *mut u8) });
    }
    w(|w| w.screen_serials.clear());
    dump(&screens);
    for window in windows {
        delete_window(window);
    }
    w(|w| {
        w.surfaces.clear();
        w.held.clear();
        w.gx.clear();
        w.current_dd = 0;
        w.hash
    })
}

/// Sequences 1..=n in order (the driver's run: its serials and the capture statics carry over), their digests.
pub(crate) fn digests(n: u64) -> Vec<String> {
    let dump = std::env::var("EE_REPLAY_DUMP").ok();
    if dump.is_some() {
        w(|w| w.lines = Some(Vec::new()));
    }
    let out = (1..=n).map(|s| format!("{s} {:016x}", sequence(s))).collect();
    if let Some(path) = dump {
        let lines = w(|w| w.lines.take()).unwrap_or_default();
        std::fs::write(
            path,
            lines.join(
                "
",
            ) + "
",
        )
        .unwrap();
    }
    out
}
