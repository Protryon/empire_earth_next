//! The Rust Scrollbar against the original's (`golden_barobj.txt`, printed by `tests/won/won_barobj_test.cpp
//! --print` over the original Scrollbar's methods and a scripted host): each sequence is replayed op for op. The
//! replay host keeps the C++ host's model (the parts' places, sizes, visibility and references, the bar's size,
//! disabled state, mouse child and point, the events) from the golden's driver lines and the traced ops, and
//! answers the untraced reads from it. The parts and the bar are blocks (compmem's map world, the bar 0x54): the
//! Rust reads and moves them in place, the world's SetEnabled and SetVisible set their flags as the driver's do.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::barobj::{EeWonBarCall as C, EeWonScrollbar};
use crate::compmem::{self, map_block};
use crate::compobj::{host_world, EeWonComp as K};
use crate::guihost::{ee_won_gui_event_type, EeWonGuiEvent, EeWonGuiOp as Op};

const GOLDEN: &str = include_str!("golden_barobj.txt");

#[derive(Clone, Copy, Default)]
struct Comp {
    held: i32,
}

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    comps: BTreeMap<isize, Comp>,
    events: BTreeMap<isize, (isize, isize)>,
    mouse: (i32, i32),
    failure: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

/// The next golden line must be `want` (or start with it when `prefix`); returns the rest.
fn expect(want: &str, prefix: bool) -> String {
    with(|r| {
        let line = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        let ok = if prefix { line.starts_with(want) } else { line == want };
        if !ok {
            if r.failure.is_none() {
                r.failure = Some(format!("line {}: golden '{line}', rust '{want}'", r.at));
            }
            return String::new();
        }
        line[want.len()..].to_string()
    })
}

fn hex(w: &str) -> isize {
    u64::from_str_radix(w.trim(), 16).unwrap_or(0) as i64 as isize
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let n = op as u32;
    // the untraced reads
    match op {
        Op::Comp if a == K::VPositionChanged as isize || a == K::VSizeChanged as isize => {
            let r = compmem::bounds(b);
            expect(&format!("{n} comp {a} {b:x} at {} {} {} {}", r.x, r.y, r.width, r.height), false);
            return 0;
        }
        Op::Comp => {
            let w = expect(&format!("{n} comp {a} {b:x} {c} -> "), true);
            // the world's SetEnabled and SetVisible (the driver's rule)
            if a == K::VSetEnabled as isize {
                compmem::set_flags(b, 0x80, c == 0);
            } else if a == K::VSetVisible as isize {
                compmem::set_flags(b, 0x1, c == 0);
            }
            return hex(&w);
        }
        Op::EventType => return with(|r| r.events.get(&a).map_or(0, |e| e.0)),
        Op::EventComponent => return with(|r| r.events.get(&a).map_or(0, |e| e.1)),
        Op::GetMousePos => {
            let (x, y) = with(|r| r.mouse);
            // SAFETY: the object passes two live i32s
            unsafe {
                *(a as *mut i32) = x;
                *(b as *mut i32) = y;
            }
            return 0;
        }
        Op::FireEvent => {
            // SAFETY: the object passes an EeWonGuiEvent's value
            let e: EeWonGuiEvent = unsafe { std::mem::transmute(a as u32) };
            expect(&format!("fire {}", ee_won_gui_event_type(e)), false);
            return 0;
        }
        _ => {}
    }
    let head = format!("{n} {:x} {:x} {:x} -> ", a as i64, b as i64, c as i64);
    if op == Op::NewButton {
        let id = hex(&expect(&head, true));
        with(|r| {
            r.comps.insert(id, Comp::default());
        });
        // SAFETY: the map world's block of the new button
        unsafe { *map_block(id) = compmem::EeWonCompMembers::default() };
        return id;
    }
    with(|r| {
        let k = r.comps.entry(a).or_default();
        match op {
            Op::ChildHold => k.held += 1,
            Op::ChildRelease => k.held -= 1,
            Op::AddChild => compmem::set_parent(a, 0x54),
            _ => {}
        }
    });
    expect(&format!("{head}0"), false);
    0
}

/// The bar's own size (its block, 0x54).
fn set_size(w: i32, h: i32) {
    // SAFETY: the map world's block of the bar
    let m = unsafe { &mut *map_block(0x54) };
    m.bounds.width = w;
    m.bounds.height = h;
}

fn words(s: &str) -> Vec<&str> {
    s.split_whitespace().collect()
}

fn replay(lines: &[String]) -> Option<String> {
    let start = words(&lines[0]);
    let (w, h, v): (i32, i32, bool) =
        (start[1].parse().unwrap_or(0), start[2].parse().unwrap_or(0), start.get(3) == Some(&"1"));
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: lines.to_vec(), at: 1, ..Replay::default() }));
    let _world = compmem::map_world();
    let _host = host_world(Some(replay_host));
    set_size(w, h);
    let holder = Box::new(EeWonScrollbar::new(0x54 as *mut c_void, Some(replay_host), v));
    let t: &EeWonScrollbar = &holder;
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
        let ws = words(&line);
        match ws[0] {
            "comp" => {
                let id = hex(ws[1]);
                with(|r| r.comps.insert(id, Comp::default()));
                let mut m = compmem::block(
                    ws[2].parse().unwrap_or(0),
                    ws[3].parse().unwrap_or(0),
                    ws[4].parse().unwrap_or(0),
                    ws[5].parse().unwrap_or(0),
                );
                if ws.get(6) == Some(&"0") {
                    m.bits = 0x1;
                }
                // SAFETY: the map world's block, no other reference to it live
                unsafe { *map_block(id) = m };
            }
            "size" => set_size(ws[1].parse().unwrap_or(0), ws[2].parse().unwrap_or(0)),
            "mouse-child" => {} // (the bar asks GetMouseChild through the component host: traced)
            "mouse-pos" => with(|r| r.mouse = (ws[1].parse().unwrap_or(0), ws[2].parse().unwrap_or(0))),
            "event" => with(|r| {
                r.events.insert(hex(ws[1]), (ws[2].parse().unwrap_or(0), hex(ws[3])));
            }),
            "call" => {
                let want: Option<isize> = line.split(" -> ").nth(1).and_then(|x| x.trim().parse().ok());
                let (a, b, c) = (hex(ws[2]), hex(ws[3]), hex(ws[4]));
                let got = match ws[1] {
                    "size" => t.call(C::SizeChanged, 0, 0, 0),
                    "down" => t.call(C::MouseDown, a, b, c),
                    "drag" => t.call(C::MouseDrag, a, b, 0),
                    "event" => t.call(C::HandleEvent, a, 0, 0),
                    "enable" => t.call(C::SetEnabled, a, 0, 0),
                    "copy" => {
                        let s = EeWonScrollbar::new(0x55 as *mut c_void, Some(replay_host), true);
                        s.call(C::SetLineScrollSizes, a, b, 0);
                        s.call(C::SetResizeThumb, c & 1, 0, 0);
                        s.call(C::SetMinThumbSize, c >> 1, 0, 0);
                        t.call(C::CopyAttributes, &s as *const EeWonScrollbar as isize, 0, 0)
                    }
                    "range" => t.call(C::SetRange, a, b, 0),
                    "lines" => t.call(C::SetLineScrollSizes, a, b, 0),
                    "position" => t.call(C::SetPosition, a, 0, 0),
                    "layout" => t.call(C::LayoutThumb, a, 0, 0),
                    "part" => t.call(C::SetPart, a, b, 0),
                    "vertical" => t.call(C::SetVertical, a, 0, 0),
                    "resize" => t.call(C::SetResizeThumb, a, 0, 0),
                    "min" => t.call(C::SetMinThumbSize, a, 0, 0),
                    "added" => t.call(C::AddedToParent, 0, 0, 0),
                    "add-controls" => t.call(C::AddControls, 0, 0, 0),
                    "get-part" => t.call(C::GetPart, a, 0, 0),
                    "is-vertical" => t.call(C::IsVertical, 0, 0, 0),
                    "get-position" => t.call(C::GetPosition, 0, 0, 0),
                    name => return Some(format!("unknown call '{name}'")),
                };
                if want.is_some_and(|w| w != got) {
                    return Some(format!("{line}: rust answered {got}"));
                }
            }
            _ => return Some(format!("unexpected golden line '{line}'")),
        }
    }
    // the bar goes (its parts given back) while the replay still answers
    drop(holder);
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            r.failure = Some(format!("line {}: the golden goes on", r.at));
        }
        if r.failure.is_none() && r.comps.values().any(|k| k.held != 0) {
            r.failure = Some("a part's reference not given back".to_string());
        }
        r.failure.clone()
    });
    REPLAY.with(|r| *r.borrow_mut() = None);
    f
}

fn sequences() -> Vec<Vec<String>> {
    let mut out: Vec<Vec<String>> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("seq ") {
            out.push(Vec::new());
        } else if let Some(s) = out.last_mut() {
            s.push(line.to_string());
        }
    }
    out
}

#[test]
fn replays_the_original_scrollbar() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_new_bar_holds_nothing_and_steps_ten() {
    let t = EeWonScrollbar::new(std::ptr::null_mut(), None, false);
    assert_eq!(t.call(C::GetPart, 0, 0, 0), 0);
    assert_eq!(t.call(C::IsVertical, 0, 0, 0), 0);
    t.call(C::SetMinThumbSize, 300, 0, 0); // an unsigned char: 300 is 44
    t.call(C::SetRange, 100, 40, 0);
    assert_eq!(t.call(C::GetPosition, 0, 0, 0), 0);
    let s = EeWonScrollbar::new(std::ptr::null_mut(), None, true);
    s.call(C::CopyAttributes, &t as *const EeWonScrollbar as isize, 0, 0);
    s.call(C::CopyAttributes, &s as *const EeWonScrollbar as isize, 0, 0); // a copy from itself keeps everything
    assert_eq!(s.call(C::IsVertical, 0, 0, 0), 1);
}
