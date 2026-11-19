//! The computer player's lists: the centres' records and their addresses, the digests (the forwarders run over the
//! game's host: the PLCE captures of the placement and oracle profiles check them).

use super::fields::EeCpFields;
use super::lists::*;

fn record() -> EeCpFields {
    EeCpFields { mLists: Box::into_raw(Box::<EeCpLists>::default()) as usize as u64, ..Default::default() }
}

#[test]
fn centres_are_listed_at_either_end_and_keep_their_address() {
    let f = record();
    let a = new_center();
    let b = new_center();
    // SAFETY: the test's record and new centres
    unsafe {
        list_center(&f, a, false);
        list_center(&f, b, true);
        assert_eq!(handles(&f), vec![b, a]);
        center(a).units.push(7);
        assert_eq!(lists(&f).centers[1].units, vec![7]);
    }
}

#[test]
fn the_digests_hash_the_lists_as_the_capture_did() {
    let l = EeCpLists {
        centers: vec![Box::new(CpCenter { units: vec![3, 4], main: 9, active: true })],
        history: vec![(20, 1)],
        kill: vec![5, 6],
        zones: vec![EeCpDangerZone::default()],
    };
    // the FNV-1a 64 of the little-endian words (count, ids, main), as EECPPlaceCapture.cpp's
    let mut h: u64 = 0xcbf29ce484222325;
    for w in [2u32, 3, 4, 9] {
        for b in w.to_le_bytes() {
            h = (h ^ u64::from(b)).wrapping_mul(0x100000001b3);
        }
    }
    assert_eq!(centers_digest(&l), [1, h as u32, (h >> 32) as u32]);
    let d = lists_digest(&l);
    assert_eq!((d[0], d[2], d[3], d[4]), (1, 2, 1, 1));
}
