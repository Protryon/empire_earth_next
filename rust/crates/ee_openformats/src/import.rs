//! glTF -> the model the game stores: the importer of GEOpenFormats.cpp (gltf.py `import_`, docs/MODEL_FORMATS.md
//! 4.3), operation by operation. Floats are combined in double precision and rounded to f32 where the Python side rounds, so that the
//! three implementations give the same bytes; Rust's `f64 as f32` is a correctly rounded IEEE conversion on every
//! target this builds for (no x87, no fused multiply-add).
#![allow(clippy::needless_range_loop)]

use std::collections::HashMap;

use crate::cgltf::{
    self, Accessor, AttributeKind, ComponentType, Data, Interpolation, Node, PrimitiveKind, Ref, TargetPath, ValueType,
};
use crate::json::{self, Value};
use crate::{fail, Failure};
use ee_ge::model_store::{CemFrame, CemMaterial, CemModel, CemPolygons, Counts, SharedFrames};
use ee_ge::types::{Ge3DPoint, GeVector};

/// `kErrorOK` and `kErrorFailed` (`LowLevelEngineCore.h`).
const OK: std::ffi::c_long = 0;
const FAILED: std::ffi::c_long = 9;

fn f32r(v: f64) -> f32 {
    v as f32
}

type Matrix = [f64; 16];

const IDENTITY: Matrix = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0];
// the Z-up -> Y-up wrapper node eemodel writes above the model (gltf.py AXES)
const AXES: Matrix = [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0];
const FRAMES_PER_SECOND: f64 = 10.0; // gltf.py FPS: frame i is keyframe time i / 10

fn mat_mul(a: &Matrix, b: &Matrix) -> Matrix {
    let mut out = [0.0; 16];
    for c in 0..4 {
        for r in 0..4 {
            let mut s = 0.0;
            for k in 0..4 {
                s += a[k * 4 + r] * b[c * 4 + k];
            }
            out[c * 4 + r] = s;
        }
    }
    out
}

// gltf.py compose: (translation, quaternion xyzw, scale) -> column-major matrix of float32 values
fn compose(t: [f64; 3], q: [f64; 4], s: [f64; 3]) -> Matrix {
    let (x, y, z, w) = (q[0], q[1], q[2], q[3]);
    let r = [
        [1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w), 2.0 * (x * z + y * w)],
        [2.0 * (x * y + z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w)],
        [2.0 * (x * z - y * w), 2.0 * (y * z + x * w), 1.0 - 2.0 * (x * x + y * y)],
    ];
    let mut g = [0.0; 16];
    for c in 0..3 {
        for i in 0..3 {
            g[c * 4 + i] = f32r(r[i][c] * s[c]) as f64;
        }
    }
    g[12] = f32r(t[0]) as f64;
    g[13] = f32r(t[1]) as f64;
    g[14] = f32r(t[2]) as f64;
    g[15] = 1.0;
    g
}

fn trs(n: &Node) -> ([f64; 3], [f64; 4], [f64; 3]) {
    let t = if n.has_translation { n.translation.map(|v| v as f64) } else { [0.0; 3] };
    let s = if n.has_scale { n.scale.map(|v| v as f64) } else { [1.0; 3] };
    let r = if n.has_rotation { n.rotation.map(|v| v as f64) } else { [0.0, 0.0, 0.0, 1.0] };
    (t, r, s)
}

// gltf.py _local
fn local(n: &Node) -> Matrix {
    if n.has_matrix {
        return n.matrix.map(|v| v as f64);
    }
    let (t, r, s) = trs(n);
    if t == [0.0; 3] && r[0] == 0.0 && r[1] == 0.0 && r[2] == 0.0 && r[3] == 1.0 && s == [1.0; 3] {
        return IDENTITY;
    }
    compose(t, r, s)
}

fn is_signed_permutation(g: &Matrix) -> bool {
    g.iter().all(|v| *v == 0.0 || *v == 1.0 || *v == -1.0)
        && g[12] == 0.0
        && g[13] == 0.0
        && g[14] == 0.0
        && g[15] == 1.0
}

// CP-1252 for the bytes 0x80..0x9f (eedata's mapping: the five undefined ones are their C1 code points)
const CP1252_HIGH: [u32; 32] = [
    0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021, 0x2c6, 0x2030, 0x160, 0x2039, 0x152, 0x8d, 0x17d,
    0x8f, 0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d,
    0x17e, 0x178,
];

fn bad_json_string(raw: &[u8]) -> Failure {
    let mut m = b"bad JSON string \"".to_vec();
    m.extend_from_slice(raw);
    m.push(b'"');
    Failure(m)
}

fn not_utf8(s: &[u8]) -> Failure {
    let mut m = b"name \"".to_vec();
    m.extend_from_slice(s);
    m.extend_from_slice(b"\" is not UTF-8");
    Failure(m)
}

// gltf.py _name: Blender's ".NNN" duplicate suffix dropped, UTF-8 -> CP-1252. A missing name is empty.
fn game_name(raw: Option<&[u8]>) -> Result<Vec<u8>, Failure> {
    let raw = raw.unwrap_or(b"");
    // the C API's string: up to a NUL
    let end = raw.iter().position(|&c| c == 0).unwrap_or(raw.len());
    let raw = &raw[..end];
    let mut s = json::unescape(raw).ok_or_else(|| bad_json_string(raw))?;
    let n = s.len();
    if n >= 4 && s[n - 4] == b'.' && s[n - 3..].iter().all(u8::is_ascii_digit) {
        s.truncate(n - 4);
    }
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while i < s.len() {
        let c = s[i];
        let (mut code, len) = if c < 0x80 {
            (c as u32, 1)
        } else if (c & 0xe0) == 0xc0 {
            ((c & 0x1f) as u32, 2)
        } else if (c & 0xf0) == 0xe0 {
            ((c & 0x0f) as u32, 3)
        } else if (c & 0xf8) == 0xf0 {
            ((c & 0x07) as u32, 4)
        } else {
            return Err(not_utf8(&s));
        };
        if i + len > s.len() {
            return Err(not_utf8(&s));
        }
        for k in 1..len {
            if s[i + k] & 0xc0 != 0x80 {
                return Err(not_utf8(&s));
            }
            code = (code << 6) | (s[i + k] & 0x3f) as u32;
        }
        i += len;
        let byte = if code < 0x80 || (0xa0..=0xff).contains(&code) {
            Some(code as u8)
        } else {
            CP1252_HIGH.iter().position(|&c| c == code).map(|k| 0x80 + k as u8)
        };
        match byte {
            Some(b) => out.push(b),
            None => {
                let mut m = b"name \"".to_vec();
                m.extend_from_slice(&s);
                m.extend_from_slice(b"\" has characters outside CP-1252");
                return Err(Failure(m));
            }
        }
    }
    Ok(out)
}

struct Sampler {
    times: Vec<f32>,
    values: Vec<f32>,
    mode: Interpolation,
}

impl Sampler {
    // gltf.py _Sampler.at
    fn at(&self, time: f64, nc: usize) -> Result<Vec<f64>, Failure> {
        let cubic = self.mode == Interpolation::CubicSpline;
        let step = if cubic { 3 * nc } else { nc };
        let offset = if cubic { nc } else { 0 };
        let t = &self.times;
        if t.is_empty() || self.values.len() < t.len() * step {
            return fail("animation sampler output shorter than its input");
        }
        let key = |i: usize| -> Vec<f64> { (0..nc).map(|c| self.values[i * step + offset + c] as f64).collect() };
        if time <= t[0] as f64 || t.len() == 1 {
            return Ok(key(0));
        }
        if time >= *t.last().unwrap() as f64 {
            return Ok(key(t.len() - 1));
        }
        let mut i = 0;
        while i + 1 < t.len() && t[i + 1] as f64 <= time {
            i += 1;
        }
        if t[i] as f64 == time || self.mode == Interpolation::Step {
            return Ok(key(i));
        }
        let (a, b) = (key(i), key(i + 1));
        let u = (time - t[i] as f64) / (t[i + 1] as f64 - t[i] as f64);
        Ok((0..nc).map(|c| a[c] + (b[c] - a[c]) * u).collect())
    }
}

#[derive(Default)]
struct Material {
    name: Vec<u8>,
    texture: Vec<u8>,
    texture_index: u32,
    ranges: Vec<(u32, u32)>,
    vertex_start: u32,
    vertex_count: u32,
}

#[derive(Default)]
struct Model {
    name: Vec<u8>,
    center: [f32; 3],
    polygon_count: u32,
    lods: Vec<Vec<u32>>,
    materials: Vec<Material>,
    tag_names: Vec<Vec<u8>>,
    radii: Vec<f32>,
    vertices: Vec<Vec<f32>>,    // per frame: 8 floats per vertex
    tags: Vec<Vec<f32>>,        // per frame: 3 floats per tag
    transforms: Vec<[f32; 16]>, // per frame: mMatrix rows
    boxes: Vec<[f32; 6]>,
    children: Vec<Model>,
}

fn floats(d: &Data, a: &Accessor) -> Result<Vec<f32>, Failure> {
    d.floats(a).ok_or_else(|| Failure(b"an accessor cannot be read".to_vec()))
}

fn ints(d: &Data, a: &Accessor) -> Result<Vec<u64>, Failure> {
    // integers below 2^24 are exact; gltf.py reads them the same way. The sources are u8/u16/u32 (checked by the
    // caller), so the float-to-integer cast never sees a negative or out-of-range value.
    Ok(floats(d, a)?.iter().map(|&f| f as u64).collect())
}

// gltf.py compute_bbox
fn bounding_box(v: &[f32], vc: usize) -> [f32; 6] {
    let mut b = [0.0f32; 6];
    for c in 0..3 {
        b[c] = if v.len() >= 3 { v[c] } else { 0.0 };
        b[c + 3] = b[c];
    }
    for j in 0..vc {
        for c in 0..3 {
            let x = v[8 * j + c];
            if x < b[c] {
                b[c] = x;
            } else if x > b[c + 3] {
                b[c + 3] = x;
            }
        }
    }
    b
}

fn float_list(v: Option<&Value>, n: usize) -> Option<Vec<f64>> {
    let Value::Array(items) = v? else { return None };
    if items.len() != n {
        return None;
    }
    let mut out = Vec::with_capacity(n);
    for item in items {
        if !item.is_number() {
            return None;
        }
        out.push(f32r(item.number()) as f64);
    }
    Some(out)
}

fn pair_list(r: Option<&Value>, n: usize) -> bool {
    let Some(Value::Array(items)) = r else { return false };
    if items.len() != n {
        return false;
    }
    items.iter().all(|p| {
        matches!(p, Value::Array(pair) if pair.len() == 2 && pair[0].index(1 << 32).is_some() && pair[1].index(1 << 32).is_some())
    })
}

fn pair_at(p: &Value, i: usize) -> u64 {
    let Value::Array(pair) = p else { return 0 };
    pair[i].index(1 << 32).unwrap_or(0)
}

fn texture_name(d: &Data, m: Option<&cgltf::Material>) -> Result<Vec<u8>, Failure> {
    let Some(m) = m else { return Ok(Vec::new()) };
    if !m.has_pbr || m.base_color_texture == 0 {
        return Ok(Vec::new());
    }
    let tex = &d.textures[m.base_color_texture as usize - 1];
    if tex.image == 0 {
        return Ok(Vec::new());
    }
    let img = &d.images[tex.image as usize - 1];
    if let Some(name) = img.name.as_deref().filter(|n| !n.is_empty()) {
        return game_name(Some(name));
    }
    if let Some(uri) = img.uri.as_deref().filter(|u| !u.is_empty() && !u.starts_with(b"data:")) {
        let raw = json::unescape(uri).ok_or_else(|| bad_json_string(uri))?;
        let mut s = cgltf::decode_uri(&raw);
        if let Some(slash) = s.iter().rposition(|&c| c == b'/' || c == b'\\') {
            s.drain(..=slash);
        }
        if let Some(dot) = s.iter().rposition(|&c| c == b'.') {
            s.truncate(dot);
        }
        // the C++ passes the decoded name through its raw-name path once more
        return game_name(Some(&s));
    }
    Ok(Vec::new())
}

fn extension_data(d: &Data, n: &Node) -> Result<Option<Value>, Failure> {
    for ext in &n.extensions {
        if ext.name == b"EE_model" {
            let text = &d.json[ext.data.0 as usize..ext.data.1 as usize];
            let Some(v) = json::parse(text) else { return fail("EE_model is not valid JSON") };
            return Ok(if matches!(v, Value::Object(_)) { Some(v) } else { None });
        }
    }
    Ok(None)
}

fn attribute(attrs: &[cgltf::Attribute], kind: AttributeKind) -> Option<&cgltf::Attribute> {
    attrs.iter().find(|a| a.kind == kind && a.index == 0)
}

// a LOD beyond the first: per-material (first, count) ranges and its indices
type ExtraLod = (Vec<(u32, u32)>, Vec<u64>);

struct Importer<'a, 'b> {
    data: &'b Data<'a>,
    channels: HashMap<(usize, TargetPath), Sampler>,
}

impl Importer<'_, '_> {
    fn accessor(&self, r: Ref) -> &Accessor {
        &self.data.accessors[r as usize - 1]
    }

    fn import_model(&self, node_index: usize, correction: Option<&Matrix>) -> Result<Model, Failure> {
        let d = self.data;
        let node = &d.nodes[node_index];
        let ext = extension_data(d, node)?;
        let ext_get = |k: &str| ext.as_ref().and_then(|e| e.get(k));
        let mut m = Model { name: game_name(node.name.as_deref())?, ..Default::default() };
        let mesh = &d.meshes[node.mesh as usize - 1];
        if mesh.primitives.is_empty() {
            return fail("a mesh without primitives");
        }
        let ntargets = mesh.primitives[0].targets.len();
        let fc = ntargets + 1;
        let nmat = mesh.primitives.len();
        let mut counts = vec![0usize; nmat];
        let mut pverts: Vec<Vec<Vec<f32>>> = vec![Vec::new(); nmat];
        let mut pidx: Vec<Vec<u64>> = vec![Vec::new(); nmat];
        for pi in 0..nmat {
            let p = &mesh.primitives[pi];
            if p.kind != PrimitiveKind::Triangles {
                return fail("only triangle lists are supported");
            }
            if p.targets.len() != ntargets {
                return fail("primitives with different morph target counts");
            }
            let Some(pa) = attribute(&p.attributes, AttributeKind::Position) else {
                return fail("a primitive without POSITION");
            };
            let pos = floats(d, self.accessor(pa.accessor))?;
            let n = pos.len() / 3;
            let nor = match attribute(&p.attributes, AttributeKind::Normal) {
                Some(a) => floats(d, self.accessor(a.accessor))?,
                None => vec![0.0; 3 * n],
            };
            let uv = match attribute(&p.attributes, AttributeKind::Texcoord) {
                Some(a) => floats(d, self.accessor(a.accessor))?,
                None => vec![0.0; 2 * n],
            };
            if nor.len() != 3 * n || uv.len() != 2 * n {
                return fail("attribute counts differ");
            }
            counts[pi] = n;
            pverts[pi].resize(fc, Vec::new());
            for f in 0..fc {
                let (mut fp, mut fn_) = (pos.clone(), nor.clone());
                if f > 0 {
                    let t = &p.targets[f - 1];
                    let dp = match attribute(t, AttributeKind::Position) {
                        Some(a) => floats(d, self.accessor(a.accessor))?,
                        None => vec![0.0; 3 * n],
                    };
                    let dn = match attribute(t, AttributeKind::Normal) {
                        Some(a) => floats(d, self.accessor(a.accessor))?,
                        None => vec![0.0; 3 * n],
                    };
                    if dp.len() != 3 * n || dn.len() != 3 * n {
                        return fail("morph target counts differ");
                    }
                    for i in 0..3 * n {
                        fp[i] = f32r(pos[i] as f64 + dp[i] as f64);
                        fn_[i] = f32r(nor[i] as f64 + dn[i] as f64);
                    }
                }
                let v = &mut pverts[pi][f];
                v.reserve_exact(8 * n);
                for i in 0..n {
                    v.extend_from_slice(&fp[3 * i..3 * i + 3]);
                    v.extend_from_slice(&fn_[3 * i..3 * i + 3]);
                    v.extend_from_slice(&uv[2 * i..2 * i + 2]);
                }
            }
            if p.indices != 0 {
                let a = self.accessor(p.indices);
                pidx[pi] = d.index_values(a).ok_or_else(|| Failure(b"an accessor cannot be read".to_vec()))?;
            } else {
                pidx[pi] = (0..n as u64).collect();
            }
            if !pidx[pi].len().is_multiple_of(3) {
                return fail("an index count that is not a multiple of 3");
            }
            if pidx[pi].iter().any(|&i| i >= n as u64) {
                return fail("an index out of range");
            }
        }
        // vertex layout: EE_model's vertex starts when valid and consistent, else one range after the other
        let mut starts: Vec<u32> = Vec::new();
        let mut layout: Vec<(usize, usize)> = Vec::new();
        let mut laid_out = false;
        if let Some(Value::Array(items)) = ext_get("vertexStarts") {
            if items.len() == nmat && items.iter().all(|v| v.index(1 << 32).is_some()) {
                let at = |i: usize| items[i].index(1 << 32).unwrap();
                let mut vc = 0u64;
                for i in 0..nmat {
                    vc = vc.max(at(i) + counts[i] as u64);
                }
                let mut slots: Vec<(usize, usize)> = Vec::new();
                slots
                    .try_reserve_exact(vc as usize)
                    .map_err(|_| Failure(b"EE_model vertexStarts too large".to_vec()))?;
                slots.resize(vc as usize, (usize::MAX, 0));
                let mut ok = true;
                'prims: for pi in 0..nmat {
                    let s = at(pi) as usize;
                    for i in 0..counts[pi] {
                        let slot = slots[s + i];
                        if slot.0 == usize::MAX {
                            slots[s + i] = (pi, i);
                        } else {
                            for f in 0..fc {
                                let a = &pverts[slot.0][f][8 * slot.1..8 * slot.1 + 8];
                                let b = &pverts[pi][f][8 * i..8 * i + 8];
                                if a.iter().zip(b).any(|(x, y)| x.to_bits() != y.to_bits()) {
                                    ok = false;
                                    break 'prims;
                                }
                            }
                        }
                    }
                }
                if ok && slots.iter().any(|s| s.0 == usize::MAX) {
                    ok = false;
                }
                if ok {
                    layout = slots;
                    starts = (0..nmat).map(|i| at(i) as u32).collect();
                    laid_out = true;
                }
            }
        }
        if !laid_out {
            for pi in 0..nmat {
                starts.push(layout.len() as u32);
                for i in 0..counts[pi] {
                    layout.push((pi, i));
                }
            }
        }
        let vc = layout.len();
        for f in 0..fc {
            let mut v = Vec::with_capacity(8 * vc);
            for &(pi, i) in &layout {
                v.extend_from_slice(&pverts[pi][f][8 * i..8 * i + 8]);
            }
            m.vertices.push(v);
        }
        // frame fixes
        if let Some(Value::Array(items)) = ext_get("frameFixes") {
            for (k, acc) in items.iter().enumerate() {
                let fi = k + 1;
                if matches!(acc, Value::Null) || fi >= fc {
                    continue;
                }
                let Some(ai) = acc.index(d.accessors.len() as u64) else {
                    return fail("EE_model frameFixes does not match the vertices");
                };
                let a = &d.accessors[ai as usize];
                if !a.is_sparse
                    || a.count != 6 * vc as u64
                    || a.ty != ValueType::Scalar
                    || a.component != ComponentType::F32
                {
                    return fail("EE_model frameFixes does not match the vertices");
                }
                let values = floats(d, a)?;
                let Some((size, base)) = d.sparse_index_bytes(a) else {
                    return fail("EE_model frameFixes does not match the vertices");
                };
                for j in 0..a.sparse.count as usize {
                    let mut idx = 0usize;
                    for b in 0..size {
                        let Some(v) = base.get(j * size + b) else {
                            return fail("EE_model frameFixes does not match the vertices");
                        };
                        idx |= (*v as usize) << (8 * b);
                    }
                    // cgltf_validate checked the sparse indices against the count (6 per vertex)
                    let slot = 8 * (idx / 6) + idx % 6;
                    if idx >= values.len() || slot >= m.vertices[fi].len() {
                        return fail("EE_model frameFixes does not match the vertices");
                    }
                    m.vertices[fi][slot] = values[idx];
                }
            }
        }
        // LOD 0
        let jr0 = ext_get("lod0Ranges");
        let mut have_r0 = pair_list(jr0, nmat);
        if have_r0 {
            if let Some(Value::Array(items)) = jr0 {
                for i in 0..nmat {
                    have_r0 = have_r0 && pair_at(&items[i], 1) == (pidx[i].len() / 3) as u64;
                }
            }
        }
        let mut r0: Vec<(u32, u32)> = Vec::with_capacity(nmat);
        if have_r0 {
            if let Some(Value::Array(items)) = jr0 {
                for i in 0..nmat {
                    r0.push((pair_at(&items[i], 0) as u32, pair_at(&items[i], 1) as u32));
                }
            }
        } else {
            let mut cum = 0u32;
            for i in 0..nmat {
                let tris = (pidx[i].len() / 3) as u32;
                r0.push((cum, tris));
                cum = cum.wrapping_add(tris);
            }
        }
        let mut total0 = 0u64;
        for i in 0..nmat {
            total0 = total0.max(r0[i].0 as u64 + r0[i].1 as u64);
        }
        if total0 > 0x1000_0000 {
            return fail("EE_model lod0Ranges too large");
        }
        let mut lod0 = vec![0u32; 3 * total0 as usize];
        for i in 0..nmat {
            for (j, v) in pidx[i].iter().enumerate() {
                lod0[3 * r0[i].0 as usize + j] = *v as u32;
            }
        }
        m.lods.push(lod0);
        // LODs 1..: all of EE_model's or none
        let mut extra: Vec<ExtraLod> = Vec::new();
        if let Some(Value::Array(lods)) = ext_get("lods") {
            let mut ok = true;
            for lod in lods {
                let (rr, mut ia) = (lod.get("ranges"), lod.get("indices"));
                if matches!(ia, Some(Value::Null)) {
                    ia = None;
                }
                ok = pair_list(rr, nmat);
                let mut accessor = None;
                if ok {
                    if let Some(iv) = ia {
                        ok = false;
                        if let Some(ai) = iv.index(d.accessors.len() as u64) {
                            let a = &d.accessors[ai as usize];
                            if a.ty == ValueType::Scalar
                                && matches!(a.component, ComponentType::U8 | ComponentType::U16 | ComponentType::U32)
                            {
                                ok = true;
                                accessor = Some(a);
                            }
                        }
                    }
                }
                if !ok {
                    break;
                }
                let arr = match accessor {
                    Some(a) => ints(d, a)?,
                    None => Vec::new(),
                };
                let mut ranges = Vec::with_capacity(nmat);
                if let Some(Value::Array(rs)) = rr {
                    for i in 0..nmat {
                        if !ok {
                            break;
                        }
                        let (s, c) = (pair_at(&rs[i], 0), pair_at(&rs[i], 1));
                        ok = 3 * (s + c) <= arr.len() as u64;
                        let mut j = 3 * s;
                        while j < 3 * (s + c) && ok {
                            ok = arr[j as usize] < counts[i] as u64;
                            j += 1;
                        }
                        ranges.push((s as u32, c as u32));
                    }
                }
                if !ok {
                    break;
                }
                extra.push((ranges, arr));
            }
            if !ok {
                extra.clear();
            }
        }
        for (_, arr) in &extra {
            m.lods.push(arr.iter().map(|&v| v as u32).collect());
        }
        for pi in 0..nmat {
            let p = &mesh.primitives[pi];
            let mat = if p.material != 0 { Some(&d.materials[p.material as usize - 1]) } else { None };
            let mut material = Material::default();
            if let Some(mt) = mat {
                if let Some((s, e)) = mt.extras {
                    let Some(ex) = json::parse(&d.json[s as usize..e as usize]) else {
                        return fail("material extras are not valid JSON");
                    };
                    if let Some(ti) = ex.get("ee_textureIndex") {
                        match ti.index(1 << 32) {
                            Some(v) => material.texture_index = v as u32,
                            None => return fail("ee_textureIndex is not an integer 0..2^32-1"),
                        }
                    }
                }
            }
            material.name = game_name(mat.and_then(|mt| mt.name.as_deref()))?;
            material.texture = texture_name(d, mat)?;
            material.ranges.push(r0[pi]);
            for (ranges, _) in &extra {
                material.ranges.push(ranges[pi]);
            }
            material.vertex_start = starts[pi];
            material.vertex_count = counts[pi] as u32;
            m.materials.push(material);
        }
        m.polygon_count = ext_get("polygonCount").and_then(|p| p.index(1 << 32)).map_or(total0 as u32, |p| p as u32);
        // children: sub-models (with a mesh) and tag points (without)
        let (subs, tags): (Vec<_>, Vec<_>) =
            node.children.iter().map(|c| *c as usize - 1).partition(|&c| d.nodes[c].mesh != 0);
        let times: Vec<f64> = (0..fc).map(|f| f32r(f as f64 / FRAMES_PER_SECOND) as f64).collect();
        m.tags = vec![Vec::new(); fc];
        for &t in &tags {
            let tag = &d.nodes[t];
            m.tag_names.push(game_name(tag.name.as_deref())?);
            let rest = local(tag);
            let channel = self.channels.get(&(t, TargetPath::Translation));
            for f in 0..fc {
                let p = match channel {
                    Some(s) => s.at(times[f], 3)?,
                    None => rest[12..15].to_vec(),
                };
                for c in 0..3 {
                    m.tags[f].push(f32r(p[c]));
                }
            }
        }
        // frame transforms
        let jt = ext_get("transforms");
        let mut tf: Vec<Matrix> = Vec::new();
        let mut exact = matches!(jt, Some(Value::Array(a)) if a.len() == fc);
        if exact {
            let Some(Value::Array(items)) = jt else { unreachable!() };
            for f in 0..fc {
                match float_list(Some(&items[f]), 16) {
                    Some(v) => {
                        let mut mtx = [0.0; 16];
                        mtx.copy_from_slice(&v);
                        tf.push(mtx);
                    }
                    None => {
                        exact = false;
                        break;
                    }
                }
            }
        }
        if !exact {
            tf.clear();
            let ts = self.channels.get(&(node_index, TargetPath::Translation));
            let rs = self.channels.get(&(node_index, TargetPath::Rotation));
            let ss = self.channels.get(&(node_index, TargetPath::Scale));
            if ts.is_some() || rs.is_some() || ss.is_some() {
                let (t0, r0q, s0) = trs(node);
                for f in 0..fc {
                    let t = match ts {
                        Some(s) => to3(s.at(times[f], 3)?),
                        None => t0,
                    };
                    let r = match rs {
                        Some(s) => to4(s.at(times[f], 4)?),
                        None => r0q,
                    };
                    let s = match ss {
                        Some(s) => to3(s.at(times[f], 3)?),
                        None => s0,
                    };
                    tf.push(compose(t, r, s));
                }
            } else {
                tf = vec![local(node); fc];
            }
        }
        if let Some(corr) = correction {
            for f in 0..fc {
                tf[f] = mat_mul(corr, &tf[f]);
                for i in 0..16 {
                    tf[f][i] = f32r(tf[f][i]) as f64;
                }
            }
        }
        for f in 0..fc {
            let mut rows = [0.0f32; 16];
            for r in 0..4 {
                for c in 0..4 {
                    rows[r * 4 + c] = f32r(tf[f][c * 4 + r]);
                }
            }
            m.transforms.push(rows);
            m.boxes.push(bounding_box(&m.vertices[f], vc));
        }
        match float_list(ext_get("center"), 3) {
            Some(c) => {
                for i in 0..3 {
                    m.center[i] = c[i] as f32;
                }
            }
            None => {
                // gltf.py compute_center
                let s = f32r(1.0 / f32r(fc as f64 * 2.0) as f64);
                let mut acc = [0.0f32; 3];
                for f in 0..fc {
                    for c in 0..3 {
                        acc[c] = f32r(acc[c] as f64 + f32r(m.boxes[f][c] as f64 + m.boxes[f][c + 3] as f64) as f64);
                    }
                }
                for c in 0..3 {
                    m.center[c] = f32r(s as f64 * acc[c] as f64);
                }
            }
        }
        match float_list(ext_get("radii"), fc) {
            Some(r) => m.radii = r.iter().map(|v| *v as f32).collect(),
            None => {
                for f in 0..fc {
                    // gltf.py compute_radii
                    let mut best = 0.0f64;
                    let v = &m.vertices[f];
                    for j in 0..vc {
                        let mut dist = [0.0f64; 3];
                        for c in 0..3 {
                            let e = f32r(v[8 * j + c] as f64 - m.center[c] as f64) as f64;
                            dist[c] = f32r(e * e) as f64;
                        }
                        let distance = f32r(f32r(dist[0] + dist[1]) as f64 + dist[2]) as f64;
                        if distance > best {
                            best = distance;
                        }
                    }
                    m.radii.push(f32r(best.sqrt()));
                }
            }
        }
        for &c in &subs {
            m.children.push(self.import_model(c, None)?);
        }
        Ok(m)
    }
}

fn to3(v: Vec<f64>) -> [f64; 3] {
    [v[0], v[1], v[2]]
}

fn to4(v: Vec<f64>) -> [f64; 4] {
    [v[0], v[1], v[2], v[3]]
}

/// Reads an external buffer a glTF names (its uri, percent-decoded): the bytes, or `None` when it cannot be found.
pub type Resolver<'r> = dyn FnMut(&[u8]) -> Option<Vec<u8>> + 'r;

/// A .gltf or .glb -> the imported model (gltf.py `import_`).
fn import(input: &[u8], mut resolve: Option<&mut Resolver>) -> Result<Model, Failure> {
    let Ok(mut d) = cgltf::parse(input) else { return fail("not a glTF 2.0 file") };
    for b in d.buffers.iter_mut() {
        let Some(uri) = b.uri.as_deref() else { continue };
        if b.data.is_some() || uri.starts_with(b"data:") {
            continue;
        }
        let name = cgltf::decode_uri(uri);
        let mut quoted = b"buffer \"".to_vec();
        quoted.extend_from_slice(&name);
        let missing = |quoted: &mut Vec<u8>, tail: &[u8]| {
            quoted.extend_from_slice(tail);
            Failure(std::mem::take(quoted))
        };
        let Some(bytes) = resolve.as_deref_mut().and_then(|r| r(&name)) else {
            return Err(missing(&mut quoted, b"\" not found"));
        };
        if (bytes.len() as u64) < b.size {
            return Err(missing(&mut quoted, b"\" is shorter than its byteLength"));
        }
        // an empty file stays a null pointer in the C++: cgltf_load_buffers then fails on it, below
        b.data = if bytes.is_empty() { None } else { Some(std::borrow::Cow::Owned(bytes)) };
    }
    if cgltf::load_buffers(&mut d).is_err() {
        return fail("the glTF's buffers cannot be loaded");
    }
    if cgltf::validate(&d).is_err() {
        return fail("the glTF is not valid (cgltf_validate)");
    }
    let mut importer = Importer { data: &d, channels: HashMap::new() };
    // the root model: the first node with a mesh, depth first from the scene's roots
    let scene = if d.scene != 0 { Some(&d.scenes[d.scene as usize - 1]) } else { d.scenes.first() };
    let mut stack: Vec<usize> = match scene {
        Some(s) => s.nodes.iter().rev().map(|r| *r as usize - 1).collect(),
        None => (0..d.nodes.len()).rev().filter(|&i| d.nodes[i].parent.is_none()).collect(),
    };
    let mut root = None;
    while let Some(n) = stack.pop() {
        if d.nodes[n].mesh != 0 {
            root = Some(n);
            break;
        }
        stack.extend(d.nodes[n].children.iter().rev().map(|r| *r as usize - 1));
    }
    let Some(root) = root else { return fail("no node with a mesh") };
    let mut above = IDENTITY;
    let mut k = d.nodes[root].parent;
    while let Some(p) = k {
        above = mat_mul(&local(&d.nodes[p]), &above);
        k = d.nodes[p].parent;
    }
    let mut correction = None;
    if above != AXES {
        let mut inverse = [0.0; 16];
        for r in 0..4 {
            for c in 0..4 {
                inverse[r * 4 + c] = AXES[c * 4 + r];
            }
        }
        let cm = mat_mul(&inverse, &above);
        if !is_signed_permutation(&cm) {
            return fail("the transform above the root model is not an axis change");
        }
        correction = Some(cm);
    }
    if let Some(an) = d.animations.first() {
        for ch in &an.channels {
            if ch.target_node == 0 || ch.sampler == 0 {
                continue;
            }
            let s = &an.samplers[ch.sampler as usize - 1];
            let sampler = Sampler {
                times: floats(&d, importer.accessor(s.input))?,
                values: floats(&d, importer.accessor(s.output))?,
                mode: s.interpolation,
            };
            importer.channels.insert((ch.target_node as usize - 1, ch.target_path), sampler);
        }
    }
    importer.import_model(root, correction.as_ref())
}

/// A .gltf or .glb -> the model `GEModel::StoreLoaded` stores (MODEL_FORMATS.md 4.3), with the frames the game
/// shares (`shared`: frame -> offset, sorted by frame) mapped as the `.cem` reader mapped them.
pub fn gltf_to_model(input: &[u8], resolve: Option<&mut Resolver>, shared: SharedFrames) -> Result<CemModel, Failure> {
    let model = import(input, resolve)?;
    let mut poisoned = false;
    Ok(to_cem_model(&model, shared, &mut poisoned))
}

/// The model as the `.cem` reader (retired under D9) left it after reading the bytes the converter wrote for it
/// before D9 (GEModel::Save's layout, version 2.0): the counts the reader refused (no polygons, vertices, materials or
/// frames) fail the model before anything is stored, and since a failed sub-model put the file back where it
/// started, every model after it in the file read the same bytes and failed the same way (`poisoned`). A later LOD
/// stores whole triangles (the reader read its count, then three indices per triangle).
fn to_cem_model(m: &Model, shared: SharedFrames, poisoned: &mut bool) -> CemModel {
    let fc = m.vertices.len() as u32;
    let counts = Counts {
        polygon: m.polygon_count,
        vertex: (m.vertices[0].len() / 8) as u32,
        tag_point: m.tag_names.len() as u32,
        material: m.materials.len() as u32,
        frame: fc,
        sub_model: m.children.len() as u32,
        lod: m.lods.len() as u32,
    };
    if *poisoned || counts.polygon == 0 || counts.vertex == 0 || counts.material == 0 || counts.frame == 0 {
        *poisoned = true;
        return CemModel { result: FAILED, ..CemModel::default() };
    }
    let to_point = |v: &[f32]| Ge3DPoint { x: v[0], y: v[1], z: v[2] };
    let lods = m
        .lods
        .iter()
        .enumerate()
        .map(|(i, lod)| {
            let count = (lod.len() / 3) as u32;
            CemPolygons { created: if i > 0 { Some(count) } else { None }, indices: lod[..3 * count as usize].to_vec() }
        })
        .collect();
    let materials = m
        .materials
        .iter()
        .map(|mat| {
            let mut values = vec![mat.texture_index];
            for &(first, count) in &mat.ranges {
                values.push(first);
                values.push(count);
            }
            values.push(mat.vertex_start);
            values.push(mat.vertex_count);
            CemMaterial { name: up_to_nul(&mat.name), values, texture_filename: up_to_nul(&mat.texture) }
        })
        .collect();
    let frames = (0..fc as usize)
        .map(|f| {
            let v = m.vertices[f].as_chunks::<8>().0;
            CemFrame {
                radius: Some(m.radii[f]),
                target: if shared.get(f as u32).is_some() { fc - 1 } else { f as u32 },
                points: v.iter().map(|c| to_point(&c[0..3])).collect(),
                normals: v.iter().map(|c| GeVector { x: c[3], y: c[4], z: c[5] }).collect(),
                uvs: v.iter().map(|c| [c[6], c[7]]).collect(),
                tag_points: m.tags[f].as_chunks::<3>().0.iter().map(|c| to_point(c)).collect(),
                transform: m.transforms[f].to_vec(),
                bbox: m.boxes[f].to_vec(),
            }
        })
        .collect();
    let mut out = CemModel {
        result: OK,
        counts: Some(counts),
        name: up_to_nul(&m.name),
        center: m.center.to_vec(),
        lods,
        materials,
        tag_point_names: m.tag_names.iter().map(|t| up_to_nul(t)).collect(),
        frames,
        sub_models: Vec::new(),
    };
    for c in &m.children {
        let sub = to_cem_model(c, shared, poisoned);
        out.sub_models.push(sub);
    }
    out
}

/// A name as `UString::Save` writes it and `UString::Load` reads it back: up to its first NUL.
fn up_to_nul(s: &[u8]) -> Vec<u8> {
    s[..s.iter().position(|&c| c == 0).unwrap_or(s.len())].to_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    // One triangle, 2 frames (one morph target), a tag point; the buffer inline as base64.
    fn triangle_gltf() -> Vec<u8> {
        let mut bin = Vec::new();
        for v in [0.0f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0] {
            bin.extend_from_slice(&v.to_le_bytes()); // POSITION, 36 bytes
        }
        for v in [0.0f32, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0] {
            bin.extend_from_slice(&v.to_le_bytes()); // NORMAL, 36
        }
        for v in [0.0f32, 0.0, 1.0, 0.0, 0.0, 1.0] {
            bin.extend_from_slice(&v.to_le_bytes()); // TEXCOORD, 24
        }
        for v in [0.1f32, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0] {
            bin.extend_from_slice(&v.to_le_bytes()); // morph POSITION delta, 36
        }
        for v in [0u16, 1, 2] {
            bin.extend_from_slice(&v.to_le_bytes()); // indices, 6
        }
        bin.extend_from_slice(&[0, 0]); // padding: 138 bytes in all
        let b64 = crate::tests::base64(&bin);
        format!(
            concat!(
                r#"{{"asset":{{"version":"2.0"}},"scene":0,"scenes":[{{"nodes":[0]}}],"#,
                r#""nodes":[{{"name":"axes","matrix":[1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1],"children":[1]}},"#,
                r#"{{"name":"box.001","mesh":0,"children":[2]}},{{"name":"tag","translation":[1.5,2.5,3.5]}}],"#,
                r#""meshes":[{{"primitives":[{{"attributes":{{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2}},"indices":4,"material":0,"#,
                r#""targets":[{{"POSITION":3}}]}}]}}],"#,
                r#""materials":[{{"name":"m\u00e9","pbrMetallicRoughness":{{"baseColorTexture":{{"index":0}}}},"extras":{{"ee_textureIndex":7}}}}],"#,
                r#""textures":[{{"source":0}}],"images":[{{"uri":"tex%20a.png"}}],"#,
                r#""accessors":[{{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}},"#,
                r#"{{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}},"#,
                r#"{{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}},"#,
                r#"{{"bufferView":3,"componentType":5126,"count":3,"type":"VEC3"}},"#,
                r#"{{"bufferView":4,"componentType":5123,"count":3,"type":"SCALAR"}}],"#,
                r#""bufferViews":[{{"buffer":0,"byteLength":36}},{{"buffer":0,"byteOffset":36,"byteLength":36}},"#,
                r#"{{"buffer":0,"byteOffset":72,"byteLength":24}},{{"buffer":0,"byteOffset":96,"byteLength":36}},"#,
                r#"{{"buffer":0,"byteOffset":132,"byteLength":6}}],"#,
                r#""buffers":[{{"byteLength":138,"uri":"data:application/octet-stream;base64,{}"}}]}}"#
            ),
            b64
        )
        .into_bytes()
    }

    fn refused(doc: &[u8], resolve: Option<&mut Resolver>) -> Vec<u8> {
        gltf_to_model(doc, resolve, SharedFrames(&[])).unwrap_err().0
    }

    #[test]
    fn broken_files_are_refused_with_the_cpp_messages() {
        let fails = |doc: &[u8]| refused(doc, None);
        assert_eq!(fails(b"not json at all"), b"not a glTF 2.0 file".to_vec());
        assert_eq!(fails(br#"{"nodes":[]}"#), b"no node with a mesh".to_vec());
        let mut none = triangle_gltf();
        let at = none.windows(9).position(|w| w == b"\"scenes\":").unwrap();
        none.splice(at..at + 8, *b"\"scens\": ");
        // without "scenes" the scene index 0 is out of range: refused by cgltf
        assert_eq!(fails(&none), b"not a glTF 2.0 file".to_vec());
    }

    #[test]
    fn a_rotated_wrapper_above_the_root_is_refused() {
        let doc = String::from_utf8(triangle_gltf())
            .unwrap()
            .replace("[1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1]", "[0.7,0.7,0,0,-0.7,0.7,0,0,0,0,1,0,0,0,0,1]");
        assert_eq!(refused(doc.as_bytes(), None), b"the transform above the root model is not an axis change");
    }

    #[test]
    fn an_external_buffer_goes_through_the_resolver() {
        let doc = br#"{"buffers":[{"byteLength":3,"uri":"a%20b.bin"}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"accessors":[{"bufferView":0,"componentType":5121,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteLength":3}]}"#;
        let mut asked = Vec::new();
        let mut resolve = |uri: &[u8]| -> Option<Vec<u8>> {
            asked.push(uri.to_vec());
            Some(vec![1, 2, 3])
        };
        let e = refused(doc, Some(&mut resolve));
        // the position accessor has one component per element: 3 floats, so one vertex; indices absent: 1 triangle index
        assert_eq!(e, b"an index count that is not a multiple of 3".to_vec());
        assert_eq!(asked, vec![b"a b.bin".to_vec()]);
    }

    #[test]
    fn missing_and_short_external_buffers() {
        let doc = br#"{"buffers":[{"byteLength":3,"uri":"x.bin"}]}"#;
        assert_eq!(refused(doc, None), b"buffer \"x.bin\" not found".to_vec());
        let mut none = |_: &[u8]| -> Option<Vec<u8>> { None };
        assert_eq!(refused(doc, Some(&mut none)), b"buffer \"x.bin\" not found".to_vec());
        let mut short = |_: &[u8]| -> Option<Vec<u8>> { Some(vec![1]) };
        assert_eq!(refused(doc, Some(&mut short)), b"buffer \"x.bin\" is shorter than its byteLength".to_vec());
    }

    #[test]
    fn game_names() {
        assert_eq!(game_name(Some(b"abc.001")).unwrap(), b"abc".to_vec());
        assert_eq!(game_name(Some(b"abc.01")).unwrap(), b"abc.01".to_vec());
        assert_eq!(game_name(None).unwrap(), Vec::<u8>::new());
        assert_eq!(game_name(Some(b"caf\xc3\xa9")).unwrap(), vec![b'c', b'a', b'f', 0xe9]);
        assert_eq!(game_name(Some(b"\xe2\x82\xac")).unwrap(), vec![0x80]); // the euro sign in CP-1252
        assert_eq!(game_name(Some(br"a\tb")).unwrap(), b"a\tb".to_vec());
        assert!(game_name(Some(b"\xff")).unwrap_err().0.ends_with(b"is not UTF-8"));
        assert!(game_name(Some("\u{4e2d}".as_bytes())).unwrap_err().0.ends_with(b"has characters outside CP-1252"));
    }

    #[test]
    fn sampler_interpolation_matches_the_python() {
        let linear = Sampler { times: vec![0.0, 1.0], values: vec![0.0, 2.0], mode: Interpolation::Linear };
        assert_eq!(linear.at(0.5, 1).unwrap(), vec![1.0]);
        let step = Sampler { times: vec![0.0, 1.0], values: vec![0.0, 2.0], mode: Interpolation::Step };
        assert_eq!(step.at(0.5, 1).unwrap(), vec![0.0]);
        assert_eq!(step.at(1.5, 1).unwrap(), vec![2.0]);
        let cubic = Sampler { times: vec![0.0], values: vec![9.0, 5.0, 8.0], mode: Interpolation::CubicSpline };
        assert_eq!(cubic.at(0.2, 1).unwrap(), vec![5.0]);
        let short = Sampler { times: vec![0.0, 1.0], values: vec![1.0], mode: Interpolation::Linear };
        assert!(short.at(0.0, 1).is_err());
    }

    #[test]
    fn matrix_helpers() {
        assert_eq!(mat_mul(&IDENTITY, &AXES), AXES);
        assert!(is_signed_permutation(&AXES));
        let mut translated = IDENTITY;
        translated[12] = 1.0;
        assert!(!is_signed_permutation(&translated));
        // compose of the identity transform is the identity
        assert_eq!(compose([0.0; 3], [0.0, 0.0, 0.0, 1.0], [1.0; 3]), IDENTITY);
    }

    #[test]
    fn the_stored_model_of_a_triangle() {
        let m = gltf_to_model(&triangle_gltf(), None, SharedFrames(&[])).unwrap();
        assert_eq!(m.result, OK);
        let c = m.counts.unwrap();
        assert_eq!((c.polygon, c.vertex, c.tag_point, c.material, c.frame, c.sub_model, c.lod), (1, 3, 1, 1, 2, 0, 1));
        assert_eq!(m.name, b"box".to_vec());
        assert_eq!(m.center[0], 0.5);
        assert_eq!(m.lods.len(), 1);
        assert_eq!(m.lods[0].created, None);
        assert_eq!(m.lods[0].indices, vec![0, 1, 2]);
        // texture index, (start, length) of LOD 0, vertex start, vertex length
        assert_eq!(m.materials[0].values, vec![7, 0, 1, 0, 3]);
        assert_eq!(m.materials[0].name, vec![b'm', 0xe9]);
        assert_eq!(m.materials[0].texture_filename, b"tex a".to_vec());
        assert_eq!(m.tag_point_names, vec![b"tag".to_vec()]);
        assert_eq!(m.frames.len(), 2);
        assert_eq!((m.frames[0].target, m.frames[1].target), (0, 1));
        assert_eq!(m.frames[1].points[0].x, 0.1);
        assert_eq!(m.frames[0].normals[0].z, 1.0);
        assert_eq!(m.frames[0].uvs[1], [1.0, 0.0]);
        assert_eq!(m.frames[0].transform.len(), 16);
        assert_eq!(m.frames[0].bbox.len(), 6);
        assert!(m.frames[0].radius.is_some());
        assert_eq!(m.frames[0].tag_points.len(), 1);
    }

    #[test]
    fn a_shared_frame_goes_into_the_last_frames_arrays() {
        let m = gltf_to_model(&triangle_gltf(), None, SharedFrames(&[(0, 1)])).unwrap();
        assert_eq!((m.frames[0].target, m.frames[1].target), (1, 1));
    }

    #[test]
    fn a_model_the_reader_refuses_and_the_sub_models_after_it() {
        let ok = || {
            let mut m = Model { polygon_count: 1, lods: vec![vec![0, 1, 2]], ..Model::default() };
            m.vertices = vec![vec![0.0; 24]];
            m.tags = vec![Vec::new()];
            m.radii = vec![1.0];
            m.transforms = vec![[0.0; 16]];
            m.boxes = vec![[0.0; 6]];
            m.materials = vec![Material { ranges: vec![(0, 1)], ..Material::default() }];
            m
        };
        let mut root = ok();
        let mut a = ok();
        a.polygon_count = 0; // the counts chunk fails: nothing stored
        a.children = vec![ok()];
        root.children = vec![a, ok()];
        let mut poisoned = false;
        let m = to_cem_model(&root, SharedFrames(&[]), &mut poisoned);
        assert_eq!(m.result, OK);
        assert_eq!(m.sub_models.len(), 2);
        for sub in &m.sub_models {
            // the second read the first one's bytes again
            assert_eq!(sub.result, FAILED);
            assert!(sub.counts.is_none());
            assert!(sub.sub_models.is_empty());
        }
        let mut poisoned = false;
        let fine = to_cem_model(&ok(), SharedFrames(&[]), &mut poisoned);
        assert_eq!(fine.result, OK);
        assert!(!poisoned);
    }

    #[test]
    fn a_later_lods_indices_are_whole_triangles_and_names_end_at_a_nul() {
        let mut m = Model { polygon_count: 1, lods: vec![vec![0, 1, 2], vec![2, 1, 0, 1]], ..Model::default() };
        m.name = b"ab\0cd".to_vec();
        m.vertices = vec![vec![0.0; 24]];
        m.tags = vec![Vec::new()];
        m.radii = vec![1.0];
        m.transforms = vec![[0.0; 16]];
        m.boxes = vec![[0.0; 6]];
        m.materials = vec![Material { ranges: vec![(0, 1), (0, 1)], ..Material::default() }];
        let c = to_cem_model(&m, SharedFrames(&[]), &mut false);
        assert_eq!(c.name, b"ab".to_vec());
        assert_eq!(c.lods[1].created, Some(1));
        assert_eq!(c.lods[1].indices, vec![2, 1, 0]);
        assert_eq!(c.materials[0].values, vec![0, 0, 1, 0, 1, 0, 0]);
    }
}
