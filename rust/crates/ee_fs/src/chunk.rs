//! `FSChunk`: one persistent in a file: a header of four dwords (major, minor, tag, size) and an `FSMemoryFile`
//! payload. The saved game's format contract; `FSChunk.cpp` keeps the persistent's virtual `Load`/`Save` around it.

use crate::file::{FsFile, FsWrite};
use crate::memfile::EeFsMemFile;
use crate::status;

const SAVE_COMPRESS_LEVEL: u32 = 0x1000;
const MAX_CHUNK_SIZE: u32 = 0x8000_0000;

/// `FSChunk`'s state, embedded in the C++ object (`mState`) so that a C++ copy copies it as before.
/// `data` is the payload; the C++ destructor frees it through `ee_fs_chunk_free`.
#[repr(C)]
pub struct EeFsChunk {
    pub data: *mut EeFsMemFile,
    pub tag: i32,
    pub chunk_size: u32,
    pub major: u32,
    pub minor: u32,
    pub data_loaded: bool,
    pub header_loaded: bool,
    pub decompressed: bool,
}

impl EeFsChunk {
    /// `FSChunk()`: empty, no header or data loaded.
    pub fn new() -> Self {
        EeFsChunk {
            data: std::ptr::null_mut(),
            tag: 0,
            chunk_size: 0,
            major: 0,
            minor: 0,
            data_loaded: false,
            header_loaded: false,
            decompressed: false,
        }
    }

    fn data(&mut self) -> Option<&mut EeFsMemFile> {
        // SAFETY: `data` is null or a live `EeFsMemFile` owned by this chunk (the C++ facade's contract)
        unsafe { self.data.as_mut() }
    }

    /// `GetChunkSize`.
    pub fn size(&self) -> Option<u32> {
        self.header_loaded.then_some(self.chunk_size)
    }

    /// `LoadChunkHeader`: major, minor, tag, size; a size of 2 GB or more fails.
    pub fn load_header(&mut self, file: &mut dyn FsFile) -> i32 {
        let mut result = file.read_u32(&mut self.major);
        if result != status::OK {
            return result;
        }
        result = file.read_u32(&mut self.minor);
        if result != status::OK {
            return result;
        }
        let mut tag = self.tag as u32;
        result = file.read_u32(&mut tag);
        self.tag = tag as i32;
        if result != status::OK {
            return result;
        }
        result = file.read_u32(&mut self.chunk_size);
        self.header_loaded = true;
        if result == status::OK && self.chunk_size >= MAX_CHUNK_SIZE {
            result = status::FAILED;
        }
        result
    }

    /// `LoadChunkFromFile`: reads the header if needed, then the payload, decompressed.
    pub fn load_from_file(&mut self, file: &mut dyn FsFile) -> i32 {
        let mut result = status::OK;
        if !self.header_loaded {
            result = self.load_header(file);
        }
        if result != status::OK || !self.data.is_null() {
            return result;
        }
        self.data = Box::into_raw(Box::new(EeFsMemFile::new(self.chunk_size as usize, false, 1)));
        let chunk_size = self.chunk_size as usize;
        let data = self.data().expect("just allocated");
        result = data.load(file, chunk_size);
        if result != status::OK {
            return result;
        }
        let (result, length) = data.decompress();
        if result != status::OK {
            return result;
        }
        self.decompressed = true;
        self.data_loaded = true;
        self.chunk_size = length;
        result
    }

    /// `SaveChunkToFile`: the header and the payload's first `chunk_size` bytes. A chunk without data writes the
    /// header and fails with `NOT_OPEN` (the C++ crashed on the null `mData`).
    pub fn save_to_file(&mut self, file: &mut dyn FsWrite) -> i32 {
        let mut result = file.write_u32(self.major);
        if result != status::OK {
            return result;
        }
        result = file.write_u32(self.minor);
        if result != status::OK {
            return result;
        }
        result = file.write_u32(self.tag as u32);
        if result == status::OK {
            result = file.write_u32(self.chunk_size);
        }
        if result != status::OK {
            return result;
        }
        let chunk_size = self.chunk_size as usize;
        match self.data() {
            Some(data) => data.save(file, chunk_size),
            None => status::NOT_OPEN,
        }
    }

    /// `SavePersistentToChunk`, part one: a fresh payload for the persistent's `Save`; `None` when one exists
    /// already (the C++ returned `kErrorOK` without saving).
    pub fn begin_save(&mut self) -> Option<*mut EeFsMemFile> {
        if !self.data.is_null() {
            return None;
        }
        self.data = Box::into_raw(Box::new(EeFsMemFile::new(1000, true, 1)));
        Some(self.data)
    }

    /// `SavePersistentToChunk`, part two, after a successful `Save`: compresses the payload (its result is
    /// not checked, as in C++) and fills in the header.
    pub fn finish_save(&mut self, major: u32, minor: u32, tag: i32) {
        let data = self.data().expect("begin_save");
        let (_, length) = data.compress(SAVE_COMPRESS_LEVEL);
        self.major = major;
        self.minor = minor;
        self.tag = tag;
        self.chunk_size = length;
    }

    /// `SkipChunk`: reads the header if needed and reads past the payload; `data_loaded` even if the read fails.
    pub fn skip(&mut self, file: &mut dyn FsFile) -> i32 {
        let mut result = status::OK;
        if !self.header_loaded {
            result = self.load_header(file);
        }
        if result == status::OK && !self.data_loaded {
            let mut buffer = vec![0u8; self.chunk_size as usize];
            result = file.read_bytes(&mut buffer).0;
            self.data_loaded = true;
        }
        result
    }

    /// Frees the payload (`~FSChunk`).
    ///
    /// # Safety
    /// `data` is null or was created by this crate and not freed.
    pub unsafe fn free(&mut self) {
        if !self.data.is_null() {
            // SAFETY: the method's contract
            drop(unsafe { Box::from_raw(self.data) });
            self.data = std::ptr::null_mut();
        }
    }
}

impl Default for EeFsChunk {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::file::tests::ScriptFile;

    fn hex(s: &str) -> Vec<u8> {
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }

    fn golden(kind: &str, name: &str) -> Vec<Vec<String>> {
        include_str!("golden_fs.txt")
            .lines()
            .filter_map(|l| {
                let f: Vec<String> = l.split('\t').map(str::to_string).collect();
                (f[0] == kind && (name.is_empty() || f[1] == name)).then_some(f)
            })
            .collect()
    }

    fn free(c: &mut EeFsChunk) {
        unsafe { c.free() }
    }

    // what the C++ test persistent wrote: a dword length and the bytes, through FSFile
    fn payload(name: &str) -> Vec<u8> {
        let bytes = match name {
            "small" => hex("0102030405"),
            "long" => (0..1500u32).map(|i| ((i * 3 + (i >> 3)) & 0xff) as u8).collect(),
            _ => (0..30u32).map(|i| ((i * 97 + (i >> 3)) & 0xff) as u8).collect(),
        };
        let mut body = (bytes.len() as u32).to_le_bytes().to_vec();
        body.extend(bytes);
        body
    }

    #[test]
    fn save_and_file_bytes_equal_the_cpp() {
        for (name, major, minor) in [("small", 0, 250), ("long", 1, 252), ("incompress", 2, 1)] {
            let mut chunk = EeFsChunk::new();
            let ptr = chunk.begin_save().unwrap();
            unsafe { (*ptr).write(&payload(name)) };
            chunk.finish_save(major, minor, 7);
            assert_eq!(chunk.size(), None); // the C++ GetChunkSize before a load
            let mut out = EeFsMemFile::new(64, true, 1);
            assert_eq!(chunk.save_to_file(&mut out), 0);
            let expected = &golden("chunk-file", name)[0][2];
            assert_eq!(out.contents(), &hex(expected)[..], "{name}");
            let g = &golden("chunk-load", name)[0];
            let mut loaded = EeFsChunk::new();
            out.set_position(0, 0);
            assert_eq!(loaded.load_from_file(&mut out).to_string(), g[2]);
            assert_eq!(format!("{}:{}", loaded.size().is_some() as i32, loaded.size().unwrap_or(0)), g[4]);
            let persisted = unsafe { (*loaded.data).contents().to_vec() };
            // strip the persistent's own length prefix to compare with the driver's `mBody`
            assert_eq!(hex(&g[5]), persisted[4..].to_vec());
            assert_eq!(out.position().to_string(), golden("chunk-pos", name)[0][2]);
            // skip the same bytes
            out.set_position(0, 0);
            let mut skip = EeFsChunk::new();
            assert_eq!(skip.skip(&mut out).to_string(), golden("chunk-skip", name)[0][2]);
            assert_eq!(out.position().to_string(), golden("chunk-skip", name)[0][3]);
            free(&mut chunk);
            free(&mut loaded);
        }
    }

    #[test]
    fn header_failures_equal_the_cpp() {
        // a truncated header
        let mut f = ScriptFile::with(vec![1, 0, 0, 0, 2, 0, 0, 0]);
        let mut c = EeFsChunk::new();
        assert_eq!(c.load_header(&mut f).to_string(), golden("chunk-short", "")[0][1]);
        assert_eq!(format!("{}", c.size().is_some() as i32), golden("chunk-short-size", "")[0][1]);
        // the 2 GB guard: the result fails but the header counts as loaded
        let mut f = ScriptFile::with(vec![1, 0, 0, 0, 2, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0x80]);
        let mut c = EeFsChunk::new();
        let g = &golden("chunk-oversize", "")[0];
        assert_eq!(c.load_header(&mut f).to_string(), g[1]);
        assert_eq!(format!("{}", c.size().is_some() as i32), g[2]);
        assert_eq!(c.size().unwrap_or(0).to_string(), g[3]);
    }

    #[test]
    fn skip_of_a_raw_file_equals_the_cpp() {
        let g = &golden("chunk-skip-raw", "")[0];
        let mut f = ScriptFile::with([1, 0, 0, 0, 2, 0, 0, 0, 9, 0, 0, 0, 4, 0, 0, 0, 1, 2, 3, 4].to_vec());
        let mut c = EeFsChunk::new();
        assert_eq!(c.skip(&mut f).to_string(), g[1]);
        assert_eq!(f.pos.to_string(), g[2]);
    }

    #[test]
    fn a_chunk_without_data_cannot_load_a_persistent() {
        // kErrorNotSuspended from the facade (the C++ path's golden); Rust has no pointer: nothing to load here
        assert_eq!(golden("chunk-no-data", "")[0][1], status::NOT_SUSPENDED.to_string());
        let c = EeFsChunk::new();
        assert!(c.data.is_null());
    }

    #[test]
    fn begin_save_only_makes_one_payload_and_is_idempotent() {
        let mut c = EeFsChunk::new();
        assert!(c.begin_save().is_some());
        assert!(c.begin_save().is_none());
        free(&mut c);
    }

    #[test]
    fn save_without_data_reports_not_open() {
        let mut c = EeFsChunk::new();
        let mut f = ScriptFile::default();
        assert_eq!(c.save_to_file(&mut f), status::NOT_OPEN);
        assert_eq!(f.data.len(), 16);
    }

    #[test]
    fn write_errors_pass_through_in_order() {
        let mut c = EeFsChunk::new();
        c.begin_save();
        c.finish_save(1, 2, 3);
        for failing in 0..4 {
            let mut f = ScriptFile::failing_at(failing);
            assert_eq!(c.save_to_file(&mut f), status::FAILED);
        }
        let mut f = ScriptFile::failing_at(4);
        assert_eq!(c.save_to_file(&mut f), status::FAILED); // the payload write
        free(&mut c);
    }

    #[test]
    fn loading_again_keeps_existing_data() {
        let mut c = EeFsChunk::new();
        c.begin_save();
        c.finish_save(1, 2, 3);
        let mut file = ScriptFile::with(vec![1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 8]);
        c.header_loaded = false;
        assert_eq!(c.load_from_file(&mut file), status::OK); // header read, data untouched
        assert_eq!(file.pos, 16);
        free(&mut c);
    }
}
