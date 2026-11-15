//! The teleport (kind 0x3f) and the time warp (kind 0x44) whole in Rust (`EEE3DTeleport`, `EEE3DTimeWarp`, deleted):
//! a model on a unit that rises or sinks over the effect's duration, launching the effect's two linked line effects
//! from the far end of its span when it is made, and drawn in pass 4 fully transparent with z writes (a depth-only
//! mask). The generic C++ class `EEE3DRust` holds the graphic object and answers the world (`EeGfxEffect3DHost`);
//! the draw goes through the rasterizer table (`raster.rs`). Their age is the aged record's, their step
//! `effect_state::timed_step`, their Load and Save `load_aged`/`save_aged`.
//!
//! The record is reached through its raw pointer field by field: the C++ reads it (the transformation, the radius)
//! during the host calls, so no Rust reference to it lives across one.

use std::ffi::c_void;

use ee_ge::types::{Ge3DPoint, GeTransformation, GeVector};

use crate::effect_additive::{additive, EeGfxUnitFrame, EE_GFX_EFFECT_KIND_ADDITIVE_PASS};
use crate::effect_aged::aged;
use crate::effect_beam::{beam, load_beam, save_beam, EeGfxBeamUnit, EE_GFX_EFFECT_KIND_CITIZEN_BEAM};
use crate::effect_farm::{farm_effect, farm_file, EeGfxFarmUnit, EE_GFX_EFFECT_KIND_FARM};
use crate::effect_follow::{follow, load_shield, save_shield, EE_GFX_EFFECT_KIND_SHIELD};
use crate::effect_hurricane::{hurricane, load_hurricane, save_hurricane, EE_GFX_EFFECT_KIND_HURRICANE};
use crate::effect_icon::{icon, load_icon, save_icon, EE_GFX_EFFECT_KIND_GOAL, EE_GFX_EFFECT_KIND_ICON};
use crate::effect_multi::{multi, EE_GFX_EFFECT_KIND_MULTI_EFFECT};
use crate::effect_nuclear::{
    blast, load_blast, save_blast, EeGfxUnitTile, EE_GFX_EFFECT_KIND_ENERGY_WAVE, EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION,
};
use crate::effect_orbit::{
    load_orbit, orbit, save_orbit, EeGfxAnimation, EeGfxOrbital, EE_GFX_EFFECT_KIND_DISEASE,
    EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE, EE_GFX_EFFECT_KIND_VIRUS,
};
use crate::effect_particles::{is_particle, particles, particles_file, EeGfxEmitterUnit};
use crate::effect_quake::{quake, quake_file, EE_GFX_EFFECT_KIND_EARTHQUAKE};
use crate::effect_shot::{is_shot, load_shot, save_shot, shot};
use crate::effect_smoke::{is_smoke, smoke, EeGfxSmokeUnit};
use crate::effect_state::EeGfxParticleTick;
use crate::effect_state::{
    identity, load_aged, save_aged, timed_step, EeGfxEffectRecord, EeGfxTimedTick, EffectFile,
    EE_GFX_EFFECT_KIND_BLAST_WAVE, EE_GFX_EFFECT_KIND_SCREEN_FLASH, EE_GFX_EFFECT_KIND_TELEPORT,
    EE_GFX_EFFECT_KIND_TIME_WARP, EE_GFX_EFFECT_KIND_WATER_SPLASH, EE_GFX_EFFECT_MOVED,
};
use crate::effect_tracer::{tracer, tracer_file, EeGfxTracerEnds, EE_GFX_EFFECT_KIND_TRACER_BEAM};
use crate::effect_vent::{damage_vent, EeGfxFrames, EeGfxVentPoints, EeGfxVentUnit, EE_GFX_EFFECT_KIND_DAMAGE_VENT};
use crate::effect_volcano::{volcano, volcano_file, EE_GFX_EFFECT_KIND_VOLCANO};
use crate::effect_wall::{wall, wall_file, EE_GFX_EFFECT_KIND_ENERGY_WALL};
use crate::effect_weather::{is_weather, weather, weather_file};
use crate::effects::EeGfxColor;
use crate::ffi_effect::{EeGfxParticleGround, EeGfxStormHost};
use crate::ffi_mesh::EeGfxFarmTerrain;
use crate::lighting::EeGfxLighting;
use crate::raster::{EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE};

/// The time warp starts this far below the unit when it rises.
const TIME_WARP_DROP: f32 = 0.2;

/// The calls of `EEE3DRust`'s virtuals.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxEffect3DOp {
    /// the constructor on a unit (after `EEEffect3D`'s)
    Create = 0,
    SetUp = 1,
    Queue = 2,
    DrawPass2 = 3,
    DrawPass3 = 4,
    DrawPass4 = 5,
    DrawPass5 = 6,
    DrawPass6 = 7,
    Step = 8,
    /// the constructor at a point (the point in the record's `aged.location`)
    CreateAt = 9,
    /// `RepairPointers` after `EEEffect`'s
    Repair = 10,
    /// `Load` and `Save` after `EEEffect3D`'s (`ee_gfx_effect3d_file`)
    Load = 11,
    Save = 12,
    /// the constructor along a line (the multi-effect's)
    CreateLine = 13,
    /// the constructor at a transformation (the 2D water ripple's: the C++ put it in the record's `aged.transformation`)
    CreateTransformation = 14,
    /// the constructor between two units or from a unit to a point (the 2D muzzle blast's: the C++ put the start in the
    /// record's `sprite.start`, the end in `sprite.draw_position`)
    CreateBetween = 15,
    /// the farm's rebuild on its unit (`EEE3DRust::ElevationChanged` after its rectangle test): the area placed again,
    /// set up, the crops built if the farm is
    Rebuild = 16,
}

/// What the effects read of their graphic effect and the tick, gathered by the C++ for each call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGraphicFacts {
    /// `mSize`
    pub size: f32,
    pub param1: f32,
    pub param2: f32,
    pub param3: f32,
    pub param4: f32,
    pub param5: f32,
    pub param6: f32,
    pub flag1: bool,
    /// ms
    pub duration: u32,
    /// the graphic effect has its graphic object, and that object's alpha reference
    pub has_object: bool,
    pub alpha_ref: f32,
    /// `GetTickSeconds`, `GetTickTime`
    pub tick_seconds: f32,
    pub tick_time: f32,
    /// the effect has a unit, and its hit points
    pub has_unit: bool,
    pub unit_hit_points: i32,
    /// `mIntParam1` .. `mIntParam3`
    pub int_param1: i32,
    pub int_param2: i32,
    pub int_param3: i32,
    /// `(unsigned long)gPhysics->mGameTime`
    pub game_time_bits: u32,
    /// `mStartSize`
    pub start_size: f32,
    /// the graphic effect has its linked effect 1
    pub has_linked1: bool,
    /// the graphic effect has its linked effect 3
    pub has_linked3: bool,
    /// `gFreeCamera`
    pub free_camera: bool,
    /// `mFlag2`, `mParam7`, `mParam8`
    pub flag2: bool,
    pub param7: f32,
    pub param8: f32,
    /// `gPhysics->mSpeed` (the game speed)
    pub speed: f32,
    /// `mParam9` .. `mParam11`
    pub param9: f32,
    pub param10: f32,
    pub param11: f32,
    /// the effect has its graphic effect
    pub has_effect: bool,
    /// `mTime1`, `mTime2` (ms)
    pub time1: u32,
    pub time2: i32,
    /// `mAlpha` (the 2D smoke's opacity and trail spacing)
    pub alpha: f32,
    /// `gWorld.GetGameSpeed()` (the snow's fall)
    pub world_speed: f32,
    /// `mEndsWithUnitOrDuration`, `mEmitPositionSpread`, `mIsEmitFlat` (the particle emitter's)
    pub ends_with_unit: bool,
    pub emit_spread: f32,
    pub emit_flat: bool,
}

/// The C++'s answers for an `EEE3DRust`.
#[repr(C)]
pub struct EeGfxEffect3DHost {
    pub context: *mut c_void,
    /// the unit's position (`EEUnit::GetPosition`)
    pub unit_position: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the graphic effect's first model's height in its bounding box at frame 0
    pub model_height: extern "C" fn(context: *mut c_void) -> f32,
    /// `MoveToTransformation(mTransformation)` (the region)
    pub move_to: extern "C" fn(context: *mut c_void),
    /// `gEffectManager->CreateLineEffect` from `start` along `direction` with linked effect 1 or 2
    pub create_line:
        extern "C" fn(context: *mut c_void, start: *const Ge3DPoint, direction: *const GeVector, linked: u32),
    /// `EEEffect3D::Queue`
    pub base_queue: extern "C" fn(context: *mut c_void) -> i32,
    /// push the effect into render queue `index` under its graphic effect's texture
    pub queue: extern "C" fn(context: *mut c_void, index: u32),
    /// the graphic object is the graphic effect's; `LoadMedia`
    pub load_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the graphic object's model
    pub model: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// the graphic object's first texture
    pub texture: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// `FollowUnit`: to the unit's first tag point of type 0x12 (`SetTransformation`)
    pub follow_unit: extern "C" fn(context: *mut c_void),
    /// the graphic object's model `index` (`mModels[index]`)
    pub model_at: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// a tracer's end: its graphic effect's linked effect 1 placed at the effect's position, then the unit's
    /// `UnitDamaged` (each when there is one)
    pub shot_hit: extern "C" fn(context: *mut c_void),
    /// the citizen beam's start: the unit's first tag point at its animation and frame
    pub beam_start: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the citizen beam's unit: a complex unit, its animation and frame
    pub beam_unit: extern "C" fn(context: *mut c_void) -> EeGfxBeamUnit,
    /// the beam's unit is cloaked, another player's and the current player's enemy
    pub beam_hidden: extern "C" fn(context: *mut c_void) -> bool,
    /// the graphic effect's linked effect 1 placed at `point` (`CreatePlacedEffect`), when there is one
    pub place_linked: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint),
    /// the unit's complex unit's position (`mComplexUnit->GetPosition`)
    pub complex_position: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the unit's model is the effect's second model (`mUnit->GetModel()`); its `LoadMedia`
    pub unit_model_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the unit's stats' scale (`mUnit->mStats->GetScale()`)
    pub unit_scale: extern "C" fn(context: *mut c_void) -> f32,
    /// the unit's complex unit's physical model's location
    pub unit_location: extern "C" fn(context: *mut c_void) -> GeTransformation,
    /// the unit's model's first model's bounding radius at frame 0
    pub unit_model_radius: extern "C" fn(context: *mut c_void) -> f32,
    /// the unit's complex unit's physical model's frame and animation
    pub unit_frame: extern "C" fn(context: *mut c_void) -> EeGfxUnitFrame,
    /// the unit's model's model `index` (`mModels[index]`)
    pub unit_model_at: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// the graphic object's orbital count (`GetOrbitalCount`)
    pub orbital_count: extern "C" fn(context: *mut c_void) -> u32,
    /// a copy of the graphic object's orbital `index` (`*GetOrbital(index)`)
    pub orbital: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxOrbital,
    /// the graphic object's animation 13: its start frame, frame time and frame count
    pub animation: extern "C" fn(context: *mut c_void) -> EeGfxAnimation,
    /// the animation's sound at `frame` (`changed`: a new frame) at the unit's location (`PlayAnimationSound`), kept
    /// as the effect's sound (`SetSound`, then its reference dropped, as the original)
    pub animation_sound: extern "C" fn(context: *mut c_void, frame: u32, changed: bool),
    /// the hurricane's spawn points: the graphic's tag points of kind 6 (0: none)
    pub spawn_count: extern "C" fn(context: *mut c_void) -> u32,
    /// spawn point `index` at `frame` around the unit, scaled by the graphic effect's size
    pub spawn_at: extern "C" fn(context: *mut c_void, index: u32, frame: u32) -> Ge3DPoint,
    /// the graphic effect's linked effect 1 placed at `point` (`CreatePlacedEffect`): its result
    pub spawn: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> i32,
    /// the graphic object's texture `index` (`mTextures[index]`)
    pub texture_at: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// the unit's tile facts (on the map, the class's tile size, the tile)
    pub unit_tile: extern "C" fn(context: *mut c_void) -> EeGfxUnitTile,
    /// the terrain mesh's height at a point (`GetMeshZ`)
    pub terrain_z: extern "C" fn(context: *mut c_void, x: f32, y: f32) -> f32,
    /// the unit's complex unit is cloaked, another player's and the current player's enemy
    pub unit_hidden: extern "C" fn(context: *mut c_void) -> bool,
    /// the terrain mesh's tile is a space tile (`GetIsSpaceTile`)
    pub space_tile: extern "C" fn(context: *mut c_void, x: u32, y: u32) -> bool,
    /// the graphic effect's linked effect `index` (1..3) made through the entry point that made the effect (on its
    /// unit, at its point, along its line); the creation's result
    pub create_linked: extern "C" fn(context: *mut c_void, index: u32) -> i32,
    /// the unit's complex unit is a gate: its two end parts' positions
    pub wall_gate_posts: extern "C" fn(context: *mut c_void, first: *mut Ge3DPoint, second: *mut Ge3DPoint) -> bool,
    /// the height of the last living, finished wall piece or gate side part of the owner on the tile into `z`
    pub wall_piece: extern "C" fn(context: *mut c_void, x: i32, y: i32, z: *mut f32) -> bool,
    /// the unit's complex unit is a gate: how open it is into `open`
    pub gate_open: extern "C" fn(context: *mut c_void, open: *mut f32) -> bool,
    /// the unit's complex unit's player's colour
    pub player_color: extern "C" fn(context: *mut c_void) -> EeGfxColor,
    /// the damage vent's tag point lists (kinds 5 and 0x38 of the unit's model at its animation), kept for the call
    pub vent_lists: extern "C" fn(context: *mut c_void),
    /// the damage vent's unit: its command is to die, its maximum hit points
    pub vent_unit: extern "C" fn(context: *mut c_void) -> EeGfxVentUnit,
    /// the number of vents (kind 5 tag points; after `vent_lists`)
    pub vent_count: extern "C" fn(context: *mut c_void) -> u32,
    /// vent `tag`'s and its aim's offsets at frame 0 into `points[0]` and `points[1]` (after `vent_lists`)
    pub vent_offsets: extern "C" fn(context: *mut c_void, tag: i32, points: *mut Ge3DPoint),
    /// vent `tag`'s places on the unit (after `vent_lists`)
    pub vent_points: extern "C" fn(context: *mut c_void, tag: i32) -> EeGfxVentPoints,
    /// the kept graphic object's animation `index`
    pub frames: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxFrames,
    /// the unit's class's scale (`GetClass()->GetScale()`)
    pub unit_class_scale: extern "C" fn(context: *mut c_void) -> f32,
    /// the graphic object is the graphic effect's; its `LoadMedia`, then the media of the one it replaces unloaded
    pub reload_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the unit's position, raised by its complex unit's effect height while on the map and alive (the origin
    /// without a unit)
    pub unit_raised_position: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the unit is an unbuilt market (a complex unit of type market, not built, not a foundation)
    pub market_unbuilt: extern "C" fn(context: *mut c_void) -> bool,
    /// push the effect into render queue `index` under the graphic effect's object's texture of the preferences'
    /// screen (none without an object)
    pub queue_screen: extern "C" fn(context: *mut c_void, index: u32),
    /// the tracer beam's ends (`EEE3DTracerBeam::Aim`, 0xfc5a4, before its `beam.rs` part): the shooter's weapon tag
    /// point (or its tag point of kind 6, or its stats' default point) and the target point (on the target unit's
    /// location, or the fixed point); whether the beam has a target unit
    pub tracer_ends: extern "C" fn(context: *mut c_void) -> EeGfxTracerEnds,
    /// the tracer beam's impact: the linked effect 1 placed at the target point on the target unit (or at the fixed
    /// point); the placement's result
    pub tracer_impact: extern "C" fn(context: *mut c_void) -> i32,
    /// the tracer beam's hit: the target unit's class's `UnitDamaged`
    pub tracer_hit: extern "C" fn(context: *mut c_void),
    /// the tracer beam's target as its saved id (`Load`: `RepairPointers` resolves it)
    pub tracer_saved_target: extern "C" fn(context: *mut c_void, id: u32),
    /// the tracer beam's target unit's id, 0xffffffff for none (`Save`)
    pub tracer_target_id: extern "C" fn(context: *mut c_void) -> u32,
    /// the volcano's `BuildModel` (0xfe79d) over the record's mesh lists (`count` points and heights): the image's
    /// colours and cone heights and the mesh (`volcano.rs`), the model made and referenced, the centre's height, the
    /// placement, the radius and the texture; its result
    pub volcano_build:
        extern "C" fn(context: *mut c_void, points: *mut Ge3DPoint, heights: *mut f32, count: u32) -> i32,
    /// the volcano's model
    pub volcano_model: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// the farm's unit: its tile, size, built state and crop variant (`mCloaked`)
    pub farm_unit: extern "C" fn(context: *mut c_void) -> EeGfxFarmUnit,
    /// the terrain the farm's meshes follow (`gWorld.mMap->mTerrainMesh`)
    pub farm_terrain: extern "C" fn(context: *mut c_void) -> EeGfxFarmTerrain,
    /// the tile of the farm's first drop site (`EEGetFarmDropSites`); false when it has none
    pub farm_drop_site: extern "C" fn(context: *mut c_void, x: *mut i32, y: *mut i32) -> bool,
    /// the farm unit's crop variant stored (`mCloaked`)
    pub farm_set_variant: extern "C" fn(context: *mut c_void, variant: i32),
    /// the graphic object's `mLoops[index]`
    pub farm_loops: extern "C" fn(context: *mut c_void, index: u32) -> bool,
    /// any player holds a doppelganger of the farm on its tile (`HasDoppelganger(tile, unit)`)
    pub farm_doppelganger_any: extern "C" fn(context: *mut c_void) -> bool,
    /// the current player's doppelganger of the farm is drawn (none, a free camera or a player who sees all: -1),
    /// built (1) or not (0)
    pub farm_doppelganger: extern "C" fn(context: *mut c_void) -> i32,
    /// the unit's draw opacity from `opacity` (`Fn23CEE2(unit, false, opacity)`)
    pub farm_opacity: extern "C" fn(context: *mut c_void, opacity: f32) -> f32,
    /// `SetScaledModel(model)` of the farm unit
    pub set_scaled_model: extern "C" fn(context: *mut c_void, model: EeGfxModel),
    /// the point on the ground under `point` (`gWorld.mMap->GetGroundPoint`; the earthquake's)
    pub ground_point: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> Ge3DPoint,
    /// a smoke generator's unit (the damage smoke's tag points of kind 0 read for the step's `smoke_tag_point`)
    pub smoke_unit: extern "C" fn(context: *mut c_void) -> EeGfxSmokeUnit,
    /// the damage smoke's tag point `index` at its unit's frame in its scaled location (after `smoke_unit`)
    pub smoke_tag_point: extern "C" fn(context: *mut c_void, index: u32) -> Ge3DPoint,
    /// the linked effect 1 placed at `point`; the placement's result
    pub smoke_place: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> i32,
    /// the weather storms' map (`effect_weather.rs`; its own context)
    pub storm: EeGfxStormHost,
    /// the rain's three sound objects' media loaded (the record's `storm.sounds`)
    pub rain_media: extern "C" fn(context: *mut c_void),
    /// the rain's sounds: the loop started (`start`), the thunder `rumble` (1 or 2: the record's `storm.sounds[rumble]`)
    /// played, then both levels from the camera's distance (`EEE3DRain::StartSounds` after its timer, 0xf8195)
    pub rain_sounds: extern "C" fn(context: *mut c_void, start: bool, rumble: i32),
    /// the particles' tick facts at `position` (`EEParticleTick`: the game clock, the wind, the graphic effect's)
    pub particle_tick: extern "C" fn(context: *mut c_void, position: *const Ge3DPoint) -> EeGfxParticleTick,
    /// the terrain the particles bounce off (`EEParticleGround`)
    pub particle_ground: extern "C" fn(context: *mut c_void) -> EeGfxParticleGround,
    /// the graphic effect's duration (ms; the particle's load)
    pub effect_duration: extern "C" fn(context: *mut c_void) -> u32,
    /// the emitter placed on its unit (`EEE3DParticleEmitter`'s unit constructor, 0xf5f23)
    pub emitter_place: extern "C" fn(context: *mut c_void),
    /// the emitter's unit for its step (`EeGfxEmitterUnit`)
    pub emitter_unit: extern "C" fn(context: *mut c_void) -> EeGfxEmitterUnit,
    /// the emitter's animation sound at `location` (`first`: its first step)
    pub emitter_sound: extern "C" fn(context: *mut c_void, location: *const Ge3DPoint, first: bool),
    /// a line effect of the linked effect 1 from `start` along `direction`; the creation's result
    pub emitter_line: extern "C" fn(context: *mut c_void, start: *const Ge3DPoint, direction: *const Ge3DPoint) -> i32,
    /// `EEEffect::Queue` (the base effect's, not the 3D one's)
    pub effect_queue: extern "C" fn(context: *mut c_void) -> i32,
    /// the marching line's graphic object: its graphic effect's, its media loaded
    pub march_media: extern "C" fn(context: *mut c_void),
    /// the marching line's ends for the draw (its end units' positions, else `from` and `to`) into `out` (two)
    pub march_ends:
        extern "C" fn(context: *mut c_void, from: *const Ge3DPoint, to: *const Ge3DPoint, out: *mut Ge3DPoint),
    /// a loaded end unit's id (0 the start, 1 the end; resolved by `RepairPointers`)
    pub march_set_unit: extern "C" fn(context: *mut c_void, which: u32, id: u32),
    /// an end unit's id for a save (-1: none)
    pub march_unit_id: extern "C" fn(context: *mut c_void, which: u32) -> i32,
}

/// The world as the effects ask it.
pub trait Effect3DHost {
    fn unit_position(&mut self) -> Ge3DPoint;
    fn model_height(&mut self) -> f32;
    fn move_to(&mut self);
    fn create_line(&mut self, start: &Ge3DPoint, direction: &GeVector, linked: u32);
    fn base_queue(&mut self) -> i32;
    fn queue(&mut self, index: u32);
    fn load_media(&mut self) -> i32;
    fn model(&mut self) -> EeGfxModel;
    fn texture(&mut self) -> EeGfxModel;
    fn follow_unit(&mut self);
    fn model_at(&mut self, index: u32) -> EeGfxModel;
    fn shot_hit(&mut self);
    fn beam_start(&mut self) -> Ge3DPoint;
    fn beam_unit(&mut self) -> EeGfxBeamUnit;
    fn beam_hidden(&mut self) -> bool;
    fn place_linked(&mut self, point: &Ge3DPoint);
    /// the tests' hosts answer the unit's position
    fn complex_position(&mut self) -> Ge3DPoint {
        self.unit_position()
    }
    /// the tests' hosts that do not draw the additive pass answer nothing
    fn unit_model_media(&mut self) -> i32 {
        0
    }
    fn unit_scale(&mut self) -> f32 {
        0.0
    }
    fn unit_location(&mut self) -> GeTransformation {
        identity()
    }
    fn unit_model_radius(&mut self) -> f32 {
        0.0
    }
    fn unit_frame(&mut self) -> EeGfxUnitFrame {
        EeGfxUnitFrame::default()
    }
    fn unit_model_at(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    /// the tests' hosts that make no orbiting effect have no orbitals
    fn orbital_count(&mut self) -> u32 {
        0
    }
    fn orbital(&mut self, _index: u32) -> EeGfxOrbital {
        EeGfxOrbital { model: std::ptr::null_mut(), speed: 0.0, angle: 0.0 }
    }
    fn animation(&mut self) -> EeGfxAnimation {
        EeGfxAnimation::default()
    }
    fn animation_sound(&mut self, _frame: u32, _changed: bool) {}
    /// the tests' hosts that make no hurricane spawn nothing
    fn spawn_count(&mut self) -> u32 {
        0
    }
    fn spawn_at(&mut self, _index: u32, _frame: u32) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn spawn(&mut self, _point: &Ge3DPoint) -> i32 {
        0
    }
    /// the tests' hosts that draw no explosion or wave answer nothing
    fn texture_at(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn unit_tile(&mut self) -> EeGfxUnitTile {
        EeGfxUnitTile::default()
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn unit_hidden(&mut self) -> bool {
        false
    }
    fn space_tile(&mut self, _x: u32, _y: u32) -> bool {
        false
    }
    /// the tests' hosts that make no multi-effect make nothing
    fn create_linked(&mut self, _index: u32) -> i32 {
        0
    }
    /// the tests' hosts that make no energy wall stand on no gate and find no wall
    fn wall_gate_posts(&mut self) -> Option<(Ge3DPoint, Ge3DPoint)> {
        None
    }
    fn wall_piece(&mut self, _x: i32, _y: i32) -> Option<f32> {
        None
    }
    fn gate_open(&mut self) -> Option<f32> {
        None
    }
    fn player_color(&mut self) -> EeGfxColor {
        EeGfxColor::default()
    }
    /// the tests' hosts that make no damage vent have no vents
    fn vent_lists(&mut self) {}
    fn vent_unit(&mut self) -> EeGfxVentUnit {
        EeGfxVentUnit::default()
    }
    fn vent_count(&mut self) -> u32 {
        0
    }
    fn vent_offsets(&mut self, _tag: i32) -> (Ge3DPoint, Ge3DPoint) {
        (Ge3DPoint::default(), Ge3DPoint::default())
    }
    fn vent_points(&mut self, _tag: i32) -> EeGfxVentPoints {
        EeGfxVentPoints::default()
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        EeGfxFrames::default()
    }
    fn unit_class_scale(&mut self) -> f32 {
        0.0
    }
    /// the tests' hosts that make no icon load nothing more and stand at the origin
    fn reload_media(&mut self) -> i32 {
        0
    }
    fn unit_raised_position(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn market_unbuilt(&mut self) -> bool {
        false
    }
    fn queue_screen(&mut self, _index: u32) {}
    /// the tests' hosts that make no tracer beam aim from the origin to the origin, without a target
    fn tracer_ends(&mut self) -> EeGfxTracerEnds {
        EeGfxTracerEnds::default()
    }
    fn tracer_impact(&mut self) -> i32 {
        0
    }
    fn tracer_hit(&mut self) {}
    fn tracer_saved_target(&mut self, _id: u32) {}
    fn tracer_target_id(&mut self) -> u32 {
        0xffff_ffff
    }
    /// the tests' hosts that make no volcano build nothing
    fn volcano_build(&mut self, _points: *mut Ge3DPoint, _heights: *mut f32, _count: u32) -> i32 {
        0
    }
    fn volcano_model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn farm_unit(&mut self) -> EeGfxFarmUnit {
        EeGfxFarmUnit::default()
    }
    fn farm_terrain(&mut self) -> EeGfxFarmTerrain {
        crate::effect_farm::flat_terrain()
    }
    fn farm_drop_site(&mut self, _x: &mut i32, _y: &mut i32) -> bool {
        false
    }
    fn farm_set_variant(&mut self, _variant: i32) {}
    fn farm_loops(&mut self, _index: u32) -> bool {
        false
    }
    fn farm_doppelganger_any(&mut self) -> bool {
        false
    }
    fn farm_doppelganger(&mut self) -> i32 {
        -1
    }
    fn farm_opacity(&mut self, opacity: f32) -> f32 {
        opacity
    }
    fn set_scaled_model(&mut self, _model: EeGfxModel) {}
    fn ground_point(&mut self, point: &Ge3DPoint) -> Ge3DPoint {
        *point
    }
    fn smoke_unit(&mut self) -> EeGfxSmokeUnit {
        EeGfxSmokeUnit::default()
    }
    fn smoke_tag_point(&mut self, _index: u32) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn smoke_place(&mut self, _point: &Ge3DPoint) -> i32 {
        0
    }
    /// the weather storms' map (the tests' hosts that make no storm: no tile is on it)
    fn storm_on_map(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn storm_is_water(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn storm_ground_z(&mut self, _x: i32, _y: i32) -> f32 {
        0.0
    }
    fn storm_step_toward(&mut self, from: [i32; 2], _to: [i32; 2]) -> [i32; 2] {
        from
    }
    fn storm_ripple(&mut self, _x: f32, _y: f32) {}
    fn rain_media(&mut self) {}
    fn rain_sounds(&mut self, _start: bool, _rumble: i32) {}
    /// the particles' world (the tests' hosts that make none: the clock at 0, flat ground everywhere)
    fn particle_tick(&mut self, position: &Ge3DPoint) -> EeGfxParticleTick {
        EeGfxParticleTick { position: *position, ..Default::default() }
    }
    fn particle_ground(&mut self) -> EeGfxParticleGround {
        crate::ffi_effect::flat_ground()
    }
    fn effect_duration(&mut self) -> u32 {
        0
    }
    fn emitter_place(&mut self) {}
    fn emitter_unit(&mut self) -> EeGfxEmitterUnit {
        EeGfxEmitterUnit::default()
    }
    fn emitter_sound(&mut self, _location: &Ge3DPoint, _first: bool) {}
    fn emitter_line(&mut self, _start: &Ge3DPoint, _direction: &Ge3DPoint) -> i32 {
        0
    }
    fn effect_queue(&mut self) -> i32 {
        self.base_queue()
    }
    fn march_media(&mut self) {}
    fn march_ends(&mut self, from: &Ge3DPoint, to: &Ge3DPoint) -> [Ge3DPoint; 2] {
        [*from, *to]
    }
    fn march_set_unit(&mut self, _which: u32, _id: u32) {}
    fn march_unit_id(&mut self, _which: u32) -> i32 {
        -1
    }
}

impl Effect3DHost for &EeGfxEffect3DHost {
    fn unit_position(&mut self) -> Ge3DPoint {
        (self.unit_position)(self.context)
    }
    fn model_height(&mut self) -> f32 {
        (self.model_height)(self.context)
    }
    fn move_to(&mut self) {
        (self.move_to)(self.context)
    }
    fn create_line(&mut self, start: &Ge3DPoint, direction: &GeVector, linked: u32) {
        (self.create_line)(self.context, start, direction, linked)
    }
    fn base_queue(&mut self) -> i32 {
        (self.base_queue)(self.context)
    }
    fn queue(&mut self, index: u32) {
        (self.queue)(self.context, index)
    }
    fn load_media(&mut self) -> i32 {
        (self.load_media)(self.context)
    }
    fn model(&mut self) -> EeGfxModel {
        (self.model)(self.context)
    }
    fn texture(&mut self) -> EeGfxModel {
        (self.texture)(self.context)
    }
    fn follow_unit(&mut self) {
        (self.follow_unit)(self.context)
    }
    fn model_at(&mut self, index: u32) -> EeGfxModel {
        (self.model_at)(self.context, index)
    }
    fn shot_hit(&mut self) {
        (self.shot_hit)(self.context)
    }
    fn beam_start(&mut self) -> Ge3DPoint {
        (self.beam_start)(self.context)
    }
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        (self.beam_unit)(self.context)
    }
    fn beam_hidden(&mut self) -> bool {
        (self.beam_hidden)(self.context)
    }
    fn place_linked(&mut self, point: &Ge3DPoint) {
        (self.place_linked)(self.context, point)
    }
    fn complex_position(&mut self) -> Ge3DPoint {
        (self.complex_position)(self.context)
    }
    fn unit_model_media(&mut self) -> i32 {
        (self.unit_model_media)(self.context)
    }
    fn unit_scale(&mut self) -> f32 {
        (self.unit_scale)(self.context)
    }
    fn unit_location(&mut self) -> GeTransformation {
        (self.unit_location)(self.context)
    }
    fn unit_model_radius(&mut self) -> f32 {
        (self.unit_model_radius)(self.context)
    }
    fn unit_frame(&mut self) -> EeGfxUnitFrame {
        (self.unit_frame)(self.context)
    }
    fn unit_model_at(&mut self, index: u32) -> EeGfxModel {
        (self.unit_model_at)(self.context, index)
    }
    fn orbital_count(&mut self) -> u32 {
        (self.orbital_count)(self.context)
    }
    fn orbital(&mut self, index: u32) -> EeGfxOrbital {
        (self.orbital)(self.context, index)
    }
    fn animation(&mut self) -> EeGfxAnimation {
        (self.animation)(self.context)
    }
    fn animation_sound(&mut self, frame: u32, changed: bool) {
        (self.animation_sound)(self.context, frame, changed)
    }
    fn spawn_count(&mut self) -> u32 {
        (self.spawn_count)(self.context)
    }
    fn spawn_at(&mut self, index: u32, frame: u32) -> Ge3DPoint {
        (self.spawn_at)(self.context, index, frame)
    }
    fn spawn(&mut self, point: &Ge3DPoint) -> i32 {
        (self.spawn)(self.context, point)
    }
    fn texture_at(&mut self, index: u32) -> EeGfxModel {
        (self.texture_at)(self.context, index)
    }
    fn unit_tile(&mut self) -> EeGfxUnitTile {
        (self.unit_tile)(self.context)
    }
    fn terrain_z(&mut self, x: f32, y: f32) -> f32 {
        (self.terrain_z)(self.context, x, y)
    }
    fn unit_hidden(&mut self) -> bool {
        (self.unit_hidden)(self.context)
    }
    fn space_tile(&mut self, x: u32, y: u32) -> bool {
        (self.space_tile)(self.context, x, y)
    }
    fn create_linked(&mut self, index: u32) -> i32 {
        (self.create_linked)(self.context, index)
    }
    fn wall_gate_posts(&mut self) -> Option<(Ge3DPoint, Ge3DPoint)> {
        let (mut first, mut second) = (Ge3DPoint::default(), Ge3DPoint::default());
        (self.wall_gate_posts)(self.context, &mut first, &mut second).then_some((first, second))
    }
    fn wall_piece(&mut self, x: i32, y: i32) -> Option<f32> {
        let mut z = 0.0;
        (self.wall_piece)(self.context, x, y, &mut z).then_some(z)
    }
    fn gate_open(&mut self) -> Option<f32> {
        let mut open = 0.0;
        (self.gate_open)(self.context, &mut open).then_some(open)
    }
    fn player_color(&mut self) -> EeGfxColor {
        (self.player_color)(self.context)
    }
    fn vent_lists(&mut self) {
        (self.vent_lists)(self.context)
    }
    fn vent_unit(&mut self) -> EeGfxVentUnit {
        (self.vent_unit)(self.context)
    }
    fn vent_count(&mut self) -> u32 {
        (self.vent_count)(self.context)
    }
    fn vent_offsets(&mut self, tag: i32) -> (Ge3DPoint, Ge3DPoint) {
        let mut points = [Ge3DPoint::default(); 2];
        (self.vent_offsets)(self.context, tag, points.as_mut_ptr());
        (points[0], points[1])
    }
    fn vent_points(&mut self, tag: i32) -> EeGfxVentPoints {
        (self.vent_points)(self.context, tag)
    }
    fn frames(&mut self, index: u32) -> EeGfxFrames {
        (self.frames)(self.context, index)
    }
    fn unit_class_scale(&mut self) -> f32 {
        (self.unit_class_scale)(self.context)
    }
    fn reload_media(&mut self) -> i32 {
        (self.reload_media)(self.context)
    }
    fn unit_raised_position(&mut self) -> Ge3DPoint {
        (self.unit_raised_position)(self.context)
    }
    fn market_unbuilt(&mut self) -> bool {
        (self.market_unbuilt)(self.context)
    }
    fn queue_screen(&mut self, index: u32) {
        (self.queue_screen)(self.context, index)
    }
    fn tracer_ends(&mut self) -> EeGfxTracerEnds {
        (self.tracer_ends)(self.context)
    }
    fn tracer_impact(&mut self) -> i32 {
        (self.tracer_impact)(self.context)
    }
    fn tracer_hit(&mut self) {
        (self.tracer_hit)(self.context)
    }
    fn tracer_saved_target(&mut self, id: u32) {
        (self.tracer_saved_target)(self.context, id)
    }
    fn tracer_target_id(&mut self) -> u32 {
        (self.tracer_target_id)(self.context)
    }
    fn volcano_build(&mut self, points: *mut Ge3DPoint, heights: *mut f32, count: u32) -> i32 {
        (self.volcano_build)(self.context, points, heights, count)
    }
    fn volcano_model(&mut self) -> EeGfxModel {
        (self.volcano_model)(self.context)
    }
    fn farm_unit(&mut self) -> EeGfxFarmUnit {
        (self.farm_unit)(self.context)
    }
    fn farm_terrain(&mut self) -> EeGfxFarmTerrain {
        (self.farm_terrain)(self.context)
    }
    fn farm_drop_site(&mut self, x: &mut i32, y: &mut i32) -> bool {
        (self.farm_drop_site)(self.context, x, y)
    }
    fn farm_set_variant(&mut self, variant: i32) {
        (self.farm_set_variant)(self.context, variant)
    }
    fn farm_loops(&mut self, index: u32) -> bool {
        (self.farm_loops)(self.context, index)
    }
    fn farm_doppelganger_any(&mut self) -> bool {
        (self.farm_doppelganger_any)(self.context)
    }
    fn farm_doppelganger(&mut self) -> i32 {
        (self.farm_doppelganger)(self.context)
    }
    fn farm_opacity(&mut self, opacity: f32) -> f32 {
        (self.farm_opacity)(self.context, opacity)
    }
    fn set_scaled_model(&mut self, model: EeGfxModel) {
        (self.set_scaled_model)(self.context, model)
    }
    fn ground_point(&mut self, point: &Ge3DPoint) -> Ge3DPoint {
        (self.ground_point)(self.context, point)
    }
    fn smoke_unit(&mut self) -> EeGfxSmokeUnit {
        (self.smoke_unit)(self.context)
    }
    fn smoke_tag_point(&mut self, index: u32) -> Ge3DPoint {
        (self.smoke_tag_point)(self.context, index)
    }
    fn smoke_place(&mut self, point: &Ge3DPoint) -> i32 {
        (self.smoke_place)(self.context, point)
    }
    fn storm_on_map(&mut self, x: i32, y: i32) -> bool {
        (self.storm.on_map)(self.storm.context, x, y)
    }
    fn storm_is_water(&mut self, x: i32, y: i32) -> bool {
        (self.storm.is_water)(self.storm.context, x, y)
    }
    fn storm_ground_z(&mut self, x: i32, y: i32) -> f32 {
        (self.storm.ground_z)(self.storm.context, x, y)
    }
    fn storm_step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2] {
        let mut out = from;
        (self.storm.step_toward)(self.storm.context, from[0], from[1], to[0], to[1], out.as_mut_ptr());
        out
    }
    fn storm_ripple(&mut self, x: f32, y: f32) {
        (self.storm.ripple)(self.storm.context, x, y)
    }
    fn rain_media(&mut self) {
        (self.rain_media)(self.context)
    }
    fn rain_sounds(&mut self, start: bool, rumble: i32) {
        (self.rain_sounds)(self.context, start, rumble)
    }
    fn particle_tick(&mut self, position: &Ge3DPoint) -> EeGfxParticleTick {
        (self.particle_tick)(self.context, position)
    }
    fn particle_ground(&mut self) -> EeGfxParticleGround {
        (self.particle_ground)(self.context)
    }
    fn effect_duration(&mut self) -> u32 {
        (self.effect_duration)(self.context)
    }
    fn emitter_place(&mut self) {
        (self.emitter_place)(self.context)
    }
    fn emitter_unit(&mut self) -> EeGfxEmitterUnit {
        (self.emitter_unit)(self.context)
    }
    fn emitter_sound(&mut self, location: &Ge3DPoint, first: bool) {
        (self.emitter_sound)(self.context, location, first)
    }
    fn emitter_line(&mut self, start: &Ge3DPoint, direction: &Ge3DPoint) -> i32 {
        (self.emitter_line)(self.context, start, direction)
    }
    fn effect_queue(&mut self) -> i32 {
        (self.effect_queue)(self.context)
    }
    fn march_media(&mut self) {
        (self.march_media)(self.context)
    }
    fn march_ends(&mut self, from: &Ge3DPoint, to: &Ge3DPoint) -> [Ge3DPoint; 2] {
        let mut out = [Ge3DPoint::default(); 2];
        (self.march_ends)(self.context, from, to, out.as_mut_ptr());
        out
    }
    fn march_set_unit(&mut self, which: u32, id: u32) {
        (self.march_set_unit)(self.context, which, id)
    }
    fn march_unit_id(&mut self, which: u32) -> i32 {
        (self.march_unit_id)(self.context, which)
    }
}

/// One virtual of an `EEE3DRust` of the record's kind.
///
/// # Safety
/// `r` is a live record, read and written only through this pointer during the call (the C++ reads it in the host
/// calls).
pub unsafe fn effect3d<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    let kind = unsafe { (*r).kind };
    match kind {
        EE_GFX_EFFECT_KIND_TELEPORT | EE_GFX_EFFECT_KIND_TIME_WARP => unsafe { rising(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_BLAST_WAVE | EE_GFX_EFFECT_KIND_SHIELD => unsafe { follow(op, r, g, h, x) },
        k if is_shot(k) => unsafe { shot(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_CITIZEN_BEAM => unsafe { beam(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_WATER_SPLASH | EE_GFX_EFFECT_KIND_SCREEN_FLASH => unsafe { aged(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_ADDITIVE_PASS => unsafe { additive(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE | EE_GFX_EFFECT_KIND_DISEASE | EE_GFX_EFFECT_KIND_VIRUS => unsafe {
            orbit(op, r, g, h, x)
        },
        EE_GFX_EFFECT_KIND_HURRICANE => unsafe { hurricane(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION | EE_GFX_EFFECT_KIND_ENERGY_WAVE => unsafe { blast(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_MULTI_EFFECT => unsafe { multi(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_ENERGY_WALL => unsafe { wall(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_DAMAGE_VENT => unsafe { damage_vent(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_ICON | EE_GFX_EFFECT_KIND_GOAL => unsafe { icon(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_TRACER_BEAM => unsafe { tracer(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_VOLCANO => unsafe { volcano(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_FARM => unsafe { farm_effect(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_EARTHQUAKE => unsafe { quake(op, r, g, h, x) },
        k if is_smoke(k) => unsafe { smoke(op, r, g, h) },
        k if is_weather(k) => unsafe { weather(op, r, g, h, x) },
        k if is_particle(k) => unsafe { particles(op, r, g, h, x) },
        _ => 0,
    }
}

/// `Load` (`op` Load) or `Save` of an `EEE3DRust` after `EEEffect3D`'s, whose result is `base`; the file versions.
pub fn effect3d_file<H: Effect3DHost, F: EffectFile>(
    op: EeGfxEffect3DOp,
    base: i32,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    let load = op == EeGfxEffect3DOp::Load;
    match r.kind {
        EE_GFX_EFFECT_KIND_SHIELD => {
            // the base's and the media's results are ignored
            if load {
                h.load_media();
                load_shield(r, f, major, minor)
            } else {
                save_shield(r, f)
            }
        }
        k if is_shot(k) => {
            // the base's and the media's results are ignored
            if load {
                h.load_media();
                load_shot(r, f)
            } else {
                save_shot(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_CITIZEN_BEAM => {
            // the base's result is ignored on loading; the media's stops it
            if load {
                let media = h.load_media();
                load_beam(r, f, media)
            } else {
                save_beam(r, f, base)
            }
        }
        EE_GFX_EFFECT_KIND_WATER_SPLASH | EE_GFX_EFFECT_KIND_SCREEN_FLASH => {
            // the base's and the media's results are ignored
            if load {
                h.load_media();
                load_aged(r, f)
            } else {
                save_aged(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_ADDITIVE_PASS => {
            // the base's error stops it; the load's result is the media's
            if base != 0 || !load {
                base
            } else {
                h.load_media()
            }
        }
        EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE | EE_GFX_EFFECT_KIND_DISEASE | EE_GFX_EFFECT_KIND_VIRUS => {
            // the base's result is ignored
            if load {
                // SAFETY: `r` is the only reference to the record during the call
                unsafe { load_orbit(r, f, h) }
            } else {
                save_orbit(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_HURRICANE => {
            // the base's result is ignored
            if load {
                // SAFETY: `r` is the only reference to the record during the call
                unsafe { load_hurricane(r, f, h) }
            } else {
                save_hurricane(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION | EE_GFX_EFFECT_KIND_ENERGY_WAVE => {
            // the base's result is ignored
            if load {
                // SAFETY: `r` is the only reference to the record during the call
                unsafe { load_blast(r, f, h) }
            } else {
                save_blast(r, f)
            }
        }
        // the base's alone
        EE_GFX_EFFECT_KIND_MULTI_EFFECT => base,
        EE_GFX_EFFECT_KIND_ENERGY_WALL => wall_file(load, base, r, f, major, minor, h),
        // the base's result ignored on loading: the media's
        EE_GFX_EFFECT_KIND_DAMAGE_VENT => {
            if load {
                h.load_media()
            } else {
                base
            }
        }
        // the base's error stops it
        EE_GFX_EFFECT_KIND_ICON | EE_GFX_EFFECT_KIND_GOAL => {
            if base != 0 {
                base
            } else if load {
                load_icon(r, f)
            } else {
                save_icon(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_TRACER_BEAM => tracer_file(load, base, r, f, major, minor, h),
        // the base's result is ignored
        EE_GFX_EFFECT_KIND_VOLCANO => volcano_file(load, r, f, h),
        // the base's result is ignored
        EE_GFX_EFFECT_KIND_FARM => farm_file(load, r, f, h),
        // the base's result is ignored
        EE_GFX_EFFECT_KIND_EARTHQUAKE => quake_file(load, r, f, h),
        // the base's result is ignored
        k if is_smoke(k) => {
            if load {
                crate::effect_state::load_smoke(r, f)
            } else {
                crate::effect_state::save_smoke(r, f)
            }
        }
        k if is_weather(k) => weather_file(load, base, r, f),
        k if is_particle(k) => particles_file(load, base, r, f, major, minor, h),
        EE_GFX_EFFECT_KIND_BLAST_WAVE if load => {
            // the base's result is ignored; the media's stops the load
            let res = h.load_media();
            if res != 0 {
                res
            } else {
                load_aged(r, f)
            }
        }
        _ if base != 0 => base,
        _ if load => load_aged(r, f),
        _ => save_aged(r, f),
    }
}

/// The teleport's and time warp's virtuals.
///
/// # Safety
/// As `effect3d`.
unsafe fn rising<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    let warp = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_TIME_WARP;
    match op {
        EeGfxEffect3DOp::Create => {
            if warp {
                // the warp's age starts at zero (the original left it unset)
                unsafe { (*r).aged.age = 0.0 };
            }
            let mut point = h.unit_position();
            let mut t = identity();
            t.m[0][3] = point.x;
            t.m[1][3] = point.y;
            t.m[2][3] = point.z;
            let height = h.model_height() * g.size;
            if g.flag1 {
                t.m[2][3] = if warp { point.z - TIME_WARP_DROP } else { point.z };
                point.z = point.z + height;
            } else {
                t.m[2][3] = point.z - height;
            }
            unsafe { (*r).transformation = t };
            h.move_to();
            unsafe { (*r).bounding_radius = g.size };
            let direction = GeVector { x: 0.0, y: 0.0, z: if g.flag1 { -g.param1 } else { g.param1 } };
            h.create_line(&point, &direction, 1);
            h.create_line(&point, &direction, 2);
            0
        }
        EeGfxEffect3DOp::Repair => h.load_media(),
        EeGfxEffect3DOp::SetUp => {
            h.load_media();
            if !warp {
                unsafe { (*r).aged.age = 0.0 };
            }
            unsafe { (*r).bounding_radius = g.size };
            0
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    (*r).aged.draw_age = (*r).aged.age;
                    let t = (*r).transformation;
                    (*r).aged.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                }
                h.queue(2);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass4 => {
            let model = h.model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            let ambient = x.get_ambient();
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            x.set_alpha_testing(0);
            x.model_opacity(model, 0.0);
            let (location, mut t) = unsafe { ((*r).aged.location, (*r).transformation) };
            x.fog_factor(&location, fog);
            t.m[0][3] = location.x;
            t.m[1][3] = location.y;
            t.m[2][3] = location.z;
            x.model_transform(model, &t);
            x.model_scale(model, g.size);
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.set_ambient(ambient);
            x.set_fog(fog);
            x.set_alpha_testing(1);
            result
        }
        EeGfxEffect3DOp::Step => {
            let tick = EeGfxTimedTick {
                tick_seconds: g.tick_seconds,
                tick_time: g.tick_time,
                duration: g.duration,
                speed: if warp { g.param1 } else { g.param2 },
                sinking: g.flag1,
            };
            let mut local = unsafe { r.read() };
            let result = timed_step(&mut local, &tick);
            unsafe { r.write(local) };
            if result == EE_GFX_EFFECT_MOVED {
                h.move_to();
                0
            } else {
                result
            }
        }
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::raster::LogRaster;

    #[derive(Default)]
    struct Host {
        calls: Vec<String>,
        queue_result: i32,
    }

    impl Effect3DHost for Host {
        fn unit_position(&mut self) -> Ge3DPoint {
            Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 }
        }
        fn model_height(&mut self) -> f32 {
            2.0
        }
        fn move_to(&mut self) {
            self.calls.push("move".into());
        }
        fn create_line(&mut self, s: &Ge3DPoint, d: &GeVector, linked: u32) {
            self.calls.push(format!("line {} {} {} {} {linked}", s.x, s.y, s.z, d.z));
        }
        fn base_queue(&mut self) -> i32 {
            self.queue_result
        }
        fn queue(&mut self, index: u32) {
            self.calls.push(format!("queue {index}"));
        }
        fn load_media(&mut self) -> i32 {
            self.calls.push("media".into());
            0
        }
        fn model(&mut self) -> EeGfxModel {
            std::ptr::null_mut()
        }
        fn texture(&mut self) -> EeGfxModel {
            std::ptr::null_mut()
        }
        fn follow_unit(&mut self) {
            self.calls.push("follow".into());
        }
        fn model_at(&mut self, _index: u32) -> EeGfxModel {
            std::ptr::null_mut()
        }
        fn shot_hit(&mut self) {
            self.calls.push("hit".into());
        }
        fn beam_start(&mut self) -> Ge3DPoint {
            Ge3DPoint::default()
        }
        fn beam_unit(&mut self) -> EeGfxBeamUnit {
            EeGfxBeamUnit::default()
        }
        fn beam_hidden(&mut self) -> bool {
            false
        }
        fn place_linked(&mut self, _point: &Ge3DPoint) {}
    }

    fn facts() -> EeGfxGraphicFacts {
        EeGfxGraphicFacts {
            size: 1.5,
            param1: 4.0,
            param2: 8.0,
            flag1: true,
            duration: 1000,
            has_object: true,
            alpha_ref: 0.25,
            tick_seconds: 0.125,
            tick_time: 125.0,
            ..Default::default()
        }
    }

    fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
        let mut x = LogRaster { fog: 1, ..Default::default() };
        // SAFETY: a live record, not otherwise borrowed during the call
        let result = unsafe { effect3d(op, r, g, h, &mut x) };
        (result, x)
    }

    #[test]
    fn made_on_a_unit_they_launch_two_lines_from_the_far_end() {
        let mut h = Host::default();
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TIME_WARP);
        w.aged.age = 7.0;
        run(EeGfxEffect3DOp::Create, &mut w, &facts(), &mut h);
        // rising: the warp starts 0.2 below the unit, the lines go down from the model's top
        assert_eq!((w.transformation.m[0][3], w.transformation.m[2][3]), (10.0, 3.0 - 0.2));
        assert_eq!((w.aged.age, w.bounding_radius), (0.0, 1.5));
        assert_eq!(h.calls, ["move", "line 10 20 6 -4 1", "line 10 20 6 -4 2"]);
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        let mut h = Host::default();
        run(EeGfxEffect3DOp::Create, &mut p, &EeGfxGraphicFacts { flag1: false, ..facts() }, &mut h);
        // sinking: the model starts one height below the unit, the lines go up from the unit
        assert_eq!(p.transformation.m[2][3], 0.0);
        assert_eq!(h.calls[1], "line 10 20 3 4 1");
    }

    #[test]
    fn set_up_queue_and_step() {
        let mut h = Host::default();
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        p.aged.age = 5.0;
        run(EeGfxEffect3DOp::SetUp, &mut p, &facts(), &mut h);
        assert_eq!((p.aged.age, p.bounding_radius), (0.0, 1.5));
        p.transformation.m[1][3] = 9.0;
        p.aged.age = 3.0;
        assert_eq!(run(EeGfxEffect3DOp::Queue, &mut p, &facts(), &mut h).0, 0);
        assert_eq!((p.aged.draw_age, p.aged.location.y), (3.0, 9.0));
        h.queue_result = 10;
        assert_eq!(run(EeGfxEffect3DOp::Queue, &mut p, &facts(), &mut h).0, 10);
        assert_eq!(h.calls, ["media", "queue 2"]);
        // the teleport sinks (flag 1) by param 2; the step moves it through the host
        p.aged.age = 0.0;
        assert_eq!(run(EeGfxEffect3DOp::Step, &mut p, &facts(), &mut h).0, 0);
        assert_eq!((p.aged.age, p.transformation.m[2][3]), (125.0, -1.0));
        assert_eq!(h.calls.last().map(String::as_str), Some("move"));
        p.aged.age = 1000.0;
        assert_eq!(run(EeGfxEffect3DOp::Step, &mut p, &facts(), &mut h).0, 7);
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TIME_WARP);
        w.aged.age = 5.0;
        run(EeGfxEffect3DOp::SetUp, &mut w, &facts(), &mut h);
        assert_eq!(w.aged.age, 5.0);
        run(EeGfxEffect3DOp::Step, &mut w, &facts(), &mut h);
        assert_eq!(w.transformation.m[2][3], -0.5);
    }

    #[test]
    fn the_draw_masks_the_depth_and_restores_the_state() {
        let mut h = Host::default();
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        p.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        let (result, x) = run(EeGfxEffect3DOp::DrawPass4, &mut p, &facts(), &mut h);
        assert_eq!(result, 0);
        assert_eq!(
            x.calls,
            [
                "alpharef 0.25",
                "zbuffer 3",
                "blend 4 5",
                "shading 0",
                "lighting White",
                "alphatest 0",
                "opacity 0",
                "fogfactor 1 2 3 1",
                "transform 1 2 3",
                "scale 1.5",
                "prepare",
                "draw 1",
                "ambient 0 0 0",
                "fog 1",
                "alphatest 1"
            ]
        );
        for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass5, EeGfxEffect3DOp::DrawPass6] {
            let (result, x) = run(op, &mut p, &facts(), &mut h);
            assert_eq!((result, x.calls.len()), (0, 0));
        }
        let (_, x) =
            run(EeGfxEffect3DOp::DrawPass4, &mut p, &EeGfxGraphicFacts { has_object: false, ..facts() }, &mut h);
        assert_eq!(x.calls[0], "alpharef 0");
    }
}
