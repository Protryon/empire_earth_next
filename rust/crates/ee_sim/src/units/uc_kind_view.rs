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
use crate::world::layout::layout;
use crate::world::view::{StatsView, UnitView};
use crate::world_field;
use std::os::raw::{c_long, c_ulong};

use crate::units::uca_air::*;
use crate::units::ucb_gate::*;
use crate::units::ucb_release::*;
use crate::units::ucg_ground::*;
use crate::units::ucg_sea::*;
use crate::units::uck_dispatch::*;
use crate::units::ucp_people::*;
use crate::units::ucs_impact::*;
use crate::units::ucs_shots::*;
use crate::units::ucx_transports::*;

/// The kind `kind`'s statement `op` when it is a plain one.
pub(crate) fn kind_view(kind: Kind, x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let _ = c;
    match kind {
        Kind::Class => None,
        Kind::Air => air(x, op, f),
        Kind::Buildings => buildings(x, op, b, f),
        Kind::Ground => ground(x, op, f),
        Kind::Kinds => kinds(x, op),
        Kind::People => people(x, op, f),
        Kind::Shots => shots(x, op, f),
        Kind::Transports => transports(x, op, f),
    }
}

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

fn ground(x: &Ctx<'_>, op: i32, f: &mut [f32; 4]) -> Option<i64> {
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
        _ => return None,
    })
}

fn kinds(x: &Ctx<'_>, op: i32) -> Option<i64> {
    Some(match op {
        EE_UNITS_UCK_D_DOMAINS => i64::from(world_field!(x.cls()?.fields(), mDomains)),
        EE_UNITS_UCK_D_IS_FLARE => i64::from(world_field!(x.cls()?.fields(), mIsFlare)),
        EE_UNITS_UCK_D_UNIT_TYPE_OF => x.o()?.class()?.unit_type() as i64,
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

fn shots(x: &Ctx<'_>, op: i32, f: &mut [f32; 4]) -> Option<i64> {
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
