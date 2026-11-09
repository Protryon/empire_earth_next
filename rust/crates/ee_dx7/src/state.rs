//! The rasterizer's redundant-state filtering (`DX7Rasterizer::Set*`): GE states mapped to D3D render and texture
//! stage states, the caches that skip redundant device calls.
//!
//! The per-screen DX7 state ([`ScreenState`]) is the pinned record shared with the C++ `DX7RScreenState`
//! (`include/dx7/DX7Rasterizer.h`, `DX7Trampolines.cpp` pins size and offsets), which the C++ draw code still reads.
//! The GE-side value (`GERScreenState`) is C++-owned: a setter gets it as `current` and returns whether the C++ base
//! class (`GERasterizer::Set...`) must record the new value.
//!
//! Golden behaviour: every mapping and call sequence below is read off the C++ function named in its comment, and the
//! tests assert the exact device-call sequence.
#![allow(clippy::unnecessary_cast)] // c_ulong is u32 on Windows, u64 elsewhere

use core::ffi::{c_ulong, c_void};

use crate::vertex::{float_to_long, rgba};

pub(crate) const GX_OK: i32 = 0;

// GXRENDERSTATETYPE
pub(crate) const RS_ANTIALIAS: u32 = 2;
pub(crate) const RS_ZENABLE: u32 = 7;
pub(crate) const RS_FILLMODE: u32 = 8;
pub(crate) const RS_SHADEMODE: u32 = 9;
pub(crate) const RS_ZWRITEENABLE: u32 = 14;
pub(crate) const RS_ALPHATESTENABLE: u32 = 15;
pub(crate) const RS_SRCBLEND: u32 = 19;
pub(crate) const RS_DESTBLEND: u32 = 20;
pub(crate) const RS_CULLMODE: u32 = 22;
pub(crate) const RS_ZFUNC: u32 = 23;
pub(crate) const RS_ALPHAREF: u32 = 24;
pub(crate) const RS_ALPHAFUNC: u32 = 25;
pub(crate) const RS_DITHERENABLE: u32 = 26;
pub(crate) const RS_ALPHABLENDENABLE: u32 = 27;
pub(crate) const RS_FOGENABLE: u32 = 28;
pub(crate) const RS_SPECULARENABLE: u32 = 29;
pub(crate) const RS_FOGCOLOR: u32 = 34;
pub(crate) const RS_FOGSTART: u32 = 36;
pub(crate) const RS_FOGEND: u32 = 37;
pub(crate) const RS_COLORKEYENABLE: u32 = 41;
pub(crate) const RS_LIGHTING: u32 = 137;
pub(crate) const RS_AMBIENT: u32 = 139;
pub(crate) const RS_COLORVERTEX: u32 = 141;
pub(crate) const RS_DIFFUSEMATERIALSOURCE: u32 = 145;
pub(crate) const RS_AMBIENTMATERIALSOURCE: u32 = 147;

// GXTEXTURESTAGESTATETYPE
pub(crate) const TSS_COLOROP: u32 = 1;
pub(crate) const TSS_COLORARG1: u32 = 2;
pub(crate) const TSS_COLORARG2: u32 = 3;
pub(crate) const TSS_ALPHAOP: u32 = 4;
pub(crate) const TSS_ALPHAARG1: u32 = 5;
pub(crate) const TSS_ALPHAARG2: u32 = 6;
pub(crate) const TSS_ADDRESS: u32 = 12;
pub(crate) const TSS_MAGFILTER: u32 = 16;
pub(crate) const TSS_MINFILTER: u32 = 17;
pub(crate) const TSS_MIPFILTER: u32 = 18;
pub(crate) const TSS_MIPMAPLODBIAS: u32 = 19;

const TOP_DISABLE: u32 = 1;
const TOP_SELECTARG1: u32 = 2;
const TOP_SELECTARG2: u32 = 3;
const TOP_MODULATE: u32 = 4;
const TA_DIFFUSE: u32 = 0;
const TA_CURRENT: u32 = 1;
const TA_TEXTURE: u32 = 2;

const FOG_SHADE_FLAT: u32 = 1;
const FOG_SHADE_GOURAUD: u32 = 2;
const MCS_MATERIAL: u32 = 0;
const MCS_COLOR1: u32 = 1;
const ANTIALIAS_NONE: u32 = 0;
const ANTIALIAS_SORTINDEPENDENT: u32 = 2;

/// The GX device as the state setters use it. In the game: the `extern "C"` GX functions on a `GXDevice*` handle
/// (`crate::gx`); in the tests: a recorder.
pub trait Device {
    fn set_render_state(&mut self, state: u32, value: u32) -> i32;
    fn set_stage_state(&mut self, stage: u32, state: u32, value: u32) -> i32;
    fn set_texture(&mut self, stage: u32, surface: *mut c_void) -> i32;
    fn set_material_white(&mut self) -> i32;
}

/// `DX7RTextureStage`. `#[repr(C)]`, pinned by the C++ (`unsigned long` is `c_ulong`, as the C++ fields were).
#[repr(C)]
#[derive(Clone, Copy)]
#[allow(non_snake_case)]
pub struct TextureStageCache {
    pub mTexture: *mut c_void,
    pub mColorMode: c_ulong,
    pub mAlphaMode: c_ulong,
    pub mColorArgument1: c_ulong,
    pub mColorArgument2: c_ulong,
    pub mAlphaArgument1: c_ulong,
    pub mAlphaArgument2: c_ulong,
    pub mTextureAddressingMode: c_ulong,
    pub mMinFilterMode: c_ulong,
    pub mMagFilterMode: c_ulong,
    pub mMipFilterMode: c_ulong,
    pub mMipMapBias: f32,
}

/// `GXVIEWPORT`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
#[allow(non_snake_case)]
pub struct Viewport {
    pub dwX: c_ulong,
    pub dwY: c_ulong,
    pub dwWidth: c_ulong,
    pub dwHeight: c_ulong,
    pub dvMinZ: f32,
    pub dvMaxZ: f32,
}

/// `DX7RScreenState`. A pinned record shared with the C++ draw code (`DX7Trampolines.cpp` asserts the size and each
/// offset).
#[repr(C)]
#[derive(Clone, Copy)]
#[allow(non_snake_case)]
pub struct ScreenState {
    pub mViewport: Viewport,
    pub mDirect3D: *mut c_void,
    pub mDevice: *mut c_void,
    pub mScreen: *mut c_void,
    pub mVertexBuffer: *mut c_void,
    pub mVertexBufferPosition: c_ulong,
    pub mStage1ColorOperation: c_ulong,
    pub mStage: TextureStageCache,
    pub mBlendSource: c_ulong,
    pub mBlendDestination: c_ulong,
    pub mZEnable: c_ulong,
    pub mIs3DMode: bool,
    pub mViewportChanged: bool,
    pub mZWriteEnable: bool,
    pub mAlphaBlendEnable: bool,
    pub mCanModulateTextureAlpha: bool,
    pub mDirtyRectangleSupport: bool,
}

/// `kBlend*` -> `GXBLEND_*` (`SetBlending`'s switch): an out-of-range value is sent as 0, as the C++ default was.
pub fn blend_value(ge: c_ulong) -> u32 {
    match ge {
        0 => 1,   // GXBLEND_ZERO
        1 => 2,   // ONE
        2 => 3,   // SRCCOLOR
        3 => 4,   // INVSRCCOLOR
        4 => 5,   // SRCALPHA
        5 => 6,   // INVSRCALPHA
        6 => 7,   // DESTALPHA
        7 => 8,   // INVDESTALPHA
        8 => 9,   // DESTCOLOR
        9 => 10,  // INVDESTCOLOR
        10 => 11, // SRCALPHASAT
        11 => 12, // BOTHSRCALPHA
        12 => 13, // BOTHINVSRCALPHA
        _ => 0,
    }
}

/// `kCompare*` -> `GXCMP_*` (`SetAlphaTestingCompareState`, `SetZCompare`): out-of-range -> 0, as the C++.
pub fn compare_value(ge: c_ulong) -> u32 {
    match ge {
        0 => 1, // NEVER
        1 => 2, // LESS
        2 => 4, // LESSEQUAL
        3 => 3, // EQUAL
        4 => 6, // NOTEQUAL
        5 => 7, // GREATEREQUAL
        6 => 5, // GREATER
        7 => 8, // ALWAYS
        _ => 0,
    }
}

/// `SetAlphaTesting`: the device value is `inState` itself (0/1 by contract).
pub fn set_alpha_testing(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_ALPHATESTENABLE, in_state as u32) == GX_OK
}

/// `SetAlphaTestingCompareState`.
pub fn set_alpha_testing_compare_state(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_ALPHAFUNC, compare_value(in_state)) == GX_OK
}

/// `SetAlphaTestingRefValue`: value (0..1) scaled to 0..255 by `(long)(v * 255.0f)` (truncation toward zero).
pub fn set_alpha_testing_ref_value(d: &mut impl Device, value: f32, current: f32) -> bool {
    if value == current {
        return false;
    }
    d.set_render_state(RS_ALPHAREF, (value * 255.0) as i32 as u32) == GX_OK
}

/// `SetAntiAliasing`: 1 -> sort-independent, anything else -> none.
pub fn set_anti_aliasing(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    let mode = if in_state == 1 { ANTIALIAS_SORTINDEPENDENT } else { ANTIALIAS_NONE };
    d.set_render_state(RS_ANTIALIAS, mode) == GX_OK
}

/// `SetBlending`: sends alpha-blend enable and the two factors as they change; the DX7-side caches in `state`.
/// `current_*` are the GE-side values. Returns whether the base class must record (false when both match).
pub fn set_blending(
    state: &mut ScreenState,
    d: &mut impl Device,
    in_source: c_ulong,
    in_destination: c_ulong,
    current_source: c_ulong,
    current_destination: c_ulong,
) -> bool {
    if in_source == current_source && in_destination == current_destination {
        return false;
    }
    let enable = !(in_source == 1 /*kBlendOne*/ && in_destination == 0/*kBlendZero*/);
    if state.mAlphaBlendEnable != enable {
        d.set_render_state(RS_ALPHABLENDENABLE, enable as u32);
        state.mAlphaBlendEnable = enable;
    }
    if state.mBlendSource != in_source {
        d.set_render_state(RS_SRCBLEND, blend_value(in_source));
        state.mBlendSource = in_source;
    }
    if state.mBlendDestination != in_destination {
        d.set_render_state(RS_DESTBLEND, blend_value(in_destination));
        state.mBlendDestination = in_destination;
    }
    true
}

/// `SetColorKeying`.
pub fn set_color_keying(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_COLORKEYENABLE, (in_state == 1) as u32) == GX_OK
}

/// `SetCulling`'s mapping: 1 -> CW, 2 -> CCW, anything else -> none.
pub fn cull_value(in_state: c_ulong) -> u32 {
    match in_state {
        1 => 2, // GXCULL_CW
        2 => 3, // GXCULL_CCW
        _ => 1, // GXCULL_NONE
    }
}

/// `SetCulling`'s per-device call (the C++ iterates its screens' devices).
pub fn set_cull_mode(d: &mut impl Device, in_state: c_ulong) -> i32 {
    d.set_render_state(RS_CULLMODE, cull_value(in_state))
}

/// `SetDithering`.
pub fn set_dithering(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_DITHERENABLE, (in_state == 1) as u32) == GX_OK
}

/// `SetDrawStyle`: 0 solid, 1 wireframe, 2 point; out of range sends 0 (the C++ default).
pub fn set_draw_style(d: &mut impl Device, in_style: c_ulong, current: c_ulong) -> bool {
    if in_style == current {
        return false;
    }
    let fill = match in_style {
        0 => 3, // GXFILL_SOLID
        1 => 2, // GXFILL_WIREFRAME
        2 => 1, // GXFILL_POINT
        _ => 0,
    };
    d.set_render_state(RS_FILLMODE, fill) == GX_OK
}

/// `SetFog`.
pub fn set_fog(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_FOGENABLE, (in_state == 1) as u32) == GX_OK
}

/// `SetFogColor`: `GXRGB(r, g, b)`: opaque, each component `(unsigned long)(c * 255)` truncated.
pub fn set_fog_color(d: &mut impl Device, color: [f32; 3], current_equal: bool) -> bool {
    if current_equal {
        return false;
    }
    let value = 0xff00_0000u32
        | (((color[0] * 255.0) as i32 as u32) << 16)
        | (((color[1] * 255.0) as i32 as u32) << 8)
        | ((color[2] * 255.0) as i32 as u32);
    d.set_render_state(RS_FOGCOLOR, value) == GX_OK
}

/// `SetFogRange`: each distance sent only when it differs, as its IEEE bits. The base class records both always.
pub fn set_fog_range(d: &mut impl Device, start: f32, end: f32, current_start: f32, current_end: f32) {
    if current_start != start {
        d.set_render_state(RS_FOGSTART, start.to_bits());
    }
    if current_end != end {
        d.set_render_state(RS_FOGEND, end.to_bits());
    }
}

/// `SetLighting`: the base class records always, except the early return (when unchanged).
pub fn set_lighting(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_LIGHTING, (in_state == 1) as u32);
    true
}

/// `SetShading`: 1 -> Gouraud, anything else -> flat.
pub fn set_shading(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_SHADEMODE, if in_state == 1 { FOG_SHADE_GOURAUD } else { FOG_SHADE_FLAT }) == GX_OK
}

/// `SetShadingColorSource`: 0 -> lighting colours from the material; otherwise from the vertex colours (and a white
/// material).
pub fn set_shading_color_source(d: &mut impl Device, in_source: c_ulong, current: c_ulong) -> bool {
    if in_source == current {
        return false;
    }
    let source = if in_source == 0 {
        d.set_render_state(RS_COLORVERTEX, 0);
        MCS_MATERIAL
    } else {
        d.set_material_white();
        d.set_render_state(RS_COLORVERTEX, 1);
        MCS_COLOR1
    };
    d.set_render_state(RS_AMBIENTMATERIALSOURCE, source);
    d.set_render_state(RS_DIFFUSEMATERIALSOURCE, source) == GX_OK
}

/// `SetSpecular`.
pub fn set_specular(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_SPECULARENABLE, (in_state == 1) as u32) == GX_OK
}

/// `SetZBuffer`: test on when either bit set, write on with bit 0; sends only what changed in the DX7 cache. Returns
/// whether the base class records (false only when unchanged in GE).
pub fn set_z_buffer(state: &mut ScreenState, d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    let write = (in_state & 1) != 0;
    if in_state == current {
        return false;
    }
    let enable: c_ulong = if (in_state & 2) == 0 && !write { 0 } else { 1 };
    if state.mZEnable != enable {
        d.set_render_state(RS_ZENABLE, enable as u32);
        state.mZEnable = enable;
    }
    if state.mZWriteEnable != write {
        d.set_render_state(RS_ZWRITEENABLE, write as u32);
        state.mZWriteEnable = write;
    }
    true
}

/// `SetZCompare`.
pub fn set_z_compare(d: &mut impl Device, in_state: c_ulong, current: c_ulong) -> bool {
    if in_state == current {
        return false;
    }
    d.set_render_state(RS_ZFUNC, compare_value(in_state)) == GX_OK
}

/// `SetAmbientColor`/`SetAmbientIntensity`'s device value: `RGBA(lrintf(c * 255), ..., lrintf(255))`.
pub fn ambient_value(computed: [f32; 3]) -> u32 {
    rgba(
        float_to_long(computed[0] * 255.0),
        float_to_long(computed[1] * 255.0),
        float_to_long(computed[2] * 255.0),
        float_to_long(255.0),
    )
}

/// `SetAmbientColor`/`SetAmbientIntensity`'s device call.
pub fn set_ambient(d: &mut impl Device, computed: [f32; 3]) -> i32 {
    d.set_render_state(RS_AMBIENT, ambient_value(computed))
}

/// `SetDebugColor`'s value: opaque, unclamped.
pub fn debug_color(color: [f32; 3]) -> u32 {
    ambient_value(color)
}

/// The texture stage plain data `SetTexture` reads through `inTexture->GetStage()` and `GetFrameSurface()`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
#[allow(non_snake_case)]
pub struct StageView {
    pub surface: *mut c_void,
    pub mColorMode: c_ulong,
    pub mAlphaMode: c_ulong,
    pub mTextureAddressingMode: c_ulong,
    pub mMinFilterMode: c_ulong,
    pub mMagFilterMode: c_ulong,
    pub mMipFilterMode: c_ulong,
    pub mMipMapBias: f32,
}

/// `SetTexture`'s colour-operation decode of `kStageOp*`: (arg1, arg2, op); unknown -> all 0 (the C++ locals' zeros).
fn stage_op(mode: c_ulong) -> (u32, u32, u32) {
    match mode {
        0 => (TA_TEXTURE, TA_DIFFUSE, TOP_SELECTARG2), // kStageOpDiffuse
        1 => (TA_TEXTURE, TA_DIFFUSE, TOP_SELECTARG1), // kStageOpTexture
        2 => (TA_TEXTURE, TA_DIFFUSE, TOP_MODULATE),   // kStageOpModulate
        3 => (TA_TEXTURE, TA_CURRENT, TOP_MODULATE),   // kStageOpModulateCurrent
        _ => (0, 0, 0),
    }
}

/// `SetTexture`'s NULL path: texturing off on stage 0.
pub fn clear_texture(state: &mut ScreenState, d: &mut impl Device) {
    d.set_texture(0, core::ptr::null_mut());
    d.set_stage_state(0, TSS_COLOROP, TOP_DISABLE);
    d.set_stage_state(0, TSS_ALPHAOP, TOP_DISABLE);
    state.mStage1ColorOperation = 1;
    state.mStage.mTexture = core::ptr::null_mut();
    state.mStage.mColorMode = 4;
    state.mStage.mAlphaMode = 4;
}

/// `SetTexture`'s non-NULL path after the reference counting: the stage-0 diff against [`TextureStageCache`] and
/// stage 1 off. `previous` is the C++ `theCurrent` copy of the cache taken before any write (the C++ compares against
/// it). The argument comparison happens only inside "mode changed", and uses the previous argument cache.
pub fn apply_texture_stage(state: &mut ScreenState, d: &mut impl Device, view: &StageView) {
    let previous = state.mStage;
    if view.surface != previous.mTexture {
        d.set_texture(0, view.surface);
        state.mStage.mTexture = view.surface;
    }
    if view.mColorMode != previous.mColorMode {
        let (arg1, arg2, op) = stage_op(view.mColorMode);
        d.set_stage_state(0, TSS_COLOROP, op);
        state.mStage.mColorMode = view.mColorMode;
        if arg1 as c_ulong != previous.mColorArgument1 {
            d.set_stage_state(0, TSS_COLORARG1, arg1);
            state.mStage.mColorArgument1 = arg1 as c_ulong;
        }
        if arg2 as c_ulong != previous.mColorArgument2 {
            d.set_stage_state(0, TSS_COLORARG2, arg2);
            state.mStage.mColorArgument2 = arg2 as c_ulong;
        }
    }
    if view.mAlphaMode != previous.mAlphaMode {
        let (arg1, arg2, op) = stage_op(view.mAlphaMode);
        d.set_stage_state(0, TSS_ALPHAOP, op);
        state.mStage.mAlphaMode = view.mAlphaMode;
        if arg1 as c_ulong != previous.mAlphaArgument1 {
            d.set_stage_state(0, TSS_ALPHAARG1, arg1);
            state.mStage.mAlphaArgument1 = arg1 as c_ulong;
        }
        if arg2 as c_ulong != previous.mAlphaArgument2 {
            d.set_stage_state(0, TSS_ALPHAARG2, arg2);
            state.mStage.mAlphaArgument2 = arg2 as c_ulong;
        }
    }
    if view.mTextureAddressingMode != previous.mTextureAddressingMode {
        let value = match view.mTextureAddressingMode {
            0 => 3, // kAddressingClamp -> GXTADDRESS_CLAMP
            1 => 2, // kAddressingMirror
            2 => 1, // kAddressingWrap
            _ => 0,
        };
        d.set_stage_state(0, TSS_ADDRESS, value);
        state.mStage.mTextureAddressingMode = view.mTextureAddressingMode;
    }
    if view.mMinFilterMode != previous.mMinFilterMode {
        let value = match view.mMinFilterMode {
            0 => 1, // POINT
            1 => 2, // LINEAR
            2 => 3, // ANISOTROPIC
            _ => 0,
        };
        d.set_stage_state(0, TSS_MINFILTER, value);
        state.mStage.mMinFilterMode = view.mMinFilterMode;
    }
    if view.mMagFilterMode != previous.mMagFilterMode {
        let value = match view.mMagFilterMode {
            0 => 1, // POINT
            1 => 2, // LINEAR
            2 => 3, // FLATCUBIC
            _ => 0,
        };
        d.set_stage_state(0, TSS_MAGFILTER, value);
        state.mStage.mMagFilterMode = view.mMagFilterMode;
    }
    if view.mMipFilterMode != previous.mMipFilterMode {
        let value = match view.mMipFilterMode {
            0 => 1, // NONE
            1 => 2, // POINT
            2 => 3, // LINEAR
            _ => 0,
        };
        d.set_stage_state(0, TSS_MIPFILTER, value);
        state.mStage.mMipFilterMode = view.mMipFilterMode;
    }
    if view.mMipMapBias != previous.mMipMapBias {
        d.set_stage_state(0, TSS_MIPMAPLODBIAS, view.mMipMapBias.to_bits());
        state.mStage.mMipMapBias = view.mMipMapBias;
    }
    d.set_texture(1, core::ptr::null_mut());
    d.set_stage_state(1, TSS_COLOROP, TOP_DISABLE);
    d.set_stage_state(1, TSS_ALPHAOP, TOP_DISABLE);
    state.mStage1ColorOperation = 1;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Fake {
        calls: Vec<(char, u32, u32, u32)>,
        fail_render_state: bool,
    }

    impl Device for Fake {
        fn set_render_state(&mut self, state: u32, value: u32) -> i32 {
            self.calls.push(('R', state, value, 0));
            i32::from(self.fail_render_state)
        }
        fn set_stage_state(&mut self, stage: u32, state: u32, value: u32) -> i32 {
            self.calls.push(('T', stage, state, value));
            0
        }
        fn set_texture(&mut self, stage: u32, surface: *mut c_void) -> i32 {
            self.calls.push(('X', stage, surface as usize as u32, 0));
            0
        }
        fn set_material_white(&mut self) -> i32 {
            self.calls.push(('M', 0, 0, 0));
            0
        }
    }

    fn state() -> ScreenState {
        ScreenState {
            mViewport: Viewport::default(),
            mDirect3D: core::ptr::null_mut(),
            mDevice: core::ptr::null_mut(),
            mScreen: core::ptr::null_mut(),
            mVertexBuffer: core::ptr::null_mut(),
            mVertexBufferPosition: 0,
            mStage1ColorOperation: 1,
            mStage: TextureStageCache {
                mTexture: core::ptr::null_mut(),
                mColorMode: 4,
                mAlphaMode: 4,
                mColorArgument1: !0,
                mColorArgument2: !0,
                mAlphaArgument1: !0,
                mAlphaArgument2: !0,
                mTextureAddressingMode: 3,
                mMinFilterMode: 3,
                mMagFilterMode: 3,
                mMipFilterMode: 3,
                mMipMapBias: f32::MAX,
            },
            mBlendSource: 0xFFFF_FFF0,
            mBlendDestination: 0xFFFF_FFF0,
            mZEnable: 1,
            mIs3DMode: false,
            mViewportChanged: true,
            mZWriteEnable: true,
            mAlphaBlendEnable: true,
            mCanModulateTextureAlpha: true,
            mDirtyRectangleSupport: true,
        }
    }

    #[test]
    fn blend_mapping_golden() {
        for (ge, value) in [
            (0, 1),
            (1, 2),
            (2, 3),
            (3, 4),
            (4, 5),
            (5, 6),
            (6, 7),
            (7, 8),
            (8, 9),
            (9, 10),
            (10, 11),
            (11, 12),
            (12, 13),
            (13, 0),
        ] {
            assert_eq!(blend_value(ge), value, "kBlend {ge}");
        }
    }

    #[test]
    fn compare_mapping_golden() {
        for (ge, value) in [(0, 1), (1, 2), (2, 4), (3, 3), (4, 6), (5, 7), (6, 5), (7, 8), (8, 0)] {
            assert_eq!(compare_value(ge), value, "kCompare {ge}");
        }
    }

    #[test]
    fn set_blending_call_sequence_and_cache() {
        let mut s = state();
        let mut d = Fake::default();
        // ge state src=-16,dst=-16 (forced); 1/0 turns blending off
        assert!(set_blending(&mut s, &mut d, 1, 0, 0xFFFF_FFF0, 0xFFFF_FFF0));
        assert_eq!(d.calls, [('R', RS_ALPHABLENDENABLE, 0, 0), ('R', RS_SRCBLEND, 2, 0), ('R', RS_DESTBLEND, 1, 0)]);
        assert!(!s.mAlphaBlendEnable);
        // the C++ early return when both match the GE values
        d.calls.clear();
        assert!(!set_blending(&mut s, &mut d, 1, 0, 1, 0));
        assert!(d.calls.is_empty());
        // only the changed factor is sent (DX7 cache), enable sent once on change
        assert!(set_blending(&mut s, &mut d, 4, 0, 1, 0));
        assert_eq!(d.calls, [('R', RS_ALPHABLENDENABLE, 1, 0), ('R', RS_SRCBLEND, 5, 0)]);
    }

    #[test]
    fn set_z_buffer_write_implies_enable_golden() {
        let mut s = state();
        let mut d = Fake::default();
        assert!(set_z_buffer(&mut s, &mut d, 0, 1));
        // state 0: ZENABLE 0, ZWRITEENABLE false (write changed from true)
        assert_eq!(d.calls, [('R', RS_ZENABLE, 0, 0), ('R', RS_ZWRITEENABLE, 0, 0)]);
        d.calls.clear();
        assert!(set_z_buffer(&mut s, &mut d, 1, 0)); // write only: ZENABLE 1 (the C++ `if test<=0 && !write`)
        assert_eq!(d.calls, [('R', RS_ZENABLE, 1, 0), ('R', RS_ZWRITEENABLE, 1, 0)]);
        assert!(!set_z_buffer(&mut s, &mut Fake::default(), 1, 1));
    }

    #[test]
    fn simple_setters_early_out_and_report_device_failure() {
        let mut d = Fake::default();
        assert!(!set_fog(&mut d, 1, 1));
        assert!(d.calls.is_empty());
        assert!(set_fog(&mut d, 1, 0));
        assert_eq!(d.calls.pop().unwrap(), ('R', RS_FOGENABLE, 1, 0));
        d.fail_render_state = true;
        assert!(!set_fog(&mut d, 0, 1), "device failure -> not recorded");
        d.calls.clear();
        assert!(!set_alpha_testing_ref_value(&mut d, 0.5, 0.5));
        assert!(!set_alpha_testing_ref_value(&mut d, 1.0, 0.5));
        assert_eq!(d.calls.pop().unwrap(), ('R', RS_ALPHAREF, 255, 0));
        assert!(!d.calls.iter().any(|c| c.2 == 127));
    }

    #[test]
    fn set_alpha_ref_truncates() {
        let mut d = Fake::default();
        set_alpha_testing_ref_value(&mut d, 0.5, 0.0);
        assert_eq!(d.calls[0], ('R', RS_ALPHAREF, 127, 0));
        d.calls.clear();
        set_alpha_testing_ref_value(&mut d, -0.5, 0.0);
        assert_eq!(d.calls[0].2, (-127i32) as u32);
    }

    #[test]
    fn shading_color_source_sequences() {
        let mut d = Fake::default();
        assert!(set_shading_color_source(&mut d, 0, 2));
        assert_eq!(
            d.calls,
            [('R', RS_COLORVERTEX, 0, 0), ('R', RS_AMBIENTMATERIALSOURCE, 0, 0), ('R', RS_DIFFUSEMATERIALSOURCE, 0, 0)]
        );
        d.calls.clear();
        assert!(set_shading_color_source(&mut d, 1, 0));
        assert_eq!(
            d.calls,
            [
                ('M', 0, 0, 0),
                ('R', RS_COLORVERTEX, 1, 0),
                ('R', RS_AMBIENTMATERIALSOURCE, 1, 0),
                ('R', RS_DIFFUSEMATERIALSOURCE, 1, 0)
            ]
        );
    }

    #[test]
    fn fog_range_only_sends_changes_as_bits() {
        let mut d = Fake::default();
        set_fog_range(&mut d, 1.5, 2.5, 1.5, 0.0);
        assert_eq!(d.calls, [('R', RS_FOGEND, 2.5f32.to_bits(), 0)]);
    }

    #[test]
    fn cull_and_ambient_device_calls() {
        let mut d = Fake::default();
        set_cull_mode(&mut d, 1);
        set_cull_mode(&mut d, 7);
        set_ambient(&mut d, [1.0, 0.5, 0.25]);
        assert_eq!(d.calls, [('R', RS_CULLMODE, 2, 0), ('R', RS_CULLMODE, 1, 0), ('R', RS_AMBIENT, 0xffff_8040, 0)]);
    }

    #[test]
    fn ambient_value_golden() {
        // RGBA(lrintf(1*255), lrintf(.5*255)=128, lrintf(.25*255)=64, 255)
        assert_eq!(ambient_value([1.0, 0.5, 0.25]), 0xffff_8040);
    }

    fn view() -> StageView {
        StageView {
            surface: 0x2000 as *mut c_void,
            mColorMode: 0,
            mAlphaMode: 1,
            mTextureAddressingMode: 2,
            mMinFilterMode: 1,
            mMagFilterMode: 1,
            mMipFilterMode: 2,
            mMipMapBias: 1.5,
        }
    }

    #[test]
    fn apply_texture_stage_full_diff_sequence() {
        let mut s = state();
        let mut d = Fake::default();
        let v = view();
        apply_texture_stage(&mut s, &mut d, &v);
        let bias = 1.5f32.to_bits();
        assert_eq!(
            d.calls,
            [
                ('X', 0, 0x2000, 0),
                ('T', 0, TSS_COLOROP, 3), // SELECTARG2
                ('T', 0, TSS_COLORARG1, 2),
                ('T', 0, TSS_COLORARG2, 0),
                ('T', 0, TSS_ALPHAOP, 2), // SELECTARG1
                ('T', 0, TSS_ALPHAARG1, 2),
                ('T', 0, TSS_ALPHAARG2, 0),
                ('T', 0, TSS_ADDRESS, 1), // wrap
                ('T', 0, TSS_MINFILTER, 2),
                ('T', 0, TSS_MAGFILTER, 2),
                ('T', 0, TSS_MIPFILTER, 3),
                ('T', 0, TSS_MIPMAPLODBIAS, bias),
                ('X', 1, 0, 0),
                ('T', 1, TSS_COLOROP, 1),
                ('T', 1, TSS_ALPHAOP, 1),
            ]
        );
        assert_eq!(s.mStage.mColorMode, 0);
        assert_eq!(s.mStage.mColorArgument1, 2);
        assert_eq!(s.mStage.mMipMapBias, 1.5);
        assert_eq!(s.mStage1ColorOperation, 1);

        // the second call with the same view: only stage 1 off
        d.calls.clear();
        apply_texture_stage(&mut s, &mut d, &v);
        assert_eq!(d.calls, [('X', 1, 0, 0), ('T', 1, TSS_COLOROP, 1), ('T', 1, TSS_ALPHAOP, 1)]);

        // a changed colour mode re-sends only the op and changed arguments, from the cache of before
        d.calls.clear();
        let mut v2 = v;
        v2.mColorMode = 3; // ModulateCurrent: args TEXTURE, CURRENT
        apply_texture_stage(&mut s, &mut d, &v2);
        assert_eq!(d.calls[0], ('T', 0, TSS_COLOROP, 4));
        assert_eq!(d.calls[1], ('T', 0, TSS_COLORARG2, 1));
    }

    #[test]
    fn clear_texture_sequence() {
        let mut s = state();
        let mut d = Fake::default();
        clear_texture(&mut s, &mut d);
        assert_eq!(d.calls, [('X', 0, 0, 0), ('T', 0, TSS_COLOROP, 1), ('T', 0, TSS_ALPHAOP, 1)]);
        assert_eq!(s.mStage.mColorMode, 4);
    }
}
