//! The 2D effects whole in Rust: `EEE2DRust` (`src/Empire Earth/Graphics Thread/EEE2DRust.cpp`), the twin of
//! `EEE3DRust` over `EEEffect2D`, calls these by the record's kind; the C++ answers the world through
//! `EeGfxEffect2DHost` and draws through the rasterizer table (`raster.rs`). Kinds: 0x41 the command confirmation
//! (`effect_confirm.rs`), 0x12 the shockwave (`effect_shockwave.rs`). The ops are `EEE3DRust`'s (`EeGfxEffect3DOp`), the facts the same `EeGfxGraphicFacts`.
//!
//! The record is reached through its raw pointer field by field: the C++ reads it (the position, the radius) during
//! the host calls, so no Rust reference to it lives across one.

use std::ffi::c_void;

use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::effect_confirm::{confirm, EeGfxConfirmGoal, EeGfxConfirmUnit, EE_GFX_EFFECT_KIND_COMMAND_CONFIRMATION};
use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::effect_shockwave::{shockwave, shockwave_file};
use crate::effect_state::{EeGfxEffectRecord, EffectFile, EE_GFX_EFFECT_KIND_SHOCKWAVE};
use crate::effect_vent::EeGfxFrames;
use crate::raster::{EeGfxModel, Raster};

/// The C++'s answers for an `EEE2DRust`.
#[repr(C)]
pub struct EeGfxEffect2DHost {
    pub context: *mut c_void,
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
    /// the unit's class's selection size
    pub selection_size: extern "C" fn(context: *mut c_void) -> f32,
    /// the unit's player is another, not player 0, and the current player's enemy
    pub confirm_enemy: extern "C" fn(context: *mut c_void) -> bool,
    /// the goal the confirmation shows (`EeGfxConfirmGoal`) for the unit's goal queue at the tile
    pub confirm_goal:
        extern "C" fn(context: *mut c_void, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal,
    /// where the unit stands for the confirmation (its position, a tree's tile, a dead unit's top point)
    pub confirm_unit: extern "C" fn(context: *mut c_void) -> EeGfxConfirmUnit,
    /// the unit's complex unit's physical location
    pub unit_location: extern "C" fn(context: *mut c_void) -> GeTransformation,
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
}

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
        (self.selection_size)(self.context)
    }
    fn confirm_enemy(&mut self) -> bool {
        (self.confirm_enemy)(self.context)
    }
    fn confirm_goal(&mut self, goal_index: u32, tile_x: i32, tile_y: i32) -> EeGfxConfirmGoal {
        (self.confirm_goal)(self.context, goal_index, tile_x, tile_y)
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        (self.confirm_unit)(self.context)
    }
    fn unit_location(&mut self) -> GeTransformation {
        (self.unit_location)(self.context)
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
        EE_GFX_EFFECT_KIND_SHOCKWAVE => unsafe { shockwave(op, r, g, h, x) },
        _ => 0,
    }
}

/// `Load` (`op` Load) or `Save` of an `EEE2DRust` after `EEEffect2D`'s (the confirmation has none: its `Call`'s 0).
pub fn effect2d_file<H: Effect2DHost, F: EffectFile>(
    op: EeGfxEffect3DOp,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    match r.kind {
        EE_GFX_EFFECT_KIND_SHOCKWAVE => shockwave_file(op == EeGfxEffect3DOp::Load, r, f, h),
        _ => 0,
    }
}
