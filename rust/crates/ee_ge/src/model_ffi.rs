//! The C ABI of `GEModel`'s file reader (`model_file.rs`, `legacy.rs`), called by `GEModel::Load`
//! ('GEModel Load.cpp'). Rust reads the whole model, its sub-models included, into a [`CemModel`]; the C++ then
//! allocates the model's arrays from the counts and stores what was read, walking the result through the views
//! below. The views borrow from the handle (a `CemModel`, opaque to the C++ as `EeGeCemModel`), which the C++ frees
//! with `ee_ge_cem_free` once it has copied them.

use std::ffi::c_long;
use std::ptr;

use crate::io::{CppIo, EeGeFieldIo};
use crate::model_file::{self, CemModel, SharedFrames};
use crate::types::{Ge3DPoint, GeVector};

/// One entry of the shared frames map (`std::map<unsigned long, long>`: frame, offset to the frame it shares).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeSharedFrame {
    pub frame: u32,
    pub offset: i32,
}

/// Borrowed bytes (no terminating NUL).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeBytes {
    pub data: *const u8,
    pub length: usize,
}

/// The model's result, counts and scalar fields.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeCemInfo {
    pub result: c_long,
    /// The counts were read and the arrays are to be allocated with them.
    pub allocated: bool,
    pub polygon_count: u32,
    pub vertex_count: u32,
    pub tag_point_count: u32,
    pub material_count: u32,
    pub frame_count: u32,
    pub sub_model_count: u32,
    pub lod_count: u32,
    pub name: EeGeBytes,
    /// The centre's values read (x, y, z; a prefix).
    pub center: *const f32,
    pub center_count: usize,
    /// The sub-models created (all of `sub_model_count` when the model's own chunks loaded, else none).
    pub sub_models_loaded: usize,
}

/// One LOD's polygons: `created` when the LOD's array is created with `count` polygons (LOD 0 has the allocation's);
/// `indices` the vertex indices set, three per polygon (a prefix).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeCemLod {
    pub created: bool,
    pub count: u32,
    pub indices: *const u32,
    pub index_count: usize,
}

/// One material: its name, the values set in order (texture index, polygon start and length per LOD, vertex start,
/// vertex length; a prefix) and its texture file name.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeCemMaterial {
    pub name: EeGeBytes,
    pub values: *const u32,
    pub value_count: usize,
    pub texture_filename: EeGeBytes,
}

/// One frame: the radius when read; the vertices and tag points for frame `target`'s arrays; the transform's
/// elements read (row by row) and the box's corner values read (prefixes).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeCemFrame {
    pub has_radius: bool,
    pub radius: f32,
    pub target: u32,
    pub points: *const Ge3DPoint,
    pub point_count: usize,
    pub normals: *const GeVector,
    pub normal_count: usize,
    /// u, v pairs.
    pub uvs: *const f32,
    pub uv_count: usize,
    pub tag_points: *const Ge3DPoint,
    pub tag_point_count: usize,
    pub transform: *const f32,
    pub transform_count: usize,
    pub bbox: *const f32,
    pub bbox_count: usize,
}

fn bytes(b: &[u8]) -> EeGeBytes {
    EeGeBytes { data: b.as_ptr(), length: b.len() }
}

/// Reads a model from the file's current position (`GEModel::Load` once the file is open): the header, the body
/// by version, the sub-models; on failure the file is put back. Never null.
///
/// # Safety
/// `io` is a valid field stream; `shared` holds `shared_count` entries sorted by frame (or is null when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_load(
    io: *const EeGeFieldIo,
    shared: *const EeGeSharedFrame,
    shared_count: usize,
) -> *mut CemModel {
    let entries: Vec<(u32, i32)> = if shared_count == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(shared, shared_count) }.iter().map(|e| (e.frame, e.offset)).collect()
    };
    // SAFETY: the caller's contract
    let mut file = CppIo(unsafe { *io });
    let model = model_file::load(&mut file, SharedFrames(&entries));
    Box::into_raw(Box::new(model))
}

/// Frees a model read by `ee_ge_model_load` (not a sub-model's handle).
///
/// # Safety
/// `model` came from `ee_ge_model_load` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_free(model: *mut CemModel) {
    if !model.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(model) });
    }
}

/// The model's result, counts and scalars.
///
/// # Safety
/// `model` is live; `out` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_info(model: *const CemModel, out: *mut EeGeCemInfo) {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    let c = m.counts.unwrap_or_default();
    let info = EeGeCemInfo {
        result: m.result,
        allocated: m.counts.is_some(),
        polygon_count: c.polygon,
        vertex_count: c.vertex,
        tag_point_count: c.tag_point,
        material_count: c.material,
        frame_count: c.frame,
        sub_model_count: c.sub_model,
        lod_count: c.lod,
        name: bytes(&m.name),
        center: m.center.as_ptr(),
        center_count: m.center.len(),
        sub_models_loaded: m.sub_models.len(),
    };
    // SAFETY: the caller's contract
    unsafe { *out = info };
}

/// LOD `lod`'s polygons (`lod` below the LOD count of an allocated model).
///
/// # Safety
/// `model` is live; `out` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_lod(model: *const CemModel, lod: u32, out: *mut EeGeCemLod) {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    let l = &m.lods[lod as usize];
    let view = EeGeCemLod {
        created: l.created.is_some(),
        count: l.created.unwrap_or(0),
        indices: l.indices.as_ptr(),
        index_count: l.indices.len(),
    };
    // SAFETY: the caller's contract
    unsafe { *out = view };
}

/// Material `index`.
///
/// # Safety
/// `model` is live; `out` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_material(model: *const CemModel, index: u32, out: *mut EeGeCemMaterial) {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    let mat = &m.materials[index as usize];
    let view = EeGeCemMaterial {
        name: bytes(&mat.name),
        values: mat.values.as_ptr(),
        value_count: mat.values.len(),
        texture_filename: bytes(&mat.texture_filename),
    };
    // SAFETY: the caller's contract
    unsafe { *out = view };
}

/// Tag point `index`'s name.
///
/// # Safety
/// `model` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_tag_point_name(model: *const CemModel, index: u32) -> EeGeBytes {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    bytes(&m.tag_point_names[index as usize])
}

/// Frame `index`.
///
/// # Safety
/// `model` is live; `out` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_frame(model: *const CemModel, index: u32, out: *mut EeGeCemFrame) {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    let f = &m.frames[index as usize];
    let view = EeGeCemFrame {
        has_radius: f.radius.is_some(),
        radius: f.radius.unwrap_or(0.0),
        target: f.target,
        points: f.points.as_ptr(),
        point_count: f.points.len(),
        normals: f.normals.as_ptr(),
        normal_count: f.normals.len(),
        uvs: f.uvs.as_ptr().cast(),
        uv_count: f.uvs.len(),
        tag_points: f.tag_points.as_ptr(),
        tag_point_count: f.tag_points.len(),
        transform: f.transform.as_ptr(),
        transform_count: f.transform.len(),
        bbox: f.bbox.as_ptr(),
        bbox_count: f.bbox.len(),
    };
    // SAFETY: the caller's contract
    unsafe { *out = view };
}

/// Sub-model `index` (borrowed from its parent: never freed on its own).
///
/// # Safety
/// `model` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_sub_model(model: *const CemModel, index: u32) -> *const CemModel {
    // SAFETY: the caller's contract
    let m = unsafe { &*model };
    match m.sub_models.get(index as usize) {
        Some(sub) => sub,
        None => ptr::null(),
    }
}
