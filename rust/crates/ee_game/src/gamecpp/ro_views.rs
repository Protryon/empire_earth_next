//! The Graphics Thread's other render objects' bodies (were EEROUserInterface.cpp, EEUnitPicker.cpp's EEROWater,
//! EEROGroundShadows.cpp, EEROTerrain.cpp, EERenderSnapshot.cpp, EEMouse.cpp, EERenderCapture.cpp): the UI's frame,
//! z-buffer clears and polygons, the water's pass, the ground shadows' lists, the terrain's frame, sky media and
//! scene, the render snapshot, the mouse's extra pointers and RFIL's fill. Each C++ call or member is a case of
//! EERenderSnapshot.cpp's `EERoViewsOp` or EEMouse.cpp's `EEMouseOp`; the draws themselves are ee_gfx's.
//!
//! Only a game with a window runs the draws (the tour, gates.sh); RFIL's fill runs under its capture only.

use std::ffi::{c_long, c_ulong, c_void, CStr};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Mutex, MutexGuard, OnceLock};

use ee_base::math::types::{GeRectangle, GeTransformation};
use ee_base::math::um::floor;
use ee_render::ge::tmobj::{
    ee_ge_tm_get, ee_ge_tm_is_valid, ee_ge_tm_tile, ee_ge_tm_update_fog_of_war, EeGeTerrainMesh, EeGeTmVal,
};
use ee_render::geval::viewport::GeViewport;
use ee_render::gfx::effect_obj::{ee_gfx_effect_queues_draw, ee_gfx_effect_storm_move, EeGfxEffectHead};
use ee_render::gfx::effect_state::EeGfxEffectRecord;
use ee_render::gfx::effects::EeGfxColor;
use ee_render::gfx::ffi_effect::ee_gfx_storm_bounds;
use ee_render::gfx::fill::{EeGfxFillObject, EeGfxFillUnit};
use ee_render::gfx::lighting::EeGfxLighting;
use ee_render::gfx::rfil::{ee_gfx_rfil_active, ee_gfx_rfil_write, EeGfxRfilPairs};
use ee_render::gfx::ro_terrain::{
    ee_gfx_ro_terrain_create, ee_gfx_ro_terrain_frame, ee_gfx_ro_terrain_release, ee_gfx_ro_terrain_render,
    EeGfxTerrainFrame, EeGfxTerrainMedia, EeGfxTerrainScene,
};
use ee_render::gfx::shadows::{ee_gfx_ground_shadows_render, EeGfxShadow, EeGfxShadowScene, EeGfxUnitShadow};

use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;

use super::globals;
use super::rast::{
    frame_viewport, mesh_of, rast, view_rectangle, world_mesh, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA,
    COMPARE_LESS_EQUAL, CULL_NONE, Z_BUFFER_DISABLED, Z_BUFFER_TEST_ONLY,
};
use super::render_object::ee_gamecpp_ro_lighting;
use super::ro_units::{lists_view, raster, RU_UNIT_PANEL_ACTIVE};
use super::{h, hd, hp, ptr, rec, O, P};

/// The panel corners of the UI's z-buffer clears (.rdata 0x441c00..: the unit panel's right, the panels' top, the
/// right panel's left, as fractions of the game window).
const GU_UNIT_PANEL_RIGHT: f32 = 0.17;
const GU_PANEL_TOP: f32 = 0.716_666_64;
const GU_RIGHT_PANEL_LEFT: f32 = 0.8275;
/// The UI state of the free camera's full-window view (no panel clears).
const GU_STATE_NO_PANELS: i64 = 0x11;
/// `kUnitCategorySubmergedNaval`: no ground shadow.
const GS_SUBMERGED_NAVAL: c_long = 0xa;

fn pairs(p: &EeGfxRfilPairs) -> &[ee_render::gfx::rfil::EeGfxRfilPair] {
    if p.count == 0 {
        &[]
    } else {
        // SAFETY: the view's flattened multimap (valid until the next view)
        unsafe { std::slice::from_raw_parts(p.data, p.count) }
    }
}

// ---- EEROUserInterface

/// EEROUserInterface::FrameStarting: the UI's frame data copied, and whether the unit panel is shown (the current
/// player has a selected unit or a viewed unit).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gu_frame_starting(ro: *mut c_void) {
    h(O::GuCopyData, ro as P, &[]);
    let player = hp(O::WorldCurrentPlayer, 0, &[]);
    let active = player != 0
        && ((hp(O::GuSelectedGroup, player, &[]) != 0
            && hp(O::GuGroupFirstUnit, hp(O::GuSelectedGroup, player, &[]), &[]) != 0)
            || hp(O::GuCurrentViewedUnit, 0, &[]) != 0);
    RU_UNIT_PANEL_ACTIVE.store(active, Ordering::Relaxed);
}

/// EEROUserInterface::SceneStarting: the z buffer cleared under the bottom-right panel, and under the unit panel when
/// it is shown, so the world draws behind them (not with the free camera or in UI state 0x11).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gu_scene_starting() {
    let zero = view_rectangle(frame_viewport());
    if crate::gamecpp::globals::get!(gFreeCamera) || h(O::GuMainUiState, 0, &[]) == GU_STATE_NO_PANELS {
        return;
    }
    let width = h(O::GameWindowWidth, 0, &[]) as u32 as f32;
    let height = h(O::GameWindowHeight, 0, &[]) as u32 as f32;
    let mut rectangle = GeRectangle {
        left: width * GU_RIGHT_PANEL_LEFT,
        top: height * GU_PANEL_TOP,
        right: zero.right,
        bottom: zero.bottom,
    };
    rast().clear_z(0.0, &rectangle);
    if !RU_UNIT_PANEL_ACTIVE.load(Ordering::Relaxed) {
        return;
    }
    rectangle.left = 0.0;
    rectangle.top = height * GU_PANEL_TOP;
    rectangle.right = width * GU_UNIT_PANEL_RIGHT;
    rectangle.bottom = zero.bottom;
    rast().clear_z(0.0, &rectangle);
}

/// EEROUserInterface::RenderScene: the UI's polygons over the whole render target with fog, lighting, alpha test and
/// z buffer off, the fog and alpha reference restored (lighting stays off).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gu_render_scene(ro: *mut c_void) {
    let r = rast();
    let fog = r.fog();
    let alpha_ref = r.alpha_ref();
    r.set_fog(0);
    r.set_shading_color_source(1);
    r.set_lighting(0);
    r.set_alpha_ref(0.0);
    r.set_z_buffer(Z_BUFFER_DISABLED);
    r.set_culling(CULL_NONE);
    let rectangle = GeRectangle {
        left: 0.0,
        top: 0.0,
        right: h(O::GuTargetWidth, 0, &[]) as u32 as f32,
        bottom: h(O::GuTargetHeight, 0, &[]) as u32 as f32,
    };
    let viewport = ui_viewport();
    // SAFETY: the UI's viewport (graphics thread only); the rectangle outlives the call
    unsafe { ee_render::geval::viewport_ffi::ee_ge_viewport_set_view_rectangle(viewport, &rectangle) };
    r.set_viewport(viewport);
    h(O::GuDrawPolygons, ro as P, &[]);
    r.set_fog(fog);
    r.set_alpha_ref(alpha_ref);
}

/// `gUIViewport` (0x529518): the UI's viewport, default-made at its first use (graphics thread only).
fn ui_viewport() -> *mut GeViewport {
    static VIEWPORT: OnceLock<usize> = OnceLock::new();
    *VIEWPORT.get_or_init(|| {
        let v = Box::into_raw(Box::new(std::mem::MaybeUninit::<GeViewport>::uninit())).cast::<GeViewport>();
        // SAFETY: room for a viewport, made as GEViewport's default constructor makes it
        unsafe { ee_render::geval::viewport_ffi::ee_ge_viewport_default(v) };
        v as usize
    }) as *mut GeViewport
}

// ---- EEROWater

/// EEROWater::RenderScene: the water over the terrain (alpha-blended, z test only, no culling, no lighting, the
/// scene's fog), then the effects' water pass (queue 4); the alpha reference restored.
#[no_mangle]
pub extern "C" fn ee_gamecpp_wa_render_scene() {
    let r = rast();
    let alpha_ref = r.alpha_ref();
    r.set_alpha_ref(0.0);
    r.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    r.set_z_buffer(Z_BUFFER_TEST_ONLY);
    r.set_z_compare(COMPARE_LESS_EQUAL);
    r.set_shading_color_source(1);
    r.set_culling(CULL_NONE);
    ee_gamecpp_ro_lighting(EeGfxLighting::Off as u32);
    ee_gamecpp_ro_lighting(EeGfxLighting::SceneFog as u32);
    let mesh = snapshot().mesh();
    r.draw_water(mesh, orthographic());
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(6) };
    r.set_alpha_ref(alpha_ref);
}

// ---- EEROGroundShadows

/// The snapshot mesh's water information at a tile (-1 off the map).
extern "C" fn gs_tile_water(_: *mut c_void, x: u32, y: u32) -> i32 {
    let mesh = snapshot().mesh();
    // SAFETY: the frame's mesh
    unsafe {
        if !ee_ge_tm_is_valid(mesh, x as c_ulong, y as c_ulong, true) {
            return -1;
        }
        (*ee_ge_tm_tile(mesh, x as c_ulong, y as c_ulong)).water_info as i32
    }
}

extern "C" fn gs_effects(_: *mut c_void) {
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(2) };
}

/// EEROGroundShadows::RenderScene: the static shadows and the unit shadows (when ground shadows are on; not the
/// submerged units nor those without a shadow scale), then the effects' ground pass, by ee_gfx shadows.rs over the
/// frame's lists (`mShadows2`, `mUnits2` in their order).
// `!(x > 0.0)`: a NaN shadow scale casts no shadow, as the C++'s test
#[allow(clippy::neg_cmp_op_on_partial_ord)]
#[no_mangle]
pub extern "C" fn ee_gamecpp_gs_render_scene() {
    let mut shadows = Vec::new();
    let mut units = Vec::new();
    let draw = h(O::GsDrawGroundShadows, 0, &[]) != 0;
    if draw {
        let frame = lists_view();
        for pair in pairs(&frame.maps[5]) {
            // SAFETY: a render shadow of the pool (EERenderShadow is EeGfxFillObject)
            let s = unsafe { &*(pair.value as *const EeGfxFillObject) };
            shadows.push(EeGfxShadow {
                texture: pair.key as *mut c_void,
                location: ee_base::math::types::Ge3DPoint { x: s.location[0], y: s.location[1], z: s.location[2] },
                scale: s.scale,
                opacity: s.opacity,
            });
        }
        for pair in pairs(&frame.maps[2]) {
            // SAFETY: a render unit of the pool (its head is EeGfxFillUnit)
            let u = unsafe { &*(pair.value as *const EeGfxFillUnit) };
            if u.category == GS_SUBMERGED_NAVAL || !(u.shadow_scale > 0.0) {
                continue;
            }
            let source = u.source as P;
            units.push(EeGfxUnitShadow {
                transformation: u.transformation,
                model: hp(O::GsShadowModel, source, &[]) as *mut c_void,
                texture: hp(O::GsShadowTexture, source, &[]) as *mut c_void,
                category: u.category as i32,
                shadow_scale: u.shadow_scale,
                ground_z: u.ground_z,
                scale: u.scale,
                class_scale: hd(O::GsClassScale, 0, &[u.unit_class_index as i64], &[]),
                opacity: u.opacity,
            });
        }
    }
    let scene = EeGfxShadowScene {
        context: std::ptr::null_mut(),
        shadows: if shadows.is_empty() { std::ptr::null() } else { shadows.as_ptr() },
        shadow_count: shadows.len(),
        units: if units.is_empty() { std::ptr::null() } else { units.as_ptr() },
        unit_count: units.len(),
        draw_shadows: draw,
        space_map: snapshot().space_map,
        tile_water: gs_tile_water,
        draw_effects: gs_effects,
    };
    // SAFETY: the raster table, the scene and its arrays live through the call
    unsafe { ee_gfx_ground_shadows_render(raster(), &scene) };
}

// ---- EEROTerrain

fn te_media() -> EeGfxTerrainMedia {
    EeGfxTerrainMedia {
        mip_levels_to_drop: h(O::TeMipLevelsToDrop, 0, &[]) as u32,
        trilinear: h(O::TeTrilinear, 0, &[]) != 0,
        draw_clouds: h(O::TeDrawClouds, 0, &[]) != 0,
    }
}

/// EEROTerrain::FrameStarting: the sky colours and models, the clouds (a black fog on space maps instead), the fog
/// of war (all seen for see-all or the free camera), the map's grid runs and the terrain's snapshot.
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_frame_starting(ro: *mut c_void) {
    super::render_object::ee_gamecpp_ro_fn10c036(ro);
    let (time_of_day, cloud_offset) = super::render_object::time_of_day_and_cloud_offset();
    let frame = EeGfxTerrainFrame {
        time_of_day,
        size: h(O::TeMapSize, 0, &[]) as i32,
        width: h(O::RtMapL195614, 0, &[]) as i32,
        space_map: mesh_is_space(world_mesh()),
        cloud_offset,
        media: te_media(),
    };
    // SAFETY: the raster table and the frame live through the call
    let out = unsafe { ee_gfx_ro_terrain_frame(raster(), &frame) };
    // SAFETY: gU4E9220 (a GEColor: red, green, blue)
    unsafe { *(hp(O::RoFogColor, 0, &[]) as *mut EeGfxColor) = out.fog };
    super::render_object::set_cloud_offset(out.cloud_offset);
    super::render_object::ee_gamecpp_ro_fn10e8d1(ro);
    // the fog of war: all seen for see-all or the free camera
    let current = hp(O::WorldCurrentPlayer, 0, &[]);
    // SAFETY: the current player's record
    let sees_all = unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, current)).mSeesAll };
    let all_seen = sees_all || globals::get!(gFreeCamera);
    // SAFETY: the world's map's mesh
    unsafe { ee_ge_tm_update_fog_of_war(world_mesh().cast_mut(), all_seen) };
    // the map's tile runs as of this frame, drawn as grid lines (gGridRuns 0x5290a0)
    let mut runs: *const [c_long; 4] = std::ptr::null();
    let count = h(O::TeMapGridRuns, 0, &[ptr(&mut runs)]) as usize;
    let runs = if count == 0 {
        Vec::new()
    } else {
        // SAFETY: the map's vector of runs (four longs each), copied under the world's lock
        unsafe { std::slice::from_raw_parts(runs, count) }.to_vec()
    };
    snapshot().grid_runs = runs;
    ee_gamecpp_sn_take_terrain();
}

/// EEROTerrain::CreateSkyModel: the sky dome, the stars, the clouds (when on) and the area-effect textures (ee_gfx
/// ro_terrain.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_create_sky_model() {
    let media = te_media();
    // SAFETY: the raster table and the media live through the call
    let textures = unsafe { ee_gfx_ro_terrain_create(raster(), &media) };
    AREA_EFFECT_TEXTURE.store(textures.area_effect as usize, Ordering::Relaxed);
    CIRCLE_AREA_EFFECT_TEXTURE.store(textures.circle_area_effect as usize, Ordering::Relaxed);
}

/// EEROTerrain::ReleaseSkyModels: the sky, cloud and star models and textures, and the area-effect textures.
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_release_sky_models() {
    // SAFETY: the raster table
    unsafe { ee_gfx_ro_terrain_release(raster()) };
    AREA_EFFECT_TEXTURE.store(0, Ordering::Relaxed);
    CIRCLE_AREA_EFFECT_TEXTURE.store(0, Ordering::Relaxed);
}

/// `gAreaEffectTexture` (0x5293f4): the area effects' ring texture (the ground circles' 3D lines, the grid lines).
static AREA_EFFECT_TEXTURE: AtomicUsize = AtomicUsize::new(0);
/// `gCircleAreaEffectTexture` (0x5293f8).
static CIRCLE_AREA_EFFECT_TEXTURE: AtomicUsize = AtomicUsize::new(0);

/// `gAreaEffectTexture`.
pub(crate) fn area_effect_texture() -> usize {
    AREA_EFFECT_TEXTURE.load(Ordering::Relaxed)
}

/// The brush highlight's colour (`gBrushHighlightColor` 0x5293e8: GEColor's default white).
const TE_BRUSH_COLOR: [f32; 3] = [1.0, 1.0, 1.0];

extern "C" fn te_draw(_: *mut c_void) {
    let mesh = snapshot().mesh();
    rast().draw_terrain(mesh, orthographic());
}

/// The editor's brush highlight and the map's coloured rectangles (the snapshot's).
extern "C" fn te_highlights(_: *mut c_void, opacity: f32) {
    let (r, ortho) = (rast(), orthographic());
    // (copied out: the draws run without the snapshot's lock)
    let (mesh, brush, colored) = {
        let s = snapshot();
        (s.mesh(), s.brush_shown.then_some(s.brush_tiles), s.colored.clone())
    };
    if let Some(tiles) = brush {
        r.draw_terrain_highlight(mesh, &tiles, &TE_BRUSH_COLOR, opacity, ortho);
    }
    for c in &colored {
        r.draw_terrain_highlight(mesh, &c.rect, &c.color, c.opacity, ortho);
    }
}

extern "C" fn te_grid(_: *mut c_void) {
    let mesh = snapshot().mesh();
    rast().draw_terrain_grid(mesh, orthographic());
}

extern "C" fn te_grid_lines(_: *mut c_void) {
    // gU4E9330: the grid's phase, the lines' width
    let width = globals::get!(gU4E9330);
    let (r, ortho) = (rast(), orthographic());
    let (mesh, runs) = {
        let s = snapshot();
        (s.mesh(), s.grid_runs.clone())
    };
    r.draw_grid_lines(mesh, &runs, area_effect_texture(), width, ortho);
}

/// The frame's textured 3D lines (`gU529050`).
extern "C" fn te_lines(_: *mut c_void) {
    for i in 0..h(O::RoLinesCount, 0, &[]) {
        h(O::TeDraw3DLine, 0, &[i]);
    }
}

extern "C" fn te_effects(_: *mut c_void) {
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(4) };
}

/// EEROTerrain::RenderScene: the sky, the terrain, the highlights, the grid, the lines and the ground effects (ee_gfx
/// ro_terrain.rs), from the frame's snapshot.
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_render_scene(ro: *mut c_void) {
    let (space_map, grid_runs) = {
        let s = snapshot();
        (s.space_map, s.grid_runs.len())
    };
    let scene = EeGfxTerrainScene {
        context: std::ptr::null_mut(),
        space_map,
        draw_clouds: h(O::TeDrawClouds, 0, &[]) != 0,
        orthographic: orthographic(),
        grid: h(O::TeGridMode, ro as P, &[]) != 0,
        grid_runs,
        draw_terrain: te_draw,
        draw_highlights: te_highlights,
        draw_grid: te_grid,
        draw_grid_lines: te_grid_lines,
        draw_lines: te_lines,
        draw_effects: te_effects,
    };
    // SAFETY: the raster table and the scene live through the call
    unsafe { ee_gfx_ro_terrain_render(raster(), &scene) };
}

// ---- the render snapshot (was ee/EERenderSnapshot.h's EERenderSnapshot)

/// `RTSColoredRectangle` (ee/EEMap.h): a tile rectangle (a `U2DRectangle<long>`'s bottom, left, right, top), its
/// colour and opacity (0: erase) and a long.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct ColoredRectangle {
    rect: [c_long; 4],
    color: [f32; 3],
    opacity: f32,
    l20: c_long,
}

/// The render phase's view of the world: plain data taken under the world's lock at the start of every frame (the
/// world-wide part by `ee_gamecpp_sn_take` in EEGraphics::FrameStarting, the terrain's in EEROTerrain's
/// FrameStarting) and read after the lock is released, while the physics tick runs.
#[derive(Default)]
pub(crate) struct Snapshot {
    /// the camera (`RTSWorld::mTransformation`, `mCameraYaw`, `mCameraPitch`): the effects' billboards and sprites
    pub(crate) camera: GeTransformation,
    pub(crate) camera_yaw: f32,
    pub(crate) camera_pitch: f32,
    /// `EEWorld::mInCinematic`: no rings, bars or labels
    pub(crate) in_cinematic: bool,
    /// the terrain mesh's Rust object (`gWorld.mMap`'s; 0 outside a game): drawn, not copied (its render buffers are
    /// the graphics thread's own)
    mesh: usize,
    pub(crate) space_map: bool,
    /// the map's coloured rectangles with an opacity, back to front
    colored: Vec<ColoredRectangle>,
    /// the scenario editor's brush highlight, in whole tiles (bottom, left, right, top)
    brush_shown: bool,
    brush_tiles: [c_long; 4],
    /// the map's tile runs as of the frame (four longs each)
    grid_runs: Vec<[c_long; 4]>,
}

impl Snapshot {
    fn mesh(&self) -> *const EeGeTerrainMesh {
        self.mesh as *const EeGeTerrainMesh
    }
}

/// The frame's snapshot (graphics thread only).
pub(crate) fn snapshot() -> MutexGuard<'static, Snapshot> {
    static SNAPSHOT: OnceLock<Mutex<Snapshot>> = OnceLock::new();
    SNAPSHOT
        .get_or_init(|| Mutex::new(Snapshot { camera: GeTransformation::IDENTITY, ..Default::default() }))
        .lock()
        .unwrap()
}

/// `gOrthographicView`.
fn orthographic() -> bool {
    h(O::RoOrthographic, 0, &[]) != 0
}

/// `GetIsSpaceMap()`: a mesh with space tiles.
fn mesh_is_space(mesh: *const EeGeTerrainMesh) -> bool {
    // SAFETY: a live map's mesh
    unsafe { ee_ge_tm_get(mesh, EeGeTmVal::SpaceTileCount) > 0 }
}

/// EETakeRenderSnapshot: the camera, its yaw and pitch, the cinematic, the terrain mesh and the space map, as the
/// render phase reads them after the world's lock is released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_sn_take() {
    // SAFETY: the world's camera (a GETransformation), read under the world's lock
    let camera = unsafe { *(hp(O::RoWorldTransformation, 0, &[]) as *const GeTransformation) };
    let yaw = hd(O::EcWorldCameraYaw, 0, &[], &[]);
    let pitch = hd(O::SnCameraPitch, 0, &[], &[]);
    let cinematic = h(O::RrCinematic, 0, &[]) != 0;
    let map = hp(O::MpWorldMap, 0, &[]);
    let mesh = if map != 0 { mesh_of(map) } else { std::ptr::null() };
    let mut s = snapshot();
    s.camera = camera;
    s.camera_yaw = yaw;
    s.camera_pitch = pitch;
    s.in_cinematic = cinematic;
    s.mesh = mesh as usize;
    s.space_map = map != 0 && mesh_is_space(mesh);
}

/// EETakeTerrainSnapshot (EEROTerrain's frame): the map's coloured rectangles back to front under their lock (a zero
/// opacity erases the element; the others are kept in that order) and the editor's brush highlight.
#[no_mangle]
pub extern "C" fn ee_gamecpp_sn_take_terrain() {
    let map = hp(O::MpWorldMap, 0, &[]);
    let mut colored = Vec::new();
    let count = h(O::SnMapColoredCount, map, &[]);
    if count > 0 {
        h(O::SnMapColoredLock, map, &[]);
        for i in (0..count).rev() {
            // SAFETY: the map's element i (an RTSColoredRectangle), under its lock
            let c = unsafe { *(hp(O::SnMapColoredAt, map, &[i]) as *const ColoredRectangle) };
            if c.opacity != 0.0 {
                colored.push(c);
            } else {
                h(O::SnMapColoredErase, map, &[i]);
            }
        }
        h(O::SnMapColoredUnlock, map, &[]);
    }
    let mut brush = None;
    if h(O::SnEditorPresent, 0, &[]) != 0 {
        let mut r = GeRectangle::default();
        h(O::SnBrushRectangle, 0, &[ptr(&mut r)]);
        if h(O::SnBrushHighlightShown, 0, &[]) != 0 {
            // the rectangle's left, top, right, bottom in whole tiles, stored as a U2DRectangle (bottom first)
            brush = Some([floor(r.bottom), floor(r.left), floor(r.right), floor(r.top)].map(|v| v as c_long));
        }
    }
    let mut s = snapshot();
    s.colored = colored;
    s.brush_shown = brush.is_some();
    if let Some(tiles) = brush {
        s.brush_tiles = tiles;
    }
}

// ---- EEMouse

/// The extra mouse pointers, loaded in this order into the frame's slots 10..23 (0x4be124).
const MO_POINTER_FILES: [&CStr; 14] = [
    c"Textures\\Convert Pointer.png",
    c"Textures\\Board Transport Pointer.png",
    c"Textures\\Leave Transport Pointer.png",
    c"Textures\\Calamity Pointer.png",
    c"Textures\\Invalid Calamity Pointer.png",
    c"Textures\\Rally Point Pointer.png",
    c"Textures\\Protected Conversion Pointer.png",
    c"Textures\\Protected Calamity Pointer.png",
    c"Textures\\Heal.png",
    c"Textures\\Invalid Heal.png",
    c"Textures\\Patrol Pointer.png",
    c"Textures\\Garrison Pointer.png",
    c"Textures\\Flare Pointer.png",
    c"Textures\\Attack Ground Pointer.png",
];

/// EEMouse::Load: the 14 extra pointer images into the frame's slots 10..23 (RTSMouse::LoadPointer), then the
/// engine's pointers (RTSMouse::Load); the first load error.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_gamecpp_mo_load(mouse: *mut c_void, screen: *mut c_void) -> c_long {
    use crate::rts::mouse::{load_pointer, MouseCpp};
    use ee_base::shim::ShimFfi;
    let mut result: c_long = 0;
    for (i, name) in MO_POINTER_FILES.iter().enumerate() {
        if result != 0 {
            break;
        }
        let slot = ShimFfi.slot(mouse as usize, 10 + i as c_ulong);
        // SAFETY: the live mouse's pointer slot and the game screen
        result = unsafe { load_pointer(&mut ShimFfi, name, screen as usize, slot) };
    }
    if result == 0 {
        // SAFETY: the live mouse and the game screen
        result = unsafe { crate::rts::mouse::ffi::ee_rts_mouse_load(mouse, screen) };
    }
    result
}

/// EEMouse::Release: the extra pointer images (the frame's op 5), then the engine's (RTSMouse::Release: op 6); 0.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_gamecpp_mo_release(mouse: *mut c_void) -> c_long {
    use crate::rts::mouse::MouseCpp;
    use ee_base::shim::ShimFfi;
    ShimFfi.frame_call(mouse as usize, 5, 0, std::ptr::null(), false);
    ShimFfi.frame_call(mouse as usize, 6, 0, std::ptr::null(), false);
    0
}

// ---- RFIL (TEST ONLY: ee/EERenderCapture.h)

/// RFIL's fill record of the frame (EERenderObject::FillRenderLists, under the world's read lock): the render lists'
/// view and the world's key. `info` is FillRenderLists' frame state (EERenderFrameInfo).
pub fn rc_fill(picked_unit: P, yaw: f32, fighting: f32, frame: u32, ambient: bool, sway: u32, unit_count: f32) {
    if !ee_gfx_rfil_active() {
        return;
    }
    let mut f = lists_view();
    f.frame = frame;
    f.ticks = if h(O::RcPhysicsPresent, 0, &[]) != 0 { h(O::RoPhysicsTicks, 0, &[]) as u32 } else { 0 };
    f.sway = sway;
    f.yaw = yaw;
    // SAFETY: gWorld.mTransformation
    f.world_transformation = unsafe { *(hp(O::RoWorldTransformation, 0, &[]) as *const GeTransformation) };
    f.flags = (if crate::gamecpp::globals::get!(gFreeCamera) { 1 } else { 0 })
        | (if h(O::RoOrthographic, 0, &[]) != 0 { 2 } else { 0 })
        | (if ambient { 4 } else { 0 });
    f.picked = if picked_unit != 0 { h(O::RoUnitId, picked_unit, &[]) as i32 } else { -1 };
    f.world_key = super::render_object::ee_gamecpp_ro_world_key();
    f.fighting = fighting;
    f.unit_count = unit_count;
    // SAFETY: the record lives through the call
    unsafe { ee_gfx_rfil_write(&f) };
}

// ---- EEEffect's storm bounds (EEEffectHost.cpp)

/// A `U2DRectangle<long>` as the C++ stores it: bottom, left, right, top.
#[repr(C)]
pub struct EeGameCppTileBounds {
    bottom: c_long,
    left: c_long,
    right: c_long,
    top: c_long,
}

fn fx_put_bounds(out: &mut EeGameCppTileBounds, b: &[i32; 4]) {
    out.left = b[0] as c_long;
    out.top = b[1] as c_long;
    out.right = b[2] as c_long;
    out.bottom = b[3] as c_long;
}

/// EEEffect::Move (the weather storms'): every tile one step towards its place around the new centre, the storm
/// recentred on the terrain there; the tiles' bounds written when it moved (ee_gfx effect_storm.rs).
///
/// # Safety
/// `effect` is a live effect, `bounds` its caller's rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_fx_storm_move(
    effect: *mut c_void,
    x: c_long,
    y: c_long,
    bounds: *mut EeGameCppTileBounds,
) {
    // SAFETY: the function's contract
    let out = unsafe { &mut *bounds };
    let mut b = [out.left as i32, out.top as i32, out.right as i32, out.bottom as i32];
    // SAFETY: the effect's head
    if unsafe { ee_gfx_effect_storm_move(effect as *mut EeGfxEffectHead, x as i32, y as i32, b.as_mut_ptr()) } {
        fx_put_bounds(out, &b);
    }
}

/// EEEffect::GetBounds: the storm's tiles' bounds, written when it has tiles.
///
/// # Safety
/// `record` is the effect's record, `bounds` its caller's rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_fx_storm_bounds(record: *const c_void, bounds: *mut EeGameCppTileBounds) {
    let mut b = [0i32; 4];
    // SAFETY: the function's contract
    if unsafe { ee_gfx_storm_bounds(record as *const EeGfxEffectRecord, b.as_mut_ptr()) } {
        // SAFETY: as above
        fx_put_bounds(unsafe { &mut *bounds }, &b);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_pointer_files_and_the_panel_corners() {
        assert_eq!(MO_POINTER_FILES.len(), 14);
        assert_eq!(MO_POINTER_FILES[0].to_bytes(), b"Textures\\Convert Pointer.png");
        assert_eq!(MO_POINTER_FILES[13].to_bytes(), b"Textures\\Attack Ground Pointer.png");
        assert!(MO_POINTER_FILES.iter().all(|n| n.to_bytes().starts_with(b"Textures\\")));
        assert_eq!(GU_PANEL_TOP, 0.716_666_64_f32);
        assert_eq!((1024.0f32 * GU_RIGHT_PANEL_LEFT), 847.36);
    }

    #[test]
    fn an_empty_map_has_no_pairs() {
        let p = EeGfxRfilPairs { data: std::ptr::null(), count: 0 };
        assert!(pairs(&p).is_empty());
        let mut r = EeGameCppTileBounds { bottom: 0, left: 0, right: 0, top: 0 };
        fx_put_bounds(&mut r, &[1, 2, 3, 4]);
        assert_eq!((r.left, r.top, r.right, r.bottom), (1, 2, 3, 4));
    }
}
