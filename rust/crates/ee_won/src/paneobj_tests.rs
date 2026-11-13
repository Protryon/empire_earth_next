//! The Rust ScrollArea against the original's (`golden_paneobj.txt`, printed by `tests/won/won_paneobj_test.cpp
//! --print` over the original ScrollArea's methods and a scripted host): each sequence is replayed op for op. The
//! replay host keeps the C++ host's model (the area's size, which scrollers are ScrollbarScrollers, the wheel's
//! answer, whether a notification lays the area out again) from the golden's driver lines and runs what the C++
//! virtuals run (the area's own AdjustPosition and Virtual08).

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::paneobj::{EeWonPaneCall as P, EeWonScrollPane};

const GOLDEN: &str = include_str!("golden_paneobj.txt");
const VIEWPORT: isize = 0x3001;

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    width: i32,
    height: i32,
    bars: BTreeMap<isize, bool>,
    wheel: isize,
    relayout: bool,
    in_relayout: bool,
    pane: usize,
    failure: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

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

fn pane() -> &'static EeWonScrollPane {
    let p = with(|r| r.pane) as *const EeWonScrollPane;
    // SAFETY: the pane under test is live while the replay runs its calls
    unsafe { &*p }
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let n = op as u32;
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
        Op::GetViewport => return VIEWPORT,
        Op::ScrollerAsBars => return with(|r| if a != 0 && r.bars.get(&a) == Some(&true) { a } else { 0 }),
        Op::ChildSetPosSize => {
            // SAFETY: the object passes a live rectangle
            let q = unsafe { *(b as *const EeWonRect4) };
            expect(&format!("{n} pos-size {:x} {} {} {} {}", a as i64, q.x, q.y, q.w, q.h), false);
            return 0;
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            expect(&format!("draw {:x} {} {} {} {} {}", a as i64, o.kind, o.a, o.b, o.c, o.d), false);
            return 0;
        }
        Op::VirtAdjustPosition => {
            expect(&format!("{n} {:x}", a as i64), false);
            pane().adjust_position(a != 0);
            return 0;
        }
        Op::VirtSizeChanged => {
            expect(&format!("{n}"), false);
            pane().size_changed();
            return 0;
        }
        Op::ScrollerNotify => {
            expect(&format!("{n} {:x} {:x}", a as i64, b as i64), false);
            let go = with(|r| {
                let go = r.relayout && !r.in_relayout;
                if go {
                    r.in_relayout = true;
                }
                go
            });
            if go {
                pane().size_changed();
                with(|r| r.in_relayout = false);
            }
            return 0;
        }
        _ => {}
    }
    let head = format!("{n} {:x} {:x} {:x} -> ", a as i64, b as i64, c as i64);
    if op == Op::NewScrollbarScroller {
        let id = hex(&expect(&head, true));
        with(|r| r.bars.insert(id, true));
        return id;
    }
    let r = with(|r| match op {
        Op::BaseMouseWheel => r.wheel,
        Op::SetSize => {
            r.width = a as i32;
            r.height = b as i32;
            0
        }
        _ => 0,
    });
    expect(&format!("{head}{:x}", r as i64), false);
    r
}

fn words(s: &str) -> Vec<&str> {
    s.split_whitespace().collect()
}

fn num(w: &str) -> i32 {
    w.parse().unwrap_or(0)
}

fn replay(lines: &[String]) -> Option<String> {
    let start = words(&lines[0]);
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.to_vec(),
            at: 1,
            width: num(start[1]),
            height: num(start[2]),
            ..Replay::default()
        })
    });
    let holder = Box::new(EeWonScrollPane::new(0x54 as *mut c_void, Some(replay_host)));
    let t: &EeWonScrollPane = &holder;
    with(|r| r.pane = t as *const EeWonScrollPane as usize);
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
            "relayout" => with(|r| r.relayout = ws[1] == "1"),
            "size" => with(|r| {
                r.width = num(ws[1]);
                r.height = num(ws[2]);
            }),
            "wheel" => with(|r| r.wheel = num(ws[1]) as isize),
            "scroller" => with(|r| {
                r.bars.insert(hex(ws[1]), ws[2] == "1");
            }),
            "call" => {
                let want: Vec<i64> = line
                    .split(" -> ")
                    .nth(1)
                    .unwrap_or("")
                    .split_whitespace()
                    .map(|w| w.parse().unwrap_or(0))
                    .collect();
                let (a, b, c, d) = (hex(ws[2]), hex(ws[3]), hex(ws[4]), hex(ws[5]));
                let (mut r1, mut r2) = (0i64, 0i64);
                match ws[1] {
                    "size" => {
                        t.call(P::SizeChanged, 0, 0, 0);
                    }
                    "padding" => {
                        let p = [a as i32, b as i32, c as i32, d as i32];
                        t.call(P::SetBorderPadding, p.as_ptr() as isize, 0, 0);
                    }
                    "dimension" => {
                        t.call(P::SetScrollDimension, a, b, 0);
                    }
                    "vert-offset" => {
                        t.call(P::SetVertOffset, a, 0, 0);
                    }
                    "offsets" => {
                        t.call(P::SetOffsets, a, b, 0);
                    }
                    "wheel" => r1 = t.call(P::MouseWheel, a, 0, 0) as i64,
                    "adjust" => {
                        t.call(P::AdjustPosition, a, 0, 0);
                    }
                    "set-scroller" => {
                        t.call(P::SetScroller, a, 0, 0);
                    }
                    "set-scrollbar" => {
                        t.call(P::SetScrollbar, a, 0, 0);
                    }
                    "translate" | "untranslate" => {
                        let (mut x, mut y) = (a as i32, b as i32);
                        let call = if ws[1] == "translate" { P::ScrollTranslate } else { P::ScrollUntranslate };
                        t.call(call, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0);
                        r1 = i64::from(x);
                        r2 = i64::from(y);
                    }
                    "fit" => {
                        t.call(P::SetSizeEqualToTotal, a, b, 0);
                    }
                    "desired" => {
                        let (mut w, mut h) = (-1i32, -1i32);
                        t.call(P::DesiredSize, &mut w as *mut i32 as isize, &mut h as *mut i32 as isize, 0);
                        r1 = i64::from(w);
                        r2 = i64::from(h);
                    }
                    "rect-visible" => {
                        let q = [a as i32, b as i32, c as i32, d as i32];
                        r1 = t.call(P::IsRectVisible, q.as_ptr() as isize, 0, 0) as i64;
                    }
                    "paint" => {
                        t.call(P::Paint, a, 0, 0);
                    }
                    "on-bottom" => r1 = t.call(P::OnBottom, 0, 0, 0) as i64,
                    "get-field" => r1 = t.call(P::GetField, a, 0, 0) as i64,
                    "set-field" => {
                        t.call(P::SetField, a, b, 0);
                    }
                    "delegate" => {
                        t.call(P::SetDelegatePos, 0, 0, 0);
                    }
                    name => return Some(format!("unknown call '{name}'")),
                }
                if want != [r1, r2] {
                    return Some(format!("{line}: rust answered {r1} {r2}"));
                }
            }
            _ => return Some(format!("unexpected golden line '{line}'")),
        }
    }
    drop(holder);
    let f = with(|r| r.failure.clone());
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
fn replays_the_original_scroll_area() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_new_area_scrolls_by_ten_and_holds_nothing() {
    let t = EeWonScrollPane::new(std::ptr::null_mut(), None);
    assert_eq!(t.call(P::GetField, F::UpScrollAmount as isize, 0, 0), 10);
    assert_eq!(t.call(P::GetField, F::RightScrollAmount as isize, 0, 0), 10);
    assert_eq!(t.call(P::GetField, F::VertOffset as isize, 0, 0), 0);
    t.call(P::SetField, F::UserHasScrolled as isize, 5, 0);
    assert_eq!(t.call(P::GetField, F::UserHasScrolled as isize, 0, 0), 1);
    assert_eq!(t.call(P::GetField, F::Width as isize, 0, 0), 0); // the size is the C++ component's
}
