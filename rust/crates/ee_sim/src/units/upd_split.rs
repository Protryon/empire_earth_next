//! The update and `ActionChanged` hosts' compound operations split into the original's statements (the units' lane,
//! round 8; were EEUnitUpdate.cpp `EEUpdateAct`'s multi-statement cases), and the host's notes (USST kinds 12 and
//! 13, which `EEUpdateAct` wrote first for every operation): `ViewUpdateHost::act` notes the operation, then asks
//! [`split`]; each C++ statement is one `EE_UNITS_UPD_X_*` case of `EEUpdateAct` (from 340, not noted), asked in the
//! original's order; the choices and loops are here. The aircraft operations that ended in
//! `EELandableActionOperation(code, ...)` reach it through one case with the code as its argument.

use crate::units::actions::*;
use crate::units::update::EE_UNITS_UPD_SET_OPEN_FRACTION;

// ---- the statements (`a`, `b` the act's arguments; units and objects as their pointers)
/// `theUnit->SetOpenFraction(the float of bits a)`
pub const EE_UNITS_UPD_X_OPEN_FRACTION: i32 = 340;
/// `theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(the float of bits a)`
pub const EE_UNITS_UPD_X_CARRIER_OPEN_FRACTION: i32 = 341;
/// `theClass->GetLastLinked(theUnit)->SetOpenFraction(the float of bits a)`
pub const EE_UNITS_UPD_X_LINKED_OPEN_FRACTION: i32 = 342;
/// `theUnit->SetGoal(EEUGRust::NewLocation(&mInitialContactLocation, false, false, true, mInitialContactHeading,
/// false, false), a != 0, 0)`
pub const EE_UNITS_UPD_X_RETURN_TO_CONTACT: i32 = 343;
/// unit `a`'s `mNextPart`
pub const EE_UNITS_UPD_X_NEXT_PART_OF: i32 = 344;
/// unit `a`'s `StartAction(b)`
pub const EE_UNITS_UPD_X_START_ACTION_OF: i32 = 345;
/// unit `a`'s `mUnitClass->ActionChanged(unit a)`
pub const EE_UNITS_UPD_X_ACTION_CHANGED_OF: i32 = 346;
/// `theUnit->mPlayer->KillUnit(unit a, false)`
pub const EE_UNITS_UPD_X_KILL_UNIT: i32 = 347;
/// `theContext->mHealee = NULL`
pub const EE_UNITS_UPD_X_CLEAR_HEALEE: i32 = 348;
/// `theClass->FindTargetNear(theUnit, theUnit->mStats->GetLineOfSight(), &theContext->mHealee)`
pub const EE_UNITS_UPD_X_FIND_HEALEE: i32 = 349;
/// `theUnit->SetGoal(EEUGRust::NewHealUnit(theContext->mHealee, false), a != 0, 0)`
pub const EE_UNITS_UPD_X_SET_HEAL_GOAL: i32 = 350;
/// `theUnit->mStats->mDeathSound`
pub const EE_UNITS_UPD_X_DEATH_SOUND: i32 = 351;
/// `theClass->GetSoundParameters(theUnit, sound a, the three floats at b)`
pub const EE_UNITS_UPD_X_SOUND_PARAMETERS: i32 = 352;
/// `gSoundManager->Play(sound a's mSampleID, the three floats at b, its mLooping, NULL)`
pub const EE_UNITS_UPD_X_PLAY_SAMPLE: i32 = 353;
/// `theClass->FindStorage(theUnit, a != 0)`
pub const EE_UNITS_UPD_X_FIND_STORAGE: i32 = 354;
/// `theUnit->SetGoal(EEUGRust::NewGather(NULL, storage a, false), 0, 0)`
pub const EE_UNITS_UPD_X_GATHER_AT: i32 = 355;
/// `EEAttackUnitCast(theUnit->GetGoal())`
pub const EE_UNITS_UPD_X_ATTACK_GOAL: i32 = 356;
/// goal `a`'s `GetAttackTarget()`
pub const EE_UNITS_UPD_X_ATTACK_TARGET: i32 = 357;
/// `theClass->FindNearestTargetBuilding(unit a)`
pub const EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING: i32 = 358;
/// goal `a`'s `SetAttackTarget(unit b)`
pub const EE_UNITS_UPD_X_SET_ATTACK_TARGET: i32 = 359;
/// `theUnit->FindPassenger(theUnit->mExitingUnitID)`
pub const EE_UNITS_UPD_X_EXITING_PASSENGER: i32 = 360;
/// `ee_units_atc_queue_request(cargo a's player's mAirTraffic[its class's mAirTrafficType - 1], theUnit, a, 0)`
pub const EE_UNITS_UPD_X_QUEUE_TAKEOFF: i32 = 361;
/// `EESoundObject::Get(kSoundGate)`
pub const EE_UNITS_UPD_X_GATE_SOUND: i32 = 362;
/// sound `a`'s `Play(the three floats at b)`
pub const EE_UNITS_UPD_X_PLAY_SOUND: i32 = 363;
/// the point at `a` = theUnit's location (x, y, z)
pub const EE_UNITS_UPD_X_LOCATION: i32 = 364;
/// `theUnit->GetNextWaypointPoint(the point at a)`
pub const EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT: i32 = 365;
/// `theUnit->SetHeight(theClass->GetHeightAt(theUnit, the point at a))`
pub const EE_UNITS_UPD_X_SET_HEIGHT_AT: i32 = 366;
/// `EEAirAnimalDeathRoll(theClass, theUnit)`
pub const EE_UNITS_UPD_X_AIR_ANIMAL_DEATH_ROLL: i32 = 367;
/// `EELandableActionOperation(a, theClass, theUnit, b, &theContext->mAirport)`
pub const EE_UNITS_UPD_X_LANDABLE: i32 = 368;
/// `EEHelicopterPathToGoal(theClass, theUnit)`
pub const EE_UNITS_UPD_X_HELICOPTER_PATH: i32 = 369;
/// `EEHelicopterTransportPathToGoal(theClass, theUnit)`
pub const EE_UNITS_UPD_X_HELICOPTER_TRANSPORT_PATH: i32 = 370;
/// `EEHelicopterPriestPathToGoal(theClass, theUnit)`
pub const EE_UNITS_UPD_X_HELICOPTER_PRIEST_PATH: i32 = 371;
/// `EEAirplaneHoldingPattern(theClass, theUnit)`
pub const EE_UNITS_UPD_X_AIRPLANE_HOLDING: i32 = 372;
/// `EEPlanetSpaceshipHoldingPattern(theClass, theUnit)`
pub const EE_UNITS_UPD_X_SPACESHIP_HOLDING: i32 = 373;
/// theUnit (the host's unit)
pub const EE_UNITS_UPD_X_UNIT: i32 = 374;

/// `kActionIdle`
const ACTION_IDLE: i64 = 5000;

/// The host's statements (`EEUpdateAct`, not noted).
pub trait UpdSplitHost {
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64;
}

fn s<H: UpdSplitHost + ?Sized>(h: &mut H, code: i32) -> i64 {
    h.act(code, 0, 0)
}

fn sound<H: UpdSplitHost + ?Sized>(h: &mut H, sound: i64, play: i32) {
    let mut p = [0.0f32; 3];
    let at = p.as_mut_ptr() as i64;
    h.act(EE_UNITS_UPD_X_SOUND_PARAMETERS, sound, at);
    h.act(play, sound, at);
}

/// `code` with `a` as `EEUpdateAct`'s compound case did it; None for any other code.
pub fn split<H: UpdSplitHost + ?Sized>(h: &mut H, code: i32, a: i64) -> Option<i64> {
    match code {
        EE_UNITS_UPD_SET_OPEN_FRACTION => {
            h.act(EE_UNITS_UPD_X_OPEN_FRACTION, a, 0);
            h.act(EE_UNITS_UPD_X_CARRIER_OPEN_FRACTION, a, 0);
            h.act(EE_UNITS_UPD_X_LINKED_OPEN_FRACTION, a, 0);
        }
        EE_UNITS_ACT_RETURN_TO_CONTACT => {
            h.act(EE_UNITS_UPD_X_RETURN_TO_CONTACT, a, 0);
        }
        EE_UNITS_ACT_NEXT_PART_ACTION => {
            let unit = s(h, EE_UNITS_UPD_X_UNIT);
            let part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, unit, 0);
            h.act(EE_UNITS_UPD_X_START_ACTION_OF, part, a);
            let part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, unit, 0);
            h.act(EE_UNITS_UPD_X_ACTION_CHANGED_OF, part, 0);
        }
        EE_UNITS_ACT_KILL_UNIT_AND_PARTS => {
            let unit = s(h, EE_UNITS_UPD_X_UNIT);
            h.act(EE_UNITS_UPD_X_KILL_UNIT, unit, 0);
            let mut part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, unit, 0);
            while part != 0 {
                h.act(EE_UNITS_UPD_X_KILL_UNIT, part, 0);
                part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, part, 0);
            }
        }
        EE_UNITS_ACT_START_IDLE => {
            let unit = s(h, EE_UNITS_UPD_X_UNIT);
            h.act(EE_UNITS_UPD_X_START_ACTION_OF, unit, ACTION_IDLE);
            h.act(EE_UNITS_UPD_X_ACTION_CHANGED_OF, unit, 0);
        }
        EE_UNITS_ACT_FIND_HEALEE => {
            s(h, EE_UNITS_UPD_X_CLEAR_HEALEE);
            return Some(i64::from(s(h, EE_UNITS_UPD_X_FIND_HEALEE) != 0));
        }
        EE_UNITS_ACT_SET_HEAL_GOAL => {
            h.act(EE_UNITS_UPD_X_SET_HEAL_GOAL, a, 0);
        }
        EE_UNITS_ACT_DEATH_SOUND => {
            let death = s(h, EE_UNITS_UPD_X_DEATH_SOUND);
            sound(h, death, EE_UNITS_UPD_X_PLAY_SAMPLE);
        }
        EE_UNITS_ACT_FIND_STORAGE_GOAL => {
            let storage = h.act(EE_UNITS_UPD_X_FIND_STORAGE, a, 0);
            if storage != 0 {
                h.act(EE_UNITS_UPD_X_GATHER_AT, storage, 0);
            }
        }
        EE_UNITS_ACT_RETARGET_NEAREST_BUILDING => {
            let goal = s(h, EE_UNITS_UPD_X_ATTACK_GOAL);
            if goal != 0 && h.act(EE_UNITS_UPD_X_ATTACK_TARGET, goal, 0) != 0 {
                let target = h.act(EE_UNITS_UPD_X_ATTACK_TARGET, goal, 0);
                let building = h.act(EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING, target, 0);
                if building != 0 {
                    h.act(EE_UNITS_UPD_X_SET_ATTACK_TARGET, goal, building);
                }
            }
        }
        EE_UNITS_ACT_QUEUE_TAKEOFF => {
            let cargo = s(h, EE_UNITS_UPD_X_EXITING_PASSENGER);
            if cargo != 0 {
                h.act(EE_UNITS_UPD_X_QUEUE_TAKEOFF, cargo, 0);
            }
        }
        EE_UNITS_ACT_GATE_SOUND => {
            let gate = s(h, EE_UNITS_UPD_X_GATE_SOUND);
            if gate != 0 {
                sound(h, gate, EE_UNITS_UPD_X_PLAY_SOUND);
            }
        }
        EE_UNITS_ACT_GATE_PARTS_ACTION => {
            let unit = s(h, EE_UNITS_UPD_X_UNIT);
            let part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, unit, 0);
            h.act(EE_UNITS_UPD_X_START_ACTION_OF, part, a);
            let part = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, unit, 0);
            let next = h.act(EE_UNITS_UPD_X_NEXT_PART_OF, part, 0);
            h.act(EE_UNITS_UPD_X_START_ACTION_OF, next, a);
        }
        EE_UNITS_ACT_HEIGHT_HERE | EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT => {
            let mut p = [0.0f32; 3];
            let at = p.as_mut_ptr() as i64;
            h.act(EE_UNITS_UPD_X_LOCATION, at, 0);
            if code == EE_UNITS_ACT_HEIGHT_HERE || h.act(EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT, at, 0) == 0 {
                h.act(EE_UNITS_UPD_X_SET_HEIGHT_AT, at, 0);
            }
        }
        EE_UNITS_ACT_DEATH_ROLL => {
            if a == 0 {
                s(h, EE_UNITS_UPD_X_AIR_ANIMAL_DEATH_ROLL);
            } else {
                h.act(EE_UNITS_UPD_X_LANDABLE, i64::from(code), a);
            }
        }
        EE_UNITS_ACT_HELICOPTER_PATH => {
            s(
                h,
                match a {
                    0 => EE_UNITS_UPD_X_HELICOPTER_PATH,
                    1 => EE_UNITS_UPD_X_HELICOPTER_TRANSPORT_PATH,
                    _ => EE_UNITS_UPD_X_HELICOPTER_PRIEST_PATH,
                },
            );
        }
        EE_UNITS_ACT_HOLDING_PATTERN => {
            s(h, if a == 0 { EE_UNITS_UPD_X_AIRPLANE_HOLDING } else { EE_UNITS_UPD_X_SPACESHIP_HOLDING });
        }
        EE_UNITS_ACT_APPROACH_CARRIER
        | EE_UNITS_ACT_BOARD_CARRIER
        | EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER
        | EE_UNITS_ACT_FLY_CHECK_PATH
        | EE_UNITS_ACT_FIND_NEAREST_AIRPORT
        | EE_UNITS_ACT_SET_HOME_BASE_FOUND
        | EE_UNITS_ACT_APPROACH_LANDING
        | EE_UNITS_ACT_SECONDARY_TARGET
        | EE_UNITS_ACT_TAKEOFF
        | EE_UNITS_ACT_QUEUE_LANDING
        | EE_UNITS_ACT_BOMBER_GROUND_GOAL
        | EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT
        | EE_UNITS_ACT_CONTACT_HERE
        | EE_UNITS_ACT_LANDABLE_PATH => return Some(h.act(EE_UNITS_UPD_X_LANDABLE, i64::from(code), a)),
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
#[path = "upd_split_tests.rs"]
mod tests;
