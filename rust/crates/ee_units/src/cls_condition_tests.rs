//! cls_condition.rs's tests over a mock world: units with their records, the host's operations as a table of
//! answers, and a log of the operations asked (the statements' order).

use super::*;
use crate::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use std::collections::BTreeMap;

struct MockUnit {
    rts: Box<EeUnitsRtsUnitFields>,
    unit: Box<EeUnitsUnitFields>,
    cx: Box<EeUnitsComplexUnitFields>,
    class: Box<EeUnitsUnitClassFields>,
    class_obj: u64,
    location: [f32; 3],
    land_building: bool,
    stats_attack: f32,
}

#[derive(Default)]
struct Mock {
    units: BTreeMap<u64, MockUnit>,
    classes: BTreeMap<u64, Box<EeUnitsUnitClassFields>>,
    /// (op, a, b, c) -> answer; (op, a, b, c) -> f (the float out)
    answers: BTreeMap<(i32, u64, u64, i64), i64>,
    floats: BTreeMap<(i32, u64, u64, i64), f32>,
    points: BTreeMap<(i32, u64), [f32; 3]>,
    landing: Option<i32>,
    log: Vec<(i32, i64)>,
}

/// The first category whose kinds include `kind` and none of `not`.
fn category(kind: u64, not: u64) -> u32 {
    (0..256).find(|&c| EE_UNIT_CLASS_KINDS[c] & kind != 0 && EE_UNIT_CLASS_KINDS[c] & not == 0).unwrap() as u32
}

impl Mock {
    fn class(&mut self, obj: u64, category: u32, unit_type: u32) -> u64 {
        let c = EeUnitsUnitClassFields { mCategory: category as _, mUnitType: unit_type as _, ..Default::default() };
        self.classes.insert(obj, Box::new(c));
        obj
    }
    fn unit(&mut self, obj: u64, class: u64) -> u64 {
        let c = **self.classes.get(&class).unwrap();
        self.units.insert(
            obj,
            MockUnit {
                rts: Box::default(),
                unit: Box::new(EeUnitsUnitFields { mHitPoints: 10, ..Default::default() }),
                cx: Box::default(),
                class: Box::new(c),
                class_obj: class,
                location: [0.0; 3],
                land_building: false,
                stats_attack: 0.0,
            },
        );
        obj
    }
    fn set(&mut self, op: i32, a: u64, b: u64, c: i64, v: i64) {
        self.answers.insert((op, a, b, c), v);
    }
    fn setf(&mut self, op: i32, a: u64, b: u64, c: i64, v: f32) {
        self.floats.insert((op, a, b, c), v);
    }
    fn asked(&self, op: i32) -> bool {
        self.log.iter().any(|&(o, _)| o == op)
    }
}

impl CondHost for Mock {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        let u = self.units.get(&obj).unwrap();
        EeUnitsUuUnit {
            obj,
            rts: &*u.rts,
            unit: &*u.unit,
            cx: &*u.cx,
            class_fields: &*u.class,
            class_obj: u.class_obj,
            complex: obj,
            location: u.location,
            land_building: u8::from(u.land_building),
            stats_attack: u.stats_attack,
            ..Default::default()
        }
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        &**self.classes.get(&class).unwrap()
    }
    fn location(&mut self, unit: u64) -> [f32; 3] {
        self.units[&unit].location
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64 {
        self.log.push((op, c));
        let key = if op == EE_UNITS_COND_DAMAGE_FACTOR { (op, c as u64, d as u64, 0) } else { (op, a, b, c) };
        if let Some(&f) = self.floats.get(&key) {
            io.f = f;
        }
        if let Some(&p) = self.points.get(&(op, a)) {
            io.p = p;
        }
        *self.answers.get(&key).unwrap_or(&0)
    }
    fn landing(&mut self, _class: u64, _unit: u64, _event: u32, _animal: bool) -> Option<i32> {
        self.landing
    }
}

const CLASS: u64 = 0x1000;
const UNIT: u64 = 0x100;
const ACTIVE_OBJ: u64 = 0x5000;
const STATE_OBJ: u64 = 0x6000;

fn with(category: u32) -> Mock {
    let mut m = Mock::default();
    m.class(CLASS, category, 1);
    m.unit(UNIT, CLASS);
    m
}

fn answer(m: &mut Mock, event: u32) -> i32 {
    match condition(m, CLASS, UNIT, event) {
        Answer::Value(v) => v,
        Answer::Combat(k) => panic!("a shot's answer {k:?}"),
    }
}

#[test]
fn the_active_goal_answers_first_and_two_when_it_is_not_the_current_goal() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 1, STATE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_CONDITION, UNIT, ACTIVE_OBJ, i64::from(TRIGGER_ALWAYS_TRUE), 1);
    assert_eq!(answer(&mut m, TRIGGER_ALWAYS_TRUE), 2);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 1, ACTIVE_OBJ as i64);
    assert_eq!(answer(&mut m, TRIGGER_ALWAYS_TRUE), 1);
    // the goal says no: the unit's own answer (a goal: no goal-less trigger)
    m.set(EE_UNITS_COND_GOAL_CONDITION, UNIT, ACTIVE_OBJ, i64::from(TRIGGER_UNIT_HAS_GOAL), 0);
    assert_eq!(answer(&mut m, TRIGGER_UNIT_HAS_GOAL), 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_CALAMITY), 1);
}

#[test]
fn the_goal_triggers_read_its_kind_and_flags() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    assert_eq!(answer(&mut m, TRIGGER_UNIT_HAS_GOAL), 0);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_ATTACK), 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_PLAYER_INITIATED), 1);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_ATTACK_GROUND) | 0x100);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_ATTACK_LOCATION), 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_ATTACK), 0);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_PLAYER_INITIATED), 1);
    m.set(EE_UNITS_COND_GOAL_ELAPSED, UNIT, ACTIVE_OBJ, 1500, 1);
    assert_eq!(answer(&mut m, TRIGGER_ONE_AND_A_HALF_SECONDS_ELAPSED), 1);
    assert_eq!(answer(&mut m, TRIGGER_TWO_SECONDS_ELAPSED), 0);
    // a forced move does not attack-move; a land unit can
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_LOCATION));
    assert_eq!(answer(&mut m, TRIGGER_ATTACK_MOVE_ENABLED), 1);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_LOCATION) | 0x200);
    assert_eq!(answer(&mut m, TRIGGER_ATTACK_MOVE_ENABLED), 0);
}

#[test]
fn the_records_answer_in_place() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    {
        let u = m.units.get_mut(&UNIT).unwrap();
        u.cx.mReloadTimer = 0;
        u.cx.mLastHitPoints = 12;
        u.cx.mGrowth = 0.5;
        u.cx.mInitialContactHeading = NO_HEADING;
        u.class.mUsesAmmunition = true;
    }
    assert_eq!(answer(&mut m, TRIGGER_RELOADED), 1);
    assert_eq!(answer(&mut m, TRIGGER_TAKING_LIGHT_DAMAGE), 1);
    assert_eq!(answer(&mut m, TRIGGER_IS_BABY), 1);
    assert_eq!(answer(&mut m, TRIGGER_IS_ADULT), 0);
    assert_eq!(answer(&mut m, TRIGGER_HAS_VALID_INITIAL_CONTACT_LOCATION), 0);
    assert_eq!(answer(&mut m, TRIGGER_AMMO_EXHAUSTED), 1);
    m.units.get_mut(&UNIT).unwrap().cx.mAmmunition = 3;
    assert_eq!(answer(&mut m, TRIGGER_AMMO_EXHAUSTED), 0);
    // the heal delay is the class's the body ran on
    m.classes.get_mut(&CLASS).unwrap().mHealDelay = 2.0;
    assert_eq!(answer(&mut m, TRIGGER_HEAL_SEARCH_TIME_ELAPSED), 1);
    // an unknown trigger is false
    assert_eq!(answer(&mut m, 1), 0);
}

#[test]
fn retaliation_needs_a_hurtable_reachable_attacker_in_range() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    let enemy_class = m.class(0x2000, category(EE_CLASS_KIND_LAND, 0), 2);
    let attacker = m.unit(0x200, enemy_class);
    m.units.get_mut(&attacker).unwrap().class.mFamily = 1;
    m.units.get_mut(&UNIT).unwrap().class.mAmmoType = 4;
    m.set(EE_UNITS_COND_LAST_ATTACKER, UNIT, 0, 0, attacker as i64);
    m.set(EE_UNITS_COND_CONDITION_TARGET, UNIT, 0, 0, attacker as i64);
    m.set(EE_UNITS_COND_BEHAVIOR, UNIT, 0, 0, 1);
    m.set(EE_UNITS_COND_CAN_BE_ATTACKED, 0, enemy_class, 0, 1);
    m.setf(EE_UNITS_COND_DAMAGE_FACTOR, 1, 4, 0, 0.5);
    m.setf(EE_UNITS_COND_RESPONSE_RANGE, UNIT, 0, 0, 100.0);
    m.setf(EE_UNITS_COND_DISTANCE_TO, UNIT, attacker, 0, 50.0);
    assert_eq!(answer(&mut m, TRIGGER_SHOULD_I_RETALIATE), 0, "not reachable");
    m.set(EE_UNITS_COND_CAN_REACH, UNIT, attacker, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_SHOULD_I_RETALIATE), 1);
    assert_eq!(answer(&mut m, TRIGGER_CAN_DAMAGE_ATTACKER), 1);
    assert_eq!(answer(&mut m, TRIGGER_IS_ATTACKER_IN_RETALIATION_RANGE), 1);
    // its ammunition does not hurt the attacker's family
    m.setf(EE_UNITS_COND_DAMAGE_FACTOR, 1, 4, 0, 0.0);
    assert_eq!(answer(&mut m, TRIGGER_SHOULD_I_RETALIATE), 0);
    assert_eq!(answer(&mut m, TRIGGER_CAN_DAMAGE_ATTACKER), 0);
    // a player-initiated goal keeps its course
    m.setf(EE_UNITS_COND_DAMAGE_FACTOR, 1, 4, 0, 0.5);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_LOCATION) | 0x100);
    assert_eq!(answer(&mut m, TRIGGER_SHOULD_I_RETALIATE), 0);
}

#[test]
fn a_defenseless_building_target_leaves_the_unit_idle() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING), 1, "no goal");
    let bclass = m.class(0x3000, category(EE_CLASS_KIND_BUILDING, 0), 3);
    let target = m.unit(0x300, bclass);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_ATTACK_UNIT));
    m.set(EE_UNITS_COND_GOAL_TARGET, UNIT, ACTIVE_OBJ, 0, target as i64);
    // a unit target: only while another hits it out of reach
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING), 0);
    m.set(EE_UNITS_COND_LAST_ATTACKER, UNIT, 0, 0, 0x777);
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING), 1);
    m.units.get_mut(&target).unwrap().land_building = true;
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING), 1, "an unarmed building");
    m.units.get_mut(&target).unwrap().stats_attack = 5.0;
    m.set(EE_UNITS_COND_CAN_BE_ATTACKED, 0, CLASS, 0, 1);
    m.setf(EE_UNITS_COND_DAMAGE_FACTOR, 0, 0, 0, 1.0);
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING), 0, "an armed one");
}

#[test]
fn a_guard_closes_on_the_unit_that_hit_it() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    let attacker = m.unit(0x200, CLASS);
    m.set(EE_UNITS_COND_BEHAVIOR, UNIT, 0, 0, 1 | 8);
    assert_eq!(answer(&mut m, TRIGGER_CAN_I_MOVE_FREELY), 0);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_ATTACK_UNIT));
    m.set(EE_UNITS_COND_GOAL_TARGET, UNIT, ACTIVE_OBJ, 0, attacker as i64);
    m.set(EE_UNITS_COND_LAST_ATTACKER, UNIT, 0, 0, attacker as i64);
    assert_eq!(answer(&mut m, TRIGGER_CAN_I_MOVE_FREELY), 1);
    m.units.get_mut(&attacker).unwrap().unit.mHitPoints = 0;
    assert_eq!(answer(&mut m, TRIGGER_CAN_I_MOVE_FREELY), 0, "a dead attacker");
}

#[test]
fn the_shots_answer_through_combat_and_the_landing_kinds_fall_back() {
    for (kind, combat) in [
        (EE_CLASS_KIND_TORPEDO, CombatKind::Torpedo),
        (EE_CLASS_KIND_MISSILE, CombatKind::Missile),
        (EE_CLASS_KIND_PROJECTILE, CombatKind::Projectile),
    ] {
        let not = EE_CLASS_KIND_TORPEDO | EE_CLASS_KIND_MISSILE;
        let mut m = with(category(kind, if kind == EE_CLASS_KIND_PROJECTILE { not } else { 0 }));
        assert_eq!(condition(&mut m, CLASS, UNIT, TRIGGER_ALWAYS_TRUE), Answer::Combat(combat));
    }
    let mut m = with(category(EE_CLASS_KIND_LANDABLE, 0));
    assert_eq!(answer(&mut m, TRIGGER_ALWAYS_TRUE), 1, "the common answer");
    m.landing = Some(0);
    assert_eq!(answer(&mut m, TRIGGER_ALWAYS_TRUE), 0, "the landing answer");
}

#[test]
fn dying_is_done_by_kind() {
    // a fortress below -2; a building below -2 or a space building at once
    let mut m = with(category(EE_CLASS_KIND_FORTRESS, 0));
    m.units.get_mut(&UNIT).unwrap().location[2] = -1.0;
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 0);
    m.units.get_mut(&UNIT).unwrap().location[2] = -2.5;
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 1);
    let plain = EE_CLASS_KIND_GATE | EE_CLASS_KIND_MINING_BUILDING | EE_CLASS_KIND_FORTRESS;
    let mut m = with(category(
        EE_CLASS_KIND_BUILDING,
        plain | EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING | EE_CLASS_KIND_BUILDING_PRIEST,
    ));
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 0);
    m.units.get_mut(&UNIT).unwrap().class.mUnitType = UNIT_TYPE_SPACE_DOCK as _;
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 1);
    // a ship once sunk; a spaceship at once; a helicopter below its tile
    let mut m = with(category(EE_CLASS_KIND_NAVAL, EE_CLASS_KIND_NAVAL_TRANSPORT));
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 0);
    m.set(EE_UNITS_COND_SUNK, UNIT, 0, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 1);
    let mut m = with(category(EE_CLASS_KIND_SPACESHIP, EE_CLASS_KIND_PLANET_SPACESHIP));
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 1);
    let mut m = with(category(EE_CLASS_KIND_HELICOPTER, EE_CLASS_KIND_HELICOPTER_PRIEST));
    m.setf(EE_UNITS_COND_TILE, UNIT, 0, 0, 3.0);
    m.units.get_mut(&UNIT).unwrap().location[2] = 2.0;
    assert_eq!(answer(&mut m, TRIGGER_DYING_ANIMATION_FINISHED), 1);
    m.set(EE_UNITS_COND_WAYPOINTS, UNIT, 0, 0, 2);
    assert_eq!(answer(&mut m, TRIGGER_MOVE_PREPARATION_COMPLETE), 1, "waypoints, no path flag asked");
}

#[test]
fn the_citizen_starts_its_next_build_and_its_goal_answers_its_triggers() {
    let mut m = with(category(EE_CLASS_KIND_LAND_CITIZEN, 0));
    assert_eq!(answer(&mut m, TRIGGER_BUILD_QUEUE_EMPTY), 1);
    assert_eq!(answer(&mut m, TRIGGER_BUILDING_IN_BUILD_QUEUE), 0);
    assert!(!m.log.contains(&(EE_UNITS_COND_BUILD_QUEUE, 1)), "an empty queue pops nothing");
    m.set(EE_UNITS_COND_BUILD_QUEUE, UNIT, 0, 0, 2);
    m.set(EE_UNITS_COND_BUILD_QUEUE, UNIT, 0, 1, 1);
    assert_eq!(answer(&mut m, TRIGGER_BUILDING_IN_BUILD_QUEUE), 1);
    // a gather trigger: the active goal's answer, 2 when it is not the current goal; none: false
    assert_eq!(answer(&mut m, 4009), 0);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 0, ACTIVE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_CONDITION, UNIT, ACTIVE_OBJ, 4009, 1);
    assert_eq!(answer(&mut m, 4009), 2);
    assert_eq!(answer(&mut m, TRIGGER_SMART_TASK_NOT_ASSIGNED), 0, "no current goal");
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, ACTIVE_OBJ, 0, i64::from(GOAL_BUILD_REPAIR));
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_REPAIR_SITE), 1);
}

#[test]
fn the_priests_conversion_triggers() {
    let mut m = with(category(EE_CLASS_KIND_LAND_PRIEST, 0));
    let attacker = 0x200;
    m.units.get_mut(&UNIT).unwrap().cx.mPower = 50;
    m.set(EE_UNITS_COND_CONVERSION_COST, UNIT, 0, 0, 60);
    assert_eq!(answer(&mut m, TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION), 0, "no attacker");
    m.set(EE_UNITS_COND_LAST_ATTACKER, UNIT, 0, 0, attacker);
    assert_eq!(answer(&mut m, TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION), 0);
    m.units.get_mut(&UNIT).unwrap().cx.mPower = 60;
    assert_eq!(answer(&mut m, TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION), 1);
    m.set(EE_UNITS_COND_CAN_CONVERT, UNIT, attacker as u64, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_ATTACKER_IS_LEGAL_CONVERSION_TARGET), 1);
    // no current goal: not a conversion
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_CONVERSION), 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_CONVERSION), 0);
    m.set(EE_UNITS_COND_GOAL, UNIT, 0, 1, STATE_OBJ as i64);
    m.set(EE_UNITS_COND_GOAL_INFO, UNIT, STATE_OBJ, 0, i64::from(GOAL_ATTACK_UNIT));
    m.set(EE_UNITS_COND_ATTACK_GOAL, UNIT, 0, 0, 1 | 2);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_CONVERSION), 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_CONVERSION), 0);
    assert_eq!(answer(&mut m, TRIGGER_CONVERT_GOAL_STILL_VALID), 1);
    assert_eq!(answer(&mut m, TRIGGER_ENEMY_UNIT_CONVERTED), 0);
    // a crusader's attack that is not a conversion
    m.set(EE_UNITS_COND_CRUSADER, UNIT, 0, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_CONVERSION), 0);
    assert_eq!(answer(&mut m, TRIGGER_GOAL_IS_NOT_CONVERSION), 1);
}

#[test]
fn the_temple_asks_the_building_then_the_priest() {
    let mut m = with(category(EE_CLASS_KIND_BUILDING_PRIEST, 0));
    m.set(EE_UNITS_COND_LAST_ATTACKER, UNIT, 0, 0, 0x200);
    m.set(EE_UNITS_COND_CAN_CONVERT, UNIT, 0x200, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_ATTACKER_IS_LEGAL_CONVERSION_TARGET), 1);
    // the building's answer came first: its common answer asked the (absent) active goal
    assert_eq!(m.log[0].0, EE_UNITS_COND_GOAL);
    assert_eq!(answer(&mut m, TRIGGER_LAUNCHED_AIRPLANE), 1);
}

#[test]
fn the_gate_and_the_mine() {
    let mut m = with(category(EE_CLASS_KIND_GATE, 0));
    m.units.get_mut(&UNIT).unwrap().cx.mOpenFraction = 1.0;
    m.units.get_mut(&UNIT).unwrap().cx.mIsLocked = true;
    assert_eq!(answer(&mut m, TRIGGER_IS_OPEN), 1);
    assert_eq!(answer(&mut m, TRIGGER_IS_CLOSED), 0);
    assert_eq!(answer(&mut m, TRIGGER_LOCKED), 1);
    assert_eq!(answer(&mut m, TRIGGER_NOT_LOCKED), 0);
    m.set(EE_UNITS_COND_MEMBER, UNIT, CLASS, MEMBER_PASSER_NEAR, 1);
    assert_eq!(answer(&mut m, TRIGGER_UNIT_IN_OPENING_RANGE), 1);
    assert_eq!(answer(&mut m, TRIGGER_SPECIAL_UNIT_NOT_IN_OPENING_RANGE), 1);
    let mut m = with(category(EE_CLASS_KIND_MINING_BUILDING, 0));
    assert_eq!(answer(&mut m, TRIGGER_RESOURCE_DEPLETED), 0);
    m.set(EE_UNITS_COND_MINE_DEPLETED, UNIT, 0, 0, 1);
    assert_eq!(answer(&mut m, TRIGGER_RESOURCE_DEPLETED), 1);
    assert!(m.asked(EE_UNITS_COND_MINE_DEPLETED));
}

#[test]
fn the_waypoint_and_location_statements_are_the_hosts() {
    let mut m = with(category(EE_CLASS_KIND_LAND, 0));
    m.set(EE_UNITS_COND_WAYPOINT_STEP, UNIT, 0, 2, 1);
    assert_eq!(answer(&mut m, TRIGGER_ARRIVED_AT_MOVE_WAYPOINT), 1);
    assert_eq!(answer(&mut m, TRIGGER_NO_MORE_WAYPOINTS), 0);
    m.set(EE_UNITS_COND_OFF_MAP, UNIT, 0, 1, 1);
    assert_eq!(answer(&mut m, TRIGGER_LOCATION_IN_SPACE), 1);
    assert_eq!(answer(&mut m, TRIGGER_LOCATION_OFF_MAP), 0);
    // the bottom of space: the position at or below -6
    m.points.insert((EE_UNITS_COND_POSITION, UNIT), [1.0, 1.0, -6.0]);
    assert_eq!(answer(&mut m, TRIGGER_REACHED_BOTTOM_OF_SPACE), 1);
    m.set(EE_UNITS_COND_TILE, UNIT, 0, 0, i64::from(TERRAIN_SPACE));
    assert_eq!(answer(&mut m, TRIGGER_UNIT_HIT_SPACE), 1);
}
