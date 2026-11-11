//! The label's geometry against the real C++ (`golden_label_fit.txt`): a scratch driver (`build/ui_label_golden`)
//! ran the unmodified `UITextLabel.cpp` (with the real `UIControl.cpp` and `UIForm.cpp`; a fake scroll bar whose
//! constructor, slider range and virtuals log, the real one needing the manager's bitmaps) over 150 labels (one-line
//! and multi-line, fit-to-text or not, each alignment, three font heights, borders) of 30 random operations: new
//! lines and appended ones with `CheckScrollBar`, `DeleteLine`, `DeleteArray`, `SetFitRectToText`, `SetLocation`,
//! `SetRectangle`, a scroll position or an alignment then `ComputeTextLabelRect`, a removed scroll bar's deletion
//! (`CopyData`'s) and a new visible row count with `CheckScrollBar`, printing the scroll bar's calls and every
//! rectangle after each. The replay below runs the same operations, this module for the moved functions and the
//! C++'s own steps (the control's move, `SetRectangle`'s flags, the fakes) around them; the driver relinked over
//! the forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;
use crate::rect::UiPoint;

const GOLDEN: &str = include_str!("../golden_label_fit.txt");
const FONT_HEIGHTS: [c_long; 3] = [8, 12, 23];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

fn rect_at(t: &[&str], at: usize) -> UiRect {
    UiRect::new(fx(t[at]), fx(t[at + 1]), fx(t[at + 2]), fx(t[at + 3]))
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

/// The driver's text measure (`fakes.cpp`): a hash advance per code point times the font's height / 8 + 1.
fn width_of(text: &[u8], height: c_long) -> f32 {
    let mut w = 0.0f32;
    let mut at = 0;
    while at < text.len() {
        let (c, l) = ee_plat_core::utf8::decode(&text[at..]);
        w += (3 + c.wrapping_mul(7) % 9) as f32 * (height / 8 + 1) as f32;
        at += l.max(1);
    }
    w
}

#[derive(Clone, Copy)]
struct Bar {
    min: c_long,
    max: c_long,
    page: c_ulong,
    value: c_long,
    rect: UiRect,
}

struct L {
    label_type: c_ulong,
    font: c_long,
    border: c_ulong,
    fit: bool,
    align: c_ulong,
    original: UiRect,
    rect: UiRect,
    text: UiRect,
    sbr: UiRect,
    bg: UiRect,
    lines: Vec<Vec<u8>>,
    displayed: c_ulong,
    removed: bool,
    pending: bool,
    dirty: bool,
    bar: Option<Bar>,
    log: Vec<String>,
}

fn bg_of(r: UiRect) -> UiRect {
    UiRect::new(r.left - 0.5, r.top - 0.5, r.right + 0.5, r.bottom + 0.5)
}

impl L {
    /// `UITextLabel::SetLocation`: `UIControl::SetLocation`'s move, background and dirty flag, then this module's
    /// rectangles and the scroll bar's move.
    fn set_location(&mut self, p: UiPoint) {
        let dx = p.x - self.rect.left;
        let dy = p.y - self.rect.top;
        let r = self.rect;
        self.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
        self.bg = bg_of(self.rect);
        self.dirty = true;
        let (s, t) = location_rects(self.rect, self.border, self.bar.is_some());
        self.sbr = s;
        self.text = t;
        if let Some(b) = &mut self.bar {
            self.log.push(format!("  sb loc {} {}", h(s.left), h(s.top)));
            let height = b.rect.bottom - b.rect.top;
            b.rect = UiRect::new(s.left, s.top, s.left + 19.0, s.top + height);
        }
    }
    /// `UITextLabel::SetRectangle`: `UIControl::SetRectangle` (the rectangle, the virtual `SetLocation`, the
    /// background), then the flags.
    fn set_rect(&mut self, r: UiRect) {
        self.rect = r;
        self.set_location(r.top_left());
        self.bg = bg_of(self.rect);
        if !self.fit {
            self.removed = true;
            self.pending = true;
        } else {
            self.pending = false;
        }
    }
    fn dump(&mut self) -> String {
        let mut s = format!(
            "  state lines {} shown {} fit {} align {} removed {} pending {} dirty {} rect {} text {} sbr {} bg {}",
            self.lines.len(),
            self.displayed,
            self.fit as u8,
            self.align,
            self.removed as u8,
            self.pending as u8,
            self.dirty as u8,
            rect_text(&self.rect),
            rect_text(&self.text),
            rect_text(&self.sbr),
            rect_text(&self.bg)
        );
        if let Some(b) = &self.bar {
            s.push_str(&format!(" sb {} {} {} {} sbrect {}", b.min, b.max, b.page, b.value, rect_text(&b.rect)));
        }
        self.dirty = false;
        s
    }
}

impl LabelHost for L {
    fn fit(&self) -> bool {
        self.fit
    }
    fn label_type(&self) -> c_ulong {
        self.label_type
    }
    fn alignment(&self) -> c_ulong {
        self.align
    }
    fn original(&self) -> UiRect {
        self.original
    }
    fn rect(&self) -> UiRect {
        self.rect
    }
    fn lines(&self) -> usize {
        self.lines.len()
    }
    fn displayed(&self) -> c_ulong {
        self.displayed
    }
    fn measure(&mut self, i: usize) -> (f32, f32) {
        (width_of(&self.lines[i], self.font), self.font as f32)
    }
    fn has_scroll_bar(&self) -> bool {
        self.bar.is_some()
    }
    fn removed(&self) -> bool {
        self.removed
    }
    fn set_removed(&mut self, removed: bool) {
        self.removed = removed;
    }
    fn scroll_value(&mut self) -> c_long {
        self.bar.unwrap().value
    }
    fn set_rectangle(&mut self, r: UiRect) {
        self.set_rect(r);
    }
    fn set_dirty(&mut self) {
        self.dirty = true;
    }
    fn set_bg_rectangle(&mut self, r: UiRect) {
        self.bg = bg_of(r);
    }
    fn set_range(&mut self, max: c_ulong) {
        self.log.push(format!("  sb range 0 {max}"));
        let b = self.bar.as_mut().unwrap();
        b.min = 0;
        b.max = max as c_long;
        if b.value > b.max {
            b.value = b.max;
        }
    }
    fn create_scroll_bar(&mut self, height: f32) {
        let s = self.sbr;
        self.log.push(format!(
            "  sb new id 0 at {} {} len {} style 1 index 3 border {}",
            h(s.left),
            h(s.top),
            h(height),
            self.border
        ));
        let rect = UiRect::new(s.left, s.top, s.left + 19.0, s.top + height - 1.0);
        self.bar = Some(Bar { min: 0, max: 0, page: 0, value: 0, rect });
    }
    fn set_page_size(&mut self, page: c_ulong) {
        self.bar.as_mut().unwrap().page = page;
    }
    fn set_scroll_bar_colors(&mut self) {
        self.log.push("  sb color 3e800000 3f000000 3f400000".to_string());
        self.log.push("  sb opacity 3f000000".to_string());
    }
    fn scroll_bar_rect(&self) -> UiRect {
        self.sbr
    }
    fn scroll_bar_control_rect(&self) -> UiRect {
        self.bar.unwrap().rect
    }
}

#[test]
fn replays_the_cpp_golden() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut l: Option<L> = None;
    let mut labels = 0;
    let mut at = 0;
    while at < golden.len() {
        let line = golden[at];
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "label" {
            // the label's inputs, then its first state line (the constructor's rectangles, as the driver set them)
            let st: Vec<&str> = golden[at + 1].split(' ').collect();
            assert_eq!(st[2], "state");
            let mut n = L {
                label_type: t[3].parse().unwrap(),
                font: FONT_HEIGHTS[t[5].parse::<usize>().unwrap()],
                border: t[7].parse().unwrap(),
                fit: t[9] == "1",
                align: t[11].parse().unwrap(),
                original: rect_at(&t, 13),
                rect: rect_at(&st, 18),
                text: rect_at(&st, 23),
                sbr: rect_at(&st, 28),
                bg: rect_at(&st, 33),
                lines: Vec::new(),
                displayed: st[6].parse().unwrap(),
                removed: false,
                pending: false,
                dirty: false,
                bar: None,
                log: Vec::new(),
            };
            out.push(line.to_string());
            out.push(n.dump());
            l = Some(n);
            labels += 1;
            at += 2;
            continue;
        }
        let l = l.as_mut().unwrap();
        out.push(line.to_string());
        match t[1] {
            "lines" => {
                let k: usize = t[2].parse().unwrap();
                l.lines = t[3..3 + k].iter().map(|s| unhex(s)).collect();
                check_scroll_bar(l);
            }
            "append" => {
                l.lines.push(unhex(t[2]));
                check_scroll_bar(l);
            }
            "deleteline" => {
                l.lines.remove(t[2].parse().unwrap());
                check_scroll_bar(l);
            }
            "deletearray" => {
                l.lines.clear();
                l.dirty = true;
                check_scroll_bar(l);
            }
            "fit" => {
                l.fit = t[2] == "1";
                compute_rect(l);
            }
            "loc" => l.set_location(UiPoint { x: fx(t[2]), y: fx(t[3]) }),
            "rect" => l.set_rect(rect_at(&t, 2)),
            "scroll" => {
                l.bar.as_mut().unwrap().value = t[2].parse().unwrap();
                compute_rect(l);
            }
            "compute" => compute_rect(l),
            "align" => {
                l.align = t[2].parse().unwrap();
                compute_rect(l);
            }
            "drop" => {
                if l.removed {
                    l.removed = false;
                    l.bar = None;
                }
            }
            "shown" => {
                l.displayed = t[2].parse().unwrap();
                check_scroll_bar(l);
            }
            other => panic!("op {other}"),
        }
        out.append(&mut l.log);
        out.push(l.dump());
        at += 1;
        while at < golden.len() && golden[at].starts_with("  ") && !golden[at].starts_with("  state") {
            at += 1;
        }
        at += 1;
    }
    for (n, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", n + 1);
    }
    assert_eq!(out.len(), golden.len(), "line count");
    assert_eq!(labels, 150);
}

#[test]
fn the_location_rects_leave_room_for_the_scroll_bar() {
    let r = UiRect::new(10.0, 20.0, 110.0, 70.0);
    let (s, t) = location_rects(r, 2, false);
    assert_eq!(s, UiRect::new(91.0, 20.0, 110.0, 70.0));
    assert_eq!(t, UiRect::new(13.0, 23.0, 107.0, 67.0));
    let (_, t) = location_rects(r, 2, true);
    assert_eq!(t.right, 107.0 - 19.0 - 1.0);
}
