//! The camera's C ABI (include/rust/ee_app.h): one callback host by operation, and an entry per `EECameraControl`
//! body. Each entry copies the record in, runs the body and copies it back: no host operation reads or writes the
//! camera's record (camera.rs), so no Rust reference to it lives across a call into C++.

use crate::camera::{self, CameraHost, EeAppCameraFields, EeAppCameraKey, EeAppCameraMove};
use ee_ge::types::Ge3DPlane;
use ee_ge::types::{Ge3DLine, Ge3DPoint, GeQuaternion, GeTransformation};
use ee_gfx::camera::EeGfxEdgePlanes;
use std::ffi::c_void;
use std::os::raw::c_long;

/// The terrain mesh's four edge planes (ee_gfx camera.rs's `EeGfxEdgePlanes`, repeated so that the C header does not
/// need ee_gfx's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppCameraPlanes {
    pub front: Ge3DPlane,
    pub back: Ge3DPlane,
    pub left: Ge3DPlane,
    pub right: Ge3DPlane,
}

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppCameraIo {
    /// a unit id (in and out), or a key's index
    pub id: u32,
    /// a followed unit's slot (`EE_APP_CAMERA_UNIT_A`/`B`)
    pub slot: u32,
    /// a query's yes or no; a unit's state bits, a key count, the switches in `id`
    pub flag: bool,
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub point: Ge3DPoint,
    pub t: GeTransformation,
    pub line: Ge3DLine,
    pub planes: EeAppCameraPlanes,
    pub key: EeAppCameraKey,
}

impl Default for EeAppCameraIo {
    fn default() -> Self {
        EeAppCameraIo {
            id: camera::EE_APP_CAMERA_NO_UNIT,
            slot: 0,
            flag: false,
            x: 0.0,
            y: 0.0,
            z: 0.0,
            point: Ge3DPoint::default(),
            t: GeTransformation::IDENTITY,
            line: Ge3DLine::default(),
            planes: EeAppCameraPlanes::default(),
            key: EeAppCameraKey::default(),
        }
    }
}

/// The host: `(ctx, op, io)`.
pub type EeAppCameraFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppCameraIo) -> i32;

// ---- the operations (io's fields: in -> out)
/// slot -> id (`EE_APP_CAMERA_NO_UNIT` when the handle does not resolve)
pub const EE_APP_CAMERA_OP_UNIT: i32 = 1;
/// slot, id: the slot's handle
pub const EE_APP_CAMERA_OP_SET_UNIT: i32 = 2;
/// id: `Reference` / `Dereference`
pub const EE_APP_CAMERA_OP_REFERENCE: i32 = 3;
pub const EE_APP_CAMERA_OP_DEREFERENCE: i32 = 4;
/// id -> id: the state bits (`EE_APP_CAMERA_EXISTS | ALIVE | ON_MAP`)
pub const EE_APP_CAMERA_OP_UNIT_STATE: i32 = 5;
/// id -> id: the complex unit's container, else the unit
pub const EE_APP_CAMERA_OP_CONTAINER: i32 = 6;
/// id -> point (`GetUnitPoint`)
pub const EE_APP_CAMERA_OP_UNIT_POINT: i32 = 7;
/// id -> flag, t: a complex unit that does not face the camera, its physical model's location
pub const EE_APP_CAMERA_OP_UNIT_LOCATION: i32 = 8;
/// id -> flag, x: following (complex, no cinematic), the velocity times the game speed
pub const EE_APP_CAMERA_OP_FOLLOW_RATE: i32 = 9;
/// id -> flag: another player's unit the local player cannot see
pub const EE_APP_CAMERA_OP_HIDDEN: i32 = 10;
/// -> id: the key count; id -> key; the keys cleared
pub const EE_APP_CAMERA_OP_KEY_COUNT: i32 = 11;
pub const EE_APP_CAMERA_OP_KEY: i32 = 12;
pub const EE_APP_CAMERA_OP_CLEAR_KEYS: i32 = 13;
/// -> id: the switches (`EE_APP_CAMERA_FREE` ...)
pub const EE_APP_CAMERA_OP_FLAGS: i32 = 14;
/// -> t: `gWorld.mTransformation`; -> x: `gWorld.mCameraYaw`
pub const EE_APP_CAMERA_OP_WORLD_TRANSFORMATION: i32 = 15;
pub const EE_APP_CAMERA_OP_WORLD_CAMERA_YAW: i32 = 16;
/// x, y -> flag (`GetIsValidTile`/`GetIsValidPoint` after `UMFloorUnsigned`); x, y -> z (`GetMeshZ`)
pub const EE_APP_CAMERA_OP_VALID_TILE: i32 = 17;
pub const EE_APP_CAMERA_OP_VALID_POINT: i32 = 18;
pub const EE_APP_CAMERA_OP_MESH_Z: i32 = 19;
/// -> planes
pub const EE_APP_CAMERA_OP_EDGE_PLANES: i32 = 20;
/// x, y -> line (`GetScreenLine`); line -> flag, point (`GetLocationOnLine(.., true)`)
pub const EE_APP_CAMERA_OP_SCREEN_LINE: i32 = 21;
pub const EE_APP_CAMERA_OP_LOCATION_ON_LINE: i32 = 22;
/// -> x, y, z: the viewport's x centre, y centre, half height; -> point: its camera; -> x: the front clipping
pub const EE_APP_CAMERA_OP_VIEWPORT: i32 = 23;
pub const EE_APP_CAMERA_OP_VIEWPORT_CAMERA: i32 = 24;
pub const EE_APP_CAMERA_OP_FRONT_CLIPPING: i32 = 25;
/// x, y: the front and rear clipping distances
pub const EE_APP_CAMERA_OP_SET_CLIPPING: i32 = 26;

struct Host {
    ctx: *mut c_void,
    call: EeAppCameraFn,
}

impl Host {
    fn op(&mut self, op: i32, io: EeAppCameraIo) -> EeAppCameraIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.call)(self.ctx, op, &mut io) };
        io
    }
    fn with_id(&mut self, op: i32, id: u32) -> EeAppCameraIo {
        self.op(op, EeAppCameraIo { id, ..Default::default() })
    }
    fn at(&mut self, op: i32, x: f32, y: f32) -> EeAppCameraIo {
        self.op(op, EeAppCameraIo { x, y, ..Default::default() })
    }
}

impl CameraHost for Host {
    fn unit(&mut self, slot: u32) -> u32 {
        self.op(EE_APP_CAMERA_OP_UNIT, EeAppCameraIo { slot, ..Default::default() }).id
    }
    fn set_unit(&mut self, slot: u32, id: u32) {
        self.op(EE_APP_CAMERA_OP_SET_UNIT, EeAppCameraIo { slot, id, ..Default::default() });
    }
    fn reference(&mut self, id: u32) {
        self.with_id(EE_APP_CAMERA_OP_REFERENCE, id);
    }
    fn dereference(&mut self, id: u32) {
        self.with_id(EE_APP_CAMERA_OP_DEREFERENCE, id);
    }
    fn unit_state(&mut self, id: u32) -> u32 {
        self.with_id(EE_APP_CAMERA_OP_UNIT_STATE, id).id
    }
    fn container(&mut self, id: u32) -> u32 {
        self.with_id(EE_APP_CAMERA_OP_CONTAINER, id).id
    }
    fn unit_point(&mut self, id: u32) -> Ge3DPoint {
        self.with_id(EE_APP_CAMERA_OP_UNIT_POINT, id).point
    }
    fn unit_location(&mut self, id: u32) -> Option<GeTransformation> {
        let io = self.with_id(EE_APP_CAMERA_OP_UNIT_LOCATION, id);
        io.flag.then_some(io.t)
    }
    fn follow_rate(&mut self, id: u32) -> Option<f32> {
        let io = self.with_id(EE_APP_CAMERA_OP_FOLLOW_RATE, id);
        io.flag.then_some(io.x)
    }
    fn hidden(&mut self, id: u32) -> bool {
        self.with_id(EE_APP_CAMERA_OP_HIDDEN, id).flag
    }
    fn key_count(&mut self) -> usize {
        self.op(EE_APP_CAMERA_OP_KEY_COUNT, EeAppCameraIo::default()).id as usize
    }
    fn key(&mut self, index: usize) -> EeAppCameraKey {
        self.with_id(EE_APP_CAMERA_OP_KEY, index as u32).key
    }
    fn clear_keys(&mut self) {
        self.op(EE_APP_CAMERA_OP_CLEAR_KEYS, EeAppCameraIo::default());
    }
    fn flags(&mut self) -> u32 {
        self.op(EE_APP_CAMERA_OP_FLAGS, EeAppCameraIo::default()).id
    }
    fn world_transformation(&mut self) -> GeTransformation {
        self.op(EE_APP_CAMERA_OP_WORLD_TRANSFORMATION, EeAppCameraIo::default()).t
    }
    fn world_camera_yaw(&mut self) -> f32 {
        self.op(EE_APP_CAMERA_OP_WORLD_CAMERA_YAW, EeAppCameraIo::default()).x
    }
    fn valid_tile(&mut self, x: f32, y: f32) -> bool {
        self.at(EE_APP_CAMERA_OP_VALID_TILE, x, y).flag
    }
    fn valid_point(&mut self, x: f32, y: f32) -> bool {
        self.at(EE_APP_CAMERA_OP_VALID_POINT, x, y).flag
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        self.at(EE_APP_CAMERA_OP_MESH_Z, x, y).z
    }
    fn edge_planes(&mut self) -> EeGfxEdgePlanes {
        let p = self.op(EE_APP_CAMERA_OP_EDGE_PLANES, EeAppCameraIo::default()).planes;
        EeGfxEdgePlanes { front: p.front, back: p.back, left: p.left, right: p.right }
    }
    fn screen_line(&mut self, x: f32, y: f32) -> Ge3DLine {
        self.at(EE_APP_CAMERA_OP_SCREEN_LINE, x, y).line
    }
    fn location_on_line(&mut self, line: &Ge3DLine) -> Option<Ge3DPoint> {
        let io = self.op(EE_APP_CAMERA_OP_LOCATION_ON_LINE, EeAppCameraIo { line: *line, ..Default::default() });
        io.flag.then_some(io.point)
    }
    fn viewport(&mut self) -> (f32, f32, f32) {
        let io = self.op(EE_APP_CAMERA_OP_VIEWPORT, EeAppCameraIo::default());
        (io.x, io.y, io.z)
    }
    fn viewport_camera(&mut self) -> Ge3DPoint {
        self.op(EE_APP_CAMERA_OP_VIEWPORT_CAMERA, EeAppCameraIo::default()).point
    }
    fn front_clipping(&mut self) -> f32 {
        self.op(EE_APP_CAMERA_OP_FRONT_CLIPPING, EeAppCameraIo::default()).x
    }
    fn set_clipping(&mut self, front: f32, rear: f32) {
        self.at(EE_APP_CAMERA_OP_SET_CLIPPING, front, rear);
    }
}

/// The record copied in, the body run over the host, the record copied back. Private: every caller is an entry
/// below, whose contract is that `rec` is a live record and `call` answers for `ctx` throughout.
fn with<R>(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    body: impl FnOnce(&mut EeAppCameraFields, &mut Host) -> R,
) -> R {
    let mut c = unsafe { *rec };
    let mut host = Host { ctx, call };
    let r = body(&mut c, &mut host);
    unsafe { *rec = c };
    r
}

/// `EECameraControl()`'s plain members.
///
/// # Safety
/// `rec` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_default(rec: *mut EeAppCameraFields) {
    unsafe { *rec = EeAppCameraFields::default() };
}

/// `GetOffset` (which 0) or `GetLocation` (1) at a step.
///
/// # Safety
/// live records.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_point(
    rec: *const EeAppCameraFields,
    which: u32,
    step: f32,
    out: *mut Ge3DPoint,
) {
    let c = unsafe { *rec };
    let p = if which == 0 { camera::offset(&c, step) } else { camera::location(&c, step) };
    unsafe { *out = p };
}

/// `GetViewTurn` (which 0) or `GetOffsetTurn` (1) at a step.
///
/// # Safety
/// live records.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_turn(
    rec: *const EeAppCameraFields,
    which: u32,
    step: f32,
    out: *mut GeTransformation,
) {
    let c = unsafe { *rec };
    let t = if which == 0 { camera::view_turn(&c, step) } else { camera::offset_turn(&c, step) };
    unsafe { *out = t };
}

/// `SlerpViewTurn` (which 0) or `SlerpOffsetTurn` (1) at a step.
///
/// # Safety
/// live records.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_slerp(
    rec: *const EeAppCameraFields,
    which: u32,
    step: f32,
    out: *mut GeQuaternion,
) {
    let c = unsafe { *rec };
    let q = if which == 0 { c.mQ014.slerp(&c.mQ04C, step) } else { c.mQ024.slerp(&c.mQ05C, step) };
    unsafe { *out = q };
}

/// `KeepPose`.
///
/// # Safety
/// `rec` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_keep_pose(rec: *mut EeAppCameraFields) {
    let mut c = unsafe { *rec };
    camera::keep_pose(&mut c);
    unsafe { *rec = c };
}

/// `SetChanged`.
///
/// # Safety
/// `rec` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_changed(rec: *mut EeAppCameraFields) {
    let mut c = unsafe { *rec };
    camera::set_changed(&mut c);
    unsafe { *rec = c };
}

/// `ResetPath`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_reset_path(rec: *mut EeAppCameraFields, ctx: *mut c_void, call: EeAppCameraFn) {
    with(rec, ctx, call, camera::reset_path)
}

/// `ClearUnitA`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_clear_unit_a(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
) {
    with(rec, ctx, call, |_c, h| camera::clear_unit_a(h))
}

/// `ClearUnitB`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_clear_unit_b(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
) {
    with(rec, ctx, call, camera::clear_unit_b)
}

/// `SetClippingDistances`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_clipping_distances(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
) {
    with(rec, ctx, call, |c, h| camera::set_clipping_distances(c, h))
}

/// `NextKey`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_next_key(rec: *mut EeAppCameraFields, ctx: *mut c_void, call: EeAppCameraFn) {
    with(rec, ctx, call, camera::next_key)
}

/// `StartPath`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_start_path(rec: *mut EeAppCameraFields, ctx: *mut c_void, call: EeAppCameraFn) {
    with(rec, ctx, call, camera::start_path)
}

/// `BuildView(out, step)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_build_view(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    step: f32,
    out: *mut GeTransformation,
) {
    with(rec, ctx, call, |c, h| {
        let t = camera::build_view(c, h, step);
        unsafe { *out = t }
    })
}

/// `GetGroundPoint(line, out)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_ground_point(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    line: *const Ge3DLine,
    out: *mut Ge3DPoint,
) {
    with(rec, ctx, call, |c, h| {
        let line = unsafe { *line };
        let p = camera::ground_point(c, h, &line);
        unsafe { *out = p }
    })
}

/// `GetUnitTurn(unit, out)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_unit_turn(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    id: u32,
    out: *mut GeTransformation,
) {
    with(rec, ctx, call, |c, h| {
        let t = camera::unit_turn(c, h, id);
        unsafe { *out = t }
    })
}

/// `Revolve(angles)`: the yaw and the pitch.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_revolve(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    yaw: f32,
    pitch: f32,
) {
    with(rec, ctx, call, |c, h| camera::revolve(c, h, yaw, pitch))
}

/// `Rotate(angles)`: the yaw and the pitch.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_rotate(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    yaw: f32,
    pitch: f32,
) {
    with(rec, ctx, call, |c, h| camera::rotate(c, h, yaw, pitch))
}

/// `MoveTo`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_move_to(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    m: *const EeAppCameraMove,
) {
    with(rec, ctx, call, |c, h| {
        let m = unsafe { *m };
        camera::move_to(c, h, &m)
    })
}

/// `Scroll(dx, dy)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_scroll(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    dx: f32,
    dy: f32,
) {
    with(rec, ctx, call, |c, h| camera::scroll(c, h, dx, dy))
}

/// `GoTo(key)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_go_to(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    key: *const EeAppCameraKey,
) {
    with(rec, ctx, call, |c, h| {
        let key = unsafe { *key };
        camera::go_to(c, h, &key)
    })
}

/// `SetOffset(point, immediate)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_offset(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    point: *const Ge3DPoint,
    immediate: bool,
) {
    with(rec, ctx, call, |c, h| {
        let point = unsafe { *point };
        camera::set_offset(c, h, point, immediate)
    })
}

/// `SetViewTurn(t, immediate)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_view_turn(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    t: *const GeTransformation,
    immediate: bool,
) {
    with(rec, ctx, call, |c, h| {
        let t = unsafe { *t };
        camera::set_view_turn(c, h, &t, immediate)
    })
}

/// `SetOffsetTurn(t, immediate)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_offset_turn(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    t: *const GeTransformation,
    immediate: bool,
) {
    with(rec, ctx, call, |c, h| {
        let t = unsafe { *t };
        camera::set_offset_turn(c, h, &t, immediate)
    })
}

/// `SetLocation(location, immediate, follow terrain)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_location(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    location: *const Ge3DPoint,
    immediate: bool,
    follow_terrain: bool,
) {
    with(rec, ctx, call, |c, h| {
        let location = unsafe { *location };
        camera::set_location(c, h, location, immediate, follow_terrain)
    })
}

/// `SetMode(mode)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_mode(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    mode: c_long,
) {
    with(rec, ctx, call, |c, h| camera::set_mode(c, h, mode))
}

/// `SetZoom(zoom)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_set_zoom(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    zoom: f32,
) {
    with(rec, ctx, call, |c, h| camera::set_zoom(c, h, zoom))
}

/// `Follow(unit, face, duration, yaw, pitch)`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_follow(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    id: u32,
    face: bool,
    duration: f32,
    yaw: f32,
    pitch: f32,
) {
    with(rec, ctx, call, |c, h| camera::follow(c, h, id, face, duration, yaw, pitch))
}

/// `Update(elapsed)`: what the C++ applies after it (`EE_APP_CAMERA_UPDATE_*`), the orthographic rectangle's half
/// size in `ortho_size`.
///
/// # Safety
/// `rec` is a live record, pointer arguments are live, and `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_camera_update(
    rec: *mut EeAppCameraFields,
    ctx: *mut c_void,
    call: EeAppCameraFn,
    elapsed: f32,
    ortho_size: *mut f32,
) -> i32 {
    with(rec, ctx, call, |c, h| {
        let mut size = 0.0;
        let r = camera::update(c, h, elapsed, &mut size);
        unsafe { *ortho_size = size };
        r
    })
}
