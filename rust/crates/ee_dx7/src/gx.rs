//! The GX contract: the `extern "C"` functions of `ee_gx` (`rust/crates/ee_gx/src/ffi.rs`), resolved in the staticlib.
//! Not compiled in `cargo test` (no wgpu in the test binary): the logic is tested through [`crate::state::Device`] and
//! [`crate::api::Gx`] with a recorder. The handle types (`GXDevice*`, ...) are the same opaque pointers the C++
//! `GX.h` shims hold. [`RawGx`] reproduces what those shims do (arguments they drop, constants they return).
#![allow(non_snake_case)] // keep the C names
#![allow(clippy::useless_conversion, clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)] // c_ulong differs per target

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::api::*;
use crate::state::Device;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_long, c_ulong, c_void};

    /// `GXMATERIAL` (`rust/crates/ee_gx/src/abi.rs GxMaterial`): colours of diffuse/ambient/specular/emissive, power.
    #[repr(C)]
    pub struct GxMaterial {
        pub diffuse: [f32; 4],
        pub ambient: [f32; 4],
        pub specular: [f32; 4],
        pub emissive: [f32; 4],
        pub power: f32,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxPixelFormat {
        pub dwSize: c_ulong,
        pub dwFlags: c_ulong,
        pub dwFourCC: c_ulong,
        pub dwRGBBitCount: c_ulong,
        pub dwRBitMask: c_ulong,
        pub dwGBitMask: c_ulong,
        pub dwBBitMask: c_ulong,
        pub dwRGBAlphaBitMask: c_ulong,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct GxSurfaceDesc {
        pub dwSize: c_ulong,
        pub dwFlags: c_ulong,
        pub dwHeight: c_ulong,
        pub dwWidth: c_ulong,
        pub lPitch: c_long,
        pub dwBackBufferCount: c_ulong,
        pub dwMipMapCount: c_ulong,
        pub dwRefreshRate: c_ulong,
        pub lpSurface: *mut c_void,
        pub ddckCKSrcBlt: [c_ulong; 2],
        pub ddpfPixelFormat: GxPixelFormat,
        pub ddsCaps: [c_ulong; 2],
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxCaps {
        pub dwSize: c_ulong,
        pub dwCaps: c_ulong,
        pub dwFXCaps: c_ulong,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct GxDeviceIdentifier {
        pub szDescription: [c_char; 512],
        pub dwVendorId: c_ulong,
        pub dwDeviceId: c_ulong,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxDeviceDesc {
        pub dwDevCaps: c_ulong,
        pub dwDeviceRenderBitDepth: c_ulong,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxRect {
        pub x1: c_long,
        pub y1: c_long,
        pub x2: c_long,
        pub y2: c_long,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxViewport {
        pub dwX: c_ulong,
        pub dwY: c_ulong,
        pub dwWidth: c_ulong,
        pub dwHeight: c_ulong,
        pub dvMinZ: f32,
        pub dvMaxZ: f32,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxLight {
        pub dltType: c_ulong,
        pub dcvDiffuse: [f32; 4],
        pub dcvSpecular: [f32; 4],
        pub dcvAmbient: [f32; 4],
        pub dvPosition: [f32; 3],
        pub dvDirection: [f32; 3],
        pub dvRange: f32,
        pub dvFalloff: f32,
        pub dvAttenuation0: f32,
        pub dvAttenuation1: f32,
        pub dvAttenuation2: f32,
        pub dvTheta: f32,
        pub dvPhi: f32,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxBltFx {
        pub dwSize: c_ulong,
        pub dwFillColor: c_ulong,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    pub struct GxColorKey {
        pub low: c_ulong,
        pub high: c_ulong,
    }

    pub type EnumDevicesCallback =
        unsafe extern "C" fn(*mut c_char, *mut c_char, *mut GxDeviceDesc, *mut c_void) -> c_long;
    pub type EnumModesCallback = unsafe extern "C" fn(*mut GxSurfaceDesc, *mut c_void) -> c_long;
    pub type EnumFormatsCallback = unsafe extern "C" fn(*mut GxPixelFormat, *mut c_void) -> c_long;
    pub type EnumerateCallback =
        unsafe extern "C" fn(*mut c_void, *mut c_char, *mut c_char, *mut c_void, *mut c_void) -> i32;

    extern "C" {
        pub fn gx_device_set_render_state(device: *mut c_void, state: u32, value: c_ulong) -> c_long;
        pub fn gx_device_set_stage_state(device: *mut c_void, stage: c_ulong, state: u32, value: c_ulong) -> c_long;
        pub fn gx_device_set_texture(device: *mut c_void, stage: c_ulong, texture: *mut c_void) -> c_long;
        pub fn gx_device_set_material(device: *mut c_void, material: *const GxMaterial) -> c_long;
        pub fn gx_device_release(device: *mut c_void) -> c_ulong;
        pub fn gx_device_get_caps(out: *mut GxDeviceDesc) -> c_long;
        pub fn gx_device_enum_texture_formats(callback: EnumFormatsCallback, context: *mut c_void) -> c_long;
        pub fn gx_device_begin_scene() -> c_long;
        pub fn gx_device_set_render_target(device: *mut c_void, target: *mut c_void) -> c_long;
        pub fn gx_device_set_viewport(device: *mut c_void, viewport: *const GxViewport) -> c_long;
        pub fn gx_device_clear(
            device: *mut c_void,
            count: c_ulong,
            rects: *const GxRect,
            flags: c_ulong,
            color: c_ulong,
            z: f32,
        ) -> c_long;
        pub fn gx_device_set_transform(device: *mut c_void, state: u32, matrix: *const f32) -> c_long;
        pub fn gx_device_set_light(device: *mut c_void, index: c_ulong, light: *const GxLight) -> c_long;
        pub fn gx_device_light_enable(device: *mut c_void, index: c_ulong, enable: i32) -> c_long;

        pub fn gx_direct_draw_enumerate(callback: EnumerateCallback, context: *mut c_void) -> c_long;
        pub fn gx_direct_draw_create(out: *mut *mut c_void) -> c_long;
        pub fn gx_direct_draw_release(dd: *mut c_void) -> c_ulong;
        pub fn gx_direct_draw_query_interface(dd: *mut c_void, iid: u32, out: *mut *mut c_void) -> c_long;
        pub fn gx_direct_draw_get_caps(caps: *mut GxCaps, hel: *mut GxCaps) -> c_long;
        pub fn gx_direct_draw_enum_display_modes(
            dd: *mut c_void,
            context: *mut c_void,
            callback: EnumModesCallback,
        ) -> c_long;
        pub fn gx_direct_draw_get_display_mode(dd: *mut c_void, out: *mut GxSurfaceDesc) -> c_long;
        pub fn gx_direct_draw_set_display_mode(dd: *mut c_void, w: c_ulong, h: c_ulong, bits: c_ulong) -> c_long;
        pub fn gx_direct_draw_set_cooperative_level(dd: *mut c_void, window: *mut c_void, flags: c_ulong) -> c_long;
        pub fn gx_direct_draw_create_surface(
            dd: *mut c_void,
            desc: *mut GxSurfaceDesc,
            out: *mut *mut c_void,
        ) -> c_long;
        pub fn gx_direct_draw_get_available_vid_mem(total: *mut c_ulong, free: *mut c_ulong) -> c_long;
        pub fn gx_direct_draw_get_device_identifier(out: *mut GxDeviceIdentifier) -> c_long;
        pub fn gx_direct_draw_enum_devices(callback: EnumDevicesCallback, context: *mut c_void) -> c_long;
        pub fn gx_direct_draw_create_device(target: *mut c_void, out: *mut *mut c_void) -> c_long;
        pub fn gx_direct_draw_create_vertex_buffer(fvf: c_ulong, count: c_ulong, out: *mut *mut c_void) -> c_long;

        pub fn gx_surface_release(surface: *mut c_void) -> c_ulong;
        pub fn gx_surface_add_attached(surface: *mut c_void, other: *mut c_void) -> c_long;
        pub fn gx_surface_delete_attached(surface: *mut c_void, other: *mut c_void) -> c_long;
        pub fn gx_surface_get_attached(surface: *mut c_void, caps: *mut [c_ulong; 2], out: *mut *mut c_void) -> c_long;
        pub fn gx_surface_blt(
            surface: *mut c_void,
            destination: *const GxRect,
            source: *mut c_void,
            source_rect: *const GxRect,
            flags: c_ulong,
            fx: *const GxBltFx,
        ) -> c_long;
        pub fn gx_surface_flip(surface: *mut c_void, flags: c_ulong) -> c_long;
        pub fn gx_surface_lock(
            surface: *mut c_void,
            rect: *const GxRect,
            out: *mut GxSurfaceDesc,
            flags: c_ulong,
        ) -> c_long;
        pub fn gx_surface_unlock(surface: *mut c_void) -> c_long;
        pub fn gx_surface_set_color_key(surface: *mut c_void, key: *const GxColorKey) -> c_long;

        pub fn gx_vertex_buffer_release(buffer: *mut c_void) -> c_ulong;
        pub fn gx_vertex_buffer_lock(buffer: *mut c_void, data: *mut *mut c_void, size: *mut c_ulong) -> c_long;

        pub fn PlatSleep(milliseconds: u32);
        pub fn ee_dx7cpp_record_native_error(error: c_long);
    }
}

use sys::*;

/// `GXDevice*`.
#[derive(Clone, Copy)]
pub struct RawDevice(pub *mut c_void);

impl Device for RawDevice {
    fn set_render_state(&mut self, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_render_state(self.0, state, c_ulong::from(value)) as i32 }
    }

    fn set_stage_state(&mut self, stage: u32, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_stage_state(self.0, c_ulong::from(stage), state, c_ulong::from(value)) as i32 }
    }

    fn set_texture(&mut self, stage: u32, surface: *mut c_void) -> i32 {
        unsafe { gx_device_set_texture(self.0, c_ulong::from(stage), surface) as i32 }
    }

    fn set_material_white(&mut self) -> i32 {
        let material = GxMaterial {
            diffuse: [1.0, 1.0, 1.0, 1.0],
            ambient: [1.0, 1.0, 1.0, 1.0],
            specular: [0.0; 4],
            emissive: [0.0; 4],
            power: 0.0,
        };
        unsafe { gx_device_set_material(self.0, &material) as i32 }
    }
}

pub struct RawHost;

impl Host for RawHost {
    fn sleep(&mut self, milliseconds: u32) {
        unsafe { PlatSleep(milliseconds) }
    }
    fn record_native_error(&mut self, error: i32) {
        unsafe { ee_dx7cpp_record_native_error(error as c_long) }
    }
}

/// The GX shim's behaviour for the plug-in's calls: what `GX.cpp` forwards to `gx_*` and what it answers itself.
pub struct RawGx;

fn rect_c(r: IRect) -> GxRect {
    GxRect { x1: r.left as c_long, y1: r.top as c_long, x2: r.right as c_long, y2: r.bottom as c_long }
}

fn rect_ptr(r: &Option<GxRect>) -> *const GxRect {
    r.as_ref().map_or(core::ptr::null(), |p| p as *const GxRect)
}

fn pixel_format(p: &GxPixelFormat) -> PixelFormat {
    PixelFormat {
        dwFlags: p.dwFlags as u32,
        dwFourCC: p.dwFourCC as u32,
        dwRGBBitCount: p.dwRGBBitCount as u32,
        dwRBitMask: p.dwRBitMask as u32,
        dwGBitMask: p.dwGBitMask as u32,
        dwBBitMask: p.dwBBitMask as u32,
        dwRGBAlphaBitMask: p.dwRGBAlphaBitMask as u32,
    }
}

fn surface_desc_c(desc: &SurfaceDesc) -> GxSurfaceDesc {
    GxSurfaceDesc {
        dwSize: core::mem::size_of::<GxSurfaceDesc>() as c_ulong,
        dwFlags: desc.dwFlags as c_ulong,
        dwHeight: desc.dwHeight as c_ulong,
        dwWidth: desc.dwWidth as c_ulong,
        lPitch: 0,
        dwBackBufferCount: desc.dwBackBufferCount as c_ulong,
        dwMipMapCount: desc.dwMipMapCount as c_ulong,
        dwRefreshRate: desc.dwRefreshRate as c_ulong,
        lpSurface: NULL,
        ddckCKSrcBlt: [0; 2],
        ddpfPixelFormat: GxPixelFormat {
            dwSize: core::mem::size_of::<GxPixelFormat>() as c_ulong,
            dwFlags: desc.format.dwFlags as c_ulong,
            dwFourCC: desc.format.dwFourCC as c_ulong,
            dwRGBBitCount: desc.format.dwRGBBitCount as c_ulong,
            dwRBitMask: desc.format.dwRBitMask as c_ulong,
            dwGBitMask: desc.format.dwGBitMask as c_ulong,
            dwBBitMask: desc.format.dwBBitMask as c_ulong,
            dwRGBAlphaBitMask: desc.format.dwRGBAlphaBitMask as c_ulong,
        },
        ddsCaps: [desc.ddsCaps as c_ulong, desc.ddsCaps2 as c_ulong],
    }
}

fn surface_desc(desc: &GxSurfaceDesc) -> SurfaceDesc {
    SurfaceDesc {
        dwFlags: desc.dwFlags as u32,
        dwHeight: desc.dwHeight as u32,
        dwWidth: desc.dwWidth as u32,
        dwBackBufferCount: desc.dwBackBufferCount as u32,
        dwMipMapCount: desc.dwMipMapCount as u32,
        dwRefreshRate: desc.dwRefreshRate as u32,
        format: pixel_format(&desc.ddpfPixelFormat),
        ddsCaps: desc.ddsCaps[0] as u32,
        ddsCaps2: desc.ddsCaps[1] as u32,
    }
}

fn zero_desc() -> GxSurfaceDesc {
    let mut d: GxSurfaceDesc = unsafe { core::mem::zeroed() };
    d.dwSize = core::mem::size_of::<GxSurfaceDesc>() as c_ulong;
    d
}

/// A callback context: the Rust closure the C callback dispatches to (no reference to GX state crosses the call).
type DevicesFn<'a> = &'a mut dyn FnMut(DevCaps) -> bool;

unsafe extern "C" fn enum_devices_tramp(
    _description: *mut c_char,
    _name: *mut c_char,
    caps: *mut GxDeviceDesc,
    context: *mut c_void,
) -> c_long {
    let f = unsafe { &mut *(context as *mut DevicesFn<'_>) };
    let caps = unsafe { *caps };
    let cancel =
        f(DevCaps { dwDevCaps: caps.dwDevCaps as u32, dwDeviceRenderBitDepth: caps.dwDeviceRenderBitDepth as u32 });
    if cancel {
        GXENUMRET_CANCEL as c_long
    } else {
        GXENUMRET_OK as c_long
    }
}

type ModesFn<'a> = &'a mut dyn FnMut(&SurfaceDesc);

unsafe extern "C" fn enum_modes_tramp(desc: *mut GxSurfaceDesc, context: *mut c_void) -> c_long {
    let f = unsafe { &mut *(context as *mut ModesFn<'_>) };
    let desc = unsafe { surface_desc(&*desc) };
    f(&desc);
    GXENUMRET_OK as c_long
}

type FormatsFn<'a> = &'a mut dyn FnMut(&PixelFormat) -> bool;

unsafe extern "C" fn enum_formats_tramp(format: *mut GxPixelFormat, context: *mut c_void) -> c_long {
    let f = unsafe { &mut *(context as *mut FormatsFn<'_>) };
    let p = pixel_format(unsafe { &*format });
    if f(&p) {
        GXENUMRET_CANCEL as c_long
    } else {
        GXENUMRET_OK as c_long
    }
}

impl Gx for RawGx {
    fn surface_release(&mut self, s: H) {
        unsafe { gx_surface_release(s) };
    }
    fn surface_add_attached(&mut self, s: H, o: H) -> i32 {
        unsafe { gx_surface_add_attached(s, o) as i32 }
    }
    fn surface_delete_attached(&mut self, s: H, o: H) -> i32 {
        unsafe { gx_surface_delete_attached(s, o) as i32 }
    }
    fn surface_get_attached(&mut self, s: H, caps: u32) -> H {
        let mut out = NULL;
        let mut c = [caps as c_ulong, 0];
        unsafe { gx_surface_get_attached(s, &mut c, &mut out) };
        out
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
        let d = dst.map(rect_c);
        let sr = src_rect.map(rect_c);
        let fx =
            fill.map(|c| GxBltFx { dwSize: core::mem::size_of::<GxBltFx>() as c_ulong, dwFillColor: c as c_ulong });
        unsafe {
            gx_surface_blt(
                s,
                rect_ptr(&d),
                src,
                rect_ptr(&sr),
                flags as c_ulong,
                fx.as_ref().map_or(core::ptr::null(), |f| f as *const GxBltFx),
            ) as i32
        }
    }
    fn surface_flip(&mut self, s: H, flags: u32) -> i32 {
        unsafe { gx_surface_flip(s, flags as c_ulong) as i32 }
    }
    fn surface_lock(&mut self, s: H) -> Locked {
        let mut desc = zero_desc();
        let result = unsafe { gx_surface_lock(s, core::ptr::null(), &mut desc, 0) as i32 };
        Locked { result, memory: desc.lpSurface, pitch: desc.lPitch as i32 }
    }
    fn surface_unlock(&mut self, s: H) -> i32 {
        unsafe { gx_surface_unlock(s) as i32 }
    }
    fn surface_set_color_key(&mut self, s: H, color: u32) -> i32 {
        let key = GxColorKey { low: color as c_ulong, high: color as c_ulong };
        unsafe { gx_surface_set_color_key(s, &key) as i32 }
    }
    fn surface_restore(&mut self, _s: H) -> i32 {
        GX_OK // `GXSurface::Restore` in GX.cpp
    }
    fn vb_release(&mut self, b: H) {
        unsafe { gx_vertex_buffer_release(b) };
    }
    fn vb_lock(&mut self, b: H, _flags: u32) -> (i32, *mut c_void) {
        let mut data = NULL;
        let mut size: c_ulong = 0;
        let r = unsafe { gx_vertex_buffer_lock(b, &mut data, &mut size) as i32 };
        (r, data)
    }
    fn vb_unlock(&mut self, _b: H) -> i32 {
        GX_OK // `GXVertexBuffer::Unlock` in GX.cpp
    }
    fn dd_create(&mut self) -> (i32, H) {
        let mut out = NULL;
        let r = unsafe { gx_direct_draw_create(&mut out) as i32 };
        (r, out)
    }
    fn dd_release(&mut self, dd: H) {
        unsafe { gx_direct_draw_release(dd) };
    }
    fn dd_query_interface(&mut self, dd: H, iid: u32) -> (i32, H) {
        let mut out = NULL;
        let r = unsafe { gx_direct_draw_query_interface(dd, iid, &mut out) as i32 };
        (r, out)
    }
    fn dd_get_caps(&mut self, _dd: H) -> (i32, Caps) {
        let mut caps = GxCaps { dwSize: core::mem::size_of::<GxCaps>() as c_ulong, ..Default::default() };
        let r = unsafe { gx_direct_draw_get_caps(&mut caps, core::ptr::null_mut()) as i32 };
        (r, Caps { dwCaps: caps.dwCaps as u32, dwFXCaps: caps.dwFXCaps as u32 })
    }
    fn dd_enum_display_modes(&mut self, dd: H, f: &mut dyn FnMut(&SurfaceDesc)) -> i32 {
        let mut f: ModesFn<'_> = f;
        let context = &mut f as *mut ModesFn<'_> as *mut c_void;
        unsafe { gx_direct_draw_enum_display_modes(dd, context, enum_modes_tramp) as i32 }
    }
    fn dd_get_display_mode(&mut self, dd: H) -> (i32, SurfaceDesc) {
        let mut desc = zero_desc();
        let r = unsafe { gx_direct_draw_get_display_mode(dd, &mut desc) as i32 };
        (r, surface_desc(&desc))
    }
    fn dd_set_display_mode(&mut self, dd: H, w: u32, h: u32, bits: u32, _rate: u32, _flags: u32) -> i32 {
        unsafe { gx_direct_draw_set_display_mode(dd, w as c_ulong, h as c_ulong, bits as c_ulong) as i32 }
    }
    fn dd_set_cooperative_level(&mut self, dd: H, window: H, flags: u32) -> i32 {
        unsafe { gx_direct_draw_set_cooperative_level(dd, window, flags as c_ulong) as i32 }
    }
    fn dd_create_surface(&mut self, dd: H, desc: &SurfaceDesc) -> (i32, H) {
        let mut c = surface_desc_c(desc);
        let mut out = NULL;
        let r = unsafe { gx_direct_draw_create_surface(dd, &mut c, &mut out) as i32 };
        (r, out)
    }
    fn dd_get_available_vid_mem(&mut self, _dd: H, _caps: u32) -> (i32, u32, u32) {
        let (mut total, mut free): (c_ulong, c_ulong) = (0, 0);
        let r = unsafe { gx_direct_draw_get_available_vid_mem(&mut total, &mut free) as i32 };
        (r, total as u32, free as u32)
    }
    fn dd_get_device_identifier(&mut self, _dd: H) -> (i32, Identifier) {
        let mut raw: GxDeviceIdentifier = unsafe { core::mem::zeroed() };
        let r = unsafe { gx_direct_draw_get_device_identifier(&mut raw) as i32 };
        let bytes: Vec<u8> = raw.szDescription.iter().take_while(|c| **c != 0).map(|c| *c as u8).collect();
        (r, Identifier { description: bytes, vendor: raw.dwVendorId as u32, device: raw.dwDeviceId as u32 })
    }
    fn dd_test_cooperative_level(&mut self, _dd: H) -> i32 {
        GX_OK // `GXDirectDraw::TestCooperativeLevel` in GX.cpp
    }
    fn dd_restore_all_surfaces(&mut self, _dd: H) -> i32 {
        GX_OK // `GXDirectDraw::RestoreAllSurfaces` in GX.cpp
    }
    fn dd_enum_devices(&mut self, _dd: H, f: &mut dyn FnMut(DevCaps) -> bool) -> i32 {
        let mut f: DevicesFn<'_> = f;
        let context = &mut f as *mut DevicesFn<'_> as *mut c_void;
        unsafe { gx_direct_draw_enum_devices(enum_devices_tramp, context) as i32 }
    }
    fn dd_create_device(&mut self, _dd: H, _iid: u32, target: H) -> (i32, H) {
        let mut out = NULL;
        let r = unsafe { gx_direct_draw_create_device(target, &mut out) as i32 };
        (r, out)
    }
    fn dd_create_vertex_buffer(&mut self, _dd: H, _caps: u32, fvf: u32, count: u32) -> (i32, H) {
        let mut out = NULL;
        let r = unsafe { gx_direct_draw_create_vertex_buffer(fvf as c_ulong, count as c_ulong, &mut out) as i32 };
        (r, out)
    }
    fn device_release(&mut self, d: H) {
        unsafe { gx_device_release(d) };
    }
    fn device_get_caps(&mut self, _d: H) -> DevCaps {
        let mut caps = GxDeviceDesc::default();
        unsafe { gx_device_get_caps(&mut caps) };
        DevCaps { dwDevCaps: caps.dwDevCaps as u32, dwDeviceRenderBitDepth: caps.dwDeviceRenderBitDepth as u32 }
    }
    fn device_enum_texture_formats(&mut self, _d: H, f: &mut dyn FnMut(&PixelFormat) -> bool) -> i32 {
        let mut f: FormatsFn<'_> = f;
        let context = &mut f as *mut FormatsFn<'_> as *mut c_void;
        unsafe { gx_device_enum_texture_formats(enum_formats_tramp, context) as i32 }
    }
    fn device_begin_scene(&mut self, _d: H) -> i32 {
        unsafe { gx_device_begin_scene() as i32 }
    }
    fn device_end_scene(&mut self, _d: H) -> i32 {
        GX_OK // `GXDevice::EndScene` in GX.cpp
    }
    fn device_set_render_target(&mut self, d: H, s: H) -> i32 {
        unsafe { gx_device_set_render_target(d, s) as i32 }
    }
    fn device_set_viewport(&mut self, d: H, v: &Viewport) -> i32 {
        let c = GxViewport {
            dwX: v.dwX,
            dwY: v.dwY,
            dwWidth: v.dwWidth,
            dwHeight: v.dwHeight,
            dvMinZ: v.dvMinZ,
            dvMaxZ: v.dvMaxZ,
        };
        unsafe { gx_device_set_viewport(d, &c) as i32 }
    }
    fn device_clear(&mut self, d: H, rect: IRect, flags: u32, color: u32, z: f32) -> i32 {
        let c = rect_c(rect);
        unsafe { gx_device_clear(d, 1, &c, flags as c_ulong, color as c_ulong, z) as i32 }
    }
    fn device_set_transform(&mut self, d: H, state: u32, m: &[f32; 16]) -> i32 {
        unsafe { gx_device_set_transform(d, state, m.as_ptr()) as i32 }
    }
    fn device_set_render_state(&mut self, d: H, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_render_state(d, state, value as c_ulong) as i32 }
    }
    fn device_set_stage_state(&mut self, d: H, stage: u32, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_stage_state(d, stage as c_ulong, state, value as c_ulong) as i32 }
    }
    fn device_set_light(&mut self, d: H, index: u32, l: &Light) -> i32 {
        let light = GxLight {
            dltType: l.kind as c_ulong,
            dcvDiffuse: [l.diffuse[0], l.diffuse[1], l.diffuse[2], 0.0],
            dcvSpecular: [0.0; 4],
            dcvAmbient: [0.0; 4],
            dvPosition: l.position,
            dvDirection: l.direction,
            dvRange: l.range,
            dvFalloff: 0.0,
            dvAttenuation0: l.attenuation[0],
            dvAttenuation1: l.attenuation[1],
            dvAttenuation2: l.attenuation[2],
            dvTheta: 0.0,
            dvPhi: 0.0,
        };
        unsafe { gx_device_set_light(d, index as c_ulong, &light) as i32 }
    }
    fn device_light_enable(&mut self, d: H, index: u32, on: bool) -> i32 {
        unsafe { gx_device_light_enable(d, index as c_ulong, on as i32) as i32 }
    }
    fn device_validate(&mut self, _d: H) -> (i32, u32) {
        (GX_OK, 1) // `GXDevice::ValidateDevice` in GX.cpp
    }
}

/// `GXDirectDrawEnumerateEx` (`GX.cpp`: the flags and GUID filter are not forwarded).
pub(crate) unsafe fn direct_draw_enumerate(
    callback: unsafe extern "C" fn(*mut c_void, *mut c_char, *mut c_char, *mut c_void, *mut c_void) -> i32,
    context: *mut c_void,
) -> i32 {
    unsafe { gx_direct_draw_enumerate(callback, context) as i32 }
}
