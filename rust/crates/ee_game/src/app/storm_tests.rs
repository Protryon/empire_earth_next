//! storm.rs over a mock host: the name, the fades, the step's states and the route costs.

use crate::app::storm::{self, StormHost, STORM_BLIZZARD, STORM_RAIN, STORM_SNOW};
use std::os::raw::{c_long, c_ulong};

#[derive(Default)]
struct Mock {
    request: Option<bool>,
    path: Vec<c_long>,
    to_back: c_long,
    to_goal: c_long,
    tiles: c_ulong,
    randoms: Vec<f32>,
    log: Vec<String>,
    intensity: f32,
}

impl StormHost for Mock {
    fn request(&mut self) -> Option<bool> {
        self.request
    }
    fn take_path(&mut self) {
        self.log.push("take".into());
        self.path = vec![1, 2];
    }
    fn request_route(&mut self) {
        self.log.push("route".into());
    }
    fn path_len(&mut self) -> usize {
        self.path.len()
    }
    fn distance_squared(&mut self, goal: bool) -> c_long {
        if goal {
            self.to_goal
        } else {
            self.to_back
        }
    }
    fn pop_path(&mut self) {
        self.path.pop();
        self.log.push("pop".into());
    }
    fn move_to(&mut self, goal: bool) {
        self.log.push(format!("move {goal}"));
    }
    fn tile_count(&mut self) -> c_ulong {
        self.tiles
    }
    fn end_effect(&mut self) {
        self.log.push("end".into());
    }
    fn set_effect_intensity(&mut self, intensity: f32) {
        self.intensity = intensity;
    }
    fn game_speed(&mut self) -> f32 {
        2.0
    }
    fn random_range(&mut self, min: f32, max: f32) -> f32 {
        self.log.push(format!("range {min} {max}"));
        20000.0
    }
    fn random(&mut self) -> f32 {
        self.randoms.remove(0)
    }
    fn reset_color(&mut self) {
        self.log.push("color".into());
    }
}

#[test]
fn names_and_the_constructor() {
    assert_eq!(storm::type_name(STORM_SNOW).to_bytes(), b"Snow");
    assert_eq!(storm::type_name(8).to_bytes(), b"PlaceHolder4");
    assert_eq!(storm::type_name(9).to_bytes(), b"Type Not defined");
    let s = storm::new_fields(STORM_BLIZZARD);
    assert!(s.mIsSevere && s.mState == 1 && s.mEndTime == 0xffff_ffff && s.mStep == 0.02);
    assert!(!storm::new_fields(STORM_RAIN).mIsSevere);
}

#[test]
fn the_fades_step_toward_the_target() {
    let mut h = Mock::default();
    let mut s = storm::new_fields(STORM_RAIN);
    storm::set_target_intensity(&mut s, 5.0);
    assert_eq!(s.mTargetIntensity, 2.0);
    storm::set_target_intensity(&mut s, -1.0);
    assert_eq!(s.mTargetIntensity, 0.0);
    s.mIntensity = 0.0;
    s.mTargetIntensity = 1.0;
    storm::grow(&mut s, &mut h);
    assert_eq!(s.mIntensity, 0.15);
    storm::grow(&mut s, &mut h);
    assert_eq!(s.mIntensity, 0.02 * 0.15 + 0.15);
    assert_eq!(h.intensity, s.mIntensity);
    s.mIntensity = 0.005;
    s.mTargetIntensity = 0.0;
    storm::shrink(&mut s, &mut h);
    assert_eq!(s.mIntensity, 0.0);
}

#[test]
fn the_step_routes_moves_lingers_and_ends() {
    let mut h = Mock { request: Some(false), tiles: 100, ..Default::default() };
    let mut s = storm::new_fields(STORM_RAIN);
    s.mState = 2;
    storm::step(&mut s, &mut h, 0);
    assert_eq!(s.mState, 2);
    // the intensity fades toward 0.4
    assert_eq!(s.mTargetIntensity, 0.4);
    assert_eq!(s.mIntensity, 1.0 - 0.02 * 1.0);
    h.request = Some(true);
    storm::step(&mut s, &mut h, 0);
    assert_eq!(s.mState, 3);
    // moving: near the path's end it pops, moves to the next; the goal far; a draw above 0.25 keeps moving
    h.to_back = 4;
    h.to_goal = 100;
    h.randoms = vec![0.5];
    storm::step(&mut s, &mut h, 10);
    assert_eq!(h.log, vec!["take", "pop", "move false"]);
    assert_eq!((s.mState, s.mNextMoveTime), (3, 30010));
    // not yet time: nothing moves; a small storm dies
    h.tiles = 10;
    storm::step(&mut s, &mut h, 20);
    assert_eq!(s.mState, 5);
    storm::step(&mut s, &mut h, 20);
    assert_eq!(s.mState, 6);
    storm::step(&mut s, &mut h, 20);
    assert_eq!(h.log.last().unwrap(), "end");
    // at the goal: linger for the drawn stay
    let mut s = storm::new_fields(STORM_SNOW);
    s.mState = 3;
    let mut h = Mock { tiles: 100, to_goal: 9, ..Default::default() };
    storm::step(&mut s, &mut h, 5);
    assert_eq!((s.mState, s.mNextMoveTime), (7, 40005));
    assert_eq!(h.log, vec!["range 15000 45000"]);
    // route again; the end time
    s.mState = 8;
    s.mEndTime = 0;
    let mut h = Mock::default();
    storm::step(&mut s, &mut h, 5);
    assert_eq!(s.mState, 5);
    assert_eq!(h.log, vec!["route"]);
    let mut r = storm::new_fields(STORM_RAIN);
    r.mEndTime = 0;
    storm::step(&mut r, &mut h, 5);
    assert_eq!(h.log.last().unwrap(), "color");
}

#[test]
fn the_route_costs() {
    // land: +2, a hill 1; cliffs 1; forest +20; edge +500; walls +5; others +100 (snow)
    assert_eq!(storm::snow_cost(false, false, 1, &mut || false), 3);
    assert_eq!(storm::snow_cost(true, true, 1, &mut || true), 1);
    assert_eq!(storm::snow_cost(true, false, 3, &mut || false), 1);
    assert_eq!(storm::snow_cost(true, false, 4, &mut || false), 34);
    assert_eq!(storm::snow_cost(true, true, 6, &mut || false), 510);
    assert_eq!(storm::snow_cost(false, true, 0xe, &mut || false), 6);
    assert_eq!(storm::snow_cost(false, true, 2, &mut || true), 101);
    // rain: water, shallows, cliffs +20; edge, space +500; shores 1; others +100
    assert_eq!(storm::rain_cost(true, true, 2,), 30);
    assert_eq!(storm::rain_cost(true, false, 0xf), 514);
    assert_eq!(storm::rain_cost(true, false, 10), 1);
    assert_eq!(storm::rain_cost(false, false, 1), 101);
}
