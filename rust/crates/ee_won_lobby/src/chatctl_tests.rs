use super::*;
use crate::lobbyctl::{
    ee_won_lobbyctl_call, ee_won_lobbyctl_free, ee_won_lobbyctl_new, EeWonLobbyCtlCall as C, EeWonLobbyCtlKind,
};
use std::cell::RefCell;
use std::ffi::c_void;

// ---- the golden replay: golden_chatctl.txt, recorded by tests/won/won_chatctl_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line (a link and a formatted line by their fields, a made
// text by its bytes), answered by its "=" value; every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_chatctl.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
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

fn made_of(op: Op, a: isize, b: isize, c: isize) -> String {
    let n = op as u32;
    match op {
        Op::ChatLinkNew => {
            // SAFETY: the Rust's link
            let l = unsafe { &*(a as *const EeWonLobbyCtlLink) };
            format!("op {n} link {} {} {} {} {} {} {}", l.kind, l.obj, l.text, l.id, l.addr, l.font, l.color)
        }
        Op::ChatAddFormatted => {
            // SAFETY: the Rust's line
            let f = unsafe { &*(a as *const EeWonLobbyCtlFormat) };
            let mut s = format!("op {n} fmt {} {} s", f.output, f.format);
            for x in &f.strings[..f.num_strings as usize] {
                s += &format!(" {x}");
            }
            s += " l";
            for x in &f.links[..f.num_links as usize] {
                s += &format!(" {x}");
            }
            s += " c";
            for x in f.colors {
                s += &format!(" {x}");
            }
            s += " f";
            for x in f.fonts {
                s += &format!(" {x}");
            }
            s + &format!(" {}", f.line_break)
        }
        Op::TextNew => {
            // SAFETY: the Rust's bytes
            let bytes = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            format!("op {n} '{}'", String::from_utf8_lossy(bytes))
        }
        Op::TextAppendBytes => {
            // SAFETY: the Rust's bytes
            let bytes = unsafe { std::slice::from_raw_parts(b as *const u8, c as usize) };
            format!("op {n} {a} '{}'", String::from_utf8_lossy(bytes))
        }
        _ => format!("op {n} {a} {b} {c}"),
    }
}

unsafe extern "C" fn replay_host(_p: *mut c_void, op: Op, a: isize, b: isize, c: isize) -> isize {
    let made = made_of(op, a, b, c);
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.rsplit_once(" = ").unwrap_or((line, "0"));
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    answer.parse().unwrap()
}

fn call_of(n: u32) -> C {
    [C::Init, C::Event, C::Paint, C::ResetBrowser, C::Field, C::Timer, C::Row, C::Cfg, C::SetField][n as usize - 1]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let p = ee_won_lobbyctl_new(0x9090 as *mut c_void, EeWonLobbyCtlKind::Chat, Some(replay_host));
        REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None }));
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    // SAFETY: the live object
                    let r = unsafe { ee_won_lobbyctl_call(p, call_of(n[0] as u32), n[1], n[2], n[3]) };
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
    let a = format!("op {} ", Op::EvGet as u32);
    let b = format!("op {} ", Op::EvSetHandled as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

#[test]
fn a_duration_splits_into_its_unit_rounded_up_or_down() {
    assert_eq!(split_duration(0, true), (6, 0));
    assert_eq!(split_duration(60, true), (0, 1));
    assert_eq!(split_duration(61, true), (1, 2));
    assert_eq!(split_duration(3540, true), (1, 59));
    assert_eq!(split_duration(3541, true), (2, 1));
    assert_eq!(split_duration(3601, true), (3, 2));
    assert_eq!(split_duration(86401, true), (5, 2));
    assert_eq!(split_duration(119, false), (0, 1));
    assert_eq!(split_duration(3599, false), (1, 59));
    assert_eq!(split_duration(7199, false), (2, 1));
    assert_eq!(split_duration(86399, false), (3, 23));
    assert_eq!(split_duration(172799, false), (4, 1));
    assert_eq!(split_duration(172800, false), (5, 2));
}

#[test]
fn the_panel_starts_with_the_originals_colours_and_no_client() {
    let s = ChatState::default();
    s.init();
    let c: Vec<i32> = s.colors.iter().map(|c| c.get()).collect();
    assert_eq!(c, [0, 0x800000, 0x8000, 0x800080, 0xff8000, 0xff0000, 0x404040, 0x80, 0xff, 0, 0xa0a0a0]);
    assert_eq!(s.client_id.get(), 0xffff);
}
