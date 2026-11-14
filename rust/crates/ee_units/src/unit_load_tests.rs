use super::*;
use crate::unit_save::*;

/// A file of primitive values: the saves push, the loads pop (a read past the end fails, leaving the old value);
/// every operation recorded; the sections and parts read and write nothing (their C++ is not here).
#[derive(Default)]
struct File {
    data: std::collections::VecDeque<(i64, u32, u8)>,
    log: Vec<(i32, u32, i64)>,
    fail_at: Option<usize>,
    reads: usize,
}

impl SaveHost for File {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        self.log.push((op, index, io.i));
        match op {
            w if crate::player_save::is_save_write(w) => {
                self.data.push_back((io.i, io.f.to_bits(), io.b));
                0
            }
            EE_UNITS_UNIT_SAVE_MAX => {
                io.i = 100;
                0
            }
            EE_UNITS_UNIT_SAVE_ID => {
                io.i = 500 + i64::from(index);
                0
            }
            // the C++ sides that are not symmetric: the turret section writes a count the load reads itself (here
            // none), the goal queues' load section reads the two effect handles the save wrote, the contact point's
            // load parts read the floats the save wrote
            EE_UNITS_UNIT_SAVE_SECTION if index == EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES => {
                self.data.push_back((0, 0, 0));
                0
            }
            EE_UNITS_UNIT_LOAD_SECTION if index == EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES => {
                self.data.pop_front();
                self.data.pop_front();
                0
            }
            EE_UNITS_UNIT_LOAD_PART
                if (EE_UNITS_UNIT_LOAD_PART_CONTACT_X..=EE_UNITS_UNIT_LOAD_PART_CONTACT_Z).contains(&index) =>
            {
                self.data.pop_front();
                0
            }
            r if crate::player_save::is_save_read(r) => {
                self.reads += 1;
                if self.fail_at == Some(self.reads) {
                    return 1;
                }
                match self.data.pop_front() {
                    Some((i, f, b)) => {
                        io.i = i;
                        io.f = f32::from_bits(f);
                        io.b = b;
                        0
                    }
                    None => 1,
                }
            }
            _ => 0,
        }
    }
}

fn records() -> (EeUnitsRtsUnitFields, EeUnitsUnitFields, EeUnitsComplexUnitFields) {
    let rts = EeUnitsRtsUnitFields {
        mTileX: 40,
        mTileY: 41,
        mUnitID: 0x0300_0102,
        mIsQueuedForDelete: true,
        mIsValid: true,
        ..Default::default()
    };
    let unit = EeUnitsUnitFields { mResourceType: 2, mHitPoints: 50, mResourceAmount: 300, mHiddenFlags: 0 };
    let cx = EeUnitsComplexUnitFields {
        mHeight: 1.25,
        mVelocity: 3.5,
        mF220: 6.25,
        mCurrentResearch: 100000,
        mAnimation: 7,
        mFacing: 3,
        mLaunchYaw: 0.5,
        mExitingUnitID: -1,
        mReloadTimer: 250,
        mRottingAwayTime: 9,
        mHealSearchTimer: 0.75,
        mAmmunition: 4,
        mTimesUpdated: 1234,
        mConversionStartTime: 77,
        mCollisionState: 0x102,
        mIsCommandable: true,
        mIsBuilt: true,
        mOpacity: 0.5,
        mIsCloaked: true,
        mNumAreaEffects: 2,
        mGarrisonPopulation: 5,
        mIsPaidFor: true,
        mGateDirection: 1,
        mOpenFraction: 0.25,
        mGrowth: 2.0,
        mNumAttackers: 3,
        mPowerRegenTimer: 600,
        mIsCloakable: true,
        mInitialContactHeading: 1.5,
        mExperience: 12,
        mIsFoundation: true,
        mFormationRank: 2,
        mWaterState: 1,
        mCloaked: 15,
        mCamouflageTimer: 8,
        mIsSpecialAttack: true,
        mDisabledReason: 4,
        mPlanetID: 3,
        mSight: 1.0,
        mU32C: 99,
        mLastHitPoints: 25,
        mFlightTime: 0,
        mPower: 50,
        ..Default::default()
    };
    (rts, unit, cx)
}

const NOW: EeUnitsUnitSaveIn = EeUnitsUnitSaveIn { major: 0, minor: 0x118, live_state: 0, running_state: 0 };

fn saved(inp: &EeUnitsUnitSaveIn) -> File {
    let (rts, unit, cx) = records();
    let mut f = File::default();
    // SAFETY: live records
    unsafe {
        assert_eq!(unit_save(&mut f, &rts, &unit, inp), 0);
        assert_eq!(complex_save_rest(&mut f, &cx, inp), 0);
    }
    f.log.clear();
    f
}

#[test]
fn a_load_reads_back_what_the_save_wrote() {
    let mut f = saved(&NOW);
    let (rts, _, cx) = records();
    let mut r2 = EeUnitsRtsUnitFields { mIsEditorSelected: true, ..Default::default() };
    let mut u2 = EeUnitsUnitFields::default();
    let mut c2 = EeUnitsComplexUnitFields::default();
    // SAFETY: live records
    let base = unsafe { unit_load(&mut f, &mut r2, &mut u2) };
    assert_eq!(base, 0);
    // SAFETY: a live record
    assert_eq!(unsafe { complex_load_rest(&mut f, &mut c2, &NOW, base) }, 0);
    assert_eq!(r2, EeUnitsRtsUnitFields { mIsEditorSelected: false, ..rts });
    assert_eq!((u2.mResourceType, u2.mResourceAmount), (2, 300));
    // the fractions' bits wait in the longs for FinishLoad: 50 / 100, 25 / 100, 50 / 100; a zero flight time 0
    assert_eq!(u2.mHitPoints as u32, 0.5f32.to_bits());
    assert_eq!(c2.mLastHitPoints as u32, 0.25f32.to_bits());
    assert_eq!(c2.mPower as u32, 0.5f32.to_bits());
    assert_eq!(c2.mFlightTime, 0);
    // every other saved member is back (the running state's exact counters are the C++ live section's)
    let expect = EeUnitsComplexUnitFields {
        mLastHitPoints: c2.mLastHitPoints,
        mPower: c2.mPower,
        mInitialContactHeading: cx.mInitialContactHeading,
        ..cx
    };
    assert_eq!(c2, expect);
    // the ids the save wrote are set again, in order
    let sets: Vec<(u32, i64)> = f.log.iter().filter(|e| e.0 == EE_UNITS_UNIT_LOAD_SET).map(|e| (e.1, e.2)).collect();
    assert!(sets.contains(&(EE_UNITS_UNIT_LOAD_SET_PATROL_GROUP, 502)));
    assert!(sets.contains(&(EE_UNITS_UNIT_LOAD_SET_LAST_RESOURCE, 515)));
    assert!(sets.contains(&(EE_UNITS_UNIT_LOAD_SET_HERD, 512)));
    // everything written was read
    assert!(f.data.is_empty());
    let sections: Vec<u32> = f.log.iter().filter(|e| e.0 == EE_UNITS_UNIT_LOAD_SECTION).map(|e| e.1).collect();
    assert_eq!(sections, [0, 1, 2, 3, 4, 5, 6]);
}

#[test]
fn a_failed_read_goes_on_at_the_original_labels() {
    // the height fails: on at LoadFlags (the conversion start time is the next read), whose reads then run dry
    let full = saved(&NOW);
    // the reads before the height: the base unit's 6 and the unit's 4, the firing flag, the scale, the frame, the
    // collision tile's 2, the launcher class, the 5 part ids, the action and the turret count
    let height_at = 6 + 4 + 3 + 2 + 1 + 5 + 2 + 1;
    let mut f = File { data: full.data.clone(), fail_at: Some(height_at), ..Default::default() };
    let mut r2 = EeUnitsRtsUnitFields::default();
    let mut u2 = EeUnitsUnitFields::default();
    let mut c2 = EeUnitsComplexUnitFields::default();
    // SAFETY: live records
    let base = unsafe { unit_load(&mut f, &mut r2, &mut u2) };
    // SAFETY: a live record
    unsafe { complex_load_rest(&mut f, &mut c2, &NOW, base) };
    let reads: Vec<usize> =
        f.log.iter().enumerate().filter(|(_, e)| crate::player_save::is_save_read(e.0)).map(|(i, _)| i).collect();
    let after = &f.log[reads[height_at - 1] + 1..];
    // no further reads of the height's chain: the next read is LoadFlags' unsigned long
    let next = after.iter().find(|e| crate::player_save::is_save_read(e.0)).unwrap();
    assert_eq!(next.0, EE_UNITS_UNIT_LOAD_READ_ULONG);
    assert_eq!(c2.mVelocity, 0.0);
    // EEUnit::Load failing: straight to LoadFinalVersions (no section)
    let mut f = File::default();
    let mut c3 = EeUnitsComplexUnitFields { mIsSpecialAttack: true, mSight: 2.0, ..Default::default() };
    let old = EeUnitsUnitSaveIn { major: 0, minor: 0xe0, ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { complex_load_rest(&mut f, &mut c3, &old, 7) }, 7);
    assert!(f.log.is_empty());
    assert!(!c3.mIsSpecialAttack && c3.mSight == 0.0 && c3.mPlanetID == 0);
}

#[test]
fn old_saves_take_their_eras_reads() {
    // 0x1b: the legacy flags instead of the opacity and the cloak; none of the late versions' reads
    let old = EeUnitsUnitSaveIn { major: 0, minor: 0x1b, ..Default::default() };
    let mut f = File::default();
    for _ in 0..400 {
        f.data.push_back((1, 0, 1));
    }
    let mut c = EeUnitsComplexUnitFields { mOpacity: 0.5, mFormationRank: 9, ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { complex_load_rest(&mut f, &mut c, &old, 0) }, 0);
    assert_eq!(c.mOpacity, 0.5);
    assert_eq!(c.mFormationRank, 9);
    assert!(!c.mIsCloaked && !c.mIsFoundation);
    assert!(!f.log.iter().any(|e| e.0 == EE_UNITS_UNIT_LOAD_PART && e.1 == EE_UNITS_UNIT_LOAD_PART_LAST_MAP_POSITION));
    // the RTS part up to 0.6 reads the same order as later (only its save differs)
    let mut f = saved(&EeUnitsUnitSaveIn { major: 0, minor: 0x118, ..Default::default() });
    let mut r = EeUnitsRtsUnitFields::default();
    // SAFETY: a live record
    assert_eq!(unsafe { rts_load(&mut f, &mut r) }, 0);
    assert_eq!((r.mTileX, r.mTileY, r.mUnitID), (40, 41, 0x0300_0102));
}

/// a fraction's bits as a load leaves them in a `long` (its first four bytes)
fn held(fraction: f32) -> std::os::raw::c_long {
    let mut v: std::os::raw::c_long = 0;
    // SAFETY: a long has at least four bytes (little-endian)
    unsafe { std::ptr::write_unaligned(std::ptr::addr_of_mut!(v).cast::<u32>(), fraction.to_bits()) };
    v
}

#[test]
fn finish_load_turns_the_fractions_into_amounts() {
    let mut cx = crate::unit_fields::EeUnitsComplexUnitFields {
        mLastHitPoints: held(0.5),
        mFlightTime: held(0.0001),
        mGarrisonCount: held(0.0),
        mPower: held(0.333_333_34),
        ..Default::default()
    };
    let input = EeUnitsFinishLoadIn { max_hit_points: 125, flight_time: 1000, garrison_capacity: 10, power: 300 };
    // SAFETY: a live record and input
    unsafe { ee_units_unit_finish_fractions(&mut cx, &input) };
    // 62.5 truncated; 0.1 truncated to 0, then 1 (not a zero fraction); 0 stays 0; 100.000_01 truncated
    assert_eq!((cx.mLastHitPoints, cx.mFlightTime, cx.mGarrisonCount, cx.mPower), (62, 1, 0, 100));
}

#[test]
fn finish_load_rounds_the_units_hit_points() {
    let mut u = crate::unit_fields::EeUnitsUnitFields { mHitPoints: held(0.5), ..Default::default() };
    // SAFETY: a live record
    unsafe { ee_units_unit_finish_hit_points(&mut u, 125) };
    // 62.500_01 rounds to 63 (the truncating four give 62)
    assert_eq!(u.mHitPoints, 63);
    let mut u = crate::unit_fields::EeUnitsUnitFields { mHitPoints: held(0.001), ..Default::default() };
    // SAFETY: a live record
    unsafe { ee_units_unit_finish_hit_points(&mut u, 100) };
    assert_eq!(u.mHitPoints, 1);
}
