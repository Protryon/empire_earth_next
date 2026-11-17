//! The Rust Scroller against the original's (`golden_scrollerobj.txt`, printed by
//! `tests/won/won_scrollerobj_test.cpp --print` over the original Scroller's and ScrollbarScroller's methods and a
//! scripted host): each sequence is replayed op for op over real Rust scroll bars. The replay host keeps the C++
//! host's model (the components, the areas' numbers, the focus child, the wheel's answers, the events) from the
//! golden's driver lines and the traced ops, answers the untraced reads from it, and runs what the C++ virtuals
//! run (a bar's SetEnabled and AddedToParent, the area's AdjustPosition and the scroller's NotifyScrollChange
//! calling the scroller back). The components are blocks (compmem's map world): the Rust reads and moves them in
//! place, the world's SetVisible and a plain component's SetEnabled set their flags as the driver's do.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::barobj::{EeWonBarCall as B, EeWonScrollbar};
use crate::compmem::{self, map_block};
use crate::compobj::{host_world, EeWonComp as K};
use crate::guihost::{ee_won_gui_event_type, EeWonGuiEvent, EeWonGuiField as F, EeWonGuiOp as Op};
use crate::scrollerobj::{EeWonScroller, EeWonScrollerCall as C};

const GOLDEN: &str = include_str!("golden_scrollerobj.txt");
const SCROLLER: isize = 0x54;

#[derive(Clone, Copy, Default)]
struct Comp {
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
    match op {
        Op::Comp if a == K::VPositionChanged as isize || a == K::VSizeChanged as isize => {
            let r = compmem::bounds(b);
            expect(&format!("{n} comp {a} {b:x} at {} {} {} {}", r.x, r.y, r.width, r.height), false);
            return 0;
        }
        Op::Comp if a == K::VSetEnabled as isize => {
            // a bar's own SetEnabled, another's the world's flag
            expect(&format!("{n} comp {a} {b:x} {c}"), false);
            let p = bar_ptr(b);
            if p.is_null() {
                compmem::set_flags(b, 0x80, c == 0);
            } else {
                // SAFETY: a live bar of the replay
                unsafe { (*p).call(B::SetEnabled, c, 0, 0) };
            }
            return 0;
        }
        Op::Comp => {
            let w = expect(&format!("{n} comp {a} {b:x} {c} -> "), true);
            if a == K::VSetVisible as isize {
                compmem::set_flags(b, 0x1, c == 0);
            }
            return hex(&w);
        }
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
        Op::AddChild | Op::AreaAdjustPosition | Op::VirtNotifyScrollChange => {
            expect(&format!("{n} {:x} {:x} {:x}", a as i64, b as i64, c as i64), false);
            match op {
                Op::AddChild => {
                    compmem::set_parent(a, me);
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
            r.comps.insert(id, Comp::default());
        });
        // SAFETY: the map world's block of the new button
        unsafe { *map_block(id) = compmem::EeWonCompMembers::default() };
        return id;
    }
    let result = with(|r| {
        let mut result = 0isize;
        match op {
            Op::ChildHold => r.comps.entry(a).or_default().held += 1,
            Op::ChildRelease => r.comps.entry(a).or_default().held -= 1,
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
    REPLAY.with(|r| {
        let mut rp = Replay { lines: lines.to_vec(), at: 1, ..Replay::default() };
        rp.comps.insert(SCROLLER, Comp::default());
        *r.borrow_mut() = Some(rp);
    });
    let _world = compmem::map_world();
    let _host = host_world(Some(replay_host));
    let mut m = compmem::block(0, 0, num(start[1]), num(start[2]));
    m.bits = num(start[3]);
    // SAFETY: the map world's block of the scroller
    unsafe { *map_block(SCROLLER) = m };
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
            "comp" => {
                let id = hex(ws[1]);
                with(|r| r.comps.insert(id, Comp::default()));
                let mut m = compmem::block(num(ws[2]), num(ws[3]), num(ws[4]), num(ws[5]));
                m.bits = num(ws[6]);
                // SAFETY: the map world's block, no other reference to it live
                unsafe { *map_block(id) = m };
            }
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
            "visible" => compmem::set_flags(hex(ws[1]), 0x1, ws[2] != "1"),
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
