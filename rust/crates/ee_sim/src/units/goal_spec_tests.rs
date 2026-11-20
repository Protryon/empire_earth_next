use super::*;

// units by id: 7 -> 0x700, 8 -> 0x800; a complex unit 0x1700 and a player 0x2700 for each
fn unit(id: u32) -> u64 {
    match id {
        7 | 8 => u64::from(id) << 8,
        _ => 0,
    }
}
fn complex_of(u: u64) -> u64 {
    u + 0x1000
}
fn player_of(u: u64) -> u64 {
    u + 0x2000
}
fn plan_of(spec: &EeGoalSpec) -> Option<GoalPlan> {
    plan(spec, &unit, &complex_of, &player_of)
}

#[test]
fn explore_patrol_and_ground() {
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_EXPLORE);
    s.flag_a = 1;
    assert_eq!(plan_of(&s), Some(GoalPlan::new(GOAL_EXPLORE, true, EE_UNITS_GOAL_NEW_EXPLORE, 0, 0)));
    let s = EeGoalSpec::new(EE_GOAL_CTOR_PATROL);
    assert_eq!(plan_of(&s), Some(GoalPlan::new(GOAL_PATROL, false, EE_UNITS_GOAL_PATROL_NEW_EMPTY, 0, 0)));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_PATROL_ROUTE);
    s.object = 0x5000 as *mut c_void;
    s.unit_a = 8;
    let p = plan_of(&s).unwrap();
    assert_eq!((p.kind, p.op, p.a, p.b), (GOAL_PATROL, EE_UNITS_GOAL_NEW_PATROL, 0x5000, 0x800));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_GROUND);
    s.point = [1.5, 2.5, 3.5];
    let p = plan_of(&s).unwrap();
    assert_eq!((p.kind, p.op, p.f), (GOAL_ATTACK_GROUND, EE_COMBAT_GOAL_NEW_GROUND, [1.5, 2.5, 3.5, 0.0]));
}

#[test]
fn units_resolved_or_none() {
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_UNIT);
    s.unit_a = 7;
    s.unit_b = 8;
    s.flag_a = 1;
    s.flag_b = 1;
    let p = plan_of(&s).unwrap();
    assert_eq!(
        (p.kind, p.arg, p.op, p.a, p.b, p.i[0]),
        (GOAL_ATTACK_UNIT, true, EE_COMBAT_GOAL_NEW_ATTACK, 0x700, 0x800, 1)
    );
    s.unit_b = 9; // gone
    assert_eq!(plan_of(&s), None);
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_GATHER);
    s.unit_b = 7; // no resource: the C++ made the goal anyway
    let p = plan_of(&s).unwrap();
    assert_eq!((p.op, p.a, p.b), (EE_UNITS_GOAL_NEW_GATHER, 0, 0x700));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_HEAL_UNIT);
    assert_eq!(plan_of(&s), None);
    s.unit_a = 7;
    assert_eq!(plan_of(&s).map(|p| (p.kind, p.a)), Some((GOAL_HEAL_UNIT, 0x700)));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_BUILD_REPAIR);
    s.unit_a = 8;
    assert_eq!(plan_of(&s).map(|p| (p.kind, p.a, p.b)), Some((GOAL_BUILD_REPAIR, 0x2800, 0x800)));
}

#[test]
fn locations() {
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION);
    s.point = [4.0, 5.0, 6.0];
    s.flag_a = 1;
    s.value = 0.75;
    s.flag_c = 1;
    let p = plan_of(&s).unwrap();
    assert_eq!((p.kind, p.arg, p.op), (GOAL_LOCATION, true, EE_UNITS_GOAL_NEW_LOCATION));
    assert_eq!((p.i, p.f, p.group_movement), ([1, 0, 0, 0], [4.0, 5.0, 6.0, 0.75], Some(true)));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION_XY);
    s.point = [4.0, 5.0, 6.0];
    let p = plan_of(&s).unwrap();
    assert_eq!((p.arg, p.i, p.f, p.group_movement), (false, [1, 0, 0, 0], [4.0, 5.0, 0.0, 0.0], None));
    s.ctor = EE_GOAL_CTOR_LOCATION_2D;
    let p = plan_of(&s).unwrap();
    assert_eq!((p.arg, p.i, p.f), (true, [1, 0, 0, 0], [4.0, 5.0, 0.0, 0.0]));
}

#[test]
fn unload_and_cast_take_longs() {
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_UNLOAD_TRANSPORT);
    s.unit_a = 7;
    s.point = [12.9, -3.7, 0.0];
    let p = plan_of(&s).unwrap();
    assert_eq!((p.kind, p.a, p.i[0], p.i[1]), (GOAL_UNLOAD_TRANSPORT, 0x1700, 12, -3));
    let mut s = EeGoalSpec::new(EE_GOAL_CTOR_CAST_CALAMITY);
    s.unit_a = 7;
    s.ints = [5, 0x1_0000_0006, -1];
    let p = plan_of(&s).unwrap();
    assert_eq!((p.kind, p.a, p.b, &p.i[..3]), (GOAL_CAST_CALAMITY, 0x700, 0, &[5, 6, -1][..]));
    assert_eq!(plan_of(&EeGoalSpec::new(999)), None);
}
