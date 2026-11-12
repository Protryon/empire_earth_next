//! The unit picker against the unmodified C++ (`golden_picker.txt`). A scratch driver (`build/gx9_golden`,
//! `driver_picker.cpp`; PORT_LEARNINGS "R4.4, third round") #included `EEUnitPicker.cpp` of the commit before this
//! work over the real GE math and `GEViewport`, with zeroed fake units, players and classes (a class's and a group's
//! vtable slots log: slot 21 `IsLandBuilding`, 6 `GetNumUnits`), fake models whose vertex arrays log each point read
//! (slot 5 `GetPoint`), a scripted `GEPhysicalModel::Intersects` and `GERasterizer::GetPixelSize` that log their
//! entry, LOD, scale and line, and an `EEWorld::GetUnit` that logs each id. From an LCG seeded 1 it drew 24 viewports
//! with 40 `FindUnit`, 20 `GetUnitPickPriority`, 20 `GetIsEntryInView` and 10 `AddUnit` cases each, and printed per
//! case an FNV-1a 64 of its log. This test draws the same stream, runs each case through `pick.rs` and the C++ glue
//! rewritten here, and compares the digests.

use std::collections::BTreeMap;

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeRectangle, GeTransformation, GeVector};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::pick::*;

const GOLDEN: &str = include_str!("golden_picker.txt");

#[derive(Clone, Copy, Default)]
struct Class {
    land: bool,
    family: u32,
    wall: bool,
    category: u32,
    unit_type: u32,
    projectile: bool,
}

#[derive(Clone, Copy, Default)]
struct Unit {
    id: u32,
    complex: bool,
    previous: Option<usize>,
    class: usize,
    hit_points: i32,
    player: usize,
    resource_type: u32,
    resource_amount: i32,
    tile_x: i32,
}

#[derive(Clone, Copy, Default)]
struct Player {
    enemies: [u32; 4],
    group_units: u32,
}

#[derive(Clone, Copy, Default)]
struct Entry {
    id: u32,
    location: GeTransformation,
    scale: f32,
    frame: u32,
    model: usize,
}

#[derive(Clone, Copy, Default)]
struct Answer {
    hit1: bool,
    hit2: bool,
    d1: f32,
    d2: f32,
    pixels: f32,
}

struct Material {
    start: Vec<u32>,
    length: Vec<u32>,
    vertex: u32,
}

struct Model {
    frames: u32,
    lods: u32,
    parent: bool,
    transforms: Vec<GeTransformation>,
    center: Ge3DPoint,
    radii: Vec<f32>,
    vertices: Vec<(i32, Vec<Ge3DPoint>)>,
    polygons: Vec<Vec<u32>>,
    materials: Vec<Material>,
    subs: Vec<usize>,
}

const NEAR: [f32; 4] = [1.0, 2.0, 2.0, 5.0];
const PIXELS: [f32; 4] = [0.35, 0.3499, 0.0, 1.0];
const WIDTHS: [f32; 4] = [640.0, 800.0, 1024.0, 1920.0];

struct Case {
    g: Rng,
    log: Log,
    vp: GeViewport,
    classes: Vec<Class>,
    units: Vec<Unit>,
    by_id: BTreeMap<u32, usize>,
    complex: Vec<usize>,
    players: [Player; 4],
    /// gWorld.mPlayers[0] (the current player is mPlayers[1], player 1)
    gaia: usize,
    entries: [Entry; 32],
    answers: [Answer; 32],
    models: Vec<Model>,
    arrays: i32,
    free_camera: bool,
    window_width: u32,
}

impl Case {
    fn transform(&mut self) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        let y = self.g.f(-3.2, 3.2);
        let p = self.g.f(-1.5, 1.5);
        let r = if self.g.b(20) { self.g.f(-0.4, 0.4) } else { 0.0 };
        t.set_orientation(RotationOrder::Ypr, y, p, r);
        if self.g.b(10) {
            let q = if self.g.b(50) { 1.5707964 } else { -1.5707964 };
            t.set_orientation(RotationOrder::Ypr, 0.0, q, 0.0);
        }
        let x = self.g.f(-4.0, 68.0);
        let yy = self.g.f(-4.0, 68.0);
        let z = self.g.f(-2.0, 30.0);
        t.m[0][3] = x;
        t.m[1][3] = yy;
        t.m[2][3] = z;
        t
    }

    fn viewport(&mut self) {
        let w = self.g.pick(&WIDTHS);
        let h = self.g.pick(&[480.0, 600.0, 768.0, 1080.0]);
        let top = if self.g.b(30) { self.g.f(0.0, 60.0) } else { 0.0 };
        self.vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top });
        let fov = self.g.f(0.3, 1.2);
        self.vp.set_field_of_view(fov);
        let front = self.g.f(0.1, 3.0);
        self.vp.set_front_clipping_distance(front);
        let rear = self.g.f(30.0, 90.0);
        self.vp.set_rear_clipping_distance(rear);
        let ow = self.g.f(10.0, 120.0);
        let oh = self.g.f(10.0, 90.0);
        self.vp.set_orthographic_rectangle(&GeRectangle { bottom: oh, left: -ow, right: ow, top: -oh });
        let camera = self.transform();
        self.vp.set_camera(&camera);
        self.log.t("vp");
        self.log.i(w as i64);
        self.log.i(h as i64);
        self.log.h(top);
    }

    fn make_world(&mut self) {
        let cats = [6, 0x16, 3, 9, 1, 0, 12];
        for _ in 0..8 {
            let land = self.g.b(30);
            let _pickable = self.g.b(80);
            let family = if self.g.b(15) { 0x2e } else { self.g.r() % 60 };
            let wall = self.g.b(15);
            let category = cats[(self.g.r() % 7) as usize];
            let unit_type = if self.g.b(10) {
                0xdf
            } else if self.g.b(10) {
                0x2ee
            } else {
                self.g.r() % 800
            };
            let projectile = self.g.b(20);
            self.classes.push(Class { land, family, wall, category, unit_type, projectile });
        }
    }

    fn make_unit(&mut self, id: u32) -> usize {
        let complex = self.g.b(60);
        let mut previous = None;
        if complex && !self.complex.is_empty() && self.g.b(20) {
            previous = Some(self.complex[(self.g.r() % self.complex.len() as u32) as usize]);
        }
        let class = (self.g.r() % 8) as usize;
        let hit_points = if self.g.b(15) { 0 } else { (self.g.r() % 60) as i32 - 5 };
        let player = (self.g.r() % 4) as usize;
        let resource_type = self.g.r() % 9;
        let resource_amount = (self.g.r() % 12) as i32 - 2;
        let tile_x = if self.g.b(5) { -10000 } else { (self.g.r() % 100) as i32 };
        let index = self.units.len();
        self.units.push(Unit {
            id,
            complex,
            previous,
            class,
            hit_points,
            player,
            resource_type,
            resource_amount,
            tile_x,
        });
        if complex {
            self.complex.push(index);
        }
        self.by_id.insert(id, index);
        index
    }

    fn make_model(&mut self, parent: Option<usize>, depth: u32) -> usize {
        let frames = match parent {
            Some(p) => self.models[p].frames,
            None => 1 + self.g.r() % 2,
        };
        let lods = match parent {
            Some(p) => self.models[p].lods,
            None => 1 + self.g.r() % 4,
        };
        let mut transforms = vec![];
        for _ in 0..frames {
            transforms.push(if self.g.b(50) { self.transform() } else { GeTransformation::IDENTITY });
        }
        let cx = self.g.f(-1.0, 1.0);
        let cy = self.g.f(-1.0, 1.0);
        let cz = self.g.f(0.0, 2.0);
        let radii = (0..frames).map(|_| self.g.f(0.1, 4.0)).collect();
        let mut vertices = vec![];
        for _ in 0..frames {
            let id = self.arrays;
            self.arrays += 1;
            let n = 3 + self.g.r() % 10;
            let mut points = vec![];
            for _ in 0..n {
                let x = self.g.f(-3.0, 3.0);
                let y = self.g.f(-3.0, 3.0);
                let z = self.g.f(-1.0, 3.0);
                points.push(Ge3DPoint::new(x, y, z));
            }
            vertices.push((id, points));
        }
        let mut polygons = vec![];
        for _ in 0..lods {
            self.arrays += 1;
            let n = 1 + self.g.r() % 6;
            polygons.push((0..3 * n).map(|_| self.g.r() % 16).collect());
        }
        let count = self.g.r() % 3;
        let mut materials = vec![];
        for _ in 0..count {
            let mut start = vec![];
            let mut length = vec![];
            for _ in 0..lods {
                start.push(self.g.r() % 3);
                length.push(self.g.r() % 4);
            }
            let vertex = self.g.r() % 4;
            materials.push(Material { start, length, vertex });
        }
        let index = self.models.len();
        self.models.push(Model {
            frames,
            lods,
            parent: parent.is_some(),
            transforms,
            center: Ge3DPoint::new(cx, cy, cz),
            radii,
            vertices,
            polygons,
            materials,
            subs: vec![],
        });
        let subs = if depth < 1 && self.g.b(30) { 1 + self.g.r() % 2 } else { 0 };
        for _ in 0..subs {
            let s = self.make_model(Some(index), depth + 1);
            self.models[index].subs.push(s);
        }
        index
    }

    // ---- the C++ glue
    fn get_unit(&mut self, id: u32) -> Option<usize> {
        self.log.t("unit");
        self.log.i(id as i64);
        self.by_id.get(&id).copied()
    }

    fn priority_of(&mut self, u: usize, prefer: bool) -> i32 {
        let unit = self.units[u];
        let class = self.classes[unit.class];
        let facts = EeGfxPickUnit {
            complex: unit.complex,
            gaia: unit.player == self.gaia,
            wall: class.wall,
            hit_points: unit.hit_points,
            family: class.family,
            category: class.category,
            resource_type: unit.resource_type,
            resource_amount: unit.resource_amount,
        };
        let log = &mut self.log;
        let players = &self.players;
        priority(&facts, prefer, |q| match q {
            EeGfxPickQuestion::LandBuilding => {
                log.t(&format!("C{}.21", unit.class));
                class.land
            }
            // the current player (1) IsAlly
            EeGfxPickQuestion::Ally => unit.player == 1 || players[1].enemies[unit.player] == 0,
        })
    }

    fn intersects(&mut self, i: usize, line: &Ge3DLine, lod: u32) -> Option<f32> {
        let e = self.entries[i];
        self.log.t("hit");
        self.log.i(i as i64);
        self.log.i(lod as i64);
        self.log.i(0);
        self.log.h(e.scale);
        self.log.i(e.frame as i64);
        self.log.h(e.location.m[0][3]);
        self.log.h(line.start.x);
        self.log.h(line.start.y);
        self.log.h(line.start.z);
        self.log.h(line.direction.x);
        self.log.h(line.direction.y);
        self.log.h(line.direction.z);
        let a = self.answers[i];
        if lod == 1 {
            a.hit1.then_some(a.d1)
        } else {
            a.hit2.then_some(a.d2)
        }
    }

    fn find_unit(&mut self, count: usize, ortho: bool, hidden: bool, point: Ge2DPoint, prefer: bool) -> Option<u32> {
        let t = tolerances(&self.vp, self.free_camera, self.window_width);
        if count == 0 {
            return None;
        }
        let ray = screen_ray(&self.vp, ortho, point);
        let line = Ge3DLine {
            start: ray.front,
            direction: GeVector {
                x: ray.rear.x - ray.front.x,
                y: ray.rear.y - ray.front.y,
                z: ray.rear.z - ray.front.z,
            },
        };
        let mut hits: Vec<EeGfxPickHit> = vec![];
        let mut distance = 0.0f32;
        for i in (0..count).rev() {
            let Some(u) = self.get_unit(self.entries[i].id) else { continue };
            let saved = self.entries[i].scale;
            let kind = self.priority_of(u, false);
            self.entries[i].scale = pick_scale(kind, saved);
            if self.units[u].tile_x != -10000 {
                if let Some(d) = self.intersects(i, &line, 1) {
                    distance = d;
                    let priority = self.priority_of(u, prefer) as u32;
                    hits.push(EeGfxPickHit { distance, priority, kind, index: i as u32, exact: false, large: false });
                }
            }
            self.entries[i].scale = saved;
        }
        let _ = distance;
        let mut saved = 1.0;
        for h in (0..hits.len()).rev() {
            let index = hits[h].index as usize;
            if hits[h].kind == 2 {
                saved = self.entries[index].scale;
                self.entries[index].scale = TREE_PICK_SCALE * saved;
            }
            if let Some(d) = self.intersects(index, &line, 2) {
                hits[h].distance = d;
                hits[h].exact = true;
            } else {
                self.log.t("pixels");
                self.log.i(index as i64);
                self.log.i(ortho as i64);
                self.log.h(self.entries[index].scale);
                let pixels = self.answers[index].pixels;
                hits[h].large = is_large(pixels, t.pixel, hits[h].kind);
            }
            if hits[h].kind == 2 {
                self.entries[index].scale = saved;
            }
        }
        let (first, second) = rank(&hits, t.tolerance, hidden);
        let group = self.players[1].group_units;
        let log = &mut self.log;
        let best = choose(&hits, first, second, || {
            log.t("G6");
            group != 0
        })?;
        let u = self.get_unit(self.entries[hits[best].index as usize].id).unwrap();
        let mut result = u;
        if self.units[u].complex {
            let mut carrier = self.units[u].previous;
            while let Some(c) = carrier {
                result = c;
                carrier = self.units[c].previous;
            }
        }
        Some(self.units[result].id)
    }

    fn model_in_view(&mut self, m: usize, t: &GeTransformation, frame: u32, lod: u32, planes: &[Ge3DPlane; 4]) -> bool {
        let t = if !self.models[m].parent { *t } else { t.compose(&self.models[m].transforms[frame as usize]) };
        let mut inside = false;
        let count = self.models[m].materials.len();
        let mut i = 0;
        while i < count && !inside {
            let (start, length, vertex) = {
                let mat = &self.models[m].materials[i];
                (mat.start[lod as usize], mat.length[lod as usize], mat.vertex)
            };
            let end = length + start;
            let mut j = start;
            while j < end && !inside {
                for k in 0..3 {
                    let polys = &self.models[m].polygons[lod as usize];
                    let index = polys[((3 * j + k) as usize) % polys.len()] + vertex;
                    let (id, points) = &self.models[m].vertices[frame as usize];
                    self.log.t(&format!("P{id}.{index}"));
                    let p = points[index as usize % points.len()];
                    inside = point_in_view(planes, t.apply_point(p));
                    if inside {
                        break;
                    }
                }
                j += 1;
            }
            i += 1;
        }
        if !inside {
            let subs = self.models[m].subs.clone();
            for s in subs {
                if inside {
                    break;
                }
                inside = self.model_in_view(s, &t, frame, lod, planes);
            }
        }
        inside
    }

    fn entry_in_view(&mut self, e: Entry, planes: &[Ge3DPlane; 4]) -> bool {
        let m = e.model;
        let mut t = e.location.compose(&self.models[m].transforms[e.frame as usize]);
        if e.scale != 1.0 {
            t.scale(e.scale, e.scale, e.scale);
            t.m[0][3] = e.location.m[0][3];
            t.m[1][3] = e.location.m[1][3];
            t.m[2][3] = e.location.m[2][3];
        }
        let center = t.apply_point(self.models[m].center);
        let below = sphere_below(center, planes);
        if !below.iter().any(|b| *b) {
            return true;
        }
        let radius = self.models[m].radii[e.frame as usize] * e.scale;
        if sphere_outside(center, planes, &below, radius) == 0 {
            let lod = view_lod(self.models[m].lods);
            self.model_in_view(m, &t, e.frame, lod, planes)
        } else {
            false
        }
    }
}

#[test]
fn picker_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut c = Case {
        g: Rng(1),
        log: Log::default(),
        vp: GeViewport::default(),
        classes: vec![],
        units: vec![],
        by_id: BTreeMap::new(),
        complex: vec![],
        players: [Player::default(); 4],
        gaia: 0,
        entries: [Entry::default(); 32],
        answers: [Answer::default(); 32],
        models: vec![],
        arrays: 0,
        free_camera: false,
        window_width: 0,
    };
    c.make_world();
    let mut n = 0i64;
    let mut next_id = 100u32;
    let mut count = 0u32;
    for _ in 0..24 {
        c.viewport();
        check(&mut golden, "viewport", n, &mut c.log);
        n += 1;
        for _ in 0..40 {
            let k = c.g.r() % 13;
            let ortho = c.g.b(25);
            let hidden = c.g.b(15);
            c.free_camera = c.g.b(15);
            c.window_width = c.g.pick(&WIDTHS) as u32;
            for p in 0..4 {
                c.players[p].group_units = if c.g.b(50) { c.g.r() % 4 } else { 0 };
            }
            for p in 0..4 {
                for q in 0..4 {
                    c.players[p].enemies[q] = c.g.b(40) as u32;
                }
            }
            c.gaia = if c.g.b(80) { 0 } else { 3 };
            c.log.t("in");
            c.log.i(k as i64);
            c.log.i(ortho as i64);
            c.log.i(hidden as i64);
            c.log.i(c.free_camera as i64);
            c.log.i(c.window_width as i64);
            for i in 0..k as usize {
                let id = next_id;
                next_id += 1;
                if !c.g.b(8) {
                    c.make_unit(id);
                }
                c.entries[i].id = id;
                c.entries[i].location = c.transform();
                c.entries[i].scale = if c.g.b(30) { 1.0 } else { c.g.f(0.3, 2.5) };
                c.entries[i].frame = c.g.r() % 3;
                let hit1 = c.g.b(60);
                let d1 = if c.g.b(40) { c.g.pick(&NEAR) } else { c.g.f(0.0, 1.0) };
                let hit2 = c.g.b(45);
                let d2 = if c.g.b(40) { c.g.pick(&NEAR) } else { c.g.f(0.0, 1.0) };
                let pixels = if c.g.b(30) { c.g.f(0.0, 50.0) } else { c.g.f(0.0, 600.0) };
                c.answers[i] = Answer { hit1, hit2, d1, d2, pixels };
            }
            let px = c.g.f(-10.0, 1930.0);
            let py = c.g.f(-10.0, 1090.0);
            let prefer = c.g.b(30);
            c.log.h(px);
            c.log.h(py);
            c.log.i(prefer as i64);
            let result = c.find_unit(k as usize, ortho, hidden, Ge2DPoint { x: px, y: py }, prefer);
            c.log.t("out");
            c.log.i(result.map_or(-1, |id| id as i64));
            for i in 0..k as usize {
                c.log.h(c.entries[i].scale);
            }
            check(&mut golden, "find", n, &mut c.log);
            n += 1;
        }
        for _ in 0..20 {
            for p in 0..4 {
                let v = c.g.b(50) as u32;
                let q = (c.g.r() % 4) as usize;
                c.players[p].enemies[q] = v;
            }
            let u = c.make_unit(next_id);
            next_id += 1;
            let prefer = c.g.b(40);
            c.log.t("in");
            c.log.i(prefer as i64);
            let p = c.priority_of(u, prefer);
            c.log.i(p as i64);
            check(&mut golden, "priority", n, &mut c.log);
            n += 1;
        }
        for _ in 0..20 {
            let ortho = c.g.b(25);
            let sx = c.g.f(0.0, 800.0);
            let sy = c.g.f(0.0, 600.0);
            let w = if c.g.b(10) { c.g.f(0.0, 3.0) } else { c.g.f(0.0, 300.0) };
            let h = if c.g.b(10) { c.g.f(0.0, 3.0) } else { c.g.f(0.0, 300.0) };
            let l = sx - w * 0.5;
            let t = sy - h * 0.5;
            let planes6 =
                c.vp.frustum_world_planes(&GeRectangle { bottom: t + h, left: l, right: l + w, top: t }, ortho);
            let planes = [planes6[2], planes6[3], planes6[4], planes6[5]];
            let model = c.make_model(None, 0);
            let mut tr = c.transform();
            let cx = sx + c.g.f(-w - 20.0, w + 20.0);
            let cy = sy + c.g.f(-h - 20.0, h + 20.0);
            let depth = c.g.f(0.0, 0.4);
            let ray = screen_ray(&c.vp, ortho, Ge2DPoint { x: cx, y: cy });
            let (a, b) = (ray.front, ray.rear);
            tr.m[0][3] = (b.x - a.x) * depth + a.x;
            tr.m[1][3] = (b.y - a.y) * depth + a.y;
            tr.m[2][3] = (b.z - a.z) * depth + a.z;
            let s = if c.g.b(40) { 1.0 } else { c.g.f(0.3, 3.0) };
            let frame = c.g.r() % c.models[model].frames;
            let e = Entry { id: 0, location: tr, scale: s, frame, model };
            c.log.t("in");
            c.log.i(ortho as i64);
            c.log.h(l);
            c.log.h(t);
            c.log.h(w);
            c.log.h(h);
            c.log.h(s);
            let r = c.entry_in_view(e, &planes);
            c.log.i(r as i64);
            check(&mut golden, "view", n, &mut c.log);
            n += 1;
        }
        for _ in 0..10 {
            count = if c.g.b(10) { 32 } else { c.g.r() % 8 };
            c.free_camera = c.g.b(30);
            let u = c.make_unit(next_id);
            next_id += 1;
            let pixels = if c.g.b(20) { c.g.pick(&PIXELS) } else { c.g.f(0.0, 2.0) };
            let _ = c.transform();
            c.log.t("in");
            c.log.i(count as i64);
            c.log.i(c.free_camera as i64);
            c.log.h(pixels);
            let class = c.classes[c.units[u].class];
            let a = EeGfxPickAdd {
                count,
                capacity: 32,
                pixel_size: pixels,
                unit_type: class.unit_type,
                category: class.category,
                projectile: class.projectile,
                free_camera: c.free_camera,
            };
            if !c.units[u].complex {
                // the driver's own test for a plain unit (its model is not faked)
                c.log.t(if admits(&a) { "skipped" } else { "refused" });
            } else if admits(&a) {
                c.log.t("assign");
                count += 1;
            }
            c.log.t("out");
            c.log.i(count as i64);
            check(&mut golden, "add", n, &mut c.log);
            n += 1;
        }
    }
    let _ = count;
    assert!(golden.next().is_none(), "golden longer than the replay");
}
