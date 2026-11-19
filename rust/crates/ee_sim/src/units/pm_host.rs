//! The physics models' host in the game: every [`PmHost`] call one C++ function (Physics
//! Thread/EEPhysicsModel.cpp, `ee_units_pm_host_*`). Absent from a test binary.

#![cfg(not(test))]

use ee_base::math::types::{Ge3DPlane, Ge3DPoint, GeOrientation, GeTransformation};

use crate::world::layout::layout;
use crate::world::view::{UnitView, WorldRead};
use crate::world_field;
use crate::units::pm::{EeUnitsPmClass, EeUnitsPmStats, EeUnitsPmTick, EeUnitsPmTile, EeUnitsPmUnit, PmHost};

extern "C" {
    fn ee_units_pm_host_tick(out: *mut EeUnitsPmTick);
    fn ee_units_pm_host_random() -> f32;
    fn ee_units_pm_host_set_transformation(obj: u64, m: *const f32);
    fn ee_units_pm_host_refresh_transformation(obj: u64);
    fn ee_units_pm_host_set_location(obj: u64, m: *const f32);
    fn ee_units_pm_host_set_location_point(obj: u64, x: f32, y: f32, z: f32);
    fn ee_units_pm_host_set_location_xy(obj: u64, x: f32, y: f32);
    fn ee_units_pm_host_update_tile(obj: u64, m: *const f32);
    fn ee_units_pm_host_set_scale(obj: u64, s: f32);
    fn ee_units_pm_host_rate(obj: u64, which: i32) -> f32;
    fn ee_units_pm_host_waypoint_count(obj: u64) -> i32;
    fn ee_units_pm_host_next_waypoint(obj: u64, out: *mut f32) -> i32;
    fn ee_units_pm_host_first_waypoint_z(obj: u64) -> f32;
    fn ee_units_pm_host_path_length(obj: u64, io: *mut f32);
    fn ee_units_pm_host_model_parts(obj: u64) -> i32;
    fn ee_units_pm_host_frame_time(obj: u64, animation: u32) -> u32;
    fn ee_units_pm_host_tile_of(obj: u64) -> u64;
    fn ee_units_pm_host_goal_unit(obj: u64, which: i32) -> u64;
    fn ee_units_pm_host_goal_point(obj: u64, which: i32, out: *mut f32);
    fn ee_units_pm_host_goal_facing(obj: u64, yaw: *mut f32) -> u8;
    fn ee_units_pm_host_flies_over(unit: u64) -> u8;
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
    fn ee_units_pm_host_carrier_tag(carrier: u64, index: i32, out: *mut f32) -> i32;
    fn ee_units_pm_host_route_dock_waypoint(obj: u64);
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

/// The class's effects (`GetEffects()` indexes): the death, splash and shell hole effects
const CLASS_EFFECT_DEATH: usize = 1;
const CLASS_EFFECT_SPLASH: usize = 8;
const CLASS_EFFECT_SHELL_HOLE: usize = 26;

/// `f` over the view of the live unit `obj` (the physics model's caller holds the world).
fn with_unit<R: Default>(obj: u64, f: impl FnOnce(&UnitView<'_>) -> R) -> R {
    // SAFETY: the physics' call runs on the simulation thread inside the C++ caller's hold of the world; `obj` is
    // the live unit it hands in
    let t = unsafe { WorldRead::begin() };
    match unsafe { UnitView::of(&t, obj) } {
        Some(u) => f(&u),
        None => R::default(),
    }
}

/// The unit's `GEOrientation` member at `offset` (`mAngularVelocity`, `mOrientation`): yaw, pitch, roll.
fn read_orientation(obj: u64, offset: u64) -> [f32; 3] {
    // SAFETY: a live complex unit's member (the physics' caller holds the world)
    unsafe { std::ptr::read_unaligned((obj as usize + offset as usize) as *const [f32; 3]) }
}

/// The member's store (`= GEOrientation(yaw, pitch, roll)`).
fn write_orientation(obj: u64, offset: u64, o: GeOrientation) {
    // SAFETY: as above; no reference to the member is held
    unsafe { std::ptr::write_unaligned((obj as usize + offset as usize) as *mut [f32; 3], [o.yaw, o.pitch, o.roll]) }
}

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
        crate::units::notes::noting()
    }
    fn note(&mut self, kind: u32, id: u32, words: &[u32]) {
        crate::units::notes::state(kind, id, words)
    }
    fn view(&mut self, obj: u64) -> EeUnitsPmUnit {
        // the records' places in the object (the C++ `PMView`'s base casts)
        let l = layout();
        let at = |offset: u64| (obj as usize + offset as usize) as *mut u8;
        EeUnitsPmUnit {
            obj,
            rts: at(l.rts_unit_fields).cast(),
            unit: at(l.unit_fields).cast(),
            cx: at(l.complex_unit_fields).cast(),
        }
    }
    fn class(&mut self, obj: u64) -> EeUnitsPmClass {
        with_unit(obj, |u| {
            let Some(class) = u.class() else { return EeUnitsPmClass::default() };
            let f = class.fields();
            let b = |v: bool| u8::from(v);
            EeUnitsPmClass {
                category: class.category() as u32,
                unit_type: class.unit_type() as u32,
                air_traffic_type: world_field!(f, mAirTrafficType) as i32,
                tile_size: class.tile_size() as i32,
                pitches_with_terrain: b(world_field!(f, mPitchesWithTerrain)),
                rolls_with_terrain: b(world_field!(f, mRollsWithTerrain)),
                fires_broadside: b(world_field!(f, mFiresBroadside)),
                is_melee: b(world_field!(f, mIsMelee)),
                has_splash_effect: b(class.effect_handle(CLASS_EFFECT_SPLASH) != 0),
                has_death_effect: b(class.effect_handle(CLASS_EFFECT_DEATH) != 0),
                has_shell_hole_effect: b(class.effect_handle(CLASS_EFFECT_SHELL_HOLE) != 0),
                cruise_altitude: world_field!(f, mCruiseAltitude),
                slope_penalty: world_field!(f, mSlopeSpeedPenalty),
                bob_pitch: world_field!(f, mBobPitch),
                bob_height: world_field!(f, mBobHeight),
                scale: world_field!(f, mScale),
                max_velocity: world_field!(f, mMaxVelocity),
                max_roll: world_field!(f, mMaxRoll),
                inner_collision_sphere: world_field!(f, mInnerCollisionSphere),
                facing_offset: world_field!(f, mFacingOffset),
                angular_acceleration: world_field!(f, mAngularAcceleration),
                max_angular_velocity: world_field!(f, mMaxAngularVelocity),
            }
        })
    }
    fn stats(&mut self, obj: u64) -> EeUnitsPmStats {
        with_unit(obj, |u| {
            let Some(stats) = u.stats() else { return EeUnitsPmStats::default() };
            let f = stats.fields();
            EeUnitsPmStats {
                max_velocity: world_field!(f, mMaxVelocity),
                acceleration: world_field!(f, mAcceleration),
                angular_acceleration: world_field!(f, mAngularAcceleration),
                max_angular_velocity: world_field!(f, mMaxAngularVelocity),
                scale: world_field!(f, mScale),
            }
        })
    }
    fn location(&mut self, obj: u64) -> GeTransformation {
        let rows = with_unit(obj, |u| u.transform());
        let mut m = [0.0f32; 16];
        for (r, row) in rows.iter().enumerate() {
            m[r * 4..r * 4 + 4].copy_from_slice(row);
        }
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
        orientation(read_orientation(obj, layout().complex_angular_velocity))
    }
    fn set_angular_velocity(&mut self, obj: u64, o: GeOrientation) {
        write_orientation(obj, layout().complex_angular_velocity, o)
    }
    fn orientation(&mut self, obj: u64) -> GeOrientation {
        orientation(read_orientation(obj, layout().complex_orientation))
    }
    fn set_orientation(&mut self, obj: u64, o: GeOrientation) {
        write_orientation(obj, layout().complex_orientation, o)
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
        with_unit(obj, |u| u.command_type())
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
        with_unit(obj, |u| u.goal().map_or(0, |g| g.0))
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
        with_unit(unit, |u| u.resource_type() as u32)
    }
    fn unit_complex(&mut self, unit: u64) -> u64 {
        with_unit(unit, |u| u.complex_unit_handle())
    }
    fn unit_tile(&mut self, unit: u64) -> (i32, i32) {
        with_unit(unit, |u| {
            let (x, y) = u.tile_xy();
            (x as i32, y as i32)
        })
    }
    fn unit_category(&mut self, unit: u64) -> u32 {
        with_unit(unit, |u| u.class().map_or(0, |c| c.category() as u32))
    }
    fn flies_over(&mut self, unit: u64) -> bool {
        unsafe { ee_units_pm_host_flies_over(unit) != 0 }
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
            normal: ee_base::math::types::GeVector { x: f[i * 6 + 3], y: f[i * 6 + 4], z: f[i * 6 + 5] },
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
    fn carrier_tag_count(&mut self, carrier: u64) -> i32 {
        unsafe { ee_units_pm_host_carrier_tag(carrier, -1, std::ptr::null_mut()) }
    }
    fn carrier_tag(&mut self, carrier: u64, index: i32) -> Ge3DPoint {
        let mut p = [0.0f32; 3];
        unsafe { ee_units_pm_host_carrier_tag(carrier, index, p.as_mut_ptr()) };
        point(p)
    }
    fn route_dock_waypoint(&mut self, obj: u64) {
        unsafe { ee_units_pm_host_route_dock_waypoint(obj) }
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
