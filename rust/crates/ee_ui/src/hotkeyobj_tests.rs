//! UIHotKey's golden (`golden_hotkeyobj.txt`): the first sequences the round-20 driver (`build/ui20`) printed over
//! UIHotKey.cpp/.h at the round's base (a world of the keyboard's entities, their values and presses, the modifier
//! states, the events and forms; two hot keys, A by the default constructor and B copied from A partway), replayed op
//! for op against the Rust records. Calls on B are numbered 50 higher; the driver's own: 90..92 the inline setters
//! (`SetEnabled`, `SetAlwaysFire`, `SetForm` on hot key b), 99 a digest of both records.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_hotkeyobj.txt");

struct Fnv(u32);

impl Fnv {
    fn mix(&mut self, v: u64) {
        for i in 0..8 {
            self.0 = (self.0 ^ ((v >> (i * 8)) as u8) as u32).wrapping_mul(16777619);
        }
    }
}

fn digest(keys: &[EeUiHotKeyFields; 2]) -> isize {
    let mut h = Fnv(2166136261);
    for k in keys {
        h.mix(k.mKey as u64);
        for m in k.mModifiers {
            h.mix(m as u64);
        }
        h.mix(k.mDoublePressedEvent as u64);
        h.mix(k.mJustPressedEvent as u64);
        h.mix(k.mJustUnpressedEvent as u64);
        h.mix(k.mPressedEvent as u64);
        h.mix(k.mForm as u64);
        h.mix(k.mModifierState as u64);
        h.mix(k.mKeyCount as u64);
        h.mix(k.mEnabled as u64 | (k.mAlwaysFire as u64) << 1);
    }
    h.0 as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, _f: *mut f32) -> isize {
    let keys = target as *mut [EeUiHotKeyFields; 2];
    // SAFETY: the test's two live records; the driver's own calls make no call out
    let k = unsafe { &mut *keys };
    match call {
        90 => k[b as usize].mEnabled = a != 0,
        91 => k[b as usize].mAlwaysFire = a != 0,
        92 => k[b as usize].mForm = a as *mut c_void,
        99 => return digest(k),
        _ => {
            let (who, call) = if (50..90).contains(&call) { (1, call - 50) } else { (0, call) };
            let rec: *mut EeUiHotKeyFields = &mut k[who];
            // SAFETY: the call numbers are EeUiHotKeyCall's (the golden's); no reference is used across the call
            return unsafe { ee_ui_hotkey_call(rec, core::mem::transmute::<u32, EeUiHotKeyCall>(call), a, b) };
        }
    }
    0
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    // SAFETY: all-zero records are valid values (NULL pointers, no host, zeros)
    let keys: *mut [EeUiHotKeyFields; 2] = Box::into_raw(Box::new(unsafe { core::mem::zeroed() }));
    r.target.set(keys as *const c_void);
    // SAFETY: the test's records (each access a fresh raw place, no reference across the calls)
    let (a, b) = unsafe { (&raw mut (*keys)[0], &raw mut (*keys)[1]) };
    r.run(|_, w| match w[0] {
        "new" => {
            // SAFETY: the test's record
            unsafe { ee_ui_hotkey_init(a, Some(host_current)) };
            true
        }
        "copy" => {
            let form = w[1].parse::<isize>().unwrap() as *mut c_void;
            // SAFETY: the test's records
            unsafe { ee_ui_hotkey_copy(b, Some(host_current), a, form) };
            true
        }
        "free" => {
            // SAFETY: the test's records, made above
            unsafe {
                ee_ui_hotkey_call(a, EeUiHotKeyCall::Release, 0, 0);
                if w[1] == "1" {
                    ee_ui_hotkey_call(b, EeUiHotKeyCall::Release, 0, 0);
                }
            }
            true
        }
        _ => false,
    });
    // SAFETY: made above
    drop(unsafe { Box::from_raw(keys) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 600);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_keyboard_state_fails() {
    // every keyboard modifier state one higher: the fires that passed the state test no longer do
    let g: Vec<String> = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 574 0 ") {
            Some(rest) => {
                let w: Vec<&str> = rest.split(' ').collect();
                format!("set 574 0 {} {}", w[0].parse::<u32>().unwrap() + 1, w[1..].join(" "))
            }
            None => l.to_string(),
        })
        .collect();
    replay(&g.join("\n"));
}

#[test]
fn the_table_starts_empty() {
    assert_eq!(ee_ui_hotkey_count(), 0);
}
