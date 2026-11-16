//! `EEEMStorm`'s C ABI (include/rust/ee_app.h): the step over one callback host, the planners' costs, the name.

use crate::storm::{self, EeAppStormFields, StormHost};
use std::ffi::c_void;
use std::os::raw::{c_char, c_long, c_ulong};

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAppStormIo {
    pub flag: bool,
    /// `REQUEST`: the request is done
    pub done: bool,
    pub n: c_long,
    pub f: f32,
    pub g: f32,
}

/// The host: `(ctx, op, io)`.
pub type EeAppStormFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppStormIo);

// ---- the operations (io's fields: in -> out)
/// -> flag (a request), done
pub const EE_APP_STORM_OP_REQUEST: i32 = 1;
/// `TakePath`; `RequestRoute`
pub const EE_APP_STORM_OP_TAKE_PATH: i32 = 2;
pub const EE_APP_STORM_OP_REQUEST_ROUTE: i32 = 3;
/// -> n: the path's length
pub const EE_APP_STORM_OP_PATH_LEN: i32 = 4;
/// flag (the goal, else the path's last point) -> n: the effect's centre's squared distance to it
pub const EE_APP_STORM_OP_DISTANCE: i32 = 5;
/// `mPath.pop_back()`; flag: the effect moved to the goal (else to the path's last point)
pub const EE_APP_STORM_OP_POP_PATH: i32 = 6;
pub const EE_APP_STORM_OP_MOVE: i32 = 7;
/// -> n: the effect's tile count; the effect ended; f: the effect's intensity
pub const EE_APP_STORM_OP_TILE_COUNT: i32 = 8;
pub const EE_APP_STORM_OP_END_EFFECT: i32 = 9;
pub const EE_APP_STORM_OP_SET_INTENSITY: i32 = 10;
/// -> f: the game speed; f, g -> f: `gPhysics->GetRandom(f, g)`; -> f: `gPhysics->mRandom.Generate()`
pub const EE_APP_STORM_OP_GAME_SPEED: i32 = 11;
pub const EE_APP_STORM_OP_RANDOM_RANGE: i32 = 12;
pub const EE_APP_STORM_OP_RANDOM: i32 = 13;
/// the environment manager's colour back to the storm colour
pub const EE_APP_STORM_OP_RESET_COLOR: i32 = 14;

struct Host {
    call: EeAppStormFn,
    ctx: *mut c_void,
}

impl Host {
    fn op(&mut self, op: i32, io: EeAppStormIo) -> EeAppStormIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.call)(self.ctx, op, &mut io) };
        io
    }
    fn go(&mut self, op: i32) -> EeAppStormIo {
        self.op(op, EeAppStormIo::default())
    }
}

impl StormHost for Host {
    fn request(&mut self) -> Option<bool> {
        let io = self.go(EE_APP_STORM_OP_REQUEST);
        io.flag.then_some(io.done)
    }
    fn take_path(&mut self) {
        self.go(EE_APP_STORM_OP_TAKE_PATH);
    }
    fn request_route(&mut self) {
        self.go(EE_APP_STORM_OP_REQUEST_ROUTE);
    }
    fn path_len(&mut self) -> usize {
        self.go(EE_APP_STORM_OP_PATH_LEN).n as usize
    }
    fn distance_squared(&mut self, goal: bool) -> c_long {
        self.op(EE_APP_STORM_OP_DISTANCE, EeAppStormIo { flag: goal, ..Default::default() }).n
    }
    fn pop_path(&mut self) {
        self.go(EE_APP_STORM_OP_POP_PATH);
    }
    fn move_to(&mut self, goal: bool) {
        self.op(EE_APP_STORM_OP_MOVE, EeAppStormIo { flag: goal, ..Default::default() });
    }
    fn tile_count(&mut self) -> c_ulong {
        self.go(EE_APP_STORM_OP_TILE_COUNT).n as c_ulong
    }
    fn end_effect(&mut self) {
        self.go(EE_APP_STORM_OP_END_EFFECT);
    }
    fn set_effect_intensity(&mut self, intensity: f32) {
        self.op(EE_APP_STORM_OP_SET_INTENSITY, EeAppStormIo { f: intensity, ..Default::default() });
    }
    fn game_speed(&mut self) -> f32 {
        self.go(EE_APP_STORM_OP_GAME_SPEED).f
    }
    fn random_range(&mut self, min: f32, max: f32) -> f32 {
        self.op(EE_APP_STORM_OP_RANDOM_RANGE, EeAppStormIo { f: min, g: max, ..Default::default() }).f
    }
    fn random(&mut self) -> f32 {
        self.go(EE_APP_STORM_OP_RANDOM).f
    }
    fn reset_color(&mut self) {
        self.go(EE_APP_STORM_OP_RESET_COLOR);
    }
}

/// `EEEMStorm(kind)`'s plain members.
///
/// # Safety
/// `out` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_app_storm_init(out: *mut EeAppStormFields, kind: c_long) {
    // SAFETY: the function's contract
    unsafe { *out = storm::new_fields(kind) };
}

/// `Step(time)`: the record copied in, the step run, copied back (no host op touches it).
///
/// # Safety
/// `fields` is live; `call` and `ctx` are live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_storm_step(
    fields: *mut EeAppStormFields,
    call: EeAppStormFn,
    ctx: *mut c_void,
    time: c_ulong,
) {
    // SAFETY: the function's contract
    let mut s = unsafe { *fields };
    storm::step(&mut s, &mut Host { call, ctx }, time);
    // SAFETY: as above
    unsafe { *fields = s };
}

/// `GetTypeName(kind)`: a NUL-terminated static name.
#[no_mangle]
pub extern "C" fn ee_app_storm_type_name(kind: c_long) -> *const c_char {
    storm::type_name(kind).as_ptr()
}

/// The route planners' step cost (`snow`, else rain): the step between sectors (`other_sector`, `straight`: the
/// centres share an axis), the tile's kind and (snow) whether it is on a hill.
#[no_mangle]
pub extern "C" fn ee_app_storm_route_cost(
    snow: bool,
    other_sector: bool,
    straight: bool,
    kind: c_long,
    hill: bool,
) -> c_long {
    if snow {
        storm::snow_cost(other_sector, straight, kind, &mut || hill)
    } else {
        storm::rain_cost(other_sector, straight, kind)
    }
}
