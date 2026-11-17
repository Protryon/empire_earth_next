//! The Rust ListArea against the original's (`golden_listobj.txt`, printed by `tests/won/won_listobj_test.cpp
//! --print` over the original ListArea's methods and a scripted host): each sequence is replayed op for op. The
//! replay host answers from the golden (every op's answer, the scroll area's fields, the sort's order, the base
//! class's size change and the list's virtuals calling back into the list) and every op the Rust object makes must
//! be the golden's next line.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::listobj::{EeWonItemClick, EeWonList, EeWonListCall as C};

const GOLDEN: &str = include_str!("golden_listobj.txt");

struct Replay {
    lines: Vec<String>,
    at: usize,
    fields: [i32; 16],
    failure: Option<String>,
    list: *const EeWonList,
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
    // SAFETY: the list is live while the replay runs
    unsafe { &*with(|r| r.list) }
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Get => with(|r| r.fields[a as usize] as isize),
        Op::BaseSizeChanged => {
            expect(&format!("{n} base-size"));
            let f = parse_fields(&next());
            with(|r| r.fields = f);
            list().call(C::AdjustPosition, 0, 0, 0);
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
        Op::ScrollTranslate => {
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
            list().call(C::AdjustPosition, a, 0, 0);
            0
        }
        Op::VirtSort => {
            expect(&format!("{n} virt-sort"));
            let s = list().call(C::Sort, 0, 0, 0);
            expect(&format!("{n} virt-sort -> {s}"));
            s
        }
        Op::VirtNotifyMouseItemGone => {
            expect(&format!("{n} virt-gone"));
            list().call(C::NotifyMouseItemGone, 0, 0, 0);
            0
        }
        Op::DeliverItemClicked => {
            // SAFETY: the object passes a live click
            let k = unsafe { *(b as *const EeWonItemClick) };
            expect(&format!("{n} click {a:x} {} {} {} {}", k.button, k.up as i32, k.x, k.y));
            0
        }
        Op::SortItems => {
            let w = expect(&format!("{n} sort"));
            // SAFETY: the object passes `b` items at `a`
            let v = unsafe { std::slice::from_raw_parts_mut(a as *mut isize, b as usize) };
            for (i, x) in w.iter().enumerate().take(v.len()) {
                v[i] = hex(x);
            }
            0
        }
        Op::LowerBound => {
            let w = expect(&format!("{n} lower-bound {c:x} {d} -> "));
            w.first().and_then(|x| x.parse().ok()).unwrap_or(0)
        }
        Op::FireEvent => {
            // SAFETY: the object fires an event code of the enum
            let e = unsafe { std::mem::transmute::<u32, crate::guihost::EeWonGuiEvent>(a as u32) };
            expect(&format!("{n} fire {}", crate::guihost::ee_won_gui_event_type(e)));
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

fn replay(lines: &[String]) -> Option<String> {
    let fields = parse_fields(&lines[0]);
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay { lines: lines.to_vec(), at: 1, fields, failure: None, list: std::ptr::null() })
    });
    let holder = Box::new(EeWonList::new(0x55 as *mut c_void, Some(replay_host)));
    let t: &EeWonList = &holder;
    with(|r| r.list = t as *const EeWonList);
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
        let (a, b, c) = (hex(words[1]), hex(words[2]), hex(words[3]));
        let got: isize = match words[0] {
            "insert" | "insert-end" | "batch-insert" => t.call(C::InsertItem, a, b, 0),
            "batch" => {
                t.call(C::BeginUpdate, 0, 0, 0);
                // the batch's inserts are the golden's next call lines, then EndUpdate
                loop {
                    let peek = with(|r| r.lines.get(r.at).cloned().unwrap_or_default());
                    let Some(rest) = peek.strip_prefix("call batch-insert ") else { break };
                    with(|r| r.at += 1);
                    let w: Vec<&str> = rest.split(" -> ").next().unwrap_or("").split_whitespace().collect();
                    t.call(C::InsertItem, hex(w[0]), hex(w[1]), 0);
                }
                t.call(C::EndUpdate, 0, 0, 0);
                0
            }
            "refresh" => t.call(C::RefreshItem, a, 0, 0),
            "delete" => t.call(C::DeleteItem, a, 0, 0),
            "remove" => t.call(C::RemoveItem, a, 0, 0),
            "range" => t.call(C::DeleteRange, a, b, 0),
            "clear" => t.call(C::Clear, a, 0, 0),
            "flags" => {
                t.call(C::SetListFlags, a, b, 0);
                t.call(C::ListFlags, 0, 0, 0)
            }
            "sel-at" => {
                t.call(C::SetSelItemAt, a, b, 0);
                t.call(C::SelItem, 0, 0, 0)
            }
            "sel" => t.call(C::SetSelItem, a, b, c),
            "ensure" => t.call(C::EnsureVisible, a, 0, 0),
            "clear-sel" => t.call(C::ClearSelection, a, 0, 0),
            "sel-range" => t.call(C::SelectRange, a, b, 0),
            "find-at" => t.call(C::FindItemAt, a, 0, 0),
            "visible" => t.call(C::IsItemVisible, a, 0, 0),
            "enforce" => t.call(C::EnforceInvariants, 0, 0, 0),
            "walk" => {
                t.call(C::RewindSelections, 0, 0, 0);
                let mut sum: isize = 0;
                while t.call(C::HasMoreSelections, 0, 0, 0) != 0 {
                    sum = sum.wrapping_mul(3).wrapping_add(t.call(C::GetNextSelection, 0, 0, 0));
                }
                sum
            }
            "pos-at-y" => t.call(C::GetItemPosAtYPos, a, 0, 0),
            "selectable" => t.call(C::FindSelectableItem, a, b, c),
            "sort-type" => {
                t.call(C::SetSortType, a, 0, 0);
                t.call(C::SortType, 0, 0, 0)
            }
            "sort" => t.call(C::Sort, 0, 0, 0),
            "size" => t.call(C::SizeChanged, 0, 0, 0),
            "adjust" => t.call(C::AdjustPosition, a, 0, 0),
            "paint" => {
                t.call(C::ScrollPaint, 6, 0, 0);
                t.call(C::PaintWidth, 0, 0, 0)
            }
            "enter" => {
                t.call(C::MouseEnter, a, b, 0);
                t.call(C::MouseItem, 0, 0, 0)
            }
            "exit" => t.call(C::MouseExit, 0, 0, 0),
            "down" => {
                t.call(C::MouseDown, a, b, c);
                t.call(C::SelItem, 0, 0, 0)
            }
            "up" => t.call(C::MouseUp, a, b, c),
            "move" => {
                t.call(C::MouseMove, a, b, 0);
                t.call(C::MouseItem, 0, 0, 0)
            }
            "drag" => t.call(C::MouseDrag, a, b, 0),
            "key" => t.call(C::KeyDown, a, 0, 0),
            "item-pos" => t.call(C::GetItemPos, a, 0, 0),
            "get-item" => t.call(C::GetItem, a, 0, 0),
            "num" => t.call(C::NumItems, 0, 0, 0),
            "gone" => t.call(C::ClearMouseItem, 0, 0, 0),
            name => return Some(format!("unknown call '{name}'")),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the list goes (its references given back) while the replay still answers
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
fn golden_listobj_replays() {
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
        seqs.iter().find_map(|s| s.iter().position(|l| l.contains(" draw 6 ")).map(|k| (s, k))).expect("a draw");
    let mut m = s.clone();
    m[k] = m[k].replacen(" draw 6 ", " draw 7 ", 1);
    assert!(replay(&m).is_some());
}
