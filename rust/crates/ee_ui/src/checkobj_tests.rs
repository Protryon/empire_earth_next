//! The check box's golden (`golden_checkobj.txt`): the first sequences the round-12 driver (`build/ui12`) printed over
//! round 11's UICheckBox.cpp statements and record FFI (the label a part, the box bitmap scaled while drawn, trigger
//! events and subclass virtuals reading the box back), replayed op for op against the Rust button.

use super::*;
use crate::buttonobj::{ee_ui_button_call, ee_ui_button_free, ee_ui_button_new};
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_checkobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiButtonCall's (the golden's); the target is the live button
    unsafe {
        ee_ui_button_call(target as *const EeUiButton, core::mem::transmute::<u32, EeUiButtonCall>(call), a, b, f)
    }
}

/// Replays a subclass golden: `new` makes the button, the first `call` (the constructor's init) sets its record.
pub(crate) fn replay_with(golden: &str, d: crate::obj::replay::Dispatch) -> usize {
    let r = Replay::new(golden, d);
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

pub(crate) fn replay(golden: &str) -> usize {
    replay_with(golden, dispatch)
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 1_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_label_fails() {
    // the first label made answers another handle: its later ops no longer match
    let at = GOLDEN.find("\nop 110 ").expect("a label made");
    let eq = at + 1 + GOLDEN[at + 1..].find("\n= ").unwrap() + 1;
    let rest = &GOLDEN[eq + 2..];
    let g = format!("{}= 9{}", &GOLDEN[..eq], &rest[rest.find(' ').unwrap()..]);
    replay(&g);
}
