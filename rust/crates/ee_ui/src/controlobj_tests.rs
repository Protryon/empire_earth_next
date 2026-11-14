//! UIControl's record golden (`golden_controlobj.txt`): the first sequences the round-19 driver (`build/ui19`) printed
//! over UIControl.cpp at the round's base (a world of the form's slots, captures, groups, tab stop, the manager's
//! current form and surfaces, the rhombi and the events; the virtual `SetDirty` and `SetLocation` calling the base
//! back), replayed op for op against the Rust record. The driver's own calls: 90..95 the inline setters (a field
//! written), 99 a digest of the fields as the C++ getters read them.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_controlobj.txt");

/// The driver's `this` (the C++ control's address in its world).
const THIS: usize = 0x1000;

struct Fnv(u32);

impl Fnv {
    fn mix(&mut self, v: u64) {
        for i in 0..8 {
            self.0 = (self.0 ^ ((v >> (i * 8)) as u8) as u32).wrapping_mul(16777619);
        }
    }
}

fn rect_words(r: UiRect) -> [u64; 2] {
    [
        r.bottom.to_bits() as u64 | (r.left.to_bits() as u64) << 32,
        r.right.to_bits() as u64 | (r.top.to_bits() as u64) << 32,
    ]
}

fn digest(r: &EeUiControlFields) -> isize {
    let mut h = Fnv(2166136261);
    for w in rect_words(r.mRectangle).into_iter().chain(rect_words(r.mCopiedDrawRectangle)) {
        h.mix(w);
    }
    h.mix(r.mForm as u64);
    h.mix(r.mHotKey as u64);
    for v in [
        r.mDisplayMode,
        r.mControlType,
        r.mBorderType,
        r.mCopiedBorderType,
        r.mIndex,
        r.mId,
        r.mDrawCount,
        r.mBorderThickness,
        r.mStringTableID,
    ] {
        h.mix(v as u64);
    }
    h.mix(r.mBorderOpacity.to_bits() as u64);
    h.mix(r.mCopiedBorderOpacity.to_bits() as u64);
    let flags = [
        r.mIsDirty,
        r.mLeftButtonDown,
        r.mRightButtonDown,
        r.mCopiedHasBgOpacity,
        r.mUnknown9C,
        r.mHasBgOpacity,
        r.mAlwaysDirty,
    ];
    h.mix(flags.iter().enumerate().fold(0u64, |a, (i, &f)| a | (f as u64) << i));
    // SAFETY: the record's map, made by init
    for &(id, e) in &unsafe { &*r.mTriggerEvents }.v {
        h.mix(id as u64);
        h.mix(e as u64);
    }
    h.0 as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, _b: isize, f: *mut f32) -> isize {
    let rec = target as *mut EeUiControlFields;
    // SAFETY: the target is the test's live record; the driver's own calls make no call out
    let r = unsafe { &mut *rec };
    match call {
        90 => r.mAlwaysDirty = a != 0,
        91 => r.mHasBgOpacity = a != 0,
        // SAFETY: the replay passes four floats
        92 => r.mBorderOpacity = unsafe { *f },
        93 => r.mBorderType = a as c_ulong,
        94 => r.mDisplayMode = a as c_ulong,
        95 => r.mStringTableID = a as c_ulong,
        99 => return digest(r),
        // SAFETY: the call numbers are EeUiControlCall's (the golden's); no reference into the record is live
        _ => unsafe {
            return ee_ui_control_call(rec, core::mem::transmute::<u32, EeUiControlCall>(call), a, _b, f);
        },
    }
    0
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    // SAFETY: an all-zero record is a valid value (NULL pointers, no host, zeros)
    let rec: *mut EeUiControlFields = Box::into_raw(Box::new(unsafe { core::mem::zeroed::<EeUiControlFields>() }));
    r.target.set(rec as *const c_void);
    r.run(|_, w| match w[0] {
        "new" => {
            let n = |i: usize| w[i].parse::<isize>().unwrap();
            let mut f = [0.0f32; 4];
            for (i, x) in w[6..10].iter().enumerate() {
                f[i] = f32::from_bits(u32::from_str_radix(x, 16).unwrap());
            }
            // SAFETY: the test's record; four floats
            unsafe {
                ee_ui_control_init(
                    rec,
                    THIS as *mut c_void,
                    Some(host_current),
                    n(1) as c_ulong,
                    n(2) as c_ulong,
                    f.as_ptr(),
                    n(3) as *mut c_void,
                    n(4) as c_ulong,
                    n(5) as c_ulong,
                )
            };
            true
        }
        "free" => {
            // SAFETY: set up by "new"
            unsafe { ee_ui_control_release(rec) };
            true
        }
        _ => false,
    });
    // SAFETY: made above, released by the last "free"
    drop(unsafe { Box::from_raw(rec) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 1_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_background_fails() {
    // the constructor's background expected with another bottom edge: the Rust's line no longer matches
    let at = GOLDEN.find("\nop 71 0 ").expect("a background") + 1;
    let end = at + GOLDEN[at..].find('\n').unwrap();
    let mut w: Vec<&str> = GOLDEN[at..end].split(' ').collect();
    w[6] = "7f7f7f7f";
    let g = format!("{}{}{}", &GOLDEN[..at], w.join(" "), &GOLDEN[end..]);
    replay(&g);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_surface_count_fails() {
    // every surface count answered one more: a CopyData's draw count, then a digest, differs
    let g: String = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 218 0 ") {
            Some(rest) => {
                let (n, f) = rest.split_once(' ').unwrap();
                format!("set 218 0 {} {f}\n", n.parse::<i64>().unwrap() + 1)
            }
            None => format!("{l}\n"),
        })
        .collect();
    replay(&g);
}

#[test]
fn the_trigger_map_keeps_id_order() {
    let mut t = EeUiTriggers::default();
    for (id, e) in [(1003, 1), (1001, 2), (1002, 3)] {
        let i = t.find(id).unwrap_err();
        t.v.insert(i, (id, e));
    }
    assert_eq!(t.v.iter().map(|e| e.0).collect::<Vec<_>>(), vec![1001, 1002, 1003]);
}
