//! `UIEditBox`'s golden trace: a scratch driver ran the real `UIEditBox.cpp` (its pre-Rust text, with one patch: the
//! module comment's divergence) over fakes for everything outside the unit, 80 seeded boxes of 60 random operations
//! each, and printed every host call and every state change. The replay runs the same operations through [`Edit`]
//! with the same fakes. The constructor stays C++; [`Fake::new`] mirrors it. The same driver, relinked over the
//! forwarders (`UIEditBox.cpp` today) and `ee_rust.lib`, printed the same file.

use super::*;
use crate::text::SingleByte;

const GOLDEN: &str = include_str!("../golden_edit.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn esc(s: &[u8]) -> String {
    let mut o = String::from("\"");
    for &b in s.iter().take_while(|&&b| b != 0) {
        if !(0x20..0x7f).contains(&b) || b == b'"' || b == b'\\' {
            o += &format!("\\x{b:02x}");
        } else {
            o.push(b as char);
        }
    }
    o + "\""
}

/// A quoted golden text -> its bytes, terminated.
fn unesc(s: &str) -> Vec<u8> {
    let s = s.strip_prefix('"').unwrap().strip_suffix('"').unwrap().as_bytes();
    let mut v = Vec::new();
    let mut i = 0;
    while i < s.len() {
        if s[i] == b'\\' {
            v.push(u8::from_str_radix(core::str::from_utf8(&s[i + 2..i + 4]).unwrap(), 16).unwrap());
            i += 4;
        } else {
            v.push(s[i]);
            i += 1;
        }
    }
    v.push(0);
    v
}

/// The driver's fakes: widths 4 + byte % 5, font height 10, a scroll bar whose slider clamps to its range, and
/// `UIControl::SetRectangle`/`SetLocation` moving the rectangle (the virtual `SetLocation` is the box's own).
struct Fake {
    s: EditState,
    lines: Vec<Vec<u8>>,
    scroll: Option<(c_long, c_long, c_long)>,
    log: Vec<String>,
}

impl Fake {
    /// `UIEditBox::UIEditBox` with the driver's arguments.
    fn new(multi: bool, max_chars: c_ulong, numbers: bool, pwd: bool, w: f32, h_: f32, border: c_ulong) -> Fake {
        let rect = UiRect::new(10.0, 20.0, 10.0 + w, 20.0 + h_);
        let mut f = EditFields {
            char_scroll: 1,
            max_chars,
            is_single_line: !multi,
            is_password_box: !multi && pwd,
            numbers_only: numbers,
            ..Default::default()
        };
        let b = border as f32;
        let mut text = UiRect::new(rect.left + b, rect.top + b, rect.right - b, rect.bottom - b);
        if text.top > text.bottom {
            text.top = rect.height() * 0.5 + rect.top;
            text.bottom = text.top + 1.0;
        }
        if text.left > text.right {
            text.left = rect.width() * 0.5 + rect.left;
            text.right = text.left + 1.0;
        }
        f.visible_line_count = um_floor(text.height() / 10.0) as c_ulong;
        let mut log = Vec::new();
        let mut scroll = None;
        if multi {
            f.scroll_bar_visible = true;
            text.right = rect.right - SCROLL_BAR_WIDTH;
            log.push(format!("  scrollbar {} {} {} 1", h(text.right), h(text.top), h(text.height())));
            log.push("  srange 0 0".into());
            scroll = Some((0, 0, 0));
        }
        Fake { s: EditState { f, text, rect, border }, lines: vec![vec![0]], scroll, log }
    }

    fn dump(&self) -> String {
        let f = &self.s.f;
        let (sv, sm) = self.scroll.map(|(v, _, m)| (v, m)).unwrap_or((-1, -1));
        let (t, r) = (self.s.text, self.s.rect);
        format!(
            "state caret {} {} sel {} {} draw {} x {} sb {sv} {sm} vis {} cs {} ime {} {} {} {} flags {}{}{}{}{}{} \
             text {} {} {} {} rect {} {} {} {}",
            f.caret_line,
            f.caret_index,
            f.selection_start_line,
            f.selection_start_index,
            f.drawing_index,
            h(f.caret_x),
            f.visible_line_count,
            f.char_scroll,
            f.ime_active as u8,
            f.ime_start_line,
            f.ime_start_index,
            f.ime_composition_length,
            f.is_text_selected as u8,
            f.selecting as u8,
            f.cursor_on as u8,
            f.scroll_bar_visible as u8,
            f.is_single_line as u8,
            f.is_password_box as u8,
            h(t.left),
            h(t.top),
            h(t.right),
            h(t.bottom),
            h(r.left),
            h(r.top),
            h(r.right),
            h(r.bottom)
        )
    }

    fn run<R>(&mut self, op: impl FnOnce(&mut Edit<SingleByte, Fake>) -> R) -> R {
        let mut e = Edit::new(&SingleByte, self);
        let r = op(&mut e);
        e.finish();
        r
    }
}

impl EditHost for Fake {
    fn load(&self) -> EditState {
        self.s
    }
    fn store(&mut self, s: &EditState) {
        self.s.f = s.f;
        self.s.text = s.text;
    }
    fn line_count(&self) -> usize {
        self.lines.len()
    }
    fn line(&self, i: usize) -> Vec<u8> {
        self.lines[i].clone()
    }
    fn set_line(&mut self, i: usize, text: &[u8]) {
        self.lines[i] = text.to_vec();
    }
    fn insert_line(&mut self, i: usize, text: &[u8]) {
        self.lines.insert(i, text.to_vec());
    }
    fn erase_line(&mut self, i: usize) {
        self.lines.remove(i);
    }
    fn keep_first_line(&mut self) {
        self.lines.truncate(1);
    }
    fn text_width(&mut self, text: &[u8]) -> f32 {
        text.iter().take_while(|&&b| b != 0).map(|&b| 4.0 + (b % 5) as f32).sum()
    }
    fn font_height(&self) -> c_long {
        10
    }
    fn has_scroll_bar(&self) -> bool {
        self.scroll.is_some()
    }
    fn scroll_value(&self) -> c_long {
        self.scroll.unwrap().0
    }
    fn scroll_max(&self) -> c_long {
        self.scroll.unwrap().2
    }
    fn set_scroll_value(&mut self, value: c_ulong) {
        self.log.push(format!("  svalue {}", value as c_long));
        let (_, min, max) = self.scroll.unwrap();
        self.scroll = Some(((value as c_long).min(max).max(min), min, max));
    }
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong) {
        self.log.push(format!("  srange {min} {max}"));
        let (v, _, _) = self.scroll.unwrap();
        let (min, max) = (min as c_long, max as c_long);
        self.scroll = Some((v.min(max).max(min), min, max));
    }
    fn scroll_set_location(&mut self, at: UiPoint) {
        self.log.push(format!("  sbloc {} {}", h(at.x), h(at.y)));
    }
    fn set_dirty(&mut self) {
        self.log.push("  dirty 1".into());
    }
    fn set_rectangle(&mut self, r: &UiRect) {
        self.base_set_rectangle(r);
    }
    fn base_set_rectangle(&mut self, r: &UiRect) {
        self.log.push(format!("  setrect {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom)));
        self.s.rect = *r;
        let at = r.top_left();
        self.run(|e| e.set_location(at));
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.log.push(format!("  baseloc {} {}", h(at.x), h(at.y)));
        let r = self.s.rect;
        let (w, hh) = (r.right - r.left, r.bottom - r.top);
        self.s.rect = UiRect::new(at.x, at.y, at.x + w, at.y + hh);
    }
    fn fire(&mut self, event: c_ulong) {
        self.log.push(format!("  fire {event:x}"));
    }
}

#[test]
fn replays_the_cpp_golden() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut fake: Option<Fake> = None;
    let mut last_state = String::new();
    let mut last_lines: Vec<Vec<u8>> = Vec::new();
    let mut force = false;
    let mut i = 0;
    while i < golden.len() {
        let line = golden[i];
        i += 1;
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "new" {
            out.push(line.into());
            let mut f = Fake::new(
                t[1] == "1",
                t[2].parse().unwrap(),
                t[3] == "1",
                t[4] == "1",
                fx(t[5]),
                fx(t[6]),
                t[7].parse().unwrap(),
            );
            out.append(&mut f.log);
            fake = Some(f);
            force = true;
            while golden[i].starts_with("  ") {
                i += 1; // the constructor's host calls, regenerated above
            }
            if golden[i].starts_with("op charscroll") {
                let cs: c_ulong = golden[i].split(' ').nth(2).unwrap().parse().unwrap();
                fake.as_mut().unwrap().s.f.char_scroll = cs;
                out.push(golden[i].into());
                i += 1;
            }
        } else if t[0] == "op" {
            let f = fake.as_mut().unwrap();
            let arg = |k: usize| -> c_ulong { t[k].parse().unwrap() };
            match t[1] {
                "key" => {
                    out.push(line.into());
                    let key = unesc(&line[7..]);
                    f.run(|e| e.type_key(&key, 1));
                    f.s.f.cursor_on = true;
                }
                "add" => {
                    out.push(line.into());
                    let text = unesc(&line[7..]);
                    f.run(|e| e.add_char(&text));
                }
                "settext" => {
                    out.push(line.into());
                    let text = unesc(&line[11..]);
                    f.run(|e| e.set_text(&text));
                }
                "enter" => {
                    out.push(line.into());
                    f.run(|e| e.enter_key());
                }
                "back" | "del" => {
                    out.push(line.into());
                    let back = t[1] == "back";
                    f.run(|e| {
                        if e.s.f.is_text_selected {
                            e.delete_selected_text()
                        } else if back {
                            e.back_space_char()
                        } else {
                            e.delete_char()
                        }
                    });
                }
                "left" => {
                    out.push(line.into());
                    f.run(|e| e.decrement_caret_pos());
                }
                "right" => {
                    out.push(line.into());
                    let n = arg(2) as c_long;
                    f.run(|e| e.increment_caret_pos(n));
                }
                "home" | "end" | "up" | "down" | "selall" | "delsel" | "clear" | "check" => {
                    out.push(line.into());
                    f.run(|e| match t[1] {
                        "home" => e.home_key(),
                        "end" => e.end_key(),
                        "up" => e.up_key(),
                        "down" => e.down_key(),
                        "selall" => e.select_all(),
                        "delsel" => e.delete_selected_text(),
                        "clear" => e.clear(),
                        _ => e.check_scroll_bar(),
                    });
                }
                "select" => {
                    out.push(line.into());
                    let (a, b, c, d) = (arg(2), arg(3), arg(4), arg(5));
                    f.run(|e| e.select(a, b, c, d));
                }
                "caret" => {
                    out.push(line.into());
                    let (a, b) = (arg(2), arg(3));
                    f.run(|e| e.set_caret_location(a, b));
                }
                "width" => {
                    out.push(line.into());
                    let w = arg(2);
                    f.run(|e| e.change_width(w));
                }
                "loc" => {
                    out.push(line.into());
                    let at = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                    f.run(|e| e.set_location(at));
                }
                "scroll" => {
                    out.push(line.into());
                    f.set_scroll_value(arg(2));
                }
                "getsel" => {
                    let (sel, all) = f.run(|e| {
                        let last = e.h.line_count() as c_ulong - 1;
                        let len = char_len(&SingleByte, &e.h.line(last as usize)) as c_ulong;
                        (e.selected_text().unwrap_or_else(empty), e.text(0, last, 0, len))
                    });
                    out.push(format!("op getsel = {}", esc(&sel)));
                    out.push(format!("  all = {}", esc(&all)));
                }
                "mouse" => {
                    let at = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                    let (l, ix) = f.run(|e| e.caret_from_mouse(at));
                    out.push(format!("op mouse {} {} = {l} {ix}", t[2], t[3]));
                }
                "imeopen" | "imeclose" => {
                    let open = t[1] == "imeopen";
                    let r = f.run(|e| if open { e.ime_open() } else { e.ime_close() });
                    out.append(&mut f.log);
                    out.push(format!("op {} = {r}", t[1]));
                }
                "imechange" => {
                    out.push(line.into());
                    let q = line.rfind(' ').unwrap();
                    let text = unesc(&line[13..q]);
                    let cursor: c_ulong = line[q + 1..].parse().unwrap();
                    let r = f.run(|e| e.ime_change(&text, cursor));
                    out.append(&mut f.log);
                    out.push(format!("  = {r}"));
                }
                other => panic!("op {other}"),
            }
        } else {
            continue; // host calls and states: regenerated below
        }
        let f = fake.as_mut().unwrap();
        out.append(&mut f.log);
        let state = f.dump();
        if force || state != last_state {
            out.push(state.clone());
        }
        last_state = state;
        if force || f.lines != last_lines {
            for (n, l) in f.lines.iter().enumerate() {
                out.push(format!("  line {n} {} {}", char_len(&SingleByte, l), esc(l)));
            }
        }
        last_lines = f.lines.clone();
        force = false;
    }
    assert_eq!(out.len(), golden.len(), "line count");
    for (n, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", n + 1);
    }
}

fn single(max_chars: c_ulong, numbers: bool) -> Fake {
    Fake::new(false, max_chars, numbers, false, 100.0, 20.0, 1)
}

#[test]
fn a_numbers_only_box_drops_letters_but_counts_them() {
    let mut f = single(4, true);
    f.run(|e| e.add_char(b"1a2b3\0"));
    assert_eq!(f.lines, [b"12\0".to_vec()], "4 characters read, 2 of them digits");
    let mut f = single(3, false);
    let read = f.run(|e| e.add_char(b"abcdef\0"));
    assert_eq!((f.lines[0].as_slice(), read), (&b"abc\0"[..], 3));
}

#[test]
fn a_multi_line_box_wraps_at_the_last_space() {
    // text width 100 - 20 (scroll bar) - 10 = 70: about 12 characters of the fake's widths
    let mut f = Fake::new(true, 99, false, false, 100.0, 60.0, 1);
    f.run(|e| e.type_key(b"x\0", 1));
    f.run(|e| e.set_text(b"aaaa bbbb cccc dddd\0"));
    assert_eq!(f.lines, [b"aaaa bbbb \0".to_vec(), b"cccc dddd\0".to_vec()]);
    let text = f.run(|e| e.text(0, 1, 0, 9));
    assert_eq!(text, b"aaaa bbbb cccc dddd\0");
}

#[test]
fn a_caret_before_the_drawing_index_sits_at_the_left_edge() {
    let mut f = single(20, false);
    f.run(|e| e.set_text(b"abcdef\0"));
    f.s.f.drawing_index = 3;
    let at = f.run(|e| e.location_from_index(1, 0));
    assert_eq!(at.x, f.s.text.left, "the original padded 2^32 - 2 NULs here");
    let at = f.run(|e| e.location_from_index(4, 0));
    assert_eq!(at.x, f.s.text.left + 4.0 + (b'd' % 5) as f32);
}

#[test]
fn typing_repeats_and_selection_replaces() {
    let mut f = single(20, false);
    f.run(|e| e.type_key(b"z\0", 3));
    f.run(|e| e.type_key(b"y\0", 0));
    f.run(|e| e.type_key(b"y\0", -2));
    assert_eq!((f.lines[0].as_slice(), f.s.f.caret_index), (&b"zzz\0"[..], 3));
    f.run(|e| e.select(1, 2, 0, 0));
    assert!(f.s.f.is_text_selected);
    assert_eq!(f.run(|e| e.selected_text()), Some(b"z\0".to_vec()));
    f.run(|e| e.type_key(b"q\0", 1));
    assert_eq!(f.lines[0], b"zqz\0");
    assert!(!f.s.f.is_text_selected);
    assert_eq!(f.run(|e| e.selected_text()), None);
}
