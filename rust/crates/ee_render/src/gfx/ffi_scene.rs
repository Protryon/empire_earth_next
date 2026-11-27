//! The C interface of `scene.rs`: the capture's fade, letterbox, clocks and time-of-day blend
//! (`EERenderObject.cpp`), and `capture.rs`'s per-object arithmetic. The state is the C++'s storage, passed for each
//! call.

use ee_base::math::types::Ge3DPoint;

use crate::gfx::capture::{self, GroundHeights, EE_GFX_RING_POINTS};
use crate::gfx::ffi_effect::EeGfxParticleGround;

// ---- the capture's per-object arithmetic (capture.rs)

struct Ground<'a>(&'a EeGfxParticleGround);

impl GroundHeights for Ground<'_> {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool {
        (self.0.is_valid_tile)(self.0.context, x, y)
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        (self.0.mesh_z)(self.0.context, x, y)
    }
}

/// `DrawGroundCircle`'s 51 points into `points` (the last the first again); returns the strip's width.
///
/// # Safety
/// `ground` is readable; `points` has room for `EE_GFX_RING_POINTS`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_capture_ground_circle(
    ground: *const EeGfxParticleGround,
    x: f32,
    y: f32,
    radius: f32,
    points: *mut Ge3DPoint,
) -> f32 {
    // SAFETY: the function's contract
    let (p, width) = capture::ground_circle(&mut Ground(unsafe { &*ground }), x, y, radius);
    // SAFETY: the function's contract
    unsafe { std::ptr::copy_nonoverlapping(p.as_ptr(), points, EE_GFX_RING_POINTS) };
    width
}

// ---- the lighting set-ups (lighting.rs)

// ---- the capture's world walk (walk.rs)
