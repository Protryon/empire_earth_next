//! `DX7Screen` (`Common/DX7Screen.cpp`): the display-mode table and enumeration, the surface description mapping,
//! CreateSurface with its Z-buffer preferences and attached chains, Capture/Release, SetMode, RestoreAllSurfaces, the
//! 32-bit support probe. The class stays C++ (`GEScreen`, `DX7Surface`s, the window); everything the algorithms need
//! from it goes through [`ScreenHost`].
//!
//! Golden: `golden_screen.txt`, the call trace of the real `DX7Screen.cpp` compiled against a fake of `GX.h` and the
//! window system (scratch generator, clang-cl, 2026-11-09), replayed here on the recorder.

#![allow(clippy::unnecessary_cast, clippy::too_many_arguments)] // c_ulong differs per target

use core::ffi::c_ulong;

use crate::api::*;
use crate::vertex::float_to_long;

// kPixelFormat* (GERenderState.h)
const PIXEL_FORMAT_Z32: u32 = 9;
const PIXEL_FORMAT_Z24: u32 = 10;
const PIXEL_FORMAT_Z16: u32 = 11;
const PIXEL_FORMAT_Z_PREFER16: u32 = 12;
const PIXEL_FORMAT_Z_PREFER32: u32 = 13;
const PIXEL_FORMAT_UNKNOWN: u32 = 14;
const PIXEL_FORMAT_A8R8G8B8: u32 = 0;

// kSurface*
const SURFACE_PRIMARY: u32 = 0;
const SURFACE_OFFSCREEN: u32 = 1;
const SURFACE_FLIPPING_PRIMARY: u32 = 2;
const SURFACE_Z_BUFFER: u32 = 3;
const SURFACE_TEXTURE: u32 = 5;
const SURFACE_MIPMAPPED_TEXTURE: u32 = 6;
const SURFACE_BACK_BUFFER: u32 = 7;

const GXSD_CAPS: u32 = 0x0000_0001;
const GXSD_HEIGHT: u32 = 0x0000_0002;
const GXSD_WIDTH: u32 = 0x0000_0004;
const GXSD_BACKBUFFERCOUNT: u32 = 0x0000_0020;
const GXSD_PIXELFORMAT: u32 = 0x0000_1000;
const GXSD_CKSRCBLT: u32 = 0x0001_0000;
const GXSD_MIPMAPCOUNT: u32 = 0x0002_0000;
const GXSCAPS_COMPLEX: u32 = 0x0000_0008;
const GXSCAPS_FLIP: u32 = 0x0000_0010;
const GXSCAPS_OFFSCREENPLAIN: u32 = 0x0000_0040;
const GXSCAPS_PRIMARYSURFACE: u32 = 0x0000_0200;
const GXSCAPS_SYSTEMMEMORY: u32 = 0x0000_0800;
const GXSCAPS_TEXTURE: u32 = 0x0000_1000;
const GXSCAPS_3DDEVICE: u32 = 0x0000_2000;
const GXSCAPS_VIDEOMEMORY: u32 = 0x0000_4000;
const GXSCAPS_ZBUFFER: u32 = 0x0002_0000;
const GXSCAPS_MIPMAP: u32 = 0x0040_0000;
const GXSCAPS_LOCALVIDMEM: u32 = 0x1000_0000;
const GXSCAPS2_HINTDYNAMIC: u32 = 4;
const GXSCAPS2_HINTSTATIC: u32 = 8;
const GXSCAPS2_TEXTUREMANAGE: u32 = 0x10;

const GXSCL_FULLSCREEN: u32 = 1;
const GXSCL_ALLOWREBOOT: u32 = 2;
const GXSCL_NORMAL: u32 = 8;
const GXSCL_EXCLUSIVE: u32 = 0x10;
const GXSCL_SETFOCUSWINDOW: u32 = 0x80;
const GXSCL_SETDEVICEWINDOW: u32 = 0x100;
const GXSCL_CREATEDEVICEWINDOW: u32 = 0x200;
const GXSCL_MULTITHREADED: u32 = 0x400;

const GXBD_32: u32 = 0x100;
const GXFXCAPS_BLTARITHSTRETCHY: u32 = 0x20;
const GXFXCAPS_BLTSHRINKX: u32 = 0x400;
const GXFXCAPS_BLTSHRINKY: u32 = 0x1000;
const GXFXCAPS_BLTSTRETCHX: u32 = 0x4000;
const GXFXCAPS_BLTSTRETCHY: u32 = 0x10000;

const GXPF_ALPHAPIXELS: u32 = 1;
const GXPF_FOURCC: u32 = 4;
const GXPF_RGB: u32 = 0x40;
const GXPF_ZBUFFER: u32 = 0x400;
const FOURCC_DXT1: u32 = u32::from_le_bytes(*b"DXT1");

/// `GEVideoMode`, pinned by `DX7Trampolines.cpp`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[allow(non_snake_case)]
pub struct VideoMode {
    pub mWidth: c_ulong,
    pub mHeight: c_ulong,
    pub mRefreshRate: c_ulong,
    pub mBitCount: c_ulong,
    pub mPixelFormat: c_ulong,
}

/// `gPixelFormats`: the GXPIXELFORMAT fields (after `dwSize`) of each GE pixel format.
const fn pf(flags: u32, four_cc: u32, bits: u32, r: u32, g: u32, b: u32, a: u32) -> PixelFormat {
    PixelFormat {
        dwFlags: flags,
        dwFourCC: four_cc,
        dwRGBBitCount: bits,
        dwRBitMask: r,
        dwGBitMask: g,
        dwBBitMask: b,
        dwRGBAlphaBitMask: a,
    }
}

pub static PIXEL_FORMATS: [PixelFormat; 12] = [
    pf(GXPF_RGB | GXPF_ALPHAPIXELS, 0, 32, 0x00ff_0000, 0x0000_ff00, 0x0000_00ff, 0xff00_0000),
    pf(GXPF_RGB, 0, 32, 0x00ff_0000, 0x0000_ff00, 0x0000_00ff, 0),
    pf(GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000),
    pf(GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000),
    pf(GXPF_RGB | GXPF_ALPHAPIXELS, 0, 16, 0xf800, 0x07c0, 0x003e, 0x0001),
    pf(GXPF_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0),
    pf(GXPF_RGB, 0, 16, 0xf800, 0x07c0, 0x003e, 0),
    pf(GXPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0),
    pf(GXPF_FOURCC, FOURCC_DXT1, 0, 0, 0, 0, 0),
    pf(GXPF_ZBUFFER, 0, 32, 0, 0xffff_ffff, 0, 0),
    pf(GXPF_ZBUFFER, 0, 24, 0, 0x00ff_ffff, 0, 0),
    pf(GXPF_ZBUFFER, 0, 16, 0, 0x0000_ffff, 0, 0),
];

/// `EnumVideoModeCallback`: the mode and its GE pixel format, `kPixelFormatUnknown` when none matches.
pub fn video_mode_of(desc: &SurfaceDesc) -> VideoMode {
    let bits = desc.format.dwRGBBitCount;
    let mut format = PIXEL_FORMAT_UNKNOWN;
    for (i, p) in PIXEL_FORMATS.iter().enumerate() {
        if p.dwRGBBitCount == bits
            && p.dwRBitMask == desc.format.dwRBitMask
            && p.dwGBitMask == desc.format.dwGBitMask
            && p.dwBBitMask == desc.format.dwBBitMask
            && p.dwRGBAlphaBitMask == desc.format.dwRGBAlphaBitMask
        {
            format = i as u32;
            break;
        }
    }
    VideoMode {
        mWidth: desc.dwWidth as c_ulong,
        mHeight: desc.dwHeight as c_ulong,
        mRefreshRate: desc.dwRefreshRate as c_ulong,
        mBitCount: bits as c_ulong,
        mPixelFormat: format as c_ulong,
    }
}

/// `EnumTextureFormatCallback`'s test: a 32-bit ARGB 8888 format (the callback cancels on it).
pub fn is_argb8888(f: &PixelFormat) -> bool {
    f.dwRGBBitCount == 32
        && (f.dwRGBAlphaBitMask & 0xff00_0000) > 0
        && (f.dwRBitMask & 0x00ff_0000) > 0
        && (f.dwGBitMask & 0x0000_ff00) > 0
        && (f.dwBBitMask & 0x0000_00ff) > 0
}

/// The GXSURFACEDESC of a CreateSurface request, the switch on `mType` in `DX7Screen::CreateSurface`.
pub fn surface_desc_for(d: &SurfaceDescription) -> SurfaceDesc {
    let (ty, format) = (d.mType as u32, d.mPixelFormat as u32);
    let key = if d.mIsTransparencyEnabled { GXSD_CKSRCBLT } else { 0 };
    let r3d = d.mIs3DRenderable;
    let color = (key | GXSD_CAPS | GXSD_HEIGHT | GXSD_WIDTH | GXSD_PIXELFORMAT, r3d);
    let (flags, caps, caps2, bbc, mips) = match ty {
        SURFACE_PRIMARY => {
            let c = GXSCAPS_PRIMARYSURFACE | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
            (key | GXSD_CAPS, if r3d { c | GXSCAPS_3DDEVICE } else { c }, 0, 0, 0)
        }
        SURFACE_OFFSCREEN => {
            let c = GXSCAPS_OFFSCREENPLAIN | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
            (color.0, if r3d { c | GXSCAPS_3DDEVICE } else { c }, 0, 0, 0)
        }
        SURFACE_FLIPPING_PRIMARY => {
            let c = GXSCAPS_PRIMARYSURFACE | GXSCAPS_FLIP | GXSCAPS_COMPLEX | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
            (key | GXSD_CAPS | GXSD_BACKBUFFERCOUNT, if r3d { c | GXSCAPS_3DDEVICE } else { c }, 0, d.m14 as u32, 0)
        }
        SURFACE_Z_BUFFER => (color.0, GXSCAPS_ZBUFFER | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM, 0, 0, 0),
        4 => {
            // kSurfaceSystemMemory
            let c = GXSCAPS_OFFSCREENPLAIN | GXSCAPS_SYSTEMMEMORY;
            (color.0, if r3d { c | GXSCAPS_3DDEVICE } else { c }, 0, 0, 0)
        }
        SURFACE_TEXTURE => {
            if r3d {
                let c = GXSCAPS_TEXTURE | GXSCAPS_3DDEVICE | GXSCAPS_VIDEOMEMORY | GXSCAPS_LOCALVIDMEM;
                let c2 = if d.mSurfaceVolatility != 0 { GXSCAPS2_HINTSTATIC } else { GXSCAPS2_HINTDYNAMIC };
                (color.0, c, c2, 0, 0)
            } else {
                let c2 = GXSCAPS2_TEXTUREMANAGE
                    | if d.mSurfaceVolatility != 0 { GXSCAPS2_HINTSTATIC } else { GXSCAPS2_HINTDYNAMIC };
                (color.0, GXSCAPS_TEXTURE, c2, 0, 0)
            }
        }
        SURFACE_MIPMAPPED_TEXTURE => {
            let c = GXSCAPS_TEXTURE | GXSCAPS_COMPLEX | GXSCAPS_MIPMAP;
            let c2 = GXSCAPS2_TEXTUREMANAGE
                | if d.mSurfaceVolatility != 0 { GXSCAPS2_HINTSTATIC } else { GXSCAPS2_HINTDYNAMIC };
            (color.0 | GXSD_MIPMAPCOUNT, if r3d { c | GXSCAPS_3DDEVICE } else { c }, c2, 0, d.mMipLevelCount as u32)
        }
        _ => (0, 0, 0, 0, 0),
    };
    let format = PIXEL_FORMATS.get(format as usize).copied().unwrap_or_default();
    SurfaceDesc {
        dwFlags: flags,
        dwHeight: d.mHeight as u32,
        dwWidth: d.mWidth as u32,
        dwBackBufferCount: bbc,
        dwMipMapCount: mips,
        dwRefreshRate: 0,
        format,
        ddsCaps: caps,
        ddsCaps2: caps2,
    }
}

/// The C++ side of a screen. In the game the `ee_dx7cpp_screen_*` trampolines around a `DX7Screen*`.
pub trait ScreenHost {
    fn new_surface(&mut self, gx_surface: H, desc: &SurfaceDescription) -> H; // new DX7Surface
    fn push_attached(&mut self, parent: H, child: H); // mAttachedSurfaces.push_back
    fn surface_gx(&mut self, surface: H) -> H; // the DX7Surface's GXSurface*
    fn surface_dereference(&mut self, surface: H);

    // the constructor
    fn set_filter_caps(&mut self, min_filter: bool, mag_filter: bool);
    fn push_mode(&mut self, mode: VideoMode);
    fn set_current_mode(&mut self, index: u32);

    // the 32-bit probe (calls that go through the class's virtuals)
    fn create_probe_window(&mut self) -> (i32, H);
    fn capture_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32;
    fn release_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32;
    fn delete_window(&mut self, window: H);
    fn dispatch_window_events(&mut self);
    fn mode_count(&mut self) -> u32;
    fn mode(&mut self, index: u32) -> VideoMode;
    fn set_32bit_support(&mut self, device: bool, texture: bool);
}

/// The constructor's device queries: the surface filter caps (the ctor's report), the display modes, the current mode.
pub fn initialize<G: Gx, S: ScreenHost>(gx: &mut G, host: &mut S, dd: H) {
    let (_, caps) = gx.dd_get_caps(dd);
    if caps.dwFXCaps & GXFXCAPS_BLTARITHSTRETCHY != 0 {
        let shrink = caps.dwFXCaps & GXFXCAPS_BLTSHRINKX != 0 && caps.dwFXCaps & GXFXCAPS_BLTSHRINKY != 0;
        let stretch = caps.dwFXCaps & GXFXCAPS_BLTSTRETCHX != 0 && caps.dwFXCaps & GXFXCAPS_BLTSTRETCHY != 0;
        host.set_filter_caps(shrink, stretch);
    } else {
        host.set_filter_caps(false, false);
    }
    let mut modes: Vec<VideoMode> = Vec::new();
    gx.dd_enum_display_modes(dd, &mut |desc| {
        modes.push(video_mode_of(desc));
    });
    for mode in &modes {
        host.push_mode(*mode);
    }
    let (_, display) = gx.dd_get_display_mode(dd);
    for (i, m) in modes.iter().enumerate() {
        if m.mWidth as u32 == display.dwWidth
            && m.mHeight as u32 == display.dwHeight
            && m.mBitCount as u32 == display.format.dwRGBBitCount
        {
            host.set_current_mode(i as u32);
            break;
        }
    }
}

/// `sScreensCaptured` and `sFirstCaptureWasAttachedToDesktop`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CaptureState {
    pub captured: u32,
    pub first_attached: bool,
}

/// `DX7Screen::Capture`.
pub fn capture<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    dd: H,
    window: H,
    to_clipboard: bool,
    attached: bool,
    state: &mut CaptureState,
    full_screen: &mut bool,
) -> i32 {
    let mut result = K_OK;
    let mut error = GX_OK;
    if to_clipboard {
        if state.captured == 0 {
            if attached {
                error = gx.dd_set_cooperative_level(dd, window, GXSCL_SETFOCUSWINDOW);
                if error == GX_OK {
                    error = gx.dd_set_cooperative_level(
                        dd,
                        window,
                        GXSCL_MULTITHREADED
                            | GXSCL_SETDEVICEWINDOW
                            | GXSCL_EXCLUSIVE
                            | GXSCL_ALLOWREBOOT
                            | GXSCL_FULLSCREEN,
                    );
                }
                if error == GX_OK {
                    *full_screen = true;
                    state.first_attached = true;
                }
            } else {
                error = gx.dd_set_cooperative_level(
                    dd,
                    window,
                    GXSCL_MULTITHREADED | GXSCL_EXCLUSIVE | GXSCL_ALLOWREBOOT | GXSCL_FULLSCREEN,
                );
                if error == GX_OK {
                    *full_screen = true;
                    state.first_attached = false;
                }
            }
        } else if state.first_attached && attached {
            error = gx.dd_set_cooperative_level(
                dd,
                window,
                GXSCL_MULTITHREADED
                    | GXSCL_CREATEDEVICEWINDOW
                    | GXSCL_SETFOCUSWINDOW
                    | GXSCL_EXCLUSIVE
                    | GXSCL_FULLSCREEN,
            );
            if error == GX_OK {
                *full_screen = true;
            }
        } else {
            result = K_NOT_SUPPORTED;
        }
        if result == K_OK && error != GX_OK {
            host.record_native_error(error);
            result = K_NATIVE;
            *full_screen = false;
        }
    } else {
        if state.captured == 0 {
            *full_screen = false;
            error = gx.dd_set_cooperative_level(dd, window, GXSCL_MULTITHREADED | GXSCL_NORMAL);
            if error == GX_OK {
                state.first_attached = attached;
            }
        } else if state.first_attached && attached {
            *full_screen = false;
            error = gx.dd_set_cooperative_level(dd, window, GXSCL_MULTITHREADED | GXSCL_NORMAL);
        } else {
            result = K_NOT_SUPPORTED;
        }
        if result == K_OK && error != GX_OK {
            host.record_native_error(error);
            result = K_NATIVE;
        }
    }
    if result == K_OK {
        state.captured = state.captured.wrapping_add(1);
    }
    result
}

/// `DX7Screen::Release`.
pub fn release<G: Gx>(
    gx: &mut G,
    host: &mut impl Host,
    dd: H,
    window: H,
    state: &mut CaptureState,
    full_screen: &mut bool,
) -> i32 {
    let error = gx.dd_set_cooperative_level(dd, window, GXSCL_NORMAL);
    if error == GX_OK {
        *full_screen = false;
        state.captured = state.captured.wrapping_sub(1);
        K_OK
    } else {
        host.record_native_error(error);
        K_NATIVE
    }
}

/// `DX7Screen::RestoreAllSurfaces`.
pub fn restore_all<G: Gx>(gx: &mut G, host: &mut impl Host, dd: H) -> i32 {
    if gx.dd_test_cooperative_level(dd) == GX_OK {
        let error = gx.dd_restore_all_surfaces(dd);
        if error != GX_OK {
            host.record_native_error(error);
            return K_NATIVE;
        }
        K_OK
    } else {
        host.sleep(100);
        K_NATIVE
    }
}

/// `DX7Screen::SetMode`: the bit count from the mode's pixel format; records the mode index when full screen.
pub fn set_mode<G: Gx>(gx: &mut G, host: &mut impl Host, dd: H, full_screen: bool, mode: &VideoMode) -> (i32, bool) {
    if !full_screen {
        return (K_OK, false);
    }
    let bits = PIXEL_FORMATS.get(mode.mPixelFormat as usize).map_or(0, |p| p.dwRGBBitCount);
    let error = gx.dd_set_display_mode(dd, mode.mWidth as u32, mode.mHeight as u32, bits, mode.mRefreshRate as u32, 0);
    if error == GX_OK {
        (K_OK, true)
    } else {
        host.record_native_error(error);
        (K_NATIVE, true)
    }
}

/// `DX7Screen::CreateSurface`: `description` keeps the updates the C++ made to its reference argument (the Z formats
/// and depths of the preference paths, the back-buffer type of flip chains).
pub fn create_surface<G: Gx, O: Host, S: ScreenHost>(
    gx: &mut G,
    host: &mut O,
    screen: &mut S,
    dd: H,
    description: &mut SurfaceDescription,
    out: &mut H,
) -> i32 {
    let mut local = *description; // the C++ copy taken before the preference rewrites
    *out = NULL;
    let mut result = K_OK;
    if description.mType as u32 == SURFACE_Z_BUFFER {
        let format = description.mPixelFormat as u32;
        if format == PIXEL_FORMAT_Z_PREFER16 || format == PIXEL_FORMAT_Z_PREFER32 {
            let order: [(u32, c_ulong); 3] = if format == PIXEL_FORMAT_Z_PREFER16 {
                [(PIXEL_FORMAT_Z16, 16), (PIXEL_FORMAT_Z24, 24), (PIXEL_FORMAT_Z32, 32)]
            } else {
                [(PIXEL_FORMAT_Z32, 32), (PIXEL_FORMAT_Z24, 24), (PIXEL_FORMAT_Z16, 16)]
            };
            for (i, (f, depth)) in order.iter().enumerate() {
                description.mPixelFormat = *f as c_ulong;
                result = create_surface(gx, host, screen, dd, description, out);
                if result == K_OK || i == 2 {
                    description.mPixelDepth = *depth;
                    if result == K_OK {
                        break;
                    }
                }
            }
            if result != K_OK {
                return result;
            }
        }
    }
    if !out.is_null() {
        return result;
    }
    let desc = surface_desc_for(description);
    let (error, surface) = gx.dd_create_surface(dd, &desc);
    if error == GX_OK {
        let surface_handle = screen.new_surface(surface, description);
        *out = surface_handle;
        if description.mType as u32 == SURFACE_FLIPPING_PRIMARY {
            description.mType = SURFACE_BACK_BUFFER as c_ulong;
            let mut parent_gx = surface;
            let mut parent = surface_handle;
            for _ in 0..description.m14 as u32 {
                let attached = gx.surface_get_attached(parent_gx, GXSCAPS_FLIP);
                let child = screen.new_surface(attached, description);
                screen.push_attached(parent, child);
                parent = child;
                parent_gx = attached;
            }
        } else if description.mType as u32 == SURFACE_MIPMAPPED_TEXTURE {
            let mut parent_gx = surface;
            let mut parent = surface_handle;
            let levels = description.mMipLevelCount as i64 - 1;
            let mut i: u64 = 0;
            while i < levels as u64 {
                // the C++ `i < inDescription.mMipLevelCount - 1` compares as unsigned
                let attached = gx.surface_get_attached(parent_gx, GXSCAPS_MIPMAP);
                local.mWidth >>= 1;
                local.mHeight >>= 1;
                let child = screen.new_surface(attached, &local);
                screen.push_attached(parent, child);
                parent = child;
                parent_gx = attached;
                i += 1;
            }
        }
    } else {
        host.record_native_error(error);
        result = K_NATIVE;
    }
    result
}

/// `DX7Screen::Test32BitSupport`: 32-bit rendering and texture support on a hidden probe window.
pub fn test_32bit_support<G: Gx, O: Host, S: ScreenHost>(gx: &mut G, host: &mut O, screen: &mut S, dd: H) {
    let mut device32 = false;
    let mut texture32 = false;
    let (window_result, window) = screen.create_probe_window();
    if window_result == K_OK {
        if screen.capture_window(gx, host, window) == K_OK {
            let count = screen.mode_count();
            let mut i = 0;
            while i < count {
                let m = screen.mode(i);
                if m.mWidth as u32 == 800 && m.mHeight as u32 == 600 && m.mBitCount as u32 == 16 {
                    break;
                }
                i += 1;
            }
            let mode = screen.mode(i); // as the C++: an out-of-range index when no 800x600x16 mode exists
            let mut description = SurfaceDescription {
                mType: SURFACE_OFFSCREEN as c_ulong,
                mPixelFormat: mode.mPixelFormat,
                mSurfaceVolatility: 0,
                mWidth: 2,
                mHeight: 2,
                m14: 0,
                mMipLevelCount: 0,
                mPixelDepth: mode.mBitCount,
                mIs3DRenderable: true,
                mIsTransparencyEnabled: false,
                mNoAlpha: false,
            };
            let mut surface = NULL;
            if create_surface(gx, host, screen, dd, &mut description, &mut surface) == K_OK && !surface.is_null() {
                let target = screen.surface_gx(surface);
                let (query, direct3d) = gx.dd_query_interface(dd, IID_DIRECT3D7);
                if query == GX_OK {
                    let (create, device) = gx.dd_create_device(direct3d, IID_HAL_DEVICE, target);
                    if create == GX_OK {
                        device32 = (gx.device_get_caps(device).dwDeviceRenderBitDepth & GXBD_32) > 0;
                        gx.device_enum_texture_formats(device, &mut |f| {
                            if is_argb8888(f) {
                                texture32 = true;
                                true
                            } else {
                                false
                            }
                        });
                        if texture32 {
                            let mut texture_desc = SurfaceDescription {
                                mType: SURFACE_TEXTURE as c_ulong,
                                mPixelFormat: PIXEL_FORMAT_A8R8G8B8 as c_ulong,
                                mSurfaceVolatility: 1,
                                mWidth: 16,
                                mHeight: 16,
                                m14: 0,
                                mMipLevelCount: 0,
                                mPixelDepth: 32,
                                mIs3DRenderable: false,
                                mIsTransparencyEnabled: false,
                                mNoAlpha: false,
                            };
                            let mut texture = NULL;
                            if create_surface(gx, host, screen, dd, &mut texture_desc, &mut texture) == K_OK
                                && !texture.is_null()
                            {
                                screen.surface_dereference(texture);
                            } else {
                                texture32 = false;
                            }
                        }
                        gx.device_release(device);
                    }
                    gx.dd_release(direct3d);
                }
                screen.surface_dereference(surface);
            }
            screen.release_window(gx, host, window);
        }
        screen.delete_window(window);
        // the activation the probe's destruction gives back to the game window (Win32 sent it during DestroyWindow)
        screen.dispatch_window_events();
    }
    screen.set_32bit_support(device32, texture32);
}

/// What `CreateAWindow` makes of its style, display and origin: which platform call, its size and flags.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct WindowParams {
    pub create_at: bool,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub flags: u32,
}

const PLAT_WINDOW_RESIZABLE: u32 = 0x1;
const PLAT_WINDOW_FULLSCREEN: u32 = 0x2;
const PLAT_WINDOW_HIDDEN: u32 = 0x4;
const PLAT_WINDOW_BORDERLESS: u32 = 0x10;
const PLAT_WINDOW_TOPMOST: u32 = 0x20;

/// `CreateAWindow`'s choice: `sim` the headless simulation; `style` as `GEScreen::CreateAWindow`.
pub fn window_params(
    sim: bool,
    style: u32,
    width: i32,
    height: i32,
    display_width: i32,
    display_height: i32,
    x: f32,
    y: f32,
) -> WindowParams {
    let plain = |flags| WindowParams { create_at: false, x: 0, y: 0, width, height, flags };
    if sim || style == 2 {
        plain(PLAT_WINDOW_HIDDEN | PLAT_WINDOW_BORDERLESS)
    } else if style == 1 {
        plain(PLAT_WINDOW_RESIZABLE)
    } else if width >= display_width && height >= display_height {
        WindowParams {
            create_at: false,
            x: 0,
            y: 0,
            width: display_width,
            height: display_height,
            flags: PLAT_WINDOW_FULLSCREEN,
        }
    } else {
        WindowParams {
            create_at: true,
            x: float_to_long(x),
            y: float_to_long(y),
            width,
            height,
            flags: PLAT_WINDOW_BORDERLESS | PLAT_WINDOW_TOPMOST,
        }
    }
}

#[cfg(test)]
#[allow(clippy::unnecessary_cast, clippy::too_many_arguments)]
mod tests {
    use super::*;
    use crate::api::test_recorder::*;
    use core::cell::RefCell;
    use std::rc::Rc;

    const DD: H = 100 as H;

    /// The class's C++ services, recording like the generator's fake of the window system. Surfaces are handled through
    /// their GX handle (`new_surface` answers it).
    struct Fake {
        log: Rc<RefCell<Vec<String>>>,
        modes: Vec<VideoMode>,
        window_ok: bool,
        captured: CaptureState,
        full: bool,
        attached: bool,
        current: Option<u32>,
        min_filter: bool,
        mag_filter: bool,
        dev32: bool,
        tex32: bool,
    }

    impl Fake {
        fn new(gx: &Recorder) -> Fake {
            Fake {
                log: gx.log.clone(),
                modes: Vec::new(),
                window_ok: false,
                captured: CaptureState::default(),
                full: false,
                attached: false,
                current: None,
                min_filter: false,
                mag_filter: false,
                dev32: false,
                tex32: false,
            }
        }
    }

    impl ScreenHost for Fake {
        fn new_surface(&mut self, gx: H, _d: &SurfaceDescription) -> H {
            gx
        }
        fn push_attached(&mut self, _parent: H, _child: H) {}
        fn surface_gx(&mut self, s: H) -> H {
            s
        }
        fn surface_dereference(&mut self, s: H) {
            self.log.borrow_mut().push(format!("surface_release {}", s as usize));
        }
        fn set_filter_caps(&mut self, min: bool, mag: bool) {
            self.min_filter = min;
            self.mag_filter = mag;
        }
        fn push_mode(&mut self, mode: VideoMode) {
            self.modes.push(mode);
        }
        fn set_current_mode(&mut self, index: u32) {
            self.current = Some(index);
        }
        fn create_probe_window(&mut self) -> (i32, H) {
            self.log.borrow_mut().push("plat_create_window 16 16 14".to_string());
            if self.window_ok {
                self.log.borrow_mut().push("plat_cursor 0".to_string());
                self.log.borrow_mut().push("dispatch_events".to_string());
                (K_OK, 33 as H)
            } else {
                self.log.borrow_mut().push("native_error 5".to_string());
                (K_NATIVE, NULL)
            }
        }
        fn capture_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32 {
            let mut state = self.captured;
            let mut full = self.full;
            let r = capture(gx, host, DD, window, false, self.attached, &mut state, &mut full);
            self.captured = state;
            self.full = full;
            r
        }
        fn release_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32 {
            let mut state = self.captured;
            let mut full = self.full;
            let r = release(gx, host, DD, window, &mut state, &mut full);
            self.captured = state;
            self.full = full;
            r
        }
        fn delete_window(&mut self, _window: H) {
            self.log.borrow_mut().push("window_delete".to_string());
        }
        fn dispatch_window_events(&mut self) {
            self.log.borrow_mut().push("dispatch_events".to_string());
        }
        fn mode_count(&mut self) -> u32 {
            self.modes.len() as u32
        }
        fn mode(&mut self, index: u32) -> VideoMode {
            self.modes.get(index as usize).copied().unwrap_or_default()
        }
        fn set_32bit_support(&mut self, d: bool, t: bool) {
            self.dev32 = d;
            self.tex32 = t;
        }
    }

    fn sd(w: u32, h: u32, bits: u32, rate: u32, r: u32, g: u32, b: u32, a: u32) -> SurfaceDesc {
        SurfaceDesc {
            dwWidth: w,
            dwHeight: h,
            dwRefreshRate: rate,
            format: PixelFormat {
                dwRGBBitCount: bits,
                dwRBitMask: r,
                dwGBitMask: g,
                dwBBitMask: b,
                dwRGBAlphaBitMask: a,
                ..Default::default()
            },
            ..Default::default()
        }
    }

    fn base_modes() -> Vec<SurfaceDesc> {
        vec![
            sd(800, 600, 16, 60, 0xf800, 0x7e0, 0x1f, 0),
            sd(1024, 768, 32, 75, 0xff0000, 0xff00, 0xff, 0),
            sd(800, 600, 16, 85, 0xf800, 0x7e0, 0x1f, 0),
        ]
    }

    fn argb_format() -> PixelFormat {
        PixelFormat {
            dwRGBBitCount: 32,
            dwRGBAlphaBitMask: 0xff00_0000,
            dwRBitMask: 0xff_0000,
            dwGBitMask: 0xff00,
            dwBBitMask: 0xff,
            ..Default::default()
        }
    }

    fn ctor(name: &str, gx: &mut Recorder, h: &mut RecHost, f: &mut Fake) -> Option<()> {
        let display_0 = |gx: &mut Recorder| gx.display_mode = gx.modes[0];
        match name {
            "ctor_window_fails" => {
                gx.modes = base_modes();
                gx.display_mode = gx.modes[1];
                gx.queue("fx_caps", &[0x15420]);
            }
            "ctor_no_arith" => {
                gx.modes = base_modes();
                gx.display_mode = gx.modes[1];
                gx.queue("fx_caps", &[0x15400]);
            }
            "ctor_probe_ok_texture" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("device_caps", &[0x100]);
                gx.texture_formats = vec![argb_format()];
                f.window_ok = true;
            }
            "ctor_probe_no_32" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("device_caps", &[0]);
                f.window_ok = true;
            }
            "ctor_capture_fails" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("coop", &[0x90]);
                f.window_ok = true;
            }
            "ctor_query_fails" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("query", &[0x91]);
                f.window_ok = true;
            }
            "ctor_device_fails" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("create_device", &[0x92]);
                f.window_ok = true;
            }
            "ctor_surface_fails" => {
                gx.modes = base_modes();
                display_0(gx);
                gx.queue("create_surface", &[0x93]);
                f.window_ok = true;
            }
            _ => return None,
        }
        initialize(gx, f, DD);
        test_32bit_support(gx, h, f, DD);
        Some(())
    }

    fn desc(
        ty: u32,
        fmt: u32,
        vol: u32,
        w: u32,
        h: u32,
        m14: u32,
        mip: i32,
        depth: u32,
        r3d: bool,
        key: bool,
    ) -> SurfaceDescription {
        SurfaceDescription {
            mType: ty as c_ulong,
            mPixelFormat: fmt as c_ulong,
            mSurfaceVolatility: vol as c_ulong,
            mWidth: w as c_ulong,
            mHeight: h as c_ulong,
            m14: m14 as c_ulong,
            mMipLevelCount: mip as core::ffi::c_long,
            mPixelDepth: depth as c_ulong,
            mIs3DRenderable: r3d,
            mIsTransparencyEnabled: key,
            mNoAlpha: false,
        }
    }

    fn cs_case(name: &str) -> Option<(SurfaceDescription, Vec<i32>)> {
        Some(match name {
            "cs_primary" => (desc(0, 7, 0, 640, 480, 0, 0, 16, false, false), vec![]),
            "cs_primary_3d" => (desc(0, 0, 0, 640, 480, 0, 0, 32, true, true), vec![]),
            "cs_offscreen" => (desc(1, 7, 0, 100, 50, 0, 0, 16, false, false), vec![]),
            "cs_offscreen_3d" => (desc(1, 0, 0, 100, 50, 0, 0, 32, true, false), vec![]),
            "cs_flip" => (desc(2, 0, 0, 640, 480, 2, 0, 32, false, false), vec![]),
            "cs_flip_3d" => (desc(2, 0, 0, 640, 480, 1, 0, 32, true, true), vec![]),
            "cs_zbuffer" => (desc(3, 11, 0, 640, 480, 0, 0, 16, true, false), vec![]),
            "cs_sysmem" => (desc(4, 7, 0, 320, 200, 0, 0, 16, false, false), vec![]),
            "cs_sysmem_3d" => (desc(4, 0, 0, 320, 200, 0, 0, 32, true, false), vec![]),
            "cs_texture_static" => (desc(5, 0, 1, 256, 256, 0, 0, 32, false, false), vec![]),
            "cs_texture_dynamic" => (desc(5, 0, 0, 256, 256, 0, 0, 32, false, false), vec![]),
            "cs_texture_3d_static" => (desc(5, 0, 1, 256, 256, 0, 0, 32, true, false), vec![]),
            "cs_texture_3d_dynamic" => (desc(5, 0, 0, 256, 256, 0, 0, 32, true, false), vec![]),
            "cs_mip" => (desc(6, 0, 1, 256, 256, 0, 4, 32, false, false), vec![]),
            "cs_mip_3d" => (desc(6, 0, 0, 64, 32, 0, 3, 32, true, true), vec![]),
            "cs_error" => (desc(1, 7, 0, 100, 50, 0, 0, 16, false, false), vec![0x95]),
            "cs_z_prefer16_ok" => (desc(3, 12, 0, 640, 480, 0, 0, 16, true, false), vec![]),
            "cs_z_prefer16_falls_to_24" => (desc(3, 12, 0, 640, 480, 0, 0, 16, true, false), vec![1, 0]),
            "cs_z_prefer16_falls_to_32" => (desc(3, 12, 0, 640, 480, 0, 0, 16, true, false), vec![1, 1, 0]),
            "cs_z_prefer16_all_fail" => (desc(3, 12, 0, 640, 480, 0, 0, 16, true, false), vec![1, 1, 1]),
            "cs_z_prefer32_ok" => (desc(3, 13, 0, 640, 480, 0, 0, 16, true, false), vec![]),
            "cs_z_prefer32_all_fail" => (desc(3, 13, 0, 640, 480, 0, 0, 16, true, false), vec![1, 1, 1]),
            _ => return None,
        })
    }

    // the capture scenarios: (name, to_clipboard, queued coop results, captured, first_attached, attached, initially full)
    #[allow(clippy::type_complexity)]
    fn cap_case(name: &str) -> Option<(bool, Vec<i32>, u32, bool, bool, bool)> {
        Some(match name {
            "cap_full_first_attached" => (true, vec![], 0, false, true, false),
            "cap_full_first_detached" => (true, vec![], 0, true, false, true),
            "cap_full_second" => (true, vec![], 1, true, true, true),
            "cap_full_second_not_allowed" => (true, vec![], 1, false, true, true),
            "cap_full_error_first_call" => (true, vec![0x96], 0, true, true, true),
            "cap_full_error_second_call" => (true, vec![0, 0x97], 0, true, true, true),
            "cap_window_first_attached" => (false, vec![], 0, true, true, true),
            "cap_window_first_detached" => (false, vec![], 0, true, false, false),
            "cap_window_second" => (false, vec![], 1, true, true, false),
            "cap_window_second_not_allowed" => (false, vec![], 1, false, true, false),
            "cap_window_error" => (false, vec![0x98], 0, false, true, false),
            _ => return None,
        })
    }

    fn other_cases(name: &str, gx: &mut Recorder, h: &mut RecHost) -> Option<(i32, Vec<String>)> {
        let mut extra = Vec::new();
        let result = match name {
            "release_ok" | "release_err" => {
                if name == "release_err" {
                    gx.queue("coop", &[0x99]);
                }
                let err = name == "release_err";
                let mut state = CaptureState { captured: if err { 1 } else { 2 }, first_attached: false };
                let mut full = !err;
                let r = release(gx, h, DD, 44 as H, &mut state, &mut full);
                extra.push(format!("rel captured={} full={}", state.captured, full as i32));
                r
            }
            "restore_ok" => restore_all(gx, h, DD),
            "restore_not_ready" => {
                gx.queue("test_coop", &[0x9a]);
                restore_all(gx, h, DD)
            }
            "restore_err" => {
                gx.queue("restore_all", &[0x9b]);
                restore_all(gx, h, DD)
            }
            "set_mode_windowed" | "set_mode_ok" | "set_mode_err" => {
                let modes = [(800u32, 600u32, 60u32), (1024, 768, 75), (800, 600, 85)];
                let (index, full, formats) = match name {
                    "set_mode_windowed" => (0usize, false, 7u32),
                    "set_mode_ok" => (0, true, 7),
                    _ => (2, true, 7),
                };
                if name == "set_mode_err" {
                    gx.queue("set_mode", &[0x9c]);
                }
                let m = modes[index];
                let vm = VideoMode {
                    mWidth: m.0 as c_ulong,
                    mHeight: m.1 as c_ulong,
                    mRefreshRate: m.2 as c_ulong,
                    mBitCount: 16,
                    mPixelFormat: formats as c_ulong,
                };
                let (r, update) = set_mode(gx, h, DD, full, &vm);
                let current = if update { index } else { 1 };
                extra.push(format!("current {current}"));
                r
            }
            "tex_cb_match" | "tex_cb_no_alpha" | "tex_cb_16" => {
                let mut f = argb_format();
                if name == "tex_cb_no_alpha" {
                    f.dwRGBAlphaBitMask = 0;
                }
                if name == "tex_cb_16" {
                    f.dwRGBBitCount = 16;
                }
                let found = is_argb8888(&f);
                extra.push(format!("found {}", found as i32));
                if found {
                    GXENUMRET_CANCEL
                } else {
                    GXENUMRET_OK
                }
            }
            _ => return None,
        };
        Some((result, extra))
    }

    #[test]
    fn matches_cpp_golden() {
        let sections = golden_sections(include_str!("golden_screen.txt"));
        let mut covered = 0;
        for section in &sections {
            let (mut gx, mut host) = Recorder::pair();
            let got_extra: Vec<String>;
            if cs_case(&section.name).is_some() {
                let (mut d, results) = cs_case(&section.name).unwrap();
                let mut fake = Fake::new(&gx);
                gx.queue("create_surface", &results);
                let mut out = NULL;
                let r = create_surface(&mut gx, &mut host, &mut fake, DD, &mut d, &mut out);
                assert_eq!(r, section.result, "{}", section.name);
                got_extra = vec![format!(
                    "desc type={} fmt={} depth={} surf={}",
                    d.mType,
                    d.mPixelFormat,
                    d.mPixelDepth,
                    if out.is_null() { "0000000000000000" } else { "0000000000000001" }
                )];
            } else if let Some((clip, coop, captured, first, attached, full)) = cap_case(&section.name) {
                gx.queue("coop", &coop);
                let mut state = CaptureState { captured, first_attached: first };
                let mut is_full = full;
                let r = capture(&mut gx, &mut host, DD, 44 as H, clip, attached, &mut state, &mut is_full);
                assert_eq!(r, section.result, "{}", section.name);
                got_extra = vec![format!(
                    "cap captured={} first={} full={}",
                    state.captured, state.first_attached as i32, is_full as i32
                )];
            } else if section.name.starts_with("ctor_") {
                let mut fake = Fake::new(&gx);
                ctor(&section.name, &mut gx, &mut host, &mut fake).unwrap();
                got_extra = {
                    let mut e: Vec<String> = fake
                        .modes
                        .iter()
                        .map(|m| {
                            format!(
                                "mode {} {} {} {} {}",
                                m.mWidth, m.mHeight, m.mRefreshRate, m.mBitCount, m.mPixelFormat
                            )
                        })
                        .collect();
                    e.push(format!("current {}", fake.current.unwrap()));
                    e.push(format!(
                        "flags min={} mag={} dev32={} tex32={} full={}",
                        fake.min_filter as i32,
                        fake.mag_filter as i32,
                        fake.dev32 as i32,
                        fake.tex32 as i32,
                        fake.full as i32
                    ));
                    e
                };
                assert_eq!(0, section.result, "{}", section.name);
            } else if let Some((result, e)) = other_cases(&section.name, &mut gx, &mut host) {
                assert_eq!(result, section.result, "{}", section.name);
                got_extra = e;
            } else {
                panic!("unknown section {}", section.name);
            }
            let got: Vec<String> = gx.log.borrow().clone();
            assert_eq!(got, section.trace, "trace of {}", section.name);
            let want_extra: Vec<&String> =
                section.extra.iter().filter(|e| !e.starts_with("emode") && !e.starts_with("deviceformats")).collect();
            let got_ref: Vec<&String> = got_extra.iter().collect();
            assert_eq!(got_ref, want_extra, "extra of {}", section.name);
            covered += 1;
        }
        assert_eq!(covered, sections.len());
    }

    #[test]
    fn video_mode_table_matches_cpp_callback() {
        // the "emode" lines of golden_screen.txt (the real EnumVideoModeCallback)
        let cases = [
            (sd(640, 480, 16, 60, 0xf800, 0x7e0, 0x1f, 0), 7),
            (sd(640, 480, 16, 60, 0xf800, 0x7c0, 0x3e, 1), 4),
            (sd(640, 480, 32, 60, 0xff0000, 0xff00, 0xff, 0xff000000), 0),
            (sd(640, 480, 8, 60, 1, 2, 3, 4), 14),
        ];
        for (desc, format) in cases {
            assert_eq!(video_mode_of(&desc).mPixelFormat as u32, format);
        }
    }

    #[test]
    fn pixel_format_table_matches_cpp_golden() {
        let text = include_str!("golden_screen.txt");
        let line = text.lines().find(|l| l.starts_with("deviceformats")).unwrap();
        let mut rows = line.split(' ').skip(1);
        for p in &PIXEL_FORMATS {
            let row: Vec<&str> = rows.next().unwrap().split(',').collect();
            let hexes = |s: &str| u32::from_str_radix(s, 16).unwrap();
            assert_eq!(p.dwFlags, hexes(row[0]));
            assert_eq!(p.dwFourCC, hexes(row[1]));
            assert_eq!(p.dwRGBBitCount.to_string(), row[2]);
            assert_eq!(p.dwRBitMask, hexes(row[3]));
            assert_eq!(p.dwGBitMask, hexes(row[4]));
            assert_eq!(p.dwBBitMask, hexes(row[5]));
            assert_eq!(p.dwRGBAlphaBitMask, hexes(row[6]));
        }
    }

    #[test]
    fn create_window_style_mapping() {
        let plain = |f| WindowParams { create_at: false, x: 0, y: 0, width: 800, height: 600, flags: f };
        assert_eq!(window_params(true, 0, 800, 600, 1920, 1080, 0.0, 0.0), plain(0x14));
        assert_eq!(window_params(false, 1, 800, 600, 1920, 1080, 0.0, 0.0), plain(1));
        assert_eq!(window_params(false, 2, 800, 600, 1920, 1080, 0.0, 0.0), plain(0x14));
        assert_eq!(
            window_params(false, 0, 1920, 1080, 1920, 1080, 0.0, 0.0),
            WindowParams { create_at: false, x: 0, y: 0, width: 1920, height: 1080, flags: 2 }
        );
        assert_eq!(
            window_params(false, 0, 800, 600, 1920, 1080, 3.4, 4.6),
            WindowParams { create_at: true, x: 3, y: 5, width: 800, height: 600, flags: 0x30 }
        );
        // the display's size is the window's in the full screen branch (a larger one is clipped by the platform)
        assert_eq!(window_params(false, 0, 2000, 2000, 1920, 1080, 0.0, 0.0).width, 1920);
    }
}
