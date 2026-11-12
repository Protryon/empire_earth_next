//! The hot key events against the real C++: a scratch driver (`build/sc8/hotkeys_driver.cpp`) ran the unmodified
//! `EEUIEHotKeyEvents.cpp` over a fake current player (8 units: some without a complex unit, some parts of another,
//! carried or contained; their classes, stats, commands, goals and locations; the selected group, the saved groups
//! and the group buttons), the keyboard's Shift, the map under the view's centre, the camera, the mini map, the
//! alarm sound (loaded for the second half) and the communications: one session of 2,000 cases, each perturbing the
//! units (printed), then firing one event drawn from the creators' arguments and a few more (`golden_hot_keys.txt`).
//! The replay builds the host's unit records from the printed units the way `ee_screenscpp_hot_key_units` does and
//! prints what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp` and this crate
//! prints the same file, and the same on seeds 2-4 at 25,000 cases.

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_hot_keys.txt");

/// A printed unit.
#[derive(Clone, Default)]
struct Unit {
    complex: i64,
    hp: i32,
    tile: bool,
    id: u32,
    unit_type: i32,
    category: u32,
    family: u32,
    ammo: u32,
    command: i64,
    velocity: u32,
    max_hp: i32,
    commandable: bool,
    tile_x: i64,
    disabled: bool,
    previous: i64,
    goal: bool,
    goal_kind: u32,
    container: i64,
    at: [u32; 3],
    selected: bool,
}

#[derive(Default)]
struct World {
    out: Vec<String>,
    shift: i64,
    on_map: bool,
    target: (u32, u32),
    center: (u32, u32),
    first: i64,
    alarm: bool,
    units: Vec<Unit>,
    following: bool,
}

impl World {
    fn line(&mut self, text: String) {
        self.out.push(text);
    }
}

impl HotKeyHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_HK_UPDATE_SELECTION => self.line("  updateselection -1 1".into()),
            EE_SCREENS_GAME_HK_RESET_SAVED => self.line("  savedreset 20 1".into()),
            EE_SCREENS_GAME_HK_GROUP => match a {
                0 => self.line(format!("  local 132 255 {b} {c}")),
                1 => self.line(format!("  local 130 255 {b}")),
                _ => self.line(format!("  local 131 255 {b}")),
            },
            EE_SCREENS_OP_HK_GROUP_BUTTON => {
                self.line(format!("  firstunit -> {}", self.first));
                self.line(format!("  groupbutton {a} {}", self.first));
            }
            EE_SCREENS_GAME_HK_PLAYER_EVENT => self.line("  postaction".into()),
            EE_SCREENS_GAME_HK_SHIFT => {
                self.line("  keyentity 225".into());
                return self.shift as u32;
            }
            EE_SCREENS_GAME_HK_SELECT_UNIT => self.line(format!("  local 72 255 {a}")),
            EE_SCREENS_GAME_HK_ADD_UNIT => self.line(format!("  local 118 255 {a}")),
            EE_SCREENS_GAME_HK_CENTER => {
                let c = self.units[a as usize].complex as usize;
                let at = self.units[c].at;
                self.line("  camchanged".into());
                self.line(format!("  camlocation {:08x} {:08x} {:08x} 0 1", at[0], at[1], at[2]));
                self.line("  minimaprefresh".into());
                self.following = false;
            }
            EE_SCREENS_GAME_HK_ALARM => {
                if self.alarm {
                    self.line("  play 77 1 0 1 0 0".into());
                }
            }
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn units(&mut self, aircraft: bool) -> Vec<EeScreensHotKeyUnit> {
        let (mut tx, mut ty) = (f32::from_bits(self.target.0), f32::from_bits(self.target.1));
        if aircraft {
            self.line(format!("  locationat {:08x} {:08x} 1 -> {}", self.center.0, self.center.1, self.on_map as i32));
            if !self.on_map {
                self.line("  tileat".into());
                tx = tx as i32 as f32 + 0.5;
                ty = ty as i32 as f32 + 0.5;
            }
        }
        let us = &self.units;
        us.iter()
            .map(|u| {
                let mut r = EeScreensHotKeyUnit { unit_id: u.id, ..Default::default() };
                if u.complex < 0 {
                    return r;
                }
                let c = &us[u.complex as usize];
                r.flags |= EE_SCREENS_HK_F_COMPLEX;
                if u.hp > 0 {
                    r.flags |= EE_SCREENS_HK_F_ALIVE;
                }
                if u.tile {
                    r.flags |= EE_SCREENS_HK_F_ON_TILE;
                }
                if u.hp > 0 && u.tile {
                    r.complex_id = c.id;
                    r.unit_type = u.unit_type;
                    r.category = u.category;
                    r.command = c.command.max(0) as u32;
                    r.max_velocity = u.velocity;
                }
                if c.selected {
                    r.flags |= EE_SCREENS_HK_F_SELECTED;
                }
                r.c_category = c.category;
                r.c_hit_points = c.hp;
                let flag = |b: bool, f: u32| if b { f } else { 0 };
                r.flags |= flag(c.commandable, EE_SCREENS_HK_F_C_COMMANDABLE)
                    | flag(c.tile_x != -10000, EE_SCREENS_HK_F_C_TILE)
                    | flag(c.container >= 0, EE_SCREENS_HK_F_C_CONTAINER)
                    | flag(c.disabled, EE_SCREENS_HK_F_C_DISABLED)
                    | flag(c.previous >= 0, EE_SCREENS_HK_F_C_PREVIOUS_PART)
                    | flag(c.goal, EE_SCREENS_HK_F_C_GOAL);
                if c.goal {
                    r.c_goal_kind = c.goal_kind;
                }
                r.c_family = c.family;
                r.c_ammo = c.ammo;
                r.c_type = c.unit_type;
                r.c_max_hit_points = c.max_hp;
                if aircraft {
                    let at = if c.container >= 0 { us[c.container as usize].at } else { c.at };
                    let dx = f32::from_bits(at[0]) - tx;
                    let dy = f32::from_bits(at[1]) - ty;
                    let dz = 0.0f32; // (both points at z 0, as the host sets them)
                    r.distance = (dx * dx + dy * dy + dz * dz).to_bits();
                }
                r
            })
            .collect()
    }
}

/// A unit's printed fields: `[i cN hpN tN idN tyN caN faN amN cmdN vH mhpN coN txN diN ppN gN gkN inN at H H H selN]`.
fn read_unit(text: &str) -> Unit {
    let v: Vec<&str> = text.split(' ').collect();
    let n = |w: &str, p: &str| w.strip_prefix(p).unwrap_or_else(|| panic!("{w} {p}")).parse::<i64>().unwrap();
    let hex = |w: &str| u32::from_str_radix(w, 16).unwrap();
    Unit {
        complex: n(v[1], "c"),
        hp: n(v[2], "hp") as i32,
        tile: n(v[3], "t") != 0,
        id: n(v[4], "id") as u32,
        unit_type: n(v[5], "ty") as i32,
        category: n(v[6], "ca") as u32,
        family: n(v[7], "fa") as u32,
        ammo: n(v[8], "am") as u32,
        command: n(v[9], "cmd"),
        velocity: hex(v[10].strip_prefix('v').unwrap()),
        max_hp: n(v[11], "mhp") as i32,
        commandable: n(v[12], "co") != 0,
        tile_x: n(v[13], "tx"),
        disabled: n(v[14], "di") != 0,
        previous: n(v[15], "pp"),
        goal: n(v[16], "g") != 0,
        goal_kind: n(v[17], "gk") as u32,
        container: n(v[18], "in"),
        at: [hex(v[20]), hex(v[21]), hex(v[22])],
        selected: n(v[23], "sel") != 0,
    }
}

fn read_state(w: &mut World, line: &str) {
    let (head, units) = line.split_once(" units").unwrap();
    let v: Vec<&str> = head.split_whitespace().collect();
    let at = |key: &str| v.iter().position(|x| *x == key).unwrap() + 1;
    let hex = |s: &str| u32::from_str_radix(s, 16).unwrap();
    w.shift = v[at("shift")].parse().unwrap();
    w.on_map = v[at("onmap")] == "1";
    w.target = (hex(v[at("target")]), hex(v[at("target") + 1]));
    w.center = (hex(v[at("center")]), hex(v[at("center") + 1]));
    w.first = v[at("first")].parse().unwrap();
    w.alarm = v[at("alarm")] == "1";
    w.units = units.split('[').skip(1).map(|u| read_unit(u.trim().trim_end_matches(']'))).collect();
}

/// The event of a driver's kind.
fn event_of(kind: &[&str]) -> u32 {
    let n = |i: usize| kind[i].parse::<u32>().unwrap();
    match kind[0] {
        "select_group" if n(2) == 0 => EE_SCREENS_HK_SELECT_GROUP + n(1),
        "select_group" => EE_SCREENS_HK_SELECT_GROUP_ADD + n(1),
        "add_to_group" => EE_SCREENS_HK_ADD_TO_GROUP + n(1),
        "create_group" => EE_SCREENS_HK_CREATE_GROUP + n(1),
        "player_event" => EE_SCREENS_HK_PLAYER_EVENT,
        "aircraft" => EE_SCREENS_HK_AIRCRAFT + n(1),
        "unit_selection" => ee_screens_hot_key_unit_selection(n(1) as i32, n(2) != 0, n(3) != 0, n(4) != 0),
        other => panic!("kind {other}"),
    }
}

#[test]
fn golden_hot_keys() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut w = World::default();
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        let kind: Vec<&str> = case[0].split(' ').skip(2).collect();
        kinds.insert(kind[0].to_string());
        read_state(&mut w, case[1]);
        w.out.clear();
        w.following = true;
        let result = dispatch(&mut w, event_of(&kind)).unwrap();
        w.line(format!("  result {result}"));
        let after = format!("  after following {}", w.following as i32);
        w.line(after);
        let expected = &case[2..];
        for (i, e) in expected.iter().enumerate() {
            assert!(i < w.out.len(), "case {n} ({}): missing line {i}: {e}", case[0]);
            assert_eq!(w.out[i], *e, "case {n} ({}) line {i}", case[0]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 6);
}
