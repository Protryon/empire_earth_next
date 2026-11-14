//! `effect_confirm.rs`'s tests: the command confirmation's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::effect_2d::effect2d;
use crate::effect_state::identity;
use crate::effect_vent::EeGfxFrames;
use crate::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    enemy: bool,
    goal: EeGfxConfirmGoal,
    unit: EeGfxConfirmUnit,
    frames: EeGfxFrames,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        self.calls.push("base".into());
        self.queue_result
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        self.calls.push("media".into());
        5
    }
    fn frames(&mut self, index: u32) -> EeGfxFrames {
        self.calls.push(format!("frames {index}"));
        self.frames
    }
    fn move_to_point(&mut self) {
        self.calls.push("move".into());
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn selection_size(&mut self) -> f32 {
        self.calls.push("selection".into());
        2.0
    }
    fn confirm_enemy(&mut self) -> bool {
        self.calls.push("enemy".into());
        self.enemy
    }
    fn confirm_goal(&mut self, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal {
        self.calls.push(format!("goal {goal_index} {tile_x} {tile_y}"));
        self.goal
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        self.calls.push("unit".into());
        self.unit
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.calls.push("location".into());
        let mut t = identity();
        t.m[0][3] = 100.0;
        t
    }
    fn terrain_z(&mut self, x: f32, y: f32) -> f32 {
        self.calls.push(format!("terrain {x} {y}"));
        7.0
    }
    fn command_texture(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("command texture {index}"));
        std::ptr::null_mut()
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 3.0,
        start_size: 0.5,
        duration: 1000,
        speed: 0.1,
        tick_time: 100.0,
        has_unit: true,
        has_object: true,
        alpha_ref: 0.25,
        ..Default::default()
    }
}

fn host() -> Host {
    Host { frames: EeGfxFrames { start: 2, time: 40, count: 4, last: 5 }, ..Default::default() }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, Vec<String>) {
    let mut x = LogRaster::default();
    // SAFETY: a live record, no other reference during the call
    let res = unsafe { effect2d(op, r, g, h, &mut x) };
    (res, x.calls)
}

fn made(op: EeGfxEffect3DOp, h: &mut Host) -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_COMMAND_CONFIRMATION);
    assert_eq!(run(op, &mut r, &facts(), h).0, 0);
    r
}

#[test]
fn a_confirmation_starts_in_state_2_without_size() {
    let mut h = host();
    let r = made(EeGfxEffect3DOp::Create, &mut h);
    assert!(h.calls.is_empty());
    let c = r.confirm;
    assert_eq!((c.state, c.age, c.start_size, c.end_size, c.color), (2, 0.0, 0.0, 0.0, WHITE));
    made(EeGfxEffect3DOp::CreateAt, &mut h);
    assert_eq!(h.calls, ["move"]);
}

#[test]
fn the_colour_and_size_follow_the_state() {
    for (state, color, sized) in
        [(1, RED, true), (2, YELLOW, true), (3, GREEN, true), (4, YELLOW, true), (7, WHITE, false)]
    {
        let mut h = host();
        let mut r = made(EeGfxEffect3DOp::Create, &mut h);
        let g = EeGfxGraphicFacts { int_param1: state, ..facts() };
        assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h).0, 0);
        let c = r.confirm;
        assert_eq!((c.state, c.color, r.bounding_radius), (state, color, 3.0));
        if sized {
            assert_eq!((c.start_size, c.end_size), (2.0 * 1.25 * 0.5, 2.5));
        } else {
            assert_eq!((c.start_size, c.end_size), (0.5, 3.0));
        }
        assert_eq!((c.time, c.frame, c.draw_frame), (0.0, 2, 0));
        assert_eq!(h.calls[0], "media");
        // without a unit: the start sizes
        let mut h = host();
        let mut r = made(EeGfxEffect3DOp::Create, &mut h);
        run(EeGfxEffect3DOp::SetUp, &mut r, &EeGfxGraphicFacts { has_unit: false, ..g }, &mut h);
        assert_eq!((r.confirm.start_size, r.confirm.end_size, r.confirm.color), (0.5, 3.0, color));
        assert!(!h.calls.contains(&"selection".to_string()));
    }
    // an attack on an enemy's unit
    let mut h = Host { enemy: true, ..host() };
    let mut r = made(EeGfxEffect3DOp::Create, &mut h);
    run(EeGfxEffect3DOp::SetUp, &mut r, &EeGfxGraphicFacts { int_param1: 4, ..facts() }, &mut h);
    assert_eq!(r.confirm.color, RED);
    assert_eq!(h.calls, ["media", "selection", "enemy", "frames 0"]);
}

#[test]
fn state_0_shows_the_units_goal() {
    for (goal, color) in [
        (EeGfxConfirmGoal::NoUnit, PINK),
        (EeGfxConfirmGoal::Blocked, RED),
        (EeGfxConfirmGoal::Forced, GREEN),
        (EeGfxConfirmGoal::Move, ORANGE),
        (EeGfxConfirmGoal::Unloading, YELLOW),
        (EeGfxConfirmGoal::Other, PINK),
    ] {
        for index in [0, 1] {
            let mut h = Host { goal, ..host() };
            let mut r = made(EeGfxEffect3DOp::Create, &mut h);
            r.confirm.goal_index = index;
            r.position = Ge3DPoint { x: 12.75, y: -3.5, z: 0.0 };
            run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h);
            assert_eq!(r.confirm.color, color);
            assert_eq!(h.calls[1], format!("goal {index} 12 -4"));
            let sized = index == 0 && goal != EeGfxConfirmGoal::NoUnit;
            assert_eq!(r.confirm.end_size, if sized { 2.5 } else { 3.0 });
        }
    }
}

#[test]
fn the_ring_ages_follows_its_unit_and_animates() {
    let mut h = Host {
        unit: EeGfxConfirmUnit { position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, ..Default::default() },
        ..host()
    };
    let mut r = made(EeGfxEffect3DOp::Create, &mut h);
    let g = EeGfxGraphicFacts { int_param1: 2, ..facts() };
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h);
    h.calls.clear();
    // 100 ms: 2.5 frames of 40 ms rounded to even, 2 frames, 20 ms left
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 0);
    assert_eq!(h.calls, ["frames 0", "unit", "move"]);
    assert_eq!((r.confirm.age, r.confirm.frame, r.confirm.time), (100.0, 4, 20.0));
    assert_eq!(r.position, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    // past the last frame: 7 without flag 1, back to the start with it
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 7);
    assert_eq!(r.confirm.frame, 4);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { flag1: true, ..g }, &mut h).0, 0);
    assert_eq!(r.confirm.frame, 2);
    // a tree: an id-based spot on its tile, on the ground
    h.unit = EeGfxConfirmUnit { tree: true, id: 7, tile_size: 2, tile_x: 10, tile_y: 20, ..h.unit };
    h.calls.clear();
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { flag1: true, ..g }, &mut h);
    // id 7: (7 % 3 - 1) x 2 x 0.125 + 1 + 10, (7 / 3 % 3 - 1) x 2 x 0.125 + 1 + 20
    assert_eq!(r.position, Ge3DPoint { x: 11.0, y: 21.25, z: 7.0 });
    assert_eq!(h.calls[2], "terrain 11 21.25");
    // a unit without hit points: its top point
    h.unit = EeGfxConfirmUnit {
        tree: false,
        dead_top: true,
        top: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
        scale: 2.0,
        ..h.unit
    };
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { flag1: true, ..g }, &mut h);
    assert_eq!(r.position, Ge3DPoint { x: 102.0, y: 4.0, z: 3.0 });
    // state 0 or no unit: it stays
    h.calls.clear();
    r.confirm.state = 0;
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { flag1: true, ..g }, &mut h);
    assert_eq!(h.calls, ["frames 0"]);
    // past the duration
    r.confirm.age = 1000.0;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 7);
}

#[test]
fn the_ring_queues_in_2_and_draws_in_pass_4() {
    let mut h = host();
    let mut r = made(EeGfxEffect3DOp::Create, &mut h);
    let g = EeGfxGraphicFacts { int_param1: 3, ..facts() };
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h);
    r.confirm.age = 500.0;
    r.confirm.frame = 4;
    h.calls.clear();
    h.queue_result = 10;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h).0, 10);
    assert_eq!(r.confirm.draw_age, 0.0);
    h.queue_result = 0;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h).0, 0);
    assert_eq!(h.calls, ["base", "base", "queue 2"]);
    assert_eq!((r.confirm.draw_age, r.confirm.draw_frame, r.confirm.draw_state), (500.0, 4, 3));
    h.calls.clear();
    let (res, calls) = run(EeGfxEffect3DOp::DrawPass4, &mut r, &g, &mut h);
    assert_eq!(h.calls, ["command texture 2"]);
    assert_eq!(
        calls[..7],
        ["alpharef 0.25", "fog 0", "zbuffer 2", "zcompare 1", "blend 4 5", "shading 0", "lighting White"]
    );
    assert!(calls.iter().any(|c| c.starts_with("frame")));
    assert_eq!(calls[calls.len() - 2..], ["zbuffer 0", "zcompare 0"]);
    assert_eq!(res, 0);
    // other states' textures; no draw at the start of its life (size 0)
    for (state, texture) in [(0, 1), (1, 0)] {
        r.confirm.draw_state = state;
        h.calls.clear();
        run(EeGfxEffect3DOp::DrawPass4, &mut r, &g, &mut h);
        assert_eq!(h.calls, [format!("command texture {texture}")]);
    }
    r.confirm.draw_age = 0.0;
    r.confirm.start_size = 0.0;
    assert_eq!(run(EeGfxEffect3DOp::DrawPass4, &mut r, &g, &mut h), (0, vec![]));
    for op in
        [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass5, EeGfxEffect3DOp::DrawPass6, EeGfxEffect3DOp::Load]
    {
        assert_eq!(run(op, &mut r, &g, &mut h), (0, vec![]));
    }
}
