//! The `FSFile` seam: the logic reads and writes the C++ object through this trait; the C++ glue is a table of function
//! pointers (`EeFsFileOps`), a test double elsewhere.

use std::ffi::c_void;

/// What `FSMemoryFile`/`FSChunk`/`FSArchive` do with a file. The values are little-endian dwords and raw bytes; a
/// `read_*` that fails leaves the output as `FSFile` left it (a short read may have overwritten part of the value).
pub trait FsFile {
    /// Reads 4 bytes. `FSFile::Read(uint32_t&)` semantics.
    fn read_u32(&mut self, value: &mut u32) -> i32;
    /// Reads up to `out.len()` bytes; returns the status and how many were read.
    fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32);
}

/// The writing half of `FsFile`.
pub trait FsWrite {
    fn write_u32(&mut self, value: u32) -> i32;
    /// Returns the status and how many were written.
    fn write_bytes(&mut self, data: &[u8]) -> (i32, u32);
}

/// The C++ glue's view of an `FSFile&` (built per call by `FSFileOps`). The callbacks take `FSFile` by `context` and
/// return its `long`; `read_bytes`/`write_bytes` report the count through their last argument.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeFsFileOps {
    pub context: *mut c_void,
    pub read_u32: unsafe extern "C" fn(context: *mut c_void, value: *mut u32) -> i32,
    pub read_bytes: unsafe extern "C" fn(context: *mut c_void, out: *mut u8, length: usize, read: *mut u32) -> i32,
    pub write_u32: unsafe extern "C" fn(context: *mut c_void, value: u32) -> i32,
    pub write_bytes:
        unsafe extern "C" fn(context: *mut c_void, data: *const u8, length: usize, written: *mut u32) -> i32,
}

impl FsFile for EeFsFileOps {
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        // SAFETY: the callback's contract: `context` is live for the FFI call that handed out `self`
        unsafe { (self.read_u32)(self.context, value) }
    }
    fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
        let mut read = 0;
        // SAFETY: as above; `out` is valid for its length
        let status = unsafe { (self.read_bytes)(self.context, out.as_mut_ptr(), out.len(), &mut read) };
        (status, read)
    }
}

impl FsWrite for EeFsFileOps {
    fn write_u32(&mut self, value: u32) -> i32 {
        // SAFETY: as `read_u32`
        unsafe { (self.write_u32)(self.context, value) }
    }
    fn write_bytes(&mut self, data: &[u8]) -> (i32, u32) {
        let mut written = 0;
        // SAFETY: as above; `data` is valid for its length
        let status = unsafe { (self.write_bytes)(self.context, data.as_ptr(), data.len(), &mut written) };
        (status, written)
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::status;

    /// A file over a byte vector with scripted failures, for the error paths.
    #[derive(Default)]
    pub(crate) struct ScriptFile {
        pub(crate) data: Vec<u8>,
        pub(crate) pos: usize,
        pub(crate) fail_at_op: Option<usize>,
        pub(crate) ops: usize,
        pub(crate) short_reads: bool,
    }
    impl ScriptFile {
        pub(crate) fn with(data: Vec<u8>) -> Self {
            ScriptFile { data, ..Default::default() }
        }
        pub(crate) fn failing_at(op: usize) -> Self {
            ScriptFile { fail_at_op: Some(op), ..Default::default() }
        }
        fn fail(&mut self) -> bool {
            let n = self.ops;
            self.ops += 1;
            Some(n) == self.fail_at_op
        }
    }
    impl FsFile for ScriptFile {
        fn read_u32(&mut self, value: &mut u32) -> i32 {
            if self.fail() {
                return status::FAILED;
            }
            if self.pos + 4 > self.data.len() {
                return status::END_OF_FILE;
            }
            *value = u32::from_le_bytes(self.data[self.pos..self.pos + 4].try_into().unwrap());
            self.pos += 4;
            status::OK
        }
        fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
            if self.fail() {
                return (status::FAILED, 0);
            }
            let n = (self.data.len() - self.pos).min(out.len());
            if n < out.len() && !self.short_reads {
                return (status::END_OF_FILE, 0);
            }
            out[..n].copy_from_slice(&self.data[self.pos..self.pos + n]);
            self.pos += n;
            (status::OK, n as u32)
        }
    }
    impl FsWrite for ScriptFile {
        fn write_u32(&mut self, value: u32) -> i32 {
            if self.fail() {
                return status::FAILED;
            }
            self.data.extend_from_slice(&value.to_le_bytes());
            status::OK
        }
        fn write_bytes(&mut self, data: &[u8]) -> (i32, u32) {
            if self.fail() {
                return (status::FAILED, 0);
            }
            self.data.extend_from_slice(data);
            (status::OK, data.len() as u32)
        }
    }

    #[test]
    fn script_file_reads_writes_and_fails() {
        let mut f = ScriptFile::default();
        assert_eq!(f.write_u32(5), status::OK);
        assert_eq!(f.write_bytes(b"ab"), (status::OK, 2));
        f.pos = 0;
        let mut v = 0;
        assert_eq!(f.read_u32(&mut v), status::OK);
        assert_eq!(v, 5);
        let mut buf = [0u8; 2];
        assert_eq!(f.read_bytes(&mut buf), (status::OK, 2));
        assert_eq!(&buf, b"ab");
        assert_eq!(f.read_u32(&mut v), status::END_OF_FILE);
        let mut f = ScriptFile::failing_at(0);
        assert_eq!(f.write_u32(5), status::FAILED);
    }
}
