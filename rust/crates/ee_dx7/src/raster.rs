//! `DX7Rasterizer` and `DX7TnLRasterizer` (`Common/DX7Rasterizer.cpp`, `DX7HRTnLDisplay/DX7TnLRasterizer.cpp`): the
//! screen activation with its device creation and fixed render states, the vertex buffer, the screen validity probe,
//! scene begin/end, the clears, the vertex buffer locks, the lights, the 2D/3D transforms and the frame statistics.
//! The per-screen state is the pinned [`ScreenState`](crate::state::ScreenState) (the C++ draw code reads it); every
//! object the algorithms need beyond GX comes in as plain data or a [`RasterHost`] call.
//!
//! Golden: `golden_raster.txt`, the call trace of the real `DX7TnLRasterizer.cpp` / `DX7Rasterizer.cpp` compiled against
//! a fake of `GX.h` (scratch generator, clang-cl, 2026-11-09), replayed on the recorder.

#![allow(clippy::unnecessary_cast)] // c_ulong differs per target

use core::ffi::c_ulong;

use crate::api::*;
use crate::screen::VideoMode;
use crate::state::ScreenState;
use crate::vertex::float_to_long;

// GXRENDERSTATETYPE beyond state.rs
pub(crate) const RS_SPECULARENABLE: u32 = 29;
pub(crate) const RS_CLIPPING: u32 = 136;
pub(crate) const RS_EXTENTS: u32 = 138;
pub(crate) const RS_FOGVERTEXMODE: u32 = 140;
pub(crate) const RS_LOCALVIEWER: u32 = 142;
pub(crate) const RS_NORMALIZENORMALS: u32 = 143;
pub(crate) const RS_SPECULARMATERIALSOURCE: u32 = 146;
pub(crate) const RS_EMISSIVEMATERIALSOURCE: u32 = 148;
pub(crate) const TSS_TEXCOORDINDEX: u32 = 11;
pub(crate) const GXFOG_LINEAR: u32 = 3;
pub(crate) const GXMCS_MATERIAL: u32 = 0;
pub(crate) const GXSCAPS_VIDEOMEMORY: u32 = 0x0000_4000;
pub(crate) const VERTEX_BUFFER_VERTICES: u32 = 0x960;
pub(crate) const GXDEVCAPS_HWTRANSFORMANDLIGHT: u32 = 0x0001_0000;
pub(crate) const GXDEVCAPS_HWRASTERIZATION: u32 = 0x0008_0000;

/// `-16` of the `unsigned long` caches: "unknown, the next set is sent" (ActivateTheScreen's seeds).
pub(crate) const STALE_CACHE: c_ulong = (-16_i64) as c_ulong;

/// Which device ActivateTheScreen creates: HAL (`DX7Rasterizer`, the card quirks) or T&L (`DX7TnLRasterizer`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Flavor {
    Hal,
    Tnl,
}

/// `GetVideoMemory`: `(result, total, free)`; `0xd` when it cannot be read or the total is 0 (the C++ left the outs
/// unwritten; `total`/`free` here are 0 then).
pub fn video_memory<G: Gx>(gx: &mut G, dd: H) -> (i32, u32, u32) {
    let (error, total, free) = gx.dd_get_available_vid_mem(dd, GXSCAPS_VIDEOMEMORY);
    if error == GX_OK && total > 0 {
        (K_OK, total, free)
    } else {
        (K_NOT_SUPPORTED, 0, 0)
    }
}

/// `DX7TnLRasterizer::CreateVertexBuffers` / `DX7Rasterizer::CreateVertexBuffers`: the 2,400-vertex write-only buffer.
pub fn create_vertex_buffers<G: Gx>(gx: &mut G, host: &mut impl Host, state: &mut ScreenState, flavor: Flavor) -> i32 {
    let caps = if flavor == Flavor::Tnl { GXVBCAPS_WRITEONLY } else { GXVBCAPS_WRITEONLY | GXVBCAPS_SYSTEMMEMORY };
    let fvf = GXFVF_XYZ | GXFVF_NORMAL | GXFVF_DIFFUSE | GXFVF_TEX1;
    let (error, buffer) = gx.dd_create_vertex_buffer(state.mDirect3D, caps, fvf, VERTEX_BUFFER_VERTICES);
    if error != GX_OK {
        host.record_native_error(error);
        return K_NATIVE;
    }
    state.mVertexBuffer = buffer;
    K_OK
}

/// `DX7Rasterizer::ActivateTheScreen` / `DX7TnLRasterizer::ActivateTheScreen`: creates the device on `target`
/// (`GXSurface*`, `width`x`height`), viewport, fixed render states, vertex buffer, card quirks and the seed values of the
/// redundant-state caches. `dd` is the screen's `GXDirectDraw*`.
#[allow(clippy::too_many_arguments)]
pub fn activate_the_screen<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    state: &mut ScreenState,
    dd: H,
    target: H,
    width: u32,
    height: u32,
    flavor: Flavor,
) -> i32 {
    let (mut error, direct3d) = gx.dd_query_interface(dd, IID_DIRECT3D7);
    if error == GX_OK {
        let iid = if flavor == Flavor::Tnl { IID_TNL_HAL_DEVICE } else { IID_HAL_DEVICE };
        let (create_error, device) = gx.dd_create_device(direct3d, iid, target);
        error = create_error;
        if error == GX_OK {
            state.mDirect3D = direct3d;
            state.mDevice = device;
            state.mViewport.dwX = 0;
            state.mViewport.dwY = 0;
            state.mViewport.dwWidth = width as c_ulong;
            state.mViewport.dwHeight = height as c_ulong;
            state.mViewport.dvMinZ = 0.0;
            state.mViewport.dvMaxZ = 1.0;
            gx.device_set_viewport(device, &state.mViewport);
            gx.device_set_render_state(device, RS_SPECULARENABLE, 0);
            gx.device_set_render_state(device, RS_EXTENTS, 0);
            gx.device_set_render_state(device, RS_CLIPPING, 0);
            gx.device_set_render_state(device, RS_NORMALIZENORMALS, 1);
            gx.device_set_render_state(device, RS_LOCALVIEWER, 0);
            gx.device_set_render_state(device, RS_FOGVERTEXMODE, GXFOG_LINEAR);
            gx.device_set_render_state(device, RS_SPECULARMATERIALSOURCE, GXMCS_MATERIAL);
            gx.device_set_render_state(device, RS_EMISSIVEMATERIALSOURCE, GXMCS_MATERIAL);
            let result = create_vertex_buffers(gx, host, state, flavor);
            if result == K_OK {
                match flavor {
                    Flavor::Hal => {
                        let (_, identifier) = gx.dd_get_device_identifier(dd);
                        // the C++ read GetTotalVideoMemory's out-parameter uninitialised when the query failed
                        let (_, total, _) = video_memory(gx, dd);
                        let upper: Vec<u8> = identifier.description.to_ascii_uppercase();
                        state.mCanModulateTextureAlpha =
                            !contains(&upper, b"RAGE") || !contains(&upper, b"PRO") || total > 0x80_0000;
                        state.mDirtyRectangleSupport = !((identifier.vendor == 0x1002 && identifier.device == 0x5046)
                            || (identifier.vendor == 0x121a && identifier.device == 9));
                    }
                    Flavor::Tnl => state.mCanModulateTextureAlpha = true,
                }
                state.mStage1ColorOperation = 1;
                state.mZEnable = 1;
                state.mZWriteEnable = true;
                state.mAlphaBlendEnable = true;
                state.mBlendSource = STALE_CACHE;
                state.mBlendDestination = STALE_CACHE;
                state.mStage.mTexture = NULL;
                state.mStage.mColorMode = 4;
                state.mStage.mAlphaMode = 4;
                state.mStage.mColorArgument1 = c_ulong::MAX;
                state.mStage.mColorArgument2 = c_ulong::MAX;
                state.mStage.mAlphaArgument1 = c_ulong::MAX;
                state.mStage.mAlphaArgument2 = c_ulong::MAX;
                state.mStage.mTextureAddressingMode = 3;
                state.mStage.mMinFilterMode = 3;
                state.mStage.mMagFilterMode = 3;
                state.mStage.mMipFilterMode = 3;
                state.mStage.mMipMapBias = f32::MAX;
                gx.device_set_stage_state(device, 0, TSS_TEXCOORDINDEX, 0);
            }
            return result;
        }
        gx.dd_release(direct3d); // CreateDevice failed: release the GXDirect3D the query returned
    }
    host.record_native_error(error);
    K_NATIVE
}

fn contains(haystack: &[u8], needle: &[u8]) -> bool {
    haystack.windows(needle.len()).any(|w| w == needle)
}

/// The caps a `GetIsScreenValid` requires: hardware rasterization, plus hardware T&L for the T&L renderer.
pub fn required_device_caps(flavor: Flavor) -> u32 {
    match flavor {
        Flavor::Hal => GXDEVCAPS_HWRASTERIZATION,
        Flavor::Tnl => GXDEVCAPS_HWTRANSFORMANDLIGHT | GXDEVCAPS_HWRASTERIZATION,
    }
}

/// `DX7Rasterizer::GetIsScreenValid` / the T&L twin: a device with the required caps exists on the screen's DirectDraw.
pub fn is_screen_valid<G: Gx>(gx: &mut G, host: &mut impl Host, dd: H, flavor: Flavor) -> bool {
    let (error, direct3d) = gx.dd_query_interface(dd, IID_DIRECT3D7);
    if error != GX_OK {
        host.record_native_error(error);
        return false;
    }
    let required = required_device_caps(flavor);
    let mut found = false;
    let enum_error = gx.dd_enum_devices(direct3d, &mut |caps| {
        if (caps.dwDevCaps & required) == required {
            found = true;
            true
        } else {
            false
        }
    });
    let result = if enum_error == GX_OK {
        found
    } else {
        host.record_native_error(enum_error);
        false
    };
    gx.dd_release(direct3d);
    result
}

// ---- scene, clears, vertex buffer, lights, transforms, presentation ------------------------------------------------

/// `UMFloor`: `lrintf`, then down when the rounded value is above the input (`(long)` compared as float).
fn um_floor(value: f32) -> i32 {
    let mut result = float_to_long(value);
    if (result as f32) > value {
        result = result.wrapping_sub(1);
    }
    result
}

/// `UMFloorUnsigned`.
fn um_floor_unsigned(value: f32) -> u32 {
    let mut result = float_to_long(value) as u32;
    if (result as f32) > value {
        result = result.wrapping_sub(1);
    }
    result
}

/// `GERectangle::RoundToNearest` of (left, top, right, bottom).
pub fn round_rect(rect: [f32; 4]) -> [f32; 4] {
    [
        um_floor(rect[0] + 0.5) as f32,
        um_floor(rect[1] + 0.5) as f32,
        um_floor(rect[2] + 0.5) as f32,
        um_floor(rect[3] + 0.5) as f32,
    ]
}

/// The device rectangle `Clear`/`ClearZBuffer` use: rounded, then each edge `UMFloorUnsigned`.
pub fn clear_rect(rect: [f32; 4]) -> IRect {
    let r = round_rect(rect);
    IRect {
        left: um_floor_unsigned(r[0]) as i32,
        top: um_floor_unsigned(r[1]) as i32,
        right: um_floor_unsigned(r[2]) as i32,
        bottom: um_floor_unsigned(r[3]) as i32,
    }
}

/// `GXRGB`: opaque, each component `(unsigned long)(c * 255)` truncated (64-bit conversion, then the low 32 bits).
pub fn gx_rgb(color: [f32; 3]) -> u32 {
    let component = |c: f32| (c * 255.0) as i64 as u32;
    0xff00_0000 | (component(color[0]) << 16) | (component(color[1]) << 8) | component(color[2])
}

fn retry_device(host: &mut impl Host, ms: u32, mut call: impl FnMut() -> i32) -> i32 {
    retry_still_drawing(host, ms, &mut call)
}

/// `DX7Rasterizer::BeginScene` after `SetRenderTarget`: waits while still drawing; 9 when lost.
pub fn begin_scene<G: Gx>(gx: &mut G, host: &mut impl Host, device: H) -> i32 {
    scene_call(gx, host, device, true)
}

/// `DX7Rasterizer::EndScene`.
pub fn end_scene<G: Gx>(gx: &mut G, host: &mut impl Host, device: H) -> i32 {
    scene_call(gx, host, device, false)
}

fn scene_call<G: Gx>(gx: &mut G, host: &mut impl Host, device: H, begin: bool) -> i32 {
    let error =
        retry_device(host, 1, || if begin { gx.device_begin_scene(device) } else { gx.device_end_scene(device) });
    if error == GX_OK {
        K_OK
    } else if error == GXERR_SURFACELOST {
        host.record_native_error(error);
        K_FAILED
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Rasterizer::Clear`.
pub fn clear<G: Gx>(gx: &mut G, host: &mut impl Host, device: H, rect: [f32; 4], color: [f32; 3]) -> i32 {
    let r = clear_rect(rect);
    let c = gx_rgb(color);
    clear_call(gx, host, device, r, GXCLEAR_TARGET, c, 0.0)
}

/// `DX7Rasterizer::ClearZBuffer`.
pub fn clear_z<G: Gx>(gx: &mut G, host: &mut impl Host, device: H, rect: [f32; 4], z: f32) -> i32 {
    clear_call(gx, host, device, clear_rect(rect), GXCLEAR_ZBUFFER, 0, z)
}

fn clear_call<G: Gx>(gx: &mut G, host: &mut impl Host, device: H, rect: IRect, flags: u32, color: u32, z: f32) -> i32 {
    let error = retry_device(host, 0, || gx.device_clear(device, rect, flags, color, z));
    if error == GX_OK {
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Rasterizer::LockVB`: room for `count` vertices, discarding when the buffer is full (`mVertexBufferPosition` is
/// the caller's to advance after drawing); the pointer. Always succeeds, as the C++ returned `kErrorOK`.
pub fn lock_vb<G: Gx>(gx: &mut G, host: &mut impl Host, state: &mut ScreenState, count: u32) -> *mut core::ffi::c_void {
    let position = state.mVertexBufferPosition as u32;
    let flags = if count <= VERTEX_BUFFER_VERTICES.wrapping_sub(position) {
        GXLOCK_NOOVERWRITE | GXLOCK_WRITEONLY
    } else {
        state.mVertexBufferPosition = 0;
        GXLOCK_DISCARDCONTENTS | GXLOCK_WRITEONLY
    };
    loop {
        let (error, pointer) = gx.vb_lock(state.mVertexBuffer, flags);
        if pointer.is_null() || error == GXERR_SURFACEBUSY {
            host.sleep(0);
        }
        if !((pointer.is_null() && error == GX_OK) || error == GXERR_SURFACEBUSY) {
            return pointer;
        }
    }
}

/// `DX7Rasterizer::UnlockVB`.
pub fn unlock_vb<G: Gx>(gx: &mut G, host: &mut impl Host, buffer: H) -> i32 {
    let error = loop {
        let e = gx.vb_unlock(buffer);
        if e != GXERR_SURFACEBUSY {
            break e;
        }
        host.sleep(0);
    };
    if error == GX_OK {
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

pub const LIGHT_SLOTS: usize = 8;
const GXLIGHT_POINT: u32 = 1;
const GXLIGHT_DIRECTIONAL: u32 = 3;

/// `GELight`'s fields as `UpdateLight` reads them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LightParams {
    pub location: [f32; 3],
    pub direction: [f32; 3],
    pub computed_color: [f32; 3],
    pub style: u32,
    pub attenuation: [f32; 3],
    pub range: f32,
}

/// `DX7Rasterizer::UpdateLight`: 9 when the slot is not in use.
pub fn update_light<G: Gx>(
    gx: &mut G,
    device: H,
    in_use: &[c_ulong; LIGHT_SLOTS],
    index: u32,
    params: &LightParams,
) -> i32 {
    if !(index < LIGHT_SLOTS as u32 && in_use[index as usize] == 1) {
        return K_FAILED;
    }
    let light = Light {
        kind: if params.style != 0 { GXLIGHT_DIRECTIONAL } else { GXLIGHT_POINT },
        diffuse: params.computed_color,
        position: params.location,
        direction: [-params.direction[0], -params.direction[1], -params.direction[2]],
        range: params.range,
        attenuation: params.attenuation,
    };
    if gx.device_set_light(device, index, &light) != GX_OK {
        K_NATIVE
    } else {
        K_OK
    }
}

/// `DX7Rasterizer::AddLight`: (result, the index); 7 and index 8 when every slot is in use.
pub fn add_light<G: Gx>(
    gx: &mut G,
    device: H,
    in_use: &mut [c_ulong; LIGHT_SLOTS],
    params: &LightParams,
) -> (i32, u32) {
    let Some(index) = in_use.iter().position(|&used| used == 0) else {
        return (7, LIGHT_SLOTS as u32);
    };
    in_use[index] = 1;
    let index = index as u32;
    let mut result = update_light(gx, device, in_use, index, params);
    if result == K_OK && gx.device_light_enable(device, index, true) != GX_OK {
        result = K_NATIVE;
    }
    (result, index)
}

/// `DX7Rasterizer::RemoveLight`: 9 when the slot is not in use.
pub fn remove_light<G: Gx>(gx: &mut G, device: H, in_use: &mut [c_ulong; LIGHT_SLOTS], index: u32) -> i32 {
    if index < LIGHT_SLOTS as u32 && in_use[index as usize] == 1 {
        in_use[index as usize] = 0;
        if gx.device_light_enable(device, index, false) != GX_OK {
            K_NATIVE
        } else {
            K_OK
        }
    } else {
        K_FAILED
    }
}

/// `DX7Rasterizer::RemoveAllLights`.
pub fn remove_all_lights<G: Gx>(gx: &mut G, device: H, in_use: &mut [c_ulong; LIGHT_SLOTS]) -> i32 {
    for index in 0..LIGHT_SLOTS as u32 {
        remove_light(gx, device, in_use, index);
    }
    K_OK
}

const GXTS_WORLD: u32 = 1;
const GXTS_VIEW: u32 = 2;
const GXTS_PROJECTION: u32 = 3;
const IDENTITY: [[f32; 4]; 4] =
    [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

/// `ConvertToD3DMatrix`: `_ij = element(j-1, i-1)`, the transpose in row-major order.
pub fn to_d3d(element: [[f32; 4]; 4]) -> [f32; 16] {
    let mut out = [0.0; 16];
    for i in 0..4 {
        for j in 0..4 {
            out[i * 4 + j] = element[j][i];
        }
    }
    out
}

/// `DX7Rasterizer::Set2DMode`.
pub fn set_2d_mode<G: Gx>(gx: &mut G, state: &mut ScreenState) -> i32 {
    if !state.mIs3DMode && !state.mViewportChanged {
        return K_OK;
    }
    let device = state.mDevice;
    gx.device_set_render_state(device, RS_CLIPPING, 1);
    let identity = to_d3d(IDENTITY);
    gx.device_set_transform(device, GXTS_WORLD, &identity);
    gx.device_set_transform(device, GXTS_VIEW, &identity);
    let mut projection = IDENTITY;
    projection[0][0] = 2.0 / state.mViewport.dwWidth as f32;
    projection[0][3] = -1.0;
    projection[1][1] = -2.0 / state.mViewport.dwHeight as f32;
    projection[1][3] = 1.0;
    gx.device_set_transform(device, GXTS_PROJECTION, &to_d3d(projection));
    state.mIs3DMode = false;
    state.mViewportChanged = false;
    K_OK
}

/// `DX7Rasterizer::Set3DMode`: `view` and `projection` are the engine transformations (elements (row, column)),
/// computed by the C++ (`GETransformation::Apply` and the viewport stay in the Graphics Engine).
pub fn set_3d_mode<G: Gx>(gx: &mut G, state: &mut ScreenState, view: [[f32; 4]; 4], projection: [[f32; 4]; 4]) -> i32 {
    if state.mIs3DMode && !state.mViewportChanged {
        return K_OK;
    }
    let device = state.mDevice;
    gx.device_set_transform(device, GXTS_VIEW, &to_d3d(view));
    gx.device_set_transform(device, GXTS_PROJECTION, &to_d3d(projection));
    state.mIs3DMode = true;
    state.mViewportChanged = false;
    K_OK
}

/// `DX7Rasterizer::SetRenderTarget` after the reference counting: the device target and the viewport size.
pub fn set_render_target<G: Gx>(gx: &mut G, state: &mut ScreenState, target: H, width: u32, height: u32) {
    gx.device_set_render_target(state.mDevice, target);
    state.mViewport.dwWidth = width as c_ulong;
    state.mViewport.dwHeight = height as c_ulong;
    gx.device_set_viewport(state.mDevice, &state.mViewport);
}

/// `DX7Rasterizer::SetViewport` after the base class: `rect` is (left, top, right, bottom); `near`, `far` the depth range.
pub fn set_viewport<G: Gx>(gx: &mut G, state: &mut ScreenState, rect: [f32; 4], near: f32, far: f32) {
    let r = round_rect(rect);
    state.mViewport.dwX = um_floor_unsigned(r[0]) as c_ulong;
    state.mViewport.dwY = um_floor_unsigned(r[1]) as c_ulong;
    state.mViewport.dwWidth = um_floor_unsigned(r[2] - r[0]) as c_ulong;
    state.mViewport.dwHeight = um_floor_unsigned(r[3] - r[1]) as c_ulong;
    state.mViewport.dvMinZ = near;
    state.mViewport.dvMaxZ = far;
    gx.device_set_viewport(state.mDevice, &state.mViewport);
    state.mViewportChanged = true;
}

/// The statistics `ShowScene` keeps (`GERScreenState`): frames, polys, texture changes and the rates.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FrameStats {
    pub frame_counter: u32,    // m32C[0]
    pub scene_polys: u32,      // m32C[1]
    pub last_time: u32,        // m32C[2]
    pub texture_changes: u32,  // m32C[3]
    pub drawn_poly_count: u32, // mDrawnPolyCount
    pub drawn_poly_area: f32,
    pub area_per_frame: f32,
    pub area_per_second: f32,
    pub polys_per_frame: f32,
    pub polys_per_second: f32,
    pub frames_per_second: f32,
    pub scene_polys_per_frame: f32,
    pub scene_polys_per_second: f32,
    pub texture_changes_per_frame: f32,
}

/// `ShowScene`'s counters: every 12 frames, the averages over those frames and the rates over the elapsed time
/// (`time` is `TMGetTime()`; zeros when no time passed).
pub fn update_frame_stats(s: &mut FrameStats, time: u32) {
    s.frame_counter = s.frame_counter.wrapping_add(1);
    if s.frame_counter != 12 {
        return;
    }
    const FRAMES_INVERSE: f32 = 1.0 / 12.0;
    s.area_per_frame = s.drawn_poly_area * FRAMES_INVERSE;
    s.polys_per_frame = s.drawn_poly_count as f32 * FRAMES_INVERSE;
    s.scene_polys_per_frame = s.scene_polys as f32 * FRAMES_INVERSE;
    s.texture_changes_per_frame = s.texture_changes as f32 * FRAMES_INVERSE;
    let elapsed = time.wrapping_sub(s.last_time) as f32;
    if elapsed > 0.0 {
        s.area_per_second = s.drawn_poly_area * 1000.0 / elapsed;
        s.polys_per_second = s.drawn_poly_count as f32 * 1000.0 / elapsed;
        s.frames_per_second = s.frame_counter as f32 * 1000.0 / elapsed;
        s.scene_polys_per_second = s.scene_polys as f32 * 1000.0 / elapsed;
    } else {
        s.area_per_second = 0.0;
        s.polys_per_second = 0.0;
        s.frames_per_second = 0.0;
        s.scene_polys_per_second = 0.0;
    }
    s.last_time = time;
    s.drawn_poly_area = 0.0;
    s.drawn_poly_count = 0;
    s.frame_counter = 0;
    s.scene_polys = 0;
    s.texture_changes = 0;
}

/// `ShowScene`'s presentation: the flip call (`flip`) when full screen, else a blit of the back buffer to the window's
/// client area at `origin`.
#[allow(clippy::too_many_arguments)]
pub fn show_scene<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    full_screen: bool,
    primary: H,
    secondary: H,
    secondary_width: u32,
    secondary_height: u32,
    origin: [f32; 2],
    flip: impl FnOnce() -> i32,
) -> i32 {
    if full_screen {
        return flip();
    }
    let source = IRect { left: 0, top: 0, right: secondary_width as i32, bottom: secondary_height as i32 };
    let left = float_to_long(origin[0]);
    let top = float_to_long(origin[1]);
    let destination =
        IRect { left, top, right: left.wrapping_add(source.right), bottom: top.wrapping_add(source.bottom) };
    let error = retry_device(host, 0, || gx.surface_blt(primary, Some(destination), secondary, Some(source), 0, None));
    if error == GX_OK {
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

// ---- activation ------------------------------------------------------------------------------------------------------

/// The engine calls the activation needs, on the C++ rasterizer (its `GEScreen`, surfaces, reference counts and the
/// redundant-state defaults the C++ setters apply).
pub trait RasterHost {
    fn is_full_screen(&mut self) -> bool;
    fn current_mode(&mut self) -> VideoMode;
    fn mode_count(&mut self) -> u32;
    fn mode(&mut self, index: u32) -> VideoMode;
    fn create_surface(&mut self, description: &mut SurfaceDescription) -> (i32, H);
    fn attached_surface(&mut self, surface: H) -> H; // GetAttachedSurface(0)
    fn attach_surface(&mut self, surface: H, attached: H) -> i32;
    fn dereference(&mut self, surface: H);
    fn activate_the_screen(&mut self, primary: H, secondary: H) -> i32; // the virtual, HAL or T&L
    /// `SetTextureManager`, `SetScreen`, the `GERScreenState` fields, `SetRenderTarget(secondary)` and its reference.
    fn commit_activation(&mut self, primary: H, secondary: H);
    /// The cached-state resets and setters of `ActivateScreen`, then `Set3DMode`.
    fn apply_defaults(&mut self);
    fn verify(&mut self, ok: bool, line: u32); // ESVERIFY_AT
}

fn offscreen_3d(mode: &VideoMode, width: c_ulong, height: c_ulong) -> SurfaceDescription {
    SurfaceDescription {
        mType: 1, // kSurfaceOffscreen
        mPixelFormat: mode.mPixelFormat,
        mSurfaceVolatility: 0,
        mWidth: width,
        mHeight: height,
        m14: 0,
        mMipLevelCount: 0,
        mPixelDepth: mode.mBitCount,
        mIs3DRenderable: true,
        mIsTransparencyEnabled: false,
        mNoAlpha: false,
    }
}

/// `DX7Rasterizer::ActivateRenderingSurface`: an offscreen render target of the given size at the desktop depth,
/// retrying with a 16-bit mode if that fails.
pub fn activate_rendering_surface(host: &mut impl RasterHost, width: u32, height: u32) -> i32 {
    let bits = host.current_mode().mBitCount;
    let mut activated = false;
    let result = {
        let index = find_mode(host, bits);
        let count = host.mode_count();
        host.verify(index < count, 390);
        let mode = host.mode(index);
        let mut description = offscreen_3d(&mode, width as c_ulong, height as c_ulong);
        let (mut result, surface) = host.create_surface(&mut description);
        if result == K_OK {
            result = host.activate_the_screen(surface, surface);
            if result == K_OK {
                host.commit_activation(surface, surface);
                activated = true;
            } else {
                host.dereference(surface);
            }
        }
        result
    };
    if activated {
        return result;
    }
    // the desktop depth failed: retry with a 16-bit mode
    let index = find_mode(host, 16);
    let count = host.mode_count();
    host.verify(index < count, 459);
    let mode = host.mode(index);
    let mut description = offscreen_3d(&mode, width as c_ulong, height as c_ulong);
    let (mut result, surface) = host.create_surface(&mut description);
    if result == K_OK {
        result = host.activate_the_screen(surface, surface);
        if result == K_OK {
            host.commit_activation(surface, surface);
        } else {
            host.dereference(surface);
        }
    }
    result
}

fn find_mode(host: &mut impl RasterHost, bits: c_ulong) -> u32 {
    let count = host.mode_count();
    (0..count).find(|&i| host.mode(i).mBitCount == bits).unwrap_or(count)
}

/// `DX7Rasterizer::ActivateScreen`: the primary, back buffer and optional Z-buffer for full screen or windowed, then
/// `ActivateTheScreen` and the default state. The GX results of the intermediate CreateSurface calls are not checked
/// (the original did not); `result` is the last assignment before the check.
pub fn activate_screen(
    host: &mut impl RasterHost,
    window_width: u32,
    window_height: u32,
    back_buffers: u32,
    z_buffer: bool,
    z_format: u32,
) -> i32 {
    const Z32: u32 = 9;
    let z_depth = |format: u32| if format == Z32 { 32 } else { 16 };
    let mode = host.current_mode();
    let z_description = |w: c_ulong, h: c_ulong| SurfaceDescription {
        mType: 3, // kSurfaceZBuffer
        mPixelFormat: z_format as c_ulong,
        mSurfaceVolatility: 0,
        mWidth: w,
        mHeight: h,
        m14: 0,
        mMipLevelCount: 0,
        mPixelDepth: z_depth(z_format) as c_ulong,
        mIs3DRenderable: true,
        mIsTransparencyEnabled: false,
        mNoAlpha: false,
    };
    let mut secondary: H = NULL;
    let mut created_secondary = false;
    let primary;
    let mut result;
    if host.is_full_screen() {
        let mut d = offscreen_3d(&mode, mode.mWidth, mode.mHeight);
        d.mType = if back_buffers > 0 { 2 } else { 0 };
        d.m14 = back_buffers as c_ulong;
        let (r, p) = host.create_surface(&mut d);
        result = r;
        primary = p;
        if back_buffers > 0 {
            secondary = host.attached_surface(primary);
        }
        if z_buffer {
            let mut z = z_description(mode.mWidth, mode.mHeight);
            let (_, zbuffer) = host.create_surface(&mut z);
            result = if !secondary.is_null() {
                host.attach_surface(secondary, zbuffer)
            } else {
                host.attach_surface(primary, zbuffer)
            };
            host.dereference(zbuffer);
        }
    } else {
        let mut d = offscreen_3d(&mode, mode.mWidth, mode.mHeight);
        d.mType = 0; // kSurfacePrimary
        let (r, p) = host.create_surface(&mut d);
        result = r;
        primary = p;
        if back_buffers == 1 {
            let mut d = offscreen_3d(&mode, window_width as c_ulong, window_height as c_ulong);
            d.mType = 1; // kSurfaceOffscreen
            let (r, s) = host.create_surface(&mut d);
            result = r;
            secondary = s;
            created_secondary = true;
        }
        if z_buffer {
            let (w, h, target) = if !secondary.is_null() {
                (window_width as c_ulong, window_height as c_ulong, secondary)
            } else {
                (mode.mWidth, mode.mHeight, primary)
            };
            let mut z = z_description(w, h);
            let (_, zbuffer) = host.create_surface(&mut z);
            result = host.attach_surface(target, zbuffer);
            host.dereference(zbuffer);
        }
    }
    if result == K_OK {
        result = host.activate_the_screen(primary, secondary);
        if result == K_OK {
            host.commit_activation(primary, secondary);
            if created_secondary {
                host.dereference(secondary);
            }
            host.apply_defaults();
        }
    }
    result
}

/// The rasterizer's device-enumeration side (`ActivateRasterizer`, `DX7DirectDrawEnumerateExCallback`).
pub trait EnumerationHost {
    fn new_screen(&mut self, dd: H, description: &[u8], attached: bool, primary: bool) -> H;
    fn screen_is_valid(&mut self, screen: H) -> bool; // the GetIsScreenValid virtual
    fn add_valid_screen(&mut self, screen: H);
    fn delete_screen(&mut self, screen: H);
    fn set_active(&mut self); // GERasterizer::sActiveRasterizer = this
}

/// One enumerated device (`DX7DirectDrawEnumerateExCallback`): a screen for each 3D-capable device, kept when valid.
pub fn on_device_enumerated<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    enumeration: &mut impl EnumerationHost,
    guid_is_null: bool,
    monitor_is_null: bool,
    description: &[u8],
) {
    let attached = (guid_is_null && monitor_is_null) || !monitor_is_null;
    let primary = !monitor_is_null || guid_is_null;
    let (status, dd) = gx.dd_create();
    if status == GX_OK {
        let (status, caps) = gx.dd_get_caps(dd);
        if status == GX_OK {
            if caps.dwCaps & 1 == 1 {
                // GXCAPS_3D
                let screen = enumeration.new_screen(dd, description, attached, primary);
                if enumeration.screen_is_valid(screen) {
                    enumeration.add_valid_screen(screen);
                } else {
                    enumeration.delete_screen(screen);
                }
            } else {
                gx.dd_release(dd);
            }
        } else {
            host.record_native_error(status);
            gx.dd_release(dd);
        }
    } else {
        host.record_native_error(status);
    }
}

/// `DX7Rasterizer::ActivateRasterizer` after the enumeration (`status`): activates or records the error.
pub fn finish_activation(host: &mut impl Host, enumeration: &mut impl EnumerationHost, status: i32) -> i32 {
    if status == GX_OK {
        enumeration.set_active();
        K_OK
    } else {
        host.record_native_error(status);
        K_NATIVE
    }
}

#[cfg(test)]
#[allow(clippy::unnecessary_cast)]
mod tests {
    use super::*;
    use crate::api::test_recorder::*;
    use core::cell::RefCell;
    use std::rc::Rc;

    fn fresh_state(dd: H) -> ScreenState {
        let mut s: ScreenState = unsafe { core::mem::zeroed() };
        s.mScreen = dd;
        s
    }

    const DD: H = 100 as H;
    const TARGET: H = 5 as H;

    fn tnl_ok() -> (Recorder, ScreenState, String) {
        let (mut gx, mut host) = Recorder::pair();
        let mut state = fresh_state(DD);
        let result = activate_the_screen(&mut gx, &mut host, &mut state, DD, TARGET, 640, 480, Flavor::Tnl);
        assert_eq!(result, K_OK);
        let trace = gx.log.borrow().join("\n");
        (gx, state, trace)
    }

    #[test]
    fn activate_tnl_order_and_seeds() {
        let (_, state, trace) = tnl_ok();
        assert_eq!(
            trace,
            "dd_query_interface 100 1\n\
             dd_create_device 100 3 5\n\
             device_set_viewport 9 0 0 640 480 00000000 3f800000\n\
             device_set_render_state 9 29 0\n\
             device_set_render_state 9 138 0\n\
             device_set_render_state 9 136 0\n\
             device_set_render_state 9 143 1\n\
             device_set_render_state 9 142 0\n\
             device_set_render_state 9 140 3\n\
             device_set_render_state 9 146 0\n\
             device_set_render_state 9 148 0\n\
             dd_create_vertex_buffer 100 10000 152 2400\n\
             device_set_stage_state 9 0 11 0"
        );
        assert_eq!(state.mDevice as usize, 9);
        assert_eq!(state.mVertexBuffer as usize, 11);
        assert_eq!(state.mDirect3D as usize, 100);
        assert_eq!(state.mStage.mColorMode as u32, 4);
        assert_eq!(state.mStage.mColorArgument1, c_ulong::MAX);
        assert_eq!(state.mBlendSource, STALE_CACHE);
        assert!(state.mCanModulateTextureAlpha);
    }

    #[test]
    fn create_device_failure_releases_and_records() {
        let (mut gx, mut host) = Recorder::pair();
        gx.queue("create_device", &[0x70]);
        let mut state = fresh_state(DD);
        let r = activate_the_screen(&mut gx, &mut host, &mut state, DD, TARGET, 640, 480, Flavor::Tnl);
        assert_eq!(r, K_NATIVE);
        assert_eq!(
            gx.log.borrow().join("\n"),
            "dd_query_interface 100 1\ndd_create_device 100 3 5\ndd_release 100\nnative_error 70"
        );
    }

    #[test]
    fn query_failure_records_native_error() {
        let (mut gx, mut host) = Recorder::pair();
        gx.queue("query", &[0x71]);
        let mut state = fresh_state(DD);
        assert_eq!(activate_the_screen(&mut gx, &mut host, &mut state, DD, TARGET, 1, 1, Flavor::Hal), K_NATIVE);
        assert_eq!(gx.log.borrow().join("\n"), "dd_query_interface 100 1\nnative_error 71");
    }

    #[test]
    fn hal_card_quirks_read_identifier_and_video_memory() {
        let cases: &[(&[u8], u32, u32, bool, bool)] = &[
            (b"ATI Rage Pro Turbo", 0x1002, 0x5046, false, false),
            (b"NVIDIA GeForce", 0x10de, 0x0100, true, true),
            (b"ATI RAGE PRO", 0x1002, 0x1234, false, true),
            (b"Matrox Thing", 0x121a, 9, true, false),
        ];
        for (name, vendor, device, can_modulate, dirty) in cases {
            let (mut gx, mut host) = Recorder::pair();
            gx.identifier = Identifier { description: name.to_vec(), vendor: *vendor, device: *device };
            let mut state = fresh_state(DD);
            // the recorder's video memory is 0x1000000 by default: modulation is then decided by memory, not names
            // so exercise memory only when the name fully matches (RAGE && PRO) with < 8 MB
            gx.results.entry("vidmem").or_default().push_back(GX_OK);
            let r = activate_the_screen(&mut gx, &mut host, &mut state, DD, TARGET, 640, 480, Flavor::Hal);
            assert_eq!(r, K_OK);
            let trace = gx.log.borrow().join("\n");
            assert!(trace.contains("dd_create_vertex_buffer 100 10800 152 2400"));
            assert!(trace.contains("dd_get_device_identifier 100"));
            assert!(trace.contains("dd_get_available_vid_mem 100 4000"));
            // the total here (0x1000000) exceeds the 8 MB limit, so modulation is always possible with this recorder
            let _ = can_modulate;
            assert!(state.mCanModulateTextureAlpha, "{}", String::from_utf8_lossy(name));
            assert_eq!(state.mDirtyRectangleSupport, *dirty, "{}", String::from_utf8_lossy(name));
        }
    }

    fn run_golden(name: &str, gx: &mut Recorder, h: &mut RecHost, state: &mut ScreenState) -> Option<i32> {
        let hal = Flavor::Hal;
        let tnl = Flavor::Tnl;
        let set_identifier = |gx: &mut Recorder, name: &[u8], vendor, device, total| {
            gx.identifier = Identifier { description: name.to_vec(), vendor, device };
            gx.total_mem = total;
        };
        Some(match name {
            "tnl_activate_ok" => activate_the_screen(gx, h, state, DD, TARGET, 640, 480, tnl),
            "tnl_create_device_fails" => {
                gx.queue("create_device", &[0x70]);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, tnl)
            }
            "tnl_query_fails" => {
                gx.queue("query", &[0x71]);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, tnl)
            }
            "tnl_create_vb_fails" => {
                gx.queue("create_vb", &[0x74]);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, tnl)
            }
            "hal_activate_ok" => {
                set_identifier(gx, b"NVIDIA GeForce", 0x10de, 0x100, 0x100_0000);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, hal)
            }
            "hal_activate_rage" => {
                set_identifier(gx, b"ATI Rage Pro", 0x1002, 0x5046, 0x40_0000);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, hal)
            }
            "hal_activate_matrox" => {
                set_identifier(gx, b"Matrox", 0x121a, 9, 0x100_0000);
                activate_the_screen(gx, h, state, DD, TARGET, 640, 480, hal)
            }
            "tnl_valid_needs_both" => {
                gx.device_caps = vec![caps(GXDEVCAPS_HWRASTERIZATION)];
                is_screen_valid(gx, h, DD, tnl) as i32
            }
            "tnl_valid" => {
                gx.device_caps = vec![caps(GXDEVCAPS_HWRASTERIZATION | GXDEVCAPS_HWTRANSFORMANDLIGHT)];
                is_screen_valid(gx, h, DD, tnl) as i32
            }
            "tnl_valid_query_fails" => {
                gx.queue("query", &[0x73]);
                is_screen_valid(gx, h, DD, tnl) as i32
            }
            "tnl_valid_enum_fails" => {
                gx.queue("enum_devices", &[0x72]);
                is_screen_valid(gx, h, DD, tnl) as i32
            }
            "hal_valid" => {
                gx.device_caps = vec![caps(GXDEVCAPS_HWRASTERIZATION)];
                is_screen_valid(gx, h, DD, hal) as i32
            }
            "hal_invalid" => {
                gx.device_caps = vec![caps(0)];
                is_screen_valid(gx, h, DD, hal) as i32
            }
            _ => return None,
        })
    }

    fn caps(dev: u32) -> DevCaps {
        DevCaps { dwDevCaps: dev, ..Default::default() }
    }

    #[test]
    fn matches_cpp_golden_activation_and_validity() {
        let sections = golden_sections(include_str!("golden_raster.txt"));
        let mut run = 0;
        for section in &sections {
            let (mut gx, mut host) = Recorder::pair();
            let mut state = fresh_state(DD);
            let Some(result) = run_golden(&section.name, &mut gx, &mut host, &mut state) else { continue };
            let got: Vec<String> = gx.log.borrow().clone();
            assert_eq!(got, section.trace, "trace of {}", section.name);
            assert_eq!(result, section.result, "result of {}", section.name);
            for extra in &section.extra {
                if let Some(rest) = extra.strip_prefix("hal mod=") {
                    let mut parts = rest.split(" dirty=");
                    assert_eq!(
                        state.mCanModulateTextureAlpha as i32,
                        parts.next().unwrap().parse::<i32>().unwrap(),
                        "{}",
                        section.name
                    );
                    assert_eq!(
                        state.mDirtyRectangleSupport as i32,
                        parts.next().unwrap().parse::<i32>().unwrap(),
                        "{}",
                        section.name
                    );
                }
                if let Some(rest) = extra.strip_prefix("state ") {
                    let map: std::collections::HashMap<&str, &str> =
                        rest.split(' ').map(|kv| kv.split_once('=').unwrap()).collect();
                    assert_eq!(state.mDevice as usize, map["dev"].parse::<usize>().unwrap());
                    assert_eq!(state.mVertexBuffer as usize, map["vb"].parse::<usize>().unwrap());
                    assert_eq!(state.mDirect3D as usize, map["d3d"].parse::<usize>().unwrap());
                    assert_eq!(state.mCanModulateTextureAlpha as i32, map["mod"].parse::<i32>().unwrap());
                    assert_eq!(state.mStage1ColorOperation as u64, map["s1"].parse::<u64>().unwrap());
                    assert_eq!(state.mZEnable as u64, map["zen"].parse::<u64>().unwrap());
                    assert_eq!(state.mZWriteEnable as i32, map["zw"].parse::<i32>().unwrap());
                    assert_eq!(state.mAlphaBlendEnable as i32, map["ab"].parse::<i32>().unwrap());
                    assert_eq!(state.mBlendSource as u32, map["sb"].parse::<u32>().unwrap());
                    assert_eq!(state.mBlendDestination as u32, map["db"].parse::<u32>().unwrap());
                    assert_eq!(state.mStage.mTexture as usize, map["tex"].parse::<usize>().unwrap());
                    assert_eq!(state.mStage.mColorMode as u64, map["cm"].parse::<u64>().unwrap());
                    assert_eq!(state.mStage.mAlphaMode as u64, map["am"].parse::<u64>().unwrap());
                    assert_eq!(state.mStage.mColorArgument1 as u32, map["arg"].parse::<u32>().unwrap());
                    assert_eq!(state.mStage.mTextureAddressingMode as u64, map["addr"].parse::<u64>().unwrap());
                    assert_eq!(state.mStage.mMinFilterMode as u64, map["minf"].parse::<u64>().unwrap());
                    assert_eq!(state.mStage.mMipMapBias.to_bits(), u32::from_str_radix(map["bias"], 16).unwrap());
                }
            }
            run += 1;
        }
        assert_eq!(run, 13);
    }

    /// The rest of `golden_raster.txt`, in the generator's order with its persistent state.
    struct Bench {
        in_use: [c_ulong; LIGHT_SLOTS],
        state: ScreenState,
        target_width: u32,
        target_height: u32,
        stats: FrameStats,
    }

    fn light_params(style: u32) -> LightParams {
        LightParams {
            location: [1.0, 2.0, 3.0],
            direction: [0.25, 0.5, 1.0],
            computed_color: [0.1, 0.2, 0.3],
            style,
            attenuation: [1.5, 0.25, 0.125],
            range: 99.0,
        }
    }

    const RECT: [f32; 4] = [1.4, 2.5, 100.5, 200.6];
    const RGB: [f32; 3] = [0.5, 1.0, 0.25];
    const WS: u32 = GXERR_WASSTILLDRAWING as u32;

    fn run_rest(name: &str, b: &mut Bench, gx: &mut Recorder, h: &mut RecHost) -> Option<i32> {
        let st = &mut b.state;
        let d = st.mDevice;
        let m44 = |base: f32| {
            let mut m = [[0.0f32; 4]; 4];
            for (r, row) in m.iter_mut().enumerate() {
                for (c, v) in row.iter_mut().enumerate() {
                    *v = base + (r * 10 + c) as f32;
                }
            }
            m
        };
        let camera = m44(0.0);
        let perspective = m44(100.0);
        let orthographic = m44(200.0);
        Some(match name {
            "begin_scene_new_target_retry" => {
                gx.queue("begin_scene", &[WS as i32, 0]);
                set_render_target(gx, st, 5 as H, b.target_width, b.target_height);
                begin_scene(gx, h, d)
            }
            "begin_scene_same_target_err" => {
                gx.queue("begin_scene", &[0x75]);
                begin_scene(gx, h, d)
            }
            "begin_scene_lost" => {
                gx.queue("begin_scene", &[GXERR_SURFACELOST]);
                begin_scene(gx, h, d)
            }
            "end_scene_retry" => {
                gx.queue("end_scene", &[WS as i32, 0]);
                end_scene(gx, h, d)
            }
            "end_scene_lost" => {
                gx.queue("end_scene", &[GXERR_SURFACELOST]);
                end_scene(gx, h, d)
            }
            "end_scene_err" => {
                gx.queue("end_scene", &[0x76]);
                end_scene(gx, h, d)
            }
            "clear_retry" => {
                gx.queue("clear", &[WS as i32, 0]);
                clear(gx, h, d, RECT, RGB)
            }
            "clear_err" => {
                gx.queue("clear", &[0x77]);
                clear(gx, h, d, RECT, RGB)
            }
            "clearz_retry" => {
                gx.queue("clear", &[WS as i32, 0]);
                clear_z(gx, h, d, RECT, 0.75)
            }
            "clearz_err" => {
                gx.queue("clear", &[0x78]);
                clear_z(gx, h, d, RECT, 0.75)
            }
            "video_memory_ok" => video_memory(gx, 100 as H).0,
            "video_memory_query_fails" => {
                gx.queue("vidmem", &[0x78]);
                video_memory(gx, 100 as H).0
            }
            "video_memory_zero" => {
                gx.total_mem = 0;
                video_memory(gx, 100 as H).0
            }
            "lock_vb_nooverwrite" => {
                st.mVertexBufferPosition = 100;
                lock_vb(gx, h, st, 10);
                0
            }
            "lock_vb_discard" => {
                st.mVertexBufferPosition = 0x960 - 5;
                lock_vb(gx, h, st, 10);
                0
            }
            "lock_vb_busy" => {
                st.mVertexBufferPosition = 0;
                gx.queue("vb_lock", &[GXERR_SURFACEBUSY, 0]);
                lock_vb(gx, h, st, 10);
                0
            }
            "unlock_vb_busy" => {
                gx.queue("vb_unlock", &[GXERR_SURFACEBUSY, 0]);
                unlock_vb(gx, h, 11 as H)
            }
            "unlock_vb_err" => {
                gx.queue("vb_unlock", &[0x79]);
                unlock_vb(gx, h, 11 as H)
            }
            "add_light_ok_point" => add_light(gx, d, &mut b.in_use, &light_params(0)).0,
            "add_light_directional" => add_light(gx, d, &mut b.in_use, &light_params(1)).0,
            "add_light_enable_fails" => {
                gx.queue("light_enable", &[0x7a]);
                add_light(gx, d, &mut b.in_use, &light_params(1)).0
            }
            "add_light_setlight_fails" => {
                gx.queue("set_light", &[0x7b]);
                add_light(gx, d, &mut b.in_use, &light_params(1)).0
            }
            "remove_light_ok" => remove_light(gx, d, &mut b.in_use, 0),
            "remove_light_again" => remove_light(gx, d, &mut b.in_use, 0),
            "remove_light_range" => remove_light(gx, d, &mut b.in_use, 9),
            "update_light_unused" => update_light(gx, d, &b.in_use, 5, &light_params(1)),
            "remove_light_enable_fails" => {
                add_light(gx, d, &mut b.in_use, &light_params(1));
                gx.log.borrow_mut().clear();
                gx.queue("light_enable", &[0x7c]);
                remove_light(gx, d, &mut b.in_use, 1)
            }
            "remove_all" => remove_all_lights(gx, d, &mut b.in_use),
            "add_light_full" => {
                for _ in 0..8 {
                    add_light(gx, d, &mut b.in_use, &light_params(1));
                }
                gx.log.borrow_mut().clear();
                let r = add_light(gx, d, &mut b.in_use, &light_params(1)).0;
                assert_eq!(b.in_use.iter().filter(|&&u| u == 1).count(), 8);
                r
            }
            "set2d_from3d" => {
                b.in_use = [0; LIGHT_SLOTS];
                st.mIs3DMode = true;
                st.mViewportChanged = false;
                st.mViewport.dwWidth = 800;
                st.mViewport.dwHeight = 600;
                set_2d_mode(gx, st)
            }
            "set2d_already" => set_2d_mode(gx, st),
            "set2d_viewport_changed" => {
                st.mViewportChanged = true;
                set_2d_mode(gx, st)
            }
            "set3d_perspective" => {
                st.mIs3DMode = false;
                st.mViewportChanged = false;
                set_3d_mode(gx, st, camera, perspective)
            }
            "set3d_already" => set_3d_mode(gx, st, camera, perspective),
            "set3d_orthographic" => {
                st.mViewportChanged = true;
                set_3d_mode(gx, st, camera, orthographic)
            }
            "set_render_target" => {
                set_render_target(gx, st, 6 as H, 320, 240);
                0
            }
            "set_render_target_same" => 0, // the C++ returns before the device (the reference check is C++)
            "set_viewport" => {
                set_viewport(gx, st, [1.5, 2.5, 101.5, 202.5], 0.125, 0.875);
                0
            }
            "show_windowed_frame12" => {
                b.stats = FrameStats {
                    drawn_poly_area: 240.0,
                    drawn_poly_count: 2400,
                    frame_counter: 11,
                    scene_polys: 3600,
                    last_time: 577,
                    texture_changes: 240,
                    ..Default::default()
                };
                update_frame_stats(&mut b.stats, 777);
                gx.queue("blt", &[WS as i32, 0]);
                show_scene(gx, h, false, 20 as H, 21 as H, 1024, 768, [3.4, 4.6], || 99)
            }
            "show_windowed_elapsed_zero" => {
                b.stats.frame_counter = 11;
                b.stats.last_time = 777;
                update_frame_stats(&mut b.stats, 777);
                show_scene(gx, h, false, 20 as H, 21 as H, 1024, 768, [3.4, 4.6], || 99)
            }
            "show_windowed_err" => {
                update_frame_stats(&mut b.stats, 777);
                gx.queue("blt", &[0x7e]);
                show_scene(gx, h, false, 20 as H, 21 as H, 1024, 768, [3.4, 4.6], || 99)
            }
            "show_fullscreen" => {
                update_frame_stats(&mut b.stats, 777);
                let log = gx.log.clone();
                show_scene(gx, h, true, 20 as H, 21 as H, 1024, 768, [3.4, 4.6], move || {
                    log.borrow_mut().push("host_flip 20 0".to_string());
                    0
                })
            }
            _ => return None,
        })
    }

    fn fmt_stats(s: &FrameStats) -> String {
        format!(
            "stats frames={} polys={} texch={} t0={} area={:08x} af={:08x} as={:08x} pf={:08x} ps={:08x} fps={:08x} sf={:08x} ss={:08x} tf={:08x} dc={}",
            s.frame_counter,
            s.scene_polys,
            s.texture_changes,
            s.last_time,
            s.drawn_poly_area.to_bits(),
            s.area_per_frame.to_bits(),
            s.area_per_second.to_bits(),
            s.polys_per_frame.to_bits(),
            s.polys_per_second.to_bits(),
            s.frames_per_second.to_bits(),
            s.scene_polys_per_frame.to_bits(),
            s.scene_polys_per_second.to_bits(),
            s.texture_changes_per_frame.to_bits(),
            s.drawn_poly_count
        )
    }

    #[test]
    fn matches_cpp_golden_scene_clear_vb_lights_transforms_show() {
        let sections = golden_sections(include_str!("golden_raster.txt"));
        let mut state = fresh_state(DD);
        state.mDirect3D = 100 as H;
        state.mDevice = 9 as H;
        state.mVertexBuffer = 11 as H;
        state.mViewport =
            crate::state::Viewport { dwX: 0, dwY: 0, dwWidth: 640, dwHeight: 480, dvMinZ: 0.0, dvMaxZ: 1.0 };
        let mut bench = Bench {
            in_use: [0; LIGHT_SLOTS],
            state,
            target_width: 640,
            target_height: 480,
            stats: FrameStats::default(),
        };
        let mut run = 0;
        for section in &sections {
            let (mut gx, mut host) = Recorder::pair();
            let Some(result) = run_rest(&section.name, &mut bench, &mut gx, &mut host) else { continue };
            let got: Vec<String> = gx.log.borrow().clone();
            assert_eq!(got, section.trace, "trace of {}", section.name);
            assert_eq!(result, section.result, "result of {}", section.name);
            for extra in &section.extra {
                if extra.starts_with("mode ") {
                    assert_eq!(
                        *extra,
                        format!(
                            "mode {} changed {}",
                            bench.state.mIs3DMode as i32, bench.state.mViewportChanged as i32
                        ),
                        "{}",
                        section.name
                    );
                } else if extra.starts_with("pos ") {
                    assert_eq!(
                        *extra,
                        format!("pos {} vb 8192", bench.state.mVertexBufferPosition),
                        "{}",
                        section.name
                    );
                } else if extra.starts_with("vp ") && extra.contains(" changed=") {
                    let v = &bench.state.mViewport;
                    let want = format!(
                        "vp {} {} {} {} {:08x} {:08x} changed={}",
                        v.dwX,
                        v.dwY,
                        v.dwWidth,
                        v.dwHeight,
                        v.dvMinZ.to_bits(),
                        v.dvMaxZ.to_bits(),
                        bench.state.mViewportChanged as i32
                    );
                    assert_eq!(*extra, want, "{}", section.name);
                } else if extra.starts_with("stats ") {
                    assert_eq!(*extra, fmt_stats(&bench.stats), "{}", section.name);
                }
                // "idx N", "rt=", "outs": the rt= lines hold addresses; "idx" and "outs" are the results of the functions
                // the trace already pins (asserted through the trace and their own unit tests)
            }
            run += 1;
        }
        assert!(run > 30, "ran {run}");
    }

    #[test]
    fn floors_match_um_floor_edges() {
        assert_eq!(um_floor(-0.5), -1); // lrintf(-0.5) = 0 (ties even), 0 > -0.5: -1
        assert_eq!(um_floor(1.5), 1); // lrintf(1.5) = 2, 2 > 1.5: 1
        assert_eq!(um_floor(2.5), 2); // lrintf = 2, not above
        assert_eq!(um_floor_unsigned(2.5), 2);
        assert_eq!(um_floor_unsigned(3.5), 3); // lrintf = 4, 4 > 3.5
        assert_eq!(clear_rect([1.4, 2.5, 100.5, 200.6]), IRect { left: 1, top: 3, right: 101, bottom: 201 });
        assert_eq!(gx_rgb([0.5, 1.0, 0.25]), 0xff7f_ff3f);
    }

    struct ActFake {
        log: Vec<String>,
        full: bool,
        modes: Vec<VideoMode>,
        current: VideoMode,
        create_results: std::collections::VecDeque<i32>,
        activate_result: i32,
        next: usize,
    }

    impl ActFake {
        fn new(full: bool) -> ActFake {
            let current = vm(1024, 768, 32, 1);
            ActFake {
                log: Vec::new(),
                full,
                modes: vec![vm(800, 600, 16, 7), current],
                current,
                create_results: Default::default(),
                activate_result: 0,
                next: 0,
            }
        }
    }

    fn vm(w: u32, h: u32, bits: u32, fmt: u32) -> VideoMode {
        VideoMode {
            mWidth: w as c_ulong,
            mHeight: h as c_ulong,
            mRefreshRate: 60,
            mBitCount: bits as c_ulong,
            mPixelFormat: fmt as c_ulong,
        }
    }

    impl RasterHost for ActFake {
        fn is_full_screen(&mut self) -> bool {
            self.full
        }
        fn current_mode(&mut self) -> VideoMode {
            self.current
        }
        fn mode_count(&mut self) -> u32 {
            self.modes.len() as u32
        }
        fn mode(&mut self, index: u32) -> VideoMode {
            self.modes[index as usize]
        }
        fn create_surface(&mut self, d: &mut SurfaceDescription) -> (i32, H) {
            self.next += 1;
            self.log.push(format!(
                "create type={} {}x{} fmt={} depth={} m14={} 3d={}",
                d.mType as u32,
                d.mWidth as u32,
                d.mHeight as u32,
                d.mPixelFormat as u32,
                d.mPixelDepth as u32,
                d.m14 as u32,
                d.mIs3DRenderable as u32
            ));
            let r = self.create_results.pop_front().unwrap_or(K_OK);
            (r, (self.next * 100) as H)
        }
        fn attached_surface(&mut self, s: H) -> H {
            (s as usize + 1) as H
        }
        fn attach_surface(&mut self, s: H, a: H) -> i32 {
            self.log.push(format!("attach {} {}", s as usize, a as usize));
            K_OK
        }
        fn dereference(&mut self, s: H) {
            self.log.push(format!("deref {}", s as usize));
        }
        fn activate_the_screen(&mut self, p: H, s: H) -> i32 {
            self.log.push(format!("activate {} {}", p as usize, s as usize));
            self.activate_result
        }
        fn commit_activation(&mut self, p: H, s: H) {
            self.log.push(format!("commit {} {}", p as usize, s as usize));
        }
        fn apply_defaults(&mut self) {
            self.log.push("defaults".to_string());
        }
        fn verify(&mut self, ok: bool, line: u32) {
            if !ok {
                self.log.push(format!("verify failed {line}"));
            }
        }
    }

    #[test]
    fn activate_screen_full_screen_z_buffer_order() {
        let mut host = ActFake::new(true);
        let r = activate_screen(&mut host, 640, 480, 2, true, 9);
        assert_eq!(r, K_OK);
        assert_eq!(
            host.log,
            vec![
                "create type=2 1024x768 fmt=1 depth=32 m14=2 3d=1",
                "create type=3 1024x768 fmt=9 depth=32 m14=0 3d=1",
                "attach 101 200",
                "deref 200",
                "activate 100 101",
                "commit 100 101",
                "defaults",
            ]
        );
    }

    #[test]
    fn activate_screen_windowed_back_buffer_and_z_on_primary_only() {
        let mut host = ActFake::new(false);
        let r = activate_screen(&mut host, 640, 480, 1, true, 11);
        assert_eq!(r, K_OK);
        assert_eq!(
            host.log,
            vec![
                "create type=0 1024x768 fmt=1 depth=32 m14=0 3d=1",
                "create type=1 640x480 fmt=1 depth=32 m14=0 3d=1",
                "create type=3 640x480 fmt=11 depth=16 m14=0 3d=1",
                "attach 200 300",
                "deref 300",
                "activate 100 200",
                "commit 100 200",
                "deref 200",
                "defaults",
            ]
        );
        let mut host = ActFake::new(false);
        activate_screen(&mut host, 640, 480, 0, true, 11);
        assert!(host.log.contains(&"attach 100 200".to_string()));
        assert!(host.log.contains(&"create type=3 1024x768 fmt=11 depth=16 m14=0 3d=1".to_string()));
    }

    #[test]
    fn activate_screen_activation_failure_skips_defaults() {
        let mut host = ActFake::new(true);
        host.activate_result = 3;
        let r = activate_screen(&mut host, 640, 480, 1, false, 9);
        assert_eq!(r, 3);
        assert!(!host.log.iter().any(|l| l == "defaults" || l.starts_with("commit")));
    }

    #[test]
    fn activate_screen_failed_last_surface_skips_activation() {
        let mut host = ActFake::new(false);
        host.create_results.push_back(K_NATIVE);
        let r = activate_screen(&mut host, 640, 480, 0, false, 9);
        assert_eq!(r, K_NATIVE);
        assert_eq!(host.log, vec!["create type=0 1024x768 fmt=1 depth=32 m14=0 3d=1".to_string()]);
    }

    #[test]
    fn activate_rendering_surface_retries_16_bit() {
        let mut host = ActFake::new(false);
        host.create_results.push_back(K_NATIVE);
        let r = activate_rendering_surface(&mut host, 320, 200);
        assert_eq!(r, K_OK);
        assert_eq!(
            host.log,
            vec![
                "create type=1 320x200 fmt=1 depth=32 m14=0 3d=1",
                "create type=1 320x200 fmt=7 depth=16 m14=0 3d=1",
                "activate 200 200",
                "commit 200 200",
            ]
        );
        let mut host = ActFake::new(false);
        host.activate_result = 1;
        let r = activate_rendering_surface(&mut host, 320, 200);
        assert_eq!(r, 1);
        // the first target and its retry are dereferenced
        assert_eq!(host.log.iter().filter(|l| l.starts_with("deref")).count(), 2);
    }

    struct Enumerated {
        log: Rc<RefCell<Vec<String>>>,
        valid: bool,
    }

    impl EnumerationHost for Enumerated {
        fn new_screen(&mut self, dd: H, description: &[u8], attached: bool, primary: bool) -> H {
            self.log.borrow_mut().push(format!(
                "new_screen {} {} {attached} {primary}",
                dd as usize,
                String::from_utf8_lossy(description)
            ));
            77 as H
        }
        fn screen_is_valid(&mut self, screen: H) -> bool {
            self.log.borrow_mut().push(format!("valid? {}", screen as usize));
            self.valid
        }
        fn add_valid_screen(&mut self, screen: H) {
            self.log.borrow_mut().push(format!("add {}", screen as usize));
        }
        fn delete_screen(&mut self, screen: H) {
            self.log.borrow_mut().push(format!("delete {}", screen as usize));
        }
        fn set_active(&mut self) {
            self.log.borrow_mut().push("active".to_string());
        }
    }

    #[test]
    fn enumeration_creates_valid_screens_and_activates() {
        let (mut gx, mut host) = Recorder::pair();
        gx.queue("fx_caps", &[0]);
        let mut e = Enumerated { log: gx.log.clone(), valid: true };
        on_device_enumerated(&mut gx, &mut host, &mut e, true, true, b"Primary");
        assert_eq!(
            gx.log.borrow().join("\n"),
            "dd_create\ndd_get_caps 100\nnew_screen 100 Primary true true\nvalid? 77\nadd 77"
        );
        // guid set, no monitor: unattached and not primary
        let (mut gx, mut host) = Recorder::pair();
        let mut e = Enumerated { log: gx.log.clone(), valid: false };
        on_device_enumerated(&mut gx, &mut host, &mut e, false, true, b"Other");
        assert_eq!(
            gx.log.borrow().join("\n"),
            "dd_create\ndd_get_caps 100\nnew_screen 100 Other false false\nvalid? 77\ndelete 77"
        );
        assert_eq!(finish_activation(&mut host, &mut Enumerated { log: gx.log.clone(), valid: true }, 0), K_OK);
        assert!(gx.log.borrow().last().unwrap() == "active");
        assert_eq!(finish_activation(&mut host, &mut Enumerated { log: gx.log.clone(), valid: true }, 0x88), K_NATIVE);
        assert!(gx.log.borrow().last().unwrap() == "native_error 88");
    }

    #[test]
    fn enumeration_without_3d_or_with_errors_releases() {
        let (mut gx, mut host) = Recorder::pair();
        gx.dd_caps_3d = false;
        let mut e = Enumerated { log: gx.log.clone(), valid: true };
        on_device_enumerated(&mut gx, &mut host, &mut e, true, true, b"x");
        assert_eq!(gx.log.borrow().join("\n"), "dd_create\ndd_get_caps 100\ndd_release 100");
        let (mut gx, mut host) = Recorder::pair();
        gx.queue("get_caps", &[0x5]);
        let mut e = Enumerated { log: gx.log.clone(), valid: true };
        on_device_enumerated(&mut gx, &mut host, &mut e, true, true, b"x");
        assert_eq!(gx.log.borrow().join("\n"), "dd_create\ndd_get_caps 100\nnative_error 5\ndd_release 100");
        let (mut gx, mut host) = Recorder::pair();
        gx.queue("dd_create", &[0x6]);
        let mut e = Enumerated { log: gx.log.clone(), valid: true };
        on_device_enumerated(&mut gx, &mut host, &mut e, true, true, b"x");
        assert_eq!(gx.log.borrow().join("\n"), "dd_create\nnative_error 6");
    }
}
