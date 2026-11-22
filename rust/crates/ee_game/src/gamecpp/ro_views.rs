//! The Graphics Thread's other render objects' bodies (were EEROUserInterface.cpp, EEUnitPicker.cpp's EEROWater,
//! EEROGroundShadows.cpp, EEROTerrain.cpp, EERenderSnapshot.cpp, EEMouse.cpp, EERenderCapture.cpp): the UI's frame,
//! z-buffer clears and polygons, the water's pass, the ground shadows' lists, the terrain's frame, sky media and
//! scene, the render snapshot, the mouse's extra pointers and RFIL's fill. Each C++ call or member is a case of
//! EERenderSnapshot.cpp's `EERoViewsOp` or EEMouse.cpp's `EEMouseOp`; the draws themselves are ee_gfx's.
//!
//! Only a game with a window runs the draws (the tour, gates.sh); RFIL's fill runs under its capture only.

use std::ffi::{c_long, c_void, CStr};
use std::sync::atomic::Ordering;

use ee_base::math::types::{GeRectangle, GeTransformation};
use ee_base::math::um::floor;
use ee_render::gfx::effect_obj::{ee_gfx_effect_queues_draw, ee_gfx_effect_storm_move, EeGfxEffectHead};
use ee_render::gfx::effect_state::EeGfxEffectRecord;
use ee_render::gfx::ffi_effect::ee_gfx_storm_bounds;
use ee_render::gfx::effects::EeGfxColor;
use ee_render::gfx::fill::{EeGfxFillObject, EeGfxFillUnit};
use ee_render::gfx::lighting::EeGfxLighting;
use ee_render::gfx::rfil::{ee_gfx_rfil_active, ee_gfx_rfil_write, EeGfxRfilPairs};
use ee_render::gfx::ro_terrain::{
    ee_gfx_ro_terrain_create, ee_gfx_ro_terrain_frame, ee_gfx_ro_terrain_release, ee_gfx_ro_terrain_render,
    EeGfxTerrainFrame, EeGfxTerrainMedia, EeGfxTerrainScene,
};
use ee_render::gfx::shadows::{ee_gfx_ground_shadows_render, EeGfxShadow, EeGfxShadowScene, EeGfxUnitShadow};

use super::render_object::ee_gamecpp_ro_lighting;
use super::ro_units::{lists_view, raster, RU_UNIT_PANEL_ACTIVE};
use super::{cptr, h, hd, hf, hp, pa, ptr, O, P};

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
    let mut zero = GeRectangle::default();
    h(O::RuMainViewRectangle, 0, &[ptr(&mut zero)]);
    if h(O::FreeCamera, 0, &[]) != 0 || h(O::GuMainUiState, 0, &[]) == GU_STATE_NO_PANELS {
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
    hf(O::GuClearZ, 0, &[cptr(&rectangle)], &[0.0]);
    if !RU_UNIT_PANEL_ACTIVE.load(Ordering::Relaxed) {
        return;
    }
    rectangle.left = 0.0;
    rectangle.top = height * GU_PANEL_TOP;
    rectangle.right = width * GU_UNIT_PANEL_RIGHT;
    rectangle.bottom = zero.bottom;
    hf(O::GuClearZ, 0, &[cptr(&rectangle)], &[0.0]);
}

/// EEROUserInterface::RenderScene: the UI's polygons over the whole render target with fog, lighting, alpha test and
/// z buffer off, the fog and alpha reference restored (lighting stays off).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gu_render_scene(ro: *mut c_void) {
    let fog = h(O::GuGetFog, 0, &[]);
    let alpha_ref = hd(O::GuGetAlphaRef, 0, &[], &[]);
    h(O::RoRastFog, 0, &[0]);
    h(O::GuShadingColorSource, 0, &[1]);
    h(O::RoRastLighting, 0, &[0]);
    hf(O::GuSetAlphaRef, 0, &[], &[0.0]);
    h(O::GuZBufferDisabled, 0, &[]);
    h(O::GuCullNone, 0, &[]);
    let rectangle = GeRectangle {
        left: 0.0,
        top: 0.0,
        right: h(O::GuTargetWidth, 0, &[]) as u32 as f32,
        bottom: h(O::GuTargetHeight, 0, &[]) as u32 as f32,
    };
    h(O::GuUiViewportSetRectangle, 0, &[cptr(&rectangle)]);
    h(O::GuSetUiViewport, 0, &[]);
    h(O::GuDrawPolygons, ro as P, &[]);
    h(O::RoRastFog, 0, &[fog]);
    hf(O::GuSetAlphaRef, 0, &[], &[alpha_ref]);
}

// ---- EEROWater

/// EEROWater::RenderScene: the water over the terrain (alpha-blended, z test only, no culling, no lighting, the
/// scene's fog), then the effects' water pass (queue 4); the alpha reference restored.
#[no_mangle]
pub extern "C" fn ee_gamecpp_wa_render_scene() {
    let alpha_ref = hd(O::GuGetAlphaRef, 0, &[], &[]);
    hf(O::GuSetAlphaRef, 0, &[], &[0.0]);
    h(O::WaBlending, 0, &[]);
    h(O::WaZTestOnly, 0, &[]);
    h(O::WaZCompareLessEqual, 0, &[]);
    h(O::GuShadingColorSource, 0, &[1]);
    h(O::GuCullNone, 0, &[]);
    ee_gamecpp_ro_lighting(EeGfxLighting::Off as u32);
    ee_gamecpp_ro_lighting(EeGfxLighting::SceneFog as u32);
    h(O::WaDrawWater, 0, &[]);
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(6) };
    hf(O::GuSetAlphaRef, 0, &[], &[alpha_ref]);
}

// ---- EEROGroundShadows

extern "C" fn gs_tile_water(_: *mut c_void, x: u32, y: u32) -> i32 {
    h(O::GsTileWater, 0, &[x as i64, y as i64]) as i32
}

extern "C" fn gs_effects(_: *mut c_void) {
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(2) };
}

/// EEROGroundShadows::RenderScene: the static shadows and the unit shadows (when ground shadows are on; not the
/// submerged units nor those without a shadow scale), then the effects' ground pass, by ee_gfx shadows.rs over the
/// frame's lists (`mShadows2`, `mUnits2` in their order).
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
        space_map: h(O::SnIsSpaceMap, 0, &[]) != 0,
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
        space_map: h(O::TeMapIsSpace, 0, &[]) != 0,
        cloud_offset,
        media: te_media(),
    };
    // SAFETY: the raster table and the frame live through the call
    let out = unsafe { ee_gfx_ro_terrain_frame(raster(), &frame) };
    // SAFETY: gU4E9220 (a GEColor: red, green, blue)
    unsafe { *(hp(O::RoFogColor, 0, &[]) as *mut EeGfxColor) = out.fog };
    super::render_object::set_cloud_offset(out.cloud_offset);
    super::render_object::ee_gamecpp_ro_fn10e8d1(ro);
    h(O::TeUpdateFogOfWar, 0, &[]);
    h(O::TeCopyGridRuns, 0, &[]);
    ee_gamecpp_sn_take_terrain();
}

/// EEROTerrain::CreateSkyModel: the sky dome, the stars, the clouds (when on) and the area-effect textures (ee_gfx
/// ro_terrain.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_create_sky_model() {
    let media = te_media();
    // SAFETY: the raster table and the media live through the call
    let textures = unsafe { ee_gfx_ro_terrain_create(raster(), &media) };
    h(O::TeSetAreaTexture, 0, &[pa(textures.area_effect as P)]);
    h(O::TeSetCircleTexture, 0, &[pa(textures.circle_area_effect as P)]);
}

/// EEROTerrain::ReleaseSkyModels: the sky, cloud and star models and textures, and the area-effect textures.
#[no_mangle]
pub extern "C" fn ee_gamecpp_te_release_sky_models() {
    // SAFETY: the raster table
    unsafe { ee_gfx_ro_terrain_release(raster()) };
    h(O::TeSetAreaTexture, 0, &[0]);
    h(O::TeSetCircleTexture, 0, &[0]);
}

extern "C" fn te_draw(_: *mut c_void) {
    h(O::TeDrawMesh, 0, &[]);
}

/// The editor's brush highlight and the map's coloured rectangles (the snapshot's).
extern "C" fn te_highlights(_: *mut c_void, opacity: f32) {
    if h(O::SnBrushShown, 0, &[]) != 0 {
        hf(O::TeDrawBrush, 0, &[], &[opacity]);
    }
    for i in 0..h(O::SnColoredCount, 0, &[]) {
        h(O::TeDrawColored, 0, &[i]);
    }
}

extern "C" fn te_grid(_: *mut c_void) {
    h(O::TeDrawGrid, 0, &[]);
}

extern "C" fn te_grid_lines(_: *mut c_void) {
    h(O::TeDrawGridLines, 0, &[]);
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
    let scene = EeGfxTerrainScene {
        context: std::ptr::null_mut(),
        space_map: h(O::SnIsSpaceMap, 0, &[]) != 0,
        draw_clouds: h(O::TeDrawClouds, 0, &[]) != 0,
        orthographic: h(O::RoOrthographic, 0, &[]) != 0,
        grid: h(O::TeGridMode, ro as P, &[]) != 0,
        grid_runs: h(O::TeGridRunCount, 0, &[]) as usize,
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

// ---- the render snapshot (ee/EERenderSnapshot.h)

/// EETakeRenderSnapshot: the camera, its yaw and pitch, the cinematic, the terrain mesh and the space map, as the
/// render phase reads them after the world's lock is released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_sn_take() {
    h(O::SnCamera, 0, &[]);
    h(O::SnCameraYaw, 0, &[]);
    h(O::SnCameraPitch, 0, &[]);
    h(O::SnCinematic, 0, &[]);
    let map = hp(O::SnMap, 0, &[]);
    let mesh = if map != 0 { hp(O::SnMeshOf, map, &[]) } else { 0 };
    h(O::SnSetMesh, 0, &[pa(mesh)]);
    let space = map != 0 && h(O::TeMeshIsSpace, mesh, &[]) != 0;
    h(O::SnSetSpaceMap, 0, &[space as i64]);
}

/// EETakeTerrainSnapshot (EEROTerrain's frame): the map's coloured rectangles back to front under their lock (a zero
/// opacity erases the element; the others are kept in that order) and the editor's brush highlight.
#[no_mangle]
pub extern "C" fn ee_gamecpp_sn_take_terrain() {
    let map = hp(O::SnMap, 0, &[]);
    h(O::SnColoredClear, 0, &[]);
    let count = h(O::SnMapColoredCount, map, &[]);
    if count > 0 {
        h(O::SnMapColoredLock, map, &[]);
        for i in (0..count).rev() {
            if hd(O::SnMapColoredOpacity, map, &[i], &[]) != 0.0 {
                h(O::SnColoredPush, map, &[i]);
            } else {
                h(O::SnMapColoredErase, map, &[i]);
            }
        }
        h(O::SnMapColoredUnlock, map, &[]);
    }
    h(O::SnSetBrushShown, 0, &[0]);
    if h(O::SnEditorPresent, 0, &[]) != 0 {
        let mut r = GeRectangle::default();
        h(O::SnBrushRectangle, 0, &[ptr(&mut r)]);
        if h(O::SnBrushHighlightShown, 0, &[]) != 0 {
            h(O::SnSetBrushShown, 0, &[1]);
            let tiles = [floor(r.left), floor(r.top), floor(r.right), floor(r.bottom)];
            h(O::SnSetBrushTiles, 0, &[tiles[0] as i64, tiles[1] as i64, tiles[2] as i64, tiles[3] as i64]);
        }
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

/// EEMouse::Load: the 14 extra pointer images, then the engine's pointers; the first load error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mo_load(mouse: *mut c_void, screen: *mut c_void) -> c_long {
    let mut result: c_long = 0;
    for (i, name) in MO_POINTER_FILES.iter().enumerate() {
        if result != 0 {
            break;
        }
        result = h(O::MoLoadPointer, mouse as P, &[name.as_ptr() as usize as i64, pa(screen as P), i as i64]) as c_long;
    }
    if result == 0 {
        result = h(O::MoRtsLoad, mouse as P, &[pa(screen as P)]) as c_long;
    }
    result
}

/// EEMouse::Release: the extra pointer images, then the engine's; the engine's result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mo_release(mouse: *mut c_void) -> c_long {
    h(O::MoReleaseExtra, mouse as P, &[]);
    h(O::MoRtsRelease, mouse as P, &[]) as c_long
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
    f.flags = (if h(O::FreeCamera, 0, &[]) != 0 { 1 } else { 0 })
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
pub unsafe extern "C" fn ee_gamecpp_fx_storm_move(effect: *mut c_void, x: c_long, y: c_long, bounds: *mut EeGameCppTileBounds) {
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
