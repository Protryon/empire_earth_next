//! goals.rs: the trigger tests and steps over mock hosts (every lazy read and operation, in order).

use super::*;

/// Fixed answers per read code; the codes read, in order.
#[derive(Default)]
struct Answers {
    answers: Vec<(i32, u32)>,
    asked: Vec<i32>,
}

impl Answers {
    fn with(answers: &[(i32, u32)]) -> Self {
        Answers { answers: answers.to_vec(), asked: Vec::new() }
    }
}

impl ConditionHost for &mut Answers {
    fn read(&mut self, code: i32) -> u32 {
        self.asked.push(code);
        self.answers.iter().find(|(c, _)| *c == code).map_or(0, |(_, v)| *v)
    }
}

fn rec(kind: i32, event: u32) -> EeCombatConditionIn {
    EeCombatConditionIn { kind, event, goal_kind: -1, ..Default::default() }
}

fn target(mut r: EeCombatConditionIn) -> EeCombatConditionIn {
    r.has_target = 1;
    r.target_hp = 100;
    r.target_max_hp = 200;
    r.target_tile = [10, 12];
    r.goal_tile = [10, 12];
    r.target_complex = 1;
    r
}

fn run(mut r: EeCombatConditionIn, answers: &[(i32, u32)]) -> (EeCombatConditionOut, Vec<i32>, EeCombatConditionIn) {
    let mut host = Answers::with(answers);
    let out = condition(&mut r, &mut host);
    (out, host.asked, r)
}

fn result(r: EeCombatConditionIn, answers: &[(i32, u32)]) -> i32 {
    run(r, answers).0.result
}

#[test]
fn a_destroyed_target_and_the_hidden_test_it_falls_into() {
    let base = target(rec(EE_COMBAT_GOAL_ATTACK_UNIT, T_ENEMY_UNIT_DESTROYED));
    assert_eq!(result(EeCombatConditionIn { target_hp: 0, ..base }, &[]), 1);
    assert_eq!(result(EeCombatConditionIn { target_tile: [OFF_MAP, 3], ..base }, &[]), 1);
    assert_eq!(result(base, &[]), 0);
    let hidden = EeCombatConditionIn { target_cloakable: 1, target_cloaked: 1, target_enemy: 1, ..base };
    assert_eq!(result(hidden, &[]), 1, "a living cloaked enemy counts as destroyed");
    assert_eq!(result(EeCombatConditionIn { target_cloak_radius: 3, ..hidden }, &[]), 0);
    assert_eq!(result(EeCombatConditionIn { target_same_player: 1, ..hidden }, &[]), 0);
    assert_eq!(result(EeCombatConditionIn { target_enemy: 0, ..hidden }, &[]), 0);
    assert_eq!(result(EeCombatConditionIn { event: T_ENEMY_UNIT_UNDETECTABLE, ..hidden }, &[]), 1);
    assert_eq!(result(EeCombatConditionIn { event: T_ENEMY_UNIT_UNDETECTABLE, has_target: 0, ..hidden }, &[]), 0);
    let (_, asked, r) = run(hidden, &[]);
    assert!(asked.is_empty());
    assert_eq!(r.reads, 0);
}

#[test]
fn the_weapon_range_test_of_a_converting_crusader_is_the_range_to_hit() {
    let base = target(rec(EE_COMBAT_GOAL_ATTACK_UNIT, T_UNIT_IN_WEAPON_RANGE));
    let (out, asked, r) =
        run(EeCombatConditionIn { convert: 1, crusader: 1, ..base }, &[(EE_COMBAT_READ_IN_RANGE_TO_HIT, 1)]);
    assert_eq!((out.result, asked), (1, vec![EE_COMBAT_READ_IN_RANGE_TO_HIT]));
    assert_eq!((r.reads, r.log[0]), (1, EeCombatRead { code: EE_COMBAT_READ_IN_RANGE_TO_HIT, value: 1 }));
    let (out, asked, _) = run(EeCombatConditionIn { convert: 1, ..base }, &[(EE_COMBAT_READ_IN_RANGE_TO_HIT, 1)]);
    assert_eq!((out.result, asked), (0, vec![EE_COMBAT_READ_IN_REACH]));
}

#[test]
fn the_sight_tests_read_the_line_of_sight_only_when_the_original_does() {
    let base = target(rec(EE_COMBAT_GOAL_ATTACK_UNIT, T_ENEMY_UNIT_NO_LONGER_VISIBLE));
    let los = (EE_COMBAT_READ_LINE_OF_SIGHT, 4.0f32.to_bits());
    // a human player's unit: no line of sight read
    let (out, asked, _) = run(EeCombatConditionIn { human: 1, ..base }, &[los]);
    assert_eq!((out.result, asked), (1, vec![EE_COMBAT_READ_SEEN]));
    // a computer's: the range past the line of sight, then the seen test
    let (out, asked, r) = run(EeCombatConditionIn { range: 5.0, ..base }, &[los, (EE_COMBAT_READ_SEEN, 1)]);
    assert_eq!((out.result, asked), (0, vec![EE_COMBAT_READ_LINE_OF_SIGHT, EE_COMBAT_READ_SEEN]));
    assert_eq!(r.log[0].value, 4.0f32.to_bits());
    let (out, asked, _) = run(EeCombatConditionIn { range: 4.0, ..base }, &[los]);
    assert_eq!((out.result, asked), (0, vec![EE_COMBAT_READ_LINE_OF_SIGHT]));
    // left the line of sight: the seen test past it, else the squared distance against its square
    let left = EeCombatConditionIn { event: T_ENEMY_UNIT_LEFT_LOS, ..base };
    let (out, asked, _) = run(EeCombatConditionIn { range: 5.0, ..left }, &[los]);
    assert_eq!((out.result, asked), (1, vec![EE_COMBAT_READ_LINE_OF_SIGHT, EE_COMBAT_READ_SEEN]));
    let far = (EE_COMBAT_READ_DISTANCE_SQUARED, 16.5f32.to_bits());
    let (out, asked, _) = run(EeCombatConditionIn { range: 1.0, ..left }, &[los, far]);
    assert_eq!((out.result, asked), (1, vec![EE_COMBAT_READ_LINE_OF_SIGHT, EE_COMBAT_READ_DISTANCE_SQUARED]));
    assert_eq!(
        result(
            EeCombatConditionIn { range: 1.0, ..left },
            &[los, (EE_COMBAT_READ_DISTANCE_SQUARED, 16.0f32.to_bits())]
        ),
        0
    );
}

#[test]
fn the_attack_goals_plain_triggers() {
    let base = target(rec(EE_COMBAT_GOAL_ATTACK_UNIT, 0));
    let at =
        |event, r: EeCombatConditionIn| result(EeCombatConditionIn { event, ..r }, &[(EE_COMBAT_READ_LINE_BLOCKED, 1)]);
    assert_eq!(at(T_UNIT_NOT_ON_MAP, EeCombatConditionIn { target_tile: [OFF_MAP, 0], ..base }), 1);
    assert_eq!(at(T_UNIT_NOT_ON_MAP, base), 0);
    assert_eq!(at(T_READY_TO_FIRE_AGAIN, EeCombatConditionIn { reload_timer: 5, reload_time: 5, ..base }), 1);
    assert_eq!(at(T_READY_TO_FIRE_AGAIN, EeCombatConditionIn { reload_timer: 4, reload_time: 5, ..base }), 0);
    assert_eq!(at(T_ATTACKER_IS_RANGED_UNIT, EeCombatConditionIn { range: 1.0, ..base }), 0);
    assert_eq!(at(T_ATTACKER_IS_RANGED_UNIT, EeCombatConditionIn { range: 1.5, ..base }), 1);
    let building = EeCombatConditionIn { target_land_building: 1, ..base };
    assert_eq!(at(T_TARGET_IS_NOT_DEFENSIVE_BUILDING, building), 1);
    assert_eq!(at(T_TARGET_IS_NOT_DEFENSIVE_BUILDING, EeCombatConditionIn { target_attack: 2.0, ..building }), 0);
    assert_eq!(at(T_TARGET_IS_NOT_DEFENSIVE_BUILDING, EeCombatConditionIn { has_target: 0, ..building }), 0);
    assert_eq!(at(T_BLOCKED_BY_WALL, base), 1);
    let (out, asked, _) = run(EeCombatConditionIn { event: T_BLOCKED_BY_WALL, fire_over_walls: 1, ..base }, &[]);
    assert_eq!((out.result, asked.len()), (0, 0), "a unit firing over walls asks nothing");
    assert_eq!(at(T_ENEMY_IS_BUILDING, EeCombatConditionIn { target_family: FAMILY_BUILDING, ..base }), 1);
    assert_eq!(at(T_ENEMY_IS_BUILDING, base), 0);
    // cease fire: never at war with an enemy's unit; otherwise unless the unit is in a herd
    let enemy = EeCombatConditionIn { target_enemy: 1, ..base };
    assert_eq!(at(T_CEASE_FIRE, enemy), 0);
    assert_eq!(at(T_CEASE_FIRE, base), 1);
    assert_eq!(at(T_CEASE_FIRE, EeCombatConditionIn { herd: 1, ..base }), 0);
    assert_eq!(at(T_CEASE_FIRE, EeCombatConditionIn { target_same_player: 1, ..enemy }), 1);
    let moved = EeCombatConditionIn { target_tile: [11, 12], ..base };
    assert_eq!((at(T_ENEMY_UNIT_MOVED, moved), at(T_ENEMY_UNIT_MOVED, base)), (1, 0));
    assert_eq!((at(T_ENEMY_UNIT_HAS_NOT_MOVED_ONE_TILE, moved), at(T_ENEMY_UNIT_HAS_NOT_MOVED_ONE_TILE, base)), (0, 1));
    // moving: a living complex target with speed the unit does not face
    let moving = EeCombatConditionIn { event: T_ENEMY_UNIT_MOVING, target_velocity: 0.5, ..base };
    assert_eq!(run(moving, &[]).1, vec![EE_COMBAT_READ_FACES_MOVING]);
    assert_eq!(result(moving, &[]), 1);
    assert_eq!(result(moving, &[(EE_COMBAT_READ_FACES_MOVING, 1)]), 0);
    assert!(run(EeCombatConditionIn { target_velocity: 0.0, ..moving }, &[]).1.is_empty());
    assert!(run(EeCombatConditionIn { target_complex: 0, ..moving }, &[]).1.is_empty());
    // the common goal: only the facing-not-required trigger
    assert_eq!(at(T_UNIT_FACING_NOT_REQUIRED, base), 1);
    assert_eq!(at(77, base), 0);
}

#[test]
fn the_attack_goals_host_triggers_are_one_read_each() {
    for (event, code) in [
        (T_ENEMY_UNIT_INSIDE_FIRING_ARCH, EE_COMBAT_READ_FACES_ARCH),
        (T_FACING_ENEMY_UNIT, EE_COMBAT_READ_FACES),
        (T_UNIT_ON_TILE_BORDERING_TARGET, EE_COMBAT_READ_BORDERING),
        (T_TARGET_IS_SPECIAL_ENEMY, EE_COMBAT_READ_GATE_CLOSED),
        (T_ENEMY_UNIT_REACHABLE, EE_COMBAT_READ_STILL_REACHABLE),
        (T_ENEMY_UNIT_IN_LOS, EE_COMBAT_READ_ENEMY_IN_RANGE),
    ] {
        let r = target(rec(EE_COMBAT_GOAL_ATTACK_UNIT, event));
        assert_eq!(run(r, &[(code, 1)]).0.result, 1);
        let (out, asked, _) = run(r, &[]);
        assert_eq!((out.result, asked), (0, vec![code]));
    }
}

#[test]
fn the_attack_ground_goals_triggers() {
    let base = rec(EE_COMBAT_GOAL_ATTACK_GROUND, 0);
    let at = |event, r: EeCombatConditionIn, a: &[(i32, u32)]| run(EeCombatConditionIn { event, ..r }, a);
    assert_eq!(at(T_LOCATION_IN_WEAPON_RANGE, base, &[(EE_COMBAT_READ_CAN_REACH_TILE, 1)]).0.result, 1);
    assert_eq!(at(T_LOCATION_INSIDE_FIRING_ARCH, base, &[]).1, vec![EE_COMBAT_READ_FACES_POINT]);
    assert_eq!(at(T_ATTACKER_IS_RANGED_UNIT, EeCombatConditionIn { range: 2.0, ..base }, &[]).0.result, 1);
    // ready to fire: the timer at 0 (not the attack goal's timer against the reload time)
    assert_eq!(at(T_READY_TO_FIRE_AGAIN, EeCombatConditionIn { reload_time: 9, ..base }, &[]).0.result, 1);
    assert_eq!(
        at(T_READY_TO_FIRE_AGAIN, EeCombatConditionIn { reload_timer: 9, reload_time: 9, ..base }, &[]).0.result,
        0
    );
    assert_eq!(at(T_FLY_OFF_MAP_AFTER_ATTACK, EeCombatConditionIn { paratrooper_bomber: 1, ..base }, &[]).0.result, 1);
    assert_eq!(at(T_UNIT_FACING_NOT_REQUIRED, base, &[]).0.result, 1);
    assert!(at(T_UNIT_FACING_NOT_REQUIRED, base, &[]).1.is_empty());
}

#[test]
fn the_heal_goals_triggers() {
    let base = target(rec(EE_COMBAT_GOAL_HEAL, 0));
    let at = |event, r: EeCombatConditionIn, a: &[(i32, u32)]| run(EeCombatConditionIn { event, ..r }, a);
    // out of sight: only a human player's healer asks
    assert_eq!(at(T_HEALEE_NO_LONGER_VISIBLE, base, &[]).1.len(), 0);
    assert_eq!(at(T_HEALEE_NO_LONGER_VISIBLE, base, &[]).0.result, 0);
    let human = EeCombatConditionIn { human: 1, ..base };
    assert_eq!(at(T_HEALEE_NO_LONGER_VISIBLE, human, &[]).0.result, 1);
    assert_eq!(at(T_HEALEE_NO_LONGER_VISIBLE, human, &[(EE_COMBAT_READ_SEEN, 1)]).0.result, 0);
    assert_eq!(at(T_HEALEE_DESTROYED, EeCombatConditionIn { target_hp: 0, ..base }, &[]).0.result, 1);
    assert_eq!(at(T_HEALEE_DESTROYED, base, &[]).0.result, 0);
    assert_eq!(at(T_FACING_HEALEE, base, &[]).1, vec![EE_COMBAT_READ_FACES]);
    assert_eq!(at(T_GOAL_IS_HEAL, base, &[]).0.result, 1);
    assert_eq!(at(T_HEALEE_IN_HEALING_RANGE, base, &[]).1, vec![EE_COMBAT_READ_HEAL_IN_RANGE]);
    // needs healing: repairable, at or below the threshold (100 of 200), not an enemy's
    let needs = EeCombatConditionIn { heal_threshold: 0.5, ..base };
    let rep = [(EE_COMBAT_READ_REPAIRABLE, 1)];
    assert_eq!(at(T_TARGET_NEEDS_HEALING, needs, &rep).0.result, 1);
    assert_eq!(at(T_TARGET_NEEDS_HEALING, EeCombatConditionIn { target_hp: 101, ..needs }, &rep).0.result, 0);
    assert_eq!(at(T_TARGET_NEEDS_HEALING, EeCombatConditionIn { target_enemy_of: 1, ..needs }, &rep).0.result, 0);
    let ours = EeCombatConditionIn { target_enemy_of: 1, target_same_player: 1, ..needs };
    assert_eq!(at(T_TARGET_NEEDS_HEALING, ours, &rep).0.result, 1);
    let (out, asked, _) = at(T_TARGET_NEEDS_HEALING, needs, &[]);
    assert_eq!((out.result, asked), (0, vec![EE_COMBAT_READ_REPAIRABLE]));
    // the location goal's test for the others
    let (out, asked, _) = at(T_UNIT_FACING_NOT_REQUIRED, base, &[(EE_COMBAT_READ_BASE, 1)]);
    assert_eq!((out.result, asked), (1, vec![EE_COMBAT_READ_BASE]));
}

#[test]
fn a_shot_that_hits_a_unit_attacks_it() {
    let base = rec(EE_COMBAT_GOAL_PROJECTILE, T_PROJECTILE_HIT_UNIT);
    let found = (EE_COMBAT_READ_IMPACT_UNIT, 1);
    let (out, asked, _) = run(base, &[found, (EE_COMBAT_READ_TARGETABLE, 1)]);
    assert_eq!(out, EeCombatConditionOut { result: 1, action: EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT });
    assert_eq!(asked, vec![EE_COMBAT_READ_IMPACT_UNIT, EE_COMBAT_READ_TARGETABLE]);
    // a unit its class may not target: a hit without an attack
    assert_eq!(run(base, &[found]).0, EeCombatConditionOut { result: 1, action: EE_COMBAT_CONDITION_NONE });
    assert_eq!(run(base, &[]).0, EeCombatConditionOut::default());
    // the missile attacks anything it hits, and with nothing hit ends when its lifetime flag is 0
    let missile = EeCombatConditionIn { kind: EE_COMBAT_GOAL_MISSILE, ..base };
    let (out, asked, _) = run(missile, &[found]);
    assert_eq!(out, EeCombatConditionOut { result: 1, action: EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT });
    assert_eq!(asked, vec![EE_COMBAT_READ_IMPACT_UNIT]);
    assert_eq!(run(missile, &[]).0.result, 1);
    assert_eq!(run(EeCombatConditionIn { cloaked: 1, ..missile }, &[]).0.result, 0);
}

#[test]
fn the_shots_other_triggers() {
    for kind in [EE_COMBAT_GOAL_PROJECTILE, EE_COMBAT_GOAL_MISSILE] {
        let base = rec(kind, 0);
        let at = |event, r: EeCombatConditionIn, a: &[(i32, u32)]| run(EeCombatConditionIn { event, ..r }, a);
        for event in [T_DYING_ANIMATION_FINISHED, T_RESOURCES_ROTTED_AWAY, T_UNIT_ROTTED_AWAY] {
            let attacking = EeCombatConditionIn { goal_kind: GOAL_ATTACK_UNIT, ..base };
            let (out, asked, _) = at(event, attacking, &[]);
            assert_eq!((out.result, asked.len()), (1, 0));
            assert_eq!(at(event, base, &[(EE_COMBAT_READ_BASE, 5)]).0.result, 1, "the base test's long as a flag");
            assert_eq!(at(event, base, &[]).0.result, 0);
        }
        assert_eq!(at(T_PROJECTILE_HIT_GROUND, base, &[(EE_COMBAT_READ_GROUND_IMPACT, 1)]).0.result, 1);
        assert_eq!(at(T_PROJECTILE_HIT_WATER, base, &[]).1, vec![EE_COMBAT_READ_WATER_IMPACT]);
        assert_eq!(at(T_NEW_TARGET_SELECTED, base, &[]).0.result, 1);
        // a target no longer valid: dead, or put into a container; only under an attack-unit goal
        let goal = target(EeCombatConditionIn { goal_kind: GOAL_ATTACK_UNIT, ..base });
        assert_eq!(at(T_UNIT_TARGET_NO_LONGER_VALID, goal, &[]).0.result, 0);
        assert_eq!(at(T_UNIT_TARGET_NO_LONGER_VALID, EeCombatConditionIn { target_hp: 0, ..goal }, &[]).0.result, 1);
        assert_eq!(
            at(T_UNIT_TARGET_NO_LONGER_VALID, EeCombatConditionIn { target_container: 1, ..goal }, &[]).0.result,
            1
        );
        let other = EeCombatConditionIn { goal_kind: 3, target_hp: 0, ..goal };
        assert_eq!(at(T_UNIT_TARGET_NO_LONGER_VALID, other, &[]).0.result, 0);
        // the base class's long, returned as it is
        let (out, asked, _) = at(4242, base, &[(EE_COMBAT_READ_BASE, 7)]);
        assert_eq!((out.result, asked), (7, vec![EE_COMBAT_READ_BASE]));
    }
    // the flight time: the projectile's own test, the missile's goes to the base class
    let p = EeCombatConditionIn {
        event: T_FLIGHT_TIME_ALARM,
        flight_time: 3,
        flight_time_max: 3,
        ..rec(EE_COMBAT_GOAL_PROJECTILE, 0)
    };
    assert_eq!(run(p, &[]).0.result, 1);
    assert_eq!(run(EeCombatConditionIn { flight_time: 2, ..p }, &[]).0.result, 0);
    let (out, asked, _) = run(EeCombatConditionIn { kind: EE_COMBAT_GOAL_MISSILE, ..p }, &[(EE_COMBAT_READ_BASE, 0)]);
    assert_eq!((out.result, asked), (0, vec![EE_COMBAT_READ_BASE]));
    // space: nothing for the missile, the base class for the projectile
    let s = rec(EE_COMBAT_GOAL_MISSILE, T_UNIT_HIT_SPACE);
    assert!(run(s, &[]).1.is_empty());
    assert_eq!(run(EeCombatConditionIn { kind: EE_COMBAT_GOAL_PROJECTILE, ..s }, &[]).1, vec![EE_COMBAT_READ_BASE]);
}

#[test]
fn the_torpedo_ignores_water_and_hands_the_rest_to_the_missile() {
    let base = rec(EE_COMBAT_GOAL_TORPEDO, 0);
    let (out, asked, _) = run(EeCombatConditionIn { event: T_PROJECTILE_HIT_WATER, ..base }, &[]);
    assert_eq!((out.result, asked.len()), (0, 0));
    assert_eq!(
        run(EeCombatConditionIn { event: T_PROJECTILE_HIT_GROUND, ..base }, &[(EE_COMBAT_READ_GROUND_IMPACT, 1)])
            .0
            .result,
        1
    );
    let (out, asked, _) =
        run(EeCombatConditionIn { event: T_PROJECTILE_HIT_UNIT, ..base }, &[(EE_COMBAT_READ_BASE, 1)]);
    assert_eq!((out, asked), (EeCombatConditionOut { result: 1, action: 0 }, vec![EE_COMBAT_READ_BASE]));
    assert_eq!(run(rec(99, T_PROJECTILE_HIT_UNIT), &[]).0, EeCombatConditionOut::default());
}

#[test]
fn the_read_log_keeps_the_first_four_and_counts_them_all() {
    let mut r = rec(EE_COMBAT_GOAL_ATTACK_UNIT, 0);
    let mut host = Answers::with(&[(5, 9)]);
    let mut reads = Reads { rec: &mut r, host: &mut host };
    for code in 1..=6 {
        reads.read(code);
    }
    assert_eq!(r.reads, 6);
    assert_eq!(r.log.map(|e| e.code), [1, 2, 3, 4]);
    let mut full = EeCombatConditionIn { reads: 4, ..rec(0, 0) };
    let mut host = Answers::default();
    Reads { rec: &mut full, host: &mut host }.read(5);
    assert_eq!((full.reads, full.log), (5, [EeCombatRead::default(); 4]));
}

// ---- the steps

/// Answers per operation; the operations issued with their arguments.
#[derive(Default)]
struct World {
    answers: Vec<(i32, i32)>,
    issued: Vec<(i32, i32)>,
}

impl StepHost for &mut World {
    fn op(&mut self, op: i32, arg: i32) -> i32 {
        self.issued.push((op, arg));
        self.answers.iter().find(|(o, _)| *o == op).map_or(0, |(_, v)| *v)
    }
}

fn plan(kind: i32, command: u32, fire_over_walls: i32, answers: &[(i32, i32)]) -> (Vec<(i32, i32)>, EeCombatStepIn) {
    let mut r = EeCombatStepIn { kind, command, reload_time: 1500, fire_over_walls, ..Default::default() };
    let mut world = World { answers: answers.to_vec(), issued: Vec::new() };
    step(&mut r, &mut world);
    let logged: Vec<(i32, i32)> =
        r.log[..usize::try_from(r.count).unwrap().min(12)].iter().map(|o| (o.op, o.arg)).collect();
    assert_eq!(logged, world.issued, "the log is the plan as issued");
    (world.issued, r)
}

#[test]
fn the_attack_step_fires_on_the_sets_sound_frame_else_draws_a_reload_timer() {
    let frames = [(EE_COMBAT_STEP_ATTACK_FRAME_SET, 3), (EE_COMBAT_STEP_SOUND_FRAME, 7), (EE_COMBAT_STEP_FRAME, 7)];
    let (issued, r) = plan(EE_COMBAT_GOAL_ATTACK_UNIT, A_ATTACK_ENEMY_UNIT, 0, &frames);
    assert_eq!(
        issued,
        vec![
            (EE_COMBAT_STEP_PHYSICS_ACTION, 1),
            (EE_COMBAT_STEP_ATTACK_FRAME_SET, 0),
            (EE_COMBAT_STEP_SOUND_FRAME, 3),
            (EE_COMBAT_STEP_SET_ANIMATION, 3),
            (EE_COMBAT_STEP_FRAME, 0),
            (EE_COMBAT_STEP_FIRE_SHOT, 0),
            (EE_COMBAT_STEP_SEARCH_TIMER, 0),
            (EE_COMBAT_STEP_DROP_PATH, 0),
            (EE_COMBAT_STEP_PATH_ATTEMPTS, 0),
        ]
    );
    // reads keep their answers, actions 0
    assert_eq!(r.log[1].value, 3);
    assert_eq!(r.log[2].value, 7);
    assert_eq!(r.log[5].value, 0);
    let (issued, _) = plan(EE_COMBAT_GOAL_ATTACK_UNIT, A_ATTACK_ENEMY_UNIT, 0, &frames[..2]);
    assert_eq!(issued[5], (EE_COMBAT_STEP_RELOAD_TIMER, 1500));
}

#[test]
fn the_facing_steps_turn_until_they_face() {
    let turning = [
        (EE_COMBAT_STEP_PHYSICS_ACTION, 1),
        (EE_COMBAT_STEP_FACES, 0),
        (EE_COMBAT_STEP_ANIMATION, 0),
        (EE_COMBAT_STEP_SET_ANIMATION, TURNING_ANIMATION),
        (EE_COMBAT_STEP_DROP_PATH, 0),
    ];
    for (kind, command) in
        [(EE_COMBAT_GOAL_ATTACK_UNIT, A_FACE_ENEMY_UNIT), (EE_COMBAT_GOAL_HEAL, A_TURN_TO_FACE_HEALEE)]
    {
        assert_eq!(plan(kind, command, 0, &[]).0, turning);
        // already turning: no new animation; facing: no animation read
        let (issued, _) = plan(kind, command, 0, &[(EE_COMBAT_STEP_ANIMATION, TURNING_ANIMATION)]);
        assert_eq!(issued, [turning[0], turning[1], turning[2], turning[4]]);
        let (issued, _) = plan(kind, command, 0, &[(EE_COMBAT_STEP_FACES, 1)]);
        assert_eq!(issued, [turning[0], turning[1], turning[4]]);
    }
    assert!(plan(EE_COMBAT_GOAL_HEAL, A_FACE_ENEMY_UNIT, 0, &[]).0.is_empty());
    assert_eq!(
        plan(EE_COMBAT_GOAL_ATTACK_UNIT, A_CHANGE_TARGET_FOR_SPECIAL_ENEMY, 0, &[]).0,
        [(EE_COMBAT_STEP_RETARGET_GATE, 0)]
    );
    assert!(plan(EE_COMBAT_GOAL_ATTACK_UNIT, A_ATTACK_LOCATION, 0, &[]).0.is_empty());
}

#[test]
fn the_attack_ground_step_fires_high_over_walls() {
    let (issued, _) = plan(EE_COMBAT_GOAL_ATTACK_GROUND, A_FACE_LOCATION, 0, &[]);
    assert_eq!(issued, [(EE_COMBAT_STEP_PHYSICS_ACTION, 1), (EE_COMBAT_STEP_DROP_PATH, 0)]);
    // no firing-high read for a unit that cannot fire over walls: set 0
    let (issued, _) = plan(EE_COMBAT_GOAL_ATTACK_GROUND, A_ATTACK_LOCATION, 0, &[(EE_COMBAT_STEP_FIRING_HIGH, 1)]);
    assert_eq!(issued[1], (EE_COMBAT_STEP_SOUND_FRAME, 0));
    let (issued, _) = plan(EE_COMBAT_GOAL_ATTACK_GROUND, A_ATTACK_ENEMY_UNIT, 1, &[(EE_COMBAT_STEP_FIRING_HIGH, 1)]);
    assert_eq!(
        &issued[..4],
        [
            (EE_COMBAT_STEP_PHYSICS_ACTION, 1),
            (EE_COMBAT_STEP_FIRING_HIGH, 0),
            (EE_COMBAT_STEP_SOUND_FRAME, HIGH_FIRING_ANIMATION),
            (EE_COMBAT_STEP_SET_ANIMATION, HIGH_FIRING_ANIMATION),
        ]
    );
    // the sound frame 0 equals the frame 0: a shot
    assert!(issued.contains(&(EE_COMBAT_STEP_FIRE_SHOT, 0)));
    let (issued, _) = plan(EE_COMBAT_GOAL_ATTACK_GROUND, A_ATTACK_LOCATION, 1, &[]);
    assert_eq!(issued[2], (EE_COMBAT_STEP_SOUND_FRAME, 0));
}

#[test]
fn the_step_log_keeps_the_first_twelve_and_counts_them_all() {
    let mut r = EeCombatStepIn::default();
    let mut world = World::default();
    let mut p = Plan { rec: &mut r, host: &mut world };
    for op in 1..=14 {
        p.act(op, op * 10);
    }
    assert_eq!(r.count, 14);
    assert_eq!(r.log[11], EeCombatStepOp { op: 12, arg: 120, value: 0 });
}

#[test]
fn the_c_hosts_answer_through_their_tables() {
    unsafe extern "C" fn read(context: *mut c_void, code: i32) -> u32 {
        // SAFETY: the test passes a u32
        unsafe { *context.cast::<u32>() + u32::try_from(code).unwrap() }
    }
    unsafe extern "C" fn op(context: *mut c_void, op: i32, arg: i32) -> i32 {
        // SAFETY: the test passes an i32
        unsafe { *context.cast::<i32>() + op + arg }
    }
    let mut base = 100u32;
    let host = EeCombatConditionHost { context: (&raw mut base).cast(), read: Some(read) };
    let mut r = rec(EE_COMBAT_GOAL_TORPEDO, 1);
    // SAFETY: a valid record and table
    let out = unsafe { ee_combat_condition(&raw mut r, &host) };
    assert_eq!(out.result, 100 + EE_COMBAT_READ_BASE);
    assert_eq!(r.log[0], EeCombatRead { code: EE_COMBAT_READ_BASE, value: 114 });
    let mut base = 0i32;
    let host = EeCombatStepHost { context: (&raw mut base).cast(), op: Some(op) };
    let mut s = EeCombatStepIn { kind: EE_COMBAT_GOAL_HEAL, command: A_TURN_TO_FACE_HEALEE, ..Default::default() };
    // SAFETY: a valid record and table
    unsafe { ee_combat_goal_step(&raw mut s, &host) };
    assert_eq!(s.count, 3);
    assert_eq!(s.log[1], EeCombatStepOp { op: EE_COMBAT_STEP_FACES, arg: 0, value: 2 });
}
