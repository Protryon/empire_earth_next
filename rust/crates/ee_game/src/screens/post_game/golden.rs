//! The post-game summary against the real C++: a scratch driver (`build/sc10/pgs_driver.cpp`) ran the unmodified
//! `EEUIEPostGameSummaryEvents.cpp` over a fake form 25 (status bars, bitmaps, labels, radio buttons by control
//! index, entity groups, backgrounds), the real line graph, `UGraph` and player statistics of a drawn history (up to
//! nine records), the strings, the preferences, the stats server's file and protocol, the campaign manager and the
//! user interface thread; every out-of-line call printed, and the graphs that changed hashed after each event. The
//! driver relinked over `EEScreens.cpp` and this crate prints the same (771,398 lines for 1,000 cases on seed 1, and
//! seeds 2-4 at 2,000 cases each). The graphs' answers depend on `UGraph`'s pruning and combining, which stay the
//! toolkit's: so this replay's input is that relinked run's host log (`golden_post_game.txt`, 200 cases of seed 1):
//! per event the history and records the host handed over, the texts other than the resource strings, every call's
//! answer, and a hash of every call with its arguments and answer in order. The replay answers from the log and
//! checks the hash: it pins the Rust's sequence of host calls (which the relinked driver proved equal to the
//! original's effects) against regressions.

use std::collections::{BTreeMap, VecDeque};

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_post_game.txt");

fn unhex(text: &str) -> Vec<u8> {
    (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).unwrap()).collect()
}
fn from_bytes<T: Copy>(bytes: &[u8]) -> T {
    assert_eq!(bytes.len(), core::mem::size_of::<T>());
    // SAFETY: the bytes are a `T` the C++ host wrote (repr(C), plain integers, floats and bools of 0 or 1)
    unsafe { core::ptr::read_unaligned(bytes.as_ptr() as *const T) }
}

struct Replay {
    hash: u64,
    history: Option<EeScreensPgsHistory>,
    records: BTreeMap<u32, EeScreensPgsRecord>,
    texts: VecDeque<(u32, u32, Option<Vec<u8>>)>,
    answers: VecDeque<u32>,
}

impl Replay {
    fn mix(&mut self, v: u64) {
        self.hash ^= v;
        self.hash = self.hash.wrapping_mul(1099511628211);
    }
}

impl PostGameHost for Replay {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        let r = self.answers.pop_front().expect("more calls than the log");
        for v in [b'c' as u64, op as u64, a as u64, b as u64, c as u64, d as u64, r as u64] {
            self.mix(v);
        }
        r
    }
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>> {
        let text = if which == EE_SCREENS_TEXT_STRING {
            Some(format!("S{arg}").into_bytes())
        } else {
            let (w, a, text) = self.texts.pop_front().expect("more texts than the log");
            assert_eq!((w, a), (which, arg));
            text
        };
        let missing = text.is_none();
        let bytes = text.clone().unwrap_or_default();
        for v in [b't' as u64, which as u64, arg as u64, missing as u64, bytes.len() as u64] {
            self.mix(v);
        }
        for &c in &bytes {
            self.mix(c as u64);
        }
        text
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        let r = self.answers.pop_front().expect("more puts than the log");
        for v in [b'p' as u64, which as u64, arg as u64, text.len() as u64, r as u64] {
            self.mix(v);
        }
        for &c in text {
            self.mix(c as u64);
        }
        r
    }
    fn history(&mut self) -> EeScreensPgsHistory {
        self.mix(b'h' as u64);
        self.history.expect("a history in the log")
    }
    fn record(&mut self, index: u32) -> EeScreensPgsRecord {
        self.mix(b'r' as u64);
        self.mix(index as u64);
        self.records[&index]
    }
}

#[test]
fn golden_post_game() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 200);
    let state = Mutex::new(PostGame::new());
    let mut events = std::collections::BTreeSet::new();
    let mut calls = 0usize;
    for (n, case) in cases.iter().enumerate() {
        let event = u32::from_str_radix(case[0].split(' ').nth(2).unwrap(), 16).unwrap();
        let kind = if (EE_SCREENS_PGS_CHECKED..EE_SCREENS_PGS_OPEN).contains(&event) { event & !0xf } else { event };
        events.insert(kind);
        let mut r = Replay {
            hash: 1469598103934665603,
            history: None,
            records: BTreeMap::new(),
            texts: VecDeque::new(),
            answers: VecDeque::new(),
        };
        let mut expected = 0u64;
        for line in &case[1..] {
            let (kind, rest) = line.split_once(' ').unwrap_or((line, ""));
            match kind {
                "h" => r.history = Some(from_bytes(&unhex(rest))),
                "r" => {
                    let (i, bytes) = rest.split_once(' ').unwrap();
                    r.records.insert(u32::from_str_radix(i, 16).unwrap(), from_bytes(&unhex(bytes)));
                }
                "t" => {
                    let t: Vec<&str> = rest.split(' ').collect();
                    let text = t[2].strip_prefix('=').map(unhex);
                    let which = u32::from_str_radix(t[0], 16).unwrap();
                    r.texts.push_back((which, u32::from_str_radix(t[1], 16).unwrap(), text));
                }
                "a" => {
                    r.answers =
                        rest.split(' ').filter(|s| !s.is_empty()).map(|s| u32::from_str_radix(s, 16).unwrap()).collect()
                }
                "x" => expected = u64::from_str_radix(rest, 16).unwrap(),
                other => panic!("case {n}: no line kind {other}"),
            }
        }
        calls += r.answers.len();
        assert_eq!(dispatch(&mut r, &state, event), Some(0), "case {n}");
        assert!(r.answers.is_empty(), "case {n}: {} calls fewer than the log", r.answers.len());
        assert!(r.texts.is_empty(), "case {n}: {} texts fewer than the log", r.texts.len());
        assert_eq!(r.hash, expected, "case {n} ({})", case[0]);
    }
    // open, close, exit, next graph, the seven tabs, both check boxes
    assert_eq!(events.len(), 13, "{events:x?}");
    assert!(calls > 100_000);
}
