//! `effect_weather.rs`'s virtuals: a rain and a snow over a mock map and the logging rasterizer.

use super::*;
use crate::effect_beam::EeGfxBeamUnit;
use crate::effect_rising::{effect3d, effect3d_file};
use crate::effect_state::{random_seed, set_random_seed, RANDOM_TESTS};
use crate::effect_storm::EeGfxStormDrop;
use crate::ffi_effect::{ee_gfx_effect_record_free, ee_gfx_effect_record_new, ee_gfx_storm_new};
use crate::raster::{EeGfxModel, LogRaster};
use ee_ge::types::GeVector;

/// A map of land tiles 0..20 square at height x + y (water where x is 7); steps go one tile in x towards the target.
#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn model_height(&mut self) -> f32 {
        0.0
    }
    fn move_to(&mut self) {
        self.calls.push("move".into());
    }
    fn create_line(&mut self, _s: &Ge3DPoint, _d: &GeVector, _l: u32) {}
    fn base_queue(&mut self) -> i32 {
        self.base
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        0
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn texture(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn follow_unit(&mut self) {}
    fn model_at(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn shot_hit(&mut self) {}
    fn beam_start(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        EeGfxBeamUnit::default()
    }
    fn beam_hidden(&mut self) -> bool {
        false
    }
    fn place_linked(&mut self, _point: &Ge3DPoint) {}
    fn storm_on_map(&mut self, x: i32, y: i32) -> bool {
        (0..20).contains(&x) && (0..20).contains(&y)
    }
    fn storm_is_water(&mut self, x: i32, _y: i32) -> bool {
        x == 7
    }
    fn storm_ground_z(&mut self, x: i32, y: i32) -> f32 {
        (x + y) as f32
    }
    fn storm_step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2] {
        [from[0] + (to[0] - from[0]).signum(), from[1]]
    }
    fn storm_ripple(&mut self, x: f32, y: f32) {
        self.calls.push(format!("ripple {x} {y}"));
    }
    fn rain_media(&mut self) {
        self.calls.push("rain media".into());
    }
    fn rain_sounds(&mut self, start: bool, rumble: i32) {
        self.calls.push(format!("sounds {start} {rumble}"));
    }
}

/// A storm of `kind` over tiles (2..5, 3) as the tiles' constructor makes it.
fn storm(kind: i32, h: &mut Host) -> *mut EeGfxEffectRecord {
    let r = ee_gfx_effect_record_new(kind);
    let tiles = [2, 3, 3, 3, 4, 3, 5, 3];
    // SAFETY: a live record and four tiles
    unsafe { ee_gfx_storm_new(r, tiles.as_ptr(), 4) };
    call(EeGfxEffect3DOp::Create, r, &facts(), h, &mut LogRaster::default());
    r
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        param1: 2.0,
        param2: 8.0,
        param3: 1.0,
        duration: 1000,
        int_param1: 11,
        int_param2: 12,
        int_param3: 13,
        tick_time: 100.0,
        world_speed: 1.0,
        has_object: true,
        alpha_ref: 0.25,
        ..Default::default()
    }
}

fn call(op: EeGfxEffect3DOp, r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: a live record, only referenced through the pointer during the call
    unsafe { effect3d(op, r, g, h, x) }
}

fn free(r: *mut EeGfxEffectRecord) {
    // SAFETY: from ee_gfx_effect_record_new, not used again
    unsafe { ee_gfx_effect_record_free(r) };
}

#[test]
fn a_rain_stands_at_its_tiles_mean_and_waits_for_its_first_thunder() {
    let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
    let seed = random_seed();
    set_random_seed(5);
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_RAIN, &mut h);
    // SAFETY: a live record
    let s = unsafe { &(*r).storm };
    assert_eq!((s.center_x, s.center_y), (3, 3));
    // SAFETY: as above
    let t = unsafe { (*r).transformation };
    assert_eq!((t.m[0][3], t.m[1][3], t.m[2][3]), (3.0, 3.0, 6.0));
    assert!(s.rumble_delay >= 0.0 && s.rumble_delay < 2250.0, "{}", s.rumble_delay);
    assert_eq!(h.calls, vec!["move"]);
    free(r);
    set_random_seed(seed);
}

#[test]
fn the_set_up_reads_the_settings_and_builds_the_raindrop_once() {
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_RAIN, &mut h);
    h.calls.clear();
    let mut x = LogRaster::default();
    assert_eq!(call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut x), 0);
    // SAFETY: a live record
    let s = unsafe { (*r).storm };
    assert_eq!((s.fall_speed, s.spawn_height, s.density, s.duration), (2.0, 8.0, 1.0, 1000));
    assert_eq!(s.sounds, [11, 12, 13]);
    assert_eq!(s.model as usize, 1);
    // SAFETY: as above
    assert_eq!(unsafe { (*r).bounding_radius }, 1.5);
    assert_eq!(h.calls, vec!["rain media", "move"]);
    assert_eq!(
        x.calls,
        vec![
            "texture textures\\raindrop 16 true 1 1 0 false",
            "create EEE3DRainDropModel 1 3 0 1",
            "store 1 3 1",
            "ranges 1 1 3",
            "material opacity 0.47",
            "color 0.71 0.87 1",
            "texture",
            "normals",
            "bounds",
        ]
    );
    assert_eq!(x.models[0].points[2], Ge3DPoint { x: 0.0, y: 0.0, z: 0.6 });
    assert_eq!(x.models[0].triangles[0], [0, 1, 2]);
    let mut x = LogRaster::default();
    call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut x);
    assert!(!x.calls.iter().any(|c| c.starts_with("create")));
    free(r);
}

#[test]
fn the_snowflake_is_a_quad_without_a_tint_or_sounds() {
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_SNOW, &mut h);
    h.calls.clear();
    let mut x = LogRaster::default();
    call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut x);
    assert_eq!(h.calls, vec!["move"]);
    assert!(x.calls.contains(&"create EEE3DSnowFlakeModel 2 4 0 1".to_string()));
    assert!(x.calls.contains(&"material opacity 0.8".to_string()));
    assert!(!x.calls.iter().any(|c| c.starts_with("color")));
    assert_eq!(x.models[0].triangles, vec![[0, 1, 2], [0, 2, 3]]);
    // SAFETY: a live record
    assert_eq!(unsafe { (*r).storm.sounds }, [0; 3]);
    free(r);
}

/// Puts drops into the storm's lists.
fn drops(r: *mut EeGfxEffectRecord, n: usize) {
    // SAFETY: a live record
    unsafe {
        let mut l = take_storm(r);
        for i in 0..n {
            l.drops.push(EeGfxStormDrop {
                position: Ge3DPoint { x: i as f32, y: 1.0, z: 2.0 },
                angle: 0.5,
                ..Default::default()
            });
        }
        put_storm(r, l);
    }
}

#[test]
fn the_queue_copies_the_drops_for_the_draw() {
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_RAIN, &mut h);
    call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut LogRaster::default());
    drops(r, 3);
    h.calls.clear();
    h.base = 10;
    assert_eq!(call(EeGfxEffect3DOp::Queue, r, &facts(), &mut h, &mut LogRaster::default()), 10);
    assert!(h.calls.is_empty());
    h.base = 0;
    assert_eq!(call(EeGfxEffect3DOp::Queue, r, &facts(), &mut h, &mut LogRaster::default()), 0);
    assert_eq!(h.calls, vec!["queue 4"]);
    drops(r, 2);
    let mut x = LogRaster::default();
    call(EeGfxEffect3DOp::DrawPass6, r, &facts(), &mut h, &mut x);
    // the frame draws the drops queued, not the ones added since
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 3);
    assert_eq!(
        &x.calls[..6],
        ["alpharef 0.25", "zbuffer 2", "blend 4 5", "shading 0", "lighting Cloud", "lighting SceneFog"]
    );
    assert_eq!(x.calls[6], "transform 0 1 2");
    free(r);
}

#[test]
fn a_flake_fades_near_the_camera() {
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_SNOW, &mut h);
    call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut LogRaster::default());
    drops(r, 2);
    call(EeGfxEffect3DOp::Queue, r, &facts(), &mut h, &mut LogRaster::default());
    let mut x = LogRaster::default();
    x.camera.m[0][3] = 0.5;
    x.camera.m[1][3] = 1.0;
    x.camera.m[2][3] = 2.0;
    call(EeGfxEffect3DOp::DrawPass6, r, &facts(), &mut h, &mut x);
    let opacities: Vec<_> = x.calls.iter().filter(|c| c.starts_with("opacity")).cloned().collect();
    assert_eq!(opacities, vec!["opacity 0.5", "opacity 0.5"]);
    free(r);
}

#[test]
fn the_rain_starts_its_sound_then_steps_its_drops() {
    let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
    let seed = random_seed();
    set_random_seed(9);
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_RAIN, &mut h);
    call(EeGfxEffect3DOp::SetUp, r, &facts(), &mut h, &mut LogRaster::default());
    h.calls.clear();
    assert_eq!(call(EeGfxEffect3DOp::Step, r, &facts(), &mut h, &mut LogRaster::default()), 0);
    assert!(h.calls[0].starts_with("sounds true"), "{:?}", h.calls);
    // SAFETY: a live record
    assert_eq!(unsafe { (*r).storm.elapsed }, 100.0);
    h.calls.clear();
    call(EeGfxEffect3DOp::Step, r, &facts(), &mut h, &mut LogRaster::default());
    assert!(h.calls[0].starts_with("sounds false"), "{:?}", h.calls);
    free(r);
    set_random_seed(seed);
}

#[test]
fn a_storm_saves_and_loads_after_its_base() {
    let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
    let mut h = Host::default();
    let r = storm(EE_GFX_EFFECT_KIND_SNOW, &mut h);
    let mut f = Mem::default();
    // SAFETY: a live record
    let rr = unsafe { &mut *r };
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 3, rr, &mut f, 0, 0, &mut h), 3);
    assert!(f.items.is_empty());
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, rr, &mut f, 0, 0, &mut h), 0);
    let l = ee_gfx_effect_record_new(EE_GFX_EFFECT_KIND_SNOW);
    // SAFETY: a live record
    let lr = unsafe { &mut *l };
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, lr, &mut f, 0, 0, &mut h), 0);
    assert_eq!((lr.storm.center_x, lr.storm.center_y), (3, 3));
    // SAFETY: both live
    assert_eq!(unsafe { ee_gfx_storm_tile_count_of(l) }, 4);
    free(l);
    free(r);
}

#[derive(Default)]
struct Mem {
    items: Vec<u32>,
    at: usize,
}

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        let Some(&x) = self.items.get(self.at) else { return 3 };
        self.at += 1;
        *v = x;
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = x as i32;
        res
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = f32::from_bits(x);
        res
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = x != 0;
        res
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.items.push(v);
        0
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        self.write_u32(v as u32)
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.write_u32(v.to_bits())
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.write_u32(v as u32)
    }
}

/// The storm's number of tiles.
///
/// # Safety
/// `r` is a live record.
unsafe fn ee_gfx_storm_tile_count_of(r: *mut EeGfxEffectRecord) -> u32 {
    // SAFETY: the function's contract
    unsafe { crate::ffi_effect::ee_gfx_storm_tile_count(r) }
}
