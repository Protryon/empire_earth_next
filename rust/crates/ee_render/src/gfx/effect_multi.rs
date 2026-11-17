//! The multi-effect (kind 0x3b) whole in Rust (`EEE3DMultiEffect`, deleted): it only starts its graphic effect's
//! linked effects 1 and 2, and 3 when there is one, through the entry point that made it (on its unit, at its point,
//! along its line: the C++ host's `create_linked`, whose results are ignored, as the original's), is never queued
//! (10), draws nothing and is removed at its first step (7). Its unit is never the effect's: the original's unit
//! constructor gave `EEEffect3D` no unit. Load and Save are the base's (its graphic effects are unsaved).

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::EeGfxEffectRecord;
use crate::gfx::raster::Raster;

/// `kEffect3DMultiEffect`
pub const EE_GFX_EFFECT_KIND_MULTI_EFFECT: i32 = 0x3b;

/// The multi-effect's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d` (the record is not read).
pub unsafe fn multi<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    _r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    _x: &mut R,
) -> i32 {
    match op {
        EeGfxEffect3DOp::Create | EeGfxEffect3DOp::CreateAt | EeGfxEffect3DOp::CreateLine => {
            h.create_linked(1);
            h.create_linked(2);
            if g.has_linked3 {
                h.create_linked(3);
            }
            0
        }
        EeGfxEffect3DOp::Queue => 10,
        EeGfxEffect3DOp::Step => 7,
        _ => 0,
    }
}

#[cfg(test)]
#[path = "effect_multi_tests.rs"]
mod tests;
