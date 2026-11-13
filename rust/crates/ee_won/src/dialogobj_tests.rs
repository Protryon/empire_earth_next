//! The Rust Dialog: its defaults and fields over a counting host, and the replay of the original's golden
//! (`golden_dialogobj.txt`, printed by `tests/won/won_dialogobj_test.cpp --print` over the original DialogBehavior.cpp
//! and the container goldens' world) through the container golden's replay.

use std::cell::RefCell;
use std::ffi::c_void;

use super::*;
use crate::ctobj::tests::{replay, sequences_of};

const GOLDEN: &str = include_str!("golden_dialogobj.txt");

thread_local! {
    static HELD: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn counting_host(_c: *mut c_void, op: Op, a: isize, _b: isize, _cc: isize, _d: isize) -> isize {
    if op == Op::ChildHold || op == Op::ChildRelease {
        HELD.with(|h| h.borrow_mut().push((op as u32, a)));
    }
    0
}

#[test]
fn the_constructor_state_and_the_default_button_reference() {
    let d = EeWonDialog::new(std::ptr::dangling_mut::<c_void>(), Some(counting_host), std::ptr::null());
    type F = EeWonDlgField;
    assert_eq!(d.field(F::AllowDrag as i32), 1);
    assert_eq!(d.field(F::KeepInRoot as i32), 1);
    assert_eq!(d.field(F::EndOnEscape as i32), 0);
    assert_eq!(d.field(F::EscapeId as i32), -1);
    assert_eq!(d.field(F::Dragging as i32), 0);
    d.call(EeWonDlgCall::SetEndOnEscape, 7, 0, 0);
    assert_eq!((d.field(F::EndOnEscape as i32), d.field(F::EscapeId as i32)), (1, 7));
    d.call(EeWonDlgCall::SetDontEndOnEscape, 0, 0, 0);
    assert_eq!((d.field(F::EndOnEscape as i32), d.field(F::EscapeId as i32)), (0, 7));
    d.set_field(F::AllowDrag as i32, 0);
    assert_eq!(d.field(F::AllowDrag as i32), 0);
    d.set_default_button(5);
    d.set_default_button(5);
    d.set_default_button(6);
    d.destroy();
    let hold = Op::ChildHold as u32;
    let release = Op::ChildRelease as u32;
    // the SmartPtr assignment: the old one released before the new one is held, nothing for the same one
    assert_eq!(HELD.with(|h| h.borrow().clone()), vec![(hold, 5), (release, 5), (hold, 6), (release, 6)]);
}

#[test]
fn golden_sequences_replay() {
    let seqs = sequences_of(GOLDEN);
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (name, lines) in &seqs {
        if let Some(f) = replay(lines) {
            panic!("{name}: {f}");
        }
    }
}

#[test]
fn golden_replay_fails_on_a_changed_rectangle() {
    // the first drag's second invalidation given the first one's rectangle (as if the rectangle were not shared)
    let seqs = sequences_of(GOLDEN);
    let pat = format!("op {} rect ", Op::DlgInvalidateRect as u32);
    let (name, lines) = seqs
        .iter()
        .find(|(_, l)| l.iter().filter(|x| x.starts_with(&pat)).any(|x| !x.starts_with(&format!("{pat}0 0 "))))
        .expect("a drag whose rectangle moved");
    let k = lines.iter().position(|x| x.starts_with(&pat) && !x.starts_with(&format!("{pat}0 0 "))).unwrap();
    let mut changed = lines.clone();
    let tail = changed[k][pat.len()..].splitn(3, ' ').nth(2).unwrap_or("").to_string();
    changed[k] = Box::leak(format!("{pat}0 0 {tail}").into_boxed_str());
    assert!(replay(&changed).is_some(), "{name} replayed with a changed rectangle");
    assert!(replay(lines).is_none());
}
