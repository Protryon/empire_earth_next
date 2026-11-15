use super::*;
use crate::calamity_cast::{EeCalamityCast, EeCalamityCastMode};

fn data(class_enum: i32) -> EeCalamityData {
    EeCalamityData { mClassEnum: class_enum, ..EeCalamityData::default() }
}

fn media(present: u32, duration: [u32; 4]) -> EeCalamityClassMedia {
    EeCalamityClassMedia { present, duration, ..EeCalamityClassMedia::default() }
}

#[test]
fn the_db_record_is_236_bytes_with_the_cpp_offsets() {
    assert_eq!(std::mem::size_of::<EeCalamityData>(), 236);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mFloatParam1), 0x64);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mClassEnum), 0x84);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mGraphicEffectID1), 0x94);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mUnitClassIndex), 0xb0);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mIntParam6), 0xc8);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mBoolParam1), 0xcd);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mIsCastByAllSelected), 0xd0);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mSoundID), 0xd4);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mNameID), 0xe4);
    assert_eq!(std::mem::offset_of!(EeCalamityData, mUnusedClassFlag), 0xe8);
}

#[test]
fn the_base_fields_and_the_class_flag() {
    let mut d = data(kind::FIRE);
    d.mLifeTime = 30000;
    d.mUnitSet = 7;
    d.mCost = 250;
    d.mRange = 2.5; // lrintf: half to even
    d.mIsBlockedByImmunity = true;
    d.mNameID = 9;
    d.mID = 140;
    d.mUnusedClassFlag = true;
    let mut f = EeCalamityClassFields::default();
    initialize_base(&mut f, &d);
    assert_eq!(f.mClassEnum, 10);
    assert_eq!(f.mLifeTime, 30000.0);
    assert_eq!((f.mUnitSet, f.mCost, f.mRange), (7, 250, 2));
    assert!(f.mIsBlockedByImmunity && !f.mIsCastByAllSelected && f.mB24);
    assert_eq!((f.mNameID, f.mID, f.mClassFlags), (9, 140, 4));
    assert_eq!(class_flags(kind::PLAGUE), 1);
    assert_eq!(class_flags(kind::LEPROSY), 2);
    assert_eq!(class_flags(kind::AE_EXTRA_DAMAGE), 0x40);
    assert_eq!(class_flags(kind::AE_MIND_CONTROL), 0x80);
    assert_eq!(class_flags(kind::SPECIAL_ATTACK), 4);
    assert_eq!(class_flags(kind::TIME_WARP), 0);
}

#[test]
fn the_get_modes_and_their_tests() {
    assert_eq!(get_mode(kind::EARTHQUAKE, 0), EE_CALAMITY_GET_ALWAYS);
    assert_eq!(get_mode(kind::FIRE, 0), EE_CALAMITY_GET_POSITIVE);
    assert_eq!(get_mode(kind::TIME_WARP, 3), EE_CALAMITY_GET_NONZERO);
    assert_eq!(get_mode(kind::TIME_WARP, 5), EE_CALAMITY_GET_ALWAYS);
    assert_eq!(get_mode(kind::LINE_OF_SIGHT, 4), EE_CALAMITY_GET_NONE);
    assert_eq!(get_mode(kind::AREA_EFFECT, 6), EE_CALAMITY_GET_NONE);
    assert!(fetches(EE_CALAMITY_GET_ALWAYS, 0));
    assert!(!fetches(EE_CALAMITY_GET_NONZERO, 0) && fetches(EE_CALAMITY_GET_NONZERO, -1));
    assert!(!fetches(EE_CALAMITY_GET_POSITIVE, -1) && fetches(EE_CALAMITY_GET_POSITIVE, 1));
    assert!(!fetches(EE_CALAMITY_GET_NONE, 5));
}

#[test]
fn the_earthquake_fill() {
    let mut d = data(kind::EARTHQUAKE);
    d.mFloatParam1 = 0.25;
    d.mFloatParam2 = 0.5;
    d.mQuakeMinimumStrength = 0.75;
    d.mQuakeStrength = 1.0;
    d.mIntParam1 = 3;
    d.mIntParam2 = 11;
    d.mIntParam3 = 12;
    d.mIntParam4 = 7;
    d.mIntParam5 = 500;
    d.mIntParam6 = 6;
    let mut m = media(3, [3000, 250, 0, 0]);
    m.size[0] = 9.0;
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &d, &m);
    assert_eq!(f.mRadius, 9.0);
    assert_eq!(f.mSourceSpeed, 250.0f32 * 0.01f32);
    assert_eq!(f.mFalloff, 1.0f32 / 3.0f32);
    assert_eq!(f.mShakeInterval, 1.0f32 / 7.0f32);
    assert_eq!(f.mDuration, 3000.0f32 * 0.001f32);
    assert_eq!(f.mSpeed, 9.0f32 / (3000.0f32 * 0.001f32));
    assert_eq!((f.mDamage, f.mShake, f.mTickTime, f.mCrackCount), (11, 12, 500, 6));
    assert_eq!((f.mMinimumFactor, f.mVolume, f.mQuakeMinimumFactor, f.mQuakeVolume), (0.25, 0.5, 0.75, 1.0));
}

#[test]
fn the_durations_and_their_defaults() {
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::AREA_EFFECT), &media(0, [0; 4]));
    assert_eq!(f.mEffect1Duration, 600);
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::AE_EXTRA_DAMAGE), &media(3, [400, 300, 0, 0]));
    assert_eq!((f.mEffect1Duration, f.mEffect2Time), (400, 700));
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::AE_MIND_CONTROL), &media(3, [400, 300, 0, 0]));
    assert_eq!((f.mEffect1Duration, f.mEffect2Time), (400, 400));
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::LOGIC_BOMB), &media(2, [0, 300, 0, 0]));
    assert_eq!((f.mEffectTime2, f.mEffectTime3), (600, 900));
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::TELEPORT), &media(0, [0; 4]));
    assert_eq!(f.mHoldTime, 2000);
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &data(kind::TIME_WARP), &media(1, [800, 0, 0, 0]));
    assert_eq!((f.mDepartTime, f.mAwayTime), (800, 3000));
    let mut f = EeCalamityClassFields { mLifeTime: 5.0, ..Default::default() };
    fill(&mut f, &data(kind::CLOAKING), &media(1, [1234, 0, 0, 0]));
    assert_eq!(f.mLifeTime, 1234.0);
    let mut f = EeCalamityClassFields { mLifeTime: 5.0, ..Default::default() };
    fill(&mut f, &data(kind::CLOAKING), &media(0, [1234, 0, 0, 0]));
    assert_eq!(f.mLifeTime, 5.0);
}

#[test]
fn the_volcano_life_time_sums_as_unsigned_long() {
    let mut d = data(kind::VOLCANO);
    d.mIntParam4 = 3;
    d.mIntParam5 = 8;
    let mut m = media(7, [16_777_217, 0, 0, 0]);
    m.time1[0] = 2;
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &d, &m);
    assert_eq!(f.mLifeTime, 16_777_219u32 as f32);
    assert_eq!((f.mMinimumRadius, f.mMaximumRadius), (3.0, 8.0));
    let mut m = media(7, [u32::MAX, 0, 0, 0]);
    m.time1[0] = 2;
    fill(&mut f, &d, &m);
    #[cfg(windows)]
    assert_eq!(f.mLifeTime, 1.0);
}

#[test]
fn the_functors() {
    assert_eq!(functor_kind(kind::AREA_EFFECT), EE_CALAMITY_FUNCTOR_AREA);
    assert_eq!(functor_kind(kind::AE_EXTRA_DAMAGE), EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE);
    assert_eq!(functor_kind(kind::AE_MIND_CONTROL), EE_CALAMITY_FUNCTOR_MIND_CONTROL);
    assert_eq!(functor_kind(kind::FIRE), EE_CALAMITY_FUNCTOR_FIRE);
    assert_eq!(functor_kind(kind::LEPROSY), EE_CALAMITY_FUNCTOR_LEPROSY);
    assert_eq!(functor_kind(kind::PLAGUE), EE_CALAMITY_FUNCTOR_PLAGUE);
    assert_eq!(functor_kind(kind::VOLCANO), EE_CALAMITY_FUNCTOR_NONE);
    let mut d = data(kind::PLAGUE);
    d.mIntParam1 = 4;
    d.mIntParam2 = 1000;
    d.mFloatParam1 = 3.5;
    d.mFloatParam2 = 0.1;
    let mut f = EeCalamityClassFields::default();
    fill(&mut f, &d, &media(0, [0; 4]));
    assert_eq!((f.mDamage, f.mSpreadTime, f.mRadius, f.mChance), (4, 1000, 3.5, 0.1));
}

fn steps(class_enum: i32, list: u32) -> Vec<u32> {
    (0..).map(|i| media_step(class_enum, list, i)).take_while(|&s| s != EE_CALAMITY_STEP_END).collect()
}

#[test]
fn the_media_steps_keep_each_class_order() {
    assert_eq!(steps(kind::EARTHQUAKE, EE_CALAMITY_MEDIA_LOAD), [2, 1, 5]);
    assert_eq!(steps(kind::TELEPORT, EE_CALAMITY_MEDIA_UNLOAD), [2, 1, 5, 6]);
    assert_eq!(steps(kind::HURRICANE, EE_CALAMITY_MEDIA_LOAD), [2, 3, 5, 7, 8]);
    assert_eq!(steps(kind::HURRICANE, EE_CALAMITY_MEDIA_DESTROY), [2, 3, 1]);
    assert_eq!(steps(kind::METEOR_STORM, EE_CALAMITY_MEDIA_UNLOAD), [5, 7]);
    assert_eq!(steps(kind::VOLCANO, EE_CALAMITY_MEDIA_DESTROY), [9, 1, 2, 3, 4]);
    assert_eq!(steps(kind::FIRE, EE_CALAMITY_MEDIA_DESTROY), [10, 1, 2]);
    assert_eq!(steps(kind::AE_MIND_CONTROL, EE_CALAMITY_MEDIA_DESTROY), [2, 1, 3, 10]);
    assert!(steps(kind::LINE_OF_SIGHT, EE_CALAMITY_MEDIA_LOAD).is_empty());
    assert!(steps(kind::TIME_WARP, 3).is_empty());
}

#[test]
fn the_sound_modes() {
    assert_eq!(sound_mode(kind::EARTHQUAKE), EE_CALAMITY_PLAY_NODE);
    assert_eq!(sound_mode(kind::VOLCANO), EE_CALAMITY_PLAY_NODE);
    assert_eq!(sound_mode(kind::METEOR_STORM), EE_CALAMITY_PLAY_AT_TARGET);
    assert_eq!(sound_mode(kind::TIME_WARP), EE_CALAMITY_PLAY_NONE);
}

#[test]
fn the_runs_pass_the_sounds_each_class_passed() {
    let f =
        EeCalamityClassFields { mClassEnum: kind::PLAGUE as c_long, mIsBlockedByImmunity: true, ..Default::default() };
    let m = media(EE_CALAMITY_MEDIA_SOUND | EE_CALAMITY_MEDIA_SECONDARY_SOUND | 1, [0; 4]);
    let r = run(&f, &m, EE_CALAMITY_RUN_START);
    assert_eq!((r.sounds, r.params.flags, r.runs), (0, EE_CALAMITY_BLOCKED_BY_IMMUNITY, true));
    let f = EeCalamityClassFields { mClassEnum: kind::TIME_WARP as c_long, ..Default::default() };
    let r = run(&f, &media(EE_CALAMITY_MEDIA_SOUND | 1, [0; 4]), EE_CALAMITY_RUN_STOP);
    assert_eq!(r.sounds, EE_CALAMITY_RUN_SOUND);
    assert_eq!(r.params.flags, EE_CALAMITY_SOUND | EE_CALAMITY_EFFECT1);
    let f = EeCalamityClassFields { mClassEnum: kind::METEOR_STORM as c_long, ..Default::default() };
    assert!(!run(&f, &media(EE_CALAMITY_MEDIA_SOUND, [0; 4]), EE_CALAMITY_RUN_STOP).runs);
    assert!(run(&f, &media(EE_CALAMITY_MEDIA_SOUND, [0; 4]), EE_CALAMITY_RUN_UPDATE).runs);
}

#[test]
fn the_update_parameters() {
    let f = EeCalamityClassFields {
        mClassEnum: kind::HURRICANE as c_long,
        mLifeTime: 60000.0,
        mDamage: 5,
        mTickTime: 250,
        mRadius: 4.0,
        ..Default::default()
    };
    let mut m = media(1 | EE_CALAMITY_MEDIA_SOUND, [1500, 0, 0, 0]);
    m.size[1] = 2.5;
    let r = run(&f, &m, EE_CALAMITY_RUN_UPDATE);
    assert_eq!(r.params.flags, EE_CALAMITY_SOUND | EE_CALAMITY_EFFECT1);
    assert_eq!((r.params.storm_delay, r.params.storm_damage, r.params.storm_tick_time), (1500, 5, 250));
    assert_eq!((r.params.storm_radius, r.params.lifetime, r.params.class_enum), (4.0, 60000.0, 4));
    let r = run(&f, &m, EE_CALAMITY_RUN_START);
    assert_eq!(r.params.start_size, 2.5);
    let f = EeCalamityClassFields {
        mClassEnum: kind::AE_EXTRA_DAMAGE as c_long,
        mEffect1Duration: 400,
        mEffect2Time: 700,
        ..Default::default()
    };
    let r = run(&f, &media(2, [0; 4]), EE_CALAMITY_RUN_UPDATE);
    assert_eq!((r.params.flags, r.params.area_delay, r.params.area_time2), (EE_CALAMITY_EFFECT2, 400, 700));
    let f = EeCalamityClassFields {
        mClassEnum: kind::MORALE as c_long,
        mPositive: true,
        mEffectDuration: 9,
        ..Default::default()
    };
    assert_eq!(
        run(&f, &media(1, [0; 4]), EE_CALAMITY_RUN_START).params.flags,
        EE_CALAMITY_EFFECT1 | EE_CALAMITY_POSITIVE
    );
    assert_eq!(run(&f, &media(1, [0; 4]), EE_CALAMITY_RUN_UPDATE).params.morale_delay, 9);
    let f = EeCalamityClassFields {
        mClassEnum: kind::EARTHQUAKE as c_long,
        mSourceSpeed: 2.5,
        mDamage: 3,
        ..Default::default()
    };
    let mut m = media(0, [0; 4]);
    m.quake_damage_factor = 1.5;
    assert_eq!(run(&f, &m, EE_CALAMITY_RUN_STOP).params.quake_source_speed, 2.5);
    let r = run(&f, &m, EE_CALAMITY_RUN_UPDATE);
    assert_eq!((r.params.quake_damage, r.params.quake_damage_factor, r.params.quake_source_speed), (3, 1.5, 0.0));
}

#[test]
fn the_resets() {
    let mut r = EeCalamityReset { bool1: true, bool2: true, ..Default::default() };
    reset(kind::TELEPORT, &mut r);
    assert!(!r.bool1 && !r.bool2);
    let mut r = EeCalamityReset { int1: 5, ..Default::default() };
    reset(kind::FIRE, &mut r);
    assert!(r.create_helper);
    reset(kind::HURRICANE, &mut r);
    assert_eq!(r.int1, 0);
    let mut r = EeCalamityReset {
        has_target: true,
        target_tile_x: 4,
        target_tile_y: 6,
        target_hit_points: 10,
        bool1: true,
        float1: 3.0,
        ..Default::default()
    };
    reset(kind::TIME_WARP, &mut r);
    assert_eq!((r.tile_x, r.tile_y, r.bool1, r.bool2, r.float1), (4, 6, false, false, 0.0));
    let mut r =
        EeCalamityReset { has_target: true, target_tile_x: -10000, target_hit_points: 10, ..Default::default() };
    reset(kind::TIME_WARP, &mut r);
    assert_eq!((r.int2, r.bool1, r.bool2), (-1, true, true));
    let mut r = EeCalamityReset { float1: 2.0, ..Default::default() };
    reset(kind::TIME_WARP, &mut r);
    assert_eq!((r.int2, r.bool1, r.float1), (0, false, 0.0));
}

#[test]
fn the_cast_questions() {
    assert!(targets_by_functor(kind::FIRE) && targets_by_functor(kind::PLAGUE) && !targets_by_functor(kind::MORALE));
    let mut cast = EeCalamityCast {
        mode: EeCalamityCastMode::Base,
        class_enum: 0,
        x: 0,
        y: 0,
        cost: 0,
        radius: 0,
        area: 0,
        space_terrain: 0,
        check: false,
        has_caster: false,
        blocked_by_immunity: false,
        over_water: false,
    };
    let f = EeCalamityClassFields {
        mClassEnum: kind::HURRICANE as c_long,
        mOverWater: true,
        mArea: 3,
        mUnitClassIndex: 77,
        ..Default::default()
    };
    assert_eq!(cast_fields(&f, &mut cast, 9), 77);
    assert!(cast.over_water && cast.area == 3 && cast.space_terrain == 0);
    let f = EeCalamityClassFields { mClassEnum: kind::LEPROSY as c_long, mRadiusTiles: 5, ..Default::default() };
    assert_eq!(cast_fields(&f, &mut cast, 9), 0);
    assert_eq!(cast.radius, 5);
    let f = EeCalamityClassFields { mClassEnum: kind::METEOR_STORM as c_long, ..Default::default() };
    cast_fields(&f, &mut cast, 9);
    assert_eq!(cast.space_terrain, 9);
}

#[test]
fn every_kind_has_its_subclass_base() {
    // the twenty EECC* classes' bases (ee/EECC*.h as they were): global, location, unit
    for k in [23, 25] {
        assert_eq!(target_kind(k), EE_CALAMITY_TARGET_GLOBAL, "kind {k}");
    }
    for k in [3, 4, 8, 9, 15, 16, 17, 19, 24, 26] {
        assert_eq!(target_kind(k), EE_CALAMITY_TARGET_LOCATION, "kind {k}");
    }
    for k in [7, 10, 13, 14, 18, 20, 21, 22] {
        assert_eq!(target_kind(k), EE_CALAMITY_TARGET_UNIT, "kind {k}");
    }
    for k in [-1, 0, 1, 2, 5, 6, 11, 12, 27, 30] {
        assert_eq!(target_kind(k), EE_CALAMITY_TARGET_NONE, "kind {k}");
    }
}
