//! The Rust RootContainer: its defaults, the background map's references and the invalid rectangle over a counting
//! host, and the replay of the original's golden (`golden_rootobj.txt`, printed by `tests/won/won_rootobj_test.cpp
//! --print` over the original RootContainer.cpp and the container goldens' world) through the container golden's
//! replay. The grabbers' reads are traced with their answers ("grab-read"); the cursors' components, the images' sizes
//! and the window's numbers are the world's (the C++ test's functions and env 14..18).

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

use super::*;
use crate::ctobj::tests::{ct_ptr, env_of, fail, next_line, nums, replay_host, replay_with, sequences_of, Ext};
use crate::ctobj::EeWonCtCall;

const GOLDEN: &str = include_str!("golden_rootobj.txt");

thread_local! {
    static LOG: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
    static GRAB: Cell<EeWonRootGrab> = Cell::new(EeWonRootGrab::default());
}

unsafe extern "C" fn counting_host(_c: *mut c_void, op: Op, a: isize, b: isize, _cc: isize, _d: isize) -> isize {
    match op {
        Op::RootGrabberRead => {
            // SAFETY: the Rust passes its grab state
            unsafe { *(b as *mut EeWonRootGrab) = GRAB.with(|g| g.get()) };
        }
        Op::RootImageHold | Op::RootImageRelease | Op::RootGrabberClear => {
            LOG.with(|h| h.borrow_mut().push((op as u32, a)));
        }
        _ => {}
    }
    0
}

fn grab(order: i32, image: isize) {
    GRAB.with(|g| g.set(EeWonRootGrab { need: 1, order, x: 1, y: 2, clip: EeWonRect4::default(), image }));
}

#[test]
fn the_constructor_state_and_the_background_references() {
    let r = EeWonRoot::new(std::ptr::dangling_mut::<c_void>(), Some(counting_host), std::ptr::null());
    type F = EeWonRootField;
    let f = |k: F| r.field(k as i32);
    assert_eq!([F::InvalidX, F::InvalidWidth, F::CursorComponent, F::Dragging, F::BgCount].map(f), [0; 5]);
    assert_eq!(f(F::AllowDrag), 1);
    let (hold, release, clear) = (Op::RootImageHold as u32, Op::RootImageRelease as u32, Op::RootGrabberClear as u32);
    // a grabber that needs no drawing is not read further
    GRAB.with(|g| g.set(EeWonRootGrab::default()));
    r.add_bg(0x9001);
    assert_eq!(LOG.with(|h| h.borrow().len()), 0);
    grab(3, 0xC001);
    r.add_bg(0x9001);
    // the temporary's hold, the entry's hold, the temporary's release, the grabber cleared
    assert_eq!(LOG.with(|h| h.take()), vec![(hold, 0xC001), (hold, 0xC001), (release, 0xC001), (clear, 0x9001)]);
    // the same image again under the same order: only the temporary's pair
    r.add_bg(0x9001);
    assert_eq!(LOG.with(|h| h.take()), vec![(hold, 0xC001), (release, 0xC001), (clear, 0x9001)]);
    // another image under the same order: the old one released by the entry's assignment
    grab(3, 0xC002);
    r.add_bg(0x9002);
    assert_eq!(
        LOG.with(|h| h.take()),
        vec![(hold, 0xC002), (release, 0xC001), (hold, 0xC002), (release, 0xC002), (clear, 0x9002)]
    );
    grab(1, 0xC003);
    r.add_bg(0x9003);
    LOG.with(|h| h.take());
    assert_eq!(f(F::BgCount), 2);
    // the destructor: the images in key order
    r.destroy();
    assert_eq!(LOG.with(|h| h.take()), vec![(release, 0xC003), (release, 0xC002)]);
    assert_eq!(f(F::BgCount), 0);
}

#[test]
fn the_invalid_rectangle_is_a_union() {
    let r = EeWonRoot::new(std::ptr::dangling_mut::<c_void>(), None, std::ptr::null());
    let q = EeWonRect4 { x: 5, y: 6, w: 10, h: 10 };
    r.call(EeWonRootCall::InvalidateRect, &q as *const EeWonRect4 as isize, 0, 0);
    let q = EeWonRect4 { x: 0, y: 20, w: 3, h: 4 };
    r.call(EeWonRootCall::InvalidateRect, &q as *const EeWonRect4 as isize, 0, 0);
    type F = EeWonRootField;
    let got = [F::InvalidX, F::InvalidY, F::InvalidWidth, F::InvalidHeight].map(|k| r.field(k as i32));
    assert_eq!(got, [0, 6, 15, 18]);
}

// ---- the golden's replay: component 1 the root over container 1

thread_local! {
    static ROOT: Cell<usize> = const { Cell::new(0) };
}

fn root_of() -> Option<&'static EeWonRoot> {
    let p = ROOT.with(|t| t.get());
    // SAFETY: the replay's root lives until its "free-root" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonRoot) })
}

/// The root's overrides of the container's calls (the C++ test's `RootOverride`).
fn side(ct: isize, call: EeWonCtCall, a: isize, b: isize, c: isize) -> Option<isize> {
    type C = EeWonCtCall;
    type R = EeWonRootCall;
    if ct != 1 {
        return None;
    }
    let root = root_of()?;
    let rc = match call {
        C::SizeChanged => R::SizeChanged,
        C::PrePaint => R::PrePaint,
        C::Paint => R::Paint,
        C::MouseExit => R::MouseExit,
        C::MouseDown => R::MouseDown,
        C::MouseUp => R::MouseUp,
        C::MouseMove => R::MouseMove,
        C::MouseDrag => R::MouseDrag,
        C::KeyChar => R::KeyChar,
        C::KeyDown => R::KeyDown,
        C::KeyUp => R::KeyUp,
        C::SetCursor => R::SetCursor,
        C::RemoveChild => R::RemoveChild,
        _ => return None,
    };
    Some(root.call(rc, a, b, c))
}

/// The C++ test's `CursorComponent`.
fn cursor_component(cursor: isize) -> (isize, i32, i32) {
    if cursor == 0 {
        return (0, 0, 0);
    }
    let k = (cursor & 0xF) as i32;
    let comp = match cursor {
        0x5002 | 0x5004 => 11,
        0x5003 => 12,
        _ => 0,
    };
    (comp, k * 2 - 3, 5 - k)
}

/// Writes two ints through the Rust's out pointers.
fn put2(a: isize, b: isize, x: i32, y: i32) {
    // SAFETY: the Rust passes two writable i32s
    unsafe {
        *(a as *mut i32) = x;
        *(b as *mut i32) = y;
    }
}

fn read(_me: isize, op: Op, a: isize, b: isize, c: isize, _d: isize) -> Option<isize> {
    match op {
        Op::RootCursorComponent => {
            let (comp, x, y) = cursor_component(a);
            put2(b, c, x, y);
            Some(comp)
        }
        Op::RootOneComponentWindow => Some(env_of(14) as isize),
        Op::RootWindowMouse => {
            put2(a, b, env_of(15) as i32, env_of(16) as i32);
            Some(0)
        }
        Op::RootWindowRect => {
            // SAFETY: the Rust passes its rectangle
            unsafe {
                *(a as *mut EeWonRect4) = EeWonRect4 { x: env_of(17) as i32, y: env_of(18) as i32, w: 640, h: 480 }
            };
            Some(0)
        }
        Op::RootMouseChildOf => {
            let p = if (1..=4).contains(&a) { ct_ptr(a as usize) } else { 0 };
            // SAFETY: a replay container lives until its "free" line
            Some(if p == 0 {
                0
            } else {
                unsafe { &*(p as *const EeWonContainer) }.field(EeWonCtField::MouseChild as i32)
            })
        }
        Op::RootScheme => Some(0xA100),
        Op::RootImageSize => {
            let k = (a & 0xF) as i32;
            put2(b, c, 8 + k * 11, 6 + k * 7);
            Some(0)
        }
        Op::RootGrabberRead => {
            let l = next_line();
            let n = nums(l);
            if !l.starts_with("grab-read ") || n.len() != 10 || n[0] != a as i64 {
                fail(format!("golden {l:?} where grabber {a} is read"));
                return Some(0);
            }
            let q = EeWonRootGrab {
                need: n[1] as i32,
                order: n[2] as i32,
                x: n[3] as i32,
                y: n[4] as i32,
                image: n[5] as isize,
                clip: EeWonRect4 { x: n[6] as i32, y: n[7] as i32, w: n[8] as i32, h: n[9] as i32 },
            };
            // SAFETY: the Rust passes its grab state
            unsafe { *(b as *mut EeWonRootGrab) = q };
            Some(0)
        }
        _ => None,
    }
}

fn line(me: isize, op: Op, a: isize, b: isize, c: isize, _d: isize) -> Option<String> {
    (op == Op::RootDrawImage).then(|| {
        // SAFETY: the Rust passes six ints
        let q = unsafe { std::slice::from_raw_parts(c as *const i32, 6) };
        format!("op {} draw {a} {b} {} {} {} {} {} {} @{me}", op as u32, q[0], q[1], q[2], q[3], q[4], q[5])
    })
}

fn root_call_of(n: i64) -> EeWonRootCall {
    assert!((1..=EeWonRootCall::GetField as i64).contains(&n), "root call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, EeWonRootCall>(n as u32) }
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
        "grab" => {} // the driver's change of a grabber: its reads are traced
        "root-new" => {
            let ct = ct_ptr(1) as *const EeWonContainer;
            let r = Box::into_raw(Box::new(EeWonRoot::new(std::ptr::dangling_mut::<c_void>(), Some(replay_host), ct)));
            ROOT.with(|t| t.set(r as usize));
        }
        "root" => {
            let r = root_of().expect("a root").call(root_call_of(n[0]), n[1] as isize, n[2] as isize, n[3] as isize);
            check_result(r);
        }
        "root-rect" => {
            let q = EeWonRect4 { x: n[0] as i32, y: n[1] as i32, w: n[2] as i32, h: n[3] as i32 };
            let r =
                root_of().expect("a root").call(EeWonRootCall::InvalidateRect, &q as *const EeWonRect4 as isize, 0, 0);
            check_result(r);
        }
        "free-root" => {
            let p = ROOT.with(|t| t.replace(0));
            // SAFETY: made by "root-new"; taken out first
            unsafe { ee_won_root_free(p as *mut EeWonRoot) };
        }
        _ => return false,
    }
    true
}

fn cleanup() {
    let p = ROOT.with(|t| t.replace(0));
    if p != 0 {
        // SAFETY: made by "root-new" and not freed: freed without its destructor's calls
        drop(unsafe { Box::from_raw(p as *mut EeWonRoot) });
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
fn golden_replay_fails_on_a_changed_window_place() {
    // the first window drag's place moved by one
    let seqs = sequences_of(GOLDEN);
    let pat = format!("op {} ", Op::RootWindowSetPos as u32);
    let (name, lines) = seqs.iter().find(|(_, l)| l.iter().any(|x| x.starts_with(&pat))).expect("a window drag");
    let k = lines.iter().position(|x| x.starts_with(&pat)).unwrap();
    let n = nums(lines[k]);
    let mut changed = lines.clone();
    let moved = format!("{pat}{} {} 0 0 @1", n[0] + 1, n[1]);
    changed[k] = Box::leak(moved.into_boxed_str());
    assert!(replay_with(&changed, Some(EXT)).is_some(), "{name} replayed with a changed place");
    assert!(replay_with(lines, Some(EXT)).is_none());
}
