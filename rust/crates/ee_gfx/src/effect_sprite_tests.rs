//! `effect_sprite.rs`'s virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::effect_2d::{effect2d, effect2d_file};
use crate::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::effect_state::{set_random_seed, EE_GFX_EFFECT_KIND_WATER_RIPPLE};
use crate::effect_vent::EeGfxFrames;
use crate::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    media: i32,
    tags: i32,
    complex: bool,
    blood: bool,
    blocked: bool,
    frames: EeGfxFrames,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        self.calls.push("base".into());
        self.base
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        panic!("the sprites load their media through object_media")
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        self.frames
    }
    fn move_to_point(&mut self) {
        self.calls.push("move".into());
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn selection_size(&mut self) -> f32 {
        0.0
    }
    fn confirm_enemy(&mut self) -> bool {
        false
    }
    fn confirm_goal(&mut self, _goal_index: u32, _tile_x: i32, _tile_y: i32) -> EeGfxConfirmGoal {
        EeGfxConfirmGoal::NoUnit
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        EeGfxConfirmUnit::default()
    }
    fn unit_location(&mut self) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 5.0;
        t.m[1][3] = 6.0;
        t.m[2][3] = 7.0;
        t
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn command_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn object_media(&mut self) -> i32 {
        self.calls.push("media".into());
        self.media
    }
    fn object_texture(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("texture {index}"));
        std::ptr::null_mut()
    }
    fn tag_count(&mut self, kind: u32) -> i32 {
        self.calls.push(format!("tags {kind}"));
        self.tags
    }
    fn tag_point(&mut self, kind: u32, index: u32) -> EeGfx2DPoint {
        self.calls.push(format!("tag {kind} {index}"));
        EeGfx2DPoint { has: self.complex, point: Ge3DPoint { x: index as f32, y: 1.0, z: 2.0 } }
    }
    fn weapon_point(&mut self, site: i32) -> Ge3DPoint {
        self.calls.push(format!("weapon {site}"));
        Ge3DPoint { x: 9.0, y: 8.0, z: 7.0 }
    }
    fn animation_sound(&mut self, l: Ge3DPoint, frame: u32, changed: bool) {
        self.calls.push(format!("sound {} {} {} {frame} {changed}", l.x, l.y, l.z));
    }
    fn draw_blood(&mut self) -> bool {
        self.blood
    }
    fn has_complex(&mut self) -> bool {
        self.complex
    }
    fn default_point(&mut self) -> Ge3DPoint {
        Ge3DPoint { x: 1.0, y: 0.0, z: 2.0 }
    }
    fn stats_scale(&mut self) -> f32 {
        2.0
    }
    fn tile_blocked(&mut self, x: i32, y: i32) -> bool {
        self.calls.push(format!("blocked {x} {y}"));
        self.blocked
    }
    fn wind(&mut self) -> Ge3DPoint {
        Ge3DPoint { x: 0.5, y: -0.5, z: 0.0 }
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 4.0,
        start_size: 1.0,
        duration: 1000,
        tick_seconds: 0.25,
        tick_time: 250.0,
        has_object: true,
        alpha_ref: 0.5,
        has_unit: true,
        unit_hit_points: 10,
        param1: 0.5,
        param2: 1.0,
        param3: 3.0,
        game_time_bits: 7,
        ..Default::default()
    }
}

fn frames() -> EeGfxFrames {
    EeGfxFrames { start: 2, time: 100, count: 4, last: 5 }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: the record is live and only reached through this pointer during the call
    unsafe { effect2d(op, r, g, h, x) }
}

#[test]
fn impact_set_up_picks_a_tag_point_by_game_time() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    let mut h = Host { tags: 3, complex: true, frames: frames(), media: 4, ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 4);
    assert_eq!(h.calls, ["media", "tags 6", "tag 6 1", "move"]);
    assert_eq!((r.sprite.tag_index, r.sprite.frame, r.bounding_radius), (1, 2, 4.0));
    assert_eq!(r.position, Ge3DPoint { x: 1.0, y: 1.0, z: 2.0 });
    assert_eq!(r.sprite.start, r.position);
    // a point's impact goes to its start
    let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    p.sprite.start = Ge3DPoint { x: 3.0, y: 4.0, z: 5.0 };
    let mut h = Host { frames: frames(), ..Default::default() };
    let g = EeGfxGraphicFacts { has_unit: false, ..facts() };
    run(EeGfxEffect3DOp::SetUp, &mut p, &g, &mut h, &mut x);
    assert_eq!(p.position, p.sprite.start);
    // no tag list: unplaced
    let mut q = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    let mut h = Host { tags: 0, frames: frames(), ..Default::default() };
    run(EeGfxEffect3DOp::SetUp, &mut q, &facts(), &mut h, &mut x);
    assert_eq!(h.calls, ["media", "tags 6"]);
}

#[test]
fn impact_steps_its_frames_and_sound() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    r.sprite.frame = 2;
    r.position = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    let mut h = Host { frames: frames(), ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!((r.sprite.frame, r.sprite.time), (4, 50.0));
    assert_eq!(h.calls, ["sound 1 2 3 4 true"]);
    // past the last frame: removed, unless looping
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    let looping = EeGfxGraphicFacts { flag1: true, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &looping, &mut h, &mut x), 0);
    assert_eq!(r.sprite.frame, 2);
    // a timed impact ages and follows its tag point on a loop
    let timed = EeGfxGraphicFacts { flag1: true, flag2: true, ..facts() };
    r.sprite.frame = 5;
    let mut h = Host { frames: frames(), complex: true, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &timed, &mut h, &mut x), 0);
    assert_eq!(r.sprite.life, 0.25);
    assert!(h.calls.contains(&"tag 6 0".to_string()));
    let dead = EeGfxGraphicFacts { unit_hit_points: 0, ..timed };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h, &mut x), 7);
}

#[test]
fn impact_queues_and_draws() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    r.sprite.frame = 3;
    r.sprite.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    let mut h = Host::default();
    let mut x = LogRaster { fog_value: 0.5, ..Default::default() };
    let g = facts();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!((r.sprite.draw_frame, r.sprite.draw_position), (3, r.sprite.start));
    assert_eq!(h.calls, ["base", "queue 4"]);
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 1 1",
            "shading 0",
            "lighting White",
            "frame 3",
            "texture",
            "fogfactor 1 2 3 0",
            "ambient 0.5 0.5 0.5",
            "transform 1 2 3",
            "scale 4",
            "prepare",
            "draw 1",
            "fog 0"
        ]
    );
    // the blood impact: nothing with blood off, red then white with it on
    r.kind = EE_GFX_EFFECT_KIND_IMPACT_BLOOD;
    let mut x = LogRaster::default();
    assert_eq!(run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x), 0);
    assert!(x.calls.is_empty());
    h.blood = true;
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(x.calls[4], "lighting Unit");
    assert_eq!(&x.calls[x.calls.len() - 3..], ["color 0.8 0 0", "draw 1", "color 1 1 1"]);
}

#[test]
fn muzzle_flash_starts_at_the_weapon_point() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MUZZLE_FLASH);
    let mut h = Host { frames: frames(), media: 3, ..Default::default() };
    let mut x = LogRaster::default();
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x), 0);
    assert_eq!(h.calls, ["weapon 3", "media", "move"]);
    assert_eq!(r.position, Ge3DPoint { x: 9.0, y: 8.0, z: 7.0 });
    assert_eq!((r.sprite.start, r.sprite.frame), (r.position, 2));
}

/// A file of typed values.
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
        if self.at >= self.items.len() {
            return 3;
        }
        *v = self.items[self.at];
        self.at += 1;
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = u as i32;
        r
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = f32::from_bits(u);
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = u != 0;
        r
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

#[test]
fn impact_files_keep_the_tag_from_0_245() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    r.sprite.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    r.sprite.time = 4.0;
    r.sprite.frame = 5;
    r.sprite.life = 0.5;
    r.sprite.tag_index = 2;
    let mut h = Host::default();
    let mut m = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut m, 0, 0x10c, &mut h), 0);
    assert_eq!(m.items.len(), 7);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut m, 0, 0x10c, &mut h), 0);
    assert_eq!(back.sprite, r.sprite);
    let mut old = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_IMPACT);
    old.sprite.tag_index = 9;
    m.at = 0;
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut old, &mut m, 0, 0xf4, &mut h), 0);
    assert_eq!(old.sprite.tag_index, 0);
    assert_eq!(h.calls, ["media", "media"]);
}

#[test]
fn lightning_flickers_and_stops_on_a_failed_load() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LIGHTNING);
    let mut h = Host { complex: true, frames: frames(), ..Default::default() };
    let mut x = LogRaster::default();
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x);
    // the default point (1, 0, 2) scaled by 2 from (5, 6, 7), half the size up
    assert_eq!(r.position, Ge3DPoint { x: 7.0, y: 6.0, z: 13.0 });
    r.sprite.draw_frame = 1;
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h, &mut x);
    assert_eq!(x.calls, ["alpharef 0.5", "fog 0"]);
    r.sprite.draw_frame = 2;
    let mut x = LogRaster::default();
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h, &mut x);
    assert_eq!(x.calls.len(), 13);
    let mut m = Mem { items: vec![0, 1], ..Default::default() };
    let mut failing = Host { media: 5, ..Default::default() };
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut r, &mut m, 0, 0, &mut failing), 5);
    assert_eq!(m.at, 0);
}

#[test]
fn plasma_loops_its_frames_while_the_unit_lives() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PLASMA_PULSE);
    r.sprite.frame = 3;
    let mut h = Host { frames: frames(), ..Default::default() };
    let mut x = LogRaster::default();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h, &mut x), 0);
    // (2 + 3) % 4
    assert_eq!((r.sprite.frame, r.sprite.time), (1, 50.0));
    assert_eq!(h.calls, ["move", "sound 5 6 7 1 true"]);
    let dead = EeGfxGraphicFacts { unit_hit_points: 0, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h, &mut x), 7);
    let hurt = EeGfxGraphicFacts { unit_hit_points: -1, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &hurt, &mut h, &mut x), 0);
}

#[test]
fn pulse_wave_shrinks_on_a_hit_and_grows_back() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PULSE_WAVE);
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.sprite.radius, r.bounding_radius), (3.0, 4.0));
    r.sprite.hit_points = 20;
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.sprite.radius, r.sprite.shrinking, r.sprite.hit_points), (3.0, true, 10));
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.sprite.radius, r.sprite.shrinking), (2.5, true));
    for _ in 0..2 {
        run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_time: 1.0, ..g }, &mut h, &mut x);
    }
    assert_eq!((r.sprite.radius, r.sprite.shrinking), (1.5, true));
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_time: 1.0, ..g }, &mut h, &mut x);
    assert_eq!((r.sprite.radius, r.sprite.shrinking), (1.0, false));
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_time: 1.0, ..g }, &mut h, &mut x);
    assert_eq!(r.sprite.radius, 1.5);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_time: 1000.0, ..g }, &mut h, &mut x), 7);
    // drawn 0.2 above, its light fading over the last tenth
    r.sprite.draw_age = 0.95;
    r.sprite.draw_radius = 1.0;
    let mut x = LogRaster { fog_value: 1.0, ..Default::default() };
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert!(x.calls.contains(&"scale 4".to_string()));
    assert!(x.calls.iter().any(|c| c.starts_with("ambient 0.49") || c.starts_with("ambient 0.50")));
    let mut m = Mem::default();
    effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut m, 0, 0, &mut h);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PULSE_WAVE);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut m, 0, 0, &mut h), 0);
    assert_eq!((back.sprite.age, back.sprite.radius, back.sprite.hit_points), (r.sprite.age, r.sprite.radius, 10));
}

#[test]
fn ripple_sits_at_water_level_while_its_tile_is_blocked() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_RIPPLE);
    r.sprite.start = Ge3DPoint { x: 3.5, y: 4.5, z: 9.0 };
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::CreateAt, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.aged.transformation.m[0][3], r.aged.transformation.m[2][3]), (3.5, 0.0));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    h.blocked = true;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(h.calls, ["blocked 3 4", "blocked 3 4"]);
    r.aged.draw_age = 0.75;
    run(EeGfxEffect3DOp::DrawPass5, &mut r, &g, &mut h, &mut x);
    assert_eq!(x.calls, ["sprite 1 3.5 4.5 0 color 1 1 1 rotation 0 size 3.25 0.5 1 0.5"]);
}

#[test]
fn fire_trail_drifts_rises_and_glows() {
    set_random_seed(1);
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_FIRE_TRAIL);
    let mut h = Host { complex: true, ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
    assert!(r.sprite.rotation >= 0.0 && r.sprite.rotation < 6.3);
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.position.x, r.bounding_radius), (5.0, 1.0));
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
    assert_eq!(r.position.x, 5.5);
    assert_eq!(r.position.z, 7.0 + 250.0 * FIRE_TRAIL_RISE);
    assert_eq!(r.bounding_radius, 0.875);
    run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert!(x.calls[0].starts_with("sprite 0 5.5 5.5"));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_time: 1000.0, ..g }, &mut h, &mut x), 7);
}
