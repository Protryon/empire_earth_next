use super::*;
use std::cell::RefCell;

// ---- the golden replay: golden_lobbypopup.txt, recorded by tests/won/won_lobbypopup_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line, answered by its "=" value; every call's result
// must be its "ret" line.

const GOLDEN: &str = include_str!("golden_lobbypopup.txt");

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

unsafe extern "C" fn replay_host(_p: *mut c_void, op: EeWonLobbyPopupOp, a: isize, b: isize, c: isize) -> isize {
    let made = format!("op {} {a} {b} {c}", op as u32);
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

fn call_of(n: u32) -> EeWonLobbyPopupCall {
    use EeWonLobbyPopupCall as C;
    [C::Construct, C::Init, C::Event, C::Field][n as usize - 1]
}

fn kind_of(n: u32) -> EeWonLobbyPopupKind {
    [EeWonLobbyPopupKind::Client, EeWonLobbyPopupKind::Game, EeWonLobbyPopupKind::Friend][n as usize]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let head: Vec<&str> = lines[w[0]].split(' ').collect();
        let p = ee_won_lobbypopup_new(0x9090 as *mut c_void, kind_of(head[3].parse().unwrap()), Some(replay_host));
        REPLAY.with(|r| *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None }));
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    // SAFETY: the live object
                    let r = unsafe { ee_won_lobbypopup_call(p, call_of(n[0] as u32), n[1], n[2], n[3]) };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                // SAFETY: the live object, freed once
                "free" => unsafe { ee_won_lobbypopup_free(p) },
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
    // the first separator recorded as a clear
    let sep = format!("op {} ", EeWonLobbyPopupOp::MenuAddSeparator as u32);
    let clear = format!("op {} ", EeWonLobbyPopupOp::MenuClear as u32);
    let changed = GOLDEN.replacen(&sep, &clear, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests over a recording host

type Answer = Box<dyn Fn(EeWonLobbyPopupOp, isize, isize) -> isize>;

thread_local! {
    static LOG: RefCell<Vec<(EeWonLobbyPopupOp, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
    static ANSWER: RefCell<Answer> = RefCell::new(Box::new(|_, _, _| 0));
}

unsafe extern "C" fn log_host(_p: *mut c_void, op: EeWonLobbyPopupOp, a: isize, b: isize, c: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b, c)));
    if op == EeWonLobbyPopupOp::MenuNew {
        return 0x44;
    }
    ANSWER.with(|f| f.borrow()(op, a, b))
}

fn made(
    kind: EeWonLobbyPopupKind,
    answer: impl Fn(EeWonLobbyPopupOp, isize, isize) -> isize + 'static,
) -> EeWonLobbyPopup {
    ANSWER.with(|a| *a.borrow_mut() = Box::new(answer));
    let p = EeWonLobbyPopup::new(std::ptr::null_mut(), kind, Some(log_host));
    p.call(EeWonLobbyPopupCall::Construct, 0, 0, 0);
    LOG.with(|l| l.borrow_mut().clear());
    p
}

fn items() -> Vec<(isize, isize)> {
    LOG.with(|l| l.borrow().iter().filter(|x| x.0 == EeWonLobbyPopupOp::MenuAddItem).map(|x| (x.2, x.3)).collect())
}

#[test]
fn a_lan_captain_may_kick_another_player_but_not_himself() {
    use EeWonLobbyPopupOp as O;
    // the client 1 seen by the client 2, the captain of a LAN chat room (flags 2: LAN, not a game room)
    let p = made(EeWonLobbyPopupKind::Client, |op, a, _| match op {
        O::ClientIsCaptain => 1,
        O::ClientId => a,
        _ => 0,
    });
    p.call(EeWonLobbyPopupCall::Init, 1, 2, 2);
    let s = |s: EeWonLobbyPopupString, n: isize| (s as isize, n);
    assert_eq!(items().last(), Some(&s(EeWonLobbyPopupString::Kick, 7)));
    LOG.with(|l| l.borrow_mut().clear());
    p.call(EeWonLobbyPopupCall::Init, 2, 2, 2);
    assert!(!items().contains(&s(EeWonLobbyPopupString::Kick, 7)));
    p.release_all();
}

#[test]
fn a_game_in_progress_is_not_pinged_when_the_config_says_so() {
    use EeWonLobbyPopupOp as O;
    let p = made(EeWonLobbyPopupKind::Game, |op, _, _| match op {
        O::GameInProgress | O::ConfigNoPingInProgress => 1,
        O::Config => 0x70,
        O::GameType => 3,
        _ => 0,
    });
    p.call(EeWonLobbyPopupCall::Init, 0x50, 0, 0);
    let got: Vec<isize> = items().iter().map(|x| x.1).collect();
    // no ping (2) and no details query (3); show details (4), remove (6) for a direct-connect game
    assert_eq!(got, [4, 6]);
    p.release_all();
}

#[test]
fn a_click_delivers_the_subjects_action_then_closes() {
    use EeWonLobbyPopupOp as O;
    let p = made(EeWonLobbyPopupKind::Friend, |op, _, _| match op {
        O::EventItem => 0x99,
        O::ItemMenuId => 3,
        _ => 0,
    });
    p.call(EeWonLobbyPopupCall::Init, 0x51, 6, 0);
    LOG.with(|l| l.borrow_mut().clear());
    p.call(EeWonLobbyPopupCall::Event, 0xE1, EeWonLobbyPopupEvent::ItemUpClicked as isize, 0);
    let log = LOG.with(|l| l.borrow().clone());
    assert_eq!(log[log.len() - 2], (O::DeliverFriend, 3, 0x51, 6));
    assert_eq!(log[log.len() - 1].0, O::ClosePopup);
    // the members released in reverse order: the friend, then the menu
    LOG.with(|l| l.borrow_mut().clear());
    p.release_all();
    let released: Vec<isize> = LOG.with(|l| l.borrow().iter().map(|x| x.1).collect());
    assert_eq!(released, [0x51, 0x44]);
}

/// The menus' strings as resource rows (round 17): every one of the 25 named.
#[test]
fn string_rows() {
    use crate::resource::EeWonLobbyRes as R;
    assert_eq!(string_row(EeWonLobbyPopupString::Whisper as isize), Some(R::ClientOptionsCtrl_Whisper_String));
    assert_eq!(
        string_row(EeWonLobbyPopupString::FriendRemove as isize),
        Some(R::FriendOptionsCtrl_RemoveFromFriends_String)
    );
    assert_eq!((0..25).filter(|&v| string_row(v).is_some()).count(), 25);
    assert_eq!(string_row(25), None);
}
