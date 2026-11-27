//! The Rust saves' file primitives (`EESavePrimitive`, ee/EEPlayer.h; the world lane, round 2: was EEPlayer.cpp's
//! switch): each operation is the `FSFile` overload the original called, over one-statement C++ shims
//! (`ee_units_file_*`, EEPlayer.cpp). A read's `io` holds the old value in and the read value out (the 4-byte
//! `long`/`unsigned long` adapters' semantics: the value goes through a 32-bit copy that starts as the old value and
//! is stored back even when the read fails); a bool travels as 0 or 1.

use std::os::raw::{c_long, c_ulong};

use crate::units::player_save::{
    EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LEVELS, EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG,
    EE_UNITS_PLAYER_SAVE_READ_USHORT, EE_UNITS_PLAYER_SAVE_WRITE_BOOL, EE_UNITS_PLAYER_SAVE_WRITE_FLOAT,
    EE_UNITS_PLAYER_SAVE_WRITE_LEVELS, EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
    EE_UNITS_PLAYER_SAVE_WRITE_USHORT,
};

/// An `FSFile`'s primitive reads and writes (`Read(uint32_t&)` .. `Write(const char*, n, written)`).
pub trait FilePrims {
    fn read_u32(&mut self, v: &mut u32) -> c_long;
    fn read_i32(&mut self, v: &mut i32) -> c_long;
    fn read_f32(&mut self, v: &mut f32) -> c_long;
    /// `Read(bool&)`: the byte (any non-zero byte is true)
    fn read_bool(&mut self, v: &mut u8) -> c_long;
    fn read_u16(&mut self, v: &mut u16) -> c_long;
    fn read_bytes(&mut self, data: *mut u8, len: c_ulong, read: &mut c_ulong) -> c_long;
    fn write_u32(&mut self, v: u32) -> c_long;
    fn write_i32(&mut self, v: i32) -> c_long;
    fn write_f32(&mut self, v: f32) -> c_long;
    fn write_bool(&mut self, v: bool) -> c_long;
    fn write_u16(&mut self, v: u16) -> c_long;
    fn write_bytes(&mut self, data: *const u8, len: c_ulong, written: &mut c_ulong) -> c_long;
}

/// A record's 16 levels as raw bytes (improvement.rs: `Read(char*, 0x40, read)`).
const LEVELS_BYTES: c_ulong = 0x40;

/// `EESavePrimitive(file, op, io)`: the primitive's result, -1 for another operation.
pub fn primitive<F: FilePrims + ?Sized>(f: &mut F, op: i32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
    let r: c_long = match op {
        EE_UNITS_PLAYER_SAVE_READ_ULONG => {
            let mut v = io.i as u32;
            let r = f.read_u32(&mut v);
            io.i = v as i64;
            r
        }
        EE_UNITS_PLAYER_SAVE_READ_LONG => {
            let mut v = io.i as i32;
            let r = f.read_i32(&mut v);
            io.i = v as i64;
            r
        }
        EE_UNITS_PLAYER_SAVE_READ_FLOAT => {
            let mut v = io.f;
            let r = f.read_f32(&mut v);
            io.f = v;
            r
        }
        EE_UNITS_PLAYER_SAVE_READ_BOOL => {
            let mut v = u8::from(io.b != 0);
            let r = f.read_bool(&mut v);
            io.b = u8::from(v != 0);
            r
        }
        EE_UNITS_PLAYER_SAVE_WRITE_ULONG => f.write_u32(io.i as u32),
        EE_UNITS_PLAYER_SAVE_WRITE_LONG => f.write_i32(io.i as i32),
        EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => f.write_f32(io.f),
        EE_UNITS_PLAYER_SAVE_WRITE_BOOL => f.write_bool(io.b != 0),
        EE_UNITS_PLAYER_SAVE_READ_LEVELS => {
            let mut read: c_ulong = 0;
            f.read_bytes(io.i as usize as *mut u8, LEVELS_BYTES, &mut read)
        }
        EE_UNITS_PLAYER_SAVE_WRITE_LEVELS => {
            let mut written: c_ulong = 0;
            f.write_bytes(io.i as usize as *const u8, LEVELS_BYTES, &mut written)
        }
        EE_UNITS_PLAYER_SAVE_READ_USHORT => {
            let mut v = io.i as u16;
            let r = f.read_u16(&mut v);
            io.i = v as i64;
            r
        }
        EE_UNITS_PLAYER_SAVE_WRITE_USHORT => f.write_u16(io.i as u16),
        _ => return -1,
    };
    r as i32
}

/// The primitives `EESaveIsPrimitive` names (the reads and writes of a number, a float, a bool, an unsigned short).
pub fn is_primitive(op: i32) -> bool {
    (EE_UNITS_PLAYER_SAVE_READ_ULONG..=EE_UNITS_PLAYER_SAVE_WRITE_BOOL).contains(&op)
        || op == EE_UNITS_PLAYER_SAVE_READ_USHORT
        || op == EE_UNITS_PLAYER_SAVE_WRITE_USHORT
}

#[cfg(not(test))]
pub mod game {
    use super::*;
    use std::ffi::c_void;

    use ee_base::fs::CppFile;
    use ee_base::fs::{FsFile, FsWrite}; // read_u32, write_u32

    /// An `FSFile*` the C++ hands in, read and written through ee_base's C++ file calls (cppfile.rs).
    pub struct GameFile(pub *mut c_void);

    impl GameFile {
        fn file(&self) -> CppFile {
            CppFile(self.0)
        }
    }

    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    impl FilePrims for GameFile {
        fn read_u32(&mut self, v: &mut u32) -> c_long {
            self.file().read_u32(v) as c_long
        }
        fn read_i32(&mut self, v: &mut i32) -> c_long {
            self.file().read_i32(v) as c_long
        }
        fn read_f32(&mut self, v: &mut f32) -> c_long {
            self.file().read_f32(v) as c_long
        }
        fn read_bool(&mut self, v: &mut u8) -> c_long {
            let mut b = *v != 0;
            let r = self.file().read_bool(&mut b);
            *v = u8::from(b);
            r as c_long
        }
        fn read_u16(&mut self, v: &mut u16) -> c_long {
            self.file().read_u16(v) as c_long
        }
        fn read_bytes(&mut self, data: *mut u8, len: c_ulong, read: &mut c_ulong) -> c_long {
            // (the `char*` overload, as the C++ called); SAFETY: the caller's buffer of `len` bytes
            let out =
                if len == 0 { &mut [][..] } else { unsafe { std::slice::from_raw_parts_mut(data, len as usize) } };
            let (r, n) = self.file().read_text(out);
            *read = n;
            r as c_long
        }
        fn write_u32(&mut self, v: u32) -> c_long {
            self.file().write_u32(v) as c_long
        }
        fn write_i32(&mut self, v: i32) -> c_long {
            self.file().write_i32(v) as c_long
        }
        fn write_f32(&mut self, v: f32) -> c_long {
            self.file().write_f32(v) as c_long
        }
        fn write_bool(&mut self, v: bool) -> c_long {
            self.file().write_bool(v) as c_long
        }
        fn write_u16(&mut self, v: u16) -> c_long {
            self.file().write_u16(v) as c_long
        }
        fn write_bytes(&mut self, data: *const u8, len: c_ulong, written: &mut c_ulong) -> c_long {
            // SAFETY: the caller's buffer of `len` bytes
            let bytes = if len == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(data, len as usize) } };
            let (r, n) = self.file().write_text(bytes);
            *written = n;
            r as c_long
        }
    }

    /// `EESavePrimitive(*file, op, io)` (ee/EEPlayer.h): -1 for another operation.
    ///
    /// # Safety
    /// `file` a live `FSFile`; `io` a live record; for the levels' operations `io.i` the address of 0x40 bytes.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_save_primitive(file: *mut c_void, op: i32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the caller's contract
        primitive(&mut GameFile(file), op, unsafe { &mut *io })
    }
}

#[cfg(test)]
#[path = "save_prim_tests.rs"]
mod tests;
