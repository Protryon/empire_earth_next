//! `UIFontTable` whole in Rust (R6 ui round 18): the UI database's font records (each a face name, style, quality
//! and size) and the shared `GEFont` list every control's font comes from, with every member of `UIFontTable.cpp`:
//! `Create` (the database file), `CreateClosestFont`, `GetFont`, `FindOrCreateFont` and `DeleteAll`.
//!
//! The C++ class is static forwarders over [`ee_ui_font_tables`] and the host of its calls out (`UIFontTableHost`:
//! the file, the string table's face names as C++ `UString`s held by address, the fonts' `CompareFont`, `Reference`,
//! construction and `Dereference`). The record reading and the search are round 5's (`font.rs`); this module is
//! their host. The lists are a `Mutex` never held across a call out (fonts are asked for from the UI thread and, by
//! the game's text, other threads, unguarded in the original).

use core::ffi::{c_long, c_ulong};
use std::sync::{Mutex, MutexGuard};

use crate::font::{self, DbFont, FontHost};
use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

/// `kErrorOK`.
const ERROR_OK: c_long = 0;

/// The record's size on disk (`UIDbDataFonts`).
const RECORD_SIZE: usize = core::mem::size_of::<DbFont>();

/// One font record (`UIFontTable`'s members).
#[derive(Clone, Copy, Debug)]
struct Entry {
    /// `mName`: a C++ `UString` (the string table's face), held by address.
    name: isize,
    style: c_ulong,
    quality: c_ulong,
    size: c_ulong,
}

struct Lists {
    /// `sFontTable`: one slot per record, `None` for a record without a font.
    tables: Vec<Option<Entry>>,
    /// `sFont`: every font handed out (referenced).
    fonts: Vec<isize>,
}

static LISTS: Mutex<Lists> = Mutex::new(Lists { tables: Vec::new(), fonts: Vec::new() });

fn lists() -> MutexGuard<'static, Lists> {
    LISTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The statics ([`ee_ui_font_tables`]).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiFontsCall {
    /// `Create(file a, manager b)` -> the last read's result.
    Create = 1,
    /// `CreateClosestFont(a, pixel height b)` -> the font or NULL.
    CreateClosestFont = 2,
    /// `DeleteAll()`.
    DeleteAll = 3,
    /// `GetFont(a)` -> the font (NULL for an empty or missing record).
    GetFont = 4,
    /// `FindOrCreateFont(name a, style b, quality c, size d)` -> the font.
    FindOrCreateFont = 5,
    /// The number of records (the drivers').
    Count = 6,
    /// Whether record `a` has a font (the drivers').
    Present = 7,
}

/// `FindOrCreateFont`'s view of the shared list: the query and the host.
struct Search {
    h: Host,
    name: isize,
    style: c_ulong,
    quality: c_ulong,
    size: c_ulong,
}

impl FontHost for Search {
    fn count(&self) -> usize {
        lists().fonts.len()
    }
    fn matches(&mut self, i: usize) -> bool {
        let f = lists().fonts[i];
        #[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64 targets
        let qs = (self.quality as u32 as u64 | (self.size as u32 as u64) << 32) as isize;
        self.h.op(Op::FontCompare, f, self.name, self.style as isize, qs) != 0
    }
    fn take(&mut self, i: usize) -> usize {
        let f = lists().fonts[i];
        self.h.op(Op::FontReference, f, 0, 0, 0);
        i
    }
    fn create(&mut self) -> usize {
        let f = self.h.op(Op::FontNew, self.name, self.style as isize, self.quality as isize, self.size as isize);
        let mut l = lists();
        l.fonts.push(f);
        l.fonts.len() - 1
    }
}

fn find_or_create(h: Host, name: isize, style: c_ulong, quality: c_ulong, size: c_ulong) -> isize {
    let i = font::find_or_create(&mut Search { h, name, style, quality, size });
    lists().fonts[i]
}

/// `Create`: every record of the file; a record with a face name is an entry -> the last read's result.
fn create(h: Host, file: isize, manager: isize) -> c_long {
    h.op(Op::TblFileOpen, file, 0, 0, 0);
    let packed = h.op(Op::TblFileReadCount, file, 0, 0, 0) as u64;
    let mut result = packed as u32 as i32 as c_long;
    if result == ERROR_OK {
        let count = (packed >> 32) as usize;
        // a shorter table drops the slots past the count (their names were leaked by the original too)
        lists().tables.resize_with(count, || None);
        let mut record = [0u8; RECORD_SIZE];
        for i in 0..count {
            result = h.op(Op::TblFileRead, file, record.as_mut_ptr() as isize, RECORD_SIZE as isize, 0) as c_long;
            // SAFETY: DbFont is plain data of RECORD_SIZE bytes
            let r: DbFont = unsafe { core::ptr::read_unaligned(record.as_ptr() as *const DbFont) };
            let e = font::entry(&r);
            let entry = e.present.then(|| Entry {
                name: h.op(Op::FontName, manager, e.string_id as isize, 0, 0),
                style: e.style,
                quality: e.quality,
                size: e.size,
            });
            if let Some(slot) = lists().tables.get_mut(i) {
                *slot = entry;
            }
        }
    }
    h.op(Op::TblFileClose, file, 0, 0, 0);
    result
}

fn entry(index: isize) -> Option<Entry> {
    lists().tables.get(index as usize).copied().flatten()
}

/// `DeleteAll`: the records' names deleted, each shared font released once, both lists emptied.
fn delete_all(h: Host) {
    let tables = core::mem::take(&mut lists().tables);
    for e in tables.into_iter().flatten() {
        h.op(Op::FontNameDelete, e.name, 0, 0, 0);
    }
    let fonts = core::mem::take(&mut lists().fonts);
    for f in fonts {
        h.op(Op::FontRelease, f, 0, 0, 0);
    }
}

/// `UIFontTable`'s statics ([`EeUiFontsCall`]).
///
/// # Safety
/// The host answers the ops; the arguments are the call's.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_font_tables(
    call: EeUiFontsCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    host: EeUiHostFn,
) -> isize {
    let h = Host { obj: core::ptr::null_mut(), f: host };
    match call {
        EeUiFontsCall::Create => create(h, a, b) as isize,
        EeUiFontsCall::CreateClosestFont => match entry(a) {
            Some(e) => find_or_create(h, e.name, e.style, e.quality, font::closest_size(b as c_ulong) as c_ulong),
            None => 0,
        },
        EeUiFontsCall::DeleteAll => {
            delete_all(h);
            0
        }
        // the original read through an empty record's NULL
        EeUiFontsCall::GetFont => match entry(a) {
            Some(e) => find_or_create(h, e.name, e.style, e.quality, e.size),
            None => 0,
        },
        EeUiFontsCall::FindOrCreateFont => find_or_create(h, a, b as c_ulong, c as c_ulong, d as c_ulong),
        EeUiFontsCall::Count => lists().tables.len() as isize,
        EeUiFontsCall::Present => entry(a).is_some() as isize,
    }
}

#[cfg(test)]
#[path = "fonttableobj_tests.rs"]
mod tests;
