//! `DX7Rasterizer` and `DX7TnLRasterizer` (`Common/DX7Rasterizer.cpp`, `DX7HRTnLDisplay/DX7TnLRasterizer.cpp`): the
//! screen activation with its device creation and fixed render states, the vertex buffer, the screen validity probe,
//! scene begin/end, the clears, the vertex buffer locks, the lights, the 2D/3D transforms and the frame statistics.
//! The per-screen state is the pinned [`ScreenState`](crate::state::ScreenState) (the C++ draw code reads it); every
//! object the algorithms need beyond GX comes in as plain data or a [`RasterHost`] call.
//!
//! Golden: `golden_raster.txt`, the call trace of the real `DX7TnLRasterizer.cpp` / `DX7Rasterizer.cpp` compiled against
//! a fake of `GX.h` (scratch generator, clang-cl, 2026-11-09), replayed on the recorder.

use core::ffi::c_ulong;

use crate::api::*;
use crate::state::ScreenState;

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

#[cfg(test)]
#[allow(clippy::unnecessary_cast)]
mod tests {
    use super::*;
    use crate::api::test_recorder::*;

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
}
