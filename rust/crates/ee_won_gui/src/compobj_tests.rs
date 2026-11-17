//! compobj's Component: the golden of the original Component statements (`golden_compobj.txt`, recorded by
//! tests/won/won_compobj_test.cpp `--print`, the C++ before the port) replayed over the driver's world rebuilt in
//! Rust (its traced components, windows, events, listeners and grabbers; an object's pointer is its id), and unit
//! tests of the corners.

use super::*;
use crate::compmem::{ee_won_comp_members_table, EeWonCompMembers};
use crate::guihost::EeWonGuiOp;
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Debug, Default)]
struct Extra {
    listener: isize,
    cursor: isize,
    scheme: isize,
    grab: isize,
    tip: isize,
    dont: bool,
}

/// A component: its members' block first (the table compobj reads them through), then the world's own state.
#[repr(C)]
#[derive(Clone, Debug, Default)]
struct Cmp {
    m: EeWonCompMembers,
    modal: bool,
    extra: Option<Extra>,
}

#[derive(Clone, Debug, Default)]
struct Ev {
    refs: i32,
    broadcast: bool,
    comp: isize,
}

#[derive(Debug, Default)]
struct World {
    comps: Vec<Cmp>,
    root: bool,
    fonts: [(i32, i32); 2],
    /// windows 1..3: state, mouse x, mouse y, key modifiers
    win: [(i32, i32, i32, i32); 4],
    win1_parent: bool,
    win2_root: isize,
    wm_scheme: [bool; 4],
    ids: BTreeSet<i64>,
    events: BTreeMap<isize, Ev>,
    /// the events the world holds (its list, in order)
    held: Vec<isize>,
    next_ev: isize,
    next_grab: isize,
    trace: Vec<String>,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

fn t(s: String) {
    W.with(|w| w.borrow_mut().trace.push(s));
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    W.with(|w| f(&mut w.borrow_mut()))
}

fn comp() -> Comp {
    Comp::new(Some(host))
}

fn base(c: isize, k: EeWonCompCall, a: isize, b: isize) -> isize {
    comp().call(c, k, a, b, 0)
}

fn idx(c: isize) -> usize {
    (c - 1) as usize
}

fn extra_of(w: &mut World, b: isize) -> &mut Extra {
    w.comps[idx(b)].extra.as_mut().expect("compobj: extra data read before it was made")
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonGuiOp, k: isize, b: isize, c: isize, d: isize) -> isize {
    assert_eq!(op, EeWonGuiOp::Comp);
    // SAFETY: the op's discriminant from compobj
    let k: EeWonComp = unsafe { std::mem::transmute(k as u32) };
    use EeWonComp as K;
    match k {
        K::AllocExtra => {
            if with(|w| w.comps[idx(b)].extra.is_none()) {
                t(format!("alloc extra {}", b));
                with(|w| {
                    w.comps[idx(b)].extra = Some(Extra::default());
                    w.comps[idx(b)].m.extra = 1;
                });
            }
            0
        }
        K::Extra | K::SetExtra => {
            // SAFETY: compobj's member names
            let e: EeWonCompExtra = unsafe { std::mem::transmute(c as u32) };
            let set = k == K::SetExtra;
            if set && e == EeWonCompExtra::GrabBG {
                let id = with(|w| {
                    w.next_grab += 1;
                    6000 + w.next_grab
                });
                t(format!("new grab {}", id));
                with(|w| extra_of(w, b).grab = id);
            }
            with(|w| {
                let x = extra_of(w, b);
                match e {
                    EeWonCompExtra::Listener => {
                        if set {
                            x.listener = d;
                        }
                        x.listener
                    }
                    EeWonCompExtra::Cursor => {
                        if set {
                            x.cursor = d;
                        }
                        x.cursor
                    }
                    EeWonCompExtra::Scheme => {
                        if set {
                            x.scheme = d;
                        }
                        x.scheme
                    }
                    EeWonCompExtra::GrabBG => x.grab,
                    EeWonCompExtra::ToolTip => x.tip,
                    EeWonCompExtra::DontDoTip => {
                        if set {
                            x.dont = d != 0;
                        }
                        x.dont as isize
                    }
                }
            })
        }
        K::VPositionChanged => {
            t(format!("{} positionchanged", b));
            0
        }
        K::VSizeChanged => {
            t(format!("{} sizechanged", b));
            base(b, EeWonCompCall::SizeChanged, 0, 0)
        }
        K::VDesiredSize => {
            t(format!("{} desired", b));
            let p = c as *mut i32;
            if b & 1 != 0 {
                // SAFETY: compobj's two ints
                base(b, EeWonCompCall::DesiredSize, p as isize, unsafe { p.add(1) } as isize);
            } else {
                // SAFETY: compobj's two ints
                unsafe {
                    *p = 10 + 3 * b as i32;
                    *p.add(1) = 7 + b as i32;
                }
            }
            0
        }
        K::VHandleEvent => {
            t(format!("{} handle ev{}", b, c));
            base(b, EeWonCompCall::HandleEvent, c, 0)
        }
        K::VHandleBroadcast => {
            t(format!("{} handlebroadcast ev{}", b, c));
            base(b, EeWonCompCall::HandleBroadcast, c, 0)
        }
        K::VInvalidateRect => {
            // SAFETY: compobj's rectangle
            let r = unsafe { *(c as *const EeWonRect) };
            t(format!("{} invalidaterect {} {} {} {}", b, r.x, r.y, r.width, r.height));
            base(b, EeWonCompCall::InvalidateRect, c, 0)
        }
        K::VInvalidate => {
            t(format!("{} invalidate", b));
            base(b, EeWonCompCall::Invalidate, 0, 0)
        }
        K::VMarkInvalid => {
            t(format!("{} markinvalid", b));
            base(b, EeWonCompCall::MarkInvalid, 0, 0)
        }
        K::VGotFocus => {
            t(format!("{} gotfocus", b));
            base(b, EeWonCompCall::GotFocus, 0, 0)
        }
        K::VLostFocus => {
            t(format!("{} lostfocus", b));
            base(b, EeWonCompCall::LostFocus, 0, 0)
        }
        K::VNotifyRootVisibility => {
            t(format!("{} rootvisibility {}", b, (c != 0) as i32));
            base(b, EeWonCompCall::NotifyRootVisibility, (c != 0) as isize, 0)
        }
        K::VSetEnabled => {
            t(format!("{} setenabled {}", b, (c != 0) as i32));
            base(b, EeWonCompCall::SetEnabled, (c != 0) as isize, 0)
        }
        K::VEnabledChanged => {
            t(format!("{} enabledchanged {}", b, (c != 0) as i32));
            0
        }
        K::VSetCursor => {
            t(format!("{} setcursor {}", b, c));
            base(b, EeWonCompCall::SetCursor, c, 0)
        }
        K::PNextChild => {
            let (f, wr) = (d & 1 != 0, d & 2 != 0);
            t(format!("{} nextchild {} {} {}", b, c, f as i32, wr as i32));
            let n = with(|w| w.comps.len()) as isize;
            let i = c - 1 + if f { 1 } else { n - 1 };
            if !wr && i >= n && f {
                0
            } else {
                i % n + 1
            }
        }
        K::PSetFocusChild => {
            t(format!("{} setfocuschild {}", b, c));
            0
        }
        K::PAddTimerChild => {
            t(format!("{} addtimerchild {}", b, c));
            0
        }
        K::PChildStateChanged => {
            t(format!("{} childstate {}", b, c));
            0
        }
        K::PChildInvalidated => {
            t(format!("{} childinvalidated {}", b, c));
            1
        }
        K::PIsModal => with(|w| w.comps[idx(b)].modal as isize),
        K::PEnsureOnTopOf => {
            t(format!("{} ensureontop {} {}", b, c, d));
            0
        }
        K::AsRoot => {
            if b == 1 && with(|w| w.root) {
                b
            } else {
                0
            }
        }
        K::RootWin => 1001,
        K::RootAddBG => {
            t(format!("{} addbg grab {}", b, c));
            0
        }
        K::DefaultWindow => 1003,
        K::WState => with(|w| w.win[(b - 1000) as usize].0 as isize),
        K::WBroadcast => {
            t(format!("wm{} broadcast ev{}", b - 1000, c));
            0
        }
        K::WQueue => {
            t(format!("w{} queue ev{}", b, c));
            with(|w| w.events.get_mut(&c).unwrap().refs += 1);
            0
        }
        K::WParent => {
            if b == 1001 && with(|w| w.win1_parent) {
                1002
            } else {
                0
            }
        }
        K::WRoot => {
            if b == 1002 {
                with(|w| w.win2_root)
            } else {
                0
            }
        }
        K::WMouse => {
            let (_, x, y, _) = with(|w| w.win[(b - 1000) as usize]);
            // SAFETY: compobj's two ints
            unsafe {
                *(c as *mut i32) = x;
                *(c as *mut i32).add(1) = y;
            }
            0
        }
        K::WOpenTip => {
            // SAFETY: compobj's four values
            let p = unsafe { std::slice::from_raw_parts(c as *const isize, 4) };
            t(format!("w{} open {} 6247 {} {} {}", b, p[0], p[1], p[2], p[3]));
            0
        }
        K::WClosePopup => {
            t(format!("w{} close {}", b, c));
            0
        }
        K::WKeyMod => with(|w| w.win[(b - 1000) as usize].3 as isize),
        K::WDoubleClick => {
            t(format!("w{} dblclick {}", b, c));
            (c == 1) as isize
        }
        K::WSubscribe => {
            t(format!("wm{} subscribe {} {}", b - 1000, c, (d != 0) as i32));
            0
        }
        K::WScheme => {
            if with(|w| w.wm_scheme[(b - 1000) as usize]) {
                5004
            } else {
                0
            }
        }
        K::WFontMod => {
            let f = with(|w| w.fonts[(c - 7901) as usize]);
            // SAFETY: compobj's two ints
            let p = unsafe { std::slice::from_raw_parts(d as *const i32, 2) };
            let style = p[0];
            let size = if p[1] != 0 { p[1] } else { f.1 };
            t(format!("w{} font {} {}", b, style, size));
            7000 + style.wrapping_mul(50) as isize + (size & 31) as isize + (b - 1000) * 1000
        }
        K::EvHold => with(|w| {
            w.events.get_mut(&b).unwrap().refs += 1;
            0
        }),
        K::EvRelease => {
            let gone = with(|w| {
                let e = w.events.get_mut(&b).unwrap();
                e.refs -= 1;
                e.refs <= 0
            });
            if gone {
                t(format!("delete ev{}", b));
                with(|w| w.events.remove(&b));
            }
            0
        }
        K::EvBroadcast => with(|w| w.events[&b].broadcast as isize),
        K::EvSetBroadcast => with(|w| {
            w.events.get_mut(&b).unwrap().broadcast = c != 0;
            0
        }),
        K::EvComp => with(|w| w.events[&b].comp),
        K::EvSetComp => {
            t(format!("ev{} setcomp {}", b, c));
            with(|w| w.events.get_mut(&b).unwrap().comp = c);
            0
        }
        K::EvNew => new_event(b, c as i32),
        K::LHandle => {
            t(format!("listener {} ev{}", b, c));
            0
        }
        K::GSizeChanged => {
            t(format!("grab {} sizechanged {}", b, c));
            0
        }
        K::GPrePaint => {
            t(format!("grab {} prepaint {} g{}", b, c, d));
            0
        }
        K::GPaint => {
            t(format!("grab {} paint {} g{}", b, c, d));
            0
        }
        K::GClear => {
            t(format!("grab {} clear", b));
            0
        }
        K::GInvalidate => {
            t(format!("grab {} invalidate {}", b, c));
            0
        }
        K::GNeedDrawBG => b & 1,
        K::GTranslucent => {
            t(format!("grab {} translucent {} {}", b, c as i32, d as i32));
            0
        }
        K::IdIn => with(|w| w.ids.contains(&(c as i64)) as isize),
        // (the other Rust objects' calls on a component: compobj itself never makes them)
        K::VSetVisible | K::VMouseChild => unreachable!("compobj makes no {k:?}"),
    }
}

fn new_event(c: isize, kind: i32) -> isize {
    let id = with(|w| {
        w.next_ev += 1;
        2000 + w.next_ev
    });
    t(format!("new ev{} {} type {}", id, c, kind));
    with(|w| w.events.insert(id, Ev { refs: 0, broadcast: false, comp: c }));
    id
}

fn nums(s: &str) -> Vec<i64> {
    s.split_whitespace().map(|x| x.parse().unwrap()).collect()
}

/// One sequence's world from its W, C, P and X lines.
fn build(lines: &[&str]) -> World {
    let w0 = nums(&lines[0][2..]);
    let n = w0[0] as usize;
    let mut w = World {
        root: w0[1] != 0,
        fonts: [(w0[2] as i32, w0[3] as i32), (w0[4] as i32, w0[5] as i32)],
        ..Default::default()
    };
    for l in &lines[1..=n] {
        let v = nums(&l[2..]);
        let mut c = Cmp {
            m: EeWonCompMembers {
                parent: if v[0] < 0 { 0 } else { v[0] as isize + 1 },
                bounds: EeWonRect::new(v[1] as i32, v[2] as i32, v[3] as i32, v[4] as i32),
                bits: v[5] as i32,
                control_id: v[6] as i32,
                invalid: v[7] as i8,
                timer: v[8] as i8,
                extra: (v[10] != 0) as isize,
                ..Default::default()
            },
            modal: v[9] != 0,
            extra: None,
        };
        if v[10] != 0 {
            c.extra = Some(Extra {
                listener: if v[11] != 0 { 3000 + v[11] as isize } else { 0 },
                cursor: if v[12] != 0 { 4000 + v[12] as isize } else { 0 },
                scheme: if v[13] != 0 { 5000 + v[13] as isize } else { 0 },
                grab: v[14] as isize,
                tip: 0,
                dont: v[15] != 0,
            });
            if v[14] != 0 {
                w.next_grab += 1;
            }
        }
        w.comps.push(c);
    }
    for l in &lines[n + 1..] {
        let v = nums(&l[2..]);
        if l.starts_with("P ") {
            w.comps[v[0] as usize].extra.as_mut().unwrap().tip = v[1] as isize + 1;
        } else {
            w.win[1] = (v[0] as i32, v[1] as i32, v[2] as i32, v[3] as i32);
            w.win[2] = (0, v[4] as i32, v[5] as i32, v[6] as i32);
            w.win[3] = (0, v[7] as i32, v[8] as i32, v[9] as i32);
            w.win1_parent = v[10] != 0;
            w.win2_root = if v[11] < 0 { 0 } else { v[11] as isize + 1 };
            w.wm_scheme = [false, v[12] != 0, v[13] != 0, v[14] != 0];
            w.ids = v[15..].iter().copied().collect();
        }
    }
    w
}

fn dump() -> String {
    with(|w| {
        let mut out: Vec<String> = Vec::new();
        for (i, c) in w.comps.iter().enumerate() {
            let x = match &c.extra {
                None => -1,
                Some(e) => e.listener + e.cursor * 7 + e.scheme * 13 + e.grab * 17 + e.tip * 19 + e.dont as isize,
            };
            out.push(format!(
                "{}:{},{},{},{},{:x},{},{},{}",
                i + 1,
                c.m.bounds.x,
                c.m.bounds.y,
                c.m.bounds.width,
                c.m.bounds.height,
                c.m.bits as u32,
                c.m.invalid,
                c.m.timer,
                x
            ));
        }
        let mut s = out.join(" ");
        for id in &w.held {
            let e = &w.events[id];
            s += &format!(" ev{}:{},{},{}", id, e.refs, e.broadcast as i32, e.comp);
        }
        s
    })
}

/// The driver's `Call` over the Rust world (the world's overrides first where the driver's call was virtual): the
/// result line.
fn run(k: EeWonCompCall, c: isize, a: i32, b: i32, cc: i32, dd: i32) -> String {
    use EeWonCompCall as C;
    let p = comp();
    let n = with(|w| w.comps.len()) as u32;
    let mut extra = String::new();
    let mut r: isize = 0;
    match k {
        C::FireBroadcast | C::BroadcastDown | C::Deliver | C::Bubble | C::HandleEvent | C::HandleBroadcast => {
            let ev = if a == 0 || with(|w| w.held.is_empty()) {
                let owner = if dd & 2 != 0 { 0 } else { ((b as u32) % n) as isize + 1 };
                let ev = new_event(owner, 7);
                with(|w| {
                    let e = w.events.get_mut(&ev).unwrap();
                    e.broadcast = cc & 1 != 0;
                    e.refs += 1;
                    w.held.push(ev);
                });
                ev
            } else {
                with(|w| w.held[((a as u32) % w.held.len() as u32) as usize])
            };
            let drop = dd & 1 != 0;
            if drop {
                with(|w| {
                    let i = w.held.iter().position(|&x| x == ev).unwrap();
                    w.held.remove(i);
                    w.events.get_mut(&ev).unwrap().refs -= 1;
                });
            }
            let now = cc & 2 != 0;
            match k {
                C::FireBroadcast => p.fire_broadcast(c, ev, now),
                C::BroadcastDown => p.broadcast_down(c, ev),
                C::Deliver => p.deliver(c, ev, now),
                C::Bubble => p.bubble(c, ev),
                C::HandleEvent => {
                    t(format!("{} handle ev{}", c, ev));
                    p.bubble(c, ev);
                }
                _ => {
                    t(format!("{} handlebroadcast ev{}", c, ev));
                    p.call(c, C::HandleBroadcast, ev, 0, 0);
                }
            }
            extra = format!(" ev{}", ev);
            if drop && with(|w| w.events.get(&ev).map(|e| e.refs <= 0).unwrap_or(false)) {
                with(|w| w.events.remove(&ev));
                t("(unheld)".to_string());
            }
        }
        C::DesiredSize => {
            let (mut x, mut y) = (-1i32, -1i32);
            t(format!("{} desired", c));
            if c & 1 != 0 {
                p.call(c, k, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0);
            } else {
                x = 10 + 3 * c as i32;
                y = 7 + c as i32;
            }
            extra = format!(" {} {}", x, y);
        }
        C::InvalidateRect => {
            let mut rr = EeWonRect::new(a, b, cc, dd);
            t(format!("{} invalidaterect {} {} {} {}", c, a, b, cc, dd));
            p.invalidate_rect(c, &mut rr);
            extra = format!(" {} {} {} {}", rr.x, rr.y, rr.width, rr.height);
        }
        C::AddedToParent => {
            if with(|w| w.comps[idx(c)].m.parent) != 0 {
                p.added_to_parent(c);
            }
        }
        C::MouseHover => {
            const E: [u32; 8] = [0, 499, 500, 501, 5499, 5500, 9000, 0xffffffff];
            p.mouse_hover(c, E[(a as u32 % 8) as usize]);
        }
        C::SetCursor | C::SetColorScheme | C::AddListener | C::SetDefaultCursor => {
            let i = (a as u32 % 4) as isize;
            let base = match k {
                C::SetColorScheme => 5000,
                C::AddListener => 3000,
                _ => 4000,
            };
            let v = if i != 0 { base + i } else { 0 };
            if k == C::SetCursor {
                t(format!("{} setcursor {}", c, v));
            }
            p.call(c, k, v, 0, 0);
        }
        C::LocalToRoot | C::RootToChild | C::MousePos => {
            let (mut x, mut y) = (a, b);
            p.call(c, k, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0);
            extra = format!(" {} {}", x, y);
        }
        C::SetPosSize => {
            let s = [cc, dd];
            p.call(c, k, a as isize, b as isize, s.as_ptr() as isize);
        }
        C::Intersects | C::EnsureOnTopOf => r = p.call(c, k, ((a as u32) % n) as isize + 1, 0, 0),
        C::FontMod => r = p.call(c, k, 7901 + (a & 1) as isize, b as isize, cc as isize),
        C::DoubleClick => r = p.call(c, k, (a & 3) as isize, 0, 0),
        C::TabFocus => r = p.call(c, k, (a & 1) as isize, (b & 1) as isize, 0),
        C::NotifyRootVisibility | C::SetEnabled | C::SetVisible | C::Subscribe | C::RequestTimer | C::NextSibling => {
            match k {
                C::NotifyRootVisibility => t(format!("{} rootvisibility {}", c, a & 1)),
                C::SetEnabled => t(format!("{} setenabled {}", c, a & 1)),
                _ => {}
            }
            r = p.call(c, k, (a & 1) as isize, (b & 1) as isize, 0);
        }
        C::EnableControlsById => r = p.call(c, k, 9000, (a & 1) as isize, 0),
        C::FireEvent | C::SetFlags => r = p.call(c, k, a as isize, (b & 1) as isize, 0),
        C::PrePaint | C::Paint => r = p.call(c, k, 8001, 0, 0),
        _ => {
            match k {
                C::SizeChanged => t(format!("{} sizechanged", c)),
                C::GotFocus => t(format!("{} gotfocus", c)),
                C::LostFocus => t(format!("{} lostfocus", c)),
                C::Invalidate => t(format!("{} invalidate", c)),
                C::MarkInvalid => t(format!("{} markinvalid", c)),
                _ => {}
            }
            r = p.call(c, k, a as isize, b as isize, 0);
        }
    }
    format!("ret {}{}", r, extra)
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_compobj.txt");
    let lines: Vec<&str> = text.lines().collect();
    let mut i = 0;
    let mut seqs = 0;
    let mut ops = 0;
    while i < lines.len() {
        assert!(lines[i].starts_with("S "), "line {}: {}", i + 1, lines[i]);
        let start = i + 1;
        let mut j = start;
        while !lines[j].starts_with("O ") {
            j += 1;
        }
        let w = build(&lines[start..j]);
        install(w);
        i = j;
        while i < lines.len() && lines[i].starts_with("O ") {
            let v = nums(&lines[i][2..]);
            // SAFETY: the driver's call numbers
            let k: EeWonCompCall = unsafe { std::mem::transmute(v[0] as u32) };
            with(|w| w.trace.clear());
            let res = run(k, v[1] as isize + 1, v[2] as i32, v[3] as i32, v[4] as i32, v[5] as i32);
            let want_res = &lines[i + 1][2..];
            let mut j = i + 2;
            let mut want = Vec::new();
            while lines[j].starts_with("T ") {
                want.push(&lines[j][2..]);
                j += 1;
            }
            let got = with(|w| w.trace.clone());
            assert_eq!(res, want_res, "line {}", i + 1);
            assert_eq!(got, want, "line {}", i + 1);
            assert_eq!(dump(), &lines[j][2..], "line {}", i + 1);
            i = j + 1;
            ops += 1;
        }
        seqs += 1;
    }
    assert_eq!(seqs, 300);
    assert_eq!(ops, 300 * 40);
}

fn one(comps: Vec<Cmp>) {
    install(World { comps, ..Default::default() });
}

/// The world installed, its components' blocks the table compobj reads (the Vec is not grown after).
fn install(w: World) {
    W.with(|x| {
        let mut x = x.borrow_mut();
        *x = w;
        let n = x.comps.len() as isize;
        ee_won_comp_members_table(x.comps.as_mut_ptr() as *mut EeWonCompMembers, 1, n, std::mem::size_of::<Cmp>());
    });
}

fn cmp(m: EeWonCompMembers) -> Cmp {
    Cmp { m, ..Default::default() }
}

#[test]
fn request_timer_keeps_the_other_bits() {
    one(vec![
        cmp(EeWonCompMembers { timer: 1, ..Default::default() }),
        cmp(EeWonCompMembers { parent: 1, timer: -2, ..Default::default() }),
    ]);
    let p = comp();
    p.request_timer(1, true);
    p.request_timer(2, false);
    with(|w| {
        assert_eq!(w.comps[0].m.timer, 3);
        assert_eq!(w.comps[1].m.timer, -4);
        // no parent for 1; 2's need did not change (-2 and -4 are both non-zero)
        assert!(w.trace.is_empty());
    });
    p.request_timer(2, true);
    with(|w| assert_eq!(w.comps[1].m.timer, -2));
}

#[test]
fn deliver_to_a_closed_window_deletes_an_unheld_event() {
    one(vec![Cmp::default()]);
    with(|w| {
        w.root = true;
        w.win[1].0 = 1;
    });
    let p = comp();
    let ev = new_event(0, 3);
    p.deliver(1, ev, true);
    with(|w| {
        assert_eq!(w.trace, vec!["new ev2001 0 type 3", "delete ev2001"]);
        assert!(w.events.is_empty());
    });
}

#[test]
fn hover_closes_the_tip_after_the_display_time() {
    assert_eq!(EE_WON_COMP_TIP_START.wrapping_add(EE_WON_COMP_TIP_END), 5500);
    one(vec![Cmp {
        m: EeWonCompMembers { extra: 1, ..Default::default() },
        extra: Some(Extra { tip: 1, dont: true, ..Default::default() }),
        ..Default::default()
    }]);
    let p = comp();
    p.mouse_hover(1, 5499);
    with(|w| assert!(w.trace.is_empty()));
    p.mouse_hover(1, 5500);
    with(|w| assert_eq!(w.trace, vec!["w1003 close 1"]));
}

#[test]
fn set_size_clamps_and_skips_the_same_size() {
    one(vec![cmp(EeWonCompMembers { bounds: EeWonRect::new(0, 0, 5, 5), ..Default::default() })]);
    let p = comp();
    p.set_size(1, -3, 5);
    with(|w| {
        assert_eq!((w.comps[0].m.bounds.width, w.comps[0].m.bounds.height), (0, 5));
        assert_eq!(w.trace, vec!["1 sizechanged"]);
        w.trace.clear();
    });
    p.set_size(1, 0, 5);
    with(|w| assert!(w.trace.is_empty()));
}

#[test]
fn contains_excludes_the_right_and_bottom_edges() {
    one(vec![cmp(EeWonCompMembers { bounds: EeWonRect::new(10, 20, 5, 4), ..Default::default() })]);
    let p = comp();
    assert!(p.contains(1, 10, 20));
    assert!(p.contains(1, 14, 23));
    assert!(!p.contains(1, 15, 20));
    assert!(!p.contains(1, 10, 24));
    assert!(!p.contains(1, 9, 20));
}
