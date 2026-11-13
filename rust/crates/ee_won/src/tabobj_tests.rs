//! The Rust TabBar and TabCtrl: their defaults, fields and copied attributes over a counting host, and the replay of
//! the original's golden (`golden_tabobj.txt`, printed by `tests/won/won_tabobj_test.cpp --print` over the original
//! TabCtrl.cpp and the container goldens' world) through the container golden's replay.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

use super::*;
use crate::ctobj::tests::{env_of, fail, next_line, replay_host, replay_with, sequences_of, world_of, Ext};
use crate::ctobj::EeWonCtCall;

const GOLDEN: &str = include_str!("golden_tabobj.txt");

thread_local! {
    static HELD: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn counting_host(c: *mut c_void, op: Op, a: isize, _b: isize, _cc: isize, _d: isize) -> isize {
    // the control's own calls (component 2), not its container's (component 3)
    if c as isize == 2 && (op == Op::ChildHold || op == Op::ChildRelease) {
        HELD.with(|h| h.borrow_mut().push((op as u32, a)));
    }
    0
}

fn dangling() -> *mut c_void {
    std::ptr::dangling_mut::<c_void>()
}

#[test]
fn the_constructor_state_the_fields_and_copied_attributes() {
    let bar = EeWonTabBar::new(dangling(), Some(counting_host), std::ptr::null(), std::ptr::null());
    let other_bar = EeWonTabBar::new(dangling(), Some(counting_host), std::ptr::null(), std::ptr::null());
    let ct = crate::ctobj::EeWonContainer::new(3 as *mut c_void, Some(counting_host), false);
    let t = EeWonTabCtrl::new(2 as *mut c_void, Some(counting_host), &ct);
    let u = EeWonTabCtrl::new(dangling(), Some(counting_host), std::ptr::null());
    t.st.borrow_mut().bar = &bar;
    u.st.borrow_mut().bar = &other_bar;
    type F = EeWonTabField;
    let f = |c: &EeWonTabCtrl, k: F| c.field(k as i32);
    assert_eq!(f(&t, F::CurTabId), -1);
    assert_eq!(f(&t, F::TabBarHeight), 0);
    assert_eq!(f(&t, F::TabOverlap), 4);
    assert_eq!(f(&t, F::TabBarOverlap), 1);
    assert_eq!(f(&t, F::BottomTabTopOffset), 3);
    assert_eq!(f(&t, F::BottomTabLeftOffset), 2);
    assert_eq!(f(&t, F::UserHasInteracted), 0);
    assert_eq!(t.st.borrow().pads, [5; 4]);
    for (k, v) in [(F::TabBarHeight, 22), (F::TabOverlap, 6), (F::TabBarOverlap, 2), (F::BottomTabTopOffset, 7)] {
        u.set_field(k as i32, v);
    }
    u.set_field(F::BottomTabLeftOffset as i32, 9);
    u.call(EeWonTabCall::SetArrowsOnLeft, 1, 0);
    u.call(EeWonTabCall::SetTabBarOnRight, 1, 0);
    u.st.borrow_mut().on_bottom = true;
    u.st.borrow_mut().pads = [1, 2, 3, 4];
    t.call(EeWonTabCall::CopyAttributes, &u as *const EeWonTabCtrl as isize, 0);
    assert_eq!(
        [F::TabBarHeight, F::TabOverlap, F::TabBarOverlap, F::BottomTabTopOffset, F::BottomTabLeftOffset]
            .map(|k| f(&t, k)),
        [22, 6, 2, 7, 9]
    );
    {
        let s = t.st.borrow();
        assert!(s.arrows_on_left && s.on_right && s.on_bottom);
        // the padding and the selection are not copied
        assert_eq!(s.pads, [5; 4]);
    }
    // a control copying itself
    t.copy_attributes(&t);
    assert_eq!(f(&t, F::TabBarHeight), 22);
    assert_eq!(t.tab_id_for_container(5), -1);
    t.st.borrow_mut().pages = vec![5, 0, 6];
    assert_eq!((t.tab_id_for_container(6), t.tab_id_for_container(0)), (2, 1));
    // the arrows: the old one released before the new one is held, nothing for the same one, the right one first
    t.set_tab_arrows(7, 8);
    t.set_tab_arrows(7, 9);
    t.destroy();
    let (hold, release) = (Op::ChildHold as u32, Op::ChildRelease as u32);
    assert_eq!(
        HELD.with(|h| h.borrow().clone()),
        vec![(hold, 7), (hold, 8), (release, 8), (hold, 9), (release, 9), (release, 7)]
    );
    assert!(bar.ctrl.get().is_null());
}

// ---- the golden's replay: component 1 the control over container 1, 2 its bottom bar over container 2

thread_local! {
    static TAB: Cell<usize> = const { Cell::new(0) };
    static BAR: Cell<usize> = const { Cell::new(0) };
}

fn tab_of() -> Option<&'static EeWonTabCtrl> {
    let p = TAB.with(|t| t.get());
    // SAFETY: the replay's control lives until its "free-tab" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonTabCtrl) })
}

fn bar_of() -> Option<&'static EeWonTabBar> {
    let p = BAR.with(|t| t.get());
    // SAFETY: the replay's bar lives until its "free-bar" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonTabBar) })
}

fn ct_of(id: usize) -> *const crate::ctobj::EeWonContainer {
    crate::ctobj::tests::ct_ptr(id) as *const crate::ctobj::EeWonContainer
}

/// The control's overrides of the container's calls (the C++ test's `TabOverride`).
fn side(ct: isize, call: EeWonCtCall, a: isize, b: isize, _c: isize) -> Option<isize> {
    type C = EeWonCtCall;
    if ct != 1 {
        return None;
    }
    let t = tab_of()?;
    let tc = match call {
        C::SizeChanged => EeWonTabCall::SizeChanged,
        C::DesiredSize => EeWonTabCall::DesiredSize,
        C::KeyDown => EeWonTabCall::KeyDown,
        C::AddedToParent => EeWonTabCall::AddedToParent,
        C::TabFocus => EeWonTabCall::TabFocus,
        _ => return None,
    };
    Some(t.call(tc, a, b))
}

fn read(_me: isize, op: Op, a: isize, b: isize, _c: isize, _d: isize) -> Option<isize> {
    Some(match op {
        Op::ChildRight => {
            let w = world_of(a);
            w.x.wrapping_add(w.w) as isize
        }
        Op::EventType => env_of(10) as isize,
        Op::EventControlId => env_of(12) as isize,
        Op::EventComponent => env_of(13) as isize,
        Op::TabTextIs => (a % 3 == b) as isize,
        _ => return None,
    })
}

fn line(me: isize, op: Op, a: isize, b: isize, _c: isize, _d: isize) -> Option<String> {
    match op {
        Op::FireEvent => {
            // SAFETY: the Rust fires an event code of the enum
            let e = unsafe { std::mem::transmute::<u32, EeWonGuiEvent>(a as u32) };
            Some(format!("op {} fire {} {b} @{me}", op as u32, crate::guihost::ee_won_gui_event_type(e)))
        }
        Op::ChildSetPosSize => {
            // SAFETY: the Rust passes its rectangle
            let q = unsafe { &*(b as *const EeWonRect4) };
            Some(format!("op {} {a} posrect {} {} {} {} @{me}", op as u32, q.x, q.y, q.w, q.h))
        }
        _ => None,
    }
}

fn check_result(r1: isize) {
    let want = format!("-> {r1} 0");
    let got = next_line();
    if got != want {
        fail(format!("golden {got:?}, rust {want:?}"));
    }
}

fn tab_call_of(n: i64) -> EeWonTabCall {
    assert!((1..=EeWonTabCall::SetField as i64).contains(&n), "tab call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, EeWonTabCall>(n as u32) }
}

fn word(w: &str, n: &[i64]) -> bool {
    match w {
        "tabctrl" => {
            let t = Box::into_raw(Box::new(EeWonTabCtrl::new(dangling(), Some(replay_host), ct_of(1))));
            TAB.with(|c| c.set(t as usize));
        }
        "tabbar" => {
            let ctrl = TAB.with(|c| c.get()) as *const EeWonTabCtrl;
            let b = Box::into_raw(Box::new(EeWonTabBar::new(2 as *mut c_void, Some(replay_host), ct_of(2), ctrl)));
            BAR.with(|c| c.set(b as usize));
        }
        "tabinit" => {
            let parts = EeWonTabParts {
                bar: BAR.with(|c| c.get()) as *const EeWonTabBar,
                bottom_bar: 2,
                top_bar: 3,
                bottom_bar_container: 4,
                top_bar_container: 5,
                layout: 7,
            };
            tab_of().expect("a control").init(&parts);
        }
        "tab" => {
            let t = tab_of().expect("a control");
            let call = tab_call_of(n[0]);
            let r1 = if call == EeWonTabCall::DesiredSize {
                let (mut w, mut h) = (-1i32, -1i32);
                t.call(call, &mut w as *mut i32 as isize, &mut h as *mut i32 as isize);
                let want = format!("-> {w} {h}");
                let got = next_line();
                if got != want {
                    fail(format!("golden {got:?}, rust {want:?}"));
                }
                return true;
            } else {
                t.call(call, n[1] as isize, n[2] as isize)
            };
            check_result(r1);
        }
        "bar" => {
            let b = bar_of().expect("a bar");
            check_result(b.call(EeWonTabBarCall::HandleComponentEvent, n[1] as isize));
        }
        "free-tab" => {
            let p = TAB.with(|c| c.replace(0));
            // SAFETY: made by "tabctrl"; taken out first
            unsafe { ee_won_tab_free(p as *mut EeWonTabCtrl) };
        }
        "free-bar" => {
            let p = BAR.with(|c| c.replace(0));
            // SAFETY: made by "tabbar"; taken out first
            unsafe { ee_won_tabbar_free(p as *mut EeWonTabBar) };
        }
        _ => return false,
    }
    true
}

fn cleanup() {
    let (t, b) = (TAB.with(|c| c.replace(0)), BAR.with(|c| c.replace(0)));
    if t != 0 {
        // SAFETY: made by "tabctrl" and not freed: freed without its destructor's calls
        drop(unsafe { Box::from_raw(t as *mut EeWonTabCtrl) });
    }
    if b != 0 {
        // SAFETY: made by "tabbar" and not freed
        drop(unsafe { Box::from_raw(b as *mut EeWonTabBar) });
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
fn golden_replay_fails_on_a_changed_scroll() {
    // the first bar scroll's x moved by one
    let seqs = sequences_of(GOLDEN);
    let pat = format!("op {} 2 ", Op::ChildSetPos as u32);
    let (name, lines) = seqs.iter().find(|(_, l)| l.iter().any(|x| x.starts_with(&pat))).expect("a bar scroll");
    let k = lines.iter().position(|x| x.starts_with(&pat)).unwrap();
    let mut changed = lines.clone();
    let rest = &changed[k][pat.len()..];
    let (x, tail) = rest.split_once(' ').unwrap();
    let moved = format!("{pat}{} {tail}", x.parse::<i64>().unwrap() + 1);
    changed[k] = Box::leak(moved.into_boxed_str());
    assert!(replay_with(&changed, Some(EXT)).is_some(), "{name} replayed with a moved bar");
    assert!(replay_with(lines, Some(EXT)).is_none());
}
