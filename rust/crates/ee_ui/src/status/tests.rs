//! `UIStatusBar` against the real C++ (`golden_status.txt`): a scratch driver (`build/ui_status_golden`) ran the
//! unmodified `UIStatusBar.cpp` (with the real `UIControl.cpp` and `GERhombus.cpp`) over 100 bars of 25 random
//! operations: `SetRange` (reversed and empty ranges, negative values), `SetCurrentRangeValue` inside and outside the
//! range, `SetLocation`, `SetText` (UTF-8) and `CopyData`, printing the value, the dirty flag and every rectangle
//! after each. The replay below is the same bar over this module; the driver relinked over the forwarders prints the
//! same file, and the same on three more seeds at ten times the size.

use super::*;
use crate::rect::um_floor;

const GOLDEN: &str = include_str!("../golden_status.txt");
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

/// The driver's text measure (`fakes.cpp`): a hash advance per code point times the font's height / 8 + 1; the
/// height is the font's.
fn extent(text: &[u8], height: c_long) -> (f32, f32) {
    let mut w = 0.0f32;
    let mut at = 0;
    while at < text.len() {
        let (c, l) = ee_plat_core::utf8::decode(&text[at..]);
        w += (3 + c.wrapping_mul(7) % 9) as f32 * (height / 8 + 1) as f32;
        at += l.max(1);
    }
    (w, height as f32)
}

struct Bar {
    rect: UiRect,
    border: c_ulong,
    fill: UiRect,
    text: UiRect,
    value: c_long,
    min: c_long,
    max: c_long,
    dirty: bool,
    font: Option<c_long>,
    label: Vec<u8>,
}

impl Bar {
    /// `SetCurrentRangeValue`.
    fn set_value(&mut self, v: c_long) {
        let width = um_floor(self.rect.width() + 1.0) as c_ulong;
        let (v, f) = fill(v, self.min, self.max, width, self.fill);
        self.fill = f;
        if self.value != v {
            self.value = v;
            self.dirty = true;
        }
    }
    fn state(&self) -> String {
        format!(
            "  state value {} min {} max {} dirty {} rect {} fill {} text {}",
            self.value,
            self.min,
            self.max,
            self.dirty as u8,
            rect_text(&self.rect),
            rect_text(&self.fill),
            rect_text(&self.text)
        )
    }
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut bar: Option<Bar> = None;
    let mut bars = 0;
    for line in &lines {
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "bar" {
            bars += 1;
            let rect = UiRect::new(fx(t[3]), fx(t[4]), fx(t[5]), fx(t[6]));
            let border: c_ulong = t[8].parse().unwrap();
            // the constructor's
            let fill = UiRect::new(rect.left - 0.5, rect.top - 0.5, rect.right + 0.5, rect.bottom + 0.5);
            let b = Bar {
                rect,
                border,
                fill,
                text: inset(rect, border),
                value: 0,
                min: 0,
                max: 0,
                dirty: false,
                font: None,
                label: Vec::new(),
            };
            out.push(line.to_string());
            out.push(b.state());
            bar = Some(b);
            continue;
        }
        if t[0] != "op" {
            continue;
        }
        let b = bar.as_mut().unwrap();
        out.push(line.to_string());
        match t[1] {
            "range" => {
                // SetRange(unsigned long, unsigned long) into the long members, then the minimum as the value
                b.min = t[2].parse().unwrap();
                b.max = t[3].parse().unwrap();
                b.set_value(b.min);
            }
            "value" => b.set_value(t[2].parse().unwrap()),
            "loc" => {
                // UIControl::SetLocation: moved by the delta, then dirty
                let (x, y) = (fx(t[2]), fx(t[3]));
                let r = b.rect;
                let (dx, dy) = (x - r.left, y - r.top);
                b.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
                b.dirty = true;
                let (f, text) = location(b.rect, b.border);
                b.fill = f;
                b.set_value(b.value);
                b.text = text;
            }
            "text" => {
                let f: i64 = t[4].parse().unwrap();
                if b.font.is_none() {
                    b.font = Some(FONT_HEIGHTS[f as usize]);
                }
                b.label = if t[2] == "-" {
                    Vec::new()
                } else {
                    (0..t[2].len()).step_by(2).map(|i| u8::from_str_radix(&t[2][i..i + 2], 16).unwrap()).collect()
                };
                b.dirty = true;
            }
            "copy" => {
                // UIControl::CopyData clears the dirty flag (the bar is not always dirty)
                b.dirty = false;
                let has = b.font.is_some() && !b.label.is_empty();
                let f = copy_fill(b.fill, b.border).unwrap_or(b.fill);
                let mut l = format!("  copy 0 has {} fill {}", has as u8, rect_text(&f));
                if has {
                    let e = extent(&b.label, b.font.unwrap());
                    l += &format!(" text {}", rect_text(&copy_text(b.text, e)));
                }
                out.push(l);
            }
            other => panic!("op {other}"),
        }
        out.push(b.state());
        b.dirty = false; // the driver clears it after each state line
    }
    for (n, (a, w)) in out.iter().zip(lines.iter()).enumerate() {
        assert_eq!(a, w, "line {}", n + 1);
    }
    assert_eq!(out.len(), lines.len());
    assert_eq!(bars, 100);
}

#[test]
fn the_fill_is_at_least_a_pixel_and_clamps() {
    let r = UiRect::new(10.0, 0.0, 10.0, 5.0);
    assert_eq!(fill(1, 0, 1000, 100, r), (1, UiRect::new(10.0, 0.0, 11.0, 5.0)));
    assert_eq!(fill(-5, 0, 10, 100, r).0, 0);
    assert_eq!(fill(50, 0, 10, 100, r), (10, UiRect::new(10.0, 0.0, 110.0, 5.0)));
    // an empty range is empty; a reversed one takes its minimum
    assert_eq!(fill(3, 3, 3, 100, r).1.right, 10.0);
    assert_eq!(fill(5, 9, 2, 100, r).0, 9);
}
