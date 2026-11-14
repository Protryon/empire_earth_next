//! The physics models' host in the game: every [`PmHost`] call one C++ function (Physics
//! Thread/EEPhysicsModel.cpp, `ee_units_pm_host_*`). Absent from a test binary.

#![cfg(not(test))]

use ee_ge::types::{Ge3DPlane, Ge3DPoint, GeOrientation, GeTransformation};

use crate::pm::{EeUnitsPmClass, EeUnitsPmStats, EeUnitsPmTick, EeUnitsPmTile, EeUnitsPmUnit, PmHost};

extern "C" {
    fn ee_units_pm_host_tick(out: *mut EeUnitsPmTick);
    fn ee_units_pm_host_random() -> f32;
    fn ee_units_pm_host_noting() -> u8;
    fn ee_units_pm_host_note(kind: u32, id: u32, words: *const u32, count: u32);
    fn ee_units_pm_host_view(obj: u64, out: *mut EeUnitsPmUnit);
    fn ee_units_pm_host_class(obj: u64, out: *mut EeUnitsPmClass);
    fn ee_units_pm_host_stats(obj: u64, out: *mut EeUnitsPmStats);
    fn ee_units_pm_host_location(obj: u64, out: *mut f32);
    fn ee_units_pm_host_set_transformation(obj: u64, m: *const f32);
    fn ee_units_pm_host_refresh_transformation(obj: u64);
    fn ee_units_pm_host_set_location(obj: u64, m: *const f32);
    fn ee_units_pm_host_set_location_point(obj: u64, x: f32, y: f32, z: f32);
    fn ee_units_pm_host_set_location_xy(obj: u64, x: f32, y: f32);
    fn ee_units_pm_host_update_tile(obj: u64, m: *const f32);
    fn ee_units_pm_host_set_scale(obj: u64, s: f32);
    fn ee_units_pm_host_angular_velocity(obj: u64, out: *mut f32);
    fn ee_units_pm_host_set_angular_velocity(obj: u64, yaw: f32, pitch: f32, roll: f32);
    fn ee_units_pm_host_orientation(obj: u64, out: *mut f32);
    fn ee_units_pm_host_set_orientation(obj: u64, yaw: f32, pitch: f32, roll: f32);
    fn ee_units_pm_host_rate(obj: u64, which: i32) -> f32;
    fn ee_units_pm_host_waypoint_count(obj: u64) -> i32;
    fn ee_units_pm_host_next_waypoint(obj: u64, out: *mut f32) -> i32;
    fn ee_units_pm_host_first_waypoint_z(obj: u64) -> f32;
    fn ee_units_pm_host_path_length(obj: u64, io: *mut f32);
    fn ee_units_pm_host_command_type(obj: u64) -> u32;
    fn ee_units_pm_host_model_parts(obj: u64) -> i32;
    fn ee_units_pm_host_frame_time(obj: u64, animation: u32) -> u32;
    fn ee_units_pm_host_tile_of(obj: u64) -> u64;
    fn ee_units_pm_host_goal_kind(obj: u64) -> u8;
    fn ee_units_pm_host_goal_unit(obj: u64, which: i32) -> u64;
    fn ee_units_pm_host_goal_point(obj: u64, which: i32, out: *mut f32);
    fn ee_units_pm_host_goal_facing(obj: u64, yaw: *mut f32) -> u8;
    fn ee_units_pm_host_unit_value(unit: u64, which: i32) -> u64;
    fn ee_units_pm_host_has_arrived(obj: u64, distance: f32) -> u8;
    fn ee_units_pm_host_is_swimming(obj: u64) -> u8;
    fn ee_units_pm_host_place_on_ground(obj: u64, x: f32, y: f32, z: f32);
    fn ee_units_pm_host_height_at(obj: u64, x: f32, y: f32, z: f32) -> f32;
    fn ee_units_pm_host_set_animation(obj: u64, a: i32, b: i32, paused: u8);
    fn ee_units_pm_host_set_blocking_unit(obj: u64, blocker: u64);
    fn ee_units_pm_host_clear_goal_queue(obj: u64);
    fn ee_units_pm_host_start_action(obj: u64, action: i32);
    fn ee_units_pm_host_action_changed(obj: u64);
    fn ee_units_pm_host_can_stand_on(obj: u64, x: i32, y: i32) -> u8;
    fn ee_units_pm_host_friendly(obj: u64, other: u64) -> u8;
    fn ee_units_pm_host_trample(obj: u64, other: u64);
    fn ee_units_pm_host_list(which: i32, of: u64) -> u64;
    fn ee_units_pm_host_contact_count(obj: u64) -> i32;
    fn ee_units_pm_host_contact_gate(obj: u64) -> i32;
    fn ee_units_pm_host_tile(x: i32, y: i32) -> u64;
    fn ee_units_pm_host_tile_info(tile: u64, out: *mut EeUnitsPmTile);
    fn ee_units_pm_host_is_edge_tile(x: i32, y: i32) -> u8;
    fn ee_units_pm_host_can_move_on_terrain(obj: u64, terrain: i32) -> u8;
    fn ee_units_pm_host_passable_by(unit: u64, obj: u64) -> u8;
    fn ee_units_pm_host_unbuilt_land_building(unit: u64) -> u8;
    fn ee_units_pm_host_ground_point(x: f32, y: f32, z: f32, out: *mut f32);
    fn ee_units_pm_host_terrain_tilt(m: *const f32, roll: u8) -> f32;
    fn ee_units_pm_host_edge_planes(out: *mut f32);
    fn ee_units_pm_host_mesh_ground(x: f32, y: f32) -> f32;
    fn ee_units_pm_host_adjust_dive_depth(obj: u64, io: *mut f32);
    fn ee_units_pm_host_effect(obj: u64, which: i32, x: f32, y: f32, z: f32);
    fn ee_units_pm_host_docked(obj: u64) -> u8;
    fn ee_units_pm_host_follow_carrier(obj: u64);
    fn ee_units_pm_host_gate_links(obj: u64, carrier: *mut u64, last: *mut u64) -> u8;
    fn ee_units_pm_host_kind_step(obj: u64, which: i32);
}

/// `ee_units_pm_host_rate`'s values: the unit's (its group's) rates.
const RATE_MAX_VELOCITY: i32 = 0;
const RATE_ACCELERATION: i32 = 1;
const RATE_INVERSE_ACCELERATION: i32 = 2;
const RATE_ANGULAR_ACCELERATION: i32 = 3;
const RATE_MAX_ANGULAR_VELOCITY: i32 = 4;
const RATE_INVERSE_ANGULAR_ACCELERATION: i32 = 5;
/// `ee_units_pm_host_goal_unit`'s: the attack target (`EEUnit*`), the healed unit, the cast goal (non-zero), the
/// gather resource, the build target, the home base
const WHICH_GOAL_ATTACK: i32 = 0;
const WHICH_GOAL_HEAL: i32 = 1;
const WHICH_GOAL_CAST: i32 = 2;
const WHICH_GOAL_GATHER: i32 = 3;
const WHICH_GOAL_BUILD: i32 = 4;
const WHICH_GOAL_HOME: i32 = 5;
/// `ee_units_pm_host_goal_point`'s: the goal's location, the calamity's target location, the path's start
const POINT_GOAL: i32 = 0;
const POINT_CALAMITY: i32 = 1;
const POINT_PATH_START: i32 = 2;
/// `ee_units_pm_host_unit_value`'s (of an `EEUnit*`): the resource type, the complex unit, the tile x, the tile y,
/// the class category, the class flies over
const UNIT_RESOURCE_TYPE: i32 = 0;
const UNIT_COMPLEX: i32 = 1;
const UNIT_TILE_X: i32 = 2;
const UNIT_TILE_Y: i32 = 3;
const UNIT_CATEGORY: i32 = 4;
const UNIT_FLIES_OVER: i32 = 5;
/// `ee_units_pm_host_list`'s: the contacts' head node, a contact node's next and unit, a tile's units' head node, a
/// tile node's next and unit
const LIST_CONTACT_FIRST: i32 = 0;
const LIST_CONTACT_NEXT: i32 = 1;
const LIST_CONTACT_UNIT: i32 = 2;
const LIST_TILE_FIRST: i32 = 3;
const LIST_TILE_NEXT: i32 = 4;
const LIST_TILE_UNIT: i32 = 5;
/// `ee_units_pm_host_effect`'s: the class's splash at the point, its death effect, the impact decal, the land ripple
const EFFECT_SPLASH: i32 = 0;
const EFFECT_DEATH: i32 = 1;
const EFFECT_DECAL: i32 = 2;
const EFFECT_RIPPLE: i32 = 3;
/// `ee_units_pm_host_kind_step`'s: the projectile's Step0 and Step2, the missile's Step0
const KIND_PROJECTILE_STEP0: i32 = 0;
const KIND_PROJECTILE_STEP2: i32 = 1;
const KIND_MISSILE_STEP0: i32 = 2;

fn matrix(m: &[f32; 16]) -> GeTransformation {
    let mut t = GeTransformation::default();
    for r in 0..4 {
        t.m[r].copy_from_slice(&m[r * 4..r * 4 + 4]);
    }
    t
}

fn flat(t: &GeTransformation) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for r in 0..4 {
        out[r * 4..r * 4 + 4].copy_from_slice(&t.m[r]);
    }
    out
}

fn point(p: [f32; 3]) -> Ge3DPoint {
    Ge3DPoint { x: p[0], y: p[1], z: p[2] }
}

fn orientation(o: [f32; 3]) -> GeOrientation {
    GeOrientation { yaw: o[0], pitch: o[1], roll: o[2] }
}

/// The game's host: the C++ functions.
pub struct Game;

// SAFETY (every call): the C++ functions take the objects the C++ handed in (live units, tiles, nodes under the
// world lock) and pointers to this side's buffers of the sizes they write.
impl PmHost for Game {
    fn tick(&mut self) -> EeUnitsPmTick {
        let mut t = EeUnitsPmTick::default();
        unsafe { ee_units_pm_host_tick(&mut t) };
        t
    }
    fn random(&mut self) -> f32 {
        unsafe { ee_units_pm_host_random() }
    }
    fn noting(&mut self) -> bool {
        unsafe { ee_units_pm_host_noting() != 0 }
    }
    fn note(&mut self, kind: u32, id: u32, words: &[u32]) {
        unsafe { ee_units_pm_host_note(kind, id, words.as_ptr(), words.len() as u32) }
    }
    fn view(&mut self, obj: u64) -> EeUnitsPmUnit {
        let mut u =
            EeUnitsPmUnit { obj, rts: std::ptr::null_mut(), unit: std::ptr::null_mut(), cx: std::ptr::null_mut() };
        unsafe { ee_units_pm_host_view(obj, &mut u) };
        u
    }
    fn class(&mut self, obj: u64) -> EeUnitsPmClass {
        let mut c = EeUnitsPmClass::default();
        unsafe { ee_units_pm_host_class(obj, &mut c) };
        c
    }
    fn stats(&mut self, obj: u64) -> EeUnitsPmStats {
        let mut s = EeUnitsPmStats::default();
        unsafe { ee_units_pm_host_stats(obj, &mut s) };
        s
    }
    fn location(&mut self, obj: u64) -> GeTransformation {
        let mut m = [0.0f32; 16];
        unsafe { ee_units_pm_host_location(obj, m.as_mut_ptr()) };
        matrix(&m)
    }
    fn set_transformation(&mut self, obj: u64, t: &GeTransformation) {
        let m = flat(t);
        unsafe { ee_units_pm_host_set_transformation(obj, m.as_ptr()) }
    }
    fn refresh_transformation(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_refresh_transformation(obj) }
    }
    fn set_location(&mut self, obj: u64, t: &GeTransformation) {
        let m = flat(t);
        unsafe { ee_units_pm_host_set_location(obj, m.as_ptr()) }
    }
    fn set_location_point(&mut self, obj: u64, p: Ge3DPoint) {
        unsafe { ee_units_pm_host_set_location_point(obj, p.x, p.y, p.z) }
    }
    fn set_location_xy(&mut self, obj: u64, x: f32, y: f32) {
        unsafe { ee_units_pm_host_set_location_xy(obj, x, y) }
    }
    fn update_tile(&mut self, obj: u64, t: &GeTransformation) {
        let m = flat(t);
        unsafe { ee_units_pm_host_update_tile(obj, m.as_ptr()) }
    }
    fn set_scale(&mut self, obj: u64, s: f32) {
        unsafe { ee_units_pm_host_set_scale(obj, s) }
    }
    fn angular_velocity(&mut self, obj: u64) -> GeOrientation {
        let mut o = [0.0f32; 3];
        unsafe { ee_units_pm_host_angular_velocity(obj, o.as_mut_ptr()) };
        orientation(o)
    }
    fn set_angular_velocity(&mut self, obj: u64, o: GeOrientation) {
        unsafe { ee_units_pm_host_set_angular_velocity(obj, o.yaw, o.pitch, o.roll) }
    }
    fn orientation(&mut self, obj: u64) -> GeOrientation {
        let mut o = [0.0f32; 3];
        unsafe { ee_units_pm_host_orientation(obj, o.as_mut_ptr()) };
        orientation(o)
    }
    fn set_orientation(&mut self, obj: u64, o: GeOrientation) {
        unsafe { ee_units_pm_host_set_orientation(obj, o.yaw, o.pitch, o.roll) }
    }
    fn max_velocity(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_MAX_VELOCITY) }
    }
    fn acceleration(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_ACCELERATION) }
    }
    fn inverse_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_INVERSE_ACCELERATION) }
    }
    fn angular_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_ANGULAR_ACCELERATION) }
    }
    fn max_angular_velocity(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_MAX_ANGULAR_VELOCITY) }
    }
    fn inverse_angular_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_rate(obj, RATE_INVERSE_ANGULAR_ACCELERATION) }
    }
    fn waypoint_count(&mut self, obj: u64) -> i32 {
        unsafe { ee_units_pm_host_waypoint_count(obj) }
    }
    fn next_waypoint(&mut self, obj: u64, out: &mut Ge3DPoint) -> i32 {
        let mut p = [out.x, out.y, out.z];
        let code = unsafe { ee_units_pm_host_next_waypoint(obj, p.as_mut_ptr()) };
        *out = point(p);
        code
    }
    fn first_waypoint_z(&mut self, obj: u64) -> f32 {
        unsafe { ee_units_pm_host_first_waypoint_z(obj) }
    }
    fn path_length(&mut self, obj: u64, io: &mut f32) {
        unsafe { ee_units_pm_host_path_length(obj, io) }
    }
    fn path_start(&mut self, obj: u64) -> Ge3DPoint {
        let mut p = [0.0f32; 3];
        unsafe { ee_units_pm_host_goal_point(obj, POINT_PATH_START, p.as_mut_ptr()) };
        point(p)
    }
    fn command_type(&mut self, obj: u64) -> u32 {
        unsafe { ee_units_pm_host_command_type(obj) }
    }
    fn model_parts(&mut self, obj: u64) -> i32 {
        unsafe { ee_units_pm_host_model_parts(obj) }
    }
    fn frame_time(&mut self, obj: u64, animation: u32) -> u32 {
        unsafe { ee_units_pm_host_frame_time(obj, animation) }
    }
    fn tile_of(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_tile_of(obj) }
    }
    fn goal_kind(&mut self, obj: u64) -> u8 {
        unsafe { ee_units_pm_host_goal_kind(obj) }
    }
    fn attack_target(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_ATTACK) }
    }
    fn heal_target(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_HEAL) }
    }
    fn is_cast_goal(&mut self, obj: u64) -> bool {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_CAST) != 0 }
    }
    fn goal_location(&mut self, obj: u64) -> Ge3DPoint {
        let mut p = [0.0f32; 3];
        unsafe { ee_units_pm_host_goal_point(obj, POINT_GOAL, p.as_mut_ptr()) };
        point(p)
    }
    fn calamity_target_location(&mut self, obj: u64) -> Ge3DPoint {
        let mut p = [0.0f32; 3];
        unsafe { ee_units_pm_host_goal_point(obj, POINT_CALAMITY, p.as_mut_ptr()) };
        point(p)
    }
    fn goal_facing(&mut self, obj: u64, yaw: &mut f32) -> bool {
        unsafe { ee_units_pm_host_goal_facing(obj, yaw) != 0 }
    }
    fn gather_resource(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_GATHER) }
    }
    fn build_target(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_BUILD) }
    }
    fn resource_type(&mut self, unit: u64) -> u32 {
        unsafe { ee_units_pm_host_unit_value(unit, UNIT_RESOURCE_TYPE) as u32 }
    }
    fn unit_complex(&mut self, unit: u64) -> u64 {
        unsafe { ee_units_pm_host_unit_value(unit, UNIT_COMPLEX) }
    }
    fn unit_tile(&mut self, unit: u64) -> (i32, i32) {
        unsafe {
            (
                ee_units_pm_host_unit_value(unit, UNIT_TILE_X) as i32,
                ee_units_pm_host_unit_value(unit, UNIT_TILE_Y) as i32,
            )
        }
    }
    fn unit_category(&mut self, unit: u64) -> u32 {
        unsafe { ee_units_pm_host_unit_value(unit, UNIT_CATEGORY) as u32 }
    }
    fn flies_over(&mut self, unit: u64) -> bool {
        unsafe { ee_units_pm_host_unit_value(unit, UNIT_FLIES_OVER) != 0 }
    }
    fn has_arrived(&mut self, obj: u64, distance: f32) -> bool {
        unsafe { ee_units_pm_host_has_arrived(obj, distance) != 0 }
    }
    fn is_swimming(&mut self, obj: u64) -> bool {
        unsafe { ee_units_pm_host_is_swimming(obj) != 0 }
    }
    fn place_on_ground(&mut self, obj: u64, p: Ge3DPoint) {
        unsafe { ee_units_pm_host_place_on_ground(obj, p.x, p.y, p.z) }
    }
    fn height_at(&mut self, obj: u64, p: Ge3DPoint) -> f32 {
        unsafe { ee_units_pm_host_height_at(obj, p.x, p.y, p.z) }
    }
    fn set_animation(&mut self, obj: u64, a: i32, b: i32, paused: bool) {
        unsafe { ee_units_pm_host_set_animation(obj, a, b, u8::from(paused)) }
    }
    fn set_blocking_unit(&mut self, obj: u64, blocker: u64) {
        unsafe { ee_units_pm_host_set_blocking_unit(obj, blocker) }
    }
    fn clear_goal_queue(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_clear_goal_queue(obj) }
    }
    fn start_action(&mut self, obj: u64, action: i32) {
        unsafe { ee_units_pm_host_start_action(obj, action) }
    }
    fn action_changed(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_action_changed(obj) }
    }
    fn can_stand_on(&mut self, obj: u64, x: i32, y: i32) -> bool {
        unsafe { ee_units_pm_host_can_stand_on(obj, x, y) != 0 }
    }
    fn friendly(&mut self, obj: u64, other: u64) -> bool {
        unsafe { ee_units_pm_host_friendly(obj, other) != 0 }
    }
    fn trample(&mut self, obj: u64, other: u64) {
        unsafe { ee_units_pm_host_trample(obj, other) }
    }
    fn contact_first(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_CONTACT_FIRST, obj) }
    }
    fn contact_next(&mut self, node: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_CONTACT_NEXT, node) }
    }
    fn contact_unit(&mut self, node: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_CONTACT_UNIT, node) }
    }
    fn contact_count(&mut self, obj: u64) -> i32 {
        unsafe { ee_units_pm_host_contact_count(obj) }
    }
    fn contact_gate(&mut self, obj: u64) -> i32 {
        unsafe { ee_units_pm_host_contact_gate(obj) }
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        unsafe { ee_units_pm_host_tile(x, y) }
    }
    fn tile_info(&mut self, tile: u64) -> EeUnitsPmTile {
        let mut t = EeUnitsPmTile::default();
        unsafe { ee_units_pm_host_tile_info(tile, &mut t) };
        t
    }
    fn tile_unit_first(&mut self, tile: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_TILE_FIRST, tile) }
    }
    fn tile_unit_next(&mut self, node: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_TILE_NEXT, node) }
    }
    fn tile_unit(&mut self, node: u64) -> u64 {
        unsafe { ee_units_pm_host_list(LIST_TILE_UNIT, node) }
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        unsafe { ee_units_pm_host_is_edge_tile(x, y) != 0 }
    }
    fn can_move_on_terrain(&mut self, obj: u64, terrain: i32) -> bool {
        unsafe { ee_units_pm_host_can_move_on_terrain(obj, terrain) != 0 }
    }
    fn passable_by(&mut self, unit: u64, obj: u64) -> bool {
        unsafe { ee_units_pm_host_passable_by(unit, obj) != 0 }
    }
    fn unbuilt_land_building(&mut self, unit: u64) -> bool {
        unsafe { ee_units_pm_host_unbuilt_land_building(unit) != 0 }
    }
    fn ground_point(&mut self, p: Ge3DPoint) -> Ge3DPoint {
        let mut out = [0.0f32; 3];
        unsafe { ee_units_pm_host_ground_point(p.x, p.y, p.z, out.as_mut_ptr()) };
        point(out)
    }
    fn terrain_pitch(&mut self, t: &GeTransformation) -> f32 {
        let m = flat(t);
        unsafe { ee_units_pm_host_terrain_tilt(m.as_ptr(), 0) }
    }
    fn terrain_roll(&mut self, t: &GeTransformation) -> f32 {
        let m = flat(t);
        unsafe { ee_units_pm_host_terrain_tilt(m.as_ptr(), 1) }
    }
    fn edge_planes(&mut self) -> [Ge3DPlane; 4] {
        let mut f = [0.0f32; 24];
        unsafe { ee_units_pm_host_edge_planes(f.as_mut_ptr()) };
        let plane = |i: usize| Ge3DPlane {
            point: point([f[i * 6], f[i * 6 + 1], f[i * 6 + 2]]),
            normal: ee_ge::types::GeVector { x: f[i * 6 + 3], y: f[i * 6 + 4], z: f[i * 6 + 5] },
        };
        [plane(0), plane(1), plane(2), plane(3)]
    }
    fn mesh_ground(&mut self, x: f32, y: f32) -> f32 {
        unsafe { ee_units_pm_host_mesh_ground(x, y) }
    }
    fn adjust_dive_depth(&mut self, obj: u64, io: &mut Ge3DPoint) {
        let mut p = [io.x, io.y, io.z];
        unsafe { ee_units_pm_host_adjust_dive_depth(obj, p.as_mut_ptr()) };
        *io = point(p);
    }
    fn splash(&mut self, obj: u64, p: Ge3DPoint) {
        unsafe { ee_units_pm_host_effect(obj, EFFECT_SPLASH, p.x, p.y, p.z) }
    }
    fn death_effect(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_effect(obj, EFFECT_DEATH, 0.0, 0.0, 0.0) }
    }
    fn impact_decal(&mut self, obj: u64, p: Ge3DPoint) {
        unsafe { ee_units_pm_host_effect(obj, EFFECT_DECAL, p.x, p.y, p.z) }
    }
    fn land_ripple(&mut self, p: Ge3DPoint) {
        unsafe { ee_units_pm_host_effect(0, EFFECT_RIPPLE, p.x, p.y, p.z) }
    }
    fn docked(&mut self, obj: u64) -> bool {
        unsafe { ee_units_pm_host_docked(obj) != 0 }
    }
    fn follow_carrier(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_follow_carrier(obj) }
    }
    fn home_base(&mut self, obj: u64) -> u64 {
        unsafe { ee_units_pm_host_goal_unit(obj, WHICH_GOAL_HOME) }
    }
    fn gate_links(&mut self, obj: u64, carrier: &mut u64, last: &mut u64) -> bool {
        unsafe { ee_units_pm_host_gate_links(obj, carrier, last) != 0 }
    }
    fn projectile_step0(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_kind_step(obj, KIND_PROJECTILE_STEP0) }
    }
    fn projectile_step2(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_kind_step(obj, KIND_PROJECTILE_STEP2) }
    }
    fn missile_step0(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_kind_step(obj, KIND_MISSILE_STEP0) }
    }
}
