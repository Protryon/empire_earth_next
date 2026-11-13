//! The Rust Button: its defaults, the flags' radio focus, the default buttons' references and repaints over a
//! counting host, and the replay of the original's golden (`golden_btnobj.txt`, printed by
//! `tests/won/won_btnobj_test.cpp --print` over the original Button.cpp and the container goldens' world) through the
//! container golden's replay. The buttons are components 4..13; the click sounds are the golden's "sound" lines.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::sync::Mutex;

use super::*;
use crate::ctobj::tests::{fail, next_line, replay_host, replay_with, sequences_of, Ext};
use crate::ctobj::EeWonCtCall;

const GOLDEN: &str = include_str!("golden_btnobj.txt");

/// The default buttons are the process's: the tests that use them run one at a time.
static SERIAL: Mutex<()> = Mutex::new(());

fn serial() -> std::sync::MutexGuard<'static, ()> {
    SERIAL.lock().unwrap_or_else(|e| e.into_inner())
}

thread_local! {
    static LOG: RefCell<Vec<(u32, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn counting_host(c: *mut c_void, op: Op, a: isize, b: isize, cc: isize, _d: isize) -> isize {
    LOG.with(|h| h.borrow_mut().push((op as u32, c as isize, a, b)));
    let _ = cc;
    match op {
        Op::BtnHandleActivate => 1,
        _ => 0,
    }
}

fn log() -> Vec<(u32, isize, isize, isize)> {
    LOG.with(|h| h.take())
}

#[test]
fn the_constructor_state_and_the_radio_focus() {
    let _s = serial();
    let me = 7usize as *mut c_void;
    let b = EeWonButton::new(me, Some(counting_host));
    type F = EeWonBtnField;
    let f = |k: F| b.field(k as i32);
    assert_eq!([F::Flags, F::KeyDown, F::DelayAcc, F::MouseDownState].map(f), [0; 4]);
    // a plain button's flags change nothing else
    b.set_button_flags(0x41, true);
    assert_eq!(f(F::Flags), 0x41);
    assert!(log().is_empty());
    // a radio's focus follows its check
    b.set_button_flags(0x8, true);
    assert_eq!(log(), vec![(Op::ChildSetFlags as u32, 7, 7, 0x8)]);
    b.set_button_flags(0x41, false);
    assert_eq!(f(F::Flags), 0x8);
    LOG.with(|h| h.borrow_mut().clear());
    // the mouse's press state: down over, then up (no press without the pressed flag stays ignored)
    b.call(EeWonBtnCall::MouseDown, 0, 0, 1);
    assert_eq!(f(F::MouseDownState), 0);
    log();
}

#[test]
fn the_default_buttons_hold_and_repaint() {
    let _s = serial();
    forget_defaults();
    log();
    let (hold, release, inv) = (Op::ChildHold as u32, Op::ChildRelease as u32, Op::ChildInvalidate as u32);
    let h = Some(counting_host as unsafe extern "C" fn(*mut c_void, Op, isize, isize, isize, isize) -> isize);
    type S = EeWonBtnStatic;
    static_call(h, S::SetDefault, 4);
    assert_eq!(log(), vec![(hold, 0, 4, 0), (inv, 0, 4, 0)]);
    // the same button again: nothing
    static_call(h, S::SetDefault, 4);
    assert!(log().is_empty());
    // the dialog's default: held, then the current default replaced (the old one repainted and released first)
    static_call(h, S::SetDialogDefault, 5);
    assert_eq!(log(), vec![(hold, 0, 5, 0), (inv, 0, 4, 0), (release, 0, 4, 0), (hold, 0, 5, 0), (inv, 0, 5, 0)]);
    assert_eq!((static_call(h, S::Default, 0), static_call(h, S::DialogDefault, 0)), (5, 5));
    static_call(h, S::SetDefault, 6);
    log();
    // UnSetDefaultButton ignores its argument: back to the dialog's default
    static_call(h, S::UnSetDefault, 9);
    assert_eq!(static_call(h, S::Default, 0), 5);
    log();
    // another button than the dialog's default: nothing
    static_call(h, S::UnSetDialogDefault, 6);
    assert!(log().is_empty());
    static_call(h, S::UnSetDialogDefault, 5);
    assert_eq!(log(), vec![(release, 0, 5, 0), (inv, 0, 5, 0), (release, 0, 5, 0)]);
    assert_eq!((static_call(h, S::Default, 0), static_call(h, S::DialogDefault, 0)), (0, 0));
}

#[test]
fn a_click_toggles_the_check_and_fires_the_event() {
    let _s = serial();
    let b = EeWonButton::new(4usize as *mut c_void, Some(counting_host));
    log();
    b.click();
    assert_eq!(b.field(EeWonBtnField::Flags as i32), 0x4);
    let ops: Vec<u32> = log().iter().map(|x| x.0).collect();
    assert_eq!(ops, vec![Op::CtBits as u32, Op::Invalidate as u32, Op::BtnHandleActivate as u32, Op::FireEvent as u32]);
    // no activation: nothing
    b.set_button_flags(0x200, true);
    log();
    b.click();
    assert_eq!(log().len(), 1);
    assert_eq!(b.field(EeWonBtnField::Flags as i32), 0x204);
}

// ---- the golden's replay: the buttons 4..13 over the containers 1..3

const FIRST: usize = 4;
const LAST: usize = 13;

thread_local! {
    static BTNS: RefCell<[usize; LAST + 1]> = const { RefCell::new([0; LAST + 1]) };
    static SOUND: RefCell<[bool; LAST + 1]> = const { RefCell::new([false; LAST + 1]) };
    static FREED: Cell<bool> = const { Cell::new(false) };
}

fn btn_ptr(id: isize) -> usize {
    if !(FIRST as isize..=LAST as isize).contains(&id) {
        return 0;
    }
    BTNS.with(|t| t.borrow()[id as usize])
}

fn btn_of(id: isize) -> Option<&'static EeWonButton> {
    let p = btn_ptr(id);
    // SAFETY: a replay button lives until its "free-btn" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonButton) })
}

fn btn_call_of(n: i64) -> EeWonBtnCall {
    assert!((1..=EeWonBtnCall::SetField as i64).contains(&n), "button call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, EeWonBtnCall>(n as u32) }
}

fn static_of(n: i64) -> EeWonBtnStatic {
    assert!((1..=EeWonBtnStatic::DialogDefault as i64).contains(&n), "static call {n}");
    // SAFETY: as above
    unsafe { std::mem::transmute::<u32, EeWonBtnStatic>(n as u32) }
}

/// The button's overrides of the container calls the world makes (the C++ test's `ButtonOverride`).
fn side(ct: isize, call: EeWonCtCall, a: isize, b: isize, c: isize) -> Option<isize> {
    type C = EeWonCtCall;
    type B = EeWonBtnCall;
    let btn = btn_of(ct)?;
    let bc = match call {
        C::MouseEnter => B::MouseEnter,
        C::MouseExit => B::MouseExit,
        C::MouseDown => B::MouseDown,
        C::MouseUp => B::MouseUp,
        C::MouseDrag => B::MouseDrag,
        C::MouseHover => B::MouseHover,
        C::GotFocus => B::GotFocus,
        C::LostFocus => B::LostFocus,
        C::KeyDown => B::KeyDown,
        C::KeyUp => B::KeyUp,
        C::TimerEvent => B::TimerEvent,
        C::AddedToParent => B::AddedToParent,
        _ => return None,
    };
    Some(btn.call(bc, a, b, c))
}

fn read(me: isize, op: Op, a: isize, _b: isize, _c: isize, _d: isize) -> Option<isize> {
    match op {
        Op::BtnOf => Some(btn_ptr(a) as isize),
        Op::BtnHasClickSound => {
            Some(SOUND.with(|s| (FIRST as isize..=LAST as isize).contains(&me) && s.borrow()[me as usize]) as isize)
        }
        _ => None,
    }
}

fn line(me: isize, op: Op, a: isize, b: isize, _c: isize, _d: isize) -> Option<String> {
    (op == Op::FireEvent).then(|| {
        // SAFETY: the Rust fires an event code of the enum
        let e = unsafe { std::mem::transmute::<u32, EeWonGuiEvent>(a as u32) };
        format!("op {} fire {} {b} @{me}", op as u32, crate::guihost::ee_won_gui_event_type(e))
    })
}

fn check_result(r: isize) {
    let want = format!("-> {r} 0");
    let got = next_line();
    if got != want {
        fail(format!("golden {got:?}, rust {want:?}"));
    }
}

fn word(w: &str, n: &[i64]) -> bool {
    match w {
        "btn-new" => {
            let id = n[0] as usize;
            let b = Box::into_raw(Box::new(EeWonButton::new(id as *mut c_void, Some(replay_host))));
            BTNS.with(|t| t.borrow_mut()[id] = b as usize);
        }
        "sound" => SOUND.with(|s| s.borrow_mut()[n[0] as usize] = n[1] != 0),
        "btn" => {
            let call = btn_call_of(n[1]);
            let a = if call == EeWonBtnCall::CopyAttributes { btn_ptr(n[2] as isize) as isize } else { n[2] as isize };
            let r = btn_of(n[0] as isize).expect("a button").call(call, a, n[3] as isize, n[4] as isize);
            check_result(r);
        }
        "bstatic" => {
            let r = static_call(Some(replay_host), static_of(n[0]), n[1] as isize);
            check_result(r);
        }
        "free-btn" => {
            let p = BTNS.with(|t| std::mem::take(&mut t.borrow_mut()[n[0] as usize]));
            // SAFETY: made by "btn-new"; taken out first
            unsafe { ee_won_btn_free(p as *mut EeWonButton) };
        }
        _ => return false,
    }
    true
}

fn cleanup() {
    let all = BTNS.with(|t| std::mem::take(&mut *t.borrow_mut()));
    for p in all {
        // SAFETY: made by "btn-new" and not freed
        unsafe { ee_won_btn_free(p as *mut EeWonButton) };
    }
    forget_defaults();
    FREED.with(|f| f.set(true));
}

const EXT: Ext = Ext { side, read, line, word, cleanup };

#[test]
fn golden_sequences_replay() {
    let _s = serial();
    forget_defaults();
    let seqs = sequences_of(GOLDEN);
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (name, lines) in &seqs {
        if let Some(f) = replay_with(lines, Some(EXT)) {
            panic!("{name}: {f}");
        }
    }
    assert!(FREED.with(|f| f.get()));
}

#[test]
fn golden_replay_fails_on_a_changed_event() {
    let _s = serial();
    forget_defaults();
    // the first button event fired as another type
    let seqs = sequences_of(GOLDEN);
    let pat = format!("op {} fire 15 ", Op::FireEvent as u32);
    let (name, lines) = seqs.iter().find(|(_, l)| l.iter().any(|x| x.starts_with(&pat))).expect("a button event");
    let k = lines.iter().position(|x| x.starts_with(&pat)).unwrap();
    let mut changed = lines.clone();
    let other = changed[k].replacen(" fire 15 ", " fire 16 ", 1);
    changed[k] = Box::leak(other.into_boxed_str());
    assert!(replay_with(&changed, Some(EXT)).is_some(), "{name} replayed with a changed event");
    assert!(replay_with(lines, Some(EXT)).is_none());
}
