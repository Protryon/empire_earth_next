//! The terrain's render object (`src/Empire Earth/Graphics Thread/EEROTerrain.cpp`, vtable 0x441b7c): its media
//! (`CreateSkyModel` 0x512e7f with `CreateStarModels` 0x5126ce and `CreateCloudModel` 0x512bc4; `ReleaseSkyModels`
//! 0x5138d2), the frame's sky and clouds (`FrameStarting` 0x512638's `UpdateSkyColors` 0x513ebd and
//! `UpdateCloudLayer` 0x51396f) and the scene pass (`RenderScene`, 0x5131a4): the sky (or the stars), the clouds,
//! the terrain, the highlights, the grid, the lines and the effects' terrain pass. The models and textures are made,
//! drawn and released through the raster table; the terrain mesh's draws come from the C++ (`EeGfxTerrainScene`).
//!
//! Drawn only (the renderer's thread).

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use ee_base::math::types::Ge3DPoint;

use crate::gfx::effects::{EeGfxColor, EeGfxTexCoord};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::mesh::{
    EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COLOR, EE_GFX_MESH_COORD, EE_GFX_MESH_OPACITY, EE_GFX_MESH_POINT,
};
use crate::gfx::raster::{
    EeGfxModel, EeGfxTextureLoad, Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, BLEND_ZERO, COMPARE_LESS,
    COMPARE_LESS_EQUAL, STAGE_OP_MODULATE, ZBUFFER_TEST_AND_WRITE, ZBUFFER_TEST_ONLY,
};
use crate::gfx::terrain::{cloud_layer, sky_frame};

/// `kHighlightOpacity` (0x441418): the editor brush's highlight.
const HIGHLIGHT_OPACITY: f32 = 0.5;
/// `kEEROTerrainStarU` (0x441428): the star texture's repeat.
const STAR_U: f32 = 6.0;
/// `kSkyRearClipping` (0x441b94): the rear clipping distance while the sky is drawn.
const SKY_REAR_CLIPPING: f32 = 20000.0;
/// `kCullNone`, `kCullCCW`; `kZBufferDisabled`.
const CULL_NONE: u32 = 0;
const CULL_CCW: u32 = 2;
const ZBUFFER_DISABLED: u32 = 0;
/// `kStageOpTexture`; `kMipFilterNone`, `kMipFilterLinear`; `kAddressingWrap`.
const STAGE_OP_TEXTURE: u32 = 1;
const MIP_FILTER_NONE: u32 = 0;
const MIP_FILTER_LINEAR: u32 = 2;
const ADDRESSING_WRAP: u32 = 2;

/// The cloud layer's ten triangles (0x44142c).
const CLOUD_INDICES: [u32; 30] =
    [0, 1, 4, 4, 1, 5, 2, 0, 6, 6, 0, 4, 3, 2, 7, 7, 2, 6, 1, 3, 5, 5, 3, 7, 4, 5, 6, 6, 5, 7];

/// The sky dome's 36 triangles (0x441524).
const SKY_INDICES: [u32; 108] = [
    0, 1, 5, 0, 5, 4, 4, 5, 9, 4, 9, 8, 8, 9, 13, 8, 13, 12, 12, 13, 17, 12, 17, 16, 1, 2, 6, 1, 6, 5, 5, 6, 10, 5, 10,
    9, 9, 10, 14, 9, 14, 13, 13, 14, 18, 13, 18, 17, 2, 3, 7, 2, 7, 6, 6, 7, 11, 6, 11, 10, 10, 11, 15, 10, 15, 14, 14,
    15, 19, 14, 19, 18, 3, 0, 4, 3, 4, 7, 7, 4, 8, 7, 8, 11, 11, 8, 12, 11, 12, 15, 15, 12, 16, 15, 16, 19, 16, 17, 18,
    16, 18, 19, 0, 3, 2, 0, 2, 1,
];

const fn c(red: f32, green: f32, blue: f32) -> EeGfxColor {
    EeGfxColor { red, green, blue }
}

/// The sky colours: 17 times of day by 4 bands (0x4416d4; `gSkyColors` 0x5290b0 was its copy).
pub(crate) const SKY_COLORS: [EeGfxColor; 68] = {
    let dawn = c(0.773, 0.255, 0.051);
    let day = c(0.698, 0.894, 0.984);
    let dusk = c(0.047, 0.22, 0.373);
    let night = c(0.023, 0.11, 0.185);
    let glow = c(0.941, 0.69, 0.259);
    let deep = c(0.008, 0.031, 0.125);
    let haze = c(0.302, 0.267, 0.349);
    let blue = c(0.2, 0.29, 0.659);
    let black = c(0.0, 0.0, 0.0);
    let mid = c(0.1, 0.15, 0.325);
    [
        dawn, day, day, day, day, day, day, day, dawn, dusk, night, night, night, night, night, dusk,
        dawn, // band 0
        glow, day, day, day, day, day, day, day, glow, night, deep, deep, deep, deep, deep, night, glow, // band 1
        haze, blue, blue, blue, blue, blue, blue, blue, haze, deep, black, black, black, black, black, deep,
        haze, // 2
        deep, mid, mid, mid, mid, mid, mid, mid, deep, black, black, black, black, black, black, black,
        deep, // band 3
    ]
};

/// The models and textures (0x5293fc..0x529410; the area-effect textures 0x5293f4, 0x5293f8).
const SKY: usize = 0;
const CLOUD: usize = 1;
const CLOUD_TEXTURE: usize = 2;
const OUTER_STAR: usize = 3;
const INNER_STAR: usize = 4;
const STAR_TEXTURE: usize = 5;
const AREA_EFFECT: usize = 6;
const CIRCLE_AREA_EFFECT: usize = 7;
#[allow(clippy::declare_interior_mutable_const)]
const NONE: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());
static MEDIA: [AtomicPtr<c_void>; 8] = [NONE; 8];

fn get(i: usize) -> EeGfxModel {
    MEDIA[i].load(Ordering::Relaxed)
}
fn set(i: usize, m: EeGfxModel) {
    MEDIA[i].store(m, Ordering::Relaxed)
}

/// The settings the media's loads read (`gTextureMipLevelsToDrop`, `gTrilinearFilter`, `gDrawClouds`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTerrainMedia {
    pub mip_levels_to_drop: u32,
    pub trilinear: bool,
    pub draw_clouds: bool,
}

/// The area-effect textures `CreateSkyModel` loads (`gAreaEffectTexture`, `gCircleAreaEffectTexture`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxTerrainTextures {
    pub area_effect: EeGfxModel,
    pub circle_area_effect: EeGfxModel,
}

fn vertex(set: u32) -> EeGfxMeshVertex {
    EeGfxMeshVertex { set, ..EeGfxMeshVertex::default() }
}

fn triangles(indices: &[u32]) -> Vec<EeGfxMeshTriangle> {
    indices.chunks(3).map(|t| EeGfxMeshTriangle { set: 1, v: [t[0], t[1], t[2]] }).collect()
}

/// A sky texture's load: 32 bits when the screen has them, the settings' MIP levels dropped, alpha and MIP maps,
/// modulated, bilinear, wrapped, the MIP filter `mip_filter`.
fn sky_texture<R: Raster>(x: &mut R, m: &EeGfxTerrainMedia, mip_filter: u32) -> EeGfxTextureLoad {
    EeGfxTextureLoad {
        pixel_depth: if x.texture_32bit() { 32 } else { 16 },
        mip_levels_to_drop: m.mip_levels_to_drop,
        frame_format: 0,
        volatility: 1,
        has_alpha: true,
        mip_maps: true,
        alpha_mode: STAGE_OP_MODULATE,
        color_mode: STAGE_OP_MODULATE,
        min_filter: 1,
        mag_filter: 1,
        mip_filter,
        mip_bias: 0.0,
        addressing: ADDRESSING_WRAP,
        reference: true,
    }
}

/// `CreateStarModels` (0x5126ce): the outer and inner star models (six quads each, the inner one's texture turned
/// half a repeat) and the star texture.
fn create_star_models<R: Raster>(x: &mut R, m: &EeGfxTerrainMedia) {
    let outer = x.create_model(0xc, 0x18, 0, "outer star model", 1);
    let inner = x.create_model(0xc, 0x18, 0, "inner star model", 1);
    x.model_reference(outer);
    x.model_reference(inner);
    x.material_ranges(outer, 0xc, 0x18);
    x.material_ranges(inner, 0xc, 0x18);
    let mut indices = Vec::with_capacity(36);
    for j in (2..0x1a).step_by(4) {
        indices.extend_from_slice(&[j - 2, j - 1, j, j - 2, j, j + 1]);
    }
    let t = triangles(&indices);
    x.store_mesh(outer, &[], &t);
    x.store_mesh(inner, &[], &t);
    x.model_normals(outer);
    x.model_bounds(outer);
    let (a, b, d, e) = ((0.0, 0.0), (STAR_U, 0.0), (STAR_U, STAR_U), (0.0, STAR_U));
    let quad = |corners: [(f32, f32); 4]| -> Vec<EeGfxMeshVertex> {
        (0..0x18)
            .map(|i| {
                let (u, v) = corners[i % 4];
                EeGfxMeshVertex {
                    coord: EeGfxTexCoord { u, v },
                    color: c(1.0, 1.0, 1.0),
                    opacity: 1.0,
                    ..vertex(EE_GFX_MESH_COORD | EE_GFX_MESH_COLOR | EE_GFX_MESH_OPACITY)
                }
            })
            .collect()
    };
    x.store_mesh(outer, &quad([a, b, d, e]), &[]);
    x.store_mesh(inner, &quad([d, e, a, b]), &[]);
    let load = sky_texture(x, m, if m.trilinear { MIP_FILTER_LINEAR } else { MIP_FILTER_NONE });
    let texture = x.load_texture("textures\\stars", &load);
    x.model_own_texture(outer, texture);
    x.model_own_texture(inner, texture);
    set(OUTER_STAR, outer);
    set(INNER_STAR, inner);
    set(STAR_TEXTURE, texture);
}

/// `CreateCloudModel` (0x512bc4): the cloud layer (an opaque inner square in a transparent outer one) and its
/// texture.
fn create_cloud_model<R: Raster>(x: &mut R, m: &EeGfxTerrainMedia) {
    let cloud = x.create_model(10, 8, 0, "cloud layer model", 1);
    x.model_reference(cloud);
    x.material_ranges(cloud, 10, 8);
    let vertices: Vec<_> = (0..8)
        .map(|i| EeGfxMeshVertex {
            color: c(1.0, 1.0, 1.0),
            opacity: if i < 4 { 0.0 } else { 1.0 },
            ..vertex(EE_GFX_MESH_COLOR | EE_GFX_MESH_OPACITY)
        })
        .collect();
    x.store_mesh(cloud, &vertices, &triangles(&CLOUD_INDICES));
    let load = sky_texture(x, m, if m.trilinear { MIP_FILTER_LINEAR } else { MIP_FILTER_NONE });
    let texture = x.load_texture("textures\\fx_cloudtexture", &load);
    x.model_own_texture(cloud, texture);
    set(CLOUD, cloud);
    set(CLOUD_TEXTURE, texture);
}

/// An area-effect texture's load: 16 bits, no alpha, no MIP maps, point filtered, wrapped, the stage operation `op`.
fn area_texture(op: u32) -> EeGfxTextureLoad {
    EeGfxTextureLoad {
        pixel_depth: 16,
        mip_levels_to_drop: 0,
        frame_format: 0,
        volatility: 1,
        has_alpha: false,
        mip_maps: false,
        alpha_mode: op,
        color_mode: op,
        min_filter: 0,
        mag_filter: 0,
        mip_filter: MIP_FILTER_NONE,
        mip_bias: 0.0,
        addressing: ADDRESSING_WRAP,
        reference: true,
    }
}

/// `CreateSkyModel` (0x512e7f): the sky dome, the stars, the clouds (when they are on) and the area-effect textures.
pub fn create_sky_model<R: Raster>(x: &mut R, m: &EeGfxTerrainMedia) -> EeGfxTerrainTextures {
    let sky = x.create_model(0x24, 0x14, 0, "sky model", 1);
    x.model_reference(sky);
    x.material_ranges(sky, 0x24, 0x14);
    x.store_mesh(sky, &[], &triangles(&SKY_INDICES));
    set(SKY, sky);
    create_star_models(x, m);
    if m.draw_clouds {
        create_cloud_model(x, m);
    }
    let area = x.load_texture("textures\\AreaEffect", &area_texture(STAGE_OP_TEXTURE));
    let circle = x.load_texture("textures\\CircleAreaEffect", &area_texture(STAGE_OP_MODULATE));
    set(AREA_EFFECT, area);
    set(CIRCLE_AREA_EFFECT, circle);
    EeGfxTerrainTextures { area_effect: area, circle_area_effect: circle }
}

/// `ReleaseSkyModels` (0x5138d2): every model and texture dereferenced and forgotten.
pub fn release_sky_models<R: Raster>(x: &mut R) {
    for i in [SKY, CLOUD, CLOUD_TEXTURE, OUTER_STAR, INNER_STAR, STAR_TEXTURE, AREA_EFFECT, CIRCLE_AREA_EFFECT] {
        let m = MEDIA[i].swap(std::ptr::null_mut(), Ordering::Relaxed);
        if m.is_null() {
            continue;
        }
        match i {
            CLOUD_TEXTURE | STAR_TEXTURE | AREA_EFFECT | CIRCLE_AREA_EFFECT => x.texture_dereference(m),
            _ => x.model_dereference(m),
        }
    }
}

/// The frame's facts for the sky and clouds.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTerrainFrame {
    /// `gRenderEnvironment.mTimeOfDay`
    pub time_of_day: f32,
    /// the map's `mSize`, `mL195614`
    pub size: i32,
    pub width: i32,
    /// the terrain mesh's `GetIsSpaceMap`
    pub space_map: bool,
    /// `gRenderEnvironment.mCloudOffset`
    pub cloud_offset: f32,
    pub media: EeGfxTerrainMedia,
}

/// What the frame changes: the fog colour (`gU4E9220`) and the cloud offset.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTerrainFrameOut {
    pub fog: EeGfxColor,
    pub cloud_offset: f32,
}

/// `FrameStarting`'s sky (`UpdateSkyColors`) and then a space map's black fog or the cloud layer
/// (`UpdateCloudLayer`, when the clouds are on).
pub fn frame<R: Raster>(x: &mut R, f: &EeGfxTerrainFrame) -> EeGfxTerrainFrameOut {
    let sky = sky_frame(f.time_of_day, &SKY_COLORS, f.size, f.width);
    let mut out = EeGfxTerrainFrameOut { fog: sky.fog, cloud_offset: f.cloud_offset };
    let (dome, outer, inner) = (get(SKY), get(OUTER_STAR), get(INNER_STAR));
    let vertices: Vec<_> = (0..20)
        .map(|i| EeGfxMeshVertex {
            point: sky.sky_points[i],
            color: sky.sky_colors[i],
            ..vertex(EE_GFX_MESH_POINT | EE_GFX_MESH_COLOR)
        })
        .collect();
    x.store_mesh(dome, &vertices, &[]);
    x.model_normals(dome);
    x.model_bounds(dome);
    let points = |p: &[Ge3DPoint; 24]| -> Vec<EeGfxMeshVertex> {
        p.iter().map(|&point| EeGfxMeshVertex { point, ..vertex(EE_GFX_MESH_POINT) }).collect()
    };
    x.store_mesh(outer, &points(&sky.outer_stars), &[]);
    x.store_mesh(inner, &points(&sky.inner_stars), &[]);
    x.model_normals(outer);
    x.model_bounds(outer);
    x.model_normals(inner);
    x.model_bounds(inner);
    if f.space_map {
        out.fog = c(0.0, 0.0, 0.0);
    } else if f.media.draw_clouds {
        if get(CLOUD).is_null() {
            create_cloud_model(x, &f.media);
        }
        let cloud = get(CLOUD);
        let layer = cloud_layer(f.size, f.width, f.cloud_offset);
        let at = |i: usize| EeGfxMeshVertex { point: layer.points[i], ..vertex(EE_GFX_MESH_POINT) };
        x.store_mesh(cloud, &(0..8).map(at).collect::<Vec<_>>(), &[]);
        x.model_normals(cloud);
        x.model_bounds(cloud);
        if layer.wrapped {
            out.cloud_offset = layer.offset;
        }
        let coord = |i: usize| EeGfxMeshVertex { coord: layer.coords[i], ..vertex(EE_GFX_MESH_COORD) };
        x.store_mesh(cloud, &(0..8).map(coord).collect::<Vec<_>>(), &[]);
    }
    out
}

/// The scene's facts and the terrain mesh's draws (the C++'s, over the frame's snapshot).
#[repr(C)]
pub struct EeGfxTerrainScene {
    pub context: *mut c_void,
    /// `gRenderSnapshot.mIsSpaceMap`, `gDrawClouds`, `gOrthographicView`
    pub space_map: bool,
    pub draw_clouds: bool,
    pub orthographic: bool,
    /// `mGridMode != 0`
    pub grid: bool,
    /// the grid runs (`gGridRuns.size()`)
    pub grid_runs: usize,
    /// the terrain mesh (`Draw(mesh, screen, gOrthographicView)`)
    pub draw_terrain: extern "C" fn(context: *mut c_void),
    /// the editor brush's highlight (at `opacity`, when shown) and the map's coloured rectangles
    pub draw_highlights: extern "C" fn(context: *mut c_void, opacity: f32),
    /// `DrawTerrainGrid`
    pub draw_grid: extern "C" fn(context: *mut c_void),
    /// `DrawGridLines` over the grid runs in the area-effect texture
    pub draw_grid_lines: extern "C" fn(context: *mut c_void),
    /// the frame's textured 3D lines (`Draw3DLine`)
    pub draw_lines: extern "C" fn(context: *mut c_void),
    /// the effects of render queue 2, batched per texture (`DrawPass4`)
    pub draw_effects: extern "C" fn(context: *mut c_void),
}

/// `RenderScene` (0x5131a4).
pub fn render_scene<R: Raster>(x: &mut R, s: &EeGfxTerrainScene) {
    let alpha_ref = x.get_alpha_ref();
    let alpha_testing = x.get_alpha_testing();
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_zcompare(COMPARE_LESS_EQUAL);
    x.set_alpha_testing(0);
    x.set_shading_source(1);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::Off);
    x.lighting(EeGfxLighting::SceneFog);
    x.set_fog(0);
    x.set_blending(BLEND_ONE, BLEND_ZERO);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.lighting(EeGfxLighting::White);
    let rear = x.rear_clipping(SKY_REAR_CLIPPING);
    x.depth_range(1.0, 1.0);
    if s.space_map {
        x.draw_sky(get(OUTER_STAR), false);
        if s.draw_clouds {
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.draw_sky(get(INNER_STAR), false);
        }
    } else {
        x.draw_sky(get(SKY), s.orthographic);
        if s.draw_clouds {
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.lighting(EeGfxLighting::Cloud);
            x.draw_sky(get(CLOUD), false);
        }
    }
    x.rear_clipping(rear);
    x.depth_range(0.0, 1.0);
    x.set_culling(CULL_CCW);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_zcompare(COMPARE_LESS_EQUAL);
    x.set_alpha_testing(0);
    x.set_shading_source(1);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::Off);
    x.lighting(EeGfxLighting::SceneFog);
    (s.draw_terrain)(s.context);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_zbuffer(ZBUFFER_DISABLED);
    (s.draw_highlights)(s.context, HIGHLIGHT_OPACITY);
    x.depth_range(0.0, 0.0);
    x.set_blending(BLEND_ONE, BLEND_ZERO);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_zcompare(COMPARE_LESS);
    if s.grid {
        (s.draw_grid)(s.context);
    }
    if s.grid_runs != 0 {
        (s.draw_grid_lines)(s.context);
    }
    (s.draw_lines)(s.context);
    x.set_zcompare(COMPARE_LESS_EQUAL);
    (s.draw_effects)(s.context);
    x.depth_range(0.0, 1.0);
    x.set_alpha_testing(alpha_testing);
    x.set_alpha_ref(alpha_ref);
}

/// `EEROTerrain::CreateSkyModel`.
///
/// # Safety
/// `raster` and `media` are readable and the raster's functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_terrain_create(
    raster: *const crate::gfx::raster::EeGfxRaster,
    media: *const EeGfxTerrainMedia,
) -> EeGfxTerrainTextures {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        create_sky_model(&mut x, &*media)
    }
}

/// `EEROTerrain::ReleaseSkyModels`.
///
/// # Safety
/// `raster` is readable and its functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_terrain_release(raster: *const crate::gfx::raster::EeGfxRaster) {
    // SAFETY: the function's contract
    let mut x = unsafe { &*raster };
    release_sky_models(&mut x);
}

/// `EEROTerrain::FrameStarting`'s sky and clouds.
///
/// # Safety
/// `raster` and `frame` are readable and the raster's functions valid; the sky model was made.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_terrain_frame(
    raster: *const crate::gfx::raster::EeGfxRaster,
    frame_facts: *const EeGfxTerrainFrame,
) -> EeGfxTerrainFrameOut {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        frame(&mut x, &*frame_facts)
    }
}

/// `EEROTerrain::RenderScene`.
///
/// # Safety
/// `raster` and `scene` are readable and their functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_terrain_render(
    raster: *const crate::gfx::raster::EeGfxRaster,
    scene: *const EeGfxTerrainScene,
) {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        render_scene(&mut x, &*scene);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gfx::raster::LogRaster;
    use std::sync::Mutex;

    /// The media are one set of statics: one test at a time.
    static LOCK: Mutex<()> = Mutex::new(());

    extern "C" fn log_call(context: *mut c_void) {
        // SAFETY: the tests pass a Vec<String>
        unsafe { (*(context as *mut Vec<String>)).push("host".into()) }
    }
    extern "C" fn log_highlights(context: *mut c_void, opacity: f32) {
        // SAFETY: the tests pass a Vec<String>
        unsafe { (*(context as *mut Vec<String>)).push(format!("highlights {opacity}")) }
    }

    fn scene(log: &mut Vec<String>, space: bool, clouds: bool) -> EeGfxTerrainScene {
        EeGfxTerrainScene {
            context: log as *mut Vec<String> as *mut c_void,
            space_map: space,
            draw_clouds: clouds,
            orthographic: true,
            grid: false,
            grid_runs: 0,
            draw_terrain: log_call,
            draw_highlights: log_highlights,
            draw_grid: log_call,
            draw_grid_lines: log_call,
            draw_lines: log_call,
            draw_effects: log_call,
        }
    }

    #[test]
    fn the_sky_colours_are_the_tables_rows() {
        assert_eq!(SKY_COLORS[0], c(0.773, 0.255, 0.051));
        assert_eq!(SKY_COLORS[17], c(0.941, 0.69, 0.259));
        assert_eq!(SKY_COLORS[34], c(0.302, 0.267, 0.349));
        assert_eq!(SKY_COLORS[44], c(0.0, 0.0, 0.0));
        assert_eq!(SKY_COLORS[51], c(0.008, 0.031, 0.125));
        assert_eq!(SKY_COLORS[67], c(0.008, 0.031, 0.125));
        assert_eq!(SKY_COLORS[26], c(0.023, 0.11, 0.185));
    }

    #[test]
    fn the_media_are_made_and_released() {
        let _g = LOCK.lock().unwrap();
        let mut x = LogRaster { texture_32bit: true, ..LogRaster::default() };
        let m = EeGfxTerrainMedia { mip_levels_to_drop: 1, trilinear: true, draw_clouds: true };
        create_sky_model(&mut x, &m);
        let made: Vec<_> = x.models.iter().map(|m| m.name.clone()).collect();
        assert_eq!(made, ["sky model", "outer star model", "inner star model", "cloud layer model"]);
        assert_eq!(x.model(get(SKY)).triangles[35], [0, 2, 1]);
        assert_eq!(x.model(get(OUTER_STAR)).triangles[1], [0, 2, 3]);
        assert_eq!(x.model(get(INNER_STAR)).triangles[11], [20, 22, 23]);
        assert_eq!(x.model(get(CLOUD)).opacities, [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0]);
        let textures: Vec<_> = x.calls.iter().filter(|c| c.starts_with("texture ")).cloned().collect();
        assert_eq!(
            textures,
            [
                "texture textures\\stars 32 true 1 2 2 true",
                "texture textures\\fx_cloudtexture 32 true 1 2 2 true",
                "texture textures\\AreaEffect 16 false 0 0 2 true",
                "texture textures\\CircleAreaEffect 16 false 0 0 2 true",
            ]
        );
        release_sky_models(&mut x);
        assert!(get(SKY).is_null() && get(CLOUD).is_null() && get(AREA_EFFECT).is_null());
        assert_eq!(x.calls.iter().filter(|c| c.starts_with("dereference")).count(), 4);
        assert_eq!(x.calls.iter().filter(|c| c.starts_with("texture dereference")).count(), 4);
    }

    #[test]
    fn a_frame_stretches_the_sky_and_a_space_map_has_black_fog() {
        let _g = LOCK.lock().unwrap();
        let mut x = LogRaster::default();
        let m = EeGfxTerrainMedia { mip_levels_to_drop: 0, trilinear: false, draw_clouds: true };
        create_sky_model(&mut x, &m);
        let f = EeGfxTerrainFrame {
            time_of_day: 0.3,
            size: 100,
            width: 50,
            space_map: false,
            cloud_offset: 1.25,
            media: m,
        };
        let out = frame(&mut x, &f);
        let sky = sky_frame(0.3, &SKY_COLORS, 100, 50);
        assert_eq!(out.fog, sky.fog);
        assert_eq!(x.model(get(SKY)).points[7], sky.sky_points[7]);
        assert_eq!(x.model(get(INNER_STAR)).points[23], sky.inner_stars[23]);
        let layer = cloud_layer(100, 50, 1.25);
        assert_eq!(x.model(get(CLOUD)).points[5], layer.points[5]);
        assert_eq!(out.cloud_offset, layer.offset);
        let space = frame(&mut x, &EeGfxTerrainFrame { space_map: true, ..f });
        assert_eq!(space.fog, c(0.0, 0.0, 0.0));
        assert_eq!(space.cloud_offset, 1.25);
        release_sky_models(&mut x);
    }

    #[test]
    fn the_scene_draws_the_sky_then_the_terrain_then_the_overlays() {
        let _g = LOCK.lock().unwrap();
        let mut x = LogRaster { alpha_ref: 0.25, alpha_testing: 1, rear: 900.0, ..LogRaster::default() };
        let mut log = Vec::new();
        render_scene(&mut x, &scene(&mut log, false, true));
        let sky: Vec<_> = x.calls.iter().filter(|c| c.starts_with("sky") || c.starts_with("rear")).cloned().collect();
        assert_eq!(sky, ["rear 20000", "sky 0 true", "sky 0 false", "rear 900"]);
        assert!(x.calls.contains(&"lighting Cloud".to_string()));
        assert_eq!(x.calls[x.calls.len() - 2], "alphatest 1");
        assert_eq!(x.calls[x.calls.len() - 1], "alpharef 0.25");
        assert_eq!(log, ["host", "highlights 0.5", "host", "host"]);
        let mut log = Vec::new();
        let mut x = LogRaster::default();
        let mut s = scene(&mut log, true, false);
        s.grid = true;
        s.grid_runs = 3;
        render_scene(&mut x, &s);
        assert_eq!(x.calls.iter().filter(|c| c.starts_with("sky")).count(), 1);
        assert_eq!(log.len(), 6);
    }
}
