use super::*;

fn reset_input() -> EeUnitsComplexResetIn {
    EeUnitsComplexResetIn {
        heading: 1.5,
        reload_time: 3000,
        power_regeneration: 7,
        garrison_count: 2,
        cloakable: true,
        paid_for: false,
    }
}

#[test]
fn reset_sets_the_originals_defaults_and_keeps_the_three_it_leaves() {
    let mut cx = EeUnitsComplexUnitFields {
        mU356: 9,
        mDisabledReason: 11,
        mU35E: 13,
        mHeight: 4.0,
        mTimesUpdated: 77,
        mIsFoundation: true,
        mHasShiftQueuedBuild: true,
        mCamouflageTimer: 0x8000_0001,
        ..Default::default()
    };
    reset(&mut cx, &reset_input());
    assert_eq!((cx.mU356, cx.mDisabledReason, cx.mU35E), (9, 11, 13));
    assert_eq!(cx.mInitialContactHeading, -10000.0);
    assert_eq!(cx.mF220, 1.5);
    assert_eq!(cx.mNextTriggerTime, c_ulong::MAX);
    assert_eq!((cx.mExitingUnitID, cx.mReloadTimer, cx.mPowerRegenTimer), (-1, 3000, 7));
    assert_eq!((cx.mAnimation, cx.mFormationSlot, cx.mGarrisonCount), (7, 1, 2));
    assert_eq!((cx.mCurrentResearch, cx.mCurrentPhysicsAction, cx.mCurrentPhysicsOrder), (100_000, 6, 5));
    assert_eq!((cx.mAIState, cx.mIsCollidable, cx.mGrowth), (300_000, 1, 1.0));
    assert!(cx.mIsCommandable && cx.mIsPathDone && cx.mIsBuilt && cx.mIsDeletable && cx.mIsCloakable);
    assert!(!cx.mIsPaidFor && !cx.mIsFoundation && !cx.mHasShiftQueuedBuild);
    assert_eq!((cx.mHeight, cx.mTimesUpdated, cx.mCamouflageTimer), (0.0, 0, 0));
}

fn other() -> EeUnitsComplexUnitFields {
    EeUnitsComplexUnitFields {
        mVelocity: 2.5,
        mLastHitPoints: 50,
        mPower: 30,
        mFlightTime: 25,
        mReloadTimer: 1000,
        mIsCloakable: true,
        mCamouflageTimer: 5,
        mGrowth: 0.5,
        mB34F: true,
        mNumAreaEffects: 4,
        mPlanetID: 3,
        mAIState: 12,
        mIsFiringHigh: true,
        ..Default::default()
    }
}

#[test]
fn copy_scales_the_fractions_to_this_units_maxima() {
    let input = EeUnitsComplexCopyIn {
        other_max_hit_points: 100,
        max_hit_points: 300,
        other_power: 60,
        power: 10,
        other_flight_time: 100,
        flight_time: 40,
        other_reload_time: 2000,
        reload_time: 3000,
        copy_cloakable: false,
    };
    let mut cx = EeUnitsComplexUnitFields::default();
    copy_first(&mut cx, &other(), &input);
    assert_eq!(cx.mVelocity, 2.5);
    assert_eq!((cx.mLastHitPoints, cx.mPower, cx.mFlightTime, cx.mReloadTimer), (150, 5, 10, 1500));
    assert!(!cx.mIsCloakable, "the camouflage set decides the cloakable flag");
    assert_eq!(cx.mCamouflageTimer, 5);
    copy_first(&mut cx, &other(), &EeUnitsComplexCopyIn { other_power: 0, copy_cloakable: true, ..input });
    assert_eq!(cx.mPower, 0, "no power maximum: a fraction of 0");
    assert!(cx.mIsCloakable);
}

#[test]
fn copy_rounds_to_nearest_even_as_lrintf() {
    let input = EeUnitsComplexCopyIn {
        other_max_hit_points: 4,
        max_hit_points: 5,
        other_flight_time: 1,
        other_reload_time: 1,
        ..Default::default()
    };
    let mut cx = EeUnitsComplexUnitFields::default();
    let o = EeUnitsComplexUnitFields { mLastHitPoints: 1, ..Default::default() };
    copy_first(&mut cx, &o, &input);
    assert_eq!(cx.mLastHitPoints, 1, "1.25 rounds to 1");
    let o = EeUnitsComplexUnitFields { mLastHitPoints: 2, ..Default::default() };
    copy_first(&mut cx, &o, &input);
    assert_eq!(cx.mLastHitPoints, 2, "2.5 rounds to even");
}

#[test]
fn the_second_and_third_blocks_copy_and_restart_the_area_effects() {
    let mut cx = EeUnitsComplexUnitFields { mNumAreaEffects: 9, mU2FC: 9, ..Default::default() };
    copy_second(&mut cx, &other());
    assert!(!cx.mIsFiringHigh && cx.mB34F);
    assert_eq!((cx.mGrowth, cx.mNumAreaEffects, cx.mU2FC), (0.5, 0, 0));
    copy_third(&mut cx, &other());
    assert_eq!((cx.mPlanetID, cx.mAIState), (3, 12));
}

fn dump_in() -> EeUnitsComplexDumpIn {
    let mut location = [[0.0f32; 4]; 4];
    location[0][3] = 12.5;
    location[1][3] = -3.25;
    location[2][3] = 1.0;
    EeUnitsComplexDumpIn {
        player_id: 2,
        tile_x: 45,
        tile_y: 7,
        angular_velocity: [0.5, 0.0, -0.0],
        build_queue: 3,
        citizen_build_queue: 0,
        collision_tile: [-10000, -10000],
        frame: 4,
        projectile_start: [0.0; 3],
        location,
    }
}

#[test]
fn the_dumps_lines_are_the_originals_printf_formats() {
    let rts = EeUnitsRtsUnitFields { mUnitID: 0x1a2b, ..Default::default() };
    let unit = EeUnitsUnitFields { mHitPoints: 250, ..Default::default() };
    let cx = EeUnitsComplexUnitFields { mTimesUpdated: 99, mVelocity: 1.0 / 3.0, mPlanetID: 5, ..Default::default() };
    let class = EeUnitsUnitClassFields { mUnitType: 17, mShootsHigh: true, ..Default::default() };
    let stats = EeUnitsUnitClassUpgradeFields::default();
    let head = dump(0, &rts, &unit, &cx, &class, &stats, &dump_in());
    assert_eq!(
        head,
        "\nOUT OF SYNCH DUMP:  **************  Unit ID: 00001a2b  *************\n\
         OUT OF SYNCH DUMP:  Times Updated:      99\n\
         OUT OF SYNCH DUMP:  Owner:               2\n\
         OUT OF SYNCH DUMP:  Object Type:          17\n\
         OUT OF SYNCH DUMP:  Tile (x):             45\n\
         OUT OF SYNCH DUMP:  Tile (y):              7\n\
         OUT OF SYNCH DUMP:  Hit Points:            250\n"
    );
    let rest = dump(1, &rts, &unit, &cx, &class, &stats, &dump_in());
    assert!(rest.starts_with("OUT OF SYNCH DUMP:  CurrentPhysicsAction:  0\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Yaw Velocity:       0.500000\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Roll Velocity:      -0.000000\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Collision Tile (x): -10000\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Velocity:           0.333333\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Shoots High:         1\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Cnvert % per sec:   0.000000\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Model Location (y): -3.250000\n"));
    assert!(rest.contains("OUT OF SYNCH DUMP:  Trans (0,3):        12.500000\n"));
    assert!(rest.ends_with("OUT OF SYNCH DUMP:  Is Cloaked?:\t\t 0\nOUT OF SYNCH DUMP:\tPlanet ID:               5\n"));
    assert_eq!(rest.lines().count(), 22 + 36 + 16 + 2);
}
