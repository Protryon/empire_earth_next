use super::*;
use crate::cp::file::test_file::{file, Bytes};

fn sample() -> Timers {
    let mut t = Timers::ZERO;
    for i in 0..EE_CP_TIMER_PLAYERS {
        t.boats[i] = i as c_long;
        t.boat_hold[i] = 1000 + i as c_ulong;
        t.schedule_epoch[i] = -(i as c_long);
        t.threat_until[i] = 7;
        t.claim_steps[i] = 3;
        t.claim_last[i] = 9;
    }
    t
}

#[test]
fn the_timers_block_is_ten_arrays_of_64_and_loads_back_without_the_claims() {
    let t = sample();
    let mut b = Bytes::default();
    assert_eq!(save_timers(&mut file(&mut b), &t), OK);
    assert_eq!(b.data.len(), 10 * (4 + 64 * 4));
    assert_eq!(&b.data[..8], &[64, 0, 0, 0, 0, 0, 0, 0]);
    let (r, back) = load_timers(&mut file(&mut b));
    assert_eq!(r, OK);
    let mut want = t;
    want.claim_steps = [0; EE_CP_TIMER_PLAYERS];
    want.claim_last = [0; EE_CP_TIMER_PLAYERS];
    assert_eq!(back, Some(want));
}

#[test]
fn a_shorter_or_longer_array_keeps_the_first_64_and_zero_after() {
    let mut b = Bytes::default();
    // boats: 2 values; boat hold: 66 values (the last two dropped); then eight empty arrays
    b.u32(2).u32(5).u32(6);
    b.u32(66);
    for i in 0..66 {
        b.u32(100 + i);
    }
    for _ in 0..8 {
        b.u32(0);
    }
    let (r, t) = load_timers(&mut file(&mut b));
    assert_eq!(r, OK);
    let t = t.unwrap();
    assert_eq!((t.boats[0], t.boats[1], t.boats[2]), (5, 6, 0));
    assert_eq!((t.boat_hold[0], t.boat_hold[63]), (100, 163));
    assert_eq!(b.at, b.data.len());
    // cut short: an error and no copy
    let mut b = Bytes { data: b.data[..20].to_vec(), at: 0 };
    assert_eq!(load_timers(&mut file(&mut b)).1, None);
}

/// The host of the resource state's tests: zones by player, the pushes logged.
#[derive(Default)]
struct Zones {
    file: Bytes,
    zones: Vec<Vec<(i64, i64, i64)>>,
    pushed: Vec<Vec<(i64, i64, i64)>>,
    partial: [i64; 2],
}

unsafe extern "C" fn host(ctx: *mut std::ffi::c_void, op: i32, index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the tests' live Zones and io
    let (h, io) = unsafe { (&mut *(ctx as *mut Zones), &mut *io) };
    let p = index as usize;
    match op {
        EE_CP_STATE_ZONES => io.i = h.zones.get(p).map_or(0, |z| z.len() as i64),
        EE_CP_STATE_ZONE => {
            let z = h.zones[p][io.i as usize];
            io.i = [z.0, z.1, z.2][io.b as usize];
        }
        EE_CP_STATE_PUSH_PLAYER => h.pushed.push(Vec::new()),
        EE_CP_STATE_PUSH_ZONE => match io.b {
            0 | 1 => h.partial[io.b as usize] = io.i,
            _ => h.pushed[p].push((h.partial[0], h.partial[1], io.i)),
        },
        // SAFETY: the file's primitives over the Bytes
        _ => {
            return unsafe {
                crate::cp::file::test_file::host(&mut h.file as *mut Bytes as *mut std::ffi::c_void, op, 0, io)
            }
        }
    }
    OK
}

fn state_file(h: &mut Zones) -> SaveFile {
    // SAFETY: h outlives the file in each test
    unsafe { SaveFile::new(h as *mut Zones as *mut std::ffi::c_void, host) }
}

#[test]
fn the_resource_state_holds_the_claims_and_every_player_s_zones() {
    let t = sample();
    let mut h = Zones { zones: vec![vec![], vec![(3, -4, 500), (7, 8, 900)], vec![]], ..Default::default() };
    // three players: the third has no computer player (no entry), so it writes 0
    assert_eq!(save_resource_state(&mut state_file(&mut h), &t, 3), OK);
    assert_eq!(h.file.data.len(), 2 * (4 + 64 * 4) + 4 + 4 + (4 + 2 * 12) + 4);
    let mut back = Zones { file: Bytes { data: h.file.data.clone(), at: 0 }, ..Default::default() };
    let (r, claims) = load_resource_state(&mut state_file(&mut back));
    assert_eq!(r, OK);
    assert_eq!(claims, Some((t.claim_steps, t.claim_last)));
    assert_eq!(back.pushed, vec![vec![], vec![(3, -4, 500), (7, 8, 900)], vec![]]);
    // cut inside the second zone: an error, the zone pushed with what was read
    let cut = 2 * (4 + 64 * 4) + 4 + 4 + 4 + 12 + 4;
    let mut short = Zones { file: Bytes { data: h.file.data[..cut + 2].to_vec(), at: 0 }, ..Default::default() };
    let (r, claims) = load_resource_state(&mut state_file(&mut short));
    assert_ne!(r, OK);
    assert_eq!(claims, None);
    assert_eq!(short.pushed, vec![vec![], vec![(3, -4, 500), (7, 0, 0)]]);
}
