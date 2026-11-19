//! `EEGraphicObject`'s media (EEGraphicObject.cpp 0xd85ed LoadMedia, 0xd907e UnloadMedia, 0xd8b09 LoadTexture,
//! 0xd76a1 CollectParts; the graphics + LLE cluster's third round): the first use loads the shared model, the shadow
//! model and texture, each part's texture and model (the animated parts that share frames with the frames their
//! model shares), the parts' tag points (tags.rs), the sounds and the part scan; the last use releases them in the
//! original's order.
//!
//! The object is the C++ `EEGraphicObject` (its members read and written in place through the registered layout,
//! effect_reads.rs); the models and textures are the Graphics Engine's Rust objects (the model manager's load, the
//! texture manager's load, the stage settings, the references) reached directly. The C++ keeps the media lock
//! (`EEMediaAcquire` / `EEMediaRelease` around these calls) and what [`EeGfxGobjHost`] answers: the sounds' virtual
//! `LoadMedia` / `UnloadMedia` and the part lists (`EETurretPart`, `EEOrbital`, `EEWheel` are C++ objects other
//! code reads).

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use ee_base::math::types::{Ge3DPoint, GeTransformation};
use ee_data::tables::{graphic_frame_maps, EeDataGraphicObjectFields, GraphicFramePart, EE_DATA_GRAPHIC_STATES};

use crate::ge::cppobj;
use crate::ge::manobj::{ee_ge_modman_load, EeGeModelManager};
use crate::ge::model_ffi::{EeGeSharedFrame, EeGeSharedFrames};
use crate::ge::modobj::{ee_ge_mod_dereference, ee_ge_mod_get, ee_ge_mod_ptr, EeGeModPtr, EeGeModVal, EeGeModel};
use crate::ge::texobj::{ee_ge_texman_load, EeGeTextureDescription, EeGeTextureManager};
use crate::ge::texobj_tex::{
    ee_ge_tex_dereference, ee_ge_tex_set_stage, ee_ge_tex_set_stage_bias, EeGeTexStageField, EeGeTexture,
};
use crate::gfx::effect_orbit::is_orbital_name;
use crate::gfx::effect_reads::{layout, EeGfxObjectLayout};
use crate::gfx::tags::{
    ee_gfx_tags_clear_weapon, ee_gfx_tags_collect, ee_gfx_tags_free, ee_gfx_tags_new, EeGfxGraphicTags, EeGfxTagList,
};

/// The parts of a graphic object (`EEGraphicObject::mModels`, `mTextures`, the names, the sounds).
const PARTS: usize = EE_DATA_GRAPHIC_STATES;

/// The C++ calls of the load and unload (EEGraphicObject.cpp).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxGobjHost {
    /// `sound->LoadMedia()` (an `EESoundObject*`, the `EEMedia` virtual)
    pub sound_load: extern "C" fn(sound: *mut c_void),
    /// `sound->UnloadMedia()`
    pub sound_unload: extern "C" fn(sound: *mut c_void),
    /// `mTurretParts.push_back(new EETurretPart(position, transformation, model))`
    pub add_turret:
        extern "C" fn(object: *mut c_void, position: *const Ge3DPoint, at: *const GeTransformation, model: *mut c_void),
    /// `mOrbitals.push_back(new EEOrbital(model))`
    pub add_orbital: extern "C" fn(object: *mut c_void, model: *mut c_void),
    /// `EEIsWheelModel(model)`, then `mWheels.push_back(new EEWheel(model))` when it is; whether it was
    pub add_wheel: extern "C" fn(object: *mut c_void, model: *mut c_void) -> bool,
    /// The orbitals, turret parts and wheels deleted and their lists emptied (in that order)
    pub clear_parts: extern "C" fn(object: *mut c_void),
}

/// One load's world: the managers (`gModelManager`, `gTextureManager`: the C++ shells, their Rust object their
/// only member), the texture settings (`gTexturePixelDepth`, `gTextureMipLevelsToDrop`, `gTrilinearFilter`) and the
/// host.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxGobjLoad {
    pub model_manager: *mut c_void,
    pub texture_manager: *mut c_void,
    pub pixel_depth: c_ulong,
    pub mip_levels_to_drop: c_ulong,
    pub trilinear: bool,
    pub host: *const EeGfxGobjHost,
}

// ---- the decisions (no C++ object)

/// The shared-frame maps of the 14 parts (`std::map<unsigned long, long> theFrames[15]`, the shared model's last):
/// each slot's entries in frame order, a frame's first value kept (`std::map::insert`); and each part's slot.
#[allow(clippy::unnecessary_cast)] // c_ulong and c_long are u64 and i64 on LP64
pub fn frame_maps(parts: &[GraphicFramePart<'_>]) -> (Vec<Vec<EeGeSharedFrame>>, Vec<c_long>) {
    let mut maps: Vec<BTreeMap<c_ulong, c_long>> = vec![BTreeMap::new(); PARTS + 1];
    let slots = graphic_frame_maps(parts, |slot, frame, value| {
        maps[slot as usize].entry(frame).or_insert(value);
    });
    let frames = maps
        .iter()
        .map(|m| {
            m.iter().map(|(&frame, &offset)| EeGeSharedFrame { frame: frame as u32, offset: offset as i32 }).collect()
        })
        .collect();
    (frames, slots)
}

/// The parts' texture's mip levels to drop: the record's and the settings' sum (unsigned, as the C++'s mixed sum)
/// while below the record's maximum, else the maximum.
pub fn mip_levels_to_drop(record: c_long, settings: c_ulong, maximum: c_ulong) -> c_long {
    let sum = (record as c_ulong).wrapping_add(settings);
    if sum < maximum {
        sum as c_long
    } else {
        maximum as c_long
    }
}

/// What a model of the part scan is, by its name: a turret part ("turret_", case kept), an orbital ("orbital_cw_"
/// or "orbital_ccw_", any case), else possibly a wheel (the host's `EEIsWheelModel`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PartKind {
    Turret,
    Orbital,
    Other,
}

pub fn part_kind(name: &[u8]) -> PartKind {
    if name.starts_with(b"turret_") {
        PartKind::Turret
    } else if is_orbital_name(name) {
        PartKind::Orbital
    } else {
        PartKind::Other
    }
}

/// The models the part scan walks.
pub trait PartModels {
    fn name(&self, model: usize) -> Vec<u8>;
    /// `GetInternalTransform(0)`
    fn internal0(&self, model: usize) -> GeTransformation;
    fn sub_models(&self, model: usize) -> Vec<usize>;
}

/// What the part scan found, in its order.
pub trait PartSink {
    fn turret(&mut self, position: Ge3DPoint, at: &GeTransformation, model: usize);
    fn orbital(&mut self, model: usize);
    /// A wheel when the model is one (`EEIsWheelModel`).
    fn maybe_wheel(&mut self, model: usize);
}

/// `CollectParts(model, parent)` (0xd76a1): the model under the parent's transformation, then its sub-models under
/// that one, depth first.
pub fn collect_parts(models: &impl PartModels, sink: &mut impl PartSink, model: usize, parent: &GeTransformation) {
    if model == 0 {
        return;
    }
    let at = parent.compose(&models.internal0(model));
    match part_kind(&models.name(model)) {
        PartKind::Turret => sink.turret(at.apply_point(Ge3DPoint::default()), &at, model),
        PartKind::Orbital => sink.orbital(model),
        PartKind::Other => sink.maybe_wheel(model),
    }
    for sub in models.sub_models(model) {
        collect_parts(models, sink, sub, &at);
    }
}

// ---- the C++ object

struct CppPartModels;

fn model_rust(model: usize) -> *mut EeGeModel {
    // SAFETY: a live GEModel (the scan's)
    unsafe { cppobj::counted::<EeGeModel>(model) }
}

impl PartModels for CppPartModels {
    fn name(&self, model: usize) -> Vec<u8> {
        // SAFETY: the model's name (a UString)
        unsafe { c_bytes(cppobj::str_text(ee_ge_mod_ptr(model_rust(model), EeGeModPtr::Name) as usize)) }
    }
    fn internal0(&self, model: usize) -> GeTransformation {
        // SAFETY: the model's per-frame transformations
        unsafe { *(ee_ge_mod_ptr(model_rust(model), EeGeModPtr::InternalTransforms) as *const GeTransformation) }
    }
    fn sub_models(&self, model: usize) -> Vec<usize> {
        // SAFETY: the model's sub-models
        unsafe {
            let n = ee_ge_mod_get(model_rust(model), EeGeModVal::SubModelCount) as usize;
            let subs = ee_ge_mod_ptr(model_rust(model), EeGeModPtr::SubModels) as *const usize;
            (0..n).map(|i| *subs.add(i)).collect()
        }
    }
}

struct HostParts<'a> {
    object: usize,
    host: &'a EeGfxGobjHost,
}

impl PartSink for HostParts<'_> {
    fn turret(&mut self, position: Ge3DPoint, at: &GeTransformation, model: usize) {
        (self.host.add_turret)(self.object as *mut c_void, &position, at, model as *mut c_void);
    }
    fn orbital(&mut self, model: usize) {
        (self.host.add_orbital)(self.object as *mut c_void, model as *mut c_void);
    }
    fn maybe_wheel(&mut self, model: usize) {
        (self.host.add_wheel)(self.object as *mut c_void, model as *mut c_void);
    }
}

fn c_bytes(text: *const c_char) -> Vec<u8> {
    if text.is_null() {
        return Vec::new();
    }
    // SAFETY: a UString's NUL-terminated text
    unsafe { CStr::from_ptr(text) }.to_bytes().to_vec()
}

/// The object's members through the layout.
#[derive(Clone, Copy)]
struct Obj(usize);

impl Obj {
    fn slot(self, off: u64, i: usize) -> *mut usize {
        (self.0 + off as usize + i * std::mem::size_of::<usize>()) as *mut usize
    }
    fn get(self, off: u64, i: usize) -> usize {
        // SAFETY: a live object's pointer member (the layout's)
        unsafe { std::ptr::read_unaligned(self.slot(off, i)) }
    }
    fn set(self, off: u64, i: usize, value: usize) {
        // SAFETY: as get
        unsafe { std::ptr::write_unaligned(self.slot(off, i), value) }
    }
    fn fields(self, l: &EeGfxObjectLayout) -> EeDataGraphicObjectFields {
        // SAFETY: the object's record base
        unsafe { std::ptr::read_unaligned((self.0 + l.gobj_fields as usize) as *const EeDataGraphicObjectFields) }
    }
    fn point(self, off: u64) -> *mut Ge3DPoint {
        (self.0 + off as usize) as *mut Ge3DPoint
    }
}

/// A `UString*` name's text and whether it has one (`name != NULL && name->GetLength() > 0`).
fn name_text(name: usize) -> Option<*const c_char> {
    if name == 0 {
        return None;
    }
    // SAFETY: a live UString
    unsafe { (cppobj::str_length(name) > 0).then(|| cppobj::str_text(name)) }
}

fn load_model(load: &EeGfxGobjLoad, name: *const c_char, shared: &[EeGeSharedFrame]) -> usize {
    let frames = EeGeSharedFrames::of(shared);
    // SAFETY: the live manager (its Rust object its only member); the name's text
    unsafe { ee_ge_modman_load(cppobj::plain::<EeGeModelManager>(load.model_manager as usize), name, &frames) as usize }
}

/// `LoadTexture(name, slot, description, drawn)` (0xd8b09): loaded into an empty slot, referenced, modulated,
/// filtered, biased, wrapped.
fn load_texture(
    load: &EeGfxGobjLoad,
    obj: Obj,
    off: u64,
    i: usize,
    name: usize,
    d: &EeGeTextureDescription,
    bias: f32,
) {
    if obj.get(off, i) != 0 {
        return;
    }
    let Some(text) = name_text(name) else { return };
    // SAFETY: the live manager
    let texture = unsafe {
        ee_ge_texman_load(cppobj::plain::<EeGeTextureManager>(load.texture_manager as usize), text, d) as usize
    };
    obj.set(off, i, texture);
    if texture == 0 {
        return;
    }
    // SAFETY: the texture just loaded
    unsafe {
        cppobj::reference(texture);
        let t = cppobj::counted::<EeGeTexture>(texture);
        ee_ge_tex_set_stage(t, EeGeTexStageField::AlphaMode, 2);
        ee_ge_tex_set_stage(t, EeGeTexStageField::ColorMode, 2);
        ee_ge_tex_set_stage(t, EeGeTexStageField::MinFilterMode, 1);
        ee_ge_tex_set_stage(t, EeGeTexStageField::MagFilterMode, 1);
        ee_ge_tex_set_stage_bias(t, bias);
        ee_ge_tex_set_stage(t, EeGeTexStageField::TextureAddressingMode, 2);
        ee_ge_tex_set_stage(t, EeGeTexStageField::MipFilterMode, if load.trilinear { 2 } else { 1 });
    }
}

fn description(load: &EeGfxGobjLoad, has_alpha: bool) -> EeGeTextureDescription {
    EeGeTextureDescription {
        surface_volatility: 1,
        frame_format: 0,
        pixel_depth: load.pixel_depth,
        num_mip_levels_to_drop: 0,
        is_3d_renderable: false,
        is_transparency_enabled: false,
        has_alpha,
        create_mipmaps: true,
        use_texture_pack: true,
    }
}

/// `EEGraphicObject::LoadMedia`'s first load (the C++ runs it under the database lock, `EEMediaAcquire`): 0, or 9
/// when a model failed.
///
/// # Safety
/// `object` is a live `EEGraphicObject` of the registered layout; `load` and its host are valid; the managers live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_gobj_load(object: *mut c_void, load: *const EeGfxGobjLoad) -> c_long {
    let l = layout();
    // SAFETY: the caller's contract
    let load = unsafe { &*load };
    // SAFETY: the caller's contract
    let host = unsafe { &*load.host };
    let obj = Obj(object as usize);
    let f = obj.fields(l);
    let mut result = 0;
    if obj.get(l.gobj_tags, 0) == 0 {
        obj.set(l.gobj_tags, 0, ee_gfx_tags_new() as usize);
    }
    // the texture-frame maps of the animated parts that share frames, by model (ee_data tables.rs)
    let names: Vec<Vec<u8>> = (0..PARTS)
        .map(|i| {
            let n = obj.get(l.gobj_model_names, i);
            // SAFETY: a live UString
            if n == 0 {
                Vec::new()
            } else {
                c_bytes(unsafe { cppobj::str_text(n) })
            }
        })
        .collect();
    let parts: Vec<GraphicFramePart<'_>> = (0..PARTS)
        .map(|i| GraphicFramePart {
            frame_count: f.mFrameCounts[i],
            shares: f.mSharesFrames[i],
            start: f.mStartFrames[i],
            last: f.mLastFrames[i],
            model: (obj.get(l.gobj_model_names, i) != 0).then_some(names[i].as_slice()),
        })
        .collect();
    let (frames, slots) = frame_maps(&parts);
    if obj.get(l.gobj_model, 0) == 0 {
        if let Some(name) = name_text(obj.get(l.gobj_file_name, 0)) {
            let model = load_model(load, name, &frames[PARTS]);
            obj.set(l.gobj_model, 0, model);
            if model != 0 {
                // SAFETY: the model just loaded
                unsafe { cppobj::reference(model) };
            }
        }
    }
    if obj.get(l.gobj_shadow_model, 0) == 0 {
        if let Some(name) = name_text(obj.get(l.gobj_shadow_model_name, 0)) {
            let model = load_model(load, name, &[]);
            obj.set(l.gobj_shadow_model, 0, model);
            if model != 0 {
                // SAFETY: the model just loaded
                unsafe { cppobj::reference(model) };
            } else {
                result = 9;
            }
        }
    }
    let mut d = description(load, f.mHasAlpha);
    d.create_mipmaps = f.mCreateMipMaps;
    d.num_mip_levels_to_drop =
        mip_levels_to_drop(f.mNumMipLevelsToDrop, load.mip_levels_to_drop, f.mMaxMipLevelsToDrop);
    let shadow = description(load, true);
    let bias = f.mMipMapBias;
    let texture_name = obj.get(l.gobj_texture_name, 0);
    if texture_name != 0 {
        load_texture(load, obj, l.gobj_texture, 0, texture_name, &d, bias);
    }
    let shadow_name = obj.get(l.gobj_shadow_texture_name, 0);
    if shadow_name != 0 {
        load_texture(load, obj, l.gobj_shadow_texture, 0, shadow_name, &shadow, bias);
    }
    let identity = GeTransformation::IDENTITY;
    for part in 0..PARTS {
        let texture_name = obj.get(l.gobj_texture_names, part);
        let shared_texture = obj.get(l.gobj_texture, 0);
        if name_text(texture_name).is_some() {
            load_texture(load, obj, l.gobj_textures, part, texture_name, &d, bias);
        } else if shared_texture != 0 {
            // SAFETY: the loaded shared texture
            unsafe { cppobj::reference(shared_texture) };
            obj.set(l.gobj_textures, part, shared_texture);
        }
        let shared_model = obj.get(l.gobj_model, 0);
        if let Some(name) = name_text(obj.get(l.gobj_model_names, part)) {
            let shares = f.mFrameCounts[part] > 4 && f.mSharesFrames[part];
            let shared: &[EeGeSharedFrame] = if shares { &frames[slots[part] as usize] } else { &[] };
            let model = load_model(load, name, shared);
            obj.set(l.gobj_models, part, model);
            if model != 0 {
                // SAFETY: the model just loaded
                unsafe { cppobj::reference(model) };
            } else {
                result = 9;
            }
        } else if shared_model != 0 {
            // SAFETY: the loaded shared model
            unsafe { cppobj::reference(shared_model) };
            obj.set(l.gobj_models, part, shared_model);
        } else {
            obj.set(l.gobj_models, part, 0);
        }
        // the part's tag points, colour groups and points (tags.rs: CollectTagPoints 0xd8bb2)
        let model = obj.get(l.gobj_models, part);
        if model != 0 {
            // SAFETY: the store, the models and the object's points and lists
            unsafe {
                ee_gfx_tags_collect(
                    obj.get(l.gobj_tags, 0) as *mut EeGfxGraphicTags,
                    part as c_ulong,
                    model as *mut c_void,
                    &identity,
                    obj.get(l.gobj_models, 0) as *mut c_void,
                    f.mLastFrames[5],
                    obj.point(l.gobj_default_point),
                    obj.point(l.gobj_top_point),
                    obj.point(l.gobj_bar),
                    obj.slot(l.gobj_tag_points, 0) as *mut *mut EeGfxTagList,
                );
            }
        }
    }
    for part in 0..PARTS {
        for off in [l.gobj_sounds1, l.gobj_sounds2] {
            let sound = obj.get(off, part);
            if sound != 0 {
                (host.sound_load)(sound as *mut c_void);
            }
        }
    }
    let shared_model = obj.get(l.gobj_model, 0);
    if shared_model != 0 {
        collect_parts(&CppPartModels, &mut HostParts { object: obj.0, host }, shared_model, &identity);
    }
    result
}

fn dereference_model(model: usize) {
    // SAFETY: a live model the object references
    unsafe { ee_ge_mod_dereference(cppobj::counted::<EeGeModel>(model)) }
}

fn dereference_texture(texture: usize) {
    // SAFETY: a live texture the object references
    unsafe { ee_ge_tex_dereference(cppobj::counted::<EeGeTexture>(texture)) }
}

/// `EEGraphicObject::UnloadMedia`'s last unload (the C++ runs it under the database lock, `EEMediaRelease`): the
/// shadow model, each part's texture, weapon list and model, the tag store, the shared model and texture, the
/// shadow texture, the sounds, the parts.
///
/// # Safety
/// `object` is a live `EEGraphicObject` of the registered layout; `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_gobj_unload(object: *mut c_void, host: *const EeGfxGobjHost) {
    let l = layout();
    // SAFETY: the caller's contract
    let host = unsafe { &*host };
    let obj = Obj(object as usize);
    let shadow = obj.get(l.gobj_shadow_model, 0);
    if shadow != 0 {
        dereference_model(shadow);
        obj.set(l.gobj_shadow_model, 0, 0);
    }
    let tags = obj.get(l.gobj_tags, 0) as *mut EeGfxGraphicTags;
    for i in 0..PARTS {
        let texture = obj.get(l.gobj_textures, i);
        if texture != 0 {
            dereference_texture(texture);
            obj.set(l.gobj_textures, i, 0);
        }
        // SAFETY: the store (or NULL) and its live models
        unsafe { ee_gfx_tags_clear_weapon(tags, i as c_ulong) };
        obj.set(l.gobj_tag_points, i, 0);
        let model = obj.get(l.gobj_models, i);
        if model != 0 {
            dereference_model(model);
            obj.set(l.gobj_models, i, 0);
        }
    }
    // the kinds' lists (by kind, then by model), the colour groups and the attack points (tags.rs)
    // SAFETY: the store (or NULL), not used again
    unsafe { ee_gfx_tags_free(tags) };
    obj.set(l.gobj_tags, 0, 0);
    let model = obj.get(l.gobj_model, 0);
    if model != 0 {
        dereference_model(model);
        obj.set(l.gobj_model, 0, 0);
    }
    for off in [l.gobj_texture, l.gobj_shadow_texture] {
        let texture = obj.get(off, 0);
        if texture != 0 {
            dereference_texture(texture);
            obj.set(off, 0, 0);
        }
    }
    for part in 0..PARTS {
        for off in [l.gobj_sounds1, l.gobj_sounds2] {
            let sound = obj.get(off, part);
            if sound != 0 {
                (host.sound_unload)(sound as *mut c_void);
            }
        }
    }
    (host.clear_parts)(object);
}

#[cfg(test)]
#[path = "gobj_media_tests.rs"]
mod tests;
