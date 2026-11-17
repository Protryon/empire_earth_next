use super::*;

/// A recording host: every operation with its index and value, a write failing at a chosen call.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u32, i64, u32, u8)>,
    fail_at: Option<usize>,
    writes: usize,
}

impl SaveHost for Mock {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        match op {
            EE_UNITS_UNIT_SAVE_MAX => io.i = [50, 20, 0, 4][index as usize],
            EE_UNITS_UNIT_SAVE_ID => io.i = 1000 + i64::from(index),
            EE_UNITS_UNIT_SAVE_FLOAT => io.f = 0.5 + index as f32,
            EE_UNITS_UNIT_SAVE_COLLISION_TILE => io.i = 7 + i64::from(index),
            _ => {}
        }
        self.log.push((op, index, io.i, io.f.to_bits(), io.b));
        let writes = matches!(
            op,
            EE_UNITS_UNIT_SAVE_WRITE_LONG
                | EE_UNITS_UNIT_SAVE_WRITE_ULONG
                | EE_UNITS_UNIT_SAVE_WRITE_FLOAT
                | EE_UNITS_UNIT_SAVE_WRITE_BOOL
                | EE_UNITS_UNIT_SAVE_WRITE_USHORT
                | EE_UNITS_UNIT_SAVE_SECTION
                | EE_UNITS_UNIT_SAVE_PART
        );
        if !writes {
            return 0;
        }
        self.writes += 1;
        i32::from(self.fail_at == Some(self.writes))
    }
}

fn records() -> (EeUnitsRtsUnitFields, EeUnitsUnitFields, EeUnitsComplexUnitFields) {
    let rts = EeUnitsRtsUnitFields {
        mTileX: 12,
        mTileY: -10000,
        mUnitID: 0x0200_0031,
        mIsQueuedForDelete: true,
        mIsEditorSelected: true,
        mIsValid: true,
        ..Default::default()
    };
    let unit = EeUnitsUnitFields { mResourceType: 3, mHitPoints: 25, mResourceAmount: 400, mHiddenFlags: 0x10 };
    let cx = EeUnitsComplexUnitFields {
        mHeight: 2.0,
        mLastHitPoints: 10,
        mFlightTime: 5,
        mGarrisonCount: 3,
        mPower: 7,
        mIsBuilt: true,
        mCollisionState: 9,
        mPlanetID: 2,
        mU32C: 77,
        mNextTriggerTime: 0xffff_0000,
        ..Default::default()
    };
    (rts, unit, cx)
}

fn ops(log: &[(i32, u32, i64, u32, u8)]) -> Vec<(i32, u32)> {
    log.iter().map(|e| (e.0, e.1)).collect()
}

#[test]
fn the_base_unit_writes_its_tile_id_and_flags() {
    let (rts, _, _) = records();
    let mut m = Mock::default();
    let inp = EeUnitsUnitSaveIn { major: 0, minor: 0x100, ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { rts_save(&mut m, &rts, &inp) }, 0);
    let values: Vec<(i32, i64, u8)> = m.log.iter().map(|e| (e.0, e.2, e.4)).collect();
    assert_eq!(
        values,
        [
            (EE_UNITS_UNIT_SAVE_WRITE_LONG, 12, 0),
            (EE_UNITS_UNIT_SAVE_WRITE_LONG, -10000, 0),
            (EE_UNITS_UNIT_SAVE_WRITE_ULONG, 0x0200_0031, 0),
            (EE_UNITS_UNIT_SAVE_WRITE_BOOL, 0, 1),
            // the editor selection: always false
            (EE_UNITS_UNIT_SAVE_WRITE_BOOL, 0, 0),
            (EE_UNITS_UNIT_SAVE_WRITE_BOOL, 0, 1),
        ]
    );
    // up to 0.6: the id, then the tile
    let mut m = Mock::default();
    let old = EeUnitsUnitSaveIn { major: 0, minor: 6, ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { rts_save(&mut m, &rts, &old) }, 0);
    assert_eq!(m.log.iter().map(|e| e.2).collect::<Vec<_>>(), [0x0200_0031, 12, -10000]);
    // the first error ends it
    let mut m = Mock { fail_at: Some(2), ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { rts_save(&mut m, &rts, &inp) }, 1);
    assert_eq!(m.log.len(), 2);
}

#[test]
fn the_unit_writes_its_hit_points_as_a_fraction() {
    let (rts, unit, _) = records();
    let mut m = Mock::default();
    let inp = EeUnitsUnitSaveIn { major: 0, minor: 0x100, ..Default::default() };
    // SAFETY: live records
    assert_eq!(unsafe { unit_save(&mut m, &rts, &unit, &inp) }, 0);
    let tail: Vec<_> = m.log[6..].to_vec();
    assert_eq!(tail[0], (EE_UNITS_UNIT_SAVE_WRITE_ULONG, 0, 3, 0, 0));
    assert_eq!(tail[1].0, EE_UNITS_UNIT_SAVE_MAX);
    assert_eq!(tail[2], (EE_UNITS_UNIT_SAVE_WRITE_FLOAT, 0, 0, 0.5f32.to_bits(), 0));
    assert_eq!(tail[3], (EE_UNITS_UNIT_SAVE_WRITE_LONG, 0, 400, 0, 0));
    assert_eq!(tail[4], (EE_UNITS_UNIT_SAVE_WRITE_LONG, 0, 0, 0, 0));
    assert_eq!(tail.len(), 5);
}

#[test]
fn the_complex_unit_writes_every_section_in_order() {
    let (rts, unit, cx) = records();
    let mut m = Mock::default();
    let inp = EeUnitsUnitSaveIn { major: 0, minor: 0x118, live_state: 1, running_state: 1 };
    // SAFETY: live records
    let r = unsafe { unit_save(&mut m, &rts, &unit, &inp) };
    assert_eq!(r, 0);
    let base = m.log.len();
    // SAFETY: a live record
    assert_eq!(unsafe { complex_save_rest(&mut m, &cx, &inp) }, 0);
    let o = ops(&m.log[base..]);
    let sections: Vec<u32> = o.iter().filter(|e| e.0 == EE_UNITS_UNIT_SAVE_SECTION).map(|e| e.1).collect();
    assert_eq!(sections, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    let parts: Vec<u32> = o.iter().filter(|e| e.0 == EE_UNITS_UNIT_SAVE_PART).map(|e| e.1).collect();
    assert_eq!(parts, [0, 1, 2, 3, 4, 5, 9, 6, 7, 8]);
    let ids: Vec<u32> = o.iter().filter(|e| e.0 == EE_UNITS_UNIT_SAVE_ID).map(|e| e.1).collect();
    assert_eq!(ids, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 12, 13, 14, 15, 0]);
    // the fractions: last hit points 10 / 50, flight 5 / 20, garrison 3 / 4, power over a zero maximum: 0
    let floats: Vec<f32> =
        m.log[base..].iter().filter(|e| e.0 == EE_UNITS_UNIT_SAVE_WRITE_FLOAT).map(|e| f32::from_bits(e.3)).collect();
    assert!(floats.contains(&0.2) && floats.contains(&0.25) && floats.contains(&0.75));
    let writes = m.log[base..].iter().filter(|e| crate::units::player_save::is_save_write(e.0)).count();
    // 116 primitive writes with the live and running state (counted by hand from the original's statements)
    assert_eq!(writes, 116);
    // the next trigger time crosses as the unsigned long it is
    assert!(m.log[base..].iter().any(|e| e.0 == EE_UNITS_UNIT_SAVE_WRITE_ULONG && e.2 == 0xffff_0000));
}

#[test]
fn a_failed_write_goes_on_at_the_original_labels() {
    let (_, _, cx) = records();
    let inp = EeUnitsUnitSaveIn { major: 0, minor: 0x118, live_state: 1, running_state: 1 };
    // the waypoints' section fails (the 4th write): straight to the goal queues
    let mut m = Mock { fail_at: Some(4), ..Default::default() };
    // SAFETY: a live record
    let r = unsafe { complex_save_rest(&mut m, &cx, &inp) };
    assert_eq!(r, 0);
    let o = ops(&m.log);
    assert_eq!(o[3], (EE_UNITS_UNIT_SAVE_SECTION, 3));
    assert_eq!(o[4], (EE_UNITS_UNIT_SAVE_SECTION, 5));
    // the build queue's failure is not tested: the citizen queue's write starts a new result
    let mut m = Mock { fail_at: Some(1), ..Default::default() };
    // SAFETY: a live record
    unsafe { complex_save_rest(&mut m, &cx, &inp) };
    assert_eq!(ops(&m.log)[4], (EE_UNITS_UNIT_SAVE_PART, 0));
    // the held units fail: the growth .. foundation part is skipped, the unit variables still written
    let mut full = Mock::default();
    // SAFETY: a live record
    unsafe { complex_save_rest(&mut full, &cx, &inp) };
    let held = full.log.iter().position(|e| (e.0, e.1) == (EE_UNITS_UNIT_SAVE_SECTION, 6)).unwrap();
    let nth = full.log[..=held]
        .iter()
        .filter(|e| {
            e.0 != EE_UNITS_UNIT_SAVE_MAX
                && e.0 != EE_UNITS_UNIT_SAVE_ID
                && e.0 != EE_UNITS_UNIT_SAVE_FLOAT
                && e.0 != EE_UNITS_UNIT_SAVE_COLLISION_TILE
        })
        .count();
    let mut m = Mock { fail_at: Some(nth), ..Default::default() };
    // SAFETY: a live record
    unsafe { complex_save_rest(&mut m, &cx, &inp) };
    let o = ops(&m.log);
    let at = o.iter().position(|e| *e == (EE_UNITS_UNIT_SAVE_SECTION, 6)).unwrap();
    assert_eq!(o[at + 1], (EE_UNITS_UNIT_SAVE_SECTION, 7));
    // the unit variables fail: that result is the save's, nothing after
    let vars = full.log.iter().position(|e| (e.0, e.1) == (EE_UNITS_UNIT_SAVE_SECTION, 7)).unwrap();
    let nth = full.log[..=vars]
        .iter()
        .filter(|e| {
            e.0 != EE_UNITS_UNIT_SAVE_MAX
                && e.0 != EE_UNITS_UNIT_SAVE_ID
                && e.0 != EE_UNITS_UNIT_SAVE_FLOAT
                && e.0 != EE_UNITS_UNIT_SAVE_COLLISION_TILE
        })
        .count();
    let mut m = Mock { fail_at: Some(nth), ..Default::default() };
    // SAFETY: a live record
    assert_eq!(unsafe { complex_save_rest(&mut m, &cx, &inp) }, 1);
    assert_eq!(ops(&m.log).last(), Some(&(EE_UNITS_UNIT_SAVE_SECTION, 7)));
}

#[test]
fn ignored_results_do_not_stop_the_save() {
    let (_, _, cx) = records();
    let inp = EeUnitsUnitSaveIn { major: 0, minor: 0x118, ..Default::default() };
    let mut full = Mock::default();
    // SAFETY: a live record
    unsafe { complex_save_rest(&mut full, &cx, &inp) };
    // the collision tile's x: its failure is not tested
    let x = full.log.iter().position(|e| e.0 == EE_UNITS_UNIT_SAVE_COLLISION_TILE).unwrap() + 1;
    let nth = full.log[..=x]
        .iter()
        .filter(|e| {
            e.0 != EE_UNITS_UNIT_SAVE_MAX
                && e.0 != EE_UNITS_UNIT_SAVE_ID
                && e.0 != EE_UNITS_UNIT_SAVE_FLOAT
                && e.0 != EE_UNITS_UNIT_SAVE_COLLISION_TILE
        })
        .count();
    let mut m = Mock { fail_at: Some(nth), ..Default::default() };
    // SAFETY: a live record
    unsafe { complex_save_rest(&mut m, &cx, &inp) };
    assert_eq!(ops(&m.log), ops(&full.log));
    // without the live state the save ends after the unsigned long mU32C
    assert_eq!(full.log.last().map(|e| (e.0, e.2)), Some((EE_UNITS_UNIT_SAVE_WRITE_ULONG, 77)));
}
