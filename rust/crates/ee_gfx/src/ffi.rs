//! The C ABI of the Graphics Thread's and the camera's math (`include/rust/ee_rust.h`), called by
//! `EECameraControl.cpp`, `RTSMouse.cpp`, `EEROUnits.cpp`, `EEUnitPicker.cpp`, `EEMiniMap.cpp`, `EEEffect2D/3D.cpp` and
//! `EERenderObject.cpp`. Every function copies its inputs before it writes an
//! output, so an output may alias an input; the terrain callbacks run while no Rust reference to a C++ object is held
//! (the inputs are copies).

use std::ffi::c_void;

use ee_ge::types::{Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeRectangle, GeTransformation, GeVector};

use crate::camera::{
    self, EeGfxCameraGoTo, EeGfxCameraPose, EeGfxCameraRays, EeGfxCameraStep, EeGfxClipping, EeGfxEdgePlanes,
    EeGfxOrthoView, Ground,
};
use crate::effects::{self, EeGfxColor, EeGfxEffectFog, EeGfxEffectQuad, EeGfxTexCoord};
use crate::minimap::{self, EeGfxMiniMapTransforms, EeGfxMiniMapView};
use crate::models::{self, EeGfxRingModel, EeGfxSelectionModel};
use crate::mouse::{self, EeGfxMouseInput, EeGfxMouseMove};
use crate::pick::{self, EeGfxPickAdd, EeGfxPickHit, EeGfxPickRay, EeGfxPickTolerances, EeGfxPickUnit};
use crate::select::{self, EeGfxSelectLines};
use crate::units::{
    self, EeGfxBarRects, EeGfxBarSizes, EeGfxLabelClip, EeGfxPlacementLine, EeGfxRingSegments, EeGfxSelectionRing,
};
use crate::view;
use ee_ge::viewport::GeViewport;

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

struct CppGround(EeGfxGround);

impl Ground for CppGround {
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        // SAFETY: the callback's contract (the C++ mesh outlives the call)
        unsafe { (self.0.valid_tile)(self.0.context, x, y) }
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        // SAFETY: as above
        unsafe { (self.0.valid_point)(self.0.context, x, y) }
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        // SAFETY: as above
        unsafe { (self.0.mesh_z)(self.0.context, x, y) }
    }
}

/// `EECameraControl::GetPitch(zoom, mode)`; `world` is `gWorld.mTransformation` (read in mode 2 only).
///
/// # Safety
/// `world` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_pitch(zoom: f32, mode: i32, world: *const GeTransformation) -> f32 {
    // SAFETY: the function's contract
    let world = unsafe { *world };
    camera::pitch(zoom, mode, &world)
}

/// `GetOffset`/`GetLocation`: `step` of the way from `start` to `end`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_lerp(
    start: *const Ge3DPoint,
    end: *const Ge3DPoint,
    step: f32,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let r = camera::lerp(*start, *end, step);
        *out = r;
    }
}

/// `GoTo`'s speed and snaps.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_go_to(
    start_offset: *const Ge3DPoint,
    end_offset: *const Ge3DPoint,
    start_focus: *const Ge3DPoint,
    end_focus: *const Ge3DPoint,
    key_rate: f32,
    following: bool,
    follow_rate: f32,
) -> EeGfxCameraGoTo {
    // SAFETY: the function's contract
    unsafe {
        camera::go_to_speed(*start_offset, *end_offset, *start_focus, *end_focus, key_rate, following, follow_rate)
    }
}

/// `ClampHeight(ioTransformation)`.
///
/// # Safety
/// `t` is valid; the callbacks are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_clamp_height(
    t: *mut GeTransformation,
    mode: i32,
    front_clipping: f32,
    plain: bool,
    ground: EeGfxGround,
) {
    // SAFETY: the function's contract (a copy, stored back once)
    unsafe {
        let mut v = *t;
        camera::clamp_height(&mut v, mode, front_clipping, plain, &mut CppGround(ground));
        *t = v;
    }
}

/// `BuildView(out, step)`.
///
/// # Safety
/// The pointers are valid; the callbacks are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_build_view(
    pose: *const EeGfxCameraPose,
    step: f32,
    mode: i32,
    front_clipping: f32,
    plain: bool,
    ground: EeGfxGround,
    out: *mut GeTransformation,
) {
    // SAFETY: the function's contract
    unsafe {
        let pose = *pose;
        let r = camera::build_view(&pose, step, mode, front_clipping, plain, &mut CppGround(ground));
        *out = r;
    }
}

/// `SetClippingDistances`' distances; `rays` is null in the orthographic view.
///
/// # Safety
/// `rays` is null or valid; `camera` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_clipping(
    rays: *const EeGfxCameraRays,
    camera: *const Ge3DPoint,
    mode: i32,
    zoom: f32,
    free_camera: bool,
    far_rear_clipping: bool,
) -> EeGfxClipping {
    // SAFETY: the function's contract
    unsafe {
        let rays = if rays.is_null() { None } else { Some(*rays) };
        camera::clipping_distances(rays.as_ref(), *camera, mode, zoom, free_camera, far_rear_clipping)
    }
}

/// `ClipToEdges(line, ioPoint)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_clip_to_edges(
    line: *const Ge3DLine,
    planes: *const EeGfxEdgePlanes,
    point: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe {
        let r = camera::clip_to_edges(&*line, &*planes, *point);
        *point = r;
    }
}

/// `GetGroundPoint(line, out)`; `offset` is `GetOffset(mFraction)`.
///
/// # Safety
/// The pointers are valid; the callbacks are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_ground_point(
    line: *const Ge3DLine,
    planes: *const EeGfxEdgePlanes,
    offset: *const Ge3DPoint,
    mode: i32,
    ground: EeGfxGround,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the callbacks and the store)
    unsafe {
        let line = *line;
        let planes = *planes;
        let r = camera::ground_point(&line, &planes, *offset, mode, &mut CppGround(ground));
        *out = r;
    }
}

/// `Scroll`'s ground direction (`scale`: the zoom + 0.5 as the scroll began).
///
/// # Safety
/// `view` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_scroll_direction(
    view: *const GeTransformation,
    dx: f32,
    dy: f32,
    scale: f32,
) -> GeVector {
    // SAFETY: the function's contract
    unsafe { camera::scroll_direction(&*view, dx, dy, scale) }
}

/// Whether `Scroll` looks for the ground under the screen (the offset within 10).
///
/// # Safety
/// `offset` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_scroll_is_near(offset: *const Ge3DPoint) -> bool {
    // SAFETY: the function's contract
    unsafe { camera::scroll_is_near(*offset) }
}

/// `Scroll`'s clamp of the target to the map's edge planes.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_clamp_to_map(target: *mut Ge3DPoint, planes: *const EeGfxEdgePlanes) {
    // SAFETY: the function's contract
    unsafe {
        let r = camera::clamp_to_map(*target, &*planes);
        *target = r;
    }
}

/// `Update`'s step of the move (`CAMERA_STEP_*`).
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_step(state: *mut EeGfxCameraStep, elapsed: f32) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut s = *state;
        let r = camera::step(&mut s, elapsed);
        *state = s;
        r
    }
}

/// `Update`'s orthographic view.
///
/// # Safety
/// `focus` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_camera_ortho_view(focus: *const Ge3DPoint, offset_y: f32) -> EeGfxOrthoView {
    // SAFETY: the function's contract
    unsafe { camera::ortho_view(*focus, offset_y) }
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

/// `RTSMouse::Update`'s move (not frozen): `min_x`/`min_y` are the unit's smallest moves (`sMinXValue`/`sMinYValue`).
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_mouse_update_move(
    input: *const EeGfxMouseInput,
    min_x: *mut i32,
    min_y: *mut i32,
) -> EeGfxMouseMove {
    // SAFETY: the function's contract
    unsafe {
        let (mut x, mut y) = (*min_x, *min_y);
        let r = mouse::update_move(&*input, &mut x, &mut y);
        *min_x = x;
        *min_y = y;
        r
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

/// `DrawBars`' bar sizes from the window's size.
#[no_mangle]
pub extern "C" fn ee_gfx_units_bar_sizes(window_width: u32, window_height: u32) -> EeGfxBarSizes {
    units::bar_sizes(window_width, window_height)
}

/// `DrawBar`'s rectangles: the unit's bar tag (`bar` scaled by `scale` through `*transformation`) projected through
/// `*viewport`, moved to row `row` and split by `fraction`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_units_unit_bar(
    viewport: *const GeViewport,
    orthographic: bool,
    transformation: *const GeTransformation,
    bar: *const Ge3DPoint,
    scale: f32,
    fraction: f32,
    row: i32,
    sizes: *const EeGfxBarSizes,
) -> EeGfxBarRects {
    // SAFETY: the function's contract
    unsafe {
        let world = units::unit_bar_point(&*transformation, *bar, scale);
        let screen = units::screen_point(&*viewport, orthographic, world);
        let origin = units::bar_origin(screen, row, &*sizes);
        units::unit_bar(origin, fraction, &*sizes)
    }
}

/// `DrawObjectBars`' rectangles: the object's bar tag (`*location` plus `bar` scaled by `scale`) projected through
/// `*viewport` and split by `health`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_units_object_bar(
    viewport: *const GeViewport,
    orthographic: bool,
    location: *const Ge3DPoint,
    bar: *const Ge3DPoint,
    scale: f32,
    health: f32,
    sizes: *const EeGfxBarSizes,
) -> EeGfxBarRects {
    // SAFETY: the function's contract
    unsafe {
        let world = units::object_bar_point(*location, *bar, scale);
        let screen = units::screen_point(&*viewport, orthographic, world);
        units::object_bar(screen, health, &*sizes)
    }
}

// ---- the unit picker (pick.rs, EEUnitPicker.cpp)

/// `FindUnit`'s ray through a screen point of `*viewport`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_ray(
    viewport: *const GeViewport,
    orthographic: bool,
    point: *const Ge2DPoint,
    out: *mut EeGfxPickRay,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let r = pick::screen_ray(&*viewport, orthographic, *point);
        *out = r;
    }
}

/// `FindUnit`'s depth tolerance and pixel share.
///
/// # Safety
/// `viewport` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_tolerances(
    viewport: *const GeViewport,
    free_camera: bool,
    window_width: u32,
) -> EeGfxPickTolerances {
    // SAFETY: the function's contract
    pick::tolerances(unsafe { &*viewport }, free_camera, window_width)
}

/// The bounding test's scale of an entry of priority `kind`.
#[no_mangle]
pub extern "C" fn ee_gfx_pick_scale(kind: i32, saved: f32) -> f32 {
    pick::pick_scale(kind, saved)
}

/// Whether a bounding hit is large on the screen (or a tree).
#[no_mangle]
pub extern "C" fn ee_gfx_pick_is_large(pixel_size: f32, pixel: f32, kind: i32) -> bool {
    pick::is_large(pixel_size, pixel, kind)
}

/// `FindUnit`'s ranking and choice over `count` hits: the chosen hit's index into `hits`, or -1. `group_nonempty`
/// (the current player's selection holds a unit) is asked only when both an exact and a bounding hit rank.
///
/// # Safety
/// `hits` holds `count` hits (or `count` is 0); the callback's contract is the caller's.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_best(
    hits: *const EeGfxPickHit,
    count: usize,
    tolerance: f32,
    show_hidden: bool,
    context: *mut c_void,
    group_nonempty: unsafe extern "C" fn(context: *mut c_void) -> bool,
) -> isize {
    let hits: Vec<EeGfxPickHit> =
        if count == 0 { vec![] } else { unsafe { std::slice::from_raw_parts(hits, count) }.to_vec() };
    let (first, second) = pick::rank(&hits, tolerance, show_hidden);
    // SAFETY: the callback's contract (the hits are a copy)
    match pick::choose(&hits, first, second, || unsafe { group_nonempty(context) }) {
        Some(i) => i as isize,
        None => -1,
    }
}

/// `GetUnitPickPriority`; `ask(context, question)` answers 0 `IsLandBuilding`, 1 the current player's `IsAlly`.
///
/// # Safety
/// `unit` is valid; the callback's contract is the caller's.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_priority(
    unit: *const EeGfxPickUnit,
    prefer_resources: bool,
    context: *mut c_void,
    ask: unsafe extern "C" fn(context: *mut c_void, question: u32) -> bool,
) -> i32 {
    // SAFETY: the function's contract (the facts copied)
    let unit = unsafe { *unit };
    // SAFETY: the callback's contract
    pick::priority(&unit, prefer_resources, |q| unsafe { ask(context, q as u32) })
}

/// `AddUnit`'s admission.
///
/// # Safety
/// `add` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_admits(add: *const EeGfxPickAdd) -> bool {
    // SAFETY: the function's contract
    pick::admits(unsafe { &*add })
}

/// # Safety
/// `planes` holds the frustum's six planes.
unsafe fn side_planes(planes: *const Ge3DPlane) -> [Ge3DPlane; 4] {
    // SAFETY: the caller's contract
    unsafe { [*planes.add(2), *planes.add(3), *planes.add(4), *planes.add(5)] }
}

/// `GetIsEntryInView`'s centre test against the side planes 2..5 of `planes` (six): `below[0..4]` and their count.
///
/// # Safety
/// `center` is valid, `planes` holds six planes, `below` has room for four.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_sphere_below(
    center: *const Ge3DPoint,
    planes: *const Ge3DPlane,
    below: *mut bool,
) -> u32 {
    // SAFETY: the function's contract
    unsafe {
        let b = pick::sphere_below(*center, &side_planes(planes));
        for (k, v) in b.iter().enumerate() {
            *below.add(k) = *v;
        }
        b.iter().filter(|v| **v).count() as u32
    }
}

/// `GetIsEntryInView`'s sphere test: of the side planes the centre is below, how many the sphere does not reach.
///
/// # Safety
/// `center` is valid, `planes` holds six planes, `below` four flags.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_sphere_outside(
    center: *const Ge3DPoint,
    planes: *const Ge3DPlane,
    below: *const bool,
    radius: f32,
) -> u32 {
    // SAFETY: the function's contract
    unsafe {
        let b = [*below, *below.add(1), *below.add(2), *below.add(3)];
        pick::sphere_outside(*center, &side_planes(planes), &b, radius)
    }
}

/// The LOD `GetIsEntryInView` walks.
#[no_mangle]
pub extern "C" fn ee_gfx_pick_view_lod(lod_count: u32) -> u32 {
    pick::view_lod(lod_count)
}

/// `GetIsModelInView`'s vertex test against the side planes 2..5 of `planes` (six).
///
/// # Safety
/// `planes` holds six planes, `point` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_point_in_view(planes: *const Ge3DPlane, point: *const Ge3DPoint) -> bool {
    // SAFETY: the function's contract
    unsafe { pick::point_in_view(&side_planes(planes), *point) }
}

// ---- the mini map (minimap.rs, EEMiniMap.cpp)

/// `SetMap`'s transformations and pixel sizes for a map of `height` (`mL195610`) by `width` (`mL195614`) tiles and
/// `size` tiles per side.
#[no_mangle]
pub extern "C" fn ee_gfx_minimap_transforms(height: i32, width: i32, size: i32) -> EeGfxMiniMapTransforms {
    minimap::transforms(height, width, size)
}

/// `ProjectToGround`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_minimap_project(
    viewport: *const GeViewport,
    orthographic: bool,
    point: *const Ge2DPoint,
    z: f32,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let r = minimap::project_to_ground(&*viewport, orthographic, *point, z);
        *out = r;
    }
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

/// `GetUnitOpacity`'s near fade: updates `*translucent` and `*opacity`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_unit_near_fade(
    viewport: *const GeViewport,
    center: *const Ge3DPoint,
    radius: f32,
    scale: f32,
    translucent: *mut bool,
    opacity: *mut f32,
) {
    // SAFETY: the function's contract (inputs copied before the stores)
    unsafe {
        let (t, o) = view::near_fade(&*viewport, *center, radius, scale, *translucent, *opacity);
        *translucent = t;
        *opacity = o;
    }
}

// ---- the selection rings (units.rs, EEROUnits.cpp)

/// `DrawSelection`'s ring model and scale for a selection size.
#[no_mangle]
pub extern "C" fn ee_gfx_units_selection_ring(size: f32) -> EeGfxSelectionRing {
    units::selection_ring(size)
}

/// `DrawRings`' segment colours for an area-effect strength.
#[no_mangle]
pub extern "C" fn ee_gfx_units_ring_segments(strength: u32) -> EeGfxRingSegments {
    units::ring_segments(strength)
}

// ---- the ring models (models.rs, EEROUnits::LoadMedia)

/// `LoadMedia`'s area-effect ring: its points, texture coordinates and triangles.
#[no_mangle]
pub extern "C" fn ee_gfx_units_area_effect_ring() -> EeGfxRingModel {
    models::area_effect_ring()
}

/// The selection rings' segments at a model detail (`gModelDetail`).
#[no_mangle]
pub extern "C" fn ee_gfx_units_selection_segments(detail: i32) -> u32 {
    models::selection_segments(detail)
}

/// `LoadMedia`'s selection ring of size `size` (0..2; larger is held at 2) at a model detail.
#[no_mangle]
pub extern "C" fn ee_gfx_units_selection_model(detail: i32, size: u32) -> EeGfxSelectionModel {
    models::selection_model(detail, (size as usize).min(2))
}

// ---- the placement footprint and the labels (units.rs, EEROUnits.cpp)

/// `DrawPlacement`'s footprint edges: `heights` and `cells` are the record's 12 x 12 `mHeights` and `mCells`
/// (`[x][y]`); writes at most `capacity` lines to `out` and returns their count.
///
/// # Safety
/// The pointers are valid; `heights` and `cells` hold 144 elements, `out` `capacity`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_units_placement_lines(
    viewport: *const GeViewport,
    orthographic: bool,
    size: i32,
    tile_x: i32,
    tile_y: i32,
    heights: *const f32,
    cells: *const u8,
    out: *mut EeGfxPlacementLine,
    capacity: usize,
) -> usize {
    // SAFETY: the function's contract (the grids copied before any store)
    unsafe {
        let vp = *viewport;
        let heights = *(heights as *const [[f32; units::PLACEMENT_GRID]; units::PLACEMENT_GRID]);
        let cells = *(cells as *const [[u8; units::PLACEMENT_GRID]; units::PLACEMENT_GRID]);
        let out = std::slice::from_raw_parts_mut(out, capacity);
        units::placement_lines(&vp, orthographic, size, tile_x, tile_y, &heights, &cells, out)
    }
}

/// `DrawLabels`' clip rectangles from the game view's rectangle `*view` and the window's size.
///
/// # Safety
/// `view` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_units_label_clip(
    view: *const GeRectangle,
    width: u32,
    height: u32,
    free_camera: bool,
    panel_active: bool,
) -> EeGfxLabelClip {
    // SAFETY: the function's contract
    let view = unsafe { *view };
    units::label_clip(view, width, height, free_camera, panel_active)
}

/// Which clip rectangles a label at screen height `y` is drawn to (bit 0 the view, 1 the strip, 2 the editor's).
#[no_mangle]
pub extern "C" fn ee_gfx_units_label_targets(y: f32, text_height: f32, bottom: f32, free_camera: bool) -> u32 {
    units::label_targets(y, text_height, bottom, free_camera)
}

/// The debug label's place: the bar tag (`*bar` scaled by `scale` through `*transformation`) on the screen, above
/// `bars` extra bars.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_units_label_point(
    viewport: *const GeViewport,
    orthographic: bool,
    transformation: *const GeTransformation,
    bar: *const Ge3DPoint,
    scale: f32,
    bars: u64,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let p = units::label_point(&*viewport, orthographic, &*transformation, *bar, scale, bars);
        *out = p;
    }
}

// ---- the effects' quads (effects.rs, EEEffect.cpp)

/// `GetEffectFogFactor`: the fog's fade at `*point` (`fog.state` 1: linear fog).
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_fog_factor(
    point: *const Ge3DPoint,
    fog: *const EeGfxEffectFog,
    camera_inverse: *const GeTransformation,
) -> f32 {
    // SAFETY: the function's contract
    unsafe { effects::fog_factor(*point, &*fog, &*camera_inverse) }
}

/// `AddGlowSprite`'s quad.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_effect_glow_sprite(
    position: *const Ge3DPoint,
    color: *const EeGfxColor,
    rotation: f32,
    size: f32,
    alpha: f32,
    brightness: f32,
    fog: f32,
    camera_yaw: f32,
    camera_pitch: f32,
) -> EeGfxEffectQuad {
    // SAFETY: the function's contract
    unsafe { effects::glow_sprite(*position, *color, rotation, size, alpha, brightness, fog, camera_yaw, camera_pitch) }
}

/// `AddGroundSprite`'s and `AddGroundQuad`'s quad; `opacities` holds the four corners'.
///
/// # Safety
/// The pointers are valid; `opacities` holds four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_ground_sprite(
    position: *const Ge3DPoint,
    color: *const EeGfxColor,
    rotation: f32,
    size: f32,
    opacities: *const f32,
    brightness: f32,
) -> EeGfxEffectQuad {
    // SAFETY: the function's contract
    unsafe {
        let opacities = *(opacities as *const [f32; 4]);
        effects::ground_sprite(*position, *color, rotation, size, opacities, brightness)
    }
}

/// `AddBlendedSprite`'s quad.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_effect_blended_sprite(
    position: *const Ge3DPoint,
    color: *const EeGfxColor,
    rotation: f32,
    size: f32,
    alpha: f32,
    brightness: f32,
    camera_yaw: f32,
    camera_pitch: f32,
) -> EeGfxEffectQuad {
    // SAFETY: the function's contract
    unsafe { effects::blended_sprite(*position, *color, rotation, size, alpha, brightness, camera_yaw, camera_pitch) }
}

/// `AddGroundStrip`'s quad.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_effect_ground_strip(
    from: *const Ge3DPoint,
    to: *const Ge3DPoint,
    height: f32,
    color: *const EeGfxColor,
    origin: *const EeGfxTexCoord,
    extent: *const EeGfxTexCoord,
    alpha: f32,
    brightness: f32,
) -> EeGfxEffectQuad {
    // SAFETY: the function's contract
    unsafe { effects::ground_strip(*from, *to, height, *color, *origin, *extent, alpha, brightness) }
}

// ---- the box selection (select.rs, EEUICSelectUnits.cpp)

/// `StartSelection`'s drag start: `*location` rounded.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_select_start(location: *const Ge2DPoint, out: *mut Ge2DPoint) {
    // SAFETY: the function's contract (the input copied before the store)
    unsafe {
        let p = select::start_point(*location);
        *out = p;
    }
}

/// `MouseMove`'s drag end: `*location` rounded, clamped to the game view `*view`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_select_drag_end(
    location: *const Ge2DPoint,
    view: *const GeRectangle,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (the inputs copied before the store)
    unsafe {
        let p = select::drag_end(*location, &*view);
        *out = p;
    }
}

/// Whether the drag from `*start` to `*end` has gone far enough to start.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_select_starts_drag(start: *const Ge2DPoint, end: *const Ge2DPoint) -> bool {
    // SAFETY: the function's contract
    unsafe { select::starts_drag(*start, *end) }
}

/// The box from `*start` to `*end`: `MouseMove`'s (`release` false) or `MouseLButtonUp`'s.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_select_box(
    start: *const Ge2DPoint,
    end: *const Ge2DPoint,
    release: bool,
    out: *mut GeRectangle,
) {
    // SAFETY: the function's contract (the inputs copied before the store)
    unsafe {
        let r = if release { select::release_box(*start, *end) } else { select::drag_box(*start, *end) };
        *out = r;
    }
}

/// `DrawPolygons`' eight lines round the box `*rectangle`.
///
/// # Safety
/// `rectangle` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_select_box_lines(rectangle: *const GeRectangle) -> EeGfxSelectLines {
    // SAFETY: the function's contract
    unsafe { select::box_lines(&*rectangle) }
}

// ---- the mini map's camera point (minimap.rs, EEUIMiniMap.cpp)

/// `EEUIMiniMap::CenterCamera`'s mini-map point for the mouse at `*mouse`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_minimap_center_point(
    mouse: *const Ge2DPoint,
    origin: *const Ge2DPoint,
    width: u32,
    height: u32,
    bounds: *const GeRectangle,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (the inputs copied before the store)
    unsafe {
        let p = minimap::center_point(*mouse, *origin, width, height, &*bounds);
        *out = p;
    }
}

/// `EEUIMiniMap::MouseRButtonDown`'s mini-map point (its own clamp; `minimap::press_point`).
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_minimap_press_point(
    mouse: *const Ge2DPoint,
    origin: *const Ge2DPoint,
    width: u32,
    height: u32,
    bounds: *const GeRectangle,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (the inputs copied before the store)
    unsafe {
        let p = minimap::press_point(*mouse, *origin, width, height, &*bounds);
        *out = p;
    }
}

/// `GetIsEntryInView`'s model transformation (the box selection's frustum test).
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_entry_transform(
    location: *const GeTransformation,
    internal: *const GeTransformation,
    scale: f32,
    out: *mut GeTransformation,
) {
    // SAFETY: the function's contract (the inputs copied before the store)
    unsafe {
        let t = pick::entry_transform(&*location, &*internal, scale);
        *out = t;
    }
}
