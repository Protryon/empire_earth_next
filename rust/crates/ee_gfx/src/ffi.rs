//! The C ABI of the Graphics Thread's and the camera's math (`include/rust/ee_rust.h`), called by
//! `EECameraControl.cpp`, `RTSMouse.cpp` and `EEROUnits.cpp`. Every function copies its inputs before it writes an
//! output, so an output may alias an input; the terrain callbacks run while no Rust reference to a C++ object is held
//! (the inputs are copies).

use std::ffi::c_void;

use ee_ge::types::{Ge2DPoint, Ge3DLine, Ge3DPoint, GeRectangle, GeTransformation, GeVector};

use crate::camera::{
    self, EeGfxCameraGoTo, EeGfxCameraPose, EeGfxCameraRays, EeGfxCameraStep, EeGfxClipping, EeGfxEdgePlanes,
    EeGfxOrthoView, Ground,
};
use crate::mouse::{self, EeGfxMouseInput, EeGfxMouseMove};
use crate::units::{self, EeGfxBarRects, EeGfxBarSizes};
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
