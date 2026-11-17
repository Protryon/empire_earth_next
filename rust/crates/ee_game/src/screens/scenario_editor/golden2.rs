//! The editor form's and its tabs' events against the real C++: a scratch driver (`build/sc14/se2_driver.cpp`) ran
//! the unmodified event bodies (extracted verbatim from their units by `build/sc14/make_g2_old.py`) on a fake editor
//! form, each case drawing the event, its control id, the elevation sliders' values, the manager's editor flag, the
//! file's open result, an edit box's lines for the "///" expansion (digits, commas, other characters, numbers past
//! `i32::MAX`, a two-byte character), and now and then the exit question's yes or no; every call out was printed
//! (the tabs', the camera's turns' matrices, the files', the controls' display modes, the labels' and edit box's
//! texts, the posted actions and load screens, the question). The driver relinked over the new unit (`EEScreenEvent`s
//! over this module and `ee_screenscpp_se`, `_text`, `_set_text`) printed the same on seeds 1-4 (3,000 cases each).
//! Its host log is `golden_scenario_editor2.txt` (seed 1): per case `case N code open-answer`, then the host calls
//! `op a b answer`, the texts `t which arg hex` and the edit box's text set `p hex` (`-` for an empty text).

use super::*;

const GOLDEN: &str = include_str!("../golden_scenario_editor2.txt");

enum Call {
    Op([u32; 4]),
    Text(u32, u32, Vec<u8>),
    Put(Vec<u8>),
}

fn hex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

struct Replay<'a> {
    calls: std::slice::Iter<'a, Call>,
    case: &'a str,
}

impl SeHost for Replay<'_> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        match self.calls.next() {
            Some(Call::Op(want)) => {
                assert_eq!([op, a, b], want[..3], "{}", self.case);
                want[3]
            }
            _ => panic!("{}: call {op} {a} {b} not in the log here", self.case),
        }
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match self.calls.next() {
            Some(Call::Text(w, a, text)) => {
                assert_eq!((which, arg), (*w, *a), "{}", self.case);
                text.clone()
            }
            _ => panic!("{}: text {which} {arg} not in the log here", self.case),
        }
    }
    fn set_edit_text(&mut self, text: &[u8]) {
        match self.calls.next() {
            Some(Call::Put(want)) => assert_eq!(text, &want[..], "{}", self.case),
            _ => panic!("{}: a text set not in the log here", self.case),
        }
    }
}

fn blocks() -> Vec<(&'static str, Vec<Call>)> {
    let mut out: Vec<(&'static str, Vec<Call>)> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(rest) = line.strip_prefix(" t ") {
            let w: Vec<&str> = rest.split(' ').collect();
            let call = Call::Text(w[0].parse().unwrap(), w[1].parse().unwrap(), hex(w[2]));
            out.last_mut().unwrap().1.push(call);
        } else if let Some(rest) = line.strip_prefix(" p ") {
            out.last_mut().unwrap().1.push(Call::Put(hex(rest)));
        } else if let Some(call) = line.strip_prefix(' ') {
            let v: Vec<u32> = call.split(' ').map(|x| x.parse().unwrap()).collect();
            out.last_mut().unwrap().1.push(Call::Op([v[0], v[1], v[2], v[3]]));
        } else {
            out.push((line, Vec::new()));
        }
    }
    out
}

#[test]
fn replays_the_golden() {
    let blocks = blocks();
    let mut seen = std::collections::BTreeMap::new();
    let mut sets = 0;
    for (header, expected) in &blocks {
        let words: Vec<&str> = header.split(' ').collect();
        let code: u32 = words[2].parse().unwrap();
        let open: i32 = words[3].parse().unwrap();
        let mut h = Replay { calls: expected.iter(), case: header };
        let want = if code == EE_SCREENS_SE_OPEN_CIV_BUILDER { open } else { 0 };
        assert_eq!(dispatch(&mut h, code), Some(want as core::ffi::c_long), "{header}");
        assert!(h.calls.next().is_none(), "{header}: calls left");
        *seen.entry(code).or_insert(0) += 1;
        sets += expected.iter().filter(|c| matches!(c, Call::Put(_))).count();
    }
    assert_eq!(blocks.len(), 3000);
    assert_eq!(seen.len(), 15, "{seen:?}");
    assert!(seen.values().all(|&n| n > 30), "{seen:?}");
    assert!(sets > 100, "{sets}");
}

/// A host for the "///" expansion: the box's lines and the strings `<id>`.
struct Find {
    lines: Vec<&'static [u8]>,
    set: Option<Vec<u8>>,
}

impl SeHost for Find {
    fn call(&mut self, op: u32, _: u32, _: u32) -> u32 {
        assert_eq!(op, EE_SCREENS_SE_OP_EDIT_LINES);
        self.lines.len() as u32
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        if which == EE_SCREENS_SE_TEXT_EDIT_LINE {
            self.lines[arg as usize].to_vec()
        } else {
            format!("<{arg}>").into_bytes()
        }
    }
    fn set_edit_text(&mut self, text: &[u8]) {
        self.set = Some(text.to_vec());
    }
}

fn find(lines: &[&'static [u8]]) -> Option<String> {
    let mut h = Find { lines: lines.to_vec(), set: None };
    dispatch(&mut h, EE_SCREENS_SE_FIND_TEXT);
    h.set.map(|s| String::from_utf8(s).unwrap())
}

#[test]
fn the_expansion_reads_ids_across_lines_and_stops_at_another_character() {
    assert_eq!(find(&[b"///12,3"]).as_deref(), Some("<12><3>"));
    assert_eq!(find(&[b"///1", b"2,", b"3"]).as_deref(), Some("<12><3>"));
    assert_eq!(find(&[b"///7,x,9"]).as_deref(), Some("<7>"));
    assert_eq!(find(&[b"///,"]).as_deref(), Some("<0>"));
    assert_eq!(find(&[b"///99999999999"]).as_deref(), Some("<2147483647>"));
    assert_eq!(find(&[b"//12"]), None);
    assert_eq!(find(&[b"///"]), None);
}
