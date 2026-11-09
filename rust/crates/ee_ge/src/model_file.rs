//! `GEModel`'s `.cem` reader ('GEModel Load.cpp' and the readers of 'GEModel Legacy.cpp'): the header, the counts
//! and every chunk of the current format (2.x) and of the legacy formats (1.0-1.3), and the sub-models that follow
//! a model. The file is a contract: every field goes through the file's own typed reads (`FieldIo`), in the C++'s
//! order and with its checks, its error codes and its rewind on failure.
//!
//! The reader decodes into a plain [`CemModel`]; the C++ (`GEModel::Load`) then allocates the model's arrays from
//! the counts and stores what was read. A load that fails part way leaves what the C++ would have stored up to the
//! failing field (a top-level model that fails is thrown away by `GEModelManager`, but a failing sub-model stays
//! attached to its parent, which ignores sub-model results): every list below is the prefix the C++ stored.
//!
//! Assumption: a failed read stores nothing. That is `FSMemoryFile`'s rule, and every model load goes through one
//! (`GEModel::Load` copies any other file into a memory file first; sub-models read from the same copy).

use std::ffi::c_long;
use std::sync::OnceLock;

use crate::io::{FieldIo, FAILED, OK};
use crate::legacy::{self, LegacyModel, LegacyPolygon};
use crate::types::{Ge3DPoint, GeVector};
use crate::vector::{build_quantized_table, QUANT_COUNT};

/// The polygon, vertex, tag point, material, frame, sub-model and LOD counts (`GEModel`'s members).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Counts {
    pub polygon: u32,
    pub vertex: u32,
    pub tag_point: u32,
    pub material: u32,
    pub frame: u32,
    pub sub_model: u32,
    pub lod: u32,
}

/// The frames a model shares with a neighbour (`ioTextures`, a `std::map<unsigned long, long>` from frame to offset),
/// sorted by frame.
#[derive(Clone, Copy, Debug, Default)]
pub struct SharedFrames<'a>(pub &'a [(u32, i32)]);

impl SharedFrames<'_> {
    pub fn get(&self, frame: u32) -> Option<i32> {
        self.0.binary_search_by_key(&frame, |e| e.0).ok().map(|i| self.0[i].1)
    }

    /// The array each frame ends up with (`AllocateInternalData`: in frame order, a shared frame takes the array its
    /// offset names, which may itself have been shared already). An offset to a later shared frame reads a pointer
    /// the C++ had not set yet; here it gives that later frame's own index.
    pub fn physical_frames(&self, frame_count: u32) -> Vec<usize> {
        let mut out: Vec<usize> = (0..frame_count as usize).collect();
        for f in 0..frame_count as usize {
            if let Some(offset) = self.get(f as u32) {
                let target = (f as i64).wrapping_add(offset as i64);
                if target >= 0 && (target as usize) < out.len() {
                    out[f] = out[target as usize];
                }
            }
        }
        out
    }
}

/// One material as read: its name, then the values in the order the C++ sets them (texture index, the polygon start
/// and length of each LOD, the vertex start, the vertex length; a prefix when the load failed), and the texture file
/// name (`mTextureFilenames[i]`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemMaterial {
    pub name: Vec<u8>,
    pub values: Vec<u32>,
    pub texture_filename: Vec<u8>,
}

/// One LOD's polygons: the array LOD 0 gets from the allocation, a later LOD's from its count (`created`); the
/// vertex indices set, three per polygon (a prefix).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemPolygons {
    pub created: Option<u32>,
    pub indices: Vec<u32>,
}

/// One frame as read. The vertices and tag points go into the arrays of frame `target` (the last frame for a frame
/// the model shares: `LoadFrameChunk`); the radius, transform and box are the frame's own. `transform` is the prefix
/// of the 16 elements read (row by row) and `bbox` of the box's six corner values; the box's dimensions follow
/// when all six were read.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemFrame {
    pub radius: Option<f32>,
    pub target: u32,
    pub points: Vec<Ge3DPoint>,
    pub normals: Vec<GeVector>,
    pub uvs: Vec<[f32; 2]>,
    pub tag_points: Vec<Ge3DPoint>,
    pub transform: Vec<f32>,
    pub bbox: Vec<f32>,
}

/// A model as `GEModel::Load` leaves it.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemModel {
    /// The load's result (`kErrorOK` = 0).
    pub result: c_long,
    /// The counts the model's arrays were allocated with (`AllocateInternalData`), when the counts were read.
    pub counts: Option<Counts>,
    pub name: Vec<u8>,
    /// The centre point's values read (a prefix of x, y, z).
    pub center: Vec<f32>,
    pub lods: Vec<CemPolygons>,
    pub materials: Vec<CemMaterial>,
    pub tag_point_names: Vec<Vec<u8>>,
    pub frames: Vec<CemFrame>,
    /// The sub-models, created when the model's own chunks all loaded (each with its own result).
    pub sub_models: Vec<CemModel>,
}

impl CemModel {
    fn allocate(&mut self, counts: Counts) {
        self.counts = Some(counts);
        self.lods = vec![CemPolygons::default(); counts.lod as usize];
        self.materials = vec![CemMaterial::default(); counts.material as usize];
        self.tag_point_names = vec![Vec::new(); counts.tag_point as usize];
        self.frames = (0..counts.frame).map(|f| CemFrame { target: f, ..CemFrame::default() }).collect();
    }
}

/// The "player color" material and polygon group name.
pub(crate) const PLAYER_COLOR: &[u8] = b"player color";

fn quantized_table() -> &'static [GeVector] {
    static TABLE: OnceLock<Vec<GeVector>> = OnceLock::new();
    TABLE.get_or_init(|| {
        let mut table = vec![GeVector::default(); QUANT_COUNT];
        build_quantized_table(&mut table);
        table
    })
}

/// The quantized vector table's entry (for the tests).
#[cfg(test)]
pub(crate) fn tests_quantized(index: usize) -> GeVector {
    quantized_table()[index]
}

fn read_u32(io: &mut dyn FieldIo, out: &mut u32) -> c_long {
    io.read_u32(out)
}

fn read_f32(io: &mut dyn FieldIo) -> Result<f32, c_long> {
    let mut v = 0.0f32;
    match io.read_f32(&mut v) {
        OK => Ok(v),
        r => Err(r),
    }
}

/// `UString::Load`: a length, then that many bytes; the text ends at the first NUL. Any failure leaves the string
/// empty (a short read of the bytes is `kErrorFailed`). A length the allocator refuses is `kErrorFailed` too (the
/// C++ would have died allocating it).
pub fn load_string(io: &mut dyn FieldIo, out: &mut Vec<u8>) -> c_long {
    let mut length = 0u32;
    let mut result = io.read_u32(&mut length);
    out.clear();
    if result == OK && length != 0 {
        let mut buffer = Vec::new();
        if buffer.try_reserve_exact(length as usize).is_err() {
            return FAILED;
        }
        buffer.resize(length as usize, 0);
        let mut read = 0u32;
        result = io.read_bytes(&mut buffer, &mut read);
        if result == OK && read == length {
            let end = buffer.iter().position(|&b| b == 0).unwrap_or(buffer.len());
            buffer.truncate(end);
            *out = buffer;
        } else if result == OK {
            result = FAILED;
        }
    }
    result
}

/// `GE3DPoint::Load` (or `GEVector::Load`) into a member: x, y, z, each stored as it is read.
fn load_floats_into(io: &mut dyn FieldIo, count: usize, out: &mut Vec<f32>) -> c_long {
    for _ in 0..count {
        match read_f32(io) {
            Ok(v) => out.push(v),
            Err(r) => return r,
        }
    }
    OK
}

/// `GE3DPoint::Load` into a temporary: the point when all three values were read.
fn load_point(io: &mut dyn FieldIo) -> Result<Ge3DPoint, c_long> {
    Ok(Ge3DPoint { x: read_f32(io)?, y: read_f32(io)?, z: read_f32(io)? })
}

fn load_vector(io: &mut dyn FieldIo) -> Result<GeVector, c_long> {
    Ok(GeVector { x: read_f32(io)?, y: read_f32(io)?, z: read_f32(io)? })
}

/// `GETextureCoordinate::Load`: u, then v; a failure puts the file back where the coordinate started.
fn load_texture_coordinate(io: &mut dyn FieldIo, out: &mut [f32; 2]) -> c_long {
    let mut position: c_long = 0;
    let mut result = io.get_position(&mut position);
    if result == OK {
        result = io.read_f32(&mut out[0]);
        if result == OK {
            result = io.read_f32(&mut out[1]);
        }
        if result != OK {
            io.set_position(position);
        }
    }
    result
}

/// `GEVector::LoadQuantizedIndex`: an index into the quantized vector table, the zero vector past its end.
fn load_quantized_vector(io: &mut dyn FieldIo) -> Result<GeVector, c_long> {
    let mut index = 0u16;
    match io.read_u16(&mut index) {
        OK => Ok(quantized_table().get(index as usize).copied().unwrap_or_default()),
        r => Err(r),
    }
}

/// `GETransformation::Load` into a member: 16 values row by row, each stored as read.
fn load_transform(io: &mut dyn FieldIo, out: &mut Vec<f32>) -> c_long {
    load_floats_into(io, 16, out)
}

/// `GEBoundingBox::Load` into a member: the two corners (each value stored as read).
fn load_box(io: &mut dyn FieldIo, out: &mut Vec<f32>) -> c_long {
    let r = load_floats_into(io, 3, out);
    if r != OK {
        return r;
    }
    load_floats_into(io, 3, out)
}

/// `LoadHeaderChunk`: "SSMF", then the version: 1.0-1.3 or 2.0-2.3.
pub fn load_header(io: &mut dyn FieldIo) -> Result<(u16, u16), c_long> {
    let mut tag = [0u8; 4];
    let mut read = 0u32;
    let result = io.read_bytes(&mut tag, &mut read);
    if result != OK {
        return Err(result);
    }
    if read != 4 {
        return Err(FAILED);
    }
    if &tag != b"SSMF" {
        return Err(FAILED);
    }
    let mut major = 0u16;
    let result = io.read_u16(&mut major);
    if result != OK {
        return Err(result);
    }
    if major != 1 && major != 2 {
        return Err(FAILED);
    }
    let mut minor = 0u16;
    let result = io.read_u16(&mut minor);
    if result != OK {
        return Err(result);
    }
    if minor > 3 {
        return Err(FAILED);
    }
    Ok((major, minor))
}

/// `GEModel::Load` once the file is known to be open: the header, the body by version, and on any failure the file
/// put back where the model started.
pub fn load(io: &mut dyn FieldIo, shared: SharedFrames) -> CemModel {
    let mut model = CemModel::default();
    let mut position: c_long = 0;
    model.result = io.get_position(&mut position);
    if model.result == OK {
        model.result = match load_header(io) {
            Ok((major, minor)) if major < 2 => load_legacy(io, shared, &mut model, minor),
            Ok(_) => load_current(io, shared, &mut model),
            Err(r) => r,
        };
        if model.result != OK {
            io.set_position(position);
        }
    }
    model
}

fn load_sub_models(io: &mut dyn FieldIo, shared: SharedFrames, model: &mut CemModel) {
    let count = model.counts.map_or(0, |c| c.sub_model);
    for _ in 0..count {
        // the C++ ignores the sub-models' results
        let sub = load(io, shared);
        model.sub_models.push(sub);
    }
}

// ---- the current format (2.x)

/// `LoadCurrentVersion`: the chunks in order, then the sub-models.
fn load_current(io: &mut dyn FieldIo, shared: SharedFrames, model: &mut CemModel) -> c_long {
    for chunk in [load_counts, load_miscellaneous, load_polygons, load_materials, load_tag_point_names] {
        let r = chunk(io, model);
        if r != OK {
            return r;
        }
    }
    let r = load_frames(io, shared, model);
    if r != OK {
        return r;
    }
    load_sub_models(io, shared, model);
    OK
}

/// `LoadCountsChunk`: polygons, vertices, tag points, materials, frames (none of polygons, vertices, materials or
/// frames fails), sub-models and LODs; then the allocation.
fn load_counts(io: &mut dyn FieldIo, model: &mut CemModel) -> c_long {
    let mut c = Counts { lod: 1, ..Counts::default() };
    let r = read_u32(io, &mut c.polygon);
    if r != OK {
        return r;
    }
    if c.polygon == 0 {
        return FAILED;
    }
    let r = read_u32(io, &mut c.vertex);
    if r != OK {
        return r;
    }
    if c.vertex == 0 {
        return FAILED;
    }
    let r = read_u32(io, &mut c.tag_point);
    if r != OK {
        return r;
    }
    let r = read_u32(io, &mut c.material);
    if r != OK {
        return r;
    }
    if c.material == 0 {
        return FAILED;
    }
    let r = read_u32(io, &mut c.frame);
    if r != OK {
        return r;
    }
    if c.frame == 0 {
        return FAILED;
    }
    let r = read_u32(io, &mut c.sub_model);
    if r != OK {
        return r;
    }
    let r = read_u32(io, &mut c.lod);
    if r != OK {
        return r;
    }
    model.allocate(c);
    OK
}

/// `LoadMiscellaneousChunk`: the name, then the centre point.
fn load_miscellaneous(io: &mut dyn FieldIo, model: &mut CemModel) -> c_long {
    let r = load_string(io, &mut model.name);
    if r != OK {
        return r;
    }
    load_floats_into(io, 3, &mut model.center)
}

/// `LoadPolygonsChunk`: per LOD a count (a new array above LOD 0) and three vertex indices per polygon. A failed
/// LOD does not stop the loop; the result is the last LOD's.
fn load_polygons(io: &mut dyn FieldIo, model: &mut CemModel) -> c_long {
    let mut result = OK;
    for i in 0..model.lods.len() {
        let mut count = 0u32;
        result = io.read_u32(&mut count);
        if result != OK {
            continue;
        }
        let lod = &mut model.lods[i];
        if i > 0 {
            lod.created = Some(count);
        }
        let mut j = 0u32;
        while j < count && result == OK {
            for _ in 0..3 {
                let mut index = 0u32;
                result = io.read_u32(&mut index);
                if result != OK {
                    break;
                }
                lod.indices.push(index);
            }
            j += 1;
        }
    }
    result
}

/// `LoadMaterialsChunk`: each material's name, texture index, polygon range per LOD, vertex range and texture
/// file name, stopping at the first failure.
fn load_materials(io: &mut dyn FieldIo, model: &mut CemModel) -> c_long {
    let lods = model.lods.len();
    let mut result = OK;
    let mut i = 0;
    while i < model.materials.len() && result == OK {
        let material = &mut model.materials[i];
        i += 1;
        let mut name = Vec::new();
        result = load_string(io, &mut name);
        if result != OK {
            continue;
        }
        material.name = name;
        let mut value = 0u32;
        result = io.read_u32(&mut value);
        if result != OK {
            continue;
        }
        material.values.push(value);
        let mut j = 0;
        while j < lods && result == OK {
            j += 1;
            result = io.read_u32(&mut value);
            if result != OK {
                continue;
            }
            material.values.push(value);
            result = io.read_u32(&mut value);
            if result == OK {
                material.values.push(value);
            }
        }
        if result != OK {
            continue;
        }
        result = io.read_u32(&mut value);
        if result != OK {
            continue;
        }
        material.values.push(value);
        result = io.read_u32(&mut value);
        if result != OK {
            continue;
        }
        material.values.push(value);
        result = load_string(io, &mut material.texture_filename);
    }
    result
}

/// `LoadTagPointsChunk`: the tag point names, stopping at the first failure.
fn load_tag_point_names(io: &mut dyn FieldIo, model: &mut CemModel) -> c_long {
    let mut result = OK;
    for name in model.tag_point_names.iter_mut() {
        result = load_string(io, name);
        if result != OK {
            break;
        }
    }
    result
}

/// `LoadTagPointsSubChunk`: a frame's tag points, each stored when all three values were read.
fn load_tag_points(io: &mut dyn FieldIo, count: u32, out: &mut Vec<Ge3DPoint>) -> c_long {
    for _ in 0..count {
        match load_point(io) {
            Ok(p) => out.push(p),
            Err(r) => return r,
        }
    }
    OK
}

/// `LoadFrameChunk`: per frame the radius, vertices (point, normal, texture coordinate), tag points, transform and
/// box. A failed box does not stop the loop.
fn load_frames(io: &mut dyn FieldIo, shared: SharedFrames, model: &mut CemModel) -> c_long {
    let counts = model.counts.expect("the counts chunk allocated the model");
    let mut result = OK;
    for i in 0..counts.frame {
        let frame = &mut model.frames[i as usize];
        frame.target = if shared.get(i).is_some() { counts.frame - 1 } else { i };
        match read_f32(io) {
            Ok(v) => frame.radius = Some(v),
            Err(r) => {
                result = r;
                break;
            }
        }
        result = load_vertices(io, counts.vertex, frame);
        if result != OK {
            break;
        }
        result = load_tag_points(io, counts.tag_point, &mut frame.tag_points);
        if result != OK {
            break;
        }
        result = load_transform(io, &mut frame.transform);
        if result != OK {
            break;
        }
        result = load_box(io, &mut frame.bbox);
    }
    result
}

/// `LoadVertexSubChunk`: each vertex's point, normal and texture coordinate, each stored when it was read whole.
fn load_vertices(io: &mut dyn FieldIo, count: u32, frame: &mut CemFrame) -> c_long {
    for _ in 0..count {
        match load_point(io) {
            Ok(p) => frame.points.push(p),
            Err(r) => return r,
        }
        match load_vector(io) {
            Ok(n) => frame.normals.push(n),
            Err(r) => return r,
        }
        let mut uv = [0.0f32; 2];
        let r = load_texture_coordinate(io, &mut uv);
        if r != OK {
            return r;
        }
        frame.uvs.push(uv);
    }
    OK
}

// ---- the legacy formats (1.0-1.3)

/// `LoadLegacyVersion`: the version-1 chunks in order (no tag point names in 1.0), the conversion to the current
/// layout (`ConvertLegacyModel`), then the sub-models. A failed load keeps what the C++'s members held: the name,
/// centre, texture file names, tag point names and the frames' radii, tag points, transforms and boxes; the
/// version-1 data itself only lived until the conversion.
fn load_legacy(io: &mut dyn FieldIo, shared: SharedFrames, model: &mut CemModel, minor: u16) -> c_long {
    let mut legacy = LegacyModel::default();
    let r = load_legacy_body(io, shared, model, &mut legacy, minor);
    if r != OK {
        return r;
    }
    legacy::convert(&legacy, shared, model);
    load_sub_models(io, shared, model);
    OK
}

fn load_legacy_body(
    io: &mut dyn FieldIo,
    shared: SharedFrames,
    model: &mut CemModel,
    legacy: &mut LegacyModel,
    minor: u16,
) -> c_long {
    let r = load_legacy_counts(io, model, legacy, minor);
    if r != OK {
        return r;
    }
    let r = load_legacy_miscellaneous(io, model, legacy, minor);
    if r != OK {
        return r;
    }
    let r = load_legacy_polygons(io, legacy);
    if r != OK {
        return r;
    }
    let r = load_legacy_polygon_groups(io, legacy);
    if r != OK {
        return r;
    }
    let r = load_legacy_materials(io, model, legacy);
    if r != OK {
        return r;
    }
    let r = load_legacy_vertices(io, legacy);
    if r != OK {
        return r;
    }
    if minor != 0 {
        let r = load_tag_point_names(io, model);
        if r != OK {
            return r;
        }
    }
    load_legacy_frames(io, shared, model, legacy, minor)
}

/// `LoadLegacyCountsChunk`: frames, materials, points, polygons (none of these fails), polygon groups, vertices
/// (none fails), then from 1.1 the tag point count and from 1.2 the sub-model count (both optional: a failed read
/// leaves 0).
fn load_legacy_counts(io: &mut dyn FieldIo, model: &mut CemModel, legacy: &mut LegacyModel, minor: u16) -> c_long {
    let (mut frames, mut materials, mut points, mut polygons, mut groups, mut vertices) = (0, 0, 0, 0, 0, 0);
    for (value, nonzero) in [
        (&mut frames, true),
        (&mut materials, true),
        (&mut points, true),
        (&mut polygons, true),
        (&mut groups, false),
        (&mut vertices, false),
    ] {
        let r = io.read_u32(value);
        if r != OK {
            return r;
        }
        if nonzero && *value == 0 {
            return FAILED;
        }
    }
    if vertices == 0 {
        return FAILED;
    }
    let mut tag_points = 0u32;
    let mut sub_models = 0u32;
    if minor != 0 && io.read_u32(&mut tag_points) == OK && minor >= 2 {
        io.read_u32(&mut sub_models);
    }
    model.allocate(Counts {
        polygon: polygons,
        vertex: vertices,
        tag_point: tag_points,
        material: materials,
        frame: frames,
        sub_model: sub_models,
        lod: 1,
    });
    legacy.polygons = vec![LegacyPolygon::default(); polygons as usize];
    legacy.vertex_points = vec![0; vertices as usize];
    legacy.points = vec![vec![Ge3DPoint::default(); points as usize]; frames as usize];
    legacy.normals = vec![vec![GeVector::default(); vertices as usize]; frames as usize];
    legacy.materials = vec![Vec::new(); materials as usize];
    legacy.groups = vec![Vec::new(); groups as usize];
    OK
}

/// `LoadLegacyMiscellaneousChunk`: the name, the centre, a flag and (1.3) one skipped value per point.
fn load_legacy_miscellaneous(io: &mut dyn FieldIo, model: &mut CemModel, legacy: &LegacyModel, minor: u16) -> c_long {
    let r = load_string(io, &mut model.name);
    if r != OK {
        return r;
    }
    let r = load_floats_into(io, 3, &mut model.center);
    if r != OK {
        return r;
    }
    let mut flag = 0u8;
    let mut r = io.read_u8(&mut flag);
    if r != OK {
        return r;
    }
    if minor >= 3 {
        let point_count = legacy.points.first().map_or(0, |p| p.len());
        let mut i = 0;
        while i < point_count && r == OK {
            let mut skipped = 0u32;
            r = io.read_u32(&mut skipped);
            i += 1;
        }
    }
    r
}

/// `LoadLegacyPolygonsChunk`: per polygon three corners, each a vertex index, a texture coordinate, a colour (its
/// result ignored) and four skipped values.
fn load_legacy_polygons(io: &mut dyn FieldIo, legacy: &mut LegacyModel) -> c_long {
    let mut result = OK;
    let mut i = 0;
    while i < legacy.polygons.len() && result == OK {
        let polygon = &mut legacy.polygons[i];
        i += 1;
        for j in 0..3 {
            result = io.read_u32(&mut polygon.vertices[j]);
            if result != OK {
                break;
            }
            result = load_texture_coordinate(io, &mut polygon.uvs[j]);
            if result != OK {
                break;
            }
            // GEColor::Load: red, green, blue, stopping at a failure that the caller ignores
            for _ in 0..3 {
                if read_f32(io).is_err() {
                    break;
                }
            }
            for _ in 0..4 {
                if let Err(r) = read_f32(io) {
                    result = r;
                    break;
                }
            }
            if result != OK {
                break;
            }
        }
    }
    result
}

/// `LoadLegacyPolygonGroupsChunk`: each group's name and polygon indices, kept only for "player color" groups.
fn load_legacy_polygon_groups(io: &mut dyn FieldIo, legacy: &mut LegacyModel) -> c_long {
    let mut result = OK;
    for group in legacy.groups.iter_mut() {
        let mut name = Vec::new();
        result = load_string(io, &mut name);
        if result != OK {
            break;
        }
        let mut count = 0u32;
        result = io.read_u32(&mut count);
        if result != OK {
            break;
        }
        let keep = name == PLAYER_COLOR;
        for _ in 0..count {
            let mut index = 0u32;
            result = io.read_u32(&mut index);
            if result != OK {
                break;
            }
            if keep {
                group.push(index);
            }
        }
        if result != OK {
            break;
        }
    }
    result
}

/// `LoadLegacyMaterialsChunk`: each material's polygon indices, a flag and, when set, the texture file name and a
/// skipped texture index.
fn load_legacy_materials(io: &mut dyn FieldIo, model: &mut CemModel, legacy: &mut LegacyModel) -> c_long {
    let mut result = OK;
    let mut i = 0;
    while i < legacy.materials.len() && result == OK {
        let indices = &mut legacy.materials[i];
        let material = &mut model.materials[i];
        i += 1;
        let mut count = 0u32;
        result = io.read_u32(&mut count);
        if result != OK {
            continue;
        }
        for _ in 0..count {
            let mut index = 0u32;
            result = io.read_u32(&mut index);
            if result != OK {
                break;
            }
            indices.push(index);
        }
        if result != OK {
            continue;
        }
        // Read(bool&): one byte; any value but 0 is true here (the C++ read the byte as a bool)
        let mut has_texture = 0u8;
        result = io.read_u8(&mut has_texture);
        if result != OK || has_texture == 0 {
            continue;
        }
        result = load_string(io, &mut material.texture_filename);
        if result == OK {
            let mut skipped = 0u32;
            result = io.read_u32(&mut skipped);
        }
    }
    result
}

/// `LoadLegacyVerticesChunk`: each vertex's point index and a skipped value.
fn load_legacy_vertices(io: &mut dyn FieldIo, legacy: &mut LegacyModel) -> c_long {
    for point in legacy.vertex_points.iter_mut() {
        let r = io.read_u32(point);
        if r != OK {
            return r;
        }
        if let Err(r) = read_f32(io) {
            return r;
        }
    }
    OK
}

/// `LoadLegacyFrameChunk`: per frame the radius, the points, the vertex normals (plain before 1.3, quantized
/// after), before 1.3 the skipped polygon normals; from 1.1 the tag points, from 1.2 the transform, from 1.3 the
/// box (a failed box does not stop the loop).
fn load_legacy_frames(
    io: &mut dyn FieldIo,
    shared: SharedFrames,
    model: &mut CemModel,
    legacy: &mut LegacyModel,
    minor: u16,
) -> c_long {
    let counts = model.counts.expect("the counts chunk allocated the model");
    let mut result = OK;
    for i in 0..counts.frame {
        let target = if shared.get(i).is_some() { counts.frame - 1 } else { i };
        let frame = &mut model.frames[i as usize];
        frame.target = target;
        match read_f32(io) {
            Ok(v) => frame.radius = Some(v),
            Err(r) => {
                result = r;
                break;
            }
        }
        let points = &mut legacy.points[target as usize];
        result = OK;
        for point in points.iter_mut() {
            // GE3DPoint::Load into the member: each value stored as read
            for value in [&mut point.x, &mut point.y, &mut point.z] {
                result = io.read_f32(value);
                if result != OK {
                    break;
                }
            }
            if result != OK {
                break;
            }
        }
        if result != OK {
            break;
        }
        let normals = &mut legacy.normals[target as usize];
        for normal in normals.iter_mut() {
            let read = if minor < 3 { load_vector(io) } else { load_quantized_vector(io) };
            match read {
                Ok(n) => *normal = n,
                Err(r) => {
                    result = r;
                    break;
                }
            }
        }
        if result != OK {
            break;
        }
        if minor < 3 {
            for _ in 0..counts.polygon {
                if let Err(r) = load_vector(io) {
                    result = r;
                    break;
                }
            }
        }
        if result != OK {
            break;
        }
        if minor == 0 {
            continue;
        }
        result = load_tag_points(io, counts.tag_point, &mut frame.tag_points);
        if result != OK {
            break;
        }
        if minor < 2 {
            continue;
        }
        result = load_transform(io, &mut frame.transform);
        if result != OK {
            break;
        }
        if minor >= 3 {
            result = load_box(io, &mut frame.bbox);
        }
    }
    result
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::io::{BufferIo, END_OF_FILE};

    /// A little-endian `.cem` writer for the tests.
    #[derive(Default)]
    pub(crate) struct Cem(pub Vec<u8>);

    impl Cem {
        pub fn header(major: u16, minor: u16) -> Cem {
            let mut c = Cem(b"SSMF".to_vec());
            c.u16(major).u16(minor);
            c
        }
        pub fn u8(&mut self, v: u8) -> &mut Self {
            self.0.push(v);
            self
        }
        pub fn u16(&mut self, v: u16) -> &mut Self {
            self.0.extend_from_slice(&v.to_le_bytes());
            self
        }
        pub fn u32(&mut self, v: u32) -> &mut Self {
            self.0.extend_from_slice(&v.to_le_bytes());
            self
        }
        pub fn f32(&mut self, v: f32) -> &mut Self {
            self.0.extend_from_slice(&v.to_le_bytes());
            self
        }
        pub fn fs(&mut self, v: &[f32]) -> &mut Self {
            for &x in v {
                self.f32(x);
            }
            self
        }
        /// `UString::Save`: the length with the NUL, the text, the NUL.
        pub fn str(&mut self, s: &str) -> &mut Self {
            self.u32(s.len() as u32 + 1);
            self.0.extend_from_slice(s.as_bytes());
            self.0.push(0);
            self
        }
    }

    pub(crate) const IDENTITY: [f32; 16] =
        [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0];

    /// A 2.0 model: one triangle, three vertices, one tag point, one material, `frames` frames, `subs` sub-models
    /// (each a one-frame model without sub-models), two LODs.
    pub(crate) fn current(frames: u32, subs: u32) -> Cem {
        let mut c = Cem::header(2, 0);
        c.u32(1).u32(3).u32(1).u32(1).u32(frames).u32(subs).u32(2);
        c.str("hut").fs(&[1.0, 2.0, 3.0]);
        c.u32(1).u32(0).u32(1).u32(2); // LOD 0: one triangle
        c.u32(1).u32(2).u32(1).u32(0); // LOD 1
        c.str("wood").u32(7).u32(0).u32(1).u32(0).u32(1).u32(0).u32(3).str("wood.tga");
        c.str("top");
        for f in 0..frames {
            let k = f as f32;
            c.f32(5.0 + k);
            for v in 0..3 {
                let x = v as f32 + 10.0 * k;
                c.fs(&[x, x + 0.5, x + 0.25]).fs(&[0.0, 0.0, 1.0]).fs(&[0.5, v as f32]);
            }
            c.fs(&[k, k, 9.0]);
            c.fs(&IDENTITY);
            c.fs(&[-1.0, -2.0, -3.0, 1.0, 2.0, 3.0 + k]);
        }
        for _ in 0..subs {
            let sub = current(1, 0);
            c.0.extend_from_slice(&sub.0);
        }
        c
    }

    fn read(bytes: Vec<u8>, shared: &[(u32, i32)]) -> (CemModel, usize) {
        let mut io = BufferIo::reader(bytes);
        let m = load(&mut io, SharedFrames(shared));
        (m, io.position)
    }

    #[test]
    fn a_current_model_reads_whole() {
        let bytes = current(2, 0).0;
        let n = bytes.len();
        let (m, pos) = read(bytes, &[]);
        assert_eq!(m.result, OK);
        assert_eq!(pos, n);
        let c = m.counts.unwrap();
        assert_eq!(c, Counts { polygon: 1, vertex: 3, tag_point: 1, material: 1, frame: 2, sub_model: 0, lod: 2 });
        assert_eq!(m.name, b"hut");
        assert_eq!(m.center, vec![1.0, 2.0, 3.0]);
        assert_eq!(m.lods[0], CemPolygons { created: None, indices: vec![0, 1, 2] });
        assert_eq!(m.lods[1], CemPolygons { created: Some(1), indices: vec![2, 1, 0] });
        assert_eq!(m.materials[0].name, b"wood");
        assert_eq!(m.materials[0].values, vec![7, 0, 1, 0, 1, 0, 3]);
        assert_eq!(m.materials[0].texture_filename, b"wood.tga");
        assert_eq!(m.tag_point_names, vec![b"top".to_vec()]);
        let f = &m.frames[1];
        assert_eq!((f.radius, f.target), (Some(6.0), 1));
        assert_eq!(f.points[2], Ge3DPoint::new(12.0, 12.5, 12.25));
        assert_eq!(f.uvs[2], [0.5, 2.0]);
        assert_eq!(f.tag_points, vec![Ge3DPoint::new(1.0, 1.0, 9.0)]);
        assert_eq!(f.transform, IDENTITY.to_vec());
        assert_eq!(f.bbox, vec![-1.0, -2.0, -3.0, 1.0, 2.0, 4.0]);
        assert!(m.sub_models.is_empty());
    }

    #[test]
    fn a_shared_frame_reads_into_the_last_frame() {
        let (m, _) = read(current(3, 0).0, &[(1, -1)]);
        assert_eq!(m.result, OK);
        assert_eq!(m.frames.iter().map(|f| f.target).collect::<Vec<_>>(), vec![0, 2, 2]);
        assert_eq!(SharedFrames(&[(1, -1), (3, 1)]).physical_frames(5), vec![0, 0, 2, 4, 4]);
        // an offset to a later shared frame takes that frame's own index; one off the ends is ignored
        assert_eq!(SharedFrames(&[(0, -1), (1, 1), (2, -1)]).physical_frames(3), vec![0, 2, 2]);
    }

    #[test]
    fn the_header_is_checked() {
        let mut bad_tag = current(1, 0);
        bad_tag.0[3] = b'G';
        for (bytes, want) in [
            (bad_tag.0, FAILED),
            (Cem::header(3, 0).0, FAILED),
            (Cem::header(2, 4).0, FAILED),
            (Cem::header(1, 9).0, FAILED),
            (b"SS".to_vec(), END_OF_FILE),
            (b"SSMF\x02".to_vec(), END_OF_FILE),
        ] {
            let (m, pos) = read(bytes, &[]);
            assert_eq!((m.result, pos, m.counts), (want, 0, None));
        }
    }

    #[test]
    fn zero_counts_fail_before_the_allocation() {
        for zero in [0, 1, 3, 4] {
            let mut c = Cem::header(2, 0);
            for k in 0..7 {
                c.u32(if k == zero { 0 } else { 1 });
            }
            let (m, pos) = read(c.0, &[]);
            assert_eq!((m.result, pos, m.counts), (FAILED, 0, None), "count {zero}");
        }
    }

    #[test]
    fn every_cut_fails_back_to_the_start_and_keeps_a_prefix() {
        let whole = current(2, 0).0;
        let (full, _) = read(whole.clone(), &[]);
        for cut in 0..whole.len() {
            let (m, pos) = read(whole[..cut].to_vec(), &[]);
            assert_ne!(m.result, OK, "cut {cut}");
            assert_eq!(pos, 0, "cut {cut}");
            for (a, b) in m.frames.iter().zip(&full.frames) {
                assert!(b.points.starts_with(&a.points) && b.normals.starts_with(&a.normals));
                assert!(b.uvs.starts_with(&a.uvs) && b.transform.starts_with(&a.transform));
                assert!(b.bbox.starts_with(&a.bbox) && a.points.len() <= a.normals.len() + 1);
            }
            for (a, b) in m.materials.iter().zip(&full.materials) {
                assert!(b.values.starts_with(&a.values));
            }
            assert!(full.center.starts_with(&m.center));
        }
    }

    #[test]
    fn a_failing_sub_model_is_kept_and_ignored() {
        let mut bytes = current(1, 2).0;
        let one = current(1, 0).0.len();
        bytes.truncate(bytes.len() - 5); // the second sub-model is cut
        let sub_start = bytes.len() + 5 - one;
        let (m, pos) = read(bytes, &[]);
        assert_eq!(m.result, OK);
        assert_eq!(m.sub_models.len(), 2);
        assert_eq!(m.sub_models[0].result, OK);
        assert_ne!(m.sub_models[1].result, OK);
        assert!(m.sub_models[1].counts.is_some());
        // the failed sub-model put the file back where it started
        assert_eq!(pos, sub_start);
    }

    #[test]
    fn strings_end_at_the_first_nul_and_fail_empty() {
        let mut c = Cem::default();
        c.u32(4).0.extend_from_slice(b"ab\0c");
        let mut io = BufferIo::reader(c.0);
        let mut s = b"old".to_vec();
        assert_eq!(load_string(&mut io, &mut s), OK);
        assert_eq!(s, b"ab");
        let mut io = BufferIo::reader(vec![9, 0, 0, 0, b'x']);
        assert_eq!(load_string(&mut io, &mut s), END_OF_FILE);
        assert!(s.is_empty());
        let mut io = BufferIo::reader(vec![0, 0, 0, 0]);
        s = b"old".to_vec();
        assert_eq!(load_string(&mut io, &mut s), OK);
        assert!(s.is_empty());
    }

    #[test]
    fn a_failed_lod_does_not_stop_the_loop() {
        // the result is the last LOD's: with the file at its end every later read fails too
        let mut c = Cem::header(2, 0);
        c.u32(1).u32(3).u32(0).u32(1).u32(1).u32(0).u32(3).str("m").fs(&[0.0; 3]);
        c.u32(1).u32(0).u32(1); // the third index of the first LOD is missing
        let (m, _) = read(c.0, &[]);
        assert_eq!(m.result, END_OF_FILE);
        assert_eq!(m.lods[0].indices, vec![0, 1]);
        assert_eq!(m.lods[1].created, None);
    }
}
