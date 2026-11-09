//! A field stream over a C++ `FSFile`: the Graphics Engine's loaders and savers in Rust decide the order, the
//! checks and the error handling, and each field still goes through the file's own typed `Read`/`Write` (so the
//! per-class end-of-file and short-read rules, and the position a failed load leaves, stay exactly the C++'s).
//! `GERustIO.h` fills the table from an `FSFile&`. Results are the engine's `long` codes, 0 for `kErrorOK`.

use std::ffi::{c_long, c_void};

/// `kErrorOK`.
pub(crate) const OK: c_long = 0;
/// `kErrorFailed` (`LowLevelEngineCore.h`).
#[cfg(test)]
pub(crate) const FAILED: c_long = 9;

/// The typed reads and writes of one `FSFile` (`FSFile::Read(T&)`, `FSFile::Write(const T&)`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGeFieldIo {
    pub file: *mut c_void,
    pub read_u8: unsafe extern "C" fn(file: *mut c_void, out: *mut u8) -> c_long,
    pub read_u16: unsafe extern "C" fn(file: *mut c_void, out: *mut u16) -> c_long,
    pub read_u32: unsafe extern "C" fn(file: *mut c_void, out: *mut u32) -> c_long,
    pub read_f32: unsafe extern "C" fn(file: *mut c_void, out: *mut f32) -> c_long,
    pub write_u8: unsafe extern "C" fn(file: *mut c_void, value: u8) -> c_long,
    pub write_u16: unsafe extern "C" fn(file: *mut c_void, value: u16) -> c_long,
    pub write_u32: unsafe extern "C" fn(file: *mut c_void, value: u32) -> c_long,
    pub write_f32: unsafe extern "C" fn(file: *mut c_void, value: f32) -> c_long,
}

/// The Rust side of a field stream: what the codecs are written against, so that they test over a byte buffer.
pub trait FieldIo {
    fn read_u8(&mut self, out: &mut u8) -> c_long;
    fn read_u16(&mut self, out: &mut u16) -> c_long;
    fn read_u32(&mut self, out: &mut u32) -> c_long;
    fn read_f32(&mut self, out: &mut f32) -> c_long;
    fn write_u8(&mut self, value: u8) -> c_long;
    fn write_u16(&mut self, value: u16) -> c_long;
    fn write_u32(&mut self, value: u32) -> c_long;
    fn write_f32(&mut self, value: f32) -> c_long;

    /// `Read(long&)`: a 4-byte read through a 32-bit copy of the old value, stored back even when the read fails
    /// (`FSFile.h`).
    fn read_i32(&mut self, out: &mut i32) -> c_long {
        let mut v = *out as u32;
        let r = self.read_u32(&mut v);
        *out = v as i32;
        r
    }

    fn write_i32(&mut self, value: i32) -> c_long {
        self.write_u32(value as u32)
    }
}

/// A field stream over the C++ table.
pub struct CppIo(pub EeGeFieldIo);

impl FieldIo for CppIo {
    fn read_u8(&mut self, out: &mut u8) -> c_long {
        // SAFETY: the table's functions take its own file and a valid out pointer
        unsafe { (self.0.read_u8)(self.0.file, out) }
    }
    fn read_u16(&mut self, out: &mut u16) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.read_u16)(self.0.file, out) }
    }
    fn read_u32(&mut self, out: &mut u32) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.read_u32)(self.0.file, out) }
    }
    fn read_f32(&mut self, out: &mut f32) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.read_f32)(self.0.file, out) }
    }
    fn write_u8(&mut self, value: u8) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.write_u8)(self.0.file, value) }
    }
    fn write_u16(&mut self, value: u16) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.write_u16)(self.0.file, value) }
    }
    fn write_u32(&mut self, value: u32) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.write_u32)(self.0.file, value) }
    }
    fn write_f32(&mut self, value: f32) -> c_long {
        // SAFETY: as above
        unsafe { (self.0.write_f32)(self.0.file, value) }
    }
}

/// A little-endian byte buffer as a field stream, with `FSMemoryFile`'s rule: a read with fewer bytes left than it
/// wants reads nothing and fails with `END_OF_FILE`; a write fails once the buffer holds `write_limit` bytes.
#[cfg(test)]
pub struct BufferIo {
    pub bytes: Vec<u8>,
    pub position: usize,
    pub write_limit: usize,
}

#[cfg(test)]
/// `kErrorEndOfFile`.
pub(crate) const END_OF_FILE: c_long = 0x12;

#[cfg(test)]
impl BufferIo {
    pub fn reader(bytes: Vec<u8>) -> BufferIo {
        BufferIo { bytes, position: 0, write_limit: usize::MAX }
    }

    pub fn writer() -> BufferIo {
        BufferIo { bytes: Vec::new(), position: 0, write_limit: usize::MAX }
    }

    fn take<const N: usize>(&mut self) -> Option<[u8; N]> {
        let end = self.position + N;
        let slice = self.bytes.get(self.position..end)?;
        self.position = end;
        Some(slice.try_into().unwrap())
    }

    fn put(&mut self, data: &[u8]) -> c_long {
        if self.bytes.len() + data.len() > self.write_limit {
            return FAILED;
        }
        self.bytes.extend_from_slice(data);
        OK
    }
}

#[cfg(test)]
impl FieldIo for BufferIo {
    fn read_u8(&mut self, out: &mut u8) -> c_long {
        self.take::<1>().map_or(END_OF_FILE, |b| {
            *out = b[0];
            OK
        })
    }
    fn read_u16(&mut self, out: &mut u16) -> c_long {
        self.take::<2>().map_or(END_OF_FILE, |b| {
            *out = u16::from_le_bytes(b);
            OK
        })
    }
    fn read_u32(&mut self, out: &mut u32) -> c_long {
        self.take::<4>().map_or(END_OF_FILE, |b| {
            *out = u32::from_le_bytes(b);
            OK
        })
    }
    fn read_f32(&mut self, out: &mut f32) -> c_long {
        self.take::<4>().map_or(END_OF_FILE, |b| {
            *out = f32::from_le_bytes(b);
            OK
        })
    }
    fn write_u8(&mut self, value: u8) -> c_long {
        self.put(&[value])
    }
    fn write_u16(&mut self, value: u16) -> c_long {
        self.put(&value.to_le_bytes())
    }
    fn write_u32(&mut self, value: u32) -> c_long {
        self.put(&value.to_le_bytes())
    }
    fn write_f32(&mut self, value: f32) -> c_long {
        self.put(&value.to_le_bytes())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn buffer_reads_little_endian_and_stops_at_the_end() {
        let mut io = BufferIo::reader(vec![1, 2, 0, 0, 0xff, 0xff, 0xff, 0xff, 7]);
        let mut v = 0u32;
        assert_eq!(io.read_u32(&mut v), OK);
        assert_eq!(v, 0x201);
        let mut s = 0i32;
        assert_eq!(io.read_i32(&mut s), OK);
        assert_eq!(s, -1);
        let mut w = 0u16;
        // one byte left: nothing read, the position stays
        assert_eq!(io.read_u16(&mut w), END_OF_FILE);
        let mut b = 0u8;
        assert_eq!(io.read_u8(&mut b), OK);
        assert_eq!(b, 7);
    }

    #[test]
    fn a_failed_signed_read_leaves_the_value() {
        let mut io = BufferIo::reader(vec![1]);
        let mut s = 5i32;
        assert_eq!(io.read_i32(&mut s), END_OF_FILE);
        assert_eq!(s, 5);
    }

    #[test]
    fn buffer_writes_until_the_limit() {
        let mut io = BufferIo { write_limit: 5, ..BufferIo::writer() };
        assert_eq!(io.write_f32(1.0), OK);
        assert_eq!(io.write_u16(3), FAILED);
        assert_eq!(io.write_u8(9), OK);
        assert_eq!(io.bytes, vec![0, 0, 0x80, 0x3f, 9]);
    }
}
