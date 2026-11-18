//! UIBitmap's golden (`golden_mediaobj.txt`): the first sequences the round-21 driver (`build/ui21`) printed over
//! UIBitmap.cpp/.h and UIMovie.cpp at the round's base (a world of bitmaps, movies, their play state and the
//! control's rectangle), replayed op for op against the Rust record. Calls 1..6 are `EeUiBitmapCall`'s; the driver's
//! own: 7 `GetBitmap`, 99 a digest of the records (the movie's member mixed as the NULL it is in every bitmap
//! sequence). The movie sequences (calls 21..25, `UIMovie`'s) were taken out when movies left the game (2026-11-17).

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_mediaobj.txt");

struct Recs {
    b: EeUiBitmapFields,
}

fn digest(r: &Recs) -> isize {
    let mut h: u32 = 2166136261;
    let mut mix = |v: u64| {
        for i in 0..8 {
            h = (h ^ ((v >> (i * 8)) as u8) as u32).wrapping_mul(16777619);
        }
    };
    mix(r.b.mLocation[0].to_bits() as u64);
    mix(r.b.mLocation[1].to_bits() as u64);
    mix(r.b.mBitmap as u64);
    mix(r.b.mCopiedBitmap as u64);
    // the movie record's member, NULL in every sequence left
    mix(0);
    h as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, _b: isize, _f: *mut f32) -> isize {
    let recs = target as *mut Recs;
    match call {
        // SAFETY: the test's live records; the driver's own calls make no call out
        7 => unsafe { (*recs).b.mBitmap as isize },
        // SAFETY: as above
        99 => digest(unsafe { &*recs }),
        // SAFETY: the call numbers are the enum's (the golden's); no reference is used across the call
        _ => unsafe {
            bitmap_call(&raw mut (*recs).b, Some(host_current), core::mem::transmute::<u32, EeUiBitmapCall>(call), a)
        },
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    // SAFETY: all-zero records are valid values (NULL pointers, no host, zeros)
    let recs: *mut Recs = Box::into_raw(Box::new(unsafe { core::mem::zeroed() }));
    r.target.set(recs as *const c_void);
    r.run(|_, _| false);
    // SAFETY: made above
    drop(unsafe { Box::from_raw(recs) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 500);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_moved_rectangle_fails() {
    // every rectangle's left edge one further: the copied locations move
    let g: Vec<String> = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 6 0 0 ") {
            Some(rest) => {
                let w: Vec<&str> = rest.split(' ').collect();
                let left = f32::from_bits(u32::from_str_radix(w[1], 16).unwrap()) + 1.0;
                format!("set 6 0 0 {} {:08x} {} {}", w[0], left.to_bits(), w[2], w[3])
            }
            None => l.to_string(),
        })
        .collect();
    replay(&g.join("\n"));
}

#[test]
fn a_bitmap_control_without_a_bitmap_makes_no_call() {
    // SAFETY: zeroed is a valid record; no host is called: the bitmap is NULL
    let mut rec: EeUiBitmapFields = unsafe { core::mem::zeroed() };
    // SAFETY: the record is live; Init with NULL and Release with nothing held make no call out
    unsafe {
        assert_eq!(bitmap_call(&mut rec, None, EeUiBitmapCall::Init, 0), 0);
        assert_eq!(bitmap_call(&mut rec, None, EeUiBitmapCall::Release, 0), 0);
    }
    assert!(rec.mBitmap.is_null() && rec.mCopiedBitmap.is_null());
}
