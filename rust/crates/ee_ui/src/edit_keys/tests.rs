//! The key dispatch's golden trace: a scratch driver (`build/ui_key_golden`) ran the real `UIEditBox.cpp` of
//! a3e4be7f (its key dispatch C++, its text model already Rust) over fakes for everything outside the unit: 60 edit
//! boxes (single- and multi-line, password, numbers-only, a scenario-editor-like subclass overriding the virtual
//! `ProcessEnterKey`, with and without an Enter trigger) of 70 random operations each: `UpdateKeyboard` with random
//! keys held, just pressed or just released (every edit key, Shift and Ctrl combinations, the keypad with Num Lock
//! off) and the repeat timers firing or not, typed keys (UTF-8 characters of one to four bytes), `SetText`, the
//! clipboard's contents, IME compositions and their results, and drags that extend a selection and auto-scroll. Every
//! key entity the C++ read went into a per-operation digest (`reads`), every call outside the unit is a line, and the
//! box's state follows each operation that changed it. The replay runs the same operations through [`Edit`]'s key
//! dispatch over the same fakes, and checks every line. The same driver relinked over the forwarders (the
//! `UIEditBox.cpp` of this change) printed the same file, and equalled the original on four more seeds at ten times
//! the size.
//!
//! Round 8 fixed the two hazards (an anchor left outside the text by a deletion becomes the caret,
//! [`Edit::keep_anchor_in_text`]'s callers): 12 of the 60 boxes left such an anchor, and only their anchor, and the
//! state lines that follow from it, changed; those were re-recorded from the driver over the fixed forwarders (the
//! other 48 boxes are the original's lines). The last two tests reproduce the hazards (they failed before the fix).

use core::ffi::{c_long, c_short, c_ulong};

use super::*;
use crate::edit::{EditFields, EditState, SCROLL_BAR_WIDTH};
use crate::rect::{um_floor, UiRect};
use crate::text::{char_len, prefix, Platform};

const GOLDEN: &str = include_str!("../golden_keys.txt");

const FONT_HEIGHTS: [c_long; 3] = [8, 10, 16];

// the driver's key entities, in its order
const SHIFT: usize = 0;
const CTRL: usize = 1;
const ENTER: usize = 2;
const NUMENTER: usize = 3;
const BACK: usize = 4;
const DEL: usize = 5;
const NUMDEL: usize = 6;
const RIGHT: usize = 7;
const NUMRIGHT: usize = 8;
const LEFT: usize = 9;
const NUMLEFT: usize = 10;
const HOME: usize = 11;
const NUMHOME: usize = 12;
const END: usize = 13;
const NUMEND: usize = 14;
const UP: usize = 15;
const NUMUP: usize = 16;
const DOWN: usize = 17;
const NUMDOWN: usize = 18;
const X: usize = 19;
const C: usize = 20;
const V: usize = 21;
const INS: usize = 22;
const A: usize = 23;
const L: usize = 24;
const KEY_NAMES: [&str; 25] = [
    "shift", "ctrl", "enter", "numenter", "back", "del", "numdel", "right", "numright", "left", "numleft", "home",
    "numhome", "end", "numend", "up", "numup", "down", "numdown", "x", "c", "v", "ins", "a", "l",
];
const POLL_NAMES: [&str; 4] = ["repeat", "delay", "cursor", "scroll"];

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

/// The driver's `hex_bytes`: the bytes before the terminator, `-` for none.
fn hex(s: &[u8]) -> String {
    let t: String = s.iter().take_while(|&&b| b != 0).map(|b| format!("{b:02x}")).collect();
    if t.is_empty() {
        "-".into()
    } else {
        t
    }
}

/// A golden hex text -> its bytes, terminated.
fn unhex(s: &str) -> Vec<u8> {
    let mut v = Vec::new();
    if s != "-" {
        for i in (0..s.len()).step_by(2) {
            v.push(u8::from_str_radix(&s[i..i + 2], 16).unwrap());
        }
    }
    v.push(0);
    v
}

fn fnv(s: &str) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for &c in s.as_bytes() {
        h ^= c as u64;
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

#[derive(Clone, Copy, Default)]
struct Key {
    v: i64,
    jp: bool,
    ju: bool,
}

/// The driver's world: the box's members, its lines, a scroll bar whose slider clamps to its range, the key
/// entities and Num Lock (every read digested), named timers, the clipboard, the form's dirty flag and trigger queue.
struct Fake {
    s: EditState,
    lines: Vec<Vec<u8>>,
    scroll: Option<(c_long, c_long, c_long)>,
    font_height: c_long,
    sub: bool,
    trig: bool,
    keys: [Key; 25],
    numlock: bool,
    polls: [(bool, c_ulong); 4],
    clipboard: Option<Vec<u8>>,
    dirty: bool,
    fired: usize,
    reads: String,
    read_count: usize,
    log: Vec<String>,
}

impl Fake {
    /// The driver's `make_box`.
    #[allow(clippy::too_many_arguments)]
    fn new(
        multi: bool,
        max_chars: c_ulong,
        numbers: bool,
        pwd: bool,
        font: usize,
        w: f32,
        hh: f32,
        border: c_ulong,
        cs: c_ulong,
        sub: bool,
        trig: bool,
    ) -> Fake {
        let rect = UiRect::new(10.0, 20.0, 10.0 + w, 20.0 + hh);
        let b = border as f32;
        let mut text = UiRect::new(rect.left + b, rect.top + b, rect.right - b, rect.bottom - b);
        if text.top > text.bottom {
            text.top = rect.height() * 0.5 + rect.top;
            text.bottom = text.top + 1.0;
        }
        let mut f = EditFields {
            char_scroll: cs,
            max_chars,
            is_single_line: !multi,
            is_password_box: pwd,
            numbers_only: numbers,
            ..Default::default()
        };
        f.visible_line_count = um_floor(text.height() / FONT_HEIGHTS[font] as f32) as c_ulong;
        let mut scroll = None;
        if multi {
            f.scroll_bar_visible = true;
            text.right = rect.right - SCROLL_BAR_WIDTH;
            scroll = Some((0, 0, 0));
        }
        Fake {
            s: EditState { f, text, rect, border },
            lines: vec![vec![0]],
            scroll,
            font_height: FONT_HEIGHTS[font],
            sub,
            trig,
            keys: [Key::default(); 25],
            numlock: true,
            polls: [(false, 0); 4],
            clipboard: None,
            dirty: false,
            fired: 0,
            reads: String::new(),
            read_count: 0,
            log: Vec::new(),
        }
    }

    fn run<R>(&mut self, op: impl FnOnce(&mut Edit<Platform, Fake>) -> R) -> R {
        let mut e = Edit::new(&Platform, self);
        let r = op(&mut e);
        e.finish();
        r
    }

    fn dump(&self) -> String {
        let f = &self.s.f;
        let (sv, sm) = self.scroll.map(|(v, _, m)| (v, m)).unwrap_or((-1, -1));
        let t = self.s.text;
        format!(
            "state caret {} {} sel {} {} draw {} x {} sb {sv} {sm} ime {} {} {} {} flags {}{}{}{}{} polls {}{}{}{} \
             text {} {} {} {}",
            f.caret_line,
            f.caret_index,
            f.selection_start_line,
            f.selection_start_index,
            f.drawing_index,
            h(f.caret_x),
            f.ime_active as u8,
            f.ime_start_line,
            f.ime_start_index,
            f.ime_composition_length,
            f.is_text_selected as u8,
            f.selecting as u8,
            f.cursor_on as u8,
            f.key_repeating as u8,
            f.scroll_bar_visible as u8,
            self.polls[0].0 as u8,
            self.polls[1].0 as u8,
            self.polls[2].0 as u8,
            self.polls[3].0 as u8,
            h(t.left),
            h(t.top),
            h(t.right),
            h(t.bottom)
        )
    }

    // the key entities' reads, as the driver's FakeKey and PlatKeyIsToggled log them
    fn read(&mut self, name: &str, what: &str, value: i64) {
        self.reads += &format!("{name}.{what}={value};");
        self.read_count += 1;
    }
    fn v(&mut self, k: usize) -> bool {
        let x = self.keys[k].v;
        self.read(KEY_NAMES[k], "v", x);
        x != 0
    }
    fn jp(&mut self, k: usize) -> bool {
        let x = self.keys[k].jp;
        self.read(KEY_NAMES[k], "jp", x as i64);
        x
    }
    fn ju(&mut self, k: usize) -> bool {
        let x = self.keys[k].ju;
        self.read(KEY_NAMES[k], "ju", x as i64);
        x
    }
    /// `UIForm::GetNumLockOff`.
    fn num_lock_off(&mut self) -> bool {
        let on = self.numlock;
        self.read("numlock", "t", on as i64);
        !on
    }
    /// `UIControl::ShiftKeyPressed`.
    fn shift(&mut self) -> bool {
        self.jp(SHIFT) || self.v(SHIFT)
    }
    /// `UIControl::DeleteKeyPressed` and the arrows': the key, or its keypad twin with Num Lock off.
    fn pad(&mut self, main: usize, num: usize) -> bool {
        self.v(main) || (self.num_lock_off() && self.v(num))
    }
    /// `UIEditBox::UpdateTranslatedKey`'s C++ after the typing: the caret blink restarted, the caret shown.
    fn typed(&mut self) {
        self.poll(EditPoll::Cursor, PollOp::Stop);
        self.poll(EditPoll::Cursor, PollOp::Start);
        self.s.f.cursor_on = true;
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
    /// The driver's `GEText::ComputeTextExtent`: a hash advance per code point times the font's height / 8 + 1.
    fn text_width(&mut self, text: &[u8]) -> f32 {
        let n = text.iter().position(|&b| b == 0).unwrap_or(text.len());
        let mut width = 0.0f32;
        let mut at = 0;
        while at < n {
            let (c, l) = ee_plat_core::utf8::decode(&text[at..n]);
            let advance = (3 + (c.wrapping_mul(7)) % 9) as f32;
            width += advance * (self.font_height / 8 + 1) as f32;
            at += l.max(1);
        }
        width
    }
    fn font_height(&self) -> c_long {
        self.font_height
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
        let v = value as c_long;
        self.log.push(format!("  svalue {v}"));
        let (_, min, max) = self.scroll.unwrap();
        self.scroll = Some((v.min(max).max(min), min, max));
    }
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong) {
        self.log.push(format!("  srange {min} {max}"));
        let (v, _, _) = self.scroll.unwrap();
        let (min, max) = (min as c_long, max as c_long);
        self.scroll = Some((v.min(max).max(min), min, max));
    }
    fn scroll_set_location(&mut self, at: UiPoint) {
        self.log.push(format!("  sbvirt 27 {} {}", h(at.x), h(at.y)));
    }
    fn set_dirty(&mut self) {
        self.dirty = true;
    }
    /// `UIControl::SetRectangle` (the box is not shown: no dirty area), then the box's own `SetLocation`.
    fn set_rectangle(&mut self, r: &UiRect) {
        self.s.rect = *r;
        let at = r.top_left();
        self.run(|e| e.set_location(at));
    }
    fn base_set_rectangle(&mut self, r: &UiRect) {
        self.set_rectangle(r);
    }
    /// `UIControl::SetLocation`: the rectangle moved by the delta, then `SetDirty(true)`.
    fn base_set_location(&mut self, at: UiPoint) {
        let r = self.s.rect;
        let (dx, dy) = (at.x - r.left, at.y - r.top);
        self.s.rect = UiRect::new(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
        self.dirty = true;
    }
    fn fire(&mut self, event: c_ulong) {
        if self.trig && event == TRIGGER_ENTER_PRESSED {
            self.fired += 1;
        }
    }
}

impl EditKeysHost for Fake {
    fn key(&mut self, k: EditKey) -> bool {
        match k {
            EditKey::Shift => self.shift(),
            EditKey::Control => self.v(CTRL),
            EditKey::BackSpace => self.v(BACK),
            EditKey::Delete => self.pad(DEL, NUMDEL),
            EditKey::Right => self.pad(RIGHT, NUMRIGHT),
            EditKey::Left => self.pad(LEFT, NUMLEFT),
            EditKey::Home => self.pad(HOME, NUMHOME),
            EditKey::End => self.pad(END, NUMEND),
            EditKey::Up => self.pad(UP, NUMUP),
            EditKey::Down => self.pad(DOWN, NUMDOWN),
            EditKey::EnterReleased => self.ju(ENTER) || self.ju(NUMENTER),
            EditKey::Cut => (self.v(CTRL) && self.v(X)) || (self.shift() && self.v(DEL)),
            EditKey::Copy => self.v(CTRL) && self.v(C),
            EditKey::Paste => (self.v(CTRL) && self.v(V)) || (self.shift() && self.v(INS)),
            EditKey::SelectAll => self.jp(A),
            EditKey::ClearLine => self.jp(L),
            EditKey::Return | EditKey::EnterPressed => self.jp(ENTER),
            EditKey::NumEnterPressed => self.jp(NUMENTER),
        }
    }
    fn poll(&mut self, p: EditPoll, op: PollOp) -> c_ulong {
        let i = p as usize;
        match op {
            PollOp::Running => self.polls[i].0 as c_ulong,
            PollOp::Poll => {
                self.log.push(format!("  poll {} = {}", POLL_NAMES[i], self.polls[i].1));
                self.polls[i].1
            }
            PollOp::Start => {
                self.polls[i].0 = true;
                self.log.push(format!("  poll {} start", POLL_NAMES[i]));
                0
            }
            PollOp::Stop => {
                self.polls[i].0 = false;
                0
            }
        }
    }
    /// The virtual `ProcessEnterKey`: the subclass logs first; the box's is the model's.
    fn process_enter_key(&mut self) {
        if self.sub {
            self.log.push("  sub enter".into());
        }
        self.run(|e| e.enter_key());
    }
    fn clipboard_set(&mut self, text: &[u8]) {
        self.log.push(format!("  clipset {}", hex(text)));
        self.clipboard = Some(text.to_vec());
    }
    fn clipboard_get(&mut self) -> Option<Vec<u8>> {
        let shown = self.clipboard.as_deref().map(hex).unwrap_or_else(|| "none".into());
        self.log.push(format!("  clipget {shown}"));
        self.clipboard.clone()
    }
    /// The driver's `UIManager::UpdateTranslatedKey`: the first character, typed into the box.
    fn translated_key(&mut self, text: &[u8]) {
        let key = prefix(&Platform, text, 1);
        self.log.push(format!("  tkey {} 1", hex(&key)));
        self.run(|e| e.type_key(&key, 1));
        self.typed();
    }
}

struct Replay {
    out: Vec<String>,
    last_state: String,
    last_lines: Vec<Vec<u8>>,
}

impl Replay {
    /// The driver's `finish`: the next frame's `CheckScrollBar`, the reads' digest, the dirty flag, the queued
    /// triggers, the state and lines when they changed.
    fn finish(&mut self, f: &mut Fake, force: bool) {
        f.run(|e| e.check_scroll_bar());
        self.out.append(&mut f.log);
        if f.read_count > 0 {
            self.out.push(format!("  reads {} {:016x}", f.read_count, fnv(&f.reads)));
        }
        f.reads.clear();
        f.read_count = 0;
        if f.dirty {
            self.out.push("  dirty".into());
        }
        f.dirty = false;
        if f.fired > 0 {
            self.out.push(format!("  fired {}", f.fired));
        }
        f.fired = 0;
        let state = f.dump();
        if force || state != self.last_state {
            self.out.push(state.clone());
        }
        self.last_state = state;
        if force || f.lines != self.last_lines {
            for (n, l) in f.lines.iter().enumerate() {
                self.out.push(format!("  line {n} {}", hex(l)));
            }
        }
        self.last_lines = f.lines.clone();
    }
}

#[test]
fn replays_the_cpp_golden() {
    let golden: Vec<&str> = GOLDEN.lines().collect();
    let mut r = Replay { out: Vec::new(), last_state: String::new(), last_lines: Vec::new() };
    let mut fake: Option<Fake> = None;
    for line in &golden {
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "new" {
            r.out.push(line.to_string());
            let flag = |k: usize| t[k] == "1";
            let mut f = Fake::new(
                flag(3),
                t[5].parse().unwrap(),
                flag(7),
                flag(9),
                t[11].parse().unwrap(),
                fx(t[13]),
                fx(t[15]),
                t[17].parse().unwrap(),
                t[19].parse().unwrap(),
                flag(21),
                flag(23),
            );
            // the driver's clipboard is the process's: it outlives a box
            f.clipboard = fake.as_ref().and_then(|old| old.clipboard.clone());
            r.last_state.clear();
            r.last_lines.clear();
            r.finish(&mut f, true);
            fake = Some(f);
            continue;
        }
        if t[0] != "op" {
            continue; // the logs and states: regenerated
        }
        let f = fake.as_mut().unwrap();
        let mut again = false;
        match t[1] {
            "keys" => {
                r.out.push(line.to_string());
                f.keys = [Key::default(); 25];
                f.numlock = t[3] == "1";
                for (i, p) in t[5..9].iter().enumerate() {
                    f.polls[i].1 = p.parse().unwrap();
                }
                for spec in &t[9..] {
                    let (name, s) = spec.split_once('=').unwrap();
                    let k = KEY_NAMES.iter().position(|n| *n == name).unwrap();
                    let s = s.as_bytes();
                    f.keys[k] = Key { v: (s[0] - b'0') as i64, jp: s[1] == b'1', ju: s[2] == b'1' };
                }
                let rr = f.run(|e| e.update_keyboard());
                if rr != 0 {
                    f.log.push(format!("  = {rr}"));
                }
            }
            "type" => {
                r.out.push(line.to_string());
                let key = unhex(t[2]);
                let rep: c_short = t[3].parse().unwrap();
                f.run(|e| e.type_key(&key, rep));
                f.typed();
            }
            "settext" => {
                r.out.push(line.to_string());
                let text = unhex(t[2]);
                f.run(|e| e.set_text(&text));
            }
            "clip" => {
                r.out.push(line.to_string());
                f.clipboard = if t[2] == "none" { None } else { Some(unhex(t[2])) };
            }
            "imeopen" => {
                let rr = f.run(|e| e.ime_open());
                r.out.append(&mut f.log);
                r.out.push(format!("op imeopen = {rr}"));
                // a refused composition (a numbers-only box) ends the driver's IME op: its loop finishes again
                again = !f.s.f.ime_active;
            }
            "imechange" => {
                r.out.push(line.to_string());
                let text = unhex(t[2]);
                let cursor: c_ulong = t[3].parse().unwrap();
                let rr = f.run(|e| e.ime_change(&text, cursor));
                f.log.push(format!("  = {rr}"));
            }
            "imeresult" => {
                r.out.push(line.to_string());
                let text = unhex(t[2]);
                f.run(|e| e.add_ime_result(&text));
            }
            "imeclose" => {
                r.out.push(line.to_string());
                let rr = f.run(|e| e.ime_close());
                f.log.push(format!("  = {rr}"));
            }
            "press" => {
                r.out.push(line.to_string());
                f.s.f.selecting = true;
                f.s.f.selection_start_index = f.s.f.caret_index;
                f.s.f.selection_start_line = f.s.f.caret_line;
            }
            "move" => {
                r.out.push(line.to_string());
                let at = UiPoint { x: fx(t[2]), y: fx(t[3]) };
                if !f.s.f.scroll_bar_captured && f.s.f.selecting {
                    f.run(|e| e.drag_to(at));
                }
            }
            "release" => {
                r.out.push(line.to_string());
                f.s.f.selecting = false;
            }
            other => panic!("op {other}"),
        }
        r.finish(f, false);
        if again {
            r.finish(f, false);
        }
    }
    for (n, (a, b)) in r.out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", n + 1);
    }
    assert_eq!(r.out.len(), golden.len(), "line count");
}

/// A single-line box with `text`, the caret at its end.
fn single(text: &[u8]) -> Fake {
    let mut f = Fake::new(false, 40, false, false, 1, 200.0, 20.0, 1, 1, false, true);
    f.run(|e| e.set_text(text));
    f.run(|e| e.end_key());
    f
}

fn press(f: &mut Fake, keys: &[usize]) -> bool {
    f.keys = [Key::default(); 25];
    for &k in keys {
        f.keys[k] = Key { v: 1, jp: true, ju: false };
    }
    f.run(|e| e.process_input(true))
}

#[test]
fn backspace_and_delete_take_whole_characters() {
    // "a€😀": the caret at the end; Backspace takes the four-byte emoji, Left steps over the euro sign
    let mut f = single("a\u{20ac}\u{1f600}\0".as_bytes());
    assert_eq!(f.s.f.caret_index, 3);
    assert!(press(&mut f, &[BACK]));
    assert_eq!(f.lines[0], "a\u{20ac}\0".as_bytes());
    assert!(press(&mut f, &[LEFT]));
    assert_eq!(f.s.f.caret_index, 1);
    assert!(press(&mut f, &[DEL]));
    assert_eq!(f.lines[0], b"a\0");
}

#[test]
fn shift_moves_select_and_ctrl_x_cuts_to_the_clipboard() {
    let mut f = single("\u{65e5}\u{672c}x\0".as_bytes());
    press(&mut f, &[SHIFT, HOME]);
    assert!(f.s.f.is_text_selected);
    assert_eq!((f.s.f.selection_start_index, f.s.f.caret_index), (3, 0));
    press(&mut f, &[CTRL, X]);
    assert_eq!(f.clipboard.as_deref(), Some("\u{65e5}\u{672c}x\0".as_bytes()));
    assert_eq!(f.lines[0], b"\0");
    // Ctrl+V types it back
    press(&mut f, &[CTRL, V]);
    assert_eq!(f.lines[0], "\u{65e5}\u{672c}x\0".as_bytes());
}

#[test]
fn keypad_arrows_move_only_with_num_lock_off() {
    let mut f = single(b"abc\0");
    f.numlock = true;
    assert!(!press(&mut f, &[NUMLEFT]));
    assert_eq!(f.s.f.caret_index, 3);
    f.numlock = false;
    assert!(press(&mut f, &[NUMLEFT]));
    assert_eq!(f.s.f.caret_index, 2);
}

#[test]
fn enter_in_a_single_line_box_goes_through_the_virtual_and_fires() {
    let mut f = single(b"hi\0");
    f.sub = true;
    f.keys = [Key::default(); 25];
    f.keys[ENTER] = Key { v: 1, jp: true, ju: false };
    f.run(|e| e.update_keyboard());
    assert_eq!(f.log.iter().filter(|l| *l == "  sub enter").count(), 1);
    assert_eq!(f.fired, 1);
    assert!(!f.s.f.key_repeating);
    assert!(f.polls[EditPoll::Cursor as usize].0);
}

#[test]
fn a_held_key_waits_for_the_delay_then_repeats() {
    let mut f = single(b"abcdef\0");
    f.keys = [Key::default(); 25];
    f.keys[LEFT] = Key { v: 1, jp: false, ju: false };
    // the first frame acts and starts the delay
    f.run(|e| e.update_keyboard());
    assert_eq!(f.s.f.caret_index, 5);
    assert!(f.polls[EditPoll::RepeatDelay as usize].0);
    // the delay has not run out: nothing
    f.polls[EditPoll::RepeatDelay as usize].1 = 0;
    f.run(|e| e.update_keyboard());
    assert_eq!(f.s.f.caret_index, 5);
    // it has: a step, and the repeat timer takes over
    f.polls[EditPoll::RepeatDelay as usize].1 = 1;
    f.run(|e| e.update_keyboard());
    assert_eq!(f.s.f.caret_index, 4);
    assert!(f.polls[EditPoll::Repeat as usize].0 && !f.polls[EditPoll::RepeatDelay as usize].0);
    // released: the timers stop, the caret blinks again
    f.keys = [Key::default(); 25];
    f.run(|e| e.update_keyboard());
    assert!(!f.polls[EditPoll::Repeat as usize].0 && f.polls[EditPoll::Cursor as usize].0);
    assert!(!f.s.f.key_repeating);
}

#[test]
fn an_ime_result_is_typed_a_character_at_a_time() {
    let mut f = single(b"\0");
    f.run(|e| e.ime_open());
    f.run(|e| e.add_ime_result("\u{65e5}\u{672c}\u{8a9e}\0".as_bytes()));
    let typed: Vec<&String> = f.log.iter().filter(|l| l.starts_with("  tkey")).collect();
    assert_eq!(typed, ["  tkey e697a5 1", "  tkey e69cac 1", "  tkey e8aa9e 1"]);
    assert_eq!(f.lines[0], "\u{65e5}\u{672c}\u{8a9e}\0".as_bytes());
    assert!(f.s.f.ime_active);
    assert_eq!(f.s.f.ime_start_index, 3);
}

/// The anchor is a place in the text: its line exists and its index is within the line.
fn anchor_in_text(f: &Fake) -> bool {
    let (line, index) = (f.s.f.selection_start_line as usize, f.s.f.selection_start_index);
    line < f.lines.len() && index >= 0 && index as usize <= char_len(&Platform, &f.lines[line])
}

#[test]
fn a_deletion_during_a_drag_keeps_the_anchor_in_the_text() {
    // round 7's hazard (the original's): a mouse press anchors the drag at the caret (the end), Backspace deletes
    // the character before it, and the next MouseMove selected from the old anchor, past the line's end
    let mut f = single(b"abcdef\0");
    f.s.f.selecting = true;
    f.s.f.selection_start_index = f.s.f.caret_index;
    f.s.f.selection_start_line = f.s.f.caret_line;
    press(&mut f, &[BACK]);
    assert_eq!(f.lines[0], b"abcde\0");
    assert!(anchor_in_text(&f), "anchor {} past the text", f.s.f.selection_start_index);
    // the drag goes on from the caret where the deletion left it
    let left = f.s.text.left;
    f.run(|e| e.drag_to(UiPoint { x: left + 0.5, y: 30.0 }));
    assert!(anchor_in_text(&f));
    assert_eq!(f.s.f.selection_start_index, 5);
    assert!(f.s.f.is_text_selected);
    let selected = f.run(|e| e.selected_text());
    assert!(selected.is_some());
}

#[test]
fn shift_keypad_del_and_an_arrow_in_one_frame_keep_the_anchor_in_the_text() {
    // round 7's other hazard: Shift+Home selects the whole line (the anchor at the end), then Shift + the keypad's
    // Del (Num Lock off) deletes it and Shift+Right in the same frame selected from the old anchor, past the text
    let mut f = single(b"abcdef\0");
    press(&mut f, &[SHIFT, HOME]);
    assert_eq!((f.s.f.selection_start_index, f.s.f.caret_index), (6, 0));
    f.numlock = false;
    press(&mut f, &[SHIFT, NUMDEL, RIGHT]);
    assert_eq!(f.lines[0], b"\0");
    assert!(anchor_in_text(&f), "anchor {} past the text", f.s.f.selection_start_index);
    assert!(!f.s.f.is_text_selected);
    // a Backspace without a selection, then Shift+Left: the selection is the one character left of the caret
    let mut f = single(b"abc\0");
    press(&mut f, &[SHIFT, BACK, LEFT]);
    assert_eq!(f.lines[0], b"ab\0");
    assert!(anchor_in_text(&f));
    assert_eq!((f.s.f.selection_start_index, f.s.f.caret_index), (2, 1));
}
