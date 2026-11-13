//! The Rust Scrollbar against the original's (`golden_barobj.txt`, printed by `tests/won/won_barobj_test.cpp
//! --print` over the original Scrollbar's methods and a scripted host): each sequence is replayed op for op. The
//! replay host keeps the C++ host's model (the parts' places, sizes, visibility and references, the bar's size,
//! disabled state, mouse child and point, the events) from the golden's driver lines and the traced ops, and
//! answers the untraced reads from it.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::barobj::{EeWonBarCall as C, EeWonScrollbar};
use crate::guictl::EeWonRect4;
use crate::guihost::{ee_won_gui_event_type, EeWonGuiEvent, EeWonGuiField as F, EeWonGuiOp as Op};

const GOLDEN: &str = include_str!("golden_barobj.txt");

#[derive(Clone, Copy, Default)]
struct Comp {
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    mine: bool,
    held: i32,
}

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    comps: BTreeMap<isize, Comp>,
    events: BTreeMap<isize, (isize, isize)>,
    width: i32,
    height: i32,
    disabled: bool,
    mouse_child: isize,
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
        Op::Get => {
            return with(|r| {
                if a == F::Width as isize {
                    r.width
                } else if a == F::Height as isize {
                    r.height
                } else {
                    0
                }
            }) as isize
        }
        Op::ChildLeft => return with(|r| r.comps.get(&a).map_or(0, |k| k.x)) as isize,
        Op::ChildTop => return with(|r| r.comps.get(&a).map_or(0, |k| k.y)) as isize,
        Op::ChildWidth => return with(|r| r.comps.get(&a).map_or(0, |k| k.w)) as isize,
        Op::ChildHeight => return with(|r| r.comps.get(&a).map_or(0, |k| k.h)) as isize,
        Op::ChildIsMine => return with(|r| r.comps.get(&a).is_some_and(|k| k.mine)) as isize,
        Op::IsDisabled => return with(|r| r.disabled) as isize,
        Op::MouseChild => return with(|r| r.mouse_child),
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
        Op::ChildSetPosSize => {
            // SAFETY: the object passes a live rectangle
            let q = unsafe { *(b as *const EeWonRect4) };
            expect(&format!("{n} pos-size {:x} {} {} {} {}", a as i64, q.x, q.y, q.w, q.h), false);
            with(|r| {
                let k = r.comps.entry(a).or_default();
                k.x = q.x;
                k.y = q.y;
                k.w = q.w;
                k.h = q.h;
            });
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
        return id;
    }
    with(|r| {
        let k = r.comps.entry(a).or_default();
        match op {
            Op::ChildSetPos => {
                k.x = b as i32;
                k.y = c as i32;
            }
            Op::ChildSetSize => {
                k.w = b as i32;
                k.h = c as i32;
            }
            Op::ChildHold => k.held += 1,
            Op::ChildRelease => k.held -= 1,
            Op::AddChild => k.mine = true,
            Op::BaseSetEnabled => r.disabled = a == 0,
            _ => {}
        }
    });
    expect(&format!("{head}0"), false);
    0
}

fn words(s: &str) -> Vec<&str> {
    s.split_whitespace().collect()
}

fn replay(lines: &[String]) -> Option<String> {
    let start = words(&lines[0]);
    let (w, h, v): (i32, i32, bool) =
        (start[1].parse().unwrap_or(0), start[2].parse().unwrap_or(0), start.get(3) == Some(&"1"));
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay { lines: lines.to_vec(), at: 1, width: w, height: h, ..Replay::default() })
    });
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
            "comp" => with(|r| {
                let k = Comp {
                    x: ws[2].parse().unwrap_or(0),
                    y: ws[3].parse().unwrap_or(0),
                    w: ws[4].parse().unwrap_or(0),
                    h: ws[5].parse().unwrap_or(0),
                    ..Comp::default()
                };
                r.comps.insert(hex(ws[1]), k);
            }),
            "size" => with(|r| {
                r.width = ws[1].parse().unwrap_or(0);
                r.height = ws[2].parse().unwrap_or(0);
            }),
            "mouse-child" => with(|r| r.mouse_child = hex(ws[1])),
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
