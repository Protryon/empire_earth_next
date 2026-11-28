//! The C ABI of the Graphics Thread's and the camera's math (`include/rust/ee_rust.h`), called by
//! `EECameraControl.cpp`, `RTSMouse.cpp`, `EEROUnits.cpp`, `EEUnitPicker.cpp`, `EEMiniMap.cpp`, `EEEffect2D/3D.cpp` and
//! `EERenderObject.cpp`. Every function copies its inputs before it writes an
//! output, so an output may alias an input; the terrain callbacks run while no Rust reference to a C++ object is held
//! (the inputs are copies).

use std::ffi::c_void;

use ee_base::math::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};

use crate::ge::viewport::GeViewport;
use crate::gfx::effect_batch;
use crate::gfx::minimap::{self, EeGfxMiniMapTransforms, EeGfxMiniMapView};
use crate::gfx::mouse::{self};
use crate::gfx::raster::EeGfxRaster;
use crate::gfx::terrain::{self};
use crate::gfx::units::{self};
use crate::gfx::view;

/// The terrain mesh under the camera, as C++ callbacks over `context` (the `GETerrainMesh`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxGround {
    pub context: *mut c_void,
    /// `GetIsValidTile(UMFloorUnsigned(x), UMFloorUnsigned(y))`
    pub valid_tile: unsafe extern "C" fn(context: *mut c_void, x: f32, y: f32) -> bool,
    /// `GetIsValidPoint(UMFloorUnsigned(x), UMFloorUnsigned(y))`
    pub valid_point: unsafe extern "C" fn(context: *mut c_void, x: f32, y: f32) -> bool,
    /// `GetMeshZ(x, y)`
    pub mesh_z: unsafe extern "C" fn(context: *mut c_void, x: f32, y: f32) -> f32,
}

/// `RTSMouse::ClampPosition`: `*position` kept inside `*bounds`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_mouse_clamp(position: *mut Ge2DPoint, bounds: *const GeRectangle) {
    // SAFETY: the function's contract
    unsafe {
        let r = mouse::clamp_position(*position, &*bounds);
        *position = r;
    }
}

/// `GetScreenPoint` (`EEROUnits.cpp`): `*point` to window coordinates in the game view `*viewport`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_units_screen_point(
    viewport: *const GeViewport,
    orthographic: bool,
    point: *const Ge3DPoint,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract
    unsafe {
        let r = units::screen_point(&*viewport, orthographic, *point);
        *out = r;
    }
}

// ---- the unit picker (pick.rs, EEUnitPicker.cpp)

// ---- the mini map (minimap.rs, EEMiniMap.cpp)

/// `SetMap`'s transformations and pixel sizes for a map of `height` (`mL195610`) by `width` (`mL195614`) tiles and
/// `size` tiles per side.
#[no_mangle]
pub extern "C" fn ee_gfx_minimap_transforms(height: i32, width: i32, size: i32) -> EeGfxMiniMapTransforms {
    minimap::transforms(height, width, size)
}

/// `Refresh`'s view trapezoid of `*viewport` through `*map_to_pixel`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_minimap_view(
    viewport: *const GeViewport,
    orthographic: bool,
    map_to_pixel: *const GeTransformation,
) -> EeGfxMiniMapView {
    // SAFETY: the function's contract
    unsafe { minimap::view_trapezoid(&*viewport, orthographic, &*map_to_pixel) }
}

// ---- the frame's view tests (view.rs, EEEffect2D/3D.cpp, EERenderObject.cpp)

/// The effects' frustum test of a sphere at `*location`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_in_view(
    viewport: *const GeViewport,
    orthographic: bool,
    location: *const Ge3DPoint,
    radius: f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe { view::effect_in_view(&*viewport, orthographic, *location, radius) }
}

// ---- the selection rings (units.rs, EEROUnits.cpp)

// ---- the ring models (models.rs, EEROUnits::LoadMedia)

// ---- the sky, the clouds and the grid mode (terrain.rs, EEROTerrain.cpp)

/// `StepGridMode`: the editor's grid mode stepped forwards or backwards through 0..17.
#[no_mangle]
pub extern "C" fn ee_gfx_terrain_grid_mode_step(mode: i32, backwards: bool) -> i32 {
    terrain::grid_mode_step(mode, backwards)
}

// ---- the earthquake's crack (quake.rs, EEE3DEarthquake.cpp)

// ---- the placement footprint and the labels (units.rs, EEROUnits.cpp)

/// Which clip rectangles a label at screen height `y` is drawn to (bit 0 the view, 1 the strip, 2 the editor's).
#[no_mangle]
pub extern "C" fn ee_gfx_units_label_targets(y: f32, text_height: f32, bottom: f32, free_camera: bool) -> u32 {
    units::label_targets(y, text_height, bottom, free_camera)
}

// ---- the effects' quad batches and plane models (effect_batch.rs, EEEffect.cpp)

fn table<'a>(raster: *const EeGfxRaster) -> &'a EeGfxRaster {
    // SAFETY: the C++ passes its raster table, alive for the program
    unsafe { &*raster }
}

/// `BeginEffectBatches`: both batches empty with `texture` (`GETexture*`, may be null), referenced for each.
///
/// # Safety
/// `raster` is the C++'s table.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_batches_begin(raster: *const EeGfxRaster, texture: *mut c_void) {
    effect_batch::begin(&mut table(raster), texture)
}

/// `FlushEffectBatches`: what the ground batch and then the camera batch still hold, drawn.
///
/// # Safety
/// `raster` is the C++'s table.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_batches_flush(raster: *const EeGfxRaster) {
    effect_batch::flush_all(&mut table(raster))
}

/// `EndEffectBatches`: the references `begin` took dropped.
///
/// # Safety
/// `raster` is the C++'s table.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_batches_end(raster: *const EeGfxRaster) {
    effect_batch::end(&mut table(raster))
}

/// `CreatePlaneModels`, `ReleasePlaneModels` (the shared plane models).
///
/// # Safety
/// `raster` is the C++'s table.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_planes_create(raster: *const EeGfxRaster) {
    effect_batch::create_planes(&mut table(raster))
}

/// # Safety
/// `raster` is the C++'s table.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_planes_release(raster: *const EeGfxRaster) {
    effect_batch::release_planes(&mut table(raster))
}

// ---- the box selection (select.rs, EEUICSelectUnits.cpp)

// ---- the mini map's camera point (minimap.rs, EEUIMiniMap.cpp)
