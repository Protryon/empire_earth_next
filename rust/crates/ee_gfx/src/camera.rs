//! The camera's math (`src/Empire Earth/Empire Earth/EECameraControl.cpp`): the pitch for a zoom, the move's
//! interpolation and speed, the view built from the interpolated turns, the height clamp over the terrain, the
//! clipping distances from the view rays, the map edge clip, the scroll direction and the orthographic view. The C++
//! class keeps its members, its key list, the followed units, the viewport and the world; it hands these functions
//! plain values and applies what they return. The terrain is queried through [`Ground`] (the C++ mesh's
//! `GetIsValidTile`/`GetIsValidPoint` after `UMFloorUnsigned`, and `GetMeshZ`).
//!
//! `EECameraControl::Update` runs in the simulation tick (`EEPhysics`) as well as on the UI thread, so RUST_PLAN 2.1
//! applies: the C++'s `f32` operations in its order, its `pow` and `sqrt` in `f64` (std's, the CRT's), the table
//! trigonometry (`ee_ge::um`).

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeQuaternion, GeTransformation, GeVector};
use ee_ge::um::{um_arccos, um_arctan2};

/// The file's degree factor (`kCameraDegreesToRadians`).
const DEG: f32 = 0.017453292;
/// The pitch at the closest and the farthest zoom (`sCameraPitchNear`, `sCameraPitchFar`).
const PITCH_NEAR: f32 = DEG * -5.0;
const PITCH_FAR: f32 = DEG * -35.0;
/// The free camera's (`sCameraFreePitchNear`, `sCameraFreePitchFar`).
const FREE_PITCH_NEAR: f32 = DEG * 0.0;
const FREE_PITCH_FAR: f32 = DEG * -35.0;
/// The view ray tilt the rear limit scales with, and the extra range (85 - 85: none).
const TILT_LIMIT: f32 = DEG * 25.0;
const REAR_MAXIMUM: f32 = 85.0;
const REAR_RANGE: f32 = REAR_MAXIMUM - 85.0;
const REAR_MAXIMUM2: f32 = 85.0;
const REAR_Z: f32 = -5.0;
const FRONT_Z: f32 = 10.0;
const FRONT_MINIMUM: f32 = 0.1;
const REAR_NORMAL: f32 = 35.0;
const REAR_FREE: f32 = 58.0;
const MINIMUM_HEIGHT: f32 = 0.4;
const MINIMUM_HEIGHT_FREE: f32 = 0.1;
const NEAR_DISTANCE: f32 = 10.0;
const MOVE_SPEED: f32 = 15.0;
const FOLLOW_SPEED: f32 = 15.0;
const MINIMUM_TIME: f32 = 2.5;
const ZOOM_EXPONENT: f64 = 0.3;
const HALF_PI: f32 = 1.5707964;
const ORTHO_WIDTH: f32 = 800.0;
const ORTHO_YAW: f32 = 0.7853982;
const ORTHO_PITCH: f32 = -0.5235988;
const ORTHO_HEIGHT: f32 = -40.0;
const ORTHO_NEAR: f32 = -5.0;
const ORTHO_FAR: f32 = -48.0;
const ORTHO_SCALE: f32 = 0.008417938;
/// The free camera mode (`mMode == 3`).
pub const MODE_FREE: i32 = 3;

/// The terrain under the camera (the C++ mesh of `gWorld.GetMap()`).
pub trait Ground {
    /// `GetIsValidTile(UMFloorUnsigned(x), UMFloorUnsigned(y))`.
    fn valid_tile(&mut self, x: f32, y: f32) -> bool;
    /// `GetIsValidPoint(UMFloorUnsigned(x), UMFloorUnsigned(y))`.
    fn valid_point(&mut self, x: f32, y: f32) -> bool;
    /// `GetMeshZ(x, y)`.
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
}

/// The terrain mesh's four edge planes (`GetFrontPlane` .. `GetRightPlane`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEdgePlanes {
    pub front: Ge3DPlane,
    pub back: Ge3DPlane,
    pub left: Ge3DPlane,
    pub right: Ge3DPlane,
}

/// The lowest camera height over the ground for a mode (`theMinimum`).
#[inline]
pub fn minimum_height(mode: i32) -> f32 {
    if mode != MODE_FREE {
        MINIMUM_HEIGHT
    } else {
        MINIMUM_HEIGHT_FREE
    }
}

/// The C++'s `(float)pow((double)base, exponent)`: the C library's `pow` itself. The exponent goes through
/// `black_box` so LLVM cannot rewrite `pow(x, 0.5)` as `fabs(sqrt(x))`, whose NaN for a negative base has the other
/// sign (the golden's mode-3 pitch past zoom 1 showed it).
#[inline]
fn pow(base: f32, exponent: f64) -> f32 {
    (base as f64).powf(std::hint::black_box(exponent)) as f32
}

/// `GetPitch(zoom, mode)`: linear (0), curved (1), the world camera's own pitch (2: `world` is
/// `gWorld.mTransformation`), the free camera's (3), else 0. Modes 0 and 1 hold the pitch past zoom 1 (the port's
/// farther zoom-out), and so does mode 3 past |zoom| 1 (a port change: the original's pitch is NaN there).
pub fn pitch(zoom: f32, mode: i32, world: &GeTransformation) -> f32 {
    let mut zoom = zoom;
    if (mode == 0 || mode == 1) && zoom > 1.0 {
        zoom = 1.0;
    }
    match mode {
        0 => (PITCH_FAR - PITCH_NEAR) * (1.0 - pow(1.0 - zoom, 1.0)) + PITCH_NEAR,
        1 => (PITCH_FAR - PITCH_NEAR) * (1.0 - pow(1.0 - zoom, ZOOM_EXPONENT)) + PITCH_NEAR,
        2 => {
            let rotated = world.apply_vector(GeVector::new(0.0, 1.0, 0.0));
            um_arccos(-rotated.z) - HALF_PI
        }
        3 => {
            let mut z = zoom;
            if z < 0.0 {
                z = -z;
            }
            // Port change: the free camera holds its pitch past |zoom| 1 as modes 0 and 1 do (the original's
            // pow(1 - |zoom|, 0.5) is NaN there, and the table cosine faults on a NaN angle).
            if z > 1.0 {
                z = 1.0;
            }
            let mut result = (FREE_PITCH_FAR - FREE_PITCH_NEAR) * (1.0 - pow(1.0 - z, 0.5)) + FREE_PITCH_NEAR;
            if zoom < 0.0 {
                result = -result;
            }
            result
        }
        _ => 0.0,
    }
}

/// `GetOffset`/`GetLocation`: `step` of the way from `start` to `end`.
#[inline]
pub fn lerp(start: Ge3DPoint, end: Ge3DPoint, step: f32) -> Ge3DPoint {
    Ge3DPoint {
        x: (end.x - start.x) * step + start.x,
        y: (end.y - start.y) * step + start.y,
        z: (end.z - start.z) * step + start.z,
    }
}

/// What `GoTo` sets after the new key is taken: the move's rate, and whether the start offset and focus jump to the
/// end (a move shorter than 0.001).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxCameraGoTo {
    pub speed: f32,
    pub snap_offset: bool,
    pub snap_focus: bool,
}

/// `GoTo`'s speed: the offset's and the focus's distances at 15 a second (the followed unit's speed times the game
/// speed when following, at least 15), the slower of the two, and at most 2.5 * rate moves a second unless
/// following. `follow_rate` is read only when `following`.
pub fn go_to_speed(
    start_offset: Ge3DPoint,
    end_offset: Ge3DPoint,
    start_focus: Ge3DPoint,
    end_focus: Ge3DPoint,
    key_rate: f32,
    following: bool,
    follow_rate: f32,
) -> EeGfxCameraGoTo {
    let mut out = EeGfxCameraGoTo::default();
    let mut speed = 0.0f32;
    let distance = start_offset.distance_to(end_offset);
    if distance > 0.001 {
        speed = MOVE_SPEED * key_rate / distance;
    } else {
        out.snap_offset = true;
    }
    let distance = start_focus.distance_to(end_focus);
    if distance > 0.001 || following {
        let rate = if following {
            let mut r = follow_rate;
            if r < FOLLOW_SPEED {
                r = FOLLOW_SPEED;
            }
            r
        } else {
            FOLLOW_SPEED
        };
        let new_speed = key_rate * rate / distance;
        if (new_speed > 0.0 && new_speed < speed) || speed == 0.0 || following {
            speed = new_speed;
            if following {
                out.speed = speed;
                return out;
            }
        }
    } else {
        out.snap_focus = true;
    }
    if speed == 0.0 || 1.0 / speed < 1.0 / (MINIMUM_TIME * key_rate) {
        speed = MINIMUM_TIME * key_rate;
    }
    out.speed = speed;
    out
}

/// `ClampHeight`: keeps a camera transformation `min` above the terrain at its position (or at `min` off the map),
/// and above the ground one front clipping distance ahead of it; `plain` (no followed unit, no free camera, no path
/// playing) keeps it at least 0.4 high. Only the height changes.
pub fn clamp_height(t: &mut GeTransformation, mode: i32, front_clipping: f32, plain: bool, ground: &mut dyn Ground) {
    let offset = GeVector::new(0.0, front_clipping + 1.0, 0.0);
    let minimum = minimum_height(mode);
    let mut position = Ge3DPoint::new(t.x(), t.y(), t.z());
    let mut z = position.z;
    if ground.valid_tile(position.x, position.y) {
        let height = ground.mesh_z(position.x, position.y) + minimum;
        if height > z {
            z = height;
        }
    } else if position.z < minimum {
        z = minimum;
    }
    let rotated = t.apply_vector(offset);
    position.x += rotated.x;
    position.y += rotated.y;
    position.z += rotated.z;
    if ground.valid_tile(position.x, position.y) {
        let height = ground.mesh_z(position.x, position.y);
        // the original compares the bare height and then adds the minimum
        if height > z {
            z = height + minimum;
        }
    }
    if plain && z < MINIMUM_HEIGHT {
        z = MINIMUM_HEIGHT;
    }
    t.m[2][3] = z;
}

/// The camera's pose at a step of the move (`BuildView`'s inputs).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxCameraPose {
    /// `mQ014`, `mQ04C`: the view turn's start and end.
    pub view_start: GeQuaternion,
    pub view_end: GeQuaternion,
    /// `mQ024`, `mQ05C`: the offset turn's.
    pub offset_start: GeQuaternion,
    pub offset_end: GeQuaternion,
    pub start_offset: Ge3DPoint,
    pub end_offset: Ge3DPoint,
    pub start_focus: Ge3DPoint,
    pub end_focus: Ge3DPoint,
}

/// `BuildView`: the interpolated offset turned by the offset turn, moved to the interpolated focus and clamped above
/// the ground, then turned to the view turn's yaw and pitch at that point.
pub fn build_view(
    pose: &EeGfxCameraPose,
    step: f32,
    mode: i32,
    front_clipping: f32,
    plain: bool,
    ground: &mut dyn Ground,
) -> GeTransformation {
    let point = lerp(pose.start_offset, pose.end_offset, step);
    let mut revolve = GeTransformation::IDENTITY;
    revolve.m[0][3] = point.x;
    revolve.m[1][3] = point.y;
    revolve.m[2][3] = point.z;
    let rotation = pose.offset_start.slerp(&pose.offset_end, step).to_transformation();
    let mut out = rotation.compose(&revolve);
    let focus = lerp(pose.start_focus, pose.end_focus, step);
    translate(&mut out, focus);
    clamp_height(&mut out, mode, front_clipping, plain, ground);
    let point = Ge3DPoint::new(out.m[0][3], out.m[1][3], out.m[2][3]);
    out.m[0][3] = 0.0;
    out.m[1][3] = 0.0;
    out.m[2][3] = 0.0;
    let orientation = pose.view_start.slerp(&pose.view_end, step).to_transformation();
    let pitch = orientation.m[2][1];
    let yaw = um_arctan2(orientation.m[1][0], orientation.m[0][0]);
    out.rotate(yaw, um_arccos(-pitch) - HALF_PI, 0.0);
    translate(&mut out, point);
    out
}

/// The header's `Translate(x, y, z)`.
#[inline]
fn translate(t: &mut GeTransformation, p: Ge3DPoint) {
    t.m[0][3] += p.x;
    t.m[1][3] += p.y;
    t.m[2][3] += p.z;
}

/// `EECameraRayDistance`: how far the view ray `screen_line` (`EEMap::GetScreenLine`) travels from the camera's
/// plane down to height `z`, else `distance`, at most the rear limit for its tilt.
pub fn ray_distance(screen_line: &Ge3DLine, camera: Ge3DPoint, z: f32, distance: f32) -> f32 {
    let mut result = distance;
    let mut up = true;
    let mut direction = screen_line.direction;
    direction.normalize();
    let mut line = Ge3DLine { start: screen_line.start, direction };
    let plane = Ge3DPlane { point: camera, normal: direction };
    // the plane's normal is the line's direction: the line always crosses it (the C++ ignores the result)
    let t = line.intersects_plane(&plane).unwrap_or(f32::NAN);
    line.start = line.point_at(t);
    let mut angle = um_arccos(-direction.z) - HALF_PI;
    if angle < 0.0 {
        angle = -angle;
        up = false;
    } else if angle <= TILT_LIMIT {
        angle = 0.0;
    }
    let maximum = if !(angle > TILT_LIMIT) { REAR_RANGE * (angle / TILT_LIMIT) + REAR_MAXIMUM2 } else { REAR_MAXIMUM };
    if !up {
        if let Some((t, hit)) = line.solve_for_z_point(z) {
            if t > 0.0 {
                result = hit.distance_to(line.start);
            }
        }
    }
    if result > maximum {
        result = maximum;
    }
    result
}

/// The view rays through the screen's top, centre and bottom (`SetClippingDistances`' three screen lines).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxCameraRays {
    pub top: Ge3DLine,
    pub center: Ge3DLine,
    pub bottom: Ge3DLine,
}

/// The front and rear clipping distances `SetClippingDistances` gives the viewport.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxClipping {
    pub front: f32,
    pub rear: f32,
}

/// `SetClippingDistances`: in perspective (`rays` given) the front distance is the nearest the three rays reach
/// height 10 (at least 0.1, at most the rear one, 0.1 in free camera) and the rear one 35 times the zoom past 1 (58
/// in free camera or with the far rear clipping); orthographic views get 0.1 and 85.
pub fn clipping_distances(
    rays: Option<&EeGfxCameraRays>,
    camera: Ge3DPoint,
    mode: i32,
    zoom: f32,
    free_camera: bool,
    far_rear_clipping: bool,
) -> EeGfxClipping {
    let Some(rays) = rays else {
        return EeGfxClipping { front: FRONT_MINIMUM, rear: REAR_MAXIMUM };
    };
    let mut rear = ray_distance(&rays.center, camera, REAR_Z, REAR_MAXIMUM);
    let d = ray_distance(&rays.top, camera, REAR_Z, REAR_MAXIMUM);
    if d > rear {
        rear = d;
    }
    let d = ray_distance(&rays.bottom, camera, REAR_Z, REAR_MAXIMUM);
    if d > rear {
        rear = d;
    }
    if rear > REAR_MAXIMUM {
        rear = REAR_MAXIMUM;
    }
    let mut front = ray_distance(&rays.center, camera, FRONT_Z, FRONT_MINIMUM);
    let d = ray_distance(&rays.top, camera, FRONT_Z, FRONT_MINIMUM);
    if d < front {
        front = d;
    }
    let d = ray_distance(&rays.bottom, camera, FRONT_Z, FRONT_MINIMUM);
    if d < front {
        front = d;
    }
    if front < FRONT_MINIMUM {
        front = FRONT_MINIMUM;
    }
    if front >= rear && front != rear {
        front = rear;
    }
    if free_camera {
        front = FRONT_MINIMUM;
    }
    let rear = if mode != MODE_FREE && !free_camera && !far_rear_clipping {
        REAR_NORMAL * (if zoom > 1.0 { zoom } else { 1.0 })
    } else {
        REAR_FREE
    };
    EeGfxClipping { front, rear }
}

/// `ClipToEdges`: moves `point` back to where `line` first crosses (0 <= t < 1) one of the map's edge planes, in the
/// order front, back, left, right, nudged 0.0001 inside.
pub fn clip_to_edges(line: &Ge3DLine, planes: &EeGfxEdgePlanes, point: Ge3DPoint) -> Ge3DPoint {
    let mut point = point;
    let mut nearest = 1.0f32;
    let nudges: [(&Ge3DPlane, f32, f32); 4] = [
        (&planes.front, 0.0001, -0.0001),
        (&planes.back, -0.0001, 0.0001),
        (&planes.left, 0.0001, 0.0001),
        (&planes.right, -0.0001, -0.0001),
    ];
    for (plane, dx, dy) in nudges {
        if let Some(t) = line.intersects_plane(plane) {
            if t >= 0.0 && t < nearest {
                point = line.point_at(t);
                nearest = t;
                point.x = point.x + dx;
                point.y = point.y + dy;
            }
        }
    }
    point
}

/// `GetGroundPoint`: the end of a scroll line clipped to the map's edges, at `min` over the terrain when the camera
/// is close (its offset at `offset` within 10) or the point is below 0 and on the map, else at `min`.
pub fn ground_point(
    line: &Ge3DLine,
    planes: &EeGfxEdgePlanes,
    offset: Ge3DPoint,
    mode: i32,
    ground: &mut dyn Ground,
) -> Ge3DPoint {
    let minimum = minimum_height(mode);
    let mut point = clip_to_edges(line, planes, line.point_at(1.0));
    if offset.x * offset.x + offset.y * offset.y + offset.z * offset.z <= NEAR_DISTANCE * NEAR_DISTANCE || point.z < 0.0
    {
        if ground.valid_point(point.x, point.y) {
            point.z = ground.mesh_z(point.x, point.y) + minimum;
            return point;
        }
    }
    point.z = minimum;
    point
}

/// Whether `Scroll` lifts its target to the ground under the screen's top centre (the camera within 10 of its
/// focus).
#[inline]
pub fn scroll_is_near(offset: Ge3DPoint) -> bool {
    offset.x * offset.x + offset.y * offset.y + offset.z * offset.z <= NEAR_DISTANCE * NEAR_DISTANCE
}

/// `Scroll`'s ground direction: the screen delta scaled by `scale` (the zoom + 0.5 as the scroll began), turned by
/// the view (`mView`; a delta the view turns straight up or down is tried as (dx, 0, dy), then (0, dy, dx)),
/// flattened and normalized, as long as the delta.
pub fn scroll_direction(view: &GeTransformation, dx: f32, dy: f32, scale: f32) -> GeVector {
    let mut delta = GeVector::new(scale * dx, scale * dy, 0.0);
    let length = ((dx * dx + dy * dy) as f64).sqrt() as f32;
    let mut direction = view.apply_vector(delta);
    if direction.x == 0.0 && direction.y == 0.0 {
        delta = GeVector::new(delta.x, 0.0, dy);
        direction = view.apply_vector(delta);
        if direction.x == 0.0 && direction.y == 0.0 {
            delta = GeVector::new(0.0, dy, dx);
            direction = view.apply_vector(delta);
        }
    }
    let mut direction = GeVector::new(direction.x, direction.y, 0.0);
    direction.normalize();
    GeVector::new(direction.x * length, direction.y * length, direction.z)
}

/// Moves a point below one edge plane onto it along the plane's normal (when the line meets it).
fn onto_plane(target: Ge3DPoint, plane: &Ge3DPlane) -> Ge3DPoint {
    let line = Ge3DLine { start: target, direction: plane.normal };
    match line.intersects_plane(plane) {
        Some(t) => line.point_at(t),
        None => target,
    }
}

/// `Scroll`'s clamp: a target past the left (else the right) edge plane, then past the front (else the back) one,
/// moved back onto it.
pub fn clamp_to_map(target: Ge3DPoint, planes: &EeGfxEdgePlanes) -> Ge3DPoint {
    let mut target = target;
    if planes.left.is_point_on_or_below(target) {
        target = onto_plane(target, &planes.left);
    } else if planes.right.is_point_on_or_below(target) {
        target = onto_plane(target, &planes.right);
    }
    if planes.front.is_point_on_or_below(target) {
        target = onto_plane(target, &planes.front);
    } else if planes.back.is_point_on_or_below(target) {
        target = onto_plane(target, &planes.back);
    }
    target
}

/// The move's state `Update` steps.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxCameraStep {
    pub fraction: f32,
    pub rate: f32,
    pub wait: f32,
    pub waited: f32,
    pub changed: bool,
    pub locked: bool,
}

/// What `Update` does after stepping: nothing more, the path's next key, or the view.
pub const CAMERA_STEP_DONE: i32 = 0;
pub const CAMERA_STEP_NEXT_KEY: i32 = 1;
pub const CAMERA_STEP_VIEW: i32 = 2;

/// `Update`'s step of the move: a finished move waits out a playing path's key (then its next key), a changed one
/// jumps to its end, a running one advances by `elapsed * rate` up to 1; the last two then apply the view.
pub fn step(state: &mut EeGfxCameraStep, elapsed: f32) -> i32 {
    if state.fraction == 1.0 {
        if !state.changed {
            if !state.locked {
                return CAMERA_STEP_DONE;
            }
            if state.waited >= state.wait {
                return CAMERA_STEP_NEXT_KEY;
            }
            state.waited += elapsed;
            return CAMERA_STEP_DONE;
        }
    } else if !state.changed {
        state.fraction += elapsed * state.rate;
        if state.fraction > 1.0 {
            state.fraction = 1.0;
        }
        return CAMERA_STEP_VIEW;
    }
    state.fraction = 1.0;
    state.changed = false;
    CAMERA_STEP_VIEW
}

/// The orthographic view and its rectangle's half size (`Update`'s orthographic branch).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxOrthoView {
    pub view: GeTransformation,
    pub size: f32,
}

/// `Update`'s orthographic view: 40 back, turned to the fixed yaw and pitch, at the focus; its rectangle's half size
/// from the offset's distance (`offset_y`) between the near and far limits.
pub fn ortho_view(focus: Ge3DPoint, offset_y: f32) -> EeGfxOrthoView {
    let mut back = GeTransformation::IDENTITY;
    back.m[0][3] = 0.0;
    back.m[1][3] = ORTHO_HEIGHT;
    back.m[2][3] = 0.0;
    let mut turn = GeTransformation::IDENTITY;
    turn.set_orientation(RotationOrder::Ypr, ORTHO_YAW, ORTHO_PITCH, 0.0);
    let mut view = turn.compose(&back);
    translate(&mut view, focus);
    let mut ratio = (-offset_y - -ORTHO_NEAR) / (-ORTHO_FAR - -ORTHO_NEAR);
    if ratio < 0.5 {
        ratio = 1.0 - (0.5 - ratio);
    } else {
        ratio = ratio + ratio;
    }
    let size = (ORTHO_WIDTH * ratio) * ORTHO_SCALE;
    EeGfxOrthoView { view, size }
}

#[cfg(test)]
mod tests;
