use super::*;
use crate::lobbyctl::{
    ee_won_lobbyctl_call, ee_won_lobbyctl_free, ee_won_lobbyctl_new, EeWonLobbyCtlCall as C, EeWonLobbyCtlKind,
};
use std::cell::RefCell;
use std::ffi::c_void;

// ---- the golden replay: golden_srvlist.txt, recorded by tests/won/won_srvlist_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line, answered by its "=" value (a text's bytes written
// where the line carries them); a "reenter" line re-enters the object with that event (up to its "back" line);
// every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_srvlist.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
    ctl: *const EeWonLobbyCtl,
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

unsafe extern "C" fn replay_host(_p: *mut c_void, op: Op, a: isize, b: isize, c: isize) -> isize {
    match op {
        Op::TextNew => {
            // SAFETY: the Rust's bytes
            let bytes = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            expect(format!("op {} '{}'", op as u32, String::from_utf8_lossy(bytes)), None)
        }
        Op::TextBytes => expect(format!("op {} {a}", op as u32), Some((b as *mut u8, c as usize))),
        _ => expect(format!("op {} {a} {b} {c}", op as u32), None),
    }
}

/// The next recorded op must be `made`: its answer, after the re-entry recorded behind it. `bytes`: the buffer a
/// recorded text is written to.
fn expect(made: String, bytes: Option<(*mut u8, usize)>) -> isize {
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.rsplit_once(" = ").unwrap_or((line, "0"));
    let head = match bytes {
        Some((buffer, cap)) => {
            let Some(text) = head.strip_prefix(&format!("{made} '")).and_then(|t| t.strip_suffix('\'')) else {
                fail(format!("expected '{line}', the Rust made '{made}'"));
                return 0;
            };
            let n = text.len().min(cap);
            // SAFETY: the Rust's buffer of `cap` bytes
            unsafe { std::ptr::copy_nonoverlapping(text.as_ptr(), buffer, n) };
            made.as_str()
        }
        None => head,
    };
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    let answer = answer.parse().unwrap();
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let n: Vec<isize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            let ctl = REPLAY.with(|r| r.borrow().as_ref().unwrap().ctl);
            // SAFETY: the live object (re-entry through a shared reference, as the C++ does)
            unsafe { ee_won_lobbyctl_call(ctl, C::Event, 0xE1, n[0], n[1]) };
            if next_line() != Some("back") {
                fail(format!("{next}: the re-entry made fewer ops than recorded"));
            }
        }
    }
    answer
}

fn call_of(n: u32) -> C {
    [C::Init, C::Event, C::Paint, C::ResetBrowser, C::Field, C::Timer, C::Row, C::Cfg][n as usize - 1]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    init_column_map();
    define_ee_column(false);
    define_ee_column(true);
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let p = ee_won_lobbyctl_new(0x9090 as *mut c_void, EeWonLobbyCtlKind::ServerList, Some(replay_host));
        REPLAY.with(|r| {
            *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None, ctl: p })
        });
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let r = if let Some((_, name)) = line.split_once(" '") {
                        // a column's name
                        let name = name.trim_end_matches('\'');
                        let n: Vec<isize> = t[1..3].iter().map(|x| x.parse().unwrap()).collect();
                        // SAFETY: the live object, the name's bytes
                        unsafe {
                            ee_won_lobbyctl_call(
                                p,
                                call_of(n[0] as u32),
                                n[1],
                                name.as_ptr() as isize,
                                name.len() as isize,
                            )
                        }
                    } else {
                        let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                        if n[0] == C::Row as isize {
                            let args = [n[3], n[4], n[5]];
                            // SAFETY: the live object, the row's three arguments
                            unsafe { ee_won_lobbyctl_call(p, C::Row, n[1], n[2], args.as_ptr() as isize) }
                        } else {
                            // SAFETY: the live object
                            unsafe { ee_won_lobbyctl_call(p, call_of(n[0] as u32), n[1], n[2], n[3]) }
                        }
                    };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                // SAFETY: the live object, freed once
                "free" => unsafe { ee_won_lobbyctl_free(p) },
                _ => fail(format!("unexpected line '{line}'")),
            }
            let failed = REPLAY.with(|r| r.borrow().as_ref().unwrap().failed.clone());
            if let Some(f) = failed {
                return Err(format!("{}: {f}", lines[w[0]]));
            }
        }
    }
    Ok(starts.len() - 1)
}

#[test]
fn golden_replay() {
    let n = replay(GOLDEN).unwrap();
    assert!(n >= 60, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_op() {
    let a = format!("op {} ", Op::SetEnabled as u32);
    let b = format!("op {} ", Op::SetVisible as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- the parts without a host

#[test]
fn the_numeric_filter_reads_each_bound() {
    let p = |s: &str| {
        let (mut lo, mut hi) = (-100000, 100000);
        let ok = parse_int_filter(s.as_bytes(), &mut lo, &mut hi);
        (ok, lo, hi)
    };
    assert_eq!(p("<5"), (true, -100000, 4));
    assert_eq!(p("<=5"), (true, -100000, 5));
    assert_eq!(p(">3"), (true, 4, 100000));
    assert_eq!(p(">=3"), (true, 3, 100000));
    assert_eq!(p("=4"), (true, 4, 4));
    assert_eq!(p("7"), (true, 7, 7));
    assert_eq!(p(" >1   <=9 "), (true, 2, 9));
    assert_eq!(p("abc <x"), (false, -100000, 100000));
    assert_eq!(p("<= 5"), (true, 5, 5));
    assert_eq!(p("\t7"), (false, -100000, 100000));
    // out of range: clamped (MSVC's atoi), then one more wraps
    assert_eq!(p(">2147483647"), (true, i32::MIN, 100000));
    assert_eq!(p("=99999999999"), (true, i32::MAX, i32::MAX));
}

#[test]
fn a_sign_starts_a_number_and_a_high_byte_does_not() {
    assert!(is_start_of_number(b'+') && is_start_of_number(b'-') && is_start_of_number(b'0'));
    assert!(!is_start_of_number(0xb2) && !is_start_of_number(b' '));
    assert_eq!(atoi(b"  -12x"), -12);
    assert_eq!(atoi(b"+"), 0);
}

#[test]
fn the_columns_are_defined_by_name_ignoring_case() {
    init_column_map();
    assert_eq!(column_by_name(b"gamename"), Some((2, 0xff)));
    // Address only for direct connect, Ping not on the LAN
    assert_eq!(column_by_name(b"ADDRESS"), Some((3, 4)));
    assert_eq!(column_by_name(b"Ping"), Some((5, 0xfd)));
    assert_eq!(column_by_name(b"Nope"), None);
    define_ee_column(true);
    assert_eq!(column_by_name(b"maptype"), Some((7, 0xff)));
}

#[test]
fn the_row_and_config_calls_are_numbered_in_order() {
    assert_eq!(row_call_of(4), Some(EeWonLobbyCtlRowCall::MouseDown));
    assert_eq!(row_call_of(6), None);
    assert_eq!(cfg_call_of(8), Some(EeWonLobbyCtlCfg::SelGame));
    assert_eq!(cfg_call_of(9), None);
}
