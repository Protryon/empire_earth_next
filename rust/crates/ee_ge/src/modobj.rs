//! `GEModel` as a whole Rust object (R4.3, ninth part): a model's transforms, centre, name, shared frames, frame
//! arrays (internal transforms, vertex arrays, tag points, boxes, radii), LOD polygon arrays, materials, texture and
//! tag point names, sub-models and counts, with every method of GEModel.cpp and 'GEModel Load.cpp' (the
//! constructors, the destructor's releases, `AllocateInternalData`, `StoreLoaded`, `LoadTexturesForScreen`, the
//! bounding volumes, normals, transforms and scale removal, the frame setters over the sub-models, `SetSubModel`,
//! `SetTexture`, `SizeOf`, `SortTagPoints`, `Verify`) and the header's accessors.
//!
//! As the managers and GETexture (`manobj.rs`, `texobj.rs`, `texobj_tex.rs`): the C++ `GEModel` stays the type the
//! game, the renderer, the effects and the managers use; its methods are one-line forwarders and its only member is
//! this object. The calls the original made on other objects that cross into C++ (the rasterizer, the vertex and
//! polygon arrays, making and releasing the materials, the texture manager, its manager, its sub-models' releases,
//! making and deleting the `UString`s) are one host operation each (`EeGeModOp`, `GEModel::Host`), in the original's
//! order. Since R4.3 part 17 the rest is read in place (`cppobj.rs`): the materials' Rust objects (`smallobj.rs`) are
//! read and set directly, as are the strings' records (`ee_ustring`), the reference counts taken and a sub-model's
//! object, whose fields are read and written on it; the walks over the
//! sub-models (frames, opacity, textures, sizes, sorting, verification) recurse in Rust as the original's non-virtual
//! calls did. No reference to an object lives across a host call.
//!
//! The arrays the header's accessors return references into (transforms, boxes, tag points, radii) are Rust
//! vectors of the `#[repr(C)]` mirrors (`types.rs`), allocated once; the accessors index them in C++ as the inline
//! accessors indexed the members. The vertex and polygon arrays, the materials and the `UString`s stay C++ objects
//! (`GERasterizer`'s, `GEMaterial`, `new UString[n]`), held by pointer.

use std::ffi::{c_long, c_ulong, c_void};
use std::ptr;

use crate::cppobj;
use crate::model_ffi::EeGeSharedFrame;
use crate::model_geom;
use crate::model_store::CemModel;
use crate::smallobj::{ee_ge_mat_get, ee_ge_mat_name, ee_ge_mat_set, ee_ge_mat_set_texture, EeGeMat, EeGeMatField};
use crate::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};

/// `kErrorOK`.
const OK: c_long = 0;
/// `kMipFilterPoint` (lle/GERenderState.h).
const MIP_FILTER_POINT: c_ulong = 1;
/// `SetStage...(mode, -1)`: every stage.
const EVERY_STAGE: isize = -1;
/// The pointers' size in `SizeOf`'s estimate (`sizeof(GEModel*)` and the like).
const POINTER: c_ulong = std::mem::size_of::<usize>() as c_ulong;

/// The calls a model makes on other objects. `obj` is the object called; `a`..`c` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeModOp {
    /// `GERasterizer::GetActiveRasterizer()`.
    RasterActive = 1,
    /// `obj->CreateVertexArray(a, b)` (flags, count).
    RasterVertexArray,
    /// `obj->CreatePolygonArray(a)`.
    RasterPolygonArray,
    /// `obj->GetScreenID()`.
    RasterScreenId,
    /// `obj->GetScreen(a)->GetTextureManager()`.
    RasterTextureManager,
    /// `delete (GEVertexArray*)obj`.
    VertexDelete,
    /// `obj->GetPoint(((GE3DPoint*)b)[i], i)` for `i < a`.
    VertexGather,
    /// `obj->GetPoint(*(GE3DPoint*)b, a)`.
    VertexGetPoint,
    /// `obj->SetPoint(*(const GE3DPoint*)b, a)`.
    VertexSetPoint,
    /// `obj->GetNormal(*(GEVector*)b, a)`.
    VertexGetNormal,
    /// `obj->SetNormal(*(const GEVector*)b, a)`.
    VertexSetNormal,
    /// `obj->SetTextureCoordinate(GETextureCoordinate(b[0], b[1]), a)` (`b` two floats).
    VertexSetUv,
    /// `obj->GetSize()`.
    VertexSize,
    /// `obj->SizeOf()`.
    VertexSizeOf,
    /// `delete (GEPolygonArray*)obj`.
    PolygonDelete,
    /// `GetVertexIndex1`, `2` and `3` of each polygon `i < a` into `((uint32_t*)b)[3 i ..]`.
    PolygonGather,
    /// `obj->SetVertexIndex{c}(a, b)` (`c` 1, 2 or 3).
    PolygonSetIndex,
    /// `obj->SizeOf()`.
    PolygonSizeOf,
    /// `new GEMaterial(a, b)` (screens, LODs).
    MaterialNew,
    /// `obj->UReferenceCounter::Dereference()`.
    MaterialDereference,
    /// `obj->SizeOf()`.
    MaterialSizeOf,
    /// `new GEModel((GEModelManager*)obj)`.
    ModelNew,
    /// `obj->UReferenceCounter::Dereference()`.
    ModelBaseDereference,
    /// `((GEModelManager*)obj)->RemoveModel((GEModel*)a)`.
    ManagerRemove,
    /// `new UString`.
    StrNew,
    /// `delete (UString*)obj`.
    StrDelete,
    /// `new UString[a]`.
    StrArrayNew,
    /// `delete[] (UString*)obj`.
    StrArrayDelete,
    /// `((GETextureManager*)obj)->LoadTexture(*(const UString*)a, *(const GETextureDescription*)b)`.
    TexLoad,
    /// `obj->SetStageMinFilterMode(a, b)`.
    TexMinFilter,
    /// `obj->SetStageMagFilterMode(a, b)`.
    TexMagFilter,
    /// `obj->SetStageMipFilterMode(a, b)`.
    TexMipFilter,
}

/// The host: `owner` is the C++ model of the object calling.
pub type EeGeModHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeModOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize;

/// A field the header's accessors read (`ee_ge_mod_get`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeModVal {
    /// `mManager`.
    Manager = 1,
    /// `mParentModel`.
    Parent,
    /// `mGraphicsCurrentFrame`.
    CurrentFrame,
    /// `mGraphicsNextFrame`.
    NextFrame,
    /// `mSubModelCount`.
    SubModelCount,
    /// `mFrameCount`.
    FrameCount,
    /// `mMaterialCount`.
    MaterialCount,
    /// `mPolygonCount`.
    PolygonCount,
    /// `mVertexCount`.
    VertexCount,
    /// `mTagPointCount`.
    TagPointCount,
    /// `mLODCount`.
    LodCount,
    /// `mSharedFrames.size()`.
    SharedFrameCount,
}

/// A float the header's accessors read (`ee_ge_mod_getf`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeModFloat {
    /// `mGraphicsScale`.
    GraphicsScale = 1,
    /// `mModelOpacity`.
    Opacity,
    /// `mFractionalFramePosition`.
    FractionalFramePosition,
}

/// An array or member the header's accessors return a pointer to (`ee_ge_mod_ptr`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeModPtr {
    /// `mInternalTransforms` (`GETransformation[frames]`).
    InternalTransforms = 1,
    /// `mSubModels` (`GEModel*[sub-models]`).
    SubModels,
    /// `mPolygonArrays` (`GEPolygonArray*[LODs]`).
    PolygonArrays,
    /// `mVertexArrays` (`GEVertexArray*[frames]`).
    VertexArrays,
    /// `mMaterials` (`GEMaterial*[materials]`).
    Materials,
    /// `mFrameTagPoints` (`GE3DPoint*[frames]`).
    FrameTagPoints,
    /// `mBoundingBoxes` (`GEBoundingBox[frames]`).
    BoundingBoxes,
    /// `mGraphicsBoundingBox` (the current frame's box).
    GraphicsBoundingBox,
    /// `mTagPoints` (the current frame's tag points).
    TagPoints,
    /// `mTextureFilenames` (`UString[materials]`).
    TextureFilenames,
    /// `mTagPointNames` (`UString[tag points]`).
    TagPointNames,
    /// `mBoundingRadii` (`float[frames]`).
    BoundingRadii,
    /// `mName` (`UString`).
    Name,
    /// `mGraphicsPreModelTransform`.
    PreTransform,
    /// `mGraphicsPostModelTransform`.
    PostTransform,
    /// `mGraphicsTransform`.
    GraphicsTransform,
    /// `mCenterPoint`.
    Center,
}

/// A transform a setter writes (`ee_ge_mod_set_transform`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeModXf {
    /// `SetGraphicsPreModelTransform`.
    Pre = 1,
    /// `SetGraphicsPostModelTransform`.
    Post,
    /// `SetInternalTransform(frame, ...)`.
    Internal,
}

/// The model.
pub struct EeGeModel {
    host: EeGeModHostFn,
    /// The C++ model (`this`).
    owner: usize,
    pre: GeTransformation,
    post: GeTransformation,
    graphics: GeTransformation,
    center: Ge3DPoint,
    /// `mName`: a C++ `UString` (made with the object, freed with it).
    name: usize,
    /// `mSharedFrames` (frame -> offset to the frame whose arrays it uses), sorted by frame as the `std::map`.
    shared: Vec<(c_ulong, c_long)>,
    internal: Option<Vec<GeTransformation>>,
    manager: usize,
    parent: usize,
    subs: Option<Vec<usize>>,
    polygons: Option<Vec<usize>>,
    vertices: Option<Vec<usize>>,
    materials: Option<Vec<usize>>,
    /// `mFrameTagPoints`: per frame a `tag_len` array this object allocated (a shared frame's is its source's).
    frame_tags: Option<Vec<*mut Ge3DPoint>>,
    /// The tag point arrays' length (the tag point count when they were allocated).
    tag_len: usize,
    boxes: Option<Vec<GeBoundingBox>>,
    /// `mGraphicsBoundingBox`: the current frame's box.
    graphics_box: *mut GeBoundingBox,
    /// `mTagPoints`: the current frame's tag points.
    tags: *mut Ge3DPoint,
    /// `mTextureFilenames` (`new UString[materials]`, NULL before the allocation).
    texture_filenames: usize,
    /// `mTagPointNames` (`new UString[tag points]`, NULL without tag points).
    tag_point_names: usize,
    radii: Option<Vec<f32>>,
    graphics_scale: f32,
    opacity: f32,
    fractional: f32,
    current_frame: c_ulong,
    next_frame: c_ulong,
    sub_count: c_ulong,
    frame_count: c_ulong,
    material_count: c_ulong,
    polygon_count: c_ulong,
    vertex_count: c_ulong,
    tag_count: c_ulong,
    lod_count: c_ulong,
    /// `mClipToPlane` (unset by the original's constructors: false here).
    clip: [bool; 6],
}

/// Every access to an object is one short statement: a host call may come back in and use it.
///
/// # Safety
/// `p` is a live object from `ee_ge_mod_new`.
unsafe fn md<'a>(p: *mut EeGeModel) -> &'a mut EeGeModel {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

/// A material's Rust object (the C++ `GEMaterial*` the model holds, read in place: `cppobj.rs`).
///
/// # Safety
/// `material` is a live material (never NULL: the original called through it).
unsafe fn mat(material: usize) -> *mut EeGeMat {
    // SAFETY: the caller's contract
    unsafe { cppobj::counted(material) }
}

/// `v[i]` of an optional array (0 or NULL past it or without it: the original read past its arrays).
fn at<T: Copy + Default>(v: &Option<Vec<T>>, i: c_ulong) -> T {
    v.as_ref().and_then(|v| v.get(i as usize).copied()).unwrap_or_default()
}

/// `v[i] = value` where the array has an `i` (the original wrote past its arrays).
fn put<T>(v: &mut Option<Vec<T>>, i: c_ulong, value: T) {
    if let Some(slot) = v.as_mut().and_then(|v| v.get_mut(i as usize)) {
        *slot = value;
    }
}

/// The base of an optional array (NULL without one).
fn base<T>(v: &mut Option<Vec<T>>) -> *mut T {
    v.as_mut().map_or(ptr::null_mut(), |v| v.as_mut_ptr())
}

/// `GEBoundingBox()`: the unit box at the origin.
fn unit_box() -> GeBoundingBox {
    GeBoundingBox::new(Ge3DPoint::default(), Ge3DPoint::new(1.0, 1.0, 1.0))
}

impl EeGeModel {
    /// A host call made by model `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeModOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeModOp, obj: usize, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (md(p).host, md(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b, c) }
    }

    /// The Rust object of model `model` (`p`'s own for `p`'s owner).
    ///
    /// # Safety
    /// `p` is live; `model` a live model.
    unsafe fn obj(p: *mut Self, model: usize) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe {
            if model == md(p).owner {
                return p;
            }
            cppobj::counted(model)
        }
    }

    /// The sub-models' Rust objects (a NULL entry skipped: the original called through it).
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn sub_objects(p: *mut Self, count: c_ulong) -> Vec<*mut Self> {
        let mut out = Vec::new();
        for i in 0..count {
            // SAFETY: the caller's contract
            unsafe {
                let sub = at(&md(p).subs, i);
                if sub != 0 {
                    out.push(Self::obj(p, sub));
                }
            }
        }
        out
    }

    /// `&array[i]` of a C++ `UString` array.
    ///
    /// # Safety
    /// `p` is live; `array` a `UString` array with an `i`.
    unsafe fn str_at(_p: *mut Self, array: usize, i: c_ulong) -> usize {
        cppobj::str_at(array, i as usize)
    }

    /// `*target = UString(bytes)`.
    ///
    /// # Safety
    /// `p` is live; `target` a `UString`.
    unsafe fn str_set(_p: *mut Self, target: usize, bytes: &[u8]) {
        // SAFETY: the caller's contract
        unsafe { cppobj::str_set_bytes(target, bytes) };
    }

    /// A frame's points (`GetPoint` of each of `count`), through the host.
    ///
    /// # Safety
    /// `p` is live; `array` a vertex array or NULL.
    unsafe fn gather_points(p: *mut Self, array: usize, count: c_ulong) -> Vec<Ge3DPoint> {
        let mut points = vec![Ge3DPoint::default(); count as usize];
        if count > 0 && array != 0 {
            // SAFETY: the caller's contract; `points` holds `count`
            unsafe {
                Self::op(p, EeGeModOp::VertexGather, array, count as isize, points.as_mut_ptr() as isize, 0);
            }
        }
        points
    }

    /// A polygon array's vertex indices, three per polygon.
    ///
    /// # Safety
    /// `p` is live; `array` a polygon array or NULL.
    unsafe fn gather_indices(p: *mut Self, array: usize, count: c_ulong) -> Vec<u32> {
        let mut indices = vec![0u32; count as usize * 3];
        if count > 0 && array != 0 {
            // SAFETY: the caller's contract; `indices` holds `3 count`
            unsafe {
                Self::op(p, EeGeModOp::PolygonGather, array, count as isize, indices.as_mut_ptr() as isize, 0);
            }
        }
        indices
    }

    fn fresh(host: EeGeModHostFn, owner: usize, manager: usize) -> Self {
        Self {
            host,
            owner,
            pre: GeTransformation::IDENTITY,
            post: GeTransformation::IDENTITY,
            graphics: GeTransformation::IDENTITY,
            center: Ge3DPoint::default(),
            name: 0,
            shared: Vec::new(),
            internal: None,
            manager,
            parent: 0,
            subs: None,
            polygons: None,
            vertices: None,
            materials: None,
            frame_tags: None,
            tag_len: 0,
            boxes: None,
            graphics_box: ptr::null_mut(),
            tags: ptr::null_mut(),
            texture_filenames: 0,
            tag_point_names: 0,
            radii: None,
            graphics_scale: 1.0,
            opacity: 1.0,
            fractional: 0.0,
            current_frame: 0,
            next_frame: 0,
            sub_count: 0,
            frame_count: 0,
            material_count: 0,
            polygon_count: 0,
            vertex_count: 0,
            tag_count: 0,
            lod_count: 1,
            clip: [false; 6],
        }
    }

    /// `GEModel(manager)`: an empty model with one LOD and no data (the counts and arrays the destructor reads are
    /// zero and NULL, as the port's constructor set them).
    ///
    /// # Safety
    /// `host` serves `owner` from now on.
    pub unsafe fn create(host: EeGeModHostFn, owner: usize, manager: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(Self::fresh(host, owner, manager)));
        // SAFETY: `p` is live
        unsafe {
            let name = Self::op(p, EeGeModOp::StrNew, 0, 0, 0, 0) as usize;
            md(p).name = name;
        }
        p
    }

    /// `GEModel(counts, name, flags, manager)`: a named model with its data allocated (no shared frames).
    ///
    /// # Safety
    /// As `create`; `name` a `UString`.
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn create_with_counts(
        host: EeGeModHostFn,
        owner: usize,
        counts: [c_ulong; 6],
        name: usize,
        flags: c_ulong,
        manager: usize,
    ) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe {
            let p = Self::create(host, owner, manager);
            let own = md(p).name;
            cppobj::str_assign(own, name);
            let m = md(p);
            m.polygon_count = counts[0];
            m.vertex_count = counts[1];
            m.material_count = counts[2];
            m.tag_count = counts[3];
            m.frame_count = counts[4];
            m.sub_count = counts[5];
            Self::allocate(p, flags, &[]);
            p
        }
    }

    /// The destructor: the frames' vertex arrays and tag points (a shared frame's once, with its source), the LOD
    /// polygon arrays, the materials' references, the name arrays, the sub-models (`SetSubModel(NULL, i)`), the name;
    /// the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let mut i: c_ulong = 0;
            while i < md(p).frame_count {
                if !Self::is_shared(p, i) {
                    let array = at(&md(p).vertices, i);
                    if array != 0 {
                        Self::op(p, EeGeModOp::VertexDelete, array, 0, 0, 0);
                    }
                    let tags = at(&md(p).frame_tags, i);
                    if !tags.is_null() {
                        let len = md(p).tag_len;
                        drop(Box::from_raw(ptr::slice_from_raw_parts_mut(tags, len)));
                    }
                }
                i += 1;
            }
            i = 0;
            while i < md(p).lod_count && md(p).polygons.is_some() {
                let array = at(&md(p).polygons, i);
                if array != 0 {
                    Self::op(p, EeGeModOp::PolygonDelete, array, 0, 0, 0);
                }
                i += 1;
            }
            let m = md(p);
            m.boxes = None;
            m.graphics_box = ptr::null_mut();
            m.radii = None;
            m.frame_tags = None;
            m.tags = ptr::null_mut();
            m.polygons = None;
            m.vertices = None;
            m.internal = None;
            if md(p).materials.is_some() {
                i = 0;
                while i < md(p).material_count {
                    let material = at(&md(p).materials, i);
                    if material != 0 {
                        Self::op(p, EeGeModOp::MaterialDereference, material, 0, 0, 0);
                    }
                    i += 1;
                }
                md(p).materials = None;
            }
            let names = md(p).texture_filenames;
            if names != 0 {
                Self::op(p, EeGeModOp::StrArrayDelete, names, 0, 0, 0);
                md(p).texture_filenames = 0;
            }
            let names = md(p).tag_point_names;
            if names != 0 {
                Self::op(p, EeGeModOp::StrArrayDelete, names, 0, 0, 0);
                md(p).tag_point_names = 0;
            }
            if md(p).subs.is_some() {
                i = 0;
                while i < md(p).sub_count {
                    Self::set_sub_model(p, 0, i);
                    i += 1;
                }
                md(p).subs = None;
            }
            let name = md(p).name;
            if name != 0 {
                Self::op(p, EeGeModOp::StrDelete, name, 0, 0, 0);
            }
            drop(Box::from_raw(p));
        }
    }

    /// Whether frame `i` is in `mSharedFrames`.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn is_shared(p: *mut Self, i: c_ulong) -> bool {
        // SAFETY: the caller's contract
        unsafe { Self::shared_offset(p, i).is_some() }
    }

    /// `mSharedFrames.find(i)->second`.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn shared_offset(p: *mut Self, i: c_ulong) -> Option<c_long> {
        // SAFETY: the caller's contract
        let shared = unsafe { &md(p).shared };
        shared.binary_search_by_key(&i, |e| e.0).ok().map(|k| shared[k].1)
    }

    /// `AllocateInternalData`: a vertex array per frame not shared and the LOD 0 polygon array (the active
    /// rasterizer's), the transforms, sub-model slots, boxes, radii and tag point arrays per frame, a shared frame's
    /// arrays taken from the frame at its offset, the materials (one screen more than the rasterizer's id) and the
    /// name arrays. The original's allocation failures (`new` returning NULL) cannot happen: always `kErrorOK`.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn allocate(p: *mut Self, flags: c_ulong, shared: &[(c_ulong, c_long)]) -> c_long {
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            md(p).current_frame = 0;
            let rasterizer = Self::op(p, EeGeModOp::RasterActive, 0, 0, 0, 0) as usize;
            let frames = md(p).frame_count;
            let mut sorted = shared.to_vec();
            sorted.sort_by_key(|e| e.0);
            sorted.dedup_by_key(|e| e.0);
            md(p).vertices = Some(vec![0; frames as usize]);
            for i in 0..frames {
                if sorted.binary_search_by_key(&i, |e| e.0).is_err() {
                    let count = md(p).vertex_count;
                    let array = Self::op(p, EeGeModOp::RasterVertexArray, rasterizer, flags as isize, count as isize, 0)
                        as usize;
                    put(&mut md(p).vertices, i, array);
                }
            }
            let lods = md(p).lod_count;
            md(p).polygons = Some(vec![0; lods as usize]);
            let count = md(p).polygon_count;
            let array = Self::op(p, EeGeModOp::RasterPolygonArray, rasterizer, count as isize, 0, 0) as usize;
            put(&mut md(p).polygons, 0, array);
            let m = md(p);
            m.shared = sorted;
            m.internal = Some(vec![GeTransformation::IDENTITY; frames as usize]);
            m.subs = Some(vec![0; m.sub_count as usize]);
            m.boxes = Some(vec![unit_box(); frames as usize]);
            m.graphics_box = base(&mut m.boxes).wrapping_add(m.current_frame as usize);
            m.radii = Some(vec![0.0; frames as usize]);
            m.tag_len = m.tag_count as usize;
            let mut tags = vec![ptr::null_mut::<Ge3DPoint>(); frames as usize];
            if m.tag_count > 0 {
                for (i, slot) in tags.iter_mut().enumerate() {
                    if m.shared.binary_search_by_key(&(i as c_ulong), |e| e.0).is_err() {
                        let points = vec![Ge3DPoint::default(); m.tag_len].into_boxed_slice();
                        *slot = Box::into_raw(points) as *mut Ge3DPoint;
                    }
                }
            }
            m.tags = tags.first().copied().unwrap_or(ptr::null_mut());
            m.frame_tags = Some(tags);
            for k in 0..m.shared.len() {
                let (i, offset) = m.shared[k];
                if i >= frames {
                    continue;
                }
                let source = (offset as c_ulong).wrapping_add(i);
                let array = at(&m.vertices, source);
                put(&mut m.vertices, i, array);
                let points = at(&m.frame_tags, source);
                put(&mut m.frame_tags, i, points);
            }
            md(p).materials = Some(vec![0; md(p).material_count as usize]);
            for i in 0..md(p).material_count {
                let screen = Self::op(p, EeGeModOp::RasterScreenId, rasterizer, 0, 0, 0) as c_ulong;
                let lods = md(p).lod_count;
                let material =
                    Self::op(p, EeGeModOp::MaterialNew, 0, screen.wrapping_add(1) as isize, lods as isize, 0) as usize;
                put(&mut md(p).materials, i, material);
            }
            let count = md(p).material_count;
            let names = Self::op(p, EeGeModOp::StrArrayNew, 0, count as isize, 0, 0) as usize;
            md(p).texture_filenames = names;
            let count = md(p).tag_count;
            let names =
                if count > 0 { Self::op(p, EeGeModOp::StrArrayNew, 0, count as isize, 0, 0) as usize } else { 0 };
            md(p).tag_point_names = names;
            OK
        }
    }

    /// `StoreLoaded`: the arrays allocated from the decoded model's counts, then its name, centre, LOD polygons,
    /// material values and names, tag point names and frames stored where the original's `.cem` chunk readers stored
    /// them, then each decoded sub-model into a new model (its result ignored). A model the reader refused is left as
    /// constructed. Returns the model's result.
    ///
    /// # Safety
    /// `p` is live; `model` a decoded model (immutable for the call).
    pub unsafe fn store_loaded(p: *mut Self, model: &CemModel, shared: &[(c_ulong, c_long)]) -> c_long {
        let Some(c) = model.counts else {
            return model.result;
        };
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            let m = md(p);
            m.polygon_count = c.polygon as c_ulong;
            m.vertex_count = c.vertex as c_ulong;
            m.tag_count = c.tag_point as c_ulong;
            m.material_count = c.material as c_ulong;
            m.frame_count = c.frame as c_ulong;
            m.sub_count = c.sub_model as c_ulong;
            m.lod_count = c.lod as c_ulong;
            Self::allocate(p, 0, shared);
            if !model.name.is_empty() {
                let name = md(p).name;
                Self::str_set(p, name, &model.name);
            }
            for (k, v) in model.center.iter().take(3).enumerate() {
                let center = &mut md(p).center;
                match k {
                    0 => center.x = *v,
                    1 => center.y = *v,
                    _ => center.z = *v,
                }
            }
            let rasterizer = Self::op(p, EeGeModOp::RasterActive, 0, 0, 0, 0) as usize;
            for i in 0..md(p).lod_count {
                let Some(lod) = model.lods.get(i as usize) else { continue };
                if let Some(count) = lod.created {
                    let array = Self::op(p, EeGeModOp::RasterPolygonArray, rasterizer, count as isize, 0, 0) as usize;
                    put(&mut md(p).polygons, i, array);
                }
                for (k, index) in lod.indices.iter().enumerate() {
                    let array = at(&md(p).polygons, i);
                    if array == 0 {
                        break;
                    }
                    let which = (k % 3 + 1) as isize;
                    Self::op(p, EeGeModOp::PolygonSetIndex, array, (k / 3) as isize, *index as isize, which);
                }
            }
            for i in 0..md(p).material_count {
                let Some(material) = model.materials.get(i as usize) else { continue };
                let target = at(&md(p).materials, i);
                if target == 0 {
                    continue;
                }
                if !material.name.is_empty() {
                    let name = &material.name;
                    cppobj::str_set_bytes(ee_ge_mat_name(mat(target)) as usize, name);
                }
                let lods = md(p).lod_count as usize;
                for (k, value) in material.values.iter().enumerate() {
                    let v = *value as c_ulong as isize;
                    let (which, lod) = if k == 0 {
                        (0, 0)
                    } else if k <= 2 * lods {
                        (if (k - 1) % 2 == 0 { 1 } else { 2 }, (k - 1) / 2)
                    } else if k == 2 * lods + 1 {
                        (3, 0)
                    } else {
                        (4, 0)
                    };
                    let field = [
                        EeGeMatField::TextureIndex,
                        EeGeMatField::PolygonStart,
                        EeGeMatField::PolygonLength,
                        EeGeMatField::VertexStart,
                        EeGeMatField::VertexLength,
                    ][which];
                    ee_ge_mat_set(mat(target), field, lod as c_ulong, v as c_ulong);
                }
                if !material.texture_filename.is_empty() && md(p).texture_filenames != 0 {
                    let names = md(p).texture_filenames;
                    let slot = Self::str_at(p, names, i);
                    Self::str_set(p, slot, &material.texture_filename);
                }
            }
            for i in 0..md(p).tag_count {
                let Some(name) = model.tag_point_names.get(i as usize) else { continue };
                if !name.is_empty() && md(p).tag_point_names != 0 {
                    let names = md(p).tag_point_names;
                    let slot = Self::str_at(p, names, i);
                    Self::str_set(p, slot, name);
                }
            }
            for i in 0..md(p).frame_count {
                let Some(frame) = model.frames.get(i as usize) else { continue };
                if let Some(radius) = frame.radius {
                    put(&mut md(p).radii, i, radius);
                }
                let target = frame.target as c_ulong;
                let array = at(&md(p).vertices, target);
                for (k, point) in frame.points.iter().enumerate().filter(|_| array != 0) {
                    Self::op(p, EeGeModOp::VertexSetPoint, array, k as isize, point as *const Ge3DPoint as isize, 0);
                }
                for (k, normal) in frame.normals.iter().enumerate().filter(|_| array != 0) {
                    Self::op(p, EeGeModOp::VertexSetNormal, array, k as isize, normal as *const GeVector as isize, 0);
                }
                for (k, uv) in frame.uvs.iter().enumerate().filter(|_| array != 0) {
                    Self::op(p, EeGeModOp::VertexSetUv, array, k as isize, uv.as_ptr() as isize, 0);
                }
                let tags = at(&md(p).frame_tags, target);
                let len = md(p).tag_len;
                for (k, point) in frame.tag_points.iter().enumerate() {
                    if !tags.is_null() && k < len {
                        *tags.add(k) = *point;
                    }
                }
                if let Some(t) = md(p).internal.as_mut().and_then(|v| v.get_mut(i as usize)) {
                    for (k, value) in frame.transform.iter().take(16).enumerate() {
                        t.m[k / 4][k % 4] = *value;
                    }
                }
                if let Some(b) = md(p).boxes.as_mut().and_then(|v| v.get_mut(i as usize)) {
                    for (k, value) in frame.bbox.iter().take(6).enumerate() {
                        match k {
                            0 => b.bottom_left_front.x = *value,
                            1 => b.bottom_left_front.y = *value,
                            2 => b.bottom_left_front.z = *value,
                            3 => b.top_right_back.x = *value,
                            4 => b.top_right_back.y = *value,
                            _ => b.top_right_back.z = *value,
                        }
                    }
                    // GEBoundingBox::Load: the corners as read, the dimensions once both were (no reordering)
                    if frame.bbox.len() == 6 {
                        b.width = b.top_right_back.x - b.bottom_left_front.x;
                        b.height = b.top_right_back.z - b.bottom_left_front.z;
                        b.depth = b.top_right_back.y - b.bottom_left_front.y;
                    }
                }
            }
            for (i, sub) in model.sub_models.iter().enumerate() {
                let manager = md(p).manager;
                let created = Self::op(p, EeGeModOp::ModelNew, manager, 0, 0, 0) as usize;
                put(&mut md(p).subs, i as c_ulong, created);
                let object = Self::obj(p, created);
                md(object).parent = md(p).owner;
                Self::store_loaded(object, sub, shared);
            }
            model.result
        }
    }

    /// `LoadTexturesForScreen`: each material's texture for one screen (none first; then the file's, point MIP
    /// filtering and the min and mag modes 1 on every stage), then the sub-models'.
    ///
    /// # Safety
    /// `p` is live; `description` a `GETextureDescription`.
    pub unsafe fn load_textures(p: *mut Self, description: usize, screen: c_ulong) -> c_long {
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            let rasterizer = Self::op(p, EeGeModOp::RasterActive, 0, 0, 0, 0) as usize;
            let manager = Self::op(p, EeGeModOp::RasterTextureManager, rasterizer, screen as isize, 0, 0) as usize;
            let mut i: c_ulong = 0;
            while i < md(p).material_count {
                let material = at(&md(p).materials, i);
                if material != 0 {
                    ee_ge_mat_set_texture(mat(material), ptr::null_mut(), screen);
                }
                let names = md(p).texture_filenames;
                let name = if names != 0 { Self::str_at(p, names, i) } else { 0 };
                let length = if name != 0 { cppobj::str_length(name) } else { 0 };
                if length > 0 && material != 0 {
                    let texture =
                        Self::op(p, EeGeModOp::TexLoad, manager, name as isize, description as isize, 0) as usize;
                    if texture != 0 {
                        ee_ge_mat_set_texture(mat(material), texture as *mut c_void, screen);
                        Self::op(p, EeGeModOp::TexMinFilter, texture, 1, EVERY_STAGE, 0);
                        Self::op(p, EeGeModOp::TexMagFilter, texture, 1, EVERY_STAGE, 0);
                        Self::op(p, EeGeModOp::TexMipFilter, texture, MIP_FILTER_POINT as isize, EVERY_STAGE, 0);
                    }
                }
                i += 1;
            }
            i = 0;
            while i < md(p).sub_count {
                let sub = at(&md(p).subs, i);
                if sub != 0 {
                    let object = Self::obj(p, sub);
                    Self::load_textures(object, description, screen);
                }
                i += 1;
            }
            OK
        }
    }

    /// `ComputeBoundingBoxes`: each frame's box from its points, then the sub-models'.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn compute_bounding_boxes(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            for i in 0..md(p).frame_count {
                let array = at(&md(p).vertices, i);
                let count = md(p).vertex_count;
                let points = Self::gather_points(p, array, count);
                put(&mut md(p).boxes, i, model_geom::frame_box(&points));
            }
            let count = md(p).sub_count;
            for sub in Self::sub_objects(p, count) {
                Self::compute_bounding_boxes(sub);
            }
        }
    }

    /// `ComputeBoundingVolumes`: the boxes, the sub-models' volumes, the centre (the frames' boxes' average) and each
    /// frame's radius about it.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn compute_bounding_volumes(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            Self::compute_bounding_boxes(p);
            let count = md(p).sub_count;
            for sub in Self::sub_objects(p, count) {
                Self::compute_bounding_volumes(sub);
            }
            let m = md(p);
            let frames = (m.frame_count as usize).min(m.boxes.as_ref().map_or(0, |b| b.len()));
            m.center = model_geom::center(m.boxes.as_ref().map_or(&[][..], |b| &b[..frames]));
            for i in 0..md(p).frame_count {
                let array = at(&md(p).vertices, i);
                let count = md(p).vertex_count;
                let points = Self::gather_points(p, array, count);
                let radius = model_geom::radius(md(p).center, &points);
                put(&mut md(p).radii, i, radius);
            }
        }
    }

    /// `ComputeModelNormals`: each frame's vertex normals from LOD 0's polygons.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn compute_normals(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let polygons = at(&md(p).polygons, 0);
            let count = md(p).polygon_count;
            let indices = Self::gather_indices(p, polygons, count);
            for i in 0..md(p).frame_count {
                let array = at(&md(p).vertices, i);
                let count = md(p).vertex_count;
                let points = Self::gather_points(p, array, count);
                if points.is_empty() {
                    continue;
                }
                let normals = model_geom::vertex_normals(&points, &indices);
                for j in 0..md(p).vertex_count {
                    let n = normals.get(j as usize).copied().unwrap_or_default();
                    Self::op(p, EeGeModOp::VertexSetNormal, array, j as isize, &n as *const GeVector as isize, 0);
                }
            }
        }
    }

    /// `Dereference`: one reference dropped while others remain; at the last outside reference (the manager holds
    /// one) the model removes itself from its manager.
    ///
    /// # Safety
    /// `p` is live; it may be freed.
    pub unsafe fn dereference(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let owner = md(p).owner;
            let count = cppobj::reference_count(owner);
            if count > 2 {
                Self::op(p, EeGeModOp::ModelBaseDereference, owner, 0, 0, 0);
            } else {
                let manager = md(p).manager;
                if count > 1 {
                    Self::op(p, EeGeModOp::ModelBaseDereference, owner, 0, 0, 0);
                }
                Self::op(p, EeGeModOp::ManagerRemove, manager, owner as isize, 0, 0);
            }
        }
    }

    /// `GetPolygonCount(lod)`: the materials' polygon ranges at `lod`, summed.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn polygon_count_at(p: *mut Self, lod: c_ulong) -> c_ulong {
        let mut count: c_ulong = 0;
        // SAFETY: the caller's contract
        unsafe {
            for i in 0..md(p).material_count {
                let material = at(&md(p).materials, i);
                let length = ee_ge_mat_get(mat(material), EeGeMatField::PolygonLength, lod);
                count = count.wrapping_add(length);
            }
        }
        count
    }

    /// `GetPolygonCountOfHierarchy(lod)`: the model's and every sub-model's.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn polygon_count_of_hierarchy(p: *mut Self, lod: c_ulong) -> c_ulong {
        // SAFETY: the caller's contract
        unsafe {
            let mut count = Self::polygon_count_at(p, lod);
            let subs = md(p).sub_count;
            for sub in Self::sub_objects(p, subs) {
                count = count.wrapping_add(Self::polygon_count_of_hierarchy(sub, lod));
            }
            count
        }
    }

    /// `PrepareTransforms`: the current frame's graphics transform, then the sub-models'.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn prepare_transforms(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let subs = md(p).sub_count;
            let m = md(p);
            let internal = at(&m.internal, m.current_frame);
            m.graphics = model_geom::graphics_transform(&internal, &m.pre, &m.post, m.graphics_scale);
            for sub in Self::sub_objects(p, subs) {
                Self::prepare_transforms(sub);
            }
        }
    }

    /// `RemoveModelScale`: the frame's scale baked into its points, normals, tag points, box, radius and (frame 0)
    /// centre, its transform left unscaled; then the sub-models' with the accumulated scale.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn remove_model_scale(p: *mut Self, frame: c_ulong, parent: &GeTransformation, x: f32, y: f32, z: f32) {
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            let subs = md(p).sub_count;
            let internal = at(&md(p).internal, frame);
            let removal = model_geom::scale_removal(&internal, parent, x, y, z);
            let scale = removal.scale;
            let m = md(p);
            if frame == 0 {
                m.center = scale.apply_point(m.center);
            }
            if let Some(r) = m.radii.as_mut().and_then(|v| v.get_mut(frame as usize)) {
                *r *= removal.largest;
            }
            if let Some(b) = m.boxes.as_mut().and_then(|v| v.get_mut(frame as usize)) {
                let t = b.transform(&scale);
                // operator=: the corners, then ComputeDimensions
                *b = GeBoundingBox::new(t.bottom_left_front, t.top_right_back);
            }
            let array = at(&md(p).vertices, frame);
            let count = Self::op(p, EeGeModOp::VertexSize, array, 0, 0, 0) as c_ulong;
            for i in 0..count {
                let mut point = Ge3DPoint::default();
                Self::op(p, EeGeModOp::VertexGetPoint, array, i as isize, &mut point as *mut Ge3DPoint as isize, 0);
                let moved = scale.apply_point(point);
                Self::op(p, EeGeModOp::VertexSetPoint, array, i as isize, &moved as *const Ge3DPoint as isize, 0);
                let mut normal = GeVector::default();
                Self::op(p, EeGeModOp::VertexGetNormal, array, i as isize, &mut normal as *mut GeVector as isize, 0);
                let turned = scale.apply_vector(normal);
                Self::op(p, EeGeModOp::VertexSetNormal, array, i as isize, &turned as *const GeVector as isize, 0);
            }
            let m = md(p);
            let tags = at(&m.frame_tags, frame);
            if !tags.is_null() {
                for i in 0..(m.tag_count as usize).min(m.tag_len) {
                    *tags.add(i) = scale.apply_point(*tags.add(i));
                }
            }
            put(&mut m.internal, frame, removal.unscaled);
            for sub in Self::sub_objects(p, subs) {
                Self::remove_model_scale(sub, frame, &removal.transform, removal.x, removal.y, removal.z);
            }
        }
    }

    /// `RemoveScale`: every frame's scale removed, then frame 0 shown.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn remove_scale(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            for i in 0..md(p).frame_count {
                Self::remove_model_scale(p, i, &GeTransformation::IDENTITY, 1.0, 1.0, 1.0);
            }
            Self::set_current_frame(p, 0);
        }
    }

    /// `SetFractionalFramePosition`, on the sub-models too.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_fractional(p: *mut Self, position: f32) {
        // SAFETY: the caller's contract
        unsafe {
            let subs = md(p).sub_count;
            md(p).fractional = position;
            for sub in Self::sub_objects(p, subs) {
                Self::set_fractional(sub, position);
            }
        }
    }

    /// `SetGraphicsCurrentFrame`: the frame, its tag points and box, on the sub-models too.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_current_frame(p: *mut Self, frame: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let subs = md(p).sub_count;
            let m = md(p);
            m.current_frame = frame;
            m.tags = at(&m.frame_tags, frame);
            m.graphics_box = base(&mut m.boxes).wrapping_add(frame as usize);
            for sub in Self::sub_objects(p, subs) {
                Self::set_current_frame(sub, frame);
            }
        }
    }

    /// `SetGraphicsNextFrame`, on the sub-models too.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_next_frame(p: *mut Self, frame: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let subs = md(p).sub_count;
            md(p).next_frame = frame;
            for sub in Self::sub_objects(p, subs) {
                Self::set_next_frame(sub, frame);
            }
        }
    }

    /// The header's `SetOpacity`, on the sub-models too.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_opacity(p: *mut Self, opacity: f32) {
        // SAFETY: the caller's contract
        unsafe {
            md(p).opacity = opacity;
            let subs = md(p).sub_count;
            for sub in Self::sub_objects(p, subs) {
                Self::set_opacity(sub, opacity);
            }
        }
    }

    /// `SetSubModel`: the old sub-model's parent cleared and its reference dropped, the new one referenced and
    /// parented.
    ///
    /// # Safety
    /// `p` is live; `model` a live model or 0.
    pub unsafe fn set_sub_model(p: *mut Self, model: usize, index: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let old = at(&md(p).subs, index);
            if old != 0 {
                md(Self::obj(p, old)).parent = 0;
                Self::op(p, EeGeModOp::ModelBaseDereference, old, 0, 0, 0);
            }
            if model != 0 {
                cppobj::reference(model);
                let owner = md(p).owner;
                md(Self::obj(p, model)).parent = owner;
            }
            put(&mut md(p).subs, index, model);
        }
    }

    /// `SetTexture`: one screen's texture on every material, on the sub-models too.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_texture(p: *mut Self, texture: usize, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let subs = md(p).sub_count;
            let materials = md(p).material_count;
            for i in 0..materials {
                let material = at(&md(p).materials, i);
                ee_ge_mat_set_texture(mat(material), texture as *mut c_void, screen);
            }
            for sub in Self::sub_objects(p, subs) {
                Self::set_texture(sub, texture, screen);
            }
        }
    }

    /// `SizeOf`: the original's estimate of the model's and its sub-models' memory, in its terms and order
    /// (`unsigned long`, wrapping).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn size_of(p: *mut Self) -> c_ulong {
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            let name = md(p).name;
            let name_size = cppobj::str_size_of(name);
            let m = md(p);
            let shared = m.shared.len() as c_ulong;
            let mut size = 0xd8u32 as c_ulong;
            size = size.wrapping_add(name_size);
            size = size.wrapping_add(shared.wrapping_mul(8));
            size = size.wrapping_add(m.frame_count.wrapping_mul(0x40));
            size = size.wrapping_add(m.sub_count.wrapping_mul(POINTER));
            for i in 0..md(p).sub_count {
                let sub = at(&md(p).subs, i);
                if sub != 0 {
                    size = size.wrapping_add(Self::size_of(Self::obj(p, sub)));
                }
            }
            size = size.wrapping_add(md(p).lod_count.wrapping_mul(POINTER)).wrapping_add(4);
            for j in 0..md(p).lod_count {
                let array = at(&md(p).polygons, j);
                if array != 0 {
                    size = size.wrapping_add(Self::op(p, EeGeModOp::PolygonSizeOf, array, 0, 0, 0) as c_ulong);
                }
            }
            let first = at(&md(p).vertices, 0);
            let vertex_size =
                if first != 0 { Self::op(p, EeGeModOp::VertexSizeOf, first, 0, 0, 0) as c_ulong } else { 0 };
            let m = md(p);
            let own_frames = m.frame_count.wrapping_sub(shared);
            size = size.wrapping_add(own_frames.wrapping_mul(vertex_size)).wrapping_add(8);
            size = size.wrapping_add(m.material_count.wrapping_mul(POINTER));
            for k in 0..md(p).material_count {
                let material = at(&md(p).materials, k);
                if material != 0 {
                    size = size.wrapping_add(Self::op(p, EeGeModOp::MaterialSizeOf, material, 0, 0, 0) as c_ulong);
                }
            }
            let m = md(p);
            size = size.wrapping_add(own_frames.wrapping_mul(m.tag_count).wrapping_mul(0xc));
            size = size.wrapping_add(m.frame_count.wrapping_mul(0x24 + 4)).wrapping_add(0x14);
            let names = m.texture_filenames;
            if names != 0 {
                for l in 0..md(p).material_count {
                    let name = Self::str_at(p, names, l);
                    size = size.wrapping_add(cppobj::str_size_of(name));
                }
            }
            size = size.wrapping_add(4);
            let names = md(p).tag_point_names;
            if names != 0 {
                for n in 0..md(p).tag_count {
                    let name = Self::str_at(p, names, n);
                    size = size.wrapping_add(cppobj::str_size_of(name));
                }
            }
            size = size.wrapping_add(md(p).frame_count.wrapping_mul(POINTER)).wrapping_add(0x60);
            size
        }
    }

    /// `SortTagPoints`: the tag points ordered by their names' first characters (a selection sort's swaps, the
    /// names and every frame's points), then the sub-models'.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn sort_tag_points(p: *mut Self) {
        // SAFETY: the caller's contract; every host call is a statement of its own
        unsafe {
            let temp = Self::op(p, EeGeModOp::StrNew, 0, 0, 0, 0) as usize;
            let frames = md(p).frame_count;
            let count = md(p).tag_count;
            let names = md(p).tag_point_names;
            let mut first = Vec::with_capacity(count as usize);
            for i in 0..count {
                let name = Self::str_at(p, names, i);
                #[allow(clippy::unnecessary_cast)] // c_char is u8 on some targets
                first.push(*cppobj::str_text(name) as i8);
            }
            let order = model_geom::tag_point_order(&first);
            for i in 0..count {
                let minimum = order[i as usize] as c_ulong;
                if minimum == i {
                    continue;
                }
                let a = Self::str_at(p, names, i);
                let b = Self::str_at(p, names, minimum);
                cppobj::str_assign(temp, a);
                cppobj::str_assign(a, b);
                cppobj::str_assign(b, temp);
                let m = md(p);
                for k in 0..frames {
                    let points = at(&m.frame_tags, k);
                    if !points.is_null() && (i as usize) < m.tag_len && (minimum as usize) < m.tag_len {
                        ptr::swap(points.add(i as usize), points.add(minimum as usize));
                    }
                }
            }
            let subs = md(p).sub_count;
            for sub in Self::sub_objects(p, subs) {
                Self::sort_tag_points(sub);
            }
            Self::op(p, EeGeModOp::StrDelete, temp, 0, 0, 0);
        }
    }

    /// `Verify`: false when a frame's LOD 0 triangle has two equal corners; then the sub-models', to the first
    /// that fails.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn verify(p: *mut Self) -> bool {
        // SAFETY: the caller's contract
        unsafe {
            let materials = md(p).material_count;
            let frames = md(p).frame_count;
            let mut result = true;
            let polygons = at(&md(p).polygons, 0);
            let count = md(p).polygon_count;
            let indices = Self::gather_indices(p, polygons, count);
            let mut ranges = Vec::with_capacity(materials as usize);
            for j in 0..materials {
                let material = at(&md(p).materials, j);
                let m = mat(material);
                let start = ee_ge_mat_get(m, EeGeMatField::PolygonStart, 0) as u32;
                let length = ee_ge_mat_get(m, EeGeMatField::PolygonLength, 0) as u32;
                let vertex = ee_ge_mat_get(m, EeGeMatField::VertexStart, 0) as u32;
                ranges.push([start, length, vertex]);
            }
            for i in 0..frames {
                let array = at(&md(p).vertices, i);
                let count = md(p).vertex_count;
                let points = Self::gather_points(p, array, count);
                if !model_geom::frame_is_sound(&points, &indices, &ranges) {
                    result = false;
                }
            }
            if result {
                let subs = md(p).sub_count;
                for sub in Self::sub_objects(p, subs) {
                    result = Self::verify(sub);
                    if !result {
                        break;
                    }
                }
            }
            result
        }
    }
}

// ---- the C++ forwarders' entry points

/// `GEModel(manager)`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_mod_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_new(
    host: EeGeModHostFn,
    owner: *mut c_void,
    manager: *mut c_void,
) -> *mut EeGeModel {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::create(host, owner as usize, manager as usize) }
}

/// `GEModel(polygons, vertices, materials, tag points, frames, sub-models, name, flags, manager)`.
///
/// # Safety
/// As `ee_ge_mod_new`; `name` a `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_new_with_counts(
    host: EeGeModHostFn,
    owner: *mut c_void,
    counts: *const c_ulong,
    name: *const c_void,
    flags: c_ulong,
    manager: *mut c_void,
) -> *mut EeGeModel {
    // SAFETY: the caller's contract (`counts` holds six)
    unsafe {
        let c = std::slice::from_raw_parts(counts, 6);
        let counts = [c[0], c[1], c[2], c[3], c[4], c[5]];
        EeGeModel::create_with_counts(host, owner as usize, counts, name as usize, flags, manager as usize)
    }
}

/// `~GEModel`.
///
/// # Safety
/// `p` is live (freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_destroy(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::destroy(p) }
}

/// `StoreLoaded(model, shared frames)`.
///
/// # Safety
/// `p` is live; `model` a decoded model; `shared` holds `count` entries (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_store_loaded(
    p: *mut EeGeModel,
    model: *const CemModel,
    shared: *const EeGeSharedFrame,
    count: usize,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let entries: Vec<(c_ulong, c_long)> = if shared.is_null() || count == 0 {
            Vec::new()
        } else {
            std::slice::from_raw_parts(shared, count).iter().map(|e| (e.frame as c_ulong, e.offset as c_long)).collect()
        };
        EeGeModel::store_loaded(p, &*model, &entries)
    }
}

/// `LoadTexturesForScreen(description, screen)`.
///
/// # Safety
/// `p` is live; `description` a `GETextureDescription`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_load_textures(
    p: *mut EeGeModel,
    description: *const c_void,
    screen: c_ulong,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::load_textures(p, description as usize, screen) }
}

/// `ComputeBoundingVolumes`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_compute_bounding_volumes(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::compute_bounding_volumes(p) }
}

/// `ComputeModelNormals`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_compute_normals(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::compute_normals(p) }
}

/// `Dereference` (GEModel's own).
///
/// # Safety
/// `p` is live; it may be freed.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_dereference(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::dereference(p) }
}

/// `GetPolygonCount(lod)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_polygon_count(p: *mut EeGeModel, lod: c_ulong) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::polygon_count_at(p, lod) }
}

/// `GetPolygonCountOfHierarchy(lod)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_polygon_count_of_hierarchy(p: *mut EeGeModel, lod: c_ulong) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::polygon_count_of_hierarchy(p, lod) }
}

/// `PrepareTransforms`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_prepare_transforms(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::prepare_transforms(p) }
}

/// `RemoveScale`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_remove_scale(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::remove_scale(p) }
}

/// `SetFractionalFramePosition`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_fractional(p: *mut EeGeModel, position: f32) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_fractional(p, position) }
}

/// `SetGraphicsCurrentFrame`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_current_frame(p: *mut EeGeModel, frame: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_current_frame(p, frame) }
}

/// `SetGraphicsNextFrame`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_next_frame(p: *mut EeGeModel, frame: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_next_frame(p, frame) }
}

/// `SetOpacity`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_opacity(p: *mut EeGeModel, opacity: f32) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_opacity(p, opacity) }
}

/// `SetSubModel(model, index)`.
///
/// # Safety
/// `p` is live; `model` a live model or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_sub_model(p: *mut EeGeModel, model: *mut c_void, index: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_sub_model(p, model as usize, index) }
}

/// `SetTexture(texture, screen)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_texture(p: *mut EeGeModel, texture: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::set_texture(p, texture as usize, screen) }
}

/// `SizeOf`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_size_of(p: *mut EeGeModel) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::size_of(p) }
}

/// `SortTagPoints`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_sort_tag_points(p: *mut EeGeModel) {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::sort_tag_points(p) }
}

/// `Verify`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_verify(p: *mut EeGeModel) -> bool {
    // SAFETY: the caller's contract
    unsafe { EeGeModel::verify(p) }
}

/// A field of the header's accessors.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_get(p: *mut EeGeModel, field: EeGeModVal) -> isize {
    // SAFETY: the caller's contract
    let m = unsafe { md(p) };
    (match field {
        EeGeModVal::Manager => m.manager,
        EeGeModVal::Parent => m.parent,
        EeGeModVal::CurrentFrame => m.current_frame as usize,
        EeGeModVal::NextFrame => m.next_frame as usize,
        EeGeModVal::SubModelCount => m.sub_count as usize,
        EeGeModVal::FrameCount => m.frame_count as usize,
        EeGeModVal::MaterialCount => m.material_count as usize,
        EeGeModVal::PolygonCount => m.polygon_count as usize,
        EeGeModVal::VertexCount => m.vertex_count as usize,
        EeGeModVal::TagPointCount => m.tag_count as usize,
        EeGeModVal::LodCount => m.lod_count as usize,
        EeGeModVal::SharedFrameCount => m.shared.len(),
    }) as isize
}

/// A float of the header's accessors.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_getf(p: *mut EeGeModel, field: EeGeModFloat) -> f32 {
    // SAFETY: the caller's contract
    let m = unsafe { md(p) };
    match field {
        EeGeModFloat::GraphicsScale => m.graphics_scale,
        EeGeModFloat::Opacity => m.opacity,
        EeGeModFloat::FractionalFramePosition => m.fractional,
    }
}

/// `SetGraphicsScale`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_graphics_scale(p: *mut EeGeModel, scale: f32) {
    // SAFETY: the caller's contract
    unsafe { md(p).graphics_scale = scale };
}

/// A pointer the header's accessors return or index (NULL for an array not allocated).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_ptr(p: *mut EeGeModel, field: EeGeModPtr) -> *mut c_void {
    // SAFETY: the caller's contract
    let m = unsafe { md(p) };
    match field {
        EeGeModPtr::InternalTransforms => base(&mut m.internal).cast(),
        EeGeModPtr::SubModels => base(&mut m.subs).cast(),
        EeGeModPtr::PolygonArrays => base(&mut m.polygons).cast(),
        EeGeModPtr::VertexArrays => base(&mut m.vertices).cast(),
        EeGeModPtr::Materials => base(&mut m.materials).cast(),
        EeGeModPtr::FrameTagPoints => base(&mut m.frame_tags).cast(),
        EeGeModPtr::BoundingBoxes => base(&mut m.boxes).cast(),
        EeGeModPtr::GraphicsBoundingBox => m.graphics_box.cast(),
        EeGeModPtr::TagPoints => m.tags.cast(),
        EeGeModPtr::TextureFilenames => m.texture_filenames as *mut c_void,
        EeGeModPtr::TagPointNames => m.tag_point_names as *mut c_void,
        EeGeModPtr::BoundingRadii => base(&mut m.radii).cast(),
        EeGeModPtr::Name => m.name as *mut c_void,
        EeGeModPtr::PreTransform => (&mut m.pre as *mut GeTransformation).cast(),
        EeGeModPtr::PostTransform => (&mut m.post as *mut GeTransformation).cast(),
        EeGeModPtr::GraphicsTransform => (&mut m.graphics as *mut GeTransformation).cast(),
        EeGeModPtr::Center => (&mut m.center as *mut Ge3DPoint).cast(),
    }
}

/// `mSharedFrames`' entry `index` in frame order.
///
/// # Safety
/// `p` is live; the outputs are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_shared_frame(
    p: *mut EeGeModel,
    index: c_ulong,
    frame: *mut c_ulong,
    offset: *mut c_long,
) {
    // SAFETY: the caller's contract
    unsafe {
        let (f, o) = md(p).shared.get(index as usize).copied().unwrap_or((0, 0));
        *frame = f;
        *offset = o;
    }
}

/// `GetClipToPlane(plane)` (false past the six planes).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_clip(p: *mut EeGeModel, plane: c_ulong) -> bool {
    // SAFETY: the caller's contract
    unsafe { md(p).clip.get(plane as usize).copied().unwrap_or(false) }
}

/// `SetClipToPlane(plane, clip)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_clip(p: *mut EeGeModel, plane: c_ulong, clip: bool) {
    // SAFETY: the caller's contract
    if let Some(slot) = unsafe { md(p).clip.get_mut(plane as usize) } {
        *slot = clip;
    }
}

/// `SetGraphicsPreModelTransform`, `SetGraphicsPostModelTransform` or `SetInternalTransform(frame, ...)`.
///
/// # Safety
/// `p` is live; `t` a transform.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_transform(
    p: *mut EeGeModel,
    which: EeGeModXf,
    frame: c_ulong,
    t: *const GeTransformation,
) {
    // SAFETY: the caller's contract
    unsafe {
        let t = *t;
        let m = md(p);
        match which {
            EeGeModXf::Pre => m.pre = t,
            EeGeModXf::Post => m.post = t,
            EeGeModXf::Internal => put(&mut m.internal, frame, t),
        }
    }
}

/// `SetTagPoint(index, point)` (`frame` -1: the current frame's) or `SetTagPoint(frame, index, point)`.
///
/// # Safety
/// `p` is live; `point` a point.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mod_set_tag_point(
    p: *mut EeGeModel,
    frame: c_long,
    index: c_ulong,
    point: *const Ge3DPoint,
) {
    // SAFETY: the caller's contract
    unsafe {
        let m = md(p);
        let tags = if frame < 0 { m.tags } else { at(&m.frame_tags, frame as c_ulong) };
        if !tags.is_null() && (index as usize) < m.tag_len {
            *tags.add(index as usize) = *point;
        }
    }
}

#[cfg(test)]
#[path = "modobj_tests.rs"]
mod tests;
