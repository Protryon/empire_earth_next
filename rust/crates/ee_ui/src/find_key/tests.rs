//! The type-to-find against the real C++ (`golden_group.txt`, its `list` cases): a scratch driver
//! (`build/ui_group_golden`) ran the unmodified `UILBString::UpdateTranslatedKey` over 400 string list boxes (a
//! vtable of logging stubs: `GetNumLines` answers the lines' count, `ChangeSelectionIndex` and `SetDirty` log) with
//! up to nine lines from a set of ASCII and UTF-8 words in both cases and empty lines, a selection inside, past the
//! end or none, a scroll bar or none, and a typed key (ASCII, UTF-8 of two and three bytes, empty, two characters).
//! Each case prints its inputs, the trace (every `GetNumLines`, the scroll bar's value, the virtual calls) and the
//! result. The replay is the same list over [`find`] and the C++'s actions after it; the driver relinked over the
//! forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_group.txt");

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

struct List {
    lines: Vec<Vec<u8>>,
    trace: String,
}

impl FindHost for List {
    fn lines(&mut self) -> usize {
        self.trace.push_str(&format!("n={} ", self.lines.len()));
        self.lines.len()
    }
    fn first(&mut self, i: usize) -> Option<u8> {
        self.lines[i].first().copied()
    }
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut cases = 0;
    for at in (0..lines.len()).step_by(3) {
        let t: Vec<&str> = lines[at].split(' ').collect();
        if t[0] != "list" {
            continue;
        }
        let selection = t[3].parse::<i64>().unwrap() as c_ulong;
        let scroll = t[5] == "1";
        let key = unhex(t[7]);
        let n: usize = t[9].parse().unwrap();
        let mut l = List { lines: t[10..10 + n].iter().map(|s| unhex(s)).collect(), trace: String::new() };
        let result = match find(&key, selection, &mut l) {
            Some(i) => {
                if scroll {
                    l.trace.push_str(&format!("sv={i} "));
                }
                l.trace.push_str(&format!("o0.select({i},1,1) o0.dirty(1) "));
                0
            }
            None => 3,
        };
        assert_eq!(format!("trace {}", l.trace), lines[at + 1], "{}", lines[at]);
        assert_eq!(format!("out {result} sel {}", selection as c_long), lines[at + 2], "{}", lines[at]);
        cases += 1;
    }
    assert_eq!(cases, 400);
}

struct Plain(Vec<&'static [u8]>);

impl FindHost for Plain {
    fn lines(&mut self) -> usize {
        self.0.len()
    }
    fn first(&mut self, i: usize) -> Option<u8> {
        self.0[i].first().copied()
    }
}

#[test]
fn the_search_starts_after_the_selection_and_wraps_once() {
    let mut l = Plain(vec![b"apple", b"Banana", b"avocado", b"", b"cherry"]);
    assert_eq!(find(b"a", c_ulong::MAX, &mut l), Some(0));
    assert_eq!(find(b"A", 0, &mut l), Some(2));
    assert_eq!(find(b"a", 2, &mut l), Some(0));
    assert_eq!(find(b"b", 1, &mut l), Some(1));
    assert_eq!(find(b"x", 1, &mut l), None);
    // a selection past the end starts at the top
    assert_eq!(find(b"c", 9, &mut l), Some(4));
    // UTF-8: the first byte of the key's case forms; "é" and "É" share their lead byte 0xc3
    let mut l = Plain(vec!["\u{c9}cole".as_bytes(), "\u{e9}t\u{e9}".as_bytes()]);
    assert_eq!(key_bytes("\u{e9}".as_bytes()), (0xc3, 0xc3));
    assert_eq!(find("\u{e9}".as_bytes(), c_ulong::MAX, &mut l), Some(0));
    assert_eq!(find(b"", c_ulong::MAX, &mut l), None);
}
