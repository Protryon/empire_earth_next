//! `effect_parts.rs`'s virtuals over a mock host holding the parts and the logging rasterizer.

use super::*;
use crate::gfx::effect_2d::effect2d;
use crate::gfx::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::gfx::effect_sprite::EeGfx2DPoint;
use crate::gfx::effect_state::set_random_seed;
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    parts: Vec<EeGfx2DPart>,
    frames: EeGfxFrames,
    max: i32,
    follow: bool,
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
        panic!("not the parts'")
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        panic!("the parts' frames are each part's")
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
        GeTransformation::IDENTITY
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn command_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn object_media(&mut self) -> i32 {
        self.calls.push("media".into());
        0
    }
    fn animation_sound(&mut self, l: Ge3DPoint, frame: u32, changed: bool, part0: bool) {
        self.calls.push(format!("sound {} {} {} {frame} {changed} {part0}", l.x, l.y, l.z));
    }
    fn wind(&mut self) -> Ge3DPoint {
        Ge3DPoint { x: 0.1, y: 0.0, z: 0.0 }
    }
    fn create_parts(&mut self, lift: f32) -> EeGfx2DPlace {
        self.calls.push(format!("parts {lift}"));
        self.parts = (0..3)
            .map(|i| EeGfx2DPart {
                position: Ge3DPoint { x: i as f32, y: 0.0, z: 1.0 },
                tag: i,
                frame: self.frames.start,
                ..Default::default()
            })
            .collect();
        EeGfx2DPlace { position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, height: 5.0 }
    }
    fn part_count(&mut self) -> u32 {
        self.parts.len() as u32
    }
    fn part(&mut self, i: u32) -> EeGfx2DPart {
        self.parts[i as usize]
    }
    fn set_part(&mut self, i: u32, part: &EeGfx2DPart) {
        self.parts[i as usize] = *part;
    }
    fn part_frames(&mut self, _i: u32) -> EeGfxFrames {
        self.frames
    }
    fn part_texture(&mut self, _i: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn parts_snapshot(&mut self) {
        for p in &mut self.parts {
            p.draw_frame = p.frame;
        }
    }
    fn part_follow(&mut self, i: u32) -> EeGfx2DPoint {
        EeGfx2DPoint { has: self.follow, point: Ge3DPoint { x: 10.0 + i as f32, y: 0.0, z: 2.0 } }
    }
    fn max_hit_points(&mut self) -> i32 {
        self.max
    }
    fn unit_point(&mut self) -> Ge3DPoint {
        Ge3DPoint { x: 4.0, y: 4.0, z: 0.0 }
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        start_size: 1.0,
        duration: 1000,
        tick_seconds: 0.1,
        tick_time: 100.0,
        has_object: true,
        alpha_ref: 0.25,
        has_unit: true,
        unit_hit_points: 2,
        param1: 10.0,
        param2: 0.5,
        param3: 0.5,
        time1: 50,
        alpha: 0.5,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: the record is live and only reached through this pointer during the call
    unsafe { effect2d(op, r, g, h, x) }
}

#[test]
fn explosion_animates_its_parts_until_all_are_done() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EXPLOSION);
    let mut h = Host { frames: EeGfxFrames { start: 1, time: 100, count: 3, last: 2 }, ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(h.calls, ["parts 0.4", "move"]);
    assert_eq!((r.position.x, r.bounding_radius), (1.0, 5.0));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(h.parts[0].frame, 2);
    assert_eq!(h.calls[2], "sound 1 2 3 2 true true");
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    assert!(h.parts.iter().all(|p| p.done && p.frame == 2));
    // queued with the parts' frames, each drawn fog-lit
    run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
    assert_eq!(h.parts[1].draw_frame, 2);
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 3);
}

#[test]
fn fire_burns_more_parts_the_more_damaged() {
    set_random_seed(5);
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_FIRE);
    let mut h = Host {
        frames: EeGfxFrames { start: 2, time: 100, count: 4, last: 5 },
        max: 10,
        follow: true,
        ..Default::default()
    };
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
    assert!(h.parts.iter().all(|p| p.position.z == 2.0 && (2..=5).contains(&p.frame)));
    // health 0.2 of a 0.5 threshold: 0.6 of 3 parts
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(r.sprite.last_part, 2);
    let healthy = EeGfxGraphicFacts { unit_hit_points: 9, ..g };
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &healthy, &mut h, &mut x), 9);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &healthy, &mut h, &mut x), 7);
    // a step follows the unit and loops the frames within the animation
    h.parts[0].frame = 5;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(r.position, Ge3DPoint { x: 4.0, y: 4.0, z: 0.0 });
    assert_eq!((h.parts[0].frame, h.parts[0].position.z), (2, 3.0));
}

#[test]
fn exhaust_trail_spins_and_grows() {
    set_random_seed(2);
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EXHAUST_TRAIL);
    let mut h = Host { frames: EeGfxFrames { start: 1, time: 100, count: 4, last: 4 }, ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x);
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
    let clockwise = r.sprite.flag;
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
    let turn = 0.1 * 6.283_185_5;
    assert_eq!(r.sprite.angle, if clockwise { turn } else { -turn });
    assert_eq!(h.parts[0].frame, 2);
    run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
    assert_eq!(h.calls.last().unwrap(), "queue 3");
    run(EeGfxEffect3DOp::DrawPass5, &mut r, &g, &mut h, &mut x);
    // (2 - 1) / 4 * 2 * 6 + 2
    assert!(x.calls.contains(&"scale 5".to_string()));
    assert_eq!(x.calls[3], "lighting Prelit");
}

#[test]
fn muzzle_blast_trails_its_puffs() {
    set_random_seed(3);
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_MUZZLE_BLAST);
    r.sprite.start = Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 };
    r.sprite.draw_position = Ge3DPoint { x: 4.0, y: 5.0, z: 1.0 };
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::CreateBetween, &mut r, &g, &mut h, &mut x);
    assert_eq!(r.sprite.velocity, Ge3DPoint { x: 0.6, y: 0.8, z: 0.0 });
    assert_eq!((r.position.x, r.bounding_radius, r.sprite.start.x), (1.0, 2.0, 0.0));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    // 100 ms of 1000: the fade (1 - 100 / 0.5) clamps to 0, so only the wind and the rise move it
    assert_eq!(r.position.x, 2.0);
    run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(x.calls.len(), 3);
    assert!(x.calls.iter().all(|c| c.starts_with("sprite 2")));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { tick_seconds: 1.0, ..g }, &mut h, &mut x), 7);
}
