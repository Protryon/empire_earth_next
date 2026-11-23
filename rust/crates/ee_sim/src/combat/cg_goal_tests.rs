use super::*;
use std::collections::HashMap;

/// A recording mock: each statement (op, a, b, c); answers by (op, a), else by op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    by_object: HashMap<(i32, u64), i64>,
    by_op: HashMap<i32, i64>,
    base: Vec<(u64, u32)>,
    targetable: Vec<u64>,
    lines: Vec<u64>,
    conditions: Vec<(EeCombatConditionIn, EeCombatConditionOut)>,
    steps: Vec<EeCombatStepIn>,
    turned: Vec<GeTransformation>,
}

impl CgHost for Mock {
    fn cg(&mut self, op: i32, _unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c));
        match op {
            EE_UNITS_CG_GROUND_TARGET => {
                // SAFETY: the body's point
                unsafe { *(b as usize as *mut [f32; 3]) = [2.5, -0.5, 0.0] };
            }
            EE_UNITS_CG_LOCATION => {
                // SAFETY: the body's matrix: the identity (facing +y)
                let m = unsafe { &mut *(b as usize as *mut [f32; 16]) };
                *m = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0];
            }
            EE_UNITS_CG_SET_TRANSFORMATION => {
                // SAFETY: the body's transformation
                self.turned.push(unsafe { *(b as usize as *const GeTransformation) });
            }
            EE_UNITS_CG_LINE_OF_SIGHT => f[0] = 6.0,
            _ => {}
        }
        self.by_object.get(&(op, a)).or_else(|| self.by_op.get(&op)).copied().unwrap_or(0)
    }
    fn fill_condition(
        &mut self,
        kind: i32,
        _unit: u64,
        event: u32,
        _target: u64,
        _complex: u64,
    ) -> EeCombatConditionIn {
        EeCombatConditionIn { kind, event, ..Default::default() }
    }
    fn fill_step(&mut self, kind: i32, _unit: u64, _target: u64) -> EeCombatStepIn {
        EeCombatStepIn { kind, ..Default::default() }
    }
    fn base_condition(&mut self, class: u64, _unit: u64, event: u32) -> i32 {
        self.base.push((class, event));
        1
    }
    fn targetable(&mut self, _unit: u64, class: u64) -> bool {
        self.targetable.push(class);
        true
    }
    fn line_blocked(&mut self, _unit: u64, target: u64) -> bool {
        self.lines.push(target);
        true
    }
    fn capture_condition(&mut self, i: &EeCombatConditionIn, o: &EeCombatConditionOut) {
        self.conditions.push((*i, *o));
    }
    fn capture_step(&mut self, i: &EeCombatStepIn) {
        self.steps.push(*i);
    }
}

fn reads(m: &mut Mock, kind: i32) -> Reads<'_, Mock> {
    Reads { h: m, kind, event: 77, unit: 1, goal: 50, class: 60, impact: 0 }
}

#[test]
fn tag_is_first_letter_low() {
    assert_eq!(TAG_CBGS, 0x5347_4243);
}

#[test]
fn the_target_is_the_kinds() {
    let mut m = Mock::default();
    m.by_object.insert((EE_UNITS_CG_ATTACK_TARGET, 50), 7);
    m.by_object.insert((EE_UNITS_CG_IN_REACH, 7), 1);
    assert_eq!(reads(&mut m, EE_COMBAT_GOAL_ATTACK_UNIT).read_code(EE_COMBAT_READ_IN_REACH), 1);
    let mut m = Mock::default();
    reads(&mut m, EE_COMBAT_GOAL_HEAL).read_code(EE_COMBAT_READ_SEEN);
    assert_eq!(m.log[0].0, EE_UNITS_CG_HEAL_TARGET);
    assert_eq!((m.log[1].0, m.log[1].2), (EE_UNITS_CG_SEEN, 1));
    let mut m = Mock::default();
    reads(&mut m, EE_COMBAT_GOAL_ATTACK_UNIT).read_code(EE_COMBAT_READ_SEEN);
    assert_eq!((m.log[1].0, m.log[1].2), (EE_UNITS_CG_SEEN, 0));
}

#[test]
fn faces_moving_faces_the_complex_unit() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_CG_ATTACK_TARGET, 7);
    m.by_object.insert((EE_UNITS_CG_TARGET_COMPLEX, 7), 8);
    reads(&mut m, EE_COMBAT_GOAL_ATTACK_UNIT).read_code(EE_COMBAT_READ_FACES_MOVING);
    assert_eq!(m.log[2], (EE_UNITS_CG_FACES, 8, 1, 0));
}

#[test]
fn floats_are_bits_and_lines_are_rust() {
    let mut m = Mock::default();
    assert_eq!(reads(&mut m, 0).read_code(EE_COMBAT_READ_LINE_OF_SIGHT), 6.0f32.to_bits());
    m.by_op.insert(EE_UNITS_CG_ATTACK_TARGET, 7);
    assert_eq!(reads(&mut m, 0).read_code(EE_COMBAT_READ_LINE_BLOCKED), 1);
    assert_eq!(m.lines, vec![7]);
}

#[test]
fn base_by_kind() {
    let mut m = Mock::default();
    assert_eq!(reads(&mut m, EE_COMBAT_GOAL_PROJECTILE).read_code(EE_COMBAT_READ_BASE), 1);
    assert_eq!(m.base, vec![(60, 77)]);
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_CG_MISSILE_CONDITION, 1);
    assert_eq!(reads(&mut m, EE_COMBAT_GOAL_TORPEDO).read_code(EE_COMBAT_READ_BASE), 1);
    assert_eq!(m.log[0], (EE_UNITS_CG_MISSILE_CONDITION, 60, 77, 0));
    let mut m = Mock::default();
    assert_eq!(reads(&mut m, EE_COMBAT_GOAL_HEAL).read_code(EE_COMBAT_READ_BASE), 0);
    assert!(m.log.is_empty());
}

#[test]
fn heal_location_faces_yaw_zero() {
    let mut m = Mock::default();
    assert!(heal_location_condition(&mut m, 1, 9014));
    assert!(m.log.is_empty());
    assert!(heal_location_condition(&mut m, 1, 9015));
    assert_eq!(m.turned.len(), 1);
    assert!(!heal_location_condition(&mut m, 1, 1));
}

#[test]
fn can_reach_tile_floors_the_ground_target() {
    let mut m = Mock::default();
    reads(&mut m, EE_COMBAT_GOAL_ATTACK_GROUND).read_code(EE_COMBAT_READ_CAN_REACH_TILE);
    assert_eq!(m.log[1], (EE_UNITS_CG_CAN_REACH_TILE, 0, 2, -1));
}

#[test]
fn the_impact_unit_is_kept() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_CG_FIND_IMPACT, 9);
    m.by_object.insert((EE_UNITS_CG_CLASS, 9), 90);
    let mut r = reads(&mut m, EE_COMBAT_GOAL_PROJECTILE);
    assert_eq!(r.read_code(EE_COMBAT_READ_IMPACT_UNIT), 1);
    assert_eq!(r.read_code(EE_COMBAT_READ_TARGETABLE), 1);
    assert_eq!(m.targetable, vec![90]);
}

#[test]
fn condition_fills_the_attack_goal_and_records() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_CG_HUMAN, 1);
    m.by_op.insert(EE_UNITS_CG_ATTACK_TILE_X, 4);
    m.by_op.insert(EE_UNITS_CG_ATTACK_TILE_Y, 5);
    condition(&mut m, EE_COMBAT_GOAL_ATTACK_UNIT, 1, 3, 7, 50, 60);
    let (i, _) = m.conditions[0];
    assert_eq!((i.human, i.goal_tile), (1, [4, 5]));
    assert_eq!(m.log[0], (EE_UNITS_CG_TARGET_COMPLEX, 7, 0, 0));
    let mut m = Mock::default();
    condition(&mut m, EE_COMBAT_GOAL_HEAL, 1, 3, 0, 50, 60);
    assert!(!m.log.iter().any(|l| l.0 == EE_UNITS_CG_TARGET_COMPLEX || l.0 == EE_UNITS_CG_ATTACK_TILE_X));
    assert_eq!(m.conditions.len(), 1);
}

#[test]
fn step_faces_by_kind_and_records() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_CG_HEAL_TARGET, 8);
    {
        let mut s = Steps { h: &mut m, kind: EE_COMBAT_GOAL_HEAL, unit: 1, goal: 50 };
        (&mut s).op(EE_COMBAT_STEP_FACES, 0);
        (&mut s).op(EE_COMBAT_STEP_RELOAD_TIMER, 33);
    }
    assert_eq!(m.log[1], (EE_UNITS_CG_FACES, 8, 0, 0));
    assert_eq!(m.log[2], (EE_UNITS_CG_RELOAD_TIMER, 0, 33, 0));
    let mut m = Mock::default();
    step(&mut m, EE_COMBAT_GOAL_ATTACK_UNIT, 1, 50, 7);
    assert_eq!(m.steps.len(), 1);
}
