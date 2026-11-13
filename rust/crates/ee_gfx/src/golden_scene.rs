//! `scene.rs` and `capture.rs` against the unmodified C++ (`golden_scene.txt`). A scratch driver
//! (`build/gx16/scene_driver`; PORT_LEARNINGS "R4.4, tenth round") took `EEStartLetterboxOut`/`In`,
//! `EEStartLightFadeIn`/`Out`, `EERenderObject::Fn10C036`'s clocks, fade, letterbox and time-of-day blend,
//! `FillObject`'s and `FillDoppelgangerObject`'s place, size and sway, `FillRenderLists`' sway clock and the whole of
//! `DrawGroundCircle` verbatim out of `EERenderObject.cpp` of the commit before this work (the real `GEColor`,
//! `GEVector`, `GE3DPoint`, `GERectangle` and `UMFloor` headers, `UMCos` from `ee_ge`), over a stub world whose
//! rasterizer, view setter and UI log their arguments and whose terrain is a 64 x 48 slope. From an LCG seeded 1 it
//! made 1,200 operations (fade and letterbox starts over 0 to 120 frames, frame steps of 0 ms, up to 100 ms or up to
//! 5,000 s with a random wind and window size) printing each step's clears, view band, background calls, fade change
//! and clocks and the state after every operation; six random light tables with 60 times of day each (every key
//! frame, the moon's bounds, past 1); 400 map objects and their remembered copies (negative ids among them); 200
//! sway clock frames (one going back in time); 300 rings (an FNV-1a 64 of their points). This test replays the lines
//! through `scene.rs` and `capture.rs`; the driver rebuilt from the new unit over the forwarders printed the same
//! bytes.

use ee_ge::types::GeVector;

use crate::capture::{ground_circle, object_place, object_scale, sway_fraction, sway_tick, GroundHeights};
use crate::effects::EeGfxColor;
use crate::scene::{
    daylight, letterbox, start_fade, start_letterbox, step, step_clocks, EeGfxDaylightTables, EeGfxSceneClocks,
    EeGfxSceneFade,
};

const GOLDEN: &str = include_str!("golden_scene.txt");

fn hex(s: &str) -> u32 {
    u32::from_str_radix(s, 16).expect("a hex word")
}

fn f(s: &str) -> f32 {
    f32::from_bits(hex(s))
}

fn b(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn state(s: &EeGfxSceneFade, zoom: f32) -> String {
    format!(
        "{} {} {} {} {} {} {} {} {} {} {}",
        b(s.light_fade),
        b(s.light_fade_start),
        s.fade_down,
        b(s.fade_rate),
        b(s.fade_time),
        s.letterbox_direction,
        b(s.letterbox_start),
        b(s.letterbox_rate),
        b(s.letterbox_time),
        s.bars_cleared as i32,
        b(zoom)
    )
}

fn rect(tag: &str, l: f32, r: f32, t: f32, bo: f32) -> String {
    format!(" {tag} {} {} {} {}", b(l), b(r), b(t), b(bo))
}

/// The driver's terrain: tiles 64 x 48, a slope above a sea level.
struct Stub(f32);

impl GroundHeights for Stub {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool {
        x < 64 && y < 48
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        x * 0.25 - y * 0.125 + self.0
    }
}

#[test]
fn scene_replays_the_cpp() {
    let mut s = EeGfxSceneFade::default();
    let mut clocks = EeGfxSceneClocks::default();
    let mut zoom: f32 = 1.0;
    let mut first_step = true;
    let mut tables = EeGfxDaylightTables::default();
    let (mut steps, mut lights, mut objects, mut ticks, mut rings) = (0, 0, 0, 0, 0);
    let (mut clock, mut last) = (0u32, 0u32);
    for (n, line) in GOLDEN.lines().enumerate() {
        let w: Vec<&str> = line.split(' ').collect();
        match w[0] {
            "F" | "L" => {
                let (out, frames) = (w[1] == "1", w[2].parse::<u32>().unwrap());
                if w[0] == "F" {
                    start_fade(&mut s, out, frames);
                } else {
                    start_letterbox(&mut s, out, frames, zoom);
                }
                assert_eq!(line.split(" | ").nth(1).unwrap(), state(&s, zoom), "line {}", n + 1);
            }
            "S" => {
                let delta: u32 = w[1].parse().unwrap();
                let wind = f(w[2]);
                let (width, height) = (w[3].parse::<u32>().unwrap() as f32, w[4].parse::<u32>().unwrap() as f32);
                // the first frame starts the C++'s clock (its static's first value): no time elapsed
                let elapsed = if first_step { 0.0 } else { delta as f32 };
                first_step = false;
                step_clocks(&mut clocks, elapsed, wind);
                let r = step(&mut s, elapsed, zoom);
                let mut log = String::new();
                if r.clear_bars {
                    let l = letterbox(r.zoom, height);
                    log += &rect("C", 0.0, width, 0.0, l.top_bar_end);
                    log += &rect("C", 0.0, width, l.bottom_bar_start, height);
                }
                if r.set_view {
                    let l = letterbox(r.zoom, height);
                    log += &rect("V", l.left, l.right, l.top, l.bottom);
                    log += &rect("C", 0.0, width, 0.0, l.top_bar_end + 1.0);
                    log += &rect("C", 0.0, width, l.bottom_bar_start, height);
                    if r.show_backgrounds {
                        log += " show";
                    }
                    if r.hide_backgrounds {
                        log += " hide";
                    }
                }
                zoom = r.zoom;
                log += &format!(
                    " F{} K {} {} {}",
                    r.fade_changed as i32,
                    b(clocks.grid_phase),
                    b(clocks.water_phase),
                    b(clocks.cloud_offset)
                );
                let head = w[..5].join(" ");
                assert_eq!(line, format!("{head}{log} | {}", state(&s, zoom)), "line {}", n + 1);
                steps += 1;
            }
            "T" => {
                let v: Vec<f32> = w[1..].iter().map(|x| f(x)).collect();
                for k in 0..17 {
                    let e = &v[k * 6..k * 6 + 6];
                    tables.colors[k] = EeGfxColor { red: e[0], green: e[1], blue: e[2] };
                    tables.light[k] = e[3];
                    tables.ambient[k] = e[4];
                    tables.unit_ambient[k] = e[5];
                }
                let v = &v[102..];
                tables.ambient_color = EeGfxColor { red: v[0], green: v[1], blue: v[2] };
                let vec = |i: usize| GeVector { x: v[3 + i * 3], y: v[4 + i * 3], z: v[5 + i * 3] };
                tables.fill = [vec(0), vec(1), vec(2)];
                tables.sun = vec(3);
                tables.moon = vec(4);
            }
            "D" => {
                let d = daylight(&tables, f(w[1]));
                let got = [
                    d.direction.x,
                    d.direction.y,
                    d.direction.z,
                    d.light.red,
                    d.light.green,
                    d.light.blue,
                    d.light_intensity,
                    d.ambient_intensity,
                    d.unit_ambient_intensity,
                    d.prelit.red,
                    d.prelit.green,
                    d.prelit.blue,
                    d.dim_prelit.red,
                    d.dim_prelit.green,
                    d.dim_prelit.blue,
                    d.prelit.red,
                ]
                .map(b)
                .join(" ");
                assert_eq!(line.split(" | ").nth(1).unwrap(), got, "line {}", n + 1);
                lights += 1;
            }
            "O" | "P" => {
                let n_ = |i: usize| w[i].parse::<i32>().unwrap();
                let (id, size, x, y, start, sway) = (n_(1), n_(2), n_(3), n_(4), f(w[5]), w[6].parse::<u32>().unwrap());
                let (px, py) = object_place(id, size, x, y);
                // FillObject scales its stats' size by the jitter, the remembered copy the jitter by the scale (the same product)
                let scale = start * object_scale(id);
                let got = [px, py, scale, sway_fraction(sway, id)].map(b).join(" ");
                assert_eq!(line.split(" | ").nth(1).unwrap(), got, "line {}", n + 1);
                objects += 1;
            }
            "W" => {
                sway_tick(&mut clock, &mut last, w[1].parse().unwrap());
                assert_eq!(line.split(" | ").nth(1).unwrap(), format!("{clock} {last}"), "line {}", n + 1);
                ticks += 1;
            }
            "R" => {
                let (x, y, radius, sea, mode) = (f(w[1]), f(w[2]), f(w[3]), f(w[4]), w[5]);
                let mut got = String::new();
                if mode == "2" {
                    let (points, width) = ground_circle(&mut Stub(sea), x, y, radius);
                    let mut hash: u64 = 0xcbf29ce484222325;
                    for p in points {
                        for word in [p.x.to_bits(), p.y.to_bits(), p.z.to_bits(), width.to_bits()] {
                            for byte in word.to_le_bytes() {
                                hash ^= byte as u64;
                                hash = hash.wrapping_mul(0x100000001b3);
                            }
                        }
                    }
                    // the line's width is the grid clock, 0.25 in the driver
                    got = format!(" {} {} {hash:016x}", points.len(), b(0.25));
                }
                assert_eq!(line.split(" |").nth(1).unwrap(), got, "line {}", n + 1);
                rings += 1;
            }
            other => panic!("line {}: {other}", n + 1),
        }
    }
    assert_eq!((steps, lights, objects, ticks, rings), (714, 360, 800, 200, 300));
}
