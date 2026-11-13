use super::*;
use std::cell::RefCell as Cell;
use std::collections::HashMap;

// ---- a small recording host for the unit tests

type Entry = (isize, Op, isize, isize, isize, isize);

thread_local! {
    static LOG: Cell<Vec<Entry>> = const { Cell::new(Vec::new()) };
    static ANSWERS: Cell<HashMap<(u32, isize), isize>> = Cell::new(HashMap::new());
}

unsafe extern "C" fn rec_host(win: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((win as isize, op, a, b, c, d)));
    let key = (op as u32, if op == Op::WinRoot { a } else { 0 });
    ANSWERS.with(|m| m.borrow().get(&key).copied().unwrap_or(0))
}

fn answer(op: Op, sub: isize, v: isize) {
    ANSWERS.with(|m| m.borrow_mut().insert((op as u32, sub), v));
}

fn take() -> Vec<Entry> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn new_win(id: isize) -> EeWonWindow {
    EeWonWindow::new(id as *mut c_void, Some(rec_host))
}

#[test]
fn the_field_count_is_the_last_field() {
    assert_eq!(EE_WON_WIN_FIELD_COUNT, EeWonWinField::ChildCount as i32);
}

#[test]
fn starts_closed_with_the_constructor_sizes() {
    let w = new_win(1);
    assert_eq!(w.field(EeWonWinField::State as i32), EE_WON_WIN_CLOSED as isize);
    assert_eq!(w.field(EeWonWinField::MaxWidth as i32), 64000);
    assert_eq!(w.field(EeWonWinField::NativeCursorShowing as i32), 1);
    assert_eq!(w.field(EeWonWinField::ChildCount as i32), 0);
    assert_eq!(w.field(0), 0);
    assert_eq!(w.field(99), 0);
}

#[test]
fn close_of_a_closed_window_does_nothing() {
    take();
    let w = new_win(1);
    assert!(w.close());
    assert!(take().is_empty());
}

#[test]
fn close_asks_the_root_and_queues_the_event() {
    take();
    let w = new_win(1);
    w.create(100, 80);
    take();
    answer(Op::WinRoot, EeWonWinRoot::CanClose as isize, 1);
    answer(Op::WinAllowClose, 0, 1);
    assert!(w.close());
    assert_eq!(w.field(EeWonWinField::State as i32), EE_WON_WIN_CLOSING as isize);
    let ops: Vec<Op> = take().iter().map(|e| e.1).collect();
    assert_eq!(ops, vec![Op::WinRoot, Op::WinAllowClose, Op::WinQueueClose]);
    answer(Op::WinRoot, EeWonWinRoot::CanClose as isize, 0);
}

#[test]
fn create_registers_with_the_parent_and_close_unregisters() {
    take();
    let w = new_win(2);
    w.call(EeWonWinCall::SetParent, 7, 0);
    w.create(10, 20);
    let log = take();
    assert_eq!(log[0], (2, Op::WinChildSet, 7, 1, 0, 0));
    assert_eq!(log[1], (2, Op::WinRoot, EeWonWinRoot::SetSize as isize, 10, 20, 0));
    assert_eq!(log[2].1, Op::WinAddToManager);
    w.platform_close();
    let log = take();
    assert_eq!(log.last().copied(), Some((2, Op::WinChildSet, 7, 0, 0, 0)));
    assert_eq!(w.field(EeWonWinField::State as i32), EE_WON_WIN_CLOSED as isize);
}

#[test]
fn the_child_set_is_ordered_by_address() {
    let w = new_win(1);
    w.call(EeWonWinCall::ChildSet, 30, 1);
    w.call(EeWonWinCall::ChildSet, 10, 1);
    w.call(EeWonWinCall::ChildSet, 10, 1);
    assert_eq!(w.field(EeWonWinField::ChildCount as i32), 2);
    take();
    answer(Op::WinCanClose, 0, 1);
    w.can_close();
    let kids: Vec<isize> = take().iter().filter(|e| e.1 == Op::WinCanClose).map(|e| e.2).collect();
    assert_eq!(kids, vec![10, 30]);
    answer(Op::WinCanClose, 0, 0);
}

#[test]
fn a_mouse_button_out_of_range_is_ignored_not_a_panic() {
    let w = new_win(1);
    let e = EeWonWinEvent { kind: 3, button: 7, ..Default::default() };
    w.handle_mouse_down(&e);
    assert_eq!(w.call(EeWonWinCall::IsMouseDown, 7, 0), 0);
    assert_eq!(w.call(EeWonWinCall::CheckDoubleClick, -1, 0), 0);
}

#[test]
fn setters_reach_only_their_fields() {
    let w = new_win(1);
    w.set_field(EeWonWinField::Style as i32, 0x22);
    w.set_field(EeWonWinField::IsPopup as i32, 1);
    w.set_field(EeWonWinField::State as i32, 1);
    assert_eq!(w.field(EeWonWinField::Style as i32), 0x22);
    assert_eq!(w.field(EeWonWinField::IsPopup as i32), 1);
    assert_eq!(w.field(EeWonWinField::State as i32), EE_WON_WIN_CLOSED as isize);
}

// ---- the golden's replay (tests/won/won_winobj_test.cpp --print)

struct Replay {
    lines: Vec<String>,
    pos: usize,
    wins: Vec<Option<Box<EeWonWindow>>>,
}

thread_local! {
    static REPLAY: Cell<Option<Replay>> = const { Cell::new(None) };
}

fn next_line() -> String {
    REPLAY.with(|r| {
        let mut r = r.borrow_mut();
        let r = r.as_mut().unwrap();
        let l = r.lines[r.pos].clone();
        r.pos += 1;
        l
    })
}

fn win_ptr(id: isize) -> *const EeWonWindow {
    REPLAY.with(|r| &**r.borrow().as_ref().unwrap().wins[id as usize].as_ref().unwrap() as *const EeWonWindow)
}

fn nums(l: &str, skip: usize) -> Vec<i64> {
    l.split_whitespace().skip(skip).map(|x| x.parse::<i64>().unwrap()).collect()
}

unsafe extern "C" fn replay_host(win: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let id = win as isize;
    let (pa, pb) = (op == Op::WinScreenRect, op == Op::WinGetRect);
    let want = format!(
        "op {} {} {} {} {} {}",
        id,
        op as u32,
        if pa { 0 } else { a as i32 },
        if pb { 0 } else { b as i32 },
        c as i32,
        d as i32
    );
    let got = next_line();
    assert_eq!(got, want, "an op");
    let nested = match op {
        // SAFETY: the replay's own live windows
        Op::WinCanClose => Some(unsafe { (*win_ptr(a)).call(EeWonWinCall::CanClose, 0, 0) }),
        Op::WinChildSet => {
            // SAFETY: as above
            unsafe { (*win_ptr(a)).call(EeWonWinCall::ChildSet, id, b) };
            None
        }
        _ => None,
    };
    let ans = next_line();
    assert!(ans.starts_with("= "), "an answer: {ans}");
    let v = nums(&ans, 1);
    if let Some(n) = nested {
        assert_eq!(n as i64, v[0], "a nested answer");
    }
    if v.len() == 5 {
        let r = EeWonRect4 { x: v[1] as i32, y: v[2] as i32, w: v[3] as i32, h: v[4] as i32 };
        let p = if pa { a } else { b } as *mut EeWonRect4;
        // SAFETY: the Rust passed a writable rectangle
        unsafe { *p = r };
    }
    v[0] as isize
}

fn state_line(id: usize) -> String {
    let w = win_ptr(id as isize);
    let mut s = format!("state {id}");
    for f in 1..=EE_WON_WIN_FIELD_COUNT {
        // SAFETY: live
        let v = unsafe { (*w).field(f) };
        let unsigned = matches!(f, 11..=13 | 20 | 28..=30 | 38);
        if unsigned {
            s += &format!(" {}", v as u32);
        } else {
            s += &format!(" {}", v as i32);
        }
    }
    s
}

fn run_call(id: usize, call: i64, a: i64, b: i64, e: &EeWonWinEvent) -> isize {
    type C = EeWonWinCall;
    let w = win_ptr(id as isize);
    let ep = e as *const EeWonWinEvent as isize;
    let (a, b) = (a as isize, b as isize);
    let (c, x, y) = match call {
        0 => (C::CanClose, 0, 0),
        1 => (C::Close, 0, 0),
        2 => (C::CheckResize, a as u32 as isize, 0),
        3 => (C::TitleChanged, 0, 0),
        4 => (C::IconChanged, 0, 0),
        5 => (C::Center, a, 0),
        6 => (C::SetClientSize, a, b),
        7 => (C::Show, a, 0),
        8 => (C::HandleMouseExit, 0, 0),
        9 => (C::HandleKeyEvent, ep, 0),
        10 => (C::HandleMouseDown, ep, 0),
        11 => (C::HandleMouseEvent, ep, 0),
        12 => (C::HandleInputEvent, ep, 0),
        13 => (C::IdleUpdate, 0, 0),
        14 => (C::TimerEvent, a as u32 as isize, 0),
        15 => (C::Create, a, b),
        16 => (C::PlatformClose, 0, 0),
        17 => (C::ShowNativeCursor, a, 0),
        18 => (C::CheckSetCursor, a, 0),
        19 => (C::InvalidateRoot, 0, 0),
        20 => (C::Minimize, a, 0),
        21 => (C::SuspendRootResize, a, 0),
        22 => (C::RemoveComponent, a, 0),
        23 => (C::IsMouseDown, a, 0),
        24 => (C::AddComponent, a, 0),
        25 => (C::SetModal, a, 0),
        26 => (C::CheckDoubleClick, a, 0),
        27 => (C::SetParent, a, 0),
        28 => (C::SetField, EeWonWinField::IsShowing as isize, a),
        29 => (C::SetField, EeWonWinField::IsPopup as isize, a),
        30 => (C::SetField, EeWonWinField::OneComponent as isize, a),
        31 => (C::SetField, EeWonWinField::Style as isize, a),
        _ => panic!("call {call}"),
    };
    // SAFETY: live
    unsafe { (*w).call(c, x, y) }
}

/// Replays golden text; returns the number of sequences.
fn replay(text: &str) -> usize {
    let lines: Vec<String> = text.lines().map(str::to_string).collect();
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines, pos: 0, wins: Vec::new() }));
    let mut seqs = 0;
    loop {
        let more = REPLAY.with(|r| {
            let r = r.borrow();
            let r = r.as_ref().unwrap();
            r.pos < r.lines.len()
        });
        if !more {
            break;
        }
        let l = next_line();
        if l.starts_with("seq ") {
            seqs += 1;
            REPLAY.with(|r| {
                let mut r = r.borrow_mut();
                let r = r.as_mut().unwrap();
                r.wins = (0..=4)
                    .map(|i| {
                        if i == 0 {
                            None
                        } else {
                            Some(Box::new(EeWonWindow::new(i as *mut c_void, Some(replay_host))))
                        }
                    })
                    .collect();
            });
        } else if l.starts_with("set ") {
        } else if l.starts_with("call ") {
            let v = nums(&l, 1);
            let e = EeWonWinEvent {
                kind: v[4] as i32,
                x: v[5] as i32,
                y: v[6] as i32,
                key_mod: v[7] as i32,
                button: v[8] as i32,
                key: v[9] as i32,
                width: v[10] as i32,
                height: v[11] as i32,
            };
            let r = run_call(v[0] as usize, v[1], v[2], v[3], &e);
            assert_eq!(next_line(), format!("ret {}", r as i32), "after {l}");
            for k in 1..=4 {
                assert_eq!(next_line(), state_line(k), "after {l}");
            }
        } else {
            panic!("unexpected line {l}");
        }
    }
    REPLAY.with(|r| *r.borrow_mut() = None);
    seqs
}

#[test]
fn golden_replays() {
    let n = replay(include_str!("golden_winobj.txt"));
    assert!(n >= 30, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_answer() {
    let text = include_str!("golden_winobj.txt");
    // a mouse place moved by one in the first mouse event's state line
    let i = text.find("\nstate 1 0 ").expect("a state line");
    let mut changed = text.to_string();
    let line_end = i + 1 + changed[i + 1..].find('\n').unwrap();
    let line = changed[i + 1..line_end].to_string();
    let mut parts: Vec<String> = line.split(' ').map(str::to_string).collect();
    parts[2] = format!("{}", parts[2].parse::<i64>().unwrap() + 1);
    changed.replace_range(i + 1..line_end, &parts.join(" "));
    let r = std::panic::catch_unwind(|| replay(&changed));
    REPLAY.with(|r| *r.borrow_mut() = None);
    assert!(r.is_err());
}
