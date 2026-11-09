use crate::capture::{encode, records, replay, replay_all, Counts};
use crate::*;
use ee_ge::um::{um_arccos, um_arctan, um_cos, HALF_PI};

fn rules() -> EeCombatDamageIn {
    EeCombatDamageIn {
        form: DAMAGE_DIRECT,
        attack: 10.0,
        attack_factor: 1.0,
        weapon_factor: 1.0,
        armor: 2.0,
        elevation: 1.0,
        minimum_damage: 1,
        fanatic_factor: 2.0,
        elevation_step: 1.0,
        ally_factor: 0.0,
        herd_ally_factor: 0.1,
        trample_damage: 1,
        trample_threshold: 1.0,
        friendly_factor: 0.5,
        ..Default::default()
    }
}

fn amount(input: &EeCombatDamageIn) -> Option<i32> {
    let out = damage(input);
    (out.hit != 0).then_some(out.amount)
}

#[test]
fn ceil_and_truncate_follow_the_x86_conversions() {
    assert_eq!(ceil(0.8), 1);
    assert_eq!(ceil(2.0), 2);
    assert_eq!(ceil(-0.5), 0);
    assert_eq!(ceil(0.0), 0);
    assert_eq!(truncate(-50.0), -50);
    assert_eq!(truncate(-49.9), -49);
    assert_eq!(truncate(49.9), 49);
    assert_eq!(truncate(f32::NAN), i32::MIN);
    assert_eq!(truncate(3.0e9), i32::MIN);
    assert_eq!(truncate(-2_147_483_648.0), i32::MIN);
}

#[test]
fn attack_factor_scales_only_between_a_human_and_a_computer() {
    let base = EeCombatAttackFactorIn {
        factor: 0.75,
        attackable: 1,
        players_present: 1,
        has_rule: 1,
        scale_human_target: 0.5,
        scale_human_attacker: 2.0,
        ..Default::default()
    };
    assert_eq!(attack_factor(&base), 0.75);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { attacker_human: 1, target_human: 1, ..base }), 0.75);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { target_human: 1, ..base }), 0.375);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { attacker_human: 1, ..base }), 1.5);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { attacker_human: 1, has_rule: 0, ..base }), 0.75);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { attackable: 0, ..base }), 0.0);
    assert_eq!(attack_factor(&EeCombatAttackFactorIn { players_present: 0, ..base }), 0.0);
}

#[test]
fn a_direct_hit_is_to_hit_times_attack_less_armour() {
    assert_eq!(amount(&rules()), Some(8));
    assert_eq!(amount(&EeCombatDamageIn { attack_factor: 0.5, ..rules() }), Some(3));
    // the attack must be positive; a zero to-hit factor does nothing
    assert_eq!(amount(&EeCombatDamageIn { attack: 0.0, ..rules() }), None);
    assert_eq!(amount(&EeCombatDamageIn { attack: f32::NAN, ..rules() }), None);
    assert_eq!(amount(&EeCombatDamageIn { weapon_factor: 0.0, ..rules() }), None);
    // at least the minimum
    assert_eq!(amount(&EeCombatDamageIn { armor: 30.0, ..rules() }), Some(1));
    // rounded half to even
    assert_eq!(amount(&EeCombatDamageIn { armor: 7.5, ..rules() }), Some(2));
    assert_eq!(amount(&EeCombatDamageIn { armor: 6.5, ..rules() }), Some(4));
    // a fanatic doubles the attack before the armour
    assert_eq!(amount(&EeCombatDamageIn { fanatic: 1, ..rules() }), Some(18));
    assert_eq!(amount(&EeCombatDamageIn { fanatic: 1, form: DAMAGE_SPLASH, ..rules() }), Some(8));
}

#[test]
fn the_elevation_moves_damage_at_least_one_step() {
    assert_eq!(amount(&EeCombatDamageIn { elevation: 1.25, ..rules() }), Some(10));
    assert_eq!(amount(&EeCombatDamageIn { elevation: 1.05, ..rules() }), Some(9));
    assert_eq!(amount(&EeCombatDamageIn { elevation: 0.75, ..rules() }), Some(6));
    assert_eq!(amount(&EeCombatDamageIn { elevation: 0.95, ..rules() }), Some(7));
    assert_eq!(amount(&EeCombatDamageIn { elevation: 0.95, elevation_step: 0.0, ..rules() }), Some(8));
}

#[test]
fn own_and_allied_units_take_the_ally_share_of_a_direct_hit() {
    assert_eq!(amount(&EeCombatDamageIn { same_player: 1, ..rules() }), Some(0));
    assert_eq!(amount(&EeCombatDamageIn { allied: 1, ..rules() }), Some(0));
    assert_eq!(amount(&EeCombatDamageIn { allied: 1, herd: 1, ..rules() }), Some(1));
    assert_eq!(amount(&EeCombatDamageIn { allied: 1, herd: 1, armor: -10.0, ..rules() }), Some(2));
    // splash: the friendly factor on the own player's units, no ally share
    let splash = EeCombatDamageIn { form: DAMAGE_SPLASH, ..rules() };
    assert_eq!(amount(&splash), Some(8));
    assert_eq!(amount(&EeCombatDamageIn { same_player: 1, ..splash }), Some(4));
    assert_eq!(amount(&EeCombatDamageIn { allied: 1, ..splash }), Some(8));
    // splash is not gated on a positive attack
    assert_eq!(amount(&EeCombatDamageIn { attack: 0.0, armor: -3.0, ..splash }), Some(3));
}

#[test]
fn a_trample_does_its_fixed_damage_over_the_threshold() {
    let trample = EeCombatDamageIn { form: DAMAGE_TRAMPLE, attack: 5.0, armor: 2.5, trample_damage: 7, ..rules() };
    // the armour is rounded (2.5 -> 2): 1 x 3 > 1
    assert_eq!(amount(&trample), Some(7));
    assert_eq!(amount(&EeCombatDamageIn { armor: 4.0, ..trample }), None);
    // without a to-hit factor the armour is not taken off, and 0 x 5 is not above the threshold
    assert_eq!(amount(&EeCombatDamageIn { attack_factor: 0.0, ..trample }), None);
    assert_eq!(amount(&EeCombatDamageIn { attack_factor: 0.0, trample_threshold: -1.0, ..trample }), Some(7));
    // the elevation does not count
    assert_eq!(amount(&EeCombatDamageIn { elevation: 3.0, ..trample }), Some(7));
}

#[test]
fn elevation_needs_the_scaled_height_difference() {
    let base = EeCombatElevationIn {
        applies: 1,
        own_z: 0.0,
        target_z: 0.0,
        high_height: 1.0,
        low_height: 1.0,
        high_bonus: 0.25,
        high_factor: 1.5,
        low_factor: 0.5,
    };
    assert_eq!(elevation(&base), 1.0);
    assert_eq!(elevation(&EeCombatElevationIn { own_z: ELEVATION_SCALE, ..base }), 1.75);
    assert_eq!(elevation(&EeCombatElevationIn { own_z: 0.4, ..base }), 1.0);
    assert_eq!(elevation(&EeCombatElevationIn { target_z: ELEVATION_SCALE, ..base }), 0.5);
    assert_eq!(elevation(&EeCombatElevationIn { applies: 0, own_z: 10.0, ..base }), 1.0);
}

#[test]
fn morale_takes_off_and_adds_rounded_shares() {
    let base = EeCombatMoraleIn { value: 10, positive: 0, negative: 0, positive_share: 0.8, negative_share: 0.6 };
    assert_eq!(morale(&base), 10);
    assert_eq!(morale(&EeCombatMoraleIn { positive: 1, ..base }), 2);
    assert_eq!(morale(&EeCombatMoraleIn { negative: 3, ..base }), 16);
    assert_eq!(morale(&EeCombatMoraleIn { positive: 1, negative: 1, ..base }), 8);
    // the shares are of the input, rounded half to even: 5 x 0.5 = 2.5 -> 2
    assert_eq!(morale(&EeCombatMoraleIn { value: 5, positive: 1, positive_share: 0.5, ..base }), 3);
}

#[test]
fn a_hit_takes_hit_points_down_to_zero() {
    let hit = |hp, damage| hit_points(&EeCombatHitPointsIn { hit_points: hp, damage, max_hit_points: 100 });
    assert_eq!(hit(50, 10), EeCombatHitPointsOut { hit_points: 40, badly_hurt: 0 });
    assert_eq!(hit(15, 6), EeCombatHitPointsOut { hit_points: 9, badly_hurt: 1 });
    assert_eq!(hit(15, 5), EeCombatHitPointsOut { hit_points: 10, badly_hurt: 1 });
    assert_eq!(hit(10, 5), EeCombatHitPointsOut { hit_points: 5, badly_hurt: 0 });
    assert_eq!(hit(5, 50), EeCombatHitPointsOut { hit_points: 0, badly_hurt: 0 });
    assert_eq!(hit(i32::MIN, 1).hit_points, i32::MAX); // long arithmetic wraps as the C++'s
}

#[test]
fn the_damage_effect_starts_when_the_share_falls_below_its_threshold() {
    let effect = |old, hp| {
        damage_effect(&EeCombatDamageEffectIn {
            old_hit_points: old,
            hit_points: hp,
            max_hit_points: 200,
            threshold: 0.5,
        })
    };
    assert_eq!(effect(120, 90), 1);
    assert_eq!(effect(100, 99), 1);
    assert_eq!(effect(99, 50), 0);
    assert_eq!(effect(150, 100), 0);
}

#[test]
fn regeneration_heals_a_living_unit_once_per_interval() {
    let base = EeCombatRegenerateIn {
        timer: 500,
        tick_ms: 100,
        hit_points: 50,
        regeneration: 3,
        max_hit_points: 52,
        interval: 1000,
    };
    assert_eq!(regenerate(&base), EeCombatRegenerateOut { timer: 400, hit_points: 50, set: 0 });
    assert_eq!(
        regenerate(&EeCombatRegenerateIn { timer: 100, ..base }),
        EeCombatRegenerateOut { timer: 1000, hit_points: 52, set: 1 }
    );
    assert_eq!(
        regenerate(&EeCombatRegenerateIn { timer: 0, hit_points: 0, ..base }),
        EeCombatRegenerateOut { timer: 1000, hit_points: 0, set: 0 }
    );
    // a negative regeneration that kills does not set
    assert_eq!(regenerate(&EeCombatRegenerateIn { timer: 0, regeneration: -60, ..base }).set, 0);
}

#[test]
fn garrison_timer_heal_and_heal_step() {
    assert_eq!(
        timer(&EeCombatTimerIn { timer: 150, tick_ms: 100, interval: 1000 }),
        EeCombatTimerOut { timer: 50, due: 0 }
    );
    assert_eq!(
        timer(&EeCombatTimerIn { timer: 100, tick_ms: 100, interval: 1000 }),
        EeCombatTimerOut { timer: 1000, due: 1 }
    );
    assert_eq!(
        heal(&EeCombatHealIn { hit_points: 40, healing: 5, max_hit_points: 50 }),
        EeCombatHealOut { hit_points: 45, set: 1 }
    );
    assert_eq!(
        heal(&EeCombatHealIn { hit_points: 48, healing: 5, max_hit_points: 50 }),
        EeCombatHealOut { hit_points: 50, set: 1 }
    );
    assert_eq!(
        heal(&EeCombatHealIn { hit_points: 0, healing: 5, max_hit_points: 50 }),
        EeCombatHealOut { hit_points: 5, set: 0 }
    );
    assert_eq!(
        heal_step(&EeCombatHealStepIn { elapsed: 900, tick_ms: 100 }),
        EeCombatHealStepOut { elapsed: 1000, due: 0 }
    );
    assert_eq!(
        heal_step(&EeCombatHealStepIn { elapsed: 1000, tick_ms: 0 }),
        EeCombatHealStepOut { elapsed: 1, due: 1 }
    );
    assert_eq!(
        heal_step(&EeCombatHealStepIn { elapsed: 950, tick_ms: 100 }),
        EeCombatHealStepOut { elapsed: 50, due: 1 }
    );
}

#[test]
fn conversion_costs_and_chances() {
    assert_eq!(conversion_cost(&EeCombatConversionCostIn { cost: 100, power: 150 }), 100);
    assert_eq!(conversion_cost(&EeCombatConversionCostIn { cost: 100, power: 60 }), 60);
    assert_eq!(conversion_cost(&EeCombatConversionCostIn { cost: 100, power: 0 }), 0);
    assert_eq!(conversion_cost(&EeCombatConversionCostIn { cost: -5, power: 10 }), 0);
    let base = EeCombatConversionIn {
        work_timer: 900,
        tick_seconds: 0.05,
        resistance: 0.5,
        adaptation: 0,
        rate: 0.4,
        factor: 1.0,
    };
    assert_eq!(conversion(&base), EeCombatConversionOut { timer: 950, ..Default::default() });
    let due = conversion(&EeCombatConversionIn { work_timer: 950, ..base });
    assert_eq!(due, EeCombatConversionOut { timer: 1000, due: 1, threshold: 0.2, miss_timer: 0 });
    // a negative timer is a large unsigned one: due
    assert_eq!(conversion(&EeCombatConversionIn { work_timer: -100, ..base }).due, 1);
    // full resistance: no chance, unless adaptation makes it 0.99
    let full = EeCombatConversionIn { work_timer: 1000, resistance: 1.0, ..base };
    assert_eq!(conversion(&full).threshold, 0.0);
    assert_eq!(conversion(&EeCombatConversionIn { adaptation: 1, ..full }).threshold, (1.0f32 - 0.99) * 1.0 * 0.4);
    assert_eq!(conversion(&EeCombatConversionIn { adaptation: 1, ..base }).threshold, 0.0); // not due
                                                                                            // the tick is truncated: 0.0333 s is 33 ms
    assert_eq!(conversion(&EeCombatConversionIn { tick_seconds: 0.0333, ..base }).timer, 933);
}

#[test]
fn the_ballistic_pitch_has_a_low_and_a_high_arc() {
    let base = EeCombatBallisticIn { to_x: 3.0, to_y: 4.0, speed: 10.0, acceleration: 10.0, ..Default::default() };
    let low = ballistic(&base);
    let high = ballistic(&EeCombatBallisticIn { high: 1, ..base });
    assert_eq!(low.reached, 1);
    assert_eq!(high.reached, 1);
    assert!(low.angle < high.angle);
    // distance 5, a = -(10 x 25) / (10 x 10 x 2) = -1.25, discriminant 25 - 1.25 x 1.25 x 4 = 18.75
    let a = -(10.0f32 * 25.0) / (10.0 * 10.0 * 2.0);
    let root = (25.0f32 - (a - 0.0) * a * 4.0).sqrt();
    let (first, second) = (um_arctan((root - 5.0) / (a + a)), um_arctan((-5.0 - root) / (a + a)));
    assert_eq!(low.angle, first.min(second));
    assert_eq!(high.angle, first.max(second));
    let far = ballistic(&EeCombatBallisticIn { to_x: 300.0, ..base });
    assert_eq!(far, EeCombatBallisticOut { reached: 0, angle: std::f32::consts::FRAC_PI_4 });
    assert_eq!(std::f32::consts::FRAC_PI_4.to_bits(), 0x3f49_0fdb); // the C++'s 0.7853982f
}

#[test]
fn the_arc_falls_under_gravity_and_a_meteor_flies_straight() {
    let base = EeCombatArcIn {
        flight_time_ms: 1000,
        gravity: 10.0,
        pitch: 0.5,
        launcher_speed: 20.0,
        own_speed: 5.0,
        ..Default::default()
    };
    let out = arc(&base);
    assert_eq!(out.move_y, 20.0 * um_cos(0.5) * 1.0);
    assert_eq!(out.move_z, 20.0 * um_cos(HALF_PI - 0.5) * 1.0 - 1.0 * 10.0 * 0.5);
    assert!(out.pitch < 0.5);
    // the launcher's speed unless it is (almost) zero
    let own = arc(&EeCombatArcIn { launcher_speed: 0.001, ..base });
    assert_eq!(own.move_y, 5.0 * um_cos(0.5));
    let meteor = arc(&EeCombatArcIn { meteor: 1, max_velocity: 8.0, ..base });
    assert_eq!(meteor.move_z, 8.0 * um_cos(HALF_PI - 0.5));
    // straight down a still projectile: the zero vector's pitch
    let still = arc(&EeCombatArcIn { launcher_speed: 0.0, own_speed: 0.0, flight_time_ms: 0, ..base });
    assert_eq!(still, EeCombatArcOut { move_y: 0.0, move_z: 0.0, pitch: um_arccos(-0.0) - HALF_PI });
}

#[test]
fn range_and_reach_tests() {
    let base = EeCombatRangeIn {
        range: 5.0,
        heal_range: 2.0,
        distance_squared: 25.0,
        inner_sphere: 2.0,
        ..Default::default()
    };
    assert_eq!(range(&base), 1);
    assert_eq!(range(&EeCombatRangeIn { distance_squared: 25.5, ..base }), 0);
    assert_eq!(range(&EeCombatRangeIn { distance_squared: 25.5, building: 1, ..base }), 1);
    assert_eq!(range(&EeCombatRangeIn { crusader: 1, ..base }), 0);
    assert_eq!(range(&EeCombatRangeIn { crusader: 1, distance_squared: 4.0, ..base }), 1);
    let reach =
        EeCombatReachIn { distance_squared: 1.0, radius: 0.5, other_radius: 0.5, scale: 1.42, ..Default::default() };
    // (0.071 + 1) x 1.42 = 1.52; 4 < 2.31
    assert_eq!(can_reach(&reach), 0);
    assert_eq!(can_reach(&EeCombatReachIn { distance_squared: 0.5, ..reach }), 1);
    // work: a target under radius 1 counts as 1, a wall as 1, a robotic farm as 2: (0.071 + 0.5 + 1) x 1.5 = 2.36
    let work = EeCombatReachIn { distance_squared: 1.0, radius: 0.5, other_radius: 0.2, ..Default::default() };
    assert_eq!(work_reach(&work), 1);
    assert_eq!(work_reach(&EeCombatReachIn { distance_squared: 1.5, ..work }), 0);
    assert_eq!(work_reach(&EeCombatReachIn { distance_squared: 1.5, flag_b: 1, ..work }), 1);
    assert_eq!(
        work_reach(&EeCombatReachIn { distance_squared: 1.5, flag_a: 1, flag_b: 1, other_radius: 5.0, ..work }),
        0
    );
    // gather: the target at least the unit, a tree 1, a naval citizen's own radius 1: (0.071 + 0.5 + 0.5) x 1.42
    let gather = EeCombatReachIn { distance_squared: 0.5, radius: 0.5, other_radius: 0.1, ..Default::default() };
    assert_eq!(gather_reach(&gather), 1);
    assert_eq!(gather_reach(&EeCombatReachIn { distance_squared: 0.6, ..gather }), 0);
    assert_eq!(gather_reach(&EeCombatReachIn { distance_squared: 0.6, flag_a: 1, ..gather }), 1);
    assert_eq!(gather_reach(&EeCombatReachIn { distance_squared: 2.0, flag_a: 1, flag_b: 1, ..gather }), 1);
    assert_eq!(gather_reach(&EeCombatReachIn { distance_squared: 2.0, flag_a: 1, ..gather }), 0);
    assert_eq!(within_range(4.0, 2.0), 1);
    assert_eq!(within_range(4.01, 2.0), 0);
    assert_eq!(within_range(f32::NAN, 2.0), 0);
}

#[test]
fn a_damage_row_is_percent_times_the_scale_and_its_twins() {
    let mut input = EeCombatDamageRowIn { percent: [0; DAMAGE_COLUMNS], scale: 0.01, twin1: 0.5, twin2: 1.5 };
    input.percent[0] = 100;
    input.percent[1] = 33;
    input.percent[67] = -20;
    let out = damage_row(&input);
    assert_eq!(out.base[0], 1.0);
    assert_eq!(out.base[1], 33.0 * 0.01f32);
    assert_eq!(out.base[67], -20.0 * 0.01f32); // -0.19999999, not -0.2
    assert_eq!(out.scaled1[1], 0.5 * (33.0 * 0.01f32));
    assert_eq!(out.scaled2[0], 1.5);
    assert_eq!(out.base[2], 0.0);
}

#[test]
fn the_ffi_returns_the_formulas_results() {
    let input = rules();
    // SAFETY: valid records
    unsafe {
        assert_eq!(ffi::ee_combat_damage(&input), damage(&input));
        assert_eq!(ffi::ee_combat_hit_factor(&input), 1.0);
        let row = EeCombatDamageRowIn { percent: [10; DAMAGE_COLUMNS], scale: 0.01, twin1: 1.0, twin2: 2.0 };
        let mut out = std::mem::MaybeUninit::<EeCombatDamageRowOut>::uninit();
        ffi::ee_combat_damage_row(&row, out.as_mut_ptr());
        assert_eq!(out.assume_init(), damage_row(&row));
    }
    assert_eq!(ffi::ee_combat_within_range(1.0, 1.0), 1);
}

#[test]
fn replay_accepts_the_formulas_records_and_names_a_difference() {
    let input = rules();
    let mut file = encode(b"CBDM", &input, &damage(&input));
    let arc_in =
        EeCombatArcIn { flight_time_ms: 250, gravity: 9.0, pitch: 0.3, launcher_speed: 12.0, ..Default::default() };
    file.extend(encode(b"CBPA", &arc_in, &arc(&arc_in)));
    file.extend(encode(b"CBHR", &[4.0f32, 2.0], &1i32));
    file.extend(encode(b"FMCI", &1i32, &2i32)); // another leaf's record: skipped
    let mut counts = Counts::new();
    let mut reports = Vec::new();
    replay_all(&file, &mut counts, &mut |line| reports.push(line)).unwrap();
    assert!(reports.is_empty(), "{reports:?}");
    assert_eq!(counts.get("CBDM"), Some(&[1, 1, 0]));
    assert_eq!(counts.get("CBPA"), Some(&[1, 1, 0]));
    assert_eq!(counts.get("CBHR"), Some(&[1, 1, 0]));
    assert_eq!(counts.len(), 3);

    // a wrong output, a wrong size, a truncated file
    let wrong = encode(b"CBDM", &input, &EeCombatDamageOut { hit: 1, amount: 9 });
    let record = records(&wrong).unwrap()[0];
    assert_eq!(replay(&record), Some(Ok(false)));
    let short = encode(b"CBDM", &input, &1i32);
    assert!(matches!(replay(&records(&short).unwrap()[0]), Some(Err(_))));
    assert!(records(&wrong[..wrong.len() - 1]).is_err());
    let mut counts = Counts::new();
    replay_all(&wrong, &mut counts, &mut |line| reports.push(line)).unwrap();
    assert_eq!(counts.get("CBDM"), Some(&[1, 0, 0]));
    assert_eq!(reports.len(), 1);
}
