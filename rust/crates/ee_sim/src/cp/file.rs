//! The saved game's file as the computer player's loads and saves use it: the primitives of `EESavePrimitive`
//! (ee/EEPlayer.h; the `FSFile` overload the original called) through the player save's host signature
//! ([`EeUnitsPlayerSaveFn`]), so `EESaveFilePrimitive` (the file itself as the context) serves it.
//!
//! A read hands the old value in and takes the read value out (`FSFile::Read`'s semantics: a failed read leaves what
//! the file left); the result is the file's (`kErrorOK` 0).

use std::ffi::c_void;

use crate::units::player_save::{
    EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_READ_USHORT,
    EE_UNITS_PLAYER_SAVE_WRITE_BOOL, EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
    EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};

/// `kErrorOK`.
pub const OK: i32 = 0;
/// `kErrorFailed`.
pub const FAILED: i32 = 9;

/// The file: the host's context and callback (`index` 0 for the primitives).
pub struct SaveFile {
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
}

impl SaveFile {
    /// # Safety
    /// `host` must accept `ctx` with the primitives' operations for as long as the value lives.
    pub unsafe fn new(ctx: *mut c_void, host: EeUnitsPlayerSaveFn) -> SaveFile {
        SaveFile { ctx, host }
    }

    pub fn call(&mut self, op: i32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        self.call_at(op, 0, io)
    }

    /// The host's operation `op` with an index (the caller's parts beyond the primitives).
    pub fn call_at(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the contract of `new`
        unsafe { (self.host)(self.ctx, op, index, io) }
    }

    /// `Read(unsigned long&)` into `old` (4 bytes).
    pub fn read_ulong(&mut self, old: u32) -> (i32, u32) {
        let mut io = EeUnitsPlayerSaveIo { i: i64::from(old), ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut io);
        (r, io.i as u32)
    }
    /// `Read(long&)` into `old` (4 bytes).
    pub fn read_long(&mut self, old: i32) -> (i32, i32) {
        let mut io = EeUnitsPlayerSaveIo { i: i64::from(old), ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_LONG, &mut io);
        (r, io.i as i32)
    }
    pub fn read_float(&mut self, old: f32) -> (i32, f32) {
        let mut io = EeUnitsPlayerSaveIo { f: old, ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_FLOAT, &mut io);
        (r, io.f)
    }
    pub fn read_bool(&mut self, old: bool) -> (i32, bool) {
        let mut io = EeUnitsPlayerSaveIo { b: u8::from(old), ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_BOOL, &mut io);
        (r, io.b != 0)
    }
    pub fn read_ushort(&mut self, old: u16) -> (i32, u16) {
        let mut io = EeUnitsPlayerSaveIo { i: i64::from(old), ..Default::default() };
        let r = self.call(EE_UNITS_PLAYER_SAVE_READ_USHORT, &mut io);
        (r, io.i as u16)
    }

    pub fn write_ulong(&mut self, v: u32) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { i: i64::from(v), ..Default::default() };
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_ULONG, &mut io)
    }
    pub fn write_long(&mut self, v: i32) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { i: i64::from(v), ..Default::default() };
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_LONG, &mut io)
    }
    pub fn write_float(&mut self, v: f32) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { f: v, ..Default::default() };
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, &mut io)
    }
    pub fn write_bool(&mut self, v: bool) -> i32 {
        let mut io = EeUnitsPlayerSaveIo { b: u8::from(v), ..Default::default() };
        self.call(EE_UNITS_PLAYER_SAVE_WRITE_BOOL, &mut io)
    }
}

/// A file of bytes for the tests: the primitives' widths (4-byte words, 1-byte bools, 2-byte shorts), little-endian,
/// `kErrorBeyondEnd` (0xa) past the end.
#[cfg(test)]
pub mod test_file {
    use super::*;

    #[derive(Default)]
    pub struct Bytes {
        pub data: Vec<u8>,
        pub at: usize,
    }

    impl Bytes {
        pub fn take(&mut self, n: usize) -> Option<&[u8]> {
            if self.at + n > self.data.len() {
                self.at = self.data.len();
                return None;
            }
            let s = &self.data[self.at..self.at + n];
            self.at += n;
            Some(s)
        }
        pub fn u32(&mut self, v: u32) -> &mut Self {
            self.data.extend_from_slice(&v.to_le_bytes());
            self
        }
        pub fn f32(&mut self, v: f32) -> &mut Self {
            self.data.extend_from_slice(&v.to_le_bytes());
            self
        }
        pub fn b(&mut self, v: bool) -> &mut Self {
            self.data.push(u8::from(v));
            self
        }
        pub fn u16(&mut self, v: u16) -> &mut Self {
            self.data.extend_from_slice(&v.to_le_bytes());
            self
        }
    }

    /// The host over a [`Bytes`] (the context).
    ///
    /// # Safety
    /// `ctx` is a live [`Bytes`], `io` a live value.
    pub unsafe extern "C" fn host(ctx: *mut c_void, op: i32, _index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the tests pass a live Bytes and io
        let (f, io) = unsafe { (&mut *(ctx as *mut Bytes), &mut *io) };
        match op {
            EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_LONG => match f.take(4) {
                Some(s) => {
                    let w = u32::from_le_bytes([s[0], s[1], s[2], s[3]]);
                    io.i = if op == EE_UNITS_PLAYER_SAVE_READ_LONG { i64::from(w as i32) } else { i64::from(w) };
                    OK
                }
                None => 0xa,
            },
            EE_UNITS_PLAYER_SAVE_READ_FLOAT => match f.take(4) {
                Some(s) => {
                    io.f = f32::from_le_bytes([s[0], s[1], s[2], s[3]]);
                    OK
                }
                None => 0xa,
            },
            EE_UNITS_PLAYER_SAVE_READ_BOOL => match f.take(1) {
                Some(s) => {
                    io.b = u8::from(s[0] != 0);
                    OK
                }
                None => 0xa,
            },
            EE_UNITS_PLAYER_SAVE_READ_USHORT => match f.take(2) {
                Some(s) => {
                    io.i = i64::from(u16::from_le_bytes([s[0], s[1]]));
                    OK
                }
                None => 0xa,
            },
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                f.u32(io.i as u32);
                OK
            }
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => {
                f.f32(io.f);
                OK
            }
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                f.b(io.b != 0);
                OK
            }
            _ => -1,
        }
    }

    pub fn file(b: &mut Bytes) -> SaveFile {
        // SAFETY: b outlives the returned file in every test
        unsafe { SaveFile::new(b as *mut Bytes as *mut c_void, host) }
    }
}
