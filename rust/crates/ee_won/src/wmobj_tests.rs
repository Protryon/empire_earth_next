use super::*;
use crate::wmcache::EeWonWmRefKind;
use crate::wmloop::ee_won_wm_set_at;
use std::cell::RefCell as Cell;
use std::collections::HashMap;

// ---- a small recording host for the unit tests

type Entry = (Op, isize, isize, isize, isize);

thread_local! {
    static LOG: Cell<Vec<Entry>> = const { Cell::new(Vec::new()) };
    static ANSWERS: Cell<HashMap<(u32, isize), isize>> = Cell::new(HashMap::new());
}

unsafe extern "C" fn rec_host(_wm: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b, c, d)));
    let key = (op as u32, if op == Op::WmComp || op == Op::WmWin { a } else { 0 });
    ANSWERS.with(|m| m.borrow().get(&key).copied().unwrap_or(0))
}

fn answer(op: Op, sub: isize, v: isize) {
    ANSWERS.with(|m| m.borrow_mut().insert((op as u32, sub), v));
}

fn take() -> Vec<Entry> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn new_wm() -> EeWonWm {
    ANSWERS.with(|m| m.borrow_mut().clear());
    take();
    EeWonWm::new(std::ptr::dangling_mut::<c_void>(), Some(rec_host))
}

#[test]
fn the_field_count_is_the_last_field() {
    assert_eq!(EE_WON_WM_FIELD_POPUP_LAST, EeWonWmField::PopupFront as i32);
    assert_eq!(EE_WON_WM_FIELD_COUNT, EeWonWmField::BroadcastCount as i32);
}

#[test]
fn starts_with_no_popup_and_popups_in_their_own_window() {
    let w = new_wm();
    assert_eq!(w.field(EeWonWmField::OwnWindow as i32), 1);
    assert_eq!(w.field(EeWonWmField::PopupIsUp as i32), 0);
    assert_eq!(w.field(EeWonWmField::PopupCount as i32), 0);
    assert_eq!(w.field(0), 0);
    assert_eq!(w.field(99), 0);
    assert_eq!(w.popup_window(), 0);
    assert!(take().is_empty());
}

#[test]
fn lost_focus_at_tick_zero_is_stored_as_one() {
    let w = new_wm();
    w.notify_got_focus(7);
    assert_eq!(w.field(EeWonWmField::FocusWindow as i32), 7);
    answer(Op::WmTick, 0, 0);
    w.notify_lost_focus();
    assert_eq!(w.field(EeWonWmField::FocusWindow as i32), 0);
    assert_eq!(w.field(EeWonWmField::LastLoseFocusTick as i32), 1);
    answer(Op::WmTick, 0, 500);
    w.notify_lost_focus();
    assert_eq!(w.field(EeWonWmField::LastLoseFocusTick as i32), 500);
}

#[test]
fn end_dialog_ends_only_the_named_dialog() {
    let w = new_wm();
    w.set_field(EeWonWmField::TargetDepth as i32, 3);
    w.end_dialog(5, 9, 0); // another window named: nothing
    assert_eq!(w.field(EeWonWmField::TargetDepth as i32), 3);
    w.end_dialog(5, 0, 0);
    assert_eq!(w.field(EeWonWmField::TargetDepth as i32), 2);
    assert_eq!(w.field(EeWonWmField::DialogResult as i32), 5);
    w.exit_message_loop();
    assert_eq!(w.field(EeWonWmField::TargetDepth as i32), 1);
}

#[test]
fn message_loop_without_windows_answers_minus_one() {
    let w = new_wm();
    assert_eq!(w.message_loop(), -1);
    // the window set is the manager's own (round 34): no op
    assert!(take().is_empty());
}

#[test]
fn the_saved_window_is_held_and_given_back() {
    let w = new_wm();
    w.set_field(EeWonWmField::PopupWindowSave as i32, 5);
    w.set_field(EeWonWmField::PopupWindowSave as i32, 5);
    w.set_field(EeWonWmField::PopupWindowSave as i32, 6);
    w.release_members();
    let log: Vec<(isize, isize)> = take().iter().map(|e| (e.1, e.2)).collect();
    let hold = EeWonWmWin::Hold as isize;
    let release = EeWonWmWin::Release as isize;
    assert_eq!(log, vec![(hold, 5), (release, 5), (hold, 6), (release, 6)]);
}

#[test]
fn not_modal_window() {
    let w = new_wm();
    assert!(!w.not_modal_window(3));
    w.st.borrow_mut().modal_window = 2;
    assert!(w.not_modal_window(3));
    assert!(!w.not_modal_window(2));
}

// ---- the golden's replay (tests/won/won_wmobj_test.cpp --print)

struct Replay {
    lines: Vec<String>,
    pos: usize,
    wm: Option<Box<EeWonWm>>,
}

thread_local! {
    static REPLAY: Cell<Option<Replay>> = const { Cell::new(None) };
}

fn peek_line() -> Option<String> {
    REPLAY.with(|r| {
        let r = r.borrow();
        let r = r.as_ref().unwrap();
        r.lines.get(r.pos).cloned()
    })
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

fn wm_ptr() -> *const EeWonWm {
    REPLAY.with(|r| &**r.borrow().as_ref().unwrap().wm.as_ref().unwrap() as *const EeWonWm)
}

fn nums(l: &str, skip: usize) -> Vec<i64> {
    l.split_whitespace().skip(skip).filter(|x| *x != "r").map(|x| x.parse::<i64>().unwrap()).collect()
}

unsafe extern "C" fn replay_host(_wm: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    type E = EeWonWmEvent;
    let rect_a = op == Op::WmScreenRect;
    let set_pos = op == Op::WmWin && a == EeWonWmWin::SetPosSize as isize;
    let get_rect = op == Op::WmWin && a == EeWonWmWin::GetRect as isize;
    let ev = |x: E| op == Op::WmEvent && a == x as isize;
    let res_get = op == Op::WmResolution && a == 0;
    let rect_c = get_rect || set_pos || ev(E::Mouse) || ev(E::SetMouse) || ev(E::Size) || ev(E::SetSize) || res_get;
    let rect_b = op == Op::WmNewPopupWindow || res_get;
    let str_b = op == Op::WmDecode;
    let str_a = op == Op::WmDecodeRaw || op == Op::WmDelayImage;
    let extra = if str_a || str_b {
        let (p, n) = if str_b { (b, c) } else { (a, b) };
        // SAFETY: the Rust passed a path's bytes
        let s = unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) };
        format!(" s {}", String::from_utf8_lossy(s))
    } else if set_pos || ev(E::SetSize) || op == Op::WmNewPopupWindow {
        // SAFETY: the Rust passed a rectangle
        let r = unsafe { *((if op == Op::WmNewPopupWindow { b } else { c }) as *const EeWonRect4) };
        format!(" r {} {} {} {}", r.x, r.y, r.w, r.h)
    } else if ev(E::SetMouse) {
        // SAFETY: the Rust passed a mouse event's fields
        let m = unsafe { *(c as *const EeWonWmMouse) };
        format!(" m {} {} {}", m.window, m.x, m.y)
    } else {
        String::new()
    };
    let want = format!(
        "op {} {} {} {} {}{}",
        op as u32,
        if rect_a || str_a { 0 } else { a },
        if rect_b || str_b { 0 } else { b },
        if rect_c { 0 } else { c },
        d,
        extra
    );
    let got = next_line();
    assert_eq!(got, want, "an op");
    // the platform's next event and a popup's end event run nested calls
    while peek_line().is_some_and(|l| l.starts_with("call ")) {
        run_call_line(false);
    }
    let ans = next_line();
    assert!(ans.starts_with("= "), "an answer: {ans}");
    let v = nums(&ans, 1);
    if rect_a || get_rect || ev(E::Size) {
        let r = EeWonRect4 { x: v[1] as i32, y: v[2] as i32, w: v[3] as i32, h: v[4] as i32 };
        let p = if rect_a { a } else { c } as *mut EeWonRect4;
        // SAFETY: the Rust passed a writable rectangle
        unsafe { *p = r };
    } else if ev(E::Mouse) {
        let m = EeWonWmMouse { window: v[1] as isize, x: v[2] as i32, y: v[3] as i32, button: v[4] as i32 };
        // SAFETY: the Rust passed a writable mouse event
        unsafe { *(c as *mut EeWonWmMouse) = m };
    } else if res_get {
        // SAFETY: the Rust passed two writable ints
        unsafe {
            *(b as *mut i32) = v[1] as i32;
            *(c as *mut i32) = v[2] as i32;
        }
    }
    v[0] as isize
}

fn state_lines() -> (String, String) {
    let w = wm_ptr();
    let mut s = "state".to_string();
    for f in 1..=EE_WON_WM_FIELD_POPUP_LAST {
        // SAFETY: live
        let v = unsafe { (*w).field(f) };
        match f {
            1..=10 => s += &format!(" {}", v as i32),
            19 | 20 => s += &format!(" {}", v as u32),
            _ => s += &format!(" {v}"),
        }
    }
    let mut l = "list".to_string();
    // SAFETY: live
    let n = unsafe { (*w).field(EeWonWmField::PopupCount as i32) };
    for i in 0..n as usize {
        // SAFETY: live
        l += &format!(" {}", unsafe { ee_won_wm_popup_at(w, i) });
    }
    // SAFETY: live
    let wm = unsafe { &*w };
    let g = |f: EeWonWmField| wm.field(f as i32);
    type F = EeWonWmField;
    let lp = format!(
        "loop {} {} {} {} {} {} {} {} {} {}",
        g(F::DefaultWindow),
        g(F::WindowCount) as u32,
        g(F::FullScreen) as i32,
        g(F::FullScreenWidth) as i32,
        g(F::FullScreenHeight) as i32,
        g(F::OriginalWidth) as i32,
        g(F::OriginalHeight) as i32,
        g(F::LastTimerTick) as u32,
        g(F::LastInputTick) as u32,
        g(F::CallingTimers) as i32
    );
    let mut sets = "sets".to_string();
    for k in 0..5u32 {
        sets += " |";
        let n =
            g([F::WindowSetCount, F::EventCount, F::TimerCount, F::RemoveTimerCount, F::BroadcastCount][k as usize]);
        for i in 0..n as usize {
            // SAFETY: live
            sets += &format!(" {}", unsafe { ee_won_wm_set_at(w, k, i) });
        }
    }
    (s, l + "\n" + &wm.cache_line() + "\n" + &lp + "\n" + &sets)
}

fn run_call(call: i64, v: &[i64]) -> (isize, Option<EeWonWmMouse>) {
    type K = EeWonWmCall;
    type F = EeWonWmField;
    let w = wm_ptr();
    let a = |i: usize| v[i] as isize;
    // SAFETY: the replay's live manager
    let wm = unsafe { &*w };
    let field = |f: F, x: isize| {
        wm.call(K::SetField, f as isize, x, 0, 0);
        0
    };
    let r = match call {
        0 => wm.call(K::NotifyLostFocus, 0, 0, 0, 0),
        1 => wm.call(K::NotifyGotFocus, a(0), 0, 0, 0),
        2 => wm.call(K::PopupWindow, 0, 0, 0, 0),
        3 => wm.call(K::CalcModality, 0, 0, 0, 0),
        4 => wm.call(K::NotModalWindow, a(0), 0, 0, 0),
        5 => wm.call(K::DoDialog, a(0), a(1), 0, 0),
        6 => wm.call(K::EndDialog, a(0), a(1), a(2), 0),
        7 => wm.call(K::ExitMessageLoop, 0, 0, 0, 0),
        8 => wm.call(K::MessageLoop, 0, 0, 0, 0),
        9 => {
            let r = EeWonRect4 { x: v[3] as i32, y: v[4] as i32, w: v[5] as i32, h: v[6] as i32 };
            wm.call(K::AdjustPopupPosition, a(0), a(1), a(2), &r as *const EeWonRect4 as isize)
        }
        10 => {
            let p = EeWonWmPopup {
                parent: a(0),
                popup: a(1),
                input: a(2),
                owner: a(3),
                flags: v[4] as i32,
                x: v[5] as i32,
                y: v[6] as i32,
                avoid: EeWonRect4 { x: v[7] as i32, y: v[8] as i32, w: v[9] as i32, h: v[10] as i32 },
            };
            wm.call(K::DoPopup, &p as *const EeWonWmPopup as isize, 0, 0, 0)
        }
        11 => wm.call(K::EndPopup, a(0), 0, 0, 0),
        12 => wm.call(K::CheckEndPopup, a(0), 0, 0, 0),
        13 | 14 => {
            let mut m = EeWonWmMouse { window: a(0), x: v[1] as i32, y: v[2] as i32, button: v[3] as i32 };
            let k = if call == 13 { K::FindMouseWindow } else { K::CheckPopupClick };
            let r = wm.call(k, &mut m as *mut EeWonWmMouse as isize, 0, 0, 0);
            return (r, Some(m));
        }
        15 => wm.call(K::CheckModalValid, 0, 0, 0, 0),
        16 => field(F::OwnWindow, a(0)),
        17 => field(F::CaptureSupport, a(0)),
        18 => field(F::EndPopup, a(0)),
        19 => field(F::MouseDownWindow, a(0)),
        20 => field(F::TargetDepth, a(0)),
        21 => field(F::LastLoseFocusTick, a(0)),
        22 => field(F::PopupWindowSave, a(0)),
        23..=30 => {
            const NAMES: [&str; 9] =
                ["a.png", "A.PNG", "b.png", "Lobby.jpg", "LOBBY.jpg", "c.png", "Title", "title", "Body"];
            let idx = match call {
                28 => v[1],
                23 | 24 | 29 | 30 => v[0],
                _ => 0,
            };
            let n = NAMES[idx as usize].as_bytes();
            let (s, len) = (n.as_ptr() as isize, n.len() as isize);
            let (r, kind) = match call {
                23 => (wm.call(K::SetNamedFont, s, len, a(1), 0), None),
                24 => (wm.call(K::GetNamedFont, s, len, 0, 0), None),
                25 => (wm.call(K::GetDefaultFont, 0, 0, 0, 0), None),
                26 => (wm.call(K::SetDefaultFont, a(0), 0, 0, 0), None),
                27 => (wm.call(K::SetCacheImages, a(0), 0, 0, 0), None),
                28 => (wm.call(K::DecodeImage, a(0), s, len, 0), Some(EeWonWmRefKind::NativeImage)),
                29 => (wm.call(K::DecodeImageRaw, s, len, 0, 0), Some(EeWonWmRefKind::RawImage)),
                _ => (wm.call(K::DecodeDelayImage, s, len, 0, 0), Some(EeWonWmRefKind::Image)),
            };
            // the caller's SmartPtr given back
            if let (Some(k), true) = (kind, r != 0) {
                // SAFETY: the replay's host
                unsafe { replay_host(std::ptr::null_mut(), Op::WmRef, k as isize, r, 0, 0) };
            }
            r
        }
        31 => wm.call(K::SetScreenResolution, a(0), a(1), 0, 0),
        32 => wm.call(K::RestoreScreenResolution, 0, 0, 0, 0),
        33 => wm.call(K::CloseAllWindows, 0, 0, 0, 0),
        34 => wm.call(K::AddWindow, a(0), 0, 0, 0),
        35 => wm.call(K::RemoveWindow, a(0), 0, 0, 0),
        36 => wm.call(K::PostEvent, a(0), a(1), 0, 0),
        37 => wm.call(K::AddTimerEvent, a(0), 0, 0, 0),
        38 => wm.call(K::RemoveTimerEvent, a(0), 0, 0, 0),
        39 => wm.call(K::RemoveAllTimerEvents, 0, 0, 0, 0),
        40 => wm.call(K::SubscribeToBroadcast, a(0), a(1), 0, 0),
        41 => wm.call(K::ClearSubscriptions, 0, 0, 0, 0),
        42 => wm.call(K::BroadcastEvent, a(0), 0, 0, 0),
        43 => wm.call(K::PeekNextEvent, 0, 0, 0, 0),
        44 => wm.call(K::ProcessEvents, 0, 0, 0, 0),
        45 => wm.call(K::DeliverEvent, a(0), 0, 0, 0),
        46 => {
            field(F::FullScreen, a(0));
            field(F::FullScreenWidth, a(1));
            field(F::FullScreenHeight, a(2))
        }
        47 => field(F::LastTimerTick, a(0)),
        _ => panic!("call {call}"),
    };
    (r, None)
}

/// A "call" line: the call made, its event and "ret" lines checked, and at the top level the state lines.
fn run_call_line(top: bool) {
    let l = next_line();
    let v = nums(&l, 1);
    let (r, ev) = run_call(v[0], &v[1..]);
    if let Some(m) = ev {
        assert_eq!(next_line(), format!("event {} {} {}", m.window, m.x, m.y), "after {l}");
    }
    assert_eq!(next_line(), format!("ret {r}"), "after {l}");
    if top {
        let (s, rest) = state_lines();
        assert_eq!(next_line(), s, "after {l}");
        for want in rest.split('\n') {
            assert_eq!(next_line(), want, "after {l}");
        }
    }
}

/// Replays golden text; returns the number of sequences.
fn replay(text: &str) -> usize {
    let lines: Vec<String> = text.lines().map(str::to_string).collect();
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines, pos: 0, wm: None }));
    let mut seqs = 0;
    while let Some(l) = peek_line() {
        if l.starts_with("seq ") {
            next_line();
            seqs += 1;
            REPLAY.with(|r| {
                r.borrow_mut().as_mut().unwrap().wm =
                    Some(Box::new(EeWonWm::new(std::ptr::dangling_mut::<c_void>(), Some(replay_host))))
            });
        } else if l.starts_with("set ") {
            next_line();
        } else if l.starts_with("call ") {
            run_call_line(true);
        } else if l.starts_with("op ") || l.starts_with("refs") {
            // the destructor's releases, then the world's reference counts
            let w = REPLAY.with(|r| r.borrow_mut().as_mut().unwrap().wm.take().unwrap());
            let p = Box::into_raw(w);
            // SAFETY: from Box::into_raw just above
            unsafe { ee_won_wm_free(p) };
            assert!(next_line().starts_with("refs"));
            assert!(next_line().starts_with("objs"));
        } else {
            panic!("unexpected line {l}");
        }
    }
    REPLAY.with(|r| *r.borrow_mut() = None);
    seqs
}

#[test]
fn golden_replays() {
    let n = replay(include_str!("golden_wmobj.txt"));
    assert!(n >= 30, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_answer() {
    let text = include_str!("golden_wmobj.txt");
    // the first popup list with an entry: its first popup changed
    let i = text.find("\nlist ").expect("a popup list");
    let mut changed = text.to_string();
    changed.replace_range(i + 1..i + 6, "list 99");
    let r = std::panic::catch_unwind(|| replay(&changed));
    REPLAY.with(|r| *r.borrow_mut() = None);
    assert!(r.is_err());
}

#[test]
fn golden_replay_fails_on_a_changed_window_set() {
    let text = include_str!("golden_wmobj.txt");
    // the first trace of the loop's sets (round 34): its window set's first entry changed (a mismatch inside the
    // host would abort: the host is an extern "C" function)
    let i = text
        .find(
            "
sets | ",
        )
        .expect("a sets line");
    let mut changed = text.to_string();
    changed.replace_range(i + 1..i + 8, "sets | 99 ");
    let r = std::panic::catch_unwind(|| replay(&changed));
    REPLAY.with(|r| *r.borrow_mut() = None);
    assert!(r.is_err());
}

#[test]
fn the_standard_colours_are_init_color_schemes() {
    // WindowManager::InitColorScheme's 17 SetStandardColor statements, in StandardColor order
    let mut c = [0u32; 17];
    // SAFETY: room for 17
    unsafe { super::ee_won_wm_standard_colors(c.as_mut_ptr()) };
    assert_eq!(
        c,
        [
            0x000000, 0xD0D0D0, 0xFFFFFF, 0x848484, 0xDEDEDE, 0x000000, 0x848484, 0x08246B, 0xFFFFFF, 0xFFFFDD,
            0x000000, 0xD0D0D0, 0, 0xFFFFFF, 0x000000, 0x0000FF, 0xFF0000
        ]
    );
}
