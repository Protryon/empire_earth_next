//! The scenario elements' saves: one I/O callback by operation code (`EE_SCENARIO_IO_*`) over the C++'s `FSFile`
//! (`Scenario Manager/EEScenarioHost.cpp`), so every value goes through the `FSFile` overload the original called
//! (`unsigned long` and `long` through their 4-byte adapters, `float`, `bool`), into or out of the field itself, and
//! the strings and id lists of the C++ object by slot.

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// `FSFile::Read(unsigned long&)` into `*value`.
pub const EE_SCENARIO_IO_READ_ULONG: u32 = 0;
/// `FSFile::Read(long&)` into `*value`.
pub const EE_SCENARIO_IO_READ_LONG: u32 = 1;
/// `FSFile::Read(float&)` into `*value`.
pub const EE_SCENARIO_IO_READ_FLOAT: u32 = 2;
/// `FSFile::Read(bool&)` into `*value`.
pub const EE_SCENARIO_IO_READ_BOOL: u32 = 3;
/// `FSFile::Write(const unsigned long&)` of `*value`.
pub const EE_SCENARIO_IO_WRITE_ULONG: u32 = 4;
/// `FSFile::Write(const long&)` of `*value`.
pub const EE_SCENARIO_IO_WRITE_LONG: u32 = 5;
/// `FSFile::Write(const float&)` of `*value`.
pub const EE_SCENARIO_IO_WRITE_FLOAT: u32 = 6;
/// `FSFile::Write(const bool&)` of `*value`.
pub const EE_SCENARIO_IO_WRITE_BOOL: u32 = 7;
/// `UWideString::Load` of the object's string `slot`.
pub const EE_SCENARIO_IO_LOAD_STRING: u32 = 8;
/// `UWideString::Save` of the object's string `slot`.
pub const EE_SCENARIO_IO_SAVE_STRING: u32 = 9;
/// The object's id list `slot`: `clear()`.
pub const EE_SCENARIO_IO_LIST_CLEAR: u32 = 10;
/// The object's id list `slot`: `push_back(*value)` (an `unsigned long`).
pub const EE_SCENARIO_IO_LIST_PUSH: u32 = 11;
/// The object's id list `slot`: its `size()` into `*value` (an `unsigned long`).
pub const EE_SCENARIO_IO_LIST_COUNT: u32 = 12;
/// The object's id list `slot`: element `index` into `*value` (an `unsigned long`).
pub const EE_SCENARIO_IO_LIST_AT: u32 = 13;
/// The trigger's entries `slot` (0 conditions, 1 effects): their number into `*value` (an `unsigned long`).
pub const EE_SCENARIO_IO_ENTRY_COUNT: u32 = 14;
/// The trigger's entry `index` of `slot` into `*value` (an [`EeScenarioEntry`]).
pub const EE_SCENARIO_IO_ENTRY_AT: u32 = 15;
/// The trigger's entry `index` of `slot` set from `*value` (an [`EeScenarioEntry`]): `SetCondition(index, first,
/// value)` then `SetConditionOperator(index, op)`, or `SetEffect(index, first, value)` then `SetEffectOperator`.
pub const EE_SCENARIO_IO_ENTRY_SET: u32 = 16;

/// The trigger's condition entries (`EESMTConditionWrapper`) and effect entries (`EESMTEffectWrapper`).
pub const EE_SCENARIO_ENTRIES_CONDITIONS: u32 = 0;
pub const EE_SCENARIO_ENTRIES_EFFECTS: u32 = 1;

/// One trigger entry: the NOT flag (0/1) or the delay, the operator, the condition or effect index.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioEntry {
    pub first: c_ulong,
    pub op: c_long,
    pub value: c_ulong,
}

/// A save's file and the object's strings and lists.
pub trait Stream {
    fn read_ulong(&mut self, value: &mut c_ulong) -> i32;
    fn read_long(&mut self, value: &mut c_long) -> i32;
    fn read_float(&mut self, value: &mut f32) -> i32;
    fn read_bool(&mut self, value: &mut bool) -> i32;
    fn write_ulong(&mut self, value: c_ulong) -> i32;
    fn write_long(&mut self, value: c_long) -> i32;
    fn write_float(&mut self, value: f32) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn load_string(&mut self, slot: u32) -> i32;
    fn save_string(&mut self, slot: u32) -> i32;
    fn list_clear(&mut self, slot: u32);
    fn list_push(&mut self, slot: u32, value: c_ulong);
    fn list_count(&mut self, slot: u32) -> c_ulong;
    fn list_at(&mut self, slot: u32, index: c_ulong) -> c_ulong;
    fn entry_count(&mut self, _slot: u32) -> c_ulong {
        0
    }
    fn entry_at(&mut self, _slot: u32, _index: c_ulong) -> EeScenarioEntry {
        EeScenarioEntry::default()
    }
    fn entry_set(&mut self, _slot: u32, _index: c_ulong, _entry: EeScenarioEntry) {}
}

/// The C ABI's stream: a context and the I/O callback (`value` the field or local the operation reads or writes).
#[repr(C)]
pub struct EeScenarioStream {
    pub ctx: *mut c_void,
    pub io: unsafe extern "C" fn(ctx: *mut c_void, op: u32, slot: u32, index: c_ulong, value: *mut c_void) -> i32,
}

impl EeScenarioStream {
    fn call(&mut self, op: u32, slot: u32, index: c_ulong, value: *mut c_void) -> i32 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed the table over, `io` is callable with
        // it, and `value` points to the value of the operation's type for the call
        unsafe { (self.io)(self.ctx, op, slot, index, value) }
    }
}

impl Stream for EeScenarioStream {
    fn read_ulong(&mut self, value: &mut c_ulong) -> i32 {
        self.call(EE_SCENARIO_IO_READ_ULONG, 0, 0, (value as *mut c_ulong).cast())
    }
    fn read_long(&mut self, value: &mut c_long) -> i32 {
        self.call(EE_SCENARIO_IO_READ_LONG, 0, 0, (value as *mut c_long).cast())
    }
    fn read_float(&mut self, value: &mut f32) -> i32 {
        self.call(EE_SCENARIO_IO_READ_FLOAT, 0, 0, (value as *mut f32).cast())
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        self.call(EE_SCENARIO_IO_READ_BOOL, 0, 0, (value as *mut bool).cast())
    }
    fn write_ulong(&mut self, mut value: c_ulong) -> i32 {
        self.call(EE_SCENARIO_IO_WRITE_ULONG, 0, 0, (&mut value as *mut c_ulong).cast())
    }
    fn write_long(&mut self, mut value: c_long) -> i32 {
        self.call(EE_SCENARIO_IO_WRITE_LONG, 0, 0, (&mut value as *mut c_long).cast())
    }
    fn write_float(&mut self, mut value: f32) -> i32 {
        self.call(EE_SCENARIO_IO_WRITE_FLOAT, 0, 0, (&mut value as *mut f32).cast())
    }
    fn write_bool(&mut self, mut value: bool) -> i32 {
        self.call(EE_SCENARIO_IO_WRITE_BOOL, 0, 0, (&mut value as *mut bool).cast())
    }
    fn load_string(&mut self, slot: u32) -> i32 {
        self.call(EE_SCENARIO_IO_LOAD_STRING, slot, 0, std::ptr::null_mut())
    }
    fn save_string(&mut self, slot: u32) -> i32 {
        self.call(EE_SCENARIO_IO_SAVE_STRING, slot, 0, std::ptr::null_mut())
    }
    fn list_clear(&mut self, slot: u32) {
        self.call(EE_SCENARIO_IO_LIST_CLEAR, slot, 0, std::ptr::null_mut());
    }
    fn list_push(&mut self, slot: u32, mut value: c_ulong) {
        self.call(EE_SCENARIO_IO_LIST_PUSH, slot, 0, (&mut value as *mut c_ulong).cast());
    }
    fn list_count(&mut self, slot: u32) -> c_ulong {
        let mut value: c_ulong = 0;
        self.call(EE_SCENARIO_IO_LIST_COUNT, slot, 0, (&mut value as *mut c_ulong).cast());
        value
    }
    fn list_at(&mut self, slot: u32, index: c_ulong) -> c_ulong {
        let mut value: c_ulong = 0;
        self.call(EE_SCENARIO_IO_LIST_AT, slot, index, (&mut value as *mut c_ulong).cast());
        value
    }
    fn entry_count(&mut self, slot: u32) -> c_ulong {
        let mut value: c_ulong = 0;
        self.call(EE_SCENARIO_IO_ENTRY_COUNT, slot, 0, (&mut value as *mut c_ulong).cast());
        value
    }
    fn entry_at(&mut self, slot: u32, index: c_ulong) -> EeScenarioEntry {
        let mut e = EeScenarioEntry::default();
        self.call(EE_SCENARIO_IO_ENTRY_AT, slot, index, (&mut e as *mut EeScenarioEntry).cast());
        e
    }
    fn entry_set(&mut self, slot: u32, index: c_ulong, mut entry: EeScenarioEntry) {
        self.call(EE_SCENARIO_IO_ENTRY_SET, slot, index, (&mut entry as *mut EeScenarioEntry).cast());
    }
}

/// A run of reads or writes that stops at the first error, as the originals' `if (theResult == kErrorOK)` chains:
/// each step runs only while the result so far is 0, and the result is the first error.
pub struct Seq<'a, S: Stream> {
    pub s: &'a mut S,
    pub r: i32,
}

impl<'a, S: Stream> Seq<'a, S> {
    pub fn new(s: &'a mut S) -> Self {
        Seq { s, r: 0 }
    }
    pub fn ok(&self) -> bool {
        self.r == 0
    }
    pub fn ulong(&mut self, v: &mut c_ulong) {
        if self.r == 0 {
            self.r = self.s.read_ulong(v);
        }
    }
    pub fn long(&mut self, v: &mut c_long) {
        if self.r == 0 {
            self.r = self.s.read_long(v);
        }
    }
    pub fn float(&mut self, v: &mut f32) {
        if self.r == 0 {
            self.r = self.s.read_float(v);
        }
    }
    pub fn bool(&mut self, v: &mut bool) {
        if self.r == 0 {
            self.r = self.s.read_bool(v);
        }
    }
    pub fn string(&mut self, slot: u32) {
        if self.r == 0 {
            self.r = self.s.load_string(slot);
        }
    }
    pub fn put_ulong(&mut self, v: c_ulong) {
        if self.r == 0 {
            self.r = self.s.write_ulong(v);
        }
    }
    pub fn put_long(&mut self, v: c_long) {
        if self.r == 0 {
            self.r = self.s.write_long(v);
        }
    }
    pub fn put_float(&mut self, v: f32) {
        if self.r == 0 {
            self.r = self.s.write_float(v);
        }
    }
    pub fn put_bool(&mut self, v: bool) {
        if self.r == 0 {
            self.r = self.s.write_bool(v);
        }
    }
    pub fn put_string(&mut self, slot: u32) {
        if self.r == 0 {
            self.r = self.s.save_string(slot);
        }
    }
    /// A list read as the effect's (the original's): its count into a local, the list cleared, then each element
    /// pushed until a read fails.
    pub fn list_replace(&mut self, slot: u32) {
        if self.r != 0 {
            return;
        }
        let mut count: c_ulong = 0;
        self.r = self.s.read_ulong(&mut count);
        if self.r != 0 {
            return;
        }
        self.s.list_clear(slot);
        let mut i: c_ulong = 0;
        while i < count {
            let mut value: c_ulong = 0;
            self.r = self.s.read_ulong(&mut value);
            if self.r != 0 {
                break;
            }
            self.s.list_push(slot, value);
            i += 1;
        }
    }
    /// A list written as the effect's and the object's: its count, then each element until a write fails.
    pub fn put_list(&mut self, slot: u32) {
        if self.r != 0 {
            return;
        }
        let count = self.s.list_count(slot);
        self.r = self.s.write_ulong(count);
        if self.r != 0 {
            return;
        }
        let mut i: c_ulong = 0;
        while i < count {
            let value = self.s.list_at(slot, i);
            self.r = self.s.write_ulong(value);
            if self.r != 0 {
                break;
            }
            i += 1;
        }
    }
}

/// TEST ONLY: a stream over bytes, the file's fixed widths (4-byte `long`s, 1-byte `bool`s, a string as its 4-byte
/// size with the NUL and its bytes), the strings and lists as slots; a read past the end fails (9) and leaves the
/// value.
#[cfg(test)]
#[derive(Default)]
pub struct Bytes {
    pub data: Vec<u8>,
    pub at: usize,
    pub strings: Vec<Vec<u8>>,
    pub lists: Vec<Vec<c_ulong>>,
    pub entries: [Vec<EeScenarioEntry>; 2],
}

#[cfg(test)]
impl Bytes {
    fn take(&mut self, n: usize) -> Option<&[u8]> {
        if self.at + n > self.data.len() {
            return None;
        }
        self.at += n;
        Some(&self.data[self.at - n..self.at])
    }
    fn u32(&mut self) -> Option<u32> {
        self.take(4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }
}

#[cfg(test)]
impl Stream for Bytes {
    fn read_ulong(&mut self, value: &mut c_ulong) -> i32 {
        match self.u32() {
            Some(v) => {
                *value = v as c_ulong;
                0
            }
            None => 9,
        }
    }
    fn read_long(&mut self, value: &mut c_long) -> i32 {
        match self.u32() {
            Some(v) => {
                *value = v as i32 as c_long;
                0
            }
            None => 9,
        }
    }
    fn read_float(&mut self, value: &mut f32) -> i32 {
        match self.u32() {
            Some(v) => {
                *value = f32::from_bits(v);
                0
            }
            None => 9,
        }
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        match self.take(1) {
            Some(b) => {
                *value = b[0] != 0;
                0
            }
            None => 9,
        }
    }
    fn write_ulong(&mut self, value: c_ulong) -> i32 {
        self.data.extend_from_slice(&(value as u32).to_le_bytes());
        0
    }
    fn write_long(&mut self, value: c_long) -> i32 {
        self.data.extend_from_slice(&(value as i32).to_le_bytes());
        0
    }
    fn write_float(&mut self, value: f32) -> i32 {
        self.data.extend_from_slice(&value.to_bits().to_le_bytes());
        0
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        self.data.push(value as u8);
        0
    }
    fn load_string(&mut self, slot: u32) -> i32 {
        let Some(size) = self.u32() else { return 9 };
        let Some(b) = self.take(size as usize) else { return 9 };
        let text = b.iter().take_while(|&&c| c != 0).copied().collect();
        self.slot(slot);
        self.strings[slot as usize] = text;
        0
    }
    fn save_string(&mut self, slot: u32) -> i32 {
        self.slot(slot);
        let text = self.strings[slot as usize].clone();
        self.data.extend_from_slice(&(text.len() as u32 + 1).to_le_bytes());
        self.data.extend_from_slice(&text);
        self.data.push(0);
        0
    }
    fn list_clear(&mut self, slot: u32) {
        self.list(slot).clear();
    }
    fn list_push(&mut self, slot: u32, value: c_ulong) {
        self.list(slot).push(value);
    }
    fn list_count(&mut self, slot: u32) -> c_ulong {
        self.list(slot).len() as c_ulong
    }
    fn list_at(&mut self, slot: u32, index: c_ulong) -> c_ulong {
        self.list(slot)[index as usize]
    }
    fn entry_count(&mut self, slot: u32) -> c_ulong {
        self.entries[slot as usize].len() as c_ulong
    }
    fn entry_at(&mut self, slot: u32, index: c_ulong) -> EeScenarioEntry {
        self.entries[slot as usize][index as usize]
    }
    fn entry_set(&mut self, slot: u32, index: c_ulong, entry: EeScenarioEntry) {
        let list = &mut self.entries[slot as usize];
        let i = index as usize;
        list[i].first = entry.first;
        list[i].value = entry.value;
        if i == list.len() - 1 && entry.op != 0 {
            list.push(EeScenarioEntry::default());
        }
        list[i].op = entry.op;
    }
}

#[cfg(test)]
impl Bytes {
    fn slot(&mut self, slot: u32) {
        while self.strings.len() <= slot as usize {
            self.strings.push(Vec::new());
        }
    }
    fn list(&mut self, slot: u32) -> &mut Vec<c_ulong> {
        while self.lists.len() <= slot as usize {
            self.lists.push(Vec::new());
        }
        &mut self.lists[slot as usize]
    }
    /// A reader over `data`.
    pub fn over(data: Vec<u8>) -> Self {
        Bytes { data, ..Default::default() }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_seq_stops_at_the_first_error_and_keeps_it() {
        let mut b = Bytes::over(vec![1, 0, 0, 0, 2, 0]);
        let mut s = Seq::new(&mut b);
        let (mut x, mut y, mut z) = (7 as c_ulong, 8 as c_ulong, 9 as c_ulong);
        s.ulong(&mut x);
        s.ulong(&mut y);
        s.ulong(&mut z);
        assert_eq!((x, y, z, s.r), (1, 8, 9, 9));
    }

    #[test]
    fn lists_round_trip_and_a_short_list_stops_with_what_it_read() {
        let mut b = Bytes::default();
        b.lists = vec![vec![], vec![5, 6, 0xffff_ffff]];
        let mut s = Seq::new(&mut b);
        s.put_list(1);
        assert_eq!(s.r, 0);
        let data = b.data.clone();
        assert_eq!(data.len(), 16);
        let mut r = Bytes::over(data[..12].to_vec());
        r.lists = vec![vec![], vec![1, 2, 3, 4]];
        let mut s = Seq::new(&mut r);
        s.list_replace(1);
        assert_eq!(s.r, 9);
        assert_eq!(r.lists[1], vec![5, 6]);
    }
}
