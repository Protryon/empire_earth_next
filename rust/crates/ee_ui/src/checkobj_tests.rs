//! The check box's and radio button's goldens (`golden_checkobj.txt`, `golden_radioobj.txt`): the first sequences
//! the round-12 driver (`build/ui12`) printed over round 11's UICheckBox.cpp and UIRadioButton.cpp statements and
//! record FFI (the label a part, the box bitmap scaled while drawn, the radio walk over the form's siblings, trigger
//! events and subclass virtuals reading the button back), replayed op for op against the Rust button.

// the replay's sibling buttons are boxed: their addresses are handed out and must not move as the list grows
#![allow(clippy::vec_box)]

use super::*;
use crate::buttonobj::Sub;
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
pub(crate) fn replay_on(r: Replay) -> usize {
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
    replay_on(Replay::new(golden, dispatch))
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

// ---- the radio button (`golden_radioobj.txt`): its siblings are Rust radio buttons the replay makes, checked as the
// golden's `set 120 i c` lines say (the driver's world printed them for this replay; no op reads them)

const GOLDEN_RADIO: &str = include_str!("golden_radioobj.txt");

thread_local!(static SIBLINGS: core::cell::RefCell<Vec<Box<EeUiButton>>> = const { core::cell::RefCell::new(Vec::new()) });

/// `RadioOf`'s answer in the replay: a sibling radio button checked as the world's flag says, or none.
fn sibling(r: &Replay, op: crate::obj::EeUiOp, a: isize, flag: isize) -> isize {
    if op != crate::obj::EeUiOp::RadioOf || flag == 0 {
        return flag;
    }
    let checked = r.model_value(120, a).unwrap_or(0) != 0;
    let b = Box::new(EeUiButton::new(crate::obj::Host { obj: core::ptr::null_mut(), f: None }, null(), null(), null()));
    *b.sub.borrow_mut() = Sub::Radio(RadioFields { is_checked: checked, ..NO_RADIO });
    let p = &*b as *const EeUiButton as isize;
    SIBLINGS.with(|s| s.borrow_mut().push(b));
    p
}

fn null() -> *mut c_void {
    core::ptr::null_mut()
}

#[test]
fn radio_golden_replays() {
    let r = Replay::new(GOLDEN_RADIO, dispatch).with_pure_hook(sibling);
    assert!(replay_on(r) > 1_000);
    SIBLINGS.with(|s| s.borrow_mut().clear());
}

#[test]
#[should_panic(expected = "golden line")]
fn a_sibling_left_checked_fails() {
    // the sibling the first walk unchecks is said to be unchecked already: the walk no longer unchecks it
    let at = GOLDEN_RADIO.find("\nop 117 ").expect("a sibling unchecked");
    let i = GOLDEN_RADIO[at + 8..].split(' ').next().unwrap();
    let key = format!("\nset 120 {i} 1 ");
    let set = GOLDEN_RADIO[..at].rfind(&key).expect("its check state");
    let g = format!("{}\nset 120 {i} 0 {}", &GOLDEN_RADIO[..set], &GOLDEN_RADIO[set + key.len()..]);
    let r = Replay::new(&g, dispatch).with_pure_hook(sibling);
    replay_on(r);
}
