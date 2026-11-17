use super::*;

/// A host that logs every operation and answers from a small table.
#[derive(Default)]
struct Log {
    ops: Vec<(i32, u64, u64, [i64; 8])>,
    answers: std::collections::BTreeMap<i32, i64>,
}

impl GoalHost for Log {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.op_ab(code, unit, 0, args)
    }
    fn op_ab(&mut self, code: i32, a: u64, b: u64, args: &mut EeCombatGoalArgs) -> i64 {
        self.ops.push((code, a, b, *args));
        *self.answers.get(&code).unwrap_or(&0)
    }
}

fn run(op: i32, kind: u8, goal: u64, a: u64, b: u64, h: &mut Log, point: &mut [f32; 3]) -> (i64, String) {
    let mut ints = [0i64; 8];
    let mut text = String::new();
    let r = body(op, kind, goal, a, b, &mut ints, point, h, &mut |t: &str| text.push_str(t));
    (r, text)
}

#[test]
fn the_ground_goal_through_the_dispatch() {
    let goal = 0x9001;
    let mut h = Log::default();
    let mut p = [1.5f32, -2.25, 0.0];
    run(EE_COMBAT_GOAL_NEW_GROUND, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut p);
    let mut q = [0.0f32; 3];
    run(OP_LOCATION, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q);
    assert_eq!(q, p);
    let (_, line) = run(OP_DEBUG_LINE, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q);
    assert_eq!(line, " attack grnd 1.50,-2.25:");
    let (_, dump) = run(OP_DUMP, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q);
    assert_eq!(dump, "OUT OF SYNCH DUMP:  Attack Ground Location(1.500000, -2.250000, 0.000000)\n");
    assert_eq!(run(OP_START_ACTION, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q).0, 2013);
    assert_eq!(run(OP_MAX_PATH_ATTEMPTS, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q).0, 3);
    // Suits: the projectile class, CBGO with the target point
    h.answers.insert(EE_COMBAT_G_PROJECTILE_CLASS, 1);
    assert_eq!(run(OP_SUITS, GOAL_ATTACK_GROUND, goal, 0x50, 0, &mut h, &mut q).0, 1);
    let capture = h.ops.last().unwrap();
    assert_eq!(
        (capture.0, capture.3[0], capture.3[3], capture.3[4]),
        (EE_COMBAT_G_CAPTURE, 2, 1, i64::from(1.5f32.to_bits()))
    );
    // the clone: a new goal object of the kind with the base goal's flag, the same target
    h.answers.insert(HOST_NEW_GOAL, 0x9002);
    assert_eq!(run(OP_CLONE, GOAL_ATTACK_GROUND, goal, 0, 1, &mut h, &mut q).0, 0x9002);
    assert!(h.ops.iter().any(|o| o.0 == HOST_NEW_GOAL && o.1 == u64::from(GOAL_ATTACK_GROUND) && o.2 == 1));
    let mut r = [0.0f32; 3];
    run(OP_LOCATION, GOAL_ATTACK_GROUND, 0x9002, 0, 0, &mut h, &mut r);
    assert_eq!(r, p);
    run(OP_DESTROY, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q);
    run(OP_DESTROY, GOAL_ATTACK_GROUND, 0x9002, 0, 0, &mut h, &mut q);
    run(OP_LOCATION, GOAL_ATTACK_GROUND, goal, 0, 0, &mut h, &mut q);
    assert_eq!(q, [0.0; 3], "the record went");
}

#[test]
fn the_attack_goal_through_the_dispatch() {
    let goal = 0x9101;
    let mut h = Log::default();
    let mut p = [0.0f32; 3];
    let mut ints = [2i64, 0, 0, 0, 0, 0, 0, 0];
    body(EE_COMBAT_GOAL_NEW_ATTACK, GOAL_ATTACK_UNIT, goal, 0, 0, &mut ints, &mut p, &mut h, &mut |_| {});
    let mut x = attack_of(goal);
    x.sidestep = 4;
    x.target = 0x77;
    with_goals(|g| g.attack.insert(goal, x));
    // SetPathAttempts(0) clears the sidestep count; another value keeps it
    run(OP_SET_PATH_ATTEMPTS, GOAL_ATTACK_UNIT, goal, 5, 0, &mut h, &mut p);
    assert_eq!(attack_of(goal).sidestep, 4);
    run(OP_SET_PATH_ATTEMPTS, GOAL_ATTACK_UNIT, goal, 0, 0, &mut h, &mut p);
    assert_eq!(attack_of(goal).sidestep, 0);
    // the path attempts plus one
    h.answers.insert(EE_COMBAT_G_PATH_ATTEMPTS, 6);
    assert_eq!(run(OP_MAX_PATH_ATTEMPTS, GOAL_ATTACK_UNIT, goal, 0, 0, &mut h, &mut p).0, 7);
    // the owner
    run(OP_SET_OWNER, GOAL_ATTACK_UNIT, goal, 0x66, 0, &mut h, &mut p);
    assert_eq!(attack_of(goal).attacker, 0x66);
    // the step and the trigger test go to the C++ with the kind, the target and the goal
    run(OP_STEP, GOAL_ATTACK_UNIT, goal, 0x66, 0, &mut h, &mut p);
    let step = h.ops.last().unwrap();
    assert_eq!((step.0, step.1, step.3[0], step.3[1], step.3[2]), (EE_COMBAT_G_STEP, 0x66, 0, 0x77, goal as i64));
    h.answers.insert(EE_COMBAT_G_EVALUATE, 1);
    assert_eq!(run(OP_EVALUATE, GOAL_ATTACK_UNIT, goal, 0x66, 2013, &mut h, &mut p).0, 1);
    // the debug line
    h.answers.insert(HOST_UNIT_ID, 0x0300_0042);
    h.answers.insert(HOST_HIT_POINTS, 25);
    assert_eq!(run(OP_DEBUG_LINE, GOAL_ATTACK_UNIT, goal, 0, 0, &mut h, &mut p).1, "attack unit (uid 66, hp 25):");
    with_goals(|g| g.attack.remove(&goal));
}
