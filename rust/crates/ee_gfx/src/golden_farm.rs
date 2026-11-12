//! `EEE3DFarm`'s ground and crop meshes against the unmodified C++ (`golden_farm.txt`). A scratch driver
//! (`build/gx12_golden`, `driver_farm.cpp`; PORT_LEARNINGS "R4.4, sixth round") #included `EEE3DFarm.cpp` of the
//! commit before this work over the real GE math, with storing fake models (`fakemesh.h`), a terrain mesh whose
//! point rows are the driver's (heights and normals a salted function of x and y) and a logging `GetMeshZ`. From an
//! LCG seeded 1 it drew 400 farms (size 1 to 6 at a random tile, 0 to 5 crop rows, a random crop height), ran
//! `BuildGroundModels`, `BuildGroundModel` and `BuildCrops`, and printed per build an FNV-1a 64 of the whole log
//! and one of the fresh model's final vertices and triangles and the place (`gmesh`, `cmesh`: the lines kept
//! here). This test draws the same stream, builds the meshes with `farm.rs` and compares.

use ee_ge::types::{Ge3DPoint, GeVector};

use crate::farm::{crops, ground, EeGfxFarmField, FarmTerrain};
use crate::golden_mouse::{check, Log, Rng};
use crate::mesh::{
    EeGfxMeshTriangle, EeGfxMeshVertex, Mesh, EE_GFX_MESH_COLOR, EE_GFX_MESH_COORD, EE_GFX_MESH_NORMAL,
    EE_GFX_MESH_OPACITY, EE_GFX_MESH_POINT,
};

const GOLDEN: &str = include_str!("golden_farm.txt");

struct Terrain {
    salt: u32,
}

impl Terrain {
    fn tz(&self, x: u32, y: u32) -> f32 {
        (x.wrapping_mul(7).wrapping_add(y.wrapping_mul(13)).wrapping_add(self.salt) % 29) as f32 * 0.0625
            + x as f32 * 0.03125
    }
}

impl FarmTerrain for Terrain {
    fn z(&mut self, x: u32, y: u32) -> f32 {
        self.tz(x, y)
    }
    fn normal(&mut self, x: u32, y: u32) -> GeVector {
        let a = (x.wrapping_mul(3).wrapping_add(y.wrapping_mul(5)).wrapping_add(self.salt) % 17) as f32 * 0.03125;
        GeVector { x: a, y: 0.25 - a, z: 0.875 }
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        x * 0.125 - y * 0.0625 + self.tz(x as u32, y as u32) * 0.5
    }
}

/// `fakemesh.h`'s dump of a model's vertices and triangles, and the place.
pub(crate) fn dump_mesh(log: &mut Log, v: &[EeGfxMeshVertex], t: &[EeGfxMeshTriangle], place: Ge3DPoint) {
    for (i, x) in v.iter().enumerate() {
        log.t("v");
        log.i(i as i64);
        let s = x.set;
        let flags = (s & EE_GFX_MESH_POINT != 0) as i64 * 16
            + (s & EE_GFX_MESH_NORMAL != 0) as i64 * 8
            + (s & EE_GFX_MESH_COORD != 0) as i64 * 4
            + (s & EE_GFX_MESH_COLOR != 0) as i64 * 2
            + (s & EE_GFX_MESH_OPACITY != 0) as i64;
        log.i(flags);
        for f in [
            x.point.x,
            x.point.y,
            x.point.z,
            x.normal.x,
            x.normal.y,
            x.normal.z,
            x.coord.u,
            x.coord.v,
            x.color.red,
            x.color.green,
            x.color.blue,
            x.opacity,
        ] {
            log.h(f);
        }
    }
    log.t("polys");
    for x in t {
        for k in x.v {
            log.i(if x.set != 0 { k as i64 } else { 0xdead });
        }
    }
    log.t("place");
    log.h(place.x);
    log.h(place.y);
    log.h(place.z);
}

#[test]
fn the_farm_meshes_match_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut n = 0;
    let mut with_crops = 0;
    for _ in 0..400 {
        let salt = g.r();
        let size = 1 + (g.r() % 6) as i32;
        let tile_x = (g.r() % 40) as i32;
        let tile_y = (g.r() % 40) as i32;
        let rows = g.r() % 6;
        let height = g.f(0.05, 1.5);
        let _drop_sites = g.b(60);
        let _drop_x = g.r();
        let _drop_y = g.r();
        let _screen = g.r();
        let (x, y, s) = (tile_x as f32, tile_y as f32, size as f32);
        let field = EeGfxFarmField {
            left: x,
            top: y,
            right: s + x - 1.0,
            bottom: s + y - 1.0,
            row_length: size + 1,
            tile_size: s,
        };
        let mut terrain = Terrain { salt };
        let rl = (size + 1) as usize;
        let mut v = vec![EeGfxMeshVertex::default(); rl * rl];
        let mut t = vec![EeGfxMeshTriangle::default(); (size * size * 2) as usize];
        let mut mesh = Mesh::new(&mut v, &mut t);
        let place = ground(&field, &mut terrain, &mut mesh);
        assert_eq!(mesh.dropped, 0);
        dump_mesh(&mut log, &v, &t, place);
        check(&mut golden, "gmesh", n, &mut log);
        n += 1;
        let r = rows as usize;
        let mut v = vec![EeGfxMeshVertex::default(); size as usize * rl * r * 2];
        let mut t = vec![EeGfxMeshTriangle::default(); (size * size) as usize * r * 2];
        let mut mesh = Mesh::new(&mut v, &mut t);
        let place = crops(&field, rows, height, &mut terrain, &mut mesh);
        assert_eq!(mesh.dropped, 0);
        with_crops += (rows > 0) as u32;
        dump_mesh(&mut log, &v, &t, place);
        check(&mut golden, "cmesh", n, &mut log);
        n += 1;
    }
    assert!(golden.next().is_none());
    assert!(with_crops > 250);
}
