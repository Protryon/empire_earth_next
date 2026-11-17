//! `ApplySettings`, `ShowAudio` and `ShowInterface` against the real C++ (`golden_settings2.txt`): a scratch driver
//! (`build/sc2/settings2_driver.cpp`) ran the unmodified `EEUIEGameSettingsEvents.cpp` over fake controls and fake
//! engine objects (the sound manager's mixer, the conductor, the mouse, the texture manager, the ambient sounds,
//! the preferences' `Save`; every engine call a trace line, floats as their bits): 2,000 cases of random controls,
//! settings, defaults, mixer volume and channels (the extremes, NaN and the infinities included), each one of the
//! three. The replay below rebuilds every decision line from the case's printed inputs over this module with the C++
//! forwarders' call sequences; the lines of what stays C++ and outside these decisions (the rasterizer change, the
//! port's `PortApply`/`PortShowAudio`/`PortShowInterface` of round 1, the adapter box's lines, the game view
//! rectangle, `Save`) are taken from the golden in place. The driver relinked over the forwarders prints the same
//! file, and the same on seeds 2-4 at ten times the size.
//!
//! The golden is the Windows build's 32-bit `long`: on a 64-bit `long` the cases whose sliders or settings hold a
//! value whose arithmetic wraps differently are skipped.

#![allow(clippy::needless_range_loop)]

use super::*;
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("../golden_settings2.txt");
const GROUPS: [&[u32]; 5] = [&[67, 68, 69, 70, 71, 72], &[75, 76, 77], &[79, 80, 81], &[27, 28, 29], &[53, 54]];

fn f(hex: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(hex, 16).unwrap())
}
fn hex(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

/// The settings the driver prints (`prefs`), in its order.
#[derive(Clone, Debug)]
struct Prefs {
    trilinear: bool,
    lighting: bool,
    mip: i64,
    terrain_mip: i64,
    texres: i64,
    shadows: bool,
    clouds: bool,
    ambients: bool,
    weather: bool,
    model: i64,
    effect: i64,
    ambient_sounds: bool,
    max_sounds: f32,
    music_enabled: bool,
    reverb: bool,
    sensitivity: f32,
    side: f32,
    grab: f32,
    right: bool,
    autosave_ms: u64,
    autosaves: u64,
    blood: bool,
    zoom: bool,
    adapter: i64,
    sound_volume: f32,
    music_volume: f32,
}

impl Prefs {
    fn parse(line: &str) -> Prefs {
        let t: Vec<&str> = line.strip_prefix("  prefs ").unwrap().split(' ').collect();
        let b = |i: usize| t[i] == "1";
        let n = |i: usize| t[i].parse::<i64>().unwrap();
        Prefs {
            trilinear: b(0),
            lighting: b(1),
            mip: n(2),
            terrain_mip: n(3),
            texres: n(4),
            shadows: b(5),
            clouds: b(6),
            ambients: b(7),
            weather: b(8),
            model: n(9),
            effect: n(10),
            ambient_sounds: b(11),
            max_sounds: f(t[12]),
            music_enabled: b(13),
            reverb: b(14),
            sensitivity: f(t[15]),
            side: f(t[16]),
            grab: f(t[17]),
            right: b(18),
            autosave_ms: t[19].parse().unwrap(),
            autosaves: t[20].parse().unwrap(),
            blood: b(21),
            zoom: b(22),
            adapter: n(23),
            sound_volume: f(t[24]),
            music_volume: f(t[25]),
        }
    }
    fn line(&self) -> String {
        let b = |v: bool| v as u8;
        format!(
            "  prefs {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
            b(self.trilinear),
            b(self.lighting),
            self.mip,
            self.terrain_mip,
            self.texres,
            b(self.shadows),
            b(self.clouds),
            b(self.ambients),
            b(self.weather),
            self.model,
            self.effect,
            b(self.ambient_sounds),
            hex(self.max_sounds),
            b(self.music_enabled),
            b(self.reverb),
            hex(self.sensitivity),
            hex(self.side),
            hex(self.grab),
            b(self.right),
            self.autosave_ms,
            self.autosaves,
            b(self.blood),
            b(self.zoom),
            self.adapter,
            hex(self.sound_volume),
            hex(self.music_volume)
        )
    }
}

/// The controls the driver prints (`controls`): check boxes, radio buttons, sliders, three combo boxes.
#[derive(Default)]
struct Controls {
    on: BTreeMap<u32, bool>,
    value: BTreeMap<u32, i64>,
    order: Vec<(char, u32)>,
    tail: String,
}

impl Controls {
    fn parse(line: &str) -> Controls {
        let mut c = Controls::default();
        let mut tail = Vec::new();
        for kv in line.strip_prefix("  controls ").unwrap().split(' ') {
            let (k, v) = kv.split_at(kv.find('=').unwrap());
            let (kind, i) = k.split_at(1);
            let kind = kind.chars().next().unwrap();
            if kind == 's' {
                tail.push(kv);
                continue;
            }
            let i: u32 = i.parse().unwrap();
            let v: i64 = v[1..].parse().unwrap();
            c.order.push((kind, i));
            if kind == 'v' {
                c.value.insert(i, v);
            } else {
                c.on.insert(i, v != 0);
            }
        }
        c.tail = tail.join(" ");
        c
    }
    fn line(&self) -> String {
        let mut s = String::from("  controls");
        for &(kind, i) in &self.order {
            let v = if kind == 'v' { self.value[&i] } else { self.on[&i] as i64 };
            s += &format!(" {kind}{i}={v}");
        }
        s + " " + &self.tail
    }
    fn check(&mut self, i: u32, out: &mut Vec<String>) {
        out.push(format!("  check {i}"));
        for g in GROUPS {
            if g.contains(&i) {
                for &m in g {
                    self.on.insert(m, false);
                }
            }
        }
        self.on.insert(i, true);
    }
    fn set(&mut self, i: u32, v: bool, out: &mut Vec<String>) {
        self.on.insert(i, v);
        out.push(format!("  set {i} {}", v as u8));
    }
    fn range(&mut self, i: u32, v: i64, out: &mut Vec<String>) {
        self.value.insert(i, v);
        out.push(format!("  range {i} {v}"));
    }
}

/// The trace lines of what stays C++ outside these decisions (taken from the golden in place).
fn passthrough(line: &str) -> bool {
    const KEEP: [&str; 13] = [
        "  changerasterizer ",
        "  rasterizername",
        "  gamesize ",
        "  applyport",
        "  view ",
        "  save",
        "  result ",
        "  range 120 ",
        "  set 122 ",
        "  sel 121 ",
        "  mode 10",
        "  sel 118 ",
        "  clear 109",
    ];
    KEEP.iter().any(|k| line.starts_with(k)) || line.starts_with("  append 109") || line == "  string 32813"
}

fn run_case(lines: &[&str], mouse: &mut f32) -> Option<Vec<String>> {
    let wide = core::mem::size_of::<c_long>() == 8;
    let head: Vec<&str> = lines[0].split(' ').collect();
    let op: u32 = head[3].parse().unwrap();
    let mixer = f(head[7]);
    let channels: u64 = head[9].parse().unwrap();
    let conductor = f(head[11]);
    let adapters: u64 = head[13].parse().unwrap();
    let missing = head[15] == "1";
    let mut c = Controls::parse(lines[1]);
    let mut p = Prefs::parse(lines[2]);
    // a 64-bit long: only the cases whose arithmetic cannot wrap differently from the golden's 32 bits
    let plain = |x: f32| x.abs() < 1e6;
    if wide
        && (c.value.values().any(|&v| !(0..=1000).contains(&v))
            || channels >= 1 << 20
            || p.autosave_ms >= 1 << 31
            || p.autosaves >= 1 << 31
            || ![p.max_sounds, p.sensitivity, p.side, p.grab, p.sound_volume, p.music_volume, mixer, conductor]
                .iter()
                .all(|&x| plain(x)))
    {
        return None;
    }
    let mut out = vec![lines[0].to_string(), c.line(), p.line()];
    // the golden's lines line up with ours: what stays C++ is copied from where we are
    let copy = |out: &mut Vec<String>| {
        while out.len() < lines.len() && passthrough(lines[out.len()]) {
            out.push(lines[out.len()].to_string());
        }
    };
    match op {
        0 => {
            copy(&mut out);
            // the adapter box's selection (C++: ApplySettings reads it before the decisions)
            p.adapter = c.tail.split(' ').find_map(|kv| kv.strip_prefix("s109=")).unwrap().parse::<i64>().unwrap();
            let sel111: i64 = c.tail.split(' ').find_map(|kv| kv.strip_prefix("s111=")).unwrap().parse().unwrap();
            let v = |i: u32| c.value[&i] as c_long;
            let o = |i: u32| c.on[&i];
            let controls = EeScreensApplyControls {
                show_blood: o(13),
                trilinear: o(82),
                unit_lighting: o(83),
                full_textures: o(84),
                texture_selected: !missing && sel111 != -1,
                texture_line: sel111 as c_ulong,
                shadows: o(86),
                clouds: o(87),
                battlefield_ambients: o(110),
                weather_effects: o(88),
                model: [o(75), o(76), o(77)],
                effect: [o(79), o(80), o(81)],
                ambient_sounds: o(66),
                voices: v(39),
                max_channels: channels as c_ulong,
                music_off: o(29),
                reverb_on: o(27),
                reverb_off: o(28),
                mouse_sensitivity: v(56),
                side_scroll: v(49),
                grab_scroll: v(62),
                right_handed: o(53),
                autosave_minutes: v(91),
                autosave_count: v(94),
            };
            let mut a = EeScreensApplied {
                texture_resolution: p.texres as c_long,
                model_detail: p.model as c_long,
                effect_detail: p.effect as c_long,
                reverb: p.reverb,
                ..Default::default()
            };
            apply(&controls, &mut a);
            p.blood = a.show_blood;
            p.zoom = true;
            p.trilinear = a.trilinear;
            out.push(format!("  mipfilter {}", if a.trilinear { 2 } else { 1 }));
            p.lighting = a.unit_lighting;
            p.mip = a.mip_levels_to_drop as i64;
            p.terrain_mip = a.terrain_mip_levels_to_drop as i64;
            if a.set_texture_resolution {
                p.texres = a.texture_resolution as i64;
                out.push(format!("  texres {}", p.texres));
            }
            p.shadows = a.shadows;
            p.clouds = a.clouds;
            p.ambients = a.battlefield_ambients;
            p.weather = a.weather_effects;
            p.model = a.model_detail as i64;
            p.effect = a.effect_detail as i64;
            p.ambient_sounds = a.ambient_sounds;
            out.push(format!("  ambient {}", a.ambient_sounds as u8));
            p.sound_volume = mixer;
            p.max_sounds = a.max_number_of_sounds;
            out.push(format!("  channels {}", a.channels));
            p.music_enabled = a.music_enabled;
            p.reverb = a.reverb;
            p.music_volume = conductor;
            *mouse = a.mouse_sensitivity;
            p.sensitivity = a.mouse_sensitivity;
            p.side = a.side_scroll;
            p.grab = a.grab_scroll;
            out.push(format!("  lefthanded {}", !a.right_handed as u8));
            p.right = a.right_handed;
            p.autosave_ms = a.autosave_ms as u64;
            p.autosaves = a.autosaves as u64;
            copy(&mut out);
        }
        1 => {
            out.push(lines[3].to_string());
            let d: Vec<&str> = lines[3].split(' ').collect();
            let s = EeScreensAudioSettings {
                music_volume: p.music_volume,
                default_music_volume: f(d[3]),
                sound_volume: p.sound_volume,
                default_sound_volume: f(d[4]),
                max_number_of_sounds: p.max_sounds,
                reverb: p.reverb,
                music_enabled: p.music_enabled,
            };
            let show = audio_show(&s);
            let mut volume = conductor;
            if show.set_music_volume {
                volume = p.music_volume;
                out.push(format!("  conductorvolume {}", hex(volume)));
                out.push(format!("  uimusic {}", hex(volume)));
            }
            c.range(25, slider_percent(volume) as i64, &mut out);
            let mut master = mixer;
            if show.set_sound_volume {
                master = p.sound_volume;
                out.push(format!("  mastervolume {}", hex(master)));
            }
            c.range(35, slider_percent(master) as i64, &mut out);
            c.range(39, show.voices as i64, &mut out);
            c.check(if show.reverb_on { 27 } else { 28 }, &mut out);
            if show.music_off {
                c.check(29, &mut out);
            }
            c.set(66, p.ambient_sounds, &mut out);
            // PortShowAudio (round 1's, C++): the master slider, the mute box, the device line
            for (i, prefix) in [(120, "  range 120 "), (122, "  set 122 "), (121, "  sel 121 ")] {
                let line = lines[out.len()];
                assert!(line.starts_with(prefix), "{line}");
                let v: i64 = line[prefix.len()..].parse().unwrap();
                if i == 120 {
                    c.value.insert(120, v);
                } else if i == 122 {
                    c.on.insert(122, v != 0);
                }
                out.push(line.to_string());
            }
        }
        _ => {
            out.push(lines[3].to_string());
            let s = EeScreensInterfaceSettings {
                mouse_sensitivity: p.sensitivity,
                side_scroll: p.side,
                grab_scroll: p.grab,
                network_adapter: p.adapter as c_long,
                adapters: adapters as c_ulong,
                right_handed: p.right,
                autosave_ms: p.autosave_ms as c_ulong,
                autosaves: p.autosaves as c_ulong,
            };
            let show = interface_show(&s);
            c.range(56, show.mouse_sensitivity as i64, &mut out);
            *mouse = p.sensitivity;
            c.range(49, show.side_scroll as i64, &mut out);
            c.range(62, show.grab_scroll as i64, &mut out);
            copy(&mut out); // the adapter box's lines
            out.push(format!("  sel 109 {}", show.adapter_line));
            out.push(format!("  lefthanded {}", show.left_handed as u8));
            c.check(if show.left_handed { 54 } else { 53 }, &mut out);
            c.range(91, show.autosave_minutes as i64, &mut out);
            c.range(94, show.autosaves as i64, &mut out);
            out.push("  text 92".to_string());
            out.push("  text 95".to_string());
            copy(&mut out); // PortShowInterface
        }
    }
    out.push(format!("  mouse {}", hex(*mouse)));
    out.push(p.line());
    // the combo boxes the driver prints are the C++'s (the adapter box: this module's line)
    if op == 2 {
        let last = lines[lines.len() - 1];
        c.tail = last[last.find(" s6=").unwrap() + 1..].to_string();
    }
    out.push(c.line());
    Some(out)
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut mouse = 0.0f32;
    let mut replayed = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        match run_case(&lines[a..b], &mut mouse) {
            Some(out) => {
                for (k, (x, y)) in out.iter().zip(&lines[a..b]).enumerate() {
                    assert_eq!(x, y, "case {n} line {k}");
                }
                assert_eq!(out.len(), b - a, "case {n}");
                replayed += 1;
            }
            // a skipped case still leaves the mouse where the C++ put it
            None => mouse = f(lines[b - 3].strip_prefix("  mouse ").unwrap()),
        }
    }
    let wide = core::mem::size_of::<c_long>() == 8;
    assert!(replayed > if wide { 20 } else { 1900 }, "{replayed} cases replayed");
}

#[test]
fn conversions_follow_the_windows_build() {
    assert_eq!(slider_percent(0.59), 58); // 0.59f * 100 is 58.99998
    assert_eq!(slider_percent(-0.001), 0);
    assert_eq!(slider_percent(f32::NAN), c_long::MIN);
    assert_eq!(slider_percent(f32::INFINITY), c_long::MIN);
    assert_eq!(float_to_ulong(-149_652_047.0), (-149_652_048i64) as c_ulong);
    assert_eq!(float_to_ulong(f32::NAN), i64::MIN as c_ulong);
    assert_eq!(slider_factor(50), 0.5);
}
