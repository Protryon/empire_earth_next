use super::*;
use crate::eegame::{testgame, EeWonEeGame, EeWonEeGameOp};
use std::cell::RefCell;

// ---- the golden replay: golden_eecolor.txt, recorded by tests/won/won_eecolor_test.cpp over the original
// statements. Every op the Rust makes must be the next "op" (or the game's "gop") line, answered by its "=" value; a
// "reenter" line re-enters an object (up to its "back" line); every call's result must be its "ret" line.

const GOLDEN: &str = include_str!("golden_eecolor.txt");

struct Replay {
    lines: Vec<&'static str>,
    at: usize,
    failed: Option<String>,
    button: *const EeWonEeColor,
    chooser: *const EeWonEeColor,
    game: *mut EeWonEeGame,
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

fn call_of(n: isize) -> EeWonEeColorCall {
    assert!((1..=EeWonEeColorCall::SetSelected as isize).contains(&n));
    // SAFETY: a value of the enum (checked above: the variants are 1.. in order)
    unsafe { std::mem::transmute::<u32, EeWonEeColorCall>(n as u32) }
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
            let (button, chooser) = with(|r| (r.button, r.chooser));
            let t: Vec<&str> = rest.split(' ').collect();
            // SAFETY: the replay's live objects; re-entry through shared references, as the C++ does
            let got = unsafe {
                match t[0] {
                    "end" => ee_won_eecolor_call(button, EeWonEeColorCall::Event, 0xE1, 1, 0),
                    "added" => ee_won_eecolor_call(chooser, EeWonEeColorCall::AddedToParent, 0, 0, 0),
                    _ => ee_won_eecolor_call(button, EeWonEeColorCall::SetColor, t[1].parse().unwrap(), 0, 0),
                }
            };
            let back = next_line().unwrap_or("-");
            if back != format!("back {got}") {
                fail(format!("{next}: expected '{back}', the re-entry returned {got}"));
            }
        }
    }
    answer
}

unsafe extern "C" fn replay_host(
    _obj: *mut c_void,
    op: EeWonEeColorOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    e: isize,
    f: isize,
) -> isize {
    match op {
        EeWonEeColorOp::ChooserRust => return with(|r| r.chooser) as isize,
        EeWonEeColorOp::GameRust => return with(|r| r.game) as isize,
        EeWonEeColorOp::LocalToRoot => {
            // SAFETY: the Rust's two coordinates (the op's contract)
            let (x, y) = unsafe { (&mut *(b as *mut i32), &mut *(c as *mut i32)) };
            let made = format!("op {} {a} x {} y {}", op as u32, *x, *y);
            let line = next_line().unwrap_or("-");
            let Some((head, to)) = line.split_once(" = ") else {
                fail(format!("expected '{line}', the Rust made '{made}'"));
                return 0;
            };
            if head != made {
                fail(format!("expected '{line}', the Rust made '{made}'"));
                return 0;
            }
            let v: Vec<i32> = to.split(' ').map(|s| s.parse().unwrap()).collect();
            *x = v[0];
            *y = v[1];
            return 0;
        }
        _ => {}
    }
    expect(format!("op {} {a} {b} {c} {d} {e} {f}", op as u32))
}

fn replay_game_ops(op: EeWonEeGameOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    expect(format!("gop {} {a} {b} {c} {d}", op as u32))
}

fn replay(golden: &'static str) -> Result<usize, String> {
    let _l = testgame::lock();
    let lines: Vec<&str> = golden.lines().filter(|l| !l.is_empty()).collect();
    let mut starts: Vec<usize> =
        lines.iter().enumerate().filter(|(_, l)| l.starts_with("seq ")).map(|(i, _)| i).collect();
    starts.push(lines.len());
    for w in starts.windows(2) {
        let game = testgame::new(replay_game_ops);
        let button = ee_won_eecolor_new(0xB00 as *mut c_void, EeWonEeColorKind::Button, Some(replay_host));
        let chooser = ee_won_eecolor_new(0xC00 as *mut c_void, EeWonEeColorKind::Chooser, Some(replay_host));
        REPLAY.with(|r| {
            *r.borrow_mut() =
                Some(Replay { lines: lines[w[0] + 1..w[1]].to_vec(), at: 0, failed: None, button, chooser, game })
        });
        while let Some(line) = next_line() {
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "gslot" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    testgame::setup(game, &[], &[(n[0], n[1], n[2])]);
                }
                "call" => {
                    let n: Vec<isize> = t[1..].iter().map(|x| x.parse().unwrap()).collect();
                    let p = if n[0] != 0 { chooser } else { button };
                    // SAFETY: the live object
                    let r = unsafe { ee_won_eecolor_call(p, call_of(n[1]), n[2], n[3], 0) };
                    let ret = next_line().unwrap_or("-");
                    if ret != format!("ret {r}") {
                        fail(format!("{line}: expected '{ret}', the Rust returned {r}"));
                    }
                }
                "free" => {
                    // SAFETY: the live objects, freed once
                    unsafe {
                        ee_won_eecolor_free(button);
                        ee_won_eecolor_free(chooser);
                    }
                }
                // SAFETY: the live game, freed once
                "gfree" => unsafe { crate::eegame::ee_won_eegame_free(game) },
                _ => fail(format!("unexpected line '{line}'")),
            }
            if let Some(f) = with(|r| r.failed.clone()) {
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
    // the first fill recorded as a frame
    let a = format!("op {} ", EeWonEeColorOp::FillRect as u32);
    let b = format!("op {} ", EeWonEeColorOp::DrawRect as u32);
    let changed = GOLDEN.replacen(&a, &b, 1);
    assert_ne!(changed, GOLDEN);
    assert!(replay(Box::leak(changed.into_boxed_str())).is_err());
}

// ---- unit tests

#[test]
fn the_swatches_and_their_gaps() {
    assert_eq!(swatch_at(8, 8), 0);
    assert_eq!(swatch_at(27, 8), 0);
    assert_eq!(swatch_at(28, 8), -1, "the gap after the first swatch");
    assert_eq!(swatch_at(36, 8), 1);
    assert_eq!(swatch_at(8 + 3 * 28 + 19, 8 + 3 * 28 + 19), 15);
    assert_eq!(swatch_at(7, 8), -1);
    assert_eq!(swatch_at(8, 7), -1);
    // past the fourth column: the original's arithmetic (no bound), a hover MouseDown ignores
    assert_eq!(swatch_at(8 + 4 * 28, 8), 4);
}

#[test]
fn the_palette_is_the_originals_until_the_game_sets_it() {
    assert_eq!(palette(0), 0x0000ff);
    assert_eq!(palette(15), 0xff8888);
}

thread_local! {
    static LOG: RefCell<Vec<(EeWonEeColorOp, isize, isize)>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn log_host(
    _o: *mut c_void,
    op: EeWonEeColorOp,
    a: isize,
    b: isize,
    _c: isize,
    _d: isize,
    _e: isize,
    _f: isize,
) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b)));
    match op {
        EeWonEeColorOp::ChooserNew => 0xC00,
        EeWonEeColorOp::Width | EeWonEeColorOp::Height => 40,
        _ => 0,
    }
}

#[test]
fn a_colour_is_set_once_and_only_in_range() {
    let b = EeWonEeColor::new(0xB00 as *mut c_void, EeWonEeColorKind::Button, Some(log_host));
    assert_eq!(b.call(EeWonEeColorCall::SetColor, 0, 0, 0), 0, "already 0");
    assert_eq!(b.call(EeWonEeColorCall::SetColor, 16, 0, 0), 0);
    assert_eq!(b.call(EeWonEeColorCall::SetColor, -1, 0, 0), 0);
    assert_eq!(b.call(EeWonEeColorCall::SetColor, 5, 0, 0), 1);
    assert_eq!(b.call(EeWonEeColorCall::GetColor, 0, 0, 0), 5);
    let invalidations = LOG.with(|l| l.borrow().iter().filter(|x| x.0 == EeWonEeColorOp::Invalidate).count());
    assert_eq!(invalidations, 1);
}

#[test]
fn the_first_click_makes_the_chooser_and_opens_it_centred_below() {
    LOG.with(|l| l.borrow_mut().clear());
    let b = EeWonEeColor::new(0xB00 as *mut c_void, EeWonEeColorKind::Button, Some(log_host));
    b.call(EeWonEeColorCall::Activate, 0, 0, 0);
    let l = LOG.with(|l| l.borrow().clone());
    assert_eq!(l[0].0, EeWonEeColorOp::ChooserNew);
    assert!(l.contains(&(EeWonEeColorOp::AddListener, 0xC00, 0xB00)));
    // (40 - 40) / 2 and the button's height, made root coordinates, then the popup
    assert_eq!(l.last().unwrap(), &(EeWonEeColorOp::OpenPopup, 0xB00, 0xC00));
    assert_eq!(b.call(EeWonEeColorCall::Field, 0, 0, 0), 0xC00);
    b.release_all();
    assert_eq!(LOG.with(|l| *l.borrow().last().unwrap()), (EeWonEeColorOp::Release, 0xC00, 0));
}

#[test]
fn a_taken_colour_cannot_be_chosen() {
    let _l = testgame::lock();
    fn none(_: EeWonEeGameOp, _: isize, _: isize, _: isize, _: isize) -> isize {
        0
    }
    let g = testgame::new(none);
    // slot 1 plays colour 2; slot 3 observes with colour 5 (not taken)
    testgame::setup(g, &[], &[(1, 0, 3), (1, 2, 2), (3, 0, 3), (3, 2, 5), (3, 4, 1)]);
    unsafe extern "C" fn game_host(
        _o: *mut c_void,
        op: EeWonEeColorOp,
        _a: isize,
        _b: isize,
        _c: isize,
        _d: isize,
        _e: isize,
        _f: isize,
    ) -> isize {
        match op {
            EeWonEeColorOp::MyGame => 0x30,
            EeWonEeColorOp::GameRust => GAME.with(|g| g.get()),
            _ => 0,
        }
    }
    thread_local! {
        static GAME: std::cell::Cell<isize> = const { std::cell::Cell::new(0) };
    }
    GAME.with(|x| x.set(g as isize));
    let c = EeWonEeColor::new(0xC00 as *mut c_void, EeWonEeColorKind::Chooser, Some(game_host));
    c.call(EeWonEeColorCall::AddedToParent, 0, 0, 0);
    assert_eq!(c.call(EeWonEeColorCall::Field, 2 + 2, 0, 0), 1);
    assert_eq!(c.call(EeWonEeColorCall::Field, 2 + 5, 0, 0), 0);
    // over swatch 2 (third column, first row): taken, the click does nothing
    c.call(EeWonEeColorCall::MouseMove, 8 + 2 * 28 + 1, 9, 0);
    assert_eq!(c.call(EeWonEeColorCall::Field, 0, 0, 0), 2);
    c.call(EeWonEeColorCall::MouseDown, 0, 0, 0);
    assert_eq!(c.call(EeWonEeColorCall::Field, 1, 0, 0), -1);
    // swatch 5 is free
    c.call(EeWonEeColorCall::MouseMove, 8 + 28 + 1, 8 + 28 + 1, 0);
    c.call(EeWonEeColorCall::MouseDown, 0, 0, 0);
    assert_eq!(c.call(EeWonEeColorCall::Field, 1, 0, 0), 5);
    // SAFETY: the test's game, freed once
    unsafe { crate::eegame::ee_won_eegame_free(g) };
}
