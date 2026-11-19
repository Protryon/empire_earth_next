//! The C ABI of `GEPhysicalModel::Intersects(viewport)` (`physical.rs`); the viewport's is `geval::viewport_ffi`.
//! (`UpdateVisibleTerrain`'s plan is the terrain mesh's own, `tmobj.rs`.)

use crate::ge::physical;
use crate::geval::viewport::GeViewport;
use ee_base::math::types::{Ge3DPoint, GeBoundingBox, GeTransformation};

// ---- GEPhysicalModel ----

/// `GEPhysicalModel::Intersects(viewport, orthographic)` with the placed model's values.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_physical_visible(
    location: *const GeTransformation,
    scale: f32,
    internal: *const GeTransformation,
    center: *const Ge3DPoint,
    radius: f32,
    bbox: *const GeBoundingBox,
    viewport: *const GeViewport,
    orthographic: bool,
) -> bool {
    // SAFETY: the function's contract
    unsafe { physical::model_visible(&*location, scale, &*internal, *center, radius, &*bbox, &*viewport, orthographic) }
}
