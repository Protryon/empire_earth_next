use super::*;

/// A table whose record k has value1 k * 10 + 1, value2 k * 10 + 2 and the float k + 0.25.
fn table() -> Vec<EeCpBehavior> {
    (0..EE_CP_BEHAVIORS)
        .map(|k| EeCpBehavior {
            index: k as i32,
            i1: k as i32 * 10 + 1,
            i2: k as i32 * 10 + 2,
            fv: k as f32 + 0.25,
            ..Default::default()
        })
        .collect()
}

fn run(difficulty: c_long, player: c_ulong, cheat: bool) -> EeCpFields {
    let t = table();
    let mut f = EeCpFields { mLCC4: 77, ..Default::default() };
    // SAFETY: a live record and a whole table
    unsafe { init(&mut f, t.as_ptr(), difficulty, player, cheat) };
    f
}

#[test]
fn the_values_are_the_cpp_lines_over_the_table() {
    let f = run(2, 13, true);
    // EEComputerPlayer::Init's lines, by hand
    assert_eq!(f.mDifficulty, 2);
    assert_eq!(f.mSeed, 3); // the player id % 10
    assert!(f.mIsCheating && f.mCanAttack && f.mActive && f.mCanBuild && f.mIsCitizenPercentageSet);
    assert!(!f.mIsStarted && !f.mIgnoreOthers && !f.mReplaceOnLoad && !f.mAttackStarted);
    assert_eq!((f.mFavoriteLandType, f.mFavoriteSeaType, f.mFavoriteAirType, f.mFavoriteSpaceType), (20, 242, 72, 730));
    // record 4 (troops to assist allies): value1 * 60000, value2 * 60000, (long)float * 60000
    assert_eq!(
        (f.mAssistAlliesDelayEasy, f.mAssistAlliesDelayMedium, f.mAssistAlliesDelayHard),
        (41 * 60000, 42 * 60000, 4 * 60000)
    );
    // record 52 (carrier usage): value1, float, value2 * 0.01f
    assert_eq!(f.mCarrierMaxPerGroup, 521);
    assert_eq!(f.mCarrierPopPercentage, 52.25);
    assert_eq!(f.mCarrierChance, 522.0f32 * 0.01);
    // record 13 (attack group size): min value1, max value2
    assert_eq!((f.mAttackGroupSizeMin, f.mAttackGroupSizeMax), (131, 132));
    // the attack frequency's bounds read record 21 (Attack Frequency Boundary), the delay's record 19 (Attack
    // Frequency Boundaries)
    assert_eq!((f.mAttackFrequencyFloor, f.mAttackFrequencyCeiling), (211, 212));
    assert_eq!((f.mAttackDelayFloor, f.mAttackDelayCeiling), (191, 192));
    // records 17 and 16, 79 by number
    assert_eq!((f.mLD78, f.mLD7C), (171, 161));
    assert_eq!(f.mFC38, 79.25);
    // (long)float times 60000 (record 1's float is 1.25: 1)
    assert_eq!(f.mInitialAttackDelayMin, 11);
    // the drop-off distance's root: record 14's value1 is 141, sqrt 11.87.. rounds to 12
    assert_eq!(f.mDropOffDistance, 12);
    // a float field from value2 * 0.01f (record 12: the military count weight from value1, the workers' value2)
    assert_eq!((f.mMilitaryCountWeight, f.mWorkerCountWeight), (121.0f32 * 0.01, 122.0f32 * 0.01));
    // the constants
    assert_eq!((f.mLD48, f.mLD4C, f.mLD50, f.mLD54), (1, 1, 1, 1));
    assert_eq!((f.mLF14, f.mLF18, f.mLF1C, f.mLF20), (-1, -1, -1, -1));
    // a member Init does not set starts at 0
    assert_eq!(f.mLCC4, 0);
}

#[test]
fn the_difficulty_and_the_cheat_are_the_caller_s() {
    let f = run(0, 20, false);
    assert_eq!((f.mDifficulty, f.mSeed), (0, 0));
    assert!(!f.mIsCheating);
}
