use super::*;
use crate::lobbyctl::{ee_won_lobbyctl_call, ee_won_lobbyctl_free, ee_won_lobbyctl_new, EeWonLobbyCtlCall as C};
use std::cell::RefCell;
use std::ffi::c_void;

// ---- the golden replay: golden_netctl.txt, recorded by tests/won/won_netctl_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line (a struct by its contents, a made text by its
// bytes), answered by its "=" value (a recorded text's bytes written to the Rust's buffer); a "reenter" line
// re-enters the object with that event (up to its "back" line); every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_netctl.txt");

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
        r.as_ref().unwrap().lines.get(r.as_ref().unwrap().at).copied()
    })
}

/// The recorded line's text in quotes after `prefix` (the bytes a buffer op wrote).
fn quoted<'a>(head: &'a str, prefix: &str) -> Option<&'a str> {
    head.strip_prefix(prefix)?.strip_prefix('\'')?.strip_suffix('\'')
}

unsafe extern "C" fn replay_host(_p: *mut c_void, op: Op, a: isize, b: isize, c: isize) -> isize {
    let n = op as u32;
    // the op as recorded, and the buffer (with its size) a recorded text is written to
    let (made, buffer): (String, Option<(*mut u8, usize)>) = match op {
        Op::TextNew => {
            // SAFETY: the Rust's bytes
            let bytes = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            (format!("op {n} '{}'", String::from_utf8_lossy(bytes)), None)
        }
        Op::TextBytes => (format!("op {n} {a} "), Some((b as *mut u8, c as usize))),
        Op::HttpDocPath => (format!("op {n} {a} {b} "), Some((c as *mut u8, 1024))),
        Op::DeliverCreateAccount => {
            // SAFETY: the Rust's seven controls
            let f = unsafe { std::slice::from_raw_parts(a as *const isize, 7) };
            (format!("op {n} create {} {} {} {} {} {} {}", f[0], f[1], f[2], f[3], f[4], f[5], f[6]), None)
        }
        Op::BrowserShowFiles => {
            // SAFETY: the Rust's two paths
            let f = unsafe { &*(b as *const EeWonLobbyCtlFiles) };
            let sys = unsafe { std::slice::from_raw_parts(f.sys, f.sys_len) };
            let game = unsafe { std::slice::from_raw_parts(f.game, f.game_len) };
            (format!("op {n} {a} '{}' '{}'", String::from_utf8_lossy(sys), String::from_utf8_lossy(game)), None)
        }
        Op::LabelSetFormat3 => {
            // SAFETY: the Rust's three texts
            let t = unsafe { std::slice::from_raw_parts(c as *const isize, 3) };
            (format!("op {n} {a} {b} f3 {} {} {}", t[0], t[1], t[2]), None)
        }
        _ => (format!("op {n} {a} {b} {c}"), None),
    };
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.rsplit_once(" = ").unwrap_or((line, "0"));
    match buffer {
        Some((p, cap)) => {
            let Some(text) = quoted(head, &made) else {
                fail(format!("expected '{line}', the Rust made '{made}'"));
                return 0;
            };
            // SAFETY: the Rust's buffer of `cap` bytes
            unsafe { std::ptr::copy_nonoverlapping(text.as_ptr(), p, text.len().min(cap)) };
        }
        None => {
            if head != made {
                fail(format!("expected '{line}', the Rust made '{made}'"));
                return 0;
            }
        }
    }
    let answer = answer.parse().unwrap();
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let v: Vec<isize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            let ctl = REPLAY.with(|r| r.borrow().as_ref().unwrap().ctl);
            // SAFETY: the live object (re-entry through a shared reference, as the C++ does)
            unsafe { ee_won_lobbyctl_call(ctl, C::Event, 0xE1, v[0], v[1]) };
            if next_line() != Some("back") {
                fail(format!("{next}: the re-entry made fewer ops than recorded"));
            }
        }
    }
    answer
}

fn call_of(n: u32) -> C {
    [C::Init, C::Event, C::Paint, C::ResetBrowser, C::Field, C::Timer, C::Row, C::Cfg, C::SetField, C::Page]
        [n as usize - 1]
}

fn kind_of(n: u32) -> Kind {
    use Kind as K;
    [
        K::Room,
        K::Friends,
        K::Users,
        K::QuickPlay,
        K::Login,
        K::CreateAccount,
        K::UpdateAccount,
        K::LostPassword,
        K::LostUserName,
        K::LoginHelp,
        K::LoginStatus,
        K::InitFailed,
        K::Motd,
        K::LoginScreen,
        K::DirectConnect,
        K::RoomLabel,
    ][n as usize - 9]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let head: Vec<&str> = lines[w[0]].split(' ').collect();
        let p = ee_won_lobbyctl_new(0x9090 as *mut c_void, kind_of(head[3].parse().unwrap()), Some(replay_host));
        REPLAY.with(|r| {
            *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None, ctl: p })
        });
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let v: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    // SAFETY: the live object
                    let r = unsafe { ee_won_lobbyctl_call(p, call_of(v[0] as u32), v[1], v[2], v[3]) };
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
    assert!(n >= 80, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_op() {
    let a = format!("op {} ", Op::SetEnabled as u32);
    let b = format!("op {} ", Op::SetVisible as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

#[test]
fn an_email_needs_an_at_after_the_first_character_and_a_dot_with_text_after_it() {
    for (e, ok) in [
        ("a@b.c", true),
        ("@b.c", false),
        ("ab@c", false),
        ("ab@c.", false),
        ("a.b@c", false),
        ("q@w.e.r", true),
        ("", false),
        ("a@.b", true),
    ] {
        assert_eq!(is_valid_email(e.as_bytes()), ok, "{e}");
    }
}

#[test]
fn a_birth_date_is_in_the_twentieth_century() {
    assert!(is_valid_birth_date(1, 1, 1890) && is_valid_birth_date(12, 31, 1999));
    assert!(!is_valid_birth_date(0, 1, 1950) && !is_valid_birth_date(13, 1, 1950));
    assert!(!is_valid_birth_date(1, 32, 1950) && !is_valid_birth_date(1, 1, 1889) && !is_valid_birth_date(1, 1, 2000));
}

#[test]
fn every_small_kind_holds_its_members() {
    assert_eq!(net_member_count(Kind::CreateAccount), 14);
    assert_eq!(net_member_count(Kind::UpdateAccount), 13);
    assert_eq!(net_member_count(Kind::Users), 5);
    assert_eq!(net_member_count(Kind::RoomLabel), 1);
}
