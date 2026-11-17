//! The Rust InputBox against the original's (`golden_inputobj.txt`, printed by `tests/won/won_inputobj_test.cpp
//! --print` over the original InputBox's methods and a scripted host): each sequence is replayed op for op. The
//! replay host answers from the golden (every op's answer, the mouse, the clipboard, the filter's verdict and
//! text, the paint's C++ members, the event handler's actions and the size change's call back) and every op the
//! Rust object makes must be the golden's next line.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{EeWonDrawOp, EeWonInputPaint};
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op};
use crate::inputobj::{units_in, wunit, EeWonInput, EeWonInputCall as C, WCHAR_FOR_TESTS};

const GOLDEN: &str = include_str!("golden_inputobj.txt");

struct Replay {
    lines: Vec<String>,
    at: usize,
    width: i32,
    height: i32,
    real: Vec<u32>,
    failure: Option<String>,
    input: *const EeWonInput,
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

/// the golden's next line
fn next() -> String {
    with(|r| {
        let l = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        l
    })
}

fn fail(line: &str, want: &str) {
    with(|r| {
        if r.failure.is_none() {
            r.failure = Some(format!("line {}: golden '{line}', rust '{want}'", r.at));
        }
    });
}

/// the golden's next line, which must start with `want`; the words after it
fn expect(want: &str) -> Vec<String> {
    let line = next();
    if !line.starts_with(want) {
        fail(&line, want);
        return Vec::new();
    }
    line[want.len()..].split_whitespace().map(str::to_string).collect()
}

fn hex(u: &[u32]) -> String {
    let parts: Vec<String> = u.iter().map(|x| format!("{x:x}")).collect();
    format!("'{}'", parts.join("."))
}

fn unhex(s: &str) -> Vec<u32> {
    s.trim_matches('\'').split('.').filter(|p| !p.is_empty()).map(|p| u32::from_str_radix(p, 16).unwrap_or(0)).collect()
}

fn slice(p: isize, n: isize) -> Vec<u32> {
    if p == 0 || n <= 0 {
        return Vec::new();
    }
    // SAFETY: the object passes `n` u32s at `p`
    unsafe { std::slice::from_raw_parts(p as *const u32, n as usize) }.to_vec()
}

fn input() -> &'static EeWonInput {
    // SAFETY: the object is live while the replay runs
    unsafe { &*with(|r| r.input) }
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
                input().call(C::SizeChanged, 0, 0, 0);
            }
            0
        }
        Op::GetMousePos => {
            let w = expect(&format!("{n} mouse "));
            if w.len() == 2 {
                // SAFETY: the object passes two live i32s
                unsafe {
                    *(a as *mut i32) = w[0].parse().unwrap_or(0);
                    *(b as *mut i32) = w[1].parse().unwrap_or(0);
                }
            }
            0
        }
        Op::TextStore => {
            let u = slice(b, c);
            expect(&format!("{n} store {a} {}", hex(&u)));
            if a == 1 {
                with(|r| r.real = u);
            }
            0
        }
        Op::Filter => {
            let u = slice(a, b);
            let w = expect(&format!("{n} filter {} -> ", hex(&u)));
            let r: isize = w.first().and_then(|x| x.parse().ok()).unwrap_or(2);
            if r != 2 {
                // SAFETY: the filter's sink is the object's Vec<u32>
                let out = unsafe { &mut *(c as *mut Vec<u32>) };
                out.extend(unhex(w.get(1).map_or("''", String::as_str)));
            }
            r
        }
        Op::ClipboardSet => {
            expect(&format!("{n} clip-set {}", hex(&slice(a, b))));
            0
        }
        Op::ClipboardTake => {
            let w = expect(&format!("{n} clip-take "));
            // SAFETY: the take's sink is the object's Vec<u32>
            let out = unsafe { &mut *(a as *mut Vec<u32>) };
            out.extend(unhex(w.first().map_or("''", String::as_str)));
            0
        }
        Op::FireEvent => {
            // SAFETY: the object fires an event code of the enum
            let e = unsafe { std::mem::transmute::<u32, crate::guihost::EeWonGuiEvent>(a as u32) };
            let ty = crate::guihost::ee_won_gui_event_type(e);
            let w = expect(&format!("{n} fire {ty} -> "));
            let action: i32 = w.first().and_then(|x| x.parse().ok()).unwrap_or(0);
            let t = input();
            if action == 1 {
                t.set_text(&['o' as u32, 'k' as u32], false);
            } else if action == 2 && ty == 20 {
                let real = with(|r| r.real.clone());
                t.add_input_history(&real, true);
                t.set_text(&[], false);
            } else if action == 3 {
                t.select_range(0, -1);
            }
            0
        }
        Op::PaintInputs => {
            let w = expect(&format!("{n} paint-inputs "));
            let v: Vec<i32> = w.iter().filter_map(|x| x.parse().ok()).collect();
            if v.len() == 8 {
                // SAFETY: the object passes a live EeWonInputPaint
                let p = unsafe { &mut *(a as *mut EeWonInputPaint) };
                p.disabled_look = v[0] != 0;
                p.has_icon = v[1] != 0;
                p.has_focus = v[2] != 0;
                p.text_color = v[3];
                p.disabled_text_color = v[4];
                p.icon_height = v[5];
                p.font_height = v[6];
                p.sel.background = 0x3366cc;
                p.sel.foreground = 0xffffff;
                p.sel.mode = v[7];
            }
            0
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            let want =
                format!("{n} draw {a:x} {} {} {} {} {} {} {} {} {}", o.kind, o.a, o.b, o.c, o.d, o.e, o.f, o.g, o.h);
            let line = next();
            if line != want {
                fail(&line, &want);
            }
            0
        }
        _ => {
            let w = expect(&format!("{n} {a} {b} {c} -> "));
            w.first().and_then(|x| x.parse().ok()).unwrap_or(0)
        }
    }
}

/// replays one sequence; the first difference, if any
fn replay(lines: &[String]) -> Option<String> {
    let start: Vec<i32> = lines[0].split_whitespace().skip(1).filter_map(|w| w.parse().ok()).collect();
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.to_vec(),
            at: 1,
            width: start[0],
            height: start[1],
            real: Vec::new(),
            failure: None,
            input: std::ptr::null(),
        })
    });
    let holder = Box::new(EeWonInput::new(0x52 as *mut c_void, Some(replay_host)));
    let t: &EeWonInput = &holder;
    with(|r| r.input = t as *const EeWonInput);
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
        let args: Vec<isize> = words[1..4].iter().filter_map(|w| w.parse().ok()).collect();
        let (a, b, c) = (args[0], args[1], args[2]);
        let text = unhex(words.get(4).copied().unwrap_or("''"));
        let got: isize = match words[0] {
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
            "got-focus" => t.call(C::GotFocus, 0, 0, 0),
            "lost-focus" => t.call(C::LostFocus, 0, 0, 0),
            "down" => t.call(C::MouseDown, a, b, c),
            "up" => t.call(C::MouseUp, a, b, c),
            "exit" => t.call(C::MouseExit, 0, 0, 0),
            "drag" => t.call(C::MouseDrag, a, b, 0),
            "timer" => t.call(C::TimerEvent, a, 0, 0),
            "key-char" => t.call(C::KeyChar, a, 0, 0),
            "key-down" => t.call(C::KeyDown, a, 0, 0),
            "paint" => t.call(C::Paint, 6, 0, 0),
            "set-text" => {
                t.set_text(&text, a != 0);
                0
            }
            "password" => t.call(C::SetPasswordChar, a as i8 as isize, 0, 0),
            "allow" => t.call(C::SetAllowSelection, a, 0, 0),
            "select" => t.call(C::SelectRange, a, b, 0),
            "icon" => t.call(C::IconChanged, 0, 0, 0),
            "char-width" => t.call(C::SetCharWidth, a, 0, 0),
            "blink" => t.call(C::SetCursorBlinkRate, a, b, 0),
            "adjust" => t.call(C::AdjustPosition, 0, 0, 0),
            "copy" => t.call(C::CopySelection, 0, 0, 0),
            "sel-text" => {
                t.set_sel_text(&text, a != 0);
                0
            }
            "add-history" => {
                t.add_input_history(&text, a != 0);
                0
            }
            "set-history" => {
                t.set_input_history(&text, a != 0);
                0
            }
            "max-chars" => t.call(C::SetMaxChars, a, 0, 0),
            "select-on-focus" => t.call(C::SetSelectOnFocus, a, 0, 0),
            "max-history" => t.call(C::SetMaxInputHistory, a, 0, 0),
            "left-pad" => t.call(C::SetLeftPad, a, 0, 0),
            "sel-start" => t.call(C::SelStart, 0, 0, 0),
            "filter" => 0,
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
fn golden_inputobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    let mut lines = 0usize;
    for (i, s) in seqs.iter().enumerate() {
        lines += s.len();
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
    assert!(lines > 3000, "{lines} lines");
}

#[test]
fn a_changed_answer_is_seen() {
    // the replay is not vacuous: one stored text changed in the golden makes it differ
    let seqs = sequences();
    let (s, k) = seqs
        .iter()
        .find_map(|s| s.iter().position(|l| l.contains(" store 0 '6")).map(|k| (s, k)))
        .expect("a stored text");
    let mut m = s.clone();
    m[k] = m[k].replacen(" store 0 '6", " store 0 '7", 1);
    assert!(replay(&m).is_some());
}

#[test]
fn units_and_characters() {
    // a wchar_t is the platform's: 2 bytes on Windows
    assert_eq!(WCHAR_FOR_TESTS, if cfg!(windows) { 2 } else { 4 });
    if cfg!(windows) {
        assert_eq!(wunit(-3), 0xfffd);
        assert_eq!(wunit(0x1263a), 0x263a);
        let w: [u16; 3] = [0x61, 0x263a, 0x20];
        // SAFETY: three units
        assert_eq!(unsafe { units_in(w.as_ptr() as isize, 3) }, vec![0x61, 0x263a, 0x20]);
    }
    // SAFETY: an empty text reads nothing
    assert!(unsafe { units_in(0, 5) }.is_empty());
}
