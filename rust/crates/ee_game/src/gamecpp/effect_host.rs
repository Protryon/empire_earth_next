//! The effects' world host's C++ answers in Rust (game core lane, round 9; was EEEffectHost.cpp's `Ask`,
//! `EEGoalOps`, `EEStormHostOps`, `EEDamagedTick`, the world tables' multi-statement entries, the capture's records,
//! the volcano's build, the quad batches' arrays and the battlefield decals' entry points). Each C++ call or member is
//! a case of EEEffectHost.cpp's `EECore9OpB` ([`super::EeGameCpp`]'s lane B section); the C++ tables name these
//! functions (`ee_gamecpp_fx_*`).
//!
//! The effects run in EE-SIM (the oracle, the `effects` and `calamity` captures); the volcano's image and model, the
//! quad batches and the goal markers' drawing only in a game with a window.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicUsize, Ordering};

use ee_audio::sound::managerobj::ee_sound_manager_node_set_parameters;
use ee_audio::sound::objects::EeSoundNode;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um;
use ee_render::dx7::api::SurfaceDescription;
use ee_render::gfx::effect_confirm::EeGfxConfirmGoal;
use ee_render::gfx::effect_glow::EeGfxGlowArea;
use ee_render::gfx::effect_obj::{
    ee_gfx_effect_part_object, ee_gfx_effect_set_transformation, ee_gfx_effects_decal_impact,
    ee_gfx_effects_decal_track, EeGfxEffectAsk, EeGfxEffectHead, EffectPtr,
};
use ee_render::gfx::effect_placement::{EE_GFX_TRACK_GIRAFFE, EE_GFX_TRACK_HORSE};
use ee_render::gfx::effect_sprite::EeGfx2DPoint;
use ee_render::gfx::effect_state::EeGfxDamagedTick;
use ee_render::gfx::effect_weather::{EE_GFX_RAIN_FADE_SHARE, EE_GFX_RAIN_FADE_TIME};
use ee_render::gfx::effects::{EeGfxColor, EeGfxEffectQuad};
use ee_render::gfx::ffi_manager::EeGfxCaptureOp;
use ee_render::gfx::ffi_mesh::{
    ee_gfx_volcano_build, ee_gfx_volcano_image, ee_gfx_volcano_sizes, EeGfxFarmTerrain, EeGfxVolcanoTerrain,
};
use ee_render::gfx::goal_markers::{EeGfxGoalPlayer, EeGfxGoalStep, EeGfxGoalUnit};
use ee_render::gfx::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex};
use ee_sim::app::capture::{
    ee_app_capture_active, ee_app_capture_commit, ee_app_capture_exclusive, ee_app_capture_keeps, ee_app_capture_only,
};
use ee_sim::units::uu_adapt::ee_units_uu_next_tile;
use ee_sim::units::uu_ffi::{
    ee_units_uu_can_stand_on, ee_units_uu_farm_drop_sites, ee_units_uu_tile_direction, ee_units_uu_units_on_tile,
};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::{cptr, h, hd, hf, hp, pa, position, ptr, rec, set_position, Owned, O, P};

// ---- the constants the C++ cases name (static_assert'd in EEEffectHost.cpp)

/// `kTerrainDeepWater`, `kTerrainShallowWater`.
const FX_TERRAIN_DEEP_WATER: i64 = 1;
const FX_TERRAIN_SHALLOW_WATER: i64 = 10;
/// `kUnitCategoryGate`.
const FX_CATEGORY_GATE: c_ulong = 0x19;
/// `kUnitTypeWallSpace`, `kUnitTypeGateSpace`, `kUnitTypeTownCenter`, `kUnitTypeCapitol`, `kUnitTypeResGiraffe`,
/// `kUnitTypeResHorse`.
const FX_TYPE_WALL_SPACE: c_ulong = 0x2fe;
const FX_TYPE_GATE_SPACE: c_ulong = 0x2ff;
const FX_TYPE_TOWN_CENTER: c_ulong = 0x23;
const FX_TYPE_CAPITOL: c_ulong = 0x29;
const FX_TYPE_GIRAFFE: c_ulong = 0x124;
const FX_TYPE_HORSE: c_ulong = 0x175;
/// `kGoalLocation`, `kGoalAttackUnit`, `kGoalUnloadTransport`, `kGoalLoadGarrison`.
const FX_GOAL_LOCATION: i64 = 1;
const FX_GOAL_ATTACK_UNIT: i64 = 2;
const FX_GOAL_UNLOAD_TRANSPORT: i64 = 8;
const FX_GOAL_LOAD_GARRISON: i64 = 12;
/// `kTechEpochAtomicWW2`.
const FX_EPOCH_ATOMIC_WW2: i64 = 10;
/// `kPixelFormatA8R8G8B8`, `kSurfaceSystemMemory`.
const FX_PIXEL_FORMAT_A8R8G8B8: c_ulong = 0;
const FX_SURFACE_SYSTEM_MEMORY: c_ulong = 4;

// ---- reads

/// The effect of an answer's context (the call's `EeGfxEffectAsk`).
fn eff(ask: *mut c_void) -> EffectPtr {
    // SAFETY: the context of every answer is the live ask of the effect's call
    unsafe { (*(ask as *const EeGfxEffectAsk)).effect }
}

/// The effect's head, read or written in place (no C++ call while it is held).
fn head<'a>(e: EffectPtr) -> &'a mut EeGfxEffectHead {
    // SAFETY: a live effect (the C++ `EEEffect` is this head)
    unsafe { &mut *e }
}

fn unit_fields<'a>(u: P) -> &'a EeUnitsUnitFields {
    // SAFETY: a live unit's record
    unsafe { &*rec::<EeUnitsUnitFields>(O::UnFields, u) }
}

fn rts_fields<'a>(u: P) -> &'a EeUnitsRtsUnitFields {
    // SAFETY: a live unit's record
    unsafe { &*rec::<EeUnitsRtsUnitFields>(O::UnRts, u) }
}

fn cx_fields<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: a live complex unit's record
    unsafe { &mut *rec::<EeUnitsComplexUnitFields>(O::CxFields, cx) }
}

fn class_fields<'a>(uc: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: a live unit class's record
    unsafe { &*rec::<EeUnitsUnitClassFields>(O::UcFields, uc) }
}

fn player_id(pl: P) -> c_ulong {
    // SAFETY: a live player's record
    unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID }
}

/// `GetPosition` of a unit.
fn unit_position(u: P) -> Ge3DPoint {
    let mut at = Ge3DPoint::default();
    h(O::UnPosition, u, &[ptr(&mut at)]);
    at
}

/// A complex unit's physical model's location.
fn cx_location(cx: P) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    h(O::CxLocation, cx, &[ptr(&mut t)]);
    t
}

/// `gWorld.mMap->GetTile(UMFloor(x), UMFloor(y))`.
fn tile_under(x: f32, y: f32) -> P {
    hp(O::MapTile, 0, &[um::floor(x) as i64, um::floor(y) as i64])
}

/// `gWorld.mMap->GetGroundPoint(at, out)`.
fn ground_point(at: &Ge3DPoint) -> Ge3DPoint {
    super::map::world_ground_point(at)
}

/// `PlayAnimationSound(at, object, 0, sound, frame, start)`: a referenced node.
fn play_animation_sound(at: &Ge3DPoint, object: P, sound: u64, frame: u32, start: bool) -> u64 {
    h(O::FxPlayAnimationSound, 0, &[cptr(at), pa(object), sound as i64, frame as i64, start as i64]) as u64
}

/// The effect's sound slot replaced by a node (referenced), then the slot's reference dropped (`EESetEffectSound`,
/// `Ask::KeepSound`: the original's order).
fn set_sound(slot: &mut u64, sound: u64) {
    if sound != *slot {
        if *slot != 0 {
            h(O::NodeDereference, *slot as P, &[]);
        }
        *slot = sound;
        if sound != 0 {
            h(O::NodeReference, sound as P, &[]);
        }
    }
    if *slot != 0 {
        h(O::NodeDereference, *slot as P, &[]);
    }
}

/// The effect's own sound slot (`mSound`) replaced.
fn keep_sound(e: EffectPtr, sound: u64) {
    let mut slot = head(e).sound;
    set_sound(&mut slot, sound);
    head(e).sound = slot;
}

/// A rain sound's volume, pan and pitch from the camera's distance to the storm (range 5 times its radius), scaled by
/// `volume` and by 0.4 in the first 6.5 s (`EESetRainSoundLevel`, EEE3DRain::SetSoundLevel 0xf80e8).
fn rain_sound_level(e: EffectPtr, sound: u64, volume: f32) {
    if sound == 0 {
        return;
    }
    // SAFETY: the effect's record
    let (at, radius) = unsafe { (position(&(*head(e).record).transformation), (*head(e).record).bounding_radius) };
    let mut out = [0f32; 3];
    let [o0, o1, o2] = &mut out;
    hf(O::FxComputeSound, 0, &[cptr(&at), ptr(o0), ptr(o1), ptr(o2)], &[1.0, radius * 5.0]);
    let (mut v, pan, pitch) = (out[0], out[1], out[2]);
    v *= volume;
    // SAFETY: as above
    if unsafe { (*head(e).record).storm.elapsed } < EE_GFX_RAIN_FADE_TIME {
        v = EE_GFX_RAIN_FADE_SHARE * v;
    }
    // SAFETY: gSoundManager (the Rust manager itself) and the effect's live sound node
    unsafe {
        ee_sound_manager_node_set_parameters(
            super::globals::gSoundManager.cast(),
            sound as *mut EeSoundNode,
            v,
            pan,
            pitch,
        )
    };
}

// ---- the weather storms' map (EeGfxStormHost)

/// The tile is deep or shallow water (false off the map).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_storm_is_water(_: *mut c_void, x: i32, y: i32) -> bool {
    let t = hp(O::MapTile, 0, &[x as i64, y as i64]);
    if t == 0 {
        return false;
    }
    let kind = h(O::FxTileTerrainType, t, &[]);
    kind == FX_TERRAIN_DEEP_WATER || kind == FX_TERRAIN_SHALLOW_WATER
}

/// `EEGetNextTile(from, EEGetTileDirection(from, to), 1)` into `out`.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_storm_step_toward(_: *mut c_void, fx: i32, fy: i32, tx: i32, ty: i32, out: *mut i32) {
    let mut tile: [c_long; 2] = [0, 0];
    let direction = ee_units_uu_tile_direction(fx, fy, tx, ty);
    let [x, y] = &mut tile;
    // SAFETY: the two outputs
    unsafe { ee_units_uu_next_tile(fx as c_long, fy as c_long, direction as c_long, 1, x, y) };
    // SAFETY: out holds two int32_t
    unsafe {
        *out = tile[0] as i32;
        *out.add(1) = tile[1] as i32;
    }
}

/// The land ripple where a drop hit the water.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_storm_ripple(_: *mut c_void, x: f32, y: f32) {
    let splash = Ge3DPoint::new(x, y, 0.0);
    let ripple = hp(O::FxLandRipple, 0, &[]);
    h(O::FxWorldPlacedEffect, 0, &[cptr(&splash), pa(ripple)]);
}

// ---- the terrain under a volcano and a farm field (their contexts: the world's map and its mesh)

unsafe extern "C" fn volcano_z(_: *mut c_void, x: u32, y: u32) -> f32 {
    hd(O::MeshGetZ, 0, &[x as i64, y as i64], &[])
}

unsafe extern "C" fn volcano_color(_: *mut c_void, x: u32, y: u32) -> EeGfxColor {
    let mut c = EeGfxColor::default();
    h(O::FxTerrainColor, 0, &[x as i64, y as i64, ptr(&mut c)]);
    c
}

unsafe extern "C" fn volcano_valid_tile(_: *mut c_void, x: u32, y: u32) -> bool {
    h(O::MeshValidTile, 0, &[x as i64, y as i64]) != 0
}

unsafe extern "C" fn volcano_light(_: *mut c_void, x: i32, y: i32, corner: i32) -> EeGfxColor {
    let mut c = EeGfxColor::default();
    h(O::FxVertexColor, 0, &[x as i64, y as i64, corner as i64, ptr(&mut c)]);
    c
}

unsafe extern "C" fn farm_z(_: *mut c_void, x: u32, y: u32) -> f32 {
    hd(O::MeshGetZ, 0, &[x as i64, y as i64], &[])
}

unsafe extern "C" fn farm_normal(_: *mut c_void, x: u32, y: u32) -> GeVector {
    let mut n = GeVector::default();
    h(O::FxVertexNormal, 0, &[x as i64, y as i64, ptr(&mut n)]);
    n
}

unsafe extern "C" fn farm_mesh_z(_: *mut c_void, x: f32, y: f32) -> f32 {
    hd(O::MeshZ, 0, &[], &[x, y])
}

// ---- the 3D kinds' answers (EeGfxEffectAnswers3D)

/// The frame's sound at the unit, kept as the effect's (EEE3DDisease::Step and SetSound, 0xec6bd).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_animation_sound(ask: *mut c_void, frame: u32, changed: bool) {
    let e = eff(ask);
    let cx = hp(O::UnComplex, head(e).unit as P, &[]);
    let at = position(&cx_location(cx));
    let (object, sound) = (head(e).graphic_object as P, head(e).sound);
    let node = play_animation_sound(&at, object, sound, frame, changed);
    keep_sound(e, node);
}

/// The energy wall's posts on a gate: its two end parts (EEE3DEnergyWall::Place, 0xed784).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_wall_gate_posts(
    ask: *mut c_void,
    first: *mut Ge3DPoint,
    second: *mut Ge3DPoint,
) -> bool {
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    let class = hp(O::FxCxGetClass, cx, &[]);
    if class_fields(class).mCategory != FX_CATEGORY_GATE {
        return false;
    }
    let a = hp(O::FxGatePart, class, &[pa(cx), 0]);
    let b = hp(O::FxGatePart, class, &[pa(cx), 4]);
    let from = unit_position(a);
    let to = unit_position(b);
    // SAFETY: the outputs are the caller's points
    unsafe {
        *first = from;
        *second = to;
    }
    true
}

/// The height of the last living, finished wall piece or gate side part of the wall's owner on a tile (0xee00c).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_wall_piece(ask: *mut c_void, x: i32, y: i32, z: *mut f32) -> bool {
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    let owner = player_id(hp(O::UnPlayer, cx, &[]));
    let mut found = false;
    let units = Owned::new(O::VecUnitsNew, O::VecUnitsDelete);
    // SAFETY: a live std::vector<EEUnit*>
    unsafe { ee_units_uu_units_on_tile(x, y, units.0 as *mut c_void) };
    let count = h(O::VecUnitsCount, units.0, &[]);
    for j in 0..count {
        let other = hp(O::FxDynComplex, hp(O::VecUnitsAt, units.0, &[j]), &[]);
        if other == 0
            || player_id(hp(O::UnPlayer, other, &[])) != owner
            || unit_fields(other).mHitPoints <= 0
            || h(O::FxIsFoundation, other, &[]) != 0
        {
            continue;
        }
        let kind = class_fields(hp(O::FxCxGetClass, other, &[])).mUnitType;
        if kind == FX_TYPE_WALL_SPACE || (kind == FX_TYPE_GATE_SPACE && h(O::FxCxIsSidePart, other, &[]) != 0) {
            let at = unit_position(other);
            // SAFETY: the caller's float
            unsafe { *z = at.z };
            found = true;
        }
    }
    found
}

/// The volcano's model over the record's mesh lists (EEE3DVolcano::BuildModel, 0xfe79d): each tile of its area split
/// 3x3, following the terrain, coloured by terrain and lighting and blended into textures\Volcano.png near the centre,
/// and the cone heights from the image's alpha (ee_gfx volcano.rs); the model made and referenced, placed at its
/// centre, the bounding radius to the cone's top, the texture.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_volcano_build(
    ask: *mut c_void,
    points: *mut Ge3DPoint,
    heights: *mut f32,
    count: u32,
) -> i32 {
    let e = eff(ask);
    let record = head(e).record;
    let height_scale = hd(O::FxGeffSize, head(e).graphic_effect as P, &[], &[]);
    // SAFETY: the effect's record
    let area = unsafe { (*record).volcano.area };
    let sizes = ee_gfx_volcano_sizes(area.right - area.left);
    let mut colors = vec![EeGfxColor::default(); sizes.vertices as usize];
    let description = SurfaceDescription {
        mWidth: 19,
        mHeight: 19,
        mIs3DRenderable: false,
        mIsTransparencyEnabled: false,
        mPixelFormat: FX_PIXEL_FORMAT_A8R8G8B8,
        mSurfaceVolatility: 0,
        m14: 0,
        mMipLevelCount: 0,
        mType: FX_SURFACE_SYSTEM_MEMORY,
        mPixelDepth: 32,
        mNoAlpha: false,
    };
    let mut surface: P = 0;
    let mut result = h(O::FxCreateSurface, 0, &[cptr(&description), ptr(&mut surface)]);
    h(O::Verify, 0, &[(result == 0) as i64, 233]);
    let mut file: P = 0;
    result = h(O::FxFindTexture, 0, &[c"textures\\Volcano.png".as_ptr() as i64, ptr(&mut file)]);
    h(O::Verify, 0, &[(result == 0) as i64, 238]);
    h(O::Verify, 0, &[(file != 0) as i64, 239]);
    h(O::FxSurfaceLoadPng, surface, &[pa(file)]);
    h(O::Verify, 0, &[(result == 0) as i64, 241]);
    if file != 0 {
        h(O::FxFileSpecDelete, file, &[]);
    }
    h(O::FxSurfaceLock, surface, &[]);
    let pixels = hp(O::FxSurfaceMemory, surface, &[]) as *const u32;
    let row = (h(O::FxSurfaceBytesPerRow, surface, &[]) as c_long / 4) as usize;
    let colors_ptr = if colors.is_empty() { std::ptr::null_mut() } else { colors.as_mut_ptr() };
    let heights = if count == 0 { std::ptr::null_mut() } else { heights };
    let points = if count == 0 { std::ptr::null_mut() } else { points };
    // SAFETY: the locked image (19 rows) and the arrays of their counts
    unsafe { ee_gfx_volcano_image(height_scale, sizes.side, pixels, row, colors_ptr, heights, colors.len()) };
    h(O::FxSurfaceUnlock, surface, &[]);
    h(O::FxSurfaceDereference, surface, &[]);
    let model = hp(O::FxCreateVolcanoModel, 0, &[sizes.polygons as i64, sizes.vertices as i64]);
    head(e).volcano_model = model as u64;
    h(O::FxModelReference, model, &[]);
    h(O::FxMaterialPolygonStart, model, &[]);
    h(O::FxMaterialPolygonLength, model, &[sizes.polygons as i64]);
    h(O::FxMaterialVertexStart, model, &[]);
    h(O::FxMaterialVertexLength, model, &[sizes.vertices as i64]);
    let terrain = EeGfxVolcanoTerrain {
        context: std::ptr::null_mut(),
        z: volcano_z,
        color: volcano_color,
        valid_tile: volcano_valid_tile,
        light: volcano_light,
    };
    let mut vertices = vec![EeGfxMeshVertex::default(); sizes.vertices as c_ulong as usize];
    let mut triangles = vec![EeGfxMeshTriangle::default(); sizes.polygons as c_ulong as usize];
    let mesh_ptr = |v: &mut Vec<EeGfxMeshVertex>| if v.is_empty() { std::ptr::null_mut() } else { v.as_mut_ptr() };
    let tri_ptr = |t: &mut Vec<EeGfxMeshTriangle>| if t.is_empty() { std::ptr::null_mut() } else { t.as_mut_ptr() };
    let (vp, tp) = (mesh_ptr(&mut vertices), tri_ptr(&mut triangles));
    // SAFETY: the area, the colours and the arrays of their counts
    let built = unsafe {
        ee_gfx_volcano_build(
            &area,
            colors_ptr,
            terrain,
            points,
            heights,
            colors.len(),
            vp,
            vertices.len(),
            tp,
            triangles.len(),
        )
    };
    h(O::FxStoreMesh, model, &[vp as i64, vertices.len() as i64, tp as i64, triangles.len() as i64]);
    // SAFETY: the effect's record (no C++ call between)
    unsafe { (*record).volcano.area.center.z = built.center_z };
    let maximum = built.maximum;
    h(O::FxModelComputeNormals, model, &[]);
    h(O::FxModelComputeBounds, model, &[]);
    let mut transformation = GeTransformation::IDENTITY;
    h(O::FxModelSetPostTransform, model, &[cptr(&transformation)]);
    h(O::FxModelPrepare, model, &[]);
    let mut center = Ge3DPoint::default();
    h(O::FxModelCenter, model, &[ptr(&mut center)]);
    set_position(&mut transformation, center.x, center.y, center.z);
    ee_gfx_effect_set_transformation(e, transformation);
    let top = Ge3DPoint::new(center.x, center.y, height_scale + maximum);
    let mut radius = center.distance_to(top);
    let bound = hd(O::GeModelRadius, model, &[0], &[]);
    if radius <= bound {
        radius = bound;
    }
    // SAFETY: the effect's record
    unsafe { (*record).bounding_radius = radius };
    if result == 0 {
        h(O::FxModelSetTexture, model, &[head(e).graphic_object as i64]);
    }
    result as i32
}

/// The terrain under a farm field (the world's mesh).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_farm_terrain(_: *mut c_void) -> EeGfxFarmTerrain {
    EeGfxFarmTerrain { context: std::ptr::null_mut(), z: farm_z, normal: farm_normal, mesh_z: farm_mesh_z }
}

/// The farm's drop site: the first drop site's tile.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_farm_drop_site(ask: *mut c_void, x: *mut i32, y: *mut i32) -> bool {
    let units = Owned::new(O::VecUnitsNew, O::VecUnitsDelete);
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    // SAFETY: a live std::vector<EEUnit*>
    if unsafe { ee_units_uu_farm_drop_sites(cx as u64, units.0 as *mut c_void) } == 0 {
        return false;
    }
    let first = hp(O::UnComplex, hp(O::VecUnitsAt, units.0, &[0]), &[]);
    let (tx, ty) = (h(O::FxCxTileX, first, &[]), h(O::FxCxTileY, first, &[]));
    // SAFETY: the caller's ints
    unsafe {
        *x = tx as i32;
        *y = ty as i32;
    }
    true
}

/// The current player's doppelganger of the farm: 1 built, 0 not, -1 none (or the free camera, or a player who sees
/// everything).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_farm_doppelganger(ask: *mut c_void) -> i32 {
    let unit = head(eff(ask)).unit as P;
    let current = hp(O::WorldCurrentPlayer, 0, &[]);
    let mut d: P = 0;
    let tile = hp(O::FxUnitTile, unit, &[]);
    if tile != 0 {
        d = hp(O::FxDoppelgangerGet, tile, &[pa(unit), player_id(current) as i64]);
    }
    // SAFETY: the current player's record
    let sees_all = unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, current)).mSeesAll };
    if d != 0 && h(O::FxFreeCamera, 0, &[]) == 0 && !sees_all {
        return if h(O::FxDoppelgangerIsBuilt, d, &[]) != 0 { 1 } else { 0 };
    }
    -1
}

/// The farm's draw opacity (Fn23CEE2).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_farm_opacity(ask: *mut c_void, opacity: f32) -> f32 {
    let mut o = opacity;
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    h(O::FxUnitOpacity, cx, &[ptr(&mut o)]);
    o
}

/// The earthquake's ground under a point (EEE3DEarthquake, 0xec6e4..0xed606).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_ground_point(_: *mut c_void, point: *const Ge3DPoint) -> Ge3DPoint {
    // SAFETY: the caller's point
    let g = unsafe { *point };
    super::map::world_ground_point(&g)
}

/// The rain's sounds (EEE3DRain::StartSounds, 0xf8195): the loop at the start or after a load, the thunder `rumble`
/// (1 or 2), both levels.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_rain_sounds(ask: *mut c_void, start: bool, rumble: i32) {
    let e = eff(ask);
    // SAFETY: the effect's record
    let ids = unsafe { (*head(e).record).storm.sounds };
    if start {
        let sound = hp(O::SoundObjectGet, 0, &[ids[0] as i64]);
        if sound != 0 {
            let node = h(O::FxPlayNodeNamed, sound, &[c"3DRain".as_ptr() as i64]) as u64;
            let mut slot = head(e).sound;
            set_sound(&mut slot, node);
            head(e).sound = slot;
        }
    }
    if rumble != 0 {
        let sound = hp(O::SoundObjectGet, 0, &[ids[rumble as usize] as i64]);
        if sound != 0 {
            let node = h(O::FxPlayNodeNamed, sound, &[c"3D Rain Rumble".as_ptr() as i64]) as u64;
            let mut slot = head(e).rumble;
            set_sound(&mut slot, node);
            head(e).rumble = slot;
        }
    }
    rain_sound_level(e, head(e).sound, 0.5);
    rain_sound_level(e, head(e).rumble, 1.0);
}

/// The emitter's animation sound at its place (its graphic effect's object, if any).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_emitter_sound(ask: *mut c_void, location: *const Ge3DPoint, first: bool) {
    let e = eff(ask);
    let object = hp(O::FxGeffObject, head(e).graphic_effect as P, &[]);
    if object != 0 {
        // SAFETY: the caller's point
        let at = unsafe { *location };
        let node = play_animation_sound(&at, object, head(e).sound, 0, first);
        keep_sound(e, node);
    }
}

// ---- the 2D kinds' answers (EeGfxEffectAnswers2D)

/// The goal a state-0 confirmation shows: the last goal of the unit's goal queue as a location goal (on the tile at
/// the effect: blocked when the unit cannot stand there and its player sees it; forced; a move), else whether its
/// active goal unloads a transport (EEE3DCommandConfirmation::SetUp).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_confirm_goal(ask: *mut c_void, queue: u32, tx: i32, ty: i32) -> EeGfxConfirmGoal {
    let unit = head(eff(ask)).unit as P;
    if unit == 0 {
        return EeGfxConfirmGoal::NoUnit;
    }
    let cx = hp(O::UnComplex, unit, &[]);
    if cx == 0 {
        return EeGfxConfirmGoal::NoUnit;
    }
    let goals = h(O::FxQueueGoalCount, cx, &[queue as i64]);
    let goal = if goals == 0 {
        0
    } else {
        hp(O::FxLocationGoalCast, hp(O::FxQueueGoalAt, cx, &[queue as i64, goals - 1]), &[])
    };
    if goal != 0 {
        let tile = hp(O::MapTile, 0, &[tx as i64, ty as i64]);
        if ee_units_uu_can_stand_on(tx, ty, cx as u64) == 0
            && tile != 0
            && h(O::FxTileVisibleTo, tile, &[pa(hp(O::UnPlayer, unit, &[]))]) != 0
        {
            return EeGfxConfirmGoal::Blocked;
        }
        return if h(O::FxGoalForcedMove, goal, &[]) != 0 { EeGfxConfirmGoal::Forced } else { EeGfxConfirmGoal::Move };
    }
    let active = hp(O::FxCxActiveGoal, cx, &[]);
    if active != 0 && h(O::GoalKind, active, &[]) == FX_GOAL_UNLOAD_TRANSPORT {
        EeGfxConfirmGoal::Unloading
    } else {
        EeGfxConfirmGoal::Other
    }
}

/// The kept object's (`part0`: the first part's) animation sound for the frame at the location, kept as the effect's
/// (EEE2DImpact::Step and SetSound, 0xe166b / 0xe17ed).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_part_sound(ask: *mut c_void, at: Ge3DPoint, frame: u32, changed: bool, part0: bool) {
    let e = eff(ask);
    let object = if part0 { ee_gfx_effect_part_object(e, 0) as P } else { head(e).graphic_object as P };
    let node = play_animation_sound(&at, object, head(e).sound, frame, changed);
    keep_sound(e, node);
}

/// The unit's area effects count set (`FnE123D` at 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_set_area_effects(ask: *mut c_void, count: i32) {
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    cx_fields(cx).mNumAreaEffects = count as c_ulong;
    if count == 0 {
        h(O::FxCxFnE123D, cx, &[]);
    }
}

/// The unit's type is in its player's active group; a town centre or capitol also with 0x23 or 0x29 there.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_icons_selected(ask: *mut c_void) -> bool {
    let cx = hp(O::UnComplex, head(eff(ask)).unit as P, &[]);
    let kind = class_fields(hp(O::UnClass, cx, &[])).mUnitType;
    let group = hp(O::PlActiveGroup, hp(O::UnPlayer, cx, &[]), &[]);
    let mut selected = if group != 0 { h(O::FxGroupHasType, group, &[kind as i64]) != 0 } else { false };
    if group != 0
        && (kind == FX_TYPE_TOWN_CENTER || kind == FX_TYPE_CAPITOL)
        && (h(O::FxGroupHasType, group, &[0x23]) != 0 || h(O::FxGroupHasType, group, &[0x29]) != 0)
    {
        selected = true;
    }
    selected
}

/// The ground point under `at` when its tile is on the map.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_ground_at(_: *mut c_void, at: Ge3DPoint) -> EeGfx2DPoint {
    let mut p = EeGfx2DPoint::default();
    if tile_under(at.x, at.y) == 0 {
        return p;
    }
    p.point = ground_point(&at);
    p.has = true;
    p
}

/// The glow's area: the visible terrain's bottom left and size, the unit's tile.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_glow_area(ask: *mut c_void) -> EeGfxGlowArea {
    let (mut x, mut y, mut w, mut ht): (c_ulong, c_ulong, c_ulong, c_ulong) = (0, 0, 0, 0);
    h(O::FxVisibleBottomLeft, 0, &[ptr(&mut x), ptr(&mut y)]);
    h(O::FxVisibleDimensions, 0, &[ptr(&mut w), ptr(&mut ht)]);
    let unit = head(eff(ask)).unit as P;
    let r = rts_fields(unit);
    EeGfxGlowArea {
        x: x as u32,
        y: y as u32,
        width: w as u32,
        height: ht as u32,
        tile_x: r.mTileX as i32,
        tile_y: r.mTileY as i32,
    }
}

// ---- the damage controller's tick facts (EeGfxManagerWorld::damaged_tick)

/// EEEDamagedEffect::Step's facts (0x10225c): its graphic effect's, then its unit's (the outer part on the map), the
/// damage tag points of linked effect 1's kind, its health, hidden when cloaked to the current player's enemy.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_damaged_tick(e: EffectPtr) -> EeGfxDamagedTick {
    let mut tick = EeGfxDamagedTick::default();
    let geff = head(e).graphic_effect as P;
    tick.has_linked2 = hp(O::FxGeffLinked2, geff, &[]) != 0;
    tick.param2 = hd(O::FxGeffParam2, geff, &[], &[]);
    let mut unit = head(e).unit as P;
    // SAFETY: the effect's record
    if unit != 0 && !unsafe { (*head(e).record).damaged.finished } {
        tick.has_unit = true;
        if hp(O::UnTileNode, unit, &[]) != 0 {
            loop {
                let previous = hp(O::CxPreviousPart, hp(O::UnComplex, unit, &[]), &[]);
                if previous == 0 {
                    break;
                }
                unit = previous;
            }
        }
        let model = hp(O::UnGetModel, unit, &[]);
        let kind = h(O::FxGeffLinked1TagKind, geff, &[]);
        let list = if hp(O::UnTileNode, unit, &[]) != 0 {
            let animation = h(O::FxCxGetAnimation, hp(O::UnComplex, unit, &[]), &[]);
            hp(O::FxTagPoints, model, &[animation, kind])
        } else {
            hp(O::FxTagPoints, model, &[0, kind])
        };
        tick.tag_count = if list != 0 { h(O::FxTagCount, list, &[]) as u32 } else { 1 };
        tick.hit_points = h(O::FxUnGetHitPoints, unit, &[]) as i32;
        tick.max_hit_points = h(O::FxUnMaxHitPoints, unit, &[]) as i32;
        if hp(O::UnTileNode, unit, &[]) != 0 {
            let cx = hp(O::UnGetComplex, unit, &[]);
            if h(O::FxCxIsCloaked, cx, &[]) != 0
                && h(O::FxPlIsAlly, hp(O::WorldCurrentPlayer, 0, &[]), &[pa(hp(O::FxCxGetPlayer, cx, &[]))]) == 0
            {
                tick.hidden = true;
            }
        }
    }
    tick
}

// ---- the goal markers' questions (EeGfxGoalHost; the selection is the last player asked)

/// `EEGoalOps::sGroup`: the selected group of the last player asked.
static GOAL_GROUP: AtomicUsize = AtomicUsize::new(0);

fn goal_unit(unit: u32) -> P {
    hp(O::FxGroupUnit, GOAL_GROUP.load(Ordering::Relaxed), &[unit as i64])
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_player(_: *mut c_void, player: u32) -> EeGfxGoalPlayer {
    let pl = hp(O::WorldPlayer, 0, &[player as i64]);
    let group = hp(O::FxPlSelectedGroup, pl, &[]);
    GOAL_GROUP.store(group, Ordering::Relaxed);
    // SAFETY: the player's record
    let shift_mode = unsafe { (*rec::<EeUnitsPlayerFields>(O::PlFields, pl)).mShiftMode };
    EeGfxGoalPlayer {
        late_epoch: h(O::FxPlCurrentEpoch, pl, &[]) >= FX_EPOCH_ATOMIC_WW2,
        shift_mode,
        unit_count: h(O::FxGroupNumUnits, group, &[]) as u32,
    }
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_facts(_: *mut c_void, unit: u32) -> EeGfxGoalUnit {
    let cx = goal_unit(unit);
    let class = class_fields(hp(O::UnClass, cx, &[]));
    EeGfxGoalUnit {
        category: class.mCategory as u32,
        unit_type: class.mUnitType as u32,
        air_traffic_type: class.mAirTrafficType as i32,
        on_tile: hp(O::FxUnitTile, cx, &[]) != 0,
        held: hp(O::CxContainer, cx, &[]) != 0,
        queue_count: h(O::CxNumGoalQueues, cx, &[]) as u32,
    }
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_count(_: *mut c_void, unit: u32, queue: u32) -> u32 {
    h(O::FxQueueGoalCount, goal_unit(unit), &[queue as i64]) as u32
}

/// A goal's kind, automatic waypoint, garrison container or attack target, and location.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_step(_: *mut c_void, unit: u32, queue: u32, goal: u32) -> EeGfxGoalStep {
    let g = hp(O::FxQueueGoalAt, goal_unit(unit), &[queue as i64, goal as i64]);
    let mut step = EeGfxGoalStep::default();
    let kind = h(O::GoalKind, g, &[]);
    step.kind = kind as u32;
    if kind == FX_GOAL_LOCATION {
        step.auto_waypoint = h(O::FxGoalAutoWaypoint, g, &[]) != 0;
        if step.auto_waypoint {
            return step;
        }
    }
    let mut to = Ge3DPoint::default();
    h(O::FxGoalLocation, g, &[ptr(&mut to)]);
    step.to = to;
    let cast = if kind == FX_GOAL_LOAD_GARRISON {
        hp(O::FxLoadGarrisonCast, g, &[])
    } else if kind == FX_GOAL_ATTACK_UNIT {
        hp(O::FxAttackUnitCast, g, &[])
    } else {
        0
    };
    if cast != 0 {
        step.has_target = true;
        step.target = if kind == FX_GOAL_LOAD_GARRISON {
            hp(O::FxGoalLoadContainer, cast, &[])
        } else {
            hp(O::FxGoalAttackTarget, cast, &[])
        } as *mut c_void;
    }
    step
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_location(
    _: *mut c_void,
    unit: u32,
    queue: u32,
    goal: u32,
    out: *mut Ge3DPoint,
) -> bool {
    let previous = hp(O::FxQueueGoalGetAt, goal_unit(unit), &[queue as i64, goal as i64]);
    if previous == 0 {
        return false;
    }
    let mut from = Ge3DPoint::default();
    h(O::FxGoalLocation, previous, &[ptr(&mut from)]);
    // SAFETY: the caller's point
    unsafe { *out = from };
    true
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_unit_location(_: *mut c_void, unit: u32, on_tile: bool) -> Ge3DPoint {
    let cx = goal_unit(unit);
    let at = if on_tile { cx } else { hp(O::CxContainer, cx, &[]) };
    position(&cx_location(at))
}

/// A placed target's outer part; a land building's: the goal's point on its centre's tile corner.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_target(_: *mut c_void, target: *mut c_void, to: *mut Ge3DPoint) -> *mut c_void {
    let mut t = target as P;
    if hp(O::UnTileNode, t, &[]) != 0 {
        let mut outer = hp(O::UnComplex, t, &[]);
        loop {
            let previous = hp(O::CxPreviousPart, outer, &[]);
            if previous == 0 {
                break;
            }
            outer = previous;
        }
        t = outer;
    }
    let class = hp(O::UnClass, t, &[]);
    if h(O::UcIsLandBuilding, class, &[]) != 0 {
        if hp(O::UnTileNode, t, &[]) != 0 {
            let at = cx_location(hp(O::UnComplex, t, &[]));
            // SAFETY: the caller's point
            unsafe {
                (*to).x = at.x();
                (*to).y = at.y();
            }
        }
        let size = class_fields(class).mTileSize as f32;
        // SAFETY: the caller's point
        unsafe {
            (*to).x = (*to).x + size * 0.5 - 0.25;
            (*to).y = (*to).y - size * 0.5 + 0.25;
        }
    }
    t as *mut c_void
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_goal_ground(_: *mut c_void, point: *const Ge3DPoint, out: *mut Ge3DPoint) -> bool {
    // SAFETY: the caller's points
    let at = unsafe { *point };
    if tile_under(at.x, at.y) == 0 {
        return false;
    }
    let g = ground_point(&at);
    // SAFETY: as above
    unsafe { *out = g };
    true
}

// ---- the world tables' other entries

/// The capture's records (EESimCaptureRecord) as the manager's captures write them.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_capture(op: EeGfxCaptureOp, tag: u32, data: *const u8, len: u32) -> u32 {
    match op {
        EeGfxCaptureOp::Active => ee_app_capture_active() as u32,
        EeGfxCaptureOp::Keeps => ee_app_capture_keeps(tag) as u32,
        EeGfxCaptureOp::Commit => {
            // SAFETY: the caller's bytes
            unsafe { ee_app_capture_commit(tag, data, len as usize) };
            0
        }
        EeGfxCaptureOp::Only => {
            if !data.is_null() {
                // SAFETY: the caller's tags
                unsafe { ee_app_capture_only(data as *const u32, len) };
            } else {
                let none = 0u32;
                // SAFETY: Only(0): the tag's word, none
                unsafe { ee_app_capture_only(&none, 0) };
            }
            0
        }
        EeGfxCaptureOp::Exclusive => {
            ee_app_capture_exclusive(tag != 0);
            0
        }
    }
}

/// A sound node stopped and dereferenced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_sound_release(node: u64) {
    h(O::FxSoundStop, node as P, &[]);
    h(O::NodeDereference, node as P, &[]);
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_sound_object_unload(id: u32) {
    let sound = hp(O::SoundObjectGet, 0, &[id as i64]);
    if sound != 0 {
        h(O::FxSoundObjectUnload, sound, &[]);
    }
}

#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_sound_object_load(id: u32) {
    let sound = hp(O::SoundObjectGet, 0, &[id as i64]);
    if sound != 0 {
        h(O::FxSoundObjectLoad, sound, &[]);
    }
}

/// The map tile at (x, y): -1 none, else whether the current player sees it.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_tile_seen(x: i64, y: i64) -> i32 {
    let tile = hp(O::MapTile, 0, &[x as c_long as i64, y as c_long as i64]);
    if tile == 0 {
        return -1;
    }
    (h(O::FxTileSeenBy, tile, &[pa(hp(O::WorldCurrentPlayer, 0, &[]))]) != 0) as i32
}

/// The current player's doppelganger of the unit (or its complex unit) on the tile at (x, y).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_doppelganger(x: i64, y: i64, unit: u64, complex: bool) -> bool {
    let tile = hp(O::MapTile, 0, &[x as c_long as i64, y as c_long as i64]);
    let u = if complex { hp(O::UnComplex, unit as P, &[]) } else { unit as P };
    h(O::FxHasDoppelganger, tile, &[pa(u)]) != 0
}

/// A shooter's weapon point (ee_combat's weapon_point with its CBEW capture): with the tag points at its animation
/// when `test_list` finds them, else assumed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_weapon_point(unit: u64, site: i32, test_list: bool, tagged: *mut bool) -> Ge3DPoint {
    let u = unit as P;
    let model = hp(O::UnGetModel, u, &[]);
    let cx = hp(O::UnComplex, u, &[]);
    let list = hp(O::FxModelTagList, hp(O::UnGetModel, u, &[]), &[cx_fields(cx).mAnimation as i64]);
    let has = list != 0 && h(O::FxTagCount, list, &[]) != 0;
    // SAFETY: the caller's flag
    unsafe { *tagged = has };
    let mut start = Ge3DPoint::default();
    h(
        O::FxWeaponPoint,
        cx,
        &[site as i64, pa(model), pa(list), (if test_list { has } else { true }) as i64, ptr(&mut start)],
    );
    start
}

/// `gEffectDetail` (`set` >= 0: set it first).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_effect_detail(set: i64) -> u32 {
    if set >= 0 {
        h(O::FxSetEffectDetail, 0, &[set as c_ulong as i64]);
    }
    h(O::FxEffectDetail, 0, &[]) as u32
}

/// An `FSMemoryFile`'s bytes and length.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_memory_file_data(file: *mut c_void, len: *mut u32) -> *const u8 {
    let mut length: c_ulong = 0;
    h(O::FxMemoryLength, file as P, &[ptr(&mut length)]);
    // SAFETY: the caller's length
    unsafe { *len = length as u32 };
    hp(O::FxMemoryData, file as P, &[]) as *const u8
}

// ---- the quad batches' arrays (EEEffect.cpp's 0x4e8db8, 0x4e8da8, 0x4e8df0)

/// A batch's quad: its four vertices from `index` on (the raster table's batch_quad; AddEffectQuad).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_batch_quad(which: u32, quad: *const EeGfxEffectQuad, index: u32) {
    for i in 0..4u32 {
        h(O::FxBatchSetVertex, 0, &[which as i64, quad as i64, i as i64, (index + i) as i64]);
    }
}

/// The quad batches' shared polygon array (two triangles per quad, 0x200 quads) and both vertex arrays.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_batch_create() {
    let polygons = hp(O::FxBatchPolygonsNew, 0, &[0x400]);
    let mut j: i64 = 0;
    for i in 0..0x200i64 {
        h(O::FxPolygonSetIndices, polygons, &[j, i * 4, i * 4 + 1, i * 4 + 2]);
        h(O::FxPolygonSetIndices, polygons, &[j + 1, i * 4, i * 4 + 2, i * 4 + 3]);
        j += 2;
    }
    h(O::FxBatchVerticesNew, 0, &[0]);
    h(O::FxBatchVerticesNew, 0, &[1]);
}

/// The quad batches' arrays deleted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_batch_delete() {
    h(O::FxBatchPolygonsDelete, 0, &[]);
    h(O::FxBatchVerticesDelete, 0, &[0]);
    h(O::FxBatchVerticesDelete, 0, &[1]);
}

// ---- the battlefield decals' entry points

/// Up to N shell-hole decals of the unit's class around a point (ambient effects on; ee_gfx effect_placement.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_place_impact_decal(cx: *mut c_void, at: *const Ge3DPoint) {
    if h(O::FxAmbientEffects, 0, &[]) == 0 {
        return;
    }
    let geff = hp(O::FxShellHoleEffect, cx as P, &[]);
    if geff != 0 {
        // SAFETY: the caller's point
        unsafe { ee_gfx_effects_decal_impact(geff as u64, *at) };
    }
}

/// The unit class's blood-stain decal under the unit if ambient effects and blood are on (moved forward along the
/// heading for giraffes and horses).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fx_place_track_decal(cx: *mut c_void) {
    let cx = cx as P;
    if h(O::FxAmbientEffects, 0, &[]) == 0 || h(O::FxDrawBlood, 0, &[]) == 0 {
        return;
    }
    let geff = hp(O::FxBloodStainEffect, cx, &[]);
    if geff == 0 {
        return;
    }
    let at = unit_position(cx);
    let kind = class_fields(hp(O::UnClass, cx, &[])).mUnitType;
    let t = cx_location(cx);
    let track = track_kind(kind);
    // SAFETY: the graphic effect is live
    unsafe { ee_gfx_effects_decal_track(geff as u64, at, track, t.m[0][0], t.m[1][0]) };
}

/// The track decal's offset kind of a unit type.
fn track_kind(kind: c_ulong) -> u32 {
    if kind == FX_TYPE_GIRAFFE {
        EE_GFX_TRACK_GIRAFFE
    } else if kind == FX_TYPE_HORSE {
        EE_GFX_TRACK_HORSE
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_track_kinds_are_the_giraffes_and_the_horses() {
        assert_eq!(track_kind(0x124), EE_GFX_TRACK_GIRAFFE);
        assert_eq!(track_kind(0x175), EE_GFX_TRACK_HORSE);
        assert_eq!(track_kind(0x23), 0);
    }

    #[test]
    fn the_capture_answers_without_a_stream() {
        assert_eq!(ee_gamecpp_fx_capture(EeGfxCaptureOp::Exclusive, 0, std::ptr::null(), 0), 0);
        assert_eq!(
            ee_gamecpp_fx_capture(EeGfxCaptureOp::Active, 0, std::ptr::null(), 0),
            ee_app_capture_active() as u32
        );
    }

    #[test]
    fn the_surface_description_is_the_cpp_one() {
        let words = 8 * std::mem::size_of::<c_ulong>();
        assert_eq!(std::mem::size_of::<SurfaceDescription>(), words + std::mem::size_of::<c_ulong>());
    }
}
