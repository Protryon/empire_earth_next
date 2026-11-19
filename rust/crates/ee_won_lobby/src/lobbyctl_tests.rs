use super::*;
use std::cell::RefCell;

// ---- the golden replay: golden_lobbyctl.txt, recorded by tests/won/won_lobbyctl_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line, answered by its "=" value; a "reenter" line
// re-enters the object with that event (up to its "back" line); every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_lobbyctl.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
    ctl: *const EeWonLobbyCtl,
    // round 6: the game is a real one (the direct path); its ops are the "gop" lines
    game: *mut crate::eegame::EeWonEeGame,
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

unsafe extern "C" fn replay_host(_p: *mut c_void, op: EeWonLobbyCtlOp, a: isize, b: isize, c: isize) -> isize {
    if op == EeWonLobbyCtlOp::GameRust {
        // a field read, not recorded
        return REPLAY.with(|r| r.borrow().as_ref().unwrap().game) as isize;
    }
    expect(format!("op {} {a} {b} {c}", op as u32))
}

fn replay_game_ops(op: crate::eegame::EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    expect(format!("gop {} {a} {b} {c} {d}", op as u32))
}

/// The next recorded op must be `made`: its answer, after the re-entry recorded behind it.
fn expect(made: String) -> isize {
    let Some(line) = next_line() else {
        fail(format!("op past the end: {made}"));
        return 0;
    };
    let (head, answer) = line.split_once(" = ").unwrap_or((line, "0"));
    if head != made {
        fail(format!("expected '{line}', the Rust made '{made}'"));
        return 0;
    }
    let answer = answer.parse().unwrap();
    // a notifying set or clear the recording re-entered
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let n: Vec<isize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            let ctl = REPLAY.with(|r| r.borrow().as_ref().unwrap().ctl);
            // SAFETY: the live object (re-entry through a shared reference, as the C++ does)
            unsafe { ee_won_lobbyctl_call(ctl, EeWonLobbyCtlCall::Event, 0xE1, n[0], n[1]) };
            if next_line() != Some("back") {
                fail(format!("{next}: the re-entry made fewer ops than recorded"));
            }
        }
    }
    answer
}

fn call_of(n: u32) -> EeWonLobbyCtlCall {
    use EeWonLobbyCtlCall as C;
    [C::Init, C::Event, C::Paint, C::ResetBrowser, C::Field][n as usize - 1]
}

fn kind_of(n: u32) -> EeWonLobbyCtlKind {
    use EeWonLobbyCtlKind as K;
    [K::Browser, K::Staging, K::LanOptions, K::NetOptions, K::UserList, K::GameOptions, K::PlayerOptions][n as usize]
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let _l = crate::eegame::testgame::lock();
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let head: Vec<&str> = lines[w[0]].split(' ').collect();
        let game = crate::eegame::testgame::new(replay_game_ops);
        let p = ee_won_lobbyctl_new(0x9090 as *mut c_void, kind_of(head[3].parse().unwrap()), Some(replay_host));
        REPLAY.with(|r| {
            *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None, ctl: p, game })
        });
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
                "gset" | "gslot" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    let (o, s) =
                        if t[0] == "gset" { (vec![(n[0], n[1])], vec![]) } else { (vec![], vec![(n[0], n[1], n[2])]) };
                    crate::eegame::testgame::setup(game, &o, &s);
                }
                // SAFETY: the live game, freed once
                "gfree" => unsafe { crate::eegame::ee_won_eegame_free(game) },
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
    // the first enable recorded as a show
    let a = format!("op {} ", EeWonLobbyCtlOp::SetEnabled as u32);
    let b = format!("op {} ", EeWonLobbyCtlOp::SetVisible as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests over a recording host

type Answer = Box<dyn Fn(EeWonLobbyCtlOp, isize, isize) -> isize>;

thread_local! {
    static LOG: RefCell<Vec<(EeWonLobbyCtlOp, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
    static ANSWER: RefCell<Answer> = RefCell::new(Box::new(|_, _, _| 0));
    // the real game the game room's kinds call directly, and its ops
    static GAME: std::cell::Cell<isize> = const { std::cell::Cell::new(0) };
    static GLOG: RefCell<Vec<(crate::eegame::EeWonEeGameOp, isize)>> = const { RefCell::new(Vec::new()) };
}

fn game_ops(op: crate::eegame::EeWonEeGameOp, a: isize, _b: isize, _c: isize, _d: isize) -> isize {
    GLOG.with(|l| l.borrow_mut().push((op, a)));
    0
}

/// A real game (its options and slots' raw options set) for the game room's kinds; the caller holds the lock.
fn with_game(options: &[(isize, isize)], slots: &[(isize, isize, isize)]) -> *mut crate::eegame::EeWonEeGame {
    let g = crate::eegame::testgame::new(game_ops);
    crate::eegame::testgame::setup(g, options, slots);
    GAME.with(|x| x.set(g as isize));
    GLOG.with(|l| l.borrow_mut().clear());
    g
}

fn glog() -> Vec<(crate::eegame::EeWonEeGameOp, isize)> {
    GLOG.with(|l| l.borrow().clone())
}

unsafe extern "C" fn log_host(_p: *mut c_void, op: EeWonLobbyCtlOp, a: isize, b: isize, c: isize) -> isize {
    if op == EeWonLobbyCtlOp::GameRust {
        return GAME.with(|x| x.get());
    }
    LOG.with(|l| l.borrow_mut().push((op, a, b, c)));
    if op == EeWonLobbyCtlOp::Find || op == EeWonLobbyCtlOp::FindOptional {
        return 0x100 + b * 0x10;
    }
    ANSWER.with(|f| f.borrow()(op, a, b))
}

fn made(kind: EeWonLobbyCtlKind, answer: impl Fn(EeWonLobbyCtlOp, isize, isize) -> isize + 'static) -> EeWonLobbyCtl {
    ANSWER.with(|a| *a.borrow_mut() = Box::new(answer));
    let p = EeWonLobbyCtl::new(std::ptr::null_mut(), kind, Some(log_host));
    p.call(EeWonLobbyCtlCall::Init, 0xC0, 0, 0);
    LOG.with(|l| l.borrow_mut().clear());
    p
}

fn log() -> Vec<(EeWonLobbyCtlOp, isize, isize, isize)> {
    LOG.with(|l| l.borrow().clone())
}

fn event(p: &EeWonLobbyCtl, e: EeWonLobbyCtlEvent, component: isize) {
    p.call(EeWonLobbyCtlCall::Event, 0xE1, e as isize, component);
}

#[test]
fn the_connection_speeds_and_their_entries() {
    let speeds: Vec<i32> = (-2..8).map(connection_speed).collect();
    assert_eq!(speeds, [5, 5, 5, 15, 25, 35, 45, 55, 55, 55]);
    assert_eq!(connection_speed_index(4), 0);
    assert_eq!(connection_speed_index(5), 0);
    assert_eq!(connection_speed_index(15), 1);
    assert_eq!(connection_speed_index(54), 4);
    assert_eq!(connection_speed_index(55), 5);
    assert_eq!(connection_speed_index(1000), 5);
}

#[test]
fn the_slots_hold_each_kinds_lookups_in_order() {
    // every name distinct within its kind and every member filled once
    let kinds = [0..=10, 11..=12, 13..=29, 30..=33, 34..=35, 36..=54, 55..=102];
    for k in kinds {
        let mut members = std::collections::BTreeSet::new();
        for i in k {
            let m = SLOTS[i].member;
            assert!(m < 0 || members.insert(m), "slot {i}");
        }
    }
    assert!(ee_won_lobbyctl_slot(SLOTS.len() as u32).is_null());
}

#[test]
fn a_host_may_start_only_a_startable_game_and_a_guest_relabels_on_ready() {
    use EeWonLobbyCtlOp as O;
    // a guest, not ready, then ready
    let ready = std::rc::Rc::new(Cell::new(0));
    let r = ready.clone();
    let p = made(EeWonLobbyCtlKind::Staging, move |op, _, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec => 1,
        O::IAmReady => r.get(),
        O::GameCanReady => 1,
        _ => 0,
    });
    event(&p, EeWonLobbyCtlEvent::JoinedGame, 0);
    let texts: Vec<isize> = log().iter().filter(|x| x.0 == O::ButtonSetText).map(|x| x.2).collect();
    assert_eq!(texts, [EeWonLobbyCtlString::LeaveGame as isize, EeWonLobbyCtlString::Ready as isize]);
    LOG.with(|l| l.borrow_mut().clear());
    ready.set(1);
    event(&p, EeWonLobbyCtlEvent::ReadyChanged, 0);
    let l = log();
    assert!(l.contains(&(O::ButtonSetText, 0x100 + 12 * 0x10, EeWonLobbyCtlString::NotReady as isize, 0)));
    assert!(l.contains(&(O::SetEnabled, 0x100 + 12 * 0x10, 1, 0)));
    assert_eq!(p.call(EeWonLobbyCtlCall::Field, 102, 0, 0), 1);
    p.release_all();
}

#[test]
fn the_kick_button_is_for_the_host_and_never_the_captain() {
    use EeWonLobbyCtlOp as O;
    let p = made(EeWonLobbyCtlKind::UserList, |op, a, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec | O::GameIsHost => 1,
        O::GameClients => 0x31,
        O::ListGetSel => 0x40,
        O::ItemClient => 0x50,
        O::ClientId => (a == 0x50) as isize * 7,
        O::GameCaptainId => 7,
        _ => 0,
    });
    event(&p, EeWonLobbyCtlEvent::JoinedGame, 0);
    // the selected player is the captain: Kick disabled
    assert_eq!(log().last().unwrap(), &(O::SetEnabled, 0x100 + 35 * 0x10, 0, 0));
    p.release_all();
}

#[test]
fn the_icon_is_the_first_match() {
    use EeWonLobbyCtlOp as O;
    let p = made(EeWonLobbyCtlKind::UserList, |op, _, b| match op {
        O::ItemClientIs => {
            (b == EeWonLobbyCtlClientTest::IsAway as isize || b == EeWonLobbyCtlClientTest::IsCaptain as isize) as isize
        }
        _ => 0,
    });
    p.update_icon(0x60);
    assert_eq!(log().last().unwrap(), &(O::ItemSetImage, 0x60, EeWonLobbyCtlImage::Away as isize, 0));
    p.release_all();
}

#[test]
fn the_dirty_word_and_afk_checks_are_set_without_their_event() {
    use EeWonLobbyCtlOp as O;
    let p = made(EeWonLobbyCtlKind::NetOptions, |_, _, _| 0);
    p.refresh_options();
    let notify: Vec<(isize, isize)> = log().iter().filter(|x| x.0 == O::ButtonSetCheck).map(|x| (x.1, x.3)).collect();
    // FilterDirtyWordsCheck (slot 33) and DoAutoAFKCheck (slot 32) first, without the event
    assert_eq!(notify[0], (0x100 + 33 * 0x10, 0));
    assert_eq!(notify[1], (0x100 + 32 * 0x10, 0));
    assert!(notify[2..].iter().all(|x| x.1 == 1));
    p.release_all();
}

#[test]
fn the_browser_goes_home_on_its_first_paint_only() {
    use EeWonLobbyCtlOp as O;
    let p = made(EeWonLobbyCtlKind::Browser, |_, _, _| 0);
    p.call(EeWonLobbyCtlCall::Paint, 0x66, 0, 0);
    p.call(EeWonLobbyCtlCall::Paint, 0x66, 0, 0);
    let homes = log().iter().filter(|x| x.0 == O::BrowserGoHome).count();
    assert_eq!(homes, 1);
    p.call(EeWonLobbyCtlCall::ResetBrowser, 0, 0, 0);
    p.call(EeWonLobbyCtlCall::Paint, 0x66, 0, 0);
    assert_eq!(log().iter().filter(|x| x.0 == O::BrowserGoHome).count(), 2);
    // the members released in reverse order
    LOG.with(|l| l.borrow_mut().clear());
    p.release_all();
    let released: Vec<isize> = log().iter().map(|x| x.1).collect();
    assert_eq!(released, (0..=10).rev().map(|i| 0x100 + i * 0x10).collect::<Vec<_>>());
}

#[test]
fn the_unit_limit_is_shown_and_sent_in_steps_of_fifty() {
    use EeWonLobbyCtlOp as O;
    let _l = crate::eegame::testgame::lock();
    let g = with_game(&[(9, 300)], &[]);
    let limits = 0x100 + (39 + 9) * 0x10;
    let p = made(EeWonLobbyCtlKind::GameOptions, move |op, a, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec | O::GameIsHost | O::IAmCaptain | O::ComboHasSel | O::IsCombo | O::IsButton => 1,
        O::ComboGetSel if a == limits => 7,
        O::EventControlId => 10,
        _ => 0,
    });
    event(&p, EeWonLobbyCtlEvent::JoinedGame, 0);
    assert!(log().contains(&(O::ComboSetSel, limits, 5, 0)));
    LOG.with(|l| l.borrow_mut().clear());
    event(&p, EeWonLobbyCtlEvent::ComboSelChanged, 0);
    // the game's unit limit changed to (7 + 1) * 50 and sent
    assert_eq!(crate::eegame::game_call(g as isize, crate::eegame::EeWonEeGameCall::GetOption, 9, 0, 0), 400);
    assert!(glog().iter().any(|x| x.0 == crate::eegame::EeWonEeGameOp::NewBuffer));
    p.release_all();
    // SAFETY: the test's game, freed once
    unsafe { crate::eegame::ee_won_eegame_free(g) };
}

#[test]
fn a_saved_game_needs_the_file_dialogs_load() {
    use EeWonLobbyCtlOp as O;
    let _l = crate::eegame::testgame::lock();
    let g = with_game(&[], &[]);
    let types = 0x100 + 39 * 0x10;
    let p = made(EeWonLobbyCtlKind::GameOptions, move |op, a, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec | O::IAmCaptain | O::ComboHasSel => 1,
        O::ComboGetSel if a == types => 1,
        O::EventControlId => 1,
        O::ChooseSavedGame => 100002,
        _ => 0,
    });
    event(&p, EeWonLobbyCtlEvent::JoinedGame, 0);
    LOG.with(|l| l.borrow_mut().clear());
    event(&p, EeWonLobbyCtlEvent::ComboSelChanged, 0);
    // cancelled: the combo box back to the game's type, nothing sent
    let l = log();
    assert_eq!(l.last().unwrap(), &(O::ComboSetSel, types, 0, 0));
    assert!(glog().is_empty(), "nothing sent");
    p.release_all();
    // SAFETY: the test's game, freed once
    unsafe { crate::eegame::ee_won_eegame_free(g) };
}

#[test]
fn a_player_row_shows_a_seated_player_and_sends_our_own_changes() {
    use EeWonLobbyCtlOp as O;
    // slot 2 is ours, a client's, not observing, 40 percent transferred; a random map; not the host
    let _l = crate::eegame::testgame::lock();
    let g = with_game(&[], &[(2, 0, 3), (2, 5, 40), (2, 2, 7)]);
    let p = made(EeWonLobbyCtlKind::PlayerOptions, move |op, _, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec => 1,
        O::MyPlayer => 0x40,
        O::PlayerSlot => 2,
        O::ComboGetSel => 3,
        _ => 0,
    });
    event_players(&p, EeWonLobbyCtlEvent::JoinedGame, 0);
    assert_eq!(p.call(EeWonLobbyCtlCall::Field, 104, 0, 0), 2);
    let l = log();
    // slot 2's row: the name, the colour button enabled and set, the percent shown
    let color = 0x100 + (55 + 2 * 6 + 2) * 0x10;
    let transfer = 0x100 + (55 + 2 * 6 + 5) * 0x10;
    // the slot has no client here: its name is the empty string
    assert!(l.contains(&(O::ComboSetName, 0x100 + (55 + 12) * 0x10, 0, 0)));
    assert!(l.contains(&(O::SetEnabled, color, 1, 0)));
    assert!(l.contains(&(O::ColorSetColor, color, 7, 0)));
    assert!(l.contains(&(O::LabelSetPercent, transfer, 40, 0)));
    // the citizen combo (id 2 * 6 + 4): the selection + 1 sent as option 3 of slot 2
    LOG.with(|l| l.borrow_mut().clear());
    GLOG.with(|l| l.borrow_mut().clear());
    p.call(EeWonLobbyCtlCall::Event, 0xE1, EeWonLobbyCtlEvent::ComboSelChanged as isize, 0);
    assert!(glog().is_empty(), "the control id answered 0");
    let p2 = made(EeWonLobbyCtlKind::PlayerOptions, move |op, _, _| match op {
        O::MyGame => 0x30,
        O::GameMatchesSpec => 1,
        O::EventControlId => 2 * 6 + 4,
        O::ComboGetSel => 3,
        _ => 0,
    });
    event_players(&p2, EeWonLobbyCtlEvent::JoinedGame, 0);
    LOG.with(|l| l.borrow_mut().clear());
    GLOG.with(|l| l.borrow_mut().clear());
    event_players(&p2, EeWonLobbyCtlEvent::ComboSelChanged, 0);
    // the citizen (4) reaches the game as slot 2's option 3; not the host, it is refused there and the row resynced
    assert_eq!(glog().last(), Some(&(crate::eegame::EeWonEeGameOp::SyncPlayer, 2)));
    // SAFETY: the test's game, freed once
    unsafe { crate::eegame::ee_won_eegame_free(g) };
}

fn event_players(p: &EeWonLobbyCtl, e: EeWonLobbyCtlEvent, component: isize) {
    p.call(EeWonLobbyCtlCall::Event, 0xE1, e as isize, component);
}

/// The controls' strings as resource rows (round 17): every one of the nine named.
#[test]
fn string_rows() {
    use crate::resource::EeWonLobbyRes as R;
    assert_eq!(string_row(EeWonLobbyCtlString::Ready as isize), Some(R::GameStagingCtrl_Ready_String));
    assert_eq!(
        string_row(EeWonLobbyCtlString::AddressUnavailable as isize),
        Some(R::LobbyOptions_AddressUnavailable_String)
    );
    assert_eq!((0..9).filter(|&v| string_row(v).is_some()).count(), 9);
    assert_eq!(string_row(9), None);
}
