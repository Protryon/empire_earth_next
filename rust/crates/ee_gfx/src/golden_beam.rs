//! `EEE3DTracerBeam::Aim` and `DrawPass6`'s stretch against the unmodified C++ (`golden_beam.txt`). A scratch
//! driver (`build/gx12_golden`, `driver_beam.cpp`; PORT_LEARNINGS "R4.4, sixth round") #included
//! `EEE3DTracerBeam.cpp` of the commit before this work over the real GE math: calloc'd shooter and target units
//! (the shooter's vtable a fake one answering `GetModel`, their physical models written raw), tag point lists,
//! turret parts, logging fakes of `EETagPointList::Scale` (the 3 x 3 scaled), `EETagPoint::GetPoint` (a point by
//! tag index and frame through the given transformations), `GetTagPoints` and `FindTurretPart`; and for
//! `DrawPass6` a fake rasterizer whose `Draw` dumps the model as drawn. From an LCG seeded 1 it drew 500 beams
//! (int param 0, 1 or 2, random params 5 and 6), aimed each one to three times at random ages and drew it once,
//! printing an FNV-1a 64 of the whole log and of the Rust-built part (`raim`: the age, length and transformation;
//! `rdraw`: the model as drawn and its sub-models' internal transforms: the lines kept here). This test draws the
//! same stream, replays the C++ glue (the tag points) around `beam.rs` and compares.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::beam::{aim, stretch, stretch_y, EeGfxBeamVertex};
use crate::effects::EeGfxTexCoord;
use crate::golden_farm::dump_mesh;
use crate::golden_mouse::{check, Log, Rng};
use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COORD, EE_GFX_MESH_POINT};

const GOLDEN: &str = include_str!("golden_beam.txt");

struct Placed {
    location: GeTransformation,
    scale: f32,
    frame: u32,
}

fn place(g: &mut Rng) -> Placed {
    let yaw = g.f(-3.2, 3.2);
    let pitch = if g.b(30) { g.f(-0.5, 0.5) } else { 0.0 };
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    t.m[0][3] = g.f(0.0, 100.0);
    t.m[1][3] = g.f(0.0, 100.0);
    t.m[2][3] = g.f(0.0, 8.0);
    let scale = if g.b(50) { 1.0 } else { g.f(0.5, 2.0) };
    Placed { location: t, scale, frame: g.r() % 12 }
}

/// The driver's `EETagPointList::Scale`.
fn scale(t: &mut GeTransformation, s: f32) {
    for r in 0..3 {
        for k in 0..3 {
            t.m[r][k] *= s;
        }
    }
}

/// The driver's tag point in its model's space.
fn tag_local(index: u32, frame: u32) -> Ge3DPoint {
    Ge3DPoint { x: index as f32 * 0.25 + 0.125, y: frame as f32 * 0.0625 - 0.5, z: 0.75 + index as f32 * 0.5 }
}

fn log_t(log: &mut Log, t: &GeTransformation) {
    for row in t.m {
        for f in row {
            log.h(f);
        }
    }
}

#[test]
fn the_beam_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut n = 0;
    let mut branches = [0u32; 4];
    for _ in 0..500 {
        let shooter = place(&mut g);
        let anim = (g.r() % 3) as usize;
        let mut lists = [None; 3];
        for l in &mut lists {
            *l = if g.b(80) { Some(g.r() % 4) } else { None };
        }
        let other = if g.b(70) { Some(g.r() % 3) } else { None };
        let parts = if g.b(40) { 1 + g.r() % 2 } else { 0 };
        let turret_found = g.b(60);
        let turret_index = if parts != 0 { g.r() % parts } else { 0 };
        let mut angles = [0.0f32; 4];
        for a in &mut angles {
            *a = g.f(-3.0, 3.0);
        }
        let default_point = Ge3DPoint { x: g.f(-1.0, 1.0), y: g.f(-1.0, 1.0), z: g.f(0.0, 2.0) };
        let count = lists[anim].unwrap_or(0);
        let targeted = g.b(70);
        let target = if targeted { Some(place(&mut g)) } else { None };
        let int_param = g.r() % 3;
        let grow = if g.b(50) { g.f(0.05, 0.5) } else { 0.0 };
        let shrink = if g.b(50) { g.f(0.4, 0.95) } else { 0.0 };
        let size = g.f(0.25, 2.0);
        let _coloured = g.b(50);
        let _effect_object = g.b(70);
        let _alpha = g.f(0.0, 0.5);
        let target_point = Ge3DPoint { x: g.f(-50.0, 150.0), y: g.f(-50.0, 150.0), z: g.f(-2.0, 10.0) };
        let weapon = if count != 0 { g.r() % count } else { 0 };
        let aims = 1 + g.r() % 3;
        let mut transform = GeTransformation::default();
        for _ in 0..aims {
            let age = if g.b(10) { 0.0 } else { g.f(0.0, 1.2) };
            // Aim's glue: the start at the weapon point, tag kind 6 or the default point; the end on the target
            let mut from = shooter.location;
            let (mut to, target_scale) = match &target {
                Some(t) => (t.location, t.scale),
                None => (GeTransformation::IDENTITY, 1.0),
            };
            let mut start = Ge3DPoint::default();
            if int_param == 1 {
                branches[0] += 1;
                if other.unwrap_or(0) != 0 {
                    scale(&mut from, shooter.scale);
                    start = from.apply_point(tag_local(20, shooter.frame));
                }
            } else if int_param == 0 && count != 0 {
                scale(&mut from, shooter.scale);
                let local = tag_local(anim as u32 * 4 + weapon, shooter.frame);
                if parts > 0 {
                    branches[1] += 1;
                    let mut part = GeTransformation::IDENTITY;
                    if turret_found {
                        part.set_orientation(RotationOrder::Ypr, angles[turret_index as usize], 0.0, 0.0);
                    }
                    start = from.apply_point(part.apply_point(local));
                } else {
                    branches[2] += 1;
                    start = from.apply_point(local);
                }
                scale(&mut to, target_scale);
            } else {
                branches[3] += 1;
                start = from.apply_point(default_point);
            }
            let end = if target.is_some() { to.apply_point(target_point) } else { target_point };
            let a = aim(start, end, age, grow, shrink);
            transform = a.transform;
            log.h(age);
            log.h(a.length);
            log_t(&mut log, &a.transform);
            check(&mut golden, "raim", n, &mut log);
            n += 1;
        }
        // DrawPass6
        let count = (1 + g.r() % 12) as usize;
        let mut vertices = vec![EeGfxBeamVertex::default(); count];
        for v in &mut vertices {
            v.point = Ge3DPoint { x: g.f(-1.0, 1.0), y: g.f(-0.5, 0.5), z: g.f(-1.0, 1.0) };
            v.coord = EeGfxTexCoord { u: g.f(0.0, 1.0), v: g.f(0.0, 1.0) };
        }
        let subs = g.r() % 3;
        let mut internal = Vec::new();
        for _ in 0..subs {
            let mut t = GeTransformation::IDENTITY;
            t.m[1][3] = g.f(-2.0, 2.0);
            t.m[0][3] = g.f(-1.0, 1.0);
            internal.push(t);
        }
        let length = g.f(0.0, 30.0);
        let width = g.f(0.1, 2.0);
        let scroll = g.f(0.0, 1.0);
        let _ortho = g.b(20);
        let _screen = g.r();
        let _fog = g.r();
        stretch(&mut vertices, length, width, scroll);
        let mesh: Vec<EeGfxMeshVertex> = vertices
            .iter()
            .map(|v| EeGfxMeshVertex {
                set: EE_GFX_MESH_POINT | EE_GFX_MESH_COORD,
                point: v.point,
                coord: v.coord,
                ..Default::default()
            })
            .collect();
        log.t("model");
        log.i(0);
        dump_mesh(&mut log, &mesh, &[EeGfxMeshTriangle::default(); 4], Ge3DPoint::default());
        let cut = log.0.rfind(" place").unwrap();
        log.0.truncate(cut);
        log.t("mat");
        for _ in 0..4 {
            log.i(77);
        }
        log.t("scale");
        log.h(0.0);
        log.t("post");
        let mut s = GeTransformation::IDENTITY;
        s.set_scale(size, size, size);
        log_t(&mut log, &transform.compose(&s));
        log.i(0);
        for t in &internal {
            let mut t = *t;
            t.m[1][3] = stretch_y(t.m[1][3], length);
            log.t("sub");
            log_t(&mut log, &t);
        }
        check(&mut golden, "rdraw", n, &mut log);
        n += 1;
    }
    assert!(golden.next().is_none());
    assert!(branches.iter().all(|&b| b > 50), "{branches:?}");
}
