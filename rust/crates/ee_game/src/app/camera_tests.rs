//! camera.rs over a mock world: the units by id, a flat terrain of 10 x 10 tiles at height 1, the switches, the keys.

use crate::app::camera::*;
use ee_render::ge::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeTransformation, GeVector};
use ee_render::gfx::camera::EeGfxEdgePlanes;
use std::collections::BTreeMap;

#[derive(Clone, Copy, Default)]
struct Unit {
    alive: bool,
    on_map: bool,
    hidden: bool,
    container: Option<u32>,
    point: Ge3DPoint,
    location: Option<GeTransformation>,
    follow: Option<f32>,
}

#[derive(Default)]
struct World {
    units: BTreeMap<u32, Unit>,
    slots: [u32; 2],
    refs: BTreeMap<u32, i32>,
    keys: Vec<EeAppCameraKey>,
    flags: u32,
    clipping: Option<(f32, f32)>,
    log: Vec<String>,
}

impl World {
    fn new() -> Self {
        World { slots: [EE_APP_CAMERA_NO_UNIT; 2], flags: EE_APP_CAMERA_HAS_MAP, ..Default::default() }
    }
    fn add(&mut self, id: u32, point: Ge3DPoint) {
        self.units.insert(id, Unit { alive: true, on_map: true, point, ..Default::default() });
    }
}

impl CameraHost for World {
    fn unit(&mut self, slot: u32) -> u32 {
        let id = self.slots[slot as usize];
        if self.units.contains_key(&id) {
            id
        } else {
            EE_APP_CAMERA_NO_UNIT
        }
    }
    fn set_unit(&mut self, slot: u32, id: u32) {
        self.slots[slot as usize] = id;
    }
    fn reference(&mut self, id: u32) {
        *self.refs.entry(id).or_default() += 1;
        self.log.push(format!("ref {id}"));
    }
    fn dereference(&mut self, id: u32) {
        *self.refs.entry(id).or_default() -= 1;
        self.log.push(format!("deref {id}"));
    }
    fn unit_state(&mut self, id: u32) -> u32 {
        match self.units.get(&id) {
            None => 0,
            Some(u) => {
                EE_APP_CAMERA_EXISTS
                    | if u.alive { EE_APP_CAMERA_ALIVE } else { 0 }
                    | if u.on_map { EE_APP_CAMERA_ON_MAP } else { 0 }
            }
        }
    }
    fn container(&mut self, id: u32) -> u32 {
        self.units.get(&id).and_then(|u| u.container).unwrap_or(id)
    }
    fn unit_point(&mut self, id: u32) -> Ge3DPoint {
        self.units[&id].point
    }
    fn unit_location(&mut self, id: u32) -> Option<GeTransformation> {
        self.units[&id].location
    }
    fn follow_rate(&mut self, id: u32) -> Option<f32> {
        self.units.get(&id).and_then(|u| u.follow)
    }
    fn hidden(&mut self, id: u32) -> bool {
        self.units.get(&id).is_some_and(|u| u.hidden)
    }
    fn key_count(&mut self) -> usize {
        self.keys.len()
    }
    fn key(&mut self, index: usize) -> EeAppCameraKey {
        self.keys[index]
    }
    fn clear_keys(&mut self) {
        self.keys.clear();
    }
    fn flags(&mut self) -> u32 {
        self.flags
    }
    fn world_transformation(&mut self) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn world_camera_yaw(&mut self) -> f32 {
        0.25
    }
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        (0.0..10.0).contains(&x) && (0.0..10.0).contains(&y)
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        self.valid_tile(x, y)
    }
    fn mesh_z(&mut self, _x: f32, _y: f32) -> f32 {
        1.0
    }
    fn edge_planes(&mut self) -> EeGfxEdgePlanes {
        let plane =
            |px, py, nx, ny| Ge3DPlane { point: Ge3DPoint::new(px, py, 0.0), normal: GeVector::new(nx, ny, 0.0) };
        EeGfxEdgePlanes {
            front: plane(0.0, 0.0, 0.0, 1.0),
            back: plane(0.0, 10.0, 0.0, -1.0),
            left: plane(0.0, 0.0, 1.0, 0.0),
            right: plane(10.0, 0.0, -1.0, 0.0),
        }
    }
    fn screen_line(&mut self, x: f32, y: f32) -> Ge3DLine {
        Ge3DLine { start: Ge3DPoint::new(x, y, 20.0), direction: GeVector::new(0.0, 1.0, -1.0) }
    }
    fn location_on_line(&mut self, line: &Ge3DLine) -> Option<Ge3DPoint> {
        Some(Ge3DPoint::new(line.start.x, line.start.y, 1.0))
    }
    fn viewport(&mut self) -> (f32, f32, f32) {
        (5.0, 5.0, 1.0)
    }
    fn viewport_camera(&mut self) -> Ge3DPoint {
        Ge3DPoint::new(5.0, 5.0, 20.0)
    }
    fn front_clipping(&mut self) -> f32 {
        0.1
    }
    fn set_clipping(&mut self, front: f32, rear: f32) {
        self.clipping = Some((front, rear));
    }
}

#[test]
fn the_constructor_is_the_originals() {
    let c = EeAppCameraFields::default();
    assert_eq!((c.mZoom, c.mPathIndex, c.mMode, c.mTargetPoint.x), (0.5, -1, 0, -10000.0));
    assert_eq!((c.mQ014.w, c.mView, c.mLocked), (1.0, GeTransformation::IDENTITY, false));
    assert_eq!(std::mem::size_of::<EeAppCameraKey>(), 0xa4);
}

#[test]
fn set_location_follows_the_terrain_and_jumps_when_immediate() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    set_location(&mut c, &mut w, Ge3DPoint::new(3.0, 4.0, 9.0), true, true);
    assert_eq!(c.mEndFocus, Ge3DPoint::new(3.0, 4.0, 1.4));
    assert_eq!(c.mStartFocus, c.mEndFocus);
    assert_eq!(c.mFraction, 0.0);
    // off the map: the minimum height
    set_location(&mut c, &mut w, Ge3DPoint::new(30.0, 4.0, 9.0), false, true);
    assert_eq!(c.mEndFocus.z, 0.4);
    assert_ne!(c.mStartFocus, c.mEndFocus);
    // a playing path refuses
    c.mLocked = true;
    set_location(&mut c, &mut w, Ge3DPoint::new(1.0, 1.0, 1.0), true, false);
    assert_eq!(c.mEndFocus.x, 30.0);
}

#[test]
fn follow_references_the_unit_and_drops_the_old_one() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    w.add(7, Ge3DPoint::new(2.0, 2.0, 1.0));
    w.add(8, Ge3DPoint::new(6.0, 6.0, 1.0));
    follow(&mut c, &mut w, 7, false, 1.0, 0.0, 0.0);
    assert_eq!(w.slots, [7, EE_APP_CAMERA_NO_UNIT]);
    assert_eq!(c.mEndFocus, Ge3DPoint::new(2.0, 2.0, 1.0));
    follow(&mut c, &mut w, 8, true, 1.0, 0.0, 0.0);
    assert_eq!(w.slots, [8, 8]);
    assert!(c.mIsFollowingUnitB);
    assert_eq!((w.refs[&7], w.refs[&8]), (0, 2));
    // a hidden unit is refused
    w.units.get_mut(&7).unwrap().hidden = true;
    follow(&mut c, &mut w, 7, false, 1.0, 0.0, 0.0);
    assert_eq!(w.slots, [8, 8]);
}

#[test]
fn a_contained_unit_is_followed_through_its_container() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    w.add(3, Ge3DPoint::new(1.0, 1.0, 1.0));
    w.add(4, Ge3DPoint::new(5.0, 5.0, 1.0));
    w.units.get_mut(&3).unwrap().container = Some(4);
    follow(&mut c, &mut w, 3, false, 0.0, 0.0, 0.0);
    assert_eq!(w.slots[0], 4);
    assert_eq!(c.mEndFocus, Ge3DPoint::new(5.0, 5.0, 1.0));
    // a zero duration jumps
    assert!(c.mChanged);
}

#[test]
fn update_drops_lost_units_and_steps_the_move() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    w.add(5, Ge3DPoint::new(4.0, 4.0, 1.0));
    follow(&mut c, &mut w, 5, false, 1.0, 0.0, 0.0);
    let mut size = 0.0;
    let r = update(&mut c, &mut w, 0.01, &mut size);
    assert_eq!(r, EE_APP_CAMERA_UPDATE_VIEW);
    assert!(c.mFraction > 0.0 && c.mFraction <= 1.0);
    w.units.get_mut(&5).unwrap().alive = false;
    update(&mut c, &mut w, 0.01, &mut size);
    assert_eq!(w.slots[0], EE_APP_CAMERA_NO_UNIT);
    assert_eq!(w.log.last().unwrap(), "deref 5");
    // no map: nothing more
    w.flags = 0;
    assert_eq!(update(&mut c, &mut w, 0.01, &mut size), EE_APP_CAMERA_UPDATE_DONE);
    // orthographic: the rectangle's size
    w.flags = EE_APP_CAMERA_HAS_MAP | EE_APP_CAMERA_ORTHOGRAPHIC;
    set_changed(&mut c);
    assert_eq!(update(&mut c, &mut w, 0.01, &mut size), EE_APP_CAMERA_UPDATE_ORTHO);
    assert!(size > 0.0);
}

#[test]
fn a_path_plays_its_keys_and_waits_out_each() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    for i in 0..2 {
        w.keys.push(EeAppCameraKey {
            mFocus: Ge3DPoint::new(i as f32 + 1.0, 1.0, 1.0),
            mWait: 0.5,
            mRate: 1.0,
            mChanged: true,
            ..Default::default()
        });
    }
    start_path(&mut c, &mut w);
    assert!(c.mLocked);
    assert_eq!((c.mPathIndex, c.mEndFocus.x), (0, 1.0));
    let mut size = 0.0;
    update(&mut c, &mut w, 0.1, &mut size); // the jump
    assert_eq!(c.mFraction, 1.0);
    for _ in 0..6 {
        update(&mut c, &mut w, 0.1, &mut size);
    }
    assert_eq!((c.mPathIndex, c.mEndFocus.x), (1, 2.0));
    // a locked camera refuses moves
    let before = c;
    scroll(&mut c, &mut w, 1.0, 0.0);
    assert_eq!(c, before);
    for _ in 0..8 {
        update(&mut c, &mut w, 0.1, &mut size);
    }
    assert!(!c.mLocked);
    assert!(w.keys.is_empty());
}

#[test]
fn scroll_leaves_the_free_camera_and_stays_on_the_map() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    set_location(&mut c, &mut w, Ge3DPoint::new(9.5, 5.0, 0.0), true, true);
    c.mMode = 3;
    c.mView.set_orientation(ee_render::ge::transform::RotationOrder::Ypr, 1.0, -0.5, 0.0);
    scroll(&mut c, &mut w, 3.0, 0.0);
    assert_eq!(c.mMode, 0);
    assert!(c.mEndFocus.x <= 10.0 && c.mEndFocus.x >= 0.0);
    assert!(c.mChanged);
    // in a cinematic the free camera stays
    c.mMode = 3;
    w.flags |= EE_APP_CAMERA_CINEMATIC;
    scroll(&mut c, &mut w, 0.5, 0.0);
    assert_eq!(c.mMode, 3);
}

#[test]
fn the_clipping_distances_reach_the_viewport() {
    let c = EeAppCameraFields::default();
    let mut w = World::new();
    set_clipping_distances(&c, &mut w);
    let (front, rear) = w.clipping.unwrap();
    assert!(front >= 0.1 && rear == 35.0);
    w.flags |= EE_APP_CAMERA_ORTHOGRAPHIC | EE_APP_CAMERA_FREE;
    set_clipping_distances(&c, &mut w);
    assert_eq!(w.clipping.unwrap(), (0.1, 85.0));
}

#[test]
fn revolve_and_rotate_turn_one_turn_each() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    revolve(&mut c, &mut w, 0.5, 0.0);
    assert!(c.mQ04C != c.mQ05C);
    let view = c.mQ04C;
    rotate(&mut c, &mut w, 0.25, 0.0);
    // the view turn is the same (through its matrix and back)
    assert!((c.mQ04C.w - view.w).abs() < 1e-6 && (c.mQ04C.z - view.z).abs() < 1e-6);
    assert!(c.mQ05C.w < 1.0);
}

#[test]
fn set_zoom_moves_the_offset_and_set_mode_keeps_the_zoom() {
    let mut c = EeAppCameraFields::default();
    let mut w = World::new();
    set_zoom(&mut c, &mut w, 1.0);
    assert!((c.mEndOffset.y - -20.5).abs() < 1e-5);
    w.flags |= EE_APP_CAMERA_FAR_REAR;
    set_mode(&mut c, &mut w, 1);
    assert_eq!((c.mMode, c.mZoom), (1, 1.0));
    assert!((c.mEndOffset.y - -42.0).abs() < 1e-5);
    set_zoom(&mut c, &mut w, -0.5);
    assert_eq!(c.mEndOffset.y, 0.1);
}
