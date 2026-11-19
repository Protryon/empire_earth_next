//! The graphic objects' tag points whole in Rust (the Graphics + LLE cluster, round 1): what `EEGraphicObject`
//! kept in `mTagPointMaps` (the lists by point kind, then by root model), `mTagPoints` (the "weapon_mount" lists by
//! model part), `mColorGroups` (the "player color" materials by part) and `mPoints` (the "attack" points), the
//! part scan that fills them (`CollectTagPoints`, 0xd8bb2), the lookups (`GetTagPoints` 0xd84ec, `PickPoint`
//! 0xd8546), the colour setter (`SetGroupColor` 0xd9008), the lists' release (`EETagPointList::Clear` 0xdab52), the
//! points' geometry (`EETagPoint::GetOffset` 0xdab97 / 0xdac22, `GetPoint` 0xdaccd / 0xdad54) and
//! `EETagPointList::Scale` (0xe1814).
//!
//! A list keeps the original's shape for the C++ readers (the units, the physics, the effects): a vector of pointers
//! to points, each point the model it was tagged on (referenced) and the index of its tag point. `EETagPointList` in
//! C++ is a view of [`EeGfxTagList`] (the vector's data, size and capacity), `EETagPoint` of [`EeGfxTagPoint`]; the
//! lists are made and freed here only.
//!
//! The models are read through [`TagModels`]: the game's are the C++ `GEModel`s over their Rust objects
//! (`ee_render::ge::modobj`), the tests' are fakes. The point arithmetic is the C++'s (`GETransformation::Apply`'s
//! sums in its order): the simulation reads these points (a unit's weapon point, a carrier's deck).

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::mem::ManuallyDrop;

use ee_base::math::types::{Ge3DPoint, GeBoundingBox, GeTransformation};

use crate::ge::cppobj;
use crate::ge::modobj::{ee_ge_mod_dereference, ee_ge_mod_get, ee_ge_mod_ptr, EeGeModPtr, EeGeModVal, EeGeModel};
use crate::ge::smallobj::{ee_ge_mat_color, ee_ge_mat_name, EeGeMat};

/// A graphic's model parts (`EEGraphicObject`'s 14 states).
const PARTS: usize = 14;

/// One tag point: the model it is tagged on (a `GEModel*`, referenced while the point lives) and the index of the
/// tag point in that model's per-frame arrays (`EETagPoint`, 8 bytes in the original).
#[repr(C)]
#[derive(Debug)]
#[allow(non_snake_case)]
pub struct EeGfxTagPoint {
    pub mModel: *mut c_void,
    pub mIndex: c_ulong,
}

/// The tag points of one kind (`EETagPointList`): the original's `std::vector<EETagPoint*>` as its data, size and
/// capacity (a Rust `Vec`'s raw parts).
#[repr(C)]
#[derive(Debug)]
pub struct EeGfxTagList {
    pub points: *mut *mut EeGfxTagPoint,
    pub len: usize,
    pub cap: usize,
}

impl EeGfxTagList {
    fn new() -> Box<EeGfxTagList> {
        let mut v = ManuallyDrop::new(Vec::<*mut EeGfxTagPoint>::new());
        Box::new(EeGfxTagList { points: v.as_mut_ptr(), len: 0, cap: 0 })
    }

    fn with_vec<R>(&mut self, f: impl FnOnce(&mut Vec<*mut EeGfxTagPoint>) -> R) -> R {
        // SAFETY: the raw parts of a Vec this list made
        let mut v = unsafe { Vec::from_raw_parts(self.points, self.len, self.cap) };
        let r = f(&mut v);
        let mut v = ManuallyDrop::new(v);
        self.points = v.as_mut_ptr();
        self.len = v.len();
        self.cap = v.capacity();
        r
    }

    /// The points (`mPoints`).
    pub fn points(&self) -> &[*mut EeGfxTagPoint] {
        if self.len == 0 {
            return &[];
        }
        // SAFETY: the list's own vector
        unsafe { std::slice::from_raw_parts(self.points, self.len) }
    }

    /// Point `i`.
    pub fn point(&self, i: usize) -> &EeGfxTagPoint {
        // SAFETY: a point this list made
        unsafe { &*self.points()[i] }
    }

    /// `Add` (0xd9a5d): a new point for tag point `index` of `model`, which it references.
    fn add<M: TagModels>(&mut self, models: &M, model: usize, index: c_ulong) {
        models.reference(model);
        let p = Box::into_raw(Box::new(EeGfxTagPoint { mModel: model as *mut c_void, mIndex: index }));
        self.with_vec(|v| v.push(p));
    }

    /// `Clear` (0xdab52): each point's model dereferenced and the point freed, in order; the list empty.
    fn clear<M: TagModels>(&mut self, models: &M) {
        self.with_vec(|v| {
            for p in v.drain(..) {
                if !p.is_null() {
                    // SAFETY: a point this list made, freed once
                    let point = unsafe { Box::from_raw(p) };
                    models.dereference(point.mModel as usize);
                }
            }
        });
    }
}

impl Drop for EeGfxTagList {
    fn drop(&mut self) {
        // the points' models are released by `clear` first; a list dropped with points frees only its memory
        self.with_vec(|v| {
            for p in v.drain(..) {
                if !p.is_null() {
                    // SAFETY: a point this list made
                    drop(unsafe { Box::from_raw(p) });
                }
            }
        });
        // SAFETY: the raw parts of the list's Vec, dropped once
        drop(unsafe { Vec::from_raw_parts(self.points, self.len, self.cap) });
        self.len = 0;
        self.cap = 0;
    }
}

/// What the part scan reads of a model (and writes to its materials) and how a point holds it.
pub trait TagModels {
    fn tag_count(&self, model: usize) -> usize;
    fn tag_name(&self, model: usize, i: usize) -> Vec<u8>;
    /// `GetTagPoint(i, frame)` (the frame's array).
    fn tag_point(&self, model: usize, i: usize, frame: usize) -> Ge3DPoint;
    fn internal(&self, model: usize, frame: usize) -> GeTransformation;
    /// `GetParentModel()` (0: none).
    fn parent(&self, model: usize) -> usize;
    fn sub_models(&self, model: usize) -> Vec<usize>;
    fn frame_count(&self, model: usize) -> c_ulong;
    /// `GetBoundingBox(0)`.
    fn bounding_box(&self, model: usize) -> GeBoundingBox;
    fn material_names(&self, model: usize) -> Vec<Vec<u8>>;
    /// `GetMaterial(i)->SetColor(color)`.
    fn set_material_color(&self, model: usize, i: usize, color: [f32; 3]);
    fn reference(&self, model: usize);
    /// `GEModel::Dereference` (the model's own: it may leave its manager).
    fn dereference(&self, model: usize);
}

/// One part's materials that take the player colour (`EEColorGroup`).
struct ColorGroup {
    model: usize,
    materials: Vec<c_long>,
}

/// A graphic object's tag points, colour groups and attack points (made at its first load, freed at its last
/// unload).
#[derive(Default)]
pub struct EeGfxGraphicTags {
    /// `mTagPointMaps`: by kind, then by root model (both in key order, as the `std::map`s).
    kinds: BTreeMap<c_long, BTreeMap<usize, Box<EeGfxTagList>>>,
    /// `mTagPoints`: the "weapon_mount" points by part.
    weapons: [Option<Box<EeGfxTagList>>; PARTS],
    /// `mColorGroups`
    colors: [Vec<ColorGroup>; PARTS],
    /// `mPoints`: every "attack" point
    points: Vec<Ge3DPoint>,
}

/// The graphic object's points the scan writes (`mDefaultPoint`, `mTopPoint`, `mBar`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScanPoints {
    pub default_point: Ge3DPoint,
    pub top: Ge3DPoint,
    pub bar: Ge3DPoint,
}

fn starts(name: &[u8], prefix: &[u8]) -> bool {
    name.starts_with(prefix)
}

impl EeGfxGraphicTags {
    /// `CollectTagPoints(part, model, parent)` (0xd8bb2): `model`'s tag points (and its sub-models', under the
    /// combined transformation) into the kinds' lists for its root model (not for part 0's model's own when `part`
    /// is not 0), "weapon_mount" into the part's list, "attack" the default point (and the top point at frame
    /// `last5`, `mLastFrames[5]`), "status bar" the bar (else the box's top raises it); "player color" materials
    /// into the part's colour group. `part0` is `mModels[0]`.
    #[allow(clippy::too_many_arguments)]
    pub fn collect<M: TagModels>(
        &mut self,
        models: &M,
        part: usize,
        model: usize,
        parent: &GeTransformation,
        part0: usize,
        last5: c_ulong,
        out: &mut ScanPoints,
    ) {
        let transformation = parent.compose(&models.internal(model, 0));
        let mut root = model;
        while models.parent(root) != 0 {
            root = models.parent(root);
        }
        let mut found = false;
        for i in 0..models.tag_count(model) {
            let name = models.tag_name(model, i);
            if root != part0 || part == 0 {
                for kind in ee_data::tables::tag_point_kinds(&name) {
                    let lists = self.kinds.entry(kind).or_default();
                    lists.entry(root).or_insert_with(EeGfxTagList::new).add(models, model, i as c_ulong);
                }
            }
            if starts(&name, b"weapon_mount") {
                self.weapons[part].get_or_insert_with(EeGfxTagList::new).add(models, model, i as c_ulong);
            }
            if starts(&name, b"attack") {
                out.default_point = transformation.apply_point(models.tag_point(model, i, 0));
                self.points.push(out.default_point);
                if last5 < models.frame_count(model) {
                    out.top = transformation.apply_point(models.tag_point(model, i, last5 as usize));
                }
            }
            if starts(&name, b"status bar") {
                out.bar = transformation.apply_point(models.tag_point(model, i, 0));
                found = true;
            }
        }
        if !found {
            let top = Ge3DPoint { x: 0.0, y: 0.0, z: models.bounding_box(model).top_right_back.z };
            let bar = transformation.apply_point(top);
            out.bar.z = if bar.z < out.bar.z { out.bar.z } else { bar.z };
        }
        let mut has_group = false;
        for (m, material) in models.material_names(model).iter().enumerate() {
            if material.as_slice() == b"player color" {
                if !has_group {
                    has_group = true;
                    self.colors[part].push(ColorGroup { model, materials: Vec::new() });
                }
                self.colors[part].last_mut().expect("the group").materials.push(m as c_long);
            }
        }
        for sub in models.sub_models(model) {
            self.collect(models, part, sub, &transformation, part0, last5, out);
        }
    }

    /// `GetTagPoints(part, kind)`'s lookup: the list of `kind` for the part's model `model` (None without one).
    pub fn find(&self, model: usize, kind: c_long) -> Option<&EeGfxTagList> {
        self.kinds.get(&kind).and_then(|lists| lists.get(&model)).map(|b| &**b)
    }

    fn find_ptr(&self, model: usize, kind: c_long) -> *mut EeGfxTagList {
        match self.kinds.get(&kind).and_then(|lists| lists.get(&model)) {
            Some(b) => &**b as *const EeGfxTagList as *mut EeGfxTagList,
            None => std::ptr::null_mut(),
        }
    }

    /// The part's "weapon_mount" list (`mTagPoints[part]`).
    pub fn weapon(&self, part: usize) -> Option<&EeGfxTagList> {
        self.weapons.get(part).and_then(|w| w.as_deref())
    }

    fn weapon_ptr(&self, part: usize) -> *mut EeGfxTagList {
        match self.weapon(part) {
            Some(l) => l as *const EeGfxTagList as *mut EeGfxTagList,
            None => std::ptr::null_mut(),
        }
    }

    /// `PickPoint` (0xd8546): attack point `time % count`, None without one (the default point then).
    pub fn pick(&self, time: c_long) -> Option<&Ge3DPoint> {
        if self.points.is_empty() {
            return None;
        }
        // the original's long converted to the vector's size_t
        let i = (time as i64 as u64) % (self.points.len() as u64);
        self.points.get(i as usize)
    }

    /// `SetGroupColor(color, part)` (0xd9008).
    pub fn set_color<M: TagModels>(&self, models: &M, part: usize, color: [f32; 3]) {
        for group in &self.colors[part] {
            for &m in &group.materials {
                models.set_material_color(group.model, m as usize, color);
            }
        }
    }

    /// The unload's release of part `part`'s list (`mTagPoints[i]->Clear()`, deleted).
    pub fn clear_weapon<M: TagModels>(&mut self, models: &M, part: usize) {
        if let Some(mut list) = self.weapons[part].take() {
            list.clear(models);
        }
    }

    /// The unload's release of the kinds' lists (each cleared, by kind, then by model) and of the rest.
    pub fn clear<M: TagModels>(&mut self, models: &M) {
        for (_, lists) in std::mem::take(&mut self.kinds) {
            for (_, mut list) in lists {
                list.clear(models);
            }
        }
        for part in 0..PARTS {
            self.clear_weapon(models, part);
        }
        for groups in &mut self.colors {
            groups.clear();
        }
        self.points.clear();
    }
}

/// `GetOffset(frame[, part])`: the tag point at `frame` (through `part` first when given), carried through its
/// model's and every parent model's internal transformation at that frame.
pub fn offset<M: TagModels>(
    models: &M,
    point: &EeGfxTagPoint,
    frame: usize,
    part: Option<&GeTransformation>,
) -> Ge3DPoint {
    let mut model = point.mModel as usize;
    let local = models.tag_point(model, point.mIndex as usize, frame);
    let mut p = match part {
        Some(t) => t.apply_point(local),
        None => local,
    };
    while model != 0 {
        p = models.internal(model, frame).apply_point(p);
        model = models.parent(model);
    }
    p
}

/// `GetPoint(frame, transformation[, part])`: the offset through `transformation`.
pub fn point_at<M: TagModels>(
    models: &M,
    point: &EeGfxTagPoint,
    frame: usize,
    transformation: &GeTransformation,
    part: Option<&GeTransformation>,
) -> Ge3DPoint {
    transformation.apply_point(offset(models, point, frame, part))
}

/// `EETagPointList::Scale` (0xe1814): the rotation rows scaled, the translation kept.
pub fn scale(t: &mut GeTransformation, s: f32) {
    for r in 0..3 {
        for c in 0..3 {
            t.m[r][c] = t.m[r][c] * s;
        }
    }
}

/// The game's models: C++ `GEModel`s over their Rust objects (`cppobj`'s layout).
pub struct CppModels;

impl CppModels {
    fn rust(model: usize) -> *mut EeGeModel {
        // SAFETY: a live GEModel (the readers' contract)
        unsafe { cppobj::counted::<EeGeModel>(model) }
    }
    fn val(model: usize, field: EeGeModVal) -> usize {
        // SAFETY: a live model
        unsafe { ee_ge_mod_get(Self::rust(model), field) as usize }
    }
    fn ptr(model: usize, field: EeGeModPtr) -> *mut c_void {
        // SAFETY: a live model
        unsafe { ee_ge_mod_ptr(Self::rust(model), field) }
    }
}

fn c_bytes(text: *const c_char) -> Vec<u8> {
    if text.is_null() {
        return Vec::new();
    }
    // SAFETY: a UString's NUL-terminated text
    unsafe { CStr::from_ptr(text) }.to_bytes().to_vec()
}

impl TagModels for CppModels {
    fn tag_count(&self, model: usize) -> usize {
        Self::val(model, EeGeModVal::TagPointCount)
    }
    fn tag_name(&self, model: usize, i: usize) -> Vec<u8> {
        let names = Self::ptr(model, EeGeModPtr::TagPointNames) as usize;
        // SAFETY: the model's `new UString[tag points]`
        c_bytes(unsafe { cppobj::str_text(cppobj::str_at(names, i)) })
    }
    fn tag_point(&self, model: usize, i: usize, frame: usize) -> Ge3DPoint {
        let frames = Self::ptr(model, EeGeModPtr::FrameTagPoints) as *const *const Ge3DPoint;
        // SAFETY: the model's per-frame tag point arrays (the C++ indexed them alike)
        unsafe { *(*frames.add(frame)).add(i) }
    }
    fn internal(&self, model: usize, frame: usize) -> GeTransformation {
        let transforms = Self::ptr(model, EeGeModPtr::InternalTransforms) as *const GeTransformation;
        // SAFETY: the model's per-frame transformations
        unsafe { *transforms.add(frame) }
    }
    fn parent(&self, model: usize) -> usize {
        Self::val(model, EeGeModVal::Parent)
    }
    fn sub_models(&self, model: usize) -> Vec<usize> {
        let n = Self::val(model, EeGeModVal::SubModelCount);
        let subs = Self::ptr(model, EeGeModPtr::SubModels) as *const usize;
        // SAFETY: the model's `mSubModels`
        (0..n).map(|i| unsafe { *subs.add(i) }).collect()
    }
    fn frame_count(&self, model: usize) -> c_ulong {
        Self::val(model, EeGeModVal::FrameCount) as c_ulong
    }
    fn bounding_box(&self, model: usize) -> GeBoundingBox {
        // SAFETY: the model's per-frame boxes
        unsafe { *(Self::ptr(model, EeGeModPtr::BoundingBoxes) as *const GeBoundingBox) }
    }
    fn material_names(&self, model: usize) -> Vec<Vec<u8>> {
        let n = Self::val(model, EeGeModVal::MaterialCount);
        let materials = Self::ptr(model, EeGeModPtr::Materials) as *const usize;
        (0..n)
            .map(|i| {
                // SAFETY: the model's live materials and their names
                unsafe {
                    let m = cppobj::counted::<EeGeMat>(*materials.add(i));
                    c_bytes(cppobj::str_text(ee_ge_mat_name(m) as usize))
                }
            })
            .collect()
    }
    fn set_material_color(&self, model: usize, i: usize, color: [f32; 3]) {
        let materials = Self::ptr(model, EeGeModPtr::Materials) as *const usize;
        // SAFETY: the model's live material; `GEColor` is its three floats
        unsafe {
            let c = ee_ge_mat_color(cppobj::counted::<EeGeMat>(*materials.add(i))) as *mut [f32; 3];
            *c = color;
        }
    }
    fn reference(&self, model: usize) {
        // SAFETY: a live model
        unsafe { cppobj::reference(model) }
    }
    fn dereference(&self, model: usize) {
        // SAFETY: a live model
        unsafe { ee_ge_mod_dereference(Self::rust(model)) }
    }
}

// ---- the C interface (EEGraphicObject.cpp; the readers through ee/EEGraphicObject.h)

/// A new, empty store (the first load).
#[no_mangle]
pub extern "C" fn ee_gfx_tags_new() -> *mut EeGfxGraphicTags {
    Box::into_raw(Box::default())
}

/// The store freed after its lists' release (the last unload; NULL: nothing).
///
/// # Safety
/// `p` is NULL or a store of [`ee_gfx_tags_new`], not used again; its models are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_free(p: *mut EeGfxGraphicTags) {
    if p.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let mut tags = unsafe { Box::from_raw(p) };
    tags.clear(&CppModels);
}

/// `CollectTagPoints(part, model, parent)`; `default_point`, `top` and `bar` are `&mDefaultPoint`, `&mTopPoint` and
/// `&mBar` (read and written), `weapons` the object's `mTagPoints` (its part's entry set to the part's list).
///
/// # Safety
/// `p` is a live store; `model` and `part0` live models (or `part0` NULL); the pointers valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_collect(
    p: *mut EeGfxGraphicTags,
    part: c_ulong,
    model: *mut c_void,
    parent: *const GeTransformation,
    part0: *mut c_void,
    last5: c_ulong,
    default_point: *mut Ge3DPoint,
    top: *mut Ge3DPoint,
    bar: *mut Ge3DPoint,
    weapons: *mut *mut EeGfxTagList,
) {
    // SAFETY: the caller's contract
    unsafe {
        let tags = &mut *p;
        let mut out = ScanPoints { default_point: *default_point, top: *top, bar: *bar };
        tags.collect(&CppModels, part as usize, model as usize, &*parent, part0 as usize, last5, &mut out);
        *default_point = out.default_point;
        *top = out.top;
        *bar = out.bar;
        *weapons.add(part as usize) = tags.weapon_ptr(part as usize);
    }
}

/// `GetTagPoints`' lookup: the list of `kind` for `model` (NULL without one or without a store).
///
/// # Safety
/// `p` is NULL or a live store.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_find(
    p: *const EeGfxGraphicTags,
    model: *const c_void,
    kind: c_long,
) -> *mut EeGfxTagList {
    if p.is_null() {
        return std::ptr::null_mut();
    }
    // SAFETY: the caller's contract
    unsafe { (*p).find_ptr(model as usize, kind) }
}

/// `PickPoint`'s attack point at game time `time` (NULL: none, the default point then).
///
/// # Safety
/// `p` is NULL or a live store.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_pick(p: *const EeGfxGraphicTags, time: c_long) -> *const Ge3DPoint {
    if p.is_null() {
        return std::ptr::null();
    }
    // SAFETY: the caller's contract
    match unsafe { (*p).pick(time) } {
        Some(point) => point,
        None => std::ptr::null(),
    }
}

/// `SetGroupColor(color, part)`.
///
/// # Safety
/// `p` is NULL or a live store whose models are live; `color` is three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_set_color(p: *const EeGfxGraphicTags, part: c_ulong, color: *const f32) {
    if p.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    unsafe { (*p).set_color(&CppModels, part as usize, *(color as *const [f32; 3])) }
}

/// The unload's release of part `part`'s list.
///
/// # Safety
/// `p` is NULL or a live store whose models are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_clear_weapon(p: *mut EeGfxGraphicTags, part: c_ulong) {
    if p.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    unsafe { (*p).clear_weapon(&CppModels, part as usize) }
}

/// `EETagPoint::GetOffset(frame[, part])` (`part` NULL: none).
///
/// # Safety
/// `point` is a live point of a list; `part` NULL or valid; `out` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tag_offset(
    point: *const EeGfxTagPoint,
    frame: c_ulong,
    part: *const GeTransformation,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the caller's contract
    unsafe { *out = offset(&CppModels, &*point, frame as usize, part.as_ref()) }
}

/// `EETagPoint::GetPoint(frame, transformation[, part])` (`part` NULL: none).
///
/// # Safety
/// As [`ee_gfx_tag_offset`]; `transformation` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tag_point(
    point: *const EeGfxTagPoint,
    frame: c_ulong,
    transformation: *const GeTransformation,
    part: *const GeTransformation,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the caller's contract
    unsafe { *out = point_at(&CppModels, &*point, frame as usize, &*transformation, part.as_ref()) }
}

/// `EETagPointList::Scale(transformation, scale)`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_tags_scale(t: *mut GeTransformation, s: f32) {
    // SAFETY: the caller's contract
    scale(unsafe { &mut *t }, s)
}

#[cfg(test)]
#[path = "tags_tests.rs"]
mod tests;
