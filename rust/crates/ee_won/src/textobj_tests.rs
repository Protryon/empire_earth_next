//! The Rust TextArea against the original's (`golden_textobj.txt`, printed by `tests/won/won_textobj_test.cpp
//! --print` over the original TextArea's methods and a scripted host): each sequence is replayed op for op. The
//! replay host answers from the golden (the base class's fields as the scripted host kept them, every op's answer,
//! the size change's call back into AdjustPosition, the link handler's actions) and every op the Rust object makes
//! must be the golden's next line.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{EeWonDrawOp, EeWonSelColor};
use crate::guihost::EeWonGuiOp as Op;
use crate::textobj::{EeWonText, EeWonTextCall as C};

const GOLDEN: &str = include_str!("golden_textobj.txt");

struct Replay {
    lines: Vec<String>,
    at: usize,
    fields: [i32; 16],
    next_seg: isize,
    failure: Option<String>,
    text: *const EeWonText,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

/// the golden's next line, which must start with `want` (up to its answer)
fn expect(want: &str) -> String {
    with(|r| {
        let line = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        let head = line.split(" -> ").next().unwrap_or("");
        if r.failure.is_none() && head != want {
            r.failure = Some(format!("line {}: golden '{}', rust '{}'", r.at, line, want));
        }
        line
    })
}

fn answer(line: &str) -> Vec<i64> {
    line.split(" -> ").nth(1).map_or(Vec::new(), |t| t.split_whitespace().filter_map(|w| w.parse().ok()).collect())
}

fn parse_fields(line: &str) -> [i32; 16] {
    let mut f = [0i32; 16];
    for (i, w) in line.split_whitespace().skip(1).enumerate() {
        if i < 16 {
            f[i] = w.parse().unwrap_or(0);
        }
    }
    f
}

fn text() -> *const EeWonText {
    with(|r| r.text)
}

fn new_seg() -> isize {
    with(|r| {
        r.next_seg += 1;
        0x1000 + r.next_seg
    })
}

unsafe extern "C" fn replay_host(_comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, _d: isize) -> isize {
    let n = op as u32;
    match op {
        Op::Get => with(|r| r.fields[a as usize] as isize),
        Op::Set => {
            expect(&format!("{n} {a} {b} {c} 0"));
            with(|r| r.fields[a as usize] = b as i32);
            0
        }
        Op::BaseSizeChanged => {
            expect(&format!("{n} base-size"));
            let f = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            with(|r| r.fields = parse_fields(&f));
            // SAFETY: the object is live while the replay runs (and called back as the original's base class did)
            unsafe { (*text()).call(C::AdjustPosition, 0, 0, 0) };
            0
        }
        Op::BaseAdjustPosition => {
            let line = expect(&format!("{n} base-adjust {a}"));
            let v = answer(&line);
            with(|r| {
                if v.len() == 3 {
                    r.fields[2] = v[0] as i32;
                    r.fields[3] = v[1] as i32;
                    r.fields[7] = v[2] as i32;
                }
            });
            0
        }
        Op::ScrollTranslate => {
            // SAFETY: the object passes two live i32s
            let (x, y) = unsafe { (&mut *(a as *mut i32), &mut *(b as *mut i32)) };
            let line = expect(&format!("{n} translate {x} {y}"));
            let v = answer(&line);
            if v.len() == 2 {
                *x = v[0] as i32;
                *y = v[1] as i32;
            }
            0
        }
        Op::GetMousePos => {
            let line = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            let w: Vec<i32> = line.split_whitespace().skip(2).filter_map(|w| w.parse().ok()).collect();
            if !line.starts_with(&format!("{n} mouse ")) || w.len() != 2 {
                with(|r| r.failure.get_or_insert(format!("line {}: golden '{line}', rust mouse", r.at)).clone());
                return 0;
            }
            // SAFETY: as above
            unsafe {
                *(a as *mut i32) = w[0];
                *(b as *mut i32) = w[1];
            }
            0
        }
        Op::SelColor => {
            let line = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            let mode = line.split_whitespace().nth(2).and_then(|w| w.parse().ok()).unwrap_or(0);
            if !line.starts_with(&format!("{n} sel-color ")) {
                with(|r| r.failure.get_or_insert(format!("line {}: golden '{line}', rust sel-color", r.at)).clone());
            }
            // SAFETY: the object passes a live EeWonSelColor
            unsafe { *(a as *mut EeWonSelColor) = EeWonSelColor { background: 0x3366cc, foreground: 0xffffff, mode } };
            0
        }
        Op::LinkActivated => {
            let line = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            let action = line.split_whitespace().nth(4).and_then(|w| w.parse::<i32>().ok()).unwrap_or(0);
            let head = format!("{n} link-activated {a:x} action {action}");
            if line != head {
                with(|r| r.failure.get_or_insert(format!("line {}: golden '{line}', rust '{head}'", r.at)).clone());
            }
            // SAFETY: as above (the chat's handler runs inside MouseUp)
            match action {
                1 => unsafe { (*text()).call(C::Reset, 0, 0, 0) },
                2 => {
                    let s = new_seg();
                    unsafe { (*text()).call(C::AddSegment, s, 0, 0) }
                }
                _ => 0,
            };
            0
        }
        Op::SelTextAppend => {
            let want = format!("{n} sel-append {a:x} {b} {c}");
            let line = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            if !line.starts_with(&format!("{want} '")) {
                with(|r| r.failure.get_or_insert(format!("line {}: golden '{line}', rust '{want}'", r.at)).clone());
            }
            0
        }
        Op::SegMeasure => {
            let line = expect(&format!("{n} measure {a:x}"));
            let v = answer(&line);
            if v.len() == 3 {
                // SAFETY: the object passes three live i32s
                unsafe {
                    for (i, x) in v.iter().enumerate() {
                        *(b as *mut i32).add(i) = *x as i32;
                    }
                }
            }
            0
        }
        Op::SegTextTake => {
            let line = with(|r| {
                let l = r.lines.get(r.at).cloned().unwrap_or_default();
                r.at += 1;
                l
            });
            let head = format!("{n} take {a:x} ");
            if !line.starts_with(&head) {
                with(|r| r.failure.get_or_insert(format!("line {}: golden '{line}', rust '{head}'", r.at)).clone());
                return 0;
            }
            // SAFETY: the take op's sink is the object's Vec<u32>
            let out = unsafe { &mut *(b as *mut Vec<u32>) };
            for u in line[head.len()..].split('.').filter(|u| !u.is_empty()) {
                out.push(u32::from_str_radix(u, 16).unwrap_or(0));
            }
            0
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            expect(&format!(
                "{n} draw {a:x} {} {} {} {} {} {} {} {} {} {c:x}",
                o.kind, o.a, o.b, o.c, o.d, o.e, o.f, o.g, o.h
            ));
            0
        }
        _ => {
            let line = expect(&format!("{n} {a} {b} {c} 0"));
            answer(&line).first().copied().unwrap_or(0) as isize
        }
    }
}

fn call_of(name: &str) -> Option<C> {
    Some(match name {
        "line-break" => C::LineBreak,
        "align" => C::SetLineAlignment,
        "size" => C::SizeChanged,
        "visible" => C::NotifyRootVisibility,
        "adjust" => C::AdjustPosition,
        "paint" => C::ScrollPaint,
        "enter" => C::MouseEnter,
        "exit" => C::MouseExit,
        "down" => C::MouseDown,
        "up" => C::MouseUp,
        "move" => C::MouseMove,
        "drag" => C::MouseDrag,
        "timer" => C::TimerEvent,
        "key-char" => C::KeyChar,
        "key-down" => C::KeyDown,
        "flags" => C::SetTextFlags,
        "max-chars" => C::SetMaxChars,
        "max-lines" => C::SetMaxLines,
        "indent" => C::SetWrapIndentSize,
        "check-sel" => C::CheckSel,
        "reset" => C::Reset,
        "num-lines" => C::GetNumLines,
        _ => return None,
    })
}

/// replays one sequence; the first difference, if any
fn replay(lines: &[String]) -> Option<String> {
    let fields = parse_fields(&lines[0]);
    REPLAY.with(|r| {
        *r.borrow_mut() =
            Some(Replay { lines: lines.to_vec(), at: 1, fields, next_seg: 0, failure: None, text: std::ptr::null() })
    });
    let mut holder = Some(Box::new(EeWonText::new(0x51 as *mut c_void, Some(replay_host))));
    let t: &EeWonText = holder.as_deref().expect("the area");
    let tp = t as *const EeWonText;
    with(|r| r.text = tp);
    t.call(C::Reset, 0, 0, 0);
    loop {
        let (line, done) = with(|r| {
            if r.failure.is_some() || r.at >= r.lines.len() {
                return (String::new(), true);
            }
            let l = r.lines[r.at].clone();
            r.at += 1;
            (l, false)
        });
        if done {
            break;
        }
        if line == "free" {
            break;
        }
        let Some(rest) = line.strip_prefix("call ") else {
            return Some(format!("unexpected golden line '{line}'"));
        };
        let mut parts = rest.split(" -> ");
        let words: Vec<&str> = parts.next().unwrap_or("").split_whitespace().collect();
        let want: Option<isize> = parts.next().and_then(|w| w.trim().parse().ok());
        let args: Vec<isize> = words[1..].iter().filter_map(|w| w.parse().ok()).collect();
        let (a, b, c) = (args[0], args[1], args[2]);
        let got = match words[0] {
            "add-text" | "add-link" => {
                let s = new_seg();
                t.call(C::AddSegment, s, 0, 0);
                continue;
            }
            "paint" => t.call(C::ScrollPaint, 6, 0, 0),
            name => match call_of(name) {
                Some(call) => t.call(call, a, b, c),
                None => return Some(format!("unknown call '{name}'")),
            },
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    // the area goes (its references given back) while the replay still answers
    drop(holder.take());
    let f = with(|r| {
        if r.failure.is_none() && r.lines.get(r.at).map(String::as_str) != Some("refs back") {
            r.failure = Some(format!("line {}: the golden goes on", r.at));
        }
        r.failure.clone()
    });
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
fn golden_textobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    let mut lines = 0usize;
    for (i, s) in seqs.iter().enumerate() {
        lines += s.len();
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
    assert!(lines > 6000);
}

#[test]
fn a_changed_answer_is_seen() {
    // the replay is not vacuous: one draw changed in the golden makes it differ
    let seqs = sequences();
    let s = &seqs[0];
    let k = s.iter().position(|l| l.contains(" draw ")).expect("a draw");
    let mut m = s.clone();
    m[k] = m[k].replace(" draw 6 ", " draw 7 ");
    assert!(replay(&m).is_some());
}
