//! A saved game's primitives over a host callback: the operation codes and record of ee_units player_save.rs
//! (`EESavePrimitive`'s, ee/EEPlayer.h), repeated here because ee_units' FFI imports C++ (absent from a test binary).
//! The C++ asserts that the codes and the record's layout are the same (EEGameOptions.cpp).

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// `EeUnitsPlayerSaveIo`'s layout: a primitive's value in and out.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppSaveIo {
    pub i: i64,
    pub f: f32,
    pub b: u8,
}

/// The host: `(ctx, op, index, io) -> the operation's result` (kErrorOK 0).
pub type EeAppSaveFn = unsafe extern "C" fn(*mut c_void, i32, u32, *mut EeAppSaveIo) -> i32;

// the file's primitives (EE_UNITS_PLAYER_SAVE_READ_ULONG .. WRITE_BOOL)
pub const EE_APP_SAVE_READ_ULONG: i32 = 1;
pub const EE_APP_SAVE_READ_LONG: i32 = 2;
pub const EE_APP_SAVE_READ_FLOAT: i32 = 3;
pub const EE_APP_SAVE_READ_BOOL: i32 = 4;
pub const EE_APP_SAVE_WRITE_ULONG: i32 = 5;
pub const EE_APP_SAVE_WRITE_LONG: i32 = 6;
pub const EE_APP_SAVE_WRITE_FLOAT: i32 = 7;
pub const EE_APP_SAVE_WRITE_BOOL: i32 = 8;

/// The C++ side of a load or save: one call per operation.
pub trait SaveHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeAppSaveIo) -> i32;

    fn op_at(&mut self, op: i32, index: u32) -> i32 {
        self.call(op, index, &mut EeAppSaveIo::default())
    }
    fn read_ulong(&mut self, v: &mut c_ulong) -> i32 {
        let mut io = EeAppSaveIo { i: *v as i64, ..Default::default() };
        let r = self.call(EE_APP_SAVE_READ_ULONG, 0, &mut io);
        *v = io.i as c_ulong;
        r
    }
    fn read_long(&mut self, v: &mut c_long) -> i32 {
        let mut io = EeAppSaveIo { i: *v as i64, ..Default::default() };
        let r = self.call(EE_APP_SAVE_READ_LONG, 0, &mut io);
        *v = io.i as c_long;
        r
    }
    fn read_float(&mut self, v: &mut f32) -> i32 {
        let mut io = EeAppSaveIo { f: *v, ..Default::default() };
        let r = self.call(EE_APP_SAVE_READ_FLOAT, 0, &mut io);
        *v = io.f;
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut io = EeAppSaveIo { b: u8::from(*v), ..Default::default() };
        let r = self.call(EE_APP_SAVE_READ_BOOL, 0, &mut io);
        *v = io.b != 0;
        r
    }
    fn write_ulong(&mut self, v: c_ulong) -> i32 {
        self.call(EE_APP_SAVE_WRITE_ULONG, 0, &mut EeAppSaveIo { i: v as i64, ..Default::default() })
    }
    fn write_long(&mut self, v: c_long) -> i32 {
        self.call(EE_APP_SAVE_WRITE_LONG, 0, &mut EeAppSaveIo { i: v as i64, ..Default::default() })
    }
    fn write_float(&mut self, v: f32) -> i32 {
        self.call(EE_APP_SAVE_WRITE_FLOAT, 0, &mut EeAppSaveIo { f: v, ..Default::default() })
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.call(EE_APP_SAVE_WRITE_BOOL, 0, &mut EeAppSaveIo { b: u8::from(v), ..Default::default() })
    }
}

/// The callback host over the C++ object (the context).
pub struct Host {
    pub ctx: *mut c_void,
    pub call: EeAppSaveFn,
}

impl SaveHost for Host {
    fn call(&mut self, op: i32, index: u32, io: &mut EeAppSaveIo) -> i32 {
        // SAFETY: the C++ passed a live context and its callback for the call's duration
        unsafe { (self.call)(self.ctx, op, index, io) }
    }
}
