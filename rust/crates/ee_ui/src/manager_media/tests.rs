//! The music bookkeeping and the IME members against the real C++ (`golden_media.txt`): a scratch driver built over
//! the original `UIManager.cpp` (the manager `calloc`ed with its lists constructed in place; a fake sound manager whose
//! loads, plays, stops and unloads are trace lines; fake instances, forms, keyboard controls, candidate window and
//! window; traced locks) printed 30 managers x 30 operations: `SetMusicVolume`, `StartMusic`, `StopMusic`,
//! `UpdateUI` with an empty form stack, `SetSoundInstance`, `RemoveAndStopSoundInstance`, or the four IME composition
//! calls, `ChangeIMECandidateWindow`, `OpenIMECandidateWindow`, `CloseIMECandidateWindow` and `CopyData`, each
//! followed by the manager's state. The replay below is the same world over this module; the same driver over the
//! forwarders prints the same file.

use super::*;

const GOLDEN: &str = include_str!("../golden_media.txt");

fn h(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}
fn fx(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}
fn rect_text(r: &UiRect) -> String {
    format!("{} {} {} {}", h(r.left), h(r.top), h(r.right), h(r.bottom))
}

const TRACKS: [&str; 8] = [
    "Sounds\\Menu.mp3",
    "sounds\\menu.mp3",
    "sounds\\BAD.mp3",
    "sounds\\game1.mp3",
    "SOUNDS\\GAME1.MP3",
    "sounds\\eeintro.mp3",
    "none",
    "",
];

/// The driver's globals: the sound ids and the instances run on across the managers.
struct Sound {
    next_sample: c_long,
    nodes: Vec<u64>, // reference counts
    play_fails: bool,
}

struct Form {
    id: c_ulong,
    captured: Option<(bool, UiRect)>,
}

struct World {
    sound: Sound,
    has_sound: bool,
    volume: f32,
    name: Vec<u8>,
    id: c_long,
    start: bool,
    stop: bool,
    instance: Instance,
    forms: Vec<Form>,
    visible: Vec<c_ulong>,
    window: Option<(f32, f32, c_ulong, c_ulong)>,
    open: bool,
    draw: bool,
    list: (c_ulong, c_ulong),
    log: Vec<String>,
}

fn node(i: Instance) -> i64 {
    if i.is_null() {
        -1
    } else {
        i as i64 - 1
    }
}

impl World {
    fn dump(&mut self) {
        let refs = if self.instance.is_null() { 0 } else { self.sound.nodes[node(self.instance) as usize] };
        let line = format!(
            "  state volume {} name [{}] id {} start {} stop {} instance {} refs {} open {} draw {}",
            h(self.volume),
            String::from_utf8_lossy(&self.name),
            self.id,
            self.start as u8,
            self.stop as u8,
            node(self.instance),
            refs,
            self.open as u8,
            self.draw as u8
        );
        self.log.push(line);
    }
    fn top(&self) -> &Form {
        self.forms.last().unwrap()
    }
}

impl MusicHost for World {
    fn has_sound_manager(&self) -> bool {
        self.has_sound
    }
    fn volume(&self) -> f32 {
        self.volume
    }
    fn set_volume(&mut self, v: f32) {
        self.volume = v;
    }
    fn file_name(&self) -> Vec<u8> {
        self.name.clone()
    }
    fn set_file_name(&mut self, name: &[u8]) {
        self.name = name.to_vec();
    }
    fn lower(&self, name: &[u8]) -> Vec<u8> {
        name.to_ascii_lowercase()
    }
    fn music_id(&self) -> c_long {
        self.id
    }
    fn set_music_id(&mut self, id: c_long) {
        self.id = id;
    }
    fn start_flag(&self) -> bool {
        self.start
    }
    fn set_start_flag(&mut self, v: bool) {
        self.start = v;
    }
    fn stop_flag(&self) -> bool {
        self.stop
    }
    fn set_stop_flag(&mut self, v: bool) {
        self.stop = v;
    }
    fn instance(&self) -> Instance {
        self.instance
    }
    fn set_instance_field(&mut self, i: Instance) {
        self.instance = i;
    }
    fn reference(&mut self, i: Instance) {
        self.sound.nodes[node(i) as usize] += 1;
    }
    fn dereference(&mut self, i: Instance) {
        self.sound.nodes[node(i) as usize] -= 1;
    }
    fn stop(&mut self, i: Instance) {
        self.log.push(format!("  stop {}", node(i)));
    }
    fn unload(&mut self, id: c_long) {
        self.log.push(format!("  unload {id}"));
    }
    fn load(&mut self, name: &[u8]) -> c_long {
        let id = if name.windows(3).any(|w| w == b"BAD") {
            -1
        } else {
            self.sound.next_sample += 1;
            self.sound.next_sample - 1
        };
        self.log.push(format!("  load [{}] = {id}", String::from_utf8_lossy(name)));
        id
    }
    fn play(&mut self, id: c_long, volume: f32) -> Instance {
        if self.sound.play_fails || self.sound.nodes.len() == 64 {
            self.log.push(format!("  play {id} {} = none", h(volume)));
            return core::ptr::null_mut();
        }
        self.sound.nodes.push(100);
        let n = self.sound.nodes.len();
        self.log.push(format!("  play {id} {} = {}", h(volume), n - 1));
        n as Instance
    }
    fn set_parameters(&mut self, i: Instance, volume: f32) {
        self.log.push(format!("  params {} {}", node(i), h(volume)));
    }
    fn lock(&mut self) {
        self.log.push("  lock".into());
    }
    fn unlock(&mut self) {
        self.log.push("  unlock".into());
    }
}

impl ImeHost for World {
    fn lock(&mut self) {
        self.log.push("  lock".into());
    }
    fn unlock(&mut self) {
        self.log.push("  unlock".into());
    }
    fn has_form(&self) -> bool {
        !self.forms.is_empty()
    }
    fn form_call(&mut self, call: ImeCall) -> c_long {
        let id = self.top().id;
        let (line, r) = match call {
            ImeCall::AddResult => (format!("  form {id} result [kana]"), 0),
            ImeCall::ChangeComposition => {
                (format!("  form {id} change [comp] [attr] {}", self.list.0), id as c_long + 10)
            }
            ImeCall::CloseComposition => (format!("  form {id} close"), id as c_long + 20),
            ImeCall::OpenComposition => (format!("  form {id} open"), id as c_long + 30),
        };
        self.log.push(line);
        r
    }
    fn captured_ime(&mut self, _current: bool) -> bool {
        let id = self.top().id;
        match self.top().captured {
            Some((ime, _)) => {
                self.log.push(format!("  ime {}", id - 1));
                ime
            }
            None => false,
        }
    }
    fn captured_rect(&self, _current: bool) -> UiRect {
        self.top().captured.unwrap().1
    }
    fn window(&mut self) -> Option<(f32, f32, c_ulong, c_ulong)> {
        if self.window.is_some() {
            self.log.push("  origin".into());
            self.log.push("  origin".into());
        }
        self.window
    }
    fn candidate_open(&self) -> bool {
        self.open
    }
    fn set_candidate_open(&mut self, v: bool) {
        self.open = v;
    }
    fn set_draw_candidate(&mut self, v: bool) {
        self.draw = v;
    }
    fn candidate_set_list(&mut self, left: f32) {
        let (s, c) = self.list;
        self.log.push(format!("  set-list {s} {c} {}", h(left)));
    }
    fn candidate_open_window(&mut self, screen: UiRect, control: UiRect) {
        self.log.push(format!("  open-window {}", rect_text(&screen)));
        self.log.push(format!("  control {}", rect_text(&control)));
    }
    fn candidate_close(&mut self) {
        self.log.push("  close-window".into());
    }
    fn candidate_copy(&mut self) -> c_long {
        self.log.push("  candidate-copy".into());
        7
    }
}

#[test]
fn replays_the_cpp_music_and_ime() {
    let mut out: Vec<String> = Vec::new();
    let mut w: Option<World> = None;
    for line in GOLDEN.lines() {
        if line.starts_with("  ") {
            continue;
        }
        if let Some(w) = w.as_mut() {
            out.append(&mut w.log);
        }
        out.push(line.to_string());
        let t: Vec<&str> = line.split(' ').collect();
        if t[0] == "manager" {
            // the driver's sound ids and instances run on across the managers
            let sound =
                w.take().map_or(Sound { next_sample: 5, nodes: Vec::new(), play_fails: false }, |old| old.sound);
            let window = (t[9] == "1").then(|| (fx(t[12]), fx(t[13]), t[10].parse().unwrap(), t[11].parse().unwrap()));
            w = Some(World {
                sound,
                has_sound: t[3] == "1",
                volume: fx(t[5]),
                name: Vec::new(),
                id: -1,
                start: false,
                stop: false,
                instance: core::ptr::null_mut(),
                forms: Vec::new(),
                visible: Vec::new(),
                window,
                open: false,
                draw: false,
                list: (0, 0),
                log: Vec::new(),
            });
            continue;
        }
        let w = w.as_mut().unwrap();
        let fails = |t: &[&str]| t.last() == Some(&"1");
        match t[0] {
            "form" => {
                let captured = (t.len() > 2).then(|| {
                    let r = UiRect::new(fx(t[5]), fx(t[6]), fx(t[7]), fx(t[8]));
                    (t[4] == "1", r)
                });
                w.forms.push(Form { id: t[1].parse().unwrap(), captured });
                continue;
            }
            "visible" => {
                w.visible = t[1..].iter().map(|v| v.parse().unwrap()).collect();
                continue;
            }
            "volume" => {
                w.sound.play_fails = fails(&t);
                set_music_volume(w, fx(t[1]));
            }
            "start" => {
                w.sound.play_fails = fails(&t);
                start_music(w, TRACKS[t[1].parse::<usize>().unwrap()].as_bytes());
            }
            "stop" => {
                w.sound.play_fails = fails(&t);
                stop_music(w);
            }
            "update" => {
                w.sound.play_fails = fails(&t);
                // UpdateKeyboard and UpdateMouse lock and find no form; the events (none) and the music, locked
                for _ in 0..2 {
                    MusicHost::lock(w);
                    MusicHost::unlock(w);
                }
                MusicHost::lock(w);
                update_music(w);
                MusicHost::unlock(w);
            }
            "remove-stop" => remove_and_stop_sound_instance(w),
            "instance" => {
                let k: i64 = t[1].parse().unwrap();
                set_sound_instance(w, if k < 0 { core::ptr::null_mut() } else { (k + 1) as Instance });
            }
            "result" => {
                ime_form_call(w, ImeCall::AddResult);
            }
            "change" => {
                w.list.0 = t[1].parse().unwrap();
                let r = ime_form_call(w, ImeCall::ChangeComposition);
                w.log.push(format!("  = {r}"));
            }
            "close" => {
                let r = ime_form_call(w, ImeCall::CloseComposition);
                w.log.push(format!("  = {r}"));
            }
            "open" => {
                let r = ime_form_call(w, ImeCall::OpenComposition);
                w.log.push(format!("  = {r}"));
            }
            "candidates" => {
                w.list = (t[1].parse().unwrap(), t[2].parse().unwrap());
                let r = change_candidate_window(w, w.list.0, w.list.1);
                w.log.push(format!("  = {r}"));
            }
            "open-candidates" => {
                let r = open_candidate_window(w);
                w.log.push(format!("  = {r}"));
            }
            "close-candidates" => {
                let r = close_candidate_window(w);
                w.log.push(format!("  = {r}"));
            }
            "copy" => {
                // the forms' copies stay C++ (UIManager::CopyData's loop): the last one's result
                let mut r = 0;
                for id in w.visible.clone() {
                    w.log.push(format!("  form {id} copy"));
                    r = id as c_long;
                }
                let r = copy_candidate_window(w, r);
                w.log.push(format!("  = {r}"));
            }
            other => panic!("unknown line {other}"),
        }
        w.dump();
    }
    if let Some(w) = w.as_mut() {
        out.append(&mut w.log);
    }
    let golden: Vec<&str> = GOLDEN.lines().collect();
    assert!(golden.len() > 2000, "the golden is the driver's output");
    for (i, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(out.len(), golden.len());
}
