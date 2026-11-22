//! The persistent helpers' file layer (game core lane, round 9): an `FSFile`'s typed reads and writes with the
//! overload the C++ called, each one case of EESaveLoad.cpp's `EECore9OpC` ([`super::EeGameCpp`]'s lane C section).
//! A value crosses by its address (a member's, as the C++ read into it, or a Rust local's), so the bytes in the file
//! are the C++'s own: `unsigned long` and `long` as 4 bytes on every data model (FSFile's `Read(long&)` and
//! `Write(const long&)`), `float`, `bool`, `unsigned short`, `signed char`, `unsigned char` as themselves.
//!
//! The bodies over it: [`super::persist_world`] (EEPWorldHelper.cpp), [`super::persist_save`] (EESaveLoad.cpp,
//! EEGameStateLoader.cpp), [`super::persist_parts`] (the players', header's, multiplayer, calamity and checksum chunks).

use std::ffi::{c_long, c_ulong};

use super::{h, O, P};

/// `kErrorOK`.
pub(super) const PF_OK: i64 = 0;
/// `kErrorFailed`.
pub(super) const PF_FAILED: i64 = 9;
/// `kErrorBeyondEnd`.
pub(super) const PF_BEYOND_END: i64 = 0xa;

/// An `FSFile*` (any of its kinds: the memory file of a save, a file spec).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Pf(pub P);

fn at<T>(v: *const T) -> i64 {
    v as usize as i64
}

impl Pf {
    /// `Read(unsigned long&)`
    pub fn read_ulong(self, v: *mut c_ulong) -> i64 {
        h(O::PfReadULong, self.0, &[at(v)])
    }
    /// `Read(long&)`
    pub fn read_long(self, v: *mut c_long) -> i64 {
        h(O::PfReadLong, self.0, &[at(v)])
    }
    /// `Read(unsigned short&)`
    pub fn read_u16(self, v: *mut u16) -> i64 {
        h(O::PfReadU16, self.0, &[at(v)])
    }
    /// `Read(signed char&)`
    pub fn read_i8(self, v: *mut i8) -> i64 {
        h(O::PfReadI8, self.0, &[at(v)])
    }
    /// `Read(unsigned char&)`
    pub fn read_u8(self, v: *mut u8) -> i64 {
        h(O::PfReadU8, self.0, &[at(v)])
    }
    /// `Read(float&)`
    pub fn read_f32(self, v: *mut f32) -> i64 {
        h(O::PfReadF32, self.0, &[at(v)])
    }
    /// `Read(bool&)`
    pub fn read_bool(self, v: *mut bool) -> i64 {
        h(O::PfReadBool, self.0, &[at(v)])
    }
    /// `Read(char*, length, read)`: the result and the count read
    pub fn read_chars(self, out: *mut u8, len: c_ulong) -> (i64, c_ulong) {
        let mut read: c_ulong = 0;
        let r = h(O::PfReadChars, self.0, &[at(out), len as i64, at(&mut read as *mut c_ulong)]);
        (r, read)
    }
    /// `Write(const unsigned long&)`
    pub fn write_ulong(self, v: c_ulong) -> i64 {
        h(O::PfWriteULong, self.0, &[at(&v)])
    }
    /// `Write(const long&)`
    pub fn write_long(self, v: c_long) -> i64 {
        h(O::PfWriteLong, self.0, &[at(&v)])
    }
    /// `Write(const unsigned short&)`
    pub fn write_u16(self, v: u16) -> i64 {
        h(O::PfWriteU16, self.0, &[at(&v)])
    }
    /// `Write(const signed char&)`
    pub fn write_i8(self, v: i8) -> i64 {
        h(O::PfWriteI8, self.0, &[at(&v)])
    }
    /// `Write(const unsigned char&)`
    pub fn write_u8(self, v: u8) -> i64 {
        h(O::PfWriteU8, self.0, &[at(&v)])
    }
    /// `Write(const float&)`
    pub fn write_f32(self, v: f32) -> i64 {
        h(O::PfWriteF32, self.0, &[at(&v)])
    }
    /// `Write(const bool&)`
    pub fn write_bool(self, v: bool) -> i64 {
        h(O::PfWriteBool, self.0, &[at(&v)])
    }
    /// `Write(const char*, length, written)`
    pub fn write_chars(self, data: &[u8]) -> i64 {
        let mut written: c_ulong = 0;
        h(O::PfWriteChars, self.0, &[at(data.as_ptr()), data.len() as i64, at(&mut written as *mut c_ulong)])
    }
}

/// A C++ `std::vector<long>` the Rust made (`std::vector<long>` ops of lane C), deleted when it goes.
pub struct Longs(pub P);

impl Longs {
    pub fn new() -> Longs {
        Longs(h(O::PvLongsNew, 0, &[]) as P)
    }
    pub fn to_vec(&self) -> Vec<c_long> {
        let n = h(O::PvLongsCount, self.0, &[]) as usize;
        (0..n).map(|i| h(O::PvLongsAt, self.0, &[i as i64]) as c_long).collect()
    }
}

impl Default for Longs {
    fn default() -> Self {
        Longs::new()
    }
}

impl Drop for Longs {
    fn drop(&mut self) {
        h(O::PvLongsDelete, self.0, &[]);
    }
}

/// A Rust callback the loaded game runs at its first tick (`EEAfterLoad`): `run(data)`, which owns `data`.
pub fn after_load(run: extern "C" fn(usize), data: usize) {
    h(O::PvAfterLoad, 0, &[run as usize as i64, data as i64]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_error_codes_are_the_engines() {
        assert_eq!((PF_OK, PF_FAILED, PF_BEYOND_END), (0, 9, 10));
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn a_read_is_a_host_call() {
        let mut v: c_ulong = 0;
        Pf(1).read_ulong(&mut v);
    }
}
