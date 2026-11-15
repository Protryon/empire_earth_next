use super::*;
use std::cell::RefCell;
use std::sync::Mutex;

// The option types are read once per process (a static, as the original's): every test here holds this lock.
static TYPES: Mutex<()> = Mutex::new(());

fn lock() -> std::sync::MutexGuard<'static, ()> {
    TYPES.lock().unwrap_or_else(|e| e.into_inner())
}

// ---- the golden replay: golden_eegame.txt, recorded by tests/won/won_eegame_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" line, answered by its "=" value ("threw": a read past
// the end); a "reenter" line re-enters the object with that call (up to its "back" line); every call's result must
// be its "ret" line.

const GOLDEN: &str = include_str!("golden_eegame.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
    game: *const EeWonEeGame,
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

fn nums(s: &str) -> Vec<isize> {
    s.split(' ').map(|x| x.parse().unwrap()).collect()
}

unsafe extern "C" fn replay_host(_p: *mut c_void, op: EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    let Some(line) = next_line() else {
        fail(format!("op {} past the end", op as u32));
        return 0;
    };
    let (head, answer) = line.split_once(" = ").unwrap_or((line, "0"));
    let answer = if op == EeWonEeGameOp::Version {
        // the checksum the recording's host wrote
        let n = nums(head.strip_prefix("op ").unwrap_or("0 0 0"));
        if n[0] != op as isize || n[2] != b {
            fail(format!("expected '{line}', the Rust made op {} {b}", op as u32));
            return 0;
        }
        // SAFETY: the Rust's out checksum
        unsafe { *(a as *mut u32) = n[1] as u32 };
        answer.parse().unwrap()
    } else {
        let made = format!("op {} {a} {b} {c} {d}", op as u32);
        if head != made {
            fail(format!("expected '{line}', the Rust made '{made}'"));
            return 0;
        }
        if answer == "threw" {
            isize::MIN
        } else {
            answer.parse().unwrap()
        }
    };
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let n = nums(rest);
            let game = REPLAY.with(|r| r.borrow().as_ref().unwrap().game);
            // SAFETY: the live object (re-entry through a shared reference, as the C++ does)
            let r = unsafe { ee_won_eegame_call(game, call_of(n[0]), n[1], n[2], n[3], 0) };
            let back = next_line().unwrap_or("-");
            if back != format!("back {r}") {
                fail(format!("{next}: expected '{back}', the re-entry returned {r}"));
            }
        }
    }
    answer
}

fn call_of(n: isize) -> EeWonEeGameCall {
    assert!((1..=EeWonEeGameCall::SlotPlayer as isize).contains(&n));
    // SAFETY: a value of the enum (checked above: the variants are 1.. in order)
    unsafe { std::mem::transmute::<u32, EeWonEeGameCall>(n as u32) }
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let _l = lock();
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    // the first recorded game read the map types
    reset_types();
    for w in starts.windows(2) {
        REPLAY.with(|r| {
            *r.borrow_mut() =
                Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None, game: std::ptr::null() })
        });
        let mut p: *mut EeWonEeGame = std::ptr::null_mut();
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "new" => {
                    p = ee_won_eegame_new(0x9090 as *mut c_void, Some(replay_host));
                    REPLAY.with(|r| r.borrow_mut().as_mut().unwrap().game = p);
                }
                "call" => {
                    let n = nums(&line[5..]);
                    let call = call_of(n[0]);
                    let mut taken = [false; 16];
                    let a = if call == EeWonEeGameCall::GetTakenColors { taken.as_mut_ptr() as isize } else { n[1] };
                    // SAFETY: the live object
                    let r = unsafe { ee_won_eegame_call(p, call, a, n[2], n[3], n[4]) };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                    if call == EeWonEeGameCall::GetTakenColors {
                        let bits: i32 = taken.iter().enumerate().map(|(k, &t)| (t as i32) << k).sum();
                        let l = next_line().unwrap_or("-");
                        if l != format!("taken {bits}") {
                            fail(format!("{line}: expected '{l}', the Rust's colours {bits}"));
                        }
                    }
                }
                // SAFETY: the live object, freed once
                "free" => unsafe { ee_won_eegame_free(p) },
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
    // the first byte appended recorded as a bool
    let a = format!("op {} ", EeWonEeGameOp::AppendByte as u32);
    let b = format!("op {} ", EeWonEeGameOp::AppendBool as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests over a recording host

type Answer = Box<dyn Fn(EeWonEeGameOp, isize) -> isize>;

thread_local! {
    static LOG: RefCell<Vec<(EeWonEeGameOp, isize, isize)>> = const { RefCell::new(Vec::new()) };
    static ANSWER: RefCell<Answer> = RefCell::new(Box::new(|_, _| 0));
}

unsafe extern "C" fn log_host(_p: *mut c_void, op: EeWonEeGameOp, a: isize, b: isize, _c: isize, _d: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b)));
    if op == EeWonEeGameOp::NumMapTypes {
        return 12;
    }
    ANSWER.with(|f| f.borrow()(op, a))
}

fn game(answer: impl Fn(EeWonEeGameOp, isize) -> isize + 'static) -> EeWonEeGame {
    ANSWER.with(|a| *a.borrow_mut() = Box::new(answer));
    let g = EeWonEeGame::new(std::ptr::null_mut(), Some(log_host));
    LOG.with(|l| l.borrow_mut().clear());
    g
}

fn log() -> Vec<(EeWonEeGameOp, isize, isize)> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn appended(l: &[(EeWonEeGameOp, isize, isize)]) -> Vec<isize> {
    l.iter().filter(|x| x.0 == Op::AppendByte || x.0 == Op::AppendBool).map(|x| x.2).collect()
}

use EeWonEeGameCall as C;

#[test]
fn the_refusal_texts_and_the_team_rule() {
    assert_eq!([10000, 10001, 10002, 2, 0].map(status_string), [1, 2, 3, 0, 0]);
    assert!(is_teammate(3, 3));
    assert!(!is_teammate(0, 0));
    assert!(!is_teammate(1, 2));
    assert_eq!(trunc_i32(f64::INFINITY), i32::MIN);
    assert_eq!(trunc_i32(f64::NAN), i32::MIN);
    assert_eq!(trunc_i32(99.9), 99);
}

#[test]
fn an_option_is_set_below_its_limit_and_the_map_types_are_read_once() {
    let _l = lock();
    reset_types();
    let g = game(|_, _| 0);
    let h = EeWonEeGame::new(std::ptr::null_mut(), Some(log_host));
    assert_eq!(log().iter().filter(|x| x.0 == Op::NumMapTypes).count(), 0, "the first game read them");
    g.call(C::SetOption, 2, 11, 0, 0);
    assert_eq!(g.call(C::GetOption, 2, 0, 0, 0), 11);
    g.call(C::SetOption, 2, 12, 0, 0);
    assert_eq!(g.call(C::GetOption, 2, 0, 0, 0), 11, "12 map types: 12 is past them");
    g.call(C::SetOption, 9, 1200, 0, 0);
    g.call(C::SetOption, 16, 0, 0, 0);
    assert_eq!(g.call(C::GetOption, 9, 0, 0, 0), 1200);
    assert_eq!(g.call(C::GetOption, 16, 0, 0, 0), 0);
    drop(h);
}

#[test]
fn the_starting_epoch_stays_at_or_before_the_ending_one() {
    let _l = lock();
    let g = game(|_, _| 0);
    g.call(C::ChangeOption, 5, 6, 0, 0);
    g.call(C::ChangeOption, 4, 9, 0, 0);
    assert_eq!((g.call(C::GetOption, 4, 0, 0, 0), g.call(C::GetOption, 5, 0, 0, 0)), (9, 9));
    g.call(C::ChangeOption, 5, 3, 0, 0);
    assert_eq!((g.call(C::GetOption, 4, 0, 0, 0), g.call(C::GetOption, 5, 0, 0, 0)), (3, 3));
    // 15 (the last epoch) keeps the ending epoch
    g.call(C::ChangeOption, 4, 15, 0, 0);
    assert_eq!(g.call(C::GetOption, 5, 0, 0, 0), 3);
    log();
    // past the limit: the controls resync
    g.call(C::ChangeOption, 4, 16, 1, 0);
    assert_eq!(log(), [(Op::SyncOption, 4, 0)]);
}

#[test]
fn a_hosted_game_seats_the_host_and_counts_one_human() {
    let _l = lock();
    let g = game(|op, a| match op {
        Op::TransferNew => 0x70,
        Op::ClientPlayer => a + 0x100,
        Op::IAmCaptain => 1,
        _ => 0,
    });
    g.call(C::HandleHostGameHook, 0x32, 0, 0, 0);
    assert_eq!(g.call(C::GetOption, 9, 0, 0, 0), 0x1f4);
    assert_eq!(g.call(C::SlotClient, 0, 0, 0, 0), 0x32);
    assert_eq!(g.call(C::SlotGetOption, 0, 0, 0, 0), 3);
    assert!(log().contains(&(Op::PlayerAttach, 0x132, 0)));
    assert_eq!(g.call(C::CanStartGameHook, 0, 0, 0, 0), 0, "one human only");
    g.call(C::SlotSetComputer, 1, 0, 0, 0);
    g.call(C::CountPlayers, 0, 0, 0, 0);
    assert_eq!(g.call(C::CanStartGameHook, 0, 0, 0, 0), 1);
    // the destructor: the last slot first, then the transfer
    log();
    g.release_all();
    assert_eq!(
        log(),
        [(Op::PlayerDetach, 0x132, 0), (Op::Release, 0x132, 0), (Op::Release, 0x32, 0), (Op::Release, 0x70, 0)]
    );
}

#[test]
fn colours_taken_by_playing_slots_only() {
    let _l = lock();
    let g = game(|_, _| 0);
    for (slot, state, colour, observe) in [(0, 3, 2, 0), (1, 4, 5, 0), (2, 3, 7, 1), (3, 1, 9, 0)] {
        g.call(C::SlotSetRawOption, slot, 0, state, 0);
        g.call(C::SlotSetRawOption, slot, 2, colour, 0);
        g.call(C::SlotSetRawOption, slot, 4, observe, 0);
    }
    let mut taken = [false; 16];
    g.call(C::GetTakenColors, taken.as_mut_ptr() as isize, -1, 0, 0);
    let t: Vec<usize> = (0..16).filter(|&i| taken[i]).collect();
    assert_eq!(t, [2, 5]);
    assert_eq!(g.call(C::IsColorFree, 7, 0, 0, 0), 1);
    assert_eq!(g.call(C::GetFreeColor, 1, 2, 0, 0), 0, "2 is slot 0's: the first free");
    assert_eq!(g.call(C::GetFreeColor, 0, 2, 0, 0), 2, "its own colour");
    g.call(C::SlotSetRawOption, 1, 2, 2, 0);
    // slot 0 first: its colour is slot 1's now, so it takes the first free one; slot 1 keeps its own
    g.call(C::MakeColorsDistinct, 0, 0, 0, 0);
    assert_eq!(g.call(C::SlotGetOption, 0, 2, 0, 0), 0);
    assert_eq!(g.call(C::SlotGetOption, 1, 2, 0, 0), 2);
}

#[test]
fn a_read_past_the_end_stops_the_hook_and_releases_the_version() {
    let _l = lock();
    let g = game(|op, _| match op {
        Op::Version => 0x60,
        Op::ReadWString => isize::MIN,
        _ => 0,
    });
    assert_eq!(g.call(C::HandleJoinGameRequestHook, 0xB0, 0x32, 0, 0), isize::MIN);
    let l = log();
    assert_eq!(l.last(), Some(&(Op::Release, 0x60, 0)));
    assert_eq!(l.len(), 3);
}

#[test]
fn a_join_with_another_version_or_checksum_is_refused() {
    let _l = lock();
    let equal = std::rc::Rc::new(Cell::new(0));
    let e = equal.clone();
    let g = game(move |op, _| match op {
        Op::Version => 0x60,
        Op::ReadWString => 0x61,
        Op::StringsEqual => e.get(),
        Op::ReadLong => 7,
        _ => 0,
    });
    assert_eq!(g.call(C::HandleJoinGameRequestHook, 0xB0, 0x32, 0, 0), 10001);
    let l = log();
    assert_eq!(&l[l.len() - 2..], [(Op::Release, 0x61, 0), (Op::Release, 0x60, 0)]);
    equal.set(1);
    assert_eq!(g.call(C::HandleJoinGameRequestHook, 0xB0, 0x32, 0, 0), 10002, "the version's checksum is 0");
}

#[test]
fn the_details_list_each_seated_clients_name() {
    let _l = lock();
    let g = game(|op, a| match op {
        Op::ClientPlayer => a + 0x100,
        Op::ClientName => a + 0x500,
        Op::GameType => 1,
        Op::Length => 40,
        _ => 0,
    });
    g.call(C::SlotSetPlayer, 2, 0x32, 0x132, 0);
    g.call(C::SlotSetPlayer, 5, 0x35, 0x135, 0);
    log();
    g.call(C::WriteDetailsHook, 0xC0, 0, 0, 0);
    let l = log();
    let names: Vec<isize> = l.iter().filter(|x| x.0 == Op::AppendWString).map(|x| x.2).collect();
    assert_eq!(names, [0x532, 0x535]);
    assert!(l.contains(&(Op::SetByte, 0xC0, 40)));
    // the summary: starting epoch, game type, map type
    g.call(C::SetOption, 4, 3, 0, 0);
    g.call(C::WriteSummaryHook, 0xC0, 0, 0, 0);
    assert_eq!(appended(&log()), [3, 0, 0]);
}

#[test]
fn a_player_asks_the_host_and_the_host_tells_everyone() {
    let _l = lock();
    let host = std::rc::Rc::new(Cell::new(0));
    let h = host.clone();
    let g = game(move |op, _| match op {
        Op::MyPlayer => 0x130,
        Op::PlayerSlot => 1,
        Op::IsHost => h.get(),
        Op::NewBuffer => 0xD0,
        _ => 0,
    });
    // our own team: to the host
    g.call(C::ChangePlayerOption, 1, 1, 4, 0);
    let l = log();
    assert_eq!(appended(&l), [0x66, 1, 4]);
    assert!(l.contains(&(Op::SendToCaptain, 0xD0, 0)));
    // another slot's team, not the host's: refused and resynced
    g.call(C::ChangePlayerOption, 3, 1, 4, 0);
    assert!(log().contains(&(Op::SyncPlayer, 3, 0)));
    // the host: to everyone
    host.set(1);
    g.call(C::ChangePlayerOption, 3, 3, 2, 0);
    let l = log();
    assert_eq!(appended(&l), [0x65, 3, 3, 2]);
    assert!(l.contains(&(Op::BroadcastToGame, 0xD0, 0)));
}
