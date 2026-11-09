//! The C ABI: `include/gx/GX.h`'s methods as `extern "C"` functions, the opaque handles, the one reentrant lock. Compiled
//! out of `cargo test` (it imports the platform layer, absent from a test binary).
#![allow(clippy::missing_safety_doc)] // the contract is GX.h's; these are its shims

use std::cell::RefCell;
use std::ffi::{c_char, c_long, c_ulong, c_void};
use std::sync::OnceLock;

use parking_lot::{ReentrantMutex, ReentrantMutexGuard};

use crate::abi::*;
use crate::device::{self, Device, VertexBuffer};
use crate::directdraw::{self, DirectDraw};
use crate::gpu::WgpuBackend;
use crate::gx::*;
use crate::keys;
use crate::modes;
use crate::plat::RealPlatform;
use crate::surface::SurfaceId;

type GxCore = Gx<WgpuBackend, RealPlatform>;

struct Shared(RefCell<GxCore>);
// SAFETY: every access holds GX's lock (below); the wgpu objects themselves are Send, and the raw handles inside are
// identities only dereferenced under the lock.
unsafe impl Send for Shared {}

static GX: OnceLock<ReentrantMutex<Shared>> = OnceLock::new();

fn lock() -> ReentrantMutexGuard<'static, Shared> {
    GX.get_or_init(|| ReentrantMutex::new(Shared(RefCell::new(Gx::new(WgpuBackend::default(), RealPlatform))))).lock()
}

fn with<R>(f: impl FnOnce(&mut GxCore) -> R) -> R {
    let guard = lock();
    let mut gx = guard.0.borrow_mut();
    f(&mut gx)
}

unsafe fn id(surface: *mut c_void) -> SurfaceId {
    surface_of_handle(surface)
}

// ---- GXDirectDraw

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_create(out: *mut *mut c_void) -> GxResult {
    *out = std::ptr::null_mut();
    with(|gx| {
        if !gx.start() {
            return GXERR_GENERIC;
        }
        let info = gx.plat.display_info();
        *out = Box::into_raw(Box::new(DirectDraw::new(&info))) as *mut c_void;
        GX_OK
    })
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_add_ref(dd: *mut c_void) -> c_ulong {
    let _guard = lock();
    let dd = &mut *(dd as *mut DirectDraw);
    dd.references += 1;
    dd.references as c_ulong
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_release(dd: *mut c_void) -> c_ulong {
    let _guard = lock();
    let object = &mut *(dd as *mut DirectDraw);
    object.references -= 1;
    let count = object.references;
    if count == 0 {
        drop(Box::from_raw(dd as *mut DirectDraw));
    }
    count as c_ulong
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_query_interface(dd: *mut c_void, iid: u32, out: *mut *mut c_void) -> GxResult {
    if iid != GXIID_DIRECT3D7 {
        *out = std::ptr::null_mut();
        return GXERR_UNSUPPORTED;
    }
    gx_direct_draw_add_ref(dd);
    *out = dd;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_get_caps(caps: *mut GxCaps, hel_caps: *mut GxCaps) -> GxResult {
    if let Some(caps) = caps.as_mut() {
        caps.dwCaps = GXCAPS_3D as c_ulong;
        caps.dwFXCaps = (GXFXCAPS_BLTARITHSTRETCHY
            | GXFXCAPS_BLTSHRINKX
            | GXFXCAPS_BLTSHRINKY
            | GXFXCAPS_BLTSTRETCHX
            | GXFXCAPS_BLTSTRETCHY) as c_ulong;
    }
    if let Some(caps) = hel_caps.as_mut() {
        caps.dwCaps = 0;
        caps.dwFXCaps = 0;
    }
    GX_OK
}

type EnumModesCallback = unsafe extern "C" fn(*mut GxSurfaceDesc, *mut c_void) -> GxResult;

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_enum_display_modes(
    _dd: *mut c_void,
    context: *mut c_void,
    callback: EnumModesCallback,
) -> GxResult {
    let info = with(|gx| gx.plat.display_info());
    let sizes = modes::display_mode_sizes(&info);
    for bits in [16, 32] {
        for &(w, h) in &sizes {
            let mut mode = directdraw::mode_description(w, h, bits);
            if callback(&mut mode, context) == GXENUMRET_CANCEL {
                return GX_OK;
            }
        }
    }
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_get_display_mode(dd: *mut c_void, out: *mut GxSurfaceDesc) -> GxResult {
    *out = (*(dd as *mut DirectDraw)).display_mode();
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_set_display_mode(
    dd: *mut c_void,
    width: c_ulong,
    height: c_ulong,
    bits: c_ulong,
) -> GxResult {
    let object = &mut *(dd as *mut DirectDraw);
    // no mode switch: the game renders at this size and the frame is scaled to the window
    object.mode_width = width as u32;
    object.mode_height = height as u32;
    object.mode_bits = bits as u32;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_set_cooperative_level(
    dd: *mut c_void,
    window: *mut c_void,
    flags: c_ulong,
) -> GxResult {
    let object = &mut *(dd as *mut DirectDraw);
    object.window = window;
    object.cooperative_flags = flags as u32;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_create_surface(
    dd: *mut c_void,
    desc: *mut GxSurfaceDesc,
    out: *mut *mut c_void,
) -> GxResult {
    *out = std::ptr::null_mut();
    with(|gx| match directdraw::create_surface(gx, &*(dd as *mut DirectDraw), &*desc) {
        Ok(id) => {
            *out = gx.s(id).handle;
            GX_OK
        }
        Err(error) => error,
    })
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_get_available_vid_mem(total: *mut c_ulong, free: *mut c_ulong) -> GxResult {
    *total = 512 << 20;
    *free = 448 << 20;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_get_device_identifier(out: *mut GxDeviceIdentifier) -> GxResult {
    let name = with(|gx| (format!("wgpu {}", gx.adapter.name), gx.adapter.vendor_id, gx.adapter.device_id));
    let (text, vendor, device) = name;
    let out = &mut *out;
    out.szDescription = [0; 512];
    for (slot, byte) in out.szDescription.iter_mut().zip(text.bytes().take(511)) {
        *slot = byte as c_char;
    }
    out.dwVendorId = vendor as c_ulong;
    out.dwDeviceId = device as c_ulong;
    GX_OK
}

type EnumDevicesCallback = unsafe extern "C" fn(*mut c_char, *mut c_char, *mut GxDeviceDesc, *mut c_void) -> GxResult;

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_enum_devices(callback: EnumDevicesCallback, context: *mut c_void) -> GxResult {
    let mut caps = GxDeviceDesc {
        dwDevCaps: (GXDEVCAPS_HWRASTERIZATION | GXDEVCAPS_HWTRANSFORMANDLIGHT | GXDEVCAPS_FLOATTLVERTEX) as c_ulong,
        dwDeviceRenderBitDepth: (GXBD_16 | GXBD_32) as c_ulong,
    };
    let mut description = *b"wgpu\0";
    let mut name = *b"wgpu\0";
    callback(description.as_mut_ptr() as *mut c_char, name.as_mut_ptr() as *mut c_char, &mut caps, context);
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_create_vertex_buffer(
    fvf: c_ulong,
    count: c_ulong,
    out: *mut *mut c_void,
) -> GxResult {
    let _guard = lock();
    *out = Box::into_raw(Box::new(VertexBuffer::new(fvf as u32, count as u32))) as *mut c_void;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_create_device(target: *mut c_void, out: *mut *mut c_void) -> GxResult {
    *out = std::ptr::null_mut();
    if target.is_null() {
        return GXERR_INVALIDPARAMS;
    }
    with(|gx| {
        let device = directdraw::create_device(gx, id(target));
        *out = Box::into_raw(Box::new(device)) as *mut c_void;
        GX_OK
    })
}

type EnumerateCallback = unsafe extern "C" fn(*mut c_void, *mut c_char, *mut c_char, *mut c_void, *mut c_void) -> i32;

#[no_mangle]
pub unsafe extern "C" fn gx_direct_draw_enumerate(callback: EnumerateCallback, context: *mut c_void) -> GxResult {
    static PRIMARY_DISPLAY: u8 = 0;
    let mut description = *b"wgpu display\0";
    let mut name = *b"display\0";
    callback(
        std::ptr::null_mut(),
        description.as_mut_ptr() as *mut c_char,
        name.as_mut_ptr() as *mut c_char,
        context,
        &PRIMARY_DISPLAY as *const u8 as *mut c_void,
    );
    GX_OK
}

// ---- GXSurface

#[no_mangle]
pub unsafe extern "C" fn gx_surface_add_ref(surface: *mut c_void) -> c_ulong {
    with(|gx| gx.add_ref(id(surface)) as c_ulong)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_release(surface: *mut c_void) -> c_ulong {
    with(|gx| gx.release(id(surface)) as c_ulong)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_add_attached(surface: *mut c_void, other: *mut c_void) -> GxResult {
    if other.is_null() {
        return GXERR_INVALIDPARAMS;
    }
    with(|gx| gx.add_attached(id(surface), id(other)))
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_delete_attached(surface: *mut c_void, other: *mut c_void) -> GxResult {
    if other.is_null() {
        return GXERR_NOTFOUND;
    }
    with(|gx| gx.delete_attached(id(surface), id(other)))
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_get_attached(
    surface: *mut c_void,
    caps: *mut GxSCaps,
    out: *mut *mut c_void,
) -> GxResult {
    *out = std::ptr::null_mut();
    let caps = (*caps).dwCaps as u32;
    with(|gx| match gx.get_attached(id(surface), caps) {
        Some(found) => {
            *out = gx.s(found).handle;
            GX_OK
        }
        None => GXERR_NOTFOUND,
    })
}

/// Runs a present left for this layer: the hook first, outside the state borrow (it may call `GXTestReadFrame`).
unsafe fn run_present(guard: &ReentrantMutexGuard<'static, Shared>, outcome: Outcome) -> GxResult {
    if let Some(request) = outcome.present {
        let arguments = guard.0.borrow().hook_arguments(&request);
        if let Some((hook, window, frame)) = arguments {
            hook(window, frame);
        }
        guard.0.borrow_mut().present(&request);
    }
    outcome.result
}

unsafe fn rect(pointer: *const Rect) -> Option<Rect> {
    pointer.as_ref().copied()
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_blt(
    surface: *mut c_void,
    destination: *const Rect,
    source: *mut c_void,
    source_rect: *const Rect,
    flags: c_ulong,
    fx: *const GxBltFx,
) -> GxResult {
    let guard = lock();
    let fill = fx.as_ref().map_or(0, |fx| fx.dwFillColor) as u32;
    let source = if source.is_null() { None } else { Some(id(source)) };
    let outcome =
        guard.0.borrow_mut().blt(id(surface), rect(destination), source, rect(source_rect), flags as u32, fill);
    run_present(&guard, outcome)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_flip(surface: *mut c_void, flags: c_ulong) -> GxResult {
    let guard = lock();
    let outcome = guard.0.borrow_mut().flip(id(surface), flags as u32);
    run_present(&guard, outcome)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_lock(
    surface: *mut c_void,
    rect_pointer: *const Rect,
    out: *mut GxSurfaceDesc,
    flags: c_ulong,
) -> GxResult {
    with(|gx| {
        let identifier = id(surface);
        match gx.lock(identifier, rect(rect_pointer), flags as u32) {
            Ok(memory) => {
                let mut description = gx.surface_desc(identifier);
                description.lpSurface = memory as *mut c_void;
                *out = description;
                GX_OK
            }
            Err(error) => error,
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_unlock(surface: *mut c_void) -> GxResult {
    with(|gx| gx.unlock(id(surface)))
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_set_color_key(surface: *mut c_void, key: *const GxColorKey) -> GxResult {
    with(|gx| gx.set_color_key(id(surface), key.as_ref().map(|k| k.dwColorSpaceLowValue as u32)))
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_get_desc(surface: *mut c_void, out: *mut GxSurfaceDesc) -> GxResult {
    *out = with(|gx| gx.surface_desc(id(surface)));
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_render_scale(surface: *mut c_void) -> f32 {
    with(|gx| gx.s(id(surface)).scale)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_is_gpu_resident(surface: *mut c_void) -> bool {
    with(|gx| !gx.s(id(surface)).cpu_valid)
}

#[no_mangle]
pub unsafe extern "C" fn gx_surface_draw_text(
    surface: *mut c_void,
    bounds: *const Rect,
    draw: TextCallback,
    context: *mut c_void,
) -> GxResult {
    let guard = lock();
    let job = guard.0.borrow_mut().draw_text_begin(id(surface), *bounds);
    let mut pending = match job {
        TextJob::Done(result) => return result,
        TextJob::Pending(pending) => pending,
    };
    let (left, top) = (pending.bounds.left, pending.bounds.top);
    let target = pending.font_target();
    // the state borrow is released: the callback may call other GX functions
    let drawn = draw(&target, left as c_long, top as c_long, context);
    let result = guard.0.borrow_mut().draw_text_finish(pending, drawn);
    result
}

// ---- GXVertexBuffer

#[no_mangle]
pub unsafe extern "C" fn gx_vertex_buffer_add_ref(buffer: *mut c_void) -> c_ulong {
    let _guard = lock();
    let buffer = &mut *(buffer as *mut VertexBuffer);
    buffer.references += 1;
    buffer.references as c_ulong
}

#[no_mangle]
pub unsafe extern "C" fn gx_vertex_buffer_release(buffer: *mut c_void) -> c_ulong {
    let _guard = lock();
    let object = &mut *(buffer as *mut VertexBuffer);
    object.references -= 1;
    let count = object.references;
    if count == 0 {
        drop(Box::from_raw(buffer as *mut VertexBuffer));
    }
    count as c_ulong
}

#[no_mangle]
pub unsafe extern "C" fn gx_vertex_buffer_lock(
    buffer: *mut c_void,
    out_data: *mut *mut c_void,
    out_size: *mut c_ulong,
) -> GxResult {
    let buffer = &mut *(buffer as *mut VertexBuffer);
    *out_data = buffer.vertices.as_mut_ptr() as *mut c_void;
    if let Some(size) = out_size.as_mut() {
        *size = (buffer.stride * buffer.count) as c_ulong;
    }
    GX_OK
}

// ---- GXDevice

unsafe fn device_mut<'a>(device: *mut c_void) -> &'a mut Device {
    &mut *(device as *mut Device)
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_add_ref(device: *mut c_void) -> c_ulong {
    let _guard = lock();
    let object = device_mut(device);
    object.references += 1;
    object.references as c_ulong
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_release(device: *mut c_void) -> c_ulong {
    with(|gx| {
        let object = device_mut(device);
        object.references -= 1;
        let count = object.references;
        if count == 0 {
            device::destroy_device(gx, object);
            drop(Box::from_raw(device as *mut Device));
        }
        count as c_ulong
    })
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_begin_scene() -> GxResult {
    with(device::begin_scene)
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_render_target(device: *mut c_void, target: *mut c_void) -> GxResult {
    if target.is_null() {
        return GXERR_INVALIDPARAMS;
    }
    with(|gx| device::set_render_target(gx, device_mut(device), id(target)))
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_viewport(device: *mut c_void, viewport: *const GxViewport) -> GxResult {
    let _guard = lock();
    let object = device_mut(device);
    object.viewport = *viewport;
    object.uniform_dirty = true;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_get_viewport(device: *mut c_void, out: *mut GxViewport) -> GxResult {
    let _guard = lock();
    *out = device_mut(device).viewport;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_clear(
    device: *mut c_void,
    count: c_ulong,
    rects: *const GxRect,
    flags: c_ulong,
    color: c_ulong,
    z: f32,
) -> GxResult {
    let rects = if rects.is_null() { None } else { Some(std::slice::from_raw_parts(rects, count as usize)) };
    with(|gx| device::clear(gx, device_mut(device), rects, flags as u32, color as u32, z))
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_transform(device: *mut c_void, state: u32, matrix: *const GxMatrix) -> GxResult {
    let _guard = lock();
    device_mut(device).set_transform(state, &*matrix)
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_get_transform(device: *mut c_void, state: u32, out: *mut GxMatrix) -> GxResult {
    let _guard = lock();
    match device_mut(device).get_transform(state) {
        Ok(matrix) => {
            *out = matrix;
            GX_OK
        }
        Err(error) => error,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_render_state(device: *mut c_void, state: u32, value: c_ulong) -> GxResult {
    let _guard = lock();
    device_mut(device).set_render_state(state, u64::from(value))
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_get_render_state(device: *mut c_void, state: u32, out: *mut c_ulong) -> GxResult {
    let _guard = lock();
    match device_mut(device).get_render_state(state) {
        Ok(value) => {
            *out = value as c_ulong;
            GX_OK
        }
        Err(error) => error,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_stage_state(
    device: *mut c_void,
    stage: c_ulong,
    state: u32,
    value: c_ulong,
) -> GxResult {
    let _guard = lock();
    device_mut(device).set_stage_state(stage as u32, state, u64::from(value))
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_texture(device: *mut c_void, stage: c_ulong, texture: *mut c_void) -> GxResult {
    with(|gx| {
        let texture = if texture.is_null() { None } else { Some(id(texture)) };
        device::set_texture(gx, device_mut(device), stage as u32, texture)
    })
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_material(device: *mut c_void, material: *const GxMaterial) -> GxResult {
    let _guard = lock();
    let object = device_mut(device);
    object.material = *material;
    object.uniform_dirty = true;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_set_light(device: *mut c_void, index: c_ulong, light: *const GxLight) -> GxResult {
    let _guard = lock();
    if index >= 8 {
        return GXERR_INVALIDPARAMS;
    }
    let object = device_mut(device);
    object.lights[index as usize] = *light;
    object.uniform_dirty = true;
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_get_light(device: *mut c_void, index: c_ulong, out: *mut GxLight) -> GxResult {
    let _guard = lock();
    if index >= 8 {
        return GXERR_INVALIDPARAMS;
    }
    *out = device_mut(device).lights[index as usize];
    GX_OK
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_light_enable(device: *mut c_void, index: c_ulong, enable: i32) -> GxResult {
    let _guard = lock();
    if index >= 8 {
        return GXERR_INVALIDPARAMS;
    }
    let object = device_mut(device);
    object.light_on[index as usize] = enable != 0;
    object.uniform_dirty = true;
    GX_OK
}

unsafe fn vertex_buffer<'a>(buffer: *mut c_void) -> Option<&'a VertexBuffer> {
    (buffer as *const VertexBuffer).as_ref()
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_draw_primitive_vb(
    device: *mut c_void,
    primitive: u32,
    buffer: *mut c_void,
    start: c_ulong,
    count: c_ulong,
) -> GxResult {
    with(|gx| device::draw(gx, device_mut(device), primitive, vertex_buffer(buffer), start as u32, count as u32, None))
}

#[no_mangle]
pub unsafe extern "C" fn gx_device_draw_indexed_primitive_vb(
    device: *mut c_void,
    primitive: u32,
    buffer: *mut c_void,
    start: c_ulong,
    count: c_ulong,
    indices: *const u16,
    index_count: c_ulong,
) -> GxResult {
    let indices = (!indices.is_null()).then(|| std::slice::from_raw_parts(indices, index_count as usize));
    with(|gx| {
        device::draw(gx, device_mut(device), primitive, vertex_buffer(buffer), start as u32, count as u32, indices)
    })
}

// ---- free functions

#[no_mangle]
pub extern "C" fn gx_set_render_percent(percent: c_ulong) {
    with(|gx| gx.set_render_percent(percent as u32));
}

#[no_mangle]
pub extern "C" fn gx_set_frame_limit(frames_per_second: c_long) {
    with(|gx| gx.set_frame_limit(frames_per_second as i64));
}

#[no_mangle]
pub extern "C" fn gx_set_texture_filtering(mode: c_ulong) {
    with(|gx| gx.set_texture_filtering(mode as u32));
}

#[no_mangle]
pub extern "C" fn gx_set_brightness(gamma: f32) {
    with(|gx| gx.set_brightness(gamma));
}

#[no_mangle]
pub unsafe extern "C" fn gx_game_size(
    percent: c_ulong,
    windowed: bool,
    out_width: *mut c_ulong,
    out_height: *mut c_ulong,
) {
    let info = with(|gx| gx.plat.display_info());
    let (width, height) = modes::game_size(&info, percent as u32, windowed);
    *out_width = width as c_ulong;
    *out_height = height as c_ulong;
}

#[no_mangle]
pub extern "C" fn gx_auto_ui_scale() -> c_ulong {
    let info = with(|gx| gx.plat.display_info());
    modes::auto_ui_scale(&info) as c_ulong
}

/// TEST ONLY (tools/harness/presentation_bridge.c finds this by map address): a callback before every presented frame.
#[no_mangle]
pub extern "C" fn GXTestSetPresentHook(hook: Option<PresentHook>) {
    with(|gx| gx.set_test_present_hook(hook));
}

/// TEST ONLY: a read-back of a frame as 32-bit BGRA rows.
#[no_mangle]
pub unsafe extern "C" fn GXTestReadFrame(
    frame: *mut c_void,
    out_pixels: *mut c_void,
    capacity: c_ulong,
    out_width: *mut c_ulong,
    out_height: *mut c_ulong,
) -> c_long {
    let guard = lock();
    let mut gx = guard.0.borrow_mut();
    let identifier = id(frame);
    let (width, height) = (gx.s(identifier).width, gx.s(identifier).height);
    *out_width = width as c_ulong;
    *out_height = height as c_ulong;
    let out = if out_pixels.is_null() {
        None
    } else {
        Some(std::slice::from_raw_parts_mut(out_pixels as *mut u8, width as usize * height as usize * 4))
    };
    let (result, _, _) = gx.test_read_frame(identifier, out, capacity as u32);
    result as c_long
}

/// Compile-time check that this file names every key input the shim needs.
const _: fn() = || {
    let _ = keys::BlitMode::Copy;
};
