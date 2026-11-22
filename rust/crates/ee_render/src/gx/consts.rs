//! The DirectX 7 constants GX uses (include/gx/GX.h has them as macros and enums: not in the generated header).

use crate::gx::abi::GxResult;

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
