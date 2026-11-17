//! The physics models whole in Rust (the physics models' lane, round 1; were `EEPhysicsModel` 0x5543b7..0x556df8 and
//! the twelve `EEPM*` subclasses, Physics Thread/EEPM*.cpp 0x556df8..0x56066d).
//!
//! The models are stateless singletons (`gPhysicsModels[12]`, one per kind; `EEUnitClass::mPhysicsModel` points at
//! one): every value they read or write is the unit's. So the move is the classes' virtual dispatch and their bodies:
//! `Step` (by the unit's physics action, then `AfterStep`), every `Step0`..`Step5`, `Move`, `Collide`, `Push`,
//! `PushMoving`, `ClipStep`, the bobs, the turns, the speed and pitch modes and the displayed pitch and roll, each
//! kind's override chosen as its vtable chose it ([`Kind`]). The C++ `EEPhysicsModel` is one class with its kind and
//! three forwarders (`Step`, `Step0` for EE-SIM's missile sweep, `ComputeRoll` for the classes' death rolls).
//!
//! The unit's plain members are its records (`EeUnitsRtsUnitFields`, `EeUnitsUnitFields`,
//! `EeUnitsComplexUnitFields`, the C++ object's bases): read and written in place through [`EeUnitsPmUnit`]'s
//! pointers, one field at a time, never as a reference held across a host call. Everything else (the location's
//! transformation, the angular velocity and orientation, the path, the goals, the map, the tiles' units, the effects,
//! the class's virtuals) is a [`PmHost`] call, made where the C++ made it. The transformation arithmetic is ee_ge's
//! (the C++'s `Apply`, `SetOrientationYPR`, `Revolve`, the `UM*` tables forward there).
//!
//! Determinism (RUST_PLAN 2.1): the C++'s `f32` statements in their order and association; the random draws
//! (`EEPMBuilding::Step2`'s two) in order; the USST notes of the arithmetic leaves (physics.rs, motion.rs) with the
//! same words, so the capture's digests stay equal.
//!
//! The projectile's and the missile's steps stay host calls: their arithmetic is ee_combat's (flight.rs) already and
//! their inputs and captures (CBPJ, CBPS, CBMA, CBMH) are the combat lane's C++. The carrier planes' deck walk
//! (`IsDocked`, `FollowCarrier`, `OrientOnCarrier`: the carrier model's tag points) stays C++ too.

// the C++'s statements as written: its negated tests (NaN goes their way), its compound assignments, its constants
#![allow(
    clippy::neg_cmp_op_on_partial_ord,
    clippy::assign_op_pattern,
    clippy::manual_clamp,
    clippy::approx_constant,
    clippy::too_many_arguments,
    clippy::collapsible_if,
    clippy::collapsible_else_if,
    clippy::needless_late_init,
    clippy::nonminimal_bool,
    // `c_long` is `i32` on Windows and `i64` on Linux: a cast is needed on one of them
    clippy::unnecessary_cast
)]

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeOrientation, GeTransformation, GeVector};
use ee_base::math::um::{floor, um_arccos, um_arctan2};

use crate::units::physics::{self, EeUnitsPitchIn, EeUnitsSpeedIn, EeUnitsTurnIn};
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

/// The kinds, as `gPhysicsModels[]` holds them (`EEInitPhysicsModels`).
pub const EE_UNITS_PM_LAND: i32 = 0;
pub const EE_UNITS_PM_AIRPLANE: i32 = 1;
pub const EE_UNITS_PM_HELICOPTER: i32 = 2;
pub const EE_UNITS_PM_HOVERCRAFT: i32 = 3;
pub const EE_UNITS_PM_NAVAL: i32 = 4;
pub const EE_UNITS_PM_SUBMERGED_NAVAL: i32 = 5;
pub const EE_UNITS_PM_PROJECTILE: i32 = 6;
pub const EE_UNITS_PM_MISSILE: i32 = 7;
pub const EE_UNITS_PM_BUILDING: i32 = 8;
pub const EE_UNITS_PM_SEA_ANIMAL: i32 = 9;
pub const EE_UNITS_PM_AIR_ANIMAL: i32 = 10;
pub const EE_UNITS_PM_SPACESHIP: i32 = 11;

/// A physics model's kind (its `gPhysicsModels[]` index).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    Land,
    Airplane,
    Helicopter,
    Hovercraft,
    Naval,
    SubmergedNaval,
    Projectile,
    Missile,
    Building,
    SeaAnimal,
    AirAnimal,
    Spaceship,
}

impl Kind {
    pub fn from_index(index: i32) -> Option<Kind> {
        Some(match index {
            EE_UNITS_PM_LAND => Kind::Land,
            EE_UNITS_PM_AIRPLANE => Kind::Airplane,
            EE_UNITS_PM_HELICOPTER => Kind::Helicopter,
            EE_UNITS_PM_HOVERCRAFT => Kind::Hovercraft,
            EE_UNITS_PM_NAVAL => Kind::Naval,
            EE_UNITS_PM_SUBMERGED_NAVAL => Kind::SubmergedNaval,
            EE_UNITS_PM_PROJECTILE => Kind::Projectile,
            EE_UNITS_PM_MISSILE => Kind::Missile,
            EE_UNITS_PM_BUILDING => Kind::Building,
            EE_UNITS_PM_SEA_ANIMAL => Kind::SeaAnimal,
            EE_UNITS_PM_AIR_ANIMAL => Kind::AirAnimal,
            EE_UNITS_PM_SPACESHIP => Kind::Spaceship,
            _ => return None,
        })
    }

    /// The submarine's bodies (`EEPMSeaAnimal` derives from `EEPMSubmergedNaval`).
    pub(crate) fn is_submerged(self) -> bool {
        matches!(self, Kind::SubmergedNaval | Kind::SeaAnimal)
    }
}

/// A complex unit: the object and its three records (the C++ object's bases; `static_cast`s of one pointer).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsPmUnit {
    /// the `EEComplexUnit*` (its `EEUnit` and `RTSUnit` bases at the same address)
    pub obj: u64,
    pub rts: *mut EeUnitsRtsUnitFields,
    pub unit: *mut EeUnitsUnitFields,
    pub cx: *mut EeUnitsComplexUnitFields,
}

/// The tick's values (`gPhysics`'s, `gWorld`'s and the unit's globals), read once per call from the C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPmTick {
    /// `mTickSeconds`, `mTicksPerGameSecond`, `GetTickTime()` (the tick in game ms, a float)
    pub tick_seconds: f32,
    pub ticks_per_second: f32,
    pub tick_time: f32,
    /// `mGameTime`
    pub game_time: i32,
    /// `gCollisionPeriod`, `gWorld.mCollisionDetectionFlags`
    pub collision_period: u32,
    pub detection_flags: u32,
    /// `gPhysicsModelsF` (world table entry 79: the trample's minimum squared distance), `kHeightEpsilon`
    pub trample_min: f32,
    pub height_epsilon: f32,
}

/// A unit class's values the models read (`EEUnitClass`'s getters and members; the three effects: set or not).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPmClass {
    pub category: u32,
    pub unit_type: u32,
    pub air_traffic_type: i32,
    pub tile_size: i32,
    pub pitches_with_terrain: u8,
    pub rolls_with_terrain: u8,
    pub fires_broadside: u8,
    pub is_melee: u8,
    pub has_splash_effect: u8,
    pub has_death_effect: u8,
    pub has_shell_hole_effect: u8,
    /// `GetCruiseAltitude()`, `mSlopeSpeedPenalty`, `GetBobPitch()`, `GetBobHeight()`, `GetScale()`, `mMaxVelocity`
    /// (`GetMaxVelocity()`), `mMaxRoll`, `mInnerCollisionSphere`, `GetFacingOffset()`, `GetAngularAcceleration()`,
    /// `GetMaxAngularVelocity()`
    pub cruise_altitude: f32,
    pub slope_penalty: f32,
    pub bob_pitch: f32,
    pub bob_height: f32,
    pub scale: f32,
    pub max_velocity: f32,
    pub max_roll: f32,
    pub inner_collision_sphere: f32,
    pub facing_offset: f32,
    pub angular_acceleration: f32,
    pub max_angular_velocity: f32,
}

/// A unit's statistics (`mStats`: `EEUnitClassUpgrade`'s getters).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPmStats {
    pub max_velocity: f32,
    pub acceleration: f32,
    pub angular_acceleration: f32,
    pub max_angular_velocity: f32,
    pub scale: f32,
}

/// A tile's values (`RTSTile`'s; zeros for no tile).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPmTile {
    pub terrain_type: i32,
    pub ai_value: i8,
    pub height: f32,
}

/// The C++ the models call. Units are `EEComplexUnit*` (`obj`), plain units `EEUnit*`, tiles `RTSTile*`, nodes the
/// lists' node pointers (walked live, as the C++ walked them).
pub trait PmHost {
    fn tick(&mut self) -> EeUnitsPmTick;
    /// `gPhysics->GetRandom()`
    fn random(&mut self) -> f32;
    /// the capture runs (the USST notes)
    fn noting(&mut self) -> bool;
    fn note(&mut self, kind: u32, id: u32, words: &[u32]);
    fn view(&mut self, obj: u64) -> EeUnitsPmUnit;
    fn class(&mut self, obj: u64) -> EeUnitsPmClass;
    fn stats(&mut self, obj: u64) -> EeUnitsPmStats;
    /// `mPhysicalModel.GetLocation()`
    fn location(&mut self, obj: u64) -> GeTransformation;
    /// `SetTransformation(t, true)`
    fn set_transformation(&mut self, obj: u64, t: &GeTransformation);
    /// `SetTransformation(mPhysicalModel.GetLocation(), true)`
    fn refresh_transformation(&mut self, obj: u64);
    /// `mPhysicalModel.SetLocation(t)`
    fn set_location(&mut self, obj: u64, t: &GeTransformation);
    /// `SetLocation(point, true)`
    fn set_location_point(&mut self, obj: u64, p: Ge3DPoint);
    /// `SetLocationXY(x, y, true)`
    fn set_location_xy(&mut self, obj: u64, x: f32, y: f32);
    /// `EEUpdateUnitTile(&t, unit, ...)`
    fn update_tile(&mut self, obj: u64, t: &GeTransformation);
    /// `mPhysicalModel.SetScale(s)`
    fn set_scale(&mut self, obj: u64, s: f32);
    fn angular_velocity(&mut self, obj: u64) -> GeOrientation;
    fn set_angular_velocity(&mut self, obj: u64, o: GeOrientation);
    fn orientation(&mut self, obj: u64) -> GeOrientation;
    fn set_orientation(&mut self, obj: u64, o: GeOrientation);
    /// `GetMaxVelocity()`, `GetAcceleration()`, `GetInverseAcceleration()`, `GetAngularAcceleration()`,
    /// `GetMaxAngularVelocity()`, `GetInverseAngularAcceleration()` (the active group's when the unit is in one)
    fn max_velocity(&mut self, obj: u64) -> f32;
    fn acceleration(&mut self, obj: u64) -> f32;
    fn inverse_acceleration(&mut self, obj: u64) -> f32;
    fn angular_acceleration(&mut self, obj: u64) -> f32;
    fn max_angular_velocity(&mut self, obj: u64) -> f32;
    fn inverse_angular_acceleration(&mut self, obj: u64) -> f32;
    /// `mWaypoints.size()`
    fn waypoint_count(&mut self, obj: u64) -> i32;
    /// `GetNextWaypointPoint(out)` (0: written)
    fn next_waypoint(&mut self, obj: u64, out: &mut Ge3DPoint) -> i32;
    /// `mWaypoints.begin()->mLocation.GetZ()`
    fn first_waypoint_z(&mut self, obj: u64) -> f32;
    /// `GetPathLength(io)`
    fn path_length(&mut self, obj: u64, io: &mut f32);
    /// `GetPathStart(out, length)`'s point
    fn path_start(&mut self, obj: u64) -> Ge3DPoint;
    /// `mCommand ? mCommand->mType : 0`
    fn command_type(&mut self, obj: u64) -> u32;
    /// `GetModel()->GetNumParts()`, `GetModel()->mFrameTimes[animation]`
    fn model_parts(&mut self, obj: u64) -> i32;
    fn frame_time(&mut self, obj: u64, animation: u32) -> u32;
    /// `mTile`
    fn tile_of(&mut self, obj: u64) -> u64;
    /// `mGoalState ? mGoalState->mKind : 0`
    fn goal_kind(&mut self, obj: u64) -> u8;
    /// `EEAttackUnitCast(goal) ? GetAttackTarget() : 0` (an `EEUnit*`), `EEHealGoalCast(goal) ? GetHealTarget() : 0`,
    /// `EECastCalamityGoalCast(goal) != 0`
    fn attack_target(&mut self, obj: u64) -> u64;
    fn heal_target(&mut self, obj: u64) -> u64;
    fn is_cast_goal(&mut self, obj: u64) -> bool;
    /// `mGoalState->GetGoalLocation(out)`, the cast goal's `GetCalamityTargetLocation(out)` (out from zeros)
    fn goal_location(&mut self, obj: u64) -> Ge3DPoint;
    fn calamity_target_location(&mut self, obj: u64) -> Ge3DPoint;
    /// `EEGoalHasLocation(goal)`, with `EELocationGoalYaw(goal)`
    fn goal_facing(&mut self, obj: u64, yaw: &mut f32) -> bool;
    /// the gather goal's `GetGatherResource()` (an `EEUnit*`), the build goal's `GetBuildTarget()`
    fn gather_resource(&mut self, obj: u64) -> u64;
    fn build_target(&mut self, obj: u64) -> u64;
    /// an `EEUnit*`'s `mResourceType`, `mComplexUnit`, tile, class category, class `FliesOver()`
    fn resource_type(&mut self, unit: u64) -> u32;
    fn unit_complex(&mut self, unit: u64) -> u64;
    fn unit_tile(&mut self, unit: u64) -> (i32, i32);
    fn unit_category(&mut self, unit: u64) -> u32;
    fn flies_over(&mut self, unit: u64) -> bool;
    /// the class's `HasArrived(unit, distance)`, the unit's `IsSwimming()`, `PlaceOnGround(p)`, the class's
    /// `GetHeightAt(unit, p)`
    fn has_arrived(&mut self, obj: u64, distance: f32) -> bool;
    fn is_swimming(&mut self, obj: u64) -> bool;
    fn place_on_ground(&mut self, obj: u64, p: Ge3DPoint);
    fn height_at(&mut self, obj: u64, p: Ge3DPoint) -> f32;
    fn set_animation(&mut self, obj: u64, a: i32, b: i32, paused: bool);
    /// `SetBlockingUnit(unit)` (0: none)
    fn set_blocking_unit(&mut self, obj: u64, blocker: u64);
    fn clear_goal_queue(&mut self, obj: u64);
    fn start_action(&mut self, obj: u64, action: i32);
    /// `mUnitClass->ActionChanged(unit)`
    fn action_changed(&mut self, obj: u64);
    /// `EECanStandOn(EETileCoordinate(x, y), unit)`
    fn can_stand_on(&mut self, obj: u64, x: i32, y: i32) -> bool;
    /// the same player, or not the unit's player's enemy
    fn friendly(&mut self, obj: u64, other: u64) -> bool;
    /// the trample's damage: `EECombatComputeDamage(kDamageTrample, ...)` and `TakeHit`
    fn trample(&mut self, obj: u64, other: u64);
    /// `mContacts`: its head node, a node's next and unit, its count
    fn contact_first(&mut self, obj: u64) -> u64;
    fn contact_next(&mut self, node: u64) -> u64;
    fn contact_unit(&mut self, node: u64) -> u64;
    fn contact_count(&mut self, obj: u64) -> i32;
    /// 0: not a gate, 1: a gate the unit collides with, 2: a closed gate (`EEUCGate` and command not 0x3a99)
    fn contact_gate(&mut self, obj: u64) -> i32;
    /// `gWorld.mMap->GetTile(x, y)` (an `RTSTile*`, 0: off the map)
    fn tile(&mut self, x: i32, y: i32) -> u64;
    fn tile_info(&mut self, tile: u64) -> EeUnitsPmTile;
    /// a tile's `mUnits`: its head node, a node's next and `EEUnit*`
    fn tile_unit_first(&mut self, tile: u64) -> u64;
    fn tile_unit_next(&mut self, node: u64) -> u64;
    fn tile_unit(&mut self, node: u64) -> u64;
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool;
    /// the class's `CanMoveOnTerrain(unit, terrain)`
    fn can_move_on_terrain(&mut self, obj: u64, terrain: i32) -> bool;
    /// `unit->IsPassableBy(obj)`; `unit->mUnitClass->IsLandBuilding() && !unit->mComplexUnit->mIsBuilt`
    fn passable_by(&mut self, unit: u64, obj: u64) -> bool;
    fn unbuilt_land_building(&mut self, unit: u64) -> bool;
    /// the map's `GetGroundPoint`, `GetTerrainPitch`, `GetTerrainRoll`; the terrain mesh's left, right, front and back
    /// planes; the mesh height at a point (0 off the mesh's tiles: `GetIsValidTile` of the floors)
    fn ground_point(&mut self, p: Ge3DPoint) -> Ge3DPoint;
    fn terrain_pitch(&mut self, t: &GeTransformation) -> f32;
    fn terrain_roll(&mut self, t: &GeTransformation) -> f32;
    fn edge_planes(&mut self) -> [Ge3DPlane; 4];
    fn mesh_ground(&mut self, x: f32, y: f32) -> f32;
    /// `EEAdjustDiveDepth(unit, io)`
    fn adjust_dive_depth(&mut self, obj: u64, io: &mut Ge3DPoint);
    /// the class's splash effect placed at p, its death effect on the unit, `EEPlaceImpactDecal`, the land ripple
    fn splash(&mut self, obj: u64, p: Ge3DPoint);
    fn death_effect(&mut self, obj: u64);
    fn impact_decal(&mut self, obj: u64, p: Ge3DPoint);
    fn land_ripple(&mut self, p: Ge3DPoint);
    /// the carrier plane's deck walk (pm_air.rs `docked`, `follow_carrier`): `mHomeBase`; the carrier model's tag
    /// points of its animation (`GetModel()->mTagPoints[mAnimation]`): their count (-1: no list) and point `index` at
    /// the carrier's frame in its location scaled by its stats (`EETagPointList::Scale`, `GetPoint`); the class's
    /// `RouteDockWaypoint(unit)`
    fn home_base(&mut self, obj: u64) -> u64;
    fn carrier_tag_count(&mut self, carrier: u64) -> i32;
    fn carrier_tag(&mut self, carrier: u64, index: i32) -> Ge3DPoint;
    fn route_dock_waypoint(&mut self, obj: u64);
    /// a gate's first part (no previous part, a next one): its last linked carrier and last linked part
    fn gate_links(&mut self, obj: u64, carrier: &mut u64, last: &mut u64) -> bool;
    /// `EEPMProjectile::Step0`, `Step2`, `EEPMMissile::Step0` (C++: ee_combat's flight arithmetic and captures)
    fn projectile_step0(&mut self, obj: u64);
    fn projectile_step2(&mut self, obj: u64);
    fn missile_step0(&mut self, obj: u64);
}

// ---- the records, read and written in place

macro_rules! rd {
    ($u:expr, $f:ident) => {
        // SAFETY: the unit's records are valid for the call (the C++ object lives across it)
        unsafe { (*$u.cx).$f }
    };
}
macro_rules! wr {
    ($u:expr, $f:ident, $v:expr) => {{
        let value = $v;
        // SAFETY: as rd!
        unsafe { (*$u.cx).$f = value }
    }};
}
/// a `bool` member read as its byte (a byte other than 0 or 1 is a C++ `true`)
macro_rules! rb {
    ($u:expr, $f:ident) => {
        // SAFETY: as rd!; the byte read is valid for any value
        unsafe { *(std::ptr::addr_of!((*$u.cx).$f) as *const u8) != 0 }
    };
}
pub(crate) use {rb, rd, wr};

pub(crate) fn hit_points(u: EeUnitsPmUnit) -> i64 {
    // SAFETY: as rd!
    unsafe { (*u.unit).mHitPoints as i64 }
}

pub(crate) fn tile_x(u: EeUnitsPmUnit) -> i32 {
    // SAFETY: as rd!
    unsafe { (*u.rts).mTileX as i32 }
}

pub(crate) fn tile_y(u: EeUnitsPmUnit) -> i32 {
    // SAFETY: as rd!
    unsafe { (*u.rts).mTileY as i32 }
}

pub(crate) fn unit_id(u: EeUnitsPmUnit) -> u32 {
    // SAFETY: as rd!
    unsafe { (*u.rts).mUnitID as u32 }
}

/// `SetVelocity` (written only when it differs: the same value either way)
pub(crate) fn set_velocity(u: EeUnitsPmUnit, v: f32) {
    if v != rd!(u, mVelocity) {
        wr!(u, mVelocity, v);
    }
}

// ---- the constants, as the C++ spells them

pub(crate) const PI: f32 = 3.141_592_7;
pub(crate) const TWO_PI: f32 = 6.283_185_5;
pub(crate) const HALF_PI: f32 = 1.570_796_4;
/// `kPMTurnEpsilon`, `kPMEdgeOffset`, `kPMFiveDegrees`, `kPMMinimumSpeed`, `kPMContactWindow`
pub(crate) const TURN_EPSILON: f32 = 0.001;
const EDGE_OFFSET: f32 = 0.001;
pub(crate) const FIVE_DEGREES: f32 = 0.087_266_47;
pub(crate) const MINIMUM_SPEED: f32 = 1.0;
pub(crate) const CONTACT_WINDOW: f32 = 0.25;
/// `kMoveDistance` (motion.rs's push past the move distance: the USST note's word)
const MOVE_DISTANCE: f32 = 0.03;

pub(crate) const GOAL_ATTACK_UNIT: u8 = 2;
pub(crate) const GOAL_BUILD_REPAIR: u8 = 4;
pub(crate) const GOAL_GATHER: u8 = 6;
pub(crate) const RESOURCE_WOOD: u32 = 3;
pub(crate) const ACTION_TAXI: u32 = 1002;
pub(crate) const ACTION_DESCEND: u32 = 1012;
pub(crate) const ACTION_CONTINUE_TO_DESCEND: u32 = 1013;
pub(crate) const ACTION_BRAKE: u32 = 1014;
pub(crate) const ACTION_CONTINUE_TO_BRAKE: u32 = 1015;
pub(crate) const ACTION_FIND_NEW_RESOURCE: i32 = 4006;
pub(crate) const ACTION_IDLE: u32 = 5000;
pub(crate) const ACTION_FADING_OUT_IN_SPACE: u32 = 6009;
pub(crate) const CATEGORY_SUBMERGED_NAVAL: u32 = 0xa;
pub(crate) const CATEGORY_SEA_ANIMAL: u32 = 0x1b;
pub(crate) const CATEGORY_GATE: u32 = 0x19;
pub(crate) const CATEGORY_PLANET_SPACESHIP: u32 = 0x22;
pub(crate) const TERRAIN_SPACE: i32 = 12;
pub(crate) const UNIT_TYPE_AIRPORT: u32 = 0x42;
pub(crate) const UNIT_TYPE_SP15_CAPITAL_SHIP: u32 = 0x2da;
pub(crate) const UNIT_TYPE_CAPITAL_SHIP_YAMATO: u32 = 0x33c;

/// The USST kinds of the arithmetic leaves (ee/EEUnitStore.h `EEUnitStateKind`).
const NOTE_ROLL: u32 = 3;
const NOTE_PUSH: u32 = 4;
const NOTE_TURN: u32 = 5;
const NOTE_SPIN_DOWN: u32 = 6;
const NOTE_STEADY_TURN: u32 = 7;
const NOTE_ACCELERATE: u32 = 8;
const NOTE_SPEED_MODE: u32 = 9;
const NOTE_PITCH_MODE: u32 = 10;
const NOTE_APPROACH_PITCH: u32 = 11;

pub(crate) fn um_abs(v: f32) -> f32 {
    if v < 0.0 {
        -v
    } else {
        v
    }
}

fn bits(v: f32) -> u32 {
    v.to_bits()
}

/// The forward vector a transformation points (`Apply(GEVector(0, 1, 0))`).
pub(crate) fn forward(t: &GeTransformation) -> GeVector {
    t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 })
}

/// `UMArcTan2(-forward.x, forward.y)`: the heading.
pub(crate) fn heading(t: &GeTransformation) -> f32 {
    let r = forward(t);
    um_arctan2(-r.x, r.y)
}

pub(crate) fn position(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.x(), y: t.y(), z: t.z() }
}

pub(crate) fn set_position(t: &mut GeTransformation, p: Ge3DPoint) {
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
}

pub(crate) fn ypr(t: &mut GeTransformation, yaw: f32, pitch: f32, roll: f32) {
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
}

/// A rotation alone: `SetOrientationYPR`, the position 0, the bottom row (0, 0, 0, 1).
pub(crate) fn rotation(yaw: f32, pitch: f32, roll: f32) -> GeTransformation {
    let mut t = GeTransformation::default();
    ypr(&mut t, yaw, pitch, roll);
    t.m[0][3] = 0.0;
    t.m[1][3] = 0.0;
    t.m[2][3] = 0.0;
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    t
}

pub(crate) fn add(p: Ge3DPoint, v: GeVector) -> Ge3DPoint {
    Ge3DPoint { x: p.x + v.x, y: p.y + v.y, z: p.z + v.z }
}

/// `GEVector(0, y, 0)`
pub(crate) fn fwd(y: f32) -> GeVector {
    GeVector { x: 0.0, y, z: 0.0 }
}

/// The xy distance of the waypoint steps (`(float)sqrt(dx * dx + dy * dy)`, under 0.05 none).
pub(crate) fn flat_distance(target: Ge3DPoint, from: Ge3DPoint) -> f32 {
    let d = (target.x - from.x) * (target.x - from.x) + (target.y - from.y) * (target.y - from.y);
    let d = d.sqrt();
    if d < 0.05 {
        0.0
    } else {
        d
    }
}

/// One call's context: the host and the tick's values.
pub struct Ctx<'a> {
    pub h: &'a mut dyn PmHost,
    pub t: EeUnitsPmTick,
}

impl Ctx<'_> {
    pub(crate) fn note(&mut self, kind: u32, u: EeUnitsPmUnit, words: &[u32]) {
        if self.h.noting() {
            self.h.note(kind, unit_id(u), words);
        }
    }

    pub(crate) fn set_has_arrived(&mut self, u: EeUnitsPmUnit, distance: f32) {
        let flag = self.h.has_arrived(u.obj, distance);
        if rb!(u, mHasArrived) != flag {
            wr!(u, mHasArrived, flag);
        }
    }

    pub(crate) fn set_angular(&mut self, u: EeUnitsPmUnit, yaw: f32, pitch: f32, roll: f32) {
        self.h.set_angular_velocity(u.obj, GeOrientation { yaw, pitch, roll });
    }
}

// ---- the dispatch (each kind's vtable)

/// `EEPhysicsModel::Step`: the step of the unit's physics action, then the after-step work (not after action 2,
/// nor for a dead unit in another action).
pub fn step(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match rd!(u, mCurrentPhysicsAction) {
        0 => step0(c, k, u),
        1 => step1(c, k, u),
        2 => {
            step2(c, k, u);
            return;
        }
        3 => step3(c, k, u),
        4 => step4(c, k, u),
        5 => step5(c, k, u),
        _ => {
            if hit_points(u) <= 0 {
                return;
            }
        }
    }
    after_step(c, k, u);
}

pub fn step0(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => crate::units::pm_ground::land_step0(c, k, u),
        Kind::Naval => crate::units::pm_ground::naval_step0(c, k, u),
        Kind::SubmergedNaval => crate::units::pm_ground::sub_step0(c, k, u),
        Kind::SeaAnimal => crate::units::pm_ground::sea_animal_step0(c, k, u),
        Kind::Spaceship => crate::units::pm_ground::spaceship_step0(c, k, u),
        Kind::Airplane => crate::units::pm_air::airplane_step0(c, k, u),
        Kind::AirAnimal => crate::units::pm_air::air_animal_step0(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_step0(c, k, u),
        Kind::Projectile => c.h.projectile_step0(u.obj),
        Kind::Missile => c.h.missile_step0(u.obj),
        Kind::Building => {}
    }
}

pub(crate) fn step1(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => crate::units::pm_ground::land_step1(c, k, u),
        Kind::Naval => crate::units::pm_ground::naval_step1(c, k, u),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_step1(c, k, u),
        Kind::Spaceship => crate::units::pm_ground::spaceship_step1(c, k, u),
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::airplane_step1(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_step1(c, k, u),
        Kind::Projectile | Kind::Missile | Kind::Building => {}
    }
}

pub(crate) fn step2(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => {
            set_velocity(u, 0.0);
            c.set_angular(u, 0.0, 0.0, 0.0);
        }
        Kind::Naval => crate::units::pm_ground::naval_step2(c, k, u),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_step2(c, k, u),
        Kind::Building => crate::units::pm_ground::building_step2(c, u),
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::airplane_step2(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_step2(c, k, u),
        Kind::Projectile => c.h.projectile_step2(u.obj),
        Kind::Missile | Kind::Spaceship => {}
    }
}

/// `EEPhysicsModel::Step3` (no override): re-bobs a unit that stands still (not under the helicopter model), else
/// brakes it as `Step4`.
pub(crate) fn step3(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    if rd!(u, mVelocity) == 0.0 && c.h.angular_velocity(u.obj).yaw == 0.0 && k != Kind::Helicopter {
        remove_bob(c, k, u);
        apply_bob(c, k, u);
    } else {
        step4(c, k, u);
    }
}

pub(crate) fn step4(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => crate::units::pm_ground::land_step4(c, k, u),
        Kind::Naval => crate::units::pm_ground::naval_step4(c, k, u),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_step4(c, k, u),
        Kind::Spaceship => crate::units::pm_ground::spaceship_step4(c, k, u),
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::airplane_step4(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_step4(c, k, u),
        Kind::Projectile | Kind::Missile | Kind::Building => {}
    }
}

pub(crate) fn step5(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => crate::units::pm_ground::land_step5(c, k, u),
        Kind::Naval => crate::units::pm_ground::naval_step5(c, k, u),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_step5(c, k, u),
        Kind::Spaceship => crate::units::pm_ground::spaceship_step5(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_step5(c, k, u),
        Kind::Airplane | Kind::AirAnimal | Kind::Projectile | Kind::Missile | Kind::Building => {}
    }
}

pub(crate) fn apply_bob(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Naval => crate::units::pm_ground::naval_apply_bob(c, u),
        Kind::Helicopter => crate::units::pm_air::heli_apply_bob(c, u),
        _ => {}
    }
}

pub(crate) fn remove_bob(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Naval => crate::units::pm_ground::naval_remove_bob(c, u),
        Kind::Spaceship => crate::units::pm_ground::spaceship_remove_bob(c, u),
        Kind::Helicopter => crate::units::pm_air::heli_remove_bob(c, u),
        _ => {}
    }
}

pub(crate) fn move_unit(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft => move_by_push(c, u, true),
        Kind::Naval | Kind::Spaceship => move_by_push(c, u, false),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_move(c, u),
        Kind::Helicopter => crate::units::pm_air::heli_move(c, u),
        Kind::Airplane | Kind::AirAnimal | Kind::Projectile | Kind::Missile | Kind::Building => {}
    }
}

pub(crate) fn collide(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Land | Kind::Hovercraft | Kind::Naval | Kind::Building | Kind::Spaceship => collide_base(c, k, u),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_collide(c, k, u),
        Kind::Helicopter => crate::units::pm_air::heli_collide(c, k, u),
        Kind::Airplane | Kind::AirAnimal | Kind::Projectile | Kind::Missile => {}
    }
}

pub(crate) fn after_step(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    match k {
        Kind::Airplane | Kind::AirAnimal | Kind::Projectile | Kind::Missile | Kind::Building => {}
        _ => after_step_base(c, k, u),
    }
}

pub(crate) fn push_moving(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, o: EeUnitsPmUnit) -> bool {
    match k {
        Kind::Naval | Kind::Spaceship => crate::units::pm_ground::push_moving_naval(c, u, o),
        _ => push_moving_base(c, u, o),
    }
}

/// `ClipStep(unit, move, io, out)`: whether the step stands (`io` moved back or slid when it does not).
pub(crate) fn clip_step(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    match k {
        Kind::Land | Kind::Hovercraft => crate::units::pm_ground::land_clip_step(c, u, mv, io),
        Kind::Naval => crate::units::pm_ground::naval_clip_step(c, u, mv, io),
        Kind::SubmergedNaval | Kind::SeaAnimal => crate::units::pm_ground::sub_clip_step(c, u, mv, io),
        Kind::Spaceship => crate::units::pm_ground::spaceship_clip_step(c, u, mv, io),
        Kind::Helicopter => crate::units::pm_air::air_clip_step(c, u, mv, io),
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::airplane_clip_step(c, u, mv, io),
        Kind::Projectile | Kind::Missile | Kind::Building => clip_step_base(c, mv, io),
    }
}

/// `TurnToward(unit, from, to, outA, outB, outC, facing)`: (the angle left, the new rate, the new heading).
pub(crate) fn turn_toward(
    c: &mut Ctx,
    k: Kind,
    u: EeUnitsPmUnit,
    from: Ge3DPoint,
    to: Ge3DPoint,
    facing: bool,
) -> (f32, f32, f32) {
    match k {
        Kind::Airplane | Kind::Spaceship => {
            if rd!(u, mCurrentPhysicsOrder) == 5 {
                turn_toward_base(c, u, from, to, facing)
            } else {
                steady_turn(c, u)
            }
        }
        Kind::AirAnimal | Kind::SeaAnimal => crate::units::pm_ground::circling_turn(c, u, from, to, facing),
        _ => turn_toward_base(c, u, from, to, facing),
    }
}

pub(crate) fn approach_pitch(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, current: f32, target: f32) -> (f32, f32) {
    let rate = c.h.angular_velocity(u.obj).pitch;
    let mode = pitch_mode(c, k, u, rate, target - current);
    // EEPMNaval's own: the class's rates; every other kind's the base's over its PitchMode
    approach_pitch_with(c, u, k == Kind::Naval, mode, current, target)
}

pub(crate) fn pitch_mode(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, rate: f32, angle: f32) -> i32 {
    pitch_mode_with(c, u, matches!(k, Kind::Naval | Kind::Spaceship), rate, angle)
}

pub(crate) fn accelerate(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, distance: f32, turn: f32) -> f32 {
    if k == Kind::SeaAnimal {
        return crate::units::pm_ground::sea_animal_accelerate(c, k, u, distance, turn);
    }
    let speed = rd!(u, mVelocity);
    let max_speed = c.h.max_velocity(u.obj);
    let accel = c.h.acceleration(u.obj);
    let mode = speed_mode(c, k, u, distance, turn);
    accelerate_with(c, u, speed, max_speed, accel, distance, mode)
}

pub(crate) fn speed_mode(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, distance: f32, turn: f32) -> i32 {
    match k {
        Kind::Helicopter => speed_mode_with(c, u, true, distance, turn),
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::airplane_speed_mode(c, k, u, distance, turn),
        Kind::SeaAnimal => crate::units::pm_ground::sea_animal_speed_mode(c, u, distance, turn),
        _ => speed_mode_with(c, u, false, distance, turn),
    }
}

/// `ComputePitch` (the airplanes', the helicopter's, the sea animal's; the others have none: the value).
pub(crate) fn compute_pitch(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, v: f32) -> f32 {
    match k {
        Kind::Airplane | Kind::AirAnimal => crate::units::pm_air::displayed_dive(c, u, v, 0.261_799_4),
        Kind::SeaAnimal => crate::units::pm_air::displayed_dive(c, u, v, 0.436_332_32),
        Kind::Helicopter => crate::units::pm_air::heli_compute_pitch(c, u, v),
        _ => v,
    }
}

/// `ComputeRoll` (the airplanes', the helicopter's, the sea animal's; the others have none: 0).
pub fn compute_roll(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit, v: f32) -> f32 {
    match k {
        Kind::Airplane | Kind::AirAnimal | Kind::SeaAnimal => crate::units::pm_air::bank_for_turn(c, u, v),
        Kind::Helicopter => crate::units::pm_air::heli_compute_roll(c, u, v),
        _ => 0.0,
    }
}

// ---- the base model's arithmetic leaves (with their USST notes)

fn turn_in(c: &mut Ctx, u: EeUnitsPmUnit) -> EeUnitsTurnIn {
    EeUnitsTurnIn {
        tick_seconds: c.t.tick_seconds,
        ticks_per_second: c.t.ticks_per_second,
        accel: c.h.angular_acceleration(u.obj),
        max: c.h.max_angular_velocity(u.obj),
        inverse_accel: c.h.inverse_angular_acceleration(u.obj),
        rate: c.h.angular_velocity(u.obj).yaw,
        ..Default::default()
    }
}

/// `TurnTowardAngle`: (the angle left, the new rate, the new heading).
pub(crate) fn turn_toward_angle(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    current: f32,
    target: f32,
    flag: bool,
) -> (f32, f32, f32) {
    let mut i = turn_in(c, u);
    let class = c.h.class(u.obj);
    i.current = current;
    i.target = target;
    i.use_facing = u8::from(flag);
    i.broadside = u8::from(flag && class.fires_broadside != 0);
    i.facing_offset = if flag { class.facing_offset } else { 0.0 };
    let t = physics::turn_toward_angle(&i);
    if c.h.noting() {
        let words = [
            bits(i.tick_seconds),
            bits(i.ticks_per_second),
            bits(i.accel),
            bits(i.max),
            bits(i.inverse_accel),
            bits(i.rate),
            bits(current),
            bits(target),
            u32::from(i.use_facing),
            u32::from(i.broadside),
            if flag { bits(i.facing_offset) } else { 0 },
            bits(t.delta),
            bits(t.rate),
            bits(t.heading),
        ];
        c.note(NOTE_TURN, u, &words);
    }
    (t.delta, t.rate, t.heading)
}

/// `EEPhysicsModel::TurnToward`: toward the bearing from `from` to `to`.
pub(crate) fn turn_toward_base(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    from: Ge3DPoint,
    to: Ge3DPoint,
    facing: bool,
) -> (f32, f32, f32) {
    let bearing = um_arctan2(-(to.x - from.x), to.y - from.y);
    let location = c.h.location(u.obj);
    let current = heading(&location);
    turn_toward_angle(c, u, current, bearing, facing)
}

/// `SteadyTurn`: (0, the new rate, the new heading).
pub(crate) fn steady_turn(c: &mut Ctx, u: EeUnitsPmUnit) -> (f32, f32, f32) {
    let step = c.t.tick_seconds;
    let stats = c.h.stats(u.obj);
    let rate = c.h.angular_velocity(u.obj).yaw;
    let state = rd!(u, mCurrentPhysicsOrder);
    let command = c.h.command_type(u.obj);
    let location = c.h.location(u.obj);
    let current = heading(&location);
    let t = physics::steady_turn(
        rate,
        current,
        step,
        stats.angular_acceleration,
        stats.max_angular_velocity,
        state as i32,
        command,
    );
    if c.h.noting() {
        let words = [
            bits(step),
            bits(stats.angular_acceleration),
            bits(stats.max_angular_velocity),
            bits(rate),
            bits(current),
            state as u32,
            command,
            bits(t.rate),
            bits(t.heading),
        ];
        c.note(NOTE_STEADY_TURN, u, &words);
    }
    (0.0, t.rate, t.heading)
}

/// `SpinDown`: (the new rate, the new heading).
pub(crate) fn spin_down(c: &mut Ctx, u: EeUnitsPmUnit) -> (f32, f32) {
    let step = c.t.tick_seconds;
    let rate = c.h.angular_velocity(u.obj).yaw;
    let location = c.h.location(u.obj);
    let current = heading(&location);
    let accel = c.h.angular_acceleration(u.obj);
    let max = c.h.max_angular_velocity(u.obj);
    let t = physics::spin_down(rate, current, step, accel, max);
    if c.h.noting() {
        let words = [bits(step), bits(accel), bits(max), bits(rate), bits(current), bits(t.rate), bits(t.heading)];
        c.note(NOTE_SPIN_DOWN, u, &words);
    }
    (t.rate, t.heading)
}

fn pitch_in(c: &mut Ctx, u: EeUnitsPmUnit, naval: bool) -> EeUnitsPitchIn {
    let (accel, max, inverse) = if naval {
        let class = c.h.class(u.obj);
        (class.angular_acceleration, class.max_angular_velocity, 0.0)
    } else {
        (c.h.angular_acceleration(u.obj), c.h.max_angular_velocity(u.obj), c.h.inverse_angular_acceleration(u.obj))
    };
    EeUnitsPitchIn {
        tick_seconds: c.t.tick_seconds,
        ticks_per_second: c.t.ticks_per_second,
        accel,
        max,
        inverse_accel: inverse,
        rate: c.h.angular_velocity(u.obj).pitch,
        naval: u8::from(naval),
    }
}

/// `EEPMPitchMode` (the base's or, `naval`, `EEPMNaval`'s).
fn pitch_mode_with(c: &mut Ctx, u: EeUnitsPmUnit, naval: bool, rate: f32, angle: f32) -> i32 {
    let i = pitch_in(c, u, naval);
    let mode = physics::pitch_mode(&i, rate, angle);
    if c.h.noting() {
        let words = [
            u32::from(i.naval),
            bits(i.tick_seconds),
            bits(i.ticks_per_second),
            bits(i.accel),
            bits(i.inverse_accel),
            bits(rate),
            bits(angle),
            mode as u32,
        ];
        c.note(NOTE_PITCH_MODE, u, &words);
    }
    mode
}

/// `EEPMApproachPitch`: (the new rate, the new pitch).
fn approach_pitch_with(c: &mut Ctx, u: EeUnitsPmUnit, naval: bool, mode: i32, current: f32, target: f32) -> (f32, f32) {
    let i = pitch_in(c, u, naval);
    let t = physics::approach_pitch(&i, current, target, mode);
    if c.h.noting() {
        let words = [
            u32::from(i.naval),
            bits(i.tick_seconds),
            bits(i.accel),
            bits(i.max),
            bits(i.rate),
            bits(current),
            bits(target),
            mode as u32,
            bits(t.rate),
            bits(t.heading),
        ];
        c.note(NOTE_APPROACH_PITCH, u, &words);
    }
    (t.rate, t.heading)
}

/// `PitchTowardSlope`: (the pitch rate, the pitch) toward the slope from `from` to `to`.
pub(crate) fn pitch_toward_slope(
    c: &mut Ctx,
    k: Kind,
    u: EeUnitsPmUnit,
    from: Ge3DPoint,
    to: Ge3DPoint,
    distance: f32,
) -> (f32, f32) {
    let location = c.h.location(u.obj);
    let angle = um_arccos(-forward(&location).z) - HALF_PI;
    if distance > 0.0 {
        let target = um_arccos(-((to.z - from.z) / distance)) - HALF_PI;
        approach_pitch(c, k, u, angle, target)
    } else {
        (0.0, angle)
    }
}

/// `EEPMAccelerate`: the speed after the mode.
pub(crate) fn accelerate_with(
    c: &mut Ctx,
    u: EeUnitsPmUnit,
    speed: f32,
    max_speed: f32,
    accel: f32,
    distance: f32,
    mode: i32,
) -> f32 {
    let i = EeUnitsSpeedIn {
        tick_seconds: c.t.tick_seconds,
        speed,
        max_speed,
        accel,
        waypoints: c.h.waypoint_count(u.obj),
        ..Default::default()
    };
    let result = physics::accelerate(&i, distance, mode);
    if c.h.noting() {
        let words = [
            bits(i.tick_seconds),
            bits(speed),
            bits(max_speed),
            bits(accel),
            i.waypoints as u32,
            bits(distance),
            mode as u32,
            bits(result),
        ];
        c.note(NOTE_ACCELERATE, u, &words);
    }
    result
}

/// `EEPMSpeedMode` (`helicopter`: `EEPMHelicopter::SpeedMode`).
pub(crate) fn speed_mode_with(c: &mut Ctx, u: EeUnitsPmUnit, helicopter: bool, distance: f32, turn: f32) -> i32 {
    let class = c.h.class(u.obj);
    let mut i = EeUnitsSpeedIn {
        tick_seconds: c.t.tick_seconds,
        ticks_per_second: c.t.ticks_per_second,
        speed: rd!(u, mVelocity),
        max_speed: c.h.max_velocity(u.obj),
        accel: c.h.acceleration(u.obj),
        inverse_accel: c.h.inverse_acceleration(u.obj),
        yaw_rate: c.h.angular_velocity(u.obj).yaw,
        slope_penalty: if helicopter { 0.0 } else { class.slope_penalty },
        waypoints: c.h.waypoint_count(u.obj),
        path_length: 0.0,
        terrain_pitch: 0.0,
    };
    // the reads the mode makes only when it needs them
    if i.waypoints >= 2 {
        c.h.path_length(u.obj, &mut i.path_length);
    }
    if !helicopter && i.slope_penalty < 1.0 {
        let location = c.h.location(u.obj);
        i.terrain_pitch = c.h.terrain_pitch(&location);
    }
    let mode = physics::speed_mode(&i, distance, turn, helicopter);
    if c.h.noting() {
        let words = [
            u32::from(helicopter),
            bits(i.tick_seconds),
            bits(i.ticks_per_second),
            bits(i.speed),
            bits(i.max_speed),
            bits(i.accel),
            bits(i.inverse_accel),
            bits(i.yaw_rate),
            if helicopter { 0 } else { bits(class.slope_penalty) },
            i.waypoints as u32,
            bits(if i.waypoints >= 2 { i.path_length } else { distance }),
            bits(i.terrain_pitch),
            bits(distance),
            bits(turn),
            mode as u32,
        ];
        c.note(NOTE_SPEED_MODE, u, &words);
    }
    mode
}

/// `EEPMPush`: the collision push's arithmetic (motion.rs over the store) with its USST note.
pub(crate) fn push_step(c: &mut Ctx, u: EeUnitsPmUnit, speed: f32) -> crate::units::motion::EeUnitsPush {
    let location = c.h.location(u.obj);
    let input = crate::units::motion::EeUnitsPushIn {
        push_x: rd!(u, mPushVelocityX),
        push_y: rd!(u, mPushVelocityY),
        loc_x: location.x(),
        loc_y: location.y(),
        period: c.t.collision_period,
        max_velocity: speed,
        tick_seconds: c.t.tick_seconds,
    };
    let id = unit_id(u);
    let p = crate::units::store::with_store(|s| {
        crate::units::step::with_reads(|reads| crate::units::motion::step_push(s, reads, id, u.obj, &input))
    });
    if c.h.noting() {
        let mut words = [
            bits(input.push_x),
            bits(input.push_y),
            input.period,
            bits(speed),
            bits(input.tick_seconds),
            bits(input.loc_x),
            bits(input.loc_y),
            0,
            0,
            0,
            0,
            0,
            0,
        ];
        if p.moving != 0 {
            words[7] = 1;
            words[8] = u32::from(p.dy + p.dx > MOVE_DISTANCE);
            words[9] = bits(p.dx);
            words[10] = bits(p.dy);
            words[11] = bits(p.x);
            words[12] = bits(p.y);
        }
        c.note(NOTE_PUSH, u, &words);
    }
    p
}

// ---- the base model's bodies

/// `EEPhysicsModel::Move` (`on_ground`; `MoveByPush(unit, false)` is EEPMNaval's and EEPMSpaceship's copy): the
/// collision push applied onto a tile the unit can stand on, the idle shuffle started or stopped, the push cleared.
pub(crate) fn move_by_push(c: &mut Ctx, u: EeUnitsPmUnit, on_ground: bool) {
    let mut stopped = false;
    let max = c.h.max_velocity(u.obj);
    let p = push_step(c, u, max);
    if p.moving != 0 {
        let ty = floor(p.y);
        let tx = floor(p.x);
        let mut free = c.h.can_stand_on(u.obj, tx, ty);
        if free
            && (tile_x(u) as i64 - tx as i64).abs() == 1
            && (tile_y(u) as i64 - ty as i64).abs() == 1
            && !c.h.can_stand_on(u.obj, tx, tile_y(u))
            && !c.h.can_stand_on(u.obj, tile_x(u), ty)
        {
            free = false;
        }
        if free {
            let mut time = rd!(u, mCollisionResolutionCount);
            if (tx != tile_x(u) || ty != tile_y(u)) && time == 0.0 && rd!(u, mF24C) != 0.0 {
                time = c.t.tick_time + rd!(u, mF24C);
            }
            c.h.set_location_xy(u.obj, p.x, p.y);
            wr!(u, mCollisionResolutionCount, time);
            let action = rd!(u, mCurrentPhysicsAction);
            if action == 5 && rd!(u, mCollisionRadius) >= MINIMUM_SPEED {
                c.set_angular(u, 0.0, 0.0, 0.0);
            }
            let action = rd!(u, mCurrentPhysicsAction);
            if action != 5 && action != 0 {
                if p.past_move != 0 {
                    let animation = rd!(u, mAnimation);
                    if c.h.command_type(u.obj) == ACTION_IDLE && (animation == 0 || animation == 7 || animation == 8) {
                        c.h.set_animation(u.obj, 0xd, -1, true);
                        if rb!(u, mIsAnimationPaused) {
                            wr!(u, mIsAnimationPaused, false);
                        }
                        let frame = c.h.frame_time(u.obj, 13);
                        if rd!(u, mAnimationTime) as u32 != frame {
                            wr!(u, mAnimationTime, frame as std::os::raw::c_ulong);
                        }
                    } else if rd!(u, mAnimation) == 0xd && rb!(u, mIsAnimationPaused) {
                        wr!(u, mIsAnimationPaused, false);
                    }
                } else {
                    stopped = true;
                }
            }
            wr!(u, mCollisionResolutionCount, c.t.tick_time + rd!(u, mCollisionResolutionCount));
            if on_ground && !c.h.is_swimming(u.obj) {
                let location = c.h.location(u.obj);
                c.h.place_on_ground(u.obj, position(&location));
            }
            c.h.refresh_transformation(u.obj);
        } else {
            stopped = true;
        }
    } else {
        stopped = true;
    }
    if stopped && c.h.command_type(u.obj) == ACTION_IDLE && rd!(u, mAnimation) == 0xd {
        c.h.set_animation(u.obj, 7, -1, true);
    }
    if rd!(u, mPushVelocityX) != 0.0 {
        wr!(u, mPushVelocityX, 0.0);
    }
    if rd!(u, mPushVelocityY) != 0.0 {
        wr!(u, mPushVelocityY, 0.0);
    }
}

/// `EEPhysicsModel::AfterStep`: the roll levelled (motion.rs over the store), then the collisions resolved
/// (`Collide`, `Move`) unless the unit rests with no push; a resting or non-collidable unit's collision time cleared.
pub(crate) fn after_step_base(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let before = rd!(u, mRoll);
    if before != 0.0 {
        let id = unit_id(u);
        let step = c.t.tick_seconds;
        let roll = crate::units::store::with_store(|s| {
            crate::units::step::with_reads(|reads| crate::units::motion::step_roll(s, reads, id, u.obj, before, step))
        });
        wr!(u, mRoll, roll);
        if c.h.noting() {
            c.note(NOTE_ROLL, u, &[bits(before), bits(step), bits(rd!(u, mRoll))]);
        }
    }
    let collidable = rd!(u, mIsCollidable) != 0;
    if (!collidable || !(c.h.contact_count(u.obj) > 0)) && rd!(u, mCollisionResolutionCount) == 0.0 {
        return;
    }
    if collidable {
        let class = c.h.class(u.obj);
        if rd!(u, mVelocity) == 0.0
            && c.h.angular_velocity(u.obj).yaw == 0.0
            && (class.max_velocity <= 0.0
                || (um_abs(rd!(u, mPushVelocityX)) < TURN_EPSILON && um_abs(rd!(u, mPushVelocityY)) < TURN_EPSILON))
            && !(class.is_melee != 0 && c.h.goal_kind(u.obj) == GOAL_ATTACK_UNIT)
        {
            wr!(u, mCollisionResolutionCount, 0.0);
            if c.h.command_type(u.obj) == ACTION_IDLE && rd!(u, mAnimation) == 0xd {
                c.h.set_animation(u.obj, 7, -1, true);
            }
        } else {
            collide(c, k, u);
            move_unit(c, k, u);
        }
    } else {
        wr!(u, mCollisionResolutionCount, 0.0);
    }
}

/// `EEPhysicsModel::Collide`: the unit pushed against each unit it touches (`PushMoving` for two large mobile units,
/// else `Push`) until one is handled; closed gates and citizens working elsewhere skipped.
pub(crate) fn collide_base(c: &mut Ctx, k: Kind, u: EeUnitsPmUnit) {
    let speed = rd!(u, mCollisionRadius);
    let mut node = c.h.contact_first(u.obj);
    while node != 0 {
        let other = c.h.contact_unit(node);
        let o = c.h.view(other);
        // each contact is judged on its own
        let mut collides = true;
        if rb!(o, mIsCitizen) {
            let kind = c.h.goal_kind(other);
            if kind == GOAL_GATHER || kind == GOAL_BUILD_REPAIR {
                if !rb!(u, mIsCitizen) {
                    collides = false;
                } else {
                    let own = c.h.goal_kind(u.obj);
                    if own != kind {
                        collides = false;
                    } else if own == GOAL_GATHER {
                        collides = c.h.gather_resource(u.obj) == c.h.gather_resource(other);
                    } else {
                        collides = c.h.build_target(u.obj) == c.h.build_target(other);
                    }
                }
            }
        }
        match c.h.contact_gate(other) {
            2 => {
                node = c.h.contact_next(node);
                continue;
            }
            1 => collides = true,
            _ => {}
        }
        if collides {
            if speed >= MINIMUM_SPEED && rd!(o, mCollisionRadius) >= MINIMUM_SPEED && c.h.max_velocity(other) > 0.0 {
                if push_moving(c, k, u, o) {
                    return;
                }
            } else if push(c, u, o) {
                return;
            }
        }
        node = c.h.contact_next(node);
    }
}

/// The C++'s `GE3DLine::IntersectsIn2D(other, t, otherT)`: the parameters written only on a crossing.
pub(crate) fn intersect_2d(a: &Ge3DLine, b: &Ge3DLine, t: &mut f32, ot: &mut f32) -> bool {
    match a.intersects_in_2d(b) {
        Some((x, y)) => {
            *t = x;
            *ot = y;
            true
        }
        None => false,
    }
}

/// `EEPhysicsModel::PushMoving`: a moving unit running into another: the smaller one pushed aside, or this one
/// stopped, slowed or backed off (a stuck one sent idle); true when it gave up its goals.
pub(crate) fn push_moving_base(c: &mut Ctx, u: EeUnitsPmUnit, o: EeUnitsPmUnit) -> bool {
    let mut result = false;
    let speed = rd!(u, mCollisionRadius);
    let other_speed = rd!(o, mCollisionRadius);
    let mut slow = 0.75f32;
    let mut t = 0.0f32;
    let mut ot = 0.0f32;
    let mut sign = 1.0f32;
    let mut other_sign = 1.0f32;
    let mut other_left = 0.0f32;
    let mut left = 0.0f32;
    let location = c.h.location(u.obj);
    let mut direction = forward(&location);
    let other_location = c.h.location(o.obj);
    let mut other_direction = forward(&other_location);
    direction.normalize();
    other_direction.normalize();
    let mut angle = um_abs(um_arccos(direction.dot(other_direction)));
    while angle > PI {
        angle -= PI;
    }
    if angle > HALF_PI {
        angle = PI - angle;
    }
    direction.x = direction.x * speed;
    direction.y = direction.y * speed;
    direction.z = direction.z * speed;
    other_direction.x = other_direction.x * other_speed;
    other_direction.y = other_direction.y * other_speed;
    other_direction.z = other_direction.z * other_speed;
    let line = Ge3DLine { start: position(&location), direction };
    let other_line = Ge3DLine { start: position(&other_location), direction: other_direction };
    let crossing = intersect_2d(&line, &other_line, &mut t, &mut ot);
    let x = -other_direction.x;
    other_direction.x = other_direction.y;
    other_direction.y = x;
    let cross_line = Ge3DLine { start: other_line.start, direction: other_direction };
    let (mut cross_t, mut cross_ot) = (0.0f32, 0.0f32);
    let near = if intersect_2d(&line, &cross_line, &mut cross_t, &mut cross_ot)
        && um_abs(cross_ot) * other_speed <= 0.5
        && ((cross_t > 0.0 && cross_t <= (other_speed + speed) * 0.5) || rd!(u, mVelocity) == 0.0)
        && angle <= FIVE_DEGREES
    {
        t = if cross_t > 0.0 { 0.5 } else { -0.5 };
        true
    } else {
        false
    };
    if crossing {
        if t > 0.0 && t < CONTACT_WINDOW {
            t = -t;
        }
        if ot > 0.0 && ot < CONTACT_WINDOW {
            ot = -ot;
        }
        sign = if t > 0.0 { -1.0 } else { 1.0 };
        other_sign = if ot > 0.0 { -1.0 } else { 1.0 };
        if t < 0.0 {
            t = -t;
        }
        if ot < 0.0 {
            ot = -ot;
        }
    }
    if near || (crossing && t < 0.75 && ot < 0.75) {
        if other_speed <= speed && !near && c.h.max_velocity(o.obj) > 0.0 {
            if rd!(u, mVelocity) > 0.0 && rd!(o, mVelocity) > 0.0 && other_speed == speed {
                if c.h.waypoint_count(o.obj) > 0 {
                    c.h.path_length(o.obj, &mut other_left);
                }
                if c.h.waypoint_count(u.obj) > 0 {
                    c.h.path_length(u.obj, &mut left);
                }
                if left > other_left || (other_left == left && unit_id(o) < unit_id(u)) {
                    set_velocity(u, rd!(u, mVelocity) * 0.25);
                    wr!(u, mCollisionResolutionCount, 0.0);
                }
            }
            let dx = other_line.direction.x;
            let dx = c.h.max_velocity(o.obj) * (1.25 - t) * dx * other_sign;
            let dir_y = other_line.direction.y;
            let dy = c.h.max_velocity(o.obj) * (1.25 - t) * dir_y * other_sign;
            wr!(o, mPushVelocityX, rd!(o, mPushVelocityX) + dx);
            wr!(o, mPushVelocityY, rd!(o, mPushVelocityY) + dy);
        } else {
            if rd!(u, mVelocity) > 0.0 && near {
                if rd!(u, mCollisionState) == 2 {
                    give_up(c, u);
                    result = true;
                } else if rd!(u, mCollisionState) == 0 && rd!(o, mVelocity) == 0.0 {
                    c.h.set_blocking_unit(u.obj, o.obj);
                } else {
                    let v = rd!(u, mVelocity);
                    slow = (if v < t { t } else { v }) - 0.5;
                }
            }
            let speed_next = slow + 0.5;
            let dx = line.direction.x;
            let dx = c.h.max_velocity(u.obj) * (speed_next - t) * dx * sign;
            let dir_y = line.direction.y;
            let dy = c.h.max_velocity(u.obj) * (speed_next - t) * dir_y * sign;
            wr!(u, mPushVelocityX, rd!(u, mPushVelocityX) + dx);
            wr!(u, mPushVelocityY, rd!(u, mPushVelocityY) + dy);
            if slow > 0.0 {
                set_velocity(u, rd!(u, mVelocity) * 0.5);
            } else {
                set_velocity(u, 0.0);
                wr!(u, mCollisionResolutionCount, rd!(u, mCollisionResolutionCount) + c.t.tick_time);
            }
            if um_abs(rd!(o, mPushVelocityX)) < TURN_EPSILON && um_abs(rd!(o, mPushVelocityY)) < TURN_EPSILON {
                wr!(o, mPushVelocityX, TURN_EPSILON);
                wr!(o, mPushVelocityY, TURN_EPSILON);
            }
        }
    }
    result
}

/// A stuck unit gives up (`PushMoving`'s collision state 2): no blocker, stopped, physics action 6, its goals
/// cleared, then idle (or, a wood gatherer, looking for new wood) and `ActionChanged`.
pub(crate) fn give_up(c: &mut Ctx, u: EeUnitsPmUnit) {
    c.h.set_blocking_unit(u.obj, 0);
    wr!(u, mCollisionResolutionCount, 0.0);
    set_velocity(u, 0.0);
    if rd!(u, mCurrentPhysicsAction) != 6 {
        wr!(u, mCurrentPhysicsAction, 6);
    }
    c.h.clear_goal_queue(u.obj);
    if rd!(u, mCollisionState) != 0 {
        wr!(u, mCollisionState, 0);
    }
    let mut wood = false;
    if c.h.goal_kind(u.obj) == GOAL_GATHER {
        let resource = c.h.gather_resource(u.obj);
        wood = resource != 0 && c.h.resource_type(resource) == RESOURCE_WOOD;
    }
    c.h.start_action(u.obj, if wood { ACTION_FIND_NEW_RESOURCE } else { ACTION_IDLE as i32 });
    c.h.action_changed(u.obj);
}

/// `EEPhysicsModel::Push`: the other unit pushed off an overlapping one; the unit stopped at its last waypoint under
/// the other; a large enemy tramples a small one (world flag 2). Always false.
pub(crate) fn push(c: &mut Ctx, u: EeUnitsPmUnit, o: EeUnitsPmUnit) -> bool {
    let radius = rd!(u, mCollisionRadius);
    let other_radius = rd!(o, mCollisionRadius);
    let location = c.h.location(u.obj);
    let other_location = c.h.location(o.obj);
    let dx = location.x() - other_location.x();
    let dy = location.y() - other_location.y();
    let reach = (other_radius + radius) * 0.5;
    let reach_squared = reach * reach;
    let mut x_squared = dx * dx;
    let mut y_squared = dy * dy;
    if y_squared + x_squared > reach_squared {
        return false;
    }
    if x_squared == 0.0 && y_squared == 0.0 {
        x_squared = 0.1;
        y_squared = 0.1;
    }
    let distance_squared = y_squared + x_squared;
    let share = x_squared / distance_squared;
    let push = (reach_squared - distance_squared) * (radius / other_radius);
    let move_x = if dx < 0.0 { push * share } else { -(push * share) };
    let move_y = if dy < 0.0 { (1.0 - share) * push } else { -((1.0 - share) * push) };
    if c.h.waypoint_count(u.obj) == 1 {
        let next = c.h.path_start(u.obj);
        let other_location = c.h.location(o.obj);
        let other_2d = Ge2DPoint { x: other_location.x(), y: other_location.y() };
        let next_2d = Ge2DPoint { x: next.x, y: next.y };
        let half = other_radius * 0.5;
        if next_2d.distance_to_squared(other_2d) < half * half {
            set_velocity(u, 0.0);
            if rd!(u, mCollisionState) != 8 {
                wr!(u, mCollisionState, 8);
            }
        }
    }
    if !(other_radius > radius) && c.h.max_velocity(o.obj) > 0.0 {
        if other_radius == radius
            && c.h.class(u.obj).is_melee != 0
            && c.h.friendly(u.obj, o.obj)
            && rd!(u, mVelocity) > 0.0
            && rd!(o, mReloadTimer) != 0
            && c.h.goal_kind(o.obj) == GOAL_ATTACK_UNIT
            && c.h.goal_kind(u.obj) == GOAL_ATTACK_UNIT
        {
            let goal = c.h.goal_location(u.obj);
            let location = c.h.location(u.obj);
            let here = position(&location);
            if here.distance_to_squared(goal) < radius + radius + 4.0 {
                let other_location = c.h.location(o.obj);
                let there = position(&other_location);
                let mut to_other = GeVector { x: there.x - here.x, y: there.y - here.y, z: there.z - here.z };
                to_other.normalize();
                let mut ahead = forward(&location);
                ahead.normalize();
                let speed = rd!(u, mVelocity);
                ahead.x = speed * ahead.x;
                ahead.y = speed * ahead.y;
                ahead.z = speed * ahead.z;
                let dot = to_other.x * ahead.x + to_other.z * ahead.z + to_other.y * ahead.y;
                if dot > 0.0 {
                    let turn = rotation(um_arctan2(location.m[1][0], location.m[0][0]), 0.0, 0.0);
                    let velocity = turn.apply_vector(GeVector { x: 0.0, y: rd!(u, mVelocity), z: 0.0 });
                    wr!(u, mPushVelocityX, rd!(u, mPushVelocityX) - velocity.x);
                    wr!(u, mPushVelocityY, rd!(u, mPushVelocityY) - velocity.y);
                    let scale = dot / (to_other.x * to_other.x + to_other.z * to_other.z + to_other.y * to_other.y);
                    wr!(u, mPushVelocityX, rd!(u, mPushVelocityX) - to_other.x * scale);
                    wr!(u, mPushVelocityY, rd!(u, mPushVelocityY) - to_other.y * scale);
                }
            }
        }
        let mut other_move_x = move_x;
        let mut other_move_y = move_y;
        if c.h.goal_kind(u.obj) == GOAL_ATTACK_UNIT && o.obj == c.h.attack_target(u.obj) {
            other_move_x = move_x * 0.25;
            other_move_y = move_y * 0.25;
        }
        wr!(o, mPushVelocityX, rd!(o, mPushVelocityX) + other_move_x);
        wr!(o, mPushVelocityY, rd!(o, mPushVelocityY) + other_move_y);
    } else if um_abs(rd!(o, mPushVelocityX)) < TURN_EPSILON && um_abs(rd!(o, mPushVelocityY)) < TURN_EPSILON {
        wr!(o, mPushVelocityX, TURN_EPSILON);
        wr!(o, mPushVelocityY, TURN_EPSILON);
    }
    if rd!(o, mVelocity) == 0.0 && (other_radius > radius || c.h.max_velocity(o.obj) == 0.0) {
        wr!(u, mPushVelocityX, rd!(u, mPushVelocityX) - (move_x + move_x));
        wr!(u, mPushVelocityY, rd!(u, mPushVelocityY) - (move_y + move_y));
    }
    if (c.t.detection_flags & 2) == 0
        || !(rd!(u, mVelocity) > 0.0)
        || rd!(o, mVelocity) != 0.0
        || c.h.class(o.obj).tile_size != 1
        || !(other_radius < radius)
        || !(distance_squared > c.t.trample_min)
        || c.h.friendly(u.obj, o.obj)
    {
        return false;
    }
    c.h.trample(u.obj, o.obj);
    false
}

/// One of `ClipStep`'s edge planes: a location on or below it is cut back along the move (to the plane, at most the
/// whole move) and nudged inside, then, still on or below it, slid along the plane's normal and nudged again.
fn clip_edge(plane: &Ge3DPlane, move_line: &mut Ge3DLine, io: &mut Ge3DPoint, sx: f32, sy: f32, restart: bool) -> bool {
    if !plane.is_point_on_or_below(*io) {
        return false;
    }
    let mut t = match move_line.intersects_plane(plane) {
        Some(v) if !(v > 0.0 || v < -1.0) => v,
        _ => -1.0,
    };
    *io = move_line.point_at(t);
    io.x = io.x + sx * EDGE_OFFSET;
    io.y = io.y + sy * EDGE_OFFSET;
    if plane.is_point_on_or_below(*io) {
        let edge_line = Ge3DLine { start: *io, direction: plane.normal };
        // a parallel normal leaves the parameter as it was (the C++'s ignored result)
        if let Some(v) = edge_line.intersects_plane(plane) {
            t = v;
        }
        *io = edge_line.point_at(t);
        io.x = io.x + sx * EDGE_OFFSET;
        io.y = io.y + sy * EDGE_OFFSET;
    }
    if restart {
        move_line.start = *io;
    }
    true
}

/// `EEPhysicsModel::ClipStep`: the new location kept on the map (a move off the map undone; one through the
/// terrain's edge planes cut back and slid inside); false when the move was changed.
pub(crate) fn clip_step_base(c: &mut Ctx, mv: GeVector, io: &mut Ge3DPoint) -> bool {
    let x = floor(io.x);
    let y = floor(io.y);
    let mut result = true;
    if mv.x != 0.0 || mv.y != 0.0 || mv.z != 0.0 {
        let mut move_line = Ge3DLine { start: *io, direction: mv };
        if c.h.tile(x, y) != 0 {
            let planes = c.h.edge_planes();
            if c.h.tile(x - 1, y - 1) == 0 {
                if clip_edge(&planes[0], &mut move_line, io, 1.0, 1.0, true) {
                    result = false;
                }
            } else if c.h.tile(x + 1, y + 1) == 0 && clip_edge(&planes[1], &mut move_line, io, -1.0, -1.0, true) {
                result = false;
            }
            if c.h.tile(x - 1, y + 1) == 0 {
                if clip_edge(&planes[2], &mut move_line, io, 1.0, -1.0, false) {
                    result = false;
                }
            } else if c.h.tile(x + 1, y - 1) == 0 && clip_edge(&planes[3], &mut move_line, io, -1.0, 1.0, false) {
                result = false;
            }
        } else {
            *io = move_line.point_at(-1.0);
            result = false;
        }
    }
    result
}

// ---- the C++ entry points

/// The game's host (C++). Absent from a test binary.
#[cfg(not(test))]
fn with_game_host<R>(f: impl FnOnce(&mut dyn PmHost) -> R) -> R {
    let mut host = crate::units::pm_host::Game;
    f(&mut host)
}

/// `EEPhysicsModel::Step(unit)` of the kind's model.
///
/// # Safety
/// `unit`'s records point into a live complex unit; the call runs on the physics thread under the world lock.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_step(kind: i32, unit: EeUnitsPmUnit) {
    let Some(k) = Kind::from_index(kind) else { return };
    with_game_host(|h| {
        let t = h.tick();
        step(&mut Ctx { h, t }, k, unit);
    });
}

/// The kind's `Step0(unit)` alone (EE-SIM's missile sweep).
///
/// # Safety
/// As [`ee_units_pm_step`].
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_step0(kind: i32, unit: EeUnitsPmUnit) {
    let Some(k) = Kind::from_index(kind) else { return };
    with_game_host(|h| {
        let t = h.tick();
        step0(&mut Ctx { h, t }, k, unit);
    });
}

/// The kind's `ComputeRoll(unit, value)` (the airplane classes' death roll).
///
/// # Safety
/// As [`ee_units_pm_step`].
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_compute_roll(kind: i32, unit: EeUnitsPmUnit, value: f32) -> f32 {
    let Some(k) = Kind::from_index(kind) else { return 0.0 };
    with_game_host(|h| {
        let t = h.tick();
        compute_roll(&mut Ctx { h, t }, k, unit, value)
    })
}
