//! The Rust Container: unit tests over a small mock world, and the replay of the original's golden
//! (`golden_ctobj.txt`, printed by `tests/won/won_ctobj_test.cpp --print` over the original Container units and a
//! scripted world).

use std::cell::RefCell;
use std::ffi::c_void;

use super::*;

// ---- a mock world: components 1..=8 with bounds, flags, parents, timer flags and references; every op logged

#[derive(Default, Clone)]
struct Comp {
    bounds: [i32; 4],
    bits: isize,
    parent: isize,
    timer: isize,
    focus_id: isize,
    refs: i32,
}

#[derive(Default)]
struct Mock {
    comps: Vec<Comp>,
    log: Vec<String>,
}

thread_local! {
    static MOCK: RefCell<Mock> = RefCell::new(Mock::default());
}

unsafe extern "C" fn mock_host(comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let me = comp as isize;
    MOCK.with(|m| {
        let mut m = m.borrow_mut();
        let w = |m: &Mock, i: isize| m.comps[i as usize].clone();
        match op {
            Op::CtBits => return w(&m, a).bits,
            Op::CtParentOf => return w(&m, a).parent,
            Op::CtTimerFlags => return w(&m, a).timer,
            Op::CtFocusId => return w(&m, a).focus_id,
            Op::ChildLeft => return w(&m, a).bounds[0] as isize,
            Op::ChildTop => return w(&m, a).bounds[1] as isize,
            Op::ChildWidth => return w(&m, a).bounds[2] as isize,
            Op::ChildHeight => return w(&m, a).bounds[3] as isize,
            Op::CtContains => {
                let r = w(&m, a).bounds;
                let (x, y) = (b as i32, c as i32);
                return (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) as isize;
            }
            Op::CtSetParent => m.comps[a as usize].parent = if b != 0 { me } else { 0 },
            Op::CtSetFocusId => m.comps[a as usize].focus_id = b,
            Op::CtSetTimerFlags => m.comps[me as usize].timer = a,
            Op::ChildHold => m.comps[a as usize].refs += 1,
            Op::ChildRelease => m.comps[a as usize].refs -= 1,
            Op::CtChild if b == V::HitTest as isize => return 1,
            Op::CtChild if b == V::TimerEvent as isize => return a % 2,
            Op::CtVirt if a == V::FindChildAt as isize => return 0,
            _ => {}
        }
        m.log.push(format!("{} {a} {b} {c} {d}", op as u32));
        0
    })
}

fn world(n: usize) {
    MOCK.with(|m| {
        let mut m = m.borrow_mut();
        m.comps = vec![Comp::default(); n + 1];
        for i in 1..=n {
            m.comps[i].bounds = [i as i32 * 10, 0, 10, 10];
        }
        m.log.clear();
    });
}

fn set_bits(i: usize, bits: isize) {
    MOCK.with(|m| m.borrow_mut().comps[i].bits = bits);
}

fn refs(i: usize) -> i32 {
    MOCK.with(|m| m.borrow().comps[i].refs)
}

fn ct() -> EeWonContainer {
    EeWonContainer::new(std::ptr::dangling_mut::<c_void>(), Some(mock_host), false)
}

#[test]
fn children_keep_their_order_under_the_always_on_top_ones() {
    world(6);
    set_bits(3, FLAG_ALWAYS_ON_TOP);
    let t = ct();
    t.add_child(2);
    t.add_child(3);
    t.add_child(4);
    t.add_child_ex(5, 0);
    assert_eq!(t.children(), vec![5, 2, 4, 3]);
    assert_eq!(t.next_child(4, true, false), 3);
    assert_eq!(t.next_child(3, true, false), 0);
    assert_eq!(t.next_child(3, true, true), 5);
    assert_eq!(t.next_child(5, false, true), 3);
    assert_eq!(t.next_child(6, true, true), 0);
    t.bring_to_top(5);
    assert_eq!(t.children(), vec![2, 4, 5, 3]);
    t.ensure_on_top_of(5, 2);
    assert_eq!(t.children(), vec![5, 2, 4, 3]);
    t.ensure_on_top_of(5, 2);
    assert_eq!(t.children(), vec![5, 2, 4, 3]);
    assert_eq!(refs(5), 1);
    t.remove_child(4);
    assert_eq!(t.children(), vec![5, 2, 3]);
    assert_eq!(refs(4), 0);
    t.destroy();
    for i in 2..=6 {
        assert_eq!(refs(i), 0, "component {i}");
    }
}

#[test]
fn focus_ids_skip_zero_and_the_last_focus_child_wins() {
    world(4);
    let t = ct();
    t.st.borrow_mut().next_focus_id = 254;
    assert_eq!(t.next_focus_id(), 255);
    assert_eq!(t.next_focus_id(), 1);
    t.destroy();
}

#[test]
fn timer_children_go_when_they_stop_wanting_ticks() {
    world(6);
    let t = ct();
    for i in [2, 3, 4] {
        t.add_child(i);
        MOCK.with(|m| m.borrow_mut().comps[i as usize].timer = 2);
        t.add_timer_child(i);
    }
    assert_eq!(MOCK.with(|m| m.borrow().comps[1].timer), 1);
    // 3 answers its tick (odd), 2 and 4 do not: they are dropped and asked to stop
    assert!(t.timer_event(5));
    assert_eq!(t.st.borrow().timers.iter().copied().collect::<Vec<_>>(), vec![3]);
    assert_eq!(refs(2), 1);
    MOCK.with(|m| m.borrow_mut().comps[3].timer = 0);
    assert!(!t.timer_event(5));
    assert!(t.st.borrow().timers.is_empty());
    t.destroy();
    for i in 2..=4 {
        assert_eq!(refs(i), 0);
    }
}

#[test]
fn the_desired_size_encloses_the_children() {
    world(4);
    let t = ct();
    t.add_child(2);
    t.add_child(3);
    assert_eq!(t.desired_size(), (40, 10));
    t.destroy();
}

#[test]
fn the_list_survives_removals_around_an_iteration() {
    let mut l = List::new();
    let a = l.insert(HEAD, 10);
    let b = l.insert(HEAD, 20);
    let c = l.insert(HEAD, 30);
    assert_eq!(l.all(), vec![10, 20, 30]);
    l.erase(b);
    // an erased node keeps its links: an iteration standing on it goes on to the next one
    assert_eq!(l.next(b), c);
    assert_eq!(l.all(), vec![10, 30]);
    let d = l.insert(c, 25);
    assert_eq!(d, b, "a freed node is reused");
    assert_eq!(l.all(), vec![10, 25, 30]);
    assert_eq!((l.front(), l.back(), l.len), (10, 30, 3));
    assert_eq!(l.find(30), Some(c));
    assert_eq!(l.find(99), None);
    assert_eq!(l.prev(a), HEAD);
}

// ---- the golden's replay: the C++ test's world (won_ct_world.h; the pure reads answered from it), its lines driving
// the calls ("call", "do", and the dialog golden's "dlg") and the world's changes ("set", "env"); every op the Rust
// makes must be the golden's next "op" line, answered by the golden's "=" line. The dialog golden
// (`golden_dialogobj.txt`) replays here too: its "dialog" line makes a Rust Dialog over container 1, which then takes
// that container's calls the Dialog overrides.

use crate::dialogobj::{EeWonDialog, EeWonDlgCall as D};

const GOLDEN: &str = include_str!("golden_ctobj.txt");
/// the most containers a golden's world has (the tab control's: six)
const CTS: usize = 7;
/// the most components a golden's world has (the tab control's: nineteen)
const COMPS: usize = 24;

#[derive(Default, Clone, Copy)]
pub(crate) struct W {
    pub(crate) x: i32,
    pub(crate) y: i32,
    pub(crate) w: i32,
    pub(crate) h: i32,
    pub(crate) bits: u32,
    pub(crate) parent: isize,
    pub(crate) timer: i32,
    pub(crate) invalid: i32,
    pub(crate) focus_id: i32,
    pub(crate) control_id: i32,
}

#[derive(Default)]
struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    w: [W; COMPS],
    /// 0..6 the world's environment; 8 the window's modal component, 9 the dialog default button, 10..12 the
    /// event's type, button flags and control id (the dialog golden), 13 the event's component (the tab golden)
    env: [i64; 20],
    scheme: isize,
    cts: [usize; CTS + 1],
    dlg: usize,
    failure: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
    static EXT: std::cell::Cell<Option<Ext>> = const { std::cell::Cell::new(None) };
}

/// A golden's own parts of the replay (the tab control's): its objects' overrides of the containers' calls
/// (`side`), its pure reads (`read`), the op lines it prints itself (`line`), its top-level lines (`word`: true when
/// it took the line) and the freeing of what a failed replay left (`cleanup`).
#[derive(Clone, Copy)]
pub(crate) struct Ext {
    pub side: fn(isize, EeWonCtCall, isize, isize, isize) -> Option<isize>,
    pub read: fn(isize, Op, isize, isize, isize, isize) -> Option<isize>,
    pub line: fn(isize, Op, isize, isize, isize, isize) -> Option<String>,
    pub word: fn(&str, &[i64]) -> bool,
    pub cleanup: fn(),
}

/// The world's component `i` (a golden's own reads).
pub(crate) fn world_of(i: isize) -> W {
    with(|r| r.w[i as usize])
}

/// The environment's value `k`.
pub(crate) fn env_of(k: usize) -> i64 {
    with(|r| r.env[k])
}

/// The replay's container `id`'s Rust object (0 when there is none).
pub(crate) fn ct_ptr(id: usize) -> usize {
    with(|r| r.cts[id])
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

pub(crate) fn fail(msg: String) {
    with(|r| {
        if r.failure.is_none() {
            r.failure = Some(msg);
        }
    });
}

pub(crate) fn next_line() -> &'static str {
    with(|r| {
        let l = r.lines.get(r.at).copied().unwrap_or("<end>");
        r.at += 1;
        l
    })
}

pub(crate) fn nums(l: &str) -> Vec<i64> {
    l.split_whitespace().skip(1).filter_map(|w| w.trim_start_matches('@').parse().ok()).collect()
}

fn ct_of(id: isize) -> Option<&'static EeWonContainer> {
    let p = with(|r| if (1..=CTS as isize).contains(&id) { r.cts[id as usize] } else { 0 });
    // SAFETY: a replay container lives until its "free" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonContainer) })
}

fn apply_set(n: &[i64]) {
    with(|r| {
        let w = &mut r.w[n[0] as usize];
        let v = n[2];
        match n[1] {
            0 => w.bits = v as u32,
            1 => w.invalid = v as i32,
            2 => w.timer = v as i32,
            3 => w.x = v as i32,
            4 => w.y = v as i32,
            5 => w.w = v as i32,
            6 => w.h = v as i32,
            7 => w.control_id = v as i32,
            _ => w.parent = v as isize,
        }
    });
}

fn dlg_of() -> Option<&'static EeWonDialog> {
    let p = with(|r| r.dlg);
    // SAFETY: the replay's dialog lives until its "free-dialog" line
    (p != 0).then(|| unsafe { &*(p as *const EeWonDialog) })
}

fn dlg_call_of(n: i64) -> D {
    assert!((1..=D::SetField as i64).contains(&n), "dialog call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, D>(n as u32) }
}

/// The Dialog's override of a container call (the C++ test's `DialogOverride`).
fn dialog_override(c: EeWonCtCall) -> Option<D> {
    type C = EeWonCtCall;
    Some(match c {
        C::SizeChanged => D::SizeChanged,
        C::MouseDown => D::MouseDown,
        C::MouseUp => D::MouseUp,
        C::MouseExit => D::MouseExit,
        C::MouseDrag => D::MouseDrag,
        C::GotFocus => D::GotFocus,
        C::LostFocus => D::LostFocus,
        C::KeyDown => D::KeyDown,
        C::CanClose => D::CanClose,
        _ => return None,
    })
}

/// A container's call as the C++ test's `SideCall` makes it: container 1's overridden calls to the dialog, a
/// viewport's overrides (ScrollAreaViewport's ChildInvalidated, its empty ChildStateChanged, its SetCursor on the
/// scroll area).
pub(crate) fn side_call(ct: isize, call: EeWonCtCall, a: isize, b: isize, c: isize) -> isize {
    if let Some(r) = EXT.with(|e| e.get()).and_then(|e| (e.side)(ct, call, a, b, c)) {
        return r;
    }
    if ct == 1 {
        if let (Some(d), Some(o)) = (dlg_of(), dialog_override(call)) {
            return d.call(o, a, b, c);
        }
    }
    let area = ct_of(ct).map_or(0, |t| t.field(EeWonCtField::ViewportArea as i32));
    if area != 0 {
        match call {
            EeWonCtCall::ChildInvalidated => return side_call(ct, EeWonCtCall::VpChildInvalidated, a, b, c),
            EeWonCtCall::ChildStateChanged => return 0,
            EeWonCtCall::SetCursor => return side_call(area, EeWonCtCall::SetCursor, a, b, c),
            _ => {}
        }
    }
    match ct_of(ct) {
        Some(t) => t.call(call, a, b, c),
        None => {
            fail(format!("line {}: no container {ct}", with(|r| r.at)));
            0
        }
    }
}

fn apply_env(n: &[i64]) {
    with(|r| {
        let k = n[0] as usize;
        for (i, v) in n[1..].iter().enumerate() {
            r.env[k + i] = *v;
        }
    });
}

pub(crate) fn call_of(n: i64) -> EeWonCtCall {
    assert!((1..=EeWonCtCall::VpChildInvalidated as i64).contains(&n), "call {n}");
    // SAFETY: a value of the enum's range (repr(u32), consecutive from 1)
    unsafe { std::mem::transmute::<u32, EeWonCtCall>(n as u32) }
}

/// The lines after an op (or a "do"'s call) up to its "=": the world's changes and the nested calls.
fn answer() -> isize {
    loop {
        let l = next_line();
        if let Some(v) = l.strip_prefix("= ") {
            return v.parse().unwrap_or(0);
        } else if l.starts_with("set ") {
            apply_set(&nums(l));
        } else if l.starts_with("env ") {
            apply_env(&nums(l));
        } else if l.starts_with("do ") {
            let n = nums(l);
            side_call(n[0] as isize, call_of(n[1]), n[2] as isize, n[3] as isize, n[4] as isize);
        } else {
            fail(format!("line {}: golden {l:?} where an op's lines go on", with(|r| r.at)));
            return 0;
        }
    }
}

fn replay_want_focus(c: isize) -> bool {
    let w = with(|r| r.w[c as usize]);
    let modal = w.parent != 0 && ct_of(w.parent).is_some_and(|t| t.field(EeWonCtField::ModalCount as i32) > 0);
    guifocus::want_focus(w.bits, w.bits & 1 == 0, w.bits & 0x80 != 0, modal)
}

pub(crate) unsafe extern "C" fn replay_host(
    comp: *mut c_void,
    op: Op,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    let me = comp as isize;
    let w = |i: isize| with(|r| r.w[i as usize]);
    let ext = EXT.with(|e| e.get());
    if let Some(r) = ext.and_then(|e| (e.read)(me, op, a, b, c, d)) {
        return r;
    }
    match op {
        Op::CtBits => return w(a).bits as isize,
        Op::CtParentOf => return w(a).parent,
        Op::CtTimerFlags => return w(a).timer as isize,
        Op::CtInvalidState => return w(a).invalid as isize,
        Op::CtFocusId => return w(a).focus_id as isize,
        Op::ChildLeft => return w(a).x as isize,
        Op::ChildTop => return w(a).y as isize,
        Op::ChildWidth => return w(a).w as isize,
        Op::ChildHeight => return w(a).h as isize,
        Op::ChildControlId => return w(a).control_id as isize,
        Op::CtContains => {
            let q = w(a);
            let (x, y) = (b as i32, c as i32);
            return (x >= q.x && x < q.x.wrapping_add(q.w) && y >= q.y && y < q.y.wrapping_add(q.h)) as isize;
        }
        Op::CtIntersects => {
            let (p, q) = (w(a), w(b));
            let r1 = crate::guilayout::EeWonRect { x: p.x, y: p.y, width: p.w, height: p.h };
            let r2 = crate::guilayout::EeWonRect { x: q.x, y: q.y, width: q.w, height: q.h };
            return crate::guilayout::intersects(r1, r2) as isize;
        }
        Op::CtGrabBG => return if w(a).bits & 0x100 != 0 { 0x9000 + a } else { 0 },
        Op::CtOwnScheme => return if a % 3 == 0 { 0xA000 + a } else { 0 },
        Op::CtScheme => return with(|r| r.scheme),
        Op::CtRoot => return with(|r| r.env[0]) as isize,
        Op::CtRootFocused => return with(|r| r.env[1]) as isize,
        Op::CtMouseInClient => return with(|r| r.env[2]) as isize,
        Op::CtDefaultCursor => return with(|r| r.env[6]) as isize,
        Op::CtWantFocus => return replay_want_focus(me) as isize,
        Op::CtNextSibling => {
            let p = w(a).parent;
            return ct_of(p).map_or(0, |t| t.next_child(a, b != 0, c != 0));
        }
        Op::CtContainerOf => return ct_of(a).map_or(0, |t| t as *const EeWonContainer as isize),
        Op::GetMousePos => {
            let (x, y) = with(|r| (r.env[3], r.env[4]));
            // SAFETY: the Rust passes two writable i32s
            unsafe {
                *(a as *mut i32) = x as i32;
                *(b as *mut i32) = y as i32;
            }
            return 0;
        }
        Op::KeyMod => return with(|r| r.env[5]) as isize,
        Op::VpAreaRectVisible => {
            if a == 0 {
                return 0;
            }
            // SAFETY: the Rust passes its rectangle
            let q = unsafe { &*(b as *const crate::guictl::EeWonRect4) };
            let p = w(a);
            let r1 = crate::guilayout::EeWonRect { x: q.x, y: q.y, width: q.w, height: q.h };
            let r2 = crate::guilayout::EeWonRect { x: 0, y: 0, width: p.w, height: p.h };
            return crate::guilayout::intersects(r1, r2) as isize;
        }
        Op::DlgIsModal => return (with(|r| r.env[8]) as isize == me) as isize,
        Op::DlgIsDefault => return (with(|r| r.env[9]) as isize == a) as isize,
        Op::EventType => return with(|r| r.env[10]) as isize,
        Op::DlgEventButtonFlags => return with(|r| r.env[11]) as isize,
        Op::EventCompControlId => return with(|r| r.env[12]) as isize,
        _ => {}
    }
    let mine = if let Some(l) = ext.and_then(|e| (e.line)(me, op, a, b, c, d)) {
        l
    } else if op == Op::DlgInvalidateRect {
        // SAFETY: the Rust passes its rectangle
        let q = unsafe { &*(a as *const crate::guictl::EeWonRect4) };
        format!("op {} rect {} {} {} {} @{me}", op as u32, q.x, q.y, q.w, q.h)
    } else if op == Op::Draw {
        // SAFETY: the Rust passes a draw operation
        let o = unsafe { &*(b as *const EeWonDrawOp) };
        format!("op {} {a} {} {} {} {} {} @{me}", op as u32, o.kind, o.a, o.b, o.c, o.d)
    } else {
        format!("op {} {a} {b} {c} {d} @{me}", op as u32)
    };
    let l = next_line();
    if l != mine {
        fail(format!("line {}: golden {l:?}, rust {mine:?}", with(|r| r.at)));
        return 0;
    }
    with(|r| match op {
        Op::CtSetParent => r.w[a as usize].parent = if b != 0 { me } else { 0 },
        Op::CtSetFocusId => r.w[a as usize].focus_id = b as i32,
        Op::CtSetInvalidState => r.w[a as usize].invalid = b as i32,
        Op::CtSetTimerFlags => r.w[me as usize].timer = a as i32,
        Op::ChildSetFlags => {
            let q = &mut r.w[a as usize];
            q.bits = if c != 0 { q.bits | b as u32 } else { q.bits & !(b as u32) };
        }
        Op::CtSetScheme => r.scheme = b,
        Op::DlgInvalidateRect => {
            // Component::InvalidateRect: moved by the component's place when it has a parent
            let w = r.w[me as usize];
            if w.parent != 0 {
                // SAFETY: the Rust passes its rectangle
                let q = unsafe { &mut *(a as *mut crate::guictl::EeWonRect4) };
                q.x = q.x.wrapping_add(w.x);
                q.y = q.y.wrapping_add(w.y);
            }
        }
        _ => {}
    });
    answer()
}

pub(crate) fn replay(seq: &[&'static str]) -> Option<String> {
    replay_with(seq, None)
}

/// The replay of a golden with its own parts.
pub(crate) fn replay_with(seq: &[&'static str], ext: Option<Ext>) -> Option<String> {
    EXT.with(|e| e.set(ext));
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay { lines: seq.to_vec(), scheme: 0xB000, ..Replay::default() });
    });
    while with(|r| r.at < r.lines.len() && r.failure.is_none()) {
        let l = next_line();
        let n = nums(l);
        let word = l.split_whitespace().next().unwrap_or("");
        match word {
            "comp" => with(|r| {
                r.w[n[0] as usize] = W {
                    x: n[1] as i32,
                    y: n[2] as i32,
                    w: n[3] as i32,
                    h: n[4] as i32,
                    bits: n[5] as u32,
                    parent: 0,
                    timer: n[6] as i32,
                    invalid: n[7] as i32,
                    focus_id: 0,
                    control_id: n[8] as i32,
                }
            }),
            "env" => apply_env(&n),
            "set" => apply_set(&n),
            "new" => {
                let t = Box::into_raw(Box::new(EeWonContainer::new(n[0] as *mut c_void, Some(replay_host), false)));
                with(|r| r.cts[n[0] as usize] = t as usize);
            }
            "free" => {
                let p = with(|r| std::mem::take(&mut r.cts[n[0] as usize]));
                // SAFETY: made by "new"; taken out of the table first, so no later line reaches it
                unsafe {
                    (*(p as *const EeWonContainer)).destroy();
                    drop(Box::from_raw(p as *mut EeWonContainer));
                }
            }
            "dialog" => {
                let ct = with(|r| r.cts[1]) as *const EeWonContainer;
                let d = Box::into_raw(Box::new(EeWonDialog::new(
                    std::ptr::dangling_mut::<c_void>(),
                    Some(replay_host),
                    ct,
                )));
                with(|r| r.dlg = d as usize);
            }
            "free-dialog" => {
                let p = with(|r| std::mem::take(&mut r.dlg));
                // SAFETY: made by "dialog"; taken out first
                unsafe {
                    (*(p as *const EeWonDialog)).destroy();
                    drop(Box::from_raw(p as *mut EeWonDialog));
                }
            }
            "dlg" => {
                let d = dlg_of().expect("a dialog");
                let r1 = d.call(dlg_call_of(n[0]), n[1] as isize, n[2] as isize, n[3] as isize);
                let want = format!("-> {r1} 0");
                let got = next_line();
                if got != want {
                    fail(format!("line {}: golden {got:?}, rust {want:?}", with(|r| r.at)));
                }
            }
            "call" => {
                let call = call_of(n[1]);
                let (r1, r2) = if call == EeWonCtCall::DesiredSize {
                    let (mut w, mut h) = (-1i32, -1i32);
                    side_call(n[0] as isize, call, &mut w as *mut i32 as isize, &mut h as *mut i32 as isize, 0);
                    (w as i64, h as i64)
                } else {
                    (side_call(n[0] as isize, call, n[2] as isize, n[3] as isize, n[4] as isize) as i64, 0)
                };
                let want = format!("-> {r1} {r2}");
                let got = next_line();
                if got != want {
                    fail(format!("line {}: golden {got:?}, rust {want:?}", with(|r| r.at)));
                }
            }
            _ => {
                if !ext.is_some_and(|e| (e.word)(word, &n)) {
                    fail(format!("line {}: unexpected {l:?}", with(|r| r.at)));
                }
            }
        }
    }
    if let Some(e) = ext {
        (e.cleanup)();
    }
    // a failure may leave containers behind: freed without their destructor's calls
    with(|r| {
        if r.dlg != 0 {
            // SAFETY: made by "dialog" and not freed
            drop(unsafe { Box::from_raw(r.dlg as *mut EeWonDialog) });
            r.dlg = 0;
        }
        for p in r.cts.iter_mut() {
            if *p != 0 {
                // SAFETY: made by "new" and not freed
                drop(unsafe { Box::from_raw(*p as *mut EeWonContainer) });
                *p = 0;
            }
        }
    });
    REPLAY.with(|r| r.borrow_mut().take().and_then(|r| r.failure))
}

fn sequences() -> Vec<(String, Vec<&'static str>)> {
    sequences_of(GOLDEN)
}

pub(crate) fn sequences_of(golden: &'static str) -> Vec<(String, Vec<&'static str>)> {
    let mut out: Vec<(String, Vec<&'static str>)> = Vec::new();
    for l in golden.lines() {
        if l.starts_with("seq ") {
            out.push((l.to_string(), Vec::new()));
        } else if let Some(s) = out.last_mut() {
            s.1.push(l);
        }
    }
    out
}

#[test]
fn golden_sequences_replay() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (name, lines) in &seqs {
        if let Some(f) = replay(lines) {
            panic!("{name}: {f}");
        }
    }
}

#[test]
fn golden_replay_fails_on_a_changed_line() {
    // the first child timer request in the golden turned on instead of off
    let pat = format!(" {} 0 0 @", V::RequestTimer as u32);
    let seqs = sequences();
    let (name, lines) = seqs
        .iter()
        .find(|(_, l)| l.iter().any(|x| x.starts_with("op 401 ") && x.contains(&pat)))
        .expect("a sequence that drops a timer child");
    let k = lines.iter().position(|x| x.starts_with("op 401 ") && x.contains(&pat)).unwrap();
    let mut changed = lines.clone();
    let swapped = changed[k].replacen(&pat, &format!(" {} 1 0 @", V::RequestTimer as u32), 1);
    changed[k] = Box::leak(swapped.into_boxed_str());
    assert!(replay(&changed).is_some(), "{name} replayed with a changed line");
    assert!(replay(lines).is_none());
}
