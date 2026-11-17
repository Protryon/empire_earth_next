//! The render fill (`EERenderObject.cpp`'s `FillRenderLists`, `IsUnitDrawn`, `FillUnit`, `FillObject`, the
//! doppelgangers, the ghost records, `GetUnitOpacity`) against the unmodified C++ (`golden_fill.txt`, the object plan's
//! Lane R gate RFIL). A scratch driver (`build/gxR/drv/make_rfil.py`; docs/progress/obj-lane-r.md) took those functions
//! verbatim out of `EERenderObject.cpp` of 4aeedb1a1 and ran them over a stand-in world and a stub GE: the world's
//! tables first (`MO` models, `OB` graphic objects, `K` unit classes, `Q` upgrades, `PL` players), then per frame its
//! remembered units (`D`), the current player's selection (`SEL`), the frame's switches (`E`), the camera (`CAM`), every
//! unit (`W`; a complex unit's physical model after `C`) and the visible tiles (`T`); the fill's calls (`F`) and its
//! output: every render unit (`U`, with the unit whose `AddUnit` placed it), object (`S`), texture map (`M0`..`M5`),
//! selected list (`SU`, `SO`), the picked and labelled units, the rectangle's ids and the sway clock (`X`), ghost record
//! (`G`) and ground circle (`L`). Seed 1, the first 40 frames; the driver printed the same bytes twice, and 3,000 frames
//! of seed 2 twice.
//!
//! Until the fill moves, this test replays what of it is Rust already: every live map object's place, size jitter,
//! sway and health (`capture.rs`), and every live unit's class fields, owner and fog (`walk.rs`'s owned food). The
//! move adds the whole fill over the world's views, rebuilt from the same lines.

use std::collections::BTreeMap;

use crate::gfx::capture::{object_place, object_scale, sway_fraction};
use crate::gfx::walk::owned_food;

const GOLDEN: &str = include_str!("golden_fill.txt");

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).expect("a hex word"))
}

fn num(s: &str, prefix: &str) -> i64 {
    s.strip_prefix(prefix).unwrap_or(s).parse().expect("a number")
}

#[derive(Default, Clone, Copy)]
struct Class {
    category: i64,
    tile: i64,
    family: i64,
    kind: i64,
}

#[derive(Default, Clone, Copy)]
struct Upgrade {
    scale: f32,
    max_hit_points: i64,
}

#[derive(Default, Clone)]
struct Unit {
    class: usize,
    stats: usize,
    player: i64,
    tile: (i64, i64),
    food: bool,
    hit_points: i64,
    complex_scale: Option<f32>,
}

#[derive(Default)]
struct Frame {
    free: bool,
    units: BTreeMap<i64, Unit>,
    seen: BTreeMap<(i64, i64), bool>,
    sway: u32,
    render_units: Vec<Vec<String>>,
    objects: Vec<Vec<String>>,
}

struct World {
    model_frames: Vec<u64>,
    object_model0: Vec<usize>,
    classes: Vec<Class>,
    upgrades: Vec<Upgrade>,
    frames: Vec<Frame>,
}

fn parse() -> World {
    let mut w =
        World { model_frames: vec![], object_model0: vec![], classes: vec![], upgrades: vec![], frames: vec![] };
    let mut cur = Frame::default();
    let mut in_frame = false;
    for line in GOLDEN.lines() {
        let t: Vec<&str> = line.split_whitespace().collect();
        let Some(&head) = t.first() else { continue };
        if head.starts_with("MO") {
            w.model_frames.push(t[1].parse().unwrap());
        } else if head.starts_with("OB") {
            w.object_model0.push(num(t[1], "m") as usize);
        } else if head.starts_with('K') {
            w.classes.push(Class {
                category: t[1].parse().unwrap(),
                tile: t[2].parse().unwrap(),
                family: t[3].parse().unwrap(),
                kind: t[4].parse().unwrap(),
            });
        } else if head.starts_with('Q') {
            w.upgrades.push(Upgrade { scale: hex(t[1]), max_hit_points: t[3].parse().unwrap() });
        } else if head == "E" {
            if in_frame {
                w.frames.push(std::mem::take(&mut cur));
            }
            in_frame = true;
            cur.free = t[1] == "free1";
        } else if head.starts_with('W') {
            let (x, y) = t[4][1..].split_once(',').unwrap();
            let (kind, amount) = t[5][1..].split_once(',').unwrap();
            let unit = Unit {
                class: num(t[1], "c") as usize,
                stats: num(t[2], "s") as usize,
                player: num(t[3], "p"),
                tile: (x.parse().unwrap(), y.parse().unwrap()),
                food: kind == "2" && amount.parse::<i64>().unwrap() > 0,
                hit_points: num(t[6], "hp"),
                complex_scale: (t.len() > 12 && t[12] == "C").then(|| hex(t[29])),
            };
            cur.units.insert(num(head, "W"), unit);
        } else if let Some(tile) = head.strip_prefix('T') {
            let (x, y) = tile.split_once(',').unwrap();
            cur.seen.insert((x.parse().unwrap(), y.parse().unwrap()), t[2] == "s1");
        } else if head == "X" {
            let at = t.iter().position(|s| *s == "sway").unwrap();
            cur.sway = u32::from_str_radix(t[at + 1], 16).unwrap();
        } else if head.starts_with('U') && head[1..].chars().all(|c| c.is_ascii_digit()) {
            cur.render_units.push(t.iter().map(|s| s.to_string()).collect());
        } else if head.starts_with('S') && head[1..].chars().all(|c| c.is_ascii_digit()) {
            cur.objects.push(t.iter().map(|s| s.to_string()).collect());
        }
    }
    if in_frame {
        w.frames.push(cur);
    }
    w
}

// a render unit's tokens after its 22 floats and seven vectors (the bars, the turrets', orbitals' and wheels' angles and models): the name, the three pointers, ten floats, thirteen
// integers and the flags
fn unit_tail(t: &[String]) -> &[String] {
    let mut i = 2 + 16 + 6;
    for _ in 0..7 {
        let count: usize = t[i].trim_start_matches('[').trim_end_matches(']').parse().unwrap();
        i += 1 + count;
    }
    &t[i..]
}

#[test]
fn the_golden_has_the_driver_s_frames() {
    let w = parse();
    assert_eq!(w.frames.len(), 40);
    assert_eq!(w.model_frames.len(), 32);
    assert_eq!(w.object_model0.len(), 10);
    assert_eq!(w.classes.len(), 24);
    assert_eq!(w.upgrades.len(), 96);
    let units: usize = w.frames.iter().map(|f| f.render_units.len()).sum();
    let objects: usize = w.frames.iter().map(|f| f.objects.len()).sum();
    assert!(units > 200 && objects > 50, "{units} units, {objects} objects");
}

#[test]
fn live_map_objects_are_placed_sized_and_swayed_as_the_cpp() {
    let w = parse();
    let mut checked = 0;
    for f in &w.frames {
        for t in &f.objects {
            let source = num(&t[1], "u");
            let fogged = t[t.len() - 1] == "1";
            if source < 0 || fogged {
                continue; // a skipped frame's entry or a remembered object (its tile, not its unit's)
            }
            let u = &f.units[&source];
            let class = w.classes[u.class];
            let stats = w.upgrades[u.stats];
            let (x, y) = object_place(source as i32, class.tile as i32, u.tile.0 as i32, u.tile.1 as i32);
            assert_eq!((hex(&t[2]).to_bits(), hex(&t[3]).to_bits()), (x.to_bits(), y.to_bits()), "{t:?}");
            let mut scale = stats.scale;
            scale *= object_scale(source as i32);
            assert_eq!(hex(&t[6]).to_bits(), scale.to_bits(), "{t:?}");
            let health = u.hit_points as f32 / stats.max_hit_points as f32;
            assert_eq!(hex(&t[10]).to_bits(), health.to_bits(), "{t:?}");
            let object = num(&t[5], "o") as usize;
            let frames = w.model_frames[w.object_model0[object]];
            let (current, fraction) = if frames > 1 { (1, sway_fraction(f.sway, source as i32)) } else { (0, 0.0) };
            assert_eq!(hex(&t[12]).to_bits(), fraction.to_bits(), "{t:?}");
            assert_eq!((t[13].as_str(), t[14].parse::<u64>().unwrap(), t[15].as_str()), ("0", current, "0"), "{t:?}");
            checked += 1;
        }
    }
    assert!(checked > 40, "{checked}");
}

#[test]
fn live_units_carry_their_class_owner_and_fog_as_the_cpp() {
    let w = parse();
    let mut checked = 0;
    for f in &w.frames {
        for t in &f.render_units {
            let source = num(&t[1], "u");
            let tail = unit_tail(t);
            let ints: Vec<i64> = tail[14..27].iter().map(|s| s.parse().unwrap()).collect();
            if source < 0 || ints[10] != source {
                continue; // a skipped frame's entry or a remembered unit (FillDoppelgangerUnit keeps the slot's id)
            }
            let u = &f.units[&source];
            let class = w.classes[u.class];
            assert_eq!(
                (ints[0], ints[1], ints[2], ints[6], ints[11]),
                (class.category, class.kind, class.family, class.tile, u.player)
            );
            let fogged = !f.free
                && owned_food(u.food, u.complex_scale.is_some(), class.category as i32, u.player as i32)
                && !f.seen[&u.tile];
            assert_eq!(&tail[27][1..2] == "1", fogged, "{t:?}");
            let scale = u.complex_scale.unwrap_or(w.upgrades[u.stats].scale);
            assert_eq!(hex(&tail[4]).to_bits(), scale.to_bits(), "{t:?}");
            if u.complex_scale.is_none() {
                // a placed object: no animation, frames, command or fraction
                assert_eq!((ints[3], ints[5], ints[7], ints[8], hex(&tail[11])), (0, 0, 0, 0, 0.0), "{t:?}");
            }
            checked += 1;
        }
    }
    assert!(checked > 100, "{checked}");
}
