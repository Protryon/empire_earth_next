use super::*;
use crate::lobbydlg::{
    ee_won_lobbydlg_call, ee_won_lobbydlg_free, ee_won_lobbydlg_new, slot_name, EeWonLobbyDlg, EeWonLobbyDlgCall,
    EeWonLobbyDlgKind, EeWonLobbyDlgOp, EeWonLobbyDlgSlot,
};
use std::ffi::CStr;

// ---- the golden replay: golden_dlglogic.txt, recorded by tests/won/won_dlglogic_test.cpp over the original
// statements. "logic" sequences drive a DialogLogic object, "kind" sequences a Status or OkCancel dialog. Every op
// the Rust makes must be the next "op" line (a Bounds op's rectangle is the line's answer, an InvalidateRect's is
// compared), a "reenter" line after an op re-enters the object with that call and its "reret" is compared, and
// every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_dlglogic.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    logic: *const EeWonDlgLogic,
    failed: Option<String>,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn fail(msg: String) {
    REPLAY.with(|r| {
        let mut r = r.borrow_mut();
        let r = r.as_mut().unwrap();
        if r.failed.is_none() {
            r.failed = Some(msg);
        }
    });
}

fn next_line() -> Option<&'static str> {
    REPLAY.with(|r| {
        let mut r = r.borrow_mut();
        let r = r.as_mut().unwrap();
        let l = r.lines.get(r.at).copied();
        r.at += 1;
        l
    })
}

fn peek_line() -> Option<&'static str> {
    REPLAY.with(|r| {
        let r = r.borrow();
        let r = r.as_ref().unwrap();
        r.lines.get(r.at).copied()
    })
}

fn nums(t: &[&str]) -> Vec<isize> {
    t.iter().map(|x| x.parse().unwrap()).collect()
}

fn logic_call_of(n: isize) -> EeWonDlgLogicCall {
    use EeWonDlgLogicCall as C;
    [
        C::Init,
        C::KillHook,
        C::Reset,
        C::StatusOpComplete,
        C::ClearDialogs,
        C::DoDialog,
        C::RunOpWithStatus,
        C::ShowStatus,
        C::ShowError,
        C::UpdateStatus,
        C::ShowOkCancel,
        C::ShowYesNo,
        C::Field,
    ][n as usize - 1]
}

unsafe extern "C" fn logic_host(
    _logic: *mut c_void,
    op: EeWonDlgLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    let Some(line) = next_line() else {
        fail(format!("op past the end: {}", op as u32));
        return 0;
    };
    let t: Vec<&str> = line.split(' ').collect();
    if op == EeWonDlgLogicOp::Bounds || op == EeWonDlgLogicOp::InvalidateRect {
        let ok =
            t.len() == 8 && t[0] == "op" && t[1] == (op as u32).to_string() && t[2] == a.to_string() && t[3] == "rect";
        if !ok {
            fail(format!("expected '{line}', the Rust made op {} {a} rect", op as u32));
            return 0;
        }
        let rect: Vec<i32> = t[4..8].iter().map(|x| x.parse().unwrap()).collect();
        let p = b as *mut i32;
        for (i, v) in rect.iter().enumerate() {
            if op == EeWonDlgLogicOp::Bounds {
                // SAFETY: the Rust's four-int rectangle
                unsafe { *p.add(i) = *v };
            } else {
                // SAFETY: the same
                let got = unsafe { *p.add(i) };
                if got != *v {
                    fail(format!("{line}: the Rust's rectangle differs at {i}: {got}"));
                }
            }
        }
        return 0;
    }
    let made = format!("op {} {a} {b} {c} {d}", op as u32);
    let (head, answer) = line.split_once(" = ").unwrap_or((line, "0"));
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    let answer: isize = answer.parse().unwrap();
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let n = nums(&rest.split(' ').collect::<Vec<_>>());
            let logic = REPLAY.with(|r| r.borrow().as_ref().unwrap().logic);
            // SAFETY: the replay's live object; the call re-enters through a shared reference, as the C++ does
            let rr = unsafe { ee_won_dlglogic_call(logic, logic_call_of(n[0]), n[1], n[2], n[3], n[4], n[5]) };
            let reret = next_line().unwrap_or("-");
            if reret != format!("reret {rr}") {
                fail(format!("{next}: expected '{reret}', the Rust returned {rr}"));
            }
        }
    }
    answer
}

fn dlg_call_of(n: isize) -> EeWonLobbyDlgCall {
    use EeWonLobbyDlgCall as C;
    [
        C::Init,
        C::Event,
        C::Reset,
        C::Slot,
        C::CheckInput,
        C::UpdatePasswordVisibility,
        C::SetPort,
        C::SetGamePort,
        C::GetPort,
        C::SetGameType,
        C::GetGame,
        C::GetPassword,
        C::ApplyToServer,
        C::NumSeconds,
        C::Field,
        C::SetText,
        C::SetStatus,
        C::SetStartTick,
    ][n as usize - 1]
}

unsafe extern "C" fn dlg_host(_dialog: *mut c_void, op: EeWonLobbyDlgOp, a: isize, b: isize, c: isize) -> isize {
    let made = if op == EeWonLobbyDlgOp::Find {
        // SAFETY: a Find's b is a slot of the table
        let slot = unsafe { &*(b as *const EeWonLobbyDlgSlot) };
        // SAFETY: static C strings
        let file = unsafe { CStr::from_ptr(slot.file) }.to_str().unwrap();
        format!("op {} {} {} {} {} {}", op as u32, a, slot_name(slot), file, slot.line, slot.ty as u32)
    } else {
        format!("op {} {} {} {}", op as u32, a, b, c)
    };
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.split_once(" = ").unwrap_or((line, "0"));
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    answer.parse().unwrap()
}

const BUTTONS: [isize; 2] = [0x53, 0x54];

fn replay_logic(body: Vec<&'static str>) {
    let logic = ee_won_dlglogic_new(std::ptr::dangling_mut::<c_void>(), Some(logic_host));
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: body, at: 0, logic, failed: None }));
    while let Some(line) = next_line() {
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "call" => {
                let n = nums(&t[1..]);
                // SAFETY: the live object
                let r = unsafe { ee_won_dlglogic_call(logic, logic_call_of(n[0]), n[1], n[2], n[3], n[4], n[5]) };
                let ret = next_line().unwrap_or("-");
                if ret != format!("ret {r}") {
                    fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                }
            }
            // SAFETY: the live object, not used again
            "free" => unsafe { ee_won_dlglogic_free(logic) },
            _ => fail(format!("unexpected line '{line}'")),
        }
        if REPLAY.with(|r| r.borrow().as_ref().unwrap().failed.is_some()) {
            break;
        }
    }
}

fn replay_dialog(kind: EeWonLobbyDlgKind, body: Vec<&'static str>) {
    let dlg: *mut EeWonLobbyDlg = ee_won_lobbydlg_new(0xD1A as *mut c_void, kind, Some(dlg_host));
    REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: body, at: 0, logic: std::ptr::null(), failed: None }));
    while let Some(line) = next_line() {
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "call" => {
                let n = nums(&t[1..]);
                let call = dlg_call_of(n[0]);
                let c = if call == EeWonLobbyDlgCall::SetText && kind == EeWonLobbyDlgKind::OkCancel {
                    BUTTONS.as_ptr() as isize
                } else {
                    n[3]
                };
                // SAFETY: the live object
                let r = unsafe { ee_won_lobbydlg_call(dlg, call, n[1], n[2], c) };
                let ret = next_line().unwrap_or("-");
                if ret != format!("ret {r}") {
                    fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                }
            }
            // SAFETY: the live object, not used again
            "free" => unsafe { ee_won_lobbydlg_free(dlg) },
            _ => fail(format!("unexpected line '{line}'")),
        }
        if REPLAY.with(|r| r.borrow().as_ref().unwrap().failed.is_some()) {
            break;
        }
    }
}

/// Replays every sequence; the numbers of logic and dialog sequences replayed.
fn replay(golden: &'static str) -> Result<(usize, usize), String> {
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    let (mut logics, mut dialogs) = (0, 0);
    for w in starts.windows(2) {
        let head: Vec<&str> = lines[w[0]].split(' ').collect();
        let body = lines[w[0] + 1..w[1]].to_vec();
        if head[2] == "logic" {
            logics += 1;
            replay_logic(body);
        } else {
            dialogs += 1;
            let kind = if head[3] == "8" { EeWonLobbyDlgKind::Status } else { EeWonLobbyDlgKind::OkCancel };
            replay_dialog(kind, body);
        }
        if let Some(f) = REPLAY.with(|r| r.borrow_mut().take().unwrap().failed) {
            return Err(format!("{}: {f}", lines[w[0]]));
        }
    }
    Ok((logics, dialogs))
}

#[test]
fn golden_replays() {
    let (logics, dialogs) = replay(GOLDEN).unwrap();
    assert!(logics >= 40 && dialogs >= 40, "{logics} {dialogs}");
}

#[test]
fn golden_fails_on_a_changed_op() {
    // the first OkCancelSetText's Yes/No flag flipped: the replay must fail
    let at = GOLDEN.find("\nop 38 ").expect("an OkCancelSetText op") + 1;
    let end = GOLDEN[at..].find(" = ").unwrap() + at;
    let mut changed = GOLDEN.to_string();
    let flag = &GOLDEN[end - 1..end];
    changed.replace_range(end - 1..end, if flag == "1" { "0" } else { "1" });
    let leaked: &'static str = Box::leak(changed.into_boxed_str());
    assert!(replay(leaked).is_err());
}

// ---- unit tests over a recording host

type Rec = (u32, isize, isize, isize, isize);

thread_local! {
    static OPS: RefCell<Vec<Rec>> = const { RefCell::new(Vec::new()) };
    static ANSWERS: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn rec_host(_l: *mut c_void, op: EeWonDlgLogicOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    OPS.with(|o| o.borrow_mut().push((op as u32, a, b, c, d)));
    ANSWERS.with(|x| x.borrow().iter().find(|(o, _)| *o == op as u32).map(|(_, r)| *r).unwrap_or(0))
}

fn with_answers(answers: &[(EeWonDlgLogicOp, isize)]) -> EeWonDlgLogic {
    OPS.with(|o| o.borrow_mut().clear());
    ANSWERS.with(|x| *x.borrow_mut() = answers.iter().map(|(o, r)| (*o as u32, *r)).collect());
    EeWonDlgLogic::new(std::ptr::null_mut(), Some(rec_host))
}

fn ops() -> Vec<u32> {
    OPS.with(|o| o.borrow().iter().map(|x| x.0).collect())
}

#[test]
fn a_status_box_waits_on_the_stack() {
    use EeWonDlgLogicOp as O;
    let l = with_answers(&[(O::FindStatus, 0x200), (O::WmPopups, -1), (O::RootWindow, 0x700), (O::LobbyRoot, 0x600)]);
    l.call(EeWonDlgLogicCall::Init, 0x77, 0, 0, 0, 0);
    // no window manager: the popups setting stays off
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 1, 0, 0, 0, 0), 0);
    l.call(EeWonDlgLogicCall::ShowStatus, 1, 2, 1, 0, 0);
    let o = ops();
    assert!(o.contains(&(O::StatusSetText as u32)) && o.contains(&(O::RunDialog as u32)));
    // the box stays on the stack, not modal, until the next dialog clears it
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 7, 0, 0, 0, 0), 1);
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 2, 0, 0, 0, 0), 0);
    l.call(EeWonDlgLogicCall::ClearDialogs, 0, 0, 0, 0, 0);
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 7, 0, 0, 0, 0), 0);
    l.release_all();
}

#[test]
fn no_window_means_no_dialog() {
    use EeWonDlgLogicOp as O;
    let l = with_answers(&[(O::RootWindow, 0)]);
    assert_eq!(l.call(EeWonDlgLogicCall::DoDialog, 0x300, 0, 0, 0, 0), -1);
    assert!(!ops().contains(&(O::RunDialog as u32)));
}

#[test]
fn a_pending_operation_is_cancelled_when_its_box_closes() {
    use EeWonDlgLogicOp as O;
    let l = with_answers(&[(O::OpGetStatus, WS_PENDING as isize), (O::LobbySuspended, 1)]);
    l.call(EeWonDlgLogicCall::RunOpWithStatus, 0x4000, 1, 2, 1, 60000);
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 0, 0, 0, 0, 0), 1);
    assert_eq!(l.call(EeWonDlgLogicCall::Field, 4, 0, 0, 0, 0), 0);
    let o = ops();
    assert_eq!(o.iter().filter(|x| **x == O::OpKill as u32).count(), 1);
    assert_eq!(o.iter().filter(|x| **x == O::AddRefOp as u32).count(), 1);
    assert_eq!(o.iter().filter(|x| **x == O::ReleaseOp as u32).count(), 1);
}

#[test]
fn ok_cancel_body_widths() {
    use crate::lobbydlg::ok_cancel_body_width as w;
    assert_eq!((w(0), w(249), w(250), w(799), w(800), w(5000)), (0, 249, 250, 250, 400, 400));
}
