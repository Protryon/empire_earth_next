//! The Rust Label against the original's (`golden_labelobj.txt`, printed by `tests/won/won_labelobj_test.cpp
//! --print` over the original Label's methods and a scripted host): each sequence is replayed op for op, the
//! replay host answering from the golden (every op's answer and the size change's call back).

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::labelobj::{EeWonLabel, EeWonLabelCall as C};

const GOLDEN: &str = include_str!("golden_labelobj.txt");

struct Replay {
    lines: Vec<String>,
    at: usize,
    width: i32,
    height: i32,
    failure: Option<String>,
    label: *const EeWonLabel,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

fn next() -> String {
    with(|r| {
        let l = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        l
    })
}

/// the golden's next line, which must start with `want`; the words after it
fn expect(want: &str) -> Vec<String> {
    let line = next();
    if !line.starts_with(want) {
        with(|r| {
            if r.failure.is_none() {
                r.failure = Some(format!("line {}: golden '{line}', rust '{want}'", r.at));
            }
        });
        return Vec::new();
    }
    line[want.len()..].split_whitespace().map(str::to_string).collect()
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Get => with(|r| {
            if a == F::Width as isize {
                r.width
            } else if a == F::Height as isize {
                r.height
            } else {
                0
            }
        }) as isize,
        Op::SetSize => {
            let w = expect(&format!("{n} set-size {a} {b} -> "));
            with(|r| {
                r.width = a as i32;
                r.height = b as i32;
            });
            if w.first().map(String::as_str) == Some("1") {
                // SAFETY: the label is live while the replay runs (Component::SetSize calls Virtual08 back)
                unsafe { (*with(|r| r.label)).call(C::SizeChanged, 0, 0, 0) };
            }
            0
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            expect(&format!(
                "{n} draw {a:x} {} {} {} {} {} {} {} {} {}",
                o.kind, o.a, o.b, o.c, o.d, o.e, o.f, o.g, o.h
            ));
            0
        }
        _ => {
            let w = expect(&format!("{n} {a} {b} {c} -> "));
            w.first().and_then(|x| x.parse().ok()).unwrap_or(0)
        }
    }
}

fn replay(lines: &[String]) -> Option<String> {
    let start: Vec<i32> = lines[0].split_whitespace().skip(1).filter_map(|w| w.parse().ok()).collect();
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.to_vec(),
            at: 1,
            width: start[0],
            height: start[1],
            failure: None,
            label: std::ptr::null(),
        })
    });
    let holder = Box::new(EeWonLabel::new(0x53 as *mut c_void, Some(replay_host)));
    let t: &EeWonLabel = &holder;
    with(|r| r.label = t as *const EeWonLabel);
    loop {
        let (line, done) = with(|r| {
            if r.failure.is_some() || r.at >= r.lines.len() {
                return (String::new(), true);
            }
            let l = r.lines[r.at].clone();
            r.at += 1;
            (l, false)
        });
        if done || line == "free" {
            break;
        }
        let Some(rest) = line.strip_prefix("call ") else {
            return Some(format!("unexpected golden line '{line}'"));
        };
        let mut parts = rest.split(" -> ");
        let words: Vec<&str> = parts.next().unwrap_or("").split_whitespace().collect();
        let want: Option<isize> = parts.next().and_then(|w| w.trim().parse().ok());
        let a: isize = words[1].parse().unwrap_or(0);
        let b: isize = words[2].parse().unwrap_or(0);
        let got = match words[0] {
            "text" => t.call(C::TextSet, b, 0, 0),
            "horz" => t.call(C::SetHorzAlign, a, 0, 0),
            "vert" => t.call(C::SetVertAlign, a, 0, 0),
            "wrap" => t.call(C::SetWrap, a, 0, 0),
            "adjust" => t.call(C::SetAdjustHeightToFitText, a, 0, 0),
            "pads" => t.call(C::SetVertPad, a, b, 0),
            "color" => t.call(C::SetTextColor, a, 0, 0),
            "size" => {
                with(|r| {
                    r.width = a as i32;
                    r.height = b as i32;
                });
                t.call(C::SizeChanged, 0, 0, 0)
            }
            "desired" => {
                let (w, h) = t.desired_size();
                (w * 1000 + h) as isize
            }
            "paint" => t.call(C::Paint, 6, 0, 0),
            "get-color" => t.call(C::TextColor, 0, 0, 0),
            name => return Some(format!("unknown call '{name}'")),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    drop(holder);
    let f = with(|r| r.failure.clone());
    REPLAY.with(|r| *r.borrow_mut() = None);
    f
}

fn sequences() -> Vec<Vec<String>> {
    let mut out: Vec<Vec<String>> = Vec::new();
    for l in GOLDEN.lines() {
        if l.starts_with("seq ") {
            out.push(Vec::new());
        } else if let Some(s) = out.last_mut() {
            s.push(l.to_string());
        }
    }
    out
}

#[test]
fn golden_labelobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_changed_answer_is_seen() {
    let seqs = sequences();
    let (s, k) =
        seqs.iter().find_map(|s| s.iter().position(|l| l.contains(" draw 6 ")).map(|k| (s, k))).expect("a draw");
    let mut m = s.clone();
    m[k] = m[k].replacen(" draw 6 ", " draw 7 ", 1);
    assert!(replay(&m).is_some());
}
