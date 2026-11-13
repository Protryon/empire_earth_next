//! The Rust MultiListArea, its rows and header against the original's (`golden_multiobj.txt`, printed by
//! `tests/won/won_multiobj_test.cpp --print` over the original units' methods and a scripted host): each sequence is
//! replayed op for op over the Rust objects and their Rust ListArea base. The replay host answers from the golden
//! (every op's answer, the fields, the sort's order) and runs the calls back the original's host made (the list's
//! virtuals, the rows' virtuals, the header's methods, a row going with its last reference); every op the Rust objects
//! make must be the golden's next line. A sort's comparisons are std::sort's: the replay takes its order and skips
//! them; the sorted insert's search is std::lower_bound's, run here with the rows' Compare.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::listobj::{EeWonItemClick, EeWonList, EeWonListCall as L};
use crate::multiobj::{
    EeWonMHead, EeWonMHeadCall as H, EeWonMRow, EeWonMRowCall as R, EeWonMulti, EeWonMultiCall as M,
};

const GOLDEN: &str = include_str!("golden_multiobj.txt");
const LIST: isize = 0x55;
const HEAD: isize = 0x66;
const G: isize = 6;

struct Replay {
    lines: Vec<String>,
    at: usize,
    fields: [i32; 16],
    head_w: i32,
    head_h: i32,
    failure: Option<String>,
    list: *const EeWonList,
    multi: *const EeWonMulti,
    head: *const EeWonMHead,
    rows: BTreeMap<isize, *mut EeWonMRow>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<T>(f: impl FnOnce(&mut Replay) -> T) -> T {
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

fn parse_fields(line: &str) -> ([i32; 16], i32, i32) {
    let mut f = [0i32; 16];
    let w: Vec<i32> = line.split_whitespace().skip(1).map(|w| w.parse().unwrap_or(0)).collect();
    for (i, v) in w.iter().take(12).enumerate() {
        f[i] = *v;
    }
    (f, w.get(12).copied().unwrap_or(0), w.get(13).copied().unwrap_or(0))
}

fn list() -> &'static EeWonList {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.list) }
}
fn multi() -> &'static EeWonMulti {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.multi) }
}
fn head() -> &'static EeWonMHead {
    // SAFETY: live while the replay runs
    unsafe { &*with(|r| r.head) }
}
fn row_call(row: isize, c: R, a: isize, b: isize, cc: isize) -> isize {
    let p = with(|r| r.rows.get(&row).copied());
    match p {
        // SAFETY: a row lives until its last reference goes (the replay drops it then)
        Some(p) => unsafe { (*p).call(Some(multi()), c, a, b, cc) },
        None => 0,
    }
}
fn is_row(a: isize) -> bool {
    (0x2000..0x3000).contains(&a)
}

fn less(x: isize, y: isize, kind: isize) -> bool {
    let r = row_call(x, R::Compare, y, 0, 0) as i32;
    if kind == 0 {
        r < 0
    } else {
        r > 0
    }
}

unsafe extern "C" fn replay_host(comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let n = op as u32;
    let comp = comp as isize;
    match op {
        Op::Get => with(|r| {
            if comp == HEAD {
                if a == F::Width as isize {
                    r.head_w as isize
                } else if a == F::Height as isize {
                    r.head_h as isize
                } else {
                    0
                }
            } else {
                r.fields[a as usize] as isize
            }
        }),
        Op::MultiOf => with(|r| r.multi as isize),
        Op::BaseSizeChanged => {
            if comp == HEAD {
                expect(&format!("{n} head-base-size"));
            } else {
                expect(&format!("{n} base-size"));
            }
            let (f, w, h) = parse_fields(&next());
            with(|r| {
                r.fields = f;
                r.head_w = w;
                r.head_h = h;
            });
            if comp != HEAD {
                multi().call(M::AdjustPosition, 0, 0, 0);
            }
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
            multi().call(M::AdjustPosition, a, 0, 0);
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
            multi().call(M::NotifyMouseItemGone, 0, 0, 0);
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
        Op::ItemSetPos if is_row(a) => {
            expect(&format!("{n} row-set-pos {a:x} {b}"));
            row_call(a, R::SetPos, 0, b, 0);
            0
        }
        Op::ItemCanSelect if is_row(a) => {
            expect(&format!("{n} row-can-select {a:x} {b} {c}"));
            let r = row_call(a, R::CanSelect, b, c, 0);
            expect(&format!("{n} row-can-select -> {r}"));
            r
        }
        Op::ItemCalcDimensions if is_row(a) => {
            expect(&format!("{n} row-calc {a:x}"));
            row_call(a, R::CalcDimensions, 0, 0, 0);
            0
        }
        Op::ItemSetViewport if is_row(a) => {
            expect(&format!("{n} row-viewport {a:x} {b:x}"));
            row_call(a, R::SetViewport, b, 0, 0);
            0
        }
        Op::ItemPaint if is_row(a) => {
            expect(&format!("{n} row-paint {a:x}"));
            row_call(a, R::Paint, b, 0, 0);
            0
        }
        Op::ItemMouseEnter if is_row(a) => {
            expect(&format!("{n} row-enter {a:x} {b} {c}"));
            row_call(a, R::MouseEnter, b, c, 0);
            0
        }
        Op::ItemMouseExit if is_row(a) => {
            expect(&format!("{n} row-exit {a:x}"));
            row_call(a, R::MouseExit, 0, 0, 0);
            0
        }
        Op::ItemMouseMove if is_row(a) => {
            expect(&format!("{n} row-move {a:x} {b} {c}"));
            row_call(a, R::MouseMove, b, c, 0);
            0
        }
        Op::ItemMouseDrag if is_row(a) => {
            expect(&format!("{n} row-drag {a:x} {b} {c}"));
            row_call(a, R::MouseDrag, b, c, 0);
            0
        }
        Op::ItemMouseDown if is_row(a) => {
            expect(&format!("{n} row-down {a:x} {b} {c} {d}"));
            row_call(a, R::MouseDown, b, c, d);
            0
        }
        Op::ItemMouseUp if is_row(a) => {
            expect(&format!("{n} row-up {a:x} {b} {c} {d}"));
            row_call(a, R::MouseUp, b, c, d);
            0
        }
        Op::ItemRelease => {
            let w = expect(&format!("{n} release {a:x} -> "));
            if w.first().map(String::as_str) == Some("0") && is_row(a) {
                // the row goes: its cells' references back (outside the replay's borrow)
                if let Some(p) = with(|r| r.rows.remove(&a)) {
                    // SAFETY: made by the replay with Box::into_raw, now unreferenced
                    drop(unsafe { Box::from_raw(p) });
                }
            }
            0
        }
        Op::HeaderNotify => {
            expect(&format!("{n} header-notify {a:x}"));
            head().call(H::NotifyColumnChange, 0, 0, 0);
            0
        }
        Op::HeaderSetSortColumn => {
            expect(&format!("{n} header-sort {a:x} {b} {c}"));
            head().call(H::SetSortColumn, b, c, 0);
            0
        }
        Op::RowSetCell => {
            expect(&format!("{n} row-set-cell {a:x} {b:x} {c}"));
            row_call(a, R::SetItem, b, c, 0);
            0
        }
        Op::ChildSetPosSize => {
            // SAFETY: the object passes a live rectangle
            let q = unsafe { *(b as *const EeWonRect4) };
            expect(&format!("{n} pos-size {a:x} {} {} {} {}", q.x, q.y, q.w, q.h));
            0
        }
        Op::ChildDesiredSize => {
            let w = expect(&format!("{n} desired {a:x} -> "));
            // SAFETY: the object passes two live i32s
            let wh = unsafe { std::slice::from_raw_parts_mut(b as *mut i32, 2) };
            wh[0] = w.first().and_then(|x| x.parse().ok()).unwrap_or(0);
            wh[1] = 20;
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

fn new_row(id: isize) {
    let p = Box::into_raw(Box::new(EeWonMRow::new(id as *mut c_void, Some(replay_host))));
    with(|r| r.rows.insert(id, p));
}

fn run(m: &EeWonMulti, l: &EeWonList, hd: &EeWonMHead, words: &[&str]) -> Result<isize, String> {
    let (a, b, c) = (hex(words[1]), hex(words[2]), hex(words[3]));
    Ok(match words[0] {
        "insert-row" => {
            new_row(a);
            // the row's cells are the golden's next call lines
            loop {
                let peek = with(|r| r.lines.get(r.at).cloned().unwrap_or_default());
                let Some(rest) = peek.strip_prefix("call row-cell ") else { break };
                with(|r| r.at += 1);
                let w: Vec<&str> = rest.split_whitespace().collect();
                row_call(hex(w[0]), R::InsertItem, hex(w[1]), 0, 0);
            }
            l.call(L::InsertItem, a, b, 0)
        }
        "set-cell" => m.call(M::SetCell, a, b, c),
        "append-cell" => {
            if a != 0 {
                row_call(a, R::InsertItem, b, 0, 0);
            }
            0
        }
        "update" => {
            if a != 0 {
                m.call(M::UpdateItem, a, 0, 0)
            } else {
                0
            }
        }
        "num-cols" => m.call(M::SetNumColumns, a, 0, 0),
        "width" => m.call(M::SetColumnWidth, a, b, c),
        "size-mode" => m.call(M::SetColumnSizeMode, a, b, 0),
        "halign" => m.call(M::SetColumnHAlign, a, b, 0),
        "sort-col" => {
            m.call(M::SetSortColumn, a, b, 0);
            m.call(M::SortColumnPrev, 0, 0, 0)
        }
        "sort-func" => m.call(M::SetSortColumnFunc, a, b, 0),
        "dynamic" => m.call(M::SetDynamicSizeColumn, a, 0, 0),
        "auto-grow" => m.call(M::SetAutoGrowColumns, a, 0, 0),
        "size" => {
            m.call(M::SizeChanged, 0, 0, 0);
            m.call(M::TotalColumnWidth, 0, 0, 0)
        }
        "adjust" => m.call(M::AdjustPosition, a, 0, 0),
        "paint" => {
            m.call(M::ScrollPaint, G, 0, 0);
            m.call(M::PaintContext, 0, 0, 0)
        }
        "enter" => {
            l.call(L::MouseEnter, a, b, 0);
            m.call(M::MouseColItem, 0, 0, 0)
        }
        "move" => {
            l.call(L::MouseMove, a, b, 0);
            m.call(M::MouseColItem, 0, 0, 0)
        }
        "down" => {
            l.call(L::MouseDown, a, b, c);
            m.call(M::MouseColItem, 0, 0, 0)
        }
        "up" => l.call(L::MouseUp, a, b, c),
        "drag" => l.call(L::MouseDrag, a, b, 0),
        "exit" => {
            l.call(L::MouseExit, 0, 0, 0);
            m.call(M::MouseColItem, 0, 0, 0)
        }
        "delete" => l.call(L::DeleteItem, a, 0, 0),
        "clear" => l.call(L::Clear, a, 0, 0),
        "flags" => l.call(L::SetListFlags, a, b, 0),
        "refresh" => {
            if a != 0 {
                l.call(L::RefreshItem, a, 0, 0);
            }
            0
        }
        "col-at" => m.call(M::ColumnAt, a, 0, 0),
        "col-width" => m.call(M::ColumnWidth, a, 0, 0),
        "col-pos" => m.call(M::ColumnPos, a, 0, 0),
        "query" => {
            const Q: [M; 8] = [
                M::PaintContext,
                M::SortColumn,
                M::SortColumnPrev,
                M::NumColumns,
                M::TotalColumnWidth,
                M::MouseColItem,
                M::Header,
                M::NumColumns,
            ];
            let q = Q[a as usize & 7];
            let r = m.call(q, 0, 0, 0);
            if q == M::Header {
                (r != 0) as isize
            } else {
                r
            }
        }
        "head-item" => hd.call(H::SetColumnItem, a, b, 0),
        "head-size" => hd.call(H::SizeChanged, 0, 0, 0),
        "head-down" => {
            hd.call(H::MouseDown, a, b, c);
            hd.call(H::DragColumn, 0, 0, 0)
        }
        "head-move" => {
            hd.call(H::MouseMove, a, b, 0);
            hd.call(H::DragColumn, 0, 0, 0)
        }
        "head-drag" => hd.call(H::MouseDrag, a, b, 0),
        "head-up" => {
            hd.call(H::MouseUp, a, b, c);
            hd.call(H::DragColumn, 0, 0, 0)
        }
        "head-exit" => hd.call(H::MouseExit, 0, 0, 0),
        "head-event" => hd.call(H::HandleEvent, a, 0, 0),
        "head-sort" => hd.call(H::SortType, a, 0, 0),
        "head-index" => hd.call(H::ColumnIndex, a, 0, 0),
        "head-notify" => {
            hd.call(H::NotifyColumnChange, 0, 0, 0);
            hd.call(H::DrawSortArrow, 0, 0, 0) * 2 + (hd.call(H::ListArea, 0, 0, 0) != 0) as isize
        }
        "gone" => {
            l.call(L::ClearMouseItem, 0, 0, 0);
            m.call(M::NotifyMouseItemGone, 0, 0, 0)
        }
        name => return Err(format!("unknown call '{name}'")),
    })
}

fn replay(lines: &[String]) -> Option<String> {
    let (fields, w, h) = parse_fields(&lines[0]);
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.to_vec(),
            at: 1,
            fields,
            head_w: w,
            head_h: h,
            failure: None,
            list: std::ptr::null(),
            multi: std::ptr::null(),
            head: std::ptr::null(),
            rows: BTreeMap::new(),
        })
    });
    let list_box = Box::new(EeWonList::new(LIST as *mut c_void, Some(replay_host)));
    let lp: *const EeWonList = &*list_box;
    let multi_box = Box::new(EeWonMulti::new(LIST as *mut c_void, Some(replay_host), lp));
    let head_box = Box::new(EeWonMHead::new(HEAD as *mut c_void, Some(replay_host)));
    with(|r| {
        r.list = lp;
        r.multi = &*multi_box;
        r.head = &*head_box;
    });
    let (l, m, hd) = (list(), multi(), head());
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
            m.call(M::SetNumColumns, words[1].parse().unwrap_or(0), 0, 0);
            hd.call(H::SetMultiListArea, LIST, 0, 0);
            continue;
        }
        let got = match run(m, l, hd, &words) {
            Ok(g) => g,
            Err(e) => return Some(e),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the header and the list's own objects go, then the base (its rows, the rows their cells)
    drop(head_box);
    drop(multi_box);
    drop(list_box);
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            let l = r.lines.get(r.at).cloned().unwrap_or_default();
            r.failure = Some(format!("line {}: the golden goes on ('{l}')", r.at));
        }
        if r.failure.is_none() && !r.rows.is_empty() {
            r.failure = Some(format!("{} rows left", r.rows.len()));
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
fn golden_multiobj_replays() {
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
        .find_map(|s| s.iter().position(|l| l.contains(" pos-size ")).map(|k| (s, k)))
        .expect("a header layout");
    let mut m = s.clone();
    m[k] = m[k].replacen(" pos-size ", " pos-size 1", 1);
    assert!(replay(&m).is_some());
}

#[test]
fn a_row_keeps_and_gives_back_its_cells() {
    use std::sync::atomic::{AtomicIsize, Ordering};
    static REFS: AtomicIsize = AtomicIsize::new(0);
    unsafe extern "C" fn host(_c: *mut c_void, op: Op, _a: isize, _b: isize, _cc: isize, _d: isize) -> isize {
        match op {
            Op::ItemHold => REFS.fetch_add(1, Ordering::SeqCst),
            Op::ItemRelease => REFS.fetch_sub(1, Ordering::SeqCst),
            _ => 0,
        };
        0
    }
    let r = EeWonMRow::new(0x2001 as *mut c_void, Some(host));
    r.insert_item(0x1001);
    r.set_item(0x1002, 3);
    r.set_item(0x1002, 3);
    r.set_item(0x1003, 0);
    assert_eq!(r.get_item(3), 0x1002);
    assert_eq!(r.get_item(0), 0x1003);
    assert_eq!(r.get_item(1), 0);
    assert_eq!(r.get_item(-1), 0);
    assert_eq!(REFS.load(Ordering::SeqCst), 2);
    drop(r);
    assert_eq!(REFS.load(Ordering::SeqCst), 0);
}
