//! The Rust ComboBox: its defaults and fields, and the replay of the original's golden (`golden_comboobj.txt`, printed
//! by `tests/won/won_comboobj_test.cpp --print` over the original ComboBox.cpp, the container goldens' world and a
//! model of the parts) through the container golden's replay. Every combo op is traced with its answer, so the replay
//! needs no model of the parts.

use std::cell::Cell;
use std::ffi::c_void;

use super::*;
use crate::ctobj::tests::{env_of, fail, next_line, replay_host, replay_with, sequences_of, Ext};
use crate::ctobj::EeWonCtCall;

const GOLDEN: &str = include_str!("golden_comboobj.txt");

#[test]
fn the_constructor_state_and_the_fields() {
    let c = EeWonComboBox::new(std::ptr::dangling_mut::<c_void>(), None, std::ptr::null());
    type F = EeWonComboField;
    let f = |k: F| c.field(k as i32);
    assert_eq!([F::ListBoxVisible, F::NoType, F::ShowButtonWhenDisabled].map(f), [0, 0, 1]);
    assert_eq!((f(F::ListBoxWidth), f(F::ListBoxHeight)), (200, 150));
    c.set_field(F::ShowButtonWhenDisabled as i32, 0);
    // only the config's flag is settable
    c.set_field(F::NoType as i32, 1);
    assert_eq!((f(F::ShowButtonWhenDisabled), f(F::NoType)), (0, 0));
    // the sizes are set without a list area (no host: every part reads 0, and the size goes nowhere)
    c.call(EeWonComboCall::SetListBoxDimensions, 30, 40, 0);
    c.call(EeWonComboCall::SetListBoxWidth, 50, 0, 0);
    c.call(EeWonComboCall::SetListBoxHeight, 60, 0, 0);
    assert_eq!((f(F::ListBoxWidth), f(F::ListBoxHeight)), (50, 60));
    // no list area: no string is added
    assert_eq!(c.call(EeWonComboCall::AddString, 1, -1, 0), 0);
}

// ---- the golden's replay: component 1 the combo box over container 1

thread_local! {
    static COMBO: Cell<usize> = const { Cell::new(0) };
}

fn combo_of() -> Option<&'static EeWonComboBox> {
    let p = COMBO.with(|t| t.get());
    // SAFETY: the replay's combo box lives until its "free-combo" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonComboBox) })
}

/// The combo box's overrides of the container's calls (the C++ test's `ComboOverride`).
fn side(ct: isize, call: EeWonCtCall, a: isize, b: isize, c: isize) -> Option<isize> {
    type C = EeWonCtCall;
    type B = EeWonComboCall;
    if ct != 1 {
        return None;
    }
    let combo = combo_of()?;
    let bc = match call {
        C::SizeChanged => B::SizeChanged,
        C::MouseDown => B::MouseDown,
        C::MouseWheel => B::MouseWheel,
        C::GotFocus => B::GotFocus,
        C::LostFocus => B::LostFocus,
        C::KeyDown => B::KeyDown,
        C::AddedToParent => B::AddedToParent,
        _ => return None,
    };
    Some(combo.call(bc, a, b, c))
}

fn read(_me: isize, op: Op, _a: isize, _b: isize, _c: isize, _d: isize) -> Option<isize> {
    (op == Op::EventComponent).then(|| env_of(13) as isize)
}

fn line(me: isize, op: Op, a: isize, b: isize, _c: isize, _d: isize) -> Option<String> {
    (op == Op::FireEvent).then(|| {
        // SAFETY: the Rust fires an event code of the enum
        let e = unsafe { std::mem::transmute::<u32, EeWonGuiEvent>(a as u32) };
        format!("op {} fire {} {b} @{me}", op as u32, crate::guihost::ee_won_gui_event_type(e))
    })
}

fn combo_call_of(n: i64) -> EeWonComboCall {
    assert!((1..=EeWonComboCall::SetField as i64).contains(&n), "combo call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, EeWonComboCall>(n as u32) }
}

fn word(w: &str, n: &[i64]) -> bool {
    match w {
        "combo-new" => {
            let ct = crate::ctobj::tests::ct_ptr(1) as *const crate::ctobj::EeWonContainer;
            let c =
                Box::into_raw(Box::new(EeWonComboBox::new(std::ptr::dangling_mut::<c_void>(), Some(replay_host), ct)));
            COMBO.with(|t| t.set(c as usize));
        }
        "combo" => {
            let c = combo_of().expect("a combo box");
            let r = c.call(combo_call_of(n[0]), n[1] as isize, n[2] as isize, n[3] as isize);
            let want = format!("-> {r} 0");
            let got = next_line();
            if got != want {
                fail(format!("golden {got:?}, rust {want:?}"));
            }
        }
        "free-combo" => {
            let p = COMBO.with(|t| t.replace(0));
            // SAFETY: made by "combo-new"; taken out first
            unsafe { ee_won_combo_free(p as *mut EeWonComboBox) };
        }
        _ => return false,
    }
    true
}

fn cleanup() {
    let p = COMBO.with(|t| t.replace(0));
    if p != 0 {
        // SAFETY: made by "combo-new" and not freed: freed without its destructor's call
        drop(unsafe { Box::from_raw(p as *mut EeWonComboBox) });
    }
}

const EXT: Ext = Ext { side, read, line, word, cleanup };

#[test]
fn golden_sequences_replay() {
    let seqs = sequences_of(GOLDEN);
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (name, lines) in &seqs {
        if let Some(f) = replay_with(lines, Some(EXT)) {
            panic!("{name}: {f}");
        }
    }
}

#[test]
fn golden_replay_fails_on_a_changed_selection_range() {
    // the first selection of the whole text made a selection to the end
    let seqs = sequences_of(GOLDEN);
    let pat = format!("op {} 0 -1 ", Op::ComboEditSelect as u32);
    let (name, lines) =
        seqs.iter().find(|(_, l)| l.iter().any(|x| x.starts_with(&pat))).expect("a whole-text selection");
    let k = lines.iter().position(|x| x.starts_with(&pat)).unwrap();
    let mut changed = lines.clone();
    let moved = changed[k].replacen(&pat, &format!("op {} 1 0 ", Op::ComboEditSelect as u32), 1);
    changed[k] = Box::leak(moved.into_boxed_str());
    assert!(replay_with(&changed, Some(EXT)).is_some(), "{name} replayed with a changed range");
    assert!(replay_with(lines, Some(EXT)).is_none());
}
