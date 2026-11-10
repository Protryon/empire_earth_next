//! The C ABI of the model `GEModel::StoreLoaded` stores (`model_store.rs`; 'GEModel Load.cpp') and of `GEModel`'s
//! geometry (`model_geom.rs`). `ee_openformats` decodes a glTF into a [`CemModel`] (`ee_of_gltf_to_model`, sub-models
//! included); the C++ allocates the model's arrays from the counts and stores the model, walking it through the views
//! below. The views borrow from the handle (a `CemModel`, opaque to the C++ as `EeGeCemModel`), which the C++ frees
//! with `ee_ge_cem_free` once it has copied them.

use std::ffi::c_long;
use std::ptr;

use crate::model_geom::{self, EeGeScaleRemoval};
use crate::model_store::CemModel;
use crate::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};

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

/// Frees a model (`ee_of_gltf_to_model`'s; not a sub-model's handle).
///
/// # Safety
/// `model` came from `ee_of_gltf_to_model` and is freed once.
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

// ---- GEModel's geometry (model_geom.rs)

/// Builds a slice from a C pointer and a count (empty for a null pointer).
///
/// # Safety
/// `data` is null or valid for `count` elements.
unsafe fn slice<'a, T>(data: *const T, count: usize) -> &'a [T] {
    if data.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, count) }
    }
}

/// `ComputeBoundingBoxes` for one frame's points.
///
/// # Safety
/// `points` holds `count` points; `out` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_frame_box(points: *const Ge3DPoint, count: usize, out: *mut GeBoundingBox) {
    // SAFETY: the caller's contract
    unsafe { *out = model_geom::frame_box(slice(points, count)) };
}

/// `ComputeBoundingVolumes`' centre from the frames' boxes.
///
/// # Safety
/// `boxes` holds `count` boxes; `out` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_center(boxes: *const GeBoundingBox, count: usize, out: *mut Ge3DPoint) {
    // SAFETY: the caller's contract
    unsafe { *out = model_geom::center(slice(boxes, count)) };
}

/// `ComputeBoundingVolumes`' radius of one frame.
///
/// # Safety
/// `center` is valid; `points` holds `count` points.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_radius(center: *const Ge3DPoint, points: *const Ge3DPoint, count: usize) -> f32 {
    // SAFETY: the caller's contract
    unsafe { model_geom::radius(*center, slice(points, count)) }
}

/// `ComputeModelNormals` for one frame: `out` receives one normal per point.
///
/// # Safety
/// `points` and `out` hold `count` elements; `indices` holds `index_count`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_vertex_normals(
    points: *const Ge3DPoint,
    count: usize,
    indices: *const u32,
    index_count: usize,
    out: *mut GeVector,
) {
    // SAFETY: the caller's contract
    let normals = unsafe { model_geom::vertex_normals(slice(points, count), slice(indices, index_count)) };
    for (k, n) in normals.into_iter().enumerate() {
        // SAFETY: `out` holds `count` vectors
        unsafe { *out.add(k) = n };
    }
}

/// `PrepareTransforms`' graphics transform.
///
/// # Safety
/// The pointers are valid; `out` may be any of the inputs.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_graphics_transform(
    internal: *const GeTransformation,
    pre: *const GeTransformation,
    post: *const GeTransformation,
    scale: f32,
    out: *mut GeTransformation,
) {
    // SAFETY: the caller's contract (every input is read before the store)
    unsafe { *out = model_geom::graphics_transform(&*internal, &*pre, &*post, scale) };
}

/// `RemoveModelScale`'s matrices for one frame.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_scale_removal(
    internal: *const GeTransformation,
    parent: *const GeTransformation,
    x: f32,
    y: f32,
    z: f32,
    out: *mut EeGeScaleRemoval,
) {
    // SAFETY: the caller's contract
    unsafe { *out = model_geom::scale_removal(&*internal, &*parent, x, y, z) };
}

/// `SortTagPoints`' order: `out[i]` is the index swapped into position `i`.
///
/// # Safety
/// `first` and `out` hold `count` elements.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_tag_point_order(first: *const i8, count: usize, out: *mut u32) {
    // SAFETY: the caller's contract
    let order = model_geom::tag_point_order(unsafe { slice(first, count) });
    for (k, v) in order.into_iter().enumerate() {
        // SAFETY: `out` holds `count` values
        unsafe { *out.add(k) = v };
    }
}

/// `Verify` for one frame: `ranges` holds per material its LOD 0 polygon start, polygon count and first vertex.
///
/// # Safety
/// `points` holds `count` points, `indices` `index_count` values, `ranges` `range_count` triples.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_model_frame_is_sound(
    points: *const Ge3DPoint,
    count: usize,
    indices: *const u32,
    index_count: usize,
    ranges: *const u32,
    range_count: usize,
) -> bool {
    // SAFETY: the caller's contract
    let flat = unsafe { slice(ranges, range_count * 3) };
    let triples: Vec<[u32; 3]> = flat.as_chunks::<3>().0.to_vec();
    // SAFETY: the caller's contract
    unsafe { model_geom::frame_is_sound(slice(points, count), slice(indices, index_count), &triples) }
}
