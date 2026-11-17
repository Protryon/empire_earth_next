//! The Rust Splitter against the original's (`golden_splitobj.txt`, printed by `tests/won/won_splitobj_test.cpp
//! --print` over the original Splitter's methods and a scripted host): each sequence is replayed op for op, the
//! replay host answering from the golden. The components are blocks (compmem's map world): a "comp" line makes one,
//! the splitter's own is 0x54; the world's SetEnabled and SetVisible set their flags as the driver's do.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::compmem::{self, map_block};
use crate::compobj::{host_world, EeWonComp as K};
use crate::guihost::EeWonGuiOp as Op;
use crate::splitobj::{EeWonSplitCall as C, EeWonSplitter};

const GOLDEN: &str = include_str!("golden_splitobj.txt");

struct Replay {
    lines: Vec<String>,
    at: usize,
    failure: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

/// A block of the world (`comp ID W H BITS EXTRA`).
fn make_comp(words: &[&str]) {
    let id = hex(words[0]);
    // SAFETY: the map world's block, no other reference to it live
    let m = unsafe { &mut *map_block(id) };
    *m = compmem::EeWonCompMembers::default();
    m.bounds.width = words[1].parse().unwrap_or(0);
    m.bounds.height = words[2].parse().unwrap_or(0);
    m.bits = hex(words[3]) as i32;
    m.extra = hex(words[4]);
}

/// The world's blocks made before the next op line.
fn take_comps() {
    loop {
        let l = with(|r| r.lines.get(r.at).cloned().unwrap_or_default());
        let Some(rest) = l.strip_prefix("comp ") else { return };
        with(|r| r.at += 1);
        make_comp(&rest.split_whitespace().collect::<Vec<_>>());
    }
}

fn set_self_size(w: i32, h: i32) {
    // SAFETY: the map world's block of the splitter
    let m = unsafe { &mut *map_block(0x54) };
    m.bounds.width = w;
    m.bounds.height = h;
}

fn expect(want: &str) -> Vec<String> {
    take_comps();
    let line = with(|r| {
        let l = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        l
    });
    if !line.starts_with(want) {
        with(|r| {
            if r.failure.is_none() {
                r.failure = Some(format!("line {}: golden '{line}', rust '{want}'", r.at));
            }
        });
        return Vec::new();
    }
    line[want.len()..].split_whitespace().map(str::to_string).collect()
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Comp if a == K::VPositionChanged as isize || a == K::VSizeChanged as isize => {
            let r = compmem::bounds(b);
            expect(&format!("{n} comp {a} {b:x} at {} {} {} {}", r.x, r.y, r.width, r.height));
            0
        }
        Op::Comp => {
            let w = expect(&format!("{n} comp {a} {b:x} {c} -> "));
            // the world's SetEnabled and SetVisible (the driver's rule)
            if a == K::VSetEnabled as isize {
                compmem::set_flags(b, 0x80, c == 0);
            } else if a == K::VSetVisible as isize {
                compmem::set_flags(b, 0x1, c == 0);
            }
            w.first().map_or(0, |x| hex(x))
        }
        _ => {
            let w = expect(&format!("{n} {a:x} {b:x} {c} -> "));
            w.first().map_or(0, |x| hex(x))
        }
    }
}

/// a `%llx` of the golden (a negative value is its 64-bit two's complement)
fn hex(w: &str) -> isize {
    u64::from_str_radix(w, 16).unwrap_or(0) as i64 as isize
}

fn replay(lines: &[String]) -> Option<String> {
    let start: Vec<i32> = lines[0].split_whitespace().skip(1).filter_map(|w| w.parse().ok()).collect();
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: lines.to_vec(), at: 1, failure: None }));
    let _world = compmem::map_world();
    let _host = host_world(Some(replay_host));
    set_self_size(start[0], start[1]);
    let holder = Box::new(EeWonSplitter::new(0x54 as *mut c_void, Some(replay_host)));
    let t: &EeWonSplitter = &holder;
    loop {
        let (line, done) = with(|r| {
            if r.failure.is_some() || r.at >= r.lines.len() {
                return (String::new(), true);
            }
            let l = r.lines[r.at].clone();
            r.at += 1;
            (l, false)
        });
        if done || line == "free" {
            break;
        }
        if let Some(rest) = line.strip_prefix("comp ") {
            make_comp(&rest.split_whitespace().collect::<Vec<_>>());
            continue;
        }
        let Some(rest) = line.strip_prefix("call ") else {
            return Some(format!("unexpected golden line '{line}'"));
        };
        let mut parts = rest.split(" -> ");
        let words: Vec<&str> = parts.next().unwrap_or("").split_whitespace().collect();
        let want: Option<isize> = parts.next().and_then(|w| w.trim().parse().ok());
        let (a, b, c) = (hex(words[1]), hex(words[2]), hex(words[3]));
        let call = match words[0] {
            "quadrant" => C::AddQuadrant,
            "splitter" => C::AddSplitter,
            "complex" => C::AddComplexSplitter,
            "invisible" => C::AddInvisibleSplitter,
            "button" => C::RegisterMaxButton,
            "min-lr" => C::SetMinLeftRight,
            "min-tb" => C::SetMinTopBottom,
            "percent" => C::SetPercentages,
            "max" => C::MaxQuadrant,
            "unmax" => C::UnMaxQuadrant,
            "set-pos" => C::SetSplitterPos,
            "size" => {
                set_self_size(a as i32, b as i32);
                C::SizeChanged
            }
            "down" => C::MouseDown,
            "up" => C::MouseUp,
            "drag" => C::MouseDrag,
            "event" => C::HandleEvent,
            "check-max" => C::CheckMaxButton,
            name => return Some(format!("unknown call '{name}'")),
        };
        let got = t.call(call, a, b, c);
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the splitter goes (its references given back) while the replay still answers
    drop(holder);
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            r.failure = Some(format!("line {}: the golden goes on", r.at));
        }
        r.failure.clone()
    });
    REPLAY.with(|r| *r.borrow_mut() = None);
    f
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

#[test]
fn golden_splitobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_changed_answer_is_seen() {
    let seqs = sequences();
    let (s, k) =
        seqs.iter().find_map(|s| s.iter().position(|l| l.contains(" at ")).map(|k| (s, k))).expect("a placement");
    let mut m = s.clone();
    let head = m[k].rsplit_once(' ').map_or(String::new(), |(h, _)| h.to_string());
    m[k] = format!("{head} -77");
    assert!(replay(&m).is_some());
}
