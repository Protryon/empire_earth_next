use super::*;

/// A file of 4-byte words with the C++ parts as tagged words; `fail_at` makes the n-th operation fail.
#[derive(Default)]
struct MockIo {
    words: Vec<(u32, u32)>,
    at: usize,
    ops: Vec<u32>,
    fail_at: Option<usize>,
    loading: bool,
    target_id: c_ulong,
    caster_id: c_ulong,
}

impl CalamityIo for MockIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        self.ops.push(op);
        if self.fail_at == Some(self.ops.len() - 1) {
            return 9;
        }
        // SAFETY: the op's type, as load/save pass it
        unsafe {
            match op {
                EE_CALAMITY_IO_READ_ULONG => *(value as *mut c_ulong) = self.next(op) as c_ulong,
                EE_CALAMITY_IO_READ_LONG => *(value as *mut c_long) = self.next(op) as i32 as c_long,
                EE_CALAMITY_IO_READ_FLOAT => *(value as *mut f32) = f32::from_bits(self.next(op)),
                EE_CALAMITY_IO_READ_BOOL => *(value as *mut bool) = self.next(op) != 0,
                EE_CALAMITY_IO_WRITE_ULONG => self.words.push((op, *(value as *mut c_ulong) as u32)),
                EE_CALAMITY_IO_WRITE_LONG => self.words.push((op, *(value as *mut c_long) as u32)),
                EE_CALAMITY_IO_WRITE_FLOAT => self.words.push((op, (*(value as *mut f32)).to_bits())),
                EE_CALAMITY_IO_WRITE_BOOL => self.words.push((op, *(value as *mut bool) as u32)),
                EE_CALAMITY_IO_TARGET_ID => *(value as *mut c_ulong) = self.target_id,
                EE_CALAMITY_IO_CASTER_ID => *(value as *mut c_ulong) = self.caster_id,
                _ => {
                    if self.loading {
                        self.next(op);
                    } else {
                        self.words.push((op, 0));
                    }
                }
            }
        }
        0
    }
}

impl MockIo {
    fn next(&mut self, _op: u32) -> u32 {
        let w = self.words[self.at].1;
        self.at += 1;
        w
    }
}

fn sample() -> EeCalamityFields {
    EeCalamityFields {
        mEpicenterFactor: 0.75,
        mFloatPlaceHolder1: 1.5,
        mFloatPlaceHolder2: -2.0,
        mHeight: 3.25,
        mTimeAlive: 1200.0,
        mDiscreteUpdateTime: 100.0,
        mTimeElapsedSinceLastUpdate: 12.5,
        mIntPlaceHolder1: 7,
        mIntPlaceHolder2: -1,
        mTargetUnitID: 4321,
        mOriginatingUnitID: 99,
        mPlayerNumber: 3,
        mBoolPlaceHolder1: true,
        mBoolPlaceHolder2: false,
        mInitialized: true,
        mHasTargetLocation: false,
        mHasTargetUnit: true,
    }
}

#[test]
fn reset_sets_the_numbers_of_a_new_calamity() {
    let mut f = sample();
    reset(&mut f);
    assert_eq!(
        f,
        EeCalamityFields {
            mEpicenterFactor: 1.0,
            mTargetUnitID: !0,
            mOriginatingUnitID: !0,
            ..EeCalamityFields::default()
        }
    );
}

#[test]
fn the_checksum_sums_bits_ids_and_flags() {
    let f = sample();
    let target = [10.5f32, 20.25, 1.0];
    let want = [
        target[0].to_bits() as i32 as i64,
        target[1].to_bits() as i32 as i64,
        target[2].to_bits() as i32 as i64,
        4,
        6,
        7,
        -1,
        4321,
        99,
        3,
        1 + 1 + 1,
        1.5f32.to_bits() as i32 as i64,
        (-2.0f32).to_bits() as i32 as i64,
        3.25f32.to_bits() as i32 as i64,
        100.0f32.to_bits() as i32 as i64,
        12.5f32.to_bits() as i32 as i64,
    ]
    .iter()
    .fold(0i64, |a, b| a.wrapping_add(*b));
    assert_eq!(checksum(&f, target, [4, 6]) as i64 as i32, want as i32);
    // the wrap of a 32-bit long
    let big = EeCalamityFields { mTargetUnitID: 0xffff_fff0, mOriginatingUnitID: 0x20, ..EeCalamityFields::default() };
    assert_eq!(checksum(&big, [0.0; 3], [0, 0]) as i32, 0x10);
}

#[test]
fn save_writes_the_original_order_and_load_reads_it_back() {
    let f = sample();
    let mut io = MockIo { target_id: 4321, caster_id: 99, ..MockIo::default() };
    assert_eq!(save(&mut io, &f), 0);
    assert_eq!(
        io.ops,
        [
            EE_CALAMITY_IO_RECTANGLE,
            EE_CALAMITY_IO_TARGET_LOCATION,
            EE_CALAMITY_IO_SHAKE_LOCATION,
            EE_CALAMITY_IO_EFFECT_GENERATION,
            EE_CALAMITY_IO_TARGET_ID,
            EE_CALAMITY_IO_WRITE_ULONG,
            EE_CALAMITY_IO_CLASS,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_WRITE_LONG,
            EE_CALAMITY_IO_WRITE_LONG,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_CASTER_ID,
            EE_CALAMITY_IO_WRITE_ULONG,
            EE_CALAMITY_IO_WRITE_ULONG,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_WRITE_FLOAT,
            EE_CALAMITY_IO_TILE_X,
            EE_CALAMITY_IO_TILE_Y,
            EE_CALAMITY_IO_EFFECT_HANDLE,
            EE_CALAMITY_IO_WRITE_BOOL,
            EE_CALAMITY_IO_WRITE_BOOL,
            EE_CALAMITY_IO_WRITE_BOOL,
            EE_CALAMITY_IO_WRITE_BOOL,
            EE_CALAMITY_IO_WRITE_BOOL,
        ]
    );
    let mut back = EeCalamityFields::default();
    let mut io = MockIo { words: io.words, loading: true, ..MockIo::default() };
    assert_eq!(load(&mut io, &mut back), 0);
    assert_eq!(back, f);
    assert_eq!(io.ops.len(), 25);
}

#[test]
fn a_failed_read_stops_the_load_with_its_error() {
    let mut io = MockIo { target_id: 1, caster_id: 2, ..MockIo::default() };
    save(&mut io, &sample());
    let mut back = EeCalamityFields { mHeight: 9.0, ..EeCalamityFields::default() };
    // the 10th operation is the height's read
    let mut io = MockIo { words: io.words, fail_at: Some(9), loading: true, ..MockIo::default() };
    assert_eq!(load(&mut io, &mut back), 9);
    assert_eq!(io.ops.len(), 10);
    assert_eq!((back.mFloatPlaceHolder2, back.mHeight, back.mTimeAlive), (-2.0, 9.0, 0.0));
}

#[test]
fn a_failed_write_stops_the_save_with_its_error() {
    let mut io = MockIo { fail_at: Some(2), ..MockIo::default() };
    assert_eq!(save(&mut io, &sample()), 9);
    assert_eq!(io.ops.len(), 3);
    let mut io = MockIo { fail_at: Some(12), ..MockIo::default() };
    assert_eq!(save(&mut io, &sample()), 9);
    assert_eq!(io.ops.len(), 13);
}
