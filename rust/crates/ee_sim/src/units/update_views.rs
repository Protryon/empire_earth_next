//! The update and `ActionChanged` hosts' plain reads over the world views (the units' round 1, branch rust-units1):
//! every `EE_UNITS_UPD_R_*` / `EE_UNITS_ACT_R_*` read that is one member of the unit, its class, its stats, the
//! world or the physics is answered here, in place, at the moment the sequence asks; the C++ host
//! (EEUnitUpdate.cpp `EEUpdateRead`) keeps only the reads that call a method or reach an object the views do not
//! cover. [`ViewUpdateHost`] wraps the C++ table for both entry points (update.rs, actions.rs).

use crate::units::actions::*;
use crate::units::update::*;
use crate::world::view::{UnitView, WorldRead, WorldView};
use crate::world_field;

/// `kAIKindAircraftCarrier`
const AI_KIND_AIRCRAFT_CARRIER: u64 = 0xf;
/// `kUnitAINoHeading`: no initial contact heading
const NO_HEADING: f32 = -10000.0;

fn b(v: bool) -> i64 {
    i64::from(v)
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// The plain read `code` of the complex unit `unit` (an `EEComplexUnit*`) whose class `class` (the host's
/// `EEUnitClass*`, the one the C++ host read as `theClass`) runs the sequence; None for a read the C++ host answers
/// (or one whose object the unit lacks: the C++ expression's own outcome).
pub fn view_read(t: &WorldRead, unit: u64, class_handle: u64, code: i32) -> Option<i64> {
    // SAFETY: the caller's handle is the live unit the host is for
    let u = unsafe { UnitView::of(t, unit) }?;
    if !u.is_complex() {
        return None;
    }
    // the unit's own class (`theUnit->mUnitClass`) and the host's (`theClass`)
    let class = u.class()?;
    // SAFETY: the host's class is live for the call
    let host_class = unsafe { crate::world::view::ClassView::of(t, class_handle) };
    let hf = || host_class.map(|h| h.fields());
    let cf = class.fields();
    let c = u.complex();
    let stats = || u.stats().map(|s| s.fields());
    let world = || WorldView::global(t);
    Some(match code {
        EE_UNITS_UPD_R_MEDIC => b(world_field!(hf()?, mIsMedic)),
        EE_UNITS_UPD_R_REGENERATES => match stats() {
            Some(s) => b(world_field!(s, mRegeneration) > 0),
            None => 0,
        },
        EE_UNITS_UPD_R_HERD => b(u.herd_handle() != 0),
        EE_UNITS_UPD_R_OWNER => i64::from(u.player()?.id()),
        EE_UNITS_UPD_R_TILE_X => u.tile_xy().0,
        EE_UNITS_UPD_R_COMMAND => i64::from(u.command_type()),
        EE_UNITS_UPD_R_GOAL_KIND => u.goal().map_or(-1, |g| i64::from(g.0)),
        EE_UNITS_UPD_R_UNIT_TYPE => class.unit_type() as i64,
        EE_UNITS_UPD_R_HIT_POINTS => u.hit_points(),
        EE_UNITS_UPD_R_FAMILY => class.family() as i64,
        EE_UNITS_UPD_R_BUILT => b(u.is_built()),
        EE_UNITS_UPD_R_FLIGHT_TIME => world_field!(c, mFlightTime) as i64,
        EE_UNITS_UPD_R_CONVERSION_START => world_field!(c, mConversionStartTime) as i64,
        EE_UNITS_UPD_R_CINEMATIC => b(world()?.in_cinematic()),
        EE_UNITS_UPD_R_TICK_MS => world()?.tick_ms(),
        EE_UNITS_UPD_R_PHYSICS_ORDER => world_field!(c, mCurrentPhysicsOrder) as i64,
        EE_UNITS_UPD_R_PHYSICS_ACTION => world_field!(c, mCurrentPhysicsAction) as i64,
        EE_UNITS_UPD_R_CLASS_UNARMED => b(world_field!(hf()?, mAttack) == 0),
        EE_UNITS_UPD_R_STATS_POWER => world_field!(stats()?, mPower) as i64,
        EE_UNITS_UPD_R_GAME_TIME => world()?.game_time(),
        EE_UNITS_UPD_R_GRAZE_TIME => world_field!(c, mGrazeTime) as i64,
        EE_UNITS_UPD_R_GATHER_TIME_2 => world_field!(stats()?, mGatherTimes)[2] as i64,
        EE_UNITS_UPD_R_GATHER_TIME_6 => world_field!(stats()?, mGatherTimes)[6] as i64,
        EE_UNITS_UPD_R_WORK_TIMER => world_field!(c, mWorkTimer) as i64,
        EE_UNITS_UPD_R_FIRST_PART => b(u.previous_part_handle() == 0),
        EE_UNITS_UPD_R_HAS_NEXT_PART => b(u.next_part_handle() != 0),
        EE_UNITS_UPD_R_OPEN_FRACTION => bits(world_field!(c, mOpenFraction)),
        EE_UNITS_ACT_R_ANIMATION => world_field!(c, mAnimation) as i64,
        EE_UNITS_ACT_R_LAND_BUILDING => b(host_class?.land_building()),
        EE_UNITS_ACT_R_CLASS_ARMED => b(world_field!(hf()?, mAttack) > 0),
        EE_UNITS_ACT_R_FIRE_OVER_WALLS => b(world_field!(stats()?, mCanFireOverWalls)),
        EE_UNITS_ACT_R_IS_WALL => b(class.wall()),
        EE_UNITS_ACT_R_ROTTING_TIME => world_field!(hf()?, mRottingTime) as i64,
        EE_UNITS_ACT_R_CLASS_RESOURCE_AMOUNT => world_field!(cf, mResourceAmount) as i64,
        EE_UNITS_ACT_R_NO_HEADING => b(world_field!(c, mInitialContactHeading) == NO_HEADING),
        EE_UNITS_ACT_R_DOMAINS => i64::from(world_field!(cf, mDomains)),
        EE_UNITS_ACT_R_CATEGORY => class.category() as i64,
        EE_UNITS_ACT_R_CONTAINER => b(u.container_handle() != 0),
        EE_UNITS_ACT_R_RESOURCE_AMOUNT => u.resource_amount(),
        EE_UNITS_ACT_R_AIRCRAFT_CARRIER => b(class.ai_kind() == AI_KIND_AIRCRAFT_CARRIER),
        EE_UNITS_ACT_R_EXITING_UNIT => world_field!(c, mExitingUnitID) as i64,
        EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE => world_field!(cf, mAirTrafficType) as i64,
        EE_UNITS_ACT_R_CLEARANCE_GRANTED => b(world_field!(c, mIsClearanceGranted)),
        EE_UNITS_ACT_R_COMMANDABLE => b(u.commandable()),
        _ => return None,
    })
}

/// The C++ update host with its plain reads answered by [`view_read`] (`unit`: the host's unit; the class is the
/// table's).
pub struct ViewUpdateHost<'h> {
    pub host: &'h EeUnitsUpdateHost,
    pub unit: u64,
}

impl UpdateHost for ViewUpdateHost<'_> {
    fn read(&mut self, code: i32) -> i64 {
        #[cfg(not(test))]
        if code == EE_UNITS_ACT_R_WAYPOINTS {
            // SAFETY: the update's live complex unit (waypoints.rs)
            return unsafe { crate::units::waypoints::list(self.unit) }.len() as i64;
        }
        // SAFETY: the host's call runs on the simulation thread, which holds the world (the C++ caller's)
        let t = unsafe { WorldRead::begin() };
        match view_read(&t, self.unit, self.host.class, code) {
            Some(v) => v,
            None => (self.host.read)(self.host.ctx, code),
        }
    }
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
        #[cfg(not(test))]
        match code {
            // the unit's path (the paths lane's round 1: waypoints.rs), noted first as the C++ act host noted them
            EE_UNITS_ACT_CANCEL_PATHS | EE_UNITS_ACT_FLY_CHECK_PATH | EE_UNITS_ACT_UNIT_NEXT_WAYPOINT => {
                crate::units::notes::host_op(code, EE_UNITS_ACT_RELEASE_HEAL_EFFECT, a, b);
                path_act(self.unit, code);
                return 0;
            }
            _ => {}
        }
        // SAFETY: as `read`'s
        let t = unsafe { WorldRead::begin() };
        if view_act(&t, self.unit, self.host.class, code, a, b) {
            return 0;
        }
        // the C++ host's note of every operation (TEST ONLY: USST kinds 12, 13), then its compound cases split into
        // their statements (upd_split.rs), which are not noted
        crate::units::notes::host_op(code, EE_UNITS_ACT_RELEASE_HEAL_EFFECT, a, b);
        if let Some(r) = crate::units::upd_split::split(&mut RawUpdateHost(self.host), code, a) {
            return r;
        }
        (self.host.act)(self.host.ctx, code, a, b)
    }
}

/// The C++ update host's operations as they are (upd_split.rs's statements).
struct RawUpdateHost<'h>(&'h EeUnitsUpdateHost);

impl crate::units::upd_split::UpdSplitHost for RawUpdateHost<'_> {
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
        (self.0.act)(self.0.ctx, code, a, b)
    }
}

/// The update host's path statements (waypoints.rs): `CancelPaths(unit, true)`, the landing aircraft's path check,
/// `NextWaypoint()`.
#[cfg(not(test))]
fn path_act(unit: u64, code: i32) {
    use crate::units::waypoints;
    // SAFETY: the update's live complex unit
    match code {
        EE_UNITS_ACT_CANCEL_PATHS => waypoints::cancel_paths(unit),
        EE_UNITS_ACT_FLY_CHECK_PATH => unsafe { waypoints::fly_check_path(unit) },
        _ => {
            unsafe { waypoints::next(unit) };
        }
    }
}

/// `mAngularVelocity = GEOrientation(0, 0, 0)` and `SetVelocity(0.0f)` (its `mF21C` the stats' `mBC` when that is
/// not below 0: `max(0.0f, mBC)`), after `SetCurrentPhysicsAction(action)` when one is given: the unit stopped (the
/// update host's `STOP`, the goal host's `HALT`).
pub fn halt(u: &UnitView<'_>, action: Option<std::os::raw::c_ulong>) {
    let bc = u.stats().map_or(0.0, |s| world_field!(s.fields(), mBC));
    let cx = u.complex().ptr() as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
    let at = (u.handle() as usize + crate::world::layout::layout().complex_angular_velocity as usize) as *mut [f32; 3];
    // SAFETY: the live unit's record and member; no other reference to them is held across the stores
    unsafe {
        if let Some(a) = action {
            (*cx).mCurrentPhysicsAction = a;
        }
        // (the setter's "when it differs" test keeps a -0.0)
        if (*cx).mVelocity != 0.0 {
            (*cx).mVelocity = 0.0;
        }
        (*cx).mF21C = if 0.0 > bc { 0.0 } else { bc };
        std::ptr::write_unaligned(at, [0.0; 3]);
    }
}

/// The operations of the update and `ActionChanged` hosts that are a store of the unit's records or one effect (the
/// units' behaviour lane, round 2; they were EEUnitUpdate.cpp `EEUpdateAct`'s cases): done here with the host's note
/// (USST kinds 12 and 13, noted first as the C++ host did); false for an operation the C++ host does. Each store is
/// the inline setter's member store (an integer setter's "when it differs" test changes nothing observable; a
/// float's keeps a -0.0, so it is kept).
pub fn view_act(t: &WorldRead, unit: u64, class_handle: u64, code: i32, a: i64, b: i64) -> bool {
    use std::os::raw::{c_long, c_ulong};
    if !matches!(
        code,
        EE_UNITS_UPD_LAST_HIT_POINTS
            | EE_UNITS_UPD_CLEAR_CONVERSION_START
            | EE_UNITS_UPD_SET_FLIGHT_TIME
            | EE_UNITS_UPD_SET_PHYSICS_ORDER
            | EE_UNITS_UPD_SET_GRAZE_TIME
            | EE_UNITS_UPD_SET_WORK_TIMER
            | EE_UNITS_UPD_SET_PARTS_ROTTING
            | EE_UNITS_ACT_SET_PHYSICS_ACTION
            | EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER
            | EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME
            | EE_UNITS_ACT_PAUSE_ANIMATION
            | EE_UNITS_ACT_CLEAR_HEADING
            | EE_UNITS_ACT_SET_ROTTING
            | EE_UNITS_ACT_CLEAR_ARRIVED
            | EE_UNITS_ACT_RESET_HEAL_SEARCH
            | EE_UNITS_ACT_DEATH_EFFECT
            | EE_UNITS_ACT_SET_CONVERSION_START
            | EE_UNITS_ACT_SET_WORK_TIMER
            | EE_UNITS_ACT_SET_RESOURCE_AMOUNT
            | EE_UNITS_ACT_SET_PRODUCTION_TIME
            | EE_UNITS_ACT_CLEAR_SHIFT_QUEUED
            | EE_UNITS_ACT_SET_EXITING_UNIT
            | EE_UNITS_ACT_SET_PHYSICS_ORDER
            | EE_UNITS_ACT_CLEAR_F220
            | EE_UNITS_ACT_SET_FLIGHT_TIME_STATS
            | EE_UNITS_ACT_SET_AMMUNITION
            | EE_UNITS_ACT_SET_COMMANDABLE
            | EE_UNITS_ACT_MAKE_COMMANDABLE
            | EE_UNITS_ACT_STOP
    ) {
        return false;
    }
    // SAFETY: the caller's handle is the live unit the host is for
    let Some(u) = (unsafe { UnitView::of(t, unit) }) else { return false };
    if !u.is_complex() {
        return false;
    }
    // SAFETY: the host's class is live for the call
    let class = unsafe { crate::world::view::ClassView::of(t, class_handle) };
    // what the stores read, before any store
    let hit_points = u.hit_points();
    let game_time = WorldView::global(t).map_or(0, |w| w.game_time());
    let stats_flight = u.stats().map(|s| world_field!(s.fields(), mFlightTime));
    let heal_delay = class.map(|c| world_field!(c.fields(), mHealDelay));
    let ammunition = class.map(|c| world_field!(c.fields(), mAmmunition));
    if (code == EE_UNITS_ACT_SET_FLIGHT_TIME_STATS && stats_flight.is_none())
        || (code == EE_UNITS_ACT_RESET_HEAL_SEARCH && heal_delay.is_none())
        || (code == EE_UNITS_ACT_SET_AMMUNITION && ammunition.is_none())
        || (code == EE_UNITS_ACT_DEATH_EFFECT && class.is_none())
    {
        return false;
    }
    let mut parts = Vec::new();
    if code == EE_UNITS_UPD_SET_PARTS_ROTTING {
        let mut next = u.next_part_handle();
        while next != 0 {
            parts.push(next);
            // SAFETY: a live unit's next part
            next = unsafe { UnitView::of(t, next) }.map_or(0, |p| p.next_part_handle());
        }
    }
    let death_effect = class.map_or(0, |c| c.effect_handle(1));
    let fields = u.fields().ptr() as *mut crate::world::unit_fields::EeUnitsUnitFields;
    let cx = u.complex().ptr() as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
    crate::units::notes::host_op(code, EE_UNITS_ACT_RELEASE_HEAL_EFFECT, a, b);
    // SAFETY: the live unit's records; no other reference to them is held across the store
    let c = unsafe { &mut *cx };
    match code {
        EE_UNITS_UPD_LAST_HIT_POINTS => c.mLastHitPoints = hit_points as c_long,
        EE_UNITS_UPD_CLEAR_CONVERSION_START => c.mConversionStartTime = 0,
        EE_UNITS_UPD_SET_FLIGHT_TIME => c.mFlightTime = a as c_long,
        EE_UNITS_UPD_SET_PHYSICS_ORDER | EE_UNITS_ACT_SET_PHYSICS_ORDER => c.mCurrentPhysicsOrder = a as c_ulong,
        EE_UNITS_UPD_SET_GRAZE_TIME => c.mGrazeTime = a as u32 as c_ulong,
        EE_UNITS_UPD_SET_WORK_TIMER | EE_UNITS_ACT_SET_WORK_TIMER => c.mWorkTimer = a as c_long,
        EE_UNITS_UPD_SET_PARTS_ROTTING => {
            for p in parts {
                // SAFETY: a live part's record
                let part = unsafe { UnitView::of(t, p) };
                if let Some(part) = part {
                    let pc = part.complex().ptr() as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
                    // SAFETY: as above
                    unsafe { (*pc).mRottingAwayTime = a as c_long };
                }
            }
        }
        EE_UNITS_ACT_SET_PHYSICS_ACTION => c.mCurrentPhysicsAction = a as c_ulong,
        EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER => c.mTargetSearchTimer = a as c_long,
        EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME => c.mNextTriggerTime = a as c_long as c_ulong,
        EE_UNITS_ACT_PAUSE_ANIMATION => c.mIsAnimationPaused = true,
        EE_UNITS_ACT_CLEAR_HEADING => c.mInitialContactHeading = NO_HEADING,
        EE_UNITS_ACT_SET_ROTTING => c.mRottingAwayTime = a as c_long,
        EE_UNITS_ACT_CLEAR_ARRIVED => c.mHasArrived = false,
        EE_UNITS_ACT_RESET_HEAL_SEARCH => {
            // (a float setter's "when it differs" test: a -0.0 and 0.0 are equal)
            let v = heal_delay.unwrap_or(0.0);
            if c.mHealSearchTimer != v {
                c.mHealSearchTimer = v;
            }
        }
        EE_UNITS_ACT_SET_CONVERSION_START => c.mConversionStartTime = game_time as c_ulong,
        // SAFETY: the unit's own record (EEUnit's), as above
        EE_UNITS_ACT_SET_RESOURCE_AMOUNT => unsafe { (*fields).mResourceAmount = a as c_long },
        EE_UNITS_ACT_SET_PRODUCTION_TIME => c.mProductionTime = a as c_long,
        EE_UNITS_ACT_CLEAR_SHIFT_QUEUED => c.mHasShiftQueuedBuild = false,
        EE_UNITS_ACT_SET_EXITING_UNIT => c.mExitingUnitID = a as c_long,
        EE_UNITS_ACT_CLEAR_F220 => {
            if c.mF220 != 0.0 {
                c.mF220 = 0.0;
            }
        }
        EE_UNITS_ACT_SET_FLIGHT_TIME_STATS => c.mFlightTime = stats_flight.unwrap_or(0),
        EE_UNITS_ACT_SET_AMMUNITION => c.mAmmunition = ammunition.unwrap_or(0) as c_ulong,
        EE_UNITS_ACT_SET_COMMANDABLE => c.mIsCommandable = a != 0,
        EE_UNITS_ACT_MAKE_COMMANDABLE => c.mIsCommandable = true,
        EE_UNITS_ACT_STOP => halt(&u, None),
        EE_UNITS_ACT_DEATH_EFFECT => {
            // `gEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0)`
            use crate::world::effects::{effects, EffectPlace, EffectRequest};
            effects().create(&EffectRequest {
                graphic_effect: death_effect,
                place: EffectPlace::Unit { unit, keeps_unit: false, tag_point: 0, goal_index: 0 },
            });
        }
        _ => {}
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn no_unit_reads_nothing() {
        // SAFETY: no object is read for handle 0
        let t = unsafe { WorldRead::begin() };
        assert_eq!(view_read(&t, 0, 0, EE_UNITS_UPD_R_HIT_POINTS), None);
    }

    #[test]
    fn bits_and_flags() {
        assert_eq!(bits(1.0), 0x3f80_0000);
        assert_eq!(b(true), 1);
        assert_eq!(b(false), 0);
    }
}
