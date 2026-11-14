//! The scroll arrow's and blinking button's goldens (`golden_scrollobj.txt`, `golden_blinkobj.txt`): the first
//! sequences the round-12 driver (`build/ui12`)
//! printed over round 11's UIScrollButton.cpp statements and record FFI (the polls and the slider by handle, the
//! virtual ButtonDown and ButtonUp dispatched by the world), replayed op for op against the Rust button.

use core::ffi::c_void;

use crate::buttonobj::{ee_ui_button_call, ee_ui_button_free, ee_ui_button_new, EeUiButton, EeUiButtonCall};
use crate::obj::replay::Replay;

const GOLDEN_SCROLL: &str = include_str!("golden_scrollobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiButtonCall's (the golden's); the target is the live button
    unsafe {
        ee_ui_button_call(target as *const EeUiButton, core::mem::transmute::<u32, EeUiButtonCall>(call), a, b, f)
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiButton = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let p = |i: usize| w[i].parse::<isize>().unwrap() as *mut c_void;
            obj = ee_ui_button_new(o, h, p(1), p(2), p(3));
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_button_free(obj) };
            obj = core::ptr::null_mut();
            true
        }
        _ => false,
    });
    r.checked.get()
}

#[test]
fn scroll_golden_replays() {
    assert!(replay(GOLDEN_SCROLL) > 1_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_repeat_poll_that_ran_out_fails() {
    // the first poll answered as run out where it had not (or the reverse): the stepping's ops differ
    let at = GOLDEN_SCROLL.find("\nop 133 ").expect("a poll");
    let eq = at + 1 + GOLDEN_SCROLL[at + 1..].find("\n= ").unwrap() + 1;
    let old = &GOLDEN_SCROLL[eq + 2..eq + 3];
    let new = if old == "0" { "1" } else { "0" };
    let g = format!("{}= {}{}", &GOLDEN_SCROLL[..eq], new, &GOLDEN_SCROLL[eq + 3..]);
    replay(&g);
}

// ---- the blinking button (`golden_blinkobj.txt`)

const GOLDEN_BLINK: &str = include_str!("golden_blinkobj.txt");

#[test]
fn blink_golden_replays() {
    assert!(replay(GOLDEN_BLINK) > 1_000);
}
