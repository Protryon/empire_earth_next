//! The hot keys against the real C++ (`golden_hotkey.txt`): a scratch driver built over the original `UIHotKey.cpp`
//! and `UIForm.cpp` (fake keys, an `ISKey` subclass whose every read is a trace line; fake events; a one-control form
//! whose Tab and Shift+Tab move nothing) printed 60 `GetHotKeyName`s, 300 `FireEvents` on random hot keys and key
//! states, and 75 forms of random `AddHotKey`s (the groups' and the list's order after them) with 4
//! `UpdateKeyboard`s each. The replay below is the same world over this module; the same driver over the forwarders
//! prints the same file.

use super::*;

const GOLDEN: &str = include_str!("../golden_hotkey.txt");

const NAMES: [&str; 12] = [
    "Left Shift key",
    "A key",
    "Ctrl",
    "key",
    " key",
    " keyboard key",
    "F1 key key",
    "",
    "Alt",
    "Num 5 key",
    "x key y",
    "Space",
];

/// `ISKeyboard::ComputeModifierState` (`include/lle/ISKeyboard.h`).
fn compute_modifier_state(key: c_ulong) -> c_ulong {
    let mut s = 0;
    if key == 0x2a || key == 0x36 || key == 0xe1 {
        s = 1;
    }
    if key == 0x38 || key == 0xb8 || key == 0xe2 {
        s |= 4;
    }
    if key == 0x1d || key == 0x9d || key == 0xe0 {
        s |= 2;
    }
    s
}

#[derive(Clone, Copy, Default)]
struct Key {
    index: c_ulong,
    value: c_long,
    double: bool,
    pressed: bool,
    unpressed: bool,
}

#[derive(Clone, Copy)]
struct Hot {
    key: usize,
    mods: [Option<usize>; 3],
    count: c_ulong,
    always: bool,
    events: [Option<c_ulong>; 4],
    state: c_ulong,
    enabled: bool,
}

impl Hot {
    fn text(&self) -> String {
        let m = |x: Option<usize>| x.map_or(-1, |v| v as i64);
        format!(
            " ({} {} {} {}:{}{})",
            self.key,
            m(self.mods[0]),
            m(self.mods[1]),
            m(self.mods[2]),
            self.count,
            self.always as u8
        )
    }
}

struct World {
    keys: [Key; 8],
    keyboard_state: c_ulong,
    log: Vec<String>,
}

struct Firing<'a> {
    w: &'a mut World,
    k: Hot,
}

impl FireHost for Firing<'_> {
    fn enabled(&self) -> bool {
        self.k.enabled
    }
    fn modifier_value(&mut self, i: usize) -> Option<c_long> {
        let m = self.k.mods[i]?;
        self.w.log.push(format!("  value {m}"));
        Some(self.w.keys[m].value)
    }
    fn keyboard_state(&mut self) -> c_ulong {
        self.w.keyboard_state
    }
    fn own_state(&self) -> c_ulong {
        self.k.state
    }
    fn key_state(&mut self) -> c_ulong {
        self.w.log.push(format!("  index {}", self.k.key));
        compute_modifier_state(self.w.keys[self.k.key].index)
    }
    fn has_event(&self, e: HotKeyEvent) -> bool {
        self.k.events[e as usize].is_some()
    }
    fn key_test(&mut self, e: HotKeyEvent) -> bool {
        let k = self.k.key;
        let key = self.w.keys[k];
        match e {
            HotKeyEvent::DoublePressed => {
                self.w.log.push(format!("  double {k}"));
                key.double
            }
            HotKeyEvent::JustPressed => {
                self.w.log.push(format!("  pressed {k}"));
                key.pressed
            }
            HotKeyEvent::Pressed => {
                self.w.log.push(format!("  value {k}"));
                key.value > 0
            }
            HotKeyEvent::JustUnpressed => {
                self.w.log.push(format!("  unpressed {k}"));
                key.unpressed
            }
        }
    }
    fn process(&mut self, e: HotKeyEvent) {
        let id = self.k.events[e as usize].unwrap();
        self.w.log.push(format!("  event {id}"));
    }
}

struct Form<'a> {
    w: &'a mut World,
    groups: Vec<Vec<Hot>>,
    tabbing: bool,
    captured: bool,
    ime: bool,
}

impl KeyboardHost for Form<'_> {
    fn captured(&self) -> bool {
        self.captured
    }
    fn captured_ime_enabled(&mut self) -> bool {
        self.w.log.push("  ime 0".into());
        self.ime
    }
    fn groups(&self) -> usize {
        self.groups.len()
    }
    fn hot_keys(&self, g: usize) -> usize {
        self.groups[g].len()
    }
    fn hot_key(&self, g: usize, j: usize) -> HotKey {
        ((g << 16) | j) as HotKey
    }
    fn always_fire(&self, k: HotKey) -> bool {
        let k = k as usize;
        self.groups[k >> 16][k & 0xffff].always
    }
    fn fire(&mut self, k: HotKey) -> bool {
        let k = k as usize;
        let hot = self.groups[k >> 16][k & 0xffff];
        fire_events(&mut Firing { w: self.w, k: hot })
    }
    fn tabbing(&self) -> bool {
        self.tabbing
    }
    fn tab_just_pressed(&mut self) -> bool {
        self.w.log.push("  pressed 5".into());
        self.w.keys[5].pressed
    }
    fn shift_value(&mut self) -> c_long {
        self.w.log.push("  value 3".into());
        self.w.keys[3].value
    }
    fn shift_tab(&mut self) {}
    fn tab(&mut self) {}
    fn captured_update(&mut self) -> c_long {
        self.w.log.push("  keyboard 0".into());
        3
    }
    fn tab_stop_update(&mut self) -> c_long {
        self.captured_update()
    }
}

fn parse_hot(t: &[&str]) -> Hot {
    // "(k m0 m1 m2:countalways)" "events" e0 e1 e2 e3 "state" s "enabled" e
    let key = t[0].trim_start_matches('(').parse().unwrap();
    let m = |s: &str| -> Option<usize> { s.parse::<i64>().ok().filter(|&v| v >= 0).map(|v| v as usize) };
    let (m2, rest) = t[3].split_once(':').unwrap();
    let rest = rest.trim_end_matches(')');
    let (count, always) = rest.split_at(rest.len() - 1);
    let e = |s: &str| -> Option<c_ulong> { s.parse::<i64>().ok().filter(|&v| v >= 0).map(|v| v as c_ulong) };
    Hot {
        key,
        mods: [m(t[1]), m(t[2]), m(m2)],
        count: count.parse().unwrap(),
        always: always == "1",
        events: [e(t[5]), e(t[6]), e(t[7]), e(t[8])],
        state: t[10].parse().unwrap(),
        enabled: t[12] == "1",
    }
}

#[test]
fn replays_the_cpp_hot_keys() {
    let mut out: Vec<String> = Vec::new();
    let mut w = World { keys: [Key::default(); 8], keyboard_state: 0, log: Vec::new() };
    let mut form: Option<(Vec<Vec<Hot>>, bool, bool, bool)> = None;
    let mut list: Vec<Hot> = Vec::new();
    let mut lines = GOLDEN.lines().peekable();
    while let Some(line) = lines.next() {
        if line.starts_with("  ") {
            continue;
        }
        out.append(&mut w.log);
        out.push(line.to_string());
        let t: Vec<&str> = line.split(' ').collect();
        match t[0] {
            "keyindex" => {
                for (i, v) in t[1..9].iter().enumerate() {
                    w.keys[i].index = v.parse().unwrap();
                }
            }
            "name" => {
                let n = |s: &str| -> Option<&[u8]> {
                    let v: usize = s.parse().unwrap();
                    (v != 99).then(|| NAMES[v].as_bytes())
                };
                let name = hot_key_name([n(t[1]), n(t[2]), n(t[3])], n(t[4]).unwrap());
                // the golden's line carries the C++ result; the replay's must be the same line
                let mine = format!("name {} {} {} {} [{}]", t[1], t[2], t[3], t[4], String::from_utf8(name).unwrap());
                out.pop();
                out.push(mine);
            }
            "keys" => {
                for (i, s) in t[1..9].iter().enumerate() {
                    let b = s.as_bytes();
                    w.keys[i].value = (b[0] - b'0') as c_long;
                    w.keys[i].double = b[1] == b'1';
                    w.keys[i].pressed = b[2] == b'1';
                    w.keys[i].unpressed = b[3] == b'1';
                }
                w.keyboard_state = t[10].parse().unwrap();
            }
            "fire" => {
                let k = parse_hot(&t[1..]);
                let mut f = Firing { w: &mut w, k };
                let fired = fire_events(&mut f);
                w.log.push(format!("  = {}", fired as u8));
            }
            "form" => {
                form = Some((Vec::new(), t[3] == "1", t[5] == "1", t[7] == "1"));
                list.clear();
            }
            "add" => {
                let g: usize = t[1].parse().unwrap();
                let k = parse_hot(&t[2..]);
                let f = form.as_mut().unwrap();
                if f.0.len() <= g {
                    f.0.resize(g + 1, Vec::new());
                }
                let order = |a: &Hot, b: &Hot| {
                    if before(a.always, a.count, b.always, b.count) {
                        core::cmp::Ordering::Less
                    } else if before(b.always, b.count, a.always, a.count) {
                        core::cmp::Ordering::Greater
                    } else {
                        core::cmp::Ordering::Equal
                    }
                };
                f.0[g].push(k);
                f.0[g].sort_by(order); // MSVC's std::sort: an insertion sort (stable) up to 32 elements
                list.push(k);
                list.sort_by(order);
                if !matches!(lines.peek(), Some(l) if l.starts_with("add")) {
                    for (g, keys) in f.0.iter().enumerate() {
                        w.log.push(format!("  group {g}{}", keys.iter().map(Hot::text).collect::<String>()));
                    }
                    w.log.push(format!("  list{}", list.iter().map(Hot::text).collect::<String>()));
                }
            }
            "update" => {
                let f = form.as_ref().unwrap();
                let mut host = Form { w: &mut w, groups: f.0.clone(), tabbing: f.1, captured: f.2, ime: f.3 };
                let r = update_keyboard(&mut host);
                w.log.push(format!("  = {r} stop 0"));
            }
            other => panic!("unknown line {other}"),
        }
        // a form without hot keys prints its (empty) lists right after its line
        if t[0] == "form" && !matches!(lines.peek(), Some(l) if l.starts_with("add")) {
            w.log.push("  list".into());
        }
    }
    out.append(&mut w.log);
    let golden: Vec<&str> = GOLDEN.lines().collect();
    assert!(golden.len() > 3000, "the golden is the driver's output");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(out.len(), golden.len());
}

#[test]
fn key_counts_and_names() {
    assert_eq!(key_count(false, [false; 3]), 0);
    assert_eq!(key_count(true, [true, false, true]), 3);
    assert_eq!(hot_key_name([Some(b"Left Ctrl key"), None, Some(b"Shift")], b"A key"), b"Left Ctrl + Shift + A");
    assert_eq!(without_key_suffix(b" key"), b"");
    assert_eq!(without_key_suffix(b"keys"), b"keys");
    assert!(before(false, 3, true, 1));
    assert!(!before(true, 1, false, 3));
    assert!(before(false, 1, false, 2));
    assert!(!before(false, 2, false, 2));
}
