//! `UIListBox<UWideString>`'s golden trace: a scratch driver instantiated the real template (`lle/UIListBox.h` before
//! the move) over fakes for everything outside it, ran 60 seeded boxes of 60 random operations each (appends,
//! inserts plain and sorted, deletes, selection changes, arrow keys, clicks, double clicks, mouse moves, moves, sorts)
//! and printed every host call and state change. The replay runs the C++ that stays (the data, `std::sort`, the
//! constructor) in [`Fake`] and the rest through [`List`]. The same driver over the forwarding header and
//! `ee_rust.lib` printed the same file.

use super::*;
use crate::text::{less_no_case, SingleByte};

const GOLDEN: &str = include_str!("../golden_list.txt");

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

/// The quoted texts in a golden line, in order, each terminated.
fn quoted(line: &str) -> Vec<Vec<u8>> {
    let s = line.as_bytes();
    let mut out = Vec::new();
    let mut i = 0;
    while i < s.len() {
        if s[i] != b'"' {
            i += 1;
            continue;
        }
        i += 1;
        let mut v = Vec::new();
        while s[i] != b'"' {
            if s[i] == b'\\' {
                v.push(u8::from_str_radix(core::str::from_utf8(&s[i + 2..i + 4]).unwrap(), 16).unwrap());
                i += 4;
            } else {
                v.push(s[i]);
                i += 1;
            }
        }
        v.push(0);
        out.push(v);
        i += 1;
    }
    out
}

/// The driver's fakes; the data is the C++ vector.
struct Fake {
    s: ListState,
    data: Vec<Vec<u8>>,
    scroll: Option<(c_long, c_long, c_long)>,
    mouse: UiPoint,
    log: Vec<String>,
}

impl Fake {
    /// `UIListBox::UIListBox` with the driver's arguments.
    fn new(line_height: c_ulong, auto_select: bool, w: f32, h_: f32, border: c_ulong, data: Vec<Vec<u8>>) -> Fake {
        let rect = UiRect::new(10.0, 20.0, 10.0 + w, 20.0 + h_);
        let b = border as f32;
        let text = UiRect::new(rect.left + b, rect.top + b, rect.right - b, rect.bottom - b);
        let mut shown = (um_floor(rect.height() - (2 * border) as f32) as c_ulong) / line_height;
        if shown as f32 > rect.height() {
            shown = 1;
        }
        let f = ListFields {
            line_height,
            auto_select,
            has_selection: true,
            number_lines_displayed: shown,
            ..Default::default()
        };
        let mut fake = Fake {
            s: ListState { f, text, rect, border },
            data,
            scroll: None,
            mouse: UiPoint { x: 0.0, y: 0.0 },
            log: Vec::new(),
        };
        fake.run(|l| l.check_scroll_bar());
        fake
    }

    fn run<R>(&mut self, op: impl FnOnce(&mut List<Fake>) -> R) -> R {
        let mut l = List::new(self);
        let r = op(&mut l);
        l.finish();
        r
    }

    fn dump(&self) -> String {
        let f = &self.s.f;
        let (v, min, max) = self.scroll.unwrap_or((-1, -1, -1));
        let (t, r) = (self.s.text, self.s.rect);
        format!(
            "state sel {} has {} shown {} lh {} auto {} bd {} removed {} sb {v} {min} {max} text {} {} {} {} \
             rect {} {} {} {}",
            f.current_selection,
            f.has_selection as u8,
            f.number_lines_displayed,
            f.line_height,
            f.auto_select as u8,
            f.button_down as u8,
            f.scroll_bar_removed as u8,
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
}

impl ListHost for Fake {
    fn load(&self) -> ListState {
        self.s
    }
    fn store(&mut self, s: &ListState) {
        self.s.f = s.f;
        self.s.text = s.text;
    }
    fn num_lines(&self) -> c_ulong {
        self.data.len() as c_ulong
    }
    fn change_selection(&mut self, index: c_ulong, fire: bool, scroll_to: bool) {
        self.run(|l| l.change_selection_index(index, fire, scroll_to));
    }
    fn clear_selection(&mut self) {
        self.s.f.has_selection = false;
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
    fn scroll_set_rectangle(&mut self, r: &UiRect) {
        self.log.push(format!("  setrect {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom)));
        self.log.push(format!("  sbloc {} {}", h(r.left), h(r.top)));
    }
    fn create_scroll_bar(&mut self, at: UiPoint, length: f32, _page: c_ulong, value: c_ulong) {
        self.log.push(format!("  scrollbar {} {} {} 1", h(at.x), h(at.y), h(length)));
        self.scroll = Some((0, 0, 0));
        self.set_scroll_value(value);
        self.log.push(format!("  sbcolor {}", h(0.0)));
        self.log.push(format!("  sbopacity {}", h(self.s.f.scroll_bar_bg_opacity)));
    }
    fn set_dirty(&mut self) {
        self.log.push("  dirty 1".into());
    }
    fn fire(&mut self, event: c_ulong) {
        self.log.push(format!("  fire {event:x}"));
    }
    fn set_bg_rectangle(&mut self, r: &UiRect) {
        self.log.push(format!("  bgrect {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom)));
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.log.push(format!("  baseloc {} {}", h(at.x), h(at.y)));
        let r = self.s.rect;
        self.s.rect = UiRect::new(at.x, at.y, at.x + (r.right - r.left), at.y + (r.bottom - r.top));
    }
    fn mouse_location(&self) -> UiPoint {
        self.mouse
    }
}

/// `std::lower_bound` from `from` (MSVC's: the first element not less than `line`).
fn lower_bound(data: &[Vec<u8>], from: usize, line: &[u8], less: impl Fn(&[u8], &[u8]) -> bool) -> usize {
    // a binary search, so an unsorted range gives MSVC's answer, not the first greater element
    let (mut first, mut count) = (from, data.len() - from);
    while count > 0 {
        let half = count / 2;
        if less(&data[first + half], line) {
            first += half + 1;
            count -= half + 1;
        } else {
            count = half;
        }
    }
    first
}

#[test]
fn replays_the_cpp_golden() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut fake: Option<Fake> = None;
    let mut last_state = String::new();
    let mut last_lines: Vec<Vec<u8>> = Vec::new();
    let mut force = false;
    for line in &golden {
        let t: Vec<&str> = line.split(' ').collect();
        let pt = |k: usize| UiPoint { x: fx(t[k]), y: fx(t[k + 1]) };
        if t[0] == "new" {
            out.push(line.to_string());
            let mut f =
                Fake::new(t[1].parse().unwrap(), t[2] == "1", fx(t[3]), fx(t[4]), t[5].parse().unwrap(), quoted(line));
            out.append(&mut f.log);
            fake = Some(f);
            force = true;
        } else if t[0] == "op" {
            let f = fake.as_mut().unwrap();
            if t[1] != "getsel" {
                out.push(line.to_string());
            }
            match t[1] {
                "append" => {
                    f.data.push(quoted(line).remove(0));
                    f.set_dirty();
                    f.run(|l| l.check_scroll_bar());
                }
                "appendarr" => {
                    f.data.extend(quoted(line));
                    f.run(|l| l.check_scroll_bar());
                    f.set_dirty();
                }
                "insert" | "insertsorted" => {
                    let from: usize = t[2].parse().unwrap();
                    let s = quoted(line).remove(0);
                    let at = if t[1] == "insert" {
                        from
                    } else {
                        lower_bound(&f.data, from, &s, |a, b| less_no_case(&SingleByte, a, b))
                    };
                    f.data.insert(at, s);
                    f.run(|l| l.inserted(at as c_ulong));
                }
                "delete" => {
                    let at: usize = t[2].parse().unwrap();
                    f.data.remove(at);
                    f.run(|l| l.deleted_line(at as c_ulong));
                }
                "deleteall" => {
                    f.data.clear();
                    f.run(|l| l.cleared());
                }
                "change" => {
                    let (i, fire, scroll) = (t[2].parse().unwrap(), t[3] == "1", t[4] == "1");
                    f.run(|l| l.change_selection_index(i, fire, scroll));
                }
                "down" => f.run(|l| l.down()),
                "up" => f.run(|l| l.up()),
                "home" => f.run(|l| l.home()),
                "end" => f.run(|l| l.end()),
                "buttondown" => {
                    f.mouse = pt(3);
                    f.run(|l| l.button_down());
                }
                "mouseup" => {
                    f.mouse = pt(3);
                    if f.run(|l| l.up_on_selection()) {
                        f.fire(0x3f0);
                    }
                    f.log.push("  release".into());
                    f.s.f.scroll_bar_captured = false;
                    f.s.f.button_down = false;
                }
                "double" => {
                    f.mouse = pt(3);
                    assert!(f.run(|l| l.double_pressed()), "the driver clicks inside the text");
                }
                "move" => {
                    f.mouse = pt(6);
                    let at = pt(3);
                    f.run(|l| l.mouse_move(at));
                }
                "loc" => {
                    let at = pt(3);
                    f.run(|l| l.set_location(at));
                }
                "sort" => {
                    // MSVC's std::sort is an insertion sort up to 32 elements (stable); the driver keeps lists shorter
                    assert!(f.data.len() <= 32);
                    f.data.sort_by(|a, b| {
                        if less_no_case(&SingleByte, a, b) {
                            core::cmp::Ordering::Less
                        } else if less_no_case(&SingleByte, b, a) {
                            core::cmp::Ordering::Greater
                        } else {
                            core::cmp::Ordering::Equal
                        }
                    })
                }
                "check" => f.run(|l| l.check_scroll_bar()),
                "getsel" => {
                    let code = if !f.data.is_empty() && f.s.f.has_selection { 0 } else { 0xd };
                    let s = quoted(line).remove(0);
                    let found = f.data.iter().position(|d| *d == s);
                    out.push(format!(
                        "op getsel = {code} {} find {} = {} {}",
                        f.s.f.current_selection,
                        esc(&s),
                        found.is_some() as u8,
                        found.map(|i| i as c_ulong).unwrap_or(0xbeef)
                    ));
                }
                "auto" => f.s.f.auto_select = t[2] == "1",
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
        if force || f.data != last_lines {
            out.push(format!("  lines{}", f.data.iter().map(|l| format!(" {}", esc(l))).collect::<String>()));
        }
        last_lines = f.data.clone();
        force = false;
    }
    for (n, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", n + 1);
    }
    assert_eq!(out.len(), golden.len(), "line count");
}

fn sample(lines: usize) -> Fake {
    let data = (0..lines).map(|i| vec![b'a' + i as u8, 0]).collect();
    Fake::new(10, false, 100.0, 40.0, 0, data)
}

#[test]
fn a_scroll_bar_comes_when_the_lines_do_not_fit() {
    let f = sample(4);
    assert!(f.scroll.is_none(), "4 lines of 10 in 40 fit");
    let mut f = sample(6);
    assert_eq!(f.scroll, Some((0, 0, 2)));
    assert_eq!(f.s.text.right, f.s.rect.right - SCROLL_BAR_WIDTH);
    f.data.truncate(3);
    f.run(|l| l.check_scroll_bar());
    assert!(f.s.f.scroll_bar_removed, "the next CopyData deletes it");
}

#[test]
fn the_arrow_keys_scroll_to_the_selection() {
    let mut f = sample(8);
    for _ in 0..5 {
        f.run(|l| l.down());
    }
    assert_eq!((f.s.f.current_selection, f.scroll.unwrap().0), (5, 2));
    f.run(|l| l.home());
    assert_eq!((f.s.f.current_selection, f.scroll.unwrap().0), (0, 0));
    f.run(|l| l.end());
    assert_eq!((f.s.f.current_selection, f.scroll.unwrap().0), (7, 4));
}

#[test]
fn deleting_above_the_selection_keeps_it_on_its_line() {
    let mut f = sample(5);
    f.run(|l| l.change_selection_index(3, false, false));
    f.data.remove(1);
    f.run(|l| l.deleted_line(1));
    assert_eq!(f.s.f.current_selection, 2);
    f.data.truncate(2);
    f.run(|l| l.deleted_line(4));
    assert_eq!(f.s.f.current_selection, 1, "a selection one past the end steps back");
}

#[test]
fn a_double_click_outside_the_text_is_not_handled() {
    let mut f = sample(3);
    f.mouse = UiPoint { x: 500.0, y: 500.0 };
    assert!(!f.run(|l| l.double_pressed()));
    f.mouse = UiPoint { x: 20.0, y: 41.0 };
    assert!(f.run(|l| l.double_pressed()));
    assert_eq!(f.s.f.current_selection, 2);
}
