//! The edit box's drawing against the real C++: the `edit` cases of `golden_draw.txt` (the driver of `draw.rs`'s
//! tests: random single- and multi-line boxes, password boxes, carets blinking and scrolled away, selections in
//! either direction across and beyond the copied lines, IME compositions with random clause attributes, UTF-8
//! text, the form's clipping). The replay rebuilds each case's whole log over this module and `draw.rs`, the
//! caret's place over `edit.rs`'s rule (the text before it measured), with the driver's fake measure.

use super::*;
use crate::draw::{draw_clipped_text, Clipping, TextDraw};

const GOLDEN: &str = include_str!("../golden_draw.txt");
const FONT_HEIGHTS: [c_ulong; 4] = [8, 12, 16, 23];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn hex_text(t: &str) -> Vec<u8> {
    if t == "-" {
        return Vec::new();
    }
    (0..t.len()).step_by(2).map(|i| u8::from_str_radix(&t[i..i + 2], 16).unwrap()).collect()
}
fn text_hex(t: &[u8]) -> String {
    if t.is_empty() {
        "-".into()
    } else {
        t.iter().map(|b| format!("{b:02x}")).collect()
    }
}
fn fnv(lines: &[String]) -> u64 {
    let mut x: u64 = 0xcbf2_9ce4_8422_2325;
    for l in lines {
        for b in l.bytes().chain(core::iter::once(b'\n')) {
            x ^= b as u64;
            x = x.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }
    x
}
fn width(text: &[u8], height: c_ulong) -> f32 {
    let s = core::str::from_utf8(text).unwrap();
    let mut w = 0.0f32;
    for c in s.chars() {
        w += (3u32.wrapping_add((c as u32).wrapping_mul(7) % 9)) as f32 * (height / 8 + 1) as f32;
    }
    w
}
fn chars(t: &[u8]) -> c_ulong {
    core::str::from_utf8(t).unwrap().chars().count() as c_ulong
}
/// `PlatMbsNInc`: past the first `n` characters.
fn tail(t: &[u8], n: c_ulong) -> Vec<u8> {
    let s = core::str::from_utf8(t).unwrap();
    let at = s.char_indices().nth(n as usize).map(|(i, _)| i).unwrap_or(s.len());
    t[at..].to_vec()
}
/// `CopyString(text, n)`: the first `n` characters.
fn prefix(t: &[u8], n: c_ulong) -> Vec<u8> {
    let s = core::str::from_utf8(t).unwrap();
    let at = s.char_indices().nth(n as usize).map(|(i, _)| i).unwrap_or(s.len());
    t[..at].to_vec()
}

struct Cursor<'a> {
    t: Vec<&'a str>,
    at: usize,
}
impl<'a> Cursor<'a> {
    fn next(&mut self) -> &'a str {
        self.at += 1;
        self.t[self.at - 1]
    }
    fn key(&mut self, k: &str) {
        assert_eq!(self.next(), k, "{:?}", self.t);
    }
    fn f(&mut self) -> f32 {
        fx(self.next())
    }
    fn u(&mut self) -> c_ulong {
        self.next().parse().unwrap()
    }
    fn i(&mut self) -> c_long {
        self.next().parse().unwrap()
    }
    fn rect(&mut self, k: &str) -> UiRect {
        self.key(k);
        let (l, t, r, b) = (self.f(), self.f(), self.f(), self.f());
        UiRect::new(l, t, r, b)
    }
    fn color(&mut self, k: &str) -> [f32; 3] {
        self.key(k);
        [self.f(), self.f(), self.f()]
    }
}
fn col(c: &[f32; 3]) -> String {
    format!("{} {} {}", h(c[0]), h(c[1]), h(c[2]))
}
fn rect_hex(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

struct Log {
    lines: Vec<String>,
    dr: c_long,
}
impl Log {
    fn line(&mut self, s: UiPoint, e: UiPoint, c: &[f32; 3], o: f32) -> c_long {
        self.lines.push(format!(
            "  line {} {} {} {} col {} op {} n 1 scr 0",
            h(s.x),
            h(s.y),
            h(e.x),
            h(e.y),
            col(c),
            h(o)
        ));
        self.dr
    }
    fn rhombus(&mut self, r: &UiRect, c: &[f32; 3], o: f32) -> c_long {
        let o = h(o);
        self.lines.push(format!("  rhombus {} col {} op {o} {o} {o} {o} scr 0", rect_hex(r), col(c)));
        self.dr
    }
    fn text(&mut self, text: &[u8], at: UiPoint, fg: &[f32; 3], r: UiRect, clipping: &Clipping) -> c_long {
        struct T<'a>(&'a mut Log, String);
        impl TextDraw for T<'_> {
            fn draw(&mut self, r: UiRect) -> c_long {
                self.0.lines.push(format!("{} rect {}", self.1, rect_hex(&r)));
                self.0.dr
            }
        }
        let prefix = format!("  text {} {} {} fg {}", text_hex(text), h(at.x), h(at.y), col(fg));
        draw_clipped_text(&mut T(self, prefix), r, clipping)
    }
}

/// The underlines over the copied lines with the fake measure.
struct Ime<'a> {
    log: &'a mut Log,
    shown: &'a [Vec<u8>],
    attributes: &'a [u8],
    height: c_ulong,
    color: [f32; 3],
    char: Vec<u8>,
}
impl ImeHost for Ime<'_> {
    fn display_len(&mut self, i: c_ulong) -> c_ulong {
        chars(&self.shown[i as usize])
    }
    fn prefix_width(&mut self, i: c_ulong, j: c_ulong) -> f32 {
        width(&prefix(&self.shown[i as usize], j), self.height)
    }
    fn char_bytes(&mut self, i: c_ulong, j: c_ulong) -> usize {
        self.char = prefix(&tail(&self.shown[i as usize], j), 1);
        self.char.len()
    }
    fn char_width(&mut self, _: c_ulong, _: c_ulong) -> f32 {
        width(&self.char, self.height)
    }
    fn attribute(&mut self, offset: i32) -> u8 {
        if offset >= 0 && (offset as usize) < self.attributes.len() {
            self.attributes[offset as usize]
        } else {
            0
        }
    }
    fn draw_line(&mut self, start: UiPoint, end: UiPoint) -> c_long {
        let c = self.color;
        self.log.line(start, end, &c, 1.0)
    }
}

fn replay(c: &mut Cursor, log: &mut Log) {
    // the control's base
    let rect = c.rect("rect");
    c.key("border");
    let (thickness, border_type) = (c.u(), c.u());
    c.key("bop");
    let bop = c.f();
    c.key("bg");
    let bg = c.u() != 0;
    let bgc = c.color("bgc");
    c.key("bgo");
    let bgo = c.f();
    c.key("single");
    let single = c.u() != 0;
    c.key("pass");
    let pass = c.u() != 0;
    c.key("font");
    let font = c.u() as usize;
    let tr = c.rect("tr");
    let fg = c.color("fg");
    let cur = c.color("cur");
    let selc = c.color("selc");
    let imec = c.color("imec");
    c.key("poll");
    let (running, fires) = (c.u() != 0, c.u());
    c.key("on");
    let mut on = c.u() != 0;
    c.key("vis");
    let visible = c.u();
    c.key("sb");
    let value = c.i();
    c.key("sbv");
    let sbv = c.u() != 0;
    let thumb = if single { None } else { Some(c.rect("thumb")) };
    c.key("caret");
    let (caret_line, caret_index) = (c.u(), c.i());
    c.key("draw");
    let drawing = c.i();
    c.key("sel");
    let (selected, anchor_line, anchor_index) = (c.u() != 0, c.u(), c.i());
    c.key("ime");
    let ime = c.u() != 0;
    let mut ime_in = (0, 0, 0, Vec::new());
    if ime {
        ime_in = (c.u(), c.u(), c.u(), c.next().bytes().map(|b| b - b'0').collect());
    }
    c.key("lines");
    let n = c.u();
    let lines: Vec<Vec<u8>> = (0..n).map(|_| hex_text(c.next())).collect();
    c.key("clip");
    let mode = c.u();
    let mut clips = Vec::new();
    let mut popup = (UiRect::default(), UiRect::default());
    let mut candidate = UiRect::default();
    match mode {
        1 | 2 => {
            for _ in 0..c.u() {
                clips.push(c.rect("c"));
            }
        }
        3 => popup = (c.rect("pr"), c.rect("pd")),
        5 => candidate = c.rect("ime"),
        _ => {}
    }
    c.key("dr");
    log.dr = c.i();
    let clipping = match mode {
        1 | 2 => Clipping::Rectangles(clips),
        3 => Clipping::Popup { is_control: false, rect: popup.0, draw_rect: popup.1 },
        4 => Clipping::Popup { is_control: true, rect, draw_rect: rect },
        5 => Clipping::Plain { candidate: Some(candidate) },
        _ => Clipping::Plain { candidate: None },
    };
    let height = FONT_HEIGHTS[font];

    // CopyData: the caret
    if running && fires > 0 {
        on = !on;
    }
    let mut cursor = None;
    // the box's measuring text holds the last text measured (edit.rs measures through it)
    let mut measured: Vec<u8> = Vec::new();
    if on && !(!single && (caret_line < value as c_ulong || caret_line >= value as c_ulong + visible)) {
        // edit.rs's location_from_index: the caret line's text from the drawing index, measured to the caret
        let line = &lines[caret_line as usize];
        let base = if pass { vec![b'*'; chars(line) as usize] } else { line.clone() };
        let text = tail(&base, drawing as c_ulong);
        let count = if caret_index < drawing { 0 } else { (caret_index - drawing) as c_ulong };
        measured = prefix(&text, count);
        let mut at = UiPoint { x: width(&measured, height) + tr.left, y: tr.top };
        if !single {
            at.y = (caret_line.wrapping_sub(value as c_ulong).wrapping_mul(height)) as f32 + tr.top;
        }
        cursor = Some(at);
    }
    // the copied lines and the composition
    let shown: Vec<Vec<u8>>;
    let (first, last): (c_ulong, c_ulong);
    let measure: (Vec<u8>, UiPoint);
    let copied_ime: EditIme;
    if single {
        let line = &lines[caret_line as usize];
        let base = if pass { vec![b'*'; chars(line) as usize] } else { line.clone() };
        let s = tail(&base, drawing as c_ulong);
        shown = vec![s.clone()];
        measure = (s, tr.top_left());
        copied_ime = if ime { copied_ime_single(ime_in.1, ime_in.2, drawing) } else { EditIme::default() };
        first = 0;
        last = 1;
    } else {
        first = value as c_ulong;
        last = first + visible;
        shown = (first..last)
            .map(|i| match lines.get(i as usize) {
                Some(l) if l.contains(&b'\n') => prefix(l, chars(l) - 1),
                Some(l) => l.clone(),
                None => Vec::new(),
            })
            .collect();
        measure = (measured, UiPoint::default());
        copied_ime = if ime {
            let after = chars(&tail(&lines[ime_in.0 as usize], ime_in.1)) as c_long;
            copied_ime_lines(ime_in.0, ime_in.1, ime_in.2, first, last, after, |i| chars(&lines[i as usize]))
        } else {
            EditIme::default()
        };
    }
    let sel = if selected {
        copied_selection(caret_line, caret_index, anchor_line, anchor_index, drawing, first, last, |i| {
            chars(&shown[i as usize])
        })
    } else {
        EditSelection::default()
    };
    log.lines.push(format!(
        "  copy 0 cursor {} sel {} {} {} {} {} ime {} {} {} {} {}",
        cursor.is_some() as u8,
        sel.selected as u8,
        sel.start_line,
        sel.start_index,
        sel.end_line,
        sel.end_index,
        copied_ime.active as u8,
        copied_ime.start_line,
        copied_ime.start_index,
        copied_ime.end_line,
        copied_ime.end_index
    ));
    for s in &shown {
        log.lines.push(format!("  shown {}", text_hex(s)));
    }
    log.lines.push(format!("  measure {} at {} {}", text_hex(&measure.0), h(measure.1.x), h(measure.1.y)));

    // DrawPolygons
    if bg {
        log.rhombus(&rect, &bgc, bgo);
    }
    let mut p = 0;
    for side in 0..4 {
        for i in 0..thickness {
            let l = crate::draw::border_line(&rect, side, i, border_type);
            p = log.line(l.start, l.end, if l.light { &[1.0, 1.0, 1.0] } else { &[0.0, 0.0, 0.0] }, bop);
        }
    }
    if sbv {
        let t = thumb.unwrap();
        log.lines.push(format!("  bitmap 100 {} {} scale {} {} scr 0", h(t.left), h(t.top), h(1.0), h(1.0)));
        p = 0;
    }
    if sel.selected {
        let mut i = sel.start_line;
        while i <= sel.end_line {
            let line = shown.get(i as usize).cloned().unwrap_or_default();
            let (from, to) = selection_span(&sel, i, chars(&line));
            let before = width(&prefix(&line, from), height);
            let span = width(&prefix(&tail(&line, from), to.wrapping_sub(from)), height);
            let r = selection_rect(i, height as c_long, &tr, before, span);
            p = log.rhombus(&r, &selc, 1.0);
            i += 1;
        }
    }
    if copied_ime.active {
        let mut host = Ime { log, shown: &shown, attributes: &ime_in.3, height, color: imec, char: Vec::new() };
        p = ime_underlines(&mut host, &copied_ime, &tr, height, p);
    }
    if let Some(at) = cursor {
        p = log.line(at, UiPoint { x: at.x, y: at.y + height as f32 }, &cur, 1.0);
    }
    log.lines.push(format!("  polygons {p}"));

    // DrawTextA
    let mut res = 0;
    for (i, line) in shown.iter().enumerate() {
        let i = i as c_ulong;
        let y = text_row_y(i, height as c_long, tr.top);
        let mut at = UiPoint { x: tr.left, y };
        res = log.text(line, at, &fg, tr, &clipping);
        let m = &copied_ime;
        if !(m.active && i >= m.start_line && i <= m.end_line) {
            continue;
        }
        if m.start_line == i {
            at.x = width(&prefix(line, m.start_index), height) + tr.left;
        }
        let mut s = line.clone();
        if m.end_line == i {
            s = prefix(&s, m.end_index);
        }
        let t = if m.start_line == i { tail(&s, m.start_index) } else { s };
        res = log.text(&t, at, &imec, tr, &clipping);
    }
    if sbv {
        res = 0;
    }
    log.lines.push(format!("  result {res}"));
}

#[test]
fn golden_edit_draws_replay() {
    let mut cases = 0;
    for line in GOLDEN.lines() {
        let (head, tail) = line.split_once(" = ").unwrap();
        let t: Vec<&str> = head.split(' ').collect();
        if t[2] != "edit" {
            continue;
        }
        cases += 1;
        let (want_lines, want_digest) = tail.split_once(' ').unwrap();
        let mut c = Cursor { t, at: 3 };
        let mut log = Log { lines: Vec::new(), dr: 0 };
        replay(&mut c, &mut log);
        assert_eq!(c.at, c.t.len(), "header not consumed: {line}");
        assert_eq!(
            (log.lines.len().to_string(), format!("{:016x}", fnv(&log.lines))),
            (want_lines.to_string(), want_digest.to_string()),
            "{line}\n{}",
            log.lines.join("\n")
        );
    }
    assert_eq!(cases, 400);
}

#[test]
fn selection_ordering_and_clipping() {
    // caret before the anchor on one line: the indices swap; a start above the window starts the copy
    let s = copied_selection(2, 1, 2, 5, 0, 0, 4, |_| 9);
    assert_eq!((s.start_index, s.end_index, s.selected), (1, 5, true));
    let s = copied_selection(0, 3, 6, 2, 0, 2, 4, |_| 7);
    assert_eq!((s.start_line, s.start_index, s.end_line, s.end_index, s.selected), (0, 0, 1, 7, true));
    // a selection wholly below the window is hidden
    assert!(!copied_selection(5, 0, 6, 1, 0, 0, 3, |_| 1).selected);
    // single-line indices are relative to the drawing index
    let s = copied_selection(0, 9, 0, 2, 4, 0, 1, |_| 5);
    assert_eq!((s.start_index, s.end_index), (0, 5));
}

#[test]
fn composition_end_walks_the_following_lines() {
    // 3 characters left on the start line, 7 composed: 4 more over lines of 2 and 5
    let m = copied_ime_lines(1, 2, 7, 0, 4, 3, |i| [9, 5, 2, 5][i as usize]);
    assert_eq!((m.start_line, m.end_line, m.end_index, m.active), (1, 3, 2, true));
    let m = copied_ime_single(3, 4, 5);
    assert_eq!((m.start_index, m.end_index), (0, 4));
}
