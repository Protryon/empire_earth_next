//! WON's message buffers (`WONBuffer.h`: `WriteBuffer`, `ReadBuffer`), byte for byte.
//!
//! Fields are little-endian; "longs" are 32 bits on every system. A length field is 1, 2 or 4 bytes (any other size
//! writes nothing and reads 0) and reads sign-extended to 32 bits (a 2-byte 0xffff is 0xffffffff). Wide strings are
//! a length in UTF-16 units, then the units (`text`). A finished message carries a length prefix in front of the
//! data: 1, 2 or 4 bytes, counting the prefix itself when asked; a 2-byte prefix of 0xffff or more becomes 0xffff and
//! a 4-byte length.

use crate::text;

/// Reading ran past the end (the C++ threw `ReadBufferException("Attempt to read past end of buffer.")`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PastEnd;

/// A length field's bytes (empty for a size other than 1, 2 or 4).
pub fn encode_length(length: u32, size: u8) -> Vec<u8> {
    match size {
        1 => vec![length as u8],
        2 => (length as u16).to_le_bytes().to_vec(),
        4 => length.to_le_bytes().to_vec(),
        _ => Vec::new(),
    }
}

/// The prefix `ToByteBuffer` writes in front of `length` data bytes.
pub fn length_prefix(field_size: u8, includes_field: bool, length: u32) -> Vec<u8> {
    match field_size {
        1 => vec![length.wrapping_add(u32::from(includes_field)) as u8],
        2 if length < 0xffff => (length.wrapping_add(if includes_field { 2 } else { 0 }) as u16).to_le_bytes().to_vec(),
        2 => {
            let mut p = vec![0xff, 0xff];
            p.extend_from_slice(&length.wrapping_add(if includes_field { 4 } else { 0 }).to_le_bytes());
            p
        }
        4 => length.wrapping_add(if includes_field { 4 } else { 0 }).to_le_bytes().to_vec(),
        _ => Vec::new(),
    }
}

/// `CheckWrite`'s growth: the capacity for `length + add` bytes (from 32, doubling), or None when it fits.
pub fn grown_capacity(capacity: u64, length: u64, add: u64) -> Option<u64> {
    let needed = length.wrapping_add(add);
    if needed <= capacity {
        return None;
    }
    let mut c = (capacity as i64).max(32);
    while c < needed as i64 {
        c <<= 1;
    }
    Some(c as u64)
}

/// `ReserveChunk`'s growth: like `grown_capacity` for a total of `total` bytes, capped at `max` when `max` is at
/// least `total`; None when it fits.
pub fn chunk_capacity(capacity: u64, total: u64, max: u64) -> Option<u64> {
    let total_i = total as i32 as i64;
    if total_i <= capacity as i64 {
        return None;
    }
    let mut size = (capacity as i64).max(32);
    while size < total_i {
        size <<= 1;
    }
    if max >= total && size > max as i64 {
        size = max as i64;
    }
    Some(size as u64)
}

/// A message being written: `WriteBuffer`.
#[derive(Clone, Debug, Default)]
pub struct Writer {
    field_size: u8,
    includes_field: bool,
    data: Vec<u8>,
}

impl Writer {
    /// `WriteBuffer(lengthFieldSize, lengthIncludesField)`.
    pub fn new(field_size: u8, includes_field: bool) -> Writer {
        Writer { field_size, includes_field, data: Vec::new() }
    }

    /// The bytes written (no prefix).
    pub fn data(&self) -> &[u8] {
        &self.data
    }

    pub fn append_bytes(&mut self, bytes: &[u8]) {
        self.data.extend_from_slice(bytes);
    }

    pub fn append_byte(&mut self, v: u8) {
        self.data.push(v);
    }

    pub fn append_bool(&mut self, v: bool) {
        self.data.push(u8::from(v));
    }

    pub fn append_short(&mut self, v: i16) {
        self.append_bytes(&v.to_le_bytes());
    }

    pub fn append_long(&mut self, v: i32) {
        self.append_bytes(&v.to_le_bytes());
    }

    pub fn append_length(&mut self, length: u32, size: u8) {
        self.append_bytes(&encode_length(length, size));
    }

    /// `AppendString`: a length field, then the bytes.
    pub fn append_string(&mut self, s: &[u8], size: u8) {
        self.append_length(s.len() as u32, size);
        self.append_bytes(s);
    }

    /// `AppendWString`: a length in units, then the units little-endian.
    pub fn append_wstring(&mut self, units: &[u16], size: u8) {
        self.append_length(units.len() as u32, size);
        self.append_bytes(&text::utf16_to_le_bytes(units));
    }

    /// `AppendBuffer`: a length field, then the bytes (None: a zero length only).
    pub fn append_buffer(&mut self, buffer: Option<&[u8]>, size: u8) {
        let b = buffer.unwrap_or(&[]);
        self.append_length(b.len() as u32, size);
        self.append_bytes(b);
    }

    /// `SetBytes`: overwrites in place (the C++ did not check the bounds; this panics past them).
    pub fn set_bytes(&mut self, at: usize, bytes: &[u8]) {
        self.data[at..at + bytes.len()].copy_from_slice(bytes);
    }

    /// `SkipBytes`: zero bytes at the end, to be set later.
    pub fn skip_bytes(&mut self, n: usize) {
        self.data.resize(self.data.len() + n, 0);
    }

    /// `ToByteBuffer`: the length prefix, then the data.
    pub fn to_message(&self) -> Vec<u8> {
        let mut out = length_prefix(self.field_size, self.includes_field, self.data.len() as u32);
        out.extend_from_slice(&self.data);
        out
    }
}

/// A message being read: `ReadBuffer`.
#[derive(Clone, Debug)]
pub struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    pub fn new(data: &'a [u8]) -> Reader<'a> {
        Reader { data, pos: 0 }
    }

    pub fn pos(&self) -> usize {
        self.pos
    }

    pub fn available(&self) -> usize {
        self.data.len() - self.pos
    }

    /// `ReadBytes`.
    pub fn read_bytes(&mut self, n: usize) -> Result<&'a [u8], PastEnd> {
        if self.available() < n {
            return Err(PastEnd);
        }
        self.pos += n;
        Ok(&self.data[self.pos - n..self.pos])
    }

    pub fn read_byte(&mut self) -> Result<u8, PastEnd> {
        Ok(self.read_bytes(1)?[0])
    }

    pub fn read_bool(&mut self) -> Result<bool, PastEnd> {
        Ok(self.read_byte()? != 0)
    }

    pub fn read_short(&mut self) -> Result<i16, PastEnd> {
        let b = self.read_bytes(2)?;
        Ok(i16::from_le_bytes([b[0], b[1]]))
    }

    pub fn read_long(&mut self) -> Result<i32, PastEnd> {
        let b = self.read_bytes(4)?;
        Ok(i32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }

    /// `ReadLength`: sign-extended to 32 bits; 0 for a size other than 1, 2 or 4.
    pub fn read_length(&mut self, size: u8) -> Result<u32, PastEnd> {
        Ok(match size {
            1 => i32::from(self.read_byte()? as i8) as u32,
            2 => i32::from(self.read_short()?) as u32,
            4 => self.read_long()? as u32,
            _ => 0,
        })
    }

    /// `ReadString`.
    pub fn read_string(&mut self, size: u8) -> Result<&'a [u8], PastEnd> {
        let n = self.read_length(size)?;
        self.read_bytes(n as usize)
    }

    /// `ReadRawWString`: `units` UTF-16 units.
    pub fn read_raw_wstring(&mut self, units: usize) -> Result<Vec<u16>, PastEnd> {
        let bytes = self.read_bytes(units.checked_mul(2).ok_or(PastEnd)?)?;
        Ok(text::utf16_from_le_bytes(bytes))
    }

    /// `ReadWString`.
    pub fn read_wstring(&mut self, size: u8) -> Result<Vec<u16>, PastEnd> {
        let n = self.read_length(size)?;
        self.read_raw_wstring(n as usize)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_wire_test_message() {
        // tests/won/won_wire_test.cpp's message, built from Rust
        let mut w = Writer::new(4, false);
        w.append_wstring(&text::utf16_from_utf32(&[0x41, 0xe9, 0x20ac, 0x1f600]), 2);
        w.append_long(-2);
        w.append_short(0x1234);
        w.append_wstring(&[], 1);
        w.append_length(0xffff_ffff, 4);
        let want = [
            0x05, 0x00, 0x41, 0x00, 0xe9, 0x00, 0xac, 0x20, 0x3d, 0xd8, 0x00, 0xde, 0xfe, 0xff, 0xff, 0xff, 0x34, 0x12,
            0x00, 0xff, 0xff, 0xff, 0xff,
        ];
        assert_eq!(w.data(), &want);
        let msg = w.to_message();
        assert_eq!(&msg[..4], &[want.len() as u8, 0, 0, 0]);
        let mut r = Reader::new(&msg[4..]);
        assert_eq!(text::utf32_from_utf16(&r.read_wstring(2).unwrap()), vec![0x41, 0xe9, 0x20ac, 0x1f600]);
        assert_eq!(r.read_long(), Ok(-2));
        assert_eq!(r.read_short(), Ok(0x1234));
        assert_eq!(r.read_wstring(1), Ok(vec![]));
        assert_eq!(r.read_length(4), Ok(0xffff_ffff));
        assert_eq!(r.available(), 0);
        assert_eq!(r.read_byte(), Err(PastEnd));
    }

    #[test]
    fn lengths_sign_extend_and_odd_sizes_are_empty() {
        let mut r = Reader::new(&[0x80, 0xff, 0xff, 0x7f]);
        assert_eq!(r.read_length(1), Ok(0xffff_ff80));
        assert_eq!(r.read_length(2), Ok(0xffff_ffff));
        assert_eq!(r.read_length(3), Ok(0));
        assert_eq!(r.read_length(1), Ok(0x7f));
        assert!(encode_length(5, 3).is_empty());
        assert_eq!(encode_length(300, 1), vec![44]);
    }

    #[test]
    fn prefixes() {
        assert_eq!(length_prefix(0, true, 5), Vec::<u8>::new());
        assert_eq!(length_prefix(1, true, 5), vec![6]);
        assert_eq!(length_prefix(2, true, 5), vec![7, 0]);
        assert_eq!(length_prefix(2, false, 0xfffe), vec![0xfe, 0xff]);
        assert_eq!(length_prefix(2, false, 0xffff), vec![0xff, 0xff, 0xff, 0xff, 0, 0]);
        assert_eq!(length_prefix(2, true, 0x10000), vec![0xff, 0xff, 4, 0, 1, 0]);
        assert_eq!(length_prefix(4, true, 1), vec![5, 0, 0, 0]);
    }

    #[test]
    fn growth() {
        assert_eq!(grown_capacity(0, 0, 0), None);
        assert_eq!(grown_capacity(0, 0, 1), Some(32));
        assert_eq!(grown_capacity(32, 32, 1), Some(64));
        assert_eq!(grown_capacity(64, 10, 500), Some(512));
        assert_eq!(chunk_capacity(64, 10_000, 9_000), Some(16_384));
        assert_eq!(chunk_capacity(64, 10_000, 10_000), Some(10_000));
        assert_eq!(chunk_capacity(16_384, 10_000, 0), None);
    }

    #[test]
    fn strings_and_buffers() {
        let mut w = Writer::new(0, false);
        w.append_string(b"abc", 2);
        w.append_buffer(None, 1);
        w.append_buffer(Some(b"hi"), 4);
        w.append_bool(true);
        w.skip_bytes(2);
        w.set_bytes(13, &[9, 8]);
        assert_eq!(w.data(), &[3, 0, b'a', b'b', b'c', 0, 2, 0, 0, 0, b'h', b'i', 1, 9, 8]);
        assert_eq!(w.to_message(), w.data());
        let mut r = Reader::new(w.data());
        assert_eq!(r.read_string(2), Ok(&b"abc"[..]));
        assert_eq!(r.read_string(1), Ok(&b""[..]));
        assert_eq!(r.read_string(4), Ok(&b"hi"[..]));
        assert_eq!(r.read_bool(), Ok(true));
        assert_eq!(r.read_raw_wstring(2), Err(PastEnd));
        assert_eq!(r.pos(), 13);
    }
}
