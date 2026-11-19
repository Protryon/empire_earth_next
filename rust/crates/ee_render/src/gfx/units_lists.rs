//! The units' render object's frame (EEROUnits.cpp's `UnitsFrame`: the graphics + LLE cluster's third round): the
//! fill's render lists read in place (the view rfil.rs reads, `EeGfxRfilFrame`) as the records and indices
//! ro_units.rs draws: each render unit with its graphic objects' models, textures and flags and its class's, each
//! placed map object, the lists by pool index, the ghost records.

#![allow(clippy::unnecessary_cast)] // c_ulong and c_long are u64 and i64 on LP64

use std::ffi::c_void;

use ee_base::math::types::Ge3DPoint;
use ee_data::tables::EeDataGraphicObjectFields;
use ee_sim::world::{ClassView, WorldRead};

use crate::ge::cppobj;
use crate::ge::modobj::{ee_ge_mod_ptr, EeGeModPtr, EeGeModel};
use crate::gfx::effect_reads::layout;
use crate::gfx::effects::EeGfxColor;
use crate::gfx::fill::{EeGfxFillObject, EeGfxFillUnit};
use crate::gfx::raster::{EeGfxModel, EeGfxRaster};
use crate::gfx::rfil::{EeGfxRfilFrame, EeGfxRfilLayout, EeGfxRfilPair, EeGfxRfilPairs};
use crate::gfx::ro_units::{
    draw_labels, render_scene, EeGfxGhostRecord, EeGfxRenderObject, EeGfxRenderUnit, EeGfxUnitsScene,
};

/// `kMovementAir` (the class's domains).
const MOVEMENT_AIR: u8 = 1;
/// `kUnitTypeWaypoint`.
const UNIT_TYPE_WAYPOINT: u64 = 0x104;

fn rd<T: Copy>(base: usize, off: usize) -> T {
    // SAFETY: a live object's member (the frame's contract)
    unsafe { std::ptr::read_unaligned((base + off) as *const T) }
}

/// A `std::vector<T>`'s data and length (NULL when empty, as the C++'s `empty() ? NULL : &v[0]`).
fn vector<T>(at: usize) -> (*const T, usize) {
    let begin: usize = rd(at, 0);
    let end: usize = rd(at, std::mem::size_of::<usize>());
    let n = (end - begin) / std::mem::size_of::<T>();
    (if n == 0 { std::ptr::null() } else { begin as *const T }, n)
}

fn object_fields(object: usize) -> EeDataGraphicObjectFields {
    rd(object, layout().gobj_fields as usize)
}

fn object_word(object: usize, off: u64, i: usize) -> usize {
    rd(object, off as usize + i * std::mem::size_of::<usize>())
}

fn color(c: [f32; 3]) -> EeGfxColor {
    EeGfxColor { red: c[0], green: c[1], blue: c[2] }
}

/// One render unit as its passes read it (`UnitRecord`).
fn unit_record(t: &WorldRead, l: &EeGfxRfilLayout, unit: usize) -> EeGfxRenderUnit {
    let u: EeGfxFillUnit = rd(unit, 0);
    let g = layout();
    let object = u.source as usize;
    let animation = u.animation as usize;
    let model = object_word(object, g.gobj_models, animation);
    let fields = object_fields(object);
    let center = if model == 0 {
        Ge3DPoint::default()
    } else {
        // SAFETY: a live model's centre
        unsafe { *(ee_ge_mod_ptr(cppobj::counted::<EeGeModel>(model), EeGeModPtr::Center) as *const Ge3DPoint) }
    };
    let mut r = EeGfxRenderUnit {
        transformation: u.transformation,
        player_color: color(u.player_color),
        tint_color: color(u.tint_color),
        object: object as EeGfxModel,
        model: model as EeGfxModel,
        texture: object_word(object, g.gobj_textures, animation) as EeGfxModel,
        center,
        bar: rd(object, g.gobj_bar as usize),
        drawn: fields.mIsDrawn,
        alpha_ref: fields.mAlphaReference,
        object_opacity: fields.mOpacity,
        object2: std::ptr::null_mut(),
        model2: std::ptr::null_mut(),
        texture2: std::ptr::null_mut(),
        drawn2: false,
        health_bars: std::ptr::null(),
        health_bar_count: 0,
        turret_angles: std::ptr::null(),
        turret_models: std::ptr::null(),
        turret_count: 0,
        orbital_angles: std::ptr::null(),
        orbital_models: std::ptr::null(),
        orbital_count: 0,
        wheel_angles: std::ptr::null(),
        wheel_models: std::ptr::null(),
        wheel_count: 0,
        // SAFETY: the render unit's live UWideString
        name: unsafe { cppobj::wstr_text(unit + l.unit_name as usize) } as *const u8,
        scale: u.scale,
        selection_size: u.selection_size,
        opacity: u.opacity,
        health: u.health,
        fraction: u.fraction,
        sub_model_offset_scale: u.sub_model_offset_scale,
        fog_scale: u.fog_scale,
        inner_collision_sphere: 0.0,
        category: u.category as i32,
        command_type: u.command_type as i32,
        animation: u.animation as i32,
        current_frame: u.current_frame as u32,
        next_frame: u.next_frame as u32,
        area_effect_strength: u.area_effect_strength as u32,
        air: false,
        waypoint: u.unit_class_index as u64 == UNIT_TYPE_WAYPOINT,
        fogged: u.is_fogged,
        tinted: u.is_tinted,
        has_sub_model_offset: u.has_sub_model_offset,
        second_full_opacity: u.second_full_opacity,
    };
    let object2 = u.source2 as usize;
    if object2 != 0 {
        r.object2 = object2 as EeGfxModel;
        r.model2 = object_word(object2, g.gobj_model, 0) as EeGfxModel;
        r.texture2 = object_word(object2, g.gobj_texture, 0) as EeGfxModel;
        r.drawn2 = object_fields(object2).mIsDrawn;
    }
    (r.health_bars, r.health_bar_count) = vector::<f32>(unit + l.unit_health_bars as usize);
    let parts = |angles: u64, models: u64| {
        let (a, na) = vector::<f32>(unit + angles as usize);
        let (m, nm) = vector::<EeGfxModel>(unit + models as usize);
        (a, m, na.min(nm))
    };
    (r.turret_angles, r.turret_models, r.turret_count) = parts(l.unit_turret_angles, l.unit_turret_models);
    (r.orbital_angles, r.orbital_models, r.orbital_count) = parts(l.unit_orbital_angles, l.unit_orbital_models);
    (r.wheel_angles, r.wheel_models, r.wheel_count) = parts(l.unit_wheel_angles, l.unit_wheel_models);
    // SAFETY: the render unit's live class
    if let Some(class) = unsafe { ClassView::of(t, u.info) } {
        r.inner_collision_sphere = ee_sim::world_field!(class.fields(), mInnerCollisionSphere);
        let domains: u8 = ee_sim::world_field!(class.fields(), mDomains);
        r.air = domains & MOVEMENT_AIR != 0;
    }
    r
}

/// One placed map object as its passes read it (`ObjectRecord`).
fn object_record(s: &EeGfxFillObject) -> EeGfxRenderObject {
    let g = layout();
    let object = s.object as usize;
    let index = s.model_index as usize;
    let fields = object_fields(object);
    EeGfxRenderObject {
        location: Ge3DPoint { x: s.location[0], y: s.location[1], z: s.location[2] },
        model: object_word(object, g.gobj_models, index) as EeGfxModel,
        texture: object_word(object, g.gobj_textures, index) as EeGfxModel,
        bar: rd(object, g.gobj_bar as usize),
        alpha_ref: fields.mAlphaReference,
        object_opacity: fields.mOpacity,
        model_scale: s.model_scale,
        selection_size: s.selection_size,
        health: s.health,
        opacity: s.opacity,
        fraction: s.fraction,
        current_frame: s.current_frame as u32,
        next_frame: s.next_frame as u32,
        fogged: s.is_fogged,
    }
}

fn index(entry: usize, pool: usize, stride: usize) -> u32 {
    (entry.wrapping_sub(pool) / stride) as u32
}

fn map_indices(map: EeGfxRfilPairs, pool: usize, stride: usize) -> Vec<u32> {
    if map.data.is_null() {
        return Vec::new();
    }
    // SAFETY: the frame's flattened map
    unsafe { std::slice::from_raw_parts(map.data, map.count) }
        .iter()
        .map(|p: &EeGfxRfilPair| index(p.value as usize, pool, stride))
        .collect()
}

fn pointer_indices(span: crate::gfx::rfil::EeGfxRfilSpan, pool: usize, stride: usize) -> Vec<u32> {
    if span.data.is_null() {
        return Vec::new();
    }
    // SAFETY: the frame's vector of pointers
    unsafe { std::slice::from_raw_parts(span.data as *const usize, span.count) }
        .iter()
        .map(|&p| index(p, pool, stride))
        .collect()
}

fn pool_or_none(entry: *const c_void, pool: usize, stride: usize) -> i32 {
    if entry.is_null() {
        -1
    } else {
        index(entry as usize, pool, stride) as i32
    }
}

fn ptr_or_null<T>(v: &[T]) -> *const T {
    if v.is_empty() {
        std::ptr::null()
    } else {
        v.as_ptr()
    }
}

/// The frame's records and lists, then `scene` (its settings: the viewport, flags, window) with them, drawn
/// (`labels`: the labels' pass, else the scene's).
///
/// # Safety
/// The frame's pointers are live (the fill's lists, under the world's read lock); `raster` and `scene` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_units_draw_lists(
    raster: *const EeGfxRaster,
    frame: *const EeGfxRfilFrame,
    scene: *const EeGfxUnitsScene,
    labels: bool,
) {
    // SAFETY: the caller's contract
    let (f, mut s) = unsafe { (&*frame, std::ptr::read(scene)) };
    // SAFETY: as above
    let l = unsafe { &*f.layout };
    // SAFETY: inside the graphics thread's fill, the world's read lock held
    let t = unsafe { WorldRead::begin() };
    let (units_pool, unit_stride) = (f.units as usize, l.unit_stride as usize);
    let (shadows_pool, shadow_stride) = (f.shadows as usize, std::mem::size_of::<EeGfxFillObject>());
    let units: Vec<EeGfxRenderUnit> =
        (0..f.units_used).map(|i| unit_record(&t, l, units_pool + i * unit_stride)).collect();
    let objects: Vec<EeGfxRenderObject> =
        (0..f.shadows_used).map(|i| object_record(&rd::<EeGfxFillObject>(shadows_pool, i * shadow_stride))).collect();
    let lists = [
        map_indices(f.maps[0], units_pool, unit_stride),
        map_indices(f.maps[1], units_pool, unit_stride),
        pointer_indices(f.selected_units, units_pool, unit_stride),
        map_indices(f.maps[3], shadows_pool, shadow_stride),
        map_indices(f.maps[4], shadows_pool, shadow_stride),
        pointer_indices(f.selected_objects, shadows_pool, shadow_stride),
    ];
    let ghosts: Vec<EeGfxGhostRecord> = (0..f.ghosts.count)
        .map(|i| {
            let record = f.ghosts.data as usize + i * l.record_stride as usize;
            let u: EeGfxFillUnit = rd(record, 0);
            EeGfxGhostRecord {
                unit: unit_record(&t, l, record),
                heights: rd(record, l.record_heights as usize),
                cells: rd(record, l.record_cells as usize),
                tile_x: rd::<std::ffi::c_long>(record, l.record_tile_x as usize) as i32,
                tile_y: rd::<std::ffi::c_long>(record, l.record_tile_y as usize) as i32,
                grid_size: u.grid_size as i32,
                can_place: u.can_place,
                // SAFETY: the record's live class
                land_building: unsafe { ClassView::of(&t, u.info) }.is_some_and(|c| c.land_building()),
            }
        })
        .collect();
    s.units = ptr_or_null(&units);
    s.unit_count = units.len();
    s.objects = ptr_or_null(&objects);
    s.object_count = objects.len();
    (s.units0, s.units0_count) = (ptr_or_null(&lists[0]), lists[0].len());
    (s.units1, s.units1_count) = (ptr_or_null(&lists[1]), lists[1].len());
    (s.selected_units, s.selected_unit_count) = (ptr_or_null(&lists[2]), lists[2].len());
    (s.objects0, s.objects0_count) = (ptr_or_null(&lists[3]), lists[3].len());
    (s.objects1, s.objects1_count) = (ptr_or_null(&lists[4]), lists[4].len());
    (s.selected_objects, s.selected_object_count) = (ptr_or_null(&lists[5]), lists[5].len());
    s.selected_unit = pool_or_none(f.selected_unit, units_pool, unit_stride);
    s.selected_object = pool_or_none(f.selected_object, shadows_pool, shadow_stride);
    s.ghosts = ptr_or_null(&ghosts);
    s.ghost_count = ghosts.len();
    s.debug_unit = pool_or_none(f.label_unit, units_pool, unit_stride);
    s.debug_text = f.label_name as *const u8;
    drop(t);
    // SAFETY: the caller's raster table
    let mut x = unsafe { &*raster };
    if labels {
        draw_labels(&mut x, &s);
    } else {
        render_scene(&mut x, &s);
    }
}
