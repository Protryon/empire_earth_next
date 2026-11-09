//! `UILayoutUtility` (was `User Interface/UILayoutUtility.cpp`): a rectangle's four edges anchored to its parent,
//! each by an offset and an offset type (the UI db tables store both). The C++ class keeps the record; its members
//! forward here (`UITrampolines.cpp`).

use core::ffi::{c_long, c_ulong};

use crate::rect::UiRect;

/// `kLayoutFromStart`: left/top + offset.
pub const FROM_START: c_ulong = 0;
/// `kLayoutAfterCenter`: centre + offset.
pub const AFTER_CENTER: c_ulong = 1;
/// `kLayoutBeforeCenter`: centre - offset.
pub const BEFORE_CENTER: c_ulong = 2;
/// `kLayoutFromEnd`: right/bottom - offset.
pub const FROM_END: c_ulong = 3;
/// `kLayoutProportional`: left/top + size x offset.
pub const PROPORTIONAL: c_ulong = 4;
/// `kLayoutFromStartEdge`: the left/top edge's own point + offset (a width or height).
pub const FROM_START_EDGE: c_ulong = 5;

/// `UILayoutUtility`'s members in their C++ order (sizeof 0x20 where `unsigned long` is 32 bits).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Layout {
    pub left_offset: f32,
    pub right_offset: f32,
    pub top_offset: f32,
    pub bottom_offset: f32,
    pub left_type: c_ulong,
    pub right_type: c_ulong,
    pub top_type: c_ulong,
    pub bottom_type: c_ulong,
}

/// Which edge `offset` moves (`OffsetLeft`, `OffsetRight`, `OffsetTop`, `OffsetBottom`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Edge {
    Left = 0,
    Right = 1,
    Top = 2,
    Bottom = 3,
}

impl Default for Layout {
    /// The C++ constructor: fills the parent (left/top from the start, right/bottom from the end, zero offsets).
    fn default() -> Self {
        Layout {
            left_offset: 0.0,
            right_offset: 0.0,
            top_offset: 0.0,
            bottom_offset: 0.0,
            left_type: FROM_START,
            right_type: FROM_END,
            top_type: FROM_START,
            bottom_type: FROM_END,
        }
    }
}

/// One axis of `GetHorizontalPoint`/`GetVerticalPoint`: `start`/`size`/`end` are the parent's left (top), width
/// (height) and right (bottom); `edge` is the start edge's (type, offset) for `kLayoutFromStartEdge`. An unknown type
/// gives 0. A start edge of type `kLayoutFromStartEdge` recurses for ever, as the C++ did (no table uses it).
fn point(start: f32, size: f32, end: f32, edge: (c_ulong, f32), kind: c_ulong, offset: f32) -> f32 {
    match kind {
        FROM_START_EDGE => point(start, size, end, edge, edge.0, edge.1) + offset,
        PROPORTIONAL => start + size * offset,
        FROM_END => end - offset,
        BEFORE_CENTER => start + (size * 0.5 - offset),
        AFTER_CENTER => start + (size * 0.5 + offset),
        FROM_START => start + offset,
        _ => 0.0,
    }
}

impl Layout {
    fn horizontal(&self, parent: &UiRect, kind: c_ulong, offset: f32) -> f32 {
        point(parent.left, parent.width(), parent.right, (self.left_type, self.left_offset), kind, offset)
    }

    fn vertical(&self, parent: &UiRect, kind: c_ulong, offset: f32) -> f32 {
        point(parent.top, parent.height(), parent.bottom, (self.top_type, self.top_offset), kind, offset)
    }

    /// `CalculateRectangle`: the rectangle inside `parent`, rounded to whole pixels; `exclusive` pulls the right and
    /// bottom edges in by one.
    pub fn calculate(&self, parent: &UiRect, exclusive: bool) -> UiRect {
        let mut out = UiRect {
            top: self.vertical(parent, self.top_type, self.top_offset),
            left: self.horizontal(parent, self.left_type, self.left_offset),
            right: self.horizontal(parent, self.right_type, self.right_offset),
            bottom: self.vertical(parent, self.bottom_type, self.bottom_offset),
        };
        out.round_to_nearest();
        if exclusive {
            out.right -= 1.0;
            out.bottom -= 1.0;
        }
        out
    }

    /// `OffsetLeft`/`OffsetRight`/`OffsetTop`/`OffsetBottom`: moves one edge by `amount` pixels in its offset type's
    /// direction; a proportional edge moves by `amount / (size - 1)` (`size - 1` wraps as an `unsigned long`).
    pub fn offset(&mut self, edge: Edge, amount: c_long, size: c_ulong) {
        let (value, kind) = match edge {
            Edge::Left => (&mut self.left_offset, self.left_type),
            Edge::Right => (&mut self.right_offset, self.right_type),
            Edge::Top => (&mut self.top_offset, self.top_type),
            Edge::Bottom => (&mut self.bottom_offset, self.bottom_type),
        };
        match kind {
            FROM_START | AFTER_CENTER | FROM_START_EDGE => *value += amount as f32,
            BEFORE_CENTER | FROM_END => *value -= amount as f32,
            PROPORTIONAL => *value += amount as f32 / size.wrapping_sub(1) as f32,
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_layout.txt");

    fn f(hex: &str) -> f32 {
        f32::from_bits(u32::from_str_radix(hex, 16).unwrap())
    }

    fn lines(tag: &str) -> impl Iterator<Item = Vec<&'static str>> + '_ {
        GOLDEN.lines().map(|l| l.split(' ').collect::<Vec<_>>()).filter(move |t| t[0] == tag)
    }

    #[test]
    fn default_matches_the_cpp_constructor() {
        let t = lines("default").next().unwrap();
        let d = Layout::default();
        assert_eq!(
            t[1..].join(" "),
            format!(
                "{:08x} {:08x} {:08x} {:08x} {} {} {} {}",
                d.left_offset.to_bits(),
                d.right_offset.to_bits(),
                d.top_offset.to_bits(),
                d.bottom_offset.to_bits(),
                d.left_type,
                d.right_type,
                d.top_type,
                d.bottom_type
            )
        );
    }

    #[test]
    fn calculate_matches_the_cpp_bit_for_bit() {
        let mut n = 0;
        for t in lines("calc") {
            let l = Layout {
                left_type: t[1].parse().unwrap(),
                left_offset: f(t[2]),
                right_type: t[3].parse().unwrap(),
                right_offset: f(t[4]),
                top_type: t[5].parse().unwrap(),
                top_offset: f(t[6]),
                bottom_type: t[7].parse().unwrap(),
                bottom_offset: f(t[8]),
            };
            let parent = UiRect { bottom: f(t[9]), left: f(t[10]), right: f(t[11]), top: f(t[12]) };
            n += 1;
            if parent.right.is_nan() && !cfg!(windows) {
                continue; // rounding NaN is the C runtime's choice (rect::float_to_long); the golden is the UCRT's
            }
            let out = l.calculate(&parent, t[13] == "1");
            let want = [f(t[14]), f(t[15]), f(t[16]), f(t[17])];
            let got = [out.bottom, out.left, out.right, out.top];
            assert_eq!(got.map(f32::to_bits), want.map(f32::to_bits), "case {}", t.join(" "));
        }
        assert_eq!(n, 1200);
    }

    #[test]
    fn offset_matches_the_cpp_bit_for_bit() {
        let mut n = 0;
        for t in lines("offset") {
            let which: u32 = t[1].parse().unwrap();
            let kind: c_ulong = t[2].parse().unwrap();
            let before = f(t[3]);
            let amount: c_long = t[4].parse().unwrap();
            let size: c_ulong = t[5].parse().unwrap();
            let mut l = Layout {
                left_offset: before,
                right_offset: before,
                top_offset: before,
                bottom_offset: before,
                // the driver gives the left edge another type when it moves the right one (OffsetRight once read
                // the left edge's type)
                left_type: if which == 1 { (kind + 1) % 7 } else { kind },
                right_type: kind,
                top_type: kind,
                bottom_type: kind,
            };
            let edge = [Edge::Left, Edge::Right, Edge::Top, Edge::Bottom][which as usize];
            l.offset(edge, amount, size);
            let after = [l.left_offset, l.right_offset, l.top_offset, l.bottom_offset][which as usize];
            assert_eq!(after.to_bits(), f(t[6]).to_bits(), "case {}", t.join(" "));
            n += 1;
        }
        assert_eq!(n, 800);
    }

    #[test]
    fn start_edge_measures_from_the_start_edge() {
        let l = Layout {
            left_type: FROM_END,
            left_offset: 30.0,
            right_type: FROM_START_EDGE,
            right_offset: 20.0,
            ..Layout::default()
        };
        let out = l.calculate(&UiRect::new(0.0, 0.0, 100.0, 50.0), false);
        assert_eq!((out.left, out.right), (70.0, 90.0));
    }

    #[test]
    fn unknown_type_is_zero_and_offset_ignores_it() {
        let mut l = Layout { top_type: 9, top_offset: 5.0, ..Layout::default() };
        let out = l.calculate(&UiRect::new(10.0, 10.0, 20.0, 20.0), true);
        assert_eq!((out.top, out.bottom, out.right), (0.0, 19.0, 19.0));
        l.offset(Edge::Top, 3, 10);
        assert_eq!(l.top_offset, 5.0);
    }

    #[test]
    fn proportional_offset_divides_by_the_wrapped_size() {
        let mut l = Layout { left_type: PROPORTIONAL, ..Layout::default() };
        l.offset(Edge::Left, 1, 0);
        assert_eq!(l.left_offset, 1.0 / c_ulong::MAX as f32);
        l.left_offset = 0.0;
        l.offset(Edge::Left, 5, 11);
        assert_eq!(l.left_offset, 0.5);
    }
}
