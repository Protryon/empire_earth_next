//! The Rust TreeArea and TreeItem against the original's (`golden_treeobj.txt`, printed by
//! `tests/won/won_treeobj_test.cpp --print` over the original units' methods and a scripted host): each sequence is
//! replayed op for op over the Rust objects and their Rust ListArea base. The replay host answers from the golden
//! (every op's answer, the fields, the sort's order, the new rows) and runs the calls back the original's host made
//! (the list's virtuals, the rows' virtuals, a row going with its last reference); every op the Rust objects make
//! must be the golden's next line. A sort's comparisons are std::sort's: the replay takes its order and skips them;
//! the sorted insert's search is std::lower_bound's, run here with the rows' Compare.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::listobj::{EeWonItemClick, EeWonList, EeWonListCall as L};
use crate::treeobj::{EeWonTNode, EeWonTNodeCall as N, EeWonTree, EeWonTreeCall as T};

const GOLDEN: &str = include_str!("golden_treeobj.txt");
const TREE: isize = 0x55;
const G: isize = 6;

struct Replay {
    lines: Vec<String>,
    at: usize,
    fields: [i32; 16],
    failure: Option<String>,
    list: *const EeWonList,
    tree: *const EeWonTree,
    nodes: BTreeMap<isize, *mut EeWonTNode>,
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
fn tree() -> &'static EeWonTree {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.tree) }
}
fn node_call(row: isize, c: N, a: isize, b: isize, cc: isize) -> isize {
    let p = with(|r| r.nodes.get(&row).copied());
    match p {
        // SAFETY: a row lives until its last reference goes (the replay drops it then)
        Some(p) => unsafe { (*p).call(Some(tree()), c, a, b, cc) },
        None => 0,
    }
}
fn is_row(a: isize) -> bool {
    (0x2000..0x3000).contains(&a)
}

fn less(x: isize, y: isize, kind: isize) -> bool {
    let r = node_call(x, N::Compare, y, 0, 0) as i32;
    if kind == 0 {
        r < 0
    } else {
        r > 0
    }
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Get => with(|r| r.fields[a as usize] as isize),
        Op::NodeOf => with(|r| r.nodes.get(&a).map_or(0, |p| *p as isize)),
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
            list().call(L::AdjustPosition, a, 0, 0);
            0
        }
        Op::VirtSort => {
            expect(&format!("{n} virt-sort -> 0"));
            0
        }
        Op::VirtNotifyMouseItemGone => {
            expect(&format!("{n} virt-gone"));
            tree().call(T::NotifyMouseItemGone, 0, 0, 0);
            0
        }
        Op::DeliverItemClicked => {
            // SAFETY: the object passes a live click
            let k = unsafe { *(b as *const EeWonItemClick) };
            expect(&format!("{n} click {a:x} {} {} {} {}", k.button, k.up as i32, k.x, k.y));
            0
        }
        Op::SortItems => {
            expect(&format!("{n} sort-start {c}"));
            // std::sort's comparisons: the order is the golden's
            let want = format!("{n} sort");
            loop {
                let l = next();
                if l.is_empty() {
                    fail("<end>", &want);
                    return 0;
                }
                if l == want || l.starts_with(&format!("{want} ")) {
                    let w: Vec<&str> = l.split_whitespace().skip(2).collect();
                    // SAFETY: the object passes `b` items at `a`
                    let v = unsafe { std::slice::from_raw_parts_mut(a as *mut isize, b as usize) };
                    for (i, x) in w.iter().enumerate().take(v.len()) {
                        v[i] = hex(x);
                    }
                    return 0;
                }
            }
        }
        Op::LowerBound => {
            expect(&format!("{n} lower-bound-start {c:x} {d}"));
            // SAFETY: the object passes `b` items at `a`
            let v = unsafe { std::slice::from_raw_parts(a as *const isize, b as usize) }.to_vec();
            let (mut first, mut count) = (0usize, v.len());
            while count > 0 {
                let step = count / 2;
                let it = first + step;
                if less(v[it], c, d) {
                    first = it + 1;
                    count -= step + 1;
                } else {
                    count = step;
                }
            }
            expect(&format!("{n} lower-bound -> {first}"));
            first as isize
        }
        Op::Less => {
            expect(&format!("{n} less {a:x} {b:x}"));
            let r = less(a, b, c) as isize;
            expect(&format!("{n} {a:x} {b:x} {c:x} {d:x} -> {r:x}"));
            r
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
        Op::NewTreeItem => {
            let w = expect(&format!("{n} {a:x} {b:x} {c:x} {d:x} -> "));
            let id = w.first().map_or(0, |x| hex(x));
            let p = Box::into_raw(Box::new(EeWonTNode::new(id as *mut c_void, Some(replay_host))));
            with(|r| r.nodes.insert(id, p));
            id
        }
        Op::ItemSetPos if is_row(a) => {
            expect(&format!("{n} row-set-pos {a:x} {b}"));
            node_call(a, N::SetPos, 0, b, 0);
            0
        }
        Op::ItemCanSelect if is_row(a) => {
            expect(&format!("{n} row-can-select {a:x} {b} {c}"));
            let r = node_call(a, N::CanSelect, b, c, 0);
            expect(&format!("{n} row-can-select -> {r}"));
            r
        }
        Op::ItemCalcDimensions if is_row(a) => {
            expect(&format!("{n} row-calc {a:x}"));
            node_call(a, N::CalcDimensions, 0, 0, 0);
            0
        }
        Op::ItemSetViewport if is_row(a) => {
            expect(&format!("{n} row-viewport {a:x} {b:x}"));
            node_call(a, N::SetViewport, b, 0, 0);
            0
        }
        Op::ItemPaint if is_row(a) => {
            expect(&format!("{n} row-paint {a:x}"));
            node_call(a, N::Paint, b, 0, 0);
            0
        }
        Op::ItemMouseEnter if is_row(a) => {
            expect(&format!("{n} row-enter {a:x} {b} {c}"));
            node_call(a, N::MouseEnter, b, c, 0);
            0
        }
        Op::ItemMouseExit if is_row(a) => {
            expect(&format!("{n} row-exit {a:x}"));
            node_call(a, N::MouseExit, 0, 0, 0);
            0
        }
        Op::ItemMouseMove if is_row(a) => {
            expect(&format!("{n} row-move {a:x} {b} {c}"));
            node_call(a, N::MouseMove, b, c, 0);
            0
        }
        Op::ItemMouseDrag if is_row(a) => {
            expect(&format!("{n} row-drag {a:x} {b} {c}"));
            node_call(a, N::MouseDrag, b, c, 0);
            0
        }
        Op::ItemMouseDown if is_row(a) => {
            expect(&format!("{n} row-down {a:x} {b} {c} {d}"));
            node_call(a, N::MouseDown, b, c, d);
            0
        }
        Op::ItemMouseUp if is_row(a) => {
            expect(&format!("{n} row-up {a:x} {b} {c} {d}"));
            node_call(a, N::MouseUp, b, c, d);
            0
        }
        Op::ItemRelease => {
            let w = expect(&format!("{n} release {a:x} -> "));
            if w.first().map(String::as_str) == Some("0") && is_row(a) {
                // the row goes: its children's and item's references back (outside the replay's borrow)
                if let Some(p) = with(|r| r.nodes.remove(&a)) {
                    // SAFETY: made by the replay with Box::into_raw, now unreferenced
                    drop(unsafe { Box::from_raw(p) });
                }
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

fn run(t: &EeWonTree, l: &EeWonList, words: &[&str]) -> Result<isize, String> {
    let (a, b, c) = (hex(words[1]), hex(words[2]), hex(words[3]));
    const Q: [N; 6] = [N::Item, N::NumChildren, N::Parent, N::IsOpen, N::Indent, N::Index];
    let some = |f: &dyn Fn() -> isize| if a != 0 { f() } else { 0 };
    Ok(match words[0] {
        "init-sort" => t.call(T::SetSortType, a, 0, 0),
        "insert" => t.call(T::InsertTreeItem, a, b, c),
        "remove" => some(&|| t.call(T::RemoveTreeItem, a, 0, 0)),
        "remove-all" => t.call(T::RemoveAllItems, a, 0, 0),
        "open" => some(&|| t.call(T::OpenItem, a, b, 0)),
        "close" => some(&|| t.call(T::CloseItem, a, b, c)),
        "toggle" => some(&|| t.call(T::ToggleItem, a, b, c)),
        "sort-type" => {
            t.call(T::SetSortType, a, 0, 0);
            t.call(T::SortType, 0, 0, 0)
        }
        "sort" => t.call(T::SortItem, a, b, 0),
        "flags" => {
            t.call(T::SetTreeFlags, a, b, 0);
            t.call(T::TreeFlags, 0, 0, 0)
        }
        "event" => t.call(T::HandleEvent, a, 0, 0),
        "enter" => {
            l.call(L::MouseEnter, a, b, 0);
            t.call(T::MouseColItem, 0, 0, 0)
        }
        "move" => {
            l.call(L::MouseMove, a, b, 0);
            t.call(T::MouseColItem, 0, 0, 0)
        }
        "down" => {
            l.call(L::MouseDown, a, b, c);
            t.call(T::MouseColItem, 0, 0, 0)
        }
        "up" => l.call(L::MouseUp, a, b, c),
        "drag" => l.call(L::MouseDrag, a, b, 0),
        "exit" => {
            l.call(L::MouseExit, 0, 0, 0);
            t.call(T::MouseColItem, 0, 0, 0)
        }
        "paint" => l.call(L::ScrollPaint, G, 0, 0),
        "size" => l.call(L::SizeChanged, 0, 0, 0),
        "refresh" => some(&|| l.call(L::RefreshItem, a, 0, 0)),
        "list-flags" => l.call(L::SetListFlags, a, b, 0),
        "child" => some(&|| node_call(a, N::GetChild, b, 0, 0)),
        "next" => some(&|| node_call(a, N::GetNextNonChild, 0, 0, 0)),
        "next-pos" => some(&|| node_call(a, N::GetNextNonChildPos, 0, 0, 0)),
        "last" => some(&|| node_call(a, N::IsLastChild, 0, 0, 0)),
        "contains" => some(&|| node_call(a, N::ContainsPoint, b, c, 0)),
        "close-all" => some(&|| node_call(a, N::CloseAll, 0, 0, 0)),
        "query" => some(&|| node_call(a, Q[b as usize % 6], 0, 0, 0)),
        "gone" => {
            l.call(L::ClearMouseItem, 0, 0, 0);
            t.call(T::NotifyMouseItemGone, 0, 0, 0)
        }
        "sel" => some(&|| l.call(L::SetSelItem, a, 0, 1)),
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
            tree: std::ptr::null(),
            nodes: BTreeMap::new(),
        })
    });
    let list_box = Box::new(EeWonList::new(TREE as *mut c_void, Some(replay_host)));
    let lp: *const EeWonList = &*list_box;
    let tree_box = Box::new(EeWonTree::new(TREE as *mut c_void, Some(replay_host), lp));
    with(|r| {
        r.list = lp;
        r.tree = &*tree_box;
    });
    let (l, t) = (list(), tree());
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
        if words[0] == "new" {
            t.call(T::Init, 0, 0, 0);
            continue;
        }
        let got = match run(t, l, &words) {
            Ok(g) => g,
            Err(e) => return Some(e),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the tree goes (its root and so every row's references), then the base (its rows)
    drop(tree_box);
    drop(list_box);
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            let l = r.lines.get(r.at).cloned().unwrap_or_default();
            r.failure = Some(format!("line {}: the golden goes on ('{l}')", r.at));
        }
        if r.failure.is_none() && !r.nodes.is_empty() {
            r.failure = Some(format!("{} rows left", r.nodes.len()));
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
fn golden_treeobj_replays() {
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
        .find_map(|s| s.iter().position(|l| l.contains(" draw 6 17 ")).map(|k| (s, k)))
        .expect("a dotted line");
    let mut m = s.clone();
    m[k] = m[k].replacen(" draw 6 17 ", " draw 6 17 1", 1);
    assert!(replay(&m).is_some());
}
