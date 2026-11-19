//! effect_reads.rs' tests: fake graphic objects, graphic effects and units at a layout this test registers (the
//! world's is the fill replay's, the test binary's one), read as the effect hosts read them. The tag points'
//! geometry over real lists is tags_tests.rs'; here the objects carry no tag store, so every reader takes its
//! "no list" path (the unit's own location or position), as the C++ did for a model without such points.

#![allow(clippy::vec_box)]

use std::mem::offset_of;
use std::sync::Once;

use super::*;
use crate::gfx::effect_state::EeGfxEffectRecord;
use crate::gfx::golden_fill_replay::{register_layout, FClass, FCommand, FPlayer, FStats, FUnit};

#[repr(C)]
struct FGobj {
    fields: EeDataGraphicObjectFields,
    models: [usize; 14],
    textures: [usize; 14],
    tag_points: [usize; 14],
    tags: usize,
    model: usize,
    default_point: Ge3DPoint,
    top_point: Ge3DPoint,
    turrets: [usize; 3],
    orbitals: [usize; 3],
}

impl Default for FGobj {
    fn default() -> Self {
        FGobj {
            fields: ee_data::tables::graphic_object_default(),
            models: [0; 14],
            textures: [0; 14],
            tag_points: [0; 14],
            tags: 0,
            model: 0,
            default_point: Ge3DPoint::default(),
            top_point: Ge3DPoint::default(),
            turrets: [0; 3],
            orbitals: [0; 3],
        }
    }
}

#[repr(C)]
#[derive(Default)]
struct FTurret {
    position: [f32; 3],
    model: usize,
}

#[repr(C)]
#[derive(Default)]
struct FOrbital {
    vtable: usize,
    model: usize,
    speed: f32,
    angle: f32,
}

#[repr(C)]
#[derive(Default)]
struct FGeffect {
    count: usize,
    fields: EeDataGraphicEffectFields,
    object: usize,
}

fn register() {
    register_layout();
    static ONCE: Once = Once::new();
    ONCE.call_once(|| {
        set_test_layout(EeGfxObjectLayout {
            size: std::mem::size_of::<EeGfxObjectLayout>() as u64,
            gobj_fields: offset_of!(FGobj, fields) as u64,
            gobj_models: offset_of!(FGobj, models) as u64,
            gobj_textures: offset_of!(FGobj, textures) as u64,
            gobj_tag_points: offset_of!(FGobj, tag_points) as u64,
            gobj_tags: offset_of!(FGobj, tags) as u64,
            gobj_model: offset_of!(FGobj, model) as u64,
            gobj_default_point: offset_of!(FGobj, default_point) as u64,
            gobj_top_point: offset_of!(FGobj, top_point) as u64,
            gobj_turret_parts: offset_of!(FGobj, turrets) as u64,
            gobj_orbitals: offset_of!(FGobj, orbitals) as u64,
            turret_part_model: offset_of!(FTurret, model) as u64,
            orbital_model: offset_of!(FOrbital, model) as u64,
            orbital_speed: offset_of!(FOrbital, speed) as u64,
            orbital_angle: offset_of!(FOrbital, angle) as u64,
            geffect_fields: offset_of!(FGeffect, fields) as u64,
            geffect_object: offset_of!(FGeffect, object) as u64,
            complex_scale: offset_of!(FUnit, phys_scale) as u64,
            complex_frame: offset_of!(FUnit, phys_frame) as u64,
            complex_turret_angles: offset_of!(FUnit, turret_angles) as u64,
            stats_model: offset_of!(FStats, model) as u64,
        });
    });
}

fn addr<T>(b: &T) -> usize {
    b as *const T as usize
}

/// A vector's three words over `items`.
fn vector(items: &[usize]) -> [usize; 3] {
    let begin = items.as_ptr() as usize;
    let end = begin + std::mem::size_of_val(items);
    [begin, end, end]
}

thread_local! {
    static GRAPHIC: std::cell::Cell<u64> = const { std::cell::Cell::new(0) };
}

extern "C" fn unit_graphic(_context: *mut c_void, _unit: u64) -> u64 {
    GRAPHIC.with(|g| g.get())
}

struct World {
    units: Vec<Box<FUnit>>,
    _class: Box<FClass>,
    _stats: Box<FStats>,
    _player: Box<FPlayer>,
    _command: Box<FCommand>,
    graphic: Box<FGobj>,
    _effect_object: Box<FGobj>,
    effect: Box<FGeffect>,
    _turrets: Vec<Box<FTurret>>,
    _orbitals: Vec<Box<FOrbital>>,
    _lists: Vec<Vec<usize>>,
    record: *mut EeGfxEffectRecord,
    kept: u64,
    unit_model: u64,
    target: u64,
    none: u64,
    plane: u64,
}

impl Drop for World {
    fn drop(&mut self) {
        // SAFETY: the record of `ee_gfx_effect_record_new`
        unsafe { crate::gfx::ffi_effect::ee_gfx_effect_record_free(self.record) };
    }
}

fn moved(x: f32, y: f32, z: f32) -> [[f32; 4]; 4] {
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t.m
}

/// A complex unit (index 0) of class tile size 2 and a plain unit (index 1), the graphic object they show, the
/// effect's graphic effect and its own object.
fn world(kind: i32) -> World {
    register();
    let mut class = Box::new(FClass::default());
    class.fields.mTileSize = 2;
    class.fields.mIsLandBuildingClass = true;
    let mut stats = Box::new(FStats::default());
    stats.fields.mScale = 1.5;
    stats.fields.mMaxHitPoints = 80;
    let mut player = Box::new(FPlayer::default());
    player.rts.mPlayerID = 3;
    let command = Box::new(FCommand { kind: 6003 });
    let mut graphic = Box::new(FGobj::default());
    graphic.default_point = Ge3DPoint { x: 0.0, y: 0.0, z: 2.0 };
    graphic.fields.mStartFrames[13] = 4;
    graphic.fields.mFrameTimes[13] = 50;
    graphic.fields.mFrameCounts[13] = 6;
    graphic.fields.mLastFrames[2] = 9;
    graphic.fields.mLoops[3] = true;
    graphic.textures[1] = 0x7100;
    graphic.models[2] = 0x7200;
    graphic.model = 0x7300;
    stats.model = addr(&*graphic);
    let turrets = vec![
        Box::new(FTurret { model: 0x8100, ..Default::default() }),
        Box::new(FTurret { model: 0x8200, ..Default::default() }),
    ];
    let orbitals = vec![Box::new(FOrbital { model: 0x9100, speed: 2.0, angle: 0.25, ..Default::default() })];
    let lists =
        vec![turrets.iter().map(|t| addr(&**t)).collect::<Vec<_>>(), orbitals.iter().map(|o| addr(&**o)).collect()];
    graphic.turrets = vector(&lists[0]);
    graphic.orbitals = vector(&lists[1]);
    let effect_object = Box::new(FGobj { model: 0x7400, ..Default::default() });
    let mut effect = Box::new(FGeffect::default());
    effect.fields.mSize = 3.0;
    effect.fields.mFlag1 = true;
    effect.fields.mDuration = 1500;
    effect.object = addr(&*effect_object);
    let mut units = vec![Box::new(FUnit::default()), Box::new(FUnit::default())];
    for u in units.iter_mut() {
        u.class = addr(&*class);
        u.stats = addr(&*stats);
        u.player = addr(&*player);
        u.fields.mHitPoints = 40;
        u.kind = 1;
    }
    units[0].kind = 2;
    units[0].complex_unit = addr(&*units[0]);
    units[0].transform = moved(10.0, 20.0, 1.0);
    units[0].phys_scale = 0.5;
    units[0].phys_frame = 7;
    units[0].complex.mAnimation = 2;
    units[0].command = addr(&*command);
    units[1].rts.mTileX = 4;
    units[1].rts.mTileY = 6;
    GRAPHIC.with(|g| g.set(addr(&*graphic) as u64));
    let kept = addr(&*graphic) as u64;
    World {
        units,
        _class: class,
        _stats: stats,
        _player: player,
        _command: command,
        graphic,
        _effect_object: effect_object,
        effect,
        _turrets: turrets,
        _orbitals: orbitals,
        _lists: lists,
        record: crate::gfx::ffi_effect::ee_gfx_effect_record_new(kind),
        kept,
        unit_model: kept,
        target: 0,
        none: 0,
        plane: 0x7500,
    }
}

fn refs(w: &World) -> EeGfxEffectRefs {
    EeGfxEffectRefs {
        record: w.record,
        graphic_effect: addr(&*w.effect) as u64,
        graphic_object: &w.kept,
        unit_model: &w.unit_model,
        target: &w.target,
        from_unit: &w.none,
        to_unit: &w.none,
        plane_model: &w.plane,
        volcano_model: &w.none,
        unit_graphic,
    }
}

fn ask<'a>(w: &World, r: &'a EeGfxEffectRefs, unit: usize) -> Ask<'a> {
    Ask { context: std::ptr::null_mut(), refs: r, unit: addr(&*w.units[unit]) as u64, player: 0 }
}

#[test]
fn the_kept_objects_members_and_records() {
    let w = world(0x1a);
    let r = refs(&w);
    let a = ask(&w, &r, 0);
    assert_eq!(a.texture_at(1) as usize, 0x7100);
    assert_eq!(a.model_at(2) as usize, 0x7200);
    assert_eq!(a.model() as usize, 0x7300);
    assert_eq!(a.unit_model_at(2) as usize, 0x7200);
    assert_eq!(a.animation13(), EeGfxAnimation { start: 4, frame_time: 50, frame_count: 6 });
    assert_eq!(a.frames(2).last, 9);
    assert!(a.loops(3) && !a.loops(2));
    assert_eq!(a.orbital_count(), 1);
    assert_eq!(a.orbital(0), EeGfxOrbital { model: 0x9100 as EeGfxModel, speed: 2.0, angle: 0.25 });
    assert_eq!(Gobj(addr(&*w.graphic)).num_parts(), 2);
    assert_eq!(Gobj(addr(&*w.graphic)).find_turret_part(0x8200), (0, Some(1)));
    assert_eq!(Gobj(addr(&*w.graphic)).find_turret_part(0x8300), (3, None));
    assert_eq!(a.default_point(), Ge3DPoint { x: 0.0, y: 0.0, z: 2.0 });
}

#[test]
fn the_unit_reads_its_physical_model() {
    let w = world(0x1a);
    let r = refs(&w);
    let a = ask(&w, &r, 0);
    assert_eq!(a.unit_frame(), EeGfxUnitFrame { frame: 7, animation: 2 });
    assert_eq!(a.beam_unit(), EeGfxBeamUnit { complex: true, animation: 2, frame: 7 });
    assert_eq!(ask(&w, &r, 1).beam_unit(), EeGfxBeamUnit::default());
    let s = a.smoke_unit();
    assert!(s.has_unit && !s.has_tag_points);
    assert_eq!(s.position, Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
    assert_eq!(s.command, 6003);
}

#[test]
fn without_tag_points_the_readers_take_the_units_own_place() {
    let w = world(0x1a);
    let r = refs(&w);
    let a = ask(&w, &r, 0);
    let mut moved_to = false;
    a.follow(|| moved_to = true);
    assert!(moved_to);
    // SAFETY: the test's record
    assert_eq!(unsafe { (*w.record).transformation.m }, moved(10.0, 20.0, 1.0));
    let seed = crate::gfx::ffi_effect::ee_gfx_effect_random_seed();
    assert_eq!(a.spawn_point(5), Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
    assert_eq!(crate::gfx::ffi_effect::ee_gfx_effect_random_seed(), seed, "no random draw without points");
    assert_eq!(a.tag_count(5), 0);
    assert_eq!(ask(&w, &r, 1).tag_count(5), -1);
    assert_eq!(a.vent_count(), 0);
    assert_eq!(a.spawn_count(), 0);
    // the emitter follows its unit (flag 1) without a list: its position in its unscaled location
    let e = a.emitter_unit();
    assert!(e.has_unit && e.complex && e.dead);
    assert_eq!(e.location, Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 });
    assert_eq!(e.transformation.m, moved(10.0, 20.0, 1.0));
    assert_eq!(e.hit_points, 40);
    let plain = ask(&w, &r, 1).emitter_unit();
    assert!(plain.has_unit && !plain.complex);
    assert_eq!(plain.hit_points, 40);
}

#[test]
fn the_tracer_without_a_list_starts_at_the_stats_default_point() {
    let w = world(0x40);
    let r = refs(&w);
    let a = ask(&w, &r, 0);
    // SAFETY: the test's record
    unsafe { (*w.record).tracer.target_point = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 } };
    let ends = a.tracer_ends();
    assert_eq!(ends.start, Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 });
    assert_eq!(ends.end, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    assert!(!ends.has_target);
}

#[test]
fn the_icons_and_glows_read_the_owner_and_the_tile() {
    let w = world(0x2d);
    let r = refs(&w);
    let icon = ask(&w, &r, 0).icon_unit();
    assert!(icon.has_unit && icon.land_building && icon.has_player);
    assert_eq!((icon.tile_size, icon.player, icon.x, icon.y), (2, 3, 10.0, 20.0));
    let z = |x: f32, y: f32| x + y;
    let set_up = ask(&w, &r, 1).glow_set_up(&z);
    // the plain unit's tile centre (4 + 1, 6 + 1) on the "terrain"
    assert_eq!(set_up, EeGfxGlowSetUp { count: 0, position: Ge3DPoint { x: 5.0, y: 7.0, z: 12.0 } });
    let (place, kept) = ask(&w, &r, 0).glow_place(&z);
    // an unfinished land building's glows are hidden
    assert!(!place.show && !place.has_tags);
    assert_eq!(kept.frame, 7);
    assert_eq!(kept.location.m, moved(10.0, 20.0, 1.0), "no list: the location unscaled");
}

#[test]
fn the_members_positions_and_ids() {
    let w = world(0x2d);
    let r = refs(&w);
    let a = ask(&w, &r, 0);
    assert_eq!(a.plane_model() as usize, 0x7500);
    assert!(a.volcano_model().is_null());
    assert_eq!(a.effect_duration(), 1500);
    assert_eq!(a.tracer_target_id(), 0xffff_ffff);
    assert_eq!(a.march_unit_id(0), -1);
    let (from, to) = (Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 }, Ge3DPoint { x: 2.0, y: 0.0, z: 0.0 });
    assert_eq!(a.march_ends(&from, &to), [from, to]);
    let here = Ge3DPoint { x: 10.0, y: 20.0, z: 1.0 };
    assert_eq!(a.unit_position(), here);
    assert_eq!(a.complex_position(), here);
    assert_eq!(a.unit_point(), here);
    // the plain unit off a tile: its tile's centre at 0
    assert_eq!(ask(&w, &r, 1).unit_point(), Ge3DPoint { x: 4.5, y: 6.5, z: 0.0 });
    assert_eq!(a.gate_open(), None, "not a gate");
    let c = a.confirm_unit();
    assert_eq!(c.position, here);
    assert!(!c.tree && !c.dead_top, "alive");
}
