//! The effects as Rust objects (`EEEffect`, ee/EEEffect.h; the graphics + LLE cluster's second round). An effect is
//! an [`EeGfxEffectHead`] (what the C++ answers of `EEEffectHost.cpp` read in place: its record and the C++ objects it
//! holds) followed by its 2D parts and its map regions. This module owns its reference count and lifetime (the
//! original base constructor's part, the destructor, `ReleaseResources`), its `Load`, `Save` and `RepairPointers`, its
//! region on the map, its unit swaps, the base queues, the parts and one operation by its kind's class
//! (`effect_rising.rs` for the 3D kinds, `effect_2d.rs` for the 2D ones) over the C++ answers' tables, which the C++
//! registers once with the world it calls ([`EeGfxEffectWorld`]). The map's effect regions and the render queues
//! (the original's `RTSRegionTable<EEEffect*>` and `gEffectQueues`) are here too.
//!
//! Every C++ object an effect holds (its unit, graphic effect, models, graphic objects, sounds) is an opaque handle;
//! its references, loads and releases are the world's calls, in the original's order. No Rust reference to an
//! effect lives across one of them: an effect's step creates and destroys effects.

// the C longs' widths differ by platform (a cast to the same type on Windows)
#![allow(clippy::unnecessary_cast)]

use std::collections::{BTreeMap, VecDeque};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um;
use ee_sim::world::{UnitView, WorldRead, WorldView};

use crate::gfx::effect_2d::{self, EeGfxEffect2DHost};
use crate::gfx::effect_parts::{EeGfx2DPart, EeGfx2DPlace};
use crate::gfx::effect_reads::{self, EeGfxEffectRefs, Gobj, GraphicEffect};
use crate::gfx::effect_rising::{self, EeGfxEffect3DHost, EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_sprite::EeGfx2DPoint;
use crate::gfx::effect_state::{self, EeGfxEffectLoaded, EeGfxEffectPart, EeGfxEffectRecord, EffectParts};
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::ffi_effect::{EeGfxEffectFile, File};
use crate::gfx::raster::{EeGfxModel, EeGfxRaster};

/// The kinds this module tells apart (ee/EEEffectKindIDs.h).
pub(crate) mod kind {
    pub const ICON: i32 = 2;
    pub const MARCHING_ICONS_SQUARE: i32 = 0xf;
    pub const MARCHING_ICONS_CIRCULAR: i32 = 0x10;
    pub const LIGHT_GLOW: i32 = 0xd;
    pub const SHOCKWAVE: i32 = 0x12;
    pub const EARTHQUAKE: i32 = 0x1a;
    pub const WATER_RIPPLE: i32 = 0x22;
    pub const FARM: i32 = 0x29;
    pub const RAIN: i32 = 0x2a;
    pub const MARCHING_LINE: i32 = 0x2d;
    pub const PARTICLE_2D: i32 = 0x2e;
    pub const PULSE_WAVE: i32 = 0x2f;
    pub const VORTEX: i32 = 0x31;
    pub const DECAL: i32 = 0x32;
    pub const ENERGY_WALL: i32 = 0x34;
    pub const PARTICLE_3D: i32 = 0x39;
    pub const GOAL: i32 = 0x3c;
    pub const SNOW: i32 = 0x3d;
    pub const DAMAGED: i32 = 0x3e;
    pub const TRACER_BEAM: i32 = 0x40;
    pub const CONFIRMATION: i32 = 0x41;
    pub const EXPLOSION: i32 = 4;
}

/// `Step`'s and `RepairPointers`' "remove me".
const REMOVE: i32 = 7;
/// The tag of a 2D part at the effect's own position.
const OWN_POSITION: u32 = 10000;
/// The tracer beam's target from `Load` to `RepairPointers` (no target saved).
const NO_TARGET_SAVED: u64 = u64::MAX;

/// What the C++ reads of an effect in place (`EEEffect`'s members: ee/EEEffect.h mirrors this layout).
#[repr(C)]
#[derive(Debug)]
pub struct EeGfxEffectHead {
    /// the plain state (`effect_state.rs`), owned
    pub record: *mut EeGfxEffectRecord,
    /// `mUnit` (an `EEUnit*`, referenced; its saved id while `unit_is_saved_id`)
    pub unit: u64,
    /// `mGraphicEffect` (an `EEGraphicEffect*`, referenced and loaded)
    pub graphic_effect: u64,
    /// the 2D kinds' shared plane model (a `GEModel*`, referenced)
    pub model: u64,
    /// the kept graphic object (an `EEGraphicObject*`, its media loaded) and the additive pass's unit model
    pub graphic_object: u64,
    pub unit_model: u64,
    /// the animation sound (the rain's loop) and the rain's thunder (`SESMInstanceNode*`, referenced)
    pub sound: u64,
    pub rumble: u64,
    /// the tracer beam's target unit (referenced; `u64::MAX` from `Load` to `RepairPointers`)
    pub target: u64,
    /// the volcano's model (referenced by its build)
    pub volcano_model: u64,
    /// the marching line's end units (saved as their ids; none in practice)
    pub from_unit: u64,
    pub to_unit: u64,
    /// the reference count (`UReferenceCounter`'s: 1 when made)
    pub references: u32,
    /// set while the unit pointers hold `Load`'s saved ids, cleared by `RepairPointers`
    pub unit_is_saved_id: bool,
    /// a 2D kind (`effect_kinds.rs`)
    pub is_2d: bool,
}

/// One sprite of a 2D effect (`EEEffect2D::mParts`' element).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct Part {
    pub position: Ge3DPoint,
    /// its graphic object (`LoadMedia` when made, `UnloadMedia` by the effect)
    pub object: u64,
    /// the tag point's index ([`OWN_POSITION`] for the effect's own position)
    pub tag: u32,
    pub frame: u32,
    pub draw_frame: u32,
    pub time: f32,
    pub done: bool,
}

/// An effect: its head (first: an `EEEffect*` is a `*mut Effect`), its 2D parts and the marching line's regions.
#[repr(C)]
pub(crate) struct Effect {
    pub head: EeGfxEffectHead,
    pub parts: Vec<Part>,
    pub regions: Vec<(i64, i64)>,
}

/// An `EEEffect*`.
pub type EffectPtr = *mut EeGfxEffectHead;

/// The tick facts the effects read besides their graphic effect's (`gPhysics`, `gWorld`, `gFreeCamera`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectTick {
    pub tick_seconds: f32,
    pub tick_time: f32,
    /// `(unsigned long)gPhysics->mGameTime`
    pub game_time_bits: u32,
    pub free_camera: bool,
    /// `gPhysics->mSpeed`
    pub speed: f32,
    /// `gWorld.GetGameSpeed()`
    pub world_speed: f32,
    /// `gPhysics->mGameTime`, `gPhysics->GetGameClock()`
    pub game_time: i64,
    pub game_clock: f32,
    /// `gPhysics->mGameOver`, `gWorld.mWindDirection`
    pub game_over: bool,
    pub wind: Ge3DPoint,
}

/// One operation's context for the C++ answers (`EEEffectHost.cpp`'s `Ask`: the effect, the operation, the
/// constructor's argument: a unit, a point or a line).
#[repr(C)]
#[derive(Debug)]
pub struct EeGfxEffectAsk {
    pub effect: EffectPtr,
    pub op: EeGfxEffect3DOp,
    pub arg: *mut c_void,
}

/// The C++ world the effects call into, registered once (`EEEffectHost.cpp`).
#[repr(C)]
pub struct EeGfxEffectWorld {
    /// `EEUnit::Reference` / `Dereference`
    pub unit_reference: extern "C" fn(unit: u64),
    pub unit_release: extern "C" fn(unit: u64),
    /// `gWorld.GetUnit(id)` (an `EEUnit*`; 0: none)
    pub unit_by_id: extern "C" fn(id: u32) -> u64,
    /// a graphic effect's `Reference`, `Dereference`, `Load(on_demand)`, `Unload` and `GetMedia` (the graphic object,
    /// or a random one of its kind: the simulation's random stream)
    pub graphic_effect_reference: extern "C" fn(effect: u64),
    pub graphic_effect_release: extern "C" fn(effect: u64),
    pub graphic_effect_load: extern "C" fn(effect: u64, on_demand: bool),
    pub graphic_effect_unload: extern "C" fn(effect: u64),
    pub graphic_effect_media: extern "C" fn(effect: u64) -> u64,
    /// `EEGraphicEffect::Get(index)` and `GetCount()`
    pub graphic_effect_by_id: extern "C" fn(id: u32) -> u64,
    pub graphic_effect_count: extern "C" fn() -> u32,
    /// a `GEModel`'s `Reference` / `Dereference`
    pub model_reference: extern "C" fn(model: u64),
    pub model_release: extern "C" fn(model: u64),
    /// an `EEGraphicObject`'s `LoadMedia` (its result) / `UnloadMedia`
    pub media_load: extern "C" fn(object: u64) -> i32,
    pub media_unload: extern "C" fn(object: u64),
    /// a sound node stopped (`gSoundManager->Stop(node, false)`) and dereferenced
    pub sound_release: extern "C" fn(node: u64),
    /// `EESoundObject::Get(id)`'s `UnloadMedia` / `LoadMedia` (none: nothing)
    pub sound_object_unload: extern "C" fn(id: u32),
    pub sound_object_load: extern "C" fn(id: u32),
    /// a hit on the unit: its class's `UnitDamaged(complex unit, NULL, 0)`
    pub unit_damaged: extern "C" fn(unit: u64),
    /// `gLoadingSavedGame`
    pub loading_saved_game: extern "C" fn() -> bool,
    /// the map's effect regions (`gWorld.mMap->mEffectRegions`; null without a map)
    pub regions: extern "C" fn() -> *mut EeGfxEffectRegions,
    /// the map tile at (x, y): -1 none, else whether the current player sees it (`IsSeenBy`)
    pub tile_seen: extern "C" fn(x: i64, y: i64) -> i32,
    /// `gDoppelgangerManager.HasDoppelganger(the tile at (x, y), unit or its complex unit, the current player)`
    pub doppelganger: extern "C" fn(x: i64, y: i64, unit: u64, complex: bool) -> bool,
    /// the frustum test of a sphere (`ee_gfx_effect_in_view` over `gViewport`, `gOrthographicView`)
    pub in_view: extern "C" fn(point: *const Ge3DPoint, radius: f32) -> bool,
    /// the tick's facts
    pub tick: extern "C" fn() -> EeGfxEffectTick,
    /// `gScreenIndex` (false) or the preferences' screen (true)
    pub screen: extern "C" fn(preferences: bool) -> u32,
    /// a shooter's weapon point (`EECombatEffectWeaponPoint` at `site`, with its CBEW capture: with the tag points
    /// at its animation when `test_list` finds them, else assumed) and whether its model has such points
    pub weapon_point: extern "C" fn(unit: u64, site: i32, test_list: bool, tagged: *mut bool) -> Ge3DPoint,
    /// the map's height at a point (`gWorld.mMap->GetHeight`)
    pub height: extern "C" fn(point: *const Ge3DPoint) -> f32,
    /// the map's terrain mesh (`gWorld.mMap->mTerrainMesh`'s Rust object)
    pub mesh: extern "C" fn() -> *const crate::ge::tmobj::EeGeTerrainMesh,
    /// the raster table (`gEERustRaster`)
    pub raster: *const EeGfxRaster,
    /// a unit's graphic (`EEUnit::GetModel`, a virtual: a foundation shows its player's class graphic)
    pub unit_graphic: extern "C" fn(context: *mut c_void, unit: u64) -> u64,
    /// the weather storms' map
    pub storm: crate::gfx::ffi_effect::EeGfxStormHost,
    /// the 3D and the 2D kinds' C++ answers (their context: the call's [`EeGfxEffectAsk`])
    pub answers3d: EeGfxEffectAnswers3D,
    pub answers2d: EeGfxEffectAnswers2D,
}

/// The 3D kinds' answers the C++ gives (`EeGfxEffect3DHost`'s entries of the same names).
#[repr(C)]
pub struct EeGfxEffectAnswers3D {
    pub animation_sound: extern "C" fn(context: *mut c_void, frame: u32, changed: bool),
    pub wall_gate_posts: extern "C" fn(context: *mut c_void, first: *mut Ge3DPoint, second: *mut Ge3DPoint) -> bool,
    pub wall_piece: extern "C" fn(context: *mut c_void, x: i32, y: i32, z: *mut f32) -> bool,
    pub volcano_build:
        extern "C" fn(context: *mut c_void, points: *mut Ge3DPoint, heights: *mut f32, count: u32) -> i32,
    pub farm_terrain: extern "C" fn(context: *mut c_void) -> crate::gfx::ffi_mesh::EeGfxFarmTerrain,
    pub farm_drop_site: extern "C" fn(context: *mut c_void, x: *mut i32, y: *mut i32) -> bool,
    pub farm_set_variant: extern "C" fn(context: *mut c_void, variant: i32),
    pub farm_doppelganger_any: extern "C" fn(context: *mut c_void) -> bool,
    pub farm_doppelganger: extern "C" fn(context: *mut c_void) -> i32,
    pub farm_opacity: extern "C" fn(context: *mut c_void, opacity: f32) -> f32,
    pub set_scaled_model: extern "C" fn(context: *mut c_void, model: EeGfxModel),
    pub ground_point: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> Ge3DPoint,
    pub rain_sounds: extern "C" fn(context: *mut c_void, start: bool, rumble: i32),
    pub emitter_sound: extern "C" fn(context: *mut c_void, location: *const Ge3DPoint, first: bool),
}

/// The 2D kinds' answers the C++ gives (`EeGfxEffect2DHost`'s entries of the same names).
#[repr(C)]
pub struct EeGfxEffectAnswers2D {
    pub confirm_goal: extern "C" fn(
        context: *mut c_void,
        goal_index: u32,
        tile_x: i32,
        tile_y: i32,
    ) -> crate::gfx::effect_confirm::EeGfxConfirmGoal,
    pub animation_sound:
        extern "C" fn(context: *mut c_void, location: Ge3DPoint, frame: u32, changed: bool, part0: bool),
    pub draw_blood: extern "C" fn(context: *mut c_void) -> bool,
    pub tile_blocked: extern "C" fn(context: *mut c_void, x: i32, y: i32) -> bool,
    pub area_effect: extern "C" fn(context: *mut c_void, kind: i32, x: f32, y: f32, player: u32) -> bool,
    pub area_effects: extern "C" fn(context: *mut c_void) -> i32,
    pub set_area_effects: extern "C" fn(context: *mut c_void, count: i32),
    pub icons_selected: extern "C" fn(context: *mut c_void) -> bool,
    pub ground_point: extern "C" fn(context: *mut c_void, at: Ge3DPoint) -> EeGfx2DPoint,
    pub glow_area: extern "C" fn(context: *mut c_void) -> crate::gfx::effect_glow::EeGfxGlowArea,
    pub glow_rows: extern "C" fn(context: *mut c_void) -> u32,
}

static WORLD: AtomicPtr<EeGfxEffectWorld> = AtomicPtr::new(std::ptr::null_mut());

/// Registers the world (once; later registrations replace it: the tests').
///
/// # Safety
/// `world` lives for the rest of the program.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_world_set(world: *const EeGfxEffectWorld) {
    WORLD.store(world as *mut EeGfxEffectWorld, Ordering::Release);
}

pub(crate) fn world() -> &'static EeGfxEffectWorld {
    let w = WORLD.load(Ordering::Acquire);
    assert!(!w.is_null(), "ee_gfx: the effects' world is registered (EEEffectHost.cpp)");
    // SAFETY: a registered world lives for the rest of the program
    unsafe { &*w }
}

// ---- the object

fn obj<'a>(e: EffectPtr) -> &'a mut Effect {
    // SAFETY: an effect pointer is a live `Effect` (the head is its first field); the callers hold no other
    // reference to it across this one's use
    unsafe { &mut *(e as *mut Effect) }
}

fn rec<'a>(e: EffectPtr) -> &'a mut EeGfxEffectRecord {
    // SAFETY: a live effect's record; as `obj`
    unsafe { &mut *(*e).record }
}

fn head<'a>(e: EffectPtr) -> &'a mut EeGfxEffectHead {
    // SAFETY: as `obj`
    unsafe { &mut *e }
}

/// A new effect (zeroed: the port's `operator new`), its record a new one's (`Init`'s values, kind 0), one
/// reference.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_new() -> EffectPtr {
    let e = Box::new(Effect {
        head: EeGfxEffectHead {
            record: Box::into_raw(Box::new(EeGfxEffectRecord::new(0))),
            unit: 0,
            graphic_effect: 0,
            model: 0,
            graphic_object: 0,
            unit_model: 0,
            sound: 0,
            rumble: 0,
            target: 0,
            volcano_model: 0,
            from_unit: 0,
            to_unit: 0,
            references: 1,
            unit_is_saved_id: false,
            is_2d: false,
        },
        parts: Vec::new(),
        regions: Vec::new(),
    });
    Box::into_raw(e) as EffectPtr
}

/// The kind's class: a 2D kind (`EEEffect2D`'s constructor) references its shared plane model, the ground plane for
/// the shockwave's, confirmation's, pulse wave's, ripple's, vortex's and decal's, else the standing one; the 2D
/// particle's motion, age, look and timing cleared (`EEE2DParticle::Clear`).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_begin(e: EffectPtr, kind: i32) {
    rec(e).kind = kind;
    let is_2d = crate::gfx::effect_kinds::ee_gfx_effect_is_2d(kind);
    head(e).is_2d = is_2d;
    if !is_2d {
        return;
    }
    let ground = matches!(
        kind,
        kind::SHOCKWAVE | kind::CONFIRMATION | kind::PULSE_WAVE | kind::WATER_RIPPLE | kind::VORTEX | kind::DECAL
    );
    let model = crate::gfx::effect_batch::plane_model(!ground) as u64;
    head(e).model = model;
    if model != 0 {
        (world().model_reference)(model);
    }
    if kind == kind::PARTICLE_2D {
        effect_state::particle_clear(rec(e));
    }
}

/// The original base constructor's part (`EEEffect` 0x10258e: `Init`, both pointers referenced, the kind, the
/// keeps-unit flag, the media load on demand unless a save is loading), then the kind's class.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_attach(e: EffectPtr, unit: u64, graphic_effect: u64, kind: i32, keeps_unit: bool) {
    let w = world();
    rec(e).init();
    head(e).unit = unit;
    if unit != 0 {
        (w.unit_reference)(unit);
    }
    head(e).graphic_effect = graphic_effect;
    if graphic_effect != 0 {
        (w.graphic_effect_reference)(graphic_effect);
    }
    rec(e).kind = kind;
    rec(e).keeps_unit = keeps_unit;
    if graphic_effect != 0 {
        (w.graphic_effect_load)(graphic_effect, !(w.loading_saved_game)());
    }
    ee_gfx_effect_begin(e, kind);
}

/// `UReferenceCounter::Reference`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_reference(e: EffectPtr) {
    head(e).references += 1;
}

/// `EEEffect::Release` (`Dereference`): the effect destroyed on its last reference.
///
/// # Safety
/// `e` is a live effect; it is not used after its last reference.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_release(e: EffectPtr) {
    head(e).references -= 1;
    if head(e).references == 0 {
        // SAFETY: the last reference
        unsafe { destroy(e) };
    }
}

/// The destructor: the leaves' parts (the marching line's regions, the sounds, the rain's sound media, the storms',
/// the farm's and the earthquake's models, the kept graphic objects, the tracer beam's target, the marching line's
/// ends), the 2D parts and plane model (`EEEffect2D` 0x1042cf), then the base's.
unsafe fn destroy(e: EffectPtr) {
    let w = world();
    let k = rec(e).kind;
    if k == kind::MARCHING_LINE {
        remove_regions(e);
    }
    for s in [&mut head(e).sound, &mut head(e).rumble] {
        if *s != 0 {
            (w.sound_release)(*s);
            *s = 0;
        }
    }
    if k == kind::RAIN {
        for i in 0..3 {
            (w.sound_object_unload)(rec(e).storm.sounds[i]);
        }
    }
    let mut models = [0u64; 5];
    if k == kind::RAIN || k == kind::SNOW {
        models[0] = rec(e).storm.model as u64;
    }
    if k == kind::FARM {
        models[1] = rec(e).farm.ground as u64;
        models[2] = rec(e).farm.ground_copy as u64;
        models[3] = rec(e).farm.crops_copy as u64;
    }
    if k == kind::EARTHQUAKE {
        models[4] = rec(e).quake.model as u64;
    }
    for i in 0..2 {
        if models[0] != 0 && i == 0 {
            (w.model_release)(models[0]);
        }
        let object = if i == 0 { &mut head(e).graphic_object } else { &mut head(e).unit_model };
        if *object != 0 {
            (w.media_unload)(*object);
            *object = 0;
        }
    }
    let h = head(e);
    if h.target != 0 && h.target != NO_TARGET_SAVED {
        (w.unit_release)(h.target);
    }
    // the marching line's units (not referenced by a repair: as the original); still saved ids after a load that
    // failed before the repair: no units
    if h.from_unit != 0 && !h.unit_is_saved_id {
        (w.unit_release)(h.from_unit);
    }
    if h.to_unit != 0 && !h.unit_is_saved_id {
        (w.unit_release)(h.to_unit);
    }
    if h.volcano_model != 0 {
        (w.model_release)(h.volcano_model);
    }
    for &m in &models[1..] {
        if m != 0 {
            (w.model_release)(m);
        }
    }
    let parts: Vec<u64> = obj(e).parts.iter().map(|p| p.object).collect();
    for object in parts {
        (w.media_unload)(object);
    }
    if head(e).model != 0 {
        (w.model_release)(head(e).model);
        head(e).model = 0;
    }
    release_base(e);
    // SAFETY: the effect's own record and box, not used again
    unsafe {
        crate::gfx::ffi_effect::ee_gfx_effect_record_free((*e).record);
        (*e).record = std::ptr::null_mut();
        drop(Box::from_raw(e as *mut Effect));
    }
}

/// The base's release (`EEEffect`'s destructor 0x102600 and `ReleaseResources` 0x102e61): the graphic effect unloaded
/// and released, the map region left, the unit released (not a failed load's saved id).
fn release_base(e: EffectPtr) {
    let w = world();
    let g = head(e).graphic_effect;
    if g != 0 {
        (w.graphic_effect_unload)(g);
        (w.graphic_effect_release)(g);
        head(e).graphic_effect = 0;
    }
    let (x, y) = (rec(e).tile_x, rec(e).tile_y);
    if x != -1 && y != -1 {
        regions_remove(x as i64, y as i64, e);
    }
    let h = head(e);
    if h.unit != 0 && !h.unit_is_saved_id {
        (w.unit_release)(h.unit);
    }
    h.unit = 0;
    h.unit_is_saved_id = false;
}

/// `ReleaseResources`: what the destructor releases without destroying the effect (the particles' graphic object,
/// the marching line's regions, the 2D parts and plane model, the base's).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_release_resources(e: EffectPtr) {
    let w = world();
    let k = rec(e).kind;
    if (k == kind::PARTICLE_3D || k == kind::PARTICLE_2D) && head(e).graphic_object != 0 {
        (w.media_unload)(head(e).graphic_object);
        head(e).graphic_object = 0;
    }
    if k == kind::MARCHING_LINE {
        remove_regions(e);
    }
    if head(e).is_2d {
        let parts: Vec<u64> = obj(e).parts.iter().map(|p| p.object).collect();
        for object in parts {
            (w.media_unload)(object);
        }
        obj(e).parts.clear();
        if head(e).model != 0 {
            (w.model_release)(head(e).model);
            head(e).model = 0;
        }
    }
    release_base(e);
    rec(e).tile_x = -1;
    rec(e).tile_y = -1;
}

/// `GetGraphicObject`: the graphic effect's object (`GetMedia`: a random one of its kind without one; 0 without a
/// graphic effect).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_graphic_object(e: EffectPtr) -> u64 {
    let g = head(e).graphic_effect;
    if g == 0 {
        0
    } else {
        (world().graphic_effect_media)(g)
    }
}

/// `GetUnitLocation`: its complex unit's position, else its tile centre at its tile's height, else the origin.
pub(crate) fn unit_location(e: EffectPtr) -> Ge3DPoint {
    let unit = head(e).unit;
    if unit == 0 || head(e).unit_is_saved_id {
        return Ge3DPoint::default();
    }
    // SAFETY: the effect's unit is live while referenced; the world is held by the effects' callers
    unsafe {
        let t = WorldRead::begin();
        let u = UnitView::of(&t, unit).expect("the effect's unit");
        if let Some(c) = u.complex_unit() {
            let l = c.transform();
            return Ge3DPoint { x: l[0][3], y: l[1][3], z: l[2][3] };
        }
        let (x, y) = u.tile_xy();
        Ge3DPoint { x: x as f32 + 0.5, y: y as f32 + 0.5, z: u.tile().map_or(0.0, |t| t.height()) }
    }
}

fn current_player() -> (u64, i64) {
    // SAFETY: the world is held by the effects' callers
    unsafe {
        let t = WorldRead::begin();
        // (no world: the unit tests')
        WorldView::global(&t)
            .map_or((0, -1), |w| (w.current_player().map_or(0, |p| p.handle()), w.current_player_index()))
    }
}

/// The 3D kinds' tile test (`EEEffect3D` 0x105429: the current player sees the tile at (x, y)); the farm's
/// (`EEE3DFarm::IsTileVisible`: any tile of its field seen, else the current player's doppelganger of it there) and
/// the energy wall's (`EEE3DEnergyWall::IsTileVisible`: else a doppelganger, which marks the wall seen).
fn tile_visible(e: EffectPtr, x: i64, y: i64) -> bool {
    let w = world();
    let k = rec(e).kind;
    if k == kind::FARM {
        let mut seen = false;
        let unit = head(e).unit;
        // SAFETY: the effect's unit is live while referenced
        let (alive, size, (tx, ty)) = unsafe {
            let t = WorldRead::begin();
            let u = UnitView::of(&t, unit).expect("the farm's unit");
            let size: std::os::raw::c_long =
                ee_sim::world_field!(u.class().expect("a unit's class").fields(), mTileSize);
            (u.hit_points() > 0, size as u64, u.tile_xy())
        };
        if alive {
            'tiles: for dy in 0..size {
                for dx in 0..size {
                    if (w.tile_seen)(tx + dx as i64, ty + dy as i64) == 1 {
                        seen = true;
                        break 'tiles;
                    }
                }
            }
        }
        if !seen {
            seen = (w.doppelganger)(x, y, unit, true);
        }
        return seen;
    }
    let seen = (w.tile_seen)(x, y) == 1;
    if seen || k != kind::ENERGY_WALL {
        return seen;
    }
    let held = (w.doppelganger)(x, y, head(e).unit, false);
    if held {
        rec(e).wall.seen = true;
    }
    held
}

/// The base queues: 10 (skip) for an effect shown only to another player; with `in_view` also off the map, on a tile
/// the current player does not see (the 3D kinds' tile test) unless the camera is free, or with its bounding sphere
/// out of view (`EEEffect2D::Queue` at its position, `EEEffect3D::Queue` at its translation); else 0.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_base_queue(e: EffectPtr, in_view: bool) -> i32 {
    let w = world();
    let r = rec(e);
    if r.local_only && i64::from(r.player) != current_player().1 {
        return 10;
    }
    if !in_view {
        return 0;
    }
    let at = if head(e).is_2d {
        r.position
    } else {
        Ge3DPoint { x: r.transformation.x(), y: r.transformation.y(), z: r.transformation.z() }
    };
    let (x, y) = (um::floor(at.x) as i64, um::floor(at.y) as i64);
    let seen = (w.tile_seen)(x, y);
    if seen < 0 {
        return 10;
    }
    let seen = if head(e).is_2d { seen == 1 } else { tile_visible(e, x, y) };
    if !seen && !(w.tick)().free_camera {
        return 10;
    }
    let radius = rec(e).bounding_radius;
    if !(w.in_view)(&at, radius) {
        return 10;
    }
    0
}

// ---- the map's effect regions

/// The effects per map region (the original's `EEEffectsTable : RTSRegionTable<EEEffect*>`: a square grid of
/// regions of `size` tiles, a vector of effects each).
pub struct EeGfxEffectRegions {
    size: i32,
    count: u64,
    cells: Vec<Vec<EffectPtr>>,
}

impl EeGfxEffectRegions {
    fn region(&mut self, x: i64, y: i64) -> Option<&mut Vec<EffectPtr>> {
        // GetRegion(unsigned long, unsigned long): a negative coordinate is past the grid
        if (x as u64) < self.count && (y as u64) < self.count {
            Some(&mut self.cells[(x as u64 * self.count + y as u64) as usize])
        } else {
            None
        }
    }
}

/// The map's effect regions: `count` regions per side of `size` tiles (`EEMap`'s `new EEEffectsTable(this, 10)`).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_regions_new(count: u32, size: i32) -> *mut EeGfxEffectRegions {
    let n = u64::from(count);
    Box::into_raw(Box::new(EeGfxEffectRegions { size, count: n, cells: vec![Vec::new(); (n * n) as usize] }))
}

/// Frees the regions (null: nothing).
///
/// # Safety
/// `r` is null or from `ee_gfx_effect_regions_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_regions_free(r: *mut EeGfxEffectRegions) {
    if !r.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(r) });
    }
}

fn with_regions<R>(f: impl FnOnce(&mut EeGfxEffectRegions) -> R) -> Option<R> {
    let r = (world().regions)();
    // SAFETY: the map's live table; no other reference to it lives during `f` (no callback)
    unsafe { r.as_mut() }.map(f)
}

fn regions_remove(x: i64, y: i64, e: EffectPtr) {
    with_regions(|t| {
        if let Some(list) = t.region(x, y) {
            if let Some(i) = list.iter().position(|&o| o == e) {
                list.remove(i);
            }
        }
    });
}

fn regions_add(x: i64, y: i64, e: EffectPtr) {
    with_regions(|t| {
        if let Some(list) = t.region(x, y) {
            list.push(e);
        }
    });
}

/// `MoveToRegion`: to the region under (x, y) when it changed (the old region's list left, the new one's joined).
fn move_to_region(e: EffectPtr, x: f32, y: f32) {
    let Some(size) = with_regions(|t| t.size) else { return };
    let (rx, ry) = EeGfxEffectRecord::region_of(x, y, size);
    if rx == rec(e).tile_x && ry == rec(e).tile_y {
        return;
    }
    regions_remove(rec(e).tile_x as i64, rec(e).tile_y as i64, e);
    rec(e).tile_x = rx;
    rec(e).tile_y = ry;
    regions_add(rx as i64, ry as i64, e);
}

/// `U2DPoint::GetLineTiles(to, step)`: the tiles a line from `from` to `to` crosses, `to` last.
pub(crate) fn line_tiles(from: (i64, i64), to: (i64, i64), step: f32) -> Vec<(i64, i64)> {
    let mut tiles = Vec::new();
    if from != to {
        let mut d = GeVector { x: to.0 as f32 - from.0 as f32, y: to.1 as f32 - from.1 as f32, z: 0.0 };
        d.normalize();
        d.x *= step;
        d.y *= step;
        let line = Ge3DLine { start: Ge3DPoint { x: from.0 as f32, y: from.1 as f32, z: 0.0 }, direction: d };
        let length = line.solve_for_x(to.0 as f32).or_else(|| line.solve_for_y(to.1 as f32)).unwrap_or(0.0);
        let mut distance = 0.0f32;
        let mut last = (0i64, 0i64);
        while distance < length {
            let p = line.point_at(distance);
            let tile = (p.x as i64, p.y as i64);
            if last != tile || tiles.is_empty() {
                tiles.push(tile);
            }
            distance += step;
            last = tile;
        }
    }
    tiles.push(to);
    tiles
}

/// The marching line's regions along it from `from` to `to` but its ends' and its own (`AddRegions`).
fn add_regions(e: EffectPtr, from: Ge3DPoint, to: Ge3DPoint) {
    let Some(size) = with_regions(|t| t.size as i64) else { return };
    let start = (um::floor(from.x) as i64 / size, um::floor(from.y) as i64 / size);
    let end = (um::floor(to.x) as i64 / size, um::floor(to.y) as i64 / size);
    let own = (rec(e).tile_x as i64, rec(e).tile_y as i64);
    let tiles = line_tiles(start, end, 1.0);
    for &tile in tiles.iter().rev() {
        if tile == start || tile == end || tile == own || obj(e).regions.contains(&tile) {
            continue;
        }
        obj(e).regions.push(tile);
        regions_add(tile.0, tile.1, e);
    }
}

/// The marching line's regions left (`RemoveRegions`).
fn remove_regions(e: EffectPtr) {
    let regions = std::mem::take(&mut obj(e).regions);
    for &(x, y) in regions.iter().rev() {
        regions_remove(x, y, e);
    }
}

/// `MoveToPoint`: to the region under the point (the marching line: its regions along it again).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_move_to_point(e: EffectPtr, at: Ge3DPoint) {
    let march = rec(e).kind == kind::MARCHING_LINE;
    if march {
        remove_regions(e);
    }
    move_to_region(e, at.x, at.y);
    if march {
        let to = rec(e).march.to;
        add_regions(e, at, to);
    }
}

/// `SetPosition` (the 2D kinds'): the position, then its region.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_set_position(e: EffectPtr, at: Ge3DPoint) {
    rec(e).position = at;
    ee_gfx_effect_move_to_point(e, at);
}

/// `SetTransformation` (the 3D kinds'): the transformation, then the region under its translation.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_set_transformation(e: EffectPtr, t: GeTransformation) {
    rec(e).transformation = t;
    ee_gfx_effect_move_to_point(e, Ge3DPoint { x: t.x(), y: t.y(), z: t.z() });
}

/// `SetFrom` / `SetTo` (the marching line's ends, set unless the end follows a unit): whether it was set.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_set_end(e: EffectPtr, to: bool, at: Ge3DPoint) -> bool {
    let unit = if to { head(e).to_unit } else { head(e).from_unit };
    if unit != 0 {
        return false;
    }
    if to {
        rec(e).march.to = at;
    } else {
        rec(e).march.from = at;
    }
    true
}

/// `SetLocation` (the icon's and goal's): a fixed one moves to `at`; others ignore it.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_set_location(e: EffectPtr, at: Ge3DPoint) {
    let k = rec(e).kind;
    if (k == kind::ICON || k == kind::GOAL) && !rec(e).icon.follow_unit {
        rec(e).icon.location = at;
    }
}

/// `ReplaceUnit`: the effect on `old` moves to `new` unless it keeps its unit; the marching line's ends and the
/// tracer beam's target (unless it keeps its unit) too; the farm's is empty.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_replace_unit(e: EffectPtr, old: u64, new: u64) {
    let w = world();
    let k = rec(e).kind;
    if k == kind::FARM {
        return;
    }
    let keeps = rec(e).keeps_unit;
    let h = head(e);
    if h.unit == old && !keeps {
        (w.unit_release)(h.unit);
        h.unit = new;
        (w.unit_reference)(h.unit);
    }
    for i in 0..3 {
        let moves = if i < 2 { k == kind::MARCHING_LINE } else { k == kind::TRACER_BEAM && !keeps };
        let end = match i {
            0 => &mut h.from_unit,
            1 => &mut h.to_unit,
            _ => &mut h.target,
        };
        if moves && old == *end {
            (w.unit_release)(*end);
            *end = new;
            (w.unit_reference)(new);
        }
    }
}

/// The farm's `ElevationChanged`: rebuilt on its unit when the changed rectangle overlaps its field.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_elevation_changed(e: EffectPtr, left: i64, bottom: i64, right: i64, top: i64) {
    if rec(e).kind != kind::FARM {
        return;
    }
    // GERectangle(inLeft, inTop, inRight, inBottom) from longs: floats
    let (l, t, r, b) = (left as f32, top as f32, right as f32, bottom as f32);
    let a = rec(e).farm.area;
    if !(l <= a.right) || !(r >= a.left) || !(t <= a.bottom) || !(b >= a.top) {
        return;
    }
    ee_gfx_effect_call(e, EeGfxEffect3DOp::Rebuild, std::ptr::null_mut());
}

// ---- Load, Save, RepairPointers

/// The 2D parts as `effect_state.rs`' Load and Save use them: a loaded part takes the effect's graphic object and is
/// kept when its media loads.
struct LoadParts(EffectPtr);

impl EffectParts for LoadParts {
    fn push(&mut self, p: &EeGfxEffectPart) -> i32 {
        let object = ee_gfx_effect_graphic_object(self.0);
        let res = (world().media_load)(object);
        if res == 0 {
            obj(self.0).parts.push(Part {
                position: p.position,
                object,
                tag: p.tag,
                frame: p.frame,
                draw_frame: 0,
                time: p.time,
                done: p.done,
            });
        }
        res
    }
    fn count(&mut self) -> u32 {
        obj(self.0).parts.len() as u32
    }
    fn get(&mut self, i: u32) -> EeGfxEffectPart {
        let p = obj(self.0).parts[i as usize];
        EeGfxEffectPart { position: p.position, tag: p.tag, frame: p.frame, time: p.time, done: p.done }
    }
}

/// Runs `f` on a copy of the record and writes the copy back (no Rust reference to the record lives across the
/// world's calls).
fn on_copy<T>(e: EffectPtr, f: impl FnOnce(&mut EeGfxEffectRecord) -> T) -> T {
    // SAFETY: the effect's live record
    let mut local = unsafe { (*e).record.read() };
    let res = f(&mut local);
    // SAFETY: as above
    unsafe { (*e).record.write(local) };
    res
}

/// A copy of the record for a read (the C++'s `r.read()`: the record holds no destructor).
fn copy(e: EffectPtr) -> EeGfxEffectRecord {
    // SAFETY: the effect's live record
    unsafe { (*e).record.read() }
}

/// `Load`: the base's state (the graphic effect, whose media loads, the region, the saved unit id, the handle, the
/// keeps-unit flag; its result ignored by every class), then the 3D kinds' transformation and radius or the 2D kinds'
/// position, radius and parts, then the kind's own; the command confirmation is never saved, the explosion has no
/// state of its own, the damage controller its children's handles; the first error.
///
/// # Safety
/// `e` is a live effect; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load(e: EffectPtr, file: *const EeGfxEffectFile, major: u32, minor: u32) -> i32 {
    let w = world();
    let k = rec(e).kind;
    if k == kind::CONFIRMATION {
        return ee_gfx_effect_call(e, EeGfxEffect3DOp::Load, std::ptr::null_mut());
    }
    // SAFETY: the function's contract
    let f = unsafe { &*file };
    let mut loaded = EeGfxEffectLoaded::default();
    on_copy(e, |r| effect_state::load_base(r, &mut File(f), &mut loaded));
    if loaded.has_graphic_effect {
        let g = (w.graphic_effect_by_id)(loaded.graphic_effect_id);
        head(e).graphic_effect = g;
        (w.graphic_effect_reference)(g);
        (w.graphic_effect_load)(g, false);
    }
    if loaded.has_unit {
        head(e).unit = u64::from(loaded.unit_id);
    }
    head(e).unit_is_saved_id = true;
    if !head(e).is_2d {
        let base = on_copy(e, |r| effect_state::load_3d(r, &mut File(f)));
        if k == kind::TRACER_BEAM {
            head(e).target = NO_TARGET_SAVED;
        }
        return file_op(e, EeGfxEffect3DOp::Load, f, base, major, minor);
    }
    let mut parts = LoadParts(e);
    let base = on_copy(e, |r| effect_state::load_2d(r, &mut File(f), &mut parts));
    if k == kind::DAMAGED {
        // SAFETY: the effect's live record
        return unsafe { crate::gfx::ffi_effect::ee_gfx_effect_load_damaged((*e).record, file, major, minor) };
    }
    if k == kind::EXPLOSION {
        return base;
    }
    file_op(e, EeGfxEffect3DOp::Load, f, 0, major, minor)
}

/// `Save`: the base's state (the graphic effect's id, the region, the unit's id or -1, the handle, the keeps-unit
/// flag; its result ignored), then the 3D kinds' or the 2D kinds' (the particle where it was drawn), then the kind's
/// own (the marching icons stop at the 2D part's error); the command confirmation is never saved.
///
/// # Safety
/// `e` is a live effect; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save(e: EffectPtr, file: *const EeGfxEffectFile, major: u32, minor: u32) -> i32 {
    let k = rec(e).kind;
    if k == kind::CONFIRMATION {
        return ee_gfx_effect_call(e, EeGfxEffect3DOp::Save, std::ptr::null_mut());
    }
    let drawn = rec(e).particle.draw_position;
    if k == kind::PARTICLE_2D && rec(e).position != drawn {
        ee_gfx_effect_set_position(e, drawn);
    }
    // SAFETY: the function's contract
    let f = unsafe { &*file };
    let g = GraphicEffect(head(e).graphic_effect as usize);
    let unit_id = if head(e).unit != 0 {
        // SAFETY: the effect's unit is live while referenced
        unsafe {
            let t = WorldRead::begin();
            UnitView::of(&t, head(e).unit).expect("the effect's unit").id()
        }
    } else {
        0xffff_ffff
    };
    effect_state::save_base(&copy(e), &mut File(f), g.fields().mID as i32, unit_id);
    if !head(e).is_2d {
        let base = effect_state::save_3d(&copy(e), &mut File(f));
        return file_op(e, EeGfxEffect3DOp::Save, f, base, major, minor);
    }
    let mut parts = LoadParts(e);
    let base = effect_state::save_2d(&copy(e), &mut File(f), &mut parts);
    if k == kind::DAMAGED {
        // SAFETY: the effect's live record
        return unsafe { crate::gfx::ffi_effect::ee_gfx_effect_save_damaged((*e).record, file, g.linked(1) != 0) };
    }
    if k == kind::EXPLOSION || ((k == kind::MARCHING_ICONS_SQUARE || k == kind::MARCHING_ICONS_CIRCULAR) && base != 0) {
        return base;
    }
    file_op(e, EeGfxEffect3DOp::Save, f, 0, major, minor)
}

/// `RepairPointers`: the saved unit id back to the unit (7 when it is gone) and the map region joined (not the
/// command confirmation's), then the tracer beam's target, the marching line's ends, the kind's own, or the light
/// glow's set-up again.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_repair(e: EffectPtr) -> i32 {
    let w = world();
    let k = rec(e).kind;
    if k == kind::CONFIRMATION {
        return ee_gfx_effect_call(e, EeGfxEffect3DOp::Repair, std::ptr::null_mut());
    }
    let mut res = 0;
    head(e).unit_is_saved_id = false;
    let id = head(e).unit as u32;
    if id == 0xffff_ffff {
        head(e).unit = 0;
    } else {
        let u = (w.unit_by_id)(id);
        head(e).unit = u;
        if u == 0 {
            res = REMOVE;
        } else {
            (w.unit_reference)(u);
        }
    }
    regions_add(rec(e).tile_x as i64, rec(e).tile_y as i64, e);
    if head(e).is_2d {
        return if k == kind::LIGHT_GLOW && res == 0 {
            ee_gfx_effect_call(e, EeGfxEffect3DOp::SetUp, std::ptr::null_mut())
        } else {
            res
        };
    }
    if k == kind::TRACER_BEAM {
        let h = head(e);
        if res == 0 && h.target != NO_TARGET_SAVED {
            h.target = (w.unit_by_id)(h.target as u32);
            if h.target == 0 {
                res = REMOVE;
            } else {
                (w.unit_reference)(h.target);
            }
        } else {
            h.target = 0;
        }
        return res;
    }
    if k == kind::MARCHING_LINE {
        let h = head(e);
        if res == 0 {
            if h.from_unit as u32 > 0 {
                h.from_unit = (w.unit_by_id)(h.from_unit as u32);
            }
            if h.to_unit as u32 > 0 {
                h.to_unit = (w.unit_by_id)(h.to_unit as u32);
            }
        } else {
            // port: a failed repair drops the effect; its ends are ids, not units
            h.from_unit = 0;
            h.to_unit = 0;
        }
    }
    if res != 0 {
        res
    } else {
        ee_gfx_effect_call(e, EeGfxEffect3DOp::Repair, std::ptr::null_mut())
    }
}

// ---- the 2D parts

/// `CreateParts`: one sprite part per tag point of the effect's kind on the unit's model (else one at the unit, tag
/// 10000), a buried position lifted by `lift`; the position and the height (the farthest part plus the effect's size).
fn create_parts(e: EffectPtr, lift: f32) -> EeGfx2DPlace {
    let w = world();
    let mut position = unit_location(e);
    let mut height = 1.0f32;
    let unit = head(e).unit;
    if unit != 0 {
        let g = GraphicEffect(head(e).graphic_effect as usize);
        let size = g.fields().mSize;
        let model = Gobj((w.unit_graphic)(std::ptr::null_mut(), unit) as usize);
        // SAFETY: the effect's unit is live while referenced
        let (complex, stats_scale, physical) = unsafe {
            let t = WorldRead::begin();
            let u = UnitView::of(&t, unit).expect("the effect's unit");
            let c = u.complex_unit();
            (
                c.map(|c| effect_reads::animation(&c)),
                u.stats().expect("a unit's stats").scale(),
                c.map(|c| effect_reads::physical(&c)),
            )
        };
        let list = model.tag_points(complex.unwrap_or(0), g.fields().mKind);
        let mesh = (w.mesh)();
        let (y, x) = (um::floor_unsigned(position.y), um::floor_unsigned(position.x));
        // SAFETY: the map's live terrain mesh
        if unsafe { crate::ge::tmobj::ee_ge_tm_is_valid(mesh, x as _, y as _, false) } {
            // SAFETY: as above
            let z = unsafe { crate::ge::tmobj::ee_ge_tm_mesh_z(mesh, position.x, position.y) };
            if position.z < z {
                position.z = z + lift;
            }
        }
        match list.filter(|l| l.len != 0) {
            Some(list) => {
                let (mut t, scale, frame) = match physical {
                    Some(p) => (p.location, p.scale, p.frame),
                    None => {
                        let mut t = GeTransformation::IDENTITY;
                        t.m[0][3] = position.x;
                        t.m[1][3] = position.y;
                        t.m[2][3] = position.z;
                        (t, stats_scale, 0)
                    }
                };
                crate::gfx::tags::scale(&mut t, scale);
                let mut maximum = 0.0f32;
                for i in 0..list.len {
                    let object = ee_gfx_effect_graphic_object(e);
                    (w.media_load)(object);
                    let point = effect_reads::tag_point(list, i, frame, &t, None);
                    let start = Gobj(object as usize).fields().mStartFrames[0] as u32;
                    obj(e).parts.push(Part { position: point, object, tag: i as u32, frame: start, ..Part::default() });
                    let distance = position.distance_to_squared(point);
                    if distance > maximum {
                        maximum = distance;
                    }
                }
                height = (f64::from(maximum)).sqrt() as f32 + size;
            }
            None => {
                let object = ee_gfx_effect_graphic_object(e);
                (w.media_load)(object);
                let start = Gobj(object as usize).fields().mStartFrames[0] as u32;
                obj(e).parts.push(Part { position, object, tag: OWN_POSITION, frame: start, ..Part::default() });
                height = size;
            }
        }
    }
    EeGfx2DPlace { position, height }
}

/// Part `index` on the unit's current frame: its model's tag point (the unit itself for tag 10000) under the unit's
/// location (`EEE2DFire::UpdatePart`; nothing without a complex unit).
fn part_follow(e: EffectPtr, index: u32) -> EeGfx2DPoint {
    let unit = head(e).unit;
    // SAFETY: the effect's unit is live while referenced
    let physical = unsafe {
        let t = WorldRead::begin();
        UnitView::of(&t, unit)
            .expect("the effect's unit")
            .complex_unit()
            .map(|c| (c.handle(), effect_reads::physical(&c)))
    };
    let Some((complex, p)) = physical else { return EeGfx2DPoint::default() };
    let tag = obj(e).parts[index as usize].tag;
    let mut offset = Ge3DPoint::default();
    if tag != OWN_POSITION {
        let model = Gobj((world().unit_graphic)(std::ptr::null_mut(), complex) as usize).shared_model();
        offset = crate::gfx::effect_reads::model_tag_point(model, tag as usize, p.frame as usize);
    }
    EeGfx2DPoint { has: true, point: p.location.apply_point(offset) }
}

// ---- one operation by the kind's class

/// The effect's refs for the answers (`effect_reads.rs`).
fn refs(e: EffectPtr) -> EeGfxEffectRefs {
    let h = head(e);
    EeGfxEffectRefs {
        record: h.record,
        graphic_effect: h.graphic_effect,
        graphic_object: &h.graphic_object,
        unit_model: &h.unit_model,
        target: &h.target,
        from_unit: &h.from_unit,
        to_unit: &h.to_unit,
        plane_model: &h.model,
        volcano_model: &h.volcano_model,
        unit_graphic: world().unit_graphic,
    }
}

fn ask_of<'a>(context: *mut c_void) -> &'a EeGfxEffectAsk {
    // SAFETY: the answers' context is the call's ask, alive for the call
    unsafe { &*(context as *const EeGfxEffectAsk) }
}

fn e_of(context: *mut c_void) -> EffectPtr {
    ask_of(context).effect
}

/// The effects' facts: their graphic effect's (the record and its object), the unit's hit points, the tick's.
fn facts(e: EffectPtr) -> EeGfxGraphicFacts {
    let w = world();
    let tick = (w.tick)();
    let g = head(e).graphic_effect;
    let unit = if head(e).unit_is_saved_id { 0 } else { head(e).unit };
    let mut f = EeGfxGraphicFacts {
        tick_seconds: tick.tick_seconds,
        tick_time: tick.tick_time,
        game_time_bits: tick.game_time_bits,
        free_camera: tick.free_camera,
        speed: tick.speed,
        world_speed: tick.world_speed,
        has_unit: unit != 0,
        ..EeGfxGraphicFacts::default()
    };
    if unit != 0 {
        // SAFETY: the effect's unit is live while referenced
        f.unit_hit_points = unsafe {
            let t = WorldRead::begin();
            UnitView::of(&t, unit).expect("the effect's unit").hit_points() as i32
        };
    }
    if g != 0 {
        let ge = GraphicEffect(g as usize);
        let r = ge.fields();
        let object = ge.object();
        f.size = r.mSize;
        f.start_size = r.mStartSize;
        f.param1 = r.mParam1;
        f.param2 = r.mParam2;
        f.param3 = r.mParam3;
        f.param4 = r.mParam4;
        f.param5 = r.mParam5;
        f.param6 = r.mParam6;
        f.flag1 = r.mFlag1;
        f.flag2 = r.mFlag2;
        f.param7 = r.mParam7;
        f.param8 = r.mParam8;
        f.duration = r.mDuration as u32;
        f.has_object = object.is_some();
        f.alpha_ref = object.map_or(0.0, |o| o.fields().mAlphaReference);
        f.int_param1 = r.mIntParam1 as i32;
        f.int_param2 = r.mIntParam2 as i32;
        f.int_param3 = r.mIntParam3 as i32;
        f.has_linked1 = ge.linked(0) != 0;
        f.has_linked3 = ge.linked(2) != 0;
        f.alpha = r.mAlpha;
        f.param9 = r.mParam9;
        f.param10 = r.mParam10;
        f.param11 = r.mParam11;
        f.has_effect = true;
        f.time1 = r.mTime1 as u32;
        f.time2 = r.mTime2 as i32;
        f.ends_with_unit = r.mEndsWithUnitOrDuration;
        f.emit_spread = r.mEmitPositionSpread;
        f.emit_flat = r.mIsEmitFlat;
    }
    f
}

// the answers this module gives (the tables' entries it replaces)
extern "C" fn a_move_to(context: *mut c_void) {
    let e = e_of(context);
    let t = rec(e).transformation;
    ee_gfx_effect_set_transformation(e, t);
}
extern "C" fn a_move_to_point(context: *mut c_void) {
    let e = e_of(context);
    let at = rec(e).position;
    ee_gfx_effect_move_to_point(e, at);
}
extern "C" fn a_base_queue_3d(context: *mut c_void) -> i32 {
    ee_gfx_effect_base_queue(e_of(context), true)
}
extern "C" fn a_base_queue_2d(context: *mut c_void) -> i32 {
    let e = e_of(context);
    ee_gfx_effect_base_queue(e, rec(e).kind != kind::CONFIRMATION)
}
extern "C" fn a_effect_queue(context: *mut c_void) -> i32 {
    ee_gfx_effect_base_queue(e_of(context), false)
}
/// pushes the effect on render queue `index` under its graphic effect's object's texture for `gScreenIndex`
extern "C" fn a_queue(context: *mut c_void, index: u32) {
    let e = e_of(context);
    let object = GraphicEffect(head(e).graphic_effect as usize).object();
    let texture = object.map_or(0, |o| o.texture((world().screen)(false) as usize)) as u64;
    queue_push(index, texture, e);
}
/// likewise under `GetGraphicObject()`'s texture for the preferences' screen
extern "C" fn a_queue_screen(context: *mut c_void, index: u32) {
    let e = e_of(context);
    let object = Gobj::of(ee_gfx_effect_graphic_object(e) as usize);
    let texture = object.map_or(0, |o| o.texture((world().screen)(true) as usize)) as u64;
    queue_push(index, texture, e);
}
/// the graphic object is `GetGraphicObject()`; its `LoadMedia`
extern "C" fn a_media(context: *mut c_void) -> i32 {
    let e = e_of(context);
    let object = ee_gfx_effect_graphic_object(e);
    head(e).graphic_object = object;
    (world().media_load)(object)
}
/// the graphic object is the graphic effect's own (not `GetGraphicObject`); its `LoadMedia`
extern "C" fn a_own_media(context: *mut c_void) -> i32 {
    let e = e_of(context);
    let object = GraphicEffect(head(e).graphic_effect as usize).object().map_or(0, |o| o.0 as u64);
    head(e).graphic_object = object;
    (world().media_load)(object)
}
extern "C" fn a_march_media(context: *mut c_void) {
    a_own_media(context);
}
/// the icon's set-up: the new object loaded, then the one it replaces unloaded (shared media stays loaded)
extern "C" fn a_reload_media(context: *mut c_void) -> i32 {
    let e = e_of(context);
    let previous = head(e).graphic_object;
    let object = ee_gfx_effect_graphic_object(e);
    head(e).graphic_object = object;
    let res = (world().media_load)(object);
    if previous != 0 {
        (world().media_unload)(previous);
    }
    res
}
/// the additive pass's unit model (`mUnit->GetModel()`), loaded
extern "C" fn a_unit_model_media(context: *mut c_void) -> i32 {
    let e = e_of(context);
    let model = (world().unit_graphic)(std::ptr::null_mut(), head(e).unit);
    head(e).unit_model = model;
    (world().media_load)(model)
}
/// the tracer beam's target as its saved id (`USavedPointer`: sign-extended, so a saved "none" is the -1 pointer)
extern "C" fn a_tracer_saved_target(context: *mut c_void, id: u32) {
    head(e_of(context)).target = id as i32 as i64 as u64;
}
extern "C" fn a_march_set_unit(context: *mut c_void, which: u32, id: u32) {
    let h = head(e_of(context));
    if which == 0 {
        h.from_unit = u64::from(id);
    } else {
        h.to_unit = u64::from(id);
    }
}
extern "C" fn a_create_parts(context: *mut c_void, lift: f32) -> EeGfx2DPlace {
    create_parts(e_of(context), lift)
}
extern "C" fn a_part_count(context: *mut c_void) -> u32 {
    obj(e_of(context)).parts.len() as u32
}
extern "C" fn a_part(context: *mut c_void, index: u32) -> EeGfx2DPart {
    let p = obj(e_of(context)).parts[index as usize];
    EeGfx2DPart {
        position: p.position,
        tag: p.tag,
        frame: p.frame,
        draw_frame: p.draw_frame,
        time: p.time,
        done: p.done,
    }
}
extern "C" fn a_set_part(context: *mut c_void, index: u32, part: *const EeGfx2DPart) {
    // SAFETY: the caller's part
    let p = unsafe { *part };
    let q = &mut obj(e_of(context)).parts[index as usize];
    q.position = p.position;
    q.tag = p.tag;
    q.frame = p.frame;
    q.draw_frame = p.draw_frame;
    q.time = p.time;
    q.done = p.done;
}
extern "C" fn a_part_frames(context: *mut c_void, index: u32) -> EeGfxFrames {
    Gobj(obj(e_of(context)).parts[index as usize].object as usize).frames(0)
}
extern "C" fn a_part_texture(context: *mut c_void, index: u32) -> EeGfxModel {
    Gobj(obj(e_of(context)).parts[index as usize].object as usize).texture(0) as EeGfxModel
}
extern "C" fn a_parts_snapshot(context: *mut c_void) {
    for p in &mut obj(e_of(context)).parts {
        p.draw_frame = p.frame;
    }
}
extern "C" fn a_part_follow(context: *mut c_void, index: u32) -> EeGfx2DPoint {
    part_follow(e_of(context), index)
}

/// The graphic effect's linked effect `i` (0..2; 0: none).
fn linked(e: EffectPtr, i: usize) -> u64 {
    GraphicEffect(head(e).graphic_effect as usize).linked(i)
}

fn place(graphic_effect: u64, at: Ge3DPoint) -> i32 {
    use crate::gfx::ffi_manager::{create_current, EeGfxEffectArgs};
    let args = EeGfxEffectArgs {
        entry: crate::gfx::effect_manager::EeGfxEffectEntry::Placed,
        graphic_effect,
        at,
        ..EeGfxEffectArgs::default()
    };
    create_current(&args)
}

fn line_effect(graphic_effect: u64, start: Ge3DPoint, direction: GeVector) -> i32 {
    use crate::gfx::ffi_manager::{create_current, EeGfxEffectArgs};
    let args = EeGfxEffectArgs {
        entry: crate::gfx::effect_manager::EeGfxEffectEntry::Line,
        graphic_effect,
        line: Ge3DLine { start, direction },
        ..EeGfxEffectArgs::default()
    };
    create_current(&args)
}

/// `CreateLineEffect` from `start` along `direction` with linked effect 1 or 2
extern "C" fn a_line(context: *mut c_void, start: *const Ge3DPoint, direction: *const GeVector, which: u32) {
    let e = e_of(context);
    // SAFETY: the caller's points
    let (start, direction) = unsafe { (*start, *direction) };
    line_effect(linked(e, if which == 1 { 0 } else { 1 }), start, direction);
}
/// the linked effect 1 placed at `point`, when there is one
extern "C" fn a_place_linked(context: *mut c_void, point: *const Ge3DPoint) {
    let g = linked(e_of(context), 0);
    if g != 0 {
        // SAFETY: the caller's point
        place(g, unsafe { *point });
    }
}
/// the linked effect 1 placed at `point`: the placement's result
extern "C" fn a_spawn(context: *mut c_void, point: *const Ge3DPoint) -> i32 {
    // SAFETY: the caller's point
    place(linked(e_of(context), 0), unsafe { *point })
}
/// a line effect of the linked effect 1 from `start` along `direction`: its result
extern "C" fn a_emitter_line(context: *mut c_void, start: *const Ge3DPoint, direction: *const Ge3DPoint) -> i32 {
    // SAFETY: the caller's points
    let (start, d) = unsafe { (*start, *direction) };
    line_effect(linked(e_of(context), 0), start, GeVector { x: d.x, y: d.y, z: d.z })
}
/// the multi-effect's linked effect `index` (1..3) through its constructor's entry point (on its unit, at its
/// point, along its line: the ask's argument)
extern "C" fn a_create_linked(context: *mut c_void, index: u32) -> i32 {
    use crate::gfx::effect_manager::EeGfxEffectEntry;
    use crate::gfx::ffi_manager::{create_current, EeGfxEffectArgs};
    let ask = ask_of(context);
    let g = linked(ask.effect, index as usize - 1);
    let mut args = EeGfxEffectArgs { graphic_effect: g, ..EeGfxEffectArgs::default() };
    // SAFETY: the constructor's argument, alive for its call
    unsafe {
        match ask.op {
            EeGfxEffect3DOp::CreateLine => {
                args.entry = EeGfxEffectEntry::Line;
                args.line = *(ask.arg as *const Ge3DLine);
            }
            EeGfxEffect3DOp::CreateAt => {
                args.entry = EeGfxEffectEntry::Placed;
                args.at = *(ask.arg as *const Ge3DPoint);
            }
            _ => {
                args.entry = EeGfxEffectEntry::Unit;
                args.unit = ask.arg as u64;
            }
        }
    }
    create_current(&args)
}
/// the tracer beam's impact: the linked effect 1 placed at the target point on the target unit (or at the fixed
/// point)
extern "C" fn a_tracer_impact(context: *mut c_void) -> i32 {
    let e = e_of(context);
    let point = rec(e).tracer.target_point;
    let target = head(e).target;
    let at = if target != 0 {
        // SAFETY: the beam's target is live while referenced
        unsafe {
            let t = WorldRead::begin();
            let c = UnitView::of(&t, target).expect("the beam's target").complex_unit().expect("a complex target");
            effect_reads::physical(&c).location.apply_point(point)
        }
    } else {
        point
    };
    place(linked(e, 0), at)
}

/// The battlefield decals' placement over the manager's list (`EEE2DBattlefieldAmbient.cpp`'s helpers, ee_gfx
/// effect_placement.rs): shell holes around a point (`EEPlaceImpactDecal`).
///
/// # Safety
/// `graphic_effect` is a live graphic effect; the game's manager is live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_decal_impact(graphic_effect: u64, at: Ge3DPoint) {
    let host = decal_host();
    // SAFETY: the function's contract
    unsafe { crate::gfx::effect_placement::ee_gfx_decal_impact(&host, graphic_effect as *mut c_void, &at) }
}

/// The unit's track decal at `at` (`EEPlaceTrackDecal`: `kind` the giraffe's or horse's offset, the heading's
/// `m00`, `m10`).
///
/// # Safety
/// As [`ee_gfx_effects_decal_impact`].
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_decal_track(graphic_effect: u64, at: Ge3DPoint, kind: u32, m00: f32, m10: f32) {
    let host = decal_host();
    // SAFETY: the function's contract
    unsafe {
        crate::gfx::effect_placement::ee_gfx_decal_track(&host, graphic_effect as *mut c_void, &at, kind, m00, m10)
    }
}

fn decal_host() -> crate::gfx::effect_placement::EeGfxDecalHost {
    use crate::gfx::effect_placement::{EeGfxDecalEffect, EeGfxDecalHost};
    extern "C" fn effect(_: *mut c_void, g: *mut c_void) -> EeGfxDecalEffect {
        let ge = GraphicEffect(g as usize);
        let f = ge.fields();
        EeGfxDecalEffect {
            param2: f.mParam2,
            param3: f.mParam3,
            param4: f.mParam4,
            param5: f.mParam5,
            int_param2: f.mIntParam2 as i32,
            linked: [ge.linked(0) as *mut c_void, ge.linked(1) as *mut c_void, ge.linked(2) as *mut c_void],
        }
    }
    extern "C" fn next(_: *mut c_void, e: *mut c_void) -> *mut c_void {
        let m = (crate::gfx::ffi_manager::mworld().current)();
        if e.is_null() {
            crate::gfx::ffi_manager::ee_gfx_manager_first(m) as *mut c_void
        } else {
            crate::gfx::ffi_manager::ee_gfx_manager_next(m, e as EffectPtr) as *mut c_void
        }
    }
    extern "C" fn record(_: *mut c_void, e: *mut c_void) -> *mut EeGfxEffectRecord {
        head(e as EffectPtr).record
    }
    extern "C" fn height(_: *mut c_void, point: *const Ge3DPoint) -> f32 {
        (world().height)(point)
    }
    extern "C" fn put(_: *mut c_void, g: *mut c_void, at: *const Ge3DPoint) {
        // SAFETY: the placement's point
        place(g as u64, unsafe { *at });
    }
    EeGfxDecalHost { context: std::ptr::null_mut(), effect, next, record, height, place: put }
}

/// the terrain mesh's height at a point (`GetMeshZ`)
extern "C" fn a_terrain_z(_: *mut c_void, x: f32, y: f32) -> f32 {
    // SAFETY: the map's live terrain mesh
    unsafe { crate::ge::tmobj::ee_ge_tm_mesh_z((world().mesh)(), x, y) }
}
/// the terrain mesh's tile is a space tile (`GetIsSpaceTile`)
extern "C" fn a_space_tile(_: *mut c_void, x: u32, y: u32) -> bool {
    // SAFETY: as above
    unsafe { crate::ge::tmobj::ee_ge_tm_is_space_tile((world().mesh)(), x as _, y as _) }
}

/// a tracer's end: its linked effect 1 placed at its position, then its unit's hit (`EEE3DTracers::Step`)
extern "C" fn a_shot_hit(context: *mut c_void) {
    let e = e_of(context);
    let g = linked(e, 0);
    if g != 0 {
        let t = rec(e).transformation;
        place(g, Ge3DPoint { x: t.x(), y: t.y(), z: t.z() });
    }
    if head(e).unit != 0 {
        (world().unit_damaged)(head(e).unit);
    }
}
/// the tracer beam's hit on its target
extern "C" fn a_tracer_hit(context: *mut c_void) {
    (world().unit_damaged)(head(e_of(context)).target);
}
/// the rain's three sound objects' media (`EEE3DRain::Virtual64`)
extern "C" fn a_rain_media(context: *mut c_void) {
    let sounds = rec(e_of(context)).storm.sounds;
    for id in sounds {
        (world().sound_object_load)(id);
    }
}
/// The particles' move facts at `position` (`EEParticleTick`: the game clock, the wind, the graphic effect's).
fn particle_tick(e: EffectPtr, position: Ge3DPoint) -> effect_state::EeGfxParticleTick {
    let tick = (world().tick)();
    let f = GraphicEffect(head(e).graphic_effect as usize).fields();
    effect_state::EeGfxParticleTick {
        now: tick.game_clock,
        game_over: tick.game_over,
        wind: tick.wind,
        position,
        duration: f.mDuration as u32,
        param4: f.mParam4,
        param8: f.mParam8,
        param9: f.mParam9,
        param10: f.mParam10,
        param11: f.mParam11,
        flag2: f.mFlag2,
    }
}
extern "C" fn a_particle_tick_3d(context: *mut c_void, position: *const Ge3DPoint) -> effect_state::EeGfxParticleTick {
    // SAFETY: the caller's point
    particle_tick(e_of(context), unsafe { *position })
}
extern "C" fn a_particle_tick_2d(context: *mut c_void) -> effect_state::EeGfxParticleTick {
    let e = e_of(context);
    particle_tick(e, rec(e).position)
}
extern "C" fn a_wind(_: *mut c_void) -> Ge3DPoint {
    (world().tick)().wind
}

// the terrain the particles bounce off (the map's mesh)
extern "C" fn g_valid_tile(_: *mut c_void, x: u32, y: u32) -> bool {
    // SAFETY: the map's live terrain mesh
    unsafe { crate::ge::tmobj::ee_ge_tm_is_valid((world().mesh)(), x as _, y as _, true) }
}
extern "C" fn g_space_tile(_: *mut c_void, x: u32, y: u32) -> bool {
    // SAFETY: as above
    unsafe { crate::ge::tmobj::ee_ge_tm_is_space_tile((world().mesh)(), x as _, y as _) }
}
extern "C" fn g_mesh_z(_: *mut c_void, x: f32, y: f32) -> f32 {
    // SAFETY: as above
    unsafe { crate::ge::tmobj::ee_ge_tm_mesh_z((world().mesh)(), x, y) }
}
extern "C" fn g_mesh_normal(_: *mut c_void, x: f32, y: f32, out: *mut Ge3DPoint) {
    let mut n = GeVector::default();
    // SAFETY: as above; the caller's point
    unsafe {
        crate::ge::tmobj::ee_ge_tm_mesh_normal((world().mesh)(), x, y, &mut n);
        *out = Ge3DPoint { x: n.x, y: n.y, z: n.z };
    }
}

/// The terrain mesh as the particles' move asks it (`EEParticleGround`).
#[no_mangle]
pub extern "C" fn ee_gfx_particle_ground() -> crate::gfx::ffi_effect::EeGfxParticleGround {
    crate::gfx::ffi_effect::EeGfxParticleGround {
        context: std::ptr::null_mut(),
        is_valid_tile: g_valid_tile,
        is_space_tile: g_space_tile,
        mesh_z: g_mesh_z,
        mesh_normal: g_mesh_normal,
    }
}
extern "C" fn a_particle_ground(_: *mut c_void) -> crate::gfx::ffi_effect::EeGfxParticleGround {
    ee_gfx_particle_ground()
}

/// the emitter on a unit (`EEE3DParticleEmitter`'s unit constructor): at its complex unit's location unless it follows
/// the tag point (the identity then); a unit without a complex unit at its tile's centre on the map (0 off it)
extern "C" fn a_emitter_place(context: *mut c_void) {
    let e = e_of(context);
    let flag1 = GraphicEffect(head(e).graphic_effect as usize).fields().mFlag1;
    // SAFETY: the effect's unit is live while referenced
    let (location, size, (tx, ty)) = unsafe {
        let t = WorldRead::begin();
        let u = UnitView::of(&t, head(e).unit).expect("the emitter's unit");
        let size: std::os::raw::c_long = ee_sim::world_field!(u.class().expect("a unit's class").fields(), mTileSize);
        (u.complex_unit().map(|c| effect_reads::physical(&c).location), size, u.tile_xy())
    };
    let t = match location {
        Some(l) => {
            if flag1 {
                GeTransformation::IDENTITY
            } else {
                l
            }
        }
        None => {
            let half = size as f32 * 0.5;
            let mut at = Ge3DPoint {
                x: tx as std::os::raw::c_long as f32 + half,
                y: ty as std::os::raw::c_long as f32 + half,
                z: 0.0,
            };
            // SAFETY: the map's live terrain mesh
            if unsafe { crate::ge::tmobj::ee_ge_tm_is_valid((world().mesh)(), tx as _, ty as _, true) } {
                at.z = (world().height)(&at);
            }
            let mut t = GeTransformation::IDENTITY;
            t.m[0][3] = at.x;
            t.m[1][3] = at.y;
            t.m[2][3] = at.z;
            t
        }
    };
    ee_gfx_effect_set_transformation(e, t);
}

/// The 3D kinds' answers for one call: this module's and the C++'s.
fn host3d(e: EffectPtr, ask: &EeGfxEffectAsk, unit: u64, player: u64) -> EeGfxEffect3DHost {
    let w = world();
    let c = &w.answers3d;
    EeGfxEffect3DHost {
        context: ask as *const EeGfxEffectAsk as *mut c_void,
        unit,
        player,
        refs: refs(e),
        move_to: a_move_to,
        create_line: a_line,
        base_queue: a_base_queue_3d,
        queue: a_queue,
        load_media: a_media,
        shot_hit: a_shot_hit,
        place_linked: a_place_linked,
        unit_model_media: a_unit_model_media,
        animation_sound: c.animation_sound,
        spawn: a_spawn,
        terrain_z: a_terrain_z,
        space_tile: a_space_tile,
        create_linked: a_create_linked,
        wall_gate_posts: c.wall_gate_posts,
        wall_piece: c.wall_piece,
        reload_media: a_reload_media,
        queue_screen: a_queue_screen,
        tracer_impact: a_tracer_impact,
        tracer_hit: a_tracer_hit,
        tracer_saved_target: a_tracer_saved_target,
        volcano_build: c.volcano_build,
        farm_terrain: c.farm_terrain,
        farm_drop_site: c.farm_drop_site,
        farm_set_variant: c.farm_set_variant,
        farm_doppelganger_any: c.farm_doppelganger_any,
        farm_doppelganger: c.farm_doppelganger,
        farm_opacity: c.farm_opacity,
        set_scaled_model: c.set_scaled_model,
        ground_point: c.ground_point,
        smoke_place: a_spawn,
        // SAFETY: a bitwise copy of the registered storm host (function pointers)
        storm: unsafe { std::ptr::read(&w.storm) },
        rain_media: a_rain_media,
        rain_sounds: c.rain_sounds,
        particle_tick: a_particle_tick_3d,
        particle_ground: a_particle_ground,
        emitter_place: a_emitter_place,
        emitter_sound: c.emitter_sound,
        emitter_line: a_emitter_line,
        effect_queue: a_effect_queue,
        march_media: a_march_media,
        march_set_unit: a_march_set_unit,
    }
}

/// The 2D kinds' answers for one call: this module's and the C++'s.
fn host2d(e: EffectPtr, ask: &EeGfxEffectAsk, unit: u64, player: u64) -> EeGfxEffect2DHost {
    let c = &world().answers2d;
    EeGfxEffect2DHost {
        context: ask as *const EeGfxEffectAsk as *mut c_void,
        unit,
        player,
        refs: refs(e),
        base_queue: a_base_queue_2d,
        queue: a_queue,
        load_media: a_own_media,
        move_to_point: a_move_to_point,
        confirm_goal: c.confirm_goal,
        terrain_z: a_terrain_z,
        object_media: a_media,
        weapon_point: a_weapon_point,
        animation_sound: c.animation_sound,
        draw_blood: c.draw_blood,
        tile_blocked: c.tile_blocked,
        wind: a_wind,
        create_parts: a_create_parts,
        part_count: a_part_count,
        part: a_part,
        set_part: a_set_part,
        part_frames: a_part_frames,
        part_texture: a_part_texture,
        parts_snapshot: a_parts_snapshot,
        part_follow: a_part_follow,
        area_effect: c.area_effect,
        area_effects: c.area_effects,
        set_area_effects: c.set_area_effects,
        icons_selected: c.icons_selected,
        ground_point: c.ground_point,
        glow_area: c.glow_area,
        glow_rows: c.glow_rows,
        particle_tick: a_particle_tick_2d,
        particle_ground: a_particle_ground,
    }
}

/// the unit's weapon tag point (site 3: the list assumed; sites 1 and 2, the 2D projectile's, test it)
extern "C" fn a_weapon_point(context: *mut c_void, site: i32) -> Ge3DPoint {
    let mut tagged = false;
    (world().weapon_point)(head(e_of(context)).unit, site, site <= 2, &mut tagged)
}

/// The graphic object of the 2D part `index` (the impact's sound reads the first one's).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_part_object(e: EffectPtr, index: u32) -> u64 {
    obj(e).parts[index as usize].object
}

fn unit_and_player(e: EffectPtr) -> (u64, u64) {
    let unit = if head(e).unit_is_saved_id { 0 } else { head(e).unit };
    (unit, current_player().0)
}

/// One operation by the kind's class (`arg`: a constructor's unit, point or line, for the C++ answers that need it).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_call_arg(e: EffectPtr, op: EeGfxEffect3DOp, arg: *mut c_void) -> i32 {
    let w = world();
    let ask = EeGfxEffectAsk { effect: e, op, arg };
    let (unit, player) = unit_and_player(e);
    let g = facts(e);
    // SAFETY: the registered raster table
    let mut raster = unsafe { &*w.raster };
    if head(e).is_2d {
        let h = host2d(e, &ask, unit, player);
        // SAFETY: the effect's live record; the answers' context lives for the call
        unsafe { effect_2d::effect2d(op, head(e).record, &g, &mut &h, &mut raster) }
    } else {
        let h = host3d(e, &ask, unit, player);
        // SAFETY: as above
        unsafe { effect_rising::effect3d(op, head(e).record, &g, &mut &h, &mut raster) }
    }
}

/// One operation by the kind's class.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_call(e: EffectPtr, op: EeGfxEffect3DOp, arg: *mut c_void) -> i32 {
    ee_gfx_effect_call_arg(e, op, arg)
}

/// `Load` or `Save` of the kind's class after the base's (`base`: the 3D base's result).
fn file_op(e: EffectPtr, op: EeGfxEffect3DOp, f: &EeGfxEffectFile, base: i32, major: u32, minor: u32) -> i32 {
    let ask = EeGfxEffectAsk { effect: e, op, arg: std::ptr::null_mut() };
    let (unit, player) = unit_and_player(e);
    if head(e).is_2d {
        let h = host2d(e, &ask, unit, player);
        on_copy(e, |r| effect_2d::effect2d_file(op, r, &mut File(f), major, minor, &mut &h))
    } else {
        let h = host3d(e, &ask, unit, player);
        on_copy(e, |r| effect_rising::effect3d_file(op, base, r, &mut File(f), major, minor, &mut &h))
    }
}

/// `SetUp`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_set_up(e: EffectPtr) -> i32 {
    ee_gfx_effect_call(e, EeGfxEffect3DOp::SetUp, std::ptr::null_mut())
}

// ---- the render queues

/// The effects' render queues, one per pass (the original's `gEffectQueues[5]`: maps from the texture to the
/// effects queued under it, in texture order), each effect referenced while queued.
static QUEUES: parking_lot::Mutex<[BTreeMap<u64, VecDeque<usize>>; 5]> =
    parking_lot::Mutex::new([BTreeMap::new(), BTreeMap::new(), BTreeMap::new(), BTreeMap::new(), BTreeMap::new()]);

fn queue_push(index: u32, texture: u64, e: EffectPtr) {
    QUEUES.lock()[index as usize].entry(texture).or_default().push_back(e as usize);
}

/// Every queued effect released, the queues emptied (each frame's start; the scene's release).
///
/// # Safety
/// The queued effects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_queues_clear(release: bool) {
    let queued: Vec<usize> = {
        let mut q = QUEUES.lock();
        let all = q.iter().flat_map(|m| m.values().flat_map(|d| d.iter().copied())).collect();
        for m in q.iter_mut() {
            m.clear();
        }
        all
    };
    if release {
        for e in queued {
            // SAFETY: a queued effect holds the reference its queueing took
            unsafe { ee_gfx_effect_release(e as EffectPtr) };
        }
    }
}

/// `EERenderObject::QueueEffects`: the queues rebuilt from the effects of every map region under a visible tile row
/// (`rows`: each row's y and first and last x, in the mesh's order); an effect shown only to another player is
/// left out, one its queue accepts (0) is referenced.
///
/// # Safety
/// `rows` holds `count` triples; the regions' effects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_queues_fill(rows: *const i64, count: usize) {
    // SAFETY: the function's contract
    unsafe { ee_gfx_effect_queues_clear(true) };
    let Some(size) = with_regions(|t| t.size as i64) else { return };
    // SAFETY: the function's contract
    let rows = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(rows, count * 3) } };
    let mut keyed: BTreeMap<i64, (i64, i64)> = BTreeMap::new();
    for r in rows.chunks(3) {
        let (y, first, last) = (r[0], r[1] / size, r[2] / size);
        let key = y / size;
        keyed.entry(key).and_modify(|v| *v = (v.0.min(first), v.1.max(last))).or_insert((first, last));
    }
    let current = current_player().1;
    for (&key, &(first, last)) in &keyed {
        for x in first..=last {
            let Some(effects) = with_regions(|t| t.region(x, key).map(|l| l.clone())).flatten() else { continue };
            for e in effects {
                let r = rec(e);
                if r.local_only && i64::from(r.player) != current {
                    continue;
                }
                if ee_gfx_effect_call(e, EeGfxEffect3DOp::Queue, std::ptr::null_mut()) == 0 {
                    ee_gfx_effect_reference(e);
                }
            }
        }
    }
}

/// One pass's draws over its queue (2..6: queues 0..4), batched per texture but the units' overlays (pass 3).
///
/// # Safety
/// The queued effects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_queues_draw(pass: u32) {
    let index = (pass - 2) as usize;
    let op = match pass {
        2 => EeGfxEffect3DOp::DrawPass2,
        3 => EeGfxEffect3DOp::DrawPass3,
        4 => EeGfxEffect3DOp::DrawPass4,
        5 => EeGfxEffect3DOp::DrawPass5,
        _ => EeGfxEffect3DOp::DrawPass6,
    };
    let entries: Vec<(u64, Vec<usize>)> =
        QUEUES.lock()[index].iter().map(|(&t, d)| (t, d.iter().copied().collect())).collect();
    let raster = world().raster;
    for (texture, effects) in entries {
        if pass != 3 {
            // SAFETY: the registered raster table
            unsafe { crate::gfx::ffi::ee_gfx_effect_batches_begin(raster, texture as *mut c_void) };
        }
        for e in effects {
            ee_gfx_effect_call(e as EffectPtr, op, std::ptr::null_mut());
        }
        if pass != 3 {
            // SAFETY: as above
            unsafe {
                crate::gfx::ffi::ee_gfx_effect_batches_flush(raster);
                crate::gfx::ffi::ee_gfx_effect_batches_end(raster);
            }
        }
    }
}

// ---- the storms (EEEMStorm's)

/// The weather storm's tiles' bounds (left, top, right, bottom); false without tiles.
///
/// # Safety
/// `bounds` holds four values.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_storm_bounds(e: EffectPtr, bounds: *mut i32) -> bool {
    // SAFETY: the function's contract
    unsafe { crate::gfx::ffi_effect::ee_gfx_storm_bounds(head(e).record, bounds) }
}

/// The weather storm's move one step towards `(x, y)`: its tiles, their bounds into `bounds`, the storm recentred on
/// the terrain there (`SetTransformation`); false when it did not move (the bounds then the caller's to ignore).
///
/// # Safety
/// `bounds` holds four values.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_storm_move(e: EffectPtr, x: i32, y: i32, bounds: *mut i32) -> bool {
    let mut p = Ge3DPoint::default();
    // SAFETY: the function's contract
    let moved =
        unsafe { crate::gfx::ffi_effect::ee_gfx_storm_move(head(e).record, x, y, bounds, &world().storm, &mut p) };
    if moved {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = p.x;
        t.m[1][3] = p.y;
        t.m[2][3] = p.z;
        ee_gfx_effect_set_transformation(e, t);
    }
    moved
}

#[cfg(test)]
#[path = "effect_obj_tests.rs"]
pub(crate) mod tests;
