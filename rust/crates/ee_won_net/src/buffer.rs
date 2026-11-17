//! WONCommon's buffers as Rust objects (round 12, the first whole-class move): `ByteBuffer` (a reference-counted
//! byte array: the C++ class is a `RefCount` shim holding an `EeWonByteBuffer`), `WriteBuffer` (a growable message
//! with room for its length prefix: the C++ shim holds an `EeWonWriteBuffer`) and `ReadBuffer` (a reader over bytes
//! it does not own: the C++ shim embeds `EeWonReadBuffer` by value, like FileReader's core). The state, growth,
//! prefixes, bounds and field codings are here; the C++ keeps the reference count, the exception it throws when a
//! read runs past the end and the `std::string`/`std::wstring` copies.
//!
//! The C++ the shims replaced (WONCommon's ByteBuffer.cpp, OffsetByteBuffer, ReadBuffer.cpp and WriteBuffer.cpp)
//! answered `tests/won/won_buffer_test.cpp`'s script as `golden_buffer.txt` records; `buffer_golden` replays it here.
//! Kept: the capacity's growth (from 32, doubling; `ReserveChunk`'s cap), 6 spare bytes in front of the data for the
//! prefix, `Release` emptying the writer, a reader's bound as the original's unsigned compare. Changed: the bytes a
//! growth, `SetSize` or `SkipBytes` adds are zero (they were indeterminate); a `SetBytes` past the allocation stops
//! the game (it wrote outside it); `ReadRawWString`'s byte count no longer wraps at 32 bits on Windows (it read
//! outside the buffer).

// (c_ulong and c_long are 32 bits on Windows, 64 elsewhere: the casts are needed off Windows)
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_long, c_ulong, CStr};

use crate::wire;

/// Spare bytes in front of a writer's data: room for the longest length prefix (0xffff and a 4-byte length).
const PREFIX_ROOM: usize = 6;

/// `ByteBuffer`'s bytes: owned, possibly a view into a writer's former allocation, or none (`ByteBuffer(NULL)`).
#[derive(Debug, Default)]
pub struct EeWonByteBuffer {
    storage: Option<Vec<u8>>,
    offset: usize,
    length: usize,
}

impl EeWonByteBuffer {
    /// A copy of `bytes`.
    pub fn copy(bytes: &[u8]) -> EeWonByteBuffer {
        EeWonByteBuffer { storage: Some(bytes.to_vec()), offset: 0, length: bytes.len() }
    }

    /// `ByteBuffer(const char*)`: the text and its terminator; none gives no bytes (a null `data()`).
    pub fn from_text(text: Option<&CStr>) -> EeWonByteBuffer {
        match text {
            None => EeWonByteBuffer::default(),
            Some(t) => EeWonByteBuffer::copy(t.to_bytes_with_nul()),
        }
    }

    /// The bytes (empty for none).
    pub fn bytes(&self) -> &[u8] {
        match &self.storage {
            Some(s) => &s[self.offset..self.offset + self.length],
            None => &[],
        }
    }

    /// Whether there are no bytes at all (`data()` is null).
    pub fn is_null(&self) -> bool {
        self.storage.is_none()
    }
}

/// `WriteBuffer`'s state.
#[derive(Debug)]
pub struct EeWonWriteBuffer {
    field_size: u8,
    includes_field: bool,
    capacity: c_ulong,
    length: c_ulong,
    /// `capacity + PREFIX_ROOM` bytes, the data after the room; none before the first growth
    allocation: Option<Vec<u8>>,
}

impl EeWonWriteBuffer {
    /// `WriteBuffer(lengthFieldSize, lengthIncludesField)`.
    pub fn new(field_size: u8, includes_field: bool) -> EeWonWriteBuffer {
        EeWonWriteBuffer { field_size, includes_field, capacity: 0, length: 0, allocation: None }
    }

    pub fn capacity(&self) -> c_ulong {
        self.capacity
    }

    pub fn length(&self) -> c_ulong {
        self.length
    }

    /// The written bytes (none before the first growth).
    pub fn data(&self) -> Option<&[u8]> {
        self.allocation.as_ref().map(|a| &a[PREFIX_ROOM..PREFIX_ROOM + self.length as usize])
    }

    /// The data's start (null before the first growth); `capacity` bytes may be written there.
    pub fn data_ptr(&mut self) -> *mut u8 {
        match &mut self.allocation {
            Some(a) => a[PREFIX_ROOM..].as_mut_ptr(),
            None => std::ptr::null_mut(),
        }
    }

    /// `Reset`: frees the allocation and empties the buffer.
    pub fn reset(&mut self) {
        self.allocation = None;
        self.length = 0;
        self.capacity = 0;
    }

    /// `Reserve`: a new allocation of at least `capacity` bytes keeping the written ones.
    pub fn reserve(&mut self, capacity: c_ulong) {
        if self.capacity >= capacity {
            return;
        }
        let mut allocation = vec![0u8; capacity as usize + PREFIX_ROOM];
        if let Some(old) = &self.allocation {
            let keep = (self.length as usize).min(old.len() - PREFIX_ROOM);
            allocation[PREFIX_ROOM..PREFIX_ROOM + keep].copy_from_slice(&old[PREFIX_ROOM..PREFIX_ROOM + keep]);
        }
        self.allocation = Some(allocation);
        self.capacity = capacity;
    }

    /// `CheckWrite`: grows (from 32, doubling) so `add` more bytes fit.
    pub fn check_write(&mut self, add: c_ulong) {
        if let Some(c) = wire::grown_capacity(u64::from(self.capacity), u64::from(self.length), u64::from(add)) {
            self.reserve(c as c_ulong);
        }
    }

    /// `ReserveChunk`: room for `total` bytes, capped at `max` when that suffices.
    pub fn reserve_chunk(&mut self, total: c_ulong, max: c_ulong) {
        if let Some(c) = wire::chunk_capacity(u64::from(self.capacity), u64::from(total), u64::from(max)) {
            self.reserve(c as c_ulong);
        }
    }

    /// `SetSize`: the written length, growing as needed.
    pub fn set_size(&mut self, size: c_ulong) {
        self.reserve(size);
        self.length = size;
    }

    /// `SkipBytes`: `n` bytes at the end, to be set later.
    pub fn skip_bytes(&mut self, n: c_ulong) {
        self.check_write(n);
        self.length = self.length.wrapping_add(n);
    }

    /// `AppendBytes`.
    pub fn append_bytes(&mut self, bytes: &[u8]) {
        self.check_write(bytes.len() as c_ulong);
        if !bytes.is_empty() {
            let at = PREFIX_ROOM + self.length as usize;
            let a = self.allocation.as_mut().expect("grown");
            a[at..at + bytes.len()].copy_from_slice(bytes);
        }
        self.length = self.length.wrapping_add(bytes.len() as c_ulong);
    }

    /// `AppendLength`: a 1, 2 or 4-byte field (nothing for another size).
    pub fn append_length(&mut self, length: c_ulong, size: u8) {
        self.append_bytes(&wire::encode_length(length as u32, size));
    }

    /// `AppendString` / `AppendBuffer`: a length field, then the bytes.
    pub fn append_field(&mut self, bytes: &[u8], size: u8) {
        self.append_length(bytes.len() as c_ulong, size);
        self.append_bytes(bytes);
    }

    /// `AppendWString` of UTF-16 little-endian bytes: a length in units, then the bytes.
    pub fn append_utf16le(&mut self, bytes: &[u8], size: u8) {
        self.append_length((bytes.len() / 2) as c_ulong, size);
        self.append_bytes(bytes);
    }

    /// `SetBytes`: overwrites at `at` (within the capacity; the C++ did not check; past the allocation stops).
    pub fn set_bytes(&mut self, at: c_ulong, bytes: &[u8]) {
        if bytes.is_empty() {
            return;
        }
        let a = self.allocation.as_mut().expect("WriteBuffer::SetBytes before any growth");
        let at = PREFIX_ROOM + at as usize;
        a[at..at + bytes.len()].copy_from_slice(bytes);
    }

    /// `ToByteBuffer`: the prefix in front of the data, as a byte buffer; `release` hands the allocation over and
    /// empties the writer, otherwise the result is a copy.
    pub fn to_byte_buffer(&mut self, release: bool) -> EeWonByteBuffer {
        // (a buffer never written to has no room for the prefix: the C++ grew it first)
        if self.allocation.is_none() {
            self.reserve(32);
        }
        let prefix = wire::length_prefix(self.field_size, self.includes_field, self.length as u32);
        let start = PREFIX_ROOM - prefix.len();
        let length = self.length as usize + prefix.len();
        let a = self.allocation.as_mut().expect("grown");
        a[start..PREFIX_ROOM].copy_from_slice(&prefix);
        if release {
            let storage = self.allocation.take();
            self.reset();
            EeWonByteBuffer { storage, offset: start, length }
        } else {
            EeWonByteBuffer::copy(&a[start..start + length])
        }
    }
}

/// `ReadBuffer`'s state, embedded by value in the C++ object (a reader over bytes someone else owns).
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EeWonReadBuffer {
    /// the read position
    pub pos: c_long,
    pub data: *const u8,
    pub length: c_long,
}

impl EeWonReadBuffer {
    /// `ReadBuffer(data, length)` / `SetData`: rewound.
    pub fn new(data: *const u8, length: c_ulong) -> EeWonReadBuffer {
        EeWonReadBuffer { pos: 0, data, length: length as c_long }
    }

    /// `length()`.
    pub fn len(&self) -> c_ulong {
        self.length as c_ulong
    }

    pub fn is_empty(&self) -> bool {
        self.length == 0
    }

    /// `ReadBytes`'s bound and step: the offset of the next `amount` bytes, or None past the end (the original's
    /// unsigned compare of what is left).
    pub fn take(&mut self, amount: u64) -> Option<usize> {
        let left = u64::from(self.len()).wrapping_sub(self.pos as i64 as u64);
        if left < amount {
            return None;
        }
        let at = self.pos as usize;
        self.pos = (self.pos as i64).wrapping_add(amount as i64) as c_long;
        Some(at)
    }

    /// The bytes at an offset `take` gave.
    ///
    /// # Safety
    /// `data` holds `length` bytes and `at + n` is within them.
    pub unsafe fn slice(&self, at: usize, n: usize) -> &[u8] {
        if n == 0 {
            return &[];
        }
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(self.data.add(at), n) }
    }

    /// `ReadBytes` as a slice.
    ///
    /// # Safety
    /// As `slice`: `data` holds `length` bytes and the position is within them.
    pub unsafe fn read(&mut self, n: u64) -> Option<&[u8]> {
        let at = self.take(n)?;
        // SAFETY: `take` checked the bound
        Some(unsafe { self.slice(at, n as usize) })
    }

    /// `ReadLength`: a sign-extended 1, 2 or 4-byte field; 0 for another size.
    ///
    /// # Safety
    /// As `read`.
    pub unsafe fn read_length(&mut self, size: u8) -> Option<u32> {
        // SAFETY: as `read`
        unsafe {
            Some(match size {
                1 => i32::from(self.read(1)?[0] as i8) as u32,
                2 => {
                    let b = self.read(2)?;
                    i32::from(i16::from_le_bytes([b[0], b[1]])) as u32
                }
                4 => {
                    let b = self.read(4)?;
                    u32::from_le_bytes([b[0], b[1], b[2], b[3]])
                }
                _ => 0,
            })
        }
    }
}

// ---- the C ABI of the C++ shims (WONBuffer.h)

/// `ByteBuffer(data, length, false)`: a copy.
///
/// # Safety
/// `data` holds `length` bytes (or `length` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_bytes_copy(data: *const u8, length: c_ulong) -> *mut EeWonByteBuffer {
    // SAFETY: the caller's contract
    let b = unsafe { crate::ffi::bytes(data, length as usize) };
    Box::into_raw(Box::new(EeWonByteBuffer::copy(b)))
}

/// `ByteBuffer(const char*)`: the text and its terminator; null gives no bytes.
///
/// # Safety
/// `text` is null or a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bytes_text(text: *const c_char) -> *mut EeWonByteBuffer {
    // SAFETY: the caller's contract
    let t = if text.is_null() { None } else { Some(unsafe { CStr::from_ptr(text) }) };
    Box::into_raw(Box::new(EeWonByteBuffer::from_text(t)))
}

/// # Safety
/// `b` came from an `ee_won_bytes_*` constructor or `ee_won_writer_to_bytes` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bytes_free(b: *mut EeWonByteBuffer) {
    if !b.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(b) });
    }
}

/// `data()`: null for a buffer of no bytes at all.
///
/// # Safety
/// `b` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bytes_data(b: *const EeWonByteBuffer) -> *const u8 {
    // SAFETY: the caller's contract
    let b = unsafe { &*b };
    if b.is_null() {
        std::ptr::null()
    } else {
        b.bytes().as_ptr()
    }
}

/// `length()`.
///
/// # Safety
/// `b` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bytes_length(b: *const EeWonByteBuffer) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*b).length as c_ulong }
}

/// `WriteBuffer(lengthFieldSize, lengthIncludesField)`.
#[no_mangle]
pub extern "C" fn ee_won_writer_new(field_size: u8, includes_field: bool) -> *mut EeWonWriteBuffer {
    Box::into_raw(Box::new(EeWonWriteBuffer::new(field_size, includes_field)))
}

/// # Safety
/// `w` came from `ee_won_writer_new` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_free(w: *mut EeWonWriteBuffer) {
    if !w.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(w) });
    }
}

/// `data()`: the data's start (null before the first growth); `capacity()` bytes may be written there until the
/// next call that grows, resets or releases.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_data(w: *mut EeWonWriteBuffer) -> *mut u8 {
    // SAFETY: the caller's contract
    unsafe { (*w).data_ptr() }
}

/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_length(w: *const EeWonWriteBuffer) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*w).length }
}

/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_capacity(w: *const EeWonWriteBuffer) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*w).capacity }
}

/// `Reset` (and `Release`, which forgot the allocation another owner had taken: no caller takes it any more).
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_reset(w: *mut EeWonWriteBuffer) {
    // SAFETY: the caller's contract
    unsafe { (*w).reset() }
}

/// `Reserve`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_reserve(w: *mut EeWonWriteBuffer, capacity: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).reserve(capacity) }
}

/// `CheckWrite`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_check_write(w: *mut EeWonWriteBuffer, add: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).check_write(add) }
}

/// `ReserveChunk`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_reserve_chunk(w: *mut EeWonWriteBuffer, total: c_ulong, max: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).reserve_chunk(total, max) }
}

/// `SetSize`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_set_size(w: *mut EeWonWriteBuffer, size: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).set_size(size) }
}

/// `SkipBytes`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_skip(w: *mut EeWonWriteBuffer, n: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).skip_bytes(n) }
}

/// `AppendBytes` (and the byte, bool, short and long appends of their little-endian bytes).
///
/// # Safety
/// `w` is valid; `data` holds `length` bytes and is not the writer's own.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_append(w: *mut EeWonWriteBuffer, data: *const u8, length: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).append_bytes(crate::ffi::bytes(data, length as usize)) }
}

/// `AppendLength`.
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_append_length(w: *mut EeWonWriteBuffer, length: c_ulong, size: u8) {
    // SAFETY: the caller's contract
    unsafe { (*w).append_length(length, size) }
}

/// `AppendString` / `AppendBuffer`: a `size`-byte length field, then the bytes.
///
/// # Safety
/// As `ee_won_writer_append`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_append_field(
    w: *mut EeWonWriteBuffer,
    data: *const u8,
    length: c_ulong,
    size: u8,
) {
    // SAFETY: the caller's contract
    unsafe { (*w).append_field(crate::ffi::bytes(data, length as usize), size) }
}

/// `AppendWString` of `length` UTF-16 little-endian bytes: a unit count field, then the bytes.
///
/// # Safety
/// As `ee_won_writer_append`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_append_utf16le(
    w: *mut EeWonWriteBuffer,
    data: *const u8,
    length: c_ulong,
    size: u8,
) {
    // SAFETY: the caller's contract
    unsafe { (*w).append_utf16le(crate::ffi::bytes(data, length as usize), size) }
}

/// `SetBytes` (and `SetByte`, `SetShort`).
///
/// # Safety
/// `w` is valid; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_set(w: *mut EeWonWriteBuffer, at: c_ulong, data: *const u8, length: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*w).set_bytes(at, crate::ffi::bytes(data, length as usize)) }
}

/// `ToByteBuffer`: a new byte buffer (the caller frees it with `ee_won_bytes_free`).
///
/// # Safety
/// `w` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_writer_to_bytes(w: *mut EeWonWriteBuffer, release: bool) -> *mut EeWonByteBuffer {
    // SAFETY: the caller's contract
    Box::into_raw(Box::new(unsafe { (*w).to_byte_buffer(release) }))
}

/// `ReadBuffer(data, length)` / `SetData`: points the reader at the bytes, rewound.
///
/// # Safety
/// `r` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reader_set(r: *mut EeWonReadBuffer, data: *const u8, length: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { *r = EeWonReadBuffer::new(data, length) }
}

/// `ReadBytes`: the next `amount` bytes into `*out`, skipped; false past the end (nothing moves).
///
/// # Safety
/// `r` and `out` are valid; the reader's bytes are alive.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reader_take(r: *mut EeWonReadBuffer, amount: u64, out: *mut *const u8) -> bool {
    // SAFETY: the caller's contract
    let r = unsafe { &mut *r };
    match r.take(amount) {
        Some(at) => {
            // SAFETY: `out` is valid; the offset is within the bytes (or the end)
            unsafe { *out = r.data.wrapping_add(at) };
            true
        }
        None => false,
    }
}

/// `ReadLength`: into `*out`; false past the end.
///
/// # Safety
/// As `ee_won_reader_take`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reader_length(r: *mut EeWonReadBuffer, size: u8, out: *mut u32) -> bool {
    // SAFETY: the caller's contract
    match unsafe { (*r).read_length(size) } {
        Some(v) => {
            // SAFETY: `out` is valid
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn growth_from_32_doubling_and_the_prefix_room() {
        let mut w = EeWonWriteBuffer::new(2, true);
        assert!(w.data().is_none());
        w.append_bytes(&[1]);
        assert_eq!((w.capacity(), w.length()), (32, 1));
        w.append_bytes(&[0; 40]);
        assert_eq!(w.capacity(), 64);
        w.reserve(10);
        assert_eq!(w.capacity(), 64);
        let b = w.to_byte_buffer(false);
        assert_eq!(&b.bytes()[..3], &[43, 0, 1]);
        let b = w.to_byte_buffer(true);
        assert_eq!(b.bytes().len(), 43);
        assert!(w.data().is_none() && w.capacity() == 0);
    }

    #[test]
    fn a_long_two_byte_prefix_uses_the_six_bytes() {
        let mut w = EeWonWriteBuffer::new(2, false);
        w.skip_bytes(0xffff);
        let b = w.to_byte_buffer(true);
        assert_eq!(&b.bytes()[..6], &[0xff, 0xff, 0xff, 0xff, 0, 0]);
        assert_eq!(b.bytes().len(), 0xffff + 6);
    }

    #[test]
    fn an_unwritten_buffer_grows_for_its_prefix() {
        let mut w = EeWonWriteBuffer::new(4, true);
        let b = w.to_byte_buffer(false);
        assert_eq!(b.bytes(), &[4, 0, 0, 0]);
        assert_eq!(w.capacity(), 32);
    }

    #[test]
    fn text_buffers_keep_the_terminator_and_null_is_none() {
        assert!(EeWonByteBuffer::from_text(None).is_null());
        assert_eq!(EeWonByteBuffer::from_text(Some(c"hi")).bytes(), b"hi\0");
        assert!(!EeWonByteBuffer::copy(&[]).is_null());
    }

    #[test]
    fn the_reader_bound_and_sign_extended_lengths() {
        let data = [0xffu8, 0xff, 5, 0x80];
        let mut r = EeWonReadBuffer::new(data.as_ptr(), 4);
        unsafe {
            assert_eq!(r.read_length(2), Some(0xffff_ffff));
            assert_eq!(r.read_length(3), Some(0));
            assert_eq!(r.read_length(1), Some(5));
            assert_eq!(r.read_length(4), None);
            assert_eq!(r.pos, 3);
            assert_eq!(r.read_length(1), Some(0xffff_ff80));
            assert_eq!(r.read(0), Some(&[][..]));
            assert_eq!(r.read(1), None);
        }
    }

    #[test]
    fn set_size_keeps_the_written_bytes_and_zero_fills() {
        let mut w = EeWonWriteBuffer::new(0, false);
        w.append_bytes(&[7, 8, 9]);
        w.set_size(1);
        w.set_size(20);
        assert_eq!(&w.data().unwrap()[..4], &[7, 8, 9, 0]);
        // a growth keeps only the written length
        w.set_size(1);
        w.set_size(100);
        assert_eq!(&w.data().unwrap()[..4], &[7, 0, 0, 0]);
        w.set_bytes(98, &[1, 2]);
        assert_eq!(&w.data().unwrap()[98..], &[1, 2]);
    }
}
