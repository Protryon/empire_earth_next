//! The scroll bar against the real C++ (`golden_scrollbar.txt`): a scratch driver built over the original
//! `UIScrollBar.cpp` (fakes for the slider, the arrow buttons and `UIControl`'s out-of-line members) printed four
//! bars under 2,000 seeded operations. The replay below uses the same fakes; the same driver over the forwarders and
//! the new `ee_rust.lib` prints the same file.
#![allow(clippy::unnecessary_cast, clippy::manual_dangling_ptr, clippy::too_many_arguments)]

use super::*;

const GOLDEN: &str = include_str!("../golden_scrollbar.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rs(r: &UiRect) -> String {
    format!(" {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

struct Fake {
    f: ScrollBarFields,
    rect: UiRect,
    parts: [UiRect; 3],
    style: c_ulong,
    value: c_long,
    mouse: UiPoint,
    log: Vec<String>,
}

const NAMES: [&str; 3] = ["inc", "dec", "slider"];

fn moved(r: UiRect, at: UiPoint) -> UiRect {
    let (dx, dy) = (at.x - r.left, at.y - r.top);
    UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy)
}

impl ScrollBarHost for Fake {
    fn load(&self) -> ScrollBarFields {
        self.f
    }
    fn store(&mut self, f: &ScrollBarFields) {
        self.f = *f;
    }
    fn rectangle(&self) -> UiRect {
        self.rect
    }
    fn mouse_location(&self) -> UiPoint {
        self.mouse
    }
    fn part_rect(&self, part: ScrollPart) -> UiRect {
        self.parts[part as usize]
    }
    fn part_mouse_down(&mut self, part: ScrollPart) -> c_long {
        self.log.push(format!("  ldown {}", NAMES[part as usize]));
        if part == ScrollPart::Slider {
            31
        } else {
            21
        }
    }
    fn part_mouse_up(&mut self, part: ScrollPart) -> c_long {
        self.log.push(format!("  lup {}", NAMES[part as usize]));
        if part == ScrollPart::Slider {
            32
        } else {
            22
        }
    }
    fn slider_mouse_move(&mut self, at: UiPoint) -> c_long {
        self.log.push(format!("  move slider {} {}", h(at.x), h(at.y)));
        33
    }
    fn button_down(&mut self, part: ScrollPart) {
        self.log.push(format!("  buttondown {}", NAMES[part as usize]));
    }
    fn capture_mouse(&mut self) {
        self.log.push("  capture-mouse bar".into());
    }
    fn release_mouse(&mut self) {
        self.log.push("  release-mouse bar".into());
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.rect = moved(self.rect, at);
        self.log.push(format!("  base-setlocation bar {} {}", h(at.x), h(at.y)));
        self.log.push("  dirty bar".into());
    }
    fn slider_style(&self) -> c_ulong {
        self.style
    }
    fn part_set_location(&mut self, part: ScrollPart, at: UiPoint) {
        self.log.push(format!("  setlocation {} {} {}", NAMES[part as usize], h(at.x), h(at.y)));
        self.parts[part as usize] = moved(self.parts[part as usize], at);
    }
    fn slider_set_rectangle(&mut self, rect: UiRect) {
        self.log.push(format!("  setrectangle slider{}", rs(&rect)));
        self.parts[2] = rect;
    }
    fn slider_value(&self) -> c_long {
        self.value
    }
    fn set_slider_value(&mut self, value: c_long) {
        self.log.push(format!("  slider-set {value}"));
        self.value = value;
    }
}

fn dump(i: usize, b: &Fake) -> String {
    format!(
        "state {i}{} |{} |{} |{} | {} {} {} value {}",
        rs(&b.rect),
        rs(&b.parts[0]),
        rs(&b.parts[1]),
        rs(&b.parts[2]),
        b.f.increment_pressed as u8,
        b.f.decrement_pressed as u8,
        b.f.slider_pressed as u8,
        b.value
    )
}

fn bar(at: UiPoint, len: f32, style: c_ulong, border: c_ulong, log: &mut Vec<String>) -> Fake {
    let l = layout(at, len, style);
    log.push(format!("  slider-new style {style} border {border}{}", rs(&l.slider)));
    let (dn, dp, inn, ip) = if style == 1 { ("UN", "UP", "DN", "DP") } else { ("LN", "LP", "RN", "RP") };
    let button = |p: UiPoint| UiRect::new(p.x, p.y, p.x + 19.0, p.y + 19.0);
    log.push(format!("  button-new {} {} dec 1 bitmaps {dn} {dp}", h(at.x), h(at.y)));
    log.push(format!("  button-new {} {} dec 0 bitmaps {inn} {ip}", h(l.increment.x), h(l.increment.y)));
    let mark = 1 as *mut c_void;
    Fake {
        f: ScrollBarFields {
            increment_button: mark,
            decrement_button: mark,
            slider: mark,
            increment_pressed: false,
            decrement_pressed: false,
            slider_pressed: false,
        },
        rect: l.rect,
        parts: [button(l.increment), button(at), l.slider],
        style,
        value: 50,
        mouse: UiPoint::default(),
        log: Vec::new(),
    }
}

#[test]
fn replays_the_cpp_trace_line_for_line() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut bars: Vec<Fake> = Vec::new();
    let mut mouse = UiPoint::default();
    for line in &golden {
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "new" => {
                out.push(line.to_string());
                let at = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                let b = bar(at, fx(t[4]), t[5].parse().unwrap(), t[6].parse().unwrap(), &mut out);
                out.push(dump(bars.len(), &b));
                bars.push(b);
            }
            "env" => {
                out.push(line.to_string());
                mouse = UiPoint { x: fx(t[1]), y: fx(t[2]) };
            }
            "op" => {
                out.push(line.to_string());
                let i: usize = t[1].parse().unwrap();
                let b = &mut bars[i];
                b.mouse = mouse;
                let r = match t[2] {
                    "ldown" => mouse_l_button_down(b),
                    "lup" => mouse_l_button_up(b),
                    "move" => mouse_move(b, UiPoint { x: fx(t[3]), y: fx(t[4]) }),
                    "setlocation" => {
                        set_location(b, UiPoint { x: fx(t[3]), y: fx(t[4]) });
                        0
                    }
                    "incby" => {
                        increment_by(b, t[3].parse().unwrap());
                        0
                    }
                    other => panic!("op {other}"),
                };
                out.append(&mut b.log);
                out.push(format!("  = {r}"));
                out.push(dump(i, b));
            }
            _ => {}
        }
    }
    assert_eq!(out.len(), golden.len(), "line count");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
}

#[test]
fn the_layout_matches_the_fixed_constructor() {
    let v = layout(UiPoint { x: 10.0, y: 10.0 }, 200.0, 1);
    assert_eq!(v.rect, UiRect::new(10.0, 10.0, 28.0, 208.0));
    assert_eq!(v.slider, UiRect::new(10.0, 30.0, 30.0, 190.0));
    assert_eq!(v.increment, UiPoint { x: 10.0, y: 190.0 });
    let hz = layout(UiPoint { x: 0.0, y: 5.0 }, 100.0, 0);
    assert_eq!(hz.rect, UiRect::new(0.0, 5.0, 98.0, 23.0), "the horizontal bar is (length - 1) x 19");
    assert_eq!(hz.slider, UiRect::new(20.0, 5.0, 80.0, 25.0));
    assert_eq!(hz.increment, UiPoint { x: 80.0, y: 5.0 });
}

#[test]
fn a_bar_without_a_slider_ignores_increments() {
    let mut log = Vec::new();
    let mut b = bar(UiPoint::default(), 100.0, 1, 0, &mut log);
    b.f.slider = core::ptr::null_mut();
    increment_by(&mut b, 3);
    assert_eq!(b.value, 50);
    assert!(b.log.is_empty());
    b.f.slider = 1 as *mut c_void;
    increment_by(&mut b, -3);
    assert_eq!(b.value, 47);
}

#[test]
fn the_press_goes_to_the_part_under_the_mouse_and_the_release_to_all_pressed() {
    let mut log = Vec::new();
    let mut b = bar(UiPoint { x: 10.0, y: 10.0 }, 200.0, 1, 0, &mut log);
    b.mouse = UiPoint { x: 15.0, y: 15.0 };
    assert_eq!(mouse_l_button_down(&mut b), 21);
    assert!(b.f.decrement_pressed && !b.f.slider_pressed);
    b.mouse = UiPoint { x: 15.0, y: 100.0 };
    assert_eq!(mouse_l_button_down(&mut b), 31);
    assert!(b.f.slider_pressed);
    b.log.clear();
    // slider, then the arrow: the arrow's result wins
    assert_eq!(mouse_l_button_up(&mut b), 22);
    assert_eq!(b.log, ["  lup slider", "  lup dec", "  release-mouse bar"]);
    assert!(!b.f.decrement_pressed && !b.f.slider_pressed && !b.f.increment_pressed);
}
