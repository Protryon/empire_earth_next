//! The settings form against the real C++ (`golden_settings.txt`): a scratch driver (`build/sc1/settings_driver.cpp`)
//! ran the unmodified `EEUIEGameSettingsEvents.cpp` over a form of fake controls (check boxes, radio buttons that
//! keep their group one-of, combo boxes; every `SetChecked`, `Check`, `SetCurrentSelection` and `SetDisplayMode` a
//! trace line): 2,000 cases of random control states (half of them a preset), each one operation of
//! `SetGraphicsQuality`, `ShowGraphics`, the advanced-settings and quality radio events, the quality combo box's
//! event, `PortShowVideo`, `PortShowInterface` and `PortApply` (with the port's combo boxes sometimes missing, the
//! quality guard sometimes set), printing the controls before and after and `GetGraphicsQuality`. The replay below
//! rebuilds every line from the case's printed inputs over this module, with the C++ forwarders' call sequences;
//! the driver relinked over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.
//!
//! The values are the Windows build's `long` (32 bits): on a 64-bit `long` the cases holding a value past 2^30
//! (the wrapping distances) are skipped.

#![allow(clippy::needless_range_loop)] // the replay mirrors the C++ index loops

use super::*;
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("../golden_settings.txt");
const CHECKS: [c_ulong; 12] = [13, 66, 82, 83, 84, 85, 86, 87, 88, 105, 110, 122];
const GROUPS: [&[c_ulong]; 5] = [&[67, 68, 69, 70, 71, 72], &[75, 76, 77], &[79, 80, 81], &[27, 28, 29], &[53, 54]];
const COMBOS: [c_ulong; 12] = [6, 109, 111, 112, 113, 114, 115, 116, 117, 118, 121, 123];
const NONE: i64 = -1;
const DEVICES: [&str; 3] = ["dev0", "dev1", "dev2"];

#[derive(Default)]
struct Form {
    on: BTreeMap<c_ulong, bool>,
    sel: BTreeMap<c_ulong, i64>,
    mode72: u64,
    missing: Vec<c_ulong>,
    guard: bool,
    slider: i64,
    out: Vec<String>,
}

#[derive(Clone, Default)]
struct Port {
    v: [i64; 8], // window mode, UI scale, render resolution, frame rate, filtering, brightness, zoom out, master
    mute: bool,
    device: String,
    full: bool,
    vsync: bool,
    w: u64,
    h: u64,
    bits: i64,
    tbits: i64,
}

impl Port {
    fn parse(line: &str) -> Port {
        let t: Vec<&str> = line.strip_prefix("  port ").unwrap().split(' ').collect();
        let n = |i: usize| t[i].parse::<i64>().unwrap();
        let mut v = [0; 8];
        for (i, x) in v.iter_mut().enumerate() {
            *x = n(i);
        }
        Port {
            v,
            mute: n(8) != 0,
            device: t[9].to_string(),
            full: n(10) != 0,
            vsync: n(11) != 0,
            w: t[12].parse().unwrap(),
            h: t[13].parse().unwrap(),
            bits: n(14),
            tbits: n(15),
        }
    }
    fn line(&self) -> String {
        let v = &self.v;
        format!(
            "  port {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
            v[0],
            v[1],
            v[2],
            v[3],
            v[4],
            v[5],
            v[6],
            v[7],
            self.mute as u8,
            self.device,
            self.full as u8,
            self.vsync as u8,
            self.w,
            self.h,
            self.bits,
            self.tbits
        )
    }
}

impl Form {
    fn present(&self, i: c_ulong) -> bool {
        !self.missing.contains(&i)
    }
    fn state(&self) -> String {
        let mut s = String::from("  state");
        for i in CHECKS {
            s += &format!(" c{i}={}", self.on[&i] as u8);
        }
        for g in GROUPS {
            for &i in g {
                s += &format!(" r{i}={}", self.on[&i] as u8);
            }
        }
        for i in COMBOS {
            s += &format!(" s{i}={}", self.sel[&i]);
        }
        s + &format!(" m72={}", self.mode72)
    }
    fn mode(&mut self, i: c_ulong, m: u64) {
        if i == 72 {
            self.mode72 = m;
        }
        self.out.push(format!("  mode {i} {m}"));
    }
    fn set(&mut self, i: c_ulong, v: bool) {
        self.on.insert(i, v);
        self.out.push(format!("  set {i} {}", v as u8));
    }
    fn select(&mut self, i: c_ulong, s: i64) {
        self.sel.insert(i, s);
        self.out.push(format!("  sel {i} {s}"));
    }
    fn check(&mut self, i: c_ulong) {
        self.out.push(format!("  check {i}"));
        for g in GROUPS {
            if g.contains(&i) {
                for &m in g {
                    self.on.insert(m, false);
                }
            }
        }
        self.on.insert(i, true);
    }
    fn graphics(&self) -> EeScreensGraphics {
        let mut g = EeScreensGraphics::default();
        for (k, i) in CHECK_CONTROLS.iter().enumerate() {
            g.checks[k] = self.on[i];
        }
        for k in 0..3 {
            g.model[k] = self.on[&MODEL_RADIOS[k]];
            g.effect[k] = self.on[&EFFECT_RADIOS[k]];
        }
        g
    }
    fn quality(&self) -> c_ulong {
        graphics_quality(&self.graphics())
    }
    fn check_detail(&mut self, radios: [c_ulong; 3], level: c_long) {
        if (0..=2).contains(&level) {
            self.check(radios[(2 - level) as usize]);
        }
    }
    fn set_quality(&mut self, q: c_ulong) {
        if let Some(p) = quality_preset(q) {
            for k in 0..9 {
                self.set(CHECK_CONTROLS[k], p.checks[k]);
            }
            self.check_detail(MODEL_RADIOS, p.model);
            self.check_detail(EFFECT_RADIOS, p.effect);
        }
    }
    fn show_quality(&mut self) {
        if !self.present(123) || self.guard {
            return;
        }
        let q = self.quality();
        if self.sel[&123] != NONE && self.sel[&123] == q as i64 {
            return;
        }
        self.select(123, q as i64);
    }
    fn radio_of_quality(&mut self, show_after_check: bool) {
        let radio = quality_radio(self.quality());
        self.mode(72, 0);
        let button = 67 + radio;
        if show_after_check {
            self.check(button);
            if radio == EE_SCREENS_QUALITY_CUSTOM {
                self.mode(72, 1);
            }
        } else {
            if radio == EE_SCREENS_QUALITY_CUSTOM {
                self.mode(72, 1);
            }
            if !self.on[&button] {
                self.check(button);
            }
        }
    }
    fn show_graphics(&mut self, s: &EeScreensGraphicsSettings) {
        let p = graphics_show(s);
        for k in 0..3 {
            self.set(CHECK_CONTROLS[k], p.checks[k]);
        }
        if self.present(111) {
            self.select(111, p.texture_line as i64);
        }
        for k in 4..9 {
            self.set(CHECK_CONTROLS[k], p.checks[k]);
        }
        self.check_detail(MODEL_RADIOS, p.model);
        self.check_detail(EFFECT_RADIOS, p.effect);
        self.set(CHECK_CONTROLS[3], p.checks[3]);
        self.radio_of_quality(true);
    }
    fn port_select(&mut self, i: c_ulong, t: u32, v: i64) {
        if self.present(i) {
            self.select(i, nearest_line(table(t).unwrap(), v as c_long) as i64);
        }
    }
    fn port_read(&self, i: c_ulong, t: u32, v: &mut i64) {
        if self.present(i) && self.sel[&i] != NONE {
            if let Some(x) = line_value(table(t).unwrap(), self.sel[&i] as c_ulong) {
                *v = x as i64;
            }
        }
    }
}

const PORT_TABLES: [(c_ulong, u32); 7] = [
    (112, EE_SCREENS_TABLE_WINDOW_MODE),
    (113, EE_SCREENS_TABLE_UI_SCALE),
    (114, EE_SCREENS_TABLE_RENDER_RESOLUTION),
    (115, EE_SCREENS_TABLE_FRAME_RATE),
    (116, EE_SCREENS_TABLE_TEXTURE_FILTERING),
    (117, EE_SCREENS_TABLE_BRIGHTNESS),
    (118, EE_SCREENS_TABLE_ZOOM_OUT),
];

fn run_case(lines: &[&str]) -> Option<Vec<String>> {
    let wide = core::mem::size_of::<c_long>() == 8;
    let big = |v: i64| !(-(1 << 30)..=(1 << 30)).contains(&v);
    let mut f = Form::default();
    let head: Vec<&str> = lines[0].split(' ').collect();
    let op: u32 = head[3].parse().unwrap();
    f.guard = head[5] == "1";
    f.slider = head[7].parse().unwrap();
    f.missing = head[9..].iter().map(|s| s.parse().unwrap()).collect();
    for kv in lines[1].split(' ').skip(3) {
        let (k, v) = kv.split_at(kv.find('=').unwrap());
        let (kind, i) = k.split_at(1);
        let v: i64 = v[1..].parse().unwrap();
        match kind {
            "c" | "r" => {
                f.on.insert(i.parse().unwrap(), v != 0);
            }
            "s" => {
                f.sel.insert(i.parse().unwrap(), v);
            }
            _ => f.mode72 = v as u64,
        }
    }
    f.out.push(lines[0].to_string());
    f.out.push(f.state());
    f.out.push(format!("  quality {}", f.quality()));
    let opline = lines[3];
    match op {
        0 => {}
        1 => {
            f.out.push(opline.to_string());
            f.set_quality(opline[12..].parse().unwrap());
        }
        2 => {
            f.out.push(opline.to_string());
            let n: Vec<i64> = opline.split(' ').skip(3).map(|s| s.parse().unwrap()).collect();
            if wide && n[8..].iter().any(|&v| big(v)) {
                return None;
            }
            let b = |i: usize| n[i] != 0;
            let s = EeScreensGraphicsSettings {
                trilinear_filter: b(0),
                unit_lighting: b(1),
                shadows: b(2),
                clouds: b(3),
                battlefield_ambients: b(4),
                weather_effects: b(5),
                anti_aliasing: b(6),
                wait_for_vsync: b(7),
                mip_levels_to_drop: n[8] as c_long,
                terrain_mip_levels_to_drop: n[9] as c_long,
                texture_resolution: n[10] as c_long,
                model_detail: n[11] as c_long,
                effect_detail: n[12] as c_long,
            };
            f.show_graphics(&s);
        }
        3 => {
            f.out.push(opline.to_string());
            f.radio_of_quality(false);
            f.show_quality();
        }
        4 => {
            f.out.push(opline.to_string());
            let k: c_ulong = opline[7..].parse().unwrap();
            if k != 0 && (67..=71).contains(&k) {
                f.set_quality(k - 67);
            }
        }
        5 => {
            f.out.push(opline.to_string());
            let s = f.sel[&123];
            if !(f.guard || !f.present(123) || s == NONE || s >= 5 || f.quality() as i64 == s) {
                let g = f.guard;
                f.guard = true;
                f.set_quality(s as c_ulong);
                f.guard = g;
            }
        }
        _ => {
            f.out.push(opline.to_string());
            let mut p = Port::parse(lines[4]);
            if wide && p.v.iter().any(|&v| big(v)) {
                return None;
            }
            f.out.push(p.line());
            if op == 6 {
                for i in [5, 6, 7, 8, 9, 10, 106, 82, 83, 84, 85] {
                    f.mode(i, 0);
                }
                for i in (14..=22).filter(|&i| i != 18 && i != 19) {
                    f.mode(i, 0);
                }
                for i in 67..=73 {
                    f.mode(i, 0);
                }
                f.show_quality();
                for (k, &(i, t)) in PORT_TABLES[..6].iter().enumerate() {
                    f.port_select(i, t, p.v[k]);
                }
            } else if op == 7 {
                for i in 107..=109 {
                    f.mode(i, 0);
                }
                f.port_select(118, EE_SCREENS_TABLE_ZOOM_OUT, p.v[6]);
            } else {
                let old = (p.v[0], p.v[1], p.v[2]);
                for (k, &(i, t)) in PORT_TABLES.iter().enumerate() {
                    f.port_read(i, t, &mut p.v[k]);
                }
                p.v[7] = f.slider;
                if f.present(122) {
                    p.mute = f.on[&122];
                }
                if f.present(121) && f.sel[&121] != NONE {
                    let d = f.sel[&121] as c_ulong;
                    p.device = if d > 0 && d <= 3 { DEVICES[d as usize - 1].to_string() } else { String::new() };
                }
                p.full = p.v[0] == 0;
                p.vsync = p.v[3] == 0;
                // the driver's GXGameSize: percent * 16 + windowed, percent * 9 (unsigned long arithmetic)
                let percent = p.v[1] as c_long as c_ulong;
                let windowed = p.v[0] != 0;
                f.out.push(format!("  gamesize {percent} {}", windowed as u8));
                p.w = percent.wrapping_mul(16).wrapping_add(windowed as c_ulong) as u64;
                p.h = percent.wrapping_mul(9) as u64;
                p.bits = 32;
                p.tbits = 32;
                f.out.push("  applyport".to_string());
                let changed = old != (p.v[0], p.v[1], p.v[2]);
                f.out.push(format!("  changed {}", changed as u8));
                f.out.push(p.line());
            }
        }
    }
    f.out.push(f.state());
    f.out.push(format!("  quality {}", f.quality()));
    Some(f.out)
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut replayed = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        if let Some(out) = run_case(&lines[a..b]) {
            assert_eq!(out, lines[a..b], "case {n}");
            replayed += 1;
        }
    }
    assert!(replayed > 1800, "{replayed} cases replayed");
}
