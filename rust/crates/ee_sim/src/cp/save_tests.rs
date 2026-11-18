use super::*;
use crate::cp::file::test_file::Bytes;

/// The host of the tests: a byte file, the managers' blocks as one marker word each, a list of centre "records" (one
/// word each) and the history.
#[derive(Default)]
struct Host {
    file: Bytes,
    tile: [i64; 2],
    centers: Vec<u32>,
    history: Vec<(u32, i32)>,
    log: Vec<i32>,
}

const COMBAT: u32 = 0xc0c0_c0c0;
const PLACEMENT: u32 = 0x9191_9191;

unsafe extern "C" fn host(ctx: *mut c_void, op: i32, index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the tests pass a live Host and io
    let (h, io) = unsafe { (&mut *(ctx as *mut Host), &mut *io) };
    if op >= 400 {
        h.log.push(op);
    }
    let file = &mut h.file as *mut Bytes as *mut c_void;
    let word = |h: &mut Host| {
        let mut w = EeUnitsPlayerSaveIo::default();
        // SAFETY: the file
        let r = unsafe { crate::cp::file::test_file::host(&mut h.file as *mut Bytes as *mut c_void, 1, 0, &mut w) };
        (r, w.i as u32)
    };
    match op {
        EE_CP_SAVE_COMBAT_SAVE | EE_CP_SAVE_PLACEMENT_SAVE => {
            h.file.u32(if op == EE_CP_SAVE_COMBAT_SAVE { COMBAT } else { PLACEMENT });
            OK
        }
        EE_CP_SAVE_COMBAT_LOAD | EE_CP_SAVE_PLACEMENT_LOAD => {
            let (r, w) = word(h);
            assert_eq!(w, if op == EE_CP_SAVE_COMBAT_LOAD { COMBAT } else { PLACEMENT });
            r
        }
        EE_CP_SAVE_TOWN_TILE => {
            io.i = h.tile[index as usize];
            OK
        }
        EE_CP_SAVE_SET_TOWN_TILE => {
            h.tile[index as usize] = io.i;
            OK
        }
        EE_CP_SAVE_CENTERS => {
            io.i = h.centers.len() as i64;
            OK
        }
        EE_CP_SAVE_CENTER_SAVE => {
            let c = h.centers[index as usize];
            h.file.u32(c);
            OK
        }
        EE_CP_SAVE_CENTER_LOAD => {
            let (r, w) = word(h);
            h.centers.push(w);
            r
        }
        EE_CP_SAVE_HISTORY_COUNT => {
            io.i = h.history.len() as i64;
            OK
        }
        EE_CP_SAVE_HISTORY_RESIZE => {
            h.history.resize(io.i as usize, (0, 0));
            OK
        }
        EE_CP_SAVE_HISTORY_FIRST => {
            io.i = i64::from(h.history[index as usize].0);
            OK
        }
        EE_CP_SAVE_HISTORY_SECOND => {
            io.i = i64::from(h.history[index as usize].1);
            OK
        }
        EE_CP_SAVE_HISTORY_SET_FIRST => {
            h.history[index as usize].0 = io.i as u32;
            OK
        }
        EE_CP_SAVE_HISTORY_SET_SECOND => {
            h.history[index as usize].1 = io.i as i32;
            OK
        }
        // SAFETY: the file's primitives
        _ => unsafe { crate::cp::file::test_file::host(file, op, index, io) },
    }
}

/// A record whose every field has its own value (the k-th field: k + 1, as a word, a float k + 0.5, or a bool of
/// k's parity).
macro_rules! set_distinct {
    ($f:ident, $k:ident; $($n:ident),* $(,)?) => {
        $( $f.$n = distinct_of(&$f.$n, $k); $k += 1; )*
    };
}

fn save_of(f: &EeCpFields, h: &mut Host, minor: u32, time: u32) -> i32 {
    // SAFETY: a live host and record
    unsafe { save(h as *mut Host as *mut c_void, host, f, 0, minor, time, OK) }
}

fn distinct() -> EeCpFields {
    let mut f = EeCpFields::default();
    let mut k = 0u32;
    // through the save's field lists: every saved field (the unsaved ones keep 0)
    seg_a!(set_distinct, f, k);
    seg_b!(set_distinct, f, k);
    seg_c!(set_distinct, f, k);
    seg_d!(set_distinct, f, k);
    seg_e!(set_distinct, f, k);
    seg_f!(set_distinct, f, k);
    seg_g!(set_distinct, f, k);
    seg_h!(set_distinct, f, k);
    seg_i!(set_distinct, f, k);
    set_distinct!(f, k; mIsEnableTimeSet, mNextAttackTime, mNextExpansionTime, mPopulationCap, mRegroupPending,
        mNextPopulationTime, mLastDamagedTime);
    f
}

/// The bytes a field takes in the file (a `long` is 4 bytes on every data model).
trait FileSize {
    const SIZE: usize;
}
impl FileSize for c_long {
    const SIZE: usize = 4;
}
impl FileSize for c_ulong {
    const SIZE: usize = 4;
}
impl FileSize for f32 {
    const SIZE: usize = 4;
}
impl FileSize for bool {
    const SIZE: usize = 1;
}
fn size_of_field<T: FileSize>(_: &T) -> usize {
    T::SIZE
}
macro_rules! add_size {
    ($f:expr, $n_bytes:ident; $($n:ident),* $(,)?) => {
        $( $n_bytes += size_of_field(&$f.$n); )*
    };
}

trait Distinct {
    fn distinct(k: u32) -> Self;
}
impl Distinct for c_long {
    fn distinct(k: u32) -> Self {
        (k + 1) as c_long
    }
}
impl Distinct for c_ulong {
    fn distinct(k: u32) -> Self {
        (k + 1) as c_ulong
    }
}
impl Distinct for f32 {
    fn distinct(k: u32) -> Self {
        k as f32 + 0.5
    }
}
impl Distinct for bool {
    fn distinct(k: u32) -> Self {
        k % 2 == 1
    }
}
fn distinct_of<T: Distinct>(_: &T, k: u32) -> T {
    T::distinct(k)
}

fn load_into(f: &mut EeCpFields, h: &mut Host, minor: u32, out: &mut EeCpLoaded) -> i32 {
    // SAFETY: as above
    unsafe { load(h as *mut Host as *mut c_void, host, f, 0, minor, OK, out) }
}

#[test]
fn a_save_loads_back_to_the_same_record_with_the_kept_differences() {
    let mut f = distinct();
    f.mNextAttackTime = 70_000;
    f.mNextExpansionTime = 5_000; // before the clock: saved wrapped
    f.mIsEnableTimeSet = true;
    let mut h = Host { centers: vec![11, 12, 13], history: vec![(20, -1), (242, 7)], ..Default::default() };
    assert_eq!(save_of(&f, &mut h, 283, 60_000), OK);
    // the words: the two markers, 197 shared fields (some bools), the four specials, 3 centres, the history, the
    // running state
    let mut back = Host { file: Bytes { data: h.file.data.clone(), at: 0 }, ..Default::default() };
    let mut g = EeCpFields::default();
    let mut out = EeCpLoaded::default();
    assert_eq!(load_into(&mut g, &mut back, 283, &mut out), OK);
    assert_eq!(back.file.at, back.file.data.len(), "read to the end");
    assert_eq!(back.centers, vec![11, 12, 13]);
    assert_eq!(back.history, vec![(20, -1), (242, 7)]);
    // the enable-time flag is saved as false, the times relative to the clock (RestoreManagers rebases them)
    assert!(!g.mIsEnableTimeSet);
    assert_eq!(g.mNextAttackTime, 10_000);
    assert_eq!(g.mNextExpansionTime as u32, 5_000u32.wrapping_sub(60_000));
    // the running state goes to `out`, not the record
    assert_eq!(
        out,
        EeCpLoaded {
            favorites_ok: true,
            running: true,
            population_cap: f.mPopulationCap,
            enable_time_set: true,
            regroup_pending: f.mRegroupPending,
            next_population_time: f.mNextPopulationTime,
            last_damaged_time: f.mLastDamagedTime,
        }
    );
    let mut want = f;
    want.mIsEnableTimeSet = false;
    want.mNextAttackTime = 10_000;
    want.mNextExpansionTime = g.mNextExpansionTime;
    want.mPopulationCap = 0;
    want.mRegroupPending = false;
    want.mNextPopulationTime = 0;
    want.mLastDamagedTime = 0;
    assert_eq!(g, want);
    assert_eq!(&h.log[..2], &[EE_CP_SAVE_COMBAT_SAVE, EE_CP_SAVE_PLACEMENT_SAVE]);
}

#[test]
fn the_block_starts_with_the_original_s_fields_in_order() {
    let f = distinct();
    let mut h = Host::default();
    assert_eq!(save_of(&f, &mut h, 283, 0), OK);
    let d = &h.file.data;
    let w = |at: usize| u32::from_le_bytes([d[at], d[at + 1], d[at + 2], d[at + 3]]);
    assert_eq!((w(0), w(4)), (COMBAT, PLACEMENT));
    // mLBD4, mFavoriteLandType, mNextEnableTime, then false, then three bools
    assert_eq!((w(8), w(12), w(16)), (f.mLBD4 as u32, f.mFavoriteLandType as u32, f.mNextEnableTime as u32));
    assert_eq!(&d[20..24], &[0, u8::from(f.mBuildingDamaged), u8::from(f.mCitizenDamaged), u8::from(f.mIsStarted)]);
    assert_eq!(w(24), f.mNextAttackTime as u32);
    assert_eq!(d[28], u8::from(f.mActive));
    assert_eq!(w(29), 0, "no centres");
    assert_eq!(d[33], u8::from(f.mUsesFishing));
    assert_eq!(f32::from_le_bytes([d[34], d[35], d[36], d[37]]), f.mUnitCostMedium);
    // the whole block: the 2 markers, the shared fields by their file sizes, the enable flag, the two times, the
    // two counts, the running state's 3 words and 2 bools
    let mut n = 2 * 4 + 1 + 2 * 4 + 2 * 4 + 3 * 4 + 2;
    seg_a!(add_size, f, n);
    seg_b!(add_size, f, n);
    seg_c!(add_size, f, n);
    seg_d!(add_size, f, n);
    seg_e!(add_size, f, n);
    seg_f!(add_size, f, n);
    seg_g!(add_size, f, n);
    seg_h!(add_size, f, n);
    seg_i!(add_size, f, n);
    assert_eq!(d.len(), n);
}

#[test]
fn a_refused_version_reads_nothing_and_every_lua_only_version_has_the_running_state() {
    let f = distinct();
    let mut h = Host::default();
    assert_eq!(save_of(&f, &mut h, 283, 0), OK);
    // 0.251-0.280 (the C++ managers' blocks with the port's additions) and before 0.229 fail at once
    for minor in [255, 280, 0xe4] {
        let mut back = Host { file: Bytes { data: h.file.data.clone(), at: 0 }, ..Default::default() };
        let mut g = EeCpFields::default();
        let mut out = EeCpLoaded { favorites_ok: true, ..Default::default() };
        assert_eq!(load_into(&mut g, &mut back, minor, &mut out), FAILED, "minor {minor}");
        assert_eq!((back.file.at, back.log.len()), (0, 0), "nothing read");
        assert_eq!(out, EeCpLoaded::default());
    }
    // 0.281 on: the same block, the running state included (0.256 on)
    let mut back = Host { file: Bytes { data: h.file.data.clone(), at: 0 }, ..Default::default() };
    let mut g = EeCpFields::default();
    let mut out = EeCpLoaded::default();
    assert_eq!(load_into(&mut g, &mut back, 281, &mut out), OK);
    assert_eq!(back.file.at, back.file.data.len());
    assert!(out.favorites_ok && out.running);
}

#[test]
fn a_short_file_keeps_the_first_error_and_the_history_s_first_values_are_read_on() {
    let f = distinct();
    let mut h = Host { history: vec![(1, 2), (3, 4), (5, 6)], ..Default::default() };
    assert_eq!(save_of(&f, &mut h, 283, 0), OK);
    // cut inside the history's second entry's second value
    // the history's count (3) and first entry (1, 2)
    let d = &h.file.data;
    let at = (0..d.len() - 12).find(|&i| d[i..i + 12] == [3, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0]).expect("the history");
    let mut back =
        Host { file: Bytes { data: h.file.data[..at + 4 + 8 + 4 + 2].to_vec(), at: 0 }, ..Default::default() };
    let mut g = EeCpFields::default();
    let mut out = EeCpLoaded::default();
    assert_eq!(load_into(&mut g, &mut back, 283, &mut out), 0xa);
    // the first entry whole, the second's first value, its second not; the third's first read again (it fails too)
    assert_eq!(back.history, vec![(1, 2), (3, 0), (0, 0)]);
    assert!(!out.favorites_ok && !out.running);
}
