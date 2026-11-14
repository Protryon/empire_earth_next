use super::*;
use std::cell::RefCell;

// ---- the golden replay: golden_lobbydlg.txt, recorded by tests/won/won_lobbydlg_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line (its arguments, a Find's slot by name, file, line
// and type), answered by that line's "=" value; a "reenter" line after an op makes the input's event re-enter the
// object; every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_lobbydlg.txt");
/// round 3's kinds (RoomDialog, TOUDialog, BadUserDialog, ChooseNetworkAdapterDialog), recorded by
/// tests/won/won_lobbyscr_test.cpp the same way
const GOLDEN_SCR: &str = include_str!("golden_lobbyscr.txt");
/// CreditsDialog (kind 14), recorded by tests/won/won_credits_test.cpp (`--print-dialog`)
const GOLDEN_CREDITS: &str = include_str!("golden_credits.txt");
// recorded by tests/won/won_eedlg_test.cpp (round 5: EELobbyConnectDialog)
const GOLDEN_EEDLG: &str = include_str!("golden_eedlg.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    dlg: *const EeWonLobbyDlg,
    failed: Option<String>,
    // round 6: the connect dialog's game is a real one (the direct path); its ops are the "gop" lines
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

fn replay_game_ops(op: crate::eegame::EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    expect(format!("gop {} {a} {b} {c} {d}", op as u32))
}

unsafe extern "C" fn replay_host(_dialog: *mut c_void, op: EeWonLobbyDlgOp, a: isize, b: isize, c: isize) -> isize {
    if op == EeWonLobbyDlgOp::GameRust {
        // a field read, not recorded
        return REPLAY.with(|r| r.borrow().as_ref().unwrap().game) as isize;
    }
    let made = if op == EeWonLobbyDlgOp::Find {
        // SAFETY: a Find's b is a slot of the table
        let slot = unsafe { &*(b as *const EeWonLobbyDlgSlot) };
        // SAFETY: static C strings
        let file = unsafe { CStr::from_ptr(slot.file) }.to_str().unwrap();
        format!("op {} {} {} {} {} {}", op as u32, a, slot_name(slot), file, slot.line, slot.ty as u32)
    } else {
        format!("op {} {} {} {}", op as u32, a, b, c)
    };
    expect(made)
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
    let answer: isize = answer.parse().unwrap();
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let dlg = REPLAY.with(|r| r.borrow().as_ref().unwrap().dlg);
            let n: Vec<isize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            if n.len() == 3 {
                // round 5: a call (the connect dialog's modal loop and EndDialog re-enter it)
                // SAFETY: the replay's live object; the call re-enters through a shared reference, as the C++ does
                let r = unsafe { ee_won_lobbydlg_call(dlg, call_of(n[0] as u32), n[1], n[2], 0) };
                let back = next_line().unwrap_or("-");
                if back != format!("back {r}") {
                    fail(format!("{next}: expected '{back}', the re-entry returned {r}"));
                }
                return answer;
            }
            let comp = n[0];
            // SAFETY: the replay's live object; the call re-enters through a shared reference, as the C++ does
            unsafe {
                ee_won_lobbydlg_call(
                    dlg,
                    EeWonLobbyDlgCall::Event,
                    0xE1,
                    EeWonLobbyDlgEvent::InputTextChanged as isize,
                    comp,
                )
            };
        }
    }
    answer
}

fn call_of(n: u32) -> EeWonLobbyDlgCall {
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
        C::SetUserList,
        C::GetIds,
        C::SelectedAddress,
        C::HoldScrollText,
        C::StartScrolling,
        C::StopScrolling,
        C::ResetScrolling,
        C::AddLine,
        C::ConnectRun,
        C::ConnectEnd,
        C::ConnectSetConnected,
    ][n as usize - 1]
}

fn kind_of(n: u32) -> EeWonLobbyDlgKind {
    use EeWonLobbyDlgKind as K;
    [
        K::LanName,
        K::Password,
        K::AddFriend,
        K::DirectConnect,
        K::LanPort,
        K::CreateRoom,
        K::CreateGame,
        K::AdminAction,
        K::Status,
        K::OkCancel,
        K::Room,
        K::Tou,
        K::BadUser,
        K::ChooseAdapter,
        K::Credits,
        K::Connect,
    ][n as usize]
}

/// Replays every sequence; the number replayed.
fn replay(golden: &'static str) -> Result<usize, String> {
    let _l = crate::eegame::testgame::lock();
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let head: Vec<&str> = lines[w[0]].split(' ').collect();
        let kind = kind_of(head[3].parse().unwrap());
        // the C++ dialog's address as the golden's driver gave it (the ops on the dialog itself name it)
        let game = crate::eegame::testgame::new(replay_game_ops);
        let dlg = ee_won_lobbydlg_new(0xD1A as *mut c_void, kind, Some(replay_host));
        REPLAY.with(|r| {
            *r.borrow_mut() = Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, dlg, failed: None, game })
        });
        let mut game_freed = false;
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "call" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    // SAFETY: the live object
                    let r = unsafe { ee_won_lobbydlg_call(dlg, call_of(n[0] as u32), n[1], n[2], n[3]) };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                // SAFETY: the live object, freed once
                "free" => unsafe { ee_won_lobbydlg_free(dlg) },
                "gset" | "gslot" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    if t[0] == "gset" {
                        crate::eegame::testgame::setup(game, &[(n[0], n[1])], &[]);
                    } else {
                        crate::eegame::testgame::setup(game, &[], &[(n[0], n[1], n[2])]);
                    }
                }
                "gfree" => {
                    game_freed = true;
                    // SAFETY: the live game, freed once
                    unsafe { crate::eegame::ee_won_eegame_free(game) }
                }
                _ => fail(format!("unexpected line '{line}'")),
            }
            let failed = REPLAY.with(|r| r.borrow().as_ref().unwrap().failed.clone());
            if let Some(f) = failed {
                return Err(format!("{}: {f}", lines[w[0]]));
            }
        }
        if !game_freed {
            // the other goldens make no game: it has no ops to free
            // SAFETY: the live game, freed once
            unsafe { crate::eegame::ee_won_eegame_free(game) }
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
fn golden_replay_of_the_screens() {
    let n = replay(GOLDEN_SCR).unwrap();
    assert!(n >= 60, "{n} sequences");
}

#[test]
fn golden_replay_of_the_credits() {
    let n = replay(GOLDEN_CREDITS).unwrap();
    assert!(n >= 60, "{n} sequences");
    // the scrolling text placed and its button hidden, as recorded
    let place = format!("op {} ", EeWonLobbyDlgOp::CreditsPlace as u32);
    let changed = GOLDEN_CREDITS.replacen(&place, &format!("op {} ", EeWonLobbyDlgOp::AddChild as u32), 1);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

#[test]
fn golden_replay_of_the_screens_fails_on_a_changed_op() {
    // the first BrowserTop recorded as a BrowserShowTou
    let top = format!("op {} ", EeWonLobbyDlgOp::BrowserTop as u32);
    let show = format!("op {} ", EeWonLobbyDlgOp::BrowserShowTou as u32);
    let changed = GOLDEN_SCR.replacen(&top, &show, 1);
    assert_ne!(changed, GOLDEN_SCR);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

#[test]
fn golden_replay_fails_on_a_changed_op() {
    // the first RequestFocus recorded as a SetEnabled
    let changed = GOLDEN.replacen("op 5 ", "op 6 ", 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests over a recording host

type Answer = Box<dyn Fn(EeWonLobbyDlgOp, isize) -> isize>;

thread_local! {
    static LOG: RefCell<Vec<(EeWonLobbyDlgOp, isize, isize, isize)>> = const { RefCell::new(Vec::new()) };
    static ANSWER: RefCell<Answer> = RefCell::new(Box::new(|_, _| 0));
}

unsafe extern "C" fn log_host(_d: *mut c_void, op: EeWonLobbyDlgOp, a: isize, b: isize, c: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b, c)));
    if op == EeWonLobbyDlgOp::Find {
        // SAFETY: a slot of the table
        let slot = unsafe { &*(b as *const EeWonLobbyDlgSlot) };
        let hash = slot_name(slot).bytes().fold(0usize, |h, b| h.wrapping_mul(31).wrapping_add(b as usize)) % 64;
        return 0x100 + hash as isize * 16;
    }
    ANSWER.with(|f| f.borrow()(op, a))
}

fn set_answer(f: impl Fn(EeWonLobbyDlgOp, isize) -> isize + 'static) {
    ANSWER.with(|a| *a.borrow_mut() = Box::new(f));
}

fn clear_log() {
    LOG.with(|l| l.borrow_mut().clear());
}

fn made(kind: EeWonLobbyDlgKind) -> *mut EeWonLobbyDlg {
    clear_log();
    set_answer(|_, _| 0);
    let d = ee_won_lobbydlg_new(std::ptr::null_mut(), kind, Some(log_host));
    call(d, EeWonLobbyDlgCall::Init, 0x77, 0);
    d
}

fn call(d: *mut EeWonLobbyDlg, c: EeWonLobbyDlgCall, a: isize, b: isize) -> isize {
    // SAFETY: the tests' live objects
    unsafe { ee_won_lobbydlg_call(d, c, a, b, 0) }
}

fn event(d: *mut EeWonLobbyDlg, kind: EeWonLobbyDlgEvent, component: isize) {
    // SAFETY: the tests' live objects
    unsafe { ee_won_lobbydlg_call(d, EeWonLobbyDlgCall::Event, 0xE1, kind as isize, component) };
}

fn free(d: *mut EeWonLobbyDlg) {
    // SAFETY: freed once
    unsafe { ee_won_lobbydlg_free(d) };
}

fn ops() -> Vec<EeWonLobbyDlgOp> {
    LOG.with(|l| l.borrow().iter().map(|x| x.0).collect())
}

#[test]
fn every_table_has_distinct_names_within_its_kind() {
    for k in 0..15 {
        let t = slots(kind_of(k));
        assert!(t.len() <= MAX_SLOTS);
        let mut names: Vec<&str> = t.iter().map(slot_name).collect();
        names.sort();
        names.dedup();
        assert_eq!(names.len(), t.len());
    }
}

#[test]
fn admin_action_looks_its_title_up_last_and_releases_in_reverse_member_order() {
    let d = made(EeWonLobbyDlgKind::AdminAction);
    let finds: Vec<isize> =
        LOG.with(|l| l.borrow().iter().filter(|x| x.0 == EeWonLobbyDlgOp::Find).map(|x| x.2).collect());
    assert_eq!(finds.len(), 10);
    assert_eq!(finds[9], &ADMIN_ACTION[0] as *const _ as isize);
    let controls: Vec<isize> = (0..10).map(|i| call(d, EeWonLobbyDlgCall::Slot, i, 0)).collect();
    clear_log();
    free(d);
    let released: Vec<isize> = LOG.with(|l| l.borrow().iter().map(|x| x.1).collect());
    assert_eq!(released, controls.iter().rev().copied().collect::<Vec<_>>());
}

#[test]
fn lan_port_sets_its_filter_after_the_input_and_checks_the_range() {
    let d = made(EeWonLobbyDlgKind::LanPort);
    assert_eq!(ops()[..3], [EeWonLobbyDlgOp::Find, EeWonLobbyDlgOp::InputSetNumeric, EeWonLobbyDlgOp::Find]);
    // the game port is 6000 and so is the input: OK, enabled (IsDisabled answers 0), is disabled
    call(d, EeWonLobbyDlgCall::SetGamePort, 6000, 0);
    set_answer(|op, _| if op == EeWonLobbyDlgOp::InputToInt { 6000 } else { 0 });
    clear_log();
    call(d, EeWonLobbyDlgCall::CheckInput, 0, 0);
    let log = LOG.with(|l| l.borrow().clone());
    assert_eq!(log.last().map(|x| (x.0, x.2)), Some((EeWonLobbyDlgOp::SetEnabled, 0)));
    assert_eq!(call(d, EeWonLobbyDlgCall::GetPort, 0, 0), 6000);
    // a port of 70000 reads back as the original's unsigned short
    set_answer(|op, _| if op == EeWonLobbyDlgOp::InputToInt { 70000 } else { 0 });
    assert_eq!(call(d, EeWonLobbyDlgCall::GetPort, 0, 0), 70000 & 0xffff);
    free(d);
}

#[test]
fn create_game_button_press_updates_then_reaches_the_dialog() {
    let d = made(EeWonLobbyDlgKind::CreateGame);
    clear_log();
    event(d, EeWonLobbyDlgEvent::ButtonPressed, 0);
    let o = ops();
    assert_eq!(o.first(), Some(&EeWonLobbyDlgOp::ButtonIsChecked));
    assert_eq!(o.last(), Some(&EeWonLobbyDlgOp::BaseEvent));
    clear_log();
    event(d, EeWonLobbyDlgEvent::InputTextChanged, 0);
    assert!(!ops().contains(&EeWonLobbyDlgOp::BaseEvent));
    free(d);
}

#[test]
fn admin_action_seconds_fall_through_the_segments() {
    let d = made(EeWonLobbyDlgKind::AdminAction);
    let duration_combo = call(d, EeWonLobbyDlgCall::Slot, 4, 0);
    for (duration, segment, want) in
        [(0, 0, 60), (1, 1, 5 * 3600), (3, 2, 30 * 86400), (4, 2, 0), (9, 0, 3600 * 60), (0, 7, 1)]
    {
        set_answer(move |op, a| match op {
            EeWonLobbyDlgOp::ComboGetSel if a == duration_combo => duration,
            EeWonLobbyDlgOp::ComboGetSel => segment,
            _ => 0,
        });
        assert_eq!(call(d, EeWonLobbyDlgCall::NumSeconds, 0, 0), want);
    }
    free(d);
}

#[test]
fn tou_rebuilds_the_page_only_after_a_terms_download() {
    let d = made(EeWonLobbyDlgKind::Tou);
    // InitComponent: the button ids, the reset (no rebuild), the broadcast subscription last
    let o = ops();
    assert!(!o.contains(&EeWonLobbyDlgOp::BrowserShowTou));
    assert_eq!(o.last(), Some(&EeWonLobbyDlgOp::SubscribeBroadcast));
    // a message of the day download (type 0) does not mark the page; the terms (type 1) do, once
    for (doc, rebuilt) in [(0, false), (1, true)] {
        set_answer(move |op, _| if op == EeWonLobbyDlgOp::HttpDocType { doc } else { 0 });
        event(d, EeWonLobbyDlgEvent::GotHttpDocument, 0);
        assert_eq!(call(d, EeWonLobbyDlgCall::Field, 5, 0), rebuilt as isize);
        clear_log();
        call(d, EeWonLobbyDlgCall::Reset, 0, 0);
        assert_eq!(ops().contains(&EeWonLobbyDlgOp::BrowserShowTou), rebuilt);
        assert_eq!(call(d, EeWonLobbyDlgCall::Field, 5, 0), 0);
    }
    free(d);
}

#[test]
fn tou_accept_follows_the_agree_box_only() {
    let d = made(EeWonLobbyDlgKind::Tou);
    let agree = call(d, EeWonLobbyDlgCall::Slot, 1, 0);
    let accept = call(d, EeWonLobbyDlgCall::Slot, 2, 0);
    set_answer(|op, _| (op == EeWonLobbyDlgOp::ButtonIsChecked) as isize);
    clear_log();
    event(d, EeWonLobbyDlgEvent::ButtonPressed, agree);
    let log = LOG.with(|l| l.borrow().clone());
    assert_eq!(log.last().map(|x| (x.0, x.1, x.2)), Some((EeWonLobbyDlgOp::SetEnabled, accept, 1)));
    clear_log();
    event(d, EeWonLobbyDlgEvent::ButtonPressed, accept);
    assert_eq!(ops(), [EeWonLobbyDlgOp::BaseEvent]);
    free(d);
}

#[test]
fn bad_user_ids_come_from_the_list_in_use() {
    let d = made(EeWonLobbyDlgKind::BadUser);
    let full = call(d, EeWonLobbyDlgCall::Slot, 0, 0);
    let simple = call(d, EeWonLobbyDlgCall::Slot, 1, 0);
    for (flag, list) in [(1, simple), (0, full)] {
        call(d, EeWonLobbyDlgCall::SetUserList, 0x8800, flag);
        assert_eq!(call(d, EeWonLobbyDlgCall::Field, 6, 0), flag);
        // two selections, then none
        let left = std::rc::Rc::new(Cell::new(2));
        let l = left.clone();
        set_answer(move |op, _| match op {
            EeWonLobbyDlgOp::ListHasMore => {
                let n = l.get();
                l.set(n - 1);
                (n > 0) as isize
            }
            EeWonLobbyDlgOp::BadUserId => 77,
            _ => 0,
        });
        clear_log();
        call(d, EeWonLobbyDlgCall::GetIds, 0x8900, 0);
        let log = LOG.with(|l| l.borrow().clone());
        assert_eq!(log[1], (EeWonLobbyDlgOp::ListRewind, list, 0, 0));
        assert_eq!(log.iter().filter(|x| x.0 == EeWonLobbyDlgOp::IdsPush && x.2 == 77).count(), 2);
    }
    free(d);
}

#[test]
fn adapter_rows_select_the_local_host() {
    let d = made(EeWonLobbyDlgKind::ChooseAdapter);
    let local = 0x0a00_0001isize;
    // two unnamed adapters, both on the local host's address
    set_answer(move |op, _| match op {
        EeWonLobbyDlgOp::LocalHost => local,
        EeWonLobbyDlgOp::AdaptersGet => 0x7000,
        EeWonLobbyDlgOp::AdaptersCount => 2,
        EeWonLobbyDlgOp::AdapterHandle => local,
        EeWonLobbyDlgOp::ListNumItems => 3,
        _ => 0,
    });
    clear_log();
    call(d, EeWonLobbyDlgCall::Reset, 0, 0);
    let log = LOG.with(|l| l.borrow().clone());
    // the default row is not selected (the local host is not 0); the rows are named "unknown"
    assert!(!log.iter().any(|x| x.0 == EeWonLobbyDlgOp::ListSetSel && x.2 == 0 && x.3 == 0));
    assert_eq!(log.iter().filter(|x| x.0 == EeWonLobbyDlgOp::AdapterSetUnknownName).count(), 2);
    assert_eq!(log.iter().filter(|x| x.0 == EeWonLobbyDlgOp::ListSetSel && x.2 == 2 && x.3 == 1).count(), 2);
    assert_eq!(log.last().map(|x| x.0), Some(EeWonLobbyDlgOp::ListEndUpdate));
    // nothing selected: the local host
    assert_eq!(call(d, EeWonLobbyDlgCall::SelectedAddress, 0, 0), local);
    free(d);
}

#[test]
fn admin_action_reset_sets_the_reason_flag_per_action() {
    let d = made(EeWonLobbyDlgKind::AdminAction);
    for (action, optional) in [(15, 0), (8, 1), (14, 1), (7, 1), (2, 0)] {
        call(d, EeWonLobbyDlgCall::Reset, action, 0x5000);
        assert_eq!(call(d, EeWonLobbyDlgCall::Field, 3, 0), optional);
        assert_eq!(call(d, EeWonLobbyDlgCall::Field, 2, 0), action);
    }
    // another action keeps the flag (the original left its locals unset)
    call(d, EeWonLobbyDlgCall::Reset, 3, 0x5000);
    assert_eq!(call(d, EeWonLobbyDlgCall::Field, 3, 0), 0);
    free(d);
}

#[test]
fn golden_replay_connect() {
    let n = replay(GOLDEN_EEDLG).unwrap();
    assert!(n >= 60, "{n} sequences");
}

#[test]
fn golden_replay_connect_fails_on_a_changed_op() {
    // the first refresh of a changed line recorded as an insert
    let refresh = format!("op {} ", EeWonLobbyDlgOp::ListRefresh as u32);
    let insert = format!("op {} ", EeWonLobbyDlgOp::ConnectInsert as u32);
    let changed = GOLDEN_EEDLG.replacen(&refresh, &insert, 1);
    assert_ne!(changed, GOLDEN_EEDLG);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}
