//! The camera's math against the unmodified C++ (`golden_camera.txt`). A scratch driver (`build/camera_golden`,
//! PORT_LEARNINGS "R4.4") compiled `EECameraControl.cpp` of the commit before this work with the real GE math and
//! `GEViewport` over a fake map: a terrain mesh of rows (rectangles and diamonds, 4 to 63 tiles a side, the real
//! edge planes) whose height is a hash per half tile, a view ray from the viewport's camera, a ground hit at height 1,
//! and a world whose camera transformation is recorded. From an LCG seeded 1 it drew 24 meshes with 600 cases each
//! (GetPitch, GetOffset/GetLocation, GoTo, ClampHeight, BuildView, SetClippingDistances, ClipToEdges with
//! GetGroundPoint, Scroll, Update with and without a key path, and the movers that stay C++) and printed per case an
//! FNV-1a 64 of its whole log: the inputs, every height query with its answer, the recorded camera and the outputs.
//! This test draws the same stream, runs each case through `camera.rs` and the C++ glue rewritten here, builds the
//! same log and compares the digests; the driver relinked over the forwarders printed the same file. The C++-only
//! movers (`cpp`, MoveTo with locations, Revolve, Rotate, ...) only advance the stream here.
//!
//! The generator keeps the camera out of two hazards of the original: no unit is followed (the handles stay empty),
//! and the free camera's zoom stays within 1 (past it the pitch is NaN and the table cosine faults; see
//! `pitch`'s note in PORT_LEARNINGS).

use std::fmt::Write as _;

use ee_ge::terrain::{edge_planes, GeTmRow};
use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DLine, Ge3DPoint, GeQuaternion, GeRectangle, GeTransformation, GeVector};
use ee_ge::um::floor_unsigned;
use ee_ge::viewport::GeViewport;

use crate::camera::*;

const GOLDEN: &str = include_str!("golden_camera.txt");

// ---- the driver's random stream ----
struct Rng(u32);
impl Rng {
    fn r(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0 >> 8
    }
    fn f(&mut self, lo: f32, hi: f32) -> f32 {
        lo + (hi - lo) * ((self.r() & 0xffff) as f32) / 65535.0
    }
    fn b(&mut self, pct: u32) -> bool {
        self.r() % 100 < pct
    }
    fn pick(&mut self, v: &[f32]) -> f32 {
        v[(self.r() % v.len() as u32) as usize]
    }
}

// ---- the log ----
#[derive(Default)]
struct Log(String);
impl Log {
    fn h(&mut self, f: f32) {
        write!(self.0, " {:08x}", f.to_bits()).unwrap();
    }
    fn i(&mut self, v: i64) {
        write!(self.0, " {v}").unwrap();
    }
    fn t(&mut self, t: &str) {
        self.0.push(' ');
        self.0.push_str(t);
    }
    fn p(&mut self, p: Ge3DPoint) {
        self.h(p.x);
        self.h(p.y);
        self.h(p.z);
    }
    fn v(&mut self, p: GeVector) {
        self.h(p.x);
        self.h(p.y);
        self.h(p.z);
    }
    fn tr(&mut self, t: &GeTransformation) {
        for r in 0..4 {
            for c in 0..4 {
                self.h(t.m[r][c]);
            }
        }
    }
    fn q(&mut self, q: GeQuaternion) {
        self.h(q.x);
        self.h(q.y);
        self.h(q.z);
        self.h(q.w);
    }
    fn l(&mut self, l: &Ge3DLine) {
        self.p(l.start);
        self.v(l.direction);
    }
}

fn digest(line: &str) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in line.bytes() {
        h ^= b as u64;
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

/// `(long)f` on the driver's Windows target: cvttss2si, 0x80000000 out of range.
fn trunc_long(v: f32) -> i32 {
    if v.is_nan() || v >= 2147483648.0 || v < -2147483648.0 {
        i32::MIN
    } else {
        v as i32
    }
}

// ---- the fake world ----
#[derive(Clone, Copy, Default)]
struct Row {
    start: u32,
    count: u32,
}

struct Mesh {
    w: u32,
    h: u32,
    seed: u32,
    tiles: Vec<Row>,
    points: Vec<Row>,
    planes: EeGfxEdgePlanes,
}

impl Mesh {
    fn hash(&self, x: u32, y: u32) -> u32 {
        let mut h = x.wrapping_mul(0x9E3779B1) ^ y.wrapping_add(0x7F4A7C15).wrapping_mul(0x85EBCA77) ^ self.seed;
        h ^= h >> 15;
        h = h.wrapping_mul(0x2C1B3C6D);
        h ^= h >> 12;
        h
    }
    fn valid(rows: &[Row], x: f32, y: f32) -> bool {
        let (x, y) = (floor_unsigned(x), floor_unsigned(y));
        (y as usize) < rows.len() && x.wrapping_sub(rows[y as usize].start) < rows[y as usize].count
    }
}

/// The terrain as the driver's mesh: inline validity tests, the logged hash height.
struct FakeGround<'a> {
    mesh: &'a Mesh,
    log: &'a mut Log,
}

impl Ground for FakeGround<'_> {
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        Mesh::valid(&self.mesh.tiles, x, y)
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        Mesh::valid(&self.mesh.points, x, y)
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        let hx = trunc_long(x * 2.0) as u32;
        let hy = trunc_long(y * 2.0) as u32;
        let z = (self.mesh.hash(hx, hy) & 1023) as f32 * (1.0 / 128.0);
        self.log.t("Z");
        self.log.h(x);
        self.log.h(y);
        self.log.h(z);
        z
    }
}

/// The driver's `EEMap::GetScreenLine`.
fn screen_line(vp: &GeViewport, px: f32, py: f32) -> Ge3DLine {
    let c = &vp.camera.m;
    let u = (px - vp.x_center) * 0.002;
    let v = (py - vp.y_center) * 0.002;
    let d = GeVector::new(
        c[0][1] + u * c[0][0] - v * c[0][2],
        c[1][1] + u * c[1][0] - v * c[1][2],
        c[2][1] + u * c[2][0] - v * c[2][2],
    );
    Ge3DLine {
        start: Ge3DPoint::new(c[0][3], c[1][3], c[2][3]),
        direction: GeVector::new(d.x * 60.0, d.y * 60.0, d.z * 60.0),
    }
}

/// The driver's `RTSMap::GetLocationOnLine` (height 1, on the map's rectangle), logged.
fn location_on_line(mesh: &Mesh, line: &Ge3DLine, log: &mut Log) -> Option<Ge3DPoint> {
    let hit = match line.solve_for_z_point(1.0) {
        Some((t, p)) if t >= 0.0 && p.x >= 0.0 && p.y >= 0.0 && p.x < mesh.w as f32 && p.y < mesh.h as f32 => Some(p),
        _ => None,
    };
    log.t("G");
    log.l(line);
    log.i(hit.is_some() as i64);
    if let Some(p) = hit {
        log.p(p);
    }
    hit
}

#[derive(Clone, Copy, Default)]
struct Key {
    t00: GeTransformation,
    t40: GeTransformation,
    offset: Ge3DPoint,
    focus: Ge3DPoint,
    wait: f32,
    rate: f32,
    changed: bool,
}

#[derive(Clone, Default)]
struct Cam {
    keys: Vec<Key>,
    q014: GeQuaternion,
    q024: GeQuaternion,
    start_offset: Ge3DPoint,
    start_focus: Ge3DPoint,
    q04c: GeQuaternion,
    q05c: GeQuaternion,
    end_offset: Ge3DPoint,
    end_focus: Ge3DPoint,
    view: GeTransformation,
    target: Ge3DPoint,
    fraction: f32,
    rate: f32,
    zoom: f32,
    wait: f32,
    waited: f32,
    turn_yaw: f32,
    turn_pitch: f32,
    mode: i32,
    path_index: i32,
    changed: bool,
    following_b: bool,
    locked: bool,
}

#[derive(Default)]
struct Globals {
    free_camera: bool,
    ortho: bool,
    far_rear: bool,
    cinematic: bool,
    world: GeTransformation,
    camera_yaw: f32,
}

struct World {
    rng: Rng,
    mesh: Mesh,
    vp: GeViewport,
    g: Globals,
    cam: Cam,
    log: Log,
}

const DEFAULT_YAW: f32 = 0.7853982;
const FIELD_OF_VIEW: f32 = 0.017453292 * 30.0;

fn identity() -> GeTransformation {
    GeTransformation::IDENTITY
}

fn ypr(t: &mut GeTransformation, y: f32, p: f32, r: f32) {
    t.set_orientation(RotationOrder::Ypr, y, p, r);
}

impl World {
    // ---- the driver's generators ----
    fn make_mesh(&mut self) {
        let w = 4 + self.rng.r() % 60;
        let h = 4 + self.rng.r() % 60;
        let diamond = self.rng.b(50);
        let mut tiles = vec![Row::default(); h as usize];
        for y in 0..h {
            let mut inset = if diamond { ((y as i32 - (h / 2) as i32).unsigned_abs()) / 2 } else { 0 };
            if inset * 2 >= w {
                inset = 0;
            }
            tiles[y as usize] = Row { start: inset, count: w - 2 * inset };
        }
        let mut points = vec![Row::default(); h as usize + 1];
        for y in 0..=h {
            let r = if y < h { y } else { h - 1 } as usize;
            points[y as usize] = Row { start: tiles[r].start, count: tiles[r].count + 1 };
        }
        let raw: Vec<GeTmRow> = tiles
            .iter()
            .map(|r| GeTmRow { start: r.start as _, count: r.count as _, items: std::ptr::null() })
            .collect();
        // SAFETY: `raw` holds `h` rows
        let p = unsafe { edge_planes(raw.as_ptr(), h as _) };
        let seed = self.rng.r();
        self.mesh = Mesh {
            w,
            h,
            seed,
            tiles,
            points,
            planes: EeGfxEdgePlanes { front: p[0], back: p[1], left: p[2], right: p[3] },
        };
        let l = &mut self.log;
        l.t("mesh");
        l.i(w as i64);
        l.i(h as i64);
        l.i(diamond as i64);
        l.i(seed as i64);
        for r in &self.mesh.tiles {
            l.i(r.start as i64);
            l.i(r.count as i64);
        }
        for plane in [p[0], p[1], p[2], p[3]] {
            l.p(plane.point);
            l.v(plane.normal);
        }
    }
    fn quat(&mut self) -> GeQuaternion {
        let mut t = identity();
        let y = self.rng.f(-3.2, 3.2);
        let p = self.rng.f(-1.5, 1.5);
        let r = if self.rng.b(30) { self.rng.f(-0.5, 0.5) } else { 0.0 };
        ypr(&mut t, y, p, r);
        GeQuaternion::from_transformation(&t)
    }
    fn p3(&mut self, lx: f32, hx: f32, ly: f32, hy: f32, lz: f32, hz: f32) -> Ge3DPoint {
        let x = self.rng.f(lx, hx);
        let y = self.rng.f(ly, hy);
        let z = self.rng.f(lz, hz);
        Ge3DPoint::new(x, y, z)
    }
    fn focus(&mut self) -> Ge3DPoint {
        let (w, h) = (self.mesh.w as f32 + 4.0, self.mesh.h as f32 + 4.0);
        self.p3(-4.0, w, -4.0, h, -1.0, 6.0)
    }
    fn offset(&mut self) -> Ge3DPoint {
        if self.rng.b(15) {
            return self.p3(-0.3, 0.3, -0.3, 0.3, -0.3, 0.3);
        }
        self.p3(-2.0, 2.0, -45.0, 0.0, -2.0, 2.0)
    }
    fn transform(&mut self) -> GeTransformation {
        let mut t = identity();
        let y = self.rng.f(-3.2, 3.2);
        let p = self.rng.f(-1.5, 1.5);
        let r = if self.rng.b(20) { self.rng.f(-0.4, 0.4) } else { 0.0 };
        ypr(&mut t, y, p, r);
        if self.rng.b(10) {
            let p = if self.rng.b(50) { 1.5707964 } else { -1.5707964 };
            ypr(&mut t, 0.0, p, 0.0);
        }
        let (w, h) = (self.mesh.w as f32 + 4.0, self.mesh.h as f32 + 4.0);
        let pos = self.p3(-4.0, w, -4.0, h, -2.0, 30.0);
        t.m[0][3] = pos.x;
        t.m[1][3] = pos.y;
        t.m[2][3] = pos.z;
        t
    }
    fn random_camera(&mut self) {
        let mut c = Cam { keys: Vec::new(), ..self.cam.clone() };
        c.q014 = self.quat();
        c.q024 = self.quat();
        c.q04c = if self.rng.b(20) { c.q014 } else { self.quat() };
        c.q05c = self.quat();
        c.start_offset = self.offset();
        c.end_offset = if self.rng.b(20) { c.start_offset } else { self.offset() };
        c.start_focus = self.focus();
        c.end_focus = if self.rng.b(20) { c.start_focus } else { self.focus() };
        c.view = self.transform();
        c.target = Ge3DPoint::new(-10000.0, 0.0, 0.0);
        c.fraction = if self.rng.b(60) { self.rng.pick(&[0.0, 1.0, 0.5, 0.999, 0.25]) } else { self.rng.f(0.0, 1.0) };
        c.rate = if self.rng.b(10) { 0.0 } else { self.rng.f(0.05, 8.0) };
        c.zoom = if self.rng.b(50) { self.rng.pick(&[0.0, 1.0, 0.5, 2.0, -0.5, 1.5]) } else { self.rng.f(-1.2, 2.2) };
        c.wait = if self.rng.b(30) { 0.0 } else { self.rng.f(0.0, 3.0) };
        c.waited = if self.rng.b(30) { 0.0 } else { self.rng.f(0.0, 3.0) };
        c.turn_yaw = if self.rng.b(70) { 0.0 } else { self.rng.f(-1.0, 1.0) };
        c.turn_pitch = if self.rng.b(70) { 0.0 } else { self.rng.f(-1.0, 1.0) };
        c.mode = (self.rng.r() % 4) as i32;
        if c.mode == 3 && (c.zoom > 1.0 || c.zoom < -1.0) {
            c.zoom = self.rng.f(-1.0, 1.0);
        }
        c.path_index = -1;
        c.changed = self.rng.b(40);
        c.following_b = false;
        c.locked = self.rng.b(15);
        self.cam = c;
    }
    fn random_globals(&mut self) {
        self.g.free_camera = self.rng.b(15);
        self.g.ortho = self.rng.b(25);
        self.g.far_rear = self.rng.b(20);
        self.g.cinematic = self.rng.b(20);
        self.g.world = self.transform();
        self.g.camera_yaw = self.rng.f(-3.0, 3.0);
    }
    fn random_viewport(&mut self) {
        let w = self.rng.pick(&[640.0, 800.0, 1024.0, 1920.0]);
        let h = self.rng.pick(&[480.0, 600.0, 768.0, 1080.0]);
        self.vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top: 0.0 });
        let fov = self.rng.f(0.3, 1.2);
        self.vp.set_field_of_view(fov);
        let front = self.rng.f(0.1, 3.0);
        self.vp.set_front_clipping_distance(front);
        let rear = self.rng.f(30.0, 90.0);
        self.vp.set_rear_clipping_distance(rear);
        let camera = self.transform();
        self.vp.set_camera(&camera);
    }
    fn random_key(&mut self) -> Key {
        let mut k = Key { t00: self.transform(), ..Key::default() };
        k.t00.m[0][3] = 0.0;
        k.t00.m[1][3] = 0.0;
        k.t00.m[2][3] = 0.0;
        k.t40 = self.transform();
        k.t40.m[0][3] = 0.0;
        k.t40.m[1][3] = 0.0;
        k.t40.m[2][3] = 0.0;
        k.offset = self.offset();
        k.focus = self.focus();
        k.wait = if self.rng.b(40) { 0.0 } else { self.rng.f(0.0, 2.0) };
        k.rate = if self.rng.b(60) { self.rng.pick(&[1.0, 0.0, 0.5, 4.0]) } else { self.rng.f(0.01, 5.0) };
        k.changed = self.rng.b(30);
        k
    }

    // ---- the driver's dumps ----
    fn dump_camera(&mut self, tag: &str) {
        let c = &self.cam;
        let l = &mut self.log;
        l.t(tag);
        l.q(c.q014);
        l.q(c.q024);
        l.p(c.start_offset);
        l.p(c.start_focus);
        l.q(c.q04c);
        l.q(c.q05c);
        l.p(c.end_offset);
        l.p(c.end_focus);
        l.tr(&c.view);
        l.p(c.target);
        for f in [c.fraction, c.rate, c.zoom, c.wait, c.waited, c.turn_yaw, c.turn_pitch] {
            l.h(f);
        }
        for i in [c.mode as i64, c.path_index as i64, c.changed as i64, c.following_b as i64, c.locked as i64] {
            l.i(i);
        }
        l.i(c.keys.len() as i64);
    }
    fn dump_globals(&mut self) {
        let l = &mut self.log;
        l.t("g");
        for b in [self.g.free_camera, self.g.ortho, self.g.far_rear, self.g.cinematic] {
            l.i(b as i64);
        }
        l.tr(&self.g.world);
        l.h(self.g.camera_yaw);
    }
    fn dump_viewport(&mut self, tag: &str) {
        let v = &self.vp;
        let l = &mut self.log;
        l.t(tag);
        for f in [
            v.x_center,
            v.y_center,
            v.half_height,
            v.front_clipping_distance,
            v.rear_clipping_distance,
            v.field_of_view,
        ] {
            l.h(f);
        }
        let o = v.orthographic_rectangle;
        for f in [o.left, o.top, o.right, o.bottom] {
            l.h(f);
        }
        l.tr(&v.camera);
    }
    fn dump_key(&mut self, k: &Key) {
        let l = &mut self.log;
        l.t("k");
        l.tr(&k.t00);
        l.tr(&k.t40);
        l.p(k.offset);
        l.p(k.focus);
        l.h(k.wait);
        l.h(k.rate);
        l.i(k.changed as i64);
    }

    // ---- EECameraControl's glue over camera.rs ----
    fn plain(&self) -> bool {
        !self.g.free_camera && !self.cam.locked
    }
    fn go_to(&mut self, k: &Key) {
        let c = &mut self.cam;
        // KeepPose
        c.q014 = c.q014.slerp(&c.q04c, c.fraction);
        c.q024 = c.q024.slerp(&c.q05c, c.fraction);
        c.start_offset = lerp(c.start_offset, c.end_offset, c.fraction);
        c.start_focus = lerp(c.start_focus, c.end_focus, c.fraction);
        c.q04c = GeQuaternion::from_transformation(&k.t00);
        c.q05c = GeQuaternion::from_transformation(&k.t40);
        c.end_offset = k.offset;
        c.end_focus = k.focus;
        c.wait = k.wait;
        c.changed = k.changed;
        let g = go_to_speed(c.start_offset, c.end_offset, c.start_focus, c.end_focus, k.rate, false, 0.0);
        if g.snap_offset {
            c.start_offset = c.end_offset;
        }
        if g.snap_focus {
            c.start_focus = c.end_focus;
        }
        c.fraction = 0.0;
        c.rate = g.speed;
        c.waited = 0.0;
    }
    /// `MoveTo` with no unit and no location (what `SetZoom` passes).
    fn move_to_nothing(&mut self, duration: f32, zoom: f32) {
        let mut k = Key { offset: self.cam.end_offset, focus: self.cam.end_focus, ..Key::default() };
        k.t40 = self.cam.q05c.to_transformation();
        if zoom >= -1.0 {
            let z = if zoom < 0.0 { 0.0 } else { zoom };
            let free = self.g.free_camera;
            let far = if self.cam.mode != 3 && !free && !self.g.far_rear { -20.5 } else { -42.0 };
            let height = if self.cam.mode != 3 && !free { 0.1 } else { 0.0 };
            k.offset.y = (far - height) * z + height;
        }
        if self.cam.mode != 2 && zoom >= -1.0 {
            let p = pitch(zoom, self.cam.mode, &self.g.world);
            if self.cam.turn_yaw == 0.0 {
                ypr(&mut k.t40, DEFAULT_YAW, p, 0.0);
            } else {
                ypr(&mut k.t40, self.g.camera_yaw, p, 0.0);
            }
        }
        k.t00 = identity();
        k.wait = 0.0;
        k.rate = if duration != 0.0 { 1.0 / duration } else { 0.0 };
        k.changed = duration == 0.0;
        self.go_to(&k);
    }
    fn set_zoom(&mut self, zoom: f32) {
        if self.cam.locked {
            return;
        }
        self.cam.zoom = zoom;
        self.move_to_nothing(0.3, zoom);
    }
    fn ground(&mut self) -> FakeGround<'_> {
        FakeGround { mesh: &self.mesh, log: &mut self.log }
    }
    fn build_view(&mut self, step: f32) -> GeTransformation {
        let c = &self.cam;
        let pose = EeGfxCameraPose {
            view_start: c.q014,
            view_end: c.q04c,
            offset_start: c.q024,
            offset_end: c.q05c,
            start_offset: c.start_offset,
            end_offset: c.end_offset,
            start_focus: c.start_focus,
            end_focus: c.end_focus,
        };
        let (mode, front, plain) = (c.mode, self.vp.front_clipping_distance, self.plain());
        build_view(&pose, step, mode, front, plain, &mut self.ground())
    }
    fn set_clipping(&mut self) {
        let rays;
        let mut camera = Ge3DPoint::default();
        let v = &self.vp;
        if !self.g.ortho {
            rays = Some(EeGfxCameraRays {
                center: screen_line(v, v.x_center, v.y_center),
                top: screen_line(v, v.x_center, v.y_center + v.half_height),
                bottom: screen_line(v, v.x_center, v.y_center - v.half_height),
            });
            camera = Ge3DPoint::new(v.camera.m[0][3], v.camera.m[1][3], v.camera.m[2][3]);
        } else {
            rays = None;
        }
        let r = clipping_distances(
            rays.as_ref(),
            camera,
            self.cam.mode,
            self.cam.zoom,
            self.g.free_camera,
            self.g.far_rear,
        );
        self.vp.set_front_clipping_distance(r.front);
        self.vp.set_rear_clipping_distance(r.rear);
    }
    fn ground_point(&mut self, line: &Ge3DLine) -> Ge3DPoint {
        let offset = lerp(self.cam.start_offset, self.cam.end_offset, self.cam.fraction);
        let (planes, mode) = (self.mesh.planes, self.cam.mode);
        ground_point(line, &planes, offset, mode, &mut self.ground())
    }
    fn scroll(&mut self, dx: f32, dy: f32) {
        let v = &self.vp;
        let center = Ge2DPoint { x: v.x_center, y: v.y_center + v.half_height };
        let scale = self.cam.zoom + 0.5;
        let minimum = minimum_height(self.cam.mode);
        let the_pitch = pitch(self.cam.zoom, self.cam.mode, &self.g.world);
        if self.cam.locked {
            return;
        }
        self.cam.following_b = false;
        self.cam.target.x = -10000.0;
        self.cam.turn_yaw = 0.0;
        self.cam.turn_pitch = 0.0;
        if !self.g.cinematic && self.cam.mode == 3 {
            // SetMode(0) (its SetZoom), then ResetZoom
            self.cam.mode = 0;
            let z = self.cam.zoom;
            self.set_zoom(z);
            self.set_zoom(1.0);
        }
        let direction = scroll_direction(&self.cam.view, dx, dy, scale);
        let start = lerp(self.cam.start_focus, self.cam.end_focus, self.cam.fraction);
        let line = Ge3DLine { start, direction };
        let mut target = self.ground_point(&line);
        let offset = lerp(self.cam.start_offset, self.cam.end_offset, self.cam.fraction);
        if scroll_is_near(offset) {
            let line = screen_line(&self.vp, center.x, center.y);
            if let Some(mut g) = location_on_line(&self.mesh, &line, &mut self.log) {
                g.z = g.z + minimum;
                if g.z > target.z {
                    target.z = g.z;
                }
            }
        }
        target = clamp_to_map(target, &self.mesh.planes);
        let mut k = Key { focus: target, ..Key::default() };
        if self.cam.mode != 2 {
            k.t40 = identity();
            ypr(&mut k.t40, DEFAULT_YAW, the_pitch, 0.0);
            self.cam.changed = true;
        } else {
            k.t40 = self.cam.q05c.to_transformation();
        }
        k.t00 = self.cam.q04c.to_transformation();
        k.offset = self.cam.end_offset;
        k.wait = 0.0;
        k.rate = 1.0;
        k.changed = self.cam.changed;
        self.go_to(&k);
        self.cam.start_focus.x = self.cam.end_focus.x;
        self.cam.start_focus.y = self.cam.end_focus.y;
    }
    fn update(&mut self, elapsed: f32) {
        let c = &mut self.cam;
        let mut s = EeGfxCameraStep {
            fraction: c.fraction,
            rate: c.rate,
            wait: c.wait,
            waited: c.waited,
            changed: c.changed,
            locked: c.locked,
        };
        let action = step(&mut s, elapsed);
        c.fraction = s.fraction;
        c.waited = s.waited;
        c.changed = s.changed;
        if action == CAMERA_STEP_NEXT_KEY {
            // NextKey
            c.path_index += 1;
            if c.path_index < c.keys.len() as i32 {
                let k = c.keys[c.path_index as usize];
                self.go_to(&k);
                self.cam.locked = true;
            } else {
                c.keys.clear();
                c.locked = false;
                c.waited = 0.0;
            }
            return;
        }
        if action == CAMERA_STEP_DONE {
            return;
        }
        if !self.g.ortho {
            let f = self.cam.fraction;
            self.cam.view = self.build_view(f);
        } else {
            let c = &self.cam;
            let focus = lerp(c.start_focus, c.end_focus, c.fraction);
            let offset = lerp(c.start_offset, c.end_offset, c.fraction);
            let o = ortho_view(focus, offset.y);
            self.cam.view = o.view;
            self.vp.set_orthographic_rectangle(&GeRectangle {
                bottom: o.size,
                left: -o.size,
                right: o.size,
                top: -o.size,
            });
        }
        let view = self.cam.view;
        self.log.t("C");
        self.log.tr(&view);
        self.vp.set_camera(&view);
        self.vp.set_field_of_view(FIELD_OF_VIEW);
        self.set_clipping();
    }
}

#[test]
fn camera_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut w = World {
        rng: Rng(1),
        mesh: Mesh { w: 1, h: 1, seed: 0, tiles: Vec::new(), points: Vec::new(), planes: EeGfxEdgePlanes::default() },
        vp: GeViewport::default(),
        g: Globals::default(),
        cam: Cam {
            target: Ge3DPoint::new(-10000.0, 0.0, 0.0),
            path_index: -1,
            zoom: 0.5,
            view: identity(),
            ..Cam::default()
        },
        log: Log::default(),
    };
    let mut checked = std::collections::BTreeMap::<&str, u32>::new();
    let mut emit = |w: &mut World, kind: &'static str, n: i64, golden: &mut std::str::Lines| {
        let line = format!("{kind} {n}{}", w.log.0);
        w.log.0.clear();
        let want = golden.next().expect("golden too short");
        let got = format!("{kind} {n} {:016x}", digest(&line));
        if kind != "cpp" {
            assert_eq!(got, want, "case {kind} {n}: the replay's log is\n{line}");
            *checked.entry(kind).or_default() += 1;
        } else {
            assert!(want.starts_with(&format!("cpp {n} ")), "{want}");
        }
    };
    for world in 0..24i64 {
        w.make_mesh();
        emit(&mut w, "mesh", world, &mut golden);
        w.random_viewport();
        let mut n = world * 1000;
        for _ in 0..40 {
            let z = if w.rng.b(50) { w.rng.pick(&[0.0, 1.0, -1.0, 0.5, 2.0, -0.25, 1.5]) } else { w.rng.f(-1.5, 2.5) };
            let mode = (w.rng.r() % 5) as i32;
            w.g.world = w.transform();
            w.log.t("in");
            w.log.h(z);
            w.log.i(mode as i64);
            let world_t = w.g.world;
            w.log.tr(&world_t);
            let p = pitch(z, mode, &world_t);
            w.log.t("out");
            w.log.h(p);
            emit(&mut w, "pitch", n, &mut golden);
            n += 1;
        }
        for _ in 0..40 {
            w.random_camera();
            let step = if w.rng.b(30) { (w.rng.r() % 3) as f32 * 0.5 } else { w.rng.f(-0.2, 1.2) };
            w.dump_camera("s");
            w.log.h(step);
            let a = lerp(w.cam.start_offset, w.cam.end_offset, step);
            let b = lerp(w.cam.start_focus, w.cam.end_focus, step);
            w.log.t("out");
            w.log.p(a);
            w.log.p(b);
            emit(&mut w, "lerp", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            w.random_camera();
            w.random_globals();
            let mut k = w.random_key();
            if w.rng.b(20) {
                k.offset = w.cam.start_offset;
                w.cam.fraction = 0.0;
            }
            if w.rng.b(20) {
                k.focus = w.cam.start_focus;
                w.cam.fraction = 0.0;
            }
            w.dump_camera("s");
            w.dump_globals();
            w.dump_key(&k);
            w.go_to(&k);
            w.dump_camera("out");
            emit(&mut w, "goto", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            w.random_camera();
            w.random_globals();
            let mut t = w.transform();
            if w.rng.b(30) {
                let (mw, mh) = (w.mesh.w as f32, w.mesh.h as f32);
                let q = w.p3(0.0, mw, 0.0, mh, -1.0, 1.0);
                t.m[0][3] = q.x;
                t.m[1][3] = q.y;
                t.m[2][3] = q.z;
            }
            let front = if w.rng.b(50) { 0.1 } else { w.rng.f(0.1, 6.0) };
            w.vp.set_front_clipping_distance(front);
            w.dump_camera("s");
            w.dump_globals();
            w.log.t("t");
            w.log.tr(&t);
            let front = w.vp.front_clipping_distance;
            w.log.h(front);
            w.log.t("calls");
            let (mode, plain) = (w.cam.mode, w.plain());
            clamp_height(&mut t, mode, front, plain, &mut w.ground());
            w.log.t("out");
            w.log.tr(&t);
            emit(&mut w, "clamp", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            w.random_camera();
            w.random_globals();
            let step = if w.rng.b(40) { w.cam.fraction } else { w.rng.f(0.0, 1.0) };
            let front = if w.rng.b(50) { 0.1 } else { w.rng.f(0.1, 6.0) };
            w.vp.set_front_clipping_distance(front);
            w.dump_camera("s");
            w.dump_globals();
            w.log.h(step);
            let front = w.vp.front_clipping_distance;
            w.log.h(front);
            w.log.t("calls");
            let out = w.build_view(step);
            w.log.t("out");
            w.log.tr(&out);
            emit(&mut w, "build", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            w.random_camera();
            w.random_globals();
            w.random_viewport();
            w.dump_camera("s");
            w.dump_globals();
            w.dump_viewport("v");
            w.log.t("calls");
            w.set_clipping();
            w.log.t("out");
            let (f, r) = (w.vp.front_clipping_distance, w.vp.rear_clipping_distance);
            w.log.h(f);
            w.log.h(r);
            emit(&mut w, "clip", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            w.random_camera();
            let s0 = w.focus();
            let dd = w.p3(-30.0, 30.0, -30.0, 30.0, -3.0, 3.0);
            let mut d = GeVector::new(dd.x, dd.y, dd.z);
            if w.rng.b(10) {
                d = GeVector::default();
            }
            let line = Ge3DLine { start: s0, direction: d };
            let p = w.focus();
            w.dump_camera("s");
            w.log.t("l");
            w.log.l(&line);
            w.log.p(p);
            w.log.t("calls");
            let p = clip_to_edges(&line, &w.mesh.planes, p);
            w.log.t("edge");
            w.log.p(p);
            let g = w.ground_point(&line);
            w.log.t("out");
            w.log.p(g);
            emit(&mut w, "ground", n, &mut golden);
            n += 1;
        }
        for _ in 0..80 {
            w.random_camera();
            w.random_globals();
            w.random_viewport();
            if w.rng.b(70) && w.cam.mode == 3 {
                w.cam.mode = (w.rng.r() % 3) as i32;
            }
            let mut dx = if w.rng.b(15) { 0.0 } else { w.rng.f(-3.0, 3.0) };
            let mut dy = if w.rng.b(15) { 0.0 } else { w.rng.f(-3.0, 3.0) };
            if w.rng.b(10) {
                ypr(&mut w.cam.view, 0.0, -1.5707964, 0.0);
            }
            if w.rng.b(10) {
                dx = 0.0;
                dy = 0.0;
            }
            w.dump_camera("s");
            w.dump_globals();
            w.dump_viewport("v");
            w.log.h(dx);
            w.log.h(dy);
            w.log.t("calls");
            w.scroll(dx, dy);
            w.dump_camera("out");
            emit(&mut w, "scroll", n, &mut golden);
            n += 1;
        }
        for _ in 0..80 {
            w.random_camera();
            w.random_globals();
            w.random_viewport();
            let keys = if w.rng.b(30) { 1 + w.rng.r() % 3 } else { 0 };
            w.cam.locked = keys != 0 && w.rng.b(70);
            for _ in 0..keys {
                let k = w.random_key();
                w.cam.keys.push(k);
            }
            w.cam.path_index = if keys != 0 { (w.rng.r() % keys) as i32 } else { -1 };
            let elapsed = if w.rng.b(20) { 0.0 } else { w.rng.f(0.0, 0.5) };
            w.dump_camera("s");
            w.dump_globals();
            w.dump_viewport("v");
            w.log.h(elapsed);
            for k in w.cam.keys.clone() {
                w.dump_key(&k);
            }
            w.log.t("calls");
            w.update(elapsed);
            w.dump_camera("out");
            w.dump_viewport("vout");
            emit(&mut w, "update", n, &mut golden);
            n += 1;
        }
        for _ in 0..60 {
            // the C++-only movers: the stream only
            w.random_camera();
            w.random_globals();
            w.random_viewport();
            let _which = w.rng.r() % 10;
            let _a = w.focus();
            let _b = w.focus();
            let _t = w.transform();
            let _r1 = w.rng.f(0.0, 2.0);
            let _b1 = w.rng.b(50);
            let _b2 = w.rng.b(50);
            let _r2 = if w.cam.mode == 3 { w.rng.f(-1.0, 1.0) } else { w.rng.f(-1.5, 1.5) };
            let _r3 = w.rng.f(-1.0, 1.0);
            let _r4 = w.rng.f(-1.0, 1.0);
            let _m = w.rng.r() % 4;
            w.log.0.clear();
            emit(&mut w, "cpp", n, &mut golden);
            n += 1;
        }
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
    let total: u32 = checked.values().sum();
    assert_eq!(total, 24 * (1 + 540), "{checked:?}");
}
