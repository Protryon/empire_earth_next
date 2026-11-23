//! The physics models' host in the game: every [`PmHost`] call one C++ function (Physics
//! Thread/EEPhysicsModel.cpp, `ee_units_pm_host_*`). Absent from a test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use ee_base::math::types::{Ge3DPlane, Ge3DPoint, GeOrientation, GeTransformation};

use crate::app::capture_record;
use crate::units::pm::{EeUnitsPmClass, EeUnitsPmStats, EeUnitsPmTick, EeUnitsPmTile, EeUnitsPmUnit, PmHost};
use crate::units::pm_flight::{self, FlightHost};
use crate::units::pm_note;
use crate::world::layout::layout;
use crate::world::view::{UnitView, WorldRead};
use crate::world_field;

extern "C" {
    fn ee_units_pm_host_collision(flags: u8) -> u32;
    pub(crate) fn ee_units_pm_host_random() -> f32;
    fn ee_units_pm_host_set_transformation(obj: u64, m: *const f32);
    fn ee_units_pm_host_refresh_transformation(obj: u64);
    fn ee_units_pm_host_set_location(obj: u64, m: *const f32);
    fn ee_units_pm_host_set_location_point(obj: u64, x: f32, y: f32, z: f32);
    fn ee_units_pm_host_set_location_xy(obj: u64, x: f32, y: f32);
    fn ee_units_pm_host_update_tile(obj: u64, m: *mut f32, tile: *mut std::ffi::c_long);
    fn ee_units_pm_host_set_scale(obj: u64, s: f32);
    fn ee_units_pm_host_model_parts(obj: u64) -> i32;
    fn ee_units_pm_host_frame_time(obj: u64, animation: u32) -> u32;
    fn ee_units_pm_host_tile_of(obj: u64) -> u64;
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
    fn ee_units_pm_host_trample_damage(obj: u64, other: u64, amount: *mut std::ffi::c_long) -> u8;
    fn ee_units_pm_host_take_hit(obj: u64, attacker: u64, amount: std::ffi::c_long);
    fn ee_units_pm_host_tile(x: i32, y: i32) -> u64;
    fn ee_units_pm_host_is_edge_tile(x: i32, y: i32) -> u8;
    fn ee_units_pm_host_can_move_on_terrain(obj: u64, terrain: i32) -> u8;
    pub(crate) fn ee_units_pm_host_passable_by(unit: u64, obj: u64) -> u8;
    fn ee_units_pm_host_unbuilt_land_building(unit: u64) -> u8;
    fn ee_units_pm_host_ground_point(x: f32, y: f32, z: f32, out: *mut f32);
    fn ee_units_pm_host_terrain_tilt(m: *const f32, roll: u8) -> f32;
    fn ee_units_pm_host_carrier(which: i32, carrier: u64, index: i32, m: *mut f32, out: *mut f32) -> u64;
    fn ee_units_pm_host_route_dock_waypoint(obj: u64);
    fn ee_units_pm_host_goal(which: i32, goal: u64, out: *mut f32) -> u64;
    fn ee_units_pm_host_path_start(obj: u64, out: *mut f32, length: *mut f32);
    fn ee_units_pm_host_is_gate_class(obj: u64) -> u8;
    fn ee_units_pm_host_edge_plane(which: i32) -> *const f32;
    fn ee_units_pm_host_mesh(which: i32, x: f32, y: f32) -> f32;
    fn ee_units_pm_host_effect(which: i32, obj: u64, point: *mut f32);
    fn ee_units_pm_host_gate_link(which: i32, obj: u64) -> u64;
    fn ee_units_pm_host_launch_speed(obj: u64, launcher: u8) -> f32;
    fn ee_units_pm_host_projectile_start(obj: u64) -> *const f32;
    fn ee_units_pm_host_set_velocity(obj: u64, v: f32);
    fn ee_units_pm_host_drop_target(obj: u64, out: *mut f32);
    fn ee_units_pm_host_blocking_id(obj: u64) -> u32;
}

/// `ee_units_pm_host_goal`'s: the attack goal's cast and target, the heal goal's cast and target, the gather goal's
/// resource, the build goal's target, the cast-calamity goal's cast and target location, the goal's location, whether
/// it has a location (a location or heal goal) and its yaw
const ASK_ATTACK_CAST: i32 = 0;
const ASK_ATTACK_TARGET: i32 = 1;
const ASK_HEAL_CAST: i32 = 2;
const ASK_HEAL_TARGET: i32 = 3;
const ASK_GATHER_RESOURCE: i32 = 4;
const ASK_BUILD_TARGET: i32 = 5;
const ASK_CAST_CALAMITY: i32 = 6;
const ASK_CALAMITY_LOCATION: i32 = 7;
const ASK_LOCATION: i32 = 8;
const ASK_HAS_LOCATION: i32 = 9;
const ASK_YAW: i32 = 10;
/// The goals' kinds (`kGoalGather`, `kGoalBuildRepair`)
const GOAL_GATHER: u8 = 6;
const GOAL_BUILD_REPAIR: u8 = 4;
/// `ee_units_pm_host_effect`'s: the class's splash at the point, its death effect, the impact decal, the land ripple
const EFFECT_SPLASH: i32 = 0;
const EFFECT_DEATH: i32 = 1;
const EFFECT_DECAL: i32 = 2;
const EFFECT_RIPPLE: i32 = 3;
/// `ee_units_pm_host_mesh`'s: the mesh's tile at the point's floors is valid; its height at the point
const MESH_VALID: i32 = 0;
const MESH_Z: i32 = 1;
/// `ee_units_pm_host_gate_link`'s: a gate's last linked carrier, its last linked part
const GATE_LAST_CARRIER: i32 = 0;
const GATE_LAST_LINKED: i32 = 1;
/// a gate's `mCommand->mType` that leaves it open
const GATE_OPEN_COMMAND: u32 = 0x3a99;
/// `kUnitCategoryGate`
const CATEGORY_GATE: u64 = 0x19;

/// The pointer member at `base + offset` (0 for a null base).
fn pointer_at(base: u64, offset: u64) -> u64 {
    if base == 0 {
        return 0;
    }
    // SAFETY: a live object's pointer member (the physics' caller holds the world)
    unsafe { std::ptr::read_unaligned((base as usize + offset as usize) as *const usize) as u64 }
}

/// `EEUpdateUnitTile(&matrix, unit, &x, &y)` (the tile ignored)
fn update_tile(obj: u64, m: &[f32; 16]) {
    let mut m = *m;
    let mut tile: [std::ffi::c_long; 2] = [0; 2];
    unsafe { ee_units_pm_host_update_tile(obj, m.as_mut_ptr(), tile.as_mut_ptr()) }
}

/// An effect at the point (`EFFECT_*`).
fn effect(which: i32, obj: u64, p: Ge3DPoint) {
    let mut p = [p.x, p.y, p.z];
    unsafe { ee_units_pm_host_effect(which, obj, p.as_mut_ptr()) }
}

/// `ee_units_pm_host_carrier`'s: the carrier's tag point list (0: none), the list's count, its location scaled by its
/// stats, a tag point at its frame
const CARRIER_TAGS: i32 = 0;
const CARRIER_TAG_COUNT: i32 = 1;
const CARRIER_SCALE: i32 = 2;
const CARRIER_POINT: i32 = 3;

fn carrier_op(which: i32, of: u64, index: i32, m: Option<&mut [f32; 16]>, out: Option<&mut [f32; 3]>) -> u64 {
    let m = m.map_or(std::ptr::null_mut(), |m| m.as_mut_ptr());
    let out = out.map_or(std::ptr::null_mut(), |o| o.as_mut_ptr());
    unsafe { ee_units_pm_host_carrier(which, of, index, m, out) }
}

fn goal(which: i32, goal: u64) -> u64 {
    unsafe { ee_units_pm_host_goal(which, goal, std::ptr::null_mut()) }
}

fn goal_point(which: i32, goal: u64) -> Ge3DPoint {
    let mut p = [0.0f32; 3];
    unsafe { ee_units_pm_host_goal(which, goal, p.as_mut_ptr()) };
    point(p)
}

/// The unit's `mGoalState` (0: none).
fn goal_state(obj: u64) -> u64 {
    with_unit(obj, |u| u.goal_handle())
}

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
        // SAFETY: the physics' call runs inside the C++ caller's hold of the world
        let t = unsafe { WorldRead::begin() };
        let mut out = EeUnitsPmTick::default();
        if let Some(p) = crate::world::view::WorldView::global(&t).and_then(|w| w.physics()) {
            out.tick_seconds = world_field!(p, mTickSeconds);
            out.ticks_per_second = world_field!(p, mTicksPerGameSecond);
            out.tick_time = world_field!(p, mTickTime);
            out.game_time = world_field!(p, mGameTime) as i32;
        }
        out.collision_period = unsafe { ee_units_pm_host_collision(0) };
        out.detection_flags = unsafe { ee_units_pm_host_collision(1) };
        out.trample_min = pm_flight::trample_min();
        out.height_epsilon = HEIGHT_EPSILON;
        out
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
        update_tile(obj, &flat(t))
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
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_MAX_VELOCITY) }
    }
    fn acceleration(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_ACCELERATION) }
    }
    fn inverse_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_INVERSE_ACCELERATION) }
    }
    fn angular_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_ANGULAR_ACCELERATION) }
    }
    fn max_angular_velocity(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_MAX_ANGULAR_VELOCITY) }
    }
    fn inverse_angular_acceleration(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_INVERSE_ANGULAR_ACCELERATION) }
    }
    // the waypoints: Rust's list (waypoints.rs); `obj` is the model's live complex unit
    fn waypoint_count(&mut self, obj: u64) -> i32 {
        unsafe { crate::units::waypoints::list(obj) }.len() as i32
    }
    fn next_waypoint(&mut self, obj: u64, out: &mut Ge3DPoint) -> i32 {
        match unsafe { crate::units::waypoints::list(obj) }.front() {
            Some(w) => {
                *out = point(w.location);
                0
            }
            None => crate::units::waypoints::EE_UNITS_WAYPOINTS_NONE,
        }
    }
    fn first_waypoint_z(&mut self, obj: u64) -> f32 {
        unsafe { crate::units::waypoints::list(obj) }.front().map_or(0.0, |w| w.location[2])
    }
    fn path_length(&mut self, obj: u64, io: &mut f32) {
        if let Some((_, l)) = unsafe { crate::units::waypoints::list(obj) }.path_start() {
            *io = l;
        }
    }
    fn path_start(&mut self, obj: u64) -> Ge3DPoint {
        let mut p = [0.0f32; 3];
        let mut length = 0.0f32;
        unsafe { ee_units_pm_host_path_start(obj, p.as_mut_ptr(), &mut length) };
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
        let cast = goal(ASK_ATTACK_CAST, goal_state(obj));
        if cast == 0 {
            return 0;
        }
        goal(ASK_ATTACK_TARGET, cast)
    }
    fn heal_target(&mut self, obj: u64) -> u64 {
        let cast = goal(ASK_HEAL_CAST, goal_state(obj));
        if cast == 0 {
            return 0;
        }
        goal(ASK_HEAL_TARGET, cast)
    }
    fn is_cast_goal(&mut self, obj: u64) -> bool {
        goal(ASK_CAST_CALAMITY, goal_state(obj)) != 0
    }
    fn goal_location(&mut self, obj: u64) -> Ge3DPoint {
        let g = goal_state(obj);
        if g == 0 {
            return Ge3DPoint::default();
        }
        goal_point(ASK_LOCATION, g)
    }
    fn calamity_target_location(&mut self, obj: u64) -> Ge3DPoint {
        let cast = goal(ASK_CAST_CALAMITY, goal_state(obj));
        if cast == 0 {
            return Ge3DPoint::default();
        }
        goal_point(ASK_CALAMITY_LOCATION, cast)
    }
    fn goal_facing(&mut self, obj: u64, yaw: &mut f32) -> bool {
        // `GetGoal()`: a location goal or a heal goal (yaw 0)
        let g = goal_state(obj);
        if goal(ASK_HAS_LOCATION, g) == 0 {
            return false;
        }
        *yaw = goal_point(ASK_YAW, g).x;
        true
    }
    fn gather_resource(&mut self, obj: u64) -> u64 {
        let g = goal_state(obj);
        if g == 0 || self.goal_kind(obj) != GOAL_GATHER {
            return 0;
        }
        goal(ASK_GATHER_RESOURCE, g)
    }
    fn build_target(&mut self, obj: u64) -> u64 {
        let g = goal_state(obj);
        if g == 0 || self.goal_kind(obj) != GOAL_BUILD_REPAIR {
            return 0;
        }
        goal(ASK_BUILD_TARGET, g)
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
        // the shared damage function (ee_combat damage.rs; combat.yaml trample_damage/_threshold), then the hit
        let mut amount: std::ffi::c_long = 0;
        if unsafe { ee_units_pm_host_trample_damage(obj, other, &mut amount) } != 0 {
            unsafe { ee_units_pm_host_take_hit(other, obj, amount) }
        }
    }
    fn contact_first(&mut self, obj: u64) -> u64 {
        pointer_at(obj, layout().complex_contacts_head)
    }
    fn contact_next(&mut self, node: u64) -> u64 {
        pointer_at(node, layout().contact_node_next)
    }
    fn contact_unit(&mut self, node: u64) -> u64 {
        pointer_at(node, layout().contact_node_data)
    }
    fn contact_count(&mut self, obj: u64) -> i32 {
        with_unit(obj, |u| u.contacts().0 as i32)
    }
    fn contact_gate(&mut self, obj: u64) -> i32 {
        // 0 none, 1 a gate the unit collides with, 2 a closed gate
        let (category, command) = with_unit(obj, |u| (u.class().map_or(0, |c| c.category()), u.command_type()));
        if category != CATEGORY_GATE {
            return 0;
        }
        if unsafe { ee_units_pm_host_is_gate_class(obj) } != 0 && command != GATE_OPEN_COMMAND {
            return 2;
        }
        1
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        unsafe { ee_units_pm_host_tile(x, y) }
    }
    fn tile_info(&mut self, tile: u64) -> EeUnitsPmTile {
        // SAFETY: the physics' call runs inside the C++ caller's hold of the world; `tile` is a live tile or 0
        let t = unsafe { WorldRead::begin() };
        match unsafe { crate::world::view::TileView::of(&t, tile) } {
            Some(v) => {
                EeUnitsPmTile { terrain_type: v.terrain_type() as i32, ai_value: v.ai_value(), height: v.height() }
            }
            None => EeUnitsPmTile::default(),
        }
    }
    fn tile_unit_first(&mut self, tile: u64) -> u64 {
        pointer_at(tile, layout().tile_units)
    }
    fn tile_unit_next(&mut self, node: u64) -> u64 {
        pointer_at(node, layout().unit_node_next)
    }
    fn tile_unit(&mut self, node: u64) -> u64 {
        pointer_at(node, layout().unit_node_unit)
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
        // the terrain mesh's left, right, front and back planes (`GE3DPlane`: its point, its normal)
        let plane = |i: i32| {
            // SAFETY: the mesh's live plane member: six floats
            let f = unsafe { std::ptr::read_unaligned(ee_units_pm_host_edge_plane(i) as *const [f32; 6]) };
            Ge3DPlane {
                point: point([f[0], f[1], f[2]]),
                normal: ee_base::math::types::GeVector { x: f[3], y: f[4], z: f[5] },
            }
        };
        [plane(0), plane(1), plane(2), plane(3)]
    }
    fn mesh_ground(&mut self, x: f32, y: f32) -> f32 {
        // the descending plane's floor: the mesh's height at the point, 0 off its tiles
        if unsafe { ee_units_pm_host_mesh(MESH_VALID, x, y) } == 0.0 {
            return 0.0;
        }
        unsafe { ee_units_pm_host_mesh(MESH_Z, x, y) }
    }
    fn adjust_dive_depth(&mut self, obj: u64, io: &mut Ge3DPoint) {
        let p = [io.x, io.y, io.z];
        // SAFETY: the live unit (uu_world.rs's adjust_dive_depth)
        io.z = unsafe { crate::units::uu_ffi::ee_units_uu_seabed(1, obj, &p) };
    }
    fn splash(&mut self, obj: u64, p: Ge3DPoint) {
        effect(EFFECT_SPLASH, obj, p)
    }
    fn death_effect(&mut self, obj: u64) {
        effect(EFFECT_DEATH, obj, Ge3DPoint::default())
    }
    fn impact_decal(&mut self, obj: u64, p: Ge3DPoint) {
        effect(EFFECT_DECAL, obj, p)
    }
    fn land_ripple(&mut self, p: Ge3DPoint) {
        effect(EFFECT_RIPPLE, 0, p)
    }
    fn carrier_tag_count(&mut self, carrier: u64) -> i32 {
        // the carrier's tag points of its animation (pm_air.rs's deck walk): their count, -1 without a list
        let tags = carrier_op(CARRIER_TAGS, carrier, 0, None, None);
        if tags == 0 {
            return -1;
        }
        carrier_op(CARRIER_TAG_COUNT, tags, 0, None, None) as i32
    }
    fn carrier_tag(&mut self, carrier: u64, index: i32) -> Ge3DPoint {
        // point `index` at the carrier's frame in its location scaled by its stats (IsDocked's and FollowCarrier's
        // EETagPointList::Scale and GetPoint)
        let mut p = [0.0f32; 3];
        let tags = carrier_op(CARRIER_TAGS, carrier, 0, None, None);
        if tags == 0 {
            return point(p);
        }
        let mut m = flat(&PmHost::location(self, carrier));
        carrier_op(CARRIER_SCALE, carrier, 0, Some(&mut m), None);
        let mut q = [0.0f32; 3];
        carrier_op(CARRIER_POINT, carrier, index, Some(&mut m), Some(&mut q));
        p = q;
        point(p)
    }
    fn route_dock_waypoint(&mut self, obj: u64) {
        pm_note::with_notes(|n| n.deck_walk());
        unsafe { ee_units_pm_host_route_dock_waypoint(obj) }
    }
    fn home_base(&mut self, obj: u64) -> u64 {
        with_unit(obj, |u| u.home_base_handle())
    }
    fn gate_links(&mut self, obj: u64, carrier: &mut u64, last: &mut u64) -> bool {
        // a gate's first part (no previous part, a next one): its last linked carrier and last linked part
        let (previous, next) = with_unit(obj, |u| (u.previous_part_handle(), u.next_part_handle()));
        if !(previous == 0 && next != 0) {
            return false;
        }
        *carrier = unsafe { ee_units_pm_host_gate_link(GATE_LAST_CARRIER, obj) };
        *last = unsafe { ee_units_pm_host_gate_link(GATE_LAST_LINKED, obj) };
        true
    }
    fn projectile_step0(&mut self, obj: u64) {
        pm_flight::projectile_step0(self, obj)
    }
    fn projectile_step2(&mut self, obj: u64) {
        pm_flight::projectile_step2(self, obj)
    }
    fn missile_step0(&mut self, obj: u64) {
        pm_flight::missile_step0(self, obj)
    }
}

/// The unit's complex record (the live unit the physics hands in).
fn cx(obj: u64) -> *mut crate::units::unit_fields::EeUnitsComplexUnitFields {
    (obj as usize + layout().complex_unit_fields as usize) as *mut _
}

/// The projectile's and the missile's reads and writes (pm_flight.rs).
impl FlightHost for Game {
    fn location(&mut self, obj: u64) -> [f32; 16] {
        flat(&PmHost::location(self, obj))
    }
    fn set_contact_location(&mut self, obj: u64, p: [f32; 3]) {
        // SAFETY: the live unit's GE3DPoint member (`mInitialContactLocation = p`)
        unsafe {
            std::ptr::write_unaligned((obj as usize + layout().complex_contact_location as usize) as *mut [f32; 3], p)
        }
    }
    fn flight_time(&mut self, obj: u64) -> i32 {
        // SAFETY: the live unit's record
        unsafe { (*cx(obj)).mFlightTime as i32 }
    }
    fn class_gravity(&mut self, obj: u64) -> f32 {
        with_unit(obj, |u| u.class().map_or(0.0, |c| world_field!(c.fields(), mAcceleration)))
    }
    fn orientation(&mut self, obj: u64) -> [f32; 3] {
        read_orientation(obj, layout().complex_orientation)
    }
    fn is_meteor(&mut self, obj: u64) -> bool {
        with_unit(obj, |u| u.class().is_some_and(|c| c.unit_type() == UNIT_TYPE_METEOR))
    }
    fn max_velocity(&mut self, obj: u64) -> f32 {
        unsafe { pm_flight::unit_rate(obj, pm_flight::EE_UNITS_PM_RATE_MAX_VELOCITY) }
    }
    fn launch_speed(&mut self, obj: u64, launcher: bool) -> f32 {
        unsafe { ee_units_pm_host_launch_speed(obj, u8::from(launcher)) }
    }
    fn projectile_start(&mut self, obj: u64) -> [f32; 3] {
        // SAFETY: the live unit's GE3DPoint member's three floats
        unsafe { std::ptr::read_unaligned(ee_units_pm_host_projectile_start(obj) as *const [f32; 3]) }
    }
    fn tick_seconds(&mut self) -> f32 {
        PmHost::tick(self).tick_seconds
    }
    fn ground_z(&mut self, p: [f32; 3]) -> f32 {
        self.ground_point(point(p)).z
    }
    fn velocity(&mut self, obj: u64) -> f32 {
        // SAFETY: the live unit's record
        unsafe { (*cx(obj)).mVelocity }
    }
    fn stats(&mut self, obj: u64) -> [f32; 3] {
        let s = PmHost::stats(self, obj);
        [s.max_velocity, s.acceleration, s.max_angular_velocity]
    }
    fn drop_target(&mut self, obj: u64) -> [f32; 3] {
        let mut p = [0.0f32; 3];
        unsafe { ee_units_pm_host_drop_target(obj, p.as_mut_ptr()) };
        p
    }
    fn set_velocity(&mut self, obj: u64, v: f32) {
        unsafe { ee_units_pm_host_set_velocity(obj, v) }
    }
    fn update_tile(&mut self, obj: u64, m: &[f32; 16]) {
        update_tile(obj, m)
    }
    fn capture(&mut self, tag: u32, input: &[u8], output: &[u8]) {
        if !capture_record::active() {
            return;
        }
        let mut r = capture_record::Record::new(tag);
        r.bytes(input);
        r.bytes(output);
        r.commit();
    }
}

/// `kUnitTypeMeteor` (ee/EEUnitClassIDs.h)
const UNIT_TYPE_METEOR: u64 = 0x325;

/// `kHeightEpsilon` (EECPEngineHelpers.cpp)
const HEIGHT_EPSILON: f32 = 0.01;

/// TEST ONLY: the PMST note of a step of model `kind` (pm_note.rs): the unit's state after it, read in place.
pub fn note_step(kind: i32, obj: u64, action: u32) {
    let l = layout();
    let unit = Game.view(obj);
    let m = flat(&PmHost::location(&mut Game, obj));
    let angular = read_orientation(obj, l.complex_angular_velocity);
    let contact = read_orientation(obj, l.complex_contact_location);
    let orient = read_orientation(obj, l.complex_orientation);
    let blocker = unsafe { ee_units_pm_host_blocking_id(obj) };
    // SAFETY: the live unit's records (the step's caller holds the world)
    let (rts, fields, c) = unsafe { (&*unit.rts, &*unit.unit, &*unit.cx) };
    let floats = [
        c.mVelocity,
        angular[0],
        angular[1],
        angular[2],
        c.mPushVelocityX,
        c.mPushVelocityY,
        c.mRoll,
        c.mF220,
        c.mCollisionResolutionCount,
        contact[0],
        contact[1],
        contact[2],
        orient[0],
        orient[1],
        orient[2],
        c.mHeight,
    ];
    let mut words: Vec<u32> = m.iter().map(|f| f.to_bits()).collect();
    words.extend(floats.iter().map(|f| f.to_bits()));
    words.extend([
        u32::from(c.mCollisionState),
        blocker,
        c.mCurrentPhysicsAction as u32,
        rts.mTileX as u32,
        rts.mTileY as u32,
        u32::from(c.mHasArrived),
        c.mAnimation as u32,
        u32::from(c.mIsAnimationPaused),
        c.mAnimationTime as u32,
        fields.mHitPoints as u32,
        c.mPushImmuneUntil as u32,
    ]);
    let ticks = {
        // SAFETY: on the physics thread inside the C++ caller's hold of the world
        let t = unsafe { WorldRead::begin() };
        crate::world::view::WorldView::global(&t)
            .and_then(|w| w.physics())
            .map_or(0, |p| world_field!(p, mNumTicksDone) as u32)
    };
    pm_note::note(ticks, kind as u32, rts.mUnitID as u32, action, &words);
}
