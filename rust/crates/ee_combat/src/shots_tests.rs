//! The shots' plans and the heal goal's work over a mock host (no game, data or window).

use std::collections::BTreeMap;

use super::*;

/// Answers reads from a table (0 by default), logs every operation with its arguments.
#[derive(Default)]
struct Mock {
    reads: BTreeMap<i32, i32>,
    log: Vec<(i32, i32, i32)>,
}

impl Mock {
    fn with(pairs: &[(i32, i32)]) -> Mock {
        Mock { reads: pairs.iter().copied().collect(), log: Vec::new() }
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|&(op, _, _)| op).filter(|&op| op >= EE_COMBAT_SHOT_SET_ANIMATION).collect()
    }
}

impl ShotHost for Mock {
    fn op(&mut self, op: i32, arg: i32, arg1: i32) -> i32 {
        self.log.push((op, arg, arg1));
        self.reads.get(&op).copied().unwrap_or(0)
    }
}

fn f(v: f32) -> i32 {
    v.to_bits() as i32
}

#[test]
fn a_projectile_hit_on_land() {
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_COMMAND, 6007),
        (EE_COMBAT_SHOT_R_LOCATION_X, f(3.5)),
        (EE_COMBAT_SHOT_R_LOCATION_Y, f(4.5)),
        (EE_COMBAT_SHOT_R_LOCATION_Z, f(0.5)),
        (EE_COMBAT_SHOT_R_TILE_HEIGHT, f(1.0)),
        (EE_COMBAT_SHOT_R_GOAL_KIND, 2),
        (EE_COMBAT_SHOT_R_DEATH_EFFECT, 1),
        (EE_COMBAT_SHOT_R_DEATH_SOUND, 1),
        (EE_COMBAT_SHOT_R_LINE_OF_SIGHT, f(2.0)),
        (EE_COMBAT_SHOT_R_REMAINS, 1),
        (EE_COMBAT_SHOT_R_IS_PROJECTILE, 1),
    ]);
    action_changed(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    assert_eq!(
        m.ops(),
        vec![
            EE_COMBAT_SHOT_GROUND_LOCATION,
            EE_COMBAT_SHOT_DEATH_EFFECT,
            EE_COMBAT_SHOT_DEATH_SOUND,
            EE_COMBAT_SHOT_PHYSICS_ACTION,
            EE_COMBAT_SHOT_SET_ANIMATION,
            EE_COMBAT_SHOT_DROP_PATH,
            EE_COMBAT_SHOT_FIRE_SHOT,
            EE_COMBAT_SHOT_SIGHT_OFF,
            EE_COMBAT_SHOT_SET_HIT_POINTS,
            EE_COMBAT_SHOT_SIGHT_ON,
            EE_COMBAT_SHOT_START_ACTION,
            EE_COMBAT_SHOT_ACTION_CHANGED,
            EE_COMBAT_SHOT_CREATE_REMAINS,
        ]
    );
    assert!(m.log.contains(&(EE_COMBAT_SHOT_START_ACTION, A_DEAD, 0)), "an attack-unit goal: dead");
    assert!(m.log.contains(&(EE_COMBAT_SHOT_PHYSICS_ACTION, 2, 0)));
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_ANIMATION, 4, 0)));
}

#[test]
fn a_splash_in_water_and_a_parachute_cargo() {
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_COMMAND, 6007),
        (EE_COMBAT_SHOT_R_LOCATION_X, f(7.75)),
        (EE_COMBAT_SHOT_R_LOCATION_Y, f(2.25)),
        (EE_COMBAT_SHOT_R_LOCATION_Z, f(-0.5)),
        (EE_COMBAT_SHOT_R_TILE_HEIGHT, f(-1.0)),
        (EE_COMBAT_SHOT_R_GOAL_KIND, 3),
        (EE_COMBAT_SHOT_R_SPLASH_EFFECT, 1),
        (EE_COMBAT_SHOT_R_DEATH_EFFECT, 1),
        (EE_COMBAT_SHOT_R_UNIT_TYPE, TYPE_PARACHUTE_BOMB),
        (EE_COMBAT_SHOT_R_CARGO, 2),
        (EE_COMBAT_SHOT_R_REMAINS, 1),
        (EE_COMBAT_SHOT_R_IS_PROJECTILE, 1),
        (EE_COMBAT_SHOT_R_WORLD_CLEARING, 1),
    ]);
    action_changed(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    let ops = m.ops();
    assert_eq!(ops[0], EE_COMBAT_SHOT_SPLASH);
    assert!(!ops.contains(&EE_COMBAT_SHOT_DEATH_EFFECT) && !ops.contains(&EE_COMBAT_SHOT_FIRE_SHOT));
    assert!(m.log.contains(&(EE_COMBAT_SHOT_UNLOAD_CARGO, f(7.75), f(2.25))));
    assert!(ops.contains(&EE_COMBAT_SHOT_KILL_CARGO));
    assert!(!ops.contains(&EE_COMBAT_SHOT_SIGHT_OFF), "no line of sight");
    assert!(m.log.contains(&(EE_COMBAT_SHOT_START_ACTION, A_DYING, 0)), "an attack-ground goal: dying");
    assert!(!ops.contains(&EE_COMBAT_SHOT_CREATE_REMAINS), "the world is clearing");
}

#[test]
fn flaming_arrows_roll_against_the_power() {
    let arrows = |roll: f32| {
        let mut m = Mock::with(&[
            (EE_COMBAT_SHOT_R_COMMAND, 6007),
            (EE_COMBAT_SHOT_R_UNIT_TYPE, TYPE_FLAMING_ARROW),
            (EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND, 2),
            (EE_COMBAT_SHOT_R_FLAMING_SET, 1),
            (EE_COMBAT_SHOT_R_RANDOM, f(roll)),
            (EE_COMBAT_SHOT_R_FLAMING_CHANCE, f(25.0)),
        ]);
        action_changed(EE_COMBAT_SHOT_PROJECTILE, &mut m);
        assert!(m.log.iter().any(|&(op, _, _)| op == EE_COMBAT_SHOT_R_RANDOM), "the draw is made");
        m.ops().contains(&EE_COMBAT_SHOT_CREATE_CALAMITY)
    };
    assert!(arrows(0.25));
    assert!(!arrows(0.2501));
    // a target out of the set: no draw
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_COMMAND, 6007),
        (EE_COMBAT_SHOT_R_UNIT_TYPE, TYPE_FLAMING_ARROW),
        (EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND, 2),
    ]);
    action_changed(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    assert!(!m.log.iter().any(|&(op, _, _)| op == EE_COMBAT_SHOT_R_RANDOM));
}

#[test]
fn the_other_actions() {
    let mut m = Mock::with(&[(EE_COMBAT_SHOT_R_COMMAND, 6002), (EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME, 4000)]);
    action_changed(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    assert_eq!(m.ops(), vec![EE_COMBAT_SHOT_SET_ANIMATION, EE_COMBAT_SHOT_SET_ROTTING]);
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_ROTTING, 4000, 0)));
    for (kind, hp, want) in [
        (EE_COMBAT_SHOT_PROJECTILE, 0, vec![EE_COMBAT_SHOT_ATTACK_TARGET_TILE]),
        (EE_COMBAT_SHOT_PROJECTILE, 5, vec![EE_COMBAT_SHOT_RETARGET_CONTAINER]),
        (EE_COMBAT_SHOT_MISSILE, 0, vec![EE_COMBAT_SHOT_START_ACTION, EE_COMBAT_SHOT_ACTION_CHANGED]),
        (EE_COMBAT_SHOT_MISSILE, 5, vec![EE_COMBAT_SHOT_RETARGET_CONTAINER]),
    ] {
        let mut m = Mock::with(&[(EE_COMBAT_SHOT_R_COMMAND, 14000), (EE_COMBAT_SHOT_R_TARGET_HIT_POINTS, hp)]);
        action_changed(kind, &mut m);
        assert_eq!(m.ops(), want);
    }
    let mut m = Mock::with(&[(EE_COMBAT_SHOT_R_COMMAND, 10003)]);
    action_changed(EE_COMBAT_SHOT_MISSILE, &mut m);
    assert_eq!(m.ops(), vec![EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED]);
    let mut m = Mock::with(&[(EE_COMBAT_SHOT_R_COMMAND, 2033)]);
    action_changed(EE_COMBAT_SHOT_MISSILE, &mut m);
    assert_eq!(
        m.ops(),
        vec![EE_COMBAT_SHOT_ATTACK_GOAL_LOCATION, EE_COMBAT_SHOT_START_ACTION, EE_COMBAT_SHOT_ACTION_CHANGED]
    );
}

#[test]
fn a_missile_hit_dies_with_its_attack_goal_or_as_an_icbm() {
    let hit = |goal: i32, ty: i32, hp: i32, cost: i32| {
        let mut m = Mock::with(&[
            (EE_COMBAT_SHOT_R_COMMAND, 6007),
            (EE_COMBAT_SHOT_R_GOAL_KIND, goal),
            (EE_COMBAT_SHOT_R_UNIT_TYPE, ty),
            (EE_COMBAT_SHOT_R_HIT_POINTS, hp),
            (EE_COMBAT_SHOT_R_POPULATION_COST, cost),
        ]);
        action_changed(EE_COMBAT_SHOT_MISSILE, &mut m);
        m
    };
    let m = hit(2, 0, 1, 3);
    assert_eq!(
        m.ops()[3..].to_vec(),
        vec![
            EE_COMBAT_SHOT_FIRE_SHOT,
            EE_COMBAT_SHOT_START_ACTION,
            EE_COMBAT_SHOT_ACTION_CHANGED,
            EE_COMBAT_SHOT_POPULATION,
            EE_COMBAT_SHOT_MINIMAP_OFF
        ]
    );
    assert!(m.log.contains(&(EE_COMBAT_SHOT_POPULATION, 3, 0)));
    let m = hit(3, TYPE_CP_ICBM, 0, 0);
    assert_eq!(
        m.ops()[3..].to_vec(),
        vec![EE_COMBAT_SHOT_START_ACTION, EE_COMBAT_SHOT_ACTION_CHANGED, EE_COMBAT_SHOT_MINIMAP_OFF]
    );
    let m = hit(3, 0, 1, 0);
    assert_eq!(m.ops()[3..].to_vec(), vec![EE_COMBAT_SHOT_FIRE_SHOT]);
}

#[test]
fn the_projectile_tick() {
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_PHYSICS_MODEL, 1),
        (EE_COMBAT_SHOT_R_COMMAND, 6002),
        (EE_COMBAT_SHOT_R_ROTTING, 30),
        (EE_COMBAT_SHOT_R_TICK, 50),
        (EE_COMBAT_SHOT_R_FLIGHT_TIME, 100),
    ]);
    update(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    assert_eq!(
        m.ops(),
        vec![
            EE_COMBAT_SHOT_STEP_PHYSICS,
            EE_COMBAT_SHOT_ORBITALS,
            EE_COMBAT_SHOT_EFFECTS,
            EE_COMBAT_SHOT_REFRESH_SOUND,
            EE_COMBAT_SHOT_SET_ROTTING,
            EE_COMBAT_SHOT_SET_FLIGHT_TIME
        ]
    );
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_ROTTING, 0, 0)), "at 0");
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_FLIGHT_TIME, 150, 0)));
    // off the map: no step; a zero tick counts 1 ms; fading in space below z 0
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_PHYSICS_MODEL, 1),
        (EE_COMBAT_SHOT_R_TILE_X, -10000),
        (EE_COMBAT_SHOT_R_COMMAND, 6009),
        (EE_COMBAT_SHOT_R_POSITION_Z, f(-3.0)),
    ]);
    update(EE_COMBAT_SHOT_PROJECTILE, &mut m);
    assert_eq!(m.ops()[0], EE_COMBAT_SHOT_ORBITALS);
    let opacity = 1.0f32 - -3.0f32 * -0.16666667f32;
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_OPACITY, f(opacity), 0)));
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_FLIGHT_TIME, 1, 0)));
}

#[test]
fn the_missile_and_torpedo_ticks() {
    let mut m = Mock::with(&[
        (EE_COMBAT_SHOT_R_TICK, 40),
        (EE_COMBAT_SHOT_R_FLIGHT_TIME, 30),
        (EE_COMBAT_SHOT_R_CLOAKED, 1000),
        (EE_COMBAT_SHOT_R_LOCATION_Z, f(0.25)),
    ]);
    update(EE_COMBAT_SHOT_TORPEDO, &mut m);
    assert_eq!(
        m.ops(),
        vec![
            EE_COMBAT_SHOT_EFFECTS,
            EE_COMBAT_SHOT_REFRESH_SOUND,
            EE_COMBAT_SHOT_SET_FLIGHT_TIME,
            EE_COMBAT_SHOT_SET_CLOAKED,
            EE_COMBAT_SHOT_WATER_PLANE
        ]
    );
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_FLIGHT_TIME, 0, 0)));
    assert!(m.log.contains(&(EE_COMBAT_SHOT_SET_CLOAKED, 960, 0)));
    // no flight time, a lifetime run out (or past 2^31 as a long): neither is set
    let mut m = Mock::with(&[(EE_COMBAT_SHOT_R_CLOAKED, i32::MIN)]);
    update(EE_COMBAT_SHOT_MISSILE, &mut m);
    assert_eq!(m.ops(), vec![EE_COMBAT_SHOT_EFFECTS, EE_COMBAT_SHOT_REFRESH_SOUND]);
}

#[test]
fn the_heal_goal_works_each_second() {
    let i = EeCombatHealWorkIn {
        command: A_HEAL_UNIT,
        has_target: 1,
        target_hp: 50,
        same_player: 1,
        elapsed: 980,
        tick_ms: 25,
        healing: 8,
        max_hp: 55,
        ..Default::default()
    };
    assert_eq!(heal_work(&i), EeCombatHealWorkOut { work: 1, elapsed: 5, heal: 1, hit_points: 55 });
    assert_eq!(
        heal_work(&EeCombatHealWorkIn { elapsed: 10, ..i }),
        EeCombatHealWorkOut { work: 1, elapsed: 35, heal: 0, hit_points: 0 }
    );
    for no in [
        EeCombatHealWorkIn { command: 0, ..i },
        EeCombatHealWorkIn { has_target: 0, ..i },
        EeCombatHealWorkIn { cinematic: 1, ..i },
        EeCombatHealWorkIn { target_hp: 0, ..i },
        EeCombatHealWorkIn { same_player: 0, target_enemy_of: 1, ..i },
    ] {
        assert_eq!(heal_work(&no), EeCombatHealWorkOut::default());
    }
    // another player's unit that is not at war with the healer's is healed
    assert_eq!(heal_work(&EeCombatHealWorkIn { same_player: 0, ..i }).heal, 1);
}
