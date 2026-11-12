//! `HTTPCache`'s index file (WONMisc/HTTPCache.cpp's `ReadCacheMap`, `WriteCacheMap` and the entries'
//! `ReadFromFile` / `WriteToFile`), read and written here on the Rust file layer (`file.rs`'s `FileReader` and
//! `FileWriter` state over `PlatFOpen`): the version, the count, each entry's id, valid flag, two times and four
//! strings; which entries are kept (`http::cache_keeps`) and which cached files are deleted, in the original's
//! order; a read or write that fails stops where the original's exception did. The C++ `HTTPCache` keeps its maps
//! of entry objects and the file deletions (`WONFile`), handed each entry and each deletion as a callback.
//!
//! Reached by nothing in the game (the HTTP cache served the WON patch and news pages; the servers are gone).

use std::ffi::{c_char, c_ulong, c_void};

use crate::file::{EeWonFileReader, EeWonFileWriter, WON_FILE_OK};
use crate::http::cache_keeps;

/// One entry as the file holds it (the times are 32-bit there).
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Entry {
    pub id: i32,
    pub valid: bool,
    pub expiration: i32,
    pub last_access: i32,
    pub remote: Vec<u8>,
    pub file_name: Vec<u8>,
    pub file_type: Vec<u8>,
    pub etag: Vec<u8>,
}

/// The reads the index needs (a `FileReader`, or bytes in the tests).
pub trait Src {
    fn read_u8(&mut self) -> Result<u8, i32>;
    fn read_i32(&mut self) -> Result<i32, i32>;
    /// `ReadLength(2)`.
    fn read_len2(&mut self) -> Result<i32, i32>;
}

impl Src for EeWonFileReader {
    fn read_u8(&mut self) -> Result<u8, i32> {
        let (v, s) = EeWonFileReader::read_u8(self);
        if s == WON_FILE_OK {
            Ok(v)
        } else {
            Err(s)
        }
    }
    fn read_i32(&mut self) -> Result<i32, i32> {
        let (v, s) = EeWonFileReader::read_i32(self);
        if s == WON_FILE_OK {
            Ok(v)
        } else {
            Err(s)
        }
    }
    fn read_len2(&mut self) -> Result<i32, i32> {
        let (v, s) = self.read_length(2);
        if s == WON_FILE_OK {
            Ok(v)
        } else {
            Err(s)
        }
    }
}

/// `ReadString(s, 2)`: the length (unsigned, as `FileReader` reads a 2-byte field), then that many bytes one by one.
fn read_string(r: &mut dyn Src) -> Result<Vec<u8>, i32> {
    let n = r.read_len2()?;
    let mut s = Vec::new();
    for _ in 0..n.max(0) {
        s.push(r.read_u8()?);
    }
    Ok(s)
}

/// `HTTPCacheEntry::ReadFromFile`.
pub fn read_entry(r: &mut dyn Src) -> Result<Entry, i32> {
    Ok(Entry {
        id: r.read_i32()?,
        valid: r.read_u8()? != 0,
        expiration: r.read_i32()?,
        last_access: r.read_i32()?,
        remote: read_string(r)?,
        file_name: read_string(r)?,
        file_type: read_string(r)?,
        etag: read_string(r)?,
    })
}

/// `ReadCacheMap` after its open: nothing for another version; each entry read whole is handed to `put` as it
/// comes (the original inserted it before reading the next), and a failed read stops.
pub fn read_map(r: &mut dyn Src, version: i32, put: &mut dyn FnMut(Entry)) {
    let step = (|| -> Result<(), i32> {
        if r.read_i32()? != version {
            return Ok(());
        }
        let count = r.read_i32()?;
        for _ in 0..count {
            put(read_entry(r)?);
        }
        Ok(())
    })();
    let _ = step;
}

/// The writes the index needs.
pub trait Dst {
    fn write(&mut self, data: &[u8]) -> Result<(), i32>;
    fn pos(&self) -> c_ulong;
    fn set_pos(&mut self, pos: c_ulong) -> Result<(), i32>;
}

impl Dst for EeWonFileWriter {
    fn write(&mut self, data: &[u8]) -> Result<(), i32> {
        match EeWonFileWriter::write(self, data) {
            WON_FILE_OK => Ok(()),
            s => Err(s),
        }
    }
    fn pos(&self) -> c_ulong {
        EeWonFileWriter::pos(self)
    }
    fn set_pos(&mut self, pos: c_ulong) -> Result<(), i32> {
        match EeWonFileWriter::set_pos(self, pos) {
            WON_FILE_OK => Ok(()),
            s => Err(s),
        }
    }
}

/// An entry to write: its fields and its times as the cache holds them (`time_t`).
#[derive(Debug, Clone, Copy)]
pub struct Out<'a> {
    pub id: i32,
    pub valid: bool,
    pub expiration: i64,
    pub last_access: i64,
    pub strings: [&'a [u8]; 4],
}

/// `WriteString(s, 2)`.
fn write_string(w: &mut dyn Dst, s: &[u8]) -> Result<(), i32> {
    w.write(&(s.len() as u16).to_ne_bytes())?;
    w.write(s)
}

/// `HTTPCacheEntry::WriteToFile` (the times truncated to 32 bits by `WriteLong`).
pub fn write_entry(w: &mut dyn Dst, e: &Out) -> Result<(), i32> {
    w.write(&e.id.to_ne_bytes())?;
    w.write(&[u8::from(e.valid)])?;
    w.write(&(e.expiration as i32).to_ne_bytes())?;
    w.write(&(e.last_access as i32).to_ne_bytes())?;
    for s in e.strings {
        write_string(w, s)?;
    }
    Ok(())
}

/// `WriteCacheMap` after its open: the version, the count's place, each kept entry (an entry not kept has its file
/// deleted through `delete`, in the map's order), then the count of those written; a failed write stops.
pub fn write_map(w: &mut dyn Dst, version: i32, now: i64, entries: &[Out], delete: &mut dyn FnMut(usize)) {
    let step = (|| -> Result<(), i32> {
        w.write(&version.to_ne_bytes())?;
        let position = w.pos();
        w.write(&(entries.len() as i32).to_ne_bytes())?;
        let mut count = 0i32;
        for (i, e) in entries.iter().enumerate() {
            if cache_keeps(e.valid, now, e.last_access) {
                write_entry(w, e)?;
                count += 1;
            } else {
                delete(i);
            }
        }
        w.set_pos(position)?;
        w.write(&count.to_ne_bytes())
    })();
    let _ = step;
}

// ---- FFI

/// An entry across the C ABI: the fields, then the four strings (remote location, file name, type, entity tag).
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EeWonCacheEntry {
    pub id: i32,
    pub valid: bool,
    pub expiration: i64,
    pub last_access: i64,
    pub strings: [*const u8; 4],
    pub lengths: [usize; 4],
}

unsafe fn sl<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `ReadCacheMap`'s file: opened (nothing when it cannot be), read with [`read_map`], each entry handed to `put`
/// (valid during the call), closed.
///
/// # Safety
/// `path` is a C string; `put` is callable with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cachemap_read(
    path: *const c_char,
    version: i32,
    put: unsafe extern "C" fn(*mut c_void, *const EeWonCacheEntry),
    context: *mut c_void,
) {
    let mut r = EeWonFileReader::new();
    if !unsafe { r.open(path) } {
        return;
    }
    read_map(&mut r, version, &mut |e: Entry| {
        let parts = [&e.remote, &e.file_name, &e.file_type, &e.etag];
        let raw = EeWonCacheEntry {
            id: e.id,
            valid: e.valid,
            expiration: i64::from(e.expiration),
            last_access: i64::from(e.last_access),
            strings: parts.map(|s| s.as_ptr()),
            lengths: parts.map(|s| s.len()),
        };
        unsafe { put(context, &raw) };
    });
    r.close();
}

/// `WriteCacheMap`'s file: opened for writing (nothing when it cannot be), written with [`write_map`] (the entries
/// in the id map's order, `delete` called with an index for each one not kept), closed.
///
/// # Safety
/// `path` is a C string; `entries` holds `count` entries whose strings hold their lengths; `delete` is callable
/// with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cachemap_write(
    path: *const c_char,
    version: i32,
    now: i64,
    entries: *const EeWonCacheEntry,
    count: usize,
    delete: unsafe extern "C" fn(*mut c_void, usize),
    context: *mut c_void,
) {
    let mut w = EeWonFileWriter::new();
    if !unsafe { w.open(path) } {
        return;
    }
    let raw =
        if entries.is_null() || count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(entries, count) } };
    let outs: Vec<Out> = raw
        .iter()
        .map(|e| Out {
            id: e.id,
            valid: e.valid,
            expiration: e.expiration,
            last_access: e.last_access,
            strings: [0, 1, 2, 3].map(|k| unsafe { sl(e.strings[k], e.lengths[k]) }),
        })
        .collect();
    write_map(&mut w, version, now, &outs, &mut |i| unsafe { delete(context, i) });
    w.close();
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Bytes<'a>(&'a [u8], usize);
    impl Src for Bytes<'_> {
        fn read_u8(&mut self) -> Result<u8, i32> {
            let b = *self.0.get(self.1).ok_or(3)?;
            self.1 += 1;
            Ok(b)
        }
        fn read_i32(&mut self) -> Result<i32, i32> {
            let s = self.0.get(self.1..self.1 + 4).ok_or(3)?;
            self.1 += 4;
            Ok(i32::from_ne_bytes(s.try_into().unwrap()))
        }
        fn read_len2(&mut self) -> Result<i32, i32> {
            let s = self.0.get(self.1..self.1 + 2).ok_or(3)?;
            self.1 += 2;
            Ok(i32::from(u16::from_ne_bytes(s.try_into().unwrap())))
        }
    }

    #[derive(Default)]
    struct Buf {
        data: Vec<u8>,
        at: usize,
        fail_after: Option<usize>,
    }
    impl Dst for Buf {
        fn write(&mut self, d: &[u8]) -> Result<(), i32> {
            if self.fail_after.is_some_and(|n| self.at + d.len() > n) {
                return Err(6);
            }
            for &b in d {
                if self.at < self.data.len() {
                    self.data[self.at] = b;
                } else {
                    self.data.push(b);
                }
                self.at += 1;
            }
            Ok(())
        }
        fn pos(&self) -> c_ulong {
            self.at as c_ulong
        }
        fn set_pos(&mut self, pos: c_ulong) -> Result<(), i32> {
            self.at = pos as usize;
            Ok(())
        }
    }

    fn out<'a>(id: i32, valid: bool, last: i64, s: &'a [u8]) -> Out<'a> {
        Out { id, valid, expiration: 0x1_0000_0005, last_access: last, strings: [s, b"f", b"", b"tag"] }
    }

    #[test]
    fn a_round_trip_keeps_the_recent_valid_entries() {
        let now = 2_000_000;
        let es = [
            out(1, true, now - 10, b"a"),
            out(2, false, now, b"b"),
            out(3, true, now - 1_209_600, b"c"),
            out(4, true, now, b"d"),
        ];
        let mut w = Buf::default();
        let mut deleted = Vec::new();
        write_map(&mut w, 7, now, &es, &mut |i| deleted.push(i));
        assert_eq!(deleted, vec![1, 2]);
        let mut got = Vec::new();
        read_map(&mut Bytes(&w.data, 0), 7, &mut |e| got.push(e));
        assert_eq!(got.iter().map(|e| e.id).collect::<Vec<_>>(), vec![1, 4]);
        assert_eq!(got[0].expiration, 5);
        assert_eq!(got[0].remote, b"a");
        assert_eq!(got[1].etag, b"tag");
        // another version: nothing
        let mut n = 0;
        read_map(&mut Bytes(&w.data, 0), 8, &mut |_| n += 1);
        assert_eq!(n, 0);
        // cut short: the entries read whole before the cut
        let mut got = Vec::new();
        read_map(&mut Bytes(&w.data[..w.data.len() - 1], 0), 7, &mut |e| got.push(e.id));
        assert_eq!(got, vec![1]);
    }

    #[test]
    fn a_failed_write_stops_with_the_count_unwritten() {
        let now = 100;
        let es = [out(1, true, now, b"a"), out(2, false, now, b"b"), out(3, true, now, b"c")];
        let mut w = Buf { fail_after: Some(8 + 26 + 5), ..Default::default() };
        let mut deleted = Vec::new();
        write_map(&mut w, 7, now, &es, &mut |i| deleted.push(i));
        // the first entry written, the second's file deleted, the third failed: the count stays 3
        assert_eq!(deleted, vec![1]);
        assert_eq!(&w.data[4..8], &3i32.to_ne_bytes());
    }

    #[test]
    fn a_length_past_the_end_fails() {
        let mut d = Vec::new();
        d.extend_from_slice(&5i32.to_ne_bytes());
        d.push(1);
        d.extend_from_slice(&[0; 8]);
        d.extend_from_slice(&(-1i16).to_ne_bytes());
        d.extend_from_slice(&[0; 6]);
        assert_eq!(read_entry(&mut Bytes(&d, 0)), Err(3));
        let mut d2 = d.clone();
        d2[13..15].copy_from_slice(&0u16.to_ne_bytes());
        let e = read_entry(&mut Bytes(&d2, 0)).unwrap();
        assert_eq!((e.id, e.valid, e.remote.len()), (5, true, 0));
    }
}
