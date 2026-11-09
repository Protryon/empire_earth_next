//! Terrain goldens: the scratch generator's `ScenTerrain` scenarios rebuilt with the same inputs. See `golden_draw.txt`.
// the literal inputs mirror the C++ generator exactly
#![allow(clippy::needless_range_loop, clippy::neg_multiply, clippy::approx_constant)]

use super::*;
use crate::draw::testing::{b, golden_section, impl_draw_host_for_rec, Rec};

const N: usize = VISIBLE_POINTS;

fn blank_vertex() -> RenderVertex {
    RenderVertex {
        point: [0.0; 3],
        color: [0.0; 3],
        visibility: 0.0,
        fog: 0.0,
        clip_flags: 0,
        linked_visibility: core::ptr::null_mut(),
    }
}

fn blank_polygon() -> TerrainPolygon {
    TerrainPolygon { vertices: [core::ptr::null_mut(); 3], alpha: [0.0; 3], uv: [[0.0; 2]; 3] }
}

/// The generator's `ClearMesh`.
struct Mesh {
    tv: Vec<RenderVertex>,
    wv: Vec<RenderVertex>,
    tvis: Vec<bool>,
    wvis: Vec<bool>,
    z: Vec<f32>,
    normals: Vec<[f32; 3]>,
    color_index: Vec<usize>,
    table: Vec<[f32; 3]>,
    water_color: [f32; 3],
    start: (c_ulong, c_ulong),
    size: (c_ulong, c_ulong),
    rows: bool,
    space: bool,
    tile_rows: c_ulong,
    tile_row_count: c_ulong,
}

impl Mesh {
    fn new(startx: c_ulong, starty: c_ulong, w: c_ulong, h: c_ulong, rows: bool) -> Mesh {
        let mut z = vec![0.0f32; 12 * 12];
        let mut normals = vec![[0.0f32; 3]; 12 * 12];
        let mut color_index = vec![0usize; 12 * 12];
        for y in 0..12 {
            for x in 0..12 {
                let mut value = (x as f32 - 4.0) * 0.75 + ((y % 3) as f32) * if x % 2 != 0 { -0.3125 } else { 0.25 };
                if y == 4 && x == 3 {
                    value = -0.5;
                }
                z[y * 12 + x] = value;
                color_index[y * 12 + x] = (x + y) % 4;
                normals[y * 12 + x] = [0.1 * ((x % 3) as f32), 0.2 * ((y % 2) as f32), 0.9];
            }
        }
        let table = (0..4).map(|i| [0.25 + 0.1 * i as f32, 0.5 - 0.05 * i as f32, 0.75]).collect();
        let mut tvis = vec![true; N * N];
        let mut wvis = vec![false; N * N];
        for cx in 0..N {
            for cy in 0..N {
                wvis[cx * N + cy] = (cx + cy) & 1 == 0;
            }
        }
        tvis[0] = true;
        Mesh {
            tv: vec![blank_vertex(); N * N],
            wv: vec![blank_vertex(); N * N],
            tvis,
            wvis,
            z,
            normals,
            color_index,
            table,
            water_color: [0.1, 0.3, 0.9],
            start: (startx, starty),
            size: (w, h),
            rows,
            space: false,
            tile_rows: 11,
            tile_row_count: 11,
        }
    }

    fn view(&mut self) -> TerrainMeshView {
        TerrainMeshView {
            mesh: self as *mut Mesh as Handle,
            terrain_vertices: self.tv.as_mut_ptr(),
            water_vertices: self.wv.as_mut_ptr(),
            terrain_visible: self.tvis.as_ptr(),
            water_visible: self.wvis.as_ptr(),
            visible_tile_rows: if self.rows { 1 } else { 0 },
            start_x: self.start.0,
            start_y: self.start.1,
            width: self.size.0,
            height: self.size.1,
            water_percentage: 0.5,
            is_space: self.space,
            edge_world: core::ptr::null(),
            edge_vertices: core::ptr::null(),
            edge_count: 0,
            cliff_world: core::ptr::null(),
            cliff_vertices: core::ptr::null(),
            cliff_count: 0,
        }
    }

    fn t_at(&mut self, cx: usize, cy: usize) -> *mut RenderVertex {
        unsafe { self.tv.as_mut_ptr().add(cx * N + cy) }
    }
}

struct Host {
    rec: Rec,
    names: Vec<(Handle, &'static str)>,
    /// (op, argument bits, result bits) from the C++ run, replayed in order.
    um: Vec<(&'static str, u32, u32)>,
    um_next: usize,
    lights: [Option<LightData>; 8],
}
impl_draw_host_for_rec!(Host, names);

impl Host {
    fn new() -> Host {
        Host {
            rec: Rec::new(),
            names: vec![(0x1000usize as Handle, "T1"), (0x2000usize as Handle, "T2"), (0x3000usize as Handle, "T3")],
            um: Vec::new(),
            um_next: 0,
            lights: [None; 8],
        }
    }

    fn replay(&mut self, op: &'static str, arg: f32) -> f32 {
        let (want_op, want_arg, res) = self.um[self.um_next];
        self.um_next += 1;
        assert_eq!(op, want_op);
        assert_eq!(arg.to_bits(), want_arg, "um {op} argument");
        self.rec.log.push(format!("um {op} {:08x} {res:08x}", want_arg));
        f32::from_bits(res)
    }
}

impl TerrainHost for Host {
    fn mesh_z(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> f32 {
        let m = unsafe { &*(mesh as *mut Mesh) };
        m.z[(y as usize) * 12 + x as usize]
    }
    fn mesh_normal(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let m = unsafe { &*(mesh as *mut Mesh) };
        m.normals[(y as usize) * 12 + x as usize]
    }
    fn mesh_vertex_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let m = unsafe { &*(mesh as *mut Mesh) };
        m.table[m.color_index[(y as usize) * 12 + x as usize]]
    }
    fn mesh_water_color(&mut self, mesh: Handle, _x: c_ulong, _y: c_ulong) -> [f32; 3] {
        let m = unsafe { &*(mesh as *mut Mesh) };
        m.water_color
    }
    fn mesh_valid_tile(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> bool {
        let m = unsafe { &*(mesh as *mut Mesh) };
        y < m.tile_row_count && x < m.tile_rows
    }
    fn mark_viewport_changed(&mut self) {}
    fn light(&mut self, index: u32) -> Option<LightData> {
        let l = self.lights[index as usize];
        if l.is_some() {
            self.rec.log.push(format!("getlight {index}"));
        }
        l
    }
    fn um_arctan(&mut self, value: f32) -> f32 {
        self.replay("atan", value)
    }
    fn um_tan(&mut self, angle: f32) -> f32 {
        self.replay("tan", angle)
    }
    fn um_sin(&mut self, angle: f32) -> f32 {
        // UMSin is UMCos(pi/2 - x), logged as the cos it calls
        self.replay("cos", 1.570_796_4 - angle)
    }
    fn um_cos(&mut self, angle: f32) -> f32 {
        self.replay("cos", angle)
    }
}

fn m(seed: f32) -> Transform {
    let mut t = [[0.0f32; 4]; 4];
    for (r, row) in t.iter_mut().enumerate() {
        for (c, e) in row.iter_mut().enumerate() {
            *e = seed * (r as f32 + 1.0) + (c as f32 * 3.0) * 0.125;
        }
    }
    t[3] = [0.0, 0.0, 0.0, 1.0];
    t
}

fn plane(p: [f32; 3], n: [f32; 3]) -> Plane {
    Plane { point: p, normal: n }
}

fn viewport() -> ViewportInfo {
    ViewportInfo {
        camera: m(0.7),
        camera_inverse: m(0.3),
        perspective: m(1.1),
        orthographic: m(0.9),
        front_clip: 2.0,
        rear_clip: 300.0,
        half_width: 320.0,
        half_height: 240.0,
        field_of_view: core::f32::consts::FRAC_PI_3,
        y_clip_plane: 20.5,
        ortho_left: -30.0,
        ortho_top: -20.0,
        ortho_right: 30.0,
        ortho_bottom: 20.0,
        left_plane: Plane::default(),
        right_plane: Plane::default(),
        top_plane: Plane::default(),
        bottom_plane: Plane::default(),
    }
}

/// The generator's `FillPolys`.
fn fill_polys(mesh: &mut Mesh, polys: &mut [TerrainPolygon], count: usize) {
    for i in 0..count {
        for j in 0..3usize {
            let rv = mesh.t_at((i + j) % 3, (i + j * 2) % 3);
            let (fi, fj) = (i as f32, j as f32);
            unsafe {
                (*rv).point = [fi * 1.5 + fj * 0.25, fi * 0.75 - fj * 1.25, 3.0 + fi + fj * 0.5];
                (*rv).color = [0.4 + fj * 0.1, 0.5, 0.6 - fj * 0.05];
                (*rv).visibility = if i % 4 == 0 {
                    0.5
                } else if i % 4 == 1 {
                    1.0
                } else {
                    -1.0
                };
                (*rv).linked_visibility = &mut (*mesh.t_at(0, 0)).visibility;
            }
            polys[i].vertices[j] = rv;
            polys[i].alpha[j] = if i % 2 == 0 { 0.25 * fj } else { -1.0 };
            polys[i].uv[j] = [fi * 0.1 + fj * 0.2, fi * 0.3 - fj * 0.05];
        }
    }
    unsafe { (*mesh.t_at(0, 0)).visibility = 0.25 };
}

struct MaterialData {
    texture: Handle,
    polygons: Vec<*mut TerrainPolygon>,
    indices: Vec<*mut TerrainPolygon>,
    strips: Vec<*mut TerrainPolygon>,
}

impl MaterialData {
    fn new(texture: usize, polys: &[usize], base: *mut TerrainPolygon) -> MaterialData {
        MaterialData {
            texture: texture as Handle,
            polygons: polys.iter().map(|p| unsafe { base.add(*p) }).collect(),
            indices: vec![core::ptr::null_mut(); polys.len() + 2],
            strips: vec![core::ptr::null_mut(); polys.len() + 2],
        }
    }

    fn set_counted(&mut self, ind: &[usize], strip: &[usize], base: *mut TerrainPolygon) {
        for (i, p) in ind.iter().enumerate() {
            self.indices[i] = unsafe { base.add(*p) };
        }
        let n = self.indices.len();
        self.indices[ind.len()] = core::ptr::null_mut();
        self.indices[n - 1] = ind.len() as *mut TerrainPolygon;
        for (i, p) in strip.iter().enumerate() {
            self.strips[i] = unsafe { base.add(*p) };
        }
        let n = self.strips.len();
        self.strips[strip.len()] = core::ptr::null_mut();
        self.strips[n - 1] = strip.len() as *mut TerrainPolygon;
    }

    fn view(&mut self) -> MaterialView {
        MaterialView {
            texture: self.texture,
            polygons: self.polygons.as_ptr(),
            polygon_count: self.polygons.len() as c_ulong,
            indices: self.indices.as_mut_ptr(),
            indices_length: self.indices.len() as c_ulong,
            strips: self.strips.as_mut_ptr(),
            strips_length: self.strips.len() as c_ulong,
        }
    }
}

fn check(mut h: Host, section: &str, r: Option<LongResult>) {
    if let Some(r) = r {
        h.rec.log.push(format!("ret {r}"));
    }
    let e = h.rec.end_line();
    h.rec.log.push(e);
    let want = golden_section(section);
    if h.rec.log != want {
        for (i, (a, b)) in h.rec.log.iter().zip(want.iter()).enumerate() {
            if a != b {
                panic!("{section}: line {i}\n got {a}\nwant {b}\n(got {} lines, want {})", h.rec.log.len(), want.len());
            }
        }
        panic!("{section}: length {} vs {}", h.rec.log.len(), want.len());
    }
}

#[test]
fn terrain_draw_and_space_map_order() {
    let mut mesh = Mesh::new(2, 1, 3, 3, true);
    let mut polys = vec![blank_polygon(); 16];
    fill_polys(&mut mesh, &mut polys, 8);
    let base = polys.as_mut_ptr();
    let mut mats = [
        MaterialData::new(0x1000, &[0, 1], base),
        MaterialData::new(0x2000, &[2], base),
        MaterialData::new(0x3000, &[3, 4], base),
    ];
    mats[0].set_counted(&[0], &[1], base);
    mats[1].set_counted(&[2], &[], base);
    mats[2].set_counted(&[3, 4], &[], base);
    let map = [
        MaterialMapEntry { texture: 0x1000usize as Handle, index: 0 },
        MaterialMapEntry { texture: 0x2000usize as Handle, index: 1 },
        MaterialMapEntry { texture: 0x3000usize as Handle, index: 2 },
    ];
    let vp = viewport();
    let views: Vec<_> = mats.iter_mut().map(MaterialData::view).collect();

    let mut h = Host::new();
    let r = draw_terrain(&mut h, &mesh.view(), &map, &views, &vp, false);
    check(h, "terrain_draw", Some(r));

    // ...then a space map with the same materials (materials 1..8 first), orthographic
    mesh.space = true;
    let mut h = Host::new();
    let r = draw_terrain(&mut h, &mesh.view(), &map, &views, &vp, true);
    check(h, "terrain_draw_space_ortho", Some(r));
    mesh.space = false;

    // the water material (polygons 5 and 6)
    let mut water = [MaterialData::new(0x2000, &[5, 6], base)];
    water[0].set_counted(&[5], &[6], base);
    let views: Vec<_> = water.iter_mut().map(MaterialData::view).collect();
    let mut h = Host::new();
    let r = draw_water_and_mark(&mut h, &mesh.view(), &views, &vp, false);
    check(h, "water_draw", Some(r));
}

#[test]
fn no_visible_rows_draws_two_black_quads() {
    let mut mesh = Mesh::new(0, 0, 1, 1, false);
    let mut h = Host::new();
    let r = draw_terrain(&mut h, &mesh.view(), &[], &[], &viewport(), false);
    check(h, "terrain_black", Some(r));
}

#[test]
fn grid_lines_skip_lines_outside_the_visible_window() {
    let mut mesh = Mesh::new(1, 1, 4, 4, true);
    mesh.tvis[N + 1] = false; // [1][1]
    let lines: [[c_long; 4]; 4] = [[1, 1, 2, 1], [2, 2, 3, 2], [0, 2, 3, 2], [3, 3, 4, 4]];
    let mut h = Host::new();
    let r = draw_grid_lines(&mut h, &mesh.view(), &lines, 0x1000usize as Handle, 0.5, &viewport(), false);
    check(h, "gridlines", Some(r));
}

#[test]
fn terrain_grid_draws_a_cell_per_visible_tile() {
    let mut mesh = Mesh::new(2, 2, 2, 2, true);
    for cx in 0..5usize {
        for cy in 0..5usize {
            let v = mesh.t_at(cx, cy);
            unsafe { (*v).point = [cx as f32 * 1.25, cy as f32 * 0.5, cx as f32 + cy as f32] };
        }
    }
    mesh.tvis[N + 1] = false;
    let mut h = Host::new();
    let r = draw_terrain_grid(&mut h, &mesh.view(), &viewport(), true);
    check(h, "terraingrid", Some(r));
}

fn highlight_mesh(w: c_ulong, h: c_ulong, points: usize) -> Mesh {
    let mut mesh = Mesh::new(0, 0, w, h, true);
    for cx in 0..points {
        for cy in 0..points {
            let v = mesh.t_at(cx, cy);
            unsafe { (*v).point = [cx as f32 * 1.5, cy as f32 * 0.25, -1.0 * cx as f32] };
        }
    }
    mesh
}

#[test]
fn highlight_small_rectangle() {
    let mut mesh = highlight_mesh(11, 11, 12);
    let rect = [2, 3, 5, 4]; // left, top, right, bottom
    let mut h = Host::new();
    let r = draw_terrain_highlight(&mut h, &mesh.view(), rect, [0.9, 0.1, 0.5], 0.6, &viewport(), false);
    check(h, "highlight_small", Some(r));
}

#[test]
fn highlight_skips_invalid_tiles() {
    let mut mesh = highlight_mesh(100, 100, 102);
    let mut h = Host::new();
    let r = draw_terrain_highlight(&mut h, &mesh.view(), [0, 0, 40, 40], [0.2, 0.4, 0.8], 0.25, &viewport(), true);
    check(h, "highlight_invalid_mostly", Some(r));
}

fn big_tile_mesh() -> Mesh {
    let mut mesh = highlight_mesh(100, 100, 102);
    mesh.tile_rows = 90;
    mesh.tile_row_count = 80;
    mesh
}

#[test]
fn highlight_splits_into_400_quad_batches() {
    let mut mesh = big_tile_mesh();
    let mut h = Host::new();
    let r = draw_terrain_highlight(&mut h, &mesh.view(), [0, 0, 20, 19], [0.5, 0.5, 0.5], 1.0, &viewport(), false);
    check(h, "highlight_batch", Some(r));
}

#[test]
fn highlight_exactly_400_quads_draws_after_the_loop() {
    let mut mesh = big_tile_mesh();
    let mut h = Host::new();
    let r = draw_terrain_highlight(&mut h, &mesh.view(), [0, 0, 19, 19], [0.1, 0.2, 0.3], 0.5, &viewport(), false);
    check(h, "highlight_exact400", Some(r));
}

#[test]
fn terrain_material_batches_and_skips_invisible_triangles() {
    let mut mesh = Mesh::new(0, 0, 3, 3, true);
    let mut polys = vec![blank_polygon(); 16];
    fill_polys(&mut mesh, &mut polys, 16);
    for i in 10..13usize {
        let rv = mesh.t_at(5 + i - 10, 6);
        for j in 0..3 {
            polys[i].vertices[j] = rv;
            polys[i].alpha[j] = 0.0;
        }
        unsafe {
            (*rv).visibility = 1.0;
            (*rv).point = [1.0, 2.0, 3.0];
        }
    }
    let base = polys.as_mut_ptr();
    let mut mat = MaterialData::new(0x1000, &[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14], base);
    mat.set_counted(&[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13], &[14], base);
    let views = [mat.view()];
    let map = [MaterialMapEntry { texture: 0x1000usize as Handle, index: 0 }];
    let mut h = Host::new();
    let r = draw_terrain(&mut h, &mesh.view(), &map, &views, &viewport(), false);
    check(h, "terrain_material_mixed", Some(r));
}

fn fmt_l(mesh: &mut Mesh, log: &mut Vec<String>) {
    log.push("  ambient-color check".into());
    for cy in 0..4usize {
        for cx in 0..4usize {
            let t = unsafe { *mesh.tv.as_ptr().add(cx * N + cy) };
            let w = unsafe { *mesh.wv.as_ptr().add(cx * N + cy) };
            log.push(format!(
                "  l {cx} {cy} {} {} {} | {} {} {}",
                b(t.color[0]),
                b(t.color[1]),
                b(t.color[2]),
                b(w.color[0]),
                b(w.color[1]),
                b(w.color[2])
            ));
        }
    }
}

#[test]
fn light_sums_directional_lights_on_visible_vertices() {
    let mut mesh = Mesh::new(1, 1, 2, 2, true);
    let mut h = Host::new();
    h.lights[0] =
        Some(LightData { light_type: 3, diffuse: [0.75, 0.5, 0.25], direction: [0.267261, -0.534522, -0.801784] });
    h.lights[1] = Some(LightData { light_type: 3, diffuse: [0.1, 0.2, 0.3], direction: [-0.5, 0.5, 0.70710678] });
    h.lights[2] = Some(LightData { light_type: 1, diffuse: [1.0, 0.0, 0.0], direction: [0.0, 0.0, 0.0] });
    light_terrain(&mut h, &mesh.view(), [0.1, 0.1, 0.2]);
    let mut log = core::mem::take(&mut h.rec.log);
    fmt_l(&mut mesh, &mut log);
    let want = golden_section("light");
    assert_eq!(log, want);
}

/// The fixed viewport of the transform scenarios.
fn camera_viewport() -> ViewportInfo {
    let mut v = viewport();
    v.camera_inverse = IDENTITY;
    v.camera_inverse[0][3] = -1.5;
    v.camera_inverse[1][3] = 4.0;
    v.camera_inverse[2][3] = 2.0;
    v.left_plane = plane([0.0, 0.0, 0.0], [1.0, 0.0, 0.0]);
    v.right_plane = plane([0.0, 0.0, 0.0], [-1.0, 0.0, 0.0]);
    v.top_plane = plane([0.0, 0.0, 1.0], [0.0, 0.0, 1.0]);
    v.bottom_plane = plane([0.0, 0.0, -1.0], [0.0, 0.0, -1.0]);
    v.ortho_left = -1.0;
    v.ortho_top = 1.0;
    v.ortho_right = 3.0;
    v.ortho_bottom = -1.0;
    v.y_clip_plane = 0.5;
    v
}

fn parse_um(section: &[String]) -> Vec<(&'static str, u32, u32)> {
    section
        .iter()
        .filter_map(|l| {
            let mut it = l.split_whitespace();
            (it.next()? == "um").then_some(())?;
            let op = match it.next()? {
                "atan" => "atan",
                "tan" => "tan",
                "cos" => "cos",
                _ => return None,
            };
            let arg = u32::from_str_radix(it.next()?, 16).ok()?;
            let res = u32::from_str_radix(it.next()?, 16).ok()?;
            Some((op, arg, res))
        })
        .collect()
}

fn dump_list(log: &mut Vec<String>, name: &str, list: &[*mut TerrainPolygon], base: *mut TerrainPolygon) {
    let mut line = format!("  {name}");
    for &p in list {
        if p.is_null() {
            line.push_str(" 0");
        } else if (p as usize) >= base as usize && (p as usize) < unsafe { base.add(16) } as usize {
            let index = (p as usize - base as usize) / core::mem::size_of::<TerrainPolygon>();
            line.push_str(&format!(" p{index}"));
        } else {
            line.push_str(&format!(" #{}", p as usize));
        }
    }
    log.push(line);
}

#[allow(clippy::too_many_arguments)]
fn dump_grid(mesh: &Mesh, sx: usize, sy: usize, w: usize, h: usize, log: &mut Vec<String>) {
    for y in sy..sy + h + 2 {
        for x in sx..sx + w + 2 {
            let v = mesh.tv[(x - sx) * N + (y - sy)];
            let vw = mesh.wv[(x - sx) * N + (y - sy)];
            log.push(format!(
                "  t {x} {y} {} {} {} {:x} | {} {} {} {:x}",
                b(v.point[0]),
                b(v.point[1]),
                b(v.point[2]),
                v.clip_flags,
                b(vw.point[0]),
                b(vw.point[1]),
                b(vw.point[2]),
                vw.clip_flags
            ));
        }
    }
}

fn run_to_camera(name: &str, ortho: bool, water_only: bool, mut time: f32) {
    let golden = golden_section(name);
    let mut mesh = Mesh::new(2, 2, 2, 2, true);
    let mut polys = vec![blank_polygon(); 16];
    fill_polys(&mut mesh, &mut polys, 6);
    let base = polys.as_mut_ptr();
    let edge_world = [[0.5f32, 0.25, 0.125]];
    let cliff_world = [[1.5f32, 2.25, -0.25]];
    let edge_vertices = [mesh.t_at(0, 0)];
    let cliff_vertices = [mesh.t_at(1, 1)];
    let mut terrain = [MaterialData::new(0x1000, &[0, 1, 2, 3, 4, 5], base)];
    let mut water = [MaterialData::new(0x2000, &[0, 1, 2], base)];
    for j in 0..3 {
        unsafe { (*polys[0].vertices[j]).clip_flags = 0 };
    }
    let mut rv = vec![blank_vertex(); 12];
    let rvb = rv.as_mut_ptr();
    let pts = |i: usize| unsafe { rvb.add(i) };
    let flags: [(usize, usize, c_ulong); 12] = [
        (1, 0, 1),
        (1, 1, 0),
        (1, 2, 0),
        (2, 3, 0x11),
        (2, 4, 1),
        (2, 5, 1),
        (3, 6, 2),
        (3, 7, 4),
        (3, 8, 8),
        (4, 9, 0x3f),
        (4, 10, 0x3f),
        (4, 11, 0x3f),
    ];
    for (i, f) in flags.iter().enumerate() {
        let (poly, slot, value) = (f.0, f.1, f.2);
        let j = i % 3;
        polys[poly].vertices[j] = pts(slot);
        unsafe { (*pts(slot)).clip_flags = value };
    }
    let terrain_views: Vec<_> = terrain.iter_mut().map(MaterialData::view).collect();
    let water_views: Vec<_> = water.iter_mut().map(MaterialData::view).collect();
    let vp = camera_viewport();
    let mut view = mesh.view();
    view.edge_world = edge_world.as_ptr();
    view.edge_vertices = edge_vertices.as_ptr();
    view.edge_count = 1;
    view.cliff_world = cliff_world.as_ptr();
    view.cliff_vertices = cliff_vertices.as_ptr();
    view.cliff_count = 1;
    let mut h = Host::new();
    h.um = parse_um(&golden);
    let ripple = WaterRipple { time_rate: 0.125, water_height: 0.5, terrain_height: 0.25 };
    transform_to_camera(&mut h, &view, &terrain_views, &water_views, &vp, 2.0, water_only, ortho, ripple, &mut time);
    let mut log = core::mem::take(&mut h.rec.log);
    dump_grid(&mesh, 2, 2, 2, 2, &mut log);
    let e = unsafe { *mesh.t_at(0, 0) };
    log.push(format!("  edge {} {} {}", b(e.point[0]), b(e.point[1]), b(e.point[2])));
    dump_list(&mut log, "indices", &terrain[0].indices, base);
    dump_list(&mut log, "strips", &terrain[0].strips, base);
    dump_list(&mut log, "windices", &water[0].indices, base);
    dump_list(&mut log, "wstrips", &water[0].strips, base);
    assert_eq!(log, golden, "{name}");
}

#[test]
fn transform_to_camera_perspective_full() {
    run_to_camera("tocamera_o0_w0", false, false, 0.0);
}

#[test]
fn transform_to_camera_perspective_water_only() {
    run_to_camera("tocamera_o0_w1", false, true, 0.25);
}

#[test]
fn transform_to_camera_orthographic_full() {
    run_to_camera("tocamera_o1_w0", true, false, 0.5);
}

#[test]
fn transform_to_camera_orthographic_water_only() {
    run_to_camera("tocamera_o1_w1", true, true, 0.75);
}

#[test]
fn transform_to_camera_returns_early_with_no_ripple_in_water_mode() {
    let golden = golden_section("tocamera_early");
    let mut mesh = Mesh::new(2, 2, 2, 2, true);
    let mut view = mesh.view();
    view.water_percentage = 0.5;
    let mut h = Host::new();
    h.um = parse_um(&golden);
    let mut time = 7.0;
    let ripple = WaterRipple { time_rate: 0.0, water_height: 0.5, terrain_height: 0.25 };
    transform_to_camera(&mut h, &view, &[], &[], &camera_viewport(), 2.0, true, false, ripple, &mut time);
    assert_eq!(time, 7.0);
    let e = unsafe { *mesh.t_at(0, 0) };
    h.rec.log.push(format!("  flag {:x}", e.clip_flags));
    let end = h.rec.end_line();
    h.rec.log.push(end);
    assert_eq!(h.rec.log, golden);
}
