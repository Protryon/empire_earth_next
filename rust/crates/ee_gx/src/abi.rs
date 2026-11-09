//! The C ABI's data: `#[repr(C)]` mirrors of `include/gx/GX.h`'s structures and the DirectX 7 constants GX uses.
//! `unsigned long` and `long` keep their C meaning (`c_ulong`/`c_long`, 32 bits on Windows and 64 on Unix); the rest of
//! the crate works in `u32`/`i32`, which is what these values are.

#![allow(non_snake_case)]

use std::ffi::{c_long, c_ulong, c_void};

pub type GxResult = c_long;

pub const GX_OK: GxResult = 0;
pub const GXERR_GENERIC: GxResult = 0x8000_4005u32 as GxResult;
pub const GXERR_INVALIDPARAMS: GxResult = 0x8007_0057u32 as GxResult;
pub const GXERR_OUTOFMEMORY: GxResult = 0x8007_000Eu32 as GxResult;
pub const GXERR_UNSUPPORTED: GxResult = 0x8000_4001u32 as GxResult;
pub const GXERR_NOTLOCKED: GxResult = (0x8876_0000u32 | 584) as GxResult;
pub const GXERR_NOTFOUND: GxResult = (0x8876_0000u32 | 255) as GxResult;
pub const GXENUMRET_CANCEL: GxResult = 0;
pub const GXENUMRET_OK: GxResult = 1;

pub const GXSD_CAPS: u32 = 0x0000_0001;
pub const GXSD_HEIGHT: u32 = 0x0000_0002;
pub const GXSD_WIDTH: u32 = 0x0000_0004;
pub const GXSD_PIXELFORMAT: u32 = 0x0000_1000;
pub const GXSD_CKSRCBLT: u32 = 0x0001_0000;
pub const GXSD_MIPMAPCOUNT: u32 = 0x0002_0000;

pub const GXSCAPS_FLIP: u32 = 0x0000_0010;
pub const GXSCAPS_PRIMARYSURFACE: u32 = 0x0000_0200;
pub const GXSCAPS_TEXTURE: u32 = 0x0000_1000;
pub const GXSCAPS_3DDEVICE: u32 = 0x0000_2000;
pub const GXSCAPS_ZBUFFER: u32 = 0x0002_0000;
pub const GXSCAPS_MIPMAP: u32 = 0x0040_0000;

pub const GXPF_ALPHAPIXELS: u32 = 0x0000_0001;
pub const GXPF_RGB: u32 = 0x0000_0040;
pub const GXPF_ZBUFFER: u32 = 0x0000_0400;

pub const GXBLT_COLORFILL: u32 = 0x0000_0400;
pub const GXBLT_KEYSRC: u32 = 0x0000_8000;
pub const GXFLIP_NOVSYNC: u32 = 0x0000_0008;
pub const GXLOCK_WRITEONLY: u32 = 0x0000_0020;
pub const GXLOCK_DISCARDCONTENTS: u32 = 0x0000_2000;

pub const GXCAPS_3D: u32 = 0x0000_0001;
pub const GXFXCAPS_BLTARITHSTRETCHY: u32 = 0x0000_0020;
pub const GXFXCAPS_BLTSHRINKX: u32 = 0x0000_0400;
pub const GXFXCAPS_BLTSHRINKY: u32 = 0x0000_1000;
pub const GXFXCAPS_BLTSTRETCHX: u32 = 0x0000_4000;
pub const GXFXCAPS_BLTSTRETCHY: u32 = 0x0001_0000;
pub const GXBD_16: u32 = 0x0000_0400;
pub const GXBD_32: u32 = 0x0000_0100;

pub const GXDEVCAPS_FLOATTLVERTEX: u32 = 0x0000_0001;
pub const GXDEVCAPS_HWTRANSFORMANDLIGHT: u32 = 0x0001_0000;
pub const GXDEVCAPS_HWRASTERIZATION: u32 = 0x0008_0000;
pub const GXFVF_XYZ: u32 = 0x002;
pub const GXFVF_NORMAL: u32 = 0x010;
pub const GXFVF_DIFFUSE: u32 = 0x040;
pub const GXFVF_TEX1: u32 = 0x100;
pub const GXCLEAR_TARGET: u32 = 1;
pub const GXCLEAR_ZBUFFER: u32 = 2;

pub const GXPT_POINTLIST: u32 = 1;
pub const GXPT_LINELIST: u32 = 2;
pub const GXPT_TRIANGLELIST: u32 = 4;
pub const GXTS_WORLD: u32 = 1;
pub const GXTS_VIEW: u32 = 2;
pub const GXTS_PROJECTION: u32 = 3;

pub const GXRS_ZENABLE: usize = 7;
pub const GXRS_FILLMODE: usize = 8;
pub const GXRS_SHADEMODE: usize = 9;
pub const GXRS_ZWRITEENABLE: usize = 14;
pub const GXRS_ALPHATESTENABLE: usize = 15;
pub const GXRS_SRCBLEND: usize = 19;
pub const GXRS_DESTBLEND: usize = 20;
pub const GXRS_CULLMODE: usize = 22;
pub const GXRS_ZFUNC: usize = 23;
pub const GXRS_ALPHAREF: usize = 24;
pub const GXRS_ALPHAFUNC: usize = 25;
pub const GXRS_ALPHABLENDENABLE: usize = 27;
pub const GXRS_FOGENABLE: usize = 28;
pub const GXRS_SPECULARENABLE: usize = 29;
pub const GXRS_FOGCOLOR: usize = 34;
pub const GXRS_FOGSTART: usize = 36;
pub const GXRS_FOGEND: usize = 37;
pub const GXRS_COLORKEYENABLE: usize = 41;
pub const GXRS_LIGHTING: usize = 137;
pub const GXRS_AMBIENT: usize = 139;
pub const GXRS_FOGVERTEXMODE: usize = 140;
pub const GXRS_COLORVERTEX: usize = 141;
pub const GXRS_DIFFUSEMATERIALSOURCE: usize = 145;
pub const GXRS_AMBIENTMATERIALSOURCE: usize = 147;
pub const GXRS_COUNT: usize = 160;

pub const GXTSS_COLOROP: usize = 1;
pub const GXTSS_COLORARG1: usize = 2;
pub const GXTSS_COLORARG2: usize = 3;
pub const GXTSS_ALPHAOP: usize = 4;
pub const GXTSS_ALPHAARG1: usize = 5;
pub const GXTSS_ALPHAARG2: usize = 6;
pub const GXTSS_ADDRESS: usize = 12;
pub const GXTSS_MAGFILTER: usize = 16;
pub const GXTSS_MINFILTER: usize = 17;
pub const GXTSS_MIPFILTER: usize = 18;
pub const GXTSS_MIPMAPLODBIAS: usize = 19;
pub const GXTSS_COUNT: usize = 20;

pub const GXTOP_DISABLE: u32 = 1;
pub const GXTOP_SELECTARG1: u32 = 2;
pub const GXTOP_MODULATE: u32 = 4;
pub const GXTA_CURRENT: u32 = 1;
pub const GXTA_TEXTURE: u32 = 2;

pub const GXBLEND_ZERO: u32 = 1;
pub const GXBLEND_ONE: u32 = 2;
pub const GXBLEND_SRCCOLOR: u32 = 3;
pub const GXBLEND_INVSRCCOLOR: u32 = 4;
pub const GXBLEND_SRCALPHA: u32 = 5;
pub const GXBLEND_INVSRCALPHA: u32 = 6;
pub const GXBLEND_DESTALPHA: u32 = 7;
pub const GXBLEND_INVDESTALPHA: u32 = 8;
pub const GXBLEND_DESTCOLOR: u32 = 9;
pub const GXBLEND_INVDESTCOLOR: u32 = 10;
pub const GXBLEND_SRCALPHASAT: u32 = 11;
pub const GXBLEND_BOTHSRCALPHA: u32 = 12;
pub const GXBLEND_BOTHINVSRCALPHA: u32 = 13;

pub const GXCMP_NEVER: u32 = 1;
pub const GXCMP_LESS: u32 = 2;
pub const GXCMP_EQUAL: u32 = 3;
pub const GXCMP_LESSEQUAL: u32 = 4;
pub const GXCMP_GREATER: u32 = 5;
pub const GXCMP_NOTEQUAL: u32 = 6;
pub const GXCMP_GREATEREQUAL: u32 = 7;
pub const GXCMP_ALWAYS: u32 = 8;

pub const GXCULL_CW: u32 = 2;
pub const GXCULL_CCW: u32 = 3;
pub const GXFILL_SOLID: u32 = 3;
pub const GXSHADE_FLAT: u32 = 1;
pub const GXSHADE_GOURAUD: u32 = 2;
pub const GXFOG_LINEAR: u32 = 3;
pub const GXMCS_MATERIAL: u32 = 0;
pub const GXMCS_COLOR1: u32 = 1;
pub const GXTADDRESS_WRAP: u32 = 1;
pub const GXTADDRESS_MIRROR: u32 = 2;
pub const GXTADDRESS_CLAMP: u32 = 3;
pub const GXTFG_POINT: u32 = 1;
pub const GXTFG_LINEAR: u32 = 2;
pub const GXTFN_POINT: u32 = 1;
pub const GXTFN_LINEAR: u32 = 2;
pub const GXTFP_NONE: u32 = 1;
pub const GXTFP_LINEAR: u32 = 3;

/// `GXIID_Direct3D7`.
pub const GXIID_DIRECT3D7: u32 = 1;

/// `RECT`: `LONG` is 32 bits everywhere (Windows' `long`, `platform/wintypes.h`'s `int32_t`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Rect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl Rect {
    pub fn new(left: i32, top: i32, right: i32, bottom: i32) -> Self {
        Rect { left, top, right, bottom }
    }

    /// The whole of a surface of this size.
    pub fn whole(width: u32, height: u32) -> Self {
        Rect::new(0, 0, width as i32, height as i32)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
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

impl GxPixelFormat {
    pub const fn rgb(bits: u32, r: u32, g: u32, b: u32) -> Self {
        Self::with_flags(GXPF_RGB, bits, r, g, b, 0)
    }

    pub const fn rgba(bits: u32, r: u32, g: u32, b: u32, a: u32) -> Self {
        Self::with_flags(GXPF_RGB | GXPF_ALPHAPIXELS, bits, r, g, b, a)
    }

    pub const fn with_flags(flags: u32, bits: u32, r: u32, g: u32, b: u32, a: u32) -> Self {
        GxPixelFormat {
            dwSize: std::mem::size_of::<GxPixelFormat>() as c_ulong,
            dwFlags: flags as c_ulong,
            dwFourCC: 0,
            dwRGBBitCount: bits as c_ulong,
            dwRBitMask: r as c_ulong,
            dwGBitMask: g as c_ulong,
            dwBBitMask: b as c_ulong,
            dwRGBAlphaBitMask: a as c_ulong,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GxSCaps {
    pub dwCaps: c_ulong,
    pub dwCaps2: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GxColorKey {
    pub dwColorSpaceLowValue: c_ulong,
    pub dwColorSpaceHighValue: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
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
    pub ddckCKSrcBlt: GxColorKey,
    pub ddpfPixelFormat: GxPixelFormat,
    pub ddsCaps: GxSCaps,
}

impl Default for GxSurfaceDesc {
    fn default() -> Self {
        // SAFETY: all zeroes is a valid value of every field (integers, a null pointer)
        unsafe { std::mem::zeroed() }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GxCaps {
    pub dwSize: c_ulong,
    pub dwCaps: c_ulong,
    pub dwFXCaps: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GxBltFx {
    pub dwSize: c_ulong,
    pub dwFillColor: c_ulong,
}

#[repr(C)]
pub struct GxDeviceIdentifier {
    pub szDescription: [std::ffi::c_char; 512],
    pub dwVendorId: c_ulong,
    pub dwDeviceId: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GxDeviceDesc {
    pub dwDevCaps: c_ulong,
    pub dwDeviceRenderBitDepth: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GxVertexBufferDesc {
    pub dwSize: c_ulong,
    pub dwCaps: c_ulong,
    pub dwFVF: c_ulong,
    pub dwNumVertices: c_ulong,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxViewport {
    pub dwX: c_ulong,
    pub dwY: c_ulong,
    pub dwWidth: c_ulong,
    pub dwHeight: c_ulong,
    pub dvMinZ: f32,
    pub dvMaxZ: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GxRect {
    pub x1: c_long,
    pub y1: c_long,
    pub x2: c_long,
    pub y2: c_long,
}

/// Row-major: `m[r * 4 + c]` is `_{r+1}{c+1}`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxMatrix {
    pub m: [f32; 16],
}

impl GxMatrix {
    pub fn identity() -> Self {
        let mut m = [0.0; 16];
        m[0] = 1.0;
        m[5] = 1.0;
        m[10] = 1.0;
        m[15] = 1.0;
        GxMatrix { m }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxColorValue {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxVector {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxMaterial {
    pub diffuse: GxColorValue,
    pub ambient: GxColorValue,
    pub specular: GxColorValue,
    pub emissive: GxColorValue,
    pub power: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GxLight {
    pub dltType: c_ulong,
    pub dcvDiffuse: GxColorValue,
    pub dcvSpecular: GxColorValue,
    pub dcvAmbient: GxColorValue,
    pub dvPosition: GxVector,
    pub dvDirection: GxVector,
    pub dvRange: f32,
    pub dvFalloff: f32,
    pub dvAttenuation0: f32,
    pub dvAttenuation1: f32,
    pub dvAttenuation2: f32,
    pub dvTheta: f32,
    pub dvPhi: f32,
}

/// `platform/PlatFont.h`'s `PlatFontTarget`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PlatFontTarget {
    pub mPixels: *mut c_void,
    pub mPitch: i32,
    pub mWidth: i32,
    pub mHeight: i32,
    pub mFlags: u32,
    pub mScale: f32,
}

/// `PLAT_FONT_TARGET_PREMULTIPLIED`.
pub const PLAT_FONT_TARGET_PREMULTIPLIED: u32 = 1;

/// `platform/PlatWindow.h`'s `PlatNativeWindow`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct NativeWindow {
    pub kind: i32,
    pub display: *mut c_void,
    pub window: *mut c_void,
}

impl Default for NativeWindow {
    fn default() -> Self {
        NativeWindow { kind: 0, display: std::ptr::null_mut(), window: std::ptr::null_mut() }
    }
}

pub const PLAT_NATIVE_WIN32: i32 = 1;
pub const PLAT_NATIVE_X11: i32 = 2;
pub const PLAT_NATIVE_WAYLAND: i32 = 3;
pub const PLAT_NATIVE_METAL: i32 = 4;

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    #[test]
    fn results_keep_their_c_bits() {
        assert_eq!(GXERR_GENERIC as u32, 0x8000_4005);
        assert_eq!(GXERR_NOTFOUND as u32, 0x8876_0000 | 255);
        assert_eq!(GXERR_NOTLOCKED as u32, 0x8876_0248);
        // `long`: negative on Windows, positive on 64-bit Unix (the C++ cast does the same)
        let expected_sign = if cfg!(windows) { -1 } else { 1 };
        assert_eq!(GXERR_UNSUPPORTED.signum(), expected_sign);
    }

    #[test]
    fn structure_layouts_match_the_header() {
        let ul = size_of::<c_ulong>();
        assert_eq!(size_of::<GxPixelFormat>(), 8 * ul);
        assert_eq!(offset_of!(GxPixelFormat, dwRBitMask), 4 * ul);
        assert_eq!(size_of::<GxCaps>(), 3 * ul);
        assert_eq!(size_of::<GxBltFx>(), 2 * ul);
        assert_eq!(size_of::<GxDeviceDesc>(), 2 * ul);
        assert_eq!(size_of::<GxVertexBufferDesc>(), 4 * ul);
        assert_eq!(size_of::<Rect>(), 16);
        assert_eq!(size_of::<GxRect>(), 4 * size_of::<c_long>());
        assert_eq!(size_of::<GxMatrix>(), 64);
        assert_eq!(size_of::<GxColorValue>(), 16);
        assert_eq!(size_of::<GxVector>(), 12);
        assert_eq!(size_of::<GxMaterial>(), 4 * 16 + 4);
        assert_eq!(offset_of!(GxSurfaceDesc, lPitch), 4 * ul);
        assert_eq!(offset_of!(GxSurfaceDesc, lpSurface), if ul == 4 { 32 } else { 64 });
        assert_eq!(offset_of!(GxSurfaceDesc, ddpfPixelFormat), offset_of!(GxSurfaceDesc, ddckCKSrcBlt) + 2 * ul);
        assert_eq!(offset_of!(GxViewport, dvMinZ), 4 * ul);
        assert_eq!(size_of::<PlatFontTarget>(), 32);
        assert_eq!(offset_of!(GxLight, dvRange) - ul.max(4), 16 * 3 + 12 * 2);
        assert_eq!(size_of::<GxDeviceIdentifier>(), 512 + 2 * ul);
    }

    #[test]
    fn identity_is_diagonal() {
        let m = GxMatrix::identity();
        assert_eq!((m.m[0], m.m[5], m.m[10], m.m[15]), (1.0, 1.0, 1.0, 1.0));
        assert_eq!(m.m[1], 0.0);
    }

    #[test]
    fn pixel_format_helpers_set_their_flags() {
        let f = GxPixelFormat::rgba(32, 0xff0000, 0xff00, 0xff, 0xff00_0000);
        assert_eq!(f.dwFlags as u32, GXPF_RGB | GXPF_ALPHAPIXELS);
        assert_eq!(f.dwSize as usize, size_of::<GxPixelFormat>());
        assert_eq!(GxPixelFormat::rgb(16, 0xf800, 0x7e0, 0x1f).dwFlags as u32, GXPF_RGB);
    }

    #[test]
    fn surface_desc_defaults_to_zero() {
        let d = GxSurfaceDesc::default();
        assert_eq!(d.dwSize, 0);
        assert!(d.lpSurface.is_null());
    }

    #[test]
    fn rect_whole_covers_the_surface() {
        assert_eq!(Rect::whole(640, 480), Rect::new(0, 0, 640, 480));
    }
}
