//! The effects' answers that read the graphic objects, the tag points and the units' physical models (the Graphics
//! and LLE cluster, round 1): what `EEEffectHost.cpp`'s `Ask` answered by reading `EEGraphicObject` (its models,
//! textures, frames, orbitals, turret parts and tag points: tags.rs), `EEGraphicEffect`'s record and the complex
//! units' `mPhysicalModel` (location, scale, frame) and turret angles. The FFI hosts (effect_rising.rs,
//! effect_2d.rs) answer them here; the tests' hosts answer their own.
//!
//! The C++ objects are read in place at the offsets this build's compiler gave them ([`EeGfxObjectLayout`],
//! registered by `EEGraphicObject::LoadTable`), the units through `ee_sim::world`'s views. One call stays the C++'s:
//! a unit's graphic (`EEUnit::GetModel`, a virtual: a foundation shows its player's class graphic), asked through
//! the host before the views are opened.

// the C long widths differ by target (LP64): the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::OnceLock;

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};
use ee_data::tables::{EeDataGraphicEffectFields, EeDataGraphicObjectFields};
use ee_sim::world::{UnitView, WorldRead};
use ee_sim::world_field;

use crate::ge::cppobj;
use crate::ge::modobj::{ee_ge_mod_ptr, EeGeModPtr, EeGeModel};
use crate::gfx::effect_additive::EeGfxUnitFrame;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_glow::{EeGfxGlowPlace, EeGfxGlowSetUp};
use crate::gfx::effect_icons::EeGfxIconUnit;
use crate::gfx::effect_orbit::{EeGfxAnimation, EeGfxOrbital};
use crate::gfx::effect_particles::EeGfxEmitterUnit;
use crate::gfx::effect_smoke::EeGfxSmokeUnit;
use crate::gfx::effect_sprite::EeGfx2DPoint;
use crate::gfx::effect_state::EeGfxEffectRecord;
use crate::gfx::effect_tracer::EeGfxTracerEnds;
use crate::gfx::effect_vent::{EeGfxFrames, EeGfxVentPoints};
use crate::gfx::raster::EeGfxModel;
use crate::gfx::tags::{self, CppModels, EeGfxGraphicTags, EeGfxTagList};

/// Where the C++ compiler put the members read here (offsets from the object's address: an `EEGraphicObject*`,
/// `EEGraphicEffect*`, `EETurretPart*`, `EEOrbital*` or `EEComplexUnit*`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxObjectLayout {
    /// `sizeof(EeGfxObjectLayout)` as the C++ compiled it
    pub size: u64,
    /// `EEGraphicObject`: its record base (`EeDataGraphicObjectFields`), `mModels[14]`, `mTextures[14]`,
    /// `mTagPoints[14]`, `mTags`, `mModel`, `mDefaultPoint`, `mTopPoint`, the vectors `mTurretParts` and `mOrbitals`
    pub gobj_fields: u64,
    pub gobj_models: u64,
    pub gobj_textures: u64,
    pub gobj_tag_points: u64,
    pub gobj_tags: u64,
    pub gobj_model: u64,
    pub gobj_default_point: u64,
    pub gobj_top_point: u64,
    pub gobj_turret_parts: u64,
    pub gobj_orbitals: u64,
    /// `EETurretPart::mModel`; `EEOrbital::mModel`, `mSpeed`, `mAngle`
    pub turret_part_model: u64,
    pub orbital_model: u64,
    pub orbital_speed: u64,
    pub orbital_angle: u64,
    /// `EEGraphicEffect`: its record base (`EeDataGraphicEffectFields`), `mGraphicObject`
    pub geffect_fields: u64,
    pub geffect_object: u64,
    /// `EEComplexUnit`: `mPhysicalModel`'s scale and frame, `mTurretAngles`
    pub complex_scale: u64,
    pub complex_frame: u64,
    pub complex_turret_angles: u64,
    /// `EEUnitClassUpgrade::mModel` (the stats' graphic)
    pub stats_model: u64,
}

static LAYOUT: OnceLock<EeGfxObjectLayout> = OnceLock::new();

/// Registers the layout (once; a second registration must be the same).
///
/// # Safety
/// `layout` points to a layout the C++ filled.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_object_layout_set(layout: *const EeGfxObjectLayout) {
    // SAFETY: the caller's contract
    let l = unsafe { *layout };
    assert_eq!(l.size, std::mem::size_of::<EeGfxObjectLayout>() as u64, "ee_gfx: a stale EeGfxObjectLayout header");
    let set = LAYOUT.get_or_init(|| l);
    assert_eq!(*set, l, "ee_gfx: two object layouts");
}

/// Registers a layout for the tests (the first one wins).
#[cfg(test)]
pub(crate) fn set_test_layout(l: EeGfxObjectLayout) {
    let _ = LAYOUT.set(l);
}

fn l() -> &'static EeGfxObjectLayout {
    LAYOUT.get().expect("ee_gfx: the object layout is registered (EEGraphicObject::LoadTable)")
}

const PTR: usize = std::mem::size_of::<usize>();

/// A member of `T` at `base + off`.
///
/// # Safety
/// The member is live and of type `T`.
unsafe fn rd<T: Copy>(base: usize, off: u64) -> T {
    // SAFETY: the caller's contract
    unsafe { std::ptr::read_unaligned((base + off as usize) as *const T) }
}

/// A `std::vector<T*>`'s elements (its first two words are its begin and end, as EEWorldLayout.cpp checks).
///
/// # Safety
/// `at` is a live vector of pointers.
unsafe fn vector(at: usize) -> Vec<usize> {
    // SAFETY: the caller's contract
    unsafe {
        let begin: usize = rd(at, 0);
        let end: usize = rd(at, PTR as u64);
        (0..(end - begin) / PTR).map(|i| rd(begin, (i * PTR) as u64)).collect()
    }
}

// ---- the models

fn model_ptr(model: usize, field: EeGeModPtr) -> *mut c_void {
    // SAFETY: a live GEModel (the readers' contract)
    unsafe { ee_ge_mod_ptr(cppobj::counted::<EeGeModel>(model), field) }
}

/// `GetBoundingBox(frame)`.
pub fn model_box(model: usize, frame: usize) -> GeBoundingBox {
    // SAFETY: the model's per-frame boxes
    unsafe { *(model_ptr(model, EeGeModPtr::BoundingBoxes) as *const GeBoundingBox).add(frame) }
}

/// `GetBoundingRadius(frame)`.
pub fn model_radius(model: usize, frame: usize) -> f32 {
    // SAFETY: the model's per-frame radii
    unsafe { *(model_ptr(model, EeGeModPtr::BoundingRadii) as *const f32).add(frame) }
}

/// `GetCenterPoint()`.
pub fn model_center(model: usize) -> Ge3DPoint {
    // SAFETY: the model's centre
    unsafe { *(model_ptr(model, EeGeModPtr::Center) as *const Ge3DPoint) }
}

// ---- the graphic objects

/// An `EEGraphicObject*` (`EEModel*`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Gobj(pub usize);

impl Gobj {
    /// The object `h` (None for NULL).
    pub fn of(h: usize) -> Option<Gobj> {
        (h != 0).then_some(Gobj(h))
    }
    fn at<T: Copy>(self, off: u64) -> T {
        // SAFETY: a live graphic object's member (the readers' contract)
        unsafe { rd(self.0, off) }
    }
    /// `mModels[i]`
    pub fn model(self, i: usize) -> usize {
        self.at(l().gobj_models + (i * PTR) as u64)
    }
    /// `mTextures[i]`
    pub fn texture(self, i: usize) -> usize {
        self.at(l().gobj_textures + (i * PTR) as u64)
    }
    /// `mModel`
    pub fn shared_model(self) -> usize {
        self.at(l().gobj_model)
    }
    pub fn default_point(self) -> Ge3DPoint {
        self.at(l().gobj_default_point)
    }
    pub fn top_point(self) -> Ge3DPoint {
        self.at(l().gobj_top_point)
    }
    /// The record (frames, loops)
    pub fn fields(self) -> &'static EeDataGraphicObjectFields {
        // SAFETY: a live object's record base
        unsafe { &*((self.0 + l().gobj_fields as usize) as *const EeDataGraphicObjectFields) }
    }
    /// Animation `i`'s start, frame time, count and last frame
    pub fn frames(self, i: usize) -> EeGfxFrames {
        let f = self.fields();
        EeGfxFrames {
            start: f.mStartFrames[i] as u32,
            time: f.mFrameTimes[i] as u32,
            count: f.mFrameCounts[i] as u32,
            last: f.mLastFrames[i] as u32,
        }
    }
    /// `mTagPoints[part]` (the weapon mounts)
    pub fn weapon_tags(self, part: usize) -> Option<&'static EeGfxTagList> {
        let list: *const EeGfxTagList = self.at(l().gobj_tag_points + (part * PTR) as u64);
        // SAFETY: a list of the object's store, live while the object is loaded
        unsafe { list.as_ref() }
    }
    /// `GetTagPoints(part, kind)`
    pub fn tag_points(self, part: usize, kind: c_long) -> Option<&'static EeGfxTagList> {
        let store: *const EeGfxGraphicTags = self.at(l().gobj_tags);
        // SAFETY: the object's store, live while the object is loaded
        unsafe { store.as_ref() }.and_then(|s| s.find(self.model(part), kind))
    }
    fn turret_parts(self) -> Vec<usize> {
        // SAFETY: the object's vector of turret parts
        unsafe { vector(self.0 + l().gobj_turret_parts as usize) }
    }
    /// `GetNumParts()`
    pub fn num_parts(self) -> usize {
        self.turret_parts().len()
    }
    /// `FindTurretPart(model, out)`: 0 and the part's index when one is `model`'s, else 3
    pub fn find_turret_part(self, model: usize) -> (i32, Option<usize>) {
        for (i, &part) in self.turret_parts().iter().enumerate() {
            // SAFETY: a live turret part
            if unsafe { rd::<usize>(part, l().turret_part_model) } == model {
                return (0, Some(i));
            }
        }
        (3, None)
    }
    fn orbitals(self) -> Vec<usize> {
        // SAFETY: the object's vector of orbitals
        unsafe { vector(self.0 + l().gobj_orbitals as usize) }
    }
    /// `GetOrbitalCount()`
    pub fn orbital_count(self) -> u32 {
        self.orbitals().len() as u32
    }
    /// A copy of `*GetOrbital(i)`
    pub fn orbital(self, i: usize) -> EeGfxOrbital {
        let o = self.orbitals()[i];
        // SAFETY: a live orbital
        unsafe {
            EeGfxOrbital {
                model: rd::<usize>(o, l().orbital_model) as EeGfxModel,
                speed: rd(o, l().orbital_speed),
                angle: rd(o, l().orbital_angle),
            }
        }
    }
}

/// An `EEGraphicEffect*`'s record and object.
#[derive(Clone, Copy, Debug)]
pub struct GraphicEffect(pub usize);

impl GraphicEffect {
    pub fn fields(self) -> &'static EeDataGraphicEffectFields {
        // SAFETY: a live graphic effect's record base
        unsafe { &*((self.0 + l().geffect_fields as usize) as *const EeDataGraphicEffectFields) }
    }
    /// `mGraphicObject`
    pub fn object(self) -> Option<Gobj> {
        // SAFETY: a live graphic effect's member
        Gobj::of(unsafe { rd(self.0, l().geffect_object) })
    }
}

// ---- a complex unit's physical model

/// A complex unit's `mPhysicalModel` and turret angles (`unit` an `EEComplexUnit*`).
#[derive(Clone, Copy, Debug)]
pub struct Physical {
    pub location: GeTransformation,
    pub scale: f32,
    pub frame: c_ulong,
}

/// The complex unit `c`'s physical model.
pub fn physical(c: &UnitView<'_>) -> Physical {
    let h = c.handle() as usize;
    // SAFETY: a live complex unit's members
    unsafe {
        Physical {
            location: GeTransformation { m: c.transform() },
            scale: rd(h, l().complex_scale),
            frame: rd(h, l().complex_frame),
        }
    }
}

/// `mTurretAngles[i]` of the complex unit `c`.
pub fn turret_angle(c: &UnitView<'_>, i: usize) -> f32 {
    // SAFETY: a live complex unit's angles (`mNumTurretAngles` of them)
    unsafe { *rd::<*const f32>(c.handle() as usize, l().complex_turret_angles).add(i) }
}

fn animation(c: &UnitView<'_>) -> usize {
    let a: c_ulong = world_field!(c.complex(), mAnimation);
    a as usize
}

fn position(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.x(), y: t.y(), z: t.z() }
}

fn set_position(t: &mut GeTransformation, p: Ge3DPoint) {
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
}

fn identity() -> GeTransformation {
    GeTransformation::IDENTITY
}

fn tag_point(
    list: &EeGfxTagList,
    i: usize,
    frame: c_ulong,
    t: &GeTransformation,
    part: Option<&GeTransformation>,
) -> Ge3DPoint {
    tags::point_at(&CppModels, list.point(i), frame as usize, t, part)
}

fn count(list: Option<&EeGfxTagList>) -> usize {
    list.map_or(0, |l| l.len)
}

// ---- the effect

/// The effect's C++ members the answers read (in both hosts): its record, its graphic effect, the addresses of its
/// kept graphic object, its unit's model and its target (members the C++ answers may change during the call), and
/// the one C++ call, a unit's graphic.
#[repr(C)]
pub struct EeGfxEffectRefs {
    pub record: *mut EeGfxEffectRecord,
    /// `mGraphicEffect` (an `EEGraphicEffect*`)
    pub graphic_effect: u64,
    /// `&mGraphicObject`, `&mUnitModel` (`EEGraphicObject**`), `&mTarget` (`EEUnit**`)
    pub graphic_object: *const u64,
    pub unit_model: *const u64,
    pub target: *const u64,
    /// `((EEUnit*)unit)->GetModel()`
    pub unit_graphic: extern "C" fn(context: *mut c_void, unit: u64) -> u64,
}

/// One answer's view of the effect: the host's context and refs, its unit and the current player.
pub struct Ask<'a> {
    pub context: *mut c_void,
    pub refs: &'a EeGfxEffectRefs,
    pub unit: u64,
    pub player: u64,
}

impl Ask<'_> {
    #[allow(clippy::mut_from_ref)]
    fn record(&self) -> &mut EeGfxEffectRecord {
        // SAFETY: the effect's live record; no other reference to it lives during an answer
        unsafe { &mut *self.refs.record }
    }
    fn graphic_effect(&self) -> GraphicEffect {
        GraphicEffect(self.refs.graphic_effect as usize)
    }
    /// `mGraphicObject` (as the call left it)
    fn object(&self) -> Gobj {
        // SAFETY: the effect's member
        Gobj(unsafe { *self.refs.graphic_object } as usize)
    }
    /// `mUnitModel`
    fn unit_model(&self) -> Gobj {
        // SAFETY: the effect's member
        Gobj(unsafe { *self.refs.unit_model } as usize)
    }
    fn target(&self) -> u64 {
        // SAFETY: the effect's member
        unsafe { *self.refs.target }
    }
    /// `((EEUnit*)unit)->GetModel()`
    fn graphic_of(&self, unit: u64) -> Gobj {
        Gobj((self.refs.unit_graphic)(self.context, unit) as usize)
    }
    /// The unit's complex unit's handle (`mUnit->mComplexUnit`).
    fn complex_handle(&self) -> u64 {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe {
            let t = WorldRead::begin();
            UnitView::of(&t, self.unit).expect("the effect's unit").complex_unit_handle()
        }
    }
    /// `f` over the view of the unit `handle`.
    fn view<R>(&self, handle: u64, f: impl FnOnce(UnitView<'_>) -> R) -> R {
        // SAFETY: as `complex_handle`
        unsafe {
            let t = WorldRead::begin();
            f(UnitView::of(&t, handle).expect("a live unit"))
        }
    }

    // ---- the graphic objects' reads

    /// `mGraphicEffect->mGraphicObject->mModels[0]->GetBoundingBox(0).GetHeight()`
    pub fn model_height(&self) -> f32 {
        let object = self.graphic_effect().object().expect("the graphic effect's object");
        model_box(object.model(0), 0).height
    }
    /// `mGraphicObject->mModel`
    pub fn model(&self) -> EeGfxModel {
        self.object().shared_model() as EeGfxModel
    }
    /// `mGraphicObject->mModels[i]`
    pub fn model_at(&self, i: u32) -> EeGfxModel {
        self.object().model(i as usize) as EeGfxModel
    }
    /// `mGraphicObject->mTextures[i]`
    pub fn texture_at(&self, i: u32) -> EeGfxModel {
        self.object().texture(i as usize) as EeGfxModel
    }
    /// `mGraphicObject`'s animation `i`
    pub fn frames(&self, i: u32) -> EeGfxFrames {
        self.object().frames(i as usize)
    }
    /// `mGraphicObject->mLoops[i]`
    pub fn loops(&self, i: u32) -> bool {
        self.object().fields().mLoops[i as usize]
    }
    /// `mGraphicObject`'s animation 13 (the disease's and the virus's)
    pub fn animation13(&self) -> EeGfxAnimation {
        let f = self.object().frames(13);
        EeGfxAnimation { start: f.start, frame_time: f.time, frame_count: f.count }
    }
    pub fn orbital_count(&self) -> u32 {
        self.object().orbital_count()
    }
    pub fn orbital(&self, i: u32) -> EeGfxOrbital {
        self.object().orbital(i as usize)
    }
    /// `mUnitModel->mModels[0]->GetBoundingRadius(0)`
    pub fn unit_model_radius(&self) -> f32 {
        model_radius(self.unit_model().model(0), 0)
    }
    /// `mUnitModel->mModels[i]`
    pub fn unit_model_at(&self, i: u32) -> EeGfxModel {
        self.unit_model().model(i as usize) as EeGfxModel
    }

    // ---- the unit's reads

    /// `mUnit->mComplexUnit`'s frame and animation
    pub fn unit_frame(&self) -> EeGfxUnitFrame {
        self.view(self.complex_handle(), |c| EeGfxUnitFrame {
            frame: physical(&c).frame as u32,
            animation: animation(&c) as u32,
        })
    }
    /// The citizen beam's unit (`dynamic_cast<EEComplexUnit*>(mUnit)`)
    pub fn beam_unit(&self) -> EeGfxBeamUnit {
        if self.unit == 0 {
            return EeGfxBeamUnit::default();
        }
        self.view(self.unit, |u| {
            if !u.is_complex() {
                return EeGfxBeamUnit::default();
            }
            EeGfxBeamUnit { complex: true, animation: animation(&u) as i32, frame: physical(&u).frame as c_long as i32 }
        })
    }
    /// `EEE3DBlastWave`'s and `EEE3DShield`'s `FollowUnit` (0xe904a): to the unit's first tag point of kind 0x12 in its
    /// scaled location (the orientation kept), else to its location; the record's transformation, then the host's
    /// `move_to`
    pub fn follow(&self, move_to: impl FnOnce()) {
        let c = self.complex_handle();
        let graphic = self.graphic_of(c);
        let t = self.view(c, |c| {
            let p = physical(&c);
            let mut location = p.location;
            match graphic.tag_points(animation(&c), 0x12).filter(|l| l.len != 0) {
                Some(list) => {
                    tags::scale(&mut location, p.scale);
                    let point = tag_point(list, 0, p.frame, &location, None);
                    set_position(&mut location, point);
                    location
                }
                None => location,
            }
        });
        self.record().transformation = t;
        move_to();
    }
    /// `EEE3DCitizenBeam::Aim` (0xe9606): the unit's first weapon tag point at its frame in its scaled location
    pub fn beam_start(&self) -> Ge3DPoint {
        let graphic = self.graphic_of(self.unit);
        self.view(self.complex_handle(), |c| {
            let p = physical(&c);
            let mut location = p.location;
            let list = graphic.weapon_tags(animation(&c)).expect("the beam's weapon tag points");
            tags::scale(&mut location, p.scale);
            tag_point(list, 0, p.frame, &location, None)
        })
    }
    /// The hurricane's spawn points: its graphic's tag points of kind 6 at animation 0 (0: none)
    pub fn spawn_count(&self) -> u32 {
        let object = self.graphic_effect().object().expect("the hurricane's object");
        count(object.tag_points(0, 6)) as u32
    }
    /// Spawn point `i` at `frame` around the unit, its location scaled by the graphic effect's size
    pub fn spawn_at(&self, i: u32, frame: u32) -> Ge3DPoint {
        let ge = self.graphic_effect();
        let list = ge.object().and_then(|o| o.tag_points(0, 6)).expect("the hurricane's spawn points");
        let size = ge.fields().mSize;
        self.view(self.complex_handle(), |c| {
            let mut location = physical(&c).location;
            tags::scale(&mut location, size);
            tag_point(list, i as usize, frame as c_ulong, &location, None)
        })
    }

    /// The damage vent's lists: kinds 5 (the vents) and 0x38 (the aims) of the unit's graphic at its complex unit's
    /// animation
    fn vent_lists(&self) -> (Option<&'static EeGfxTagList>, Option<&'static EeGfxTagList>) {
        let graphic = self.graphic_of(self.unit);
        let a = self.view(self.complex_handle(), |c| animation(&c));
        (graphic.tag_points(a, 5), graphic.tag_points(a, 0x38))
    }
    /// The vents' count: none without both lists (the original read through a null list there) or with fewer aims
    /// than vents
    pub fn vent_count(&self) -> u32 {
        match self.vent_lists() {
            (Some(from), Some(to)) if to.len >= from.len => from.len as u32,
            _ => 0,
        }
    }
    /// Vent `tag`'s and its aim's offsets at frame 0 (`EEE3DDamageVentPart::Aim`, 0xeb4e9)
    pub fn vent_offsets(&self, tag: i32) -> (Ge3DPoint, Ge3DPoint) {
        let (from, to) = self.vent_lists();
        let (from, to) = (from.expect("the vents"), to.expect("the aims"));
        (
            tags::offset(&CppModels, from.point(tag as usize), 0, None),
            tags::offset(&CppModels, to.point(tag as usize), 0, None),
        )
    }
    /// Vent `tag` and its aim at frame 0 in the unit's location scaled by its stats, and the vent's offset from the
    /// model's centre turned by that location (`EEE3DDamageVent::Step`)
    pub fn vent_points(&self, tag: i32) -> EeGfxVentPoints {
        let (from, to) = self.vent_lists();
        let (from, to) = (from.expect("the vents"), to.expect("the aims"));
        let c = self.complex_handle();
        let model = self.graphic_of(c).model(0);
        let (location, scale) = self.view(c, |c| (physical(&c).location, c.stats().expect("the unit's stats").scale()));
        let mut t = location;
        tags::scale(&mut t, scale);
        let i = tag as usize;
        let p_from = tag_point(from, i, 0, &t, None);
        let p_to = tag_point(to, i, 0, &t, None);
        let offset = tags::offset(&CppModels, from.point(i), 0, None);
        let center = model_center(model);
        let translation =
            t.apply_vector(GeVector { x: offset.x - center.x, y: offset.y - center.y, z: offset.z - center.z });
        EeGfxVentPoints { from: p_from, to: p_to, translation }
    }

    /// `EEE3DTracerBeam::Aim`'s ends (0xfc5a4): from the shooter's weapon tag point (on its turret part when the tag
    /// point's model is one), or its tag point of kind 6 (int param 1 is 1), or its stats' default point, to the
    /// target point on the target's location (scaled by the target's scale when the weapon tag point was used) or
    /// the fixed point
    pub fn tracer_ends(&self) -> EeGfxTracerEnds {
        let unit_h = self.complex_handle();
        let graphic = self.graphic_of(self.unit);
        let target = self.target();
        let target_c = if target != 0 { self.view(target, |t| t.complex_unit_handle()) } else { 0 };
        let int1 = self.graphic_effect().fields().mIntParam1;
        let r = self.record();
        let weapon = r.tracer.weapon_tag_point as usize;
        let target_point = r.tracer.target_point;
        let (mut from, scale, frame, a, turret) = self.view(unit_h, |c| {
            let p = physical(&c);
            (p.location, p.scale, p.frame, animation(&c), c.handle())
        });
        let mut to;
        let mut target_scale = 1.0f32;
        if target != 0 {
            let p = self.view(target_c, |t| physical(&t));
            to = p.location;
            target_scale = p.scale;
        } else {
            to = identity();
        }
        let mut start = Ge3DPoint::default();
        let tag_list = graphic.weapon_tags(a);
        if int1 == 1 {
            if let Some(other) = self.graphic_of(unit_h).tag_points(a, 6).filter(|l| l.len != 0) {
                tags::scale(&mut from, scale);
                start = tag_point(other, 0, frame, &from, None);
            }
        } else if int1 == 0 && count(tag_list) != 0 {
            let list = tag_list.expect("the weapon tag points");
            tags::scale(&mut from, scale);
            if graphic.num_parts() > 0 {
                let mut part = identity();
                if let (0, Some(i)) = graphic.find_turret_part(list.point(weapon).mModel as usize) {
                    let yaw = self.view(turret, |c| turret_angle(&c, i));
                    part.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
                }
                start = tag_point(list, weapon, frame, &from, Some(&part));
            } else {
                start = tag_point(list, weapon, frame, &from, None);
            }
            tags::scale(&mut to, target_scale);
        } else {
            let stats = self.view(self.unit, |u| u.stats().expect("the shooter's stats").handle());
            // SAFETY: the stats' graphic member
            let graphic: usize = unsafe { rd(stats as usize, l().stats_model) };
            let default_point = Gobj::of(graphic).expect("the stats' model").default_point();
            start = from.apply_point(default_point);
        }
        let end = if target != 0 { to.apply_point(target_point) } else { target_point };
        EeGfxTracerEnds { start, end, has_target: target != 0 }
    }

    /// The smoke generators' unit (`EEE3DSmokeGenerator::Step`, 0xf9a1b); the damage smoke's (kind 0x2c) whole unit
    pub fn smoke_unit(&self) -> EeGfxSmokeUnit {
        let mut facts = EeGfxSmokeUnit::default();
        if self.record().kind == 0x2c {
            let c = self.complex_handle();
            let list = self.graphic_of(self.unit).tag_points(self.view(c, |c| animation(&c)), 0);
            let (location, command) = self.view(c, |c| (physical(&c).location, c.command_type()));
            let (hp, max) =
                self.view(self.unit, |u| (u.hit_points(), u.stats().expect("the unit's stats").max_hit_points()));
            facts.has_unit = true;
            facts.position = position(&location);
            facts.command = command as i32;
            facts.hit_points = hp as i32;
            facts.max_hit_points = max as i32;
            facts.has_tag_points = list.is_some();
            facts.tag_point_count = count(list) as u32;
        } else if self.unit != 0 {
            let (location, command) = self.view(self.complex_handle(), |c| (physical(&c).location, c.command_type()));
            facts.has_unit = true;
            facts.position = position(&location);
            facts.command = command as i32;
        }
        facts
    }
    /// The damage smoke's tag point `i` at its unit's frame in its scaled location (its kind 0 points)
    pub fn smoke_tag_point(&self, i: u32) -> Ge3DPoint {
        let c = self.complex_handle();
        let graphic = self.graphic_of(self.unit);
        let (p, a) = self.view(c, |c| (physical(&c), animation(&c)));
        let list = graphic.tag_points(a, 0).expect("the smoke's tag points");
        let mut location = p.location;
        tags::scale(&mut location, p.scale);
        tag_point(list, i as usize, p.frame, &location, None)
    }

    /// The emitter's unit for its step (0xf6417): its death and hit points; followed (the graphic effect's flag 1),
    /// the tag point at its frame in its scaled location (turned by its turret part), else its position
    pub fn emitter_unit(&self) -> EeGfxEmitterUnit {
        let mut facts = EeGfxEmitterUnit::default();
        if self.unit == 0 {
            return facts;
        }
        facts.has_unit = true;
        let c = self.complex_handle();
        if c == 0 {
            facts.hit_points = self.view(self.unit, |u| u.hit_points()) as i32;
            return facts;
        }
        facts.complex = true;
        let ge = self.graphic_effect().fields();
        if ge.mFlag1 {
            let graphic = self.graphic_of(self.unit);
            let (p, a) = self.view(c, |c| (physical(&c), animation(&c)));
            let mut t = p.location;
            let list = graphic.tag_points(a, ge.mTagPointKind);
            let tag = self.record().emitter.tag_point as usize;
            let location;
            match list {
                Some(list) if ge.mTagPointKind != 0x15 => {
                    tags::scale(&mut t, p.scale);
                    if graphic.num_parts() > 0 {
                        let mut part = identity();
                        let (result, index) = graphic.find_turret_part(list.point(tag).mModel as usize);
                        facts.follow_result = result;
                        if let Some(i) = index {
                            let yaw = self.view(c, |c| turret_angle(&c, i));
                            part.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
                        }
                        location = tag_point(list, tag, p.frame, &t, Some(&part));
                    } else {
                        location = tag_point(list, tag, p.frame, &t, None);
                    }
                }
                _ => location = position(&t),
            }
            facts.location = location;
            facts.transformation = t;
        }
        let (command, hp) = self.view(c, |c| (c.command_type(), c.hit_points()));
        facts.dead = command == EE_GFX_ACTION_DEAD;
        facts.hit_points = hp as i32;
        facts
    }

    // ---- the 2D kinds' reads

    /// A random one of the unit's tag points of `kind` (the unit itself without any; `EEE2DShockwave::SetUp`,
    /// 0xe6259): the effects' random draw only when there are points
    pub fn spawn_point(&self, kind: u32) -> Ge3DPoint {
        let c = self.complex_handle();
        let graphic = self.graphic_of(c);
        let (p, a) = self.view(c, |c| (physical(&c), animation(&c)));
        let mut t = p.location;
        match graphic.tag_points(a, kind as c_long).filter(|l| l.len != 0) {
            Some(list) => {
                let n = list.len;
                let mut i = ee_base::math::um::lrintf(crate::gfx::ffi_effect::ee_gfx_effect_random() * n as f32) as i64;
                if i == n as i64 {
                    i = n as i64 - 1;
                }
                tags::scale(&mut t, p.scale);
                tag_point(list, i as usize, p.frame, &t, None)
            }
            None => position(&t),
        }
    }
    /// The unit's tag points of `kind` at its animation: -1 without a complex unit (`EEE2DImpact::PickTagPoint`)
    pub fn tag_count(&self, kind: u32) -> i32 {
        if self.unit == 0 {
            return -1;
        }
        let c = self.complex_handle();
        if c == 0 {
            return -1;
        }
        let graphic = self.graphic_of(c);
        let a = self.view(c, |c| animation(&c));
        count(graphic.tag_points(a, kind as c_long)) as i32
    }
    /// Tag point `index` of `kind` at the unit's frame in its scaled location (`EEE2DImpact::UpdatePosition`)
    pub fn tag_point(&self, kind: u32, index: u32) -> EeGfx2DPoint {
        if self.unit == 0 {
            return EeGfx2DPoint::default();
        }
        let c = self.complex_handle();
        if c == 0 {
            return EeGfx2DPoint::default();
        }
        let graphic = self.graphic_of(c);
        let (p, a) = self.view(c, |c| (physical(&c), animation(&c)));
        let mut t = p.location;
        let list = graphic.tag_points(a, kind as c_long).expect("the impact's tag points");
        tags::scale(&mut t, p.scale);
        EeGfx2DPoint { has: true, point: tag_point(list, index as usize, p.frame, &t, None) }
    }
    /// `mUnit->GetModel()->mDefaultPoint`
    pub fn default_point(&self) -> Ge3DPoint {
        self.graphic_of(self.unit).default_point()
    }
    /// The marching icons' unit (`EEE2DMarchingIcons`)
    pub fn icon_unit(&self) -> EeGfxIconUnit {
        if self.unit == 0 {
            return EeGfxIconUnit::default();
        }
        self.view(self.unit, |u| {
            let class = u.class().expect("the unit's class");
            let mut facts = EeGfxIconUnit {
                has_unit: true,
                land_building: class.land_building(),
                tile_size: world_field!(class.fields(), mTileSize) as i32,
                ..Default::default()
            };
            if let Some(owner) = u.player() {
                facts.has_player = true;
                facts.player = owner.id();
                let c = u.complex_unit().expect("the unit's complex unit");
                let m = c.transform();
                facts.x = m[0][3];
                facts.y = m[1][3];
            }
            facts
        })
    }

    /// The light glows' unit: its light tags (kind 0xd) at its animation (at 0 without a complex unit) and where it
    /// stands (its tile's centre on the terrain without one: `terrain_z`)
    fn glow_tags(
        &self,
        terrain_z: &dyn Fn(f32, f32) -> f32,
    ) -> (Option<&'static EeGfxTagList>, Ge3DPoint, Option<Physical>) {
        let graphic = self.graphic_of(self.unit);
        let c = self.complex_handle();
        if c != 0 {
            let (p, a) = self.view(c, |c| (physical(&c), animation(&c)));
            (graphic.tag_points(a, 0xd), position(&p.location), Some(p))
        } else {
            let (size, x, y) = self.view(self.unit, |u| {
                let class = u.class().expect("the unit's class");
                let (x, y) = u.tile_xy();
                (world_field!(class.fields(), mTileSize), x, y)
            });
            let px = size as f32 * 0.5 + x as f32;
            let py = size as f32 * 0.5 + y as f32;
            (graphic.tag_points(0, 0xd), Ge3DPoint { x: px, y: py, z: terrain_z(px, py) }, None)
        }
    }
    /// `EEE2DLightGlow::SetUp` (0xe2067): the light tags' count and the unit's place
    pub fn glow_set_up(&self, terrain_z: &dyn Fn(f32, f32) -> f32) -> EeGfxGlowSetUp {
        let (list, position, _) = self.glow_tags(terrain_z);
        EeGfxGlowSetUp { count: count(list) as u32, position }
    }
    /// Light tag `i`'s name (its model's tag point name)
    pub fn glow_tag_name(&self, i: u32, terrain_z: &dyn Fn(f32, f32) -> f32) -> Vec<u8> {
        let (list, _, _) = self.glow_tags(terrain_z);
        let point = list.expect("the light tags").point(i as usize);
        tags::TagModels::tag_name(&CppModels, point.mModel as usize, point.mIndex as usize)
    }
    /// `PlaceGlows`' unit (0xe266f): hidden for another player's cloaked unit or an unfinished land building; its
    /// light tags, its frame and its scaled location for `glow_point`
    pub fn glow_place(&self, terrain_z: &dyn Fn(f32, f32) -> f32) -> (EeGfxGlowPlace, GlowPlace) {
        let mut place = EeGfxGlowPlace { show: true, has_tags: false, position: Ge3DPoint::default() };
        let (list, position, p) = self.glow_tags(terrain_z);
        let (frame, mut location, scale);
        match p {
            Some(p) => {
                let c = self.complex_handle();
                let player = self.player;
                let (cloaked, other, unbuilt) = self.view(c, |c| {
                    let cloaked = if c.cloakable() { c.cloaked() } else { false };
                    let owner = c.player().map_or(0, |o| o.handle());
                    let land = c.class().expect("the unit's class").land_building();
                    (cloaked, owner != player, land && !c.is_built())
                });
                if cloaked && other {
                    place.show = false;
                }
                if unbuilt {
                    place.show = false;
                }
                scale = p.scale;
                frame = p.frame;
                location = p.location;
            }
            None => {
                scale = self.view(self.unit, |u| u.stats().expect("the unit's stats").scale());
                frame = 0;
                location = identity();
                set_position(&mut location, position);
            }
        }
        if list.is_some() {
            tags::scale(&mut location, scale);
        }
        place.has_tags = list.is_some();
        place.position = position;
        (place, GlowPlace { list, frame, location })
    }
}

/// `PlaceGlows`' kept list, frame and scaled location.
pub struct GlowPlace {
    pub list: Option<&'static EeGfxTagList>,
    pub frame: c_ulong,
    pub location: GeTransformation,
}

impl GlowPlace {
    /// Light tag `i` at the kept frame in the kept location
    pub fn point(&self, i: u32) -> Ge3DPoint {
        tag_point(self.list.expect("the light tags"), i as usize, self.frame, &self.location, None)
    }
}

/// `kActionDead`
const EE_GFX_ACTION_DEAD: u32 = 6003;

#[cfg(test)]
#[path = "effect_reads_tests.rs"]
mod tests;
