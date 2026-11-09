//! `UISlider` (`User Interface/UISlider.cpp`): a thumb moving along a horizontal (style 0) or vertical (style 1)
//! track, its value range mapped onto the track's pixels. The scroll bar's middle part, and the game's volume and
//! speed sliders.
//!
//! The C++ object keeps its members; [`SliderFields`] is the pinned record of `UISlider`'s own members (from
//! `mThumbRectangle` on). The C++ keeps what touches other objects: the base `UIControl` members, the thumb bitmap,
//! the mouse capture and the virtual `ChangeRange`/`SetDirty`/`ButtonDown`, reached through [`SliderHost`]. The
//! algorithms re-read the record after every host call, because a `ChangeRange` override reads the C++ object
//! (`EEUIFSEESElevation::ChangeRange`).
//!
//! Integer arithmetic follows the C++'s mixed `long`/`unsigned long` conversions (`-fwrapv`), floats the same `f32`
//! operations in the same order.

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{um_floor, UiPoint, UiRect};

/// `UISlider`'s members in their C++ order (the record starts at `mThumbRectangle`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SliderFields {
    pub thumb: UiRect,
    pub copied_thumb: UiRect,
    /// `GEBitmap*`, C++'s (never touched here).
    pub thumb_bitmap: *mut c_void,
    pub style: c_ulong,
    pub pixels_per_unit: f32,
    pub units_per_pixel: f32,
    pub current: c_long,
    pub page_size: c_ulong,
    pub range: c_long,
    pub min_range: c_long,
    pub max_range: c_long,
    pub min_pixel: c_long,
    pub max_pixel: c_long,
    pub max_thumb_pixel: c_long,
    pub drag_offset: c_long,
    pub is_dragging: bool,
}

/// The slider's way back into its C++ object.
pub trait SliderHost {
    /// Reads the record.
    fn load(&self) -> SliderFields;
    /// Writes the record.
    fn store(&mut self, fields: &SliderFields);
    /// `UIControl::mRectangle`.
    fn rectangle(&self) -> UiRect;
    /// The virtual `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// The virtual `ChangeRange(value)` (the base sets `mCurrentRangeValue` and fires the range-changed trigger).
    fn change_range(&mut self, value: c_ulong);
    /// `UIControl::SetLocation(at)` (non-virtual: moves `mRectangle`, dirties the control).
    fn base_set_location(&mut self, at: UiPoint);
}

impl SliderFields {
    /// `GetCurrentPoint`: the pixel of a point along the track.
    pub fn current_point(&self, p: UiPoint) -> c_long {
        um_floor(if self.style == 1 { p.y } else { p.x })
    }

    /// `SetCurrentPoint`: puts a track pixel into the point's coordinate along the track.
    pub fn set_current_point(&self, point: c_long, out: &mut UiPoint) {
        if self.style == 1 {
            out.y = point as f32;
        } else {
            out.x = point as f32;
        }
    }

    /// `GetLocFromRange`: the thumb's top-left for a value (across the track: the control's left or top).
    pub fn loc_from_range(&self, rectangle: &UiRect, value: c_long, out: &mut UiPoint) {
        let point = um_floor(value.wrapping_sub(self.min_range) as f32 * self.pixels_per_unit + self.min_pixel as f32);
        self.set_current_point(point, out);
        if self.style == 0 {
            out.y = rectangle.top;
        } else {
            out.x = rectangle.left;
        }
    }

    /// `GetRangeFromLoc`: the value for a thumb position, exactly the minimum and maximum at the track's ends.
    pub fn range_from_loc(&self, at: UiPoint) -> c_ulong {
        let point = self.current_point(at);
        let value = if point == self.max_thumb_pixel {
            self.max_range as f32
        } else if point == self.min_pixel {
            self.min_range as f32
        } else {
            point.wrapping_sub(self.min_pixel) as f32 * self.units_per_pixel + self.min_range as f32
        };
        um_floor(value) as c_ulong
    }

    /// The thumb rectangle at a new top-left, keeping its size.
    fn thumb_at(&self, at: UiPoint) -> UiRect {
        let right = at.x + self.thumb.width();
        let bottom = at.y + self.thumb.height();
        UiRect::new(at.x, at.y, right, bottom)
    }

    /// The pixel scale of `SetRange` and `SetLocation`.
    fn scale(&mut self) {
        let pixel_range = self.max_thumb_pixel.wrapping_sub(self.min_pixel);
        if self.range != 0 {
            self.pixels_per_unit = pixel_range as f32 / self.range as f32;
            self.units_per_pixel = self.range as f32 / pixel_range as f32;
        } else {
            self.pixels_per_unit = 0.0;
            self.units_per_pixel = 0.0;
        }
    }

    /// The track's pixels for a style and rectangle (start pixel from `start`) -> the thumb's thickness.
    fn track(&mut self, rectangle: &UiRect, start: UiPoint) -> f32 {
        let thickness;
        if self.style == 0 {
            thickness = rectangle.height();
            self.min_pixel = um_floor(start.x);
            self.max_pixel = um_floor(rectangle.right).wrapping_add(1);
        } else {
            thickness = rectangle.width();
            self.min_pixel = um_floor(start.y);
            self.max_pixel = um_floor(rectangle.bottom).wrapping_add(1);
        }
        self.max_thumb_pixel = um_floor(self.max_pixel as f32 - thickness);
        thickness
    }
}

/// The constructor's part after `UIControl`'s: style, track pixels and the square thumb at the control's top-left.
/// The C++ then makes the thumb bitmap for [`SliderFields::thumb`], calls [`set_range`]`(0, 0)` and zeroes the page
/// size.
pub fn init(f: &mut SliderFields, rectangle: &UiRect, style: c_ulong) {
    f.style = style;
    f.is_dragging = false;
    f.current = 0;
    f.drag_offset = 0;
    let thickness = f.track(rectangle, rectangle.top_left());
    let right = rectangle.left + thickness;
    let bottom = rectangle.top + thickness;
    f.thumb = UiRect::new(rectangle.left, rectangle.top, right, bottom);
}

/// `ButtonUp`: ends a drag, or pages toward a click on the track outside the thumb.
pub fn button_up(h: &mut impl SliderHost, mouse: UiPoint) {
    let mut f = h.load();
    if f.is_dragging {
        f.is_dragging = false;
        h.store(&f);
        return;
    }
    if !h.rectangle().contains(mouse) {
        return;
    }
    let before = if f.style == 1 { mouse.y < f.thumb.top } else { mouse.x < f.thumb.left };
    if before {
        decrement_by_page_size(h);
    } else {
        increment_by_page_size(h);
    }
}

/// `DecrementByPageSize`: a page down, stopping at the minimum.
pub fn decrement_by_page_size(h: &mut impl SliderHost) {
    let f = h.load();
    let value = (f.current as c_ulong).wrapping_sub(f.page_size) as c_long;
    set_current_range_value(h, if value <= f.min_range { f.min_range } else { value });
}

/// `IncrementByPageSize`: a page up, stopping at the maximum.
pub fn increment_by_page_size(h: &mut impl SliderHost) {
    let f = h.load();
    let value = (f.current as c_ulong).wrapping_add(f.page_size) as c_long;
    set_current_range_value(h, if value >= f.max_range { f.max_range } else { value });
}

/// The middle of `MouseLButtonDown` (the C++ captures the mouse before and calls `ButtonDown` after): a press on the
/// thumb starts a drag, remembering where on the thumb it was taken.
pub fn press(f: &mut SliderFields, mouse: UiPoint) {
    if f.thumb.contains(mouse) {
        f.is_dragging = true;
        f.drag_offset = f.current_point(mouse).wrapping_sub(f.current_point(f.thumb.top_left()));
    }
}

/// `MouseMove`: drags the thumb within the track and updates the value.
pub fn mouse_move(h: &mut impl SliderHost, at: UiPoint) {
    let mut f = h.load();
    let mut location = f.thumb.top_left();
    let mut point = f.current_point(at).wrapping_sub(f.drag_offset);
    if f.is_dragging && f.max_thumb_pixel >= f.min_pixel {
        if point < f.min_pixel {
            point = f.min_pixel;
        } else if point > f.max_thumb_pixel {
            point = f.max_thumb_pixel;
        }
        f.set_current_point(point, &mut location);
        f.thumb = f.thumb_at(location);
        h.store(&f);
        let value = f.range_from_loc(f.thumb.top_left());
        h.change_range(value);
        h.set_dirty();
    }
}

/// `SetCurrentRangeValue`: the value clamped to the range, the thumb moved to match.
pub fn set_current_range_value(h: &mut impl SliderHost, value: c_long) {
    let f = h.load();
    let mut location = f.thumb.top_left();
    let value = if value < f.min_range {
        f.min_range
    } else if value > f.max_range {
        f.max_range
    } else {
        value
    };
    if f.current == value {
        return;
    }
    h.set_dirty();
    h.change_range(value as c_ulong);
    let mut f = h.load();
    if f.current == f.min_range {
        f.set_current_point(f.min_pixel, &mut location);
    } else if f.current == f.max_range {
        f.set_current_point(f.max_thumb_pixel, &mut location);
    } else {
        f.loc_from_range(&h.rectangle(), f.current, &mut location);
    }
    f.thumb = f.thumb_at(location);
    h.store(&f);
}

/// `SetLocation`: moves the control (the base class's move), then recomputes the track pixels, the scale and the
/// thumb -> the thumb's thickness, which the C++ uses to rescale the thumb bitmap.
pub fn set_location(h: &mut impl SliderHost, at: UiPoint) -> f32 {
    h.base_set_location(at);
    let rectangle = h.rectangle();
    let mut f = h.load();
    let thickness = f.track(&rectangle, at);
    f.scale();
    let mut location = UiPoint::default();
    f.loc_from_range(&rectangle, f.current, &mut location);
    let right = location.x + thickness;
    let bottom = location.y + thickness;
    f.thumb = UiRect::new(location.x, location.y, right, bottom);
    h.store(&f);
    thickness
}

/// `SetRange`: the value range, the pixel scale, and the current value clamped into the new range.
pub fn set_range(h: &mut impl SliderHost, min: c_ulong, max: c_ulong) {
    let mut f = h.load();
    if min == f.min_range as c_ulong && max == f.max_range as c_ulong {
        return;
    }
    f.min_range = min as c_long;
    f.max_range = max as c_long;
    f.range = max.wrapping_sub(min) as c_long;
    f.scale();
    h.store(&f);
    if f.current < min as c_long {
        set_current_range_value(h, min as c_long);
    } else if f.current > max as c_long {
        set_current_range_value(h, max as c_long);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_slider.txt");

    /// The golden driver's fakes (`UIControl::SetLocation` moving the rectangle, `ChangeRange` setting the value and
    /// firing 0x3e8), with the same trace lines.
    struct Fake {
        f: SliderFields,
        rect: UiRect,
        log: Vec<String>,
    }

    fn h(v: f32) -> String {
        format!("{:08x}", v.to_bits())
    }

    impl SliderHost for Fake {
        fn load(&self) -> SliderFields {
            self.f
        }
        fn store(&mut self, fields: &SliderFields) {
            self.f = *fields;
        }
        fn rectangle(&self) -> UiRect {
            self.rect
        }
        fn set_dirty(&mut self) {
            self.log.push("  dirty 1".into());
        }
        fn change_range(&mut self, value: c_ulong) {
            self.f.current = value as c_long;
            self.log.push(format!("  fire 3e8 current {}", self.f.current));
        }
        fn base_set_location(&mut self, at: UiPoint) {
            let dx = at.x - self.rect.left;
            let dy = at.y - self.rect.top;
            let r = self.rect;
            self.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
            self.log.push(format!("  base-setlocation {} {}", h(at.x), h(at.y)));
            self.set_dirty();
        }
    }

    impl Fake {
        fn dump(&self, what: &str) -> String {
            let (r, t, f) = (self.rect, self.f.thumb, &self.f);
            format!(
                "{what} {} {} {} {} | {} {} {} {} | {} {} {} {} {} {} {} {} {} {} {} {} {}",
                h(r.bottom),
                h(r.left),
                h(r.right),
                h(r.top),
                h(t.bottom),
                h(t.left),
                h(t.right),
                h(t.top),
                f.style,
                h(f.pixels_per_unit),
                h(f.units_per_pixel),
                f.current,
                f.page_size,
                f.range,
                f.min_range,
                f.max_range,
                f.min_pixel,
                f.max_pixel,
                f.max_thumb_pixel,
                f.drag_offset,
                f.is_dragging as u8
            )
        }
    }

    fn fx(s: &str) -> f32 {
        f32::from_bits(u32::from_str_radix(s, 16).unwrap())
    }

    fn blank() -> SliderFields {
        // the driver's `new UISlider` met zeroed memory: SetRange(0, 0) in the constructor returned at once
        SliderFields {
            thumb: UiRect::default(),
            copied_thumb: UiRect::default(),
            thumb_bitmap: core::ptr::null_mut(),
            style: 0,
            pixels_per_unit: 0.0,
            units_per_pixel: 0.0,
            current: 0,
            page_size: 0,
            range: 0,
            min_range: 0,
            max_range: 0,
            min_pixel: 0,
            max_pixel: 0,
            max_thumb_pixel: 0,
            drag_offset: 0,
            is_dragging: false,
        }
    }

    #[test]
    fn replays_the_cpp_trace_line_for_line() {
        let golden: Vec<&str> = GOLDEN.lines().collect();
        let mut out: Vec<String> = Vec::new();
        let mut s: Option<Fake> = None;
        for line in &golden {
            let t: Vec<&str> = line.split(' ').collect();
            let fake = s.as_mut();
            match t[0] {
                "new" => {
                    out.push(line.to_string());
                    let style: c_ulong = t[3].parse().unwrap();
                    let rect = UiRect::new(fx(t[5]), fx(t[6]), fx(t[7]), fx(t[8]));
                    let mut f = Fake { f: blank(), rect, log: Vec::new() };
                    init(&mut f.f, &rect, style);
                    let th = f.f.thumb;
                    out.push(format!("  scaled-bitmap {} {} {} {}", h(th.left), h(th.top), h(th.right), h(th.bottom)));
                    set_range(&mut f, 0, 0);
                    f.f.page_size = 0;
                    out.append(&mut f.log);
                    out.push(f.dump("ctor"));
                    s = Some(f);
                }
                "op" => {
                    let f = fake.unwrap();
                    out.push(line.to_string());
                    match t[1] {
                        "setrange" => set_range(f, t[2].parse().unwrap(), t[3].parse().unwrap()),
                        "setvalue" => set_current_range_value(f, t[2].parse().unwrap()),
                        "setpage" => f.f.page_size = t[2].parse().unwrap(),
                        "inc" => increment_by_page_size(f),
                        "dec" => decrement_by_page_size(f),
                        "down" => {
                            let mouse = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                            f.log.push("  capture".into());
                            press(&mut f.f, mouse);
                            f.log.push("  = 0".into());
                        }
                        "move" => {
                            let mouse = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                            mouse_move(f, mouse);
                            f.log.push("  = 0".into());
                        }
                        "up" => {
                            let mouse = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                            f.log.push("  release".into());
                            button_up(f, mouse);
                            f.log.push("  = 0".into());
                        }
                        "setlocation" => {
                            let at = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                            let thickness = set_location(f, at);
                            let (xs, ys) = (0.75f32 * thickness * 0.05, 1.25f32 * thickness * 0.05);
                            f.log.push(format!("  bitmap-scale {} {}", h(xs), h(ys)));
                        }
                        "loc" => {
                            // "op loc V -> X Y; range X Y -> N": recomputed, so the line itself is the check
                            let v: c_long = t[2].parse().unwrap();
                            let mut p = UiPoint { x: -1.0, y: -1.0 };
                            f.f.loc_from_range(&f.rect, v, &mut p);
                            let q = UiPoint { x: fx(t[7]), y: fx(t[8]) };
                            out.pop();
                            out.push(format!(
                                "op loc {v} -> {} {}; range {} {} -> {}",
                                h(p.x),
                                h(p.y),
                                t[7],
                                t[8],
                                f.f.range_from_loc(q)
                            ));
                        }
                        other => panic!("unknown op {other}"),
                    }
                    out.append(&mut f.log);
                }
                "state" => out.push(fake.unwrap().dump("state")),
                _ => {}
            }
        }
        assert_eq!(out.len(), golden.len(), "line count");
        for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
            assert_eq!(a, b, "line {}", i + 1);
        }
    }

    fn sample(style: c_ulong) -> Fake {
        let rect = if style == 0 { UiRect::new(10.0, 20.0, 129.0, 39.0) } else { UiRect::new(10.0, 20.0, 29.0, 139.0) };
        let mut f = Fake { f: blank(), rect, log: Vec::new() };
        init(&mut f.f, &rect, style);
        f
    }

    #[test]
    fn the_track_spans_the_control_less_the_thumb() {
        let f = sample(0);
        assert_eq!((f.f.min_pixel, f.f.max_pixel, f.f.max_thumb_pixel), (10, 130, 111));
        assert_eq!(f.f.thumb, UiRect::new(10.0, 20.0, 29.0, 39.0));
        let v = sample(1);
        assert_eq!((v.f.min_pixel, v.f.max_pixel, v.f.max_thumb_pixel), (20, 140, 121));
    }

    #[test]
    fn set_range_clamps_the_value_and_fires_once() {
        let mut f = sample(0);
        set_range(&mut f, 0, 10);
        set_current_range_value(&mut f, 7);
        f.log.clear();
        set_range(&mut f, 0, 5);
        assert_eq!(f.f.current, 5);
        assert_eq!(f.log, ["  dirty 1", "  fire 3e8 current 5"]);
        assert_eq!(f.f.thumb.left, f.f.max_thumb_pixel as f32);
        f.log.clear();
        set_range(&mut f, 0, 5); // unchanged: nothing
        set_current_range_value(&mut f, 99); // clamped to the same value: nothing
        assert!(f.log.is_empty());
    }

    #[test]
    fn a_page_step_wraps_like_the_unsigned_sum() {
        let mut f = sample(0);
        set_range(&mut f, 0, 100);
        f.f.page_size = 30;
        increment_by_page_size(&mut f);
        increment_by_page_size(&mut f);
        assert_eq!(f.f.current, 60);
        decrement_by_page_size(&mut f);
        decrement_by_page_size(&mut f);
        decrement_by_page_size(&mut f);
        assert_eq!(f.f.current, 0);
        // a page as large as an unsigned long's top half wraps negative: the C++ clamps it to the minimum
        f.f.page_size = c_ulong::MAX;
        increment_by_page_size(&mut f);
        assert_eq!(f.f.current, 0);
    }

    #[test]
    fn a_drag_follows_the_mouse_within_the_track() {
        let mut f = sample(0);
        set_range(&mut f, 0, 100);
        press(&mut f.f, UiPoint { x: 15.0, y: 25.0 });
        assert!(f.f.is_dragging);
        assert_eq!(f.f.drag_offset, 5);
        mouse_move(&mut f, UiPoint { x: 500.0, y: 25.0 });
        assert_eq!((f.f.thumb.left, f.f.current), (111.0, 100));
        mouse_move(&mut f, UiPoint { x: -50.0, y: 25.0 });
        assert_eq!((f.f.thumb.left, f.f.current), (10.0, 0));
        button_up(&mut f, UiPoint { x: 0.0, y: 0.0 });
        assert!(!f.f.is_dragging);
        f.log.clear();
        mouse_move(&mut f, UiPoint { x: 60.0, y: 25.0 }); // not dragging: nothing
        assert!(f.log.is_empty());
    }

    #[test]
    fn a_click_beside_the_thumb_pages_toward_it() {
        let mut f = sample(1);
        set_range(&mut f, 0, 100);
        f.f.page_size = 10;
        button_up(&mut f, UiPoint { x: 15.0, y: 100.0 });
        assert_eq!(f.f.current, 10);
        button_up(&mut f, UiPoint { x: 15.0, y: 21.0 });
        assert_eq!(f.f.current, 0);
        button_up(&mut f, UiPoint { x: 300.0, y: 100.0 }); // outside the control
        assert_eq!(f.f.current, 0);
    }

    #[test]
    fn an_empty_range_has_no_scale() {
        let mut f = sample(0);
        f.f.range = 5;
        set_range(&mut f, 3, 3);
        assert_eq!((f.f.range, f.f.pixels_per_unit, f.f.units_per_pixel), (0, 0.0, 0.0));
        assert_eq!(f.f.current, 3);
        assert_eq!(f.f.range_from_loc(UiPoint { x: 10.0, y: 0.0 }), 3);
    }
}
