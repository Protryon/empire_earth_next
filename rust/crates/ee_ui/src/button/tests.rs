//! The buttons against the real C++ (`golden_button.txt`): a scratch driver built over the original `UIButton.cpp`,
//! `UIDualButton.cpp`, `UIScrollButton.cpp`, `UICheckBox.cpp` and `UIRadioButton.cpp` (fakes for what they reach:
//! `UIControl`'s out-of-line members, the form's `SetTabStop`, the sounds, keys, polls, slider and text label)
//! printed 12 controls under 1,500 seeded operations: every host-visible call, each touched control's record and the
//! bitmaps' reference counts. The replay below is the same world: the fakes as the driver's, the C++ members that
//! stay C++ (constructors, `SetText`, the sounds, the inline overrides) as the C++ does them, and everything else
//! through this module, dispatched as the vtables do. The same driver over the forwarders and the new `ee_rust.lib`
//! prints the same file.
#![allow(clippy::unnecessary_cast, clippy::manual_dangling_ptr, clippy::too_many_arguments)]

use super::*;

const GOLDEN: &str = include_str!("../golden_button.txt");

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Kind {
    Button,
    Dual,
    Check,
    Radio,
    Scroll,
}

const NAMES: [&str; 17] = [
    "N0", "P0", "F0", "N1", "N2", "P2", "F2", "N9", "P9", "CON", "COFF", "CDON", "CDOFF", "RON", "ROFF", "RDON",
    "RDOFF",
];
const CON: usize = 9;
const COFF: usize = 10;
const CDON: usize = 11;
const CDOFF: usize = 12;
const RON: usize = 13;
const ROFF: usize = 14;
const RDON: usize = 15;
const RDOFF: usize = 16;

/// A bitmap as a record pointer: its index + 1 (NULL: none).
fn bm(i: usize) -> *mut c_void {
    (i + 1) as *mut c_void
}
fn bm_index(p: *mut c_void) -> Option<usize> {
    if p.is_null() {
        None
    } else {
        Some(p as usize - 1)
    }
}
const MARK: *mut c_void = 1 as *mut c_void;

struct Ctl {
    kind: Kind,
    s: ButtonState,
    check: CheckFields,
    radio: RadioFields,
    decrement: c_ulong,
    delay: bool,
    repeat: bool,
    /// `mText`'s text (with `mText` set), and `mCopiedText`'s colour.
    text: Option<Vec<u8>>,
    copied_fg: UiColor,
}

struct World {
    ctls: Vec<Ctl>,
    refs: Vec<i64>,
    tab_stop: c_ulong,
    tabbing: bool,
    mouse: UiPoint,
    capture: bool,
    right: bool,
    keys: [bool; 4],
    polls: [bool; 2],
    slider: c_long,
    next_sound: c_ulong,
    log: Vec<String>,
}

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn show(t: &[u8]) -> String {
    String::from_utf8_lossy(&t[..t.iter().position(|&b| b == 0).unwrap_or(t.len())]).into_owned()
}

impl World {
    fn reference(&mut self, p: *mut c_void) {
        if let Some(i) = bm_index(p) {
            self.refs[i] += 1;
        }
    }
    fn dereference(&mut self, p: *mut c_void) {
        if let Some(i) = bm_index(p) {
            self.refs[i] -= 1;
        }
    }
    fn dirty(&mut self, i: usize) {
        let idx = self.ctls[i].s.index;
        self.log.push(format!("  dirty {idx} 1"));
    }
    /// `SetNormalBitmap` / `SetPressedBitmap` (header inlines).
    fn set_normal(&mut self, i: usize, b: *mut c_void) {
        let old = self.ctls[i].s.f.normal_bitmap;
        self.dereference(old);
        self.ctls[i].s.f.normal_bitmap = b;
        self.reference(b);
        self.dirty(i);
    }
    fn set_pressed(&mut self, i: usize, b: *mut c_void) {
        let old = self.ctls[i].s.f.pressed_bitmap;
        self.dereference(old);
        self.ctls[i].s.f.pressed_bitmap = b;
        self.reference(b);
        self.dirty(i);
    }
}

/// One control seen from the logic, as the C++ forwarders give it (`ctx`: a `SetText`'s text).
struct Host<'w> {
    w: &'w mut World,
    i: usize,
    text: Vec<u8>,
}

fn host(w: &mut World, i: usize) -> Host<'_> {
    Host { w, i, text: crate::text::empty() }
}

impl Host<'_> {
    fn c(&mut self) -> &mut Ctl {
        &mut self.w.ctls[self.i]
    }
    fn idx(&self) -> c_ulong {
        self.w.ctls[self.i].s.index
    }
    fn push(&mut self, s: String) {
        self.w.log.push(s);
    }
}

impl ButtonHost for Host<'_> {
    fn load(&self) -> ButtonState {
        self.w.ctls[self.i].s
    }
    fn store(&mut self, s: &ButtonState) {
        let c = self.c();
        c.s.f = s.f;
        c.s.border_type = s.border_type;
    }
    fn tab_stop(&self) -> c_ulong {
        self.w.tab_stop
    }
    fn tabbing_on(&self) -> bool {
        self.w.tabbing
    }
    fn set_tab_stop(&mut self, index: c_ulong) {
        self.push(format!("  settabstop {index}"));
        self.w.tab_stop = index;
    }
    fn tabable(&self) -> bool {
        let c = &self.w.ctls[self.i];
        match c.kind {
            Kind::Scroll => false,
            Kind::Radio => !c.radio.is_push_button,
            _ => true,
        }
    }
    fn set_dirty(&mut self) {
        self.w.dirty(self.i);
    }
    fn play(&mut self, sound: c_ulong) {
        self.push(format!("  play {sound}"));
    }
    fn capture_mouse(&mut self) {
        let i = self.idx();
        self.push(format!("  capture-mouse {i}"));
    }
    fn release_mouse(&mut self) {
        let i = self.idx();
        self.push(format!("  release-mouse {i}"));
    }
    fn capture_keyboard(&mut self) {
        let i = self.idx();
        self.push(format!("  capture-keyboard {i}"));
    }
    fn release_keyboard(&mut self) {
        let i = self.idx();
        self.push(format!("  release-keyboard {i}"));
    }
    fn fire(&mut self, event: c_ulong) {
        let i = self.idx();
        self.push(format!("  fire {i} {event:x}"));
    }
    fn mouse_location(&self) -> UiPoint {
        self.w.mouse
    }
    fn has_mouse_capture(&self) -> bool {
        self.w.capture
    }
    fn right_button_down(&self) -> bool {
        self.w.right
    }
    fn key(&mut self, key: Key) -> bool {
        let (n, what, v) = match key {
            Key::SpaceJustPressed => ("space", "pressed", self.w.keys[0]),
            Key::EnterJustPressed => ("enter", "pressed", self.w.keys[1]),
            Key::SpaceJustUnpressed => ("space", "unpressed", self.w.keys[2]),
            Key::EnterJustUnpressed => ("enter", "unpressed", self.w.keys[3]),
        };
        self.push(format!("  key {n}-{what} {}", v as u8));
        v
    }
    fn button_down(&mut self) {
        v_button_down(self.w, self.i);
    }
    fn button_up(&mut self) {
        v_button_up(self.w, self.i);
    }
    fn mouse_over(&mut self) {
        mouse_over(&mut host(self.w, self.i));
    }
    fn mouse_move(&mut self, at: UiPoint) {
        v_mouse_move(self.w, self.i, at);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        let c = self.c();
        let r = c.s.rect;
        let (dx, dy) = (at.x - r.left, at.y - r.top);
        c.s.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
        let i = self.idx();
        self.push(format!("  base-setlocation {i} {} {}", h(at.x), h(at.y)));
        self.set_dirty();
    }
    fn base_copy_data(&mut self) -> c_long {
        let i = self.idx();
        self.push(format!("  base-copy {i}"));
        7
    }
    fn set_copied_bitmap(&mut self, bitmap: *mut c_void) {
        let old = self.c().s.f.copied_bitmap;
        self.w.dereference(old);
        self.c().s.f.copied_bitmap = bitmap;
        self.w.reference(bitmap);
    }
    fn text(&self) -> Vec<u8> {
        self.w.ctls[self.i].text.clone().unwrap()
    }
    fn text_extent(&self) -> (f32, f32) {
        let t = self.text();
        (6.5 * crate::text::char_len(&crate::text::SingleByte, &t) as u32 as f32, 13.0)
    }
    fn copy_text(&mut self, focused: bool) {
        // operator= copies mText's colour (the default white), then the focused yellow
        let c = self.c();
        c.copied_fg = if focused { c.s.f.focused_text_color } else { UiColor { red: 1.0, green: 1.0, blue: 1.0 } };
    }
}

impl ScrollButtonHost for Host<'_> {
    fn decrement(&self) -> c_ulong {
        self.w.ctls[self.i].decrement
    }
    fn delay_running(&self) -> bool {
        self.w.ctls[self.i].delay
    }
    fn delay_poll(&mut self) -> bool {
        let r = self.w.polls[0];
        self.push(format!("  delay-poll {}", r as u8));
        r
    }
    fn delay_start(&mut self) {
        self.push("  delay-start".into());
        self.c().delay = true;
    }
    fn delay_stop(&mut self) {
        self.c().delay = false;
    }
    fn repeat_running(&self) -> bool {
        self.w.ctls[self.i].repeat
    }
    fn repeat_poll(&mut self) -> bool {
        let r = self.w.polls[1];
        self.push(format!("  repeat-poll {}", r as u8));
        r
    }
    fn repeat_start(&mut self) {
        self.push("  repeat-start".into());
        self.c().repeat = true;
    }
    fn slider_value(&self) -> c_long {
        self.w.slider
    }
    fn set_slider_value(&mut self, value: c_long) {
        self.push(format!("  slider-set {value}"));
        self.w.slider = value;
    }
}

impl Host<'_> {
    fn set_label(&mut self, on: bool) {
        let p = if on { MARK } else { core::ptr::null_mut() };
        let c = self.c();
        if c.kind == Kind::Check {
            c.check.text_label = p;
        } else {
            c.radio.text_label = p;
        }
    }
}

impl LabelHost for Host<'_> {
    fn label_set_text(&mut self) {
        let i = self.idx();
        let t = show(&self.text);
        self.push(format!("  label-settext {i} [{t}] 1"));
    }
    fn label_create(&mut self, r: UiRect) {
        let i = self.idx();
        let t = show(&self.text);
        self.push(format!("  label-new {i} [{t}] {} {} {} {} 0 0", h(r.left), h(r.top), h(r.right), h(r.bottom)));
        self.set_label(true);
    }
    fn label_set_location(&mut self, at: UiPoint) {
        let i = self.idx();
        self.push(format!("  label-setlocation {i} {} {}", h(at.x), h(at.y)));
    }
}

impl CheckHost for Host<'_> {
    fn load_check(&self) -> CheckFields {
        self.w.ctls[self.i].check
    }
    fn store_check(&mut self, c: &CheckFields) {
        self.c().check = *c;
    }
    fn set_check_bitmaps(&mut self, on: bool) {
        let i = self.i;
        self.w.set_normal(i, bm(if on { CON } else { COFF }));
        self.w.set_pressed(i, bm(if on { CDON } else { CDOFF }));
    }
}

impl RadioHost for Host<'_> {
    fn load_radio(&self) -> RadioFields {
        self.w.ctls[self.i].radio
    }
    fn store_radio(&mut self, r: &RadioFields) {
        self.c().radio = *r;
    }
    fn swap_bitmaps(&mut self) {
        let i = self.i;
        let w = &mut *self.w;
        let n = w.ctls[i].s.f.normal_bitmap;
        w.reference(n);
        let p = w.ctls[i].s.f.pressed_bitmap;
        w.reference(p);
        let on = w.ctls[i].radio.other_normal_bitmap;
        w.set_normal(i, on);
        let op = w.ctls[i].radio.other_pressed_bitmap;
        w.set_pressed(i, op);
        w.dereference(on);
        w.ctls[i].radio.other_normal_bitmap = n;
        w.reference(n);
        w.dereference(op);
        w.ctls[i].radio.other_pressed_bitmap = p;
        w.reference(p);
        w.dirty(i);
        w.dereference(n);
        w.dereference(p);
    }
    fn control_count(&self) -> c_long {
        self.w.ctls.len() as c_long
    }
    fn is_radio(&self, i: c_long) -> bool {
        self.w.ctls[i as usize].kind == Kind::Radio
    }
    fn radio_checked(&self, i: c_long) -> bool {
        self.w.ctls[i as usize].radio.is_checked
    }
    fn uncheck_radio(&mut self, i: c_long) {
        radio_uncheck(&mut host(self.w, i as usize));
    }
}

// --- the vtables ---------------------------------------------------------------------------------------------------

fn v_button_down(w: &mut World, i: usize) {
    let mut hh = host(w, i);
    match hh.w.ctls[i].kind {
        Kind::Check => {
            if check_enabled(&hh) {
                button_down(&mut hh);
            }
        }
        Kind::Scroll => scroll_button_down(&mut hh),
        _ => button_down(&mut hh),
    }
}

fn v_button_up(w: &mut World, i: usize) {
    let kind = w.ctls[i].kind;
    let mut hh = host(w, i);
    match kind {
        Kind::Check => check_toggle(&mut hh),
        Kind::Radio => radio_check(&mut hh),
        Kind::Scroll => {
            hh.c().repeat = false;
            hh.c().delay = false;
        }
        _ => {}
    }
    button_up(&mut hh);
}

fn v_mouse_move(w: &mut World, i: usize, at: UiPoint) -> c_long {
    let mut hh = host(w, i);
    if hh.w.ctls[i].kind == Kind::Check && !check_enabled(&hh) {
        return 0;
    }
    mouse_move(&mut hh, at)
}

/// `CopyData`'s label part (check box and radio button).
fn label_copy(w: &mut World, i: usize, mut r: c_long) -> c_long {
    let c = &mut w.ctls[i];
    let (has_text, label) = if c.kind == Kind::Check {
        (c.check.has_text, &mut c.check.text_label)
    } else {
        (c.radio.has_text, &mut c.radio.text_label)
    };
    let idx = c.s.index;
    if has_text {
        w.log.push(format!("  label-copy {idx}"));
        r = 3;
    } else if !label.is_null() {
        *label = core::ptr::null_mut();
        w.log.push(format!("  label-delete {idx}"));
    }
    r
}

// --- construction (the C++ constructors) ---------------------------------------------------------------------------

fn blank_check() -> CheckFields {
    CheckFields {
        text_label: core::ptr::null_mut(),
        is_checked: false,
        has_text: false,
        scale_bitmap: false,
        is_disabled: false,
    }
}
fn blank_radio() -> RadioFields {
    RadioFields {
        other_normal_bitmap: core::ptr::null_mut(),
        other_pressed_bitmap: core::ptr::null_mut(),
        text_label: core::ptr::null_mut(),
        is_checked: false,
        has_text: false,
        is_push_button: false,
        scale_bitmap: false,
    }
}

/// `UIButton`'s constructor (after the fake `UIControl`'s).
fn new_button(
    w: &mut World,
    kind: Kind,
    normal: Option<usize>,
    focused: Option<usize>,
    pressed: Option<usize>,
    rect: UiRect,
    index: c_ulong,
    border: c_ulong,
) -> usize {
    let mut f = init(&rect, border);
    let p = |o: Option<usize>| o.map_or(core::ptr::null_mut(), bm);
    f.normal_bitmap = p(normal);
    f.pressed_bitmap = if pressed.is_some() { p(pressed) } else { f.normal_bitmap };
    f.focused_bitmap = p(focused);
    w.reference(f.normal_bitmap);
    w.reference(f.pressed_bitmap);
    w.reference(f.focused_bitmap);
    w.ctls.push(Ctl {
        kind,
        s: ButtonState { f, border_type: 1, rect, border, index },
        check: blank_check(),
        radio: blank_radio(),
        decrement: 0,
        delay: false,
        repeat: false,
        text: None,
        copied_fg: UiColor { red: 1.0, green: 1.0, blue: 1.0 },
    });
    w.ctls.len() - 1
}

fn r(l: f32, t: f32, rr: f32, b: f32) -> UiRect {
    UiRect::new(l, t, rr, b)
}

fn set_offset(w: &mut World, i: usize, height: f32, border: c_ulong, scale: bool, float_border: bool) {
    if let Some(p) = label_bitmap_offset(height, border, scale, float_border) {
        w.ctls[i].s.f.bitmap_draw_offset = p;
    }
}

fn new_check(w: &mut World, text: &str, rect: UiRect, index: c_ulong, border: c_ulong, scale: bool) {
    let i = new_button(w, Kind::Check, Some(COFF), None, Some(CDOFF), box_rect(&rect, scale), index, border);
    w.ctls[i].check.scale_bitmap = scale;
    set_offset(w, i, rect.height(), border, scale, true);
    let t = crate::text::from_bytes(text.as_bytes());
    let mut hh = Host { w, i, text: t.clone() };
    check_set_text(&mut hh, &t);
}

#[allow(clippy::too_many_arguments)]
fn new_radio(
    w: &mut World,
    text: &str,
    rect: UiRect,
    index: c_ulong,
    border: c_ulong,
    push: bool,
    checked: bool,
    scale: bool,
) {
    let i = new_button(w, Kind::Radio, Some(ROFF), None, Some(RDOFF), box_rect(&rect, scale), index, border);
    {
        let c = &mut w.ctls[i];
        c.radio.is_push_button = push;
        c.radio.is_checked = checked;
        c.radio.scale_bitmap = scale;
        c.radio.other_normal_bitmap = bm(RON);
        c.radio.other_pressed_bitmap = bm(RDON);
    }
    w.reference(bm(RON));
    w.reference(bm(RDON));
    if checked {
        // the constructor's swap: no NULL checks, no extra SetDirty
        let n = w.ctls[i].s.f.normal_bitmap;
        w.reference(n);
        let p = w.ctls[i].s.f.pressed_bitmap;
        w.reference(p);
        let on = w.ctls[i].radio.other_normal_bitmap;
        w.set_normal(i, on);
        let op = w.ctls[i].radio.other_pressed_bitmap;
        w.set_pressed(i, op);
        w.dereference(on);
        w.ctls[i].radio.other_normal_bitmap = n;
        w.reference(n);
        w.dereference(op);
        w.ctls[i].radio.other_pressed_bitmap = p;
        w.reference(p);
        w.dereference(n);
        w.dereference(p);
    }
    set_offset(w, i, rect.height(), border, scale, false);
    let t = crate::text::from_bytes(text.as_bytes());
    let mut hh = Host { w, i, text: t.clone() };
    radio_set_text(&mut hh, &t);
}

fn new_scroll(w: &mut World, normal: usize, pressed: Option<usize>, x: f32, y: f32, dec: c_ulong, index: c_ulong) {
    let i = new_button(w, Kind::Scroll, Some(normal), None, pressed, r(x, y, x + 19.0, y + 19.0), index, 0);
    w.ctls[i].decrement = dec;
}

fn construct(w: &mut World, n: usize) {
    match n {
        0 => drop(new_button(w, Kind::Button, Some(0), Some(2), Some(1), r(10.0, 10.0, 90.0, 30.0), 0, 2)),
        1 => drop(new_button(w, Kind::Button, Some(3), None, None, r(100.0, 10.0, 160.5, 34.0), 1, 0)),
        2 => drop(new_button(w, Kind::Dual, Some(4), Some(6), Some(5), r(10.0, 40.0, 50.0, 60.0), 2, 1)),
        3 => new_check(w, "Check", r(10.0, 100.0, 200.0, 130.0), 3, 1, false),
        4 => new_check(w, "", r(10.0, 140.0, 25.0, 155.0), 4, 3, true),
        5 => new_radio(w, "One", r(10.0, 160.0, 18.0, 170.0), 5, 0, false, true, false),
        6 => new_radio(w, "Two", r(10.0, 180.0, 120.0, 205.0), 6, 2, true, false, true),
        7 => new_radio(w, "", r(10.0, 210.0, 50.0, 240.0), 7, 1, false, false, false),
        8 => new_scroll(w, 7, Some(8), 300.0, 10.0, 1, 8),
        9 => new_scroll(w, 7, None, 300.0, 200.0, 0, 9),
        10 => drop(new_button(w, Kind::Button, Some(0), None, Some(1), r(10.0, 300.0, 110.0, 330.0), 10, 3)),
        11 => new_radio(w, "Lone", r(10.0, 340.0, 30.0, 360.0), 11, 0, false, false, false),
        _ => panic!("control {n}"),
    }
}

// --- the dumps (the driver's format) -------------------------------------------------------------------------------

fn rect_s(what: &str, r: &UiRect) -> String {
    format!(" {what} {} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}
fn name(p: *mut c_void) -> &'static str {
    bm_index(p).map_or("-", |i| NAMES[i])
}

fn dump(w: &World, i: usize) -> String {
    let c = &w.ctls[i];
    let f = &c.s.f;
    let kind = ["button", "dual", "check", "radio", "scroll"][c.kind as usize];
    let mut s = format!(
        "state {i} {kind} st {} bt {} kd {} md {} hct {}",
        f.state, c.s.border_type, f.key_down as u8, f.mouse_down as u8, f.has_copied_text as u8
    );
    s += &rect_s("rect", &c.s.rect);
    s += &rect_s("text", &f.text_rect);
    s += &rect_s("ctext", &f.copied_text_rect);
    s += &format!(
        " bl {} {} off {} {} bm {}/{}/{}/{} snd {:x} {:x}",
        h(f.bitmap_location.x),
        h(f.bitmap_location.y),
        h(f.bitmap_draw_offset.x),
        h(f.bitmap_draw_offset.y),
        name(f.normal_bitmap),
        name(f.pressed_bitmap),
        name(f.focused_bitmap),
        name(f.copied_bitmap),
        f.button_down_sound as u32,
        f.button_up_sound as u32
    );
    if let Some(t) = &c.text {
        s += &format!(" txt [{}] fg {} {} {}", show(t), h(c.copied_fg.red), h(c.copied_fg.green), h(c.copied_fg.blue));
    }
    match c.kind {
        Kind::Check => {
            let k = &c.check;
            s += &format!(
                " | chk {} txt {} scale {} dis {} label {}",
                k.is_checked as u8,
                k.has_text as u8,
                k.scale_bitmap as u8,
                k.is_disabled as u8,
                !k.text_label.is_null() as u8
            );
        }
        Kind::Radio => {
            let k = &c.radio;
            s += &format!(
                " | chk {} txt {} push {} scale {} label {} other {}/{}",
                k.is_checked as u8,
                k.has_text as u8,
                k.is_push_button as u8,
                k.scale_bitmap as u8,
                !k.text_label.is_null() as u8,
                name(k.other_normal_bitmap),
                name(k.other_pressed_bitmap)
            );
        }
        Kind::Scroll => {
            s += &format!(
                " | dec {} delay {} repeat {} slider {}",
                c.decrement, c.delay as u8, c.repeat as u8, w.slider
            );
        }
        _ => {}
    }
    s
}

fn world_line(w: &World) -> String {
    let mut s = format!("world stop {} tab {} radios", w.tab_stop, w.tabbing as u8);
    for (i, c) in w.ctls.iter().enumerate() {
        if c.kind == Kind::Radio {
            s += &format!(" {i}:{}", c.radio.is_checked as u8);
        }
    }
    s += " refs";
    for r in &w.refs {
        s += &format!(" {r}");
    }
    s
}

const TEXTS: [&str; 4] = ["", "OK", "A longer label", "x"];

/// One driver operation on control `i`.
fn op(w: &mut World, i: usize, t: &[&str]) -> c_long {
    let kind = w.ctls[i].kind;
    match t[2] {
        "ldown" => {
            let mut hh = host(w, i);
            match kind {
                Kind::Check if !check_enabled(&hh) => 0,
                Kind::Radio => radio_mouse_l_button_down(&mut hh),
                _ => mouse_l_button_down(&mut hh),
            }
        }
        "lup" => {
            let mut hh = host(w, i);
            if kind == Kind::Check && !check_enabled(&hh) {
                0
            } else {
                mouse_l_button_up(&mut hh)
            }
        }
        "move" => v_mouse_move(w, i, UiPoint { x: fx(t[3]), y: fx(t[4]) }),
        "rdown" if kind == Kind::Dual => dual_mouse_r_button_down(&mut host(w, i)),
        "rup" if kind == Kind::Dual => dual_mouse_r_button_up(&mut host(w, i)),
        "rdown" | "rup" => 0,
        "tabto" => {
            tabbed_to(&mut host(w, i));
            0
        }
        "tabfrom" => {
            tabbed_from(&mut host(w, i));
            0
        }
        "keyboard" if kind == Kind::Scroll => 0,
        "keyboard" => update_keyboard(&mut host(w, i)),
        "setlocation" => {
            let at = UiPoint { x: fx(t[3]), y: fx(t[4]) };
            let mut hh = host(w, i);
            match kind {
                Kind::Check => check_set_location(&mut hh, at),
                Kind::Radio => radio_set_location(&mut hh, at),
                _ => set_location(&mut hh, at),
            }
            0
        }
        "copy" => match kind {
            Kind::Check => {
                let r = copy_data(&mut host(w, i));
                label_copy(w, i, r)
            }
            Kind::Radio => {
                let r = radio_copy_data(&mut host(w, i));
                label_copy(w, i, r)
            }
            _ => copy_data(&mut host(w, i)),
        },
        "buttondown" => {
            v_button_down(w, i);
            0
        }
        "buttonup" => {
            v_button_up(w, i);
            0
        }
        "over" => {
            mouse_over(&mut host(w, i));
            0
        }
        "buttontext" => {
            // UIButton::SetText (C++): the texts on first use, the text, SetDirty
            let text = crate::text::from_bytes(TEXTS[t[3].parse::<usize>().unwrap()].as_bytes());
            let c = &mut w.ctls[i];
            if c.text.is_none() {
                c.s.f.text = MARK;
                c.s.f.copied_text = MARK;
            }
            c.text = Some(text);
            w.dirty(i);
            0
        }
        "sounds" => {
            // SetButtonDownSound / SetButtonUpSound (C++)
            for up in [false, true] {
                let f = &w.ctls[i].s.f;
                let old = if up { f.button_up_sound } else { f.button_down_sound };
                if old as u32 != u32::MAX {
                    w.log.push(format!("  unload {old}"));
                }
                let n = w.next_sound;
                w.next_sound += 1;
                w.log.push(format!("  load {n}"));
                let f = &mut w.ctls[i].s.f;
                if up {
                    f.button_up_sound = n;
                } else {
                    f.button_down_sound = n;
                }
            }
            0
        }
        "settext" => {
            let text = crate::text::from_bytes(TEXTS[t[3].parse::<usize>().unwrap()].as_bytes());
            let mut hh = Host { w, i, text: text.clone() };
            if kind == Kind::Check {
                check_set_text(&mut hh, &text);
            } else {
                radio_set_text(&mut hh, &text);
            }
            0
        }
        "setchecked" => {
            check_set_checked(&mut host(w, i), t[3] == "1");
            0
        }
        "toggle" => {
            check_toggle(&mut host(w, i));
            0
        }
        "setdisabled" => {
            check_set_disabled(&mut host(w, i), t[3] == "1");
            0
        }
        "check" => {
            radio_check(&mut host(w, i));
            0
        }
        "uncheck" => {
            radio_uncheck(&mut host(w, i));
            0
        }
        "removeother" => {
            radio_remove_other_checks(&mut host(w, i));
            0
        }
        other => panic!("op {other}"),
    }
}

#[test]
fn replays_the_cpp_trace_line_for_line() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut w = World {
        ctls: Vec::new(),
        refs: vec![0; NAMES.len()],
        tab_stop: 0,
        tabbing: false,
        mouse: UiPoint::default(),
        capture: false,
        right: false,
        keys: [false; 4],
        polls: [false; 2],
        slider: 5,
        next_sound: 40,
        log: Vec::new(),
    };
    let mut out: Vec<String> = Vec::new();
    let mut started = false;
    for line in &golden {
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "new" => {
                out.push(line.to_string());
                construct(&mut w, t[1].parse().unwrap());
                out.append(&mut w.log);
            }
            "state" if !started => {
                // the constructors' dumps, then the world: once
                started = true;
                for i in 0..w.ctls.len() {
                    out.push(dump(&w, i));
                }
                out.push(world_line(&w));
            }
            "env" => {
                out.push(line.to_string());
                w.mouse = UiPoint { x: fx(t[1]), y: fx(t[2]) };
                w.capture = t[4] == "1";
                w.right = t[6] == "1";
                for (j, b) in t[8].bytes().enumerate() {
                    w.keys[j] = b == b'1';
                }
                for (j, b) in t[10].bytes().enumerate() {
                    w.polls[j] = b == b'1';
                }
                w.tabbing = t[12] == "1";
                w.tab_stop = t[14].parse().unwrap();
            }
            "op" => {
                out.push(line.to_string());
                let i: usize = t[1].parse().unwrap();
                let r = op(&mut w, i, &t);
                out.append(&mut w.log);
                out.push(format!("  = {r}"));
                out.push(dump(&w, i));
                out.push(world_line(&w));
            }
            _ => {}
        }
    }
    assert!(started);
    assert_eq!(out.len(), golden.len(), "line count");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
}

// --- edge cases beyond the trace -----------------------------------------------------------------------------------

#[test]
fn none32_looks_at_the_low_32_bits() {
    assert!(is_none32(c_ulong::MAX));
    assert!(is_none32(0xffff_ffff as c_ulong));
    assert!(!is_none32(0));
    assert!(!is_none32(41));
}

#[test]
fn box_rect_clamps_to_19_pixels_unless_scaled() {
    let small = UiRect::new(5.0, 6.0, 20.0, 26.0);
    assert_eq!(box_rect(&small, false), UiRect::new(5.0, 6.0, 24.0, 25.0));
    assert_eq!(box_rect(&small, true), small);
    let wide = UiRect::new(0.0, 0.0, 100.0, 10.0);
    assert_eq!(box_rect(&wide, false), UiRect::new(0.0, 0.0, 100.0, 19.0));
    // NaN widths compare false: the box keeps its edge
    let nan = UiRect::new(0.0, 0.0, f32::NAN, 10.0);
    assert!(box_rect(&nan, false).right.is_nan());
}

#[test]
fn the_bitmap_offset_centres_tall_boxes_only() {
    assert_eq!(label_bitmap_offset(20.0, 1, false, true), None);
    assert_eq!(label_bitmap_offset(30.0, 1, true, true), None);
    assert_eq!(label_bitmap_offset(f32::NAN, 1, false, true), None);
    assert_eq!(label_bitmap_offset(30.0, 1, false, true), Some(UiPoint { x: 0.0, y: 4.0 }));
    // the radio button doubles the thickness as an `unsigned long` (wrapping), the check box as a float
    let big: c_ulong = 0x8000_0001;
    let wrapped = big.wrapping_mul(2) as f32;
    assert_eq!(
        label_bitmap_offset(30.0, big, false, false),
        Some(UiPoint { x: 0.0, y: (30.0 - wrapped) * 0.5 - 10.0 })
    );
    assert_eq!(
        label_bitmap_offset(30.0, big, false, true),
        Some(UiPoint { x: 0.0, y: (30.0 - 2.0 * big as f32) * 0.5 - 10.0 })
    );
}

#[test]
fn text_rect_insets_by_the_border() {
    let rr = UiRect::new(10.0, 20.0, 50.0, 40.0);
    assert_eq!(text_rect(&rr, 3), UiRect::new(13.0, 23.0, 47.0, 37.0));
    let f = init(&rr, 0);
    assert_eq!(f.text_rect, rr);
    assert!(is_none32(f.button_down_sound) && is_none32(f.button_up_sound));
    assert_eq!(f.focused_text_color, UiColor { red: 1.0, green: 1.0, blue: 0.0 });
    assert!(f.normal_bitmap.is_null() && f.copied_bitmap.is_null() && f.text.is_null());
}

fn world_of(controls: &[usize]) -> World {
    let mut w = World {
        ctls: Vec::new(),
        refs: vec![0; NAMES.len()],
        tab_stop: 0,
        tabbing: false,
        mouse: UiPoint::default(),
        capture: false,
        right: false,
        keys: [false; 4],
        polls: [false; 2],
        slider: 5,
        next_sound: 40,
        log: Vec::new(),
    };
    for &n in controls {
        construct(&mut w, n);
    }
    w.log.clear();
    w
}

#[test]
fn a_click_needs_the_release_over_the_button() {
    let mut w = world_of(&[0]);
    w.mouse = UiPoint { x: 20.0, y: 20.0 };
    mouse_l_button_down(&mut host(&mut w, 0));
    assert_eq!(w.ctls[0].s.f.state, STATE_PRESSED);
    assert!(w.log.contains(&"  fire 0 3ef".to_string()));
    w.log.clear();
    w.mouse = UiPoint { x: 200.0, y: 20.0 };
    mouse_l_button_up(&mut host(&mut w, 0));
    assert!(!w.log.iter().any(|l| l.contains("3f0")), "no click outside: {:?}", w.log);
    assert_eq!(w.ctls[0].s.f.state, STATE_NORMAL);
}

#[test]
fn a_key_release_clicks_only_after_a_key_press() {
    let mut w = world_of(&[0]);
    w.keys = [false, false, false, true];
    update_keyboard(&mut host(&mut w, 0));
    assert!(!w.log.iter().any(|l| l.contains("3f0")), "the fix of the missing parentheses: {:?}", w.log);
    w.keys = [true, false, false, false];
    update_keyboard(&mut host(&mut w, 0));
    assert!(w.ctls[0].s.f.key_down);
    w.keys = [false, false, false, true];
    w.log.clear();
    update_keyboard(&mut host(&mut w, 0));
    assert!(w.log.contains(&"  fire 0 3f0".to_string()));
    assert!(!w.ctls[0].s.f.key_down);
}

#[test]
fn checking_a_radio_button_unchecks_its_neighbour_only() {
    // 5 (checked), 6, 7 are one run; 11 is alone after a button
    let mut w = world_of(&[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);
    radio_check(&mut host(&mut w, 7));
    assert!(w.ctls[7].radio.is_checked && !w.ctls[5].radio.is_checked);
    radio_check(&mut host(&mut w, 11));
    assert!(w.ctls[11].radio.is_checked && w.ctls[7].radio.is_checked);
    // a checked push button stays pressed and drops its border
    radio_check(&mut host(&mut w, 6));
    assert!(!w.ctls[7].radio.is_checked);
    assert_eq!(w.ctls[6].s.f.state, STATE_PRESSED);
    assert_eq!(w.ctls[6].s.border_type, 0);
    assert_eq!(radio_mouse_l_button_down(&mut host(&mut w, 6)), 0);
    assert!(!w.ctls[6].s.f.mouse_down);
}

#[test]
fn a_disabled_check_box_ignores_checks_and_shows_pressed() {
    let mut w = world_of(&[0, 1, 2, 3]);
    check_set_disabled(&mut host(&mut w, 3), true);
    assert_eq!(w.ctls[3].s.f.state, STATE_PRESSED);
    check_set_checked(&mut host(&mut w, 3), true);
    assert!(!w.ctls[3].check.is_checked);
    check_set_disabled(&mut host(&mut w, 3), false);
    assert_eq!(w.ctls[3].s.f.state, STATE_NORMAL, "enabling shows the box normal (the kept fix)");
    check_toggle(&mut host(&mut w, 3));
    assert!(w.ctls[3].check.is_checked);
    assert_eq!(name(w.ctls[3].s.f.normal_bitmap), "CON");
}

#[test]
fn a_scroll_arrow_steps_on_press_then_after_the_delay() {
    let mut w = world_of(&[0, 1, 2, 3, 4, 5, 6, 7, 8]);
    scroll_button_down(&mut host(&mut w, 8));
    assert_eq!(w.slider, 4);
    assert!(w.ctls[8].delay);
    w.polls = [false, false];
    scroll_button_down(&mut host(&mut w, 8));
    assert_eq!(w.slider, 4);
    w.polls = [true, false];
    scroll_button_down(&mut host(&mut w, 8));
    assert_eq!(w.slider, 3);
    assert!(!w.ctls[8].delay && w.ctls[8].repeat);
    w.polls = [false, true];
    scroll_button_down(&mut host(&mut w, 8));
    assert_eq!(w.slider, 2);
}

#[test]
fn copy_data_centres_the_text_and_offsets_it_when_pressed() {
    let mut w = world_of(&[0]);
    let c = &mut w.ctls[0];
    c.s.f.text = MARK;
    c.s.f.copied_text = MARK;
    c.text = Some(crate::text::from_bytes(b"OK"));
    copy_data(&mut host(&mut w, 0));
    let f = w.ctls[0].s.f;
    assert!(f.has_copied_text);
    // text rect 12..88 x 12..28; the text 13 x 13
    assert_eq!(f.copied_text_rect.left, 76.0 * 0.5 + 12.0 - 13.0 * 0.5);
    assert_eq!(f.copied_text_rect.top, 16.0 * 0.5 + 12.0 - 13.0 * 0.5);
    assert_eq!(name(f.copied_bitmap), "N0");
    w.ctls[0].s.f.state = STATE_PRESSED;
    copy_data(&mut host(&mut w, 0));
    let f = w.ctls[0].s.f;
    assert_eq!(f.copied_text_rect.bottom, 29.0);
    assert_eq!(name(f.copied_bitmap), "P0");
    assert_eq!(w.ctls[0].copied_fg, f.focused_text_color);
    assert_eq!(w.refs[1], 2, "P0: pressed + copied");
    assert_eq!(w.refs[0], 1, "N0's copy reference released");
    // empty text: nothing copied
    w.ctls[0].text = Some(crate::text::empty());
    copy_data(&mut host(&mut w, 0));
    assert!(!w.ctls[0].s.f.has_copied_text);
}
