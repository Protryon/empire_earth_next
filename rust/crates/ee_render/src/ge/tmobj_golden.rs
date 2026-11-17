//! GETerrainMesh's golden replayed (`golden_gtm.txt`): the scratch driver of PORT_LEARNINGS "R4.3, eleventh part"
//! (`build/ge11/golden/driver_gtm.cpp`) ran random sequences of every public member over the original unit (the
//! census commit 030d814a) and traced fakes, and printed one FNV-1a digest of each sequence's trace lines. This is
//! the driver ported line for line onto the Rust object's C ABI (the calls the C++ forwarders make), with a mock host
//! that is the driver's fakes (the clock, the fractal's hash values, the 48 textures and their reference counts, the
//! texture sets, the quantized normals, the memory file): it draws the same stream, makes the same calls, writes the
//! same lines and must print the same digests.

#![allow(clippy::unnecessary_cast)]

use std::cell::RefCell;
use std::ffi::{c_long, c_ulong, c_void};
use std::fmt::Write as _;

use super::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{GeRectangle, GeTransformation};
use ee_base::math::vector::{build_quantized_table, quantized_index, QUANT_COUNT};

const GOLDEN: &str = include_str!("golden_gtm.txt");
/// The sequences replayed (the driver's first ones, in order: they share the clock, the counters and the textures'
/// reference counts).
const SEQUENCES: usize = 12;
const FNV: u64 = 0xcbf2_9ce4_8422_2325;

struct Set {
    id: i32,
    width: c_ulong,
    height: c_ulong,
    coverage: c_ulong,
    textures: Vec<usize>,
}

/// The driver's globals and fakes.
struct World {
    hash: u64,
    time: c_ulong,
    fractals: i32,
    set_ids: i32,
    refs: [i32; 48],
    sets: Vec<Set>,
    fractal_seeds: Vec<c_ulong>,
    rng: u32,
}

thread_local! {
    static W: RefCell<World> = const { RefCell::new(World {
        hash: FNV,
        time: 0,
        fractals: 0,
        set_ids: 0,
        refs: [0; 48],
        sets: Vec::new(),
        fractal_seeds: Vec::new(),
        rng: 0,
    }) };
    static TABLE: &'static [GeVector] = {
        let mut t = vec![GeVector::default(); QUANT_COUNT];
        build_quantized_table(&mut t);
        Box::leak(t.into_boxed_slice())
    };
    static ZERO: &'static GeVector = Box::leak(Box::new(GeVector::default()));
}

fn fnv(h: &mut u64, bytes: &[u8]) {
    for &b in bytes {
        *h ^= b as u64;
        *h = h.wrapping_mul(0x100000001b3);
    }
}

fn trace(line: String) {
    W.with(|w| {
        let mut w = w.borrow_mut();
        fnv(&mut w.hash, line.as_bytes());
        fnv(&mut w.hash, b"\n");
    });
}

/// A texture's handle: the pool's addresses increase with the index, as the driver's `gPool`.
fn tex_handle(i: usize) -> usize {
    (i + 1) * 16
}
fn tex_id(h: usize) -> i32 {
    if h == 0 {
        -1
    } else {
        (h / 16) as i32
    }
}
fn set_handle(k: usize) -> usize {
    0x10_0000 + k * 16
}
fn set_index(h: usize) -> usize {
    (h - 0x10_0000) / 16
}

unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeTmOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    match op {
        EeGeTmOp::Time => {
            let t = W.with(|w| w.borrow().time);
            trace(format!("time {t}"));
            t as isize
        }
        EeGeTmOp::FractalNew => {
            let id = W.with(|w| {
                let mut w = w.borrow_mut();
                w.fractals += 1;
                w.fractal_seeds.push(a as c_ulong);
                w.fractals
            });
            trace(format!("fractal new {id} seed {}", a as c_ulong));
            id as isize
        }
        EeGeTmOp::FractalDelete => {
            trace(format!("fractal delete {o}"));
            0
        }
        EeGeTmOp::FractalCompute => {
            W.with(|w| w.borrow_mut().fractal_seeds[o - 1] = a as c_ulong);
            trace(format!("fractal compute {o} seed {}", a as c_ulong));
            0
        }
        EeGeTmOp::FractalValue => {
            let seed = W.with(|w| w.borrow().fractal_seeds[o - 1]) as u32;
            let (x, y) = ((b >> 16) as u32, (b & 0xffff) as u32);
            let mut h = seed.wrapping_mul(2654435761) ^ x.wrapping_mul(40503).wrapping_add(y.wrapping_mul(9973));
            h ^= h >> 13;
            h = h.wrapping_mul(0x5bd1e995);
            h ^= h >> 15;
            // SAFETY: the operation's contract
            unsafe { *(a as *mut f32) = (h >> 8) as f32 * (1.0 / 16777216.0) };
            0
        }
        EeGeTmOp::Quantize => {
            // SAFETY: the operation's contract
            let v = unsafe { *(a as *const GeVector) };
            let i = quantized_index(v);
            if i >= 0 {
                TABLE.with(|t| &t[i as usize] as *const GeVector as isize)
            } else {
                ZERO.with(|z| *z as *const GeVector as isize)
            }
        }
        EeGeTmOp::SetDelete => {
            if o != 0 {
                let id = W.with(|w| w.borrow().sets[set_index(o)].id);
                trace(format!("set {id} delete"));
            }
            0
        }
        EeGeTmOp::SetWidth => W.with(|w| w.borrow().sets[set_index(o)].width as isize),
        EeGeTmOp::SetHeight => W.with(|w| w.borrow().sets[set_index(o)].height as isize),
        EeGeTmOp::SetCoverage => W.with(|w| w.borrow().sets[set_index(o)].coverage as isize),
        EeGeTmOp::SetTexture | EeGeTmOp::SetTextureAt => W.with(|w| {
            let w = w.borrow();
            let s = &w.sets[set_index(o)];
            let extra = if op == EeGeTmOp::SetTextureAt { 1 } else { 0 };
            let k = ((a as c_ulong).wrapping_mul(s.height).wrapping_add(b as c_ulong).wrapping_add(extra)) as usize;
            s.textures[k % s.textures.len()] as isize
        }),
        EeGeTmOp::TexReference | EeGeTmOp::TexDereference => {
            let i = o / 16 - 1;
            let n = W.with(|w| {
                let mut w = w.borrow_mut();
                w.refs[i] += if op == EeGeTmOp::TexReference { 1 } else { -1 };
                w.refs[i]
            });
            let what = if op == EeGeTmOp::TexReference { "ref" } else { "deref" };
            trace(format!("tex {} {what} {n}", i + 1));
            0
        }
    }
}

fn rnd() -> u32 {
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.rng = w.rng.wrapping_mul(1664525).wrapping_add(1013904223);
        w.rng >> 8
    })
}
fn r(n: u32) -> u32 {
    if n == 0 {
        0
    } else {
        rnd() % n
    }
}
fn f(lo: f32, hi: f32) -> f32 {
    lo + (rnd() & 0xffff) as f32 / 65536.0 * (hi - lo)
}
fn bits(v: f32) -> u32 {
    v.to_bits()
}

// ---- the memory file (the driver's FSFile) ----

struct MemFile {
    data: Vec<u8>,
    pos: usize,
    limit: usize,
}

impl MemFile {
    fn get(&mut self, out: &mut [u8]) -> c_long {
        if self.pos + out.len() > self.data.len() {
            return 0x12;
        }
        out.copy_from_slice(&self.data[self.pos..self.pos + out.len()]);
        self.pos += out.len();
        0
    }
    fn put(&mut self, data: &[u8]) -> c_long {
        if self.data.len() + data.len() > self.limit {
            return 9;
        }
        self.data.extend_from_slice(data);
        0
    }
}

fn file<'a>(p: *mut c_void) -> &'a mut MemFile {
    // SAFETY: the table's file is a live MemFile
    unsafe { &mut *(p as *mut MemFile) }
}
unsafe extern "C" fn r8(p: *mut c_void, o: *mut u8) -> c_long {
    let mut b = [0u8; 1];
    let r = file(p).get(&mut b);
    if r == 0 {
        // SAFETY: the table's contract
        unsafe { *o = b[0] };
    }
    r
}
unsafe extern "C" fn r16(p: *mut c_void, o: *mut u16) -> c_long {
    let mut b = [0u8; 2];
    let r = file(p).get(&mut b);
    if r == 0 {
        // SAFETY: as above
        unsafe { *o = u16::from_le_bytes(b) };
    }
    r
}
unsafe extern "C" fn r32(p: *mut c_void, o: *mut u32) -> c_long {
    let mut b = [0u8; 4];
    let r = file(p).get(&mut b);
    if r == 0 {
        // SAFETY: as above
        unsafe { *o = u32::from_le_bytes(b) };
    }
    r
}
unsafe extern "C" fn rf(p: *mut c_void, o: *mut f32) -> c_long {
    let mut b = [0u8; 4];
    let r = file(p).get(&mut b);
    if r == 0 {
        // SAFETY: as above
        unsafe { *o = f32::from_le_bytes(b) };
    }
    r
}
unsafe extern "C" fn w8(p: *mut c_void, v: u8) -> c_long {
    file(p).put(&[v])
}
unsafe extern "C" fn w16(p: *mut c_void, v: u16) -> c_long {
    file(p).put(&v.to_le_bytes())
}
unsafe extern "C" fn w32(p: *mut c_void, v: u32) -> c_long {
    file(p).put(&v.to_le_bytes())
}
unsafe extern "C" fn wf(p: *mut c_void, v: f32) -> c_long {
    file(p).put(&v.to_le_bytes())
}
unsafe extern "C" fn rb(p: *mut c_void, o: *mut u8, n: u32, read: *mut u32) -> c_long {
    let mut b = vec![0u8; n as usize];
    let r = file(p).get(&mut b);
    // SAFETY: the table's contract
    unsafe {
        if r == 0 {
            std::ptr::copy_nonoverlapping(b.as_ptr(), o, n as usize);
        }
        *read = if r == 0 { n } else { 0 };
    }
    r
}
unsafe extern "C" fn wb(p: *mut c_void, d: *const u8, n: u32, written: *mut u32) -> c_long {
    // SAFETY: the table's contract
    let r = file(p).put(unsafe { std::slice::from_raw_parts(d, n as usize) });
    // SAFETY: as above
    unsafe { *written = if r == 0 { n } else { 0 } };
    r
}
unsafe extern "C" fn no_position(_: *mut c_void, _: *mut c_long) -> c_long {
    9
}
unsafe extern "C" fn no_set_position(_: *mut c_void, _: c_long) -> c_long {
    9
}

fn io_of(f: &mut MemFile) -> EeGeFieldIo {
    EeGeFieldIo {
        file: f as *mut MemFile as *mut c_void,
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
        get_position: no_position,
        set_position: no_set_position,
    }
}

// ---- the driver ----

struct Seq {
    m: *mut EeGeTerrainMesh,
    w: c_long,
    h: c_long,
    sets: Vec<c_ulong>,
    color_sets: c_ulong,
    frames: c_ulong,
}

// the forwarders, as the C++ class calls them
fn valid(m: *mut EeGeTerrainMesh, x: c_ulong, y: c_ulong, tile: bool) -> bool {
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_is_valid(m, x, y, tile) }
}
fn get(m: *mut EeGeTerrainMesh, v: EeGeTmVal) -> c_ulong {
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_get(m, v) }
}
fn get_f(m: *mut EeGeTerrainMesh, v: EeGeTmVal) -> f32 {
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_get_f(m, v) }
}
fn point(m: *mut EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> GeTmPoint {
    // SAFETY: the point is valid
    unsafe { *ee_ge_tm_point(m, x, y) }
}
fn tile(m: *mut EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> GeTmTile {
    // SAFETY: the tile is valid
    unsafe { *ee_ge_tm_tile(m, x, y) }
}
fn tile_edit(m: *mut EeGeTerrainMesh, x: c_ulong, y: c_ulong, what: u32, value: c_ulong) {
    // SAFETY: the tile is valid
    unsafe { ee_ge_tm_tile_edit(m, x, y, what, value) }
}
fn point_edit(m: *mut EeGeTerrainMesh, x: c_ulong, y: c_ulong, what: u32, value: c_ulong, z: f32) {
    // SAFETY: the point is valid
    unsafe { ee_ge_tm_point_edit(m, x, y, what, value, z) }
}
fn edge(m: *mut EeGeTerrainMesh, x: c_long, y: c_long, which: u32) -> u32 {
    // SAFETY: the mesh is live
    u32::from(unsafe { ee_ge_tm_is_edge_tile(m, x, y, which) })
}
fn material(m: *mut EeGeTerrainMesh, water: bool, k: c_ulong) -> EeGeTmMaterialView {
    let mut v = EeGeTmMaterialView {
        texture: std::ptr::null_mut(),
        polygons: std::ptr::null_mut(),
        polygon_count: 0,
        indices: std::ptr::null_mut(),
        indices_length: 0,
        strips: std::ptr::null_mut(),
        strips_length: 0,
    };
    // SAFETY: the material exists
    unsafe { ee_ge_tm_material(m, water, k, &mut v) };
    v
}

fn random_tile(s: &Seq) -> (c_ulong, c_ulong) {
    loop {
        let x = r((s.w + s.h + 1) as u32) as c_ulong;
        let y = r((s.w + s.h + 1) as u32) as c_ulong;
        if valid(s.m, x, y, true) {
            return (x, y);
        }
    }
}
fn random_point(s: &Seq) -> (c_ulong, c_ulong) {
    loop {
        let x = r((s.w + s.h + 2) as u32) as c_ulong;
        let y = r((s.w + s.h + 2) as u32) as c_ulong;
        if valid(s.m, x, y, false) {
            return (x, y);
        }
    }
}

const RV: usize = std::mem::size_of::<EeGeTmRenderVertex>();
const SPAN: usize = RV * SIDE * SIDE;

fn vertex_code(m: *mut EeGeTerrainMesh, p: *const EeGeTmRenderVertex) -> u32 {
    if p.is_null() {
        return 0;
    }
    // SAFETY: the mesh is live; the arrays are its
    unsafe {
        let tb = ee_ge_tm_render_vertices(m, false) as usize;
        let wb = ee_ge_tm_render_vertices(m, true) as usize;
        let c = p as usize;
        if c >= tb && c < tb + SPAN {
            return 0x100_0000 | ((c - tb) / RV) as u32;
        }
        if c >= wb && c < wb + SPAN {
            return 0x200_0000 | ((c - wb) / RV) as u32;
        }
        for i in 0..get(m, EeGeTmVal::EdgeVertexCount) as usize {
            if std::ptr::eq(*ee_ge_tm_edge_vertices(m, false).add(i), p) {
                return 0x300_0000 | i as u32;
            }
        }
        for i in 0..get(m, EeGeTmVal::CliffVertexCount) as usize {
            if std::ptr::eq(*ee_ge_tm_edge_vertices(m, true).add(i), p) {
                return 0x400_0000 | i as u32;
            }
        }
    }
    0xdead
}

fn link_code(m: *mut EeGeTerrainMesh, p: *const f32) -> u32 {
    if p.is_null() {
        return 0;
    }
    // SAFETY: the mesh is live
    let tb = unsafe { std::ptr::addr_of!((*ee_ge_tm_render_vertices(m, false)).visibility) } as usize;
    0x500_0000 | ((p as usize).wrapping_sub(tb) / RV) as u32
}

fn dump_vertex(m: *mut EeGeTerrainMesh, v: &EeGeTmRenderVertex, what: &str, i: usize) {
    trace(format!(
        "{what} {i} p {:08x} {:08x} {:08x} c {:08x} {:08x} {:08x} vis {:08x} fog {:08x} clip {:x} link {:x}",
        bits(v.point.x),
        bits(v.point.y),
        bits(v.point.z),
        bits(v.color[0]),
        bits(v.color[1]),
        bits(v.color[2]),
        bits(v.visibility),
        bits(v.fog),
        v.clip_flags,
        link_code(m, v.linked_visibility)
    ));
}

fn dump_frame(s: &Seq) {
    let m = s.m;
    let n_terrain = get(m, EeGeTmVal::TerrainMaterialCount);
    let n_water = get(m, EeGeTmVal::WaterMaterialCount);
    trace(format!(
        "frame tiles {} {} rows {} polys {} bl {} {} dim {} {} mats {} {}",
        get(m, EeGeTmVal::NumVisibleTiles),
        get(m, EeGeTmVal::NumVisibleTiles),
        get(m, EeGeTmVal::VisibleTileRows),
        get(m, EeGeTmVal::VisiblePolyCount),
        get(m, EeGeTmVal::BottomLeftX),
        get(m, EeGeTmVal::BottomLeftY),
        get(m, EeGeTmVal::VisibleTileWidth),
        get(m, EeGeTmVal::VisibleTileHeight),
        n_terrain,
        n_water
    ));
    for i in 0..get(m, EeGeTmVal::VisibleTileRows) {
        let mut row = [0 as c_long; 3];
        // SAFETY: the row exists
        unsafe { ee_ge_tm_visible_row(m, i, row.as_mut_ptr()) };
        trace(format!("row {} {} {}", row[0], row[1], row[2]));
    }
    for water in 0..2 {
        let n = if water == 1 { n_water } else { n_terrain };
        for k in 0..n {
            let v = material(m, water == 1, k);
            let mut h = FNV;
            // SAFETY: the lists are the material's
            unsafe {
                for p in 0..v.polygon_count {
                    let q = &**v.polygons.add(p);
                    for c in 0..3 {
                        let w = [vertex_code(m, q.vertices[c]), bits(q.alpha[c]), bits(q.uv[c][0]), bits(q.uv[c][1])];
                        for x in w {
                            fnv(&mut h, &x.to_le_bytes());
                        }
                    }
                }
            }
            let mut nulls = 0;
            // SAFETY: as above
            unsafe {
                for p in 0..v.indices_length {
                    nulls += i32::from((*v.indices.add(p)).is_null());
                }
                for p in 0..v.strips_length {
                    nulls += i32::from((*v.strips.add(p)).is_null());
                }
            }
            trace(format!(
                "mat {water} {k} tex {} polys {} idx {} strips {} nulls {nulls} hash {h:016x}",
                tex_id(v.texture as usize),
                v.polygon_count,
                v.indices_length,
                v.strips_length
            ));
        }
    }
    let mut textures = vec![std::ptr::null_mut(); n_terrain as usize];
    let mut indices = vec![0; n_terrain as usize];
    // SAFETY: the arrays hold the material count
    let n = unsafe { ee_ge_tm_material_map(m, textures.as_mut_ptr(), indices.as_mut_ptr(), n_terrain as usize) };
    for i in 0..n {
        trace(format!("map {} {}", tex_id(textures[i] as usize), indices[i]));
    }
    for (cliff, name, vname) in [(false, "edge", "ev"), (true, "cliff", "cv")] {
        let count = get(m, if cliff { EeGeTmVal::CliffVertexCount } else { EeGeTmVal::EdgeVertexCount }) as usize;
        for i in 0..count {
            // SAFETY: i is below the count
            let (p, v) =
                unsafe { (*ee_ge_tm_edge_points(m, cliff).add(i), &**ee_ge_tm_edge_vertices(m, cliff).add(i)) };
            trace(format!("{name} {i} {:08x} {:08x} {:08x}", bits(p.x), bits(p.y), bits(p.z)));
            dump_vertex(m, v, vname, i);
        }
    }
}

fn dump_arrays(s: &Seq) {
    let m = s.m;
    let mut h = FNV;
    // SAFETY: the arrays are the mesh's
    unsafe {
        for water in [false, true] {
            fnv(&mut h, std::slice::from_raw_parts(ee_ge_tm_point_visibility(m, water), SIDE * SIDE));
        }
        for water in [false, true] {
            let a = std::slice::from_raw_parts(ee_ge_tm_render_vertices(m, water), SIDE * SIDE);
            for v in a {
                let w = [
                    bits(v.visibility),
                    bits(v.color[0]),
                    bits(v.point.x),
                    v.clip_flags as u32,
                    link_code(m, v.linked_visibility),
                ];
                for x in w {
                    fnv(&mut h, &x.to_le_bytes());
                }
            }
        }
    }
    trace(format!(
        "arrays {h:016x} water {:08x} space {:08x}",
        bits(get_f(m, EeGeTmVal::VisibleWaterPercentage)),
        bits(get_f(m, EeGeTmVal::VisibleSpacePercentage))
    ));
}

fn dump_state(s: &Seq) {
    let m = s.m;
    let mut h = FNV;
    let n = (s.w + s.h + 3) as c_ulong;
    let (table, zero) = (TABLE.with(|t| t.as_ptr()), ZERO.with(|z| *z as *const GeVector));
    for y in 0..n {
        for x in 0..n {
            let mut w = [0u32; 8];
            w[0] = u32::from(valid(m, x, y, false));
            if w[0] != 0 {
                let pt = point(m, x, y);
                w[1] = bits(pt.z);
                w[2] = pt.color_table_index as u32;
                w[3] = if pt.normal == zero {
                    0xffff_ffff
                } else if pt.normal.is_null() {
                    0xffff_fffe
                } else {
                    ((pt.normal as usize - table as usize) / std::mem::size_of::<GeVector>()) as u32
                };
                // SAFETY: the point is valid
                let c = unsafe { std::slice::from_raw_parts(ee_ge_tm_vertex_color(m, x, y), 3) };
                w[4] = bits(c[0]) ^ bits(c[1]).wrapping_mul(3) ^ bits(c[2]).wrapping_mul(7);
            }
            w[5] = u32::from(valid(m, x, y, true));
            if w[5] != 0 {
                let t = tile(m, x, y);
                w[6] = t.texture_set_id as u32 | (t.water_info as u32) << 8 | u32::from(t.water_info & 0x7f != 0) << 16;
            }
            // SAFETY: the mesh is live
            let space = u32::from(unsafe { ee_ge_tm_is_space_tile(m, x, y) });
            let (lx, ly) = (x as c_long, y as c_long);
            w[7] = space
                | edge(m, lx, ly, 0) << 1
                | edge(m, lx, ly, 1) << 2
                | edge(m, lx, ly, 2) << 3
                | edge(m, lx, ly, 3) << 4
                | edge(m, lx, ly, 4) << 5;
            for v in w {
                fnv(&mut h, &v.to_le_bytes());
            }
        }
    }
    for i in 0..4 {
        // SAFETY: the plane is the mesh's
        let p = unsafe { *ee_ge_tm_plane(m, i) };
        for v in [p.point.x, p.point.y, p.point.z, p.normal.x, p.normal.y, p.normal.z] {
            fnv(&mut h, &v.to_le_bytes());
        }
    }
    // SAFETY: the mesh is live
    let wc = unsafe { std::slice::from_raw_parts(ee_ge_tm_water_color(m), 3) };
    trace(format!(
        "state {h:016x} z {:08x} {:08x} space {} gw {} cliffs {} water {:08x} {:08x} {:08x}",
        bits(get_f(m, EeGeTmVal::MinimumZ)),
        bits(get_f(m, EeGeTmVal::MaximumZ)),
        u32::from(get(m, EeGeTmVal::SpaceTileCount) > 0),
        get(m, EeGeTmVal::GlobalWaterSetId),
        get(m, EeGeTmVal::SpaceCliffTextureCount),
        bits(wc[0]),
        bits(wc[1]),
        bits(wc[2])
    ));
}

fn make_viewport(s: &Seq) -> (GeViewport, bool) {
    let (tx, ty) = random_tile(s);
    let tz = f(-2.0, 12.0);
    let yaw = f(0.0, 6.2831853);
    let pitch = -f(0.06, 1.5);
    let far = r(7) == 0;
    let distance = if far { f(60.0, 260.0) } else { f(4.0, 90.0) };
    let ortho = r(5) == 0;
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    let (dx, dy, dz) = (t.m[0][1], t.m[1][1], t.m[2][1]);
    t.m[0][3] = tx as f32 - dx * distance;
    t.m[1][3] = ty as f32 - dy * distance;
    t.m[2][3] = tz - dz * distance;
    let front = f(0.1, 3.0);
    let rear = if ortho {
        85.0
    } else if far {
        f(100.0, 300.0)
    } else {
        f(15.0, 120.0)
    };
    let fov = f(0.35, 1.3);
    let view = match r(3) {
        0 => GeRectangle { bottom: 768.0, left: 0.0, right: 1024.0, top: 0.0 },
        1 => GeRectangle { bottom: 480.0, left: 0.0, right: 640.0, top: 0.0 },
        _ => GeRectangle { bottom: 704.0, left: 128.0, right: 896.0, top: 64.0 },
    };
    let ow = (50 + r(850)) as c_long;
    let oh = (50 + r(850)) as c_long;
    let orect =
        GeRectangle { bottom: (oh / 2) as f32, left: (-ow / 2) as f32, right: (ow / 2) as f32, top: (-oh / 2) as f32 };
    (GeViewport::new(view, orect, t, front, rear, fov), ortho)
}

fn run_sequence(seq: i32) {
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.rng = 0x9e37_79b9 ^ (seq as u32).wrapping_mul(7919);
    });
    let dt = 1000 + r(5000) as c_ulong;
    W.with(|w| w.borrow_mut().time += dt);
    // SAFETY: the mock serves every operation
    let m = unsafe { ee_ge_tm_new(host, std::ptr::null_mut()) };
    let mut s = Seq { m, w: 0, h: 0, sets: Vec::new(), color_sets: 0, frames: 0 };
    trace(format!("seq {seq}"));
    let nsets = 1 + r(4) as i32;
    for i in 0..nsets {
        let mut handle = 0usize;
        let mut sid = 0;
        if !(i > 0 && r(10) == 0) {
            let width = 1 + r(3) as c_ulong;
            let height = 1 + r(3) as c_ulong;
            let coverage = 1 + r(4) as c_ulong;
            let textures = (0..width * height).map(|_| tex_handle(r(48) as usize)).collect();
            W.with(|w| {
                let mut w = w.borrow_mut();
                w.set_ids += 1;
                sid = w.set_ids;
                w.sets.push(Set { id: sid, width, height, coverage, textures });
                handle = set_handle(w.sets.len() - 1);
            });
        }
        // SAFETY: the mesh is live
        let id = unsafe { ee_ge_tm_register_texture_set(m, handle as *mut c_void) };
        trace(format!("regset {sid} -> {id} 0"));
        if handle != 0 {
            s.sets.push(id);
        }
    }
    s.color_sets = 1 + r(3) as c_ulong;
    for _ in 0..s.color_sets {
        let n = 1 + r(6);
        let c: Vec<c_ulong> = (0..n).map(|_| r(256) as c_ulong).collect();
        // SAFETY: the mesh is live
        let id = unsafe { ee_ge_tm_register_color_set(m, c.as_ptr(), c.len()) };
        trace(format!("regcolors {id} 0"));
    }
    let n = r(300);
    let table: Vec<[f32; 3]> = (0..n).map(|_| [f(0.0, 1.0), f(0.0, 1.0), f(0.0, 1.0)]).collect();
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_set_color_table(m, table.as_ptr(), table.len()) };
    let cliffs: Vec<*mut c_void> = (0..13).map(|_| tex_handle(r(48) as usize) as *mut c_void).collect();
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_set_space_cliff_textures(m, cliffs.as_ptr(), cliffs.len()) };
    s.w = 1 + r(20) as c_long;
    s.h = 1 + r(20) as c_long;
    // SAFETY: the mesh is live
    unsafe { ee_ge_tm_create_flat_mesh(m, s.w, s.h) };
    trace(format!("flat {} {}", s.w, s.h));
    dump_state(&s);
    let space_map = r(4) == 0;
    let nops = 30 + r(90);
    for _ in 0..nops {
        let kind = r(24);
        match kind {
            0 | 1 => {
                let (x, y) = random_point(&s);
                let z = f(-3.0, 8.0);
                point_edit(m, x, y, 0, 0, z);
                trace(format!("setz {x} {y} {:08x}", bits(z)));
            }
            2 => {
                let (x, y) = random_tile(&s);
                let id = s.sets[r(s.sets.len() as u32) as usize];
                tile_edit(m, x, y, 3, id);
                trace(format!("settex {x} {y} {id}"));
            }
            3 => {
                let (x, y) = random_tile(&s);
                let w = r(2) != 0;
                tile_edit(m, x, y, 5, w as c_ulong);
                trace(format!("setwater {x} {y} {}", w as i32));
            }
            4 => {
                if !space_map && r(3) != 0 {
                } else {
                    let (x, y) = random_tile(&s);
                    let sp = r(3) != 0;
                    tile_edit(m, x, y, 6, sp as c_ulong);
                    trace(format!("setspace {x} {y} {}", sp as i32));
                }
            }
            5 => {
                let (x, y) = random_tile(&s);
                let v = r(0x80) as u8;
                tile_edit(m, x, y, 4, v as c_ulong);
                trace(format!("setinfo {x} {y} {v}"));
            }
            6 => {
                let (x, y) = random_tile(&s);
                match r(3) {
                    0 => {
                        tile_edit(m, x, y, 0, 0);
                        trace(format!("reftile {x} {y}"));
                    }
                    1 => {
                        tile_edit(m, x, y, 1, 0);
                        trace(format!("dereftile {x} {y}"));
                    }
                    _ => {
                        let c = r(4) as c_ulong;
                        tile_edit(m, x, y, 2, c);
                        trace(format!("refcount {x} {y} {c}"));
                    }
                }
            }
            7 => {
                let (x, y) = random_tile(&s);
                tile_edit(m, x, y, 7, 0);
                trace(format!("shore {x} {y} info {}", tile(m, x, y).water_info));
            }
            8 => {
                let (x, y) = random_point(&s);
                point_edit(m, x, y, 4, 0, 0.0);
                trace(format!("normal {x} {y}"));
            }
            9 => {
                let (x, y) = random_point(&s);
                let c = r(256) as u8;
                point_edit(m, x, y, 1, c as c_ulong, 0.0);
                trace(format!("colidx {x} {y} {c}"));
            }
            10 => {
                let (x, y) = random_point(&s);
                let c = r(s.color_sets as u32) as c_ulong;
                point_edit(m, x, y, 2, c, 0.0);
                trace(format!("colset {x} {y} {c} -> {}", point(m, x, y).color_table_index));
            }
            11 => {
                let (x, y) = random_point(&s);
                point_edit(m, x, y, 3, 0, 0.0);
                trace(format!("watercol {x} {y} -> {}", point(m, x, y).color_table_index));
            }
            12 => {
                let set = s.sets[r(s.sets.len() as u32) as usize];
                let col = r(s.color_sets as u32) as c_ulong;
                let period = 1 + r(100) as c_ulong;
                let scale = f(0.5, 4.0);
                // SAFETY: the set and the colours exist
                unsafe { ee_ge_tm_set_global_water_state(m, set, col, period, scale) };
                // SAFETY: the mesh is live
                let handle = unsafe { ee_ge_tm_texture_set(m, set) } as usize;
                s.frames = W.with(|w| {
                    let w = w.borrow();
                    let st = &w.sets[set_index(handle)];
                    st.width * st.height
                });
                trace(format!("waterstate {set} {col} {period} {:08x}", bits(scale)));
            }
            13 | 14 => {
                let px = f(0.0, (s.w + s.h) as f32);
                let py = f(0.0, (s.w + s.h) as f32);
                let pz = f(-2.0, 5.0);
                let (mut tx, mut ty) = (0 as c_long, 0 as c_long);
                // SAFETY: the mesh is live
                unsafe { ee_ge_tm_closest_tile_point(m, &Ge3DPoint { x: px, y: py, z: pz }, &mut tx, &mut ty) };
                trace(format!("closest point {tx} {ty}"));
                let ok = |x: c_long, y: c_long| valid(m, x as c_ulong, y as c_ulong, true);
                if ok(tx, ty) && ok(tx + 1, ty) && ok(tx, ty + 1) {
                    let fx = tx as f32 + f(0.0, 0.99);
                    let fy = ty as f32 + f(0.0, 0.99);
                    let mut n = GeVector::default();
                    // SAFETY: the tile is valid
                    let z = unsafe { ee_ge_tm_mesh_z(m, fx, fy) };
                    // SAFETY: as above
                    unsafe { ee_ge_tm_mesh_normal(m, fx, fy, &mut n) };
                    trace(format!("meshz {:08x} n {:08x} {:08x} {:08x}", bits(z), bits(n.x), bits(n.y), bits(n.z)));
                }
                let l0 = px + f(-5.0, 5.0);
                let l1 = py + f(-5.0, 5.0);
                let l2 = f(5.0, 30.0);
                let d0 = f(-10.0, 10.0);
                let d1 = f(-10.0, 10.0);
                let d2 = -f(5.0, 40.0);
                let line =
                    Ge3DLine { start: Ge3DPoint { x: l0, y: l1, z: l2 }, direction: GeVector { x: d0, y: d1, z: d2 } };
                // SAFETY: the mesh is live
                unsafe { ee_ge_tm_closest_tile_line(m, &line, &mut tx, &mut ty) };
                let mut d = -7.0f32;
                // SAFETY: as above
                let hit = unsafe { ee_ge_tm_intersects(m, &line, &mut d) };
                trace(format!("line {tx} {ty} hit {} {:08x}", hit as i32, bits(d)));
                let nv = 3 + r(6);
                let mut vol = Vec::new();
                for _ in 0..nv {
                    let v0 = f(-3.0, (s.w + s.h + 3) as f32);
                    let v1 = f(-3.0, (s.w + s.h + 3) as f32);
                    vol.push(Ge3DPoint { x: v0, y: v1, z: 0.0 });
                }
                let mut rows = vec![0 as c_long; 3 * get(m, EeGeTmVal::TileRowCount) as usize];
                let mut b = [0 as c_long; 4];
                // SAFETY: the buffers hold the sizes
                let n =
                    unsafe { ee_ge_tm_rows_in_volume(m, vol.as_ptr(), vol.len(), rows.as_mut_ptr(), b.as_mut_ptr()) };
                trace(format!("volume {n} bounds {} {} {} {}", b[0], b[1], b[2], b[3]));
                for k in 0..n {
                    trace(format!("vrow {} {} {}", rows[3 * k], rows[3 * k + 1], rows[3 * k + 2]));
                }
            }
            15 | 16 => {
                let (v, ortho) = make_viewport(&s);
                // SAFETY: the mesh has rows
                unsafe { ee_ge_tm_update_visible_terrain(m, &v, ortho) };
                trace(format!("visible {}", ortho as i32));
                dump_frame(&s);
                dump_arrays(&s);
            }
            17 => {
                let all = r(3) == 0;
                // SAFETY: as above
                unsafe { ee_ge_tm_update_fog_of_war(m, all) };
                trace("fog".to_string());
                dump_arrays(&s);
            }
            18 => {
                let dt = r(300) as c_ulong;
                W.with(|w| w.borrow_mut().time += dt);
            }
            19 => {
                let mut file = MemFile { data: Vec::new(), pos: 0, limit: usize::MAX };
                if r(4) == 0 {
                    file.limit = r(200) as usize;
                }
                // SAFETY: the file is live
                let result = unsafe { ee_ge_tm_save(m, &io_of(&mut file)) };
                trace(format!("save {result} size {}", file.data.len()));
                let mut h = FNV;
                fnv(&mut h, &file.data);
                trace(format!("file {h:016x}"));
                if result == 0 {
                    if r(3) == 0 {
                        let keep = r(file.data.len() as u32) as usize;
                        file.data.truncate(keep);
                    }
                    file.pos = 0;
                    // SAFETY: as above
                    let lr = unsafe { ee_ge_tm_load(m, &io_of(&mut file)) };
                    trace(format!("load {lr} at {}", file.pos));
                    dump_state(&s);
                }
            }
            20 => {
                let p = f(0.0, 1.0);
                // SAFETY: the mesh is live
                unsafe { ee_ge_tm_set_water_percentage(m, p) };
                trace(format!("waterpct {:08x}", bits(p)));
            }
            21 => {
                for _ in 0..6 {
                    let id = r(6) as c_ulong;
                    let set = if id < nsets as c_ulong {
                        // SAFETY: the mesh is live
                        unsafe { ee_ge_tm_texture_set(m, id) as usize }
                    } else {
                        0
                    };
                    let sid = if set == 0 { 0 } else { W.with(|w| w.borrow().sets[set_index(set)].id) };
                    trace(format!("getset {id} {sid}"));
                }
            }
            _ => dump_state(&s),
        }
        if (kind == 12 || kind == 18) && s.frames > 0 && get(m, EeGeTmVal::WaterMaterialCount) >= s.frames {
            let dt = r(400) as c_ulong;
            W.with(|w| w.borrow_mut().time += dt);
            // SAFETY: the mesh is live
            unsafe { ee_ge_tm_update_water_animation(m) };
            trace("wateranim".to_string());
            for k in 0..get(m, EeGeTmVal::WaterMaterialCount) {
                trace(format!("watermat {k} {}", tex_id(material(m, true, k).texture as usize)));
            }
        }
    }
    dump_state(&s);
    // SAFETY: the mesh is freed once
    unsafe { ee_ge_tm_destroy(m) };
    trace("end".to_string());
}

#[test]
fn the_golden_sequences_replay_with_the_same_digests() {
    let _lock = super::tests::WATER_CLOCK.lock().unwrap_or_else(|e| e.into_inner());
    WATER_LAST_TIME.store(0, Ordering::Relaxed);
    let mut out = String::new();
    let mut mismatches = Vec::new();
    for (seq, line) in GOLDEN.lines().take(SEQUENCES).enumerate() {
        W.with(|w| w.borrow_mut().hash = FNV);
        run_sequence(seq as i32);
        let h = W.with(|w| w.borrow().hash);
        let got = format!("{seq} {h:016x}");
        writeln!(out, "{got}").unwrap();
        if got != line.trim() {
            mismatches.push(format!("{got} (golden {line})"));
        }
    }
    assert!(mismatches.is_empty(), "{} of {SEQUENCES} differ: {mismatches:?}", mismatches.len());
}
