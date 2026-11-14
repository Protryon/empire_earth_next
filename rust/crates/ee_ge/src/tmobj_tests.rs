//! `tmobj.rs` through a mock host: the calls the mesh makes on other objects, in order, and its state through the
//! forwarders the C++ calls. The golden (scratch `build/ge11/golden`, PORT_LEARNINGS "R4.3, eleventh part") compares
//! the whole class with the original's unit; these pin each method's own rules.

use std::cell::RefCell;
use std::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

use super::*;
use crate::io::{BufferIo, FieldIo};
use crate::types::{GeRectangle, GeTransformation};

/// Held by the tests that move the shared water clock (`WATER_LAST_TIME`).
pub(super) static WATER_CLOCK: Mutex<()> = Mutex::new(());

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static CLOCK: RefCell<c_ulong> = const { RefCell::new(5000) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take_log() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

/// Texture sets in the mock are `0x1000 + 0x100 * w + 0x10 * h + coverage`; textures are small integers.
unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeTmOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    match op {
        EeGeTmOp::Time => CLOCK.with(|c| *c.borrow()) as isize,
        EeGeTmOp::FractalNew => {
            log(format!("fractal new {a}"));
            0x77
        }
        EeGeTmOp::FractalDelete => {
            log(format!("fractal delete {o:x}"));
            0
        }
        EeGeTmOp::FractalValue => {
            // SAFETY: the operation's contract
            unsafe { *(a as *mut f32) = 0.5 };
            log(format!("fractal value {} {}", b >> 16, b & 0xffff));
            0
        }
        EeGeTmOp::FractalCompute => {
            log(format!("fractal compute {a}"));
            0
        }
        // every vector quantizes to the table's first entry but up, which is the third
        EeGeTmOp::Quantize => {
            // SAFETY: the operation's contract
            let v = unsafe { *(a as *const GeVector) };
            if v.x == 0.0 && v.y == 0.0 && v.z == 1.0 {
                QUANTIZED.with(|q| q.as_ptr().wrapping_add(2) as isize)
            } else {
                QUANTIZED.with(|q| q.as_ptr() as isize)
            }
        }
        EeGeTmOp::SetDelete => {
            log(format!("set delete {o:x}"));
            0
        }
        EeGeTmOp::SetWidth => ((o >> 8) & 0xf) as isize,
        EeGeTmOp::SetHeight => ((o >> 4) & 0xf) as isize,
        EeGeTmOp::SetCoverage => (o & 0xf) as isize,
        EeGeTmOp::SetTexture => 100 + a * 10 + b,
        EeGeTmOp::SetTextureAt => 200 + a * 10 + b,
        EeGeTmOp::TexReference => {
            log(format!("ref {o}"));
            0
        }
        EeGeTmOp::TexDereference => {
            log(format!("deref {o}"));
            0
        }
    }
}

thread_local! {
    static QUANTIZED: [GeVector; 3] = const {
        [
            GeVector { x: 1.0, y: 0.0, z: 0.0 },
            GeVector { x: 0.0, y: 1.0, z: 0.0 },
            GeVector { x: 0.0, y: 0.0, z: 1.0 },
        ]
    };
}

fn obj<'a>(p: *mut EeGeTerrainMesh) -> &'a mut EeGeTerrainMesh {
    // SAFETY: the tests' meshes are live while they read them
    unsafe { &mut *p }
}

fn mesh(w: c_long, h: c_long) -> *mut EeGeTerrainMesh {
    // SAFETY: the mock serves every operation
    unsafe {
        let p = ee_ge_tm_new(host, std::ptr::null_mut());
        ee_ge_tm_create_flat_mesh(p, w, h);
        take_log();
        p
    }
}

fn valid_points(p: *mut EeGeTerrainMesh, side: c_ulong) -> Vec<(c_ulong, c_ulong)> {
    let mut out = Vec::new();
    for y in 0..side {
        for x in 0..side {
            // SAFETY: `p` is live
            if unsafe { ee_ge_tm_is_valid(p, x, y, false) } {
                out.push((x, y));
            }
        }
    }
    out
}

#[test]
fn creation_reads_the_clock_and_seeds_the_fractal_and_destruction_releases_in_order() {
    // SAFETY: the mock serves every operation
    unsafe {
        let p = ee_ge_tm_new(host, std::ptr::null_mut());
        assert_eq!(take_log(), vec!["fractal new 5000"]);
        assert_eq!(obj(p).water_animation_period, 80);
        assert_eq!(obj(p).water_texture_scale, -0.71428573);
        assert_eq!(ee_ge_tm_get(p, EeGeTmVal::SpaceTileCount), 0);
        assert_eq!(ee_ge_tm_get_f(p, EeGeTmVal::MinimumZ), f32::MAX);
        assert_eq!(ee_ge_tm_get_f(p, EeGeTmVal::MaximumZ), -f32::MAX);
        // the default planes: the origin, the normal up
        assert_eq!((*ee_ge_tm_plane(p, 2)).normal.z, 1.0);
        assert_eq!(*ee_ge_tm_water_color(p), 1.0);
        assert_eq!(ee_ge_tm_register_texture_set(p, 0x1221 as *mut c_void), 0);
        assert_eq!(ee_ge_tm_register_texture_set(p, std::ptr::null_mut()), 1);
        assert_eq!(ee_ge_tm_texture_set(p, 0) as usize, 0x1221);
        assert!(ee_ge_tm_texture_set(p, 9).is_null());
        ee_ge_tm_destroy(p);
        assert_eq!(take_log(), vec!["set delete 1221", "set delete 0", "fractal delete 77"]);
    }
}

#[test]
fn the_flat_mesh_is_a_diamond_of_up_normals_at_height_zero() {
    let p = mesh(3, 2);
    // SAFETY: `p` is live
    unsafe {
        assert_eq!(ee_ge_tm_get(p, EeGeTmVal::TileRowCount), 5);
        let points = valid_points(p, 8);
        let expected: c_ulong = terrain_mesh::flat_mesh(3, 2).point_rows.iter().map(|&(s, e)| e - s + 1).sum();
        assert_eq!(points.len() as c_ulong, expected);
        for &(x, y) in &points {
            let pt = &*ee_ge_tm_point(p, x, y);
            assert_eq!(pt.z, 0.0);
            assert_eq!((*pt.normal).z, 1.0);
        }
        assert_eq!(ee_ge_tm_get_f(p, EeGeTmVal::MinimumZ), 0.0);
        // a point's height widens the range; the edges are the diamond's
        let (x, y) = points[3];
        ee_ge_tm_point_edit(p, x, y, 0, 0, -2.5);
        ee_ge_tm_point_edit(p, x, y, 0, 0, 4.0);
        assert_eq!(ee_ge_tm_get_f(p, EeGeTmVal::MinimumZ), -2.5);
        assert_eq!(ee_ge_tm_get_f(p, EeGeTmVal::MaximumZ), 4.0);
        assert_eq!((*ee_ge_tm_point(p, x, y)).z, 4.0);
        assert!(ee_ge_tm_is_edge_tile(p, 1, 0, 1)); // x + y == height - 1
        assert!(!ee_ge_tm_is_edge_tile(p, 1, 1, 0));
        ee_ge_tm_destroy(p);
    }
}

#[test]
fn tile_edits_follow_the_header() {
    let p = mesh(4, 4);
    // SAFETY: `p` is live; (4, 3) is a tile
    unsafe {
        assert!(ee_ge_tm_is_valid(p, 4, 3, true));
        let t = || *ee_ge_tm_tile(p, 4, 3);
        ee_ge_tm_tile_edit(p, 4, 3, 0, 0);
        assert_eq!(t().reference_count, 2, "a first reference counts two");
        ee_ge_tm_tile_edit(p, 4, 3, 0, 0);
        assert_eq!(t().reference_count, 3);
        ee_ge_tm_tile_edit(p, 4, 3, 1, 0);
        assert_eq!(t().reference_count, 2);
        ee_ge_tm_tile_edit(p, 4, 3, 2, 0x10007);
        assert_eq!(t().reference_count, 7, "the count is truncated to 16 bits");
        ee_ge_tm_tile_edit(p, 4, 3, 3, 0x105);
        assert_eq!(t().texture_set_id, 5);
        ee_ge_tm_tile_edit(p, 4, 3, 5, 1);
        assert_eq!(t().water_info, 1);
        ee_ge_tm_tile_edit(p, 4, 3, 6, 1);
        assert_eq!(ee_ge_tm_get(p, EeGeTmVal::SpaceTileCount), 1);
        assert!(ee_ge_tm_is_space_tile(p, 4, 3));
        assert!(!ee_ge_tm_is_space_tile(p, 400, 3), "off the map");
        // a space map ignores SetIsWater; SetWaterInfo keeps the space bit
        ee_ge_tm_tile_edit(p, 4, 3, 5, 0);
        assert_eq!(t().water_info & 0x80, 0x80);
        ee_ge_tm_tile_edit(p, 4, 3, 4, 0x1e);
        assert_eq!(t().water_info, 0x9e);
        ee_ge_tm_tile_edit(p, 4, 3, 6, 0);
        assert_eq!(ee_ge_tm_get(p, EeGeTmVal::SpaceTileCount), 0);
        assert_eq!(t().water_info & 0x80, 0);
        ee_ge_tm_destroy(p);
    }
}

#[test]
fn set_is_space_on_row_zero_recomputes_no_shoreline() {
    let p = mesh(3, 3);
    // SAFETY: `p` is live
    unsafe {
        let row0 = obj(p).tile_rows[0];
        let x = row0.start;
        // a neighbour's water info that a shoreline pass would rewrite
        ee_ge_tm_tile_edit(p, x, 1, 4, 0x1e);
        ee_ge_tm_tile_edit(p, x, 0, 6, 1);
        assert_eq!((*ee_ge_tm_tile(p, x, 1)).water_info, 0x1e, "the unsigned loop from y - 1 runs no row");
        ee_ge_tm_destroy(p);
    }
}

#[test]
fn colour_sets_pick_by_the_fractal_and_the_water_by_depth() {
    let p = mesh(2, 2);
    // SAFETY: `p` is live
    unsafe {
        let colors: [c_ulong; 5] = [10, 11, 12, 13, 14];
        assert_eq!(ee_ge_tm_register_color_set(p, colors.as_ptr(), 5), 0);
        let (x, y) = valid_points(p, 6)[2];
        ee_ge_tm_point_edit(p, x, y, 2, 0, 0.0);
        assert_eq!(take_log(), vec![format!("fractal value {} {}", x & 63, y & 63)]);
        // 0.5 * (5 - 1) = 2
        assert_eq!((*ee_ge_tm_point(p, x, y)).color_table_index, 12);
        ee_ge_tm_point_edit(p, x, y, 1, 200, 0.0);
        assert_eq!((*ee_ge_tm_point(p, x, y)).color_table_index, 200);
        // the colour table: up to 256 entries, the rest white
        let table = [[0.25f32, 0.5, 0.75]; 3];
        ee_ge_tm_set_color_table(p, table.as_ptr(), 3);
        ee_ge_tm_point_edit(p, x, y, 1, 2, 0.0);
        assert_eq!(*ee_ge_tm_vertex_color(p, x, y).add(2), 0.75);
        ee_ge_tm_point_edit(p, x, y, 1, 3, 0.0);
        assert_eq!(*ee_ge_tm_vertex_color(p, x, y), 1.0);
        // the water colour: depth -1 at scale 1/(-1/2)... the default scale is -1/1.4
        ee_ge_tm_point_edit(p, x, y, 0, 0, -1.4);
        ee_ge_tm_point_edit(p, x, y, 3, 0, 0.0);
        let index = terrain_mesh::water_color_index(-0.71428573, -1.4, 5);
        assert_eq!((*ee_ge_tm_point(p, x, y)).color_table_index as c_ulong, colors[index as usize]);
        ee_ge_tm_destroy(p);
    }
}

#[test]
fn the_water_state_reads_the_set_and_recolours_underwater_points() {
    let p = mesh(3, 3);
    // SAFETY: `p` is live
    unsafe {
        let colors: [c_ulong; 3] = [7, 8, 9];
        ee_ge_tm_register_color_set(p, colors.as_ptr(), 3);
        ee_ge_tm_register_texture_set(p, 0x1231 as *mut c_void);
        let (x, y) = valid_points(p, 8)[4];
        ee_ge_tm_point_edit(p, x, y, 0, 0, -0.5);
        ee_ge_tm_set_global_water_state(p, 0, 0, 40, 2.0);
        assert_eq!(obj(p).water_animation, vec![100, 101, 102, 110, 111, 112]);
        assert_eq!(obj(p).water_texture_scale, 0.5);
        let index = terrain_mesh::water_color_index(0.5, -0.5, 3);
        assert_eq!((*ee_ge_tm_point(p, x, y)).color_table_index as c_ulong, colors[index as usize]);
        let grid = terrain_mesh::water_texture_indices(6);
        assert_eq!(obj(p).a.water_texture_indices[5][7], grid[5][7]);
        ee_ge_tm_destroy(p);
    }
}

fn viewport() -> GeViewport {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(crate::transform::RotationOrder::Ypr, 0.6, -0.9, 0.0);
    let dir = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    t.m[0][3] = 4.0 - dir.x * 12.0;
    t.m[1][3] = 4.0 - dir.y * 12.0;
    t.m[2][3] = -dir.z * 12.0;
    let view = GeRectangle { bottom: 768.0, left: 0.0, right: 1024.0, top: 0.0 };
    let ortho = GeRectangle { bottom: 100.0, left: -100.0, right: 100.0, top: -100.0 };
    GeViewport::new(view, ortho, t, 0.5, 60.0, 0.9)
}

#[test]
fn a_frame_references_its_textures_and_the_next_releases_them() {
    let p = mesh(6, 6);
    let v = viewport();
    // SAFETY: `p` is live
    unsafe {
        ee_ge_tm_register_texture_set(p, 0x1221 as *mut c_void);
        for &(x, y) in valid_points(p, 14).iter().step_by(3) {
            if ee_ge_tm_is_valid(p, x, y, true) {
                ee_ge_tm_tile_edit(p, x, y, 0, 0);
            }
        }
        ee_ge_tm_update_visible_terrain(p, &v, false);
        let refs: Vec<String> = take_log().into_iter().filter(|l| l.starts_with("ref")).collect();
        let materials = ee_ge_tm_get(p, EeGeTmVal::TerrainMaterialCount);
        assert!(materials > 0 && ee_ge_tm_get(p, EeGeTmVal::NumVisibleTiles) > 0);
        assert_eq!(
            refs.len() as c_ulong,
            obj(p).terrain_materials.iter().filter(|m| m.texture != 0).count() as c_ulong
        );
        // every material's two index lists hold its polygon count plus two NULLs
        for k in 0..materials {
            let mut view = std::mem::zeroed::<EeGeTmMaterialView>();
            ee_ge_tm_material(p, false, k, &mut view);
            assert_eq!(view.indices_length, view.polygon_count + 2);
            assert_eq!(view.strips_length, view.polygon_count + 2);
            assert!((0..view.strips_length).all(|i| (*view.strips.add(i)).is_null()));
        }
        // the map: one entry per texture, in address order, at its first index
        let mut textures = vec![std::ptr::null_mut(); materials as usize];
        let mut indices = vec![0; materials as usize];
        let n = ee_ge_tm_material_map(p, textures.as_mut_ptr(), indices.as_mut_ptr(), materials as usize);
        assert!(n >= 1 && textures[..n].windows(2).all(|w| (w[0] as usize) < (w[1] as usize)));
        // skirt vertices start black, visible and clipped everywhere
        for i in 0..ee_ge_tm_get(p, EeGeTmVal::EdgeVertexCount) as usize {
            let e = &**ee_ge_tm_edge_vertices(p, false).add(i);
            assert_eq!((e.color, e.visibility, e.clip_flags), ([0.0; 3], 1.0, 0x3f));
        }
        // the fog of war writes the visible vertices' visibility
        ee_ge_tm_update_fog_of_war(p, true);
        assert!(obj(p).a.terrain_vertices.iter().flatten().any(|v| v.visibility == 1.0));
        // the next frame releases each texture of the map once (not the untextured material's NULL)
        let n = textures[..n].iter().filter(|t| !t.is_null()).count();
        ee_ge_tm_update_visible_terrain(p, &v, false);
        let derefs = take_log().into_iter().filter(|l| l.starts_with("deref")).count();
        assert_eq!(derefs, n);
        ee_ge_tm_destroy(p);
        let last = take_log();
        assert_eq!(last.iter().filter(|l| l.starts_with("deref")).count(), n);
    }
}

#[test]
fn the_water_animation_steps_by_whole_periods() {
    let _lock = WATER_CLOCK.lock().unwrap_or_else(|e| e.into_inner());
    let p = mesh(2, 2);
    // SAFETY: `p` is live
    unsafe {
        ee_ge_tm_register_texture_set(p, 0x1211 as *mut c_void);
        ee_ge_tm_set_global_water_state(p, 0, 0, 100, 1.0);
        obj(p).water_materials = vec![Material::of(0), Material::of(0)];
        WATER_LAST_TIME.store(1000, Ordering::Relaxed);
        CLOCK.with(|c| *c.borrow_mut() = 1099);
        ee_ge_tm_update_water_animation(p);
        assert_eq!(obj(p).water_animation_frame, 0, "not a whole period yet");
        CLOCK.with(|c| *c.borrow_mut() = 1350);
        ee_ge_tm_update_water_animation(p);
        assert_eq!(obj(p).water_animation_frame, 1, "three periods over two frames");
        assert_eq!(WATER_LAST_TIME.load(Ordering::Relaxed), 1300);
        // each material shows frame (frame + i) % 2 of the set's second column
        assert_eq!(obj(p).water_materials[0].texture, 111);
        assert_eq!(obj(p).water_animation[1], 101);
        CLOCK.with(|c| *c.borrow_mut() = 5000);
        ee_ge_tm_destroy(p);
    }
}

// a field stream table over a BufferIo
unsafe extern "C" fn r8(f: *mut c_void, o: *mut u8) -> c_long {
    // SAFETY: the table's contract
    unsafe { (*(f as *mut BufferIo)).read_u8(&mut *o) }
}
unsafe extern "C" fn r16(f: *mut c_void, o: *mut u16) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_u16(&mut *o) }
}
unsafe extern "C" fn r32(f: *mut c_void, o: *mut u32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_u32(&mut *o) }
}
unsafe extern "C" fn rf(f: *mut c_void, o: *mut f32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_f32(&mut *o) }
}
unsafe extern "C" fn w8(f: *mut c_void, v: u8) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u8(v) }
}
unsafe extern "C" fn w16(f: *mut c_void, v: u16) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u16(v) }
}
unsafe extern "C" fn w32(f: *mut c_void, v: u32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u32(v) }
}
unsafe extern "C" fn wf(f: *mut c_void, v: f32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_f32(v) }
}
unsafe extern "C" fn rb(_: *mut c_void, _: *mut u8, _: u32, _: *mut u32) -> c_long {
    9
}
unsafe extern "C" fn wb(_: *mut c_void, _: *const u8, _: u32, _: *mut u32) -> c_long {
    9
}
unsafe extern "C" fn gp(_: *mut c_void, _: *mut c_long) -> c_long {
    9
}
unsafe extern "C" fn sp(_: *mut c_void, _: c_long) -> c_long {
    9
}

fn table(io: &mut BufferIo) -> EeGeFieldIo {
    EeGeFieldIo {
        file: io as *mut BufferIo as *mut c_void,
        read_u8: r8,
        read_u16: r16,
        read_u32: r32,
        read_f32: rf,
        write_u8: w8,
        write_u16: w16,
        write_u32: w32,
        write_f32: wf,
        read_bytes: rb,
        write_bytes: wb,
        get_position: gp,
        set_position: sp,
    }
}

#[test]
fn a_saved_mesh_loads_back_with_its_heights_tiles_and_normals() {
    let p = mesh(4, 3);
    // SAFETY: `p` and `q` are live
    unsafe {
        ee_ge_tm_register_texture_set(p, 0x1111 as *mut c_void);
        let points = valid_points(p, 10);
        for (k, &(x, y)) in points.iter().enumerate() {
            ee_ge_tm_point_edit(p, x, y, 0, 0, k as f32 * 0.25 - 1.0);
            ee_ge_tm_point_edit(p, x, y, 1, k as c_ulong, 0.0);
        }
        let (tx, ty) = *points.iter().find(|&&(x, y)| ee_ge_tm_is_valid(p, x, y, true)).unwrap();
        ee_ge_tm_tile_edit(p, tx, ty, 4, 0x1e);
        let mut out = BufferIo::writer();
        assert_eq!(ee_ge_tm_save(p, &table(&mut out)), 0);
        let q = mesh(1, 1);
        ee_ge_tm_register_texture_set(q, 0x1111 as *mut c_void);
        let mut input = BufferIo::reader(out.bytes.clone());
        assert_eq!(ee_ge_tm_load(q, &table(&mut input)), 0);
        assert_eq!(take_log(), vec![format!("fractal compute {}", obj(p).fractal_seed)]);
        assert_eq!(valid_points(q, 10), points);
        for &(x, y) in &points {
            assert_eq!((*ee_ge_tm_point(q, x, y)).z, (*ee_ge_tm_point(p, x, y)).z);
            assert_eq!((*ee_ge_tm_point(q, x, y)).color_table_index, (*ee_ge_tm_point(p, x, y)).color_table_index);
            // every normal recomputed (the mock quantizes a tilted one to the first entry)
            assert!(!(*ee_ge_tm_point(q, x, y)).normal.is_null());
        }
        assert_eq!((*ee_ge_tm_tile(q, tx, ty)).water_info, 0x1e);
        assert_eq!(ee_ge_tm_get_f(q, EeGeTmVal::MinimumZ), -1.0);
        // a truncated file fails and keeps what it read
        let mut short = BufferIo::reader(out.bytes[..20].to_vec());
        assert_eq!(ee_ge_tm_load(q, &table(&mut short)), crate::io::END_OF_FILE);
        ee_ge_tm_destroy(p);
        ee_ge_tm_destroy(q);
        take_log();
    }
}

#[test]
fn a_loaded_tile_naming_an_unregistered_set_reads_zero() {
    let p = mesh(3, 3);
    // SAFETY: `p` and `q` are live
    unsafe {
        ee_ge_tm_register_texture_set(p, 0x1111 as *mut c_void);
        ee_ge_tm_register_texture_set(p, std::ptr::null_mut());
        ee_ge_tm_register_texture_set(p, 0x1221 as *mut c_void);
        let tiles: Vec<(c_ulong, c_ulong)> =
            valid_points(p, 8).into_iter().filter(|&(x, y)| ee_ge_tm_is_valid(p, x, y, true)).collect();
        ee_ge_tm_tile_edit(p, tiles[0].0, tiles[0].1, 3, 1);
        ee_ge_tm_tile_edit(p, tiles[1].0, tiles[1].1, 3, 2);
        let mut out = BufferIo::writer();
        assert_eq!(ee_ge_tm_save(p, &table(&mut out)), 0);
        let q = mesh(1, 1);
        ee_ge_tm_register_texture_set(q, 0x1111 as *mut c_void);
        ee_ge_tm_register_texture_set(q, std::ptr::null_mut());
        ee_ge_tm_register_texture_set(q, 0x1221 as *mut c_void);
        let mut input = BufferIo::reader(out.bytes);
        assert_eq!(ee_ge_tm_load(q, &table(&mut input)), 0);
        assert_eq!((*ee_ge_tm_tile(q, tiles[0].0, tiles[0].1)).texture_set_id, 0, "the NULL slot is not registered");
        assert_eq!((*ee_ge_tm_tile(q, tiles[1].0, tiles[1].1)).texture_set_id, 2);
        ee_ge_tm_destroy(p);
        ee_ge_tm_destroy(q);
        take_log();
    }
}
