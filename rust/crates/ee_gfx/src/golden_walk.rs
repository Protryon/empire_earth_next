//! `walk.rs` against the unmodified C++ (`golden_walk.txt`). A scratch driver (`build/gx17/walk`; PORT_LEARNINGS
//! "R4.4, eleventh round") took `EERenderObject::IsUnitDrawn` whole, `FillUnit`'s owned-food test and waypoint tint
//! and `FillRenderLists`' music intensity verbatim out of `EERenderObject.cpp` of the commit before this work, over a
//! stub world: 24 random unit classes (categories among the citizens and the gate, trees, flares, path points, sizes
//! 1 to 4), 48 units reused with random owners, resources, stats scales (0 among them), ids, tiles, complex parts,
//! commands (the dead action), foundations and path markers, random seen tiles, free camera, orthographic view,
//! camera yaw and pool fills (full among them), a terrain `z = x / 4 - y / 8`, the physical models' view tests
//! answering from the same LCG and printing what they test (the model, frame, view, yaw, place and scale). From an
//! LCG seeded 1: 2,000 units (the drawn set emptied about every 50th), 600 waypoint carriers (negative and large ids)
//! and 800 music cases. This test draws the same values and replays them through `walk.rs`; the driver rebuilt from
//! the new unit over the forwarders printed the same bytes (and 20,000 units on seed 2).
// the driver's arithmetic as written
#![allow(clippy::manual_is_multiple_of, clippy::manual_clamp, clippy::field_reassign_with_default)]

use std::collections::BTreeSet;

use crate::walk::{music_intensity, owned_food, unit_drawn, waypoint_tint, DrawnHost, EeGfxUnitFacts};

const GOLDEN: &str = include_str!("golden_walk.txt");

struct Lcg(u64);

impl Lcg {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn unit(&mut self) -> f32 {
        (self.next() & 0xffffff) as f32 / 16777216.0
    }
}

fn b(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).expect("a hex word"))
}

struct Host<'a> {
    r: &'a mut Lcg,
    drawn: &'a mut BTreeSet<usize>,
    unit: usize,
    model: usize,
    orthographic: bool,
    yaw: f32,
    scale: f32,
    text: String,
}

impl DrawnHost for Host<'_> {
    fn first_drawn(&mut self) -> bool {
        self.drawn.insert(self.unit)
    }
    fn complex_in_view(&mut self) -> bool {
        let r = self.r.next() % 4 != 0;
        self.text.push_str(&format!(" C{}", r as i32));
        r
    }
    fn model_in_view(&mut self, x: f32, y: f32) -> bool {
        let r = self.r.next() % 4 != 0;
        let z = x * 0.25 - y * 0.125;
        self.text.push_str(&format!(
            " M {} 0 {} {} {} {} {} {} 00000000 00000000 00000000 3f800000 00000000 00000000 {}",
            self.model,
            self.orthographic as i32,
            b(self.yaw),
            b(x),
            b(y),
            b(z),
            b(self.scale),
            r as i32
        ));
        r
    }
}

#[derive(Clone, Copy, Default)]
struct Class {
    category: i32,
    tree: bool,
    flare: bool,
    kind: i32,
    size: i32,
}

#[test]
fn the_world_walk_matches_the_cpp() {
    let mut r = Lcg(1);
    let mut lines = GOLDEN.lines();
    const CATEGORIES: [i32; 5] = [1, 6, 0x16, 0x19, 3];
    const KINDS: [i32; 4] = [0xdf, 5, 0x104, 77];
    const SCALES: [f32; 4] = [1.0, 0.0, 1.5, 0.75];
    let mut classes = [Class::default(); 24];
    for c in classes.iter_mut() {
        c.category = CATEGORIES[(r.next() % 5) as usize];
        c.tree = r.next() % 4 == 0;
        c.flare = r.next() % 5 == 0;
        c.kind = KINDS[(r.next() % 4) as usize];
        c.size = (r.next() % 4 + 1) as i32;
    }
    let mut drawn = BTreeSet::new();
    for case in 0..2000 {
        if r.next() % 50 == 0 {
            drawn.clear();
        }
        let unit = (r.next() % 48) as usize;
        let food = r.next() % 3 == 0;
        let amount = (r.next() % 3) as i32 - 1;
        let class = classes[(r.next() % 24) as usize];
        let owner = (r.next() % 3) as i32;
        let scale = SCALES[(r.next() % 4) as usize];
        let id = (r.next() % 2000) as i32 - 100;
        let tile_x = (r.next() % 300) as i32;
        let tile_y = (r.next() % 300) as i32;
        let path_marker = r.next() % 6 == 0;
        let model = (r.next() % 4) as usize;
        let has_complex = r.next() % 2 == 0;
        let command = if r.next() % 3 == 0 { None } else { Some(r.next() % 2) };
        let foundation = r.next() % 5 == 0;
        let seen = r.next() % 3 != 0;
        let free_camera = r.next() % 6 == 0;
        let orthographic = r.next() % 2 != 0;
        let yaw = r.unit() * 6.0;
        let unit_count = if r.next() % 8 == 0 { 0x200 } else { r.next() % 0x200 };
        let object_count = if r.next() % 8 == 0 { 0x600 } else { r.next() % 0x600 };
        let mut f = EeGfxUnitFacts {
            food: food && amount > 0,
            has_complex,
            category: class.category,
            owner_id: owner,
            seen,
            free_camera,
            scale,
            is_tree: class.tree,
            unit_type: class.kind,
            unit_count,
            object_count,
            tile_size: class.size,
            mine: owner == 1,
            marker_shown: false,
            path_marker,
            id,
            tile_x,
            tile_y,
            ..Default::default()
        };
        if has_complex {
            f.dead = command == Some(0);
            f.complex_gate = class.category == 0x19;
            f.complex_path_point = class.kind == 0xdf;
            f.complex_flare = class.flare;
            f.foundation = foundation;
        }
        let mut h = Host {
            r: &mut r,
            drawn: &mut drawn,
            unit,
            model,
            orthographic,
            yaw,
            scale,
            text: format!("U {unit} {case} |"),
        };
        let result = unit_drawn(&f, &mut h);
        let mut text = h.text;
        text.push_str(&format!(
            " = {} F {}",
            result as i32,
            owned_food(f.food, has_complex, class.category, owner) as i32
        ));
        assert_eq!(text, lines.next().expect("a golden line"), "unit case {case}");
    }
    for line in lines {
        let (head, tail) = line.split_once(" | ").expect("a separator");
        let words: Vec<&str> = head.split(' ').collect();
        let out: Vec<&str> = tail.split(' ').collect();
        match words[0] {
            "T" => {
                let id: i32 = words[1].parse().expect("an id");
                let c = waypoint_tint(id);
                assert_eq!((c.red, c.green, c.blue), (hex(out[0]), hex(out[1]), hex(out[2])), "{line}");
            }
            "S" => {
                let current: u32 = words[1].parse().expect("an intensity");
                let set = music_intensity(current, hex(words[2]), hex(words[3]));
                assert_eq!(set, out[0].parse::<u32>().expect("an intensity"), "{line}");
            }
            _ => panic!("{line}"),
        }
    }
}
