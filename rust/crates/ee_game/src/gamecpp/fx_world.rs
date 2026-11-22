//! The effects' world tables in Rust (game core lane, round 11; was EEEffectHost.cpp's `EEGfxObjectLayoutRegister`,
//! `EEMakeWorld`, `EEMakeManagerWorld`, `Ask`, `EEGoalOps`, `EEFileOps`, `EEStormHostOps`, `EEEffectFile` and
//! `CreateWeatherEffect`, and EEEffectManager.h's entry points, Load and Save): the members ee_gfx reads in place
//! (each offset one case), the C++ world the effects and their manager call into (each C++ call one case of
//! EEEffectHost.cpp's `EECore11OpA2`), the effects' file over an `FSFile`, and the manager's `Create*` arguments.
//! The tables are registered before `main` (EEEffectHost.cpp's static initializer calls [`ee_gamecpp_fw_register`]).
//!
//! The effects run in EE-SIM (the oracle, the `effects` and `calamity` captures); their drawing only in a game with a
//! window.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation};
use ee_render::ge::tmobj::EeGeTerrainMesh;
use ee_render::ge::viewport::GeViewport;
use ee_render::gfx::effect_manager::EeGfxEffectEntry;
use ee_render::gfx::effect_obj::{
    ee_gfx_effect_world_set, EeGfxEffectAnswers2D, EeGfxEffectAnswers3D, EeGfxEffectAsk, EeGfxEffectRegions,
    EeGfxEffectTick, EeGfxEffectWorld,
};
use ee_render::gfx::effect_reads::{ee_gfx_object_layout_set, EeGfxObjectLayout};
use ee_render::gfx::effect_state::EeGfxEffectRecord;
use ee_render::gfx::ffi::ee_gfx_effect_in_view;
use ee_render::gfx::ffi_effect::{EeGfxEffectFile, EeGfxStormHost};
use ee_render::gfx::ffi_manager::{
    ee_gfx_manager_create, ee_gfx_manager_load, ee_gfx_manager_save, ee_gfx_manager_world_set, EeGfxEffectArgs,
    EeGfxEffectManager, EeGfxManagerWorld,
};
use ee_render::gfx::goal_markers::EeGfxGoalHost;
use ee_render::gfx::raster::EeGfxModel;
use ee_sim::world::unit_fields::EeUnitsComplexUnitFields;

use super::effect_host::{
    ee_gamecpp_fx_animation_sound, ee_gamecpp_fx_capture, ee_gamecpp_fx_confirm_goal, ee_gamecpp_fx_damaged_tick, ee_gamecpp_fx_doppelganger, ee_gamecpp_fx_effect_detail, ee_gamecpp_fx_emitter_sound, ee_gamecpp_fx_farm_doppelganger, ee_gamecpp_fx_farm_drop_site, ee_gamecpp_fx_farm_opacity, ee_gamecpp_fx_farm_terrain, ee_gamecpp_fx_glow_area, ee_gamecpp_fx_goal_count, ee_gamecpp_fx_goal_facts, ee_gamecpp_fx_goal_ground, ee_gamecpp_fx_goal_location, ee_gamecpp_fx_goal_player, ee_gamecpp_fx_goal_step, ee_gamecpp_fx_goal_target, ee_gamecpp_fx_goal_unit_location, ee_gamecpp_fx_ground_at, ee_gamecpp_fx_ground_point, ee_gamecpp_fx_icons_selected, ee_gamecpp_fx_memory_file_data, ee_gamecpp_fx_part_sound, ee_gamecpp_fx_rain_sounds, ee_gamecpp_fx_set_area_effects, ee_gamecpp_fx_sound_object_load, ee_gamecpp_fx_sound_object_unload, ee_gamecpp_fx_sound_release, ee_gamecpp_fx_storm_is_water, ee_gamecpp_fx_storm_ripple, ee_gamecpp_fx_storm_step_toward, ee_gamecpp_fx_tile_seen, ee_gamecpp_fx_volcano_build, ee_gamecpp_fx_wall_gate_posts, ee_gamecpp_fx_wall_piece, ee_gamecpp_fx_weapon_point,
};
use super::{cptr, h, hd, hf, hp, ptr, rec, O, P};

// ---- the layout (ee_gfx effect_reads.rs EeGfxObjectLayout)

fn off(op: O) -> u64 {
    h(op, 0, &[]) as u64
}

/// The members ee_gfx effect_reads.rs reads in place and gobj_media.rs loads and releases; the physical model's
/// scale and frame from the C++ (GEPhysicalModel's friend).
fn layout(complex_scale: u64, complex_frame: u64) -> EeGfxObjectLayout {
    EeGfxObjectLayout {
        size: std::mem::size_of::<EeGfxObjectLayout>() as u64,
        gobj_fields: off(O::FwLayGobjFields),
        gobj_models: off(O::FwLayGobjModels),
        gobj_textures: off(O::FwLayGobjTextures),
        gobj_tag_points: off(O::FwLayGobjTagPoints),
        gobj_tags: off(O::FwLayGobjTags),
        gobj_model: off(O::FwLayGobjModel),
        gobj_default_point: off(O::FwLayGobjDefaultPoint),
        gobj_top_point: off(O::FwLayGobjTopPoint),
        gobj_turret_parts: off(O::FwLayGobjTurretParts),
        gobj_orbitals: off(O::FwLayGobjOrbitals),
        turret_part_model: off(O::FwLayTurretPartModel),
        orbital_model: off(O::FwLayOrbitalModel),
        orbital_speed: off(O::FwLayOrbitalSpeed),
        orbital_angle: off(O::FwLayOrbitalAngle),
        geffect_fields: off(O::FwLayGeffectFields),
        geffect_object: off(O::FwLayGeffectObject),
        complex_scale,
        complex_frame,
        complex_turret_angles: off(O::FwLayComplexTurretAngles),
        stats_model: off(O::FwLayStatsModel),
        geffect_linked: off(O::FwLayGeffectLinked),
        gobj_file_name: off(O::FwLayGobjFileName),
        gobj_texture_name: off(O::FwLayGobjTextureName),
        gobj_shadow_model_name: off(O::FwLayGobjShadowModelName),
        gobj_shadow_texture_name: off(O::FwLayGobjShadowTextureName),
        gobj_model_names: off(O::FwLayGobjModelNames),
        gobj_texture_names: off(O::FwLayGobjTextureNames),
        gobj_texture: off(O::FwLayGobjTexture),
        gobj_shadow_model: off(O::FwLayGobjShadowModel),
        gobj_shadow_texture: off(O::FwLayGobjShadowTexture),
        gobj_sounds1: off(O::FwLayGobjSounds1),
        gobj_sounds2: off(O::FwLayGobjSounds2),
        gobj_bar: off(O::FwLayGobjBar),
        gobj_media_use: off(O::FwLayGobjMediaUse),
        geffect_num_loads: off(O::FwLayGeffectNumLoads),
    }
}

// ---- the effects' world (ee_gfx effect_obj.rs EeGfxEffectWorld)

extern "C" fn unit_reference(u: u64) {
    h(O::FwUnitRef, u as P, &[]);
}
extern "C" fn unit_release(u: u64) {
    h(O::FwUnitDeref, u as P, &[]);
}
extern "C" fn unit_by_id(id: u32) -> u64 {
    h(O::WorldUnit, 0, &[id as i64]) as u64
}
extern "C" fn graphic_effect_reference(e: u64) {
    h(O::FwGeffRef, e as P, &[]);
}
extern "C" fn graphic_effect_release(e: u64) {
    h(O::FwGeffDeref, e as P, &[]);
}
extern "C" fn graphic_effect_load(e: u64, on_demand: bool) {
    h(O::FwGeffLoad, e as P, &[on_demand as i64]);
}
extern "C" fn graphic_effect_unload(e: u64) {
    h(O::FwGeffUnload, e as P, &[]);
}
extern "C" fn graphic_effect_media(e: u64) -> u64 {
    h(O::FwGeffMedia, e as P, &[]) as u64
}
extern "C" fn graphic_effect_by_id(id: u32) -> u64 {
    h(O::FwGeffGet, 0, &[id as i64]) as u64
}
extern "C" fn graphic_effect_count() -> u32 {
    h(O::FwGeffCount, 0, &[]) as u32
}
extern "C" fn model_reference(m: u64) {
    h(O::FxModelReference, m as P, &[]);
}
extern "C" fn model_release(m: u64) {
    h(O::FwModelDeref, m as P, &[]);
}
extern "C" fn media_load(o: u64) -> i32 {
    h(O::FwMediaLoad, o as P, &[]) as i32
}
extern "C" fn media_unload(o: u64) {
    h(O::FwMediaUnload, o as P, &[]);
}
extern "C" fn unit_damaged(u: u64) {
    h(O::FwUnitDamaged, u as P, &[]);
}
extern "C" fn loading_saved_game() -> bool {
    h(O::FwLoadingSaved, 0, &[]) != 0
}
extern "C" fn regions() -> *mut EeGfxEffectRegions {
    let map = hp(O::MpWorldMap, 0, &[]);
    if map == 0 {
        return std::ptr::null_mut();
    }
    hp(O::FwMapEffectRegions, map, &[]) as *mut EeGfxEffectRegions
}
extern "C" fn in_view(point: *const Ge3DPoint, radius: f32) -> bool {
    let viewport = hp(O::FwViewport, 0, &[]) as *const GeViewport;
    // SAFETY: the main viewport and the caller's point
    unsafe { ee_gfx_effect_in_view(viewport, h(O::MpOrthographic, 0, &[]) != 0, point, radius) }
}

/// `(uint32_t)(unsigned long)gPhysics->mGameTime` from its `(int64_t)` widening.
fn game_time_bits(game_time: i64) -> u32 {
    game_time as c_long as c_ulong as u32
}

extern "C" fn tick() -> EeGfxEffectTick {
    let game_time = h(O::FwGameTime, 0, &[]);
    let mut wind = Ge3DPoint::default();
    h(O::FwWind, 0, &[ptr(&mut wind)]);
    EeGfxEffectTick {
        tick_seconds: hd(O::FwTickSeconds, 0, &[], &[]),
        tick_time: hd(O::FwTickTime, 0, &[], &[]),
        game_time_bits: game_time_bits(game_time),
        free_camera: h(O::FxFreeCamera, 0, &[]) != 0,
        speed: hd(O::FwPhysicsSpeed, 0, &[], &[]),
        world_speed: hd(O::FwWorldSpeed, 0, &[], &[]),
        game_time,
        game_clock: hd(O::FwGameClock, 0, &[], &[]),
        game_over: h(O::FwGameOver, 0, &[]) != 0,
        wind,
    }
}
extern "C" fn screen(preferences: bool) -> u32 {
    h(O::FwScreen, 0, &[preferences as i64]) as u32
}
extern "C" fn height(point: *const Ge3DPoint) -> f32 {
    // SAFETY: the caller's point
    let at = unsafe { *point };
    hd(O::MpMeshZ, hp(O::MpWorldMap, 0, &[]), &[], &[at.x, at.y])
}
extern "C" fn mesh() -> *const EeGeTerrainMesh {
    hp(O::MpMeshRust, hp(O::MpWorldMap, 0, &[]), &[]) as *const EeGeTerrainMesh
}
extern "C" fn unit_graphic(_: *mut c_void, u: u64) -> u64 {
    h(O::UnGetModel, u as P, &[]) as u64
}

// the weather storms' map
extern "C" fn storm_on_map(_: *mut c_void, x: i32, y: i32) -> bool {
    hp(O::MapTile, 0, &[x as i64, y as i64]) != 0
}
extern "C" fn storm_ground_z(_: *mut c_void, x: i32, y: i32) -> f32 {
    hd(O::FwMeshGetZ, hp(O::MpWorldMap, 0, &[]), &[x as i64, y as i64], &[])
}

/// The complex unit of an answer's effect's unit.
fn ask_complex(ask: *mut c_void) -> P {
    // SAFETY: the context of every answer is the live ask of the effect's call, its effect live
    let unit = unsafe { (*(*(ask as *const EeGfxEffectAsk)).effect).unit };
    hp(O::UnComplex, unit as P, &[])
}
fn complex_fields<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: a live complex unit's record
    unsafe { &mut *rec::<EeUnitsComplexUnitFields>(O::CxFields, cx) }
}

extern "C" fn farm_set_variant(ask: *mut c_void, variant: i32) {
    complex_fields(ask_complex(ask)).mCloaked = variant as c_long as c_ulong;
}
extern "C" fn farm_doppelganger_any(ask: *mut c_void) -> bool {
    // SAFETY: as ask_complex
    let unit = unsafe { (*(*(ask as *const EeGfxEffectAsk)).effect).unit };
    h(O::FwDoppelgangerAny, unit as P, &[]) != 0
}
extern "C" fn set_scaled_model(ask: *mut c_void, model: EeGfxModel) {
    h(O::FwCxSetScaledModel, ask_complex(ask), &[model as usize as i64]);
}
extern "C" fn draw_blood(_: *mut c_void) -> bool {
    h(O::FxDrawBlood, 0, &[]) != 0
}
extern "C" fn tile_blocked(_: *mut c_void, x: i32, y: i32) -> bool {
    h(O::FwTileBlocked, 0, &[x as i64, y as i64]) != 0
}
extern "C" fn area_effect(ask: *mut c_void, kind: i32, x: f32, y: f32, player: u32) -> bool {
    hf(O::FwAreaEffect, ask_complex(ask), &[kind as i64, player as i64], &[x, y]) != 0
}
extern "C" fn area_effects(ask: *mut c_void) -> i32 {
    complex_fields(ask_complex(ask)).mNumAreaEffects as i32
}
extern "C" fn glow_rows(_: *mut c_void) -> u32 {
    h(O::FwGlowRows, 0, &[]) as u32
}

fn world() -> EeGfxEffectWorld {
    EeGfxEffectWorld {
        unit_reference,
        unit_release,
        unit_by_id,
        graphic_effect_reference,
        graphic_effect_release,
        graphic_effect_load,
        graphic_effect_unload,
        graphic_effect_media,
        graphic_effect_by_id,
        graphic_effect_count,
        model_reference,
        model_release,
        media_load,
        media_unload,
        sound_release: ee_gamecpp_fx_sound_release,
        sound_object_unload: ee_gamecpp_fx_sound_object_unload,
        sound_object_load: ee_gamecpp_fx_sound_object_load,
        unit_damaged,
        loading_saved_game,
        regions,
        tile_seen: ee_gamecpp_fx_tile_seen,
        doppelganger: ee_gamecpp_fx_doppelganger,
        in_view,
        tick,
        screen,
        weapon_point: ee_gamecpp_fx_weapon_point,
        height,
        mesh,
        raster: super::ro_raster::ee_gamecpp_ro_raster(),
        unit_graphic,
        storm: EeGfxStormHost {
            context: std::ptr::null_mut(),
            on_map: storm_on_map,
            is_water: ee_gamecpp_fx_storm_is_water,
            ground_z: storm_ground_z,
            step_toward: ee_gamecpp_fx_storm_step_toward,
            ripple: ee_gamecpp_fx_storm_ripple,
        },
        answers3d: EeGfxEffectAnswers3D {
            animation_sound: ee_gamecpp_fx_animation_sound,
            wall_gate_posts: ee_gamecpp_fx_wall_gate_posts,
            wall_piece: ee_gamecpp_fx_wall_piece,
            volcano_build: ee_gamecpp_fx_volcano_build,
            farm_terrain: ee_gamecpp_fx_farm_terrain,
            farm_drop_site: ee_gamecpp_fx_farm_drop_site,
            farm_set_variant,
            farm_doppelganger_any,
            farm_doppelganger: ee_gamecpp_fx_farm_doppelganger,
            farm_opacity: ee_gamecpp_fx_farm_opacity,
            set_scaled_model,
            ground_point: ee_gamecpp_fx_ground_point,
            rain_sounds: ee_gamecpp_fx_rain_sounds,
            emitter_sound: ee_gamecpp_fx_emitter_sound,
        },
        answers2d: EeGfxEffectAnswers2D {
            confirm_goal: ee_gamecpp_fx_confirm_goal,
            animation_sound: ee_gamecpp_fx_part_sound,
            draw_blood,
            tile_blocked,
            area_effect,
            area_effects,
            set_area_effects: ee_gamecpp_fx_set_area_effects,
            icons_selected: ee_gamecpp_fx_icons_selected,
            ground_point: ee_gamecpp_fx_ground_at,
            glow_area: ee_gamecpp_fx_glow_area,
            glow_rows,
        },
    }
}

// ---- the manager's world (ee_gfx ffi_manager.rs EeGfxManagerWorld)

extern "C" fn current() -> *mut EeGfxEffectManager {
    hp(O::FwManagerRust, 0, &[]) as *mut EeGfxEffectManager
}
extern "C" fn goal_current_player(_: *mut c_void) -> u32 {
    h(O::FwCurrentPlayerIndex, 0, &[]) as u32
}
extern "C" fn goal_player_count(_: *mut c_void) -> u32 {
    h(O::FwNumPlayers, 0, &[]) as u32
}
extern "C" fn memory_file() -> EeGfxEffectFile {
    file(hp(O::FwMemoryNew, 0, &[]) as *mut c_void)
}
extern "C" fn memory_file_rewind(context: *mut c_void) {
    h(O::FwMemoryRewind, context as P, &[]);
}
extern "C" fn memory_file_free(context: *mut c_void) {
    h(O::FwMemoryDelete, context as P, &[]);
}

// the markers' effect calls: the manager answers them itself (ffi_manager.rs replaces these; the C++ passed NULL)
extern "C" fn goal_create(_: *mut c_void, _: u32, _: u32) -> u32 {
    0
}
extern "C" fn goal_destroy(_: *mut c_void, _: u32) {}
extern "C" fn goal_move_line(_: *mut c_void, _: u32, _: *const Ge3DPoint, _: *const Ge3DPoint) {}
extern "C" fn goal_move_icon(_: *mut c_void, _: u32, _: *const Ge3DPoint) {}

fn manager_world() -> EeGfxManagerWorld {
    EeGfxManagerWorld {
        damaged_tick: ee_gamecpp_fx_damaged_tick,
        effect_detail: ee_gamecpp_fx_effect_detail,
        current,
        goals: EeGfxGoalHost {
            context: std::ptr::null_mut(),
            current_player: goal_current_player,
            player_count: goal_player_count,
            player: ee_gamecpp_fx_goal_player,
            unit: ee_gamecpp_fx_goal_facts,
            goal_count: ee_gamecpp_fx_goal_count,
            goal: ee_gamecpp_fx_goal_step,
            goal_location: ee_gamecpp_fx_goal_location,
            unit_location: ee_gamecpp_fx_goal_unit_location,
            target: ee_gamecpp_fx_goal_target,
            ground: ee_gamecpp_fx_goal_ground,
            create: goal_create,
            destroy: goal_destroy,
            move_line: goal_move_line,
            move_icon: goal_move_icon,
        },
        capture: ee_gamecpp_fx_capture,
        memory_file,
        memory_file_data: ee_gamecpp_fx_memory_file_data,
        memory_file_rewind,
        memory_file_free,
        save_major: h(O::FwSaveMajor, 0, &[]) as u32,
        save_minor: h(O::FwSaveMinor, 0, &[]) as u32,
    }
}

/// Registers the effects' layout, world and manager world (EEEffectHost.cpp's `EEGfxObjectLayoutRegister`, before
/// `main`); `complex_scale` and `complex_frame` are the complex unit's physical model's scale and frame offsets.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_register(complex_scale: u64, complex_frame: u64) {
    let l = layout(complex_scale, complex_frame);
    // SAFETY: a filled layout; the worlds live for the rest of the program
    unsafe {
        ee_gfx_object_layout_set(&l);
        ee_gfx_effect_world_set(Box::leak(Box::new(world())));
        ee_gfx_manager_world_set(Box::leak(Box::new(manager_world())));
    }
}

// ---- the effects' file over an FSFile (each the FSFile call of that type)

extern "C" fn file_is_open(context: *mut c_void) -> bool {
    h(O::PsFileIsOpen, context as P, &[]) != 0
}
extern "C" fn file_read_u32(context: *mut c_void, v: *mut u32) -> i32 {
    h(O::FwReadU32, context as P, &[v as usize as i64]) as i32
}
extern "C" fn file_read_i32(context: *mut c_void, v: *mut i32) -> i32 {
    h(O::FwReadI32, context as P, &[v as usize as i64]) as i32
}
extern "C" fn file_read_f32(context: *mut c_void, v: *mut f32) -> i32 {
    h(O::PfReadF32, context as P, &[v as usize as i64]) as i32
}
extern "C" fn file_read_bool(context: *mut c_void, v: *mut bool) -> i32 {
    h(O::PfReadBool, context as P, &[v as usize as i64]) as i32
}
extern "C" fn file_write_u32(context: *mut c_void, v: u32) -> i32 {
    h(O::FwWriteU32, context as P, &[cptr(&v)]) as i32
}
extern "C" fn file_write_i32(context: *mut c_void, v: i32) -> i32 {
    h(O::FwWriteI32, context as P, &[cptr(&v)]) as i32
}
extern "C" fn file_write_f32(context: *mut c_void, v: f32) -> i32 {
    h(O::PfWriteF32, context as P, &[cptr(&v)]) as i32
}
extern "C" fn file_write_bool(context: *mut c_void, v: bool) -> i32 {
    h(O::PfWriteBool, context as P, &[cptr(&v)]) as i32
}

fn file(context: *mut c_void) -> EeGfxEffectFile {
    EeGfxEffectFile {
        context,
        is_open: file_is_open,
        read_u32: file_read_u32,
        read_i32: file_read_i32,
        read_f32: file_read_f32,
        read_bool: file_read_bool,
        write_u32: file_write_u32,
        write_i32: file_write_i32,
        write_f32: file_write_f32,
        write_bool: file_write_bool,
    }
}

/// `EEEffectFile`: an `FSFile` as the effects' file.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_file(file_: *mut c_void) -> EeGfxEffectFile {
    file(file_)
}

/// EEEffectManager::Load: the manager's effects from the file.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_load(m: *mut EeGfxEffectManager, file_: *mut c_void, major: c_ulong, minor: c_ulong) -> c_long {
    let f = file(file_);
    // SAFETY: a live manager and the file's view for the call
    unsafe { ee_gfx_manager_load(m, &f, major as u32, minor as u32) as c_long }
}

/// EEEffectManager::Save: the manager's effects to the file.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_save(m: *mut EeGfxEffectManager, file_: *mut c_void, major: c_ulong, minor: c_ulong) -> c_long {
    let f = file(file_);
    // SAFETY: as ee_gamecpp_fw_load
    unsafe { ee_gfx_manager_save(m, &f, major as u32, minor as u32) as c_long }
}

// ---- the manager's entry points (EEEffectManager's Create methods: the entry's arguments)

/// An entry's arguments: the graphic effect, everything else zero (the C++'s `= {}`).
fn args(entry: EeGfxEffectEntry, effect: *mut c_void) -> EeGfxEffectArgs {
    EeGfxEffectArgs {
        entry,
        graphic_effect: effect as usize as u64,
        unit: 0,
        to_unit: 0,
        at: Ge3DPoint::default(),
        to: Ge3DPoint::default(),
        transformation: GeTransformation { m: [[0.0; 4]; 4] },
        line: Ge3DLine::default(),
        tiles: std::ptr::null(),
        tile_count: 0,
        keeps_unit: false,
        tag_point: 0,
        goal_index: 0,
    }
}

fn create(m: *mut EeGfxEffectManager, a: &EeGfxEffectArgs, handle: *mut u32) -> c_long {
    // SAFETY: a live manager; the handle null or writable
    unsafe { ee_gfx_manager_create(m, a, handle) as c_long }
}

fn point(at: *const Ge3DPoint) -> Ge3DPoint {
    // SAFETY: the caller's point
    unsafe { *at }
}

/// CreateUnitEffect: on a unit (`keeps_unit`: the light glow's; `tag_point`: the emitter's; `goal_index`: the
/// confirmation's goal queue).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_unit_effect(
    m: *mut EeGfxEffectManager,
    unit: *mut c_void,
    effect: *mut c_void,
    handle: *mut u32,
    keeps_unit: bool,
    tag_point: c_long,
    goal_index: c_long,
) -> c_long {
    let a = EeGfxEffectArgs {
        unit: unit as usize as u64,
        keeps_unit,
        tag_point: tag_point as i32,
        goal_index: goal_index as u32,
        ..args(EeGfxEffectEntry::Unit, effect)
    };
    create(m, &a, handle)
}

/// CreateEffectBetweenUnits: from a unit to a unit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_between_units(
    m: *mut EeGfxEffectManager,
    from: *mut c_void,
    to: *mut c_void,
    effect: *mut c_void,
    handle: *mut u32,
) -> c_long {
    let a = EeGfxEffectArgs {
        unit: from as usize as u64,
        to_unit: to as usize as u64,
        ..args(EeGfxEffectEntry::BetweenUnits, effect)
    };
    create(m, &a, handle)
}

/// CreateEffect: from a unit to a point.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_unit_to_point(
    m: *mut EeGfxEffectManager,
    unit: *mut c_void,
    at: *const Ge3DPoint,
    effect: *mut c_void,
    handle: *mut u32,
    goal_index: c_long,
) -> c_long {
    let a = EeGfxEffectArgs {
        unit: unit as usize as u64,
        at: point(at),
        goal_index: goal_index as u32,
        ..args(EeGfxEffectEntry::UnitToPoint, effect)
    };
    create(m, &a, handle)
}

/// CreatePlacedEffect: at a point.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_placed(
    m: *mut EeGfxEffectManager,
    at: *const Ge3DPoint,
    effect: *mut c_void,
    handle: *mut u32,
) -> c_long {
    let a = EeGfxEffectArgs { at: point(at), ..args(EeGfxEffectEntry::Placed, effect) };
    create(m, &a, handle)
}

/// CreateTransformationEffect: at a transformation.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_transformation(
    m: *mut EeGfxEffectManager,
    t: *const GeTransformation,
    effect: *mut c_void,
    handle: *mut u32,
) -> c_long {
    // SAFETY: the caller's transformation
    let a = EeGfxEffectArgs { transformation: unsafe { *t }, ..args(EeGfxEffectEntry::Transformation, effect) };
    create(m, &a, handle)
}

/// CreateEffectBetween: between two points.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_between(
    m: *mut EeGfxEffectManager,
    from: *const Ge3DPoint,
    to: *const Ge3DPoint,
    effect: *mut c_void,
    handle: *mut u32,
) -> c_long {
    let a = EeGfxEffectArgs { at: point(from), to: point(to), ..args(EeGfxEffectEntry::Between, effect) };
    create(m, &a, handle)
}

/// The tiles' coordinates as x, y pairs of 32 bits.
fn tile_pairs(tiles: &[c_long]) -> Vec<i32> {
    tiles.iter().map(|&v| v as i32).collect()
}

/// CreateWeatherEffect: rain or snow over a set of tiles (`tiles`: `count` x, y pairs of `long`).
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_weather(
    m: *mut EeGfxEffectManager,
    tiles: *const c_long,
    count: usize,
    effect: *mut c_void,
    handle: *mut u32,
) -> c_long {
    let pairs = if count == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's count of coordinate pairs
        tile_pairs(unsafe { std::slice::from_raw_parts(tiles, 2 * count) })
    };
    let a = EeGfxEffectArgs {
        tiles: if pairs.is_empty() { std::ptr::null() } else { pairs.as_ptr() },
        tile_count: count as u32,
        ..args(EeGfxEffectEntry::Weather, effect)
    };
    create(m, &a, handle)
}

/// EEEffect::SetLocalTo: shown only to `player`, not saved.
#[no_mangle]
pub extern "C" fn ee_gamecpp_fw_set_local(record: *mut EeGfxEffectRecord, player: c_long) {
    // SAFETY: a live effect's record
    let r = unsafe { &mut *record };
    r.saved = false;
    r.local_only = true;
    r.player = player as i32;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_game_time_bits_are_its_low_word() {
        assert_eq!(game_time_bits(5), 5);
        assert_eq!(game_time_bits(-1), u32::MAX);
        assert_eq!(game_time_bits(0x1_0000_0002), 2);
    }

    #[test]
    fn the_tiles_are_pairs_of_32_bits_and_the_arguments_start_at_zero() {
        assert_eq!(tile_pairs(&[1, 2, -3, 4]), vec![1, 2, -3, 4]);
        let a = args(EeGfxEffectEntry::Weather, 0x10 as *mut c_void);
        assert_eq!(a.graphic_effect, 0x10);
        assert_eq!(a.transformation.m, [[0.0; 4]; 4]);
        assert!(a.tiles.is_null() && a.unit == 0 && !a.keeps_unit);
    }

    #[test]
    fn set_local_hides_the_effect_from_the_save() {
        // SAFETY: a zeroed record is a valid plain record
        let mut r: EeGfxEffectRecord = unsafe { std::mem::zeroed() };
        r.saved = true;
        ee_gamecpp_fw_set_local(&mut r, 3);
        assert!(!r.saved && r.local_only);
        assert_eq!(r.player, 3);
    }
}
