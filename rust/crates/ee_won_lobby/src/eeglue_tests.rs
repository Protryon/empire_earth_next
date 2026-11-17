use super::*;
use crate::eegame::{testgame, EeWonEeGame, EeWonEeGameOp};
use crate::lobbydlg::{ee_won_lobbydlg_free, ee_won_lobbydlg_new, EeWonLobbyDlgKind, EeWonLobbyDlgOp};
use std::cell::RefCell;
use std::ffi::c_void;

// ---- the golden replay: golden_eeglue.txt, recorded by tests/won/won_eeglue_test.cpp over the original statements.
// Every op the Rust makes must be the next "op" (the game's "gop", the connect dialog's "dop") line, answered by its
// "=" value; the struct reads the recording did not trace are answered from its "options" lines; a "reenter" line
// re-enters the glue (up to its "back" line); every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_eeglue.txt");

struct Player {
    ty: isize,
    color: isize,
    team: isize,
    citizens: isize,
    _name: isize,
    name_empty: isize,
}

struct Options {
    fields: Vec<isize>,
    players: Vec<Player>,
}

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
    game: *mut EeWonEeGame,
    dialog: *mut crate::lobbydlg::EeWonLobbyDlg,
    options: Vec<Options>,
    player_type: isize,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<T>(f: impl FnOnce(&mut Replay) -> T) -> T {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().unwrap()))
}

fn fail(msg: String) {
    with(|r| {
        if r.failed.is_none() {
            r.failed = Some(msg);
        }
    });
}

fn next_line() -> Option<&'static str> {
    with(|r| {
        let l = r.lines.get(r.at).copied();
        r.at += 1;
        l
    })
}

fn peek_line() -> Option<&'static str> {
    with(|r| r.lines.get(r.at).copied())
}

fn call_of(n: isize) -> EeWonEeGlueCall {
    assert!((1..=EeWonEeGlueCall::Reset as isize).contains(&n));
    // SAFETY: a value of the enum (checked above: the variants are 1.. in order)
    unsafe { std::mem::transmute::<u32, EeWonEeGlueCall>(n as u32) }
}

const NAME: isize = 0x9000;
const DEFAULT_OPTIONS: isize = 0x8200;
const CONNECT_DIALOG: isize = 0xC0DE;

/// The recording's names: a pointer's letter.
fn name_units(p: isize) -> Vec<u32> {
    if p == 0 {
        Vec::new()
    } else {
        vec![b'A' as u32 + ((p - NAME) / 16) as u32]
    }
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
    if let Some(next) = peek_line() {
        if let Some(rest) = next.strip_prefix("reenter ") {
            next_line();
            let n: Vec<isize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            let got = GLUE.call(call_of(n[0]), n[1], n[2], 0);
            let back = next_line().unwrap_or("-");
            if back != format!("back {got}") {
                fail(format!("{next}: expected '{back}', the re-entry returned {got}"));
            }
        }
    }
    answer
}

unsafe extern "C" fn replay_host(
    op: EeWonEeGlueOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    e: isize,
    f: isize,
) -> isize {
    use EeWonEeGlueOp as O;
    match op {
        O::GameRust => return with(|r| r.game) as isize,
        O::DialogRust => return if a == CONNECT_DIALOG { with(|r| r.dialog) as isize } else { 0 },
        O::ItemGame => return if a != 0 { 0x3000 } else { 0 },
        O::SaveChecksum => return 0x1234,
        O::PlayerType => return with(|r| r.player_type),
        O::OptionsField => {
            let k = (a == DEFAULT_OPTIONS) as usize;
            return with(|r| {
                let o = &r.options[k];
                if b == EeWonEeGlueField::MapTypeIndex as isize {
                    o.fields[15]
                } else {
                    o.fields[(b - EeWonEeGlueField::MapSize as isize) as usize]
                }
            });
        }
        O::OptionsPlayer => {
            let k = (a == DEFAULT_OPTIONS) as usize;
            return with(|r| {
                let p = &r.options[k].players[b as usize];
                match c {
                    x if x == EeWonEeGlueField::Type as isize => p.ty,
                    x if x == EeWonEeGlueField::Color as isize => p.color,
                    x if x == EeWonEeGlueField::Team as isize => p.team,
                    x if x == EeWonEeGlueField::StartingCitizens as isize => p.citizens,
                    x if x == EeWonEeGlueField::NameEmpty as isize => p.name_empty,
                    _ => 0,
                }
            });
        }
        O::StringUnits => {
            let n = name_units(a);
            if b != 0 {
                // SAFETY: the Rust's buffer of the length it asked first
                unsafe { std::ptr::copy_nonoverlapping(n.as_ptr(), b as *mut u32, n.len()) };
            }
            return n.len() as isize;
        }
        O::PlayerColor => {
            let line = next_line().unwrap_or("-");
            let prefix = format!("op {} {a} rgb ", op as u32);
            let Some(rest) = line.strip_prefix(&prefix) else {
                fail(format!("expected '{line}', the Rust made '{prefix}'"));
                return 0;
            };
            let bits: Vec<u32> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
            // SAFETY: the Rust's three channels
            let rgb = unsafe { std::slice::from_raw_parts_mut(b as *mut f32, 3) };
            for (c, bits) in rgb.iter_mut().zip(bits) {
                *c = f32::from_bits(bits);
            }
            return 0;
        }
        O::PlayerNew => with(|r| r.player_type = 3),
        O::PlayerSet if a == EeWonEeGlueField::Type as isize => with(|r| r.player_type = b),
        _ => {}
    }
    expect(format!("op {} {a} {b} {c} {d} {e} {f}", op as u32))
}

fn replay_game_ops(op: EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    expect(format!("gop {} {a} {b} {c} {d}", op as u32))
}

unsafe extern "C" fn replay_dialog_host(_d: *mut c_void, op: EeWonLobbyDlgOp, a: isize, b: isize, c: isize) -> isize {
    if op == EeWonLobbyDlgOp::GameRust {
        return with(|r| r.game) as isize;
    }
    expect(format!("dop {} {a} {b} {c}", op as u32))
}

fn options_of(line: &str) -> Options {
    let v: Vec<isize> = line.split(' ').skip(2).map(|x| x.parse().unwrap()).collect();
    let players = v[16..]
        .chunks(6)
        .map(|p| Player { ty: p[0], color: p[1], team: p[2], citizens: p[3], _name: p[4], name_empty: p[5] })
        .collect();
    Options { fields: v[..16].to_vec(), players }
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let _l = testgame::lock();
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let game = testgame::new(replay_game_ops);
        let dialog =
            ee_won_lobbydlg_new(CONNECT_DIALOG as *mut c_void, EeWonLobbyDlgKind::Connect, Some(replay_dialog_host));
        REPLAY.with(|r| {
            *r.borrow_mut() = Some(Replay {
                lines: lines[w[0] + 1..w[1]].to_vec(),
                at: 0,
                failed: None,
                game,
                dialog,
                options: Vec::new(),
                player_type: 3,
            })
        });
        ee_won_eeglue_set_host(Some(replay_host));
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            let n = |i: usize| -> isize { t[i].parse().unwrap() };
            match t[0] {
                "options" => {
                    let o = options_of(line);
                    with(|r| r.options.push(o));
                }
                "gset" => testgame::setup(game, &[(n(1), n(2))], &[]),
                "gslot" => testgame::setup(game, &[], &[(n(1), n(2), n(3))]),
                "gplayer" => {
                    crate::eegame::game_call(
                        game as isize,
                        crate::eegame::EeWonEeGameCall::SlotSetPlayer,
                        n(1),
                        n(2),
                        n(3),
                    );
                }
                "call" => {
                    let call = call_of(n(1));
                    let r = match call {
                        EeWonEeGlueCall::SavedGameInfo => {
                            let (mut length, mut checksum) = (7u32, 9u32);
                            let r = GLUE.call(
                                call,
                                n(2),
                                &mut length as *mut u32 as isize,
                                &mut checksum as *mut u32 as isize,
                            );
                            let out = next_line().unwrap_or("-");
                            if out != format!("out {length} {checksum}") {
                                fail(format!("{line}: expected '{out}', the Rust wrote {length} {checksum}"));
                            }
                            r
                        }
                        EeWonEeGlueCall::GameDone => {
                            let mut count = n(3) as i32;
                            let r = GLUE.call(call, n(2), &mut count as *mut i32 as isize, 0);
                            let out = next_line().unwrap_or("-");
                            if out != format!("out {count}") {
                                fail(format!("{line}: expected '{out}', the Rust counted {count}"));
                            }
                            r
                        }
                        _ => GLUE.call(call, n(2), n(3), n(4)),
                    };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                "palette" => {
                    if crate::eecolor::palette(n(1) as usize) as isize != n(2) {
                        fail(format!("{line}: the palette holds {:#x}", crate::eecolor::palette(n(1) as usize)));
                    }
                }
                "free" => {}
                "field" => {
                    let v = GLUE.call(EeWonEeGlueCall::Field, n(1), 0, 0);
                    if v != n(2) {
                        fail(format!("{line}: the Rust's is {v}"));
                    }
                }
                "gfree" => {
                    GLUE.call(EeWonEeGlueCall::Reset, 0, 0, 0);
                    // SAFETY: the replay's live objects, freed once
                    unsafe {
                        ee_won_lobbydlg_free(dialog);
                        crate::eegame::ee_won_eegame_free(game);
                    }
                }
                _ => fail(format!("unexpected line '{line}'")),
            }
            if let Some(f) = with(|r| r.failed.clone()) {
                ee_won_eeglue_set_host(None);
                return Err(format!("{}: {f}", lines[w[0]]));
            }
        }
    }
    ee_won_eeglue_set_host(None);
    // the palette back to the original's (the replays set the game's colours)
    crate::eecolor::set_palette(&ORIGINAL_PALETTE);
    Ok(starts.len() - 1)
}

const ORIGINAL_PALETTE: [u32; 16] = [
    0x0000ff, 0xff0000, 0xffff00, 0xff00ff, 0xff8800, 0x008800, 0x00ffff, 0x880000, 0x888888, 0x000088, 0xbbbbbb,
    0xdddddd, 0xffff88, 0x88ff88, 0x8800ff, 0xff8888,
];

#[test]
fn golden_replay() {
    let n = replay(GOLDEN).unwrap();
    assert!(n >= 80, "{n} sequences");
}

#[test]
fn golden_replay_fails_on_a_changed_op() {
    // the first text drawn recorded as measured
    let a = format!("op {} ", EeWonEeGlueOp::DrawText as u32);
    let b = format!("op {} ", EeWonEeGlueOp::TextWidth as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests

#[test]
fn the_resource_rows_the_glue_names() {
    assert_eq!(res("gEELobbyGameOptionsCtrl_Epoch16"), 15);
    assert_eq!(res("gEELobbyGameOptionsCtrl_GameType1"), 0x10);
    assert_eq!(res("gEELobbyGameOptionsCtrl_MapSize1"), 0x13);
    assert_eq!(res("gEELobbyGameOptionsCtrl_Resources1"), 0x19);
    assert_eq!(res("gEELobbyGameOptionsCtrl_GameVariant1"), 0x1e);
    assert_eq!(res("gEELobbyGameOptionsCtrl_Difficulty1"), 0x20);
    assert_eq!(res("gEELobbyGameOptionsCtrl_GameSpeed1"), 0x23);
    assert_eq!(res("gEELobbyGameOptionsCtrl_GameType2"), 17);
    assert_eq!(res("gEELobbyGameOptionsCtrl_OptionOn"), 39);
    assert_eq!(res("gEELobbyGameOptionsCtrl_WondersOff"), 41);
    assert_eq!(res("gEELobbyInterface_Players"), 70);
}

#[test]
fn player_colours_are_clamped_bytes() {
    assert_eq!(pack_color([1.0, 0.0, 0.5]), 0xff007f);
    assert_eq!(pack_color([1.5, -0.5, 0.999]), 0xff00fe);
    assert_eq!(pack_color([f32::NAN, 2e10, 0.0039]), 0x000000, "out of range: cvttss2si's INT_MIN, clamped to 0");
}

thread_local! {
    static LOG: RefCell<Vec<(EeWonEeGlueOp, isize, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn log_host(
    op: EeWonEeGlueOp,
    a: isize,
    b: isize,
    _c: isize,
    _d: isize,
    _e: isize,
    _f: isize,
) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b)));
    match op {
        EeWonEeGlueOp::LobbyRun => 0,
        EeWonEeGlueOp::IsLobbyOpen => 1,
        _ => 0,
    }
}

#[test]
fn leaving_the_lobby_shuts_it_down_and_the_loop_runs_once() {
    let _l = testgame::lock();
    LOG.with(|l| l.borrow_mut().clear());
    ee_won_eeglue_set_host(Some(log_host));
    GLUE.call(EeWonEeGlueCall::RunLoop, 0, 0, 0);
    let ops: Vec<EeWonEeGlueOp> = LOG.with(|l| l.borrow().iter().map(|x| x.0).collect());
    use EeWonEeGlueOp as O;
    assert_eq!(ops, [O::LocksRelease, O::LobbyRun, O::LobbyClose, O::ResourceDestroy, O::LocksRetake]);
    assert_eq!(GLUE.call(EeWonEeGlueCall::Field, 2, 0, 0), 0, "not closing any more");
    // a game done while the lobby is hidden is reported on the return
    LOG.with(|l| l.borrow_mut().clear());
    GLUE.call(EeWonEeGlueCall::SetQuitFlag, 0, 0, 0);
    assert_eq!(GLUE.call(EeWonEeGlueCall::Field, 3, 0, 0), 1);
    GLUE.call(EeWonEeGlueCall::ReturnFromGame, 0, 0, 0);
    assert!(LOG.with(|l| l.borrow().iter().any(|x| x.0 == O::BoxDissolved)));
    assert_eq!(GLUE.call(EeWonEeGlueCall::Field, 3, 0, 0), 0, "the run loop clears it");
    GLUE.call(EeWonEeGlueCall::Reset, 0, 0, 0);
    ee_won_eeglue_set_host(None);
}

#[test]
fn the_timeouts_are_the_originals_defaults() {
    assert_eq!((TIMEOUTS.l00, TIMEOUTS.l04, TIMEOUTS.l08, TIMEOUTS.s1c), (3, 2000, 45000, 1));
    assert_eq!((TIMEOUTS.l30, TIMEOUTS.b44, TIMEOUTS.l58), (0, false, 0));
}
