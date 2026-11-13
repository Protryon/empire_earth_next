//! The Rust PopupMenu against the original's (`golden_menuobj.txt`, printed by `tests/won/won_menuobj_test.cpp
//! --print` over the original's methods and a scripted host): each sequence is replayed op for op over the Rust menu
//! and its Rust ListArea base. The replay host answers from the golden (every op's answer, the fields, the
//! translated points) and runs the calls back the original's host made (the list's virtuals, the selection hook);
//! every op the Rust objects make must be the golden's next line.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::listobj::{EeWonItemClick, EeWonList, EeWonListCall as L};
use crate::menuobj::{EeWonMenu, EeWonMenuCall as M};

const GOLDEN: &str = include_str!("golden_menuobj.txt");
const MENU: isize = 0x55;

struct Replay {
    lines: Vec<String>,
    at: usize,
    fields: [i32; 16],
    failure: Option<String>,
    list: *const EeWonList,
    menu: *const EeWonMenu,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

fn next() -> String {
    with(|r| {
        let l = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        l
    })
}

fn fail(line: &str, want: &str) {
    with(|r| {
        if r.failure.is_none() {
            r.failure = Some(format!("line {}: golden '{line}', rust '{want}'", r.at));
        }
    });
}

/// the golden's next line, which must start with `want`; the words after it
fn expect(want: &str) -> Vec<String> {
    let line = next();
    if !line.starts_with(want) {
        fail(&line, want);
        return Vec::new();
    }
    line[want.len()..].split_whitespace().map(str::to_string).collect()
}

fn hex(w: &str) -> isize {
    u64::from_str_radix(w, 16).unwrap_or(0) as i64 as isize
}

fn parse_fields(line: &str) -> [i32; 16] {
    let mut f = [0i32; 16];
    for (i, w) in line.split_whitespace().skip(1).enumerate() {
        if i < 16 {
            f[i] = w.parse().unwrap_or(0);
        }
    }
    f
}

fn list() -> &'static EeWonList {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.list) }
}
fn menu() -> &'static EeWonMenu {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.menu) }
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Get => with(|r| r.fields[a as usize] as isize),
        Op::BaseSizeChanged => {
            expect(&format!("{n} base-size"));
            let f = parse_fields(&next());
            with(|r| r.fields = f);
            list().call(L::AdjustPosition, 0, 0, 0);
            0
        }
        Op::BaseAdjustPosition => {
            let w = expect(&format!("{n} base-adjust {a} -> "));
            if w.len() >= 2 {
                with(|r| {
                    r.fields[F::VertOffset as usize] = w[0].parse().unwrap_or(0);
                    r.fields[F::HorzOffset as usize] = w[1].parse().unwrap_or(0);
                });
            }
            0
        }
        Op::ScrollTranslate | Op::ScrollUntranslate | Op::LocalToRoot => {
            // SAFETY: the object passes two live i32s
            let (x, y) = unsafe { (&mut *(a as *mut i32), &mut *(b as *mut i32)) };
            let w = expect(&format!("{n} translate {x} {y} -> "));
            if w.len() == 2 {
                *x = w[0].parse().unwrap_or(0);
                *y = w[1].parse().unwrap_or(0);
            }
            0
        }
        Op::VirtAdjustPosition => {
            expect(&format!("{n} virt-adjust {a}"));
            list().call(L::AdjustPosition, a, 0, 0);
            0
        }
        Op::VirtSort => {
            expect(&format!("{n} virt-sort"));
            let s = list().call(L::Sort, 0, 0, 0);
            expect(&format!("{n} virt-sort -> {s}"));
            s
        }
        Op::VirtNotifyMouseItemGone => {
            expect(&format!("{n} virt-gone"));
            list().call(L::NotifyMouseItemGone, 0, 0, 0);
            0
        }
        Op::VirtListSelChangedHook => {
            expect(&format!("{n} hook"));
            menu().call(M::ListSelChangedHook, 0, 0, 0);
            0
        }
        Op::DeliverItemClicked => {
            // SAFETY: the object passes a live click
            let k = unsafe { *(b as *const EeWonItemClick) };
            expect(&format!("{n} click {a:x} {} {} {} {}", k.button, k.up as i32, k.x, k.y));
            0
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            let want = format!("{n} draw {a:x} {} {} {} {} {}", o.kind, o.a, o.b, o.c, o.d);
            let line = next();
            if line != want {
                fail(&line, &want);
            }
            0
        }
        Op::QueueSubMenu => {
            // SAFETY: the object passes a live point and rectangle
            let (at, q) = unsafe { (std::slice::from_raw_parts(b as *const i32, 2), *(c as *const EeWonRect4)) };
            expect(&format!("{n} queue-sub {a:x} {} {} {d}", at[0], at[1]));
            expect(&format!("{n} avoid {} {} {} {}", q.x, q.y, q.w, q.h));
            0
        }
        _ => {
            let w = expect(&format!("{n} {a:x} {b:x} {c:x} {d:x} -> "));
            match op {
                Op::Set => with(|r| r.fields[a as usize] = b as i32),
                Op::SetVertOffset => with(|r| r.fields[F::VertOffset as usize] = a as i32),
                _ => {}
            }
            w.first().map_or(0, |x| hex(x))
        }
    }
}

fn run(m: &EeWonMenu, l: &EeWonList, words: &[&str]) -> Result<isize, String> {
    let (a, b, c) = (hex(words[1]), hex(words[2]), hex(words[3]));
    Ok(match words[0] {
        "add" => {
            l.call(L::InsertItem, a, -1, 0);
            l.call(L::NumItems, 0, 0, 0)
        }
        "down" => {
            m.call(M::MouseDown, a, b, c);
            m.call(M::MouseUpCount, 0, 0, 0)
        }
        "up" => {
            m.call(M::MouseUp, a, b, c);
            m.call(M::MouseUpCount, 0, 0, 0)
        }
        "move" | "move-same" => {
            m.call(M::MouseMove, a, b, 0);
            m.call(M::KeyboardMode, 0, 0, 0)
        }
        "timer" => m.call(M::TimerEvent, a, 0, 0) * 1000 + m.call(M::TimerAcc, 0, 0, 0),
        "event" => m.call(M::HandleEvent, a, 0, 0),
        "end" => {
            m.call(M::EndSubMenu, 0, 0, 0);
            m.call(M::SubMenu, 0, 0, 0)
        }
        "check" => {
            m.call(M::CheckSubMenu, 0, 0, 0);
            m.call(M::SubMenu, 0, 0, 0)
        }
        "key" => m.call(M::TryKeyDown, a, 0, 0),
        "reset" => m.call(M::Reset, 0, 0, 0),
        "sel" => {
            l.call(L::SetSelItemAt, a, 1, 0);
            l.call(L::SelItem, 0, 0, 0)
        }
        "flags" => l.call(L::SetListFlags, a, b, 0),
        "size" => l.call(L::SizeChanged, 0, 0, 0),
        "query" => {
            const Q: [M; 4] = [M::SubMenu, M::KeyboardMode, M::TimerAcc, M::MouseUpCount];
            m.call(Q[a as usize & 3], 0, 0, 0)
        }
        name => return Err(format!("unknown call '{name}'")),
    })
}

fn replay(lines: &[String]) -> Option<String> {
    let fields = parse_fields(&lines[0]);
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.to_vec(),
            at: 1,
            fields,
            failure: None,
            list: std::ptr::null(),
            menu: std::ptr::null(),
        })
    });
    let list_box = Box::new(EeWonList::new(MENU as *mut c_void, Some(replay_host)));
    let lp: *const EeWonList = &*list_box;
    with(|r| r.list = lp);
    list_box.call(L::SetListFlags, 0x806, 1, 0); // the constructor's list flags
    let menu_box = Box::new(EeWonMenu::new(MENU as *mut c_void, Some(replay_host), lp));
    with(|r| r.menu = &*menu_box);
    let (l, m) = (list(), menu());
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
        let Some(rest) = line.strip_prefix("call ") else {
            return Some(format!("unexpected golden line '{line}'"));
        };
        let mut parts = rest.split(" -> ");
        let words: Vec<&str> = parts.next().unwrap_or("").split_whitespace().collect();
        let want: Option<isize> = parts.next().and_then(|w| w.trim().parse().ok());
        let got = match run(m, l, &words) {
            Ok(g) => g,
            Err(e) => return Some(e),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the menu goes (its submenu's references), then the base (its items)
    drop(menu_box);
    drop(list_box);
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            let l = r.lines.get(r.at).cloned().unwrap_or_default();
            r.failure = Some(format!("line {}: the golden goes on ('{l}')", r.at));
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
fn golden_menuobj_replays() {
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
    let (s, k) = seqs
        .iter()
        .find_map(|s| s.iter().position(|l| l.contains(" queue-sub ")).map(|k| (s, k)))
        .expect("a submenu opened");
    let mut m = s.clone();
    m[k] = m[k].replacen(" queue-sub ", " queue-sub 1", 1);
    assert!(replay(&m).is_some());
}
