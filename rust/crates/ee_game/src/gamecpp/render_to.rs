//! EEGraphics::RenderTo (was EEGraphics.cpp): a capture of the current view, or of the whole map top-down tile by
//! tile into a new surface of the asked width (the post-game map), zooming out until a tile fits the window, the
//! camera restored afterwards. The GE math is ee_base's (the very functions the C++ inlines forward to: the line's
//! solve, the planes' intersection, the transformation's point apply and orientation, UMCos, UMRint); each C++ call
//! or member is a case of EEGraphics.cpp's `EECore10OpA`. The doubles and floats are the C++'s, in its order.
//!
//! Only a game with a window runs this (a screenshot, the post-game map); no gate reaches the post-game map.

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeRectangle, GeTransformation};
use ee_base::math::um::{lrintf, um_cos};
use ee_render::dx7::api::SurfaceDescription;

use super::{cptr, h, hd, hf, hp, pa, ptr, O, P};

/// RenderTo's constants (the unit's .rdata: the free camera's capture rectangle, scaled by 0.64; the floats widened)
const RT_CAPTURE_LEFT: f32 = 0.0;
const RT_CAPTURE_RIGHT: f32 = 1600.0;
const RT_CAPTURE_TOP: f32 = 35.0;
const RT_CAPTURE_BOTTOM: f32 = 1085.0;
const RT_CAPTURE_SCALE: f32 = 0.64;
const RT_MINUS_ONE: f64 = -1.0;
const RT_EPSILON: f64 = 0.001f32 as f64;
const RT_MINUS_HALF: f64 = -0.5;
const RT_HALF: f64 = 0.5;
const RT_TILE_FACTOR: f64 = 0.707_106_77_f32 as f64;
const RT_ONE: f64 = 1.0;
/// `kSurfaceSystemMemory`.
const RT_SURFACE_SYSTEM_MEMORY: c_ulong = 4;

/// `UMCeil`: rounds to nearest, then steps up when that went down.
fn um_ceil(value: f32) -> c_long {
    let mut result = lrintf(value) as c_long;
    if (result as f32) < value {
        result += 1;
    }
    result
}

/// `GE3DLine::SolveForZ(z, t, point)`: the point written only when the line is not parallel.
fn solve_for_z(line: &Ge3DLine, z: f32, point: &mut Ge3DPoint) {
    if let Some((_, p)) = line.solve_for_z_point(z) {
        *point = p;
    }
}

fn flag(op: O) -> *mut bool {
    hp(op, 0, &[]) as *mut bool
}

fn set_flag(op: O, value: bool) {
    // SAFETY: the address of a bool global (RO flags)
    unsafe { *flag(op) = value };
}

fn world_transformation() -> GeTransformation {
    // SAFETY: the address of gWorld.mTransformation
    unsafe { *(hp(O::RoWorldTransformation, 0, &[]) as *const GeTransformation) }
}

fn world_line() -> Ge3DLine {
    let mut line = Ge3DLine::default();
    h(O::RtWorldLine, 0, &[ptr(&mut line)]);
    line
}

fn ortho_rectangle() -> GeRectangle {
    let mut r = GeRectangle::default();
    h(O::RtViewportOrthoRectangle, 0, &[ptr(&mut r)]);
    r
}

fn zoom() -> f32 {
    hd(O::RtCameraZoom, 0, &[], &[])
}

fn set_zoom(z: f32) {
    hf(O::CameraSetZoom, 0, &[], &[z]);
}

fn camera_changed() {
    h(O::CameraSetChanged, 0, &[]);
    hf(O::CameraUpdate, 0, &[], &[0.0]);
}

/// SetLocation(location, false, true), SetChanged, Update(0).
fn camera_to(location: &Ge3DPoint) {
    h(O::CameraSetLocation, 0, &[cptr(location), 0, 1]);
    camera_changed();
}

fn width_of(r: &GeRectangle) -> f32 {
    r.right - r.left
}

fn height_of(r: &GeRectangle) -> f32 {
    r.bottom - r.top
}

/// EEGraphics' constructor body (after RTSGraphics'): the frame lock mode 0, the message-queue-done event, and the
/// main viewport registered for the Rust readers (ee_gfx input_views.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_construct(g: *mut c_void) {
    let g = g as P;
    // SAFETY: the graphics object's long member
    unsafe { *(hp(O::GfxLockMode, g, &[]) as *mut c_long) = 0 };
    let created = h(O::GtQueueDoneCreate, g, &[]);
    h(O::Verify, 0, &[created, 78]);
    // SAFETY: gViewport lives as long as the program
    unsafe { ee_render::gfx::input_views::ee_gfx_main_viewport_set(hp(O::GtMainViewport, 0, &[]) as *mut _) };
}

/// EEGraphics' destructor body: the main viewport unregistered, the scene released, the event destroyed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_destruct(g: *mut c_void) {
    // SAFETY: a null viewport unregisters it
    unsafe { ee_render::gfx::input_views::ee_gfx_main_viewport_set(std::ptr::null_mut()) };
    super::render_object::ee_gamecpp_ro_release_scene();
    h(O::GtQueueDoneDestroy, g as P, &[]);
}

/// EEGraphics::PostMessage50AAndWait: the queue-done message (0x50a) posted, then a wait until the graphics thread
/// has handled every earlier message; the post's result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_post_50a_and_wait(g: *mut c_void) -> c_long {
    let g = g as P;
    let result = h(O::GtPost, g, &[0x50a]) as c_long;
    if result == 0 {
        h(O::GtQueueDoneWait, g, &[]);
    }
    result
}

/// EEGraphics::RenderTo (its member forwards here).
#[no_mangle]
pub extern "C" fn ee_gamecpp_gfx_render_to(g: *mut c_void, capture: *mut c_void) {
    let (g, capture) = (g as P, capture as P);
    // SAFETY: the capture's members (EEScreenCapture: mWidth, mSurface)
    let width_slot = hp(O::RtCaptureWidth, capture, &[]) as *mut c_long;
    let surface_slot = hp(O::RtCaptureSurface, capture, &[]);
    let the_zoom = zoom();
    let screen = h(O::RtScreenIndex, 0, &[]);
    let saved_orthographic_view = h(O::RoOrthographic, 0, &[]) != 0;
    let mut row: c_long = 0;
    let mut column: c_long = 0;
    // SAFETY: gB529065
    let saved_b529065 = unsafe { *flag(O::RoFlag529065) };

    h(O::WorldReadLock, 0, &[]);
    set_flag(O::RoFlag529065, true);
    // SAFETY: the capture's mWidth (read and written only here, between C++ calls)
    if unsafe { *width_slot } == -1 {
        h(O::RtRender, g, &[]);
        h(O::RtCaptureSetRenderTarget, capture, &[screen]);
    } else {
        let mut saved_location = Ge3DPoint::default();
        let mut saved_target = Ge3DPoint::default();
        h(O::CameraGetLocation, 0, &[ptr(&mut saved_location)]);
        h(O::CameraGetLocation, 0, &[ptr(&mut saved_target)]);
        let mut saved_view = GeRectangle::default();
        h(O::RtViewportViewRectangle, 0, &[ptr(&mut saved_view)]);
        let saved_orthographic = ortho_rectangle();
        let mut source = GeRectangle::default();
        let (window_width, window_height): (c_ulong, c_ulong);
        if h(O::FreeCamera, 0, &[]) == 0 {
            window_width = h(O::GameWindowWidth, 0, &[]) as c_ulong;
            window_height = h(O::GameWindowHeight, 0, &[]) as c_ulong;
        } else {
            window_width = 1024;
            window_height = 768;
            source.left = RT_CAPTURE_SCALE * RT_CAPTURE_LEFT;
            source.right = RT_CAPTURE_SCALE * RT_CAPTURE_RIGHT;
            source.top = RT_CAPTURE_SCALE * RT_CAPTURE_TOP;
            source.bottom = RT_CAPTURE_SCALE * RT_CAPTURE_BOTTOM;
            h(O::ViewportSetViewRectangle, 0, &[cptr(&source)]);
        }
        h(O::RtSetOrthographic, 0, &[1]);
        h(O::RtProjectionMode, 0, &[0, screen]);
        set_zoom(1.0);
        camera_changed();
        let mut transformation = world_transformation();
        let orientation = transformation.orientation();
        let cos = um_cos(1.570_796_4_f32 - orientation.pitch * -1.0f32) as f64;
        let map_width = h(O::RtMapL195610, 0, &[]) as c_long;
        let map_height = h(O::RtMapL195614, 0, &[]) as c_long;
        let aspect = map_width as f64 / map_height as f64;
        // SAFETY: as above
        unsafe {
            if aspect > RT_ONE {
                *width_slot = (*width_slot as f64 / (aspect * cos)) as c_long;
            }
        }
        let view = ortho_rectangle();
        let mut view_width = width_of(&view) as f64;
        let mut view_height = height_of(&view) as f64;
        let the_map_height = map_height as f64;
        // SAFETY: as above
        let mut ratio = unsafe { *width_slot } as f64 / the_map_height;
        let mut tile_width = ratio * view_width * RT_TILE_FACTOR;
        let mut tile_height = tile_width * cos;
        while tile_width > window_width as f64 || (window_height as f64) < tile_height {
            if zoom() <= 0.0 {
                // SAFETY: as above
                unsafe {
                    *width_slot /= 2;
                    ratio = *width_slot as f64 / the_map_height;
                }
                set_zoom(1.0);
            } else {
                set_zoom(zoom() - 0.1f32);
            }
            camera_changed();
            let view = ortho_rectangle();
            view_width = width_of(&view) as f64;
            view_height = height_of(&view) as f64;
            tile_width = ratio * view_width * RT_TILE_FACTOR;
            tile_height = tile_width * cos;
        }

        // SAFETY: as above
        let capture_width = unsafe { *width_slot };
        let description = SurfaceDescription {
            mType: RT_SURFACE_SYSTEM_MEMORY,
            mPixelFormat: h(O::RtTargetPixelFormat, 0, &[screen]) as c_ulong,
            mSurfaceVolatility: 0,
            mWidth: capture_width as c_ulong,
            mHeight: (capture_width as f64 * aspect * cos) as c_ulong,
            m14: 0,
            mMipLevelCount: 0,
            mPixelDepth: h(O::RtTargetPixelDepth, 0, &[screen]) as c_ulong,
            mIs3DRenderable: false,
            mIsTransparencyEnabled: false,
            mNoAlpha: false,
        };
        let result = h(O::RtCreateSurface, 0, &[cptr(&description), pa(surface_slot), screen]);
        h(O::Verify, 0, &[(result == 0) as i64, 810]);

        let fraction_x =
            description.mWidth as f64 / tile_width - (description.mWidth as f64 / tile_width) as c_long as f64;
        let fraction_y =
            description.mHeight as f64 / tile_height - (description.mHeight as f64 / tile_height) as c_long as f64;
        let tile_height_f = tile_height as f32;
        source.bottom = tile_height_f;
        source.top = 0.0;
        let tile_width_f = tile_width as f32;
        source.right = tile_width_f;
        source.left = 0.0;
        h(O::ViewportSetViewRectangle, 0, &[cptr(&source)]);
        let mut destination = GeRectangle { bottom: tile_height_f, left: 0.0, right: tile_width_f, top: 0.0 };

        let mut front = Ge3DPlane::default();
        let mut left = Ge3DPlane::default();
        h(O::RtMeshFrontPlane, 0, &[ptr(&mut front)]);
        h(O::RtMeshLeftPlane, 0, &[ptr(&mut left)]);
        // GE3DLine's default: the origin, along +x (kept when the planes are parallel)
        let corner = left.intersects(&front).unwrap_or(Ge3DLine {
            start: Ge3DPoint::default(),
            direction: ee_base::math::types::GeVector { x: 1.0, y: 0.0, z: 0.0 },
        });
        let mut location = Ge3DPoint { x: corner.start.x, y: corner.start.y, z: 0.0 };
        camera_to(&location);

        transformation = world_transformation();
        let mut row_step = Ge3DPoint {
            x: (view_width * RT_HALF) as f32,
            y: 0.0,
            z: (hd(O::RtViewportYClipping, 0, &[], &[]) as f64 * view_height * RT_MINUS_HALF) as f32,
        };
        location = transformation.apply_point(row_step);
        let mut line = world_line();
        line.start = location;
        solve_for_z(&line, 0.0, &mut location);
        camera_to(&location);

        let columns = um_ceil(description.mWidth as f32 / tile_width_f) as f32 as c_long;
        let rows = um_ceil(description.mHeight as f32 / tile_height_f) as f32 as c_long;
        let column_step = Ge3DPoint { x: view_width as f32, y: 0.0, z: 0.0 };
        loop {
            loop {
                set_flag(O::RoFlag529066, false);
                set_flag(O::RoCameraFollowing, false);
                set_flag(O::RoFlag529068, false);
                h(O::RtRender, g, &[]);
                if destination.bottom > description.mHeight as f32 {
                    destination.bottom = description.mHeight as f32;
                }
                if destination.right > description.mWidth as f32 {
                    destination.right = description.mWidth as f32;
                }
                h(O::RtCopyTo, capture, &[screen, cptr(&source), cptr(&destination)]);
                column += 1;
                destination.left = (column as f64 * tile_width) as f32;
                destination.right = ((column + 1) as f64 * tile_width) as f32;
                transformation = world_transformation();
                location = transformation.apply_point(column_step);
                line = world_line();
                line.start = location;
                solve_for_z(&line, 0.0, &mut location);
                camera_to(&location);
                if column + 1 == columns && fraction_x > RT_EPSILON {
                    source.right = (fraction_x * tile_width) as f32;
                }
                if column >= columns {
                    break;
                }
            }
            row += 1;
            destination.top = (row as f64 * tile_height) as f32;
            destination.bottom = ((row + 1) as f64 * tile_height) as f32;
            column = 0;
            destination.right = tile_width_f;
            destination.left = 0.0;
            source.right = tile_width_f;
            source.left = 0.0;
            if row + 1 == rows && fraction_y > RT_EPSILON {
                source.bottom = (fraction_y * tile_height) as f32;
            }
            line = world_line();
            line.start = corner.start;
            solve_for_z(&line, 0.0, &mut location);
            camera_to(&location);
            row_step.z =
                (hd(O::RtViewportYClipping, 0, &[], &[]) as f64 * (row as f64 + RT_HALF) * view_height * RT_MINUS_ONE)
                    as f32;
            transformation = world_transformation();
            location = transformation.apply_point(row_step);
            line = world_line();
            line.start = location;
            solve_for_z(&line, 0.0, &mut location);
            camera_to(&location);
            if row >= rows {
                break;
            }
        }

        h(O::ViewportSetViewRectangle, 0, &[cptr(&saved_view)]);
        h(O::RtSetViewportOrthoRectangle, 0, &[cptr(&saved_orthographic)]);
        h(O::RtSetOrthographic, 0, &[saved_orthographic_view as i64]);
        h(O::RtProjectionMode, 0, &[1, screen]);
        set_zoom(the_zoom);
        h(O::CameraSetLocation, 0, &[cptr(&saved_location), 0, 1]);
        camera_changed();
    }
    set_flag(O::RoFlag529066, false);
    set_flag(O::RoCameraFollowing, false);
    set_flag(O::RoFlag529068, false);
    set_flag(O::RoFlag529065, saved_b529065);
    h(O::WorldReadUnlock, 0, &[]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ceiling_rounds_to_nearest_then_steps_up() {
        assert_eq!(um_ceil(2.0), 2);
        assert_eq!(um_ceil(2.2), 3);
        assert_eq!(um_ceil(2.5), 3);
        assert_eq!(um_ceil(2.7), 3);
        assert_eq!(um_ceil(-0.5), 0);
        assert_eq!(RT_EPSILON, 0.0010000000474974513);
        assert_eq!(RT_TILE_FACTOR as f32, 0.707_106_77_f32);
    }

    #[test]
    fn a_parallel_line_keeps_the_point() {
        let line = Ge3DLine { start: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, direction: Default::default() };
        let mut p = Ge3DPoint { x: 9.0, y: 9.0, z: 9.0 };
        solve_for_z(&line, 0.0, &mut p);
        assert_eq!(p, Ge3DPoint { x: 9.0, y: 9.0, z: 9.0 });
        let line = Ge3DLine {
            start: Ge3DPoint { x: 1.0, y: 2.0, z: 4.0 },
            direction: ee_base::math::types::GeVector { x: 1.0, y: 0.0, z: -2.0 },
        };
        solve_for_z(&line, 0.0, &mut p);
        assert_eq!(p, Ge3DPoint { x: 3.0, y: 2.0, z: 0.0 });
    }
}
