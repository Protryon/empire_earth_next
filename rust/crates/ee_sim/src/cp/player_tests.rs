use super::*;

fn player(difficulty: c_ulong) -> EeCpFields {
    EeCpFields {
        mDifficulty: difficulty,
        mAttackDelayModifierEasy: 150,
        mAttackDelayModifierHard: 50,
        mAttackFrequencyModifierEasy: 200,
        mAttackFrequencyModifierHard: 75,
        mUnitCostReductionEasy: 120,
        mUnitCostReductionHard: 80,
        mUnitCostMedium: 100.0,
        mDMAttackFrequencyModifierEasy: 1.5,
        mDMAttackFrequencyModifierMedium: 1.25,
        mDMAttackFrequencyModifierHard: 0.5,
        mDMInitialAttackModifierEasy: 2.0,
        mDMInitialAttackModifierMedium: 1.0,
        mDMInitialAttackModifierHard: 0.25,
        mPopLimitAdjustmentEasy: 90,
        mPopLimitAdjustmentMedium: 100,
        mPopLimitAdjustmentHard: 140,
        mIslandExpansionTimerEasy: 3,
        mIslandExpansionTimerMedium: 2,
        mIslandExpansionTimerHard: 1,
        mInitialAttackDelayMin: 10,
        mInitialAttackDelayMax: 20,
        mInitialAttackDelayPerEpoch: 2,
        mAttackDelayFloor: 5,
        mAttackDelayCeiling: 30,
        mAttackFrequencyMin: 4,
        mAttackFrequencyMax: 8,
        mAttackFrequencyPerEpoch: 0.5,
        mAttackFrequencyFloor: 3,
        mAttackFrequencyCeiling: 12,
        ..Default::default()
    }
}

#[test]
fn the_scales_follow_the_difficulty() {
    let (e, m, h, other) = (player(EASY), player(MEDIUM), player(HARD), player(7));
    assert_eq!(
        (e.attack_delay_modifier(), m.attack_delay_modifier(), h.attack_delay_modifier()),
        (150.0 * 0.01, 1.0, 50.0 * 0.01)
    );
    assert_eq!(
        (e.attack_frequency_modifier(), m.attack_frequency_modifier(), h.attack_frequency_modifier()),
        (2.0, 1.0, 75.0 * 0.01)
    );
    assert_eq!(
        (e.unit_cost_modifier(), m.unit_cost_modifier(), h.unit_cost_modifier()),
        (120.0 * 0.01, 1.0, 80.0 * 0.01)
    );
    assert_eq!(other.unit_cost_modifier(), 1.0, "medium's for any other");
    assert_eq!(
        (e.dm_attack_frequency_modifier(), m.dm_attack_frequency_modifier(), h.dm_attack_frequency_modifier()),
        (1.5, 1.25, 0.5)
    );
    assert_eq!(other.dm_attack_frequency_modifier(), 1.0);
    assert_eq!((e.dm_initial_attack_modifier(), other.dm_initial_attack_modifier()), (2.0, 1.0));
    assert_eq!(
        (e.pop_limit_adjustment(), m.pop_limit_adjustment(), h.pop_limit_adjustment()),
        (90.0 * 0.01, 1.0, 140.0 * 0.01)
    );
    assert_eq!(
        (
            e.island_expansion_timer(),
            m.island_expansion_timer(),
            h.island_expansion_timer(),
            other.island_expansion_timer()
        ),
        (3, 2, 1, 60000)
    );
}

#[test]
fn scale_cost_only_while_cheating_and_never_below_zero() {
    let mut p = player(EASY);
    assert_eq!(p.scale_cost(101), 101);
    assert_eq!(p.scale_cost(-5), 0);
    p.mIsCheating = true;
    assert_eq!(p.scale_cost(101), 121); // 121.2 rounded
    assert_eq!(p.scale_cost(-5), 0);
    p.mDifficulty = HARD;
    assert_eq!(p.scale_cost(5), 4);
}

#[test]
fn the_setters_write_the_difficulty_s_slot() {
    let mut p = player(MEDIUM);
    p.set_dm_attack_frequency_modifier(9.0);
    p.set_dm_initial_attack_modifier(8.0);
    p.set_island_expansion_timer(7);
    assert_eq!(
        (p.mDMAttackFrequencyModifierMedium, p.mDMInitialAttackModifierMedium, p.mIslandExpansionTimerMedium),
        (9.0, 8.0, 7)
    );
    let mut q = player(5);
    let before = q;
    q.set_dm_attack_frequency_modifier(9.0);
    q.set_island_expansion_timer(7);
    assert_eq!(q, before, "no slot for another difficulty");
}

#[test]
fn the_next_attack_time_draws_once_and_clamps() {
    let mut draws = Vec::new();
    let mut p = player(EASY);
    // the first: 15 + 2 * 3 epochs = 21, * 1.5 = 31.5 -> 32, clamped to 30
    p.set_next_attack_time(1000, 3, false, &mut |lo, hi| {
        draws.push((lo, hi));
        15
    });
    assert_eq!(draws, vec![(10, 20)]);
    assert_eq!(p.mNextAttackTime, 30 * 60000 + 1000);
    assert!(!p.mAttackStarted);
    // deathmatch, one epoch: 15 + 2 = 17, * 2.0 = 34 -> 30; three epochs on medium: 21 * 1.0
    p.set_next_attack_time(0, 1, true, &mut |_, _| 15);
    assert_eq!(p.mNextAttackTime, 30 * 60000);
    p.mDifficulty = MEDIUM;
    p.set_next_attack_time(0, 3, false, &mut |_, _| 15);
    assert_eq!(p.mNextAttackTime, 21 * 60000);
    // later ones: 5 + lrint(0.5 * 3 = 1.5) = 5 + 2 = 7, * 2.0 = 14 -> 12; started
    p.mDifficulty = EASY;
    p.mIsStarted = true;
    let mut draws = Vec::new();
    p.set_next_attack_time(500, 3, false, &mut |lo, hi| {
        draws.push((lo, hi));
        5
    });
    assert_eq!(draws, vec![(4, 8)]);
    assert_eq!(p.mNextAttackTime, 12 * 60000 + 500);
    assert!(p.mAttackStarted);
    // the floor
    p.mDifficulty = HARD;
    p.set_next_attack_time(0, 1, false, &mut |_, _| 0);
    assert_eq!(p.mNextAttackTime, 3 * 60000);
}

#[test]
fn restore_rebases_the_times_and_puts_the_bounds_in_order() {
    let mut p = player(EASY);
    p.mNextAttackTime = 100;
    p.mNextExpansionTime = u32::MAX as c_ulong; // -1 relative
    p.rebase_attack_time(5000);
    p.rebase_expansion_time(5000);
    assert_eq!((p.mNextAttackTime, p.mNextExpansionTime), (5100, 4999));
    p.mAttackFrequencyMin = 9;
    p.mAttackFrequencyMax = 4;
    p.mAttackFrequencyFloor = 20;
    p.mAttackFrequencyCeiling = 10;
    p.mDropOffDistance = 144;
    p.after_restore(false);
    assert_eq!((p.mAttackFrequencyMin, p.mAttackFrequencyMax), (4, 4), "min, then the max of the new min");
    assert_eq!((p.mAttackFrequencyFloor, p.mAttackFrequencyCeiling), (10, 10));
    assert_eq!(p.mDropOffDistance, 144, "kept after a failed restore");
    assert!(p.mRegroupPending);
    p.after_restore(true);
    assert_eq!(p.mDropOffDistance, 12);
    p.after_restore(true);
    assert_eq!(p.mDropOffDistance, 12, "25 and below kept");
    p.mExpansionTimerEasy = 6;
    p.old_expansion_timers(0, 0xe5);
    assert_eq!(p.mExpansionTimerMedium, 0);
    p.old_expansion_timers(0, 0xe4);
    assert_eq!((p.mExpansionTimerMedium, p.mExpansionTimerHard), (6, 6));
}
