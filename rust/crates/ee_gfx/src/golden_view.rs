//! The effects' frustum test and `GetUnitOpacity` against the unmodified C++ (`golden_effects.txt`,
//! `golden_opacity.txt`). Two scratch drivers (`build/gx9_golden`; PORT_LEARNINGS "R4.4, third round"):
//! `driver_effects.cpp` #included `EEEffect2D.cpp` and `EEEffect3D.cpp` of the commit before this work over the real
//! GE math and `GEViewport`, zeroed effects (the 3D one's vtable logs: slot 21 `IsTileVisible`), a 70-tile map of
//! fake tiles (a tenth missing) and logging `EEEffect::Queue` and `EEMapTile::IsSeenBy`; from an LCG seeded 1 it drew
//! 24 viewports with 100 effects each, most on a screen point's ray. `driver_opacity.cpp` #included
//! `EERenderObject.cpp` with zeroed units and a class whose vtables log (a unit's 16 `UpdateHidden` and 21
//! `GetModel`, the class's 21 `IsLandBuilding`), zeroed models and logging `Fn23CEE2` and `GETerrainMesh::GetMeshZ`;
//! 24 viewports with 60 units each, most land buildings near the front clipping plane. Each printed per case an
//! FNV-1a 64 of its log; these tests draw the same streams, run each case through `view.rs` and the C++ glue
//! rewritten here and compare the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::pick::screen_ray;
use crate::view::*;

const EFFECTS: &str = include_str!("golden_effects.txt");
const OPACITY: &str = include_str!("golden_opacity.txt");

fn transform(g: &mut Rng) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    let y = g.f(-3.2, 3.2);
    let p = g.f(-1.5, 1.5);
    let r = if g.b(20) { g.f(-0.4, 0.4) } else { 0.0 };
    t.set_orientation(RotationOrder::Ypr, y, p, r);
    if g.b(10) {
        let q = if g.b(50) { 1.5707964 } else { -1.5707964 };
        t.set_orientation(RotationOrder::Ypr, 0.0, q, 0.0);
    }
    let x = g.f(-4.0, 74.0);
    let yy = g.f(-4.0, 74.0);
    let z = g.f(-2.0, 30.0);
    t.m[0][3] = x;
    t.m[1][3] = yy;
    t.m[2][3] = z;
    t
}

fn viewport(g: &mut Rng, log: &mut Log, vp: &mut GeViewport) {
    let w = g.pick(&[640.0, 800.0, 1024.0, 1920.0]);
    let h = g.pick(&[480.0, 600.0, 768.0, 1080.0]);
    let top = if g.b(30) { g.f(0.0, 60.0) } else { 0.0 };
    vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top });
    let fov = g.f(0.3, 1.2);
    vp.set_field_of_view(fov);
    let front = g.f(0.1, 3.0);
    vp.set_front_clipping_distance(front);
    let rear = g.f(30.0, 90.0);
    vp.set_rear_clipping_distance(rear);
    let ow = g.f(10.0, 120.0);
    let oh = g.f(10.0, 90.0);
    vp.set_orthographic_rectangle(&GeRectangle { bottom: oh, left: -ow, right: ow, top: -oh });
    let camera = transform(g);
    vp.set_camera(&camera);
    log.t("vp");
    log.i(w as i64);
    log.i(h as i64);
    log.h(top);
}

/// A point `depth` of the way along a screen point's ray.
fn on_ray(vp: &GeViewport, ortho: bool, sx: f32, sy: f32, depth: f32) -> Ge3DPoint {
    let r = screen_ray(vp, ortho, Ge2DPoint { x: sx, y: sy });
    let (a, b) = (r.front, r.rear);
    Ge3DPoint::new((b.x - a.x) * depth + a.x, (b.y - a.y) * depth + a.y, (b.z - a.z) * depth + a.z)
}

/// `UMFloor`.
fn floor(v: f32) -> i32 {
    ee_ge::um::floor(v)
}

#[test]
fn effects_match_the_cpp_golden() {
    let mut golden = EFFECTS.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let size = 70u32;
    // the map's tiles: None missing, Some(seen)
    let mut tiles = vec![];
    for _ in 0..size * size {
        if g.b(10) {
            tiles.push(None);
        } else {
            tiles.push(Some(g.b(70)));
        }
    }
    let mut n = 0i64;
    for _ in 0..24 {
        viewport(&mut g, &mut log, &mut vp);
        check(&mut golden, "viewport", n, &mut log);
        n += 1;
        for _ in 0..100 {
            let three = g.b(50);
            let ortho = g.b(20);
            let free = g.b(15);
            let base = if g.b(5) { 10 } else { 0 };
            let visible = g.b(70);
            let p = if g.b(70) {
                let sx = g.f(-200.0, 2100.0);
                let sy = g.f(-200.0, 1300.0);
                let depth = g.f(-0.05, 1.05);
                on_ray(&vp, ortho, sx, sy, depth)
            } else {
                let x = g.f(-5.0, 75.0);
                let y = g.f(-5.0, 75.0);
                let z = g.f(-3.0, 30.0);
                Ge3DPoint::new(x, y, z)
            };
            let radius = if g.b(10) {
                0.0
            } else if g.b(70) {
                g.f(0.0, 3.0)
            } else {
                g.f(0.0, 40.0)
            };
            log.t("in");
            log.i(three as i64);
            log.i(ortho as i64);
            log.i(free as i64);
            log.h(p.x);
            log.h(p.y);
            log.h(p.z);
            log.h(radius);
            // Queue's glue: the base's answer, the tile, its visibility, then the sphere
            log.t("base");
            let result = (|| {
                if base != 0 {
                    return base;
                }
                let (x, y) = (floor(p.x) as u32, floor(p.y) as u32);
                let tile = if x < size && y < size { tiles[(y * size + x) as usize] } else { None };
                let Some(seen) = tile else { return 10 };
                if three {
                    log.t("V21");
                    log.i(seen as i64);
                    if !visible && !free {
                        return 10;
                    }
                } else {
                    log.t("seen 1");
                    if !seen && !free {
                        return 10;
                    }
                }
                if effect_in_view(&vp, ortho, p, radius) {
                    0
                } else {
                    10
                }
            })();
            log.t("out");
            log.i(result);
            check(&mut golden, "queue", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
}

#[test]
fn opacity_matches_the_cpp_golden() {
    let mut golden = OPACITY.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let mut centers = [Ge3DPoint::default(); 3];
    let mut radii = [[0.0f32; 3]; 3];
    let mut n = 0i64;
    for _ in 0..24 {
        viewport(&mut g, &mut log, &mut vp);
        check(&mut golden, "viewport", n, &mut log);
        n += 1;
        for _ in 0..60 {
            let complex = g.b(60);
            let mode = g.r() % 3;
            let obstacle = g.b(40);
            let flags = g.r() % 256;
            let hidden = g.b(50);
            let land = g.b(75);
            let cinematic = g.b(10);
            let ortho = g.b(15);
            let fn_translucent = g.b(30);
            let fn_opacity = if g.b(50) { 1.0 } else { g.f(0.0, 1.0) };
            let anim = (g.r() % 3) as usize;
            let m = if complex { anim } else { 0 };
            let cx = g.f(-1.0, 1.0);
            let cy = g.f(-1.0, 1.0);
            let cz = g.f(0.0, 3.0);
            centers[m] = Ge3DPoint::new(cx, cy, cz);
            let frame = (g.r() % 3) as usize;
            for r in radii[m].iter_mut() {
                *r = g.f(0.0, 6.0);
            }
            let s = if g.b(30) { 1.0 } else { g.f(0.3, 3.0) };
            let sx = g.f(-100.0, 2000.0);
            let sy = g.f(-100.0, 1200.0);
            let radius = radii[m][if complex { frame } else { 0 }] * s;
            let want = g.f(-0.3, 0.8);
            let depth = if g.b(70) {
                (radius + want) / (vp.rear_clipping_distance - vp.front_clipping_distance)
            } else {
                g.f(0.0, 1.0)
            };
            let p = on_ray(&vp, false, sx, sy, depth);
            let mut location = GeTransformation::IDENTITY;
            let (tile_x, tile_y);
            if complex {
                location = transform(&mut g);
                location.m[0][3] = p.x;
                location.m[1][3] = p.y;
                location.m[2][3] = p.z;
                tile_x = 0;
                tile_y = 0;
            } else {
                tile_x = p.x.floor() as i32;
                tile_y = p.y.floor() as i32;
            }
            log.t("in");
            log.i(complex as i64);
            log.i(mode as i64);
            log.i(obstacle as i64);
            log.i(flags as i64);
            log.i(cinematic as i64);
            log.i(ortho as i64);
            log.h(p.x);
            log.h(p.y);
            log.h(p.z);
            log.h(s);
            // GetUnitOpacity's glue
            let mut translucent = false;
            let mut opacity = 1.0f32;
            if mode == 1 {
                if obstacle || (flags & 0x10) == 0x10 {
                    log.t("U16");
                    if hidden {
                        translucent = true;
                        opacity = 0.35;
                    }
                }
            } else if mode == 2 && obstacle {
                translucent = true;
                opacity = 0.35;
            }
            if complex {
                log.t("fn");
                log.i(translucent as i64);
                log.h(opacity);
                if fn_opacity < opacity {
                    opacity = fn_opacity;
                }
                translucent = translucent || fn_translucent;
            }
            log.t("C21");
            if land && !cinematic && !ortho {
                let (center, r, scale);
                if complex {
                    log.t("U21");
                    center = location.apply_point(centers[anim]);
                    r = radii[anim][frame];
                    scale = s;
                } else {
                    let mut t = GeTransformation::IDENTITY;
                    t.m[0][3] = tile_x as f32 + 0.5;
                    t.m[1][3] = tile_y as f32 + 0.5;
                    log.t("z");
                    log.h(t.m[0][3]);
                    log.h(t.m[1][3]);
                    t.m[2][3] = t.m[0][3] * 0.125 - t.m[1][3] * 0.0625 + 1.0;
                    log.t("U21");
                    center = t.apply_point(centers[0]);
                    r = radii[0][0];
                    scale = s;
                }
                (translucent, opacity) = near_fade(&vp, center, r, scale, translucent, opacity);
            }
            log.t("out");
            log.i(translucent as i64);
            log.h(opacity);
            check(&mut golden, "opacity", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
}
