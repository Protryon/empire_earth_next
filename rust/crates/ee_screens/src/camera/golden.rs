//! The camera, scroll and zoom events and the formation change against the real C++: a scratch driver
//! (`build/sc9/view_driver.cpp`) ran the unmodified `EEUIECameraEvents.cpp`, `EEUIEScrollZoomEvents.cpp`,
//! `EEUIEWheelZoom.cpp` and `EEUIEFormationChange.cpp` over a fake camera (its mode and zoom; Revolve, Rotate,
//! Scroll, SetMode, SetZoom, ResetZoom, SetChanged), the mouse's axes, middle button and wheel (at times absent), the
//! game mouse's freeze, the settings, the two forms the grab scroll hangs on (their mouse move events), the selected
//! unit's commands, the formation, the selected group and the formation buttons: one session of 2,000 cases, each
//! perturbing the state (printed), then firing one event, a grab on its form, a form's mouse move event, or making a
//! form's grab anew (`golden_camera.txt`; `UMCos` a stand-in the replay computes the same). The replay keeps the same
//! world and prints what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp` and
//! this crate prints the same file, and the same on seeds 2-4 at 20,000 cases.

use super::*;
use crate::main_game::{
    self, MainGameHost, EE_SCREENS_GAME_MG_COPY_BITMAPS, EE_SCREENS_GAME_MG_SELECTED_COMMAND,
    EE_SCREENS_GAME_MG_SET_FORMATION, EE_SCREENS_MG_FORMATION,
};

const GOLDEN: &[u8] = include_bytes!("../golden_camera.txt");

const GRAB_NAMES: [&str; 2] = ["class EEUIEGrabScroll(main)", "class EEUIEGrabScroll(editor)"];
const FORMS: [&str; 2] = ["main", "editor"];

#[derive(Default)]
struct World {
    out: Vec<String>,
    values: [i32; 4],
    normalized: [i32; 4],
    present: [bool; 4],
    mode: u32,
    zoom: u32,
    speed: u32,
    zoom_on: bool,
    wheel_on: bool,
    cinema: bool,
    free: bool,
    max: u32,
    unit: [bool; 3],
    commands: [u32; 9],
    frozen: bool,
    moves: [String; 2],
    buttons: [(i32, i32); 9],
    /// The form the event runs on (0 main, 1 editor).
    form: usize,
}

fn f(bits: u32) -> f32 {
    f32::from_bits(bits)
}

impl CameraHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_CAM_FREEZE => self.frozen = a != 0,
            EE_SCREENS_GAME_CAM_MOUSE_PRESENT => return self.present[a as usize] as u32,
            EE_SCREENS_GAME_CAM_MOUSE_VALUE => {
                return if b != 0 { self.normalized[a as usize] } else { self.values[a as usize] } as u32
            }
            EE_SCREENS_GAME_CAM_FREE => return self.free as u32,
            EE_SCREENS_GAME_CAM_REVOLVE => self.out.push(format!("  revolve {a:08x} {b:08x} 00000000")),
            EE_SCREENS_GAME_CAM_ROTATE => self.out.push(format!("  rotate {a:08x} {b:08x} 00000000")),
            EE_SCREENS_GAME_CAM_ZOOM => return self.zoom,
            EE_SCREENS_GAME_CAM_GRAB_SPEED => return self.speed,
            EE_SCREENS_GAME_CAM_SCROLL => self.out.push(format!("  scroll {a:08x} {b:08x}")),
            EE_SCREENS_GAME_CAM_ZOOM_ENABLED => return if a != 0 { self.wheel_on } else { self.zoom_on } as u32,
            EE_SCREENS_GAME_CAM_CINEMATIC => return self.cinema as u32,
            EE_SCREENS_GAME_CAM_RESET_ZOOM => {
                self.out.push("  resetzoom".into());
                self.zoom = 1.0f32.to_bits();
            }
            EE_SCREENS_GAME_CAM_CHANGED => self.out.push("  camchanged".into()),
            EE_SCREENS_GAME_CAM_SET_ZOOM => {
                self.out.push(format!("  setzoom {a:08x}"));
                self.zoom = a;
            }
            EE_SCREENS_GAME_CAM_MAX_ZOOM => return self.max,
            // (the driver's stand-in for UMCos)
            EE_SCREENS_GAME_CAM_COS => return (1.0 - f(a) * f(a) * 0.5).to_bits(),
            EE_SCREENS_OP_CAM_GRAB_BEGIN => {
                let form = self.form;
                assert_eq!(a as usize, form, "a grab runs on its own form");
                self.out.push(format!("  mousemove {} {}", FORMS[form], GRAB_NAMES[form]));
                self.moves[form] = format!("saved:{}", self.moves[form]);
            }
            EE_SCREENS_OP_CAM_GRAB_END => {
                let form = self.form;
                // (the form's mouse move event is the grab while it holds; the saved one comes back)
                let saved = self.moves[form].strip_prefix("saved:").expect("a held grab").to_string();
                self.out.push(format!("  mousemove {} {saved}", FORMS[form]));
                self.moves[form] = saved;
            }
            EE_SCREENS_GAME_MG_CAMERA_MODE => return self.mode,
            EE_SCREENS_GAME_MG_SET_CAMERA_MODE => {
                self.out.push(format!("  setmode {}", a as i32));
                self.mode = a;
            }
            EE_SCREENS_GAME_MG_SELECTED_COMMAND => {
                return if self.unit.iter().all(|&u| u) { self.commands[a as usize] } else { 0 };
            }
            EE_SCREENS_GAME_MG_SET_FORMATION => {
                self.out.push(format!("  formationcreate {a}"));
                self.out.push(format!("  local 120 255 formation {a} flag 1"));
                self.out.push("  groupdisable".into());
                self.out.push("  backgroundbyid main 152".into());
                self.out.push("  background 0".into());
                self.out.push("  setformation b5c 1 refs 1".into());
            }
            EE_SCREENS_GAME_MG_COPY_BITMAPS => {
                // the formation buttons' bitmaps: normal 0..7, pressed 10..17
                let f = (b - 236) as i32;
                self.buttons[(a - 0x4e) as usize] = (f, 10 + f);
                self.out.push(format!("  dirty {a} 1"));
                self.out.push(format!("  dirty {a} 1"));
            }
            other => panic!("unexpected op {other:#x}"),
        }
        0
    }
}

impl MainGameHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        CameraHost::call(self, op, a, b)
    }
    fn put_text(&mut self, which: u32, _: u32, _: &[u8]) {
        panic!("unexpected text {which:#x}");
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        panic!("unexpected string {id}");
    }
}

impl World {
    /// The form's mouse move events as the driver names them, the grab's saved one behind it while it holds.
    fn shown(&self, form: usize) -> String {
        if self.moves[form].starts_with("saved:") {
            GRAB_NAMES[form].to_string()
        } else {
            self.moves[form].clone()
        }
    }
}

fn split3(t: &str) -> (i32, i32, bool) {
    let p: Vec<&str> = t.split('/').collect();
    (p[0].parse().unwrap(), p[1].parse().unwrap(), p[2] == "1")
}

fn read_state(w: &mut World, line: &str) {
    let t: Vec<&str> = line.split_whitespace().collect();
    let at = |key: &str| t.iter().position(|x| *x == key).unwrap() + 1;
    for (k, key) in ["x", "y"].iter().enumerate() {
        let (v, n, p) = split3(t[at(key)]);
        w.values[k] = v;
        w.normalized[k] = n;
        w.present[k] = p;
    }
    let mid: Vec<&str> = t[at("mid")].split('/').collect();
    w.values[2] = mid[0].parse().unwrap();
    w.present[2] = mid[1] == "1";
    let (v, n, p) = split3(t[at("z")]);
    w.values[3] = v;
    w.normalized[3] = n;
    w.present[3] = p;
    w.mode = t[at("mode")].parse::<i32>().unwrap() as u32;
    w.zoom = u32::from_str_radix(t[at("zoom")], 16).unwrap();
    w.speed = u32::from_str_radix(t[at("speed")], 16).unwrap();
    w.zoom_on = t[at("zoomon")] == "1";
    w.wheel_on = t[at("wheelon")] == "1";
    w.cinema = t[at("cinema")] == "1";
    w.free = t[at("free")] == "1";
    w.max = u32::from_str_radix(t[at("max")], 16).unwrap();
    let u = at("unit");
    for k in 0..3 {
        w.unit[k] = t[u + k] == "1";
    }
    let c = at("commands");
    for k in 0..9 {
        w.commands[k] = t[c + k].parse().unwrap();
    }
}

fn event_of(kind: &str) -> u32 {
    match kind {
        "disable" => EE_SCREENS_CAM_DISABLE_MOUSE_MOVE,
        "enable" => EE_SCREENS_CAM_ENABLE_MOUSE_MOVE,
        "revolve" => EE_SCREENS_CAM_REVOLVE,
        "rotate" => EE_SCREENS_CAM_ROTATE,
        "wheel" => EE_SCREENS_CAM_WHEEL_ZOOM,
        _ if kind.starts_with("scroll") => EE_SCREENS_CAM_SCROLL_UP + kind[6..].parse::<u32>().unwrap(),
        _ if kind.starts_with("zoom") => EE_SCREENS_CAM_ZOOM_IN + kind[4..].parse::<u32>().unwrap(),
        _ if kind.starts_with("formation") => EE_SCREENS_MG_FORMATION + kind[9..].parse::<u32>().unwrap(),
        other => panic!("no kind {other}"),
    }
}

#[test]
fn golden_camera() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut lines = text.lines();
    let start: Vec<&str> = lines.next().unwrap().split(' ').collect();
    assert_eq!(start[0], "start");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in lines {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let grabs = Mutex::new(CameraGrabs::default());
    let mut w = World { buttons: [(18, 19); 9], ..Default::default() };
    // the forms' mouse move events (names of the driver's; "null" none)
    let rest = start[1..].join(" ");
    let (main, editor) = rest.strip_prefix("main ").unwrap().split_once(" editor ").unwrap();
    w.moves = [main.to_string(), editor.to_string()];
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        let head: Vec<&str> = case[0].splitn(4, ' ').collect();
        let kind = head[2];
        let what = head.get(3).copied().unwrap_or("");
        read_state(&mut w, case[1]);
        w.out.clear();
        let form = |name: &str| FORMS.iter().position(|f| name.starts_with(f)).unwrap();
        let mut event = None;
        match kind {
            "grab" => {
                w.form = form(what);
                event = Some(EE_SCREENS_CAM_GRAB_MAIN + w.form as u32);
            }
            "mousemove" => {
                w.form = form(what);
                let name = &what[FORMS[w.form].len() + 1..];
                assert_eq!(name, w.shown(w.form), "case {n}");
                if let Some(g) = GRAB_NAMES.iter().position(|g| *g == name) {
                    event = Some(EE_SCREENS_CAM_GRAB_MAIN + g as u32);
                } else if name != "null" {
                    w.out.push("  dummy run".into());
                    w.out.push("  result 0".into());
                }
            }
            "newgrab" => {
                let g = form(what);
                reset_grab(&grabs, g);
                let move_name = what[FORMS[g].len() + 1..].strip_prefix(GRAB_NAMES[g]).unwrap().trim_start();
                w.moves[g] = move_name.to_string();
            }
            _ => {
                w.form = form(what);
                event = Some(event_of(kind));
            }
        }
        kinds.insert(kind.to_string());
        if let Some(event) = event {
            let result = if (EE_SCREENS_MG_FORMATION..EE_SCREENS_MG_FORMATION + 8).contains(&event) {
                main_game::dispatch(&mut w, event).unwrap()
            } else {
                dispatch(&mut w, &grabs, event).unwrap()
            };
            w.out.push(format!("  result {result}"));
        }
        let mut after = format!(
            "  after mode {} zoom {:08x} frozen {} move main {} editor {} buttons",
            w.mode as i32,
            w.zoom,
            w.frozen as i32,
            w.shown(0),
            w.shown(1)
        );
        for (normal, pressed) in w.buttons {
            after += &format!(" {normal}/{pressed}");
        }
        w.out.push(after);
        let expected = &case[2..];
        for (k, e) in expected.iter().enumerate() {
            assert!(k < w.out.len(), "case {n} ({}): missing line {k}: {e}", case[0]);
            assert_eq!(w.out[k], *e, "case {n} ({}) line {k}", case[0]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 22);
}
