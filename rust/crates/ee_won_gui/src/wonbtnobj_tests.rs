//! The Rust WONButton against the original's (`golden_wonbtnobj.txt`, printed by
//! `tests/won/won_wonbtnobj_test.cpp --print` over the original statements and a logged world): each sequence is
//! replayed op for op on two Rust buttons, the replay host answering every traced op from the golden (the draws and
//! their slots compared, the size change's call back); the references' holds and releases are not traced (the
//! golden compares their counts after each call).

use std::cell::RefCell;
use std::ffi::c_void;

use super::{EeWonWbCall as C, EeWonWbField as B, EeWonWonButton};
use crate::guidraw::EeWonDrawOp;
use crate::guihost::{EeWonGuiField as F, EeWonGuiOp as Op, EeWonWbFont as Wf, EeWonWbImage as Wi, EeWonWbText as Wt};

const GOLDEN: &str = include_str!("golden_wonbtnobj.txt");
const COMP0: usize = 0x53;
const COMP1: usize = 0x54;

struct Replay {
    lines: Vec<String>,
    at: usize,
    width: [i32; 2],
    height: [i32; 2],
    failure: Option<String>,
    btn: [*const EeWonWonButton; 2],
}

thread_local! {
    static REPLAY: RefCell<Option<Replay>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Replay) -> R) -> R {
    REPLAY.with(|r| f(r.borrow_mut().as_mut().expect("a replay")))
}

/// the golden's next op line, which must start with `want`; the words after it
fn expect(want: &str) -> Vec<String> {
    let line = with(|r| {
        let l = r.lines.get(r.at).cloned().unwrap_or_default();
        r.at += 1;
        l
    });
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

unsafe extern "C" fn replay_host(comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let side = usize::from(comp as usize == COMP1);
    let n = op as u32;
    match op {
        Op::Get => with(|r| {
            if a == F::Width as isize {
                r.width[side]
            } else if a == F::Height as isize {
                r.height[side]
            } else {
                0
            }
        }) as isize,
        Op::WbText if a == Wt::Hold as isize || a == Wt::Release as isize => 0,
        Op::WbImage if a == Wi::Hold as isize || a == Wi::Release as isize => 0,
        Op::WbFont if a == Wf::Hold as isize || a == Wf::Release as isize => 0,
        // a NULL text's method (SetFont over an empty normal state): the world logs it and does nothing
        Op::WbText if b == 0 && a != Wt::New as isize => {
            expect(&format!("{n} NULL text {a}"));
            0
        }
        Op::SetSize => {
            let w = expect(&format!("{n} set-size {side} {a} {b} -> "));
            with(|r| {
                r.width[side] = a as i32;
                r.height[side] = b as i32;
            });
            if w.first().map(String::as_str) == Some("1") {
                // SAFETY: the buttons are live while the replay runs (Component::SetSize calls Virtual08 back)
                unsafe { (*with(|r| r.btn[side])).call(C::SizeChanged, 0, 0, 0, 0) };
            }
            0
        }
        Op::Draw => {
            // SAFETY: the object passes a live operation
            let o = unsafe { *(b as *const EeWonDrawOp) };
            expect(&format!(
                "{n} draw {side} {a:x} {} {} {} {} {} {} {} {} {} img {c} txt {d}",
                o.kind, o.a, o.b, o.c, o.d, o.e, o.f, o.g, o.h
            ));
            0
        }
        _ => {
            let w = expect(&format!("{n} {side} {a} {b} {c} {d} -> "));
            w.first().and_then(|x| x.parse().ok()).unwrap_or(0)
        }
    }
}

fn replay(lines: &[String]) -> Option<String> {
    // the reference and use-after-free lines are the C++ golden's checks: the replay skips them
    let lines: Vec<String> =
        lines.iter().filter(|l| !l.starts_with("refs") && !l.starts_with("DEAD ")).cloned().collect();
    let start: Vec<i32> = lines[0].split_whitespace().skip(1).filter_map(|w| w.parse().ok()).collect();
    REPLAY.with(|r| {
        *r.borrow_mut() = Some(Replay {
            lines: lines.clone(),
            at: 1,
            width: [start[0], start[2]],
            height: [start[1], start[3]],
            failure: None,
            btn: [std::ptr::null(); 2],
        })
    });
    let b0 = Box::new(EeWonWonButton::new(COMP0 as *mut c_void, Some(replay_host)));
    let b1 = Box::new(EeWonWonButton::new(COMP1 as *mut c_void, Some(replay_host)));
    let btns: [&EeWonWonButton; 2] = [&b0, &b1];
    with(|r| r.btn = [btns[0] as *const _, btns[1] as *const _]);
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
        let num = |i: usize| -> isize { words.get(i).and_then(|w| w.parse().ok()).unwrap_or(0) };
        let side = num(1) as usize & 1;
        let (a, b, c, d, e) = (num(2), num(3), num(4), num(5), num(6));
        let t = btns[side];
        let got = match words[0] {
            "set-state" => {
                t.set_button_state(a as i32, b, c as i32, d, e != 0);
                0
            }
            "text" => t.call(C::SetTextHook, a, 0, 0, 0),
            "size" => {
                with(|r| {
                    r.width[side] = a as i32;
                    r.height[side] = b as i32;
                });
                t.call(C::SizeChanged, 0, 0, 0, 0)
            }
            "adjust" => t.call(C::SetAdjustHeightToFitText, a, 0, 0, 0),
            "adjust-now" => t.call(C::AdjustHeightToFitText, 0, 0, 0, 0),
            "wrap" => t.call(C::SetWrapText, a, 0, 0, 0),
            "transparent" => t.call(C::SetTransparent, a, 0, 0, 0),
            "text-size" => {
                let (w, h) = t.text_size();
                (w * 1000 + h) as isize
            }
            "desired" => {
                let (w, h) = t.desired_size();
                (w * 1000 + h) as isize
            }
            "horz" => t.call(C::SetHorzAlign, a, 0, 0, 0),
            "color" => t.call(C::SetTextColor, a, 0, 0, 0),
            "font" => t.call(C::SetFont, a, 0, 0, 0),
            "copy" => {
                btns[0].copy_attributes(btns[1], COMP1 as isize);
                0
            }
            "state" => {
                let (col, tx, im) = t.button_state();
                tx * 100000 + im * 10 + (col & 7) as isize
            }
            "draw-text" => {
                let (col, tx, _) = t.button_state();
                t.draw_button_text(6, tx, col);
                0
            }
            "paint" => t.call(C::Paint, 6, 0, 0, 0),
            "get" => t.call(C::Get, a, 0, 0, 0),
            "set" => t.call(C::Set, a, b, 0, 0),
            "get-text" => t.call(C::GetText, 0, 0, 0, 0),
            "world" => 0,
            name => return Some(format!("unknown call '{name}'")),
        };
        if let Some(w) = want {
            if w != got {
                return Some(format!("{line}: rust answered {got}"));
            }
        }
    }
    drop(b0);
    drop(b1);
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
fn golden_wonbtnobj_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40, "the golden holds {} sequences", seqs.len());
    for (i, s) in seqs.iter().enumerate() {
        if let Some(f) = replay(s) {
            panic!("sequence {i}: {f}");
        }
    }
}

#[test]
fn a_changed_text_slot_is_seen() {
    let seqs = sequences();
    let (s, k) = seqs
        .iter()
        .find_map(|s| s.iter().position(|l| l.contains(" draw ") && !l.ends_with(" txt 0")).map(|k| (s, k)))
        .expect("a draw with a text");
    let mut m = s.clone();
    m[k] = m[k].replacen(" txt ", " txt 9", 1);
    assert!(replay(&m).is_some());
}

#[test]
fn defaults_and_fields() {
    let t = EeWonWonButton::new(std::ptr::null_mut(), None);
    let get = |f: B| t.call(C::Get, f as isize, 0, 0, 0);
    assert_eq!(get(B::LeftTextPad), 8);
    assert_eq!(get(B::TopTextPad), 4);
    assert_eq!(get(B::TopFocusPad), 3);
    assert_eq!(get(B::DefaultTextColor), 0x80000005_u32 as i32 as isize);
    assert_eq!(get(B::ScaleImage), 1);
    assert_eq!(get(B::HAlign), 1);
    t.call(C::Set, B::VAlign as isize, 258, 0, 0);
    assert_eq!(get(B::VAlign), 2);
    // with scaled images only the normal and default states take the colour
    t.call(C::SetTextColor, 7, 0, 0, 0);
    {
        let s = t.st.borrow();
        assert_eq!((s.cells[0][0].color, s.cells[1][0].color, s.cells[4][1].color), (7, -1, 7));
    }
    assert_eq!(t.call(C::GetText, 0, 0, 0, 0), 0);
    assert_eq!(t.call(C::Get, 99, 0, 0, 0), 0);
}
