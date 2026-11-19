//! effect_obj.rs' tests: effects as Rust objects over a fake world (every C++ call logged, in order): their
//! references and lifetime, the map's regions and the marching line's tiles, the base queues, the unit swaps, a
//! repair, a save and load round trip of a kind with no state of its own (the explosion's 2D parts), the render
//! queues. The effects' world is one registration per test binary, so the tests share it behind a lock.

use std::ffi::c_void;
use std::sync::{Mutex, MutexGuard, Once};

use super::*;
use crate::gfx::effect_reads::tests::{register, FGeffect, FGobj};

static LOCK: Mutex<()> = Mutex::new(());
static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
static TILES: Mutex<i32> = Mutex::new(1);
static REGIONS: AtomicPtr<EeGfxEffectRegions> = AtomicPtr::new(std::ptr::null_mut());
static MEDIA: AtomicPtr<FGobj> = AtomicPtr::new(std::ptr::null_mut());
static GRAPHIC: AtomicPtr<FGeffect> = AtomicPtr::new(std::ptr::null_mut());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}

pub(crate) fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

extern "C" fn never() {
    panic!("an answer the tests do not reach");
}

/// A function pointer of any answer's type that the tests never call.
pub(crate) fn unused<F: Copy>() -> F {
    assert_eq!(std::mem::size_of::<F>(), std::mem::size_of::<extern "C" fn()>());
    // SAFETY: a function pointer of another signature, never called
    unsafe { std::mem::transmute_copy(&(never as extern "C" fn())) }
}

extern "C" fn unit_reference(u: u64) {
    log(format!("unit+ {u:x}"));
}
extern "C" fn unit_release(u: u64) {
    log(format!("unit- {u:x}"));
}
extern "C" fn unit_by_id(id: u32) -> u64 {
    log(format!("unit? {id}"));
    if id == 7 {
        0
    } else {
        0x1000 + u64::from(id)
    }
}
extern "C" fn ge_reference(g: u64) {
    log(format!("ge+ {g:x}"));
}
extern "C" fn ge_release(g: u64) {
    log(format!("ge- {g:x}"));
}
extern "C" fn ge_load(g: u64, on_demand: bool) {
    log(format!("ge load {g:x} {on_demand}"));
}
extern "C" fn ge_unload(g: u64) {
    log(format!("ge unload {g:x}"));
}
extern "C" fn ge_media(g: u64) -> u64 {
    log(format!("ge media {g:x}"));
    MEDIA.load(Ordering::Relaxed) as u64
}
extern "C" fn ge_by_id(id: u32) -> u64 {
    log(format!("ge? {id}"));
    GRAPHIC.load(Ordering::Relaxed) as u64
}
extern "C" fn ge_count() -> u32 {
    0
}
extern "C" fn model_reference(m: u64) {
    log(format!("model+ {m:x}"));
}
extern "C" fn model_release(m: u64) {
    log(format!("model- {m:x}"));
}
extern "C" fn media_load(o: u64) -> i32 {
    log(format!("media load {o:x}"));
    0
}
extern "C" fn media_unload(o: u64) {
    log(format!("media unload {o:x}"));
}
extern "C" fn sound_release(n: u64) {
    log(format!("sound- {n:x}"));
}
extern "C" fn sound_object_unload(id: u32) {
    log(format!("sound object unload {id}"));
}
extern "C" fn loading() -> bool {
    false
}
extern "C" fn regions() -> *mut EeGfxEffectRegions {
    REGIONS.load(Ordering::Relaxed)
}
extern "C" fn tile_seen(x: i64, y: i64) -> i32 {
    if x < 0 || y < 0 {
        -1
    } else {
        *TILES.lock().unwrap()
    }
}
extern "C" fn doppelganger(_: i64, _: i64, _: u64, _: bool) -> bool {
    false
}
extern "C" fn in_view(_: *const Ge3DPoint, radius: f32) -> bool {
    radius < 100.0
}
extern "C" fn tick() -> EeGfxEffectTick {
    EeGfxEffectTick::default()
}
extern "C" fn screen(_: bool) -> u32 {
    0
}
extern "C" fn height(_: *const Ge3DPoint) -> f32 {
    0.0
}
extern "C" fn mesh() -> *const crate::ge::tmobj::EeGeTerrainMesh {
    std::ptr::null()
}

fn world_once() {
    static ONCE: Once = Once::new();
    ONCE.call_once(|| {
        register();
        let w = Box::leak(Box::new(EeGfxEffectWorld {
            unit_reference,
            unit_release,
            unit_by_id,
            graphic_effect_reference: ge_reference,
            graphic_effect_release: ge_release,
            graphic_effect_load: ge_load,
            graphic_effect_unload: ge_unload,
            graphic_effect_media: ge_media,
            graphic_effect_by_id: ge_by_id,
            graphic_effect_count: ge_count,
            model_reference,
            model_release,
            media_load,
            media_unload,
            sound_release,
            sound_object_unload,
            loading_saved_game: loading,
            regions,
            tile_seen,
            doppelganger,
            in_view,
            tick,
            screen,
            weapon_point: unused(),
            height,
            mesh,
            // a raster table of answers never called (the tests draw nothing)
            raster: {
                let n = std::mem::size_of::<EeGfxRaster>() / std::mem::size_of::<usize>();
                Box::leak(vec![never as *const () as usize; n].into_boxed_slice()).as_ptr() as *const EeGfxRaster
            },
            unit_graphic: unused(),
            storm: crate::gfx::ffi_effect::EeGfxStormHost {
                context: std::ptr::null_mut(),
                on_map: unused(),
                is_water: unused(),
                ground_z: unused(),
                step_toward: unused(),
                ripple: unused(),
            },
            answers3d: EeGfxEffectAnswers3D {
                shot_hit: unused(),
                animation_sound: unused(),
                wall_gate_posts: unused(),
                wall_piece: unused(),
                tracer_hit: unused(),
                volcano_build: unused(),
                farm_terrain: unused(),
                farm_drop_site: unused(),
                farm_set_variant: unused(),
                farm_doppelganger_any: unused(),
                farm_doppelganger: unused(),
                farm_opacity: unused(),
                set_scaled_model: unused(),
                ground_point: unused(),
                rain_media: unused(),
                rain_sounds: unused(),
                particle_tick: unused(),
                particle_ground: unused(),
                emitter_place: unused(),
                emitter_sound: unused(),
            },
            answers2d: EeGfxEffectAnswers2D {
                confirm_goal: unused(),
                animation_sound: unused(),
                draw_blood: unused(),
                tile_blocked: unused(),
                wind: unused(),
                area_effect: unused(),
                area_effects: unused(),
                set_area_effects: unused(),
                icons_selected: unused(),
                ground_point: unused(),
                glow_area: unused(),
                glow_rows: unused(),
                particle_tick: unused(),
                particle_ground: unused(),
            },
        }));
        // SAFETY: a leaked world
        unsafe { ee_gfx_effect_world_set(w) };
    });
}

/// The lock, a fresh log, regions of 4 x 4 regions of 10 tiles, one graphic effect (id 42) and its graphic object.
pub(crate) fn setup() -> (MutexGuard<'static, ()>, Box<FGeffect>, Box<FGobj>) {
    let guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    world_once();
    take();
    *TILES.lock().unwrap() = 1;
    let old = REGIONS.swap(ee_gfx_effect_regions_new(4, 10), Ordering::Relaxed);
    // SAFETY: the last test's regions
    unsafe { ee_gfx_effect_regions_free(old) };
    let mut g = Box::<FGeffect>::default();
    g.fields.mID = 42;
    g.fields.mKind = 4;
    let o = Box::<FGobj>::default();
    GRAPHIC.store(&*g as *const FGeffect as *mut FGeffect, Ordering::Relaxed);
    MEDIA.store(&*o as *const FGobj as *mut FGobj, Ordering::Relaxed);
    (guard, g, o)
}

pub(crate) fn addr<T>(b: &T) -> u64 {
    b as *const T as u64
}

fn region(x: i64, y: i64) -> Vec<EffectPtr> {
    with_regions(|t| t.region(x, y).cloned().unwrap_or_default()).unwrap()
}

#[test]
fn an_effect_is_made_attached_and_destroyed_on_its_last_reference() {
    let (_l, g, _o) = setup();
    let e = ee_gfx_effect_new();
    assert_eq!((head(e).references, rec(e).kind, rec(e).tile_x), (1, 0, -1));
    ee_gfx_effect_attach(e, 0x77, addr(&*g), 0x24, true);
    let ge = addr(&*g);
    assert_eq!(take(), vec!["unit+ 77".to_string(), format!("ge+ {ge:x}"), format!("ge load {ge:x} true")]);
    assert_eq!((rec(e).kind, rec(e).keeps_unit, head(e).is_2d), (0x24, true, false));
    ee_gfx_effect_reference(e);
    // SAFETY: a live effect, two references
    unsafe { ee_gfx_effect_release(e) };
    assert!(take().is_empty(), "one reference left");
    // SAFETY: the last reference
    unsafe { ee_gfx_effect_release(e) };
    assert_eq!(take(), vec![format!("ge unload {ge:x}"), format!("ge- {ge:x}"), "unit- 77".to_string()]);
}

#[test]
fn the_rain_releases_its_sounds_and_storm_model_in_the_original_order() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, kind::RAIN);
    head(e).sound = 0x51;
    head(e).rumble = 0x52;
    head(e).graphic_object = 0x60;
    rec(e).storm.sounds = [1, 2, 3];
    rec(e).storm.model = 0x90 as *mut c_void;
    // SAFETY: the last reference
    unsafe { ee_gfx_effect_release(e) };
    assert_eq!(
        take(),
        vec![
            "sound- 51",
            "sound- 52",
            "sound object unload 1",
            "sound object unload 2",
            "sound object unload 3",
            "model- 90",
            "media unload 60",
        ]
    );
}

#[test]
fn an_effect_joins_and_leaves_the_region_under_it() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, 0x24);
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = 15.0;
    t.m[1][3] = 27.0;
    ee_gfx_effect_set_transformation(e, t);
    assert_eq!((rec(e).tile_x, rec(e).tile_y), (1, 2));
    assert_eq!(region(1, 2), vec![e]);
    t.m[0][3] = 39.9;
    ee_gfx_effect_set_transformation(e, t);
    assert!(region(1, 2).is_empty());
    assert_eq!(region(3, 2), vec![e]);
    // off the grid: the region changes, no list holds it
    t.m[0][3] = 55.0;
    ee_gfx_effect_set_transformation(e, t);
    assert_eq!((rec(e).tile_x, region(3, 2).len()), (5, 0));
    // SAFETY: the last reference
    unsafe { ee_gfx_effect_release(e) };
}

#[test]
fn line_tiles_follow_the_original_walk() {
    assert_eq!(line_tiles((0, 0), (2, 0), 1.0), vec![(0, 0), (1, 0), (2, 0)]);
    assert_eq!(line_tiles((3, 3), (3, 3), 1.0), vec![(3, 3)]);
    assert_eq!(line_tiles((0, 0), (0, 2), 1.0), vec![(0, 0), (0, 1), (0, 2)]);
    // a diagonal: steps of one along the normalized direction, then the end
    assert_eq!(line_tiles((0, 0), (2, 2), 1.0), vec![(0, 0), (1, 1), (2, 2)]);
    assert_eq!(line_tiles((0, 0), (3, 1), 1.0), vec![(0, 0), (1, 0), (2, 0), (3, 1)]);
}

#[test]
fn the_marching_line_joins_the_regions_along_it_but_its_ends_and_its_own() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, kind::MARCHING_LINE);
    rec(e).march.to = Ge3DPoint { x: 35.0, y: 5.0, z: 0.0 };
    ee_gfx_effect_move_to_point(e, Ge3DPoint { x: 5.0, y: 5.0, z: 0.0 });
    assert_eq!((rec(e).tile_x, rec(e).tile_y), (0, 0));
    assert_eq!(obj(e).regions, vec![(2, 0), (1, 0)]);
    assert_eq!((region(0, 0), region(1, 0), region(2, 0), region(3, 0).len()), (vec![e], vec![e], vec![e], 0));
    // SAFETY: the last reference: its regions left
    unsafe { ee_gfx_effect_release(e) };
    assert!(region(1, 0).is_empty() && region(2, 0).is_empty() && region(0, 0).is_empty());
}

#[test]
fn the_base_queue_skips_unseen_off_map_and_out_of_view_effects() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, 0x24);
    assert_eq!(ee_gfx_effect_base_queue(e, false), 0);
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = 5.0;
    rec(e).transformation = t;
    assert_eq!(ee_gfx_effect_base_queue(e, true), 0);
    *TILES.lock().unwrap() = 0;
    assert_eq!(ee_gfx_effect_base_queue(e, true), 10, "an unseen tile without a free camera");
    *TILES.lock().unwrap() = 1;
    rec(e).bounding_radius = 200.0;
    assert_eq!(ee_gfx_effect_base_queue(e, true), 10, "out of view");
    rec(e).transformation.m[0][3] = -3.0;
    assert_eq!(ee_gfx_effect_base_queue(e, true), 10, "off the map");
    // SAFETY: the last reference
    unsafe { ee_gfx_effect_release(e) };
}

#[test]
fn unit_swaps_move_the_unit_and_the_beams_target_unless_kept() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, kind::TRACER_BEAM);
    head(e).unit = 0x10;
    head(e).target = 0x10;
    ee_gfx_effect_replace_unit(e, 0x10, 0x20);
    assert_eq!(take(), vec!["unit- 10", "unit+ 20", "unit- 10", "unit+ 20"]);
    assert_eq!((head(e).unit, head(e).target), (0x20, 0x20));
    rec(e).keeps_unit = true;
    ee_gfx_effect_replace_unit(e, 0x20, 0x30);
    assert!(take().is_empty());
    head(e).unit = 0;
    head(e).target = 0;
    // SAFETY: the last reference
    unsafe { ee_gfx_effect_release(e) };
}

#[test]
fn a_repair_resolves_the_saved_ids_or_drops_the_effect() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_begin(e, kind::TRACER_BEAM);
    head(e).unit = 3;
    head(e).unit_is_saved_id = true;
    head(e).target = 5;
    assert_eq!(ee_gfx_effect_repair(e), 0);
    assert_eq!(take(), vec!["unit? 3", "unit+ 1003", "unit? 5", "unit+ 1005"]);
    assert_eq!((head(e).unit, head(e).target, head(e).unit_is_saved_id), (0x1003, 0x1005, false));
    let f = ee_gfx_effect_new();
    ee_gfx_effect_begin(f, kind::TRACER_BEAM);
    head(f).unit = 7;
    head(f).target = NO_TARGET_SAVED;
    assert_eq!(ee_gfx_effect_repair(f), REMOVE, "its unit is gone");
    assert_eq!(head(f).target, 0);
    for x in [e, f] {
        // SAFETY: the last reference
        unsafe { ee_gfx_effect_release(x) };
    }
}

/// An effect file over a byte vector (FSFile's little-endian values, a bool as a byte).
struct Bytes {
    data: Vec<u8>,
    at: usize,
}

fn me<'a>(c: *mut c_void) -> &'a mut Bytes {
    // SAFETY: the test's file
    unsafe { &mut *(c as *mut Bytes) }
}

fn read<const N: usize>(c: *mut c_void) -> Option<[u8; N]> {
    let b = me(c);
    let v: [u8; N] = b.data.get(b.at..b.at + N)?.try_into().ok()?;
    b.at += N;
    Some(v)
}

extern "C" fn is_open(_: *mut c_void) -> bool {
    true
}
extern "C" fn ru(c: *mut c_void, v: *mut u32) -> i32 {
    let Some(b) = read::<4>(c) else { return 0xe };
    // SAFETY: the reader's value
    unsafe { *v = u32::from_le_bytes(b) };
    0
}
extern "C" fn ri(c: *mut c_void, v: *mut i32) -> i32 {
    let Some(b) = read::<4>(c) else { return 0xe };
    // SAFETY: as above
    unsafe { *v = i32::from_le_bytes(b) };
    0
}
extern "C" fn rf(c: *mut c_void, v: *mut f32) -> i32 {
    let Some(b) = read::<4>(c) else { return 0xe };
    // SAFETY: as above
    unsafe { *v = f32::from_le_bytes(b) };
    0
}
extern "C" fn rb(c: *mut c_void, v: *mut bool) -> i32 {
    let Some(b) = read::<1>(c) else { return 0xe };
    // SAFETY: as above
    unsafe { *v = b[0] != 0 };
    0
}
extern "C" fn wu(c: *mut c_void, v: u32) -> i32 {
    me(c).data.extend_from_slice(&v.to_le_bytes());
    0
}
extern "C" fn wi(c: *mut c_void, v: i32) -> i32 {
    me(c).data.extend_from_slice(&v.to_le_bytes());
    0
}
extern "C" fn wf(c: *mut c_void, v: f32) -> i32 {
    me(c).data.extend_from_slice(&v.to_le_bytes());
    0
}
extern "C" fn wb(c: *mut c_void, v: bool) -> i32 {
    me(c).data.push(u8::from(v));
    0
}

fn file(b: &mut Bytes) -> EeGfxEffectFile {
    EeGfxEffectFile {
        context: b as *mut Bytes as *mut c_void,
        is_open,
        read_u32: ru,
        read_i32: ri,
        read_f32: rf,
        read_bool: rb,
        write_u32: wu,
        write_i32: wi,
        write_f32: wf,
        write_bool: wb,
    }
}

#[test]
fn a_2d_effect_saves_and_loads_its_parts() {
    let (_l, g, o) = setup();
    let e = ee_gfx_effect_new();
    ee_gfx_effect_attach(e, 0, addr(&*g), kind::EXPLOSION, false);
    assert!(head(e).is_2d);
    obj(e).parts.push(Part {
        position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
        object: addr(&*o),
        tag: 10000,
        frame: 4,
        draw_frame: 9,
        time: 0.5,
        done: true,
    });
    rec(e).handle = 12;
    take();
    let mut bytes = Bytes { data: Vec::new(), at: 0 };
    let f = file(&mut bytes);
    // SAFETY: a live effect; the test's file
    assert_eq!(unsafe { ee_gfx_effect_save(e, &f, 0, 0x100) }, 0);
    let copy = crate::gfx::ffi_manager::ee_gfx_effect_new_loaded(kind::EXPLOSION as u32);
    // SAFETY: the copy; the test's file
    assert_eq!(unsafe { ee_gfx_effect_load(copy, &f, 0, 0x100) }, 0);
    assert!(!bytes.data.is_empty() && bytes.at == bytes.data.len(), "every byte read back");
    assert_eq!(rec(copy).handle, 12);
    assert!(head(copy).unit_is_saved_id);
    assert_eq!(head(copy).graphic_effect, addr(&*g));
    let p = obj(copy).parts[0];
    assert_eq!(
        (p.position, p.object, p.tag, p.frame, p.draw_frame, p.time, p.done),
        (Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, addr(&*o), 10000, 4, 0, 0.5, true)
    );
    let log = take();
    assert!(log.contains(&"ge? 42".to_string()) && log.contains(&format!("media load {:x}", addr(&*o))), "{log:?}");
    for x in [e, copy] {
        head(x).unit_is_saved_id = false;
        head(x).unit = 0;
        // SAFETY: the last reference
        unsafe { ee_gfx_effect_release(x) };
    }
}

#[test]
fn the_render_queues_keep_texture_order_and_release_their_references() {
    let (_l, _g, _o) = setup();
    let e = ee_gfx_effect_new();
    let f = ee_gfx_effect_new();
    for x in [e, f] {
        ee_gfx_effect_begin(x, 0x24);
    }
    queue_push(1, 0x300, e);
    queue_push(1, 0x100, f);
    queue_push(1, 0x300, f);
    ee_gfx_effect_reference(e);
    ee_gfx_effect_reference(f);
    ee_gfx_effect_reference(f);
    let order: Vec<(u64, Vec<usize>)> =
        QUEUES.lock()[1].iter().map(|(&t, d)| (t, d.iter().copied().collect())).collect();
    assert_eq!(order, vec![(0x100, vec![f as usize]), (0x300, vec![e as usize, f as usize])]);
    // SAFETY: the queued effects' references
    unsafe { ee_gfx_effect_queues_clear(true) };
    assert_eq!((head(e).references, head(f).references), (1, 1));
    assert!(QUEUES.lock().iter().all(|m| m.is_empty()));
    for x in [e, f] {
        // SAFETY: the last reference
        unsafe { ee_gfx_effect_release(x) };
    }
}
