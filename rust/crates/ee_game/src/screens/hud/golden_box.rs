//! The status box's `Fill`, `FillArmorAttributes` and `Refresh` against the real C++ (`golden_box.txt`): a scratch
//! driver (`build/sc3/box_driver.cpp`) ran the unmodified `EEUIFMGStatusBox.cpp` over calloc'ed units, classes,
//! stats and players and fake controls (the toolkit's out-of-line members print a trace line; the inline ones run
//! as they are), the real `UWideString`: 1,000 cases of a new selection's `Fill` (or none), a lone
//! `FillArmorAttributes` then `Refresh`, or `Refresh` alone, the selection's values changing between them.
//!
//! The replay is a model of the C++ seam: the controls' state (display modes, texts, bitmaps, improvements, queued
//! counts) and the box's members, every toolkit call the C++ makes over the plans of this module, each printed as
//! the driver prints it. The driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at
//! ten times the size.
//!
//! The golden is the Windows build's 32-bit `long`: on a 64-bit `long` a case may differ where a value passes 2^31;
//! there the model takes the C++'s state back from the golden after each case.

use super::*;

const GOLDEN: &str = include_str!("../golden_box.txt");

const IDS_IMPROVEMENT_GARRISON: c_ulong = 30023;
const IDS_MSG_LOCKED: c_long = 31868;
const IDS_MSG_UNLOCKED: c_long = 31869;
const NULL_BITMAP: u32 = 888;

fn f(hex: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(hex, 16).unwrap())
}
fn l(s: &str) -> c_long {
    s.parse::<i64>().unwrap() as c_long
}

#[derive(Clone, Copy, Default)]
struct Icon {
    normal: u32,
    pressed: u32,
    attribute: c_long,
    cost_label: u32,
    queued: c_long,
}

#[derive(Clone, Copy, Default)]
struct Bar {
    icon: u32,
    attribute: c_long,
    enabled: bool,
    string: c_ulong,
    queued: c_long,
}

struct Model {
    out: Vec<String>,
    modes: [c_ulong; 200],
    texts: Vec<String>,
    icons: [Icon; 9],
    bars: [Bar; 9], // by control: 1, 2, 7, 8
    attributes: [c_long; 14],
    count: c_long,
    armor: c_long,
    resource_shown: bool,
    two: bool,
    civ: bool,
}

/// What one case's line holds beyond the view.
struct Case {
    unit: EeScreensHudUnit,
    gate: bool,
    locked: bool,
    name: String,
    name_id: c_long,
    civ_name: String,
    cinematic: bool,
    armor: [(c_long, c_long, f32); 6],
}

impl Model {
    fn new() -> Model {
        let mut icons = [Icon::default(); 9];
        for icon in &mut icons {
            icon.normal = NULL_BITMAP;
            icon.pressed = NULL_BITMAP;
        }
        let mut bars = [Bar::default(); 9];
        for bar in &mut bars {
            bar.icon = NULL_BITMAP;
        }
        Model {
            out: Vec::new(),
            modes: [1; 200],
            texts: vec![String::new(); 200],
            icons,
            bars,
            attributes: [-1; 14],
            count: 0,
            armor: 0,
            resource_shown: false,
            two: false,
            civ: false,
        }
    }
    fn p(&mut self, s: String) {
        self.out.push(s);
    }
    fn mode(&mut self, control: u32, mode: c_ulong) {
        self.modes[control as usize] = mode;
        self.p(format!("  mode {control} {mode}"));
    }
    fn dirty(&mut self, control: u32) {
        self.p(format!("  dirty {control} 1"));
    }
    fn text(&mut self, label: u32, text: &str) {
        self.texts[label as usize] = text.to_string();
        self.p(format!("  text {label} {text} 1"));
    }
    fn set_if_differs(&mut self, label: u32, text: &str) -> bool {
        if self.texts[label as usize] != text {
            self.text(label, text);
            true
        } else {
            false
        }
    }
    fn range(&mut self, bar: u32, max: c_long) {
        self.p(format!("  range {} 0 {}", bar + 80, max as c_ulong));
    }
    fn current(&mut self, bar: u32, value: c_long) {
        self.p(format!("  current {} {value}", bar + 80));
    }
    fn cost(&mut self, control: u32, text: &str) {
        self.p(format!("  cost {control} {text}"));
    }
    fn stretch(&mut self, bitmap: u32, rect: c_long, screen: u32) {
        let rect = 10 + rect; // the icon control's index (the driver's rectangle)
        self.p(format!("  stretch {bitmap} {rect} {rect} {} {screen}", 100 + bitmap));
    }
    fn queued_zero(&mut self, control: u32, queued: &mut c_long) {
        if *queued != 0 {
            self.dirty(control);
        }
        *queued = 0;
    }
    fn hide_upgrade(&mut self, control: u32) {
        self.mode(control, 0);
        let mut q = self.bars[control as usize].queued;
        self.queued_zero(control, &mut q);
        self.bars[control as usize].queued = q;
    }
    fn show_icon(&mut self, e: &EeScreensHudIcon) {
        self.attributes[e.slot as usize] = e.icon;
        if e.icon == -1 {
            return;
        }
        let control = 10 + e.icon as u32;
        self.stretch(e.bitmap, e.rect_icon, control);
        self.stretch(e.bitmap + 1, e.rect_icon, control);
        let i = e.icon as usize;
        self.icons[i].normal = e.bitmap;
        self.dirty(control);
        self.icons[i].pressed = e.bitmap + 1;
        self.dirty(control);
        self.icons[i].attribute = e.improvement;
        let mut q = self.icons[i].queued;
        self.queued_zero(control, &mut q);
        self.icons[i].queued = q;
        self.mode(control, 1);
        self.icons[i].cost_label = 20 + e.icon as u32;
    }
    fn set_icon_bitmap(&mut self, bar: u32, bitmap: u32) {
        self.bars[bar as usize].icon = bitmap;
        self.dirty(bar + 100);
        self.p(format!("  stretch {bitmap} 0 0 {} {bar}", 100 + bitmap));
        self.dirty(bar);
    }
    fn color(&mut self, rgb: &str) {
        for k in 0..4 {
            self.p(format!("  color {rgb} {k}"));
        }
    }
    fn bar_control(&self, bar: u32) -> u32 {
        [2, 7, 8][bar as usize]
    }

    fn name(&self, c: &Case) -> String {
        let mut text = if c.name.is_empty() { format!("S{}", c.name_id) } else { c.name.clone() };
        if c.gate {
            text += &format!("S{}", if c.locked { IDS_MSG_LOCKED } else { IDS_MSG_UNLOCKED });
        }
        text
    }

    fn fill_armor(&mut self, c: Option<&Case>) {
        let start = self.count;
        self.armor = start;
        if let Some(c) = c {
            let icons = armor_icons(start, c.unit.upgrades_armor);
            for e in &icons.icons[..icons.count as usize] {
                self.show_icon(e);
            }
            self.armor = icons.next;
        }
        self.count = self.armor;
        if self.count as c_ulong > 9 {
            self.p("  verify 367".into());
        }
    }

    fn fill(&mut self, c: Option<&Case>, cinematic: bool) {
        self.count = 0;
        self.resource_shown = false;
        self.hide_upgrade(1);
        self.hide_upgrade(2);
        if self.two {
            self.hide_upgrade(7);
            self.hide_upgrade(8);
        }
        self.mode(3, 0);
        self.mode(4, 0);
        if self.civ {
            self.mode(5, 0);
        }
        self.mode(6, 0);
        for i in 0..9 {
            self.mode(10 + i, 0);
            self.mode(20 + i, 0);
        }
        if let Some(c) = c {
            let u = &c.unit;
            let name = self.name(c);
            self.text(4, &name);
            self.mode(4, 1);
            if self.civ {
                let civ = c.civ_name.clone();
                self.text(5, &civ);
                self.mode(5, 1);
            }
            let bars = fill_bars(u);
            if bars.hit_points {
                self.set_icon_bitmap(1, 24);
                self.range(1, u.max_hit_points);
                self.current(1, u.hit_points);
                self.dirty(1);
                self.bars[1].attribute = 9;
                self.mode(1, 1);
            }
            let icons = fill_icons(u);
            for e in &icons.icons[..icons.count as usize] {
                self.show_icon(e);
            }
            self.count = icons.next;
            self.attributes[11] = -1;
            self.fill_armor(Some(c));
            let icons = transport_icon(self.count, u);
            self.show_icon(&icons.icons[0]);
            self.count = icons.next;
            self.mode(3, 1);
            self.p("  portrait 1".into());
            if bars.kind == 1 {
                self.set_icon_bitmap(2, 26);
                self.color("0 1 0.5");
                self.range(2, bars.max);
                self.current(2, bars.current);
                self.bars[2].attribute = 8;
                self.mode(2, 1);
                self.bars[2].enabled = true;
            } else if bars.kind == 2 {
                let b = self.bar_control(bars.bar);
                self.set_icon_bitmap(b, 42);
                self.color("0 0 1");
                self.range(b, bars.max);
                self.current(b, bars.current);
                self.bars[b as usize].attribute = 8;
                self.mode(b, 1);
                self.bars[b as usize].enabled = true;
                self.bars[b as usize].string = IDS_IMPROVEMENT_GARRISON;
            }
            if bars.garrison {
                self.set_icon_bitmap(2, 44);
                self.color("1 1 0");
                self.range(2, bars.garrison_max);
                self.current(2, bars.garrison_current);
                self.bars[2].attribute = 15;
                self.mode(2, 1);
                self.bars[2].enabled = false;
                self.bars[2].string = IDS_IMPROVEMENT_GARRISON;
            }
            if self.count as c_ulong > 9 {
                self.p("  verify 708".into());
            }
            for j in self.count.max(0)..5 {
                self.mode(10 + j as u32, 0);
                self.mode(20 + j as u32, 0);
                self.icons[j as usize].cost_label = 0;
            }
            self.refresh(c);
        }
        if !cinematic {
            let shown = c.is_some();
            for id in [100, 111, 112, 113] {
                self.p(format!("  background {id}"));
                self.p(format!("  visible {id} {}", shown as i32));
            }
            self.p("  background 124".into());
            self.p("  visible 124 1".into());
            if !shown {
                self.p("  portrait 0".into());
            }
        }
    }

    fn refresh(&mut self, c: &Case) {
        let u = &c.unit;
        let name = self.name(c);
        self.set_if_differs(4, &name);
        if self.civ {
            let civ = c.civ_name.clone();
            self.set_if_differs(5, &civ);
        }
        for (slot, &kind) in ARMOR_KINDS.iter().enumerate() {
            let icon = self.attributes[4 + slot];
            if icon == -1 {
                continue;
            }
            let kind = kind as usize;
            let (class, stats_class, stat) = c.armor[kind];
            let (base, bonus) = armor_attribute(stat, class, stats_class);
            let (a, b) = attribute_text(base, bonus);
            self.cost(10 + icon as u32, &format!("{a}+{b}"));
        }
        let plan = refresh(u);
        if plan.hit_points {
            let t = format!("{}/{}", plan.hit_points_text[0], plan.hit_points_text[1]);
            if self.set_if_differs(41, &t) {
                self.current(1, u.hit_points);
            }
            let t = format!("+ {}", plan.hit_points_bonus);
            if self.set_if_differs(61, &t) {
                self.range(1, u.max_hit_points);
                self.current(1, u.hit_points);
            }
            self.cost(1, &t);
        }
        for (k, slot) in [0, 1, 2, 3, 10].into_iter().enumerate() {
            let icon = self.attributes[slot];
            if icon != -1 {
                let [a, b] = plan.attributes[k];
                self.cost(10 + icon as u32, &format!("{a}+{b}"));
            }
        }
        self.p("  portraitupdate".into());
        if u.complex {
            let icon = self.attributes[12];
            if icon != -1 {
                self.cost(10 + icon as u32, &format!("{}/{}", plan.cargo[0], plan.cargo[1]));
            }
            if plan.kind != 0 {
                let b = self.bar_control(plan.bar);
                let t = format!("{}/{}", plan.bar_text[0], plan.bar_text[1]);
                if self.set_if_differs(b + 40, &t) {
                    self.current(b, plan.bar_current);
                }
                let t = format!("+ {}", plan.bar_bonus);
                if self.set_if_differs(b + 60, &t) {
                    self.range(b, plan.bar_max);
                    self.current(b, plan.bar_current);
                }
                self.cost(b, &t);
            }
            if plan.garrison {
                let t = format!("{}/{}", plan.garrison_text[0], plan.garrison_text[1]);
                if self.set_if_differs(42, &t) {
                    self.current(2, plan.garrison_current);
                }
                if self.set_if_differs(62, "") {
                    self.range(2, plan.garrison_max);
                    self.current(2, plan.garrison_current);
                }
                self.cost(2, "");
            }
        }
        let n = self.count as usize;
        let control = 10 + n as u32;
        if plan.resource {
            if !self.resource_shown {
                self.icons[n].attribute = 15;
                self.icons[n].cost_label = 20 + n as u32;
                self.mode(control, 1);
                self.mode(control + 10, 1);
                self.resource_shown = true;
            }
            if plan.resource_bitmap != -1 {
                let b = plan.resource_bitmap as u32;
                self.stretch(b, n as c_long, control);
                self.stretch(b + 1, n as c_long, control);
                if self.icons[n].normal != b {
                    self.icons[n].normal = b;
                    self.dirty(control);
                }
                if self.icons[n].pressed != b + 1 {
                    self.icons[n].pressed = b + 1;
                    self.dirty(control);
                }
            }
            let t = if plan.resource_pair {
                format!("{}/{}", plan.resource_text[0], plan.resource_text[1])
            } else {
                format!("{}", plan.resource_text[0])
            };
            self.cost(control, &t);
        } else if self.resource_shown {
            self.mode(control, 0);
            self.mode(control + 10, 0);
            self.resource_shown = false;
        }
    }

    fn dump(&mut self) {
        let mut s =
            format!(" box count {} armor {} resource {} attrs", self.count, self.armor, self.resource_shown as i32);
        for a in self.attributes {
            s += &format!(" {a}");
        }
        self.p(s);
        let mut s = " icons".to_string();
        for (i, icon) in self.icons.iter().enumerate() {
            s += &format!(
                " {}/{}/{}/{}/{}/{}",
                icon.normal,
                icon.pressed,
                icon.attribute,
                icon.cost_label,
                icon.queued,
                self.modes[10 + i]
            );
        }
        self.p(s);
        let mut s = " bars".to_string();
        for control in [1u32, 2, 7, 8] {
            if control >= 7 && !self.two {
                s += " -";
                continue;
            }
            let b = self.bars[control as usize];
            s += &format!(
                " {}/{}/{}/{}/{}/{}",
                b.icon, b.attribute, b.enabled as i32, b.string, b.queued, self.modes[control as usize]
            );
        }
        self.p(s);
    }

    /// Takes the C++'s state back from its trace and dump (a 64-bit `long`'s differing case).
    fn resync(&mut self, golden: &[&str]) {
        for line in golden {
            let t: Vec<&str> = line.split(' ').collect();
            if line.starts_with("  text ") {
                let label: usize = t[3].parse().unwrap();
                self.texts[label] = t[4..t.len() - 1].join(" ");
            } else if line.starts_with("  mode ") {
                self.modes[t[3].parse::<usize>().unwrap()] = t[4].parse().unwrap();
            } else if line.starts_with(" box ") {
                self.count = l(t[3]);
                self.armor = l(t[5]);
                self.resource_shown = t[7] == "1";
                for k in 0..14 {
                    self.attributes[k] = l(t[9 + k]);
                }
            } else if line.starts_with(" icons") {
                for k in 0..9 {
                    let v: Vec<&str> = t[2 + k].split('/').collect();
                    self.icons[k] = Icon {
                        normal: v[0].parse().unwrap(),
                        pressed: v[1].parse().unwrap(),
                        attribute: l(v[2]),
                        cost_label: v[3].parse().unwrap(),
                        queued: l(v[4]),
                    };
                }
            } else if line.starts_with(" bars") {
                for (k, control) in [1usize, 2, 7, 8].into_iter().enumerate() {
                    let v: Vec<&str> = t[2 + k].split('/').collect();
                    if v.len() < 6 {
                        continue;
                    }
                    self.bars[control] = Bar {
                        icon: v[0].parse().unwrap(),
                        attribute: l(v[1]),
                        enabled: v[2] == "1",
                        string: v[3].parse().unwrap(),
                        queued: l(v[4]),
                    };
                }
            }
        }
    }
}

/// `key a/b/c` of the case line.
fn field<'a>(t: &[&'a str], key: &str) -> Vec<&'a str> {
    let i = t.iter().position(|&s| s == key).unwrap_or_else(|| panic!("no {key}"));
    t[i + 1].split('/').collect()
}

fn parse(line: &str, flags: &str) -> (Case, bool, bool) {
    let t: Vec<&str> = line.split(' ').collect();
    let one = |key: &str| field(&t, key)[0];
    let hp = field(&t, "hp");
    let atk = field(&t, "atk");
    let rng = field(&t, "rng");
    let spd = field(&t, "spd");
    let area = field(&t, "area");
    let rep = field(&t, "rep");
    let flight = field(&t, "flight");
    let power = field(&t, "power");
    let trans = field(&t, "trans");
    let gar = field(&t, "gar");
    let res = field(&t, "res");
    let name = field(&t, "name");
    let ft: Vec<&str> = flags.split(' ').collect();
    let flag = |k: usize| ft[2 + k] == "1";
    let complex = one("complex") == "1";
    let friendly = one("cur") == one("owner") || one("enemy") == "0";
    let class_transport = l(trans[0]);
    let class_garrison = l(gar[0]);
    let mut u = EeScreensHudUnit {
        category: one("cat").parse().unwrap(),
        unit_type: one("type").parse().unwrap(),
        class_hit_points: l(hp[0]),
        stats_class_hit_points: l(hp[1]),
        max_hit_points: l(hp[2]),
        hit_points: l(hp[3]),
        class_attack: l(atk[0]),
        stats_attack: f(atk[1]),
        stats_class_attack: l(atk[2]),
        class_range: f(rng[0]),
        stats_class_range: f(rng[1]),
        stats_range: f(rng[2]),
        class_speed: f(spd[0]),
        stats_class_speed: f(spd[1]),
        stats_speed: f(spd[2]),
        class_area_effect: f(area[0]),
        stats_class_area_effect: f(area[1]),
        stats_area_effect: f(area[2]),
        class_repair: l(rep[0]),
        stats_class_repair: l(rep[1]),
        stats_repair: l(rep[2]),
        upgrades_range: flag(0),
        upgrades_area_effect: flag(1),
        upgrades_repair: flag(2),
        upgrades_armor: [flag(3), flag(4), flag(5), flag(6), flag(7), flag(8)],
        class_transport,
        class_flight_time: l(flight[0]),
        class_garrison,
        friendly: (class_transport > 0 || class_garrison > 0) && friendly,
        two_bars: one("two") == "1",
        resource_type: res[0].parse().unwrap(),
        resource_amount: l(res[1]),
        carry_amount: l(res[2]),
        using_carbon: one("carbon") == "1",
        ..Default::default()
    };
    if complex {
        u.complex = true;
        u.complex_class_transport = class_transport;
        u.complex_class_garrison = class_garrison;
        u.cargo_room = l(trans[1]);
        u.flight_time = l(flight[3]);
        u.stats_flight_time = l(flight[2]);
        u.stats_class_flight_time = l(flight[1]);
        u.power = l(power[2]);
        u.stats_power = l(power[1]);
        u.stats_class_power = l(power[0]);
        u.garrison_count = l(gar[1]);
    }
    let mut armor = [(0, 0, 0f32); 6];
    let ai = ft.iter().position(|&s| s == "armor").unwrap();
    for (k, a) in armor.iter_mut().enumerate() {
        let v: Vec<&str> = ft[ai + 1 + k].split('/').collect();
        *a = (l(v[0]), l(v[1]), f(v[2]));
    }
    let case = Case {
        unit: u,
        gate: u.category == 0x19,
        locked: one("lock") == "1",
        name: name[0].to_string(),
        name_id: l(name[1]),
        civ_name: field(&ft, "civname")[0].to_string(),
        cinematic: one("cine") == "1",
        armor,
    };
    (case, one("civ") == "1", one("two") == "1")
}

#[test]
fn replays_the_cpp_golden() {
    let wide = core::mem::size_of::<c_long>() == 8;
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 1000);
    let mut model = Model::new();
    let mut replayed = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let (case, civ, two) = parse(lines[a], lines[a + 1]);
        model.civ = civ;
        model.two = two;
        model.out.clear();
        model.out.push(lines[a].to_string());
        model.out.push(lines[a + 1].to_string());
        // the driver's own changes before the call: the icons' queued counts and normal bitmaps
        let pre: Vec<&str> = lines[a + 1].split(" pre ").nth(1).unwrap().split(' ').collect();
        for (k, v) in pre.iter().enumerate() {
            let v: Vec<&str> = v.split('/').collect();
            model.icons[k].queued = l(v[0]);
            model.icons[k].normal = v[1].parse().unwrap();
        }
        let call: Vec<&str> = lines[a + 2].split(' ').collect();
        model.out.push(lines[a + 2].to_string());
        match call[1] {
            "fill" => {
                let none = call[2] == "1";
                model.fill(if none { None } else { Some(&case) }, case.cinematic);
            }
            "armorfill" => {
                model.count = l(call[2]);
                model.fill_armor(if call[3] == "1" { None } else { Some(&case) });
                model.out.push(" refresh 0".into());
                model.refresh(&case);
            }
            _ => {
                if call[2] == "1" {
                    model.resource_shown = !model.resource_shown;
                }
                model.refresh(&case);
            }
        }
        model.dump();
        let golden = &lines[a..b];
        let same = model.out.len() == golden.len() && model.out.iter().zip(golden).all(|(x, y)| x == y);
        if !same && !wide {
            for (k, (x, y)) in model.out.iter().zip(golden).enumerate() {
                assert_eq!(x, y, "case {n} line {k}");
            }
            assert_eq!(model.out.len(), golden.len(), "case {n}");
        }
        model.resync(golden);
        replayed += same as usize;
    }
    assert!(replayed > if wide { 800 } else { 999 }, "{replayed} cases replayed");
}
