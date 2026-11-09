//! The combo box against the real C++ (`golden_combo.txt`): a scratch driver built over the original
//! `UIComboBox.cpp` with driver classes for its parts (text label, edit box, list box, drop button; every call
//! traced) printed five combo boxes under 2,000 seeded operations. The replay below is the same world; the same driver
//! over the forwarders and the new `ee_rust.lib` prints the same file.
#![allow(clippy::unnecessary_cast, clippy::manual_dangling_ptr, clippy::too_many_arguments)]

use super::*;
use crate::text::SingleByte;

const GOLDEN: &str = include_str!("../golden_combo.txt");
const WORDS: [&str; 6] = ["", "Alpha", "Beta", "Gamma", "alpha", "Delta"];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rs(r: &UiRect) -> String {
    format!(" {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn show(t: &[u8]) -> String {
    String::from_utf8_lossy(&t[..t.iter().position(|&b| b == 0).unwrap_or(t.len())]).into_owned()
}
fn word(i: usize) -> Vec<u8> {
    crate::text::from_bytes(WORDS[i].as_bytes())
}
fn moved(r: UiRect, at: UiPoint) -> UiRect {
    let (dx, dy) = (at.x - r.left, at.y - r.top);
    UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy)
}

#[derive(Clone, Copy)]
struct Colours {
    fg_red: f32,
    bg_blue: f32,
    transparent: bool,
}

struct Fake {
    f: ComboFields,
    rect: UiRect,
    index: c_ulong,
    label: bool,
    /// text box, drop button, list
    parts: [UiRect; 3],
    border: c_ulong,
    data: Vec<Vec<u8>>,
    sel: c_ulong,
    has_sel: bool,
    sb_captured: bool,
    shown: Vec<u8>,
    text_colours: Colours,
    list_colours: Colours,
    // the environment
    mouse: UiPoint,
    capture: bool,
    keys: [bool; 5],
    tabbing: bool,
    stop: c_ulong,
    log: Vec<String>,
}

impl Fake {
    fn name(&self, part: ComboPart) -> &'static str {
        match part {
            ComboPart::TextBox => {
                if self.label {
                    "label"
                } else {
                    "edit"
                }
            }
            ComboPart::Label => "label",
            ComboPart::Edit => "edit",
            ComboPart::Drop => "drop",
            ComboPart::List => "list",
        }
    }
    fn slot(part: ComboPart) -> usize {
        match part {
            ComboPart::Drop => 1,
            ComboPart::List => 2,
            _ => 0,
        }
    }
    fn say(&mut self, s: String) {
        self.log.push(s);
    }
}

impl ComboHost for Fake {
    fn load(&self) -> ComboState {
        ComboState { f: self.f, rect: self.rect, index: self.index }
    }
    fn store(&mut self, f: &ComboFields) {
        self.f = *f;
    }
    fn has_label(&self) -> bool {
        self.label
    }
    fn rect_of(&self, part: ComboPart) -> UiRect {
        self.parts[Self::slot(part)]
    }
    fn text_box_border(&self) -> c_ulong {
        self.border
    }
    fn line_count(&self) -> c_ulong {
        self.data.len() as c_ulong
    }
    fn line(&self, i: c_ulong) -> Vec<u8> {
        self.data[i as usize].clone()
    }
    fn line_height(&self) -> c_ulong {
        13
    }
    fn list_border(&self) -> c_ulong {
        self.border
    }
    fn list_scroll_bar_rect(&self) -> Option<UiRect> {
        None
    }
    fn list_scroll_bar_captured(&self) -> bool {
        self.sb_captured
    }
    fn list_selection(&self, out: &mut c_ulong) -> c_long {
        *out = self.sel;
        if (self.sel as usize) < self.data.len() && self.has_sel {
            0
        } else {
            0xd
        }
    }
    fn list_change_selection(&mut self, i: c_ulong, fire: bool, scroll: bool) {
        self.say(format!("  list changeselection {i} {} {}", fire as u8, scroll as u8));
        self.sel = i;
        self.has_sel = true;
    }
    fn list_insert_line(&mut self, text: &[u8], i: c_ulong) {
        self.say(format!("  list insertline [{}] {i}", show(text)));
        self.data.insert(i as usize, text.to_vec());
    }
    fn list_delete_line(&mut self, i: c_ulong) {
        self.say(format!("  list deleteline {i}"));
        if (i as usize) < self.data.len() {
            self.data.remove(i as usize);
        }
        if self.sel as usize >= self.data.len() {
            self.has_sel = false;
        }
    }
    fn list_delete_all(&mut self) {
        self.say("  list deleteall".into());
        self.data.clear();
        self.has_sel = false;
    }
    fn list_change_width(&mut self, w: c_ulong) {
        self.say(format!("  list changewidth {w}"));
    }
    fn window_height(&self) -> c_ulong {
        600
    }
    fn edit_text(&self) -> Vec<u8> {
        self.shown.clone()
    }
    fn show_line(&mut self, i: c_ulong) {
        let t = self.data[i as usize].clone();
        if self.label {
            self.say(format!("  label settext [{}] 1", show(&t)));
        } else {
            self.say(format!("  edit settext [{}]", show(&t)));
        }
        self.shown = t;
    }
    fn clear_text(&mut self) {
        if self.label {
            self.say("  label settext [] 1".into());
        } else {
            self.say("  edit settext []".into());
        }
        self.shown = crate::text::empty();
    }
    fn delete_text(&mut self) {
        if self.label {
            self.say("  label deleteline 0".into());
        } else {
            self.say("  edit deletetext".into());
        }
        self.shown = crate::text::empty();
    }
    fn part_mouse_down(&mut self, part: ComboPart) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} ldown"));
        if part == ComboPart::Drop {
            21
        } else {
            11
        }
    }
    fn part_mouse_up(&mut self, part: ComboPart) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} lup"));
        if part == ComboPart::Drop {
            22
        } else {
            12
        }
    }
    fn part_mouse_move(&mut self, part: ComboPart, at: UiPoint) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} move {} {}", h(at.x), h(at.y)));
        if part == ComboPart::Drop {
            23
        } else {
            13
        }
    }
    fn list_wheel(&mut self, delta: c_long) -> c_long {
        self.say(format!("  list wheel {delta}"));
        17
    }
    fn part_update_keyboard(&mut self, part: ComboPart) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} keyboard"));
        14
    }
    fn part_translated_key(&mut self, part: ComboPart) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} translated [k] 2"));
        15
    }
    fn part_tabbed(&mut self, part: ComboPart, to: bool) {
        let n = self.name(part);
        self.say(format!("  {n} {}", if to { "tabbedto" } else { "tabbedfrom" }));
    }
    fn part_set_rectangle(&mut self, part: ComboPart, r: UiRect) {
        let n = self.name(part);
        self.say(format!("  {n} setrectangle{}", rs(&r)));
        self.parts[Self::slot(part)] = r;
    }
    fn part_set_location(&mut self, part: ComboPart, at: UiPoint) {
        let n = self.name(part);
        self.say(format!("  {n} setlocation {} {}", h(at.x), h(at.y)));
        let s = Self::slot(part);
        self.parts[s] = moved(self.parts[s], at);
    }
    fn part_copy_data(&mut self, part: ComboPart) -> c_long {
        let n = self.name(part);
        self.say(format!("  {n} copy"));
        if part == ComboPart::Drop {
            24
        } else {
            16
        }
    }
    fn copy_list_colours(&mut self) {
        self.list_colours = self.text_colours;
    }
    fn set_focus_rect(&mut self, r: UiRect) {
        self.say(format!("  rhombus setrectangle{}", rs(&r)));
    }
    fn base_copy_data(&mut self) -> c_long {
        self.say("  base-copy combo".into());
        7
    }
    fn tabbing_on(&self) -> bool {
        self.tabbing
    }
    fn tab_stop(&self) -> c_ulong {
        self.stop
    }
    fn set_popup(&mut self, this: bool) {
        self.say(format!("  setpopup {}", if this { "combo" } else { "null" }));
    }
    fn set_dirty(&mut self) {
        self.say("  dirty combo".into());
    }
    fn set_rectangle(&mut self, r: UiRect) {
        self.base_set_rectangle(r);
    }
    fn base_set_rectangle(&mut self, r: UiRect) {
        self.say(format!("  base-setrectangle combo{}", rs(&r)));
        self.rect = r;
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.rect = moved(self.rect, at);
        self.say(format!("  base-setlocation combo {} {}", h(at.x), h(at.y)));
    }
    fn set_always_dirty(&mut self, _on: bool) {}
    fn fire(&mut self, event: c_ulong) {
        self.say(format!("  fire combo {event:x}"));
    }
    fn capture_mouse(&mut self) {
        self.say("  capture-mouse combo".into());
    }
    fn capture_keyboard(&mut self) {
        self.say("  capture-keyboard combo".into());
    }
    fn release_mouse(&mut self) {
        self.say("  release-mouse combo".into());
    }
    fn release_keyboard(&mut self) {
        self.say("  release-keyboard combo".into());
    }
    fn has_mouse_capture(&self) -> bool {
        self.capture
    }
    fn mouse_location(&self) -> UiPoint {
        self.mouse
    }
    fn key(&self, key: ComboKey) -> bool {
        self.keys[key as usize]
    }
    fn open_list(&mut self) {
        open_list_box(self);
    }
    fn close_list(&mut self) {
        close_list_box(self);
    }
    fn add_string(&mut self) -> c_long {
        add_string(&SingleByte, self)
    }
    fn change_selection(&mut self) {
        change_selection(self);
    }
    fn process_arrow(&mut self, _down: bool) {
        process_arrow(self);
    }
}

fn dump(i: usize, c: &Fake) -> String {
    format!(
        "state {i} type {} sel {:x} max {} open {} once {} copen {} always {} cfocus {}{} |{} |{} |{} | lines {} sel {} {} ltext {} {} {} shown [{}]",
        c.f.text_box_type,
        c.f.current_selection as u32,
        c.f.max_list_box_height,
        c.f.is_list_box_open as u8,
        c.f.released_once as u8,
        c.f.copied_list_box_open as u8,
        c.f.list_box_always_dirty as u8,
        c.f.copied_has_focus as u8,
        rs(&c.rect),
        rs(&c.parts[0]),
        rs(&c.parts[1]),
        rs(&c.parts[2]),
        c.data.len(),
        c.sel,
        c.has_sel as u8,
        h(c.list_colours.fg_red),
        h(c.list_colours.bg_blue),
        c.list_colours.transparent as u8,
        show(&c.shown)
    )
}

/// The C++ constructor, with the fakes' trace (every part is still "new" to the driver).
#[allow(clippy::too_many_arguments)]
fn construct(
    ty: c_ulong,
    given: UiRect,
    max: c_ulong,
    index: c_ulong,
    border: c_ulong,
    lines: usize,
    out: &mut Vec<String>,
) -> Fake {
    let control = given;
    let drop = drop_rect(&control, &given);
    out.push(format!("  scaled-bitmap DN{}", rs(&drop)));
    out.push(format!("  scaled-bitmap DP{}", rs(&drop)));
    out.push(format!("  button-new  {}", rs(&drop)));
    let drop_width = crate::rect::um_floor(drop.width() + 1.0) as c_ulong;
    let tb = text_rect(&control, &given, drop_width);
    let label = ty == 1;
    if label {
        out.push(format!("  label-new [] 0 {index} {border}{}", rs(&tb)));
    } else {
        out.push(format!("  edit-new 32 0 {index} {border} 0 0{}", rs(&tb)));
    }
    let list = list_rect(&control, tb.bottom, max);
    out.push(format!("  list-new {lines} 1 {index} {border}{}", rs(&list)));
    let text_colours = if label {
        Colours { fg_red: 0.25, bg_blue: 1.0, transparent: true }
    } else {
        Colours { fg_red: 1.0, bg_blue: 0.5, transparent: false }
    };
    let mut c = Fake {
        f: ComboFields {
            text_box: 1 as *mut c_void,
            text_box_type: ty,
            current_selection: c_ulong::MAX,
            max_list_box_height: max,
            is_list_box_open: false,
            released_once: false,
            copied_list_box_open: false,
            list_box_always_dirty: false,
            copied_has_focus: false,
        },
        rect: control,
        index,
        label,
        parts: [tb, drop, list],
        border,
        data: (0..lines).map(|j| word(1 + j % 5)).collect(),
        sel: 0,
        has_sel: false,
        sb_captured: false,
        shown: crate::text::empty(),
        text_colours,
        list_colours: Colours { fg_red: 1.0, bg_blue: 1.0, transparent: false },
        mouse: UiPoint::default(),
        capture: false,
        keys: [false; 5],
        tabbing: false,
        stop: 0,
        log: Vec::new(),
    };
    c.list_change_selection(0, true, false);
    out.append(&mut c.log);
    out.push("  new setbgopacity".into());
    out.push("  new setbgcolor".into());
    out.push("  new setscrollbgopacity".into());
    out.push("  new setscrollbgcolor".into());
    if !c.data.is_empty() {
        c.show_line(0);
        out.append(&mut c.log);
        c.f.current_selection = 0;
    }
    let r = UiRect { top: tb.top, left: tb.left, right: drop.right, bottom: drop.bottom };
    out.push(format!("  base-setrectangle new{}", rs(&r)));
    c.rect = r;
    out.push("  rhombus setcolor 3f000000 3e800000 3e000000".into());
    c
}

#[test]
fn replays_the_cpp_trace_line_for_line() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut out: Vec<String> = Vec::new();
    let mut combos: Vec<Fake> = Vec::new();
    let mut env: Vec<&str> = Vec::new();
    let mut mouse = UiPoint::default();
    let mut capture = false;
    let mut keys = [false; 5];
    let mut tabbing = false;
    let mut stop: c_ulong = 0;
    for (n, line) in golden.iter().enumerate() {
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "new" => {
                out.push(line.to_string());
                let r = UiRect::new(fx(t[4]), fx(t[5]), fx(t[6]), fx(t[7]));
                let c = construct(
                    t[3].parse().unwrap(),
                    r,
                    t[9].parse().unwrap(),
                    t[11].parse().unwrap(),
                    t[13].parse().unwrap(),
                    t[15].parse().unwrap(),
                    &mut out,
                );
                out.push(dump(combos.len(), &c));
                combos.push(c);
            }
            "env" => {
                out.push(line.to_string());
                mouse = UiPoint { x: fx(t[1]), y: fx(t[2]) };
                capture = t[4] == "1";
                for (j, b) in t[6].bytes().enumerate() {
                    keys[j] = b == b'1';
                }
                tabbing = t[8] == "1";
                stop = t[10].parse().unwrap();
                env = t.clone();
            }
            "op" => {
                out.push(line.to_string());
                let i: usize = t[1].parse().unwrap();
                let c = &mut combos[i];
                c.mouse = mouse;
                c.capture = capture;
                c.keys = keys;
                c.tabbing = tabbing;
                c.stop = stop;
                c.sb_captured = env[12] == "1";
                c.f.list_box_always_dirty = env[14] == "1";
                c.has_sel = env[16] == "1";
                let r = match t[2] {
                    "ldown" => mouse_l_button_down(c),
                    "lup" => mouse_l_button_up(c),
                    "move" => mouse_move(c, UiPoint { x: fx(t[3]), y: fx(t[4]) }),
                    "wheel" => mouse_wheel_turned(c, t[3].parse().unwrap()),
                    "keyboard" => update_keyboard(c),
                    "translated" => update_translated_key(c),
                    "tabto" => {
                        tabbed_to(c);
                        0
                    }
                    "tabfrom" => {
                        tabbed_from(c);
                        0
                    }
                    "copy" => copy_data(c),
                    "setlocation" => {
                        set_location(c, UiPoint { x: fx(t[3]), y: fx(t[4]) });
                        0
                    }
                    "changewidth" => {
                        change_width(c, t[3].parse().unwrap());
                        0
                    }
                    "setselection" => {
                        set_current_selection(c, t[3].parse().unwrap());
                        0
                    }
                    "insert" => {
                        insert_line(c, &word(t[3].parse().unwrap()), t[4].parse().unwrap());
                        0
                    }
                    "deleteline" => {
                        delete_line(c, t[3].parse().unwrap());
                        0
                    }
                    "deleteall" => {
                        delete_all_entries(c);
                        0
                    }
                    "append" => {
                        // UIComboBox::AppendLine (C++): SetDirty, the list's AppendLine, then the reopen
                        let w = word(t[3].parse().unwrap());
                        c.set_dirty();
                        c.say(format!("  list appendline [{}]", show(&w)));
                        c.data.push(w);
                        reopen(c);
                        0
                    }
                    "addstring" => {
                        if c.label {
                            0
                        } else {
                            // the driver's "  (edit text W)" line, then the edit box holds that word
                            let next: Vec<&str> = golden[n + 1].split(' ').collect();
                            let w: usize = next[4].trim_end_matches(')').parse().unwrap();
                            c.say(golden[n + 1].to_string());
                            c.shown = word(w);
                            add_string(&SingleByte, c)
                        }
                    }
                    "hide" => {
                        hide_control(c);
                        0
                    }
                    "open" => {
                        open_list_box(c);
                        0
                    }
                    "close" => {
                        close_list_box(c);
                        0
                    }
                    other => panic!("op {other}"),
                };
                out.append(&mut c.log);
                out.push(format!("  = {r}"));
                out.push(dump(i, c));
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
fn the_list_opens_where_there_is_room() {
    let tb = UiRect::new(10.0, 500.0, 100.0, 520.0);
    let drop = UiRect::new(101.0, 500.0, 120.0, 520.0);
    // 10 lines of 13 + 2 + 1 = 133 > 600 - 520 - 1 = 79 below, 499 above: above, at full height
    let (list, rect) = open_rects(&tb, &drop, 10, 13, 1, 400, 600);
    assert_eq!(list, UiRect::new(10.0, 499.0 - 133.0, 120.0, 499.0));
    assert_eq!(rect, UiRect::new(10.0, 366.0, 120.0, 520.0));
    // capped by the maximum, it fits below
    let (list, _) = open_rects(&tb, &drop, 10, 13, 1, 50, 600);
    assert_eq!(list, UiRect::new(10.0, 521.0, 120.0, 571.0));
    // more room below than above: below, shrunk to the room
    let high = UiRect::new(10.0, 40.0, 100.0, 60.0);
    let (list, _) = open_rects(&high, &drop, 100, 13, 0, 2000, 600);
    assert_eq!(list, UiRect::new(10.0, 61.0, 120.0, 61.0 + 539.0));
}

#[test]
fn add_string_selects_or_inserts_and_refuses_empty_text() {
    let mut out = Vec::new();
    let mut c = construct(0, UiRect::new(0.0, 0.0, 100.0, 20.0), 100, 0, 0, 3, &mut out);
    c.shown = word(2);
    assert_eq!(add_string(&SingleByte, &mut c), 0);
    assert_eq!(c.sel, 1);
    c.shown = word(5);
    assert_eq!(add_string(&SingleByte, &mut c), 3, "kErrorNotFound: added");
    assert_eq!(c.data.len(), 4);
    assert_eq!(c.data[0], word(5));
    assert_eq!(c.sel, 0);
    c.shown = word(0);
    assert_eq!(add_string(&SingleByte, &mut c), 9, "kErrorFailed");
    // the comparison is exact: "alpha" is not "Alpha"
    c.shown = word(4);
    assert_eq!(add_string(&SingleByte, &mut c), 3);
}

#[test]
fn the_first_release_after_opening_keeps_the_list_open() {
    let mut out = Vec::new();
    let mut c = construct(1, UiRect::new(10.0, 10.0, 200.0, 30.0), 100, 0, 1, 3, &mut out);
    c.mouse = UiPoint { x: 50.0, y: 20.0 };
    mouse_l_button_down(&mut c);
    assert!(c.f.is_list_box_open);
    c.mouse = UiPoint { x: 500.0, y: 500.0 };
    mouse_l_button_up(&mut c);
    assert!(c.f.is_list_box_open && c.f.released_once);
    mouse_l_button_up(&mut c);
    assert!(!c.f.is_list_box_open, "the second release anywhere commits");
}
