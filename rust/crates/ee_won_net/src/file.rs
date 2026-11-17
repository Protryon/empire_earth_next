//! WON's `FileReader` and `FileWriter` (WONCommon/FileReader.cpp, FileWriter.cpp): binary reads from a file or a
//! memory buffer and binary writes to a file, over the C runtime's stdio as the original (a `FILE*` from
//! `PlatFOpen`, so a text-mode open turns CR LF into LF on every system, and `ftell` / `fseek` are the CRT's).
//!
//! The state is a `#[repr(C)]` struct the C++ classes embed by value (`FileReader::mCore`, `FileWriter::mCore`);
//! every function returns a status the C++ turns into its exception (`FileReaderException` /
//! `FileWriterException` with [`status_text`]'s message). The string reads and writes stay the C++ glue's loops over
//! these primitives (a failed read leaves the partly read string, as before).

use std::ffi::{c_char, c_int, c_long, c_ulong, c_void, CStr};

/// No error.
pub const WON_FILE_OK: i32 = 0;
/// "NULL File.": a read with nothing open, or a write with no file.
pub const WON_FILE_NULL: i32 = 1;
/// "Attempt to read past end of mem buffer."
pub const WON_FILE_PAST_END_MEM: i32 = 2;
/// "Attempt to read past end of file."
pub const WON_FILE_PAST_END_FILE: i32 = 3;
/// "Attempt to setpos past end of mem buffer."
pub const WON_FILE_SETPOS_MEM: i32 = 4;
/// "Attempt to setpos past end of file."
pub const WON_FILE_SETPOS_FILE: i32 = 5;
/// "fwrite failed (disk full)"
pub const WON_FILE_WRITE_FAILED: i32 = 6;

/// The exception text of a status (the original's literals), as a C string.
pub fn status_text(status: i32) -> &'static CStr {
    match status {
        WON_FILE_NULL => c"NULL File.",
        WON_FILE_PAST_END_MEM => c"Attempt to read past end of mem buffer.",
        WON_FILE_PAST_END_FILE => c"Attempt to read past end of file.",
        WON_FILE_SETPOS_MEM => c"Attempt to setpos past end of mem buffer.",
        WON_FILE_SETPOS_FILE => c"Attempt to setpos past end of file.",
        WON_FILE_WRITE_FAILED => c"fwrite failed (disk full)",
        _ => c"",
    }
}

const SEEK_SET: c_int = 0;
const SEEK_CUR: c_int = 1;
const SEEK_END: c_int = 2;

/// cbindgen:ignore
mod stdio {
    use std::ffi::{c_char, c_int, c_long, c_void};
    extern "C" {
        // the platform layer's fopen (ee_plat_sys): Windows-form paths and the CRT's text mode on every system
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn fclose(file: *mut c_void) -> c_int;
        pub fn fread(data: *mut c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn fwrite(data: *const c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn fseek(file: *mut c_void, offset: c_long, origin: c_int) -> c_int;
        pub fn ftell(file: *mut c_void) -> c_long;
        pub fn fgetc(file: *mut c_void) -> c_int;
        pub fn feof(file: *mut c_void) -> c_int;
        pub fn rewind(file: *mut c_void);
        pub fn malloc(size: usize) -> *mut c_void;
        pub fn free(data: *mut c_void);
    }
}
use stdio::*;

/// `FileReader`'s state: the file, or the memory buffer (owned when read into memory), its length and position.
/// `memory_mode` is the original's flag: set by a memory open and by `length()` once it measured the file.
#[repr(C)]
pub struct EeWonFileReader {
    pub file: *mut c_void,
    pub buffer: *const u8,
    pub length: c_ulong,
    pub position: c_ulong,
    pub own_buffer: bool,
    pub memory_mode: bool,
}

/// `FileWriter`'s state.
#[repr(C)]
pub struct EeWonFileWriter {
    pub file: *mut c_void,
}

impl EeWonFileReader {
    pub const fn new() -> Self {
        EeWonFileReader {
            file: std::ptr::null_mut(),
            buffer: std::ptr::null(),
            length: 0,
            position: 0,
            own_buffer: false,
            memory_mode: false,
        }
    }

    pub fn is_open(&self) -> bool {
        !self.file.is_null() || !self.buffer.is_null()
    }

    /// # Safety
    /// `path` is a C string.
    unsafe fn open_mode(&mut self, path: *const c_char, mode: &CStr) -> bool {
        self.close();
        // SAFETY: a C string (contract)
        if unsafe { *path } == 0 {
            return false;
        }
        self.file = unsafe { PlatFOpen(path, mode.as_ptr()) };
        self.memory_mode = false;
        !self.file.is_null()
    }

    /// Opens a file for binary reading; false for an empty path or a failed open.
    ///
    /// # Safety
    /// `path` is a C string.
    pub unsafe fn open(&mut self, path: *const c_char) -> bool {
        unsafe { self.open_mode(path, c"rb") }
    }

    /// Opens a file for text reading.
    ///
    /// # Safety
    /// `path` is a C string.
    pub unsafe fn open_text(&mut self, path: *const c_char) -> bool {
        unsafe { self.open_mode(path, c"r") }
    }

    /// Reads from a memory buffer (not owned).
    pub fn open_memory(&mut self, data: *const u8, length: c_ulong) {
        self.close();
        self.buffer = data;
        self.length = length;
        self.memory_mode = true;
    }

    /// Releases the buffer (freeing it when owned) and closes the file.
    pub fn close(&mut self) {
        if !self.buffer.is_null() {
            if self.own_buffer {
                // SAFETY: an owned buffer is `read_into_memory`'s malloc
                unsafe { free(self.buffer as *mut c_void) };
            }
            self.buffer = std::ptr::null();
            self.length = 0;
            self.position = 0;
            self.own_buffer = false;
        }
        if !self.file.is_null() {
            // SAFETY: the open FILE*
            unsafe { fclose(self.file) };
            self.file = std::ptr::null_mut();
        }
        self.memory_mode = false;
    }

    pub fn pos(&self) -> c_ulong {
        if !self.buffer.is_null() {
            self.position
        } else if !self.file.is_null() {
            // SAFETY: the open FILE*
            unsafe { ftell(self.file) as c_ulong }
        } else {
            0
        }
    }

    /// The data's length; a file is measured once (seeking to its end), which sets `memory_mode`.
    pub fn length(&mut self) -> (c_ulong, i32) {
        if self.memory_mode || self.file.is_null() {
            return (self.length, WON_FILE_OK);
        }
        let old = self.pos();
        // SAFETY: the open FILE*
        unsafe { fseek(self.file, 0, SEEK_END) };
        self.length = self.pos();
        self.memory_mode = true;
        let status = self.set_pos(old);
        (self.length, status)
    }

    pub fn set_pos(&mut self, pos: c_ulong) -> i32 {
        if !self.buffer.is_null() {
            if pos >= self.length {
                return WON_FILE_SETPOS_MEM;
            }
            self.position = pos;
        // SAFETY: the open FILE*
        } else if !self.file.is_null() && unsafe { fseek(self.file, pos as c_long, SEEK_SET) } != 0 {
            return WON_FILE_SETPOS_FILE;
        }
        WON_FILE_OK
    }

    /// Reads up to `max` bytes; the count read.
    ///
    /// # Safety
    /// `out` holds `max` bytes.
    pub unsafe fn read_max(&mut self, out: *mut u8, max: c_int) -> (c_int, i32) {
        if !self.buffer.is_null() {
            let mut count = self.length.wrapping_sub(self.position) as c_int;
            if count > max {
                count = max;
            }
            if count > 0 {
                // SAFETY: `count` bytes remain in the buffer and fit `out`
                unsafe { std::ptr::copy_nonoverlapping(self.buffer.add(self.position as usize), out, count as usize) };
            }
            self.position = self.position.wrapping_add(count as c_ulong);
            return (count, WON_FILE_OK);
        }
        if self.file.is_null() {
            return (0, WON_FILE_NULL);
        }
        // SAFETY: the open FILE*, `out` (contract)
        (unsafe { fread(out.cast(), 1, max as usize, self.file) } as c_int, WON_FILE_OK)
    }

    /// Reads exactly `length` bytes.
    ///
    /// # Safety
    /// `out` holds `length` bytes.
    pub unsafe fn read_bytes(&mut self, out: *mut u8, length: c_ulong) -> i32 {
        if !self.buffer.is_null() {
            if self.position.wrapping_add(length) > self.length {
                return WON_FILE_PAST_END_MEM;
            }
            // SAFETY: checked against the buffer's length
            unsafe { std::ptr::copy_nonoverlapping(self.buffer.add(self.position as usize), out, length as usize) };
            self.position += length;
            WON_FILE_OK
        } else if self.file.is_null() {
            WON_FILE_NULL
        // SAFETY: the open FILE*, `out` (contract)
        } else if unsafe { fread(out.cast(), 1, length as usize, self.file) } != length as usize {
            WON_FILE_PAST_END_FILE
        } else {
            WON_FILE_OK
        }
    }

    /// One byte, -1 at the end of the data or with nothing open.
    pub fn try_read_byte(&mut self) -> c_int {
        if !self.buffer.is_null() {
            if self.position < self.length {
                // SAFETY: inside the buffer
                let b = unsafe { *self.buffer.add(self.position as usize) };
                self.position += 1;
                return b as c_int;
            }
        } else if !self.file.is_null() {
            // SAFETY: the open FILE*
            return unsafe { fgetc(self.file) };
        }
        -1
    }

    pub fn skip(&mut self, length: c_ulong) -> i32 {
        if !self.buffer.is_null() {
            if self.position.wrapping_add(length) > self.length {
                return WON_FILE_PAST_END_MEM;
            }
            self.position += length;
            WON_FILE_OK
        } else if self.file.is_null() {
            WON_FILE_NULL
        // SAFETY: the open FILE*
        } else if unsafe { fseek(self.file, length as c_long, SEEK_CUR) } != 0 {
            WON_FILE_PAST_END_FILE
        } else {
            WON_FILE_OK
        }
    }

    /// Loads the open file into an owned buffer and returns to the read position (which fails, as before, when the
    /// position was at or past the end: the buffer is open then, at 0).
    pub fn read_into_memory(&mut self) -> i32 {
        if !self.buffer.is_null() || self.file.is_null() {
            return WON_FILE_OK;
        }
        let (size, status) = self.length();
        if status != WON_FILE_OK {
            return status;
        }
        if size == 0 {
            return WON_FILE_OK;
        }
        let old = self.pos();
        let status = self.set_pos(0);
        if status != WON_FILE_OK {
            return status;
        }
        // SAFETY: a fresh block of `size` bytes, freed by `close`
        let data = unsafe { malloc(size as usize) } as *mut u8;
        let (count, _) = unsafe { self.read_max(data, size as c_int) };
        self.open_memory(data, count as c_ulong);
        self.own_buffer = true;
        self.set_pos(old)
    }

    pub fn read_u8(&mut self) -> (u8, i32) {
        let mut b = [0u8; 1];
        // SAFETY: one byte
        let status = unsafe { self.read_bytes(b.as_mut_ptr(), 1) };
        (b[0], status)
    }

    pub fn read_u16(&mut self) -> (u16, i32) {
        let mut b = [0u8; 2];
        // SAFETY: two bytes
        let status = unsafe { self.read_bytes(b.as_mut_ptr(), 2) };
        (u16::from_ne_bytes(b), status)
    }

    pub fn read_i32(&mut self) -> (i32, i32) {
        let mut b = [0u8; 4];
        // SAFETY: four bytes
        let status = unsafe { self.read_bytes(b.as_mut_ptr(), 4) };
        (i32::from_ne_bytes(b), status)
    }

    /// A length field of 1, 2 or 4 bytes; 0 (nothing read) for any other size.
    pub fn read_length(&mut self, field_size: c_int) -> (c_int, i32) {
        match field_size {
            1 => {
                let (v, s) = self.read_u8();
                (v as c_int, s)
            }
            2 => {
                let (v, s) = self.read_u16();
                (v as c_int, s)
            }
            4 => self.read_i32(),
            _ => (0, WON_FILE_OK),
        }
    }

    pub fn rewind(&mut self) {
        if !self.buffer.is_null() {
            self.position = 0;
        } else if !self.file.is_null() {
            // SAFETY: the open FILE*
            unsafe { rewind(self.file) };
        }
    }

    pub fn end_of_file(&self) -> bool {
        if !self.buffer.is_null() {
            self.position >= self.length
        } else if !self.file.is_null() {
            // SAFETY: the open FILE*
            unsafe { feof(self.file) != 0 }
        } else {
            true
        }
    }
}

impl Default for EeWonFileReader {
    fn default() -> Self {
        Self::new()
    }
}

impl EeWonFileWriter {
    pub const fn new() -> Self {
        EeWonFileWriter { file: std::ptr::null_mut() }
    }

    /// Opens a file for binary writing; false for an empty path or a failed open. (The C++ retries once after
    /// clearing the file's read-only flag when asked.)
    ///
    /// # Safety
    /// `path` is a C string.
    pub unsafe fn open(&mut self, path: *const c_char) -> bool {
        self.close();
        // SAFETY: a C string (contract)
        if unsafe { *path } == 0 {
            return false;
        }
        self.file = unsafe { PlatFOpen(path, c"wb".as_ptr()) };
        !self.file.is_null()
    }

    pub fn close(&mut self) {
        if !self.file.is_null() {
            // SAFETY: the open FILE*
            unsafe { fclose(self.file) };
            self.file = std::ptr::null_mut();
        }
    }

    pub fn pos(&self) -> c_ulong {
        if self.file.is_null() {
            0
        } else {
            // SAFETY: the open FILE*
            unsafe { ftell(self.file) as c_ulong }
        }
    }

    pub fn set_pos(&mut self, pos: c_ulong) -> i32 {
        // SAFETY: the open FILE*
        if !self.file.is_null() && unsafe { fseek(self.file, pos as c_long, SEEK_SET) } != 0 {
            return WON_FILE_SETPOS_FILE;
        }
        WON_FILE_OK
    }

    /// # Safety
    /// `data` holds `length` bytes.
    pub unsafe fn write_bytes(&mut self, data: *const u8, length: c_ulong) -> i32 {
        if self.file.is_null() {
            return WON_FILE_NULL;
        }
        // SAFETY: the open FILE*, `data` (contract)
        if unsafe { fwrite(data.cast(), 1, length as usize, self.file) } != length as usize {
            return WON_FILE_WRITE_FAILED;
        }
        WON_FILE_OK
    }

    pub fn write(&mut self, data: &[u8]) -> i32 {
        // SAFETY: a slice
        unsafe { self.write_bytes(data.as_ptr(), data.len() as c_ulong) }
    }

    /// A length as a 1, 2 or 4 byte field (native order, as the original's raw writes); nothing for any other size.
    pub fn write_length(&mut self, length: c_ulong, field_size: u8) -> i32 {
        match field_size {
            1 => self.write(&[length as u8]),
            2 => self.write(&(length as u16).to_ne_bytes()),
            4 => self.write(&(length as i32).to_ne_bytes()),
            _ => WON_FILE_OK,
        }
    }
}

impl Default for EeWonFileWriter {
    fn default() -> Self {
        Self::new()
    }
}

// ---- the C ABI (WONCommon/FileReader.cpp, FileWriter.cpp)

/// The exception text of a status.
#[no_mangle]
pub extern "C" fn ee_won_file_status_text(status: i32) -> *const c_char {
    status_text(status).as_ptr()
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_init(reader: &mut EeWonFileReader) {
    *reader = EeWonFileReader::new();
}

/// # Safety
/// `path` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_reader_open(reader: &mut EeWonFileReader, path: *const c_char) -> bool {
    unsafe { reader.open(path) }
}

/// # Safety
/// `path` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_reader_open_text(reader: &mut EeWonFileReader, path: *const c_char) -> bool {
    unsafe { reader.open_text(path) }
}

/// The buffer is not owned and must outlive the reading.
#[no_mangle]
pub extern "C" fn ee_won_file_reader_open_memory(reader: &mut EeWonFileReader, data: *const c_void, length: c_ulong) {
    reader.open_memory(data.cast(), length);
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_close(reader: &mut EeWonFileReader) {
    reader.close();
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_pos(reader: &EeWonFileReader) -> c_ulong {
    reader.pos()
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_length(reader: &mut EeWonFileReader, status: &mut i32) -> c_ulong {
    let (length, s) = reader.length();
    *status = s;
    length
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_set_pos(reader: &mut EeWonFileReader, pos: c_ulong) -> i32 {
    reader.set_pos(pos)
}

/// # Safety
/// `out` holds `max` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_reader_read_max(
    reader: &mut EeWonFileReader,
    out: *mut c_void,
    max: c_int,
    status: &mut i32,
) -> c_int {
    let (count, s) = unsafe { reader.read_max(out.cast(), max) };
    *status = s;
    count
}

/// # Safety
/// `out` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_reader_read_bytes(
    reader: &mut EeWonFileReader,
    out: *mut c_void,
    length: c_ulong,
) -> i32 {
    unsafe { reader.read_bytes(out.cast(), length) }
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_try_read_byte(reader: &mut EeWonFileReader) -> c_int {
    reader.try_read_byte()
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_skip(reader: &mut EeWonFileReader, length: c_ulong) -> i32 {
    reader.skip(length)
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_read_into_memory(reader: &mut EeWonFileReader) -> i32 {
    reader.read_into_memory()
}

/// A 1, 2 or 4 byte length field (0 for another size), into `value`.
#[no_mangle]
pub extern "C" fn ee_won_file_reader_read_length(
    reader: &mut EeWonFileReader,
    field_size: c_int,
    value: &mut c_int,
) -> i32 {
    let (v, s) = reader.read_length(field_size);
    *value = v;
    s
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_rewind(reader: &mut EeWonFileReader) {
    reader.rewind();
}

#[no_mangle]
pub extern "C" fn ee_won_file_reader_end_of_file(reader: &EeWonFileReader) -> bool {
    reader.end_of_file()
}

#[no_mangle]
pub extern "C" fn ee_won_file_writer_init(writer: &mut EeWonFileWriter) {
    *writer = EeWonFileWriter::new();
}

/// # Safety
/// `path` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_writer_open(writer: &mut EeWonFileWriter, path: *const c_char) -> bool {
    unsafe { writer.open(path) }
}

#[no_mangle]
pub extern "C" fn ee_won_file_writer_close(writer: &mut EeWonFileWriter) {
    writer.close();
}

#[no_mangle]
pub extern "C" fn ee_won_file_writer_pos(writer: &EeWonFileWriter) -> c_ulong {
    writer.pos()
}

#[no_mangle]
pub extern "C" fn ee_won_file_writer_set_pos(writer: &mut EeWonFileWriter, pos: c_ulong) -> i32 {
    writer.set_pos(pos)
}

/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_writer_write_bytes(
    writer: &mut EeWonFileWriter,
    data: *const c_void,
    length: c_ulong,
) -> i32 {
    unsafe { writer.write_bytes(data.cast(), length) }
}

#[no_mangle]
pub extern "C" fn ee_won_file_writer_write_length(
    writer: &mut EeWonFileWriter,
    length: c_ulong,
    field_size: u8,
) -> i32 {
    writer.write_length(length, field_size)
}

#[cfg(test)]
#[path = "file_golden.rs"]
mod file_golden;
