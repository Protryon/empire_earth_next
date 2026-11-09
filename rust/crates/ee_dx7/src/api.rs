//! The GX contract as the plug-in's algorithms use it (`include/gx/GX.h`'s classes) and the C++ services around it, as
//! one trait so that every algorithm is tested with a recorder. In the game: [`crate::gx::RawGx`], the `extern "C"`
//! functions of `ee_gx` (no wgpu in the test binary). Handles are the opaque pointers of the C++ `GX.h` shims
//! (`GXSurface*`, `GXDevice*`, ...).
#![allow(non_snake_case)]

use core::ffi::{c_long, c_ulong, c_void};

pub type H = *mut c_void;
pub(crate) const NULL: H = core::ptr::null_mut();

pub(crate) const GX_OK: i32 = 0;
pub(crate) const GXERR_SURFACEBUSY: i32 = (0x8876_0000u32 | 430) as i32;
pub(crate) const GXERR_SURFACELOST: i32 = (0x8876_0000u32 | 450) as i32;
pub(crate) const GXERR_WASSTILLDRAWING: i32 = (0x8876_0000u32 | 540) as i32;
pub(crate) const GXERR_NOTLOCKED: i32 = (0x8876_0000u32 | 584) as i32;
pub(crate) const GXENUMRET_CANCEL: i32 = 0;
pub(crate) const GXENUMRET_OK: i32 = 1;

// ESError (lle/LowLevelEngineCore.h)
pub(crate) const K_OK: i32 = 0;
pub(crate) const K_NATIVE: i32 = 1;
pub(crate) const K_ALREADY: i32 = 2;
pub(crate) const K_FAILED: i32 = 9;
pub(crate) const K_NOT_SUPPORTED: i32 = 0xd;

// kSurface*
pub(crate) const SURFACE_SYSTEM_MEMORY: c_ulong = 4;

// GXIID
pub(crate) const IID_DIRECT3D7: u32 = 1;
pub(crate) const IID_HAL_DEVICE: u32 = 2;
pub(crate) const IID_TNL_HAL_DEVICE: u32 = 3;

// flags
pub(crate) const GXBLT_COLORFILL: u32 = 0x0000_0400;
pub(crate) const GXBLT_DONOTWAIT: u32 = 0x0800_0000;
pub(crate) const GXBLT_KEYSRC: u32 = 0x0000_8000;
pub(crate) const GXFLIP_NOVSYNC: u32 = 0x0000_0008;
pub(crate) const GXFLIP_DONOTWAIT: u32 = 0x0000_0020;
pub(crate) const GXLOCK_NOOVERWRITE: u32 = 0x0000_1000;
pub(crate) const GXLOCK_WRITEONLY: u32 = 0x0000_0020;
pub(crate) const GXLOCK_DISCARDCONTENTS: u32 = 0x0000_2000;
pub(crate) const GXCLEAR_TARGET: u32 = 1;
pub(crate) const GXCLEAR_ZBUFFER: u32 = 2;
pub(crate) const GXVBCAPS_SYSTEMMEMORY: u32 = 0x0000_0800;
pub(crate) const GXVBCAPS_WRITEONLY: u32 = 0x0001_0000;
pub(crate) const GXFVF_XYZ: u32 = 0x002;
pub(crate) const GXFVF_NORMAL: u32 = 0x010;
pub(crate) const GXFVF_DIFFUSE: u32 = 0x040;
pub(crate) const GXFVF_TEX1: u32 = 0x100;

/// `RECT`/`GXRECT` as the plug-in computes it (the original's 32-bit `long`s).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct IRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// What `GXSurface::Lock` returns for the whole surface.
#[derive(Clone, Copy, Debug)]
pub struct Locked {
    pub result: i32,
    pub memory: *mut c_void,
    pub pitch: i32,
}

/// `GXPIXELFORMAT` without `dwSize`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PixelFormat {
    pub dwFlags: u32,
    pub dwFourCC: u32,
    pub dwRGBBitCount: u32,
    pub dwRBitMask: u32,
    pub dwGBitMask: u32,
    pub dwBBitMask: u32,
    pub dwRGBAlphaBitMask: u32,
}

/// `GXSURFACEDESC`'s fields the plug-in reads or writes (the GX shim fills in `dwSize`s).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SurfaceDesc {
    pub dwFlags: u32,
    pub dwHeight: u32,
    pub dwWidth: u32,
    pub dwBackBufferCount: u32,
    pub dwMipMapCount: u32,
    pub dwRefreshRate: u32,
    pub format: PixelFormat,
    pub ddsCaps: u32,
    pub ddsCaps2: u32,
}

/// `GXCAPS`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Caps {
    pub dwCaps: u32,
    pub dwFXCaps: u32,
}

/// `GXDEVICEDESC`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DevCaps {
    pub dwDevCaps: u32,
    pub dwDeviceRenderBitDepth: u32,
}

/// `GXVIEWPORT`.
pub use crate::state::Viewport;

/// `GXDEVICEIDENTIFIER`: the description is the C string `szDescription`, upper-cased by the caller when needed.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Identifier {
    pub description: Vec<u8>,
    pub vendor: u32,
    pub device: u32,
}

/// `GXLIGHT`'s fields the plug-in fills in.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Light {
    pub kind: u32,
    pub diffuse: [f32; 3],
    pub position: [f32; 3],
    pub direction: [f32; 3],
    pub range: f32,
    pub attenuation: [f32; 3],
}

/// `GESurfaceDescription` (`lle/GESurface.h`; pinned by `DX7Trampolines.cpp`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SurfaceDescription {
    pub mType: c_ulong,
    pub mPixelFormat: c_ulong,
    pub mSurfaceVolatility: c_ulong,
    pub mWidth: c_ulong,
    pub mHeight: c_ulong,
    pub m14: c_ulong,
    pub mMipLevelCount: c_long,
    pub mPixelDepth: c_ulong,
    pub mIs3DRenderable: bool,
    pub mIsTransparencyEnabled: bool,
    pub mNoAlpha: bool,
}

/// The GX calls of the plug-in. `s`/`o` are `GXSurface*`, `b` a `GXVertexBuffer*`, `dd` a `GXDirectDraw*`, `d` a
/// `GXDevice*`.
#[allow(clippy::too_many_arguments)]
pub trait Gx {
    // ---- surface
    fn surface_release(&mut self, s: H);
    fn surface_add_attached(&mut self, s: H, o: H) -> i32;
    fn surface_delete_attached(&mut self, s: H, o: H) -> i32;
    fn surface_get_attached(&mut self, s: H, caps: u32) -> H;
    fn surface_blt(
        &mut self,
        s: H,
        dst: Option<IRect>,
        src: H,
        src_rect: Option<IRect>,
        flags: u32,
        fill: Option<u32>,
    ) -> i32;
    fn surface_flip(&mut self, s: H, flags: u32) -> i32;
    fn surface_lock(&mut self, s: H) -> Locked;
    fn surface_unlock(&mut self, s: H) -> i32;
    fn surface_set_color_key(&mut self, s: H, color: u32) -> i32;
    fn surface_restore(&mut self, s: H) -> i32;
    // ---- vertex buffer
    fn vb_release(&mut self, b: H);
    fn vb_lock(&mut self, b: H, flags: u32) -> (i32, *mut c_void);
    fn vb_unlock(&mut self, b: H) -> i32;
    // ---- DirectDraw / Direct3D
    fn dd_create(&mut self) -> (i32, H);
    fn dd_release(&mut self, dd: H);
    fn dd_query_interface(&mut self, dd: H, iid: u32) -> (i32, H);
    fn dd_get_caps(&mut self, dd: H) -> (i32, Caps);
    fn dd_enum_display_modes(&mut self, dd: H, f: &mut dyn FnMut(&SurfaceDesc)) -> i32;
    fn dd_get_display_mode(&mut self, dd: H) -> (i32, SurfaceDesc);
    fn dd_set_display_mode(&mut self, dd: H, w: u32, h: u32, bits: u32, rate: u32, flags: u32) -> i32;
    fn dd_set_cooperative_level(&mut self, dd: H, window: H, flags: u32) -> i32;
    fn dd_create_surface(&mut self, dd: H, desc: &SurfaceDesc) -> (i32, H);
    fn dd_get_available_vid_mem(&mut self, dd: H, caps: u32) -> (i32, u32, u32);
    fn dd_get_device_identifier(&mut self, dd: H) -> (i32, Identifier);
    fn dd_test_cooperative_level(&mut self, dd: H) -> i32;
    fn dd_restore_all_surfaces(&mut self, dd: H) -> i32;
    fn dd_enum_devices(&mut self, dd: H, f: &mut dyn FnMut(DevCaps) -> bool) -> i32;
    fn dd_create_device(&mut self, dd: H, iid: u32, target: H) -> (i32, H);
    fn dd_create_vertex_buffer(&mut self, dd: H, caps: u32, fvf: u32, count: u32) -> (i32, H);
    // ---- device
    fn device_release(&mut self, d: H);
    fn device_get_caps(&mut self, d: H) -> DevCaps;
    fn device_enum_texture_formats(&mut self, d: H, f: &mut dyn FnMut(&PixelFormat) -> bool) -> i32;
    fn device_begin_scene(&mut self, d: H) -> i32;
    fn device_end_scene(&mut self, d: H) -> i32;
    fn device_set_render_target(&mut self, d: H, s: H) -> i32;
    fn device_set_viewport(&mut self, d: H, v: &Viewport) -> i32;
    fn device_clear(&mut self, d: H, rect: IRect, flags: u32, color: u32, z: f32) -> i32;
    fn device_set_transform(&mut self, d: H, state: u32, m: &[f32; 16]) -> i32;
    fn device_set_render_state(&mut self, d: H, state: u32, value: u32) -> i32;
    fn device_set_stage_state(&mut self, d: H, stage: u32, state: u32, value: u32) -> i32;
    fn device_set_light(&mut self, d: H, index: u32, light: &Light) -> i32;
    fn device_light_enable(&mut self, d: H, index: u32, on: bool) -> i32;
    fn device_validate(&mut self, d: H) -> (i32, u32);
}

/// The C++ services the plug-in's algorithms call, apart from GX: `PlatSleep`, `ESRecordNativeError`.
pub trait Host {
    fn sleep(&mut self, milliseconds: u32);
    fn record_native_error(&mut self, error: i32);
}

/// The retry-while-still-drawing loop of the plug-in's calls (`PlatSleep(ms)` between tries).
pub fn retry_still_drawing(host: &mut impl Host, ms: u32, mut call: impl FnMut() -> i32) -> i32 {
    loop {
        let error = call();
        if error != GXERR_WASSTILLDRAWING {
            return error;
        }
        host.sleep(ms);
    }
}

/// `(long)float`: C++'s truncation as `cvttss2si` gives it (x86-64 MSVC ABI, `long` is 32 bits): a NaN or
/// out-of-range value is `i32::MIN` (Rust's `as i32` would saturate). Golden in `surface.rs`.
#[inline]
pub fn trunc_to_long(value: f32) -> i32 {
    let t = value.trunc();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&t) {
        t as i32
    } else {
        i32::MIN
    }
}

#[cfg(test)]
pub(crate) mod test_recorder {
    use super::*;
    use core::cell::RefCell;
    use std::collections::{HashMap, VecDeque};
    use std::rc::Rc;

    type Log = Rc<RefCell<Vec<String>>>;

    /// Records every GX call as a line (the format of the C++ scratch generators' fake GX). Results are queued by method
    /// name; an empty queue answers `GX_OK` (and handles answer 7 where a handle is asked for).
    #[derive(Default)]
    pub struct Recorder {
        pub log: Log,
        pub results: HashMap<&'static str, VecDeque<i32>>,
        pub modes: Vec<SurfaceDesc>,
        pub device_caps: Vec<DevCaps>,
        pub texture_formats: Vec<PixelFormat>,
        pub identifier: Identifier,
        pub display_mode: SurfaceDesc,
        pub total_mem: u32,
        pub free_mem: u32,
        pub dd_caps_3d: bool,
    }

    pub struct RecHost {
        pub log: Log,
    }

    impl Host for RecHost {
        fn sleep(&mut self, ms: u32) {
            self.log.borrow_mut().push(format!("sleep {ms}"));
        }
        fn record_native_error(&mut self, e: i32) {
            self.log.borrow_mut().push(format!("native_error {e:x}"));
        }
    }

    impl Recorder {
        pub fn pair() -> (Recorder, RecHost) {
            let gx = Recorder { total_mem: 0x100_0000, free_mem: 0x80_0000, dd_caps_3d: true, ..Default::default() };
            let log = gx.log.clone();
            (gx, RecHost { log })
        }
        fn line(&self, text: String) {
            self.log.borrow_mut().push(text);
        }
        fn result(&mut self, method: &'static str) -> i32 {
            self.results.get_mut(method).and_then(VecDeque::pop_front).unwrap_or(GX_OK)
        }
        /// Queue the results of `method`.
        pub fn queue(&mut self, method: &'static str, values: &[i32]) {
            self.results.entry(method).or_default().extend(values.iter().copied());
        }
    }

    /// A golden file section: `# name`, the trace lines, `= result`, and the extra lines up to the next `#`.
    pub struct Section {
        pub name: String,
        pub trace: Vec<String>,
        pub result: i32,
        pub extra: Vec<String>,
    }

    pub fn golden_sections(text: &str) -> Vec<Section> {
        let mut sections: Vec<Section> = Vec::new();
        let mut trace: Vec<String> = Vec::new();
        let mut name = String::new();
        let mut result: Option<i32> = None;
        let mut extra: Vec<String> = Vec::new();
        let mut flush =
            |name: &mut String, trace: &mut Vec<String>, extra: &mut Vec<String>, result: &mut Option<i32>| {
                if let Some(result) = result.take() {
                    sections.push(Section {
                        name: core::mem::take(name),
                        trace: core::mem::take(trace),
                        result,
                        extra: core::mem::take(extra),
                    });
                }
            };
        for line in text.lines() {
            if let Some(rest) = line.strip_prefix("# ") {
                flush(&mut name, &mut trace, &mut extra, &mut result);
                name = rest.to_string();
                trace.clear();
            } else if let Some(value) = line.strip_prefix("= ") {
                result = Some(value.parse().unwrap());
            } else if result.is_some() {
                extra.push(line.to_string());
            } else {
                trace.push(line.to_string());
            }
        }
        flush(&mut name, &mut trace, &mut extra, &mut result);
        sections
    }

    fn hx(handle: H) -> usize {
        handle as usize
    }

    fn rc(rect: Option<IRect>) -> String {
        match rect {
            Some(r) => format!("{},{},{},{}", r.left, r.top, r.right, r.bottom),
            None => "-".into(),
        }
    }

    impl Gx for Recorder {
        fn surface_release(&mut self, s: H) {
            self.line(format!("surface_release {}", hx(s)));
        }
        fn surface_add_attached(&mut self, s: H, o: H) -> i32 {
            self.line(format!("surface_add_attached {} {}", hx(s), hx(o)));
            self.result("add_attached")
        }
        fn surface_delete_attached(&mut self, s: H, o: H) -> i32 {
            self.line(format!("surface_delete_attached {} {}", hx(s), hx(o)));
            self.result("delete_attached")
        }
        fn surface_get_attached(&mut self, s: H, caps: u32) -> H {
            self.line(format!("surface_get_attached {} {caps:x}", hx(s)));
            let r = self.result("get_attached");
            if r == 0 {
                7 as H
            } else {
                r as usize as H
            }
        }
        fn surface_blt(
            &mut self,
            s: H,
            d: Option<IRect>,
            src: H,
            sr: Option<IRect>,
            flags: u32,
            fill: Option<u32>,
        ) -> i32 {
            let f = fill.map_or("-".to_string(), |f| format!("{f:x}"));
            self.line(format!(
                "surface_blt {} dst={} src={} srcr={} flags={flags:x} fill={f}",
                hx(s),
                rc(d),
                hx(src),
                rc(sr)
            ));
            self.result("blt")
        }
        fn surface_flip(&mut self, s: H, flags: u32) -> i32 {
            self.line(format!("surface_flip {} {flags:x}", hx(s)));
            self.result("flip")
        }
        fn surface_lock(&mut self, s: H) -> Locked {
            self.line(format!("surface_lock {}", hx(s)));
            let result = self.result("lock");
            let ok = result == GX_OK;
            Locked { result, memory: if ok { 0x1000 as H } else { NULL }, pitch: if ok { 1280 } else { 0 } }
        }
        fn surface_unlock(&mut self, s: H) -> i32 {
            self.line(format!("surface_unlock {}", hx(s)));
            self.result("unlock")
        }
        fn surface_set_color_key(&mut self, s: H, c: u32) -> i32 {
            self.line(format!("surface_set_color_key {} {c:x}", hx(s)));
            self.result("set_color_key")
        }
        fn surface_restore(&mut self, s: H) -> i32 {
            self.line(format!("surface_restore {}", hx(s)));
            self.result("restore")
        }
        fn vb_release(&mut self, b: H) {
            self.line(format!("vb_release {}", hx(b)));
        }
        fn vb_lock(&mut self, b: H, flags: u32) -> (i32, *mut c_void) {
            self.line(format!("vb_lock {} {flags:x}", hx(b)));
            let r = self.result("vb_lock");
            (r, if r == GX_OK { 0x2000 as H } else { NULL })
        }
        fn vb_unlock(&mut self, b: H) -> i32 {
            self.line(format!("vb_unlock {}", hx(b)));
            self.result("vb_unlock")
        }
        fn dd_create(&mut self) -> (i32, H) {
            self.line("dd_create".into());
            let r = self.result("dd_create");
            (r, if r == GX_OK { 100 as H } else { NULL })
        }
        fn dd_release(&mut self, dd: H) {
            self.line(format!("dd_release {}", hx(dd)));
        }
        fn dd_query_interface(&mut self, dd: H, iid: u32) -> (i32, H) {
            self.line(format!("dd_query_interface {} {iid}", hx(dd)));
            let r = self.result("query");
            (r, if r == GX_OK { dd } else { NULL })
        }
        fn dd_get_caps(&mut self, dd: H) -> (i32, Caps) {
            self.line(format!("dd_get_caps {}", hx(dd)));
            let r = self.result("get_caps");
            (r, Caps { dwCaps: self.dd_caps_3d as u32, dwFXCaps: self.result("fx_caps") as u32 })
        }
        fn dd_enum_display_modes(&mut self, dd: H, f: &mut dyn FnMut(&SurfaceDesc)) -> i32 {
            self.line(format!("dd_enum_display_modes {}", hx(dd)));
            let modes = self.modes.clone();
            for m in &modes {
                f(m);
            }
            self.result("enum_modes")
        }
        fn dd_get_display_mode(&mut self, dd: H) -> (i32, SurfaceDesc) {
            self.line(format!("dd_get_display_mode {}", hx(dd)));
            (GX_OK, self.display_mode)
        }
        fn dd_set_display_mode(&mut self, dd: H, w: u32, h: u32, bits: u32, rate: u32, flags: u32) -> i32 {
            self.line(format!("dd_set_display_mode {} {w} {h} {bits} {rate} {flags}", hx(dd)));
            self.result("set_mode")
        }
        fn dd_set_cooperative_level(&mut self, dd: H, window: H, flags: u32) -> i32 {
            self.line(format!("dd_set_cooperative_level {} {} {flags:x}", hx(dd), hx(window)));
            self.result("coop")
        }
        fn dd_create_surface(&mut self, dd: H, desc: &SurfaceDesc) -> (i32, H) {
            let f = desc.format;
            self.line(format!(
                "dd_create_surface {} flags={:x} w={} h={} bbc={} mips={} caps={:x} caps2={:x} pf={:x},{:x},{},{:x},{:x},{:x},{:x}",
                hx(dd), desc.dwFlags, desc.dwWidth, desc.dwHeight, desc.dwBackBufferCount, desc.dwMipMapCount, desc.ddsCaps,
                desc.ddsCaps2, f.dwFlags, f.dwFourCC, f.dwRGBBitCount, f.dwRBitMask, f.dwGBitMask, f.dwBBitMask,
                f.dwRGBAlphaBitMask
            ));
            let r = self.result("create_surface");
            (r, if r == GX_OK { 8 as H } else { NULL })
        }
        fn dd_get_available_vid_mem(&mut self, dd: H, caps: u32) -> (i32, u32, u32) {
            self.line(format!("dd_get_available_vid_mem {} {caps:x}", hx(dd)));
            let r = self.result("vidmem");
            (r, self.total_mem, self.free_mem)
        }
        fn dd_get_device_identifier(&mut self, dd: H) -> (i32, Identifier) {
            self.line(format!("dd_get_device_identifier {}", hx(dd)));
            (GX_OK, self.identifier.clone())
        }
        fn dd_test_cooperative_level(&mut self, dd: H) -> i32 {
            self.line(format!("dd_test_cooperative_level {}", hx(dd)));
            self.result("test_coop")
        }
        fn dd_restore_all_surfaces(&mut self, dd: H) -> i32 {
            self.line(format!("dd_restore_all_surfaces {}", hx(dd)));
            self.result("restore_all")
        }
        fn dd_enum_devices(&mut self, dd: H, f: &mut dyn FnMut(DevCaps) -> bool) -> i32 {
            self.line(format!("dd_enum_devices {}", hx(dd)));
            let caps = self.device_caps.clone();
            for c in caps {
                if f(c) {
                    break;
                }
            }
            self.result("enum_devices")
        }
        fn dd_create_device(&mut self, dd: H, iid: u32, target: H) -> (i32, H) {
            self.line(format!("dd_create_device {} {iid} {}", hx(dd), hx(target)));
            let r = self.result("create_device");
            (r, if r == GX_OK { 9 as H } else { NULL })
        }
        fn dd_create_vertex_buffer(&mut self, dd: H, caps: u32, fvf: u32, count: u32) -> (i32, H) {
            self.line(format!("dd_create_vertex_buffer {} {caps:x} {fvf:x} {count}", hx(dd)));
            let r = self.result("create_vb");
            (r, if r == GX_OK { 11 as H } else { NULL })
        }
        fn device_release(&mut self, d: H) {
            self.line(format!("device_release {}", hx(d)));
        }
        fn device_get_caps(&mut self, d: H) -> DevCaps {
            self.line(format!("device_get_caps {}", hx(d)));
            DevCaps { dwDevCaps: 0, dwDeviceRenderBitDepth: self.result("device_caps") as u32 }
        }
        fn device_enum_texture_formats(&mut self, d: H, f: &mut dyn FnMut(&PixelFormat) -> bool) -> i32 {
            self.line(format!("device_enum_texture_formats {}", hx(d)));
            let formats = self.texture_formats.clone();
            for p in &formats {
                if f(p) {
                    break;
                }
            }
            self.result("enum_tex")
        }
        fn device_begin_scene(&mut self, d: H) -> i32 {
            self.line(format!("device_begin_scene {}", hx(d)));
            self.result("begin_scene")
        }
        fn device_end_scene(&mut self, d: H) -> i32 {
            self.line(format!("device_end_scene {}", hx(d)));
            self.result("end_scene")
        }
        fn device_set_render_target(&mut self, d: H, s: H) -> i32 {
            self.line(format!("device_set_render_target {} {}", hx(d), hx(s)));
            self.result("set_rt")
        }
        fn device_set_viewport(&mut self, d: H, v: &Viewport) -> i32 {
            let (x, y, w, h) = (v.dwX, v.dwY, v.dwWidth, v.dwHeight);
            self.line(format!(
                "device_set_viewport {} {x} {y} {w} {h} {:08x} {:08x}",
                hx(d),
                v.dvMinZ.to_bits(),
                v.dvMaxZ.to_bits()
            ));
            self.result("set_viewport")
        }
        fn device_clear(&mut self, d: H, r: IRect, flags: u32, color: u32, z: f32) -> i32 {
            self.line(format!(
                "device_clear {} {},{},{},{} {flags:x} {color:x} {:08x}",
                hx(d),
                r.left,
                r.top,
                r.right,
                r.bottom,
                z.to_bits()
            ));
            self.result("clear")
        }
        fn device_set_transform(&mut self, d: H, state: u32, m: &[f32; 16]) -> i32 {
            let values: Vec<String> = m.iter().map(|v| format!("{:08x}", v.to_bits())).collect();
            self.line(format!("device_set_transform {} {state} {}", hx(d), values.join(",")));
            self.result("set_transform")
        }
        fn device_set_render_state(&mut self, d: H, state: u32, value: u32) -> i32 {
            self.line(format!("device_set_render_state {} {state} {value:x}", hx(d)));
            self.result("set_render_state")
        }
        fn device_set_stage_state(&mut self, d: H, stage: u32, state: u32, value: u32) -> i32 {
            self.line(format!("device_set_stage_state {} {stage} {state} {value:x}", hx(d)));
            self.result("set_stage_state")
        }
        fn device_set_light(&mut self, d: H, index: u32, l: &Light) -> i32 {
            let hexes = |v: &[f32]| v.iter().map(|x| format!("{:08x}", x.to_bits())).collect::<Vec<_>>().join(",");
            self.line(format!(
                "device_set_light {} {index} kind={} d={} p={} dr={} r={:08x} a={}",
                hx(d),
                l.kind,
                hexes(&l.diffuse),
                hexes(&l.position),
                hexes(&l.direction),
                l.range.to_bits(),
                hexes(&l.attenuation)
            ));
            self.result("set_light")
        }
        fn device_light_enable(&mut self, d: H, index: u32, on: bool) -> i32 {
            self.line(format!("device_light_enable {} {index} {}", hx(d), on as u32));
            self.result("light_enable")
        }
        fn device_validate(&mut self, d: H) -> (i32, u32) {
            self.line(format!("device_validate {}", hx(d)));
            (self.result("validate"), 1)
        }
    }
}
