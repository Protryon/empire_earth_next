//! Tests of the physics models (pm.rs, pm_ground.rs, pm_air.rs) over a mock host: the dispatch by kind, the
//! steps' arithmetic against the C++ statements written out, the clip steps, the push, the bob, the collapse's draws.

// the units are boxed for stable record addresses; `c_long` casts; the C++'s constants and `* -1` as written
#![allow(clippy::vec_box, clippy::unnecessary_cast, clippy::neg_multiply, clippy::approx_constant)]

use std::collections::BTreeMap;

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPlane, Ge3DPoint, GeOrientation, GeTransformation, GeVector};
use ee_base::math::um::{um_arctan2, um_cos};

use crate::units::physics::{self, EeUnitsSpeedIn};
use crate::units::pm::*;
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

#[derive(Default)]
struct MockUnit {
    rts: EeUnitsRtsUnitFields,
    unit: EeUnitsUnitFields,
    cx: EeUnitsComplexUnitFields,
    location: GeTransformation,
    angular: GeOrientation,
    orientation: GeOrientation,
    class: EeUnitsPmClass,
    stats: EeUnitsPmStats,
    waypoints: Vec<Ge3DPoint>,
    command: u32,
    goal_kind: u8,
    goal_location: Ge3DPoint,
    attack: u64,
    blocking: u64,
    arrived_within: f32,
    contacts: Vec<u64>,
    player: u32,
    transformations: u32,
    home: u64,
    tags: Option<Vec<Ge3DPoint>>,
}

#[derive(Default)]
struct World {
    units: Vec<Box<MockUnit>>,
    /// tiles that exist (x, y) -> (terrain, units on it)
    tiles: BTreeMap<(i32, i32), (i32, Vec<u64>)>,
    blocked: Vec<u64>,
    randoms: Vec<f32>,
    draws: u32,
    notes: Vec<(u32, u32, usize)>,
    trampled: Vec<(u64, u64)>,
    routed: Vec<u64>,
    tick: EeUnitsPmTick,
}

fn ident() -> GeTransformation {
    GeTransformation::IDENTITY
}

fn at(x: f32, y: f32, z: f32, yaw: f32) -> GeTransformation {
    let mut t = ident();
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t
}

impl World {
    fn new() -> World {
        World {
            tick: EeUnitsPmTick {
                tick_seconds: 0.025,
                ticks_per_second: 40.0,
                tick_time: 25.0,
                game_time: 1000,
                collision_period: 1,
                detection_flags: 15,
                trample_min: 0.5,
                height_epsilon: 0.01,
            },
            ..Default::default()
        }
    }

    fn add(&mut self, location: GeTransformation, radius: f32) -> u64 {
        let mut u = Box::<MockUnit>::default();
        u.location = location;
        u.cx.mCollisionRadius = radius;
        u.cx.mIsCollidable = 1;
        u.unit.mHitPoints = 100;
        u.rts.mUnitID = self.units.len() as _;
        u.rts.mTileX = location.x().floor() as _;
        u.rts.mTileY = location.y().floor() as _;
        u.class.cruise_altitude = 1.0;
        u.class.max_velocity = 2.0;
        u.class.slope_penalty = 1.0;
        u.class.tile_size = 1;
        u.stats = EeUnitsPmStats {
            max_velocity: 2.0,
            acceleration: 1.0,
            angular_acceleration: 3.0,
            max_angular_velocity: 2.0,
            scale: 1.0,
        };
        self.units.push(u);
        self.units.len() as u64
    }

    fn u(&mut self, obj: u64) -> &mut MockUnit {
        &mut self.units[obj as usize - 1]
    }

    fn view_of(&mut self, obj: u64) -> EeUnitsPmUnit {
        let u = self.u(obj);
        EeUnitsPmUnit { obj, rts: &mut u.rts, unit: &mut u.unit, cx: &mut u.cx }
    }

    fn floor_tiles(&mut self, x0: i32, x1: i32, y0: i32, y1: i32) {
        for x in x0..=x1 {
            for y in y0..=y1 {
                self.tiles.insert((x, y), (0, Vec::new()));
            }
        }
    }

    fn ctx(&mut self) -> Ctx<'_> {
        let t = self.tick;
        Ctx { h: self, t }
    }
}

impl PmHost for World {
    fn tick(&mut self) -> EeUnitsPmTick {
        self.tick
    }
    fn random(&mut self) -> f32 {
        self.draws += 1;
        self.randoms.remove(0)
    }
    fn noting(&mut self) -> bool {
        true
    }
    fn note(&mut self, kind: u32, id: u32, words: &[u32]) {
        self.notes.push((kind, id, words.len()));
    }
    fn view(&mut self, obj: u64) -> EeUnitsPmUnit {
        self.view_of(obj)
    }
    fn class(&mut self, obj: u64) -> EeUnitsPmClass {
        self.u(obj).class
    }
    fn stats(&mut self, obj: u64) -> EeUnitsPmStats {
        self.u(obj).stats
    }
    fn location(&mut self, obj: u64) -> GeTransformation {
        self.u(obj).location
    }
    fn set_transformation(&mut self, obj: u64, t: &GeTransformation) {
        let u = self.u(obj);
        u.location = *t;
        u.transformations += 1;
    }
    fn refresh_transformation(&mut self, obj: u64) {
        self.u(obj).transformations += 1;
    }
    fn set_location(&mut self, obj: u64, t: &GeTransformation) {
        self.u(obj).location = *t;
    }
    fn set_location_point(&mut self, obj: u64, p: Ge3DPoint) {
        let u = self.u(obj);
        u.location.m[0][3] = p.x;
        u.location.m[1][3] = p.y;
        u.location.m[2][3] = p.z;
    }
    fn set_location_xy(&mut self, obj: u64, x: f32, y: f32) {
        let u = self.u(obj);
        u.location.m[0][3] = x;
        u.location.m[1][3] = y;
    }
    fn update_tile(&mut self, obj: u64, t: &GeTransformation) {
        self.u(obj).location = *t;
    }
    fn set_scale(&mut self, _obj: u64, _s: f32) {}
    fn angular_velocity(&mut self, obj: u64) -> GeOrientation {
        self.u(obj).angular
    }
    fn set_angular_velocity(&mut self, obj: u64, o: GeOrientation) {
        self.u(obj).angular = o;
    }
    fn orientation(&mut self, obj: u64) -> GeOrientation {
        self.u(obj).orientation
    }
    fn set_orientation(&mut self, obj: u64, o: GeOrientation) {
        self.u(obj).orientation = o;
    }
    fn max_velocity(&mut self, obj: u64) -> f32 {
        self.u(obj).stats.max_velocity
    }
    fn acceleration(&mut self, obj: u64) -> f32 {
        self.u(obj).stats.acceleration
    }
    fn inverse_acceleration(&mut self, obj: u64) -> f32 {
        1.0 / self.u(obj).stats.acceleration
    }
    fn angular_acceleration(&mut self, obj: u64) -> f32 {
        self.u(obj).stats.angular_acceleration
    }
    fn max_angular_velocity(&mut self, obj: u64) -> f32 {
        self.u(obj).stats.max_angular_velocity
    }
    fn inverse_angular_acceleration(&mut self, obj: u64) -> f32 {
        1.0 / self.u(obj).stats.angular_acceleration
    }
    fn waypoint_count(&mut self, obj: u64) -> i32 {
        self.u(obj).waypoints.len() as i32
    }
    fn next_waypoint(&mut self, obj: u64, out: &mut Ge3DPoint) -> i32 {
        match self.u(obj).waypoints.first() {
            Some(p) => {
                *out = *p;
                0
            }
            None => 7,
        }
    }
    fn first_waypoint_z(&mut self, obj: u64) -> f32 {
        self.u(obj).waypoints.first().map(|p| p.z).unwrap_or(0.0)
    }
    fn path_length(&mut self, obj: u64, io: &mut f32) {
        let u = self.u(obj);
        let l = Ge3DPoint { x: u.location.x(), y: u.location.y(), z: 0.0 };
        if let Some(p) = u.waypoints.last() {
            *io = l.distance_to(*p);
        }
    }
    fn path_start(&mut self, obj: u64) -> Ge3DPoint {
        self.u(obj).waypoints.first().copied().unwrap_or_default()
    }
    fn command_type(&mut self, obj: u64) -> u32 {
        self.u(obj).command
    }
    fn model_parts(&mut self, _obj: u64) -> i32 {
        0
    }
    fn frame_time(&mut self, _obj: u64, _animation: u32) -> u32 {
        400
    }
    fn tile_of(&mut self, obj: u64) -> u64 {
        let u = self.u(obj);
        tile_key(u.rts.mTileX as i32, u.rts.mTileY as i32)
    }
    fn goal_kind(&mut self, obj: u64) -> u8 {
        self.u(obj).goal_kind
    }
    fn attack_target(&mut self, obj: u64) -> u64 {
        self.u(obj).attack
    }
    fn heal_target(&mut self, _obj: u64) -> u64 {
        0
    }
    fn is_cast_goal(&mut self, _obj: u64) -> bool {
        false
    }
    fn goal_location(&mut self, obj: u64) -> Ge3DPoint {
        self.u(obj).goal_location
    }
    fn calamity_target_location(&mut self, _obj: u64) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn goal_facing(&mut self, _obj: u64, _yaw: &mut f32) -> bool {
        false
    }
    fn gather_resource(&mut self, _obj: u64) -> u64 {
        0
    }
    fn build_target(&mut self, _obj: u64) -> u64 {
        0
    }
    fn resource_type(&mut self, _unit: u64) -> u32 {
        0
    }
    fn unit_complex(&mut self, unit: u64) -> u64 {
        unit
    }
    fn unit_tile(&mut self, unit: u64) -> (i32, i32) {
        let u = self.u(unit);
        (u.rts.mTileX as i32, u.rts.mTileY as i32)
    }
    fn unit_category(&mut self, unit: u64) -> u32 {
        self.u(unit).class.category
    }
    fn flies_over(&mut self, _unit: u64) -> bool {
        false
    }
    fn has_arrived(&mut self, obj: u64, distance: f32) -> bool {
        distance < self.u(obj).arrived_within
    }
    fn is_swimming(&mut self, _obj: u64) -> bool {
        false
    }
    fn place_on_ground(&mut self, _obj: u64, _p: Ge3DPoint) {}
    fn height_at(&mut self, _obj: u64, p: Ge3DPoint) -> f32 {
        p.z
    }
    fn set_animation(&mut self, obj: u64, a: i32, _b: i32, paused: bool) {
        let u = self.u(obj);
        u.cx.mAnimation = a as _;
        u.cx.mIsAnimationPaused = paused;
    }
    fn set_blocking_unit(&mut self, obj: u64, blocker: u64) {
        self.u(obj).blocking = blocker;
    }
    fn clear_goal_queue(&mut self, obj: u64) {
        self.u(obj).goal_kind = 0;
    }
    fn start_action(&mut self, obj: u64, action: i32) {
        self.u(obj).command = action as u32;
    }
    fn action_changed(&mut self, _obj: u64) {}
    fn can_stand_on(&mut self, _obj: u64, x: i32, y: i32) -> bool {
        self.tiles.get(&(x, y)).map(|t| t.1.is_empty()).unwrap_or(false)
    }
    fn friendly(&mut self, obj: u64, other: u64) -> bool {
        self.u(obj).player == self.u(other).player
    }
    fn trample(&mut self, obj: u64, other: u64) {
        self.trampled.push((obj, other));
    }
    fn contact_first(&mut self, obj: u64) -> u64 {
        // a node is (unit << 8) + index + 1
        if self.u(obj).contacts.is_empty() {
            0
        } else {
            (obj << 8) + 1
        }
    }
    fn contact_next(&mut self, node: u64) -> u64 {
        let (obj, i) = (node >> 8, (node & 0xff) as usize);
        if i < self.u(obj).contacts.len() {
            node + 1
        } else {
            0
        }
    }
    fn contact_unit(&mut self, node: u64) -> u64 {
        let (obj, i) = (node >> 8, (node & 0xff) as usize);
        self.u(obj).contacts[i - 1]
    }
    fn contact_count(&mut self, obj: u64) -> i32 {
        self.u(obj).contacts.len() as i32
    }
    fn contact_gate(&mut self, _obj: u64) -> i32 {
        0
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        if self.tiles.contains_key(&(x, y)) {
            tile_key(x, y)
        } else {
            0
        }
    }
    fn tile_info(&mut self, tile: u64) -> EeUnitsPmTile {
        let (x, y) = key_tile(tile);
        EeUnitsPmTile { terrain_type: self.tiles.get(&(x, y)).map(|t| t.0).unwrap_or(0), ai_value: 0, height: 0.0 }
    }
    fn tile_unit_first(&mut self, tile: u64) -> u64 {
        let (x, y) = key_tile(tile);
        if self.tiles[&(x, y)].1.is_empty() {
            0
        } else {
            (tile << 8) + 1
        }
    }
    fn tile_unit_next(&mut self, node: u64) -> u64 {
        let (x, y) = key_tile(node >> 8);
        if ((node & 0xff) as usize) < self.tiles[&(x, y)].1.len() {
            node + 1
        } else {
            0
        }
    }
    fn tile_unit(&mut self, node: u64) -> u64 {
        let (x, y) = key_tile(node >> 8);
        self.tiles[&(x, y)].1[(node & 0xff) as usize - 1]
    }
    fn is_edge_tile(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn can_move_on_terrain(&mut self, _obj: u64, terrain: i32) -> bool {
        terrain == 0
    }
    fn passable_by(&mut self, unit: u64, _obj: u64) -> bool {
        !self.blocked.contains(&unit)
    }
    fn unbuilt_land_building(&mut self, _unit: u64) -> bool {
        false
    }
    fn ground_point(&mut self, p: Ge3DPoint) -> Ge3DPoint {
        Ge3DPoint { z: 0.0, ..p }
    }
    fn terrain_pitch(&mut self, _t: &GeTransformation) -> f32 {
        0.0
    }
    fn terrain_roll(&mut self, _t: &GeTransformation) -> f32 {
        0.0
    }
    fn edge_planes(&mut self) -> [Ge3DPlane; 4] {
        [Ge3DPlane::default(); 4]
    }
    fn mesh_ground(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn adjust_dive_depth(&mut self, _obj: u64, _io: &mut Ge3DPoint) {}
    fn splash(&mut self, _obj: u64, _p: Ge3DPoint) {}
    fn death_effect(&mut self, _obj: u64) {}
    fn impact_decal(&mut self, _obj: u64, _p: Ge3DPoint) {}
    fn land_ripple(&mut self, _p: Ge3DPoint) {}
    fn home_base(&mut self, obj: u64) -> u64 {
        self.u(obj).home
    }
    fn carrier_tag_count(&mut self, carrier: u64) -> i32 {
        self.u(carrier).tags.as_ref().map_or(-1, |t| t.len() as i32)
    }
    fn carrier_tag(&mut self, carrier: u64, index: i32) -> Ge3DPoint {
        self.u(carrier).tags.as_ref().unwrap()[index as usize]
    }
    fn route_dock_waypoint(&mut self, obj: u64) {
        self.routed.push(obj);
    }
    fn gate_links(&mut self, _obj: u64, _carrier: &mut u64, _last: &mut u64) -> bool {
        false
    }
    fn projectile_step0(&mut self, obj: u64) {
        self.u(obj).command = 0x5030;
    }
    fn projectile_step2(&mut self, obj: u64) {
        self.u(obj).command = 0x5032;
    }
    fn missile_step0(&mut self, obj: u64) {
        self.u(obj).command = 0x4d30;
    }
}

/// a tile's mock pointer (never 0)
fn tile_key(x: i32, y: i32) -> u64 {
    (((x + 1000) as u64) << 16) | (y + 1000) as u64
}

fn key_tile(k: u64) -> (i32, i32) {
    (((k >> 16) & 0xffff) as i32 - 1000, (k & 0xffff) as i32 - 1000)
}

#[test]
fn the_kinds_are_the_singletons_order() {
    assert_eq!(Kind::from_index(0), Some(Kind::Land));
    assert_eq!(Kind::from_index(3), Some(Kind::Hovercraft));
    assert_eq!(Kind::from_index(9), Some(Kind::SeaAnimal));
    assert_eq!(Kind::from_index(11), Some(Kind::Spaceship));
    assert_eq!(Kind::from_index(12), None);
    assert!(Kind::SeaAnimal.is_submerged() && Kind::SubmergedNaval.is_submerged() && !Kind::Naval.is_submerged());
}

#[test]
fn the_projectile_and_missile_steps_are_the_hosts() {
    let mut w = World::new();
    let p = w.add(ident(), 0.1);
    let view = w.view_of(p);
    step(&mut w.ctx(), Kind::Projectile, view);
    assert_eq!(w.u(p).command, 0x5030);
    w.u(p).cx.mCurrentPhysicsAction = 2;
    step(&mut w.ctx(), Kind::Projectile, view);
    assert_eq!(w.u(p).command, 0x5032);
    w.u(p).cx.mCurrentPhysicsAction = 0;
    step0(&mut w.ctx(), Kind::Missile, view);
    assert_eq!(w.u(p).command, 0x4d30);
}

#[test]
fn a_land_unit_without_a_waypoint_stays() {
    let mut w = World::new();
    w.floor_tiles(0, 20, 0, 20);
    let a = w.add(at(10.5, 10.5, 0.0, 0.0), 0.5);
    let view = w.view_of(a);
    step0(&mut w.ctx(), Kind::Land, view);
    assert_eq!(w.u(a).transformations, 0);
    assert_eq!(w.u(a).cx.mVelocity, 0.0);
}

#[test]
fn a_land_unit_drives_toward_its_waypoint() {
    let mut w = World::new();
    w.floor_tiles(0, 30, 0, 30);
    let a = w.add(at(10.5, 10.5, 0.0, 0.0), 0.5);
    w.u(a).waypoints = vec![Ge3DPoint { x: 10.5, y: 20.5, z: 0.0 }];
    w.u(a).cx.mVelocity = 1.0;
    let view = w.view_of(a);
    step0(&mut w.ctx(), Kind::Land, view);
    // the speed: Accelerate's mode 3 (accelerate) from 1 by the acceleration times the tick
    let i = EeUnitsSpeedIn {
        tick_seconds: 0.025,
        speed: 1.0,
        max_speed: 2.0,
        accel: 1.0,
        waypoints: 1,
        ..Default::default()
    };
    let speed = physics::accelerate(&i, 10.0, 3);
    assert_eq!(w.u(a).cx.mVelocity.to_bits(), speed.to_bits());
    // heading north (yaw 0), moved by speed * tick along +y, on the ground
    let l = w.u(a).location;
    let forward = l.apply_vector(GeVector { x: 0.0, y: speed * 0.025, z: 0.0 });
    assert_eq!(l.y().to_bits(), (10.5f32 + forward.y).to_bits());
    assert_eq!(l.z(), 0.0);
    assert_eq!(w.u(a).transformations, 1);
    // the turn and the speed noted (USST kinds 5, 9, 8)
    let kinds: Vec<u32> = w.notes.iter().map(|n| n.0).collect();
    assert_eq!(kinds, vec![5, 9, 8]);
    assert_eq!(w.notes[0].2, 14);
    assert_eq!(w.notes[1].2, 15);
    assert_eq!(w.notes[2].2, 8);
}

#[test]
fn a_blocker_on_the_waypoints_tile_blocks_the_land_step() {
    let mut w = World::new();
    w.floor_tiles(0, 30, 0, 30);
    let a = w.add(at(10.5, 10.98, 0.0, 0.0), 0.5);
    let b = w.add(at(10.5, 11.5, 0.0, 0.0), 0.5);
    w.tiles.get_mut(&(10, 11)).unwrap().1.push(b);
    w.blocked.push(b);
    w.u(a).waypoints = vec![Ge3DPoint { x: 10.5, y: 11.5, z: 0.0 }];
    let view = w.view_of(a);
    let mut io = Ge3DPoint { x: 10.5, y: 11.03, z: 0.0 };
    let mv = GeVector { x: 0.0, y: 0.05, z: 0.0 };
    let moved = clip_step(&mut w.ctx(), Kind::Land, view, mv, &mut io);
    assert!(!moved);
    assert_eq!(w.u(a).blocking, b);
    assert_eq!(w.u(a).cx.mCollisionState, 2);
    assert_eq!(io.y.to_bits(), (11.03f32 - 0.05).to_bits());
    // not slowed: no collision time
    assert_eq!(w.u(a).cx.mCollisionResolutionCount, 0.0);
}

#[test]
fn a_step_off_the_map_is_undone_along_the_whole_move() {
    let mut w = World::new();
    w.floor_tiles(0, 5, 0, 5);
    let mut io = Ge3DPoint { x: 6.2, y: 2.5, z: 0.5 };
    let mv = GeVector { x: 0.5, y: 0.25, z: 0.125 };
    assert!(!clip_step_base(&mut w.ctx(), mv, &mut io));
    assert_eq!((io.x, io.y, io.z), (mv.x * -1.0 + 6.2, mv.y * -1.0 + 2.5, mv.z * -1.0 + 0.5));
    let mut io = Ge3DPoint { x: 2.2, y: 2.5, z: 0.0 };
    assert!(clip_step_base(&mut w.ctx(), mv, &mut io));
}

#[test]
fn a_ship_bobs_with_game_time() {
    let mut w = World::new();
    let s = w.add(at(4.5, 4.5, 0.0, 0.3), 1.0);
    w.u(s).class.bob_pitch = 0.1;
    w.u(s).class.bob_height = 0.2;
    w.u(s).cx.mF220 = 3.1;
    let view = w.view_of(s);
    apply_bob(&mut w.ctx(), Kind::Naval, view);
    let mut phase = 25.0f32 * 0.001_570_796_5 + 3.1;
    while phase > 3.141_592_7 {
        phase -= 6.283_185_5;
    }
    assert_eq!(w.u(s).cx.mF220.to_bits(), phase.to_bits());
    let bob = (um_cos(1.570_796_4 - phase) + 1.0) * 0.5;
    assert_eq!(w.u(s).location.z().to_bits(), (-(0.2f32 * bob)).to_bits());
    // the heading kept
    let l = w.u(s).location;
    let f = l.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    assert!((um_arctan2(-f.x, f.y) - 0.3).abs() < 1e-3);
    // the land model has no bob
    apply_bob(&mut w.ctx(), Kind::Land, view);
    assert_eq!(w.u(s).transformations, 1);
}

#[test]
fn overlapping_units_push_each_other_and_a_large_enemy_tramples() {
    let mut w = World::new();
    let a = w.add(at(10.0, 10.0, 0.0, 0.0), 1.0);
    let b = w.add(at(10.6, 10.4, 0.0, 0.0), 0.5);
    w.u(b).player = 1;
    w.u(a).cx.mVelocity = 1.0;
    let (ua, ub) = (w.view_of(a), w.view_of(b));
    assert!(!push(&mut w.ctx(), ua, ub));
    let (dx, dy) = (10.0f32 - 10.6, 10.0f32 - 10.4);
    let reach = (0.5f32 + 1.0) * 0.5;
    let reach_sq = reach * reach;
    let (xs, ys) = (dx * dx, dy * dy);
    let dist = ys + xs;
    let share = xs / dist;
    let p = (reach_sq - dist) * (1.0 / 0.5);
    // dx and dy below 0: the other is pushed +x, +y
    assert_eq!(w.u(b).cx.mPushVelocityX.to_bits(), (0.0f32 + p * share).to_bits());
    assert_eq!(w.u(b).cx.mPushVelocityY.to_bits(), (0.0f32 + (1.0 - share) * p).to_bits());
    // the other stands, is smaller and an enemy past the minimum distance: trampled
    assert_eq!(w.trampled, vec![(a, b)]);
    // a friend is not
    w.u(b).player = 0;
    w.trampled.clear();
    push(&mut w.ctx(), ua, ub);
    assert!(w.trampled.is_empty());
}

#[test]
fn a_collapsing_building_draws_twice_and_sinks() {
    let mut w = World::new();
    let b = w.add(at(7.5, 8.5, 0.2, 0.0), 2.0);
    w.u(b).class.unit_type = 0x10;
    w.randoms = vec![0.25, 0.75];
    let view = w.view_of(b);
    step(&mut w.ctx(), Kind::Building, {
        let v = view;
        unsafe { (*v.cx).mCurrentPhysicsAction = 2 };
        v
    });
    assert_eq!(w.draws, 2);
    let l = w.u(b).location;
    assert_eq!(l.x().to_bits(), (7.5f32 + (0.5 - 0.25) * 0.01).to_bits());
    assert_eq!(l.y().to_bits(), ((0.5f32 - 0.75) * 0.01 + 8.5).to_bits());
    assert_eq!(l.z().to_bits(), (0.2f32 - 0.025 * 0.13).to_bits());
}

#[test]
fn the_bank_follows_the_turn_rate_beyond_a_tenth() {
    let mut w = World::new();
    let a = w.add(ident(), 1.0);
    w.u(a).class.max_roll = 0.6;
    let view = w.view_of(a);
    assert_eq!(compute_roll(&mut w.ctx(), Kind::Airplane, view, 0.1), 0.0);
    let r = compute_roll(&mut w.ctx(), Kind::SeaAnimal, view, 1.0);
    let want = ((1.0f32 / 2.0 - 0.100_000_024) * 1.111_111_2) * 0.6;
    assert_eq!(r.to_bits(), want.to_bits());
    let r = compute_roll(&mut w.ctx(), Kind::AirAnimal, view, -1.0);
    let want = ((-1.0f32 / 2.0 + 0.100_000_024) * 1.111_111_2) * 0.6;
    assert_eq!(r.to_bits(), want.to_bits());
    // the helicopter rolls only when its class does; the others never
    assert_eq!(compute_roll(&mut w.ctx(), Kind::Helicopter, view, 1.0), 0.0);
    assert_eq!(compute_roll(&mut w.ctx(), Kind::Land, view, 1.0), 0.0);
}

#[test]
fn the_sea_animal_circles_until_order_five() {
    let mut w = World::new();
    let a = w.add(at(3.0, 3.0, -0.5, 0.0), 1.0);
    w.u(a).angular.yaw = 0.5;
    w.u(a).cx.mCurrentPhysicsOrder = 0;
    let view = w.view_of(a);
    let from = Ge3DPoint::default();
    let (left, rate, heading) = turn_toward(&mut w.ctx(), Kind::SeaAnimal, view, from, from, false);
    assert_eq!(left, 0.0);
    assert_eq!(rate.to_bits(), (0.5f32 - 3.0 * 0.025).to_bits());
    assert_eq!(heading.to_bits(), (rate * 0.025 + 0.0).to_bits());
    w.u(a).cx.mCurrentPhysicsOrder = 1;
    let (_, rate, _) = turn_toward(&mut w.ctx(), Kind::AirAnimal, view, from, from, false);
    assert_eq!(rate.to_bits(), (3.0f32 * 0.025 + 0.5).to_bits());
    // the airplane turns steadily (noted as kind 7), the spaceship too
    w.notes.clear();
    turn_toward(&mut w.ctx(), Kind::Airplane, view, from, from, false);
    turn_toward(&mut w.ctx(), Kind::Spaceship, view, from, from, false);
    assert_eq!(w.notes.iter().map(|n| n.0).collect::<Vec<_>>(), vec![7, 7]);
}

#[test]
fn an_idle_unit_without_contacts_rests_and_the_roll_levels() {
    let mut w = World::new();
    let a = w.add(ident(), 1.0);
    w.u(a).cx.mRoll = 0.3;
    w.u(a).cx.mCollisionResolutionCount = 0.0;
    let view = w.view_of(a);
    after_step(&mut w.ctx(), Kind::Naval, view);
    let want = 0.3f32 - 0.025 * 0.5;
    assert_eq!(w.u(a).cx.mRoll.to_bits(), want.to_bits());
    // the airplanes have no after-step
    w.u(a).cx.mRoll = 0.3;
    after_step(&mut w.ctx(), Kind::Airplane, view);
    assert_eq!(w.u(a).cx.mRoll, 0.3);
}

#[test]
fn a_stuck_unit_gives_up_to_idle() {
    let mut w = World::new();
    let a = w.add(ident(), 1.0);
    w.u(a).cx.mVelocity = 1.0;
    w.u(a).cx.mCollisionState = 2;
    w.u(a).goal_kind = 1;
    let view = w.view_of(a);
    give_up(&mut w.ctx(), view);
    assert_eq!(w.u(a).cx.mVelocity, 0.0);
    assert_eq!(w.u(a).cx.mCurrentPhysicsAction, 6);
    assert_eq!(w.u(a).cx.mCollisionState, 0);
    assert_eq!(w.u(a).command, 5000);
    assert_eq!(w.u(a).goal_kind, 0);
}

// ---- the carrier planes' deck walk (pm_air.rs docked, follow_carrier; EEPMAirplane::IsDocked, FollowCarrier,
// OrientOnCarrier written out)

fn deck_world() -> (World, u64, u64) {
    let mut w = World::new();
    w.floor_tiles(0, 20, 0, 20);
    let carrier = w.add(at(10.0, 10.0, 0.0, 0.5), 3.0);
    let plane = w.add(at(10.5, 10.5, 0.2, 0.0), 0.5);
    w.u(carrier).tags = Some(vec![
        Ge3DPoint { x: 9.0, y: 9.5, z: 0.4 },
        Ge3DPoint { x: 9.5, y: 10.0, z: 0.4 },
        Ge3DPoint { x: 11.0, y: 11.5, z: 0.4 },
    ]);
    let p = w.u(plane);
    p.home = carrier;
    p.class.air_traffic_type = 2;
    p.command = ACTION_TAXI;
    p.cx.mIsClearanceGranted = true;
    p.waypoints = vec![Ge3DPoint { x: 10.2, y: 10.7, z: 0.4 }];
    (w, carrier, plane)
}

#[test]
fn docked_needs_a_carrier_plane_taxiing_with_clearance_short_of_the_deck() {
    let (mut w, carrier, plane) = deck_world();
    let u = w.view_of(plane);
    assert!(crate::units::pm_air::docked(&mut w.ctx(), u));
    w.u(plane).waypoints = vec![Ge3DPoint { x: 11.0, y: 11.5, z: 0.4 }];
    assert!(!crate::units::pm_air::docked(&mut w.ctx(), u));
    // no tag list: the deck point is the carrier's position at height 1
    w.u(carrier).tags = None;
    w.u(plane).waypoints = vec![Ge3DPoint { x: 10.0, y: 10.0, z: 1.0 }];
    assert!(!crate::units::pm_air::docked(&mut w.ctx(), u));
    w.u(plane).waypoints.clear();
    assert!(crate::units::pm_air::docked(&mut w.ctx(), u));
    for (att, command, clearance) in
        [(1, ACTION_TAXI, true), (2, ACTION_CONTINUE_TO_BRAKE, true), (4, ACTION_BRAKE, false)]
    {
        let p = w.u(plane);
        p.class.air_traffic_type = att;
        p.command = command;
        p.cx.mIsClearanceGranted = clearance;
        assert!(!crate::units::pm_air::docked(&mut w.ctx(), u));
    }
    let p = w.u(plane);
    p.class.air_traffic_type = 4;
    p.cx.mIsClearanceGranted = true;
    p.home = 0;
    assert!(!crate::units::pm_air::docked(&mut w.ctx(), u));
}

#[test]
fn follow_carrier_places_the_plane_along_the_deck() {
    let (mut w, carrier, plane) = deck_world();
    let u = w.view_of(plane);
    crate::units::pm_air::follow_carrier(&mut w.ctx(), u);
    // the C++ statements: the walked distance, the deck's length less it, the carrier's axes at tag 0
    let here = Ge3DPoint { x: 10.5, y: 10.5, z: 0.2 };
    let next = Ge3DPoint { x: 10.2, y: 10.7, z: 0.4 };
    let start = Ge3DPoint { x: 9.0, y: 9.5, z: 0.4 };
    let deck = Ge3DPoint { x: 11.0, y: 11.5, z: 0.4 };
    let distance = start.distance_to(deck) - here.distance_to(next);
    let mut t = at(10.0, 10.0, 0.0, 0.5);
    t.m[0][3] = start.x;
    t.m[1][3] = start.y;
    t.m[2][3] = start.z;
    let p = t.apply_point(Ge3DPoint { x: 0.0, y: distance, z: 0.0 });
    let placed = w.u(plane).location;
    assert_eq!((placed.x(), placed.y(), placed.z()), (p.x, p.y, p.z));
    assert_eq!(w.u(plane).orientation, at(0.0, 0.0, 0.0, 0.5).orientation());
    assert_eq!(w.routed, vec![plane]);
    // an empty or missing list: nothing
    w.u(carrier).tags = Some(Vec::new());
    crate::units::pm_air::follow_carrier(&mut w.ctx(), u);
    w.u(carrier).tags = None;
    crate::units::pm_air::follow_carrier(&mut w.ctx(), u);
    assert_eq!(w.routed.len(), 1);
}
