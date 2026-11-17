use super::*;

/// A file of 4-byte words and a C++ side of unit ids and helpers.
#[derive(Default)]
struct MockIo {
    words: Vec<u32>,
    at: usize,
    ops: Vec<u32>,
    fail_at: Option<usize>,
    units: Vec<c_ulong>,
    pushed: Vec<c_ulong>,
    helpers: Vec<Option<c_ulong>>,
    loaded: Vec<c_ulong>,
    cleared: bool,
}

impl ContagionIo for MockIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        self.ops.push(op);
        if self.fail_at == Some(self.ops.len() - 1) {
            return 9;
        }
        // SAFETY: the op's type, as the bodies pass it
        unsafe {
            match op {
                EE_CONTAGION_IO_READ_ULONG => {
                    *(value as *mut c_ulong) = self.words[self.at] as c_ulong;
                    self.at += 1;
                }
                EE_CONTAGION_IO_READ_LONG => {
                    *(value as *mut c_long) = self.words[self.at] as i32 as c_long;
                    self.at += 1;
                }
                EE_CONTAGION_IO_READ_FLOAT => {
                    *(value as *mut f32) = f32::from_bits(self.words[self.at]);
                    self.at += 1;
                }
                EE_CONTAGION_IO_WRITE_ULONG => self.words.push(*(value as *mut c_ulong) as u32),
                EE_CONTAGION_IO_WRITE_LONG => self.words.push(*(value as *mut c_long) as u32),
                EE_CONTAGION_IO_WRITE_FLOAT => self.words.push((*(value as *mut f32)).to_bits()),
                EE_CONTAGION_IO_PUSH_UNIT_ID => self.pushed.push(*(value as *mut c_ulong)),
                EE_CONTAGION_IO_UNIT_COUNT => *(value as *mut c_ulong) = self.units.len() as c_ulong,
                EE_CONTAGION_IO_UNIT_ID => {
                    let i = *(value as *mut c_ulong) as usize;
                    *(value as *mut c_ulong) = self.units[i];
                }
                EE_CONTAGION_IO_CLEAR => self.cleared = true,
                EE_CONTAGION_IO_LOAD_HELPER => {
                    self.loaded.push(*(value as *mut c_ulong));
                    self.at += 1; // the helper's own words: one here
                }
                EE_CONTAGION_IO_HELPER_COUNT => *(value as *mut c_ulong) = self.helpers.len() as c_ulong,
                EE_CONTAGION_IO_HELPER_CALAMITY => {
                    let i = *(value as *mut c_ulong) as usize;
                    match self.helpers[i] {
                        Some(index) => *(value as *mut c_ulong) = index,
                        None => return 1,
                    }
                }
                EE_CONTAGION_IO_SAVE_HELPER => self.words.push(0xabcd),
                _ => return 1,
            }
        }
        0
    }
}

fn helper() -> EeContagionHelperFields {
    EeContagionHelperFields { mHelperID: 4, mTimeSinceLastSpread: 250.5, mTimeSinceLastUpdate: 12.0 }
}

#[test]
fn a_helper_saves_and_loads_back() {
    let mut io = MockIo { units: vec![11, 12, 13], ..MockIo::default() };
    assert_eq!(helper_save(&mut io, &helper()), 0);
    assert_eq!(io.words[..4], [3, 11, 12, 13]);
    let mut io = MockIo { words: io.words, ..MockIo::default() };
    let mut back = EeContagionHelperFields::default();
    assert_eq!(helper_load(&mut io, &mut back, 0, 0x118), 0);
    assert_eq!(back, helper());
    assert_eq!(io.pushed, [11, 12, 13]);
}

#[test]
fn an_old_helper_has_no_update_timer() {
    let mut io = MockIo { units: vec![5], ..MockIo::default() };
    helper_save(&mut io, &helper());
    let mut io = MockIo { words: io.words, ..MockIo::default() };
    let mut back = EeContagionHelperFields { mTimeSinceLastUpdate: 7.0, ..Default::default() };
    assert_eq!(helper_load(&mut io, &mut back, 0, 0xe6), 0);
    assert_eq!((back.mHelperID, back.mTimeSinceLastSpread, back.mTimeSinceLastUpdate), (4, 250.5, 7.0));
    assert!(without_update_timer(0, 0xe6) && !without_update_timer(0, 0xe7) && !without_update_timer(1, 0));
}

#[test]
fn a_failed_unit_read_still_pushes_and_stops() {
    let mut io = MockIo { words: vec![3, 11, 12, 13, 4], fail_at: Some(3), ..MockIo::default() };
    let mut back = EeContagionHelperFields::default();
    assert_eq!(helper_load(&mut io, &mut back, 0, 0x118), 9);
    // the failed read leaves the id the last one read (the original's one local)
    assert_eq!(io.pushed, [11, 11]);
}

#[test]
fn the_helper_checksum() {
    let f = helper();
    let want = (30i64 + 5 + 4 + 250.5f32.to_bits() as i64) as i32;
    assert_eq!(helper_checksum(&f, 30, 5) as i32, want);
}

#[test]
fn the_manager_saves_indexes_and_helpers_and_loads_them() {
    let mut io = MockIo { helpers: vec![Some(2), Some(0)], ..MockIo::default() };
    assert_eq!(manager_save(&mut io, 17, true), 0);
    assert_eq!(io.words, [2, 2, 0xabcd, 0, 0xabcd, 17]);
    let mut io = MockIo { words: io.words, ..MockIo::default() };
    let mut next = -5;
    assert_eq!(manager_load(&mut io, &mut next, true), 0);
    assert!(io.cleared);
    assert_eq!((io.loaded.clone(), next), (vec![2, 0], 17));
    let mut io = MockIo { words: vec![0, 99], ..MockIo::default() };
    assert_eq!(manager_load(&mut io, &mut next, false), 0);
    assert_eq!(next, 0);
}

#[test]
fn a_helper_without_its_calamity_fails_the_save_with_9() {
    let mut io = MockIo { helpers: vec![Some(1), None, Some(0)], ..MockIo::default() };
    assert_eq!(manager_save(&mut io, 3, true), 9);
    assert_eq!(io.words, [3, 1, 0xabcd]);
}
