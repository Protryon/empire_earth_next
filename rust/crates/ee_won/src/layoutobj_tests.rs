//! The Rust child layouts against the original's (`golden_layoutobj.txt`, printed by
//! `tests/won/won_layoutobj_test.cpp --print` over the original manager and instruction classes and a world of
//! components): each sequence's calls ("call", "set") are made on the Rust objects over the same world, and the
//! trace they make (the moves, the references, the relayouts, the answers) must be the golden's.

use std::cell::RefCell;
use std::ffi::c_void;

use super::*;
use crate::guictl::EeWonRect4;
use crate::guilayout::percent_borders;

const GOLDEN: &str = include_str!("golden_layoutobj.txt");
const N: usize = 14;
const CONTAINER: isize = 1;

#[derive(Clone, Copy, Default)]
struct Bx {
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    parent: isize,
    refs: i32,
}

/// An instruction's C++ shell: its id and reference count over its Rust object.
struct Shell {
    id: i32,
    refs: i32,
    lay: *mut EeWonLayout,
}

struct World {
    w: [Bx; N + 1],
    trace: Vec<String>,
    depth: i32,
    mgr: *const EeWonLayoutMgr,
    live: i32,
}

thread_local! {
    static WORLD: RefCell<Option<World>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(w.borrow_mut().as_mut().expect("a world")))
}
fn trace(s: String) {
    with(|w| w.trace.push(s));
}

fn size_changed(c: usize, nw: i32, nh: i32) {
    let run = with(|w| {
        let changed = w.w[c].w != nw || w.w[c].h != nh;
        w.w[c].w = nw;
        w.w[c].h = nh;
        let run = changed && c as isize == CONTAINER && w.depth < 1;
        if run {
            w.trace.push("relayout".into());
            w.depth += 1;
        }
        run
    });
    if run {
        let m = with(|w| w.mgr);
        // SAFETY: the manager is live while its sequence runs (a re-entrant shared call, as Container::SizeChanged's)
        unsafe { (*m).execute(CONTAINER) };
        with(|w| w.depth -= 1);
    }
}

fn hold(t: *mut Shell) {
    // SAFETY: a live shell (the replay's own)
    let s = unsafe { &mut *t };
    s.refs += 1;
    trace(format!("hold {}", s.id));
}
fn release(t: *mut Shell) {
    // SAFETY: a live shell; freed at its last reference
    let s = unsafe { &mut *t };
    s.refs -= 1;
    trace(format!("release {}", s.id));
    if s.refs == 0 {
        trace(format!("delete {}", s.id));
        with(|w| w.live -= 1);
        // SAFETY: the shell owns its Rust object and was boxed by the replay
        unsafe {
            ee_won_layout_free(s.lay);
            drop(Box::from_raw(t));
        }
    }
}

unsafe extern "C" fn host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let k = a as usize;
    let get = |f: fn(&Bx) -> isize| with(|w| f(&w.w[k]));
    match op {
        Op::ChildLeft => get(|b| b.x as isize),
        Op::ChildTop => get(|b| b.y as isize),
        Op::ChildWidth => get(|b| b.w as isize),
        Op::ChildHeight => get(|b| b.h as isize),
        Op::LayParentOf => get(|b| b.parent),
        Op::ChildSetPos => {
            trace(format!("pos {k} {b} {c}"));
            with(|w| {
                w.w[k].x = b as i32;
                w.w[k].y = c as i32;
            });
            0
        }
        Op::ChildSetSize => {
            trace(format!("size {k} {b} {c}"));
            size_changed(k, b as i32, c as i32);
            0
        }
        Op::ChildSetPosSize => {
            // SAFETY: the object passes a live rectangle
            let r = unsafe { *(b as *const EeWonRect4) };
            trace(format!("possize {k} {} {} {} {}", r.x, r.y, r.w, r.h));
            with(|w| {
                w.w[k].x = r.x;
                w.w[k].y = r.y;
            });
            size_changed(k, r.w, r.h);
            0
        }
        Op::LaySetSizeAtLeast => {
            trace(format!("atleast {k} {b} {c}"));
            let (cw, ch) = with(|w| (w.w[k].w, w.w[k].h));
            size_changed(k, (b as i32).max(cw), (c as i32).max(ch));
            0
        }
        Op::LayHold => {
            hold(a as *mut Shell);
            0
        }
        Op::LayRelease => {
            release(a as *mut Shell);
            0
        }
        Op::ChildHold => {
            trace(format!("phold {k}"));
            with(|w| w.w[k].refs += 1);
            0
        }
        Op::ChildRelease => {
            trace(format!("prelease {k}"));
            with(|w| w.w[k].refs -= 1);
            0
        }
        _ => {
            trace(format!("unexpected-op {}", op as u32));
            0
        }
    }
}

fn nums(s: &str) -> Vec<i64> {
    s.split_whitespace().filter_map(|w| w.parse().ok()).collect()
}

/// Replays one sequence; the Rust's trace.
fn replay(lines: &[String]) -> Vec<String> {
    let start = nums(&lines[0]);
    let mut w = [Bx::default(); N + 1];
    for c in 1..=N {
        let q = &start[(c - 1) * 5..c * 5];
        w[c] = Bx { x: q[0] as i32, y: q[1] as i32, w: q[2] as i32, h: q[3] as i32, parent: q[4] as isize, refs: 0 };
    }
    let mgr = Box::new(EeWonLayoutMgr::new(0x77 as *mut c_void, Some(host)));
    let mp: *const EeWonLayoutMgr = &*mgr;
    WORLD.with(|x| *x.borrow_mut() = Some(World { w, trace: vec![lines[0].clone()], depth: 0, mgr: mp, live: 0 }));
    let mut next_id = 0;
    for line in &lines[1..] {
        let n = nums(line);
        if line.starts_with("call ") || line.starts_with("set ") || line == "free" {
            trace(line.clone());
        }
        if let Some(rest) = line.strip_prefix("call new ") {
            let v = nums(rest);
            let kind = match v[0] {
                1 => EeWonLayoutKind::Child,
                2 => EeWonLayoutKind::Center,
                3 => EeWonLayoutKind::SameSize,
                4 => EeWonLayoutKind::ClearRight,
                5 => EeWonLayoutKind::SpaceEvenly,
                6 => EeWonLayoutKind::NoOverlap,
                7 => EeWonLayoutKind::SizeAtLeast,
                _ => EeWonLayoutKind::Percent,
            };
            let (usage, group, parent, asked) = (v[1], v[2] as i32, v[3] as isize, v[4] as isize);
            let mut refs = [0isize; 5];
            for i in 0..5 {
                refs[i] = v[7 + i] as isize;
            }
            let mut ints = [0i32; 8];
            for i in 0..8 {
                ints[i] = v[12 + i] as i32;
            }
            if kind == EeWonLayoutKind::Percent {
                let given = [refs[1] != 0, refs[2] != 0, refs[3] != 0, refs[4] != 0];
                let idx = percent_borders(given);
                let anchors = [refs[1], refs[2], refs[3], refs[4]];
                for i in 0..4 {
                    refs[1 + i] = if idx[i] < 0 { 0 } else { anchors[idx[i] as usize] };
                }
            }
            let init = EeWonLayoutInit { kind, refs, ints, horz: v[5] != 0, vert: v[6] != 0 };
            next_id += 1;
            with(|w| w.live += 1);
            let shell = Box::into_raw(Box::new(Shell { id: next_id, refs: 0, lay: std::ptr::null_mut() }));
            let lay = Box::into_raw(Box::new(EeWonLayout::new(shell as *mut c_void, Some(host), &init)));
            // SAFETY: the shell is the replay's own, live until its last release
            unsafe { (*shell).lay = lay };
            for &c in &v[20..] {
                // SAFETY: live
                unsafe { (*lay).add(c as isize) };
            }
            hold(shell);
            match usage {
                // SAFETY: held by the creator's reference
                0 => mgr.add_child_layout(shell as isize, unsafe { &*lay }, group, CONTAINER),
                // SAFETY: held by the creator's reference
                1 => unsafe { (*lay).execute(parent) },
                // SAFETY: held
                _ => trace(format!("uses {}", unsafe { (*lay).uses_component(asked) } as i32)),
            }
            release(shell);
        } else if line.starts_with("call center ") {
            let hv = n[2];
            ee_won_layout_center(Some(host), n[0] as isize, n[1] as isize, hv & 2 != 0, hv & 1 != 0);
        } else if line == "call run" {
            mgr.execute(CONTAINER);
        } else if line.starts_with("call remove ") {
            mgr.remove_component(n[0] as isize);
        } else if line == "call clear" {
            mgr.clear();
        } else if line.starts_with("call enable ") {
            mgr.enable_instructions(n[0] as i32, n[1] != 0);
        } else if line.starts_with("call point ") {
            mgr.add_layout_point(n[0] as isize);
        } else if line.starts_with("set ") {
            let c = n[0] as usize;
            with(|w| {
                w.w[c].x = n[1] as i32;
                w.w[c].y = n[2] as i32;
                w.w[c].w = n[3] as i32;
                w.w[c].h = n[4] as i32;
            });
        } else if line == "free" {
            mgr.destroy();
        }
    }
    drop(mgr);
    let out = with(|w| {
        for c in 1..=N {
            assert_eq!(w.w[c].refs, 0, "point {c}'s references");
        }
        assert_eq!(w.live, 0, "instructions left");
        std::mem::take(&mut w.trace)
    });
    WORLD.with(|x| *x.borrow_mut() = None);
    out
}

fn sequences() -> Vec<Vec<String>> {
    let mut out: Vec<Vec<String>> = Vec::new();
    for l in GOLDEN.lines() {
        if l.starts_with("seq ") {
            out.push(Vec::new());
        } else if let Some(s) = out.last_mut() {
            s.push(l.to_string());
        }
    }
    out
}

fn first_difference(a: &[String], b: &[String]) -> Option<String> {
    let n = a.len().max(b.len());
    (0..n).find_map(|i| {
        let (x, y) = (a.get(i).map_or("<end>", String::as_str), b.get(i).map_or("<end>", String::as_str));
        (x != y).then(|| format!("line {i}: golden '{x}', rust '{y}'"))
    })
}

#[test]
fn golden_layoutobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        let got = replay(s);
        if let Some(d) = first_difference(s, &got) {
            panic!("sequence {i}: {d}");
        }
    }
}

#[test]
fn a_changed_move_is_seen() {
    let seqs = sequences();
    let (s, k) =
        seqs.iter().find_map(|s| s.iter().position(|l| l.starts_with("pos ")).map(|k| (s, k))).expect("a move");
    let mut m = s.clone();
    let mut w: Vec<String> = m[k].split(' ').map(str::to_string).collect();
    let x: i32 = w[2].parse().unwrap();
    w[2] = (x + 1).to_string();
    m[k] = w.join(" ");
    assert!(first_difference(&m, &replay(&m)).is_some());
}

fn init(kind: EeWonLayoutKind, refs: [isize; 5], ints: [i32; 8]) -> EeWonLayoutInit {
    EeWonLayoutInit { kind, refs, ints, horz: true, vert: true }
}

#[test]
fn only_the_child_instruction_uses_a_component() {
    let child = EeWonLayout::new(std::ptr::null_mut(), None, &init(EeWonLayoutKind::Child, [5, 6, 0, 0, 0], [0; 8]));
    assert!(child.uses_component(5) && child.uses_component(6) && !child.uses_component(7));
    let center = EeWonLayout::new(std::ptr::null_mut(), None, &init(EeWonLayoutKind::Center, [5, 0, 0, 0, 0], [0; 8]));
    center.add(6);
    assert!(!center.uses_component(5) && !center.uses_component(6));
}

#[test]
fn a_group_is_a_byte_compared_with_the_int() {
    let m = EeWonLayoutMgr::new(std::ptr::null_mut(), None);
    let l = EeWonLayout::new(std::ptr::null_mut(), None, &init(EeWonLayoutKind::Base, [0; 5], [0; 8]));
    m.add_child_layout(0x10, &l, 256, 0);
    // stored as 0, but the id was not 0: disabled, and group 256 never matches again
    assert_eq!(l.group(), 0);
    assert!(!l.enabled());
    m.enable_instructions(256, true);
    assert!(!l.enabled());
    m.enable_instructions(0, true);
    assert!(l.enabled());
    assert_eq!(m.call(EeWonLayoutMgrCall::Count, 0, 0, 0, 0), 1);
    m.clear();
    assert_eq!(m.call(EeWonLayoutMgrCall::Count, 0, 0, 0, 0), 0);
}
