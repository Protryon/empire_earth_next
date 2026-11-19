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
        // SAFETY: the host's call runs on the simulation thread, which holds the world (the C++ caller's)
        let t = unsafe { WorldRead::begin() };
        match view_read(&t, self.unit, self.host.class, code) {
            Some(v) => v,
            None => (self.host.read)(self.host.ctx, code),
        }
    }
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
        (self.host.act)(self.host.ctx, code, a, b)
    }
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
