//! The 2D effects whole in Rust: `EEE2DRust` (`src/Empire Earth/Graphics Thread/EEE2DRust.cpp`), the twin of
//! `EEE3DRust` over `EEEffect2D`, calls these by the record's kind; the C++ answers the world through
//! `EeGfxEffect2DHost` and draws through the rasterizer table (`raster.rs`). Kinds: 0x41 the command confirmation
//! (`effect_confirm.rs`), 0x12 the shockwave (`effect_shockwave.rs`), the sprites of `effect_sprite.rs` (6, 7, 8, 9, 0xa,
//! 0xe, 0x22, 0x2f), of `effect_parts.rs` (3, 4, 5, 0xb, 0xc, 0x14), of `effect_trail.rs` (0x16, 0x30) and the vortex
//! (0x31, `effect_vortex.rs`); since the twenty-first round the marching icons (0xf, 0x10, `effect_icons.rs`), the light
//! glow (0xd, `effect_glow.rs`), the battlefield decal (0x32) and the pooled 2D particle (0x2e, `effect_decal.rs`).
//! The ops are `EEE3DRust`'s (`EeGfxEffect3DOp`), the facts the same `EeGfxGraphicFacts`.
//!
//! The record is reached through its raw pointer field by field: the C++ reads it (the position, the radius) during
//! the host calls, so no Rust reference to it lives across one.

use std::ffi::c_void;

use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_confirm::{
    confirm, EeGfxConfirmGoal, EeGfxConfirmUnit, EE_GFX_EFFECT_KIND_COMMAND_CONFIRMATION,
};
use crate::gfx::effect_decal::{decal, decal_file, EE_GFX_EFFECT_KIND_DECAL, EE_GFX_EFFECT_KIND_PARTICLE_2D};
use crate::gfx::effect_glow::{
    glow, glow_file, EeGfxGlowArea, EeGfxGlowPlace, EeGfxGlowSetUp, EE_GFX_EFFECT_KIND_LIGHT_GLOW,
};
use crate::gfx::effect_icons::{marching, marching_file, EeGfxIconUnit};
use crate::gfx::effect_orbit::EeGfxOrbital;
use crate::gfx::effect_parts::{parts, parts_file, EeGfx2DPart, EeGfx2DPlace};
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_shockwave::{shockwave, shockwave_file};
use crate::gfx::effect_sprite::{sprite, sprite_file, EeGfx2DPoint};
use crate::gfx::effect_state::{
    EeGfxEffectRecord, EeGfxParticleTick, EffectFile, EE_GFX_EFFECT_KIND_CIRCULAR_ICONS, EE_GFX_EFFECT_KIND_SHOCKWAVE,
    EE_GFX_EFFECT_KIND_SQUARE_ICONS,
};
use crate::gfx::effect_trail::{trail, trail_file};
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::effect_views;
use crate::gfx::effect_vortex::{vortex, vortex_file};
use crate::gfx::ffi_effect::EeGfxParticleGround;
use crate::gfx::raster::{EeGfxModel, Raster};

/// The C++'s answers for an `EEE2DRust`.
#[repr(C)]
pub struct EeGfxEffect2DHost {
    pub context: *mut c_void,
    /// the effect's unit (`mUnit`, an `EEUnit*`; 0: none, or its saved id before RepairPointers) and the
    /// current player (`gWorld.GetCurrentPlayer()`): ee_gfx effect_views.rs reads them through the world views
    pub unit: u64,
    pub player: u64,
    /// the base's queue: `EEEffect::Queue` for the confirmation (not `EEEffect2D`'s projection), else `EEEffect2D`'s
    pub base_queue: extern "C" fn(context: *mut c_void) -> i32,
    /// pushes the effect on render queue `index` under its graphic's texture for the current screen
    pub queue: extern "C" fn(context: *mut c_void, index: u32),
    /// the graphic effect's object (kept as the effect's) loaded; its result
    pub load_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the kept graphic object's animation `index`
    pub frames: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxFrames,
    /// `MoveToPoint(mPosition)` (the second half of `SetPosition`)
    pub move_to_point: extern "C" fn(context: *mut c_void),
    /// `EEEffect2D::mModel`
    pub model: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// the goal the confirmation shows (`EeGfxConfirmGoal`) for the unit's goal queue at the tile
    pub confirm_goal:
        extern "C" fn(context: *mut c_void, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal,
    /// where the unit stands for the confirmation (its position, a tree's tile, a dead unit's top point)
    pub confirm_unit: extern "C" fn(context: *mut c_void) -> EeGfxConfirmUnit,
    /// the terrain mesh's height at a point (`GetMeshZ`)
    pub terrain_z: extern "C" fn(context: *mut c_void, x: f32, y: f32) -> f32,
    /// one of the command textures (0 `sTexture4E0`, 1 `sTexture4E4`, 2 `sTexture4E8`)
    pub command_texture: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// a random one of the unit's tag points of `kind` at its animation and frame (`EEEffect::GetRandom`), the unit's
    /// position without any (the shockwave's `SetUp`, 0xe6259)
    pub spawn_point: extern "C" fn(context: *mut c_void, kind: u32) -> Ge3DPoint,
    /// the graphic object (`GetGraphicObject`, kept as the effect's) loaded; its result
    pub object_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the kept graphic object's texture `index`
    pub object_texture: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// the unit's tag points of `kind` at its animation: -1 without a complex unit, 0 without a list
    pub tag_count: extern "C" fn(context: *mut c_void, kind: u32) -> i32,
    /// the unit's tag point `index` of `kind` at its animation and frame under its scaled location (not without a
    /// complex unit; the list is not tested, as `EEE2DImpact::UpdatePosition`'s)
    pub tag_point: extern "C" fn(context: *mut c_void, kind: u32, index: u32) -> EeGfx2DPoint,
    /// the unit's weapon tag point (`EECombatEffectWeaponPoint` with its capture `site`)
    pub weapon_point: extern "C" fn(context: *mut c_void, site: i32) -> Ge3DPoint,
    /// the kept graphic object's (`part0`: the first part's) animation 0 sound for `frame` at `location`
    /// (`PlayAnimationSound`), kept as the effect's (`SetSound`) and dereferenced
    pub animation_sound:
        extern "C" fn(context: *mut c_void, location: Ge3DPoint, frame: u32, changed: bool, part0: bool),
    /// `gDrawBlood`
    pub draw_blood: extern "C" fn(context: *mut c_void) -> bool,
    /// the unit's model's default point, the unit's stats' scale
    pub default_point: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// `gWorld.IsTileBlocked`
    pub tile_blocked: extern "C" fn(context: *mut c_void, x: i32, y: i32) -> bool,
    /// `gWorld.mWindDirection`
    pub wind: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// `CreateParts(position, height, lift)`: one part per tag point of the effect's kind on the unit
    pub create_parts: extern "C" fn(context: *mut c_void, lift: f32) -> EeGfx2DPlace,
    /// the parts: their count, part `index` read and written, its graphic object's animation 0 and texture 0
    pub part_count: extern "C" fn(context: *mut c_void) -> u32,
    pub part: extern "C" fn(context: *mut c_void, index: u32) -> EeGfx2DPart,
    pub set_part: extern "C" fn(context: *mut c_void, index: u32, part: *const EeGfx2DPart),
    pub part_frames: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxFrames,
    pub part_texture: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// every part's frame copied for the draw
    pub parts_snapshot: extern "C" fn(context: *mut c_void),
    /// part `index` on the unit's current frame: its model's tag point (the unit itself for tag 10000) under the
    /// unit's location (not without a complex unit)
    pub part_follow: extern "C" fn(context: *mut c_void, index: u32) -> crate::gfx::effect_sprite::EeGfx2DPoint,
    /// `EEEffect::GetUnitLocation` (the complex unit's location, else its tile's centre)
    pub unit_point: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the kept graphic object's model (`mModel`)
    pub object_model: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// the kept graphic object's orbitals (`GetOrbitalCount`, `GetOrbital`: model, speed and angle)
    pub orbital_count: extern "C" fn(context: *mut c_void) -> u32,
    pub orbital: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxOrbital,
    /// the marching icons': the unit as `IsIconShown` reads it; whether `player` has the area effect `kind` at (x, y)
    /// (`gAEManager.HasEffect` with the unit's own list)
    pub icon_unit: extern "C" fn(context: *mut c_void) -> EeGfxIconUnit,
    pub area_effect: extern "C" fn(context: *mut c_void, kind: i32, x: f32, y: f32, player: u32) -> bool,
    /// the unit's pending area effects (`mNumAreaEffects`), and its new count (`FnE123D` at 0)
    pub area_effects: extern "C" fn(context: *mut c_void) -> i32,
    pub set_area_effects: extern "C" fn(context: *mut c_void, count: i32),
    /// the unit's type is in its player's active group (a town centre or capitol also with 0x23 or 0x29 there)
    pub icons_selected: extern "C" fn(context: *mut c_void) -> bool,
    /// `gWorld.GetNumGhostUnits()`
    pub ghost_units: extern "C" fn(context: *mut c_void) -> u32,
    /// the ground under a point on a map tile (`GetGroundPoint`; `has` false off the map)
    pub ground_point: extern "C" fn(context: *mut c_void, at: Ge3DPoint) -> EeGfx2DPoint,
    /// the light glow's: the set-up's unit (its light tags kept for the names), tag `index`'s name into `out` (its
    /// length returned; `capacity` bytes written at most)
    pub glow_set_up: extern "C" fn(context: *mut c_void) -> EeGfxGlowSetUp,
    pub glow_tag_name: extern "C" fn(context: *mut c_void, index: u32, out: *mut u8, capacity: u32) -> u32,
    /// the placing's unit (its tags, frame and scaled location kept), and tag `index`'s point
    pub glow_place: extern "C" fn(context: *mut c_void) -> EeGfxGlowPlace,
    pub glow_point: extern "C" fn(context: *mut c_void, index: u32) -> Ge3DPoint,
    /// the visible terrain and the unit's tile; its visible rows
    pub glow_area: extern "C" fn(context: *mut c_void) -> EeGfxGlowArea,
    pub glow_rows: extern "C" fn(context: *mut c_void) -> u32,
    /// the particle's move facts (`EEParticleTick` at the effect's position) and the terrain it bounces on
    pub particle_tick: extern "C" fn(context: *mut c_void) -> EeGfxParticleTick,
    pub particle_ground: extern "C" fn(context: *mut c_void) -> EeGfxParticleGround,
}

extern "C" fn no_tile(_context: *mut c_void, _x: u32, _y: u32) -> bool {
    false
}
extern "C" fn no_z(_context: *mut c_void, _x: f32, _y: f32) -> f32 {
    0.0
}
extern "C" fn no_normal(_context: *mut c_void, _x: f32, _y: f32, _normal: *mut Ge3DPoint) {}

/// The world as the 2D effects ask it.
pub trait Effect2DHost {
    fn base_queue(&mut self) -> i32;
    fn queue(&mut self, index: u32);
    fn load_media(&mut self) -> i32;
    fn frames(&mut self, index: u32) -> EeGfxFrames;
    fn move_to_point(&mut self);
    fn model(&mut self) -> EeGfxModel;
    fn selection_size(&mut self) -> f32;
    fn confirm_enemy(&mut self) -> bool;
    fn confirm_goal(&mut self, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal;
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit;
    fn unit_location(&mut self) -> GeTransformation;
    fn terrain_z(&mut self, x: f32, y: f32) -> f32;
    fn command_texture(&mut self, index: u32) -> EeGfxModel;
    /// the tests' hosts that make no shockwave answer the origin, no media and no texture
    fn spawn_point(&mut self, _kind: u32) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn object_media(&mut self) -> i32 {
        0
    }
    fn object_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    /// the sprites' (the tests' other hosts answer nothing)
    fn tag_count(&mut self, _kind: u32) -> i32 {
        -1
    }
    fn tag_point(&mut self, _kind: u32, _index: u32) -> EeGfx2DPoint {
        EeGfx2DPoint::default()
    }
    fn weapon_point(&mut self, _site: i32) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn animation_sound(&mut self, _location: Ge3DPoint, _frame: u32, _changed: bool, _part0: bool) {}
    fn draw_blood(&mut self) -> bool {
        false
    }
    fn has_complex(&mut self) -> bool {
        false
    }
    fn default_point(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn stats_scale(&mut self) -> f32 {
        1.0
    }
    fn tile_blocked(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn wind(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn create_parts(&mut self, _lift: f32) -> EeGfx2DPlace {
        EeGfx2DPlace::default()
    }
    fn part_count(&mut self) -> u32 {
        0
    }
    fn part(&mut self, _index: u32) -> EeGfx2DPart {
        EeGfx2DPart::default()
    }
    fn set_part(&mut self, _index: u32, _part: &EeGfx2DPart) {}
    fn part_frames(&mut self, _index: u32) -> EeGfxFrames {
        EeGfxFrames::default()
    }
    fn part_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn parts_snapshot(&mut self) {}
    fn part_follow(&mut self, _index: u32) -> EeGfx2DPoint {
        EeGfx2DPoint::default()
    }
    fn max_hit_points(&mut self) -> i32 {
        1
    }
    fn unit_point(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn object_model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn orbital_count(&mut self) -> u32 {
        0
    }
    fn orbital(&mut self, _index: u32) -> EeGfxOrbital {
        EeGfxOrbital { model: std::ptr::null_mut(), speed: 0.0, angle: 0.0 }
    }
    /// the marching icons' (the tests' other hosts answer no unit)
    fn icon_unit(&mut self) -> EeGfxIconUnit {
        EeGfxIconUnit::default()
    }
    fn area_effect(&mut self, _kind: i32, _x: f32, _y: f32, _player: u32) -> bool {
        false
    }
    fn area_effects(&mut self) -> i32 {
        0
    }
    fn set_area_effects(&mut self, _count: i32) {}
    fn icons_selected(&mut self) -> bool {
        false
    }
    fn icons_owner(&mut self) -> bool {
        false
    }
    fn ghost_units(&mut self) -> u32 {
        0
    }
    fn ground_point(&mut self, _at: Ge3DPoint) -> EeGfx2DPoint {
        EeGfx2DPoint::default()
    }
    /// the light glow's
    fn glow_set_up(&mut self) -> EeGfxGlowSetUp {
        EeGfxGlowSetUp::default()
    }
    fn glow_tag_name(&mut self, _index: u32) -> Vec<u8> {
        Vec::new()
    }
    fn glow_place(&mut self) -> EeGfxGlowPlace {
        EeGfxGlowPlace::default()
    }
    fn glow_point(&mut self, _index: u32) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn glow_area(&mut self) -> EeGfxGlowArea {
        EeGfxGlowArea::default()
    }
    fn glow_rows(&mut self) -> u32 {
        0
    }
    /// the particle's
    fn particle_tick(&mut self) -> EeGfxParticleTick {
        EeGfxParticleTick::default()
    }
    fn particle_ground(&mut self) -> EeGfxParticleGround {
        EeGfxParticleGround {
            context: std::ptr::null_mut(),
            is_valid_tile: no_tile,
            is_space_tile: no_tile,
            mesh_z: no_z,
            mesh_normal: no_normal,
        }
    }
}

impl Effect2DHost for &EeGfxEffect2DHost {
    fn base_queue(&mut self) -> i32 {
        (self.base_queue)(self.context)
    }
    fn queue(&mut self, index: u32) {
        (self.queue)(self.context, index)
    }
    fn load_media(&mut self) -> i32 {
        (self.load_media)(self.context)
    }
    fn frames(&mut self, index: u32) -> EeGfxFrames {
        (self.frames)(self.context, index)
    }
    fn move_to_point(&mut self) {
        (self.move_to_point)(self.context)
    }
    fn model(&mut self) -> EeGfxModel {
        (self.model)(self.context)
    }
    fn selection_size(&mut self) -> f32 {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::selection_size(self.unit) }
    }
    fn confirm_enemy(&mut self) -> bool {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::confirm_enemy(self.unit, self.player) }
    }
    fn confirm_goal(&mut self, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal {
        (self.confirm_goal)(self.context, goal_index, tile_x, tile_y)
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        (self.confirm_unit)(self.context)
    }
    fn unit_location(&mut self) -> GeTransformation {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::unit_location(self.unit) }
    }
    fn terrain_z(&mut self, x: f32, y: f32) -> f32 {
        (self.terrain_z)(self.context, x, y)
    }
    fn command_texture(&mut self, index: u32) -> EeGfxModel {
        (self.command_texture)(self.context, index)
    }
    fn spawn_point(&mut self, kind: u32) -> Ge3DPoint {
        (self.spawn_point)(self.context, kind)
    }
    fn object_media(&mut self) -> i32 {
        (self.object_media)(self.context)
    }
    fn object_texture(&mut self, index: u32) -> EeGfxModel {
        (self.object_texture)(self.context, index)
    }
    fn tag_count(&mut self, kind: u32) -> i32 {
        (self.tag_count)(self.context, kind)
    }
    fn tag_point(&mut self, kind: u32, index: u32) -> EeGfx2DPoint {
        (self.tag_point)(self.context, kind, index)
    }
    fn weapon_point(&mut self, site: i32) -> Ge3DPoint {
        (self.weapon_point)(self.context, site)
    }
    fn animation_sound(&mut self, location: Ge3DPoint, frame: u32, changed: bool, part0: bool) {
        (self.animation_sound)(self.context, location, frame, changed, part0)
    }
    fn draw_blood(&mut self) -> bool {
        (self.draw_blood)(self.context)
    }
    fn has_complex(&mut self) -> bool {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::has_complex(self.unit) }
    }
    fn default_point(&mut self) -> Ge3DPoint {
        (self.default_point)(self.context)
    }
    fn stats_scale(&mut self) -> f32 {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::unit_scale(self.unit) }
    }
    fn tile_blocked(&mut self, x: i32, y: i32) -> bool {
        (self.tile_blocked)(self.context, x, y)
    }
    fn wind(&mut self) -> Ge3DPoint {
        (self.wind)(self.context)
    }
    fn create_parts(&mut self, lift: f32) -> EeGfx2DPlace {
        (self.create_parts)(self.context, lift)
    }
    fn part_count(&mut self) -> u32 {
        (self.part_count)(self.context)
    }
    fn part(&mut self, index: u32) -> EeGfx2DPart {
        (self.part)(self.context, index)
    }
    fn set_part(&mut self, index: u32, part: &EeGfx2DPart) {
        (self.set_part)(self.context, index, part)
    }
    fn part_frames(&mut self, index: u32) -> EeGfxFrames {
        (self.part_frames)(self.context, index)
    }
    fn part_texture(&mut self, index: u32) -> EeGfxModel {
        (self.part_texture)(self.context, index)
    }
    fn parts_snapshot(&mut self) {
        (self.parts_snapshot)(self.context)
    }
    fn part_follow(&mut self, index: u32) -> EeGfx2DPoint {
        (self.part_follow)(self.context, index)
    }
    fn max_hit_points(&mut self) -> i32 {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::max_hit_points(self.unit) }
    }
    fn unit_point(&mut self) -> Ge3DPoint {
        (self.unit_point)(self.context)
    }
    fn object_model(&mut self) -> EeGfxModel {
        (self.object_model)(self.context)
    }
    fn orbital_count(&mut self) -> u32 {
        (self.orbital_count)(self.context)
    }
    fn orbital(&mut self, index: u32) -> EeGfxOrbital {
        (self.orbital)(self.context, index)
    }
    fn icon_unit(&mut self) -> EeGfxIconUnit {
        (self.icon_unit)(self.context)
    }
    fn area_effect(&mut self, kind: i32, x: f32, y: f32, player: u32) -> bool {
        (self.area_effect)(self.context, kind, x, y, player)
    }
    fn area_effects(&mut self) -> i32 {
        (self.area_effects)(self.context)
    }
    fn set_area_effects(&mut self, count: i32) {
        (self.set_area_effects)(self.context, count)
    }
    fn icons_selected(&mut self) -> bool {
        (self.icons_selected)(self.context)
    }
    fn icons_owner(&mut self) -> bool {
        // SAFETY: the effect steps with the world held; its unit is live while the effect references it
        unsafe { effect_views::icons_owner(self.unit, self.player) }
    }
    fn ghost_units(&mut self) -> u32 {
        (self.ghost_units)(self.context)
    }
    fn ground_point(&mut self, at: Ge3DPoint) -> EeGfx2DPoint {
        (self.ground_point)(self.context, at)
    }
    fn glow_set_up(&mut self) -> EeGfxGlowSetUp {
        (self.glow_set_up)(self.context)
    }
    fn glow_tag_name(&mut self, index: u32) -> Vec<u8> {
        let mut out = vec![0u8; 256];
        let mut n = (self.glow_tag_name)(self.context, index, out.as_mut_ptr(), out.len() as u32) as usize;
        if n > out.len() {
            out.resize(n, 0);
            n = (self.glow_tag_name)(self.context, index, out.as_mut_ptr(), out.len() as u32) as usize;
        }
        out.truncate(n.min(out.len()));
        out
    }
    fn glow_place(&mut self) -> EeGfxGlowPlace {
        (self.glow_place)(self.context)
    }
    fn glow_point(&mut self, index: u32) -> Ge3DPoint {
        (self.glow_point)(self.context, index)
    }
    fn glow_area(&mut self) -> EeGfxGlowArea {
        (self.glow_area)(self.context)
    }
    fn glow_rows(&mut self) -> u32 {
        (self.glow_rows)(self.context)
    }
    fn particle_tick(&mut self) -> EeGfxParticleTick {
        (self.particle_tick)(self.context)
    }
    fn particle_ground(&mut self) -> EeGfxParticleGround {
        (self.particle_ground)(self.context)
    }
}

/// One virtual of an `EEE2DRust` of the record's kind (0 for a kind without one).
///
/// # Safety
/// `r` is a live record, read and written only through this pointer during the call (the C++ reads it in the host
/// calls).
pub unsafe fn effect2d<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    match unsafe { (*r).kind } {
        EE_GFX_EFFECT_KIND_COMMAND_CONFIRMATION => unsafe { confirm(op, r, g, h, x) },
        // the damage controller draws nothing (its queue's 10, 0x1020dc) and sets up nothing; its tick is the C++'s
        crate::gfx::effect_state::EE_GFX_EFFECT_KIND_DAMAGED => {
            if op == EeGfxEffect3DOp::Queue {
                10
            } else {
                0
            }
        }
        EE_GFX_EFFECT_KIND_SHOCKWAVE => unsafe { shockwave(op, r, g, h, x) },
        3 | 4 | 5 | 0xb | 0xc | 0x14 => unsafe { parts(op, r, g, h, x) },
        0x16 | 0x30 => unsafe { trail(op, r, g, h, x) },
        0x31 => unsafe { vortex(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_SQUARE_ICONS | EE_GFX_EFFECT_KIND_CIRCULAR_ICONS => unsafe { marching(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_LIGHT_GLOW => unsafe { glow(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_DECAL | EE_GFX_EFFECT_KIND_PARTICLE_2D => unsafe { decal(op, r, g, h, x) },
        _ => unsafe { sprite(op, r, g, h, x) },
    }
}

/// `Load` (`op` Load) or `Save` of an `EEE2DRust` after `EEEffect2D`'s (the confirmation has none: its `Call`'s 0).
pub fn effect2d_file<H: Effect2DHost, F: EffectFile>(
    op: EeGfxEffect3DOp,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    match r.kind {
        EE_GFX_EFFECT_KIND_SHOCKWAVE => shockwave_file(op == EeGfxEffect3DOp::Load, r, f, h),
        3 | 4 | 5 | 0xb | 0xc | 0x14 => parts_file(op == EeGfxEffect3DOp::Load, r, f, h),
        0x16 | 0x30 => trail_file(op == EeGfxEffect3DOp::Load, r, f, h),
        0x31 => vortex_file(op == EeGfxEffect3DOp::Load, r, f, h),
        EE_GFX_EFFECT_KIND_SQUARE_ICONS | EE_GFX_EFFECT_KIND_CIRCULAR_ICONS => {
            marching_file(op == EeGfxEffect3DOp::Load, r, f, h)
        }
        EE_GFX_EFFECT_KIND_LIGHT_GLOW => glow_file(op == EeGfxEffect3DOp::Load, r, f),
        EE_GFX_EFFECT_KIND_DECAL | EE_GFX_EFFECT_KIND_PARTICLE_2D => {
            decal_file(op == EeGfxEffect3DOp::Load, r, f, major, minor, h)
        }
        _ => sprite_file(op == EeGfxEffect3DOp::Load, r, f, major, minor, h),
    }
}
