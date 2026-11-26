//! The kinds' hosts' plain statements over the world views (the units' lane, round 13): each `EE_UNITS_UCA_*`,
//! `_UCB_*`, `_UCG_*`, `_UCK_*`, `_UCP_*`, `_UCS_*` and `_UCX_*` statement that reads or stores one member of a
//! Rust record or layout entry, answered here before the kind's C++ host (EEUnitClassAir.cpp, Buildings, Ground,
//! Kinds, People, Shots, Transports) is asked; their cases left those switches. The kinds' operation values
//! overlap each other, so each kind has its own match, asked only by its own host ([`uc_view`]'s `kind`).
//!
//! As uc_view.rs: `class` the class, `unit` its unit, `a` an object (a unit, a class, a player, a goal, a tile or a
//! tile node), the C++ returns' conversions.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_view::{air_traffic, complex_mut, long, obj, stats_links, ulong, word, Ctx, Kind};
use crate::units::waypoints::{ee_units_waypoints_len, ee_units_waypoints_start, EeUnitsWaypoints};
use crate::world::unit_fields::EeUnitsUnitFields;
use crate::world::layout::layout;
use crate::world::view::{StatsView, UnitView};
use crate::world_field;
use std::os::raw::{c_long, c_ulong};

use crate::units::uc_anim::*;
use crate::units::uc_create::*;
use crate::units::uc_rest::*;
use crate::units::uca_air::*;
use crate::units::ucb_gate::*;
use crate::units::ucb_release::*;
use crate::units::ucg_ground::*;
use crate::units::ucg_sea::*;
use crate::units::uck_dispatch::*;
use crate::units::uck_kinds::*;
use crate::units::ucp_people::*;
use crate::units::ucs_impact::*;
use crate::units::ucs_shots::*;
use crate::units::ucx_transports::*;

/// The kind `kind`'s statement `op` when it is a plain one.
pub(crate) fn kind_view(kind: Kind, x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    match kind {
        Kind::Class => None,
        Kind::Air => air(x, op, f),
        Kind::Buildings => buildings(x, op, b, f),
        Kind::Ground => ground(x, op, b, c, f),
        Kind::Kinds => kinds(x, op, b).or_else(|| kinds_call(x, op, b, c, f)),
        Kind::People => people(x, op, f),
        Kind::Shots => shots(x, op, b, c, f),
        Kind::Transports => transports(x, op, f),
    }
}

/// `kUnitCategoryBomb`, `kUnitTypeCpMissileBase`, `kUnitTypeMissileBase` (rust/sim/unit_ids.h)
const SHOTS_CATEGORY_BOMB: u64 = 11;
const SHOTS_TYPE_CP_MISSILE_BASE: u64 = 749;
const SHOTS_TYPE_MISSILE_BASE: u64 = 67;

/// A goal's `mKind` (an `unsigned char`).
fn goal_kind(goal: u64) -> Option<i64> {
    if goal == 0 {
        return None;
    }
    // SAFETY: a live goal's kind byte
    Some(i64::from(unsafe { std::ptr::read((goal as usize + layout().goal_kind as usize) as *const u8) }))
}

/// `unit->mInitialContactLocation = (f[0], f[1], f[2])`.
fn set_contact(u: &UnitView<'_>, f: &[f32; 4]) {
    let at = u.handle() as usize + layout().complex_contact_location as usize;
    // SAFETY: a live complex unit's GE3DPoint member (three floats)
    unsafe { std::ptr::write(at as *mut [f32; 3], [f[0], f[1], f[2]]) }
}

/// `unit->mPlayer->mComputerPlayer`.
fn computer_player(x: &Ctx<'_>) -> Option<i64> {
    // SAFETY: the unit's player's pointer member
    Some(unsafe { word(x.u()?.player()?.handle(), layout().player_computer_player) } as i64)
}

fn air(x: &Ctx<'_>, op: i32, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        EE_UNITS_UCA_LOCATION => {
            f[..3].copy_from_slice(&u()?.location());
            0
        }
        EE_UNITS_UCA_GOAL_STATE => u()?.goal_handle() as i64,
        EE_UNITS_UCA_AIR_TRAFFIC_TYPE => long(world_field!(u()?.class()?.fields(), mAirTrafficType)),
        EE_UNITS_UCA_COMPLEX_OF => o()?.complex_unit_handle() as i64,
        EE_UNITS_UCA_TILE_SIZE_OF => o()?.class()?.tile_size(),
        EE_UNITS_UCA_TILE_X_OF => o()?.tile_xy().0,
        EE_UNITS_UCA_TILE_Y_OF => o()?.tile_xy().1,
        EE_UNITS_UCA_ATC => air_traffic(&u()?)? as i64,
        EE_UNITS_UCA_HOME_BASE => u()?.home_base_handle() as i64,
        EE_UNITS_UCA_CONTAINER => u()?.container_handle() as i64,
        EE_UNITS_UCA_PARATROOPER_BOMBER => i64::from(world_field!(x.cls()?.fields(), mIsParatrooperBomber)),
        EE_UNITS_UCA_HEIGHT => {
            f[0] = u()?.height();
            0
        }
        EE_UNITS_UCA_SET_HEIGHT_FIELD => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mHeight = f[0] };
            0
        }
        EE_UNITS_UCA_SET_CONTACT_FIELD => {
            set_contact(&u()?, f);
            0
        }
        EE_UNITS_UCA_CONTACT_HEADING => {
            f[0] = world_field!(u()?.complex(), mInitialContactHeading);
            0
        }
        EE_UNITS_UCA_SET_CONTACT_HEADING => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mInitialContactHeading = f[0] };
            0
        }
        EE_UNITS_UCA_GOAL_KIND => goal_kind(x.a)?,
        EE_UNITS_UCA_VELOCITY_OF => {
            f[0] = world_field!(o()?.complex(), mVelocity);
            0
        }
        EE_UNITS_UCA_GRAVITY => {
            // SAFETY: the class's projectile class
            let p = unsafe { crate::world::view::ClassView::of(x.t, x.cls()?.projectile_class_handle()) }?;
            f[0] = world_field!(p.fields(), mAcceleration);
            0
        }
        EE_UNITS_UCA_AMMUNITION => ulong(world_field!(u()?.complex(), mAmmunition)),
        EE_UNITS_UCA_PLAYER_ID => i64::from(u()?.player()?.id()),
        EE_UNITS_UCA_CARGO_COUNT => u()?.cargo_len() as c_ulong as i64,
        EE_UNITS_UCA_CAPACITY => long(world_field!(u()?.class()?.fields(), mTransportCapacity)),
        EE_UNITS_UCA_CRUISE_FIELD => {
            f[0] = world_field!(x.cls()?.fields(), mCruiseAltitude);
            0
        }
        EE_UNITS_UCA_OPACITY_OF => {
            f[0] = world_field!(o()?.complex(), mOpacity);
            0
        }
        EE_UNITS_UCA_SET_OPACITY_OF => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&o()?)).mOpacity = f[0] };
            0
        }
        EE_UNITS_UCA_NEXT_PART_OF => o()?.next_part_handle() as i64,
        // SAFETY: the unit's home base
        EE_UNITS_UCA_HOME_TILE_X => unsafe { UnitView::of(x.t, u()?.home_base_handle()) }?.tile_xy().0,
        // SAFETY: as above
        EE_UNITS_UCA_HOME_TILE_Y => unsafe { UnitView::of(x.t, u()?.home_base_handle()) }?.tile_xy().1,
        _ => return None,
    })
}

fn buildings(x: &Ctx<'_>, op: i32, b: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o, cls, oc) = (|| x.u(), || x.o(), || x.cls(), || x.oc());
    Some(match op {
        EE_UNITS_UCB_UNIT_CLASS_OF => o()?.class().map_or(0, |k| k.handle() as i64),
        EE_UNITS_UCB_CLASS_IS_WALL => i64::from(oc()?.wall()),
        EE_UNITS_UCB_CLASS_TYPE => long(oc()?.unit_type() as c_ulong as c_long),
        EE_UNITS_UCB_PLAYER_AT => x.world()?.player(b as usize).map_or(0, |p| p.handle() as i64),
        EE_UNITS_UCB_HIT_RATIO => {
            let o = o()?;
            f[0] = o.hit_points() as c_long as f32 / o.stats()?.max_hit_points() as c_long as f32;
            0
        }
        EE_UNITS_UCB_PLAYER_ID | EE_UNITS_UCB_PLAYER_ID_FIELD => i64::from(x.op()?.id()),
        EE_UNITS_UCB_COMMAND_TYPE_OF => i64::from(o()?.complex_unit()?.command_type()),
        EE_UNITS_UCB_GATE_DIRECTION_OF => ulong(world_field!(o()?.complex_unit()?.complex(), mGateDirection)),
        EE_UNITS_UCB_UNIT_PLAYER_ID => i64::from(o()?.player()?.id()),
        EE_UNITS_UCB_COMPLEX_OF | EE_UNITS_UCB_COMPLEX_FIELD | EE_UNITS_UCB_COMPLEX_FIELD_OF => {
            o()?.complex_unit_handle() as i64
        }
        EE_UNITS_UCB_BUILT_ALIVE => {
            let o = o()?;
            i64::from(o.complex_unit()?.is_built() && o.hit_points() > 0)
        }
        EE_UNITS_UCB_IS_TINTED => i64::from(world_field!(o()?.complex(), mIsTinted)),
        // SAFETY: a live tile's node list
        EE_UNITS_UCB_TILE_NODES | EE_UNITS_UCB_TILE_UNITS => unsafe { word(x.a, layout().tile_units) as i64 },
        // SAFETY: a live tile node's unit
        EE_UNITS_UCB_GATE_NODE_UNIT | EE_UNITS_UCB_NODE_UNIT => unsafe { word(x.a, layout().unit_node_unit) as i64 },
        // SAFETY: a live tile node's next node
        EE_UNITS_UCB_GATE_NODE_NEXT | EE_UNITS_UCB_NODE_NEXT => unsafe { word(x.a, layout().unit_node_next) as i64 },
        EE_UNITS_UCB_TINTED_WALL => {
            let o = o()?;
            i64::from(
                o.class()?.wall()
                    && o.hit_points() > 0
                    && o.has_tile_node()
                    && world_field!(o.complex_unit()?.complex(), mIsTinted),
            )
        }
        EE_UNITS_UCB_UNIT_TILE_X => o()?.tile_xy().0,
        EE_UNITS_UCB_UNIT_TILE_Y => o()?.tile_xy().1,
        EE_UNITS_UCB_PREVIOUS_PART | EE_UNITS_UCB_PREVIOUS_PART_OF => o()?.previous_part_handle() as i64,
        EE_UNITS_UCB_NEXT_PART_OF => o()?.next_part_handle() as i64,
        EE_UNITS_UCB_COLLISION_STATE => i64::from(world_field!(u()?.complex(), mCollisionState)),
        EE_UNITS_UCB_SELF_TILE_X | EE_UNITS_UCB_TILE_X_FIELD => u()?.tile_xy().0,
        EE_UNITS_UCB_SELF_TILE_Y => u()?.tile_xy().1,
        EE_UNITS_UCB_CHAIN_HEAD => {
            let u = u()?;
            i64::from(u.previous_part_handle() == 0 && u.next_part_handle() != 0)
        }
        EE_UNITS_UCB_CARGO_SIZE => u()?.cargo_len() as i64,
        EE_UNITS_UCB_CARGO_INDEX => u()?.cargo_handle(b as usize) as i64,
        EE_UNITS_UCB_HIT_POINTS_OF | EE_UNITS_UCB_HIT_POINTS_FIELD_OF | EE_UNITS_UCB_UNIT_HIT_POINTS => {
            o()?.hit_points()
        }
        EE_UNITS_UCB_MAX_HIT_POINTS_OF | EE_UNITS_UCB_MAX_HIT_POINTS_FIELD_OF => o()?.stats()?.max_hit_points(),
        EE_UNITS_UCB_GOAL_STATE_OF | EE_UNITS_UCB_GOAL_OF => o()?.goal_handle() as i64,
        EE_UNITS_UCB_UNIT_ID_OF => i64::from(o()?.id()),
        EE_UNITS_UCB_DETACHED => i64::from(world_field!(o()?.complex(), mIsDetachedFromAI)),
        EE_UNITS_UCB_COMPUTER_PLAYER => computer_player(x)?,
        EE_UNITS_UCB_EXITING_ID => long(world_field!(u()?.complex(), mExitingUnitID)),
        EE_UNITS_UCB_AIR_TRAFFIC_SELF => long(world_field!(u()?.class()?.fields(), mAirTrafficType)),
        EE_UNITS_UCB_TILE_SIZE_FIELD => cls()?.tile_size(),
        EE_UNITS_UCB_FACING => u()?.facing() as i64,
        EE_UNITS_UCB_CATEGORY_SELF => u()?.class()?.category() as i64,
        EE_UNITS_UCB_UNIT_TYPE_SELF => u()?.class()?.unit_type() as i64,
        EE_UNITS_UCB_CATEGORY_OF => o()?.class()?.category() as i64,
        EE_UNITS_UCB_FAMILY_OF => o()?.class()?.family() as i64,
        EE_UNITS_UCB_PARATROOPER_OF => i64::from(world_field!(o()?.class()?.fields(), mIsParatrooperBomber)),
        EE_UNITS_UCB_UNIT_TYPE_OF => o()?.class()?.unit_type() as i64,
        EE_UNITS_UCB_RESOURCE_TYPE => u()?.resource_type() as i64,
        EE_UNITS_UCB_DISABLED => i64::from(world_field!(u()?.complex(), mIsDisabled)),
        EE_UNITS_UCB_PHYSICS_ACTION => ulong(world_field!(u()?.complex(), mCurrentPhysicsAction)),
        EE_UNITS_UCB_IS_MEDIC => i64::from(world_field!(cls()?.fields(), mIsMedic)),
        EE_UNITS_UCB_REGENERATION => long(world_field!(u()?.stats()?.fields(), mRegeneration)),
        EE_UNITS_UCB_CONVERSION_START => ulong(world_field!(u()?.complex(), mConversionStartTime)),
        EE_UNITS_UCB_CLEAR_CONVERSION_START => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mConversionStartTime = 0 };
            0
        }
        EE_UNITS_UCB_CRUISE_ALTITUDE => {
            f[0] = world_field!(cls()?.fields(), mCruiseAltitude);
            0
        }
        EE_UNITS_UCB_TILE_SIZE_OF => o()?.class()?.tile_size(),
        _ => return None,
    })
}

fn ground(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        EE_UNITS_UCG_TILE_X_OF => o()?.tile_xy().0,
        EE_UNITS_UCG_TILE_Y_OF => o()?.tile_xy().1,
        EE_UNITS_UCG_QUEUED_FOR_DELETE => i64::from(o()?.queued_for_delete()),
        EE_UNITS_UCG_RESOURCE_TYPE => o()?.resource_type() as i64,
        EE_UNITS_UCG_UNIT_TYPE => u()?.class()?.unit_type() as i64,
        EE_UNITS_UCG_CRUISE_ALTITUDE_OF => {
            f[0] = world_field!(x.oc()?.fields(), mCruiseAltitude);
            0
        }
        EE_UNITS_UCG_COLLISION_STATE => i64::from(world_field!(u()?.complex(), mCollisionState)),
        EE_UNITS_UCG_COLLISION_RADIUS => {
            f[0] = world_field!(u()?.complex(), mCollisionRadius);
            0
        }
        EE_UNITS_UCG_WHEEL_COUNT => ulong(world_field!(u()?.complex(), mNumWheels)),
        // `theSimple->SetResourceType(theClass->mResourceType)`, `SetResourceAmount(theClass->mResourceAmount)`
        EE_UNITS_UCG_SET_RESOURCE_TYPE => {
            let v: c_long = world_field!(x.cls()?.fields(), mResourceType);
            // SAFETY: the simple unit's own record, written on the simulation thread
            unsafe { (*(o()?.fields().ptr() as *mut EeUnitsUnitFields)).mResourceType = v as c_ulong };
            0
        }
        EE_UNITS_UCG_SET_RESOURCE_AMOUNT => {
            let v: c_long = world_field!(x.cls()?.fields(), mResourceAmount);
            // SAFETY: as above
            unsafe { (*(o()?.fields().ptr() as *mut EeUnitsUnitFields)).mResourceAmount = v };
            0
        }
        // `theUnit->SetPower(theUnit->mUnitClass->mPower)`
        EE_UNITS_UCG_SET_CLASS_POWER => {
            let v: c_long = world_field!(u()?.class()?.fields(), mPower);
            // SAFETY: the unit's own record
            unsafe { (*complex_mut(&u()?)).mPower = v };
            0
        }
        // `theOther->SetFlightTime((unsigned long)inB)` (a long member)
        EE_UNITS_UCG_SET_FLIGHT_TIME_OF => {
            // SAFETY: the other unit's own record
            unsafe { (*complex_mut(&o()?)).mFlightTime = b as c_ulong as c_long };
            0
        }
        // `gWorld.GetMap()->GetTile(inB, inC)`
        EE_UNITS_UCG_MAP_TILE => x.world()?.map()?.tile(b as c_long as i64, c as c_long as i64).map_or(0, |t| t.handle() as i64),
        // `theUnit->mWaypoints.size()`, `GetNextWaypointPoint(f)` (the Rust list behind the member)
        EE_UNITS_UCG_WAYPOINT_COUNT => {
            // SAFETY: the unit's waypoint list handle (its member's first word, never null)
            let list = unsafe { word(u()?.handle(), layout().complex_waypoints) } as usize as *const EeUnitsWaypoints;
            // SAFETY: the live list
            unsafe { ee_units_waypoints_len(list) as i64 }
        }
        EE_UNITS_UCG_NEXT_WAYPOINT => {
            // SAFETY: as above
            let list = unsafe { word(u()?.handle(), layout().complex_waypoints) } as usize as *const EeUnitsWaypoints;
            // SAFETY: the live list; three floats out
            unsafe { ee_units_waypoints_start(list, f.as_mut_ptr(), std::ptr::null_mut()) };
            0
        }
        _ => return None,
    })
}

fn kinds(x: &Ctx<'_>, op: i32, b: i64) -> Option<i64> {
    Some(match op {
        EE_UNITS_UCK_D_DOMAINS => i64::from(world_field!(x.cls()?.fields(), mDomains)),
        EE_UNITS_UCK_D_IS_FLARE => i64::from(world_field!(x.cls()?.fields(), mIsFlare)),
        EE_UNITS_UCK_D_UNIT_TYPE_OF => x.o()?.class()?.unit_type() as i64,
        // `EEUnitClassKinds(theClass)`, `EEClassBody(theClass, inB)` (the category's rows of kinds.rs, cls_dispatch.rs)
        EE_UNITS_UCK_KINDS => {
            let category = x.cls()?.category();
            if category < 256 {
                crate::units::kinds::EE_UNIT_CLASS_KINDS[category as usize] as i64
            } else {
                0
            }
        }
        EE_UNITS_UCK_BODY => {
            let category = x.cls()?.category();
            if category < 256 {
                i64::from(crate::units::cls_dispatch::EE_UNIT_CLASS_BODIES[b as usize][category as usize])
            } else {
                0
            }
        }
        // `!theUnit->IsFoundation() || theUnit->mHitPoints <= 0 || theUnit->mCurrentPhysicsAction == 2`
        EE_UNITS_UCK_PRIEST_UPDATES => {
            let u = x.u()?;
            let foundation: bool = world_field!(u.complex(), mIsFoundation);
            let action: c_ulong = world_field!(u.complex(), mCurrentPhysicsAction);
            i64::from(!foundation || u.hit_points() <= 0 || action == 2)
        }
        _ => return None,
    })
}

fn people(x: &Ctx<'_>, op: i32, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        EE_UNITS_UCP_WORK_TIMER => long(world_field!(u()?.complex(), mWorkTimer)),
        EE_UNITS_UCP_CURRENT_RESEARCH => long(world_field!(u()?.complex(), mCurrentResearch)),
        EE_UNITS_UCP_POWER => u()?.power(),
        EE_UNITS_UCP_ENEMY_STATE => {
            let other = o()?.player()?.id() as usize;
            u()?.player()?.enemy(other) as i64
        }
        EE_UNITS_UCP_RESISTANCE => {
            f[0] = world_field!(o()?.stats()?.fields(), mConversionResistance);
            0
        }
        EE_UNITS_UCP_UNIT_CATEGORY => u()?.class()?.category() as i64,
        EE_UNITS_UCP_TYPE_OF => o()?.class()?.unit_type() as i64,
        EE_UNITS_UCP_CONVERSION_RATE => {
            f[0] = world_field!(u()?.stats()?.fields(), mConversionRate);
            0
        }
        EE_UNITS_UCP_CONVERSION_FACTOR => {
            f[0] = world_field!(o()?.player()?.fields(), mConversionFactor);
            0
        }
        EE_UNITS_UCP_DISABLED_OF => i64::from(world_field!(o()?.complex_unit()?.complex(), mIsDisabled)),
        EE_UNITS_UCP_PLAYER_OF => o()?.player().map_or(0, |p| p.handle() as i64),
        EE_UNITS_UCP_STATS_OF => o()?.stats().map_or(0, |s| s.handle() as i64),
        EE_UNITS_UCP_CONVERSION_START => ulong(world_field!(u()?.complex(), mConversionStartTime)),
        EE_UNITS_UCP_CLEAR_CONVERSION_START => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mConversionStartTime = 0 };
            0
        }
        EE_UNITS_UCP_STATS_CREATION_SOUND => {
            // SAFETY: the live stats the body handed over
            let s = unsafe { StatsView::of(x.t, x.a) }?;
            obj(world_field!(stats_links(&s), mCreationSound))
        }
        EE_UNITS_UCP_CLASS_CATEGORY => x.cls()?.category() as i64,
        EE_UNITS_UCP_CLOAKED => ulong(world_field!(u()?.complex(), mCloaked)),
        EE_UNITS_UCP_CLEAR_CLOAKED => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCloaked = 0 };
            0
        }
        EE_UNITS_UCP_TICK_TIME => {
            f[0] = world_field!(x.world()?.physics()?, mTickTime);
            0
        }
        _ => return None,
    })
}

fn shots(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        EE_UNITS_UCS_GOAL_STATE => u()?.goal_handle() as i64,
        EE_UNITS_UCS_IMPACT_FLIGHT_TIME | EE_UNITS_UCS_FLIGHT_TIME => long(world_field!(u()?.complex(), mFlightTime)),
        EE_UNITS_UCS_GOAL_KIND => goal_kind(x.a)?,
        EE_UNITS_UCS_IMPACT_TILE_X | EE_UNITS_UCS_TILE_X_OF => o()?.tile_xy().0,
        EE_UNITS_UCS_IMPACT_TILE_Y | EE_UNITS_UCS_TILE_Y_OF => o()?.tile_xy().1,
        EE_UNITS_UCS_IMPACT_TILE_SIZE | EE_UNITS_UCS_TILE_SIZE_OF => o()?.class()?.tile_size(),
        EE_UNITS_UCS_SET_CONTACT_LOCATION => {
            set_contact(&u()?, f);
            0
        }
        EE_UNITS_UCS_UNIT_TYPE => u()?.class()?.unit_type() as i64,
        EE_UNITS_UCS_LAUNCH_PITCH => {
            // SAFETY: the unit's launcher class
            let l = unsafe { crate::world::view::ClassView::of(x.t, u()?.launcher_class_handle()) }?;
            f[0] = world_field!(l.fields(), mLaunchPitch);
            0
        }
        EE_UNITS_UCS_FIRST_CARGO => u()?.cargo_handle(0) as i64,
        EE_UNITS_UCS_COMPLEX_FIELD => o()?.complex_unit_handle() as i64,
        EE_UNITS_UCS_TILE_OF => o()?.tile().map_or(0, |t| t.handle() as i64),
        EE_UNITS_UCS_PREVIOUS_PART => o()?.previous_part_handle() as i64,
        EE_UNITS_UCS_NEXT_PART => o()?.next_part_handle() as i64,
        EE_UNITS_UCS_SET_LIFETIME => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCloaked = 300000 };
            0
        }
        // units round 14: the shots' host's other plain statements
        EE_UNITS_UCS_IS_BOMB => i64::from(x.cls()?.category() == SHOTS_CATEGORY_BOMB),
        EE_UNITS_UCS_LOCATION => {
            let at = u()?.location();
            f[..3].copy_from_slice(&at);
            0
        }
        EE_UNITS_UCS_LAUNCHER_CP_BASE | EE_UNITS_UCS_LAUNCHER_MISSILE_BASE => {
            // SAFETY: the unit's launcher class (set for every shot)
            let l = unsafe { crate::world::view::ClassView::of(x.t, u()?.launcher_class_handle()) }?;
            let ty = if op == EE_UNITS_UCS_LAUNCHER_CP_BASE { SHOTS_TYPE_CP_MISSILE_BASE } else { SHOTS_TYPE_MISSILE_BASE };
            i64::from(l.unit_type() == ty)
        }
        EE_UNITS_UCS_SET_LAUNCH_DELAY => {
            // SAFETY: as above
            let l = unsafe { crate::world::view::ClassView::of(x.t, u()?.launcher_class_handle()) }?;
            let delay: c_long = world_field!(l.fields(), mMissileDelayTime);
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mFlightTime = delay };
            0
        }
        EE_UNITS_UCS_SET_FLIGHT_TIME => {
            // SAFETY: the unit's member (`SetFlightTime((unsigned long)inB)`, a long)
            unsafe { (*complex_mut(&u()?)).mFlightTime = b as c_ulong as c_long };
            0
        }
        EE_UNITS_UCS_TILE_HEIGHT => {
            f[0] = u()?.tile()?.height();
            0
        }
        EE_UNITS_UCS_TILE_PRESENT => i64::from(x.world()?.map()?.tile(b as c_long as i64, c as c_long as i64).is_some()),
        EE_UNITS_UCS_SAME_TILE => {
            let (tx, ty) = (ee_base::random::float_to_long(f[0]), ee_base::random::float_to_long(f[1]));
            let tile = x.world()?.map()?.tile(i64::from(tx), i64::from(ty)).map_or(0, |t| t.handle());
            i64::from(tile == u()?.tile().map_or(0, |t| t.handle()))
        }
        EE_UNITS_UCS_SHELL_HOLE => {
            i64::from(!world_field!(crate::units::uc_view::links(&u()?.class()?), mShellHoleEffect).is_null())
        }
        EE_UNITS_UCS_HAS_TRAIL => i64::from(!world_field!(crate::units::uc_view::links(&x.cls()?), mTrailEffect).is_null()),
        EE_UNITS_UCS_HAS_RIPPLE => {
            i64::from(!world_field!(crate::units::uc_view::links(&x.cls()?), mRippleEffect).is_null())
        }
        // `EEUnitCast<EEComplexUnit>(a)`
        EE_UNITS_UCS_COMPLEX_CAST => {
            // SAFETY: the unit (or 0) the body handed over
            unsafe { UnitView::of(x.t, x.a) }.filter(|v| v.is_complex()).map_or(0, |v| v.handle() as i64)
        }
        _ => return None,
    })
}

fn transports(x: &Ctx<'_>, op: i32, f: &mut [f32; 4]) -> Option<i64> {
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        EE_UNITS_UCX_DOMAINS_OF => i64::from(world_field!(o()?.class()?.fields(), mDomains)),
        EE_UNITS_UCX_MAX_VELOCITY_OF => {
            f[0] = world_field!(o()?.stats()?.fields(), mMaxVelocity);
            0
        }
        EE_UNITS_UCX_TILE_SIZE_OF => o()?.class()?.tile_size(),
        EE_UNITS_UCX_RESOURCE_TYPE => o()?.resource_type() as i64,
        EE_UNITS_UCX_CARGO_COUNT_OF => o()?.cargo_len() as c_ulong as i64,
        EE_UNITS_UCX_TILE_X_OF => o()?.tile_xy().0,
        EE_UNITS_UCX_TILE_Y_OF => o()?.tile_xy().1,
        EE_UNITS_UCX_TYPE_OF => o()?.class()?.unit_type() as i64,
        EE_UNITS_UCX_POWER => u()?.power(),
        EE_UNITS_UCX_CRUISE_ALTITUDE => {
            f[0] = world_field!(x.cls()?.fields(), mCruiseAltitude);
            0
        }
        EE_UNITS_UCX_TRANSPORT_CAPACITY => long(world_field!(u()?.class()?.fields(), mTransportCapacity)),
        _ => return None,
    })
}

/// `unit->mPlayer->mPopulationCount -=` (or `+=`) `a->mUnitClass->GetPopulationCost()`.
fn population(x: &Ctx<'_>, add: bool) -> Option<()> {
    let cost: c_long = world_field!(x.o()?.class()?.fields(), mWeight);
    let p = x.u()?.player()?.saved().ptr() as *mut crate::world::player_fields::EeUnitsPlayerSaved;
    // SAFETY: the unit's player's member
    unsafe {
        if add {
            (*p).mPopulationCount += cost;
        } else {
            (*p).mPopulationCount -= cost;
        }
    }
    Some(())
}

/// The kinds' host's statements that were a call of an `EEUnitClass` member whose inline body is one call of a
/// Rust entry point (ee/EEUnit.h): the entry point called here with the arguments the member passed on (a `long`
/// truncated, a bool 0 or 1, the location's and the transformation's floats in place), and the population count
/// stores of `POPULATION_SUB` and `_ADD`.
fn kinds_call(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    use crate::units::cls_host::{
        ee_units_cls_condition, ee_units_cls_landing_has_arrived, ee_units_tr_naval_has_arrived,
    };
    use crate::units::cls_move_host::ee_units_mv_has_arrived;
    use crate::units::uc_create::game::ee_units_uc_call;
    use crate::units::uc_rest::game::ee_units_uc_rest;
    use crate::units::uca_air::game::ee_units_uca_call;
    use crate::units::ucb_gate::game::ee_units_ucb_gate;
    use crate::units::ucb_release::game::ee_units_ucb_call;
    use crate::units::ucg_ground::game::ee_units_ucg_call;
    use crate::units::ucg_sea::game::ee_units_ucg_sea;
    use crate::units::ucp_people::game::ee_units_ucp_call;
    use crate::units::ucs_impact::game::ee_units_ucs_impact;
    use crate::units::ucs_shots::game::ee_units_ucs_call;
    use crate::units::ucx_transports::game::ee_units_ucx_call;
    use std::os::raw::c_void;
    use std::ptr::{null, null_mut};
    let (class, unit, a) = (x.class, x.unit, x.a);
    let (k, u, n) = (class as usize as *mut c_void, unit as usize as *mut c_void, null_mut::<c_void>());
    // `(long)inB`, `(long)inC`, `inC != 0`, `(long)(inC >> 1)` and `inC & 1`, as the members took them
    let (lb, lc, bc) = (b as c_long as i64, c as c_long as i64, i64::from(c != 0));
    let (player, found) = ((c >> 1) as c_long as i64, c & 1);
    let m = b as usize as *const [[f32; 4]; 4];
    let mut value = [f[0], 0.0, 0.0, 0.0];
    let flag = |v: i64| i64::from(v != 0);
    let p = |h: u64| h as usize as *mut c_void;
    // SAFETY: the Rust entry points the C++ members called, with the live class, unit and objects the body handed
    // over, as the members passed them on
    unsafe {
        match op {
            EE_UNITS_UCK_BOMB_UPDATE => {
                ee_units_ucs_call(EE_UNITS_UCS_DO_BOMB_UPDATE, k, u, 0, null());
            }
            EE_UNITS_UCK_PRIEST_UPDATE => {
                ee_units_ucb_call(EE_UNITS_UCB_DO_PRIEST_UPDATE, k, u, 0, 0, 0, null());
            }
            EE_UNITS_UCK_CITIZEN_UPDATE => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_UPDATE_CITIZEN, k, u, 0, 0);
            }
            EE_UNITS_UCK_UPDATE_RIPPLES => {
                ee_units_uc_call(EE_UNITS_UC_DO_UPDATE_RIPPLES, k, u, 0, 0, 0, null(), null());
            }
            EE_UNITS_UCK_UPDATE_TRAIL => {
                ee_units_uc_call(EE_UNITS_UC_DO_UPDATE_TRAIL, k, u, 0, 0, 0, null(), null());
            }
            EE_UNITS_UCK_PROJECTILE_EFFECTS => {
                ee_units_ucs_call(EE_UNITS_UCS_DO_PROJECTILE_EFFECTS, k, u, 0, null());
            }
            EE_UNITS_UCK_CONDITION => return Some(i64::from(ee_units_cls_condition(class, unit, b as u32))),
            EE_UNITS_UCK_LANDING_ARRIVED => {
                return Some(i64::from(ee_units_cls_landing_has_arrived(class, unit, f[0], b != 0)))
            }
            EE_UNITS_UCK_SPACE_BUILDING_ARRIVED => {
                return Some(flag(ee_units_ucb_gate(EE_UNITS_UCB_DO_SPACE_ARRIVED, k, u, 0, 0, 0, f[0])))
            }
            EE_UNITS_UCK_NAVAL_TRANSPORT_ARRIVED => return Some(i64::from(ee_units_tr_naval_has_arrived(unit, f[0]))),
            EE_UNITS_UCK_NAVAL_ARRIVED => {
                return Some(flag(ee_units_ucg_call(EE_UNITS_UCG_DO_NAVAL_ARRIVED, k, u, 0, f[0])))
            }
            EE_UNITS_UCK_SUBMERGED_ARRIVED => {
                let v = ee_units_ucg_sea(EE_UNITS_UCG_DO_SUBMERGED_HAS_ARRIVED, k, u, 0, 0, 0, value.as_mut_ptr());
                return Some(flag(v));
            }
            EE_UNITS_UCK_BASE_ARRIVED => return Some(i64::from(ee_units_mv_has_arrived(class, unit, f[0]) != 0)),
            EE_UNITS_UCK_FORTRESS_HAS_ROOM => {
                return Some(flag(ee_units_ucx_call(EE_UNITS_UCX_DO_FORTRESS_HAS_ROOM, k, u, a, 0, 0)))
            }
            EE_UNITS_UCK_NAVAL_SELECT => {
                return Some(flag(ee_units_ucb_call(EE_UNITS_UCB_DO_NAVAL_SELECT_TO_RELEASE, k, u, 0, 0, 0, null())))
            }
            EE_UNITS_UCK_BUILDING_SELECT => {
                return Some(flag(ee_units_ucb_call(EE_UNITS_UCB_DO_SELECT_TO_RELEASE, k, u, 0, 0, 0, null())))
            }
            EE_UNITS_UCK_GATE_SURROUNDING => {
                ee_units_ucb_gate(EE_UNITS_UCB_DO_GATE_SURROUNDING, k, u, a, lb, bc, 0.0);
            }
            EE_UNITS_UCK_BUILDING_SURROUNDING => {
                ee_units_ucb_call(EE_UNITS_UCB_DO_SURROUNDING_TILES, k, u, a, lb, bc, null());
            }
            EE_UNITS_UCK_BASE_SURROUNDING => {
                ee_units_uc_rest(EE_UNITS_UC_DO_R_BASE_SURROUNDING, k, u, a, lb, bc, null_mut());
            }
            EE_UNITS_UCK_D_BASE_ADD_TO_REGION => {
                ee_units_uc_call(EE_UNITS_UC_DO_ADD_TO_REGION, k, n, a, 0, 0, null(), null());
            }
            EE_UNITS_UCK_D_SPACE_TRANSPORT_ADD_TO_TILE => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_SPACE_ADD, k, n, a, 0, 0);
            }
            EE_UNITS_UCK_D_AMBIENT_ADD_TO_TILE => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_AMBIENT_ADD, k, n, a, 0.0);
            }
            EE_UNITS_UCK_D_RESOURCE_ADD_TO_TILE => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_RESOURCE_ADD, k, n, a, 0.0);
            }
            EE_UNITS_UCK_D_SUBMERGED_ADD_TO_TILE => {
                ee_units_ucg_sea(EE_UNITS_UCG_DO_SUBMERGED_ADD_TO_TILE, k, n, a, 0, 0, null_mut());
            }
            EE_UNITS_UCK_D_LAND_TRANSPORT_BADLY_HURT => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_BADLY_HURT, k, n, a, 0, 0);
            }
            EE_UNITS_UCK_D_LAND_CALL_PARATROOP_PLANE => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_CALL_PARATROOP_PLANE, k, u, 0, 0.0);
            }
            EE_UNITS_UCK_D_POPULATION_SUB | EE_UNITS_UCK_D_POPULATION_ADD => {
                population(x, op == EE_UNITS_UCK_D_POPULATION_ADD)?
            }
            EE_UNITS_UCK_BOARDING_LOCATION => {
                let (at, out) = (b as usize as *const [f32; 3], c as usize as *mut [f32; 3]);
                return Some(i64::from(crate::units::ucx_transports::game::ee_units_ucx_boarding(k, u, p(a), at, out)));
            }
            EE_UNITS_UCK_D_BASE_SIMPLE_CREATED => {
                let max = x.o()?.stats()?.max_hit_points();
                crate::units::eu_body::game::ee_units_eu_body(
                    crate::units::eu_body::EE_UNITS_EU_DO_SET_HIT_POINTS,
                    p(a),
                    max,
                    0,
                    0,
                );
            }
            EE_UNITS_UCK_D_TORPEDO_GROUND_IMPACT => {
                return Some(i64::from(ee_units_ucs_impact(EE_UNITS_UCS_DO_TORPEDO_GROUND, k, u)))
            }
            EE_UNITS_UCK_D_MISSILE_GROUND_IMPACT => {
                return Some(i64::from(ee_units_ucs_impact(EE_UNITS_UCS_DO_MISSILE_GROUND, k, u)))
            }
            EE_UNITS_UCK_D_PROJECTILE_GROUND_IMPACT => {
                return Some(i64::from(ee_units_ucs_impact(EE_UNITS_UCS_DO_PROJECTILE_GROUND, k, u)))
            }
            EE_UNITS_UCK_D_MISSILE_WATER_IMPACT => {
                return Some(i64::from(ee_units_ucs_impact(EE_UNITS_UCS_DO_MISSILE_WATER, k, u)))
            }
            EE_UNITS_UCK_D_PROJECTILE_WATER_IMPACT => {
                return Some(i64::from(ee_units_ucs_impact(EE_UNITS_UCS_DO_PROJECTILE_WATER, k, u)))
            }
            // the creations (b the tile, x | y << 32; c the player * 2 + the foundation flag): the unit made
            EE_UNITS_UCK_D_MINING_CREATE_UNIT => {
                return Some(ee_units_ucb_call(
                    EE_UNITS_UCB_DO_MINING_CREATE_UNIT,
                    k,
                    n,
                    a,
                    b,
                    player * 2 + found,
                    null(),
                ))
            }
            EE_UNITS_UCK_D_NAVAL_CITIZEN_CREATE_UNIT => {
                let w = EE_UNITS_UCG_DO_NAVAL_CITIZEN_CREATE_UNIT;
                return Some(ee_units_ucg_sea(w, k, n, a, b, player * 2 + found, null_mut()));
            }
            EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT => {
                return Some(ee_units_ucb_call(
                    EE_UNITS_UCB_DO_SPACE_CREATE_UNIT,
                    k,
                    n,
                    a,
                    b,
                    player * 2 + found,
                    null(),
                ))
            }
            EE_UNITS_UCK_D_BUILDING_CREATE_UNIT => {
                return Some(ee_units_ucb_call(EE_UNITS_UCB_DO_CREATE_UNIT, k, n, a, b, player * 2 + found, null()))
            }
            EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT => {
                let w = EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT;
                return Some(ee_units_ucg_sea(w, k, n, a, b, player * 2 + found, null_mut()));
            }
            EE_UNITS_UCK_D_BASE_CREATE_UNIT => {
                let w = EE_UNITS_UC_DO_BASE_CREATE_UNIT;
                return Some(ee_units_uc_call(w, k, n, a, b, player * 2 + found, null(), null()));
            }
            // b the transformation
            EE_UNITS_UCK_D_MINING_CREATE_UNIT_AT => {
                return Some(ee_units_ucb_call(EE_UNITS_UCB_DO_MINING_CREATE_UNIT_AT, k, n, a, player, found, m))
            }
            EE_UNITS_UCK_D_NAVAL_PRIEST_CREATE_UNIT_AT => {
                let w = EE_UNITS_UCG_DO_NAVAL_PRIEST_CREATE_UNIT_AT;
                return Some(ee_units_ucg_sea(w, k, n, a, player * 2 + found, 0, m as *mut f32));
            }
            EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT_AT => {
                return Some(ee_units_ucb_call(EE_UNITS_UCB_DO_SPACE_CREATE_UNIT_AT, k, n, a, player, found, m))
            }
            EE_UNITS_UCK_D_BUILDING_CREATE_UNIT_AT => {
                return Some(ee_units_ucb_call(EE_UNITS_UCB_DO_CREATE_UNIT_AT, k, n, a, player, found, m))
            }
            EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT_AT => {
                let w = EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT_AT;
                return Some(ee_units_ucg_sea(w, k, n, a, player * 2 + found, 0, m as *mut f32));
            }
            EE_UNITS_UCK_D_BASE_CREATE_UNIT_AT => {
                return Some(ee_units_uc_call(EE_UNITS_UC_DO_BASE_CREATE_UNIT_AT, k, n, a, player, found, null(), m))
            }
            EE_UNITS_UCK_D_GATE_GATE_REPLACED => {
                ee_units_ucb_gate(EE_UNITS_UCB_DO_GATE_REPLACED, k, a as usize as *mut c_void, b as u64, 0, 0, 0.0);
            }
            EE_UNITS_UCK_D_BUILDING_GROUND_LOCATION => {
                ee_units_ucg_sea(EE_UNITS_UCG_DO_BUILDING_GROUND_LOCATION, k, n, 0, 0, 0, f.as_mut_ptr());
            }
            EE_UNITS_UCK_D_SUBMERGED_GROUND_LOCATION => {
                ee_units_ucg_sea(EE_UNITS_UCG_DO_SUBMERGED_GROUND_LOCATION, k, n, 0, 0, 0, f.as_mut_ptr());
            }
            EE_UNITS_UCK_D_BOMB_LAUNCH => {
                return Some(flag(ee_units_ucs_call(EE_UNITS_UCS_DO_BOMB_LAUNCH, k, u, 0, null())))
            }
            EE_UNITS_UCK_D_MISSILE_LAUNCH => {
                return Some(flag(ee_units_ucs_call(EE_UNITS_UCS_DO_MISSILE_LAUNCH, k, u, 0, null())))
            }
            EE_UNITS_UCK_D_PROJECTILE_LAUNCH => {
                let at = a as usize as *const [f32; 3];
                return Some(flag(ee_units_ucs_call(EE_UNITS_UCS_DO_PROJECTILE_LAUNCH, k, u, 0, at)));
            }
            // a the location, b and c the tile's x and y outputs
            EE_UNITS_UCK_D_NAVAL_CITIZEN_TO_TILE => {
                let w = EE_UNITS_UCG_DO_NAVAL_CITIZEN_LOCATION_TO_TILE;
                ee_units_ucg_sea(w, k, n, b as u64, c, 0, a as usize as *mut f32);
            }
            EE_UNITS_UCK_D_SPACE_BUILDING_TO_TILE => {
                let w = EE_UNITS_UCG_DO_SPACE_BUILDING_LOCATION_TO_TILE;
                ee_units_ucg_sea(w, k, n, b as u64, c, 0, a as usize as *mut f32);
            }
            EE_UNITS_UCK_D_AMBIENT_TO_TILE => {
                let w = EE_UNITS_UCG_DO_AMBIENT_LOCATION_TO_TILE;
                ee_units_ucg_sea(w, k, n, b as u64, c, 0, a as usize as *mut f32);
            }
            EE_UNITS_UCK_D_SUBMERGED_TO_TILE => {
                let w = EE_UNITS_UCG_DO_SUBMERGED_LOCATION_TO_TILE;
                ee_units_ucg_sea(w, k, n, b as u64, c, 0, a as usize as *mut f32);
            }
            EE_UNITS_UCK_D_BASE_REMOVE_FROM_REGION => {
                ee_units_uc_call(EE_UNITS_UC_DO_REMOVE_FROM_REGION, k, n, a, 0, 0, null(), null());
            }
            EE_UNITS_UCK_D_SPACE_TRANSPORT_REMOVE => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_SPACE_REMOVE, k, n, a, 0, 0);
            }
            EE_UNITS_UCK_D_AMBIENT_REMOVE => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_AMBIENT_REMOVE, k, n, a, 0.0);
            }
            EE_UNITS_UCK_D_RESOURCE_SIMPLE_CREATED => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_RESOURCE_CREATED, k, n, a, 0.0);
            }
            EE_UNITS_UCK_D_AIR_ANIMAL_CREATED => {
                ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_CREATED, k, u, 0, 0, 0, null_mut());
            }
            EE_UNITS_UCK_D_FORTRESS_CREATED => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_FORTRESS_CREATED, k, u, 0, 0, 0);
            }
            EE_UNITS_UCK_D_GATE_CREATED => {
                ee_units_ucb_gate(EE_UNITS_UCB_DO_GATE_UNIT_CREATED, k, u, 0, 0, 0, 0.0);
            }
            EE_UNITS_UCK_D_HELICOPTER_TRANSPORT_CREATED => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_HELICOPTER_CREATED, k, u, 0, 0, 0);
            }
            EE_UNITS_UCK_D_SPACE_BUILDING_CREATED => {
                ee_units_ucb_gate(EE_UNITS_UCB_DO_SPACE_UNIT_CREATED, k, u, 0, 0, 0, 0.0);
            }
            EE_UNITS_UCK_D_SPACE_TRANSPORT_CREATED => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_SPACE_CREATED, k, u, 0, 0, 0);
            }
            EE_UNITS_UCK_D_BUILDING_CREATED => {
                ee_units_ucb_call(EE_UNITS_UCB_DO_UNIT_CREATED, k, u, 0, 0, 0, null());
            }
            EE_UNITS_UCK_D_LAND_CREATED => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_LAND_CREATED, k, u, 0, 0.0);
            }
            EE_UNITS_UCK_D_LANDABLE_CREATED => {
                ee_units_uca_call(EE_UNITS_UCA_DO_LANDABLE_CREATED, k, u, 0, 0, 0, null_mut());
            }
            EE_UNITS_UCK_D_MISSILE_CREATED => {
                ee_units_ucs_call(EE_UNITS_UCS_DO_MISSILE_CREATED, k, u, 0, null());
            }
            EE_UNITS_UCK_D_NAVAL_CREATED => {
                ee_units_ucg_call(EE_UNITS_UCG_DO_NAVAL_CREATED, k, u, 0, 0.0);
            }
            EE_UNITS_UCK_D_PROJECTILE_CREATED => {
                ee_units_ucs_call(EE_UNITS_UCS_DO_PROJECTILE_CREATED, k, u, 0, null());
            }
            EE_UNITS_UCK_D_TRANSPORT_CREATED => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_TRANSPORT_CREATED, k, u, 0, 0, 0);
            }
            EE_UNITS_UCK_D_FORTRESS_UNLOAD_AT => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_FORTRESS_UNLOAD_AT, k, u, 0, lb, lc);
            }
            EE_UNITS_UCK_D_TRANSPORT_UNLOAD_AT => {
                ee_units_ucx_call(EE_UNITS_UCX_DO_UNLOAD_AT, k, u, 0, lb, lc);
            }
            _ => return None,
        }
    }
    Some(0)
}
