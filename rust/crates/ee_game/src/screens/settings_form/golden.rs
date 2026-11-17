//! The settings form against the real C++: a scratch driver (`build/sc5/settings_driver.cpp`) ran the unmodified
//! `EEUIEGameSettingsEvents.cpp` over fake controls, forms, manager, user interface thread, mixer, conductor, sound
//! and music managers, rasterizers, displays and preferences: 2,000 cases of the form's 19 events (each open event,
//! both OKs, Cancel, Restore Default and its yes, the three tabs, the video card and rendering system lists, the
//! volume sliders, the music radios, the test sound, the quality radios, the advanced controls, the port's quality
//! box) with random settings and defaults, control states, display tables, engine state and entities; the form's
//! statics carried from case to case; every toolkit and engine call a trace line, the settings dumped when the port
//! applies them, when they are saved and after each case (`golden_settings_form.txt`). The replay runs [`dispatch`]
//! over a host that prints what the driver's fakes print for each host call and checks every line of every case.
//! The driver relinked over `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at ten
//! times the size.

use std::collections::HashMap;

use super::*;

const GOLDEN: &str = include_str!("../golden_settings_form.txt");

const GROUPS: [&[u32]; 5] = [&[67, 68, 69, 70, 71, 72], &[75, 76, 77], &[79, 80, 81], &[27, 28, 29], &[53, 54]];
const SOUNDS: [u32; 4] = [0x34, 0x47, 0xa0, 0x17a];

fn hex(s: &str) -> u32 {
    u32::from_str_radix(s, 16).unwrap()
}

/// A settings dump (`PrintSettings`): the record and the rasterizer name and audio device.
#[derive(Clone, Debug, Default)]
struct Dump {
    r: EeScreensSettingsRecord,
    rasterizer: String,
    device: String,
}

fn parse_dump(line: &str) -> Dump {
    // "  what f f f f f f n ... bools 'name' 'device'"
    let q = line.find('\'').unwrap();
    let (head, quoted) = line.split_at(q);
    let parts: Vec<&str> = quoted.split('\'').collect();
    let t: Vec<&str> = head.split_whitespace().collect();
    let f = |i: usize| f32::from_bits(hex(t[1 + i]));
    let l = |i: usize| t[1 + i].parse::<i64>().unwrap() as c_long;
    let u = |i: usize| t[1 + i].parse::<u64>().unwrap() as c_ulong;
    let b: Vec<bool> = t[28].bytes().map(|c| c == b'1').collect();
    assert_eq!(b.len(), 16);
    let r = EeScreensSettingsRecord {
        mouse_sensitivity: f(0),
        side_scroll_speed: f(1),
        grab_scroll_speed: f(2),
        sound_volume: f(3),
        music_volume: f(4),
        max_number_of_sounds: f(5),
        game_bit_depth: l(6),
        game_window_height: u(7),
        game_window_width: u(8),
        mip_levels_to_drop: l(9),
        model_detail: l(10),
        effect_detail: l(11),
        network_adapter: l(12),
        screen_id: l(13),
        terrain_mip_levels_to_drop: l(14),
        texture_bit_depth: l(15),
        number_of_auto_saves: u(16),
        auto_save_in_milliseconds: u(17),
        texture_resolution: l(18),
        ui_scale: l(19),
        render_resolution: l(20),
        window_mode: l(21),
        frame_rate_limit: l(22),
        texture_filtering: l(23),
        brightness: l(24),
        master_volume: l(25),
        maximum_zoom_out: l(26),
        zoom_enabled: b[0],
        display_clouds: b[1],
        display_shadows: b[2],
        display_battlefield_ambients: b[3],
        trilinear_filter: b[4],
        full_screen: b[5],
        reverb_enabled: b[6],
        wait_for_vsync: b[7],
        ambient_sounds: b[8],
        show_blood: b[9],
        music_enabled: b[10],
        right_hand_mouse_orientation: b[11],
        unit_lighting: b[12],
        display_weather_effects: b[13],
        anti_aliasing: b[14],
        mute_in_background: b[15],
    };
    Dump { r, rasterizer: parts[1].to_string(), device: parts[3].to_string() }
}

fn print_dump(what: &str, d: &Dump) -> String {
    let r = &d.r;
    let bools = [
        r.zoom_enabled,
        r.display_clouds,
        r.display_shadows,
        r.display_battlefield_ambients,
        r.trilinear_filter,
        r.full_screen,
        r.reverb_enabled,
        r.wait_for_vsync,
        r.ambient_sounds,
        r.show_blood,
        r.music_enabled,
        r.right_hand_mouse_orientation,
        r.unit_lighting,
        r.display_weather_effects,
        r.anti_aliasing,
        r.mute_in_background,
    ];
    let bools: String = bools.iter().map(|&b| if b { '1' } else { '0' }).collect();
    let floats = [
        r.mouse_sensitivity,
        r.side_scroll_speed,
        r.grab_scroll_speed,
        r.sound_volume,
        r.music_volume,
        r.max_number_of_sounds,
    ];
    let floats: Vec<String> = floats.iter().map(|f| format!("{:08x}", f.to_bits())).collect();
    let numbers = [
        r.game_bit_depth.to_string(),
        r.game_window_height.to_string(),
        r.game_window_width.to_string(),
        r.mip_levels_to_drop.to_string(),
        r.model_detail.to_string(),
        r.effect_detail.to_string(),
        r.network_adapter.to_string(),
        r.screen_id.to_string(),
        r.terrain_mip_levels_to_drop.to_string(),
        r.texture_bit_depth.to_string(),
        r.number_of_auto_saves.to_string(),
        r.auto_save_in_milliseconds.to_string(),
        r.texture_resolution.to_string(),
        r.ui_scale.to_string(),
        r.render_resolution.to_string(),
        r.window_mode.to_string(),
        r.frame_rate_limit.to_string(),
        r.texture_filtering.to_string(),
        r.brightness.to_string(),
        r.master_volume.to_string(),
        r.maximum_zoom_out.to_string(),
    ];
    format!("  {what} {} {} {bools} '{}' '{}'", floats.join(" "), numbers.join(" "), d.rasterizer, d.device)
}

struct Screen {
    name: String,
    index: c_ulong,
    modes: Vec<EeScreensVideoMode>,
}
struct Rasterizer {
    name: String,
    index: c_ulong,
    screens: Vec<Screen>,
}

fn mode(s: &str) -> EeScreensVideoMode {
    let (size, rate) = s.split_once('@').unwrap_or((s, "0"));
    let v: Vec<c_ulong> = size.split('x').map(|x| x.parse().unwrap()).collect();
    EeScreensVideoMode { width: v[0], height: v[1], refresh_rate: rate.parse().unwrap(), bit_count: v[2] }
}

fn parse_displays(line: &str) -> Vec<Rasterizer> {
    let body = line.trim_start().strip_prefix("displays").unwrap();
    let mut out = Vec::new();
    for r in body.split('[').skip(1) {
        let r = r.trim().trim_end_matches(']');
        let (head, rest) = match r.split_once(" (") {
            Some((h, x)) => (h, format!("({x}")),
            None => (r, String::new()),
        };
        let h: Vec<&str> = head.split_whitespace().collect();
        let mut rasterizer = Rasterizer { name: h[0].into(), index: h[1].parse().unwrap(), screens: Vec::new() };
        for s in rest.split('(').skip(1) {
            let t: Vec<&str> = s.trim().trim_end_matches(')').split_whitespace().collect();
            rasterizer.screens.push(Screen {
                name: t[0].into(),
                index: t[1].parse().unwrap(),
                modes: t[2..].iter().map(|m| mode(m)).collect(),
            });
        }
        out.push(rasterizer);
    }
    out
}

fn parse_list(s: &str) -> Vec<String> {
    let mut parts = s.split(':');
    let n: usize = parts.next().unwrap().parse().unwrap();
    let v: Vec<String> = parts.map(String::from).collect();
    assert_eq!(v.len(), n);
    v
}

#[derive(Default)]
struct List {
    data: Vec<String>,
    selection: c_ulong,
    has: bool,
}

/// The driver's fakes, as the host.
#[derive(Default)]
struct Driver {
    out: Vec<String>,
    settings: Dump,
    defaults: Dump,
    checks: HashMap<u32, bool>,
    radios: HashMap<u32, bool>,
    sliders: HashMap<u32, c_long>,
    combos: HashMap<u32, (c_ulong, Vec<String>)>,
    lists: HashMap<u32, List>,
    entity: u32,
    apply: (c_ulong, c_ulong),
    time: u32,
    mixer: u32,
    channels: u32,
    conductor: u32,
    conductor_state: u32,
    adapters: u32,
    devices: u32,
    server: bool,
    korean: bool,
    save: u32,
    master_result: u32,
    conductor_result: u32,
    rand: u32,
    display: (bool, i32, i32),
    sounds: [bool; 4],
    rasterizer_names: Vec<String>,
    current: u32,
    current_modes: Vec<EeScreensVideoMode>,
    displays: Vec<Rasterizer>,
    // carried from case to case
    music: String,
    sensitivity: u32,
    sliders_set: Vec<(u32, Vec<String>)>,
}

fn text(b: &[u8]) -> String {
    String::from_utf8(b.to_vec()).unwrap()
}

impl Driver {
    fn line(&mut self, s: String) {
        self.out.push(format!("  {s}"));
    }
}

impl SettingsHost for Driver {
    fn group(&mut self, _: Option<u32>, group: u32, enable: bool) {
        self.line(format!("group 5 {group} {}", if enable { "on" } else { "off" }));
    }
    fn background(&mut self, _: Option<u32>, id: u32, visible: bool) {
        self.line(format!("background 5 {id} {}", visible as u8));
    }
    fn checkbox(&mut self, control: u32) -> bool {
        self.checks[&control]
    }
    fn set_checkbox(&mut self, control: u32, checked: bool) {
        self.checks.insert(control, checked);
        self.line(format!("set {control} {}", checked as u8));
    }
    fn radio(&mut self, control: u32) -> bool {
        self.radios[&control]
    }
    fn check_radio(&mut self, control: u32) {
        self.line(format!("check {control}"));
        for g in GROUPS {
            if g.contains(&control) {
                for r in g {
                    self.radios.insert(*r, false);
                }
            }
        }
        self.radios.insert(control, true);
    }
    fn range(&mut self, control: u32) -> c_long {
        self.sliders[&control]
    }
    fn set_range(&mut self, control: u32, value: c_long) {
        self.sliders.insert(control, value);
        self.line(format!("range {control} {value}"));
    }
    fn entity_range(&mut self) -> c_long {
        self.sliders[&self.entity]
    }
    fn combo_selection(&mut self, control: u32) -> (c_long, c_ulong) {
        let s = self.combos[&control].0;
        (if s != c_ulong::MAX { 0 } else { 9 }, s)
    }
    fn list_selection(&mut self, control: u32) -> (c_long, c_ulong) {
        let l = &self.lists[&control];
        (if !l.data.is_empty() && l.has { 0 } else { 13 }, l.selection)
    }
    fn combo_select(&mut self, control: u32, line: c_ulong) {
        self.combos.get_mut(&control).unwrap().0 = line;
        self.line(format!("sel {control} {}", line as c_long));
    }
    fn list_select(&mut self, control: u32, line: c_ulong) {
        let l = self.lists.get_mut(&control).unwrap();
        l.selection = line;
        l.has = true;
        self.line(format!("listselect {control} {line} 0 1"));
    }
    fn combo_clear(&mut self, control: u32) {
        self.combos.get_mut(&control).unwrap().1.clear();
        self.line(format!("clear {control}"));
    }
    fn list_clear(&mut self, control: u32) {
        self.lists.get_mut(&control).unwrap().data.clear();
        self.line(format!("listclear {control}"));
    }
    fn combo_append(&mut self, control: u32, t: &[u8]) {
        self.combos.get_mut(&control).unwrap().1.push(text(t));
        self.line(format!("append {control} '{}'", text(t)));
    }
    fn combo_insert(&mut self, control: u32, line: u32, t: &[u8]) {
        let d = &mut self.combos.get_mut(&control).unwrap().1;
        let at = (line as usize).min(d.len());
        d.insert(at, text(t));
        self.line(format!("insert {control} {line} '{}'", text(t)));
    }
    fn list_append(&mut self, control: u32, t: &[u8]) {
        self.lists.get_mut(&control).unwrap().data.push(text(t));
        self.line(format!("listappend {control} '{}'", text(t)));
    }
    fn combo_data(&mut self, control: u32, line: c_ulong) -> Vec<u8> {
        self.combos[&control].1.get(line as usize).cloned().unwrap_or_default().into_bytes()
    }
    fn list_data(&mut self, control: u32, line: c_ulong) -> Vec<u8> {
        self.lists[&control].data.get(line as usize).cloned().unwrap_or_default().into_bytes()
    }
    fn release_mouse(&mut self, control: u32) {
        self.line(format!("release {control}"));
    }
    fn show(&mut self, control: u32, mode: u32) {
        self.line(format!("mode {control} {mode}"));
    }
    fn label_number(&mut self, control: u32, value: c_long) {
        self.line(format!("label {control} '{value}' 1"));
    }
    fn entity_id(&mut self) -> Option<c_ulong> {
        (self.entity != 0).then_some(1000 + self.entity as c_ulong)
    }
    fn control_id(&mut self, control: u32) -> c_ulong {
        1000 + control as c_ulong
    }
    fn trigger(&mut self, control: u32, trigger: u32, event: u32) {
        let name = match event {
            EE_SCREENS_GS_PORT_QUALITY => "class EEUIEGSEPortQuality",
            EE_SCREENS_GS_IN_GAME_OK => "class EEUIEGSEInGameOk",
            EE_SCREENS_GS_OUT_OF_GAME_OK => "class EEUIEGSEOutOfGameOk",
            _ => panic!("trigger {event:#x}"),
        };
        self.line(format!("trigger {control} {trigger} {name}"));
    }
    fn slider_label(&mut self, slider: u32, label: u32, off: Option<u32>) {
        self.line(format!("trigger {slider} 1000 slider {label} 3f800000 0"));
        let mut strings = Vec::new();
        if let Some(id) = off {
            let s = self.string(id);
            strings.push(format!("00000000:'{}'", text(&s)));
        }
        self.sliders_set.push((slider, strings));
    }
    fn music_file(&mut self) -> Vec<u8> {
        self.music.clone().into_bytes()
    }
    fn set_music_file(&mut self, t: &[u8]) {
        self.music = text(t);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.line(format!("string {id}"));
        format!("S{id}").into_bytes()
    }
    fn question(&mut self, t: &[u8]) {
        self.line(format!("question '{}' 1 5 1 0", text(t)));
    }
    fn pop(&mut self) {
        self.line("pop".into());
    }
    fn call(&mut self, op: u32) {
        self.call_with(op, 0, 0);
    }
    fn call_with(&mut self, op: u32, a: u32, b: u32) -> u32 {
        match op {
            EE_SCREENS_OP_CLOSE_FORM => self.line("closeform 0".into()),
            EE_SCREENS_OP_DISPLAY_CHANGED => self.line("displaychanged 0".into()),
            EE_SCREENS_OP_RESIZE_MENU => self.line("resize 1024 768 1 0".into()),
            EE_SCREENS_OP_SRAND_TIME => {
                let t = self.time;
                self.line(format!("srand {t}"))
            }
            EE_SCREENS_OP_RAND => {
                self.rand = self.rand.wrapping_mul(214013).wrapping_add(2531011);
                let v = (self.rand >> 16) & 0x7fff;
                self.line(format!("rand {v}"));
                return v;
            }
            EE_SCREENS_GAME_CONDUCTOR_VOLUME => {
                self.conductor = a;
                self.line(format!("conductorvolume {a:08x}"));
                return self.conductor_result;
            }
            EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME => return self.conductor,
            EE_SCREENS_GAME_REVERB => self.line(format!("reverb {a}")),
            EE_SCREENS_GAME_CONDUCTOR_STATE => {
                self.conductor_state = a;
                self.line(format!("conductorstate {a}"))
            }
            EE_SCREENS_GAME_GET_CONDUCTOR_STATE => return self.conductor_state,
            EE_SCREENS_GAME_UI_MUSIC_VOLUME => self.line(format!("uimusic {a:08x}")),
            EE_SCREENS_GAME_MASTER_VOLUME => {
                self.mixer = a;
                self.line(format!("mastervolume {a:08x}"));
                return self.master_result;
            }
            EE_SCREENS_GAME_GET_MASTER_VOLUME => return self.mixer,
            EE_SCREENS_GAME_GET_CHANNELS => return self.channels,
            EE_SCREENS_GAME_CHANNELS => self.line(format!("channels {a}")),
            EE_SCREENS_GAME_MUSIC_START => self.line("musicstart".into()),
            EE_SCREENS_GAME_MUSIC_STOP => self.line("musicstop".into()),
            EE_SCREENS_GAME_MOUSE_SENSITIVITY => self.sensitivity = a,
            EE_SCREENS_GAME_LEFT_HANDED => self.line(format!("lefthanded {a}")),
            EE_SCREENS_GAME_IS_SERVER => return self.server as u32,
            EE_SCREENS_GAME_PAUSE => self.line("pause".into()),
            EE_SCREENS_GAME_CURRENT_RASTERIZER => return if self.current < 3 { self.current } else { 0 },
            EE_SCREENS_GAME_CHANGE_RASTERIZER => self.line(format!("changerasterizer {} {}", a as i32, b as i32)),
            EE_SCREENS_GAME_APPLY_PORT => {
                let d = print_dump("applyport", &self.settings);
                self.out.push(d);
            }
            EE_SCREENS_GAME_VIEW => self.line("view 00000000 420c0000 44c80000 4487a000".into()),
            EE_SCREENS_GAME_MIP_FILTER => self.line(format!("mipfilter {a}")),
            EE_SCREENS_GAME_TEXTURE_RESOLUTION => self.line(format!("texres {}", a as i32)),
            EE_SCREENS_GAME_AMBIENT => self.line(format!("ambient {a}")),
            EE_SCREENS_GAME_SAVE => {
                let d = print_dump("save", &self.settings);
                self.out.push(d);
                return self.save;
            }
            EE_SCREENS_GAME_NOT_KOREAN => return self.korean as u32,
            EE_SCREENS_GAME_TEST_SOUND => {
                let k = SOUNDS.iter().position(|&s| s == a).unwrap();
                if self.sounds[k] {
                    self.line(format!("loadmedia {}", 500 + a));
                    self.line(format!("play {} 3f800000 00000000 3f800000 0 0", 500 + a));
                }
            }
            EE_SCREENS_GAME_WINDOW_MODE => {
                if self.settings.r.window_mode == 0 {
                    self.line("fixedsize 0".into());
                    self.line("fullscreen 1".into());
                } else {
                    self.line("fullscreen 0".into());
                    self.line("fixedsize 0".into());
                    let (ok, w, h) = self.display;
                    if ok && w > 0 {
                        self.line(format!("windowsize {} {}", w * 4 / 5, h * 4 / 5));
                    }
                    self.line("fixedsize 1".into());
                }
            }
            EE_SCREENS_GAME_RASTERIZER_COUNT => return self.displays.len() as u32,
            EE_SCREENS_GAME_SCREEN_COUNT => {
                return self.displays.get(a as usize).map_or(0, |r| r.screens.len() as u32);
            }
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn settings(&mut self, defaults: bool) -> EeScreensSettingsRecord {
        if defaults {
            self.defaults.r
        } else {
            self.settings.r
        }
    }
    fn put_settings(&mut self, settings: &EeScreensSettingsRecord) {
        self.settings.r = *settings;
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        let of = |d: &Driver| if arg != 0 { d.defaults.clone() } else { d.settings.clone() };
        let s: String = match which {
            EE_SCREENS_TEXT_RASTERIZER_SETTING => of(self).rasterizer,
            EE_SCREENS_TEXT_AUDIO_DEVICE => of(self).device,
            EE_SCREENS_TEXT_DISPLAY_RASTERIZER => self.displays[arg as usize].name.clone(),
            EE_SCREENS_TEXT_DISPLAY_SCREEN => {
                self.displays[(arg >> 16) as usize].screens[(arg & 0xffff) as usize].name.clone()
            }
            EE_SCREENS_TEXT_RASTERIZER_NAME => self.rasterizer_names.get(arg as usize).cloned().unwrap_or_default(),
            EE_SCREENS_TEXT_ADAPTERS => (0..self.adapters).map(|i| format!("A{i}\0")).collect(),
            EE_SCREENS_TEXT_OUTPUT_DEVICES => {
                let n = self.devices;
                self.line(format!("devices {n}"));
                (0..n)
                    .map(|i| if i == 2 { format!("dev{i} with a long name\0") } else { format!("dev{i}\0") })
                    .collect()
            }
            _ => panic!("text {which:#x}"),
        };
        s.into_bytes()
    }
    fn put_text(&mut self, which: u32, _: u32, t: &[u8]) {
        match which {
            EE_SCREENS_TEXT_RASTERIZER_SETTING => self.settings.rasterizer = text(t),
            EE_SCREENS_TEXT_AUDIO_DEVICE => self.settings.device = text(&t[..t.len().min(127)]),
            _ => panic!("put {which:#x}"),
        }
    }
    fn modes(&mut self, rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode> {
        for r in self.displays.iter().filter(|r| r.index == rasterizer) {
            if let Some(s) = r.screens.iter().find(|s| s.index == screen) {
                return s.modes.clone();
            }
        }
        Vec::new()
    }
    fn current_mode(&mut self, screen: c_ulong) -> EeScreensVideoMode {
        self.current_modes[screen as usize]
    }
    fn game_size(&mut self, percent: c_ulong, windowed: bool) -> (c_ulong, c_ulong) {
        self.line(format!("gamesize {percent} {}", windowed as u8));
        self.apply
    }
}

const KINDS: [u32; 19] = [
    EE_SCREENS_GS_OUT_OF_GAME_OPEN,
    EE_SCREENS_GS_IN_GAME_OPEN,
    EE_SCREENS_GS_OUT_OF_GAME_OK,
    EE_SCREENS_GS_IN_GAME_OK,
    EE_SCREENS_GS_CANCEL,
    EE_SCREENS_GS_RESTORE_DEFAULT,
    EE_SCREENS_GS_VIDEO_TAB,
    EE_SCREENS_GS_AUDIO_TAB,
    EE_SCREENS_GS_INTERFACE_TAB,
    EE_SCREENS_GS_VIDEO_CARD,
    EE_SCREENS_GS_RENDERING_SYSTEM,
    EE_SCREENS_GS_MUSIC_VOLUME,
    EE_SCREENS_GS_SOUND_VOLUME,
    EE_SCREENS_GS_MUSIC_QUALITY,
    EE_SCREENS_GS_TEST_SOUND,
    EE_SCREENS_GS_GRAPHICS_QUALITY,
    EE_SCREENS_GS_ADVANCED_CHANGED,
    EE_SCREENS_GS_PORT_QUALITY,
    EE_SCREENS_GS_RESTORE_YES,
];

fn fields(line: &str) -> HashMap<&str, &str> {
    line.split_whitespace().filter_map(|t| t.split_once('=')).collect()
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 2000);
    let state = Mutex::new(SettingsForm::new());
    let mut d = Driver::default();
    let mut seen = [0usize; 19];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let t: Vec<&str> = lines[a].split(' ').collect();
        let kind: usize = t[3].parse().unwrap();
        d.entity = t[5].parse().unwrap();
        let (w, h) = t[7].split_once('x').unwrap();
        d.apply = (w.parse().unwrap(), h.parse().unwrap());
        d.time = t[9].parse().unwrap();
        d.settings = parse_dump(lines[a + 1]);
        d.defaults = parse_dump(lines[a + 2]);
        let c = fields(lines[a + 3]);
        d.checks.clear();
        d.radios.clear();
        d.sliders.clear();
        d.combos.clear();
        for (k, v) in &c {
            let (tag, id) = k.split_at(1);
            let Ok(id) = id.parse::<u32>() else { continue };
            match tag {
                "c" => drop(d.checks.insert(id, *v == "1")),
                "r" => drop(d.radios.insert(id, *v == "1")),
                "v" => drop(d.sliders.insert(id, v.parse().unwrap())),
                "s" => drop(d.combos.insert(id, (v.parse::<i64>().unwrap() as c_ulong, Vec::new()))),
                _ => {}
            }
        }
        let (sel, has) = c["l8"].split_once('/').unwrap();
        let l = fields(lines[a + 4]);
        d.lists.clear();
        d.lists.insert(8, List { data: parse_list(l["l8"]), selection: sel.parse().unwrap(), has: has == "1" });
        d.lists.insert(10, List::default());
        d.combos.get_mut(&6).unwrap().1 = parse_list(l["c6"]);
        let e = fields(lines[a + 5]);
        d.mixer = hex(e["mixer"]);
        d.channels = e["channels"].parse().unwrap();
        d.conductor = hex(e["conductor"]);
        d.conductor_state = e["state"].parse().unwrap();
        d.adapters = e["adapters"].parse().unwrap();
        d.devices = e["devices"].parse().unwrap();
        d.server = e["server"] == "1";
        d.korean = e["korean"] == "1";
        d.save = e["save"].parse().unwrap();
        d.master_result = e["mres"].parse().unwrap();
        d.conductor_result = e["cres"].parse().unwrap();
        d.rand = e["rand"].parse().unwrap();
        let v: Vec<i32> = e["display"].split('/').map(|x| x.parse().unwrap()).collect();
        d.display = (v[0] != 0, v[1], v[2]);
        let s: Vec<&str> = lines[a + 6].split_whitespace().collect();
        for k in 0..4 {
            d.sounds[k] = s[1 + k] == "1";
        }
        let r: Vec<&str> = lines[a + 7].split_whitespace().collect();
        d.rasterizer_names = r[1..4].iter().map(|x| x.to_string()).collect();
        d.current = r[4].strip_prefix("current=").unwrap().parse().unwrap();
        d.current_modes = r[6..14].iter().map(|m| mode(m)).collect();
        d.displays = parse_displays(lines[a + 8]);
        d.out = lines[a..a + 9].iter().map(|x| x.to_string()).collect();
        d.sliders_set.clear();
        let result = dispatch(&mut d, &state, KINDS[kind]).unwrap();
        let music = d.music.clone();
        let sens = d.sensitivity;
        let (cond, cs, mixer) = (d.conductor, d.conductor_state, d.mixer);
        d.line(format!(
            "result {result} music '{music}' sens {sens:08x} conductor {cond:08x} state {cs} mixer {mixer:08x}"
        ));
        let fin = print_dump("final", &d.settings);
        d.out.push(fin);
        for (slider, strings) in std::mem::take(&mut d.sliders_set) {
            let mut s = format!("  sliderstrings {slider}");
            for x in strings {
                s.push(' ');
                s.push_str(&x);
            }
            d.out.push(s);
        }
        let want: Vec<String> = lines[a..b].iter().map(|x| x.to_string()).collect();
        for (i, (got, want)) in d.out.iter().zip(&want).enumerate() {
            assert_eq!(got, want, "case {n} line {i}");
        }
        assert_eq!(d.out.len(), want.len(), "case {n}: {:?}", &d.out[want.len().min(d.out.len())..]);
        seen[kind] += 1;
    }
    assert!(seen.iter().all(|&s| s > 50), "{seen:?}");
}
