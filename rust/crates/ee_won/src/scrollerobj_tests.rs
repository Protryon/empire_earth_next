//! The Rust Scroller against the original's (`golden_scrollerobj.txt`, printed by
//! `tests/won/won_scrollerobj_test.cpp --print` over the original Scroller's and ScrollbarScroller's methods and a
//! scripted host): each sequence is replayed op for op over real Rust scroll bars. The replay host keeps the C++
//! host's model (the components, the areas' numbers, the focus child, the wheel's answers, the events) from the
//! golden's driver lines and the traced ops, answers the untraced reads from it, and runs what the C++ virtuals
//! run (a bar's SetEnabled and AddedToParent, the area's AdjustPosition and the scroller's NotifyScrollChange
//! calling the scroller back).

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::barobj::{EeWonBarCall as B, EeWonScrollbar};
use crate::guictl::EeWonRect4;
use crate::guihost::{ee_won_gui_event_type, EeWonGuiEvent, EeWonGuiField as F, EeWonGuiOp as Op};
use crate::scrollerobj::{EeWonScroller, EeWonScrollerCall as C};

const GOLDEN: &str = include_str!("golden_scrollerobj.txt");
const SCROLLER: isize = 0x54;

#[derive(Clone, Copy, Default)]
struct Comp {
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    visible: bool,
    disabled: bool,
    parent: isize,
    held: i32,
}

#[derive(Default)]
struct Replay {
    lines: Vec<String>,
    at: usize,
    comps: BTreeMap<isize, Comp>,
    areas: BTreeMap<isize, BTreeMap<u32, i32>>,
    area_scroller: BTreeMap<isize, isize>,
    bars: BTreeMap<isize, Box<EeWonScrollbar>>,
    events: BTreeMap<isize, (isize, isize)>,
    focus: bool,
    wheel: (isize, isize),
    scroller: usize,
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

fn bar_ptr(id: isize) -> *const EeWonScrollbar {
    with(|r| r.bars.get(&id).map_or(std::ptr::null(), |b| &**b as *const EeWonScrollbar))
}

fn scroller() -> *const EeWonScroller {
    with(|r| r.scroller as *const EeWonScroller)
}

unsafe extern "C" fn replay_host(comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let me = comp as isize;
    let n = op as u32;
    let comp_of = |id: isize| with(|r| r.comps.get(&id).copied().unwrap_or_default());
    match op {
        Op::Get => {
            let k = comp_of(me);
            return if a == F::Width as isize {
                k.w
            } else if a == F::Height as isize {
                k.h
            } else {
                0
            } as isize;
        }
        Op::ChildLeft => return comp_of(a).x as isize,
        Op::ChildTop => return comp_of(a).y as isize,
        Op::ChildWidth => return comp_of(a).w as isize,
        Op::ChildHeight => return comp_of(a).h as isize,
        Op::ChildIsVisible => return comp_of(a).visible as isize,
        Op::ChildIsMine => return (comp_of(a).parent == me) as isize,
        Op::IsDisabled => return comp_of(me).disabled as isize,
        Op::MouseChild => return 0,
        Op::EventType => return with(|r| r.events.get(&a).map_or(0, |e| e.0)),
        Op::EventComponent => return with(|r| r.events.get(&a).map_or(0, |e| e.1)),
        Op::GetMousePos => {
            // SAFETY: the object passes two live i32s
            unsafe {
                *(a as *mut i32) = 0;
                *(b as *mut i32) = 0;
            }
            return 0;
        }
        Op::BarOf => return bar_ptr(a) as isize,
        Op::AreaGet => {
            return with(|r| r.areas.get(&a).and_then(|f| f.get(&(b as u32)).copied()).unwrap_or(0)) as isize
        }
        Op::HasFocusChild => return with(|r| r.focus) as isize,
        Op::FireEvent => {
            // SAFETY: the object passes an EeWonGuiEvent's value
            let e: EeWonGuiEvent = unsafe { std::mem::transmute(a as u32) };
            expect(&format!("fire {} from {:x}", ee_won_gui_event_type(e), me as i64), false);
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
        Op::ChildSetEnabled | Op::AddChild | Op::AreaAdjustPosition | Op::VirtNotifyScrollChange => {
            expect(&format!("{n} {:x} {:x} {:x}", a as i64, b as i64, c as i64), false);
            match op {
                Op::ChildSetEnabled => {
                    let p = bar_ptr(a);
                    if p.is_null() {
                        return 0;
                    }
                    // SAFETY: a live bar of the replay
                    unsafe { (*p).call(B::SetEnabled, b, 0, 0) };
                }
                Op::AddChild => {
                    with(|r| r.comps.entry(a).or_default().parent = me);
                    let p = bar_ptr(a);
                    if !p.is_null() {
                        // SAFETY: as above
                        unsafe { (*p).call(B::AddedToParent, 0, 0, 0) };
                    }
                }
                Op::AreaAdjustPosition => {
                    if with(|r| r.area_scroller.get(&a).copied()) == Some(SCROLLER) {
                        // SAFETY: the scroller under test is live during its calls
                        unsafe { (*scroller()).call(C::NotifyScrollChange, 1, 0, 0) };
                    }
                }
                _ => {
                    // SAFETY: as above
                    unsafe { (*scroller()).call(C::NotifyScrollChange, a, 0, 0) };
                }
            }
            return 0;
        }
        _ => {}
    }
    let head = format!("{n} {:x} {:x} {:x} -> ", a as i64, b as i64, c as i64);
    if op == Op::NewButton {
        let id = hex(&expect(&head, true));
        with(|r| {
            r.comps.insert(id, Comp { visible: true, ..Comp::default() });
        });
        return id;
    }
    let result = with(|r| {
        let mut result = 0isize;
        match op {
            Op::ChildSetPos => {
                let k = r.comps.entry(a).or_default();
                k.x = b as i32;
                k.y = c as i32;
            }
            Op::ChildSetSize => {
                let k = r.comps.entry(a).or_default();
                k.w = b as i32;
                k.h = c as i32;
            }
            Op::ChildSetVisible => r.comps.entry(a).or_default().visible = b != 0,
            Op::ChildHold => r.comps.entry(a).or_default().held += 1,
            Op::ChildRelease => r.comps.entry(a).or_default().held -= 1,
            Op::BaseSetEnabled => r.comps.entry(me).or_default().disabled = a == 0,
            Op::AreaSet => {
                r.areas.entry(a).or_default().insert(b as u32, c as i32);
            }
            Op::AreaSetScroller => {
                r.area_scroller.insert(a, me);
            }
            Op::BaseMouseWheel => result = r.wheel.0,
            Op::AreaMouseWheel => result = r.wheel.1,
            _ => {}
        }
        result
    });
    expect(&format!("{head}{:x}", result as i64), false);
    result
}

fn words(s: &str) -> Vec<&str> {
    s.split_whitespace().collect()
}

fn num(w: &str) -> i32 {
    w.parse().unwrap_or(0)
}

const AREA_FIELDS: [F; 11] = [
    F::VertOffset,
    F::HorzOffset,
    F::TotalWidth,
    F::TotalHeight,
    F::ExtraHeight,
    F::ScrollX,
    F::ScrollY,
    F::ScrollWidth,
    F::ScrollHeight,
    F::UpScrollAmount,
    F::DownScrollAmount,
];

fn replay(lines: &[String]) -> Option<String> {
    let start = words(&lines[0]);
    let me = Comp { w: num(start[1]), h: num(start[2]), visible: true, ..Comp::default() };
    REPLAY.with(|r| {
        let mut rp = Replay { lines: lines.to_vec(), at: 1, ..Replay::default() };
        rp.comps.insert(SCROLLER, me);
        *r.borrow_mut() = Some(rp);
    });
    let mut holder = Some(Box::new(EeWonScroller::new(SCROLLER as *mut c_void, Some(replay_host))));
    let t: *const EeWonScroller = &**holder.as_ref().expect("the scroller");
    with(|r| r.scroller = t as usize);
    loop {
        let (line, done) = with(|r| {
            if r.failure.is_some() || r.at >= r.lines.len() {
                return (String::new(), true);
            }
            let l = r.lines[r.at].clone();
            r.at += 1;
            (l, false)
        });
        if done || line == "refs back" {
            break;
        }
        let ws = words(&line);
        match ws[0] {
            "comp" => with(|r| {
                let k = Comp {
                    x: num(ws[2]),
                    y: num(ws[3]),
                    w: num(ws[4]),
                    h: num(ws[5]),
                    visible: ws[6] == "1",
                    ..Comp::default()
                };
                r.comps.insert(hex(ws[1]), k);
            }),
            "area" => with(|r| {
                let f = r.areas.entry(hex(ws[1])).or_default();
                for (i, field) in AREA_FIELDS.iter().enumerate() {
                    f.insert(*field as u32, num(ws[2 + i]));
                }
            }),
            "area-set" => with(|r| {
                r.areas.entry(hex(ws[1])).or_default().insert(num(ws[2]) as u32, num(ws[3]));
            }),
            "bar" => {
                let id = hex(ws[1]);
                let bar = Box::new(EeWonScrollbar::new(id as *mut c_void, Some(replay_host), ws[2] == "1"));
                with(|r| r.bars.insert(id, bar));
            }
            "bar-part" => {
                // SAFETY: a live bar of the replay
                unsafe { (*bar_ptr(hex(ws[1]))).call(B::SetPart, hex(ws[2]), hex(ws[3]), 0) };
            }
            "bar-pos" => {
                // SAFETY: as above
                unsafe { (*bar_ptr(hex(ws[1]))).call(B::SetPosition, num(ws[2]) as isize, 0, 0) };
            }
            "bar-free" => {
                let bar = with(|r| r.bars.remove(&hex(ws[1])));
                drop(bar);
            }
            "event" => with(|r| {
                r.events.insert(hex(ws[1]), (num(ws[2]) as isize, hex(ws[3])));
            }),
            "focus" => with(|r| r.focus = ws[1] == "1"),
            "wheel" => with(|r| r.wheel = (num(ws[1]) as isize, num(ws[2]) as isize)),
            "visible" => with(|r| r.comps.entry(hex(ws[1])).or_default().visible = ws[2] == "1"),
            "free" => {
                drop(holder.take());
                with(|r| r.scroller = 0);
            }
            "call" => {
                let Some(s) = holder.as_ref() else {
                    return Some("a call after the free".to_string());
                };
                let want: Option<isize> = line.split(" -> ").nth(1).and_then(|x| x.trim().parse().ok());
                let (a, b, c) = (hex(ws[2]), hex(ws[3]), hex(ws[4]));
                let call = match ws[1] {
                    "set-area" => C::SetScrollArea,
                    "set-scrollbar" => C::SetScrollbar,
                    "set-bar" => C::SetBar,
                    "added" => C::AddedToParent,
                    "add-controls" => C::AddControls,
                    "conditions" => C::SetScrollbarConditions,
                    "enable" => C::EnableScrollbar,
                    "notify" => C::NotifyScrollChange,
                    "event" => C::HandleEvent,
                    "offsets" => C::SetOffsets,
                    "vert-offset" => C::SetVertOffset,
                    "horz-offset" => C::SetHorzOffset,
                    "down" => C::MouseDown,
                    "wheel" => C::MouseWheel,
                    "get-area" => C::GetScrollArea,
                    "get-bar" => C::GetScrollbar,
                    name => return Some(format!("unknown call '{name}'")),
                };
                let got = s.call(call, a, b, c);
                if want.is_some_and(|w| w != got) {
                    return Some(format!("{line}: rust answered {got}"));
                }
            }
            _ => return Some(format!("unexpected golden line '{line}'")),
        }
    }
    let f = with(|r| {
        if r.failure.is_none() && r.comps.values().any(|k| k.held != 0) {
            r.failure = Some("a reference not given back".to_string());
        }
        r.failure.clone()
    });
    // what is left (nothing after a whole replay) goes while the replay still answers
    drop(holder);
    let bars = with(|r| std::mem::take(&mut r.bars));
    drop(bars);
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
fn replays_the_original_scroller() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "{} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_plain_scroller_has_nothing() {
    let t = EeWonScroller::new(std::ptr::null_mut(), None);
    assert_eq!(t.call(C::GetScrollArea, 0, 0, 0), 0);
    assert_eq!(t.call(C::GetScrollbar, 1, 0, 0), 0);
    assert_eq!(t.call(C::EnableScrollbar, 0, 1, 0), 0); // no bar: nothing changes
    assert_eq!(t.call(C::MouseWheel, 3, 0, 0), 0); // no host answers, no area
}
