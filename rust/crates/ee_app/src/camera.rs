//! The game camera (`EECameraControl`, ee/EECameraControl.h; `src/Empire Earth/Empire Earth/EECameraControl.cpp`)
//! whole in Rust: its plain members are [`EeAppCameraFields`], the base of the C++ class, and every body is here: the
//! moves (`MoveTo`, `Follow`, `Scroll`, `GoTo`, the setters, the turns), the path (`StartPath`, `NextKey`,
//! `ResetPath`), the per-tick `Update` and the clipping distances. The math is ee_gfx camera.rs's and ee_ge's (the
//! same functions the C++ called).
//!
//! The C++ class keeps the key list (`std::vector<EECameraKey>`; a key is [`EeAppCameraKey`]'s layout), the two
//! followed units (`EEUnitHandle`s, by id here: [`EE_APP_CAMERA_NO_UNIT`] for none or a unit that no longer resolves) and the world:
//! the units, the terrain mesh, the viewport and the switches, reached through [`CameraHost`]. `Update` returns what
//! the C++ applies after it (the view to the world and the viewport, the mini map's refresh), so no host call here
//! reaches back into the camera.
//!
//! `Update` runs in the simulation tick (`EEPhysics`): RUST_PLAN 2.1 applies (the C++'s `f32` operations in its order).

#![allow(non_snake_case)]

use ee_render::ge::transform::RotationOrder;
use ee_render::ge::types::{Ge3DLine, Ge3DPoint, GeQuaternion, GeTransformation, GeVector};
use ee_render::ge::um::{um_arccos, um_arctan2};
use ee_render::gfx::camera::{
    self as math, EeGfxCameraPose, EeGfxCameraRays, EeGfxCameraStep, EeGfxEdgePlanes, Ground,
};
use std::os::raw::c_long;

/// No followed unit (`EEUnitHandle::kNone`).
pub const EE_APP_CAMERA_NO_UNIT: u32 = 0xffff_ffff;
/// The x of "no location" (`GE3DPoint(-10000, 0, 0)`).
const NO_X: f32 = -10000.0;
#[allow(clippy::approx_constant)] // the original's .rdata value
const DEFAULT_YAW: f32 = 0.7853982;
const ZOOM_HEIGHT: f32 = 0.1;
const ZOOM_HEIGHT_FREE: f32 = 0.0;
const ZOOM_DISTANCE: f32 = -20.5;
const ZOOM_DISTANCE_FREE: f32 = -42.0;
const FOLLOW_RISE: f32 = -0.1;
const UPDATE_ZOOM: f32 = -1.1;
const HALF_PI: f32 = 1.5707964;

/// The followed units' slots.
pub const EE_APP_CAMERA_UNIT_A: u32 = 0;
pub const EE_APP_CAMERA_UNIT_B: u32 = 1;

/// The switches [`CameraHost::flags`] answers.
pub const EE_APP_CAMERA_FREE: u32 = 1;
pub const EE_APP_CAMERA_FAR_REAR: u32 = 2;
pub const EE_APP_CAMERA_CINEMATIC: u32 = 4;
pub const EE_APP_CAMERA_ORTHOGRAPHIC: u32 = 8;
pub const EE_APP_CAMERA_HAS_MAP: u32 = 16;

/// A unit's state bits ([`CameraHost::unit_state`]).
pub const EE_APP_CAMERA_EXISTS: u32 = 1;
/// `mHitPoints > 0`
pub const EE_APP_CAMERA_ALIVE: u32 = 2;
/// `mTileX != -10000 && GetTileY() != -10000`
pub const EE_APP_CAMERA_ON_MAP: u32 = 4;

/// What [`update`] leaves the C++ to do: nothing, the view (perspective), or the view with the orthographic
/// rectangle's half size.
pub const EE_APP_CAMERA_UPDATE_DONE: i32 = 0;
pub const EE_APP_CAMERA_UPDATE_VIEW: i32 = 1;
pub const EE_APP_CAMERA_UPDATE_ORTHO: i32 = 2;

/// `EECameraControl`'s plain members, in the class's order (the key list and the two unit handles stay C++).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppCameraFields {
    /// the start orientations (slerped to `mQ04C`/`mQ05C`)
    pub mQ014: GeQuaternion,
    pub mQ024: GeQuaternion,
    /// the start points (lerped to `mEndOffset`/`mEndFocus`)
    pub mStartOffset: Ge3DPoint,
    pub mStartFocus: Ge3DPoint,
    /// the target key's orientations
    pub mQ04C: GeQuaternion,
    pub mQ05C: GeQuaternion,
    pub mEndOffset: Ge3DPoint,
    pub mEndFocus: Ge3DPoint,
    pub mView: GeTransformation,
    /// x = -10000: none
    pub mTargetPoint: Ge3DPoint,
    pub mFraction: f32,
    pub mRate: f32,
    /// 0 (near) .. 1 (far), past 1 with the port's farther zoom
    pub mZoom: f32,
    /// the key's hold time
    pub mWait: f32,
    pub mWaited: f32,
    pub mTurnYaw: f32,
    pub mTurnPitch: f32,
    /// 3: the free camera mode
    pub mMode: c_long,
    pub mPathIndex: c_long,
    pub mChanged: bool,
    pub mIsFollowingUnitB: bool,
    /// a path plays
    pub mLocked: bool,
    pub mB0FF: bool,
}

/// `EECameraKey`'s layout (ee/EECameraKey.h, 0xa4 bytes): the view and offset turns, the offset, the focus, the hold
/// time, the speed (1 / duration) and the jump flag.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppCameraKey {
    pub mT00: GeTransformation,
    pub mT40: GeTransformation,
    pub mOffset: Ge3DPoint,
    pub mFocus: Ge3DPoint,
    pub mWait: f32,
    pub mRate: f32,
    pub mChanged: bool,
}

impl Default for EeAppCameraKey {
    /// `EECameraKey()`: two identities, two zero points (the hold time untouched there: 0 here)
    fn default() -> Self {
        EeAppCameraKey {
            mT00: GeTransformation::IDENTITY,
            mT40: GeTransformation::IDENTITY,
            mOffset: Ge3DPoint::default(),
            mFocus: Ge3DPoint::default(),
            mWait: 0.0,
            mRate: 0.0,
            mChanged: false,
        }
    }
}

impl Default for EeAppCameraFields {
    /// `EECameraControl()`: no target, mode 0, zoom 0.5, no path (the members its constructor leaves are 0 here)
    fn default() -> Self {
        let q = GeQuaternion { x: 0.0, y: 0.0, z: 0.0, w: 1.0 };
        EeAppCameraFields {
            mQ014: q,
            mQ024: q,
            mStartOffset: Ge3DPoint::default(),
            mStartFocus: Ge3DPoint::default(),
            mQ04C: q,
            mQ05C: q,
            mEndOffset: Ge3DPoint::default(),
            mEndFocus: Ge3DPoint::default(),
            mView: GeTransformation::IDENTITY,
            mTargetPoint: Ge3DPoint::new(NO_X, 0.0, 0.0),
            mFraction: 0.0,
            mRate: 0.0,
            mZoom: 0.5,
            mWait: 0.0,
            mWaited: 0.0,
            mTurnYaw: 0.0,
            mTurnPitch: 0.0,
            mMode: 0,
            mPathIndex: -1,
            mChanged: false,
            mIsFollowingUnitB: false,
            mLocked: false,
            mB0FF: false,
        }
    }
}

/// The camera's C++ side: the followed units' handles, the key list and the world. Every call is a read or a write of
/// something that is not the camera's record.
pub trait CameraHost {
    /// the unit of a slot (`mUnitA`/`mUnitB`) when it resolves, else [`EE_APP_CAMERA_NO_UNIT`]
    fn unit(&mut self, slot: u32) -> u32;
    /// stores a slot's handle (no reference counting)
    fn set_unit(&mut self, slot: u32, id: u32);
    fn reference(&mut self, id: u32);
    fn dereference(&mut self, id: u32);
    /// `EE_APP_CAMERA_EXISTS | ALIVE | ON_MAP` of a unit id (0 for one that does not resolve)
    fn unit_state(&mut self, id: u32) -> u32;
    /// a complex unit's container, else the unit itself
    fn container(&mut self, id: u32) -> u32;
    /// `GetUnitPoint`: the model centre at camera-focus height, else the tile centre
    fn unit_point(&mut self, id: u32) -> Ge3DPoint;
    /// a complex unit that does not face the camera: its physical model's location (`GetUnitTurn`'s), else None
    fn unit_location(&mut self, id: u32) -> Option<GeTransformation>;
    /// the followed unit's speed times the game speed when it is a complex unit outside a cinematic (`GoTo`'s
    /// following), else None
    fn follow_rate(&mut self, id: u32) -> Option<f32>;
    /// a unit of another player than the local one that the local player cannot see (false for one that does not
    /// resolve)
    fn hidden(&mut self, id: u32) -> bool;
    fn key_count(&mut self) -> usize;
    fn key(&mut self, index: usize) -> EeAppCameraKey;
    fn clear_keys(&mut self);
    /// `EE_APP_CAMERA_FREE | FAR_REAR | CINEMATIC | ORTHOGRAPHIC | HAS_MAP`
    fn flags(&mut self) -> u32;
    /// `gWorld.mTransformation` (the pitch of mode 2)
    fn world_transformation(&mut self) -> GeTransformation;
    /// `gWorld.mCameraYaw`
    fn world_camera_yaw(&mut self) -> f32;
    /// the terrain mesh (ee_gfx camera.rs's `Ground`)
    fn valid_tile(&mut self, x: f32, y: f32) -> bool;
    fn valid_point(&mut self, x: f32, y: f32) -> bool;
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
    fn edge_planes(&mut self) -> EeGfxEdgePlanes;
    /// `EEMap::GetScreenLine` of a screen point
    fn screen_line(&mut self, x: f32, y: f32) -> Ge3DLine;
    /// `EEMap::GetLocationOnLine(line, point, true)`: the point when a tile was hit
    fn location_on_line(&mut self, line: &Ge3DLine) -> Option<Ge3DPoint>;
    /// the viewport's x centre, y centre and half height
    fn viewport(&mut self) -> (f32, f32, f32);
    /// the viewport camera's position
    fn viewport_camera(&mut self) -> Ge3DPoint;
    fn front_clipping(&mut self) -> f32;
    /// the viewport's front, then rear clipping distance
    fn set_clipping(&mut self, front: f32, rear: f32);
}

/// The terrain mesh through the host.
struct HostGround<'a, H: CameraHost + ?Sized>(&'a mut H);

impl<H: CameraHost + ?Sized> Ground for HostGround<'_, H> {
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        self.0.valid_tile(x, y)
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        self.0.valid_point(x, y)
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        self.0.mesh_z(x, y)
    }
}

/// The mode as ee_gfx camera.rs takes it (`long` is 64 bits on LP64).
#[allow(clippy::unnecessary_cast)]
fn mode_of(mode: c_long) -> i32 {
    mode as i32
}

fn no_location() -> Ge3DPoint {
    Ge3DPoint::new(NO_X, 0.0, 0.0)
}

/// `GETransformation t; t.SetOrientationYPR(yaw, pitch, roll)` on an identity.
fn ypr(yaw: f32, pitch: f32, roll: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
    t
}

/// `GetPitch(zoom, mode)` (the world's transformation for mode 2).
pub fn pitch<H: CameraHost + ?Sized>(h: &mut H, zoom: f32, mode: c_long) -> f32 {
    let world = h.world_transformation();
    math::pitch(zoom, mode_of(mode), &world)
}

/// `GetOffset(step)`.
pub fn offset(c: &EeAppCameraFields, step: f32) -> Ge3DPoint {
    math::lerp(c.mStartOffset, c.mEndOffset, step)
}

/// `GetLocation(step)`.
pub fn location(c: &EeAppCameraFields, step: f32) -> Ge3DPoint {
    math::lerp(c.mStartFocus, c.mEndFocus, step)
}

/// `GetViewTurn(step)`.
pub fn view_turn(c: &EeAppCameraFields, step: f32) -> GeTransformation {
    c.mQ014.slerp(&c.mQ04C, step).to_transformation()
}

/// `GetOffsetTurn(step)`.
pub fn offset_turn(c: &EeAppCameraFields, step: f32) -> GeTransformation {
    c.mQ024.slerp(&c.mQ05C, step).to_transformation()
}

/// `ResetPath`: drops the keys and unlocks the camera.
pub fn reset_path<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H) {
    h.clear_keys();
    c.mLocked = false;
    c.mWaited = 0.0;
}

/// `ClearUnitA`: stops following the focused unit, releasing it.
pub fn clear_unit_a<H: CameraHost + ?Sized>(h: &mut H) {
    let id = h.unit(EE_APP_CAMERA_UNIT_A);
    if id != EE_APP_CAMERA_NO_UNIT {
        h.dereference(id);
        h.set_unit(EE_APP_CAMERA_UNIT_A, EE_APP_CAMERA_NO_UNIT);
    }
}

/// `ClearUnitB`: stops facing the target unit or point, releasing the unit.
pub fn clear_unit_b<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H) {
    let id = h.unit(EE_APP_CAMERA_UNIT_B);
    if id != EE_APP_CAMERA_NO_UNIT {
        h.dereference(id);
        h.set_unit(EE_APP_CAMERA_UNIT_B, EE_APP_CAMERA_NO_UNIT);
    }
    c.mIsFollowingUnitB = false;
    c.mTargetPoint.x = NO_X;
}

/// `SetClippingDistances`: the viewport's front and rear clipping distances from the view rays through the screen's
/// centre, top and bottom (fixed ones in the orthographic view).
pub fn set_clipping_distances<H: CameraHost + ?Sized>(c: &EeAppCameraFields, h: &mut H) {
    let flags = h.flags();
    let perspective = flags & EE_APP_CAMERA_ORTHOGRAPHIC == 0;
    let mut rays = EeGfxCameraRays::default();
    let mut camera = Ge3DPoint::default();
    if perspective {
        let (x, y, half) = h.viewport();
        rays.center = h.screen_line(x, y);
        rays.top = h.screen_line(x, y + half);
        rays.bottom = h.screen_line(x, y - half);
        camera = h.viewport_camera();
    }
    let clipping = math::clipping_distances(
        if perspective { Some(&rays) } else { None },
        camera,
        mode_of(c.mMode),
        c.mZoom,
        flags & EE_APP_CAMERA_FREE != 0,
        flags & EE_APP_CAMERA_FAR_REAR != 0,
    );
    h.set_clipping(clipping.front, clipping.rear);
}

/// `KeepPose`: the current interpolated pose becomes the next move's start.
pub fn keep_pose(c: &mut EeAppCameraFields) {
    c.mQ014 = c.mQ014.slerp(&c.mQ04C, c.mFraction);
    c.mQ024 = c.mQ024.slerp(&c.mQ05C, c.mFraction);
    c.mStartOffset = offset(c, c.mFraction);
    c.mStartFocus = location(c, c.mFraction);
}

/// `GoTo`: the move from the current pose to a key, at a speed from the distances or the followed unit's velocity.
pub fn go_to<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, key: &EeAppCameraKey) {
    let a = h.unit(EE_APP_CAMERA_UNIT_A);
    // the followed unit's speed is read only when following (as the original did)
    let follow = if a != EE_APP_CAMERA_NO_UNIT { h.follow_rate(a) } else { None };
    let key = *key;
    keep_pose(c);
    c.mQ04C = GeQuaternion::from_transformation(&key.mT00);
    c.mQ05C = GeQuaternion::from_transformation(&key.mT40);
    c.mEndOffset = key.mOffset;
    c.mEndFocus = key.mFocus;
    c.mWait = key.mWait;
    c.mChanged = key.mChanged;
    let g = math::go_to_speed(
        c.mStartOffset,
        c.mEndOffset,
        c.mStartFocus,
        c.mEndFocus,
        key.mRate,
        follow.is_some(),
        follow.unwrap_or(0.0),
    );
    if g.snap_offset {
        c.mStartOffset = c.mEndOffset;
    }
    if g.snap_focus {
        c.mStartFocus = c.mEndFocus;
    }
    c.mFraction = 0.0;
    c.mRate = g.speed;
    c.mWaited = 0.0;
}

/// A key from the current end pose: the turns as transformations, the end offset and focus, no wait, rate 1, the
/// jump flag kept.
fn end_key(c: &EeAppCameraFields) -> EeAppCameraKey {
    EeAppCameraKey {
        mT00: c.mQ04C.to_transformation(),
        mT40: c.mQ05C.to_transformation(),
        mOffset: c.mEndOffset,
        mFocus: c.mEndFocus,
        mWait: 0.0,
        mRate: 1.0,
        mChanged: c.mChanged,
    }
}

/// `NextKey`: the path's next key (locking the camera), or the path's end.
pub fn next_key<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H) {
    c.mPathIndex = c.mPathIndex.wrapping_add(1);
    if c.mPathIndex >= 0 && (c.mPathIndex as usize) < h.key_count() {
        let key = h.key(c.mPathIndex as usize);
        go_to(c, h, &key);
        c.mLocked = true;
        return;
    }
    reset_path(c, h);
}

/// `StartPath`: plays the path from its first key, dropping the followed units (not while a path plays).
pub fn start_path<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H) {
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    c.mPathIndex = -1;
    next_key(c, h);
}

/// `BuildView(step)`: the view at a point of the move, kept above the ground.
pub fn build_view<H: CameraHost + ?Sized>(c: &EeAppCameraFields, h: &mut H, step: f32) -> GeTransformation {
    let pose = EeGfxCameraPose {
        view_start: c.mQ014,
        view_end: c.mQ04C,
        offset_start: c.mQ024,
        offset_end: c.mQ05C,
        start_offset: c.mStartOffset,
        end_offset: c.mEndOffset,
        start_focus: c.mStartFocus,
        end_focus: c.mEndFocus,
    };
    let plain = h.unit(EE_APP_CAMERA_UNIT_A) == EE_APP_CAMERA_NO_UNIT
        && h.unit(EE_APP_CAMERA_UNIT_B) == EE_APP_CAMERA_NO_UNIT
        && h.flags() & EE_APP_CAMERA_FREE == 0
        && !c.mLocked;
    let front = h.front_clipping();
    math::build_view(&pose, step, mode_of(c.mMode), front, plain, &mut HostGround(h))
}

/// `GetGroundPoint(line)`: a scroll line's end clipped to the map edges, on the ground when close or below 0.
pub fn ground_point<H: CameraHost + ?Sized>(c: &EeAppCameraFields, h: &mut H, line: &Ge3DLine) -> Ge3DPoint {
    let planes = h.edge_planes();
    let start = offset(c, c.mFraction);
    math::ground_point(line, &planes, start, mode_of(c.mMode), &mut HostGround(h))
}

/// `GetUnitTurn(unit)`: the unit's heading plus the follow turn and the zoom's pitch, else the current offset turn.
pub fn unit_turn<H: CameraHost + ?Sized>(c: &EeAppCameraFields, h: &mut H, id: u32) -> GeTransformation {
    let pitch = pitch(h, c.mZoom, c.mMode);
    match h.unit_location(id) {
        Some(mut location) => {
            location.m[0][3] = 0.0;
            location.m[1][3] = 0.0;
            location.m[2][3] = 0.0;
            location.rotate(c.mTurnYaw, c.mTurnPitch, 0.0);
            location.compose(&ypr(0.0, pitch, 0.0))
        }
        None => c.mQ05C.to_transformation(),
    }
}

/// `Revolve(angles)`: turns the view turn by a yaw and a pitch as a new move.
pub fn revolve<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, yaw: f32, pitch: f32) {
    if c.mLocked {
        return;
    }
    let rotation = c.mQ04C.to_transformation();
    let result = rotation.compose(&ypr(0.0, pitch, 0.0));
    let mut key = end_key(c);
    key.mT00 = ypr(yaw, 0.0, 0.0).compose(&result);
    go_to(c, h, &key);
}

/// `Rotate(angles)`: turns the offset turn by a yaw and a pitch as a new move.
pub fn rotate<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, yaw: f32, pitch: f32) {
    if c.mLocked {
        return;
    }
    let rotation = c.mQ05C.to_transformation();
    let result = rotation.compose(&ypr(0.0, pitch, 0.0));
    let mut key = end_key(c);
    key.mT40 = ypr(yaw, 0.0, 0.0).compose(&result);
    go_to(c, h, &key);
}

/// Whether a unit passed to `MoveTo` is usable: there, alive and on the map.
fn usable<H: CameraHost + ?Sized>(h: &mut H, id: u32) -> bool {
    let all = EE_APP_CAMERA_EXISTS | EE_APP_CAMERA_ALIVE | EE_APP_CAMERA_ON_MAP;
    id != EE_APP_CAMERA_NO_UNIT && h.unit_state(id) & all == all
}

/// Whether a container (or the unit itself) is on the map (`mTileX != -10000 && GetTileY() != -10000`).
fn on_map<H: CameraHost + ?Sized>(h: &mut H, id: u32) -> bool {
    let all = EE_APP_CAMERA_EXISTS | EE_APP_CAMERA_ON_MAP;
    id != EE_APP_CAMERA_NO_UNIT && h.unit_state(id) & all == all
}

/// `MoveTo`'s arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppCameraMove {
    pub unit_a: u32,
    pub location_a: Ge3DPoint,
    pub flag_a: bool,
    pub duration: f32,
    pub unit_b: u32,
    pub location_b: Ge3DPoint,
    pub flag_b: bool,
    pub zoom: f32,
    pub turn_yaw: f32,
    pub turn_pitch: f32,
}

/// `MoveTo`: a move that focuses on a unit or point and faces another unit or point, optionally following them, with
/// a zoom, a duration and a follow turn.
pub fn move_to<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, m: &EeAppCameraMove) {
    let m = *m;
    let mut target = m.location_b;
    let mut unit_a = m.unit_a;
    let mut unit_b = m.unit_b;
    let mut turn = false;
    if !usable(h, m.unit_a) {
        unit_a = EE_APP_CAMERA_NO_UNIT;
    }
    if !usable(h, m.unit_b) {
        unit_b = EE_APP_CAMERA_NO_UNIT;
    }
    if c.mLocked {
        return;
    }
    let flags = h.flags();
    let free = flags & EE_APP_CAMERA_FREE != 0;
    c.mTurnYaw = m.turn_yaw;
    c.mTurnPitch = m.turn_pitch;
    let mut key = EeAppCameraKey { mOffset: c.mEndOffset, mFocus: c.mEndFocus, ..Default::default() };
    target.x = NO_X;
    key.mT40 = c.mQ05C.to_transformation();
    if m.zoom >= -1.0 {
        let zoom = if m.zoom < 0.0 { 0.0 } else { m.zoom };
        let far = if c.mMode != 3 && !free && flags & EE_APP_CAMERA_FAR_REAR == 0 {
            ZOOM_DISTANCE
        } else {
            ZOOM_DISTANCE_FREE
        };
        let height = if c.mMode != 3 && !free { ZOOM_HEIGHT } else { ZOOM_HEIGHT_FREE };
        key.mOffset.y = (far - height) * zoom + height;
    }
    if unit_a != EE_APP_CAMERA_NO_UNIT {
        if h.unit(EE_APP_CAMERA_UNIT_A) != EE_APP_CAMERA_NO_UNIT {
            clear_unit_a(h);
        }
        if m.zoom >= -1.0 {
            c.mZoom = m.zoom;
        }
        unit_a = h.container(unit_a);
        if on_map(h, unit_a) {
            if m.flag_a {
                h.reference(unit_a);
                h.set_unit(EE_APP_CAMERA_UNIT_A, unit_a);
            }
            key.mFocus = h.unit_point(unit_a);
            if c.mMode == 3 || free {
                key.mFocus.z += key.mOffset.y * FOLLOW_RISE;
            }
        }
    } else if m.location_a.x != NO_X {
        if h.unit(EE_APP_CAMERA_UNIT_A) != EE_APP_CAMERA_NO_UNIT {
            clear_unit_a(h);
        }
        key.mFocus = m.location_a;
    }
    if unit_b != EE_APP_CAMERA_NO_UNIT {
        if h.unit(EE_APP_CAMERA_UNIT_B) != EE_APP_CAMERA_NO_UNIT {
            clear_unit_b(c, h);
        }
        if m.zoom >= -1.0 {
            c.mZoom = m.zoom;
        }
        unit_b = h.container(unit_b);
        if on_map(h, unit_b) {
            if m.flag_b {
                if h.unit(EE_APP_CAMERA_UNIT_B) != unit_b {
                    h.reference(unit_b);
                    h.set_unit(EE_APP_CAMERA_UNIT_B, unit_b);
                }
                c.mIsFollowingUnitB = true;
            } else {
                c.mIsFollowingUnitB = false;
            }
            target = h.unit_point(unit_b);
        }
    } else if m.location_b.x != NO_X {
        if h.unit(EE_APP_CAMERA_UNIT_B) != EE_APP_CAMERA_NO_UNIT {
            clear_unit_b(c, h);
        }
        target = m.location_b;
        if m.flag_b {
            c.mTargetPoint = m.location_b;
        }
    }
    'done: {
        if target.x != NO_X {
            if unit_a != EE_APP_CAMERA_NO_UNIT && unit_a == unit_b {
                key.mT40 = unit_turn(c, h, unit_b);
            } else {
                if m.location_a.x != NO_X && m.location_b.x != NO_X {
                    if c.mMode == 2 || m.zoom < -1.0 {
                        break 'done;
                    }
                    if c.mMode == 3 || free {
                        key.mFocus.z += key.mOffset.y * FOLLOW_RISE;
                    }
                    c.mZoom = m.zoom;
                    let p = pitch(h, m.zoom, c.mMode);
                    key.mT40.set_orientation(RotationOrder::Ypr, DEFAULT_YAW, p, 0.0);
                }
                turn = true;
            }
        } else if c.mMode != 2 && m.zoom >= -1.0 {
            let p = pitch(h, m.zoom, c.mMode);
            if c.mTurnYaw == 0.0 {
                key.mT40.set_orientation(RotationOrder::Ypr, DEFAULT_YAW, p, 0.0);
            } else {
                let yaw = h.world_camera_yaw();
                key.mT40.set_orientation(RotationOrder::Ypr, yaw, p, 0.0);
            }
        }
    }
    key.mT00 = GeTransformation::IDENTITY;
    key.mWait = 0.0;
    key.mRate = if m.duration != 0.0 { 1.0 / m.duration } else { 0.0 };
    key.mChanged = m.duration == 0.0;
    go_to(c, h, &key);
    if turn {
        let mut direction = GeVector::new(target.x - key.mFocus.x, target.y - key.mFocus.y, target.z - key.mFocus.z);
        direction.normalize();
        key.mT40.set_orientation(
            RotationOrder::Ypr,
            um_arctan2(-direction.x, direction.y),
            um_arccos(-direction.z) - HALF_PI,
            0.0,
        );
        go_to(c, h, &key);
    }
}

/// The follow move `Update`, `SetZoom` and `SetMode` restart: the followed units and target as they are.
fn refollow<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, duration: f32, zoom: f32) {
    let a = h.unit(EE_APP_CAMERA_UNIT_A);
    let b = h.unit(EE_APP_CAMERA_UNIT_B);
    let m = EeAppCameraMove {
        unit_a: a,
        location_a: no_location(),
        flag_a: a != EE_APP_CAMERA_NO_UNIT,
        duration,
        unit_b: b,
        location_b: c.mTargetPoint,
        flag_b: !(c.mTargetPoint.x == NO_X && b == EE_APP_CAMERA_NO_UNIT),
        zoom,
        turn_yaw: c.mTurnYaw,
        turn_pitch: c.mTurnPitch,
    };
    move_to(c, h, &m);
}

/// `SetZoom`: the zoom, re-aimed at the current focus and target over 0.3 s (not while a path plays).
pub fn set_zoom<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, zoom: f32) {
    if c.mLocked {
        return;
    }
    c.mZoom = zoom;
    refollow(c, h, 0.3, zoom);
}

/// `SetMode`: the mode, then the zoom again.
pub fn set_mode<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, mode: c_long) {
    c.mMode = mode;
    set_zoom(c, h, c.mZoom);
}

/// `SetChanged`: the camera jumps to its target on the next update, unless a path plays.
pub fn set_changed(c: &mut EeAppCameraFields) {
    if !c.mLocked {
        c.mChanged = true;
    }
}

/// `Scroll(dx, dy)`: the focus moved over the ground by a screen delta scaled by the zoom, kept inside the map;
/// leaves the free camera mode outside cinematics.
pub fn scroll<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, dx: f32, dy: f32) {
    let (x, y, half) = h.viewport();
    let scale = c.mZoom + 0.5; // the zoom before a mode switch below resets it
    let flags = h.flags();
    let minimum = math::minimum_height(mode_of(c.mMode));
    let p = pitch(h, c.mZoom, c.mMode);
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    c.mTurnYaw = 0.0;
    c.mTurnPitch = 0.0;
    if flags & EE_APP_CAMERA_CINEMATIC == 0 && c.mMode == 3 {
        set_mode(c, h, 0);
        set_zoom(c, h, 1.0);
    }
    let direction = math::scroll_direction(&c.mView, dx, dy, scale);
    let mut target = location(c, c.mFraction);
    let line = Ge3DLine { start: target, direction };
    target = ground_point(c, h, &line);
    let start = offset(c, c.mFraction);
    if math::scroll_is_near(start) {
        let line = h.screen_line(x, y + half);
        if let Some(mut ground) = h.location_on_line(&line) {
            ground.z += minimum;
            if ground.z > target.z {
                target.z = ground.z;
            }
        }
    }
    if flags & EE_APP_CAMERA_HAS_MAP != 0 {
        let planes = h.edge_planes();
        target = math::clamp_to_map(target, &planes);
    }
    let mut key = end_key(c);
    key.mFocus = target;
    if c.mMode != 2 {
        key.mT40 = ypr(DEFAULT_YAW, p, 0.0);
        c.mChanged = true;
    }
    key.mChanged = c.mChanged;
    go_to(c, h, &key);
    c.mStartFocus.x = c.mEndFocus.x;
    c.mStartFocus.y = c.mEndFocus.y;
}

/// `SetOffset(point, immediate)`: the camera offset as a new move, dropping the followed units.
pub fn set_offset<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, point: Ge3DPoint, immediate: bool) {
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    let mut key = end_key(c);
    key.mOffset = point;
    go_to(c, h, &key);
    if immediate {
        c.mStartOffset = c.mEndOffset;
    }
}

/// `SetViewTurn(t, immediate)`.
pub fn set_view_turn<H: CameraHost + ?Sized>(
    c: &mut EeAppCameraFields,
    h: &mut H,
    t: &GeTransformation,
    immediate: bool,
) {
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    let mut key = end_key(c);
    key.mT00 = *t;
    go_to(c, h, &key);
    if immediate {
        c.mQ014 = c.mQ04C;
    }
}

/// `SetOffsetTurn(t, immediate)`.
pub fn set_offset_turn<H: CameraHost + ?Sized>(
    c: &mut EeAppCameraFields,
    h: &mut H,
    t: &GeTransformation,
    immediate: bool,
) {
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    let mut key = end_key(c);
    key.mT40 = *t;
    go_to(c, h, &key);
    if immediate {
        c.mQ024 = c.mQ05C;
    }
}

/// `SetLocation(location, immediate, follow terrain)`: the focus as a new move, on the terrain when asked.
pub fn set_location<H: CameraHost + ?Sized>(
    c: &mut EeAppCameraFields,
    h: &mut H,
    location: Ge3DPoint,
    immediate: bool,
    follow_terrain: bool,
) {
    let mut location = location;
    let minimum = math::minimum_height(mode_of(c.mMode));
    if c.mLocked {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    let flags = h.flags();
    if flags & EE_APP_CAMERA_ORTHOGRAPHIC == 0 && follow_terrain {
        if h.valid_tile(location.x, location.y) {
            location.z = h.mesh_z(location.x, location.y) + minimum;
            if location.z < 0.0 && flags & EE_APP_CAMERA_FREE == 0 {
                location.z = minimum;
            }
        } else {
            location.z = minimum;
        }
    }
    let mut key = end_key(c);
    key.mFocus = location;
    go_to(c, h, &key);
    if immediate {
        c.mStartFocus = c.mEndFocus;
    }
}

/// `Follow(unit, face, duration, yaw, pitch)`: follows a unit (and faces it), unless it is another player's unit the
/// local player cannot see.
pub fn follow<H: CameraHost + ?Sized>(
    c: &mut EeAppCameraFields,
    h: &mut H,
    id: u32,
    face: bool,
    duration: f32,
    yaw: f32,
    pitch: f32,
) {
    if id != EE_APP_CAMERA_NO_UNIT && h.hidden(id) {
        return;
    }
    clear_unit_a(h);
    clear_unit_b(c, h);
    c.mTurnYaw = yaw;
    c.mTurnPitch = pitch;
    let m = EeAppCameraMove {
        unit_a: id,
        location_a: no_location(),
        flag_a: true,
        duration,
        unit_b: if face { id } else { EE_APP_CAMERA_NO_UNIT },
        location_b: no_location(),
        flag_b: face,
        zoom: c.mZoom,
        turn_yaw: yaw,
        turn_pitch: pitch,
    };
    move_to(c, h, &m);
}

/// Whether `Update` drops a followed unit: dead, off the map, or (outside the free camera) another player's unit the
/// local player cannot see.
fn lost<H: CameraHost + ?Sized>(c: &EeAppCameraFields, h: &mut H, id: u32) -> bool {
    let state = h.unit_state(id);
    state & EE_APP_CAMERA_ALIVE == 0 || state & EE_APP_CAMERA_ON_MAP == 0 || (c.mMode != 3 && h.hidden(id))
}

/// `Update(elapsed)`: drops followed units that died or went out of sight, refollows, steps the move or the path and
/// builds the view. Returns what the C++ applies: nothing, the view, or the view with the orthographic rectangle's
/// half size (`ortho_size`).
pub fn update<H: CameraHost + ?Sized>(c: &mut EeAppCameraFields, h: &mut H, elapsed: f32, ortho_size: &mut f32) -> i32 {
    let a = h.unit(EE_APP_CAMERA_UNIT_A);
    if a != EE_APP_CAMERA_NO_UNIT && lost(c, h, a) {
        clear_unit_a(h);
    }
    let b = h.unit(EE_APP_CAMERA_UNIT_B);
    if b != EE_APP_CAMERA_NO_UNIT && lost(c, h, b) {
        clear_unit_b(c, h);
    }
    if h.unit(EE_APP_CAMERA_UNIT_A) != EE_APP_CAMERA_NO_UNIT
        || h.unit(EE_APP_CAMERA_UNIT_B) != EE_APP_CAMERA_NO_UNIT
        || c.mTargetPoint.x != NO_X
    {
        let duration = if c.mChanged { 0.0 } else { 1.0 };
        refollow(c, h, duration, UPDATE_ZOOM);
    }
    let flags = h.flags();
    if flags & EE_APP_CAMERA_HAS_MAP == 0 {
        return EE_APP_CAMERA_UPDATE_DONE;
    }
    let mut step = EeGfxCameraStep {
        fraction: c.mFraction,
        rate: c.mRate,
        wait: c.mWait,
        waited: c.mWaited,
        changed: c.mChanged,
        locked: c.mLocked,
    };
    let action = math::step(&mut step, elapsed);
    c.mFraction = step.fraction;
    c.mWaited = step.waited;
    c.mChanged = step.changed;
    if action == math::CAMERA_STEP_NEXT_KEY {
        next_key(c, h);
        return EE_APP_CAMERA_UPDATE_DONE;
    }
    if action == math::CAMERA_STEP_DONE {
        return EE_APP_CAMERA_UPDATE_DONE;
    }
    if flags & EE_APP_CAMERA_ORTHOGRAPHIC == 0 {
        c.mView = build_view(c, h, c.mFraction);
        return EE_APP_CAMERA_UPDATE_VIEW;
    }
    let point = location(c, c.mFraction);
    let off = offset(c, c.mFraction);
    let view = math::ortho_view(point, off.y);
    c.mView = view.view;
    *ortho_size = view.size;
    EE_APP_CAMERA_UPDATE_ORTHO
}
