//! The message box against the real C++: a scratch driver (`build/sc13/dlg_driver.cpp`) ran the unmodified
//! `EEUIFError.cpp` (`Display*`, `Open` with its `Layout*`) over fake controls (the buttons with their texts, the
//! message label's lines, the edit box's line, single-line flag and index), a fake layout utility, the fonts and window
//! sizes it drew, printing every toolkit call and each control's state after; the driver relinked over the new unit
//! (`Display`, `Open` over this module and `ee_screenscpp_dialog`) printed the same on seeds 1-4 (2,000 cases; 20,000
//! on seeds 2-4). Its host log is `golden_dialog.txt`: per case the kind shown (alignment, flag) and the mode opened,
//! each followed by the host calls `op a b answer` of the relinked run. The replay runs `show` and `open` with each
//! case's inputs over a host that checks every call against the log and gives its answer.

use super::*;

const GOLDEN: &str = include_str!("../golden_dialog.txt");

struct Replay<'a> {
    calls: std::slice::Iter<'a, [u32; 4]>,
    case: &'a str,
}

impl DialogHost for Replay<'_> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        let want = self.calls.next().unwrap_or_else(|| panic!("{}: an extra call {op} {a} {b}", self.case));
        assert_eq!([op, a, b], want[..3], "{}", self.case);
        want[3]
    }
}

/// The log as (header, calls) blocks.
fn blocks() -> Vec<(&'static str, Vec<[u32; 4]>)> {
    let mut out: Vec<(&'static str, Vec<[u32; 4]>)> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(call) = line.strip_prefix(' ') {
            let v: Vec<u32> = call.split(' ').map(|x| x.parse().unwrap()).collect();
            out.last_mut().unwrap().1.push([v[0], v[1], v[2], v[3]]);
        } else {
            out.push((line, Vec::new()));
        }
    }
    out
}

#[test]
fn replays_the_golden() {
    let blocks = blocks();
    let (mut shows, mut opens, mut calls) = (0, 0, 0);
    let mut case = "";
    for (header, expected) in &blocks {
        let words: Vec<&str> = header.split(' ').collect();
        let mut h = Replay { calls: expected.iter(), case: header };
        if words[0] == "case" {
            case = header;
            assert_eq!(words[2], "show");
            let (kind, alignment, flag) = (words[3].parse().unwrap(), words[4].parse().unwrap(), words[5] == "1");
            show(&mut h, kind, alignment, flag);
            shows += 1;
        } else {
            assert_eq!(words[0], "open", "{case}");
            h.case = case;
            open(&mut h, words[1].parse().unwrap());
            opens += 1;
        }
        assert!(h.calls.next().is_none(), "{header}: calls left");
        calls += expected.len();
    }
    assert_eq!((shows, opens), (2000, 2000));
    assert!(calls > 30_000, "{calls}");
}

/// Every kind and every mode reached, and the message box's centring both ways.
#[test]
fn the_golden_reaches_every_path() {
    let blocks = blocks();
    let mut kinds = [0u32; 5];
    let mut modes = [0u32; 7];
    let (mut centred, mut not_centred) = (0, 0);
    for (header, calls) in &blocks {
        let words: Vec<&str> = header.split(' ').collect();
        if words[0] == "case" {
            kinds[words[3].parse::<usize>().unwrap()] += 1;
        } else {
            let mode: usize = words[1].parse().unwrap();
            modes[mode] += 1;
            if mode == 0 {
                let top = calls.iter().find(|c| c[0] == EE_SCREENS_DLG_RECT && c[1] == 1).unwrap()[3];
                let set = calls.iter().find(|c| c[0] == EE_SCREENS_DLG_SET_RECT && c[1] == 1).unwrap()[2];
                if top == set {
                    not_centred += 1;
                } else {
                    centred += 1;
                }
            }
        }
    }
    assert!(kinds.iter().all(|&k| k > 300), "{kinds:?}");
    assert!(modes.iter().all(|&m| m > 20), "{modes:?}");
    assert!(centred > 50 && not_centred > 50, "{centred} {not_centred}");
}

/// A host that records the calls.
#[derive(Default)]
struct Record(Vec<[u32; 3]>);

impl DialogHost for Record {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        self.0.push([op, a, b]);
        match op {
            EE_SCREENS_DLG_RECT => [0.0f32, 100.0, 0.0, 300.0][a as usize].to_bits(),
            EE_SCREENS_DLG_LINE_HEIGHT => 12,
            EE_SCREENS_DLG_LINES => 3,
            _ => 0,
        }
    }
}

#[test]
fn a_message_box_centres_its_text() {
    // 200 / 12 -> 16, less 2: 14 lines fit, 3 used: (14 - 3) * 12 / 2 = 66
    let mut h = Record::default();
    open(&mut h, EE_SCREENS_DIALOG_MESSAGE);
    let n = h.0.len();
    assert_eq!(h.0[n - 3], [EE_SCREENS_DLG_SET_RECT, 1, 166.0f32.to_bits()]);
    assert_eq!(h.0[n - 2], [EE_SCREENS_DLG_SET_RECT, 3, 234.0f32.to_bits()]);
}

#[test]
fn an_unknown_kind_shows_nothing() {
    let mut h = Record::default();
    show(&mut h, 5, 0, true);
    open(&mut h, 5);
    assert!(h.0.is_empty());
}
