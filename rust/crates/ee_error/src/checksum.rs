//! The checksum sets (`ESChecksum`, `ESChecksumEntry` before; LLE lane round 2): named 32-bit checksums of the world,
//! saved with a game (`EEPChecksum`) and compared between network peers and against a loaded game. A verbose set
//! keeps one entry per described checksum; a summed set adds every value into one anonymous entry. The total is the
//! 32-bit wrapping sum of the entries (simulation-visible: saved and compared).
//!
//! C++ keeps `ESChecksum` as the type callers use (its vtable, `EEGameState`'s member, `EEPChecksum`'s base) holding
//! a pointer to this object; copies clone it. `golden_checksum.txt` is the original classes' trace
//! (`tests/error/es_checksum_golden.cpp`), replayed by `golden_checksum.rs`.

use std::ffi::{c_char, c_ulong, CStr};

/// One entry: its description (the original's `UString`, bytes up to a NUL) and its checksum.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Entry {
    pub description: Vec<u8>,
    pub checksum: u32,
}

/// A checksum set (`ESChecksum`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EeErrorChecksum {
    pub verbose: bool,
    pub entries: Vec<Entry>,
}

impl EeErrorChecksum {
    pub fn new(verbose: bool) -> Self {
        EeErrorChecksum { verbose, entries: Vec::new() }
    }

    /// `AddChecksum(description, value)`: a new entry in a verbose set, else the value added to the sum. The value
    /// is the original's `unsigned long` stored in 32 bits.
    pub fn add(&mut self, description: &[u8], value: u32) {
        if self.verbose {
            self.entries.push(Entry { description: description.to_vec(), checksum: value });
        } else {
            self.add_value(value);
        }
    }

    /// The protected `AddChecksum(value)`: added to the last entry (an anonymous one created first if none).
    pub fn add_value(&mut self, value: u32) {
        if self.entries.is_empty() {
            self.entries.push(Entry::default());
        }
        let last = self.entries.last_mut().expect("an entry");
        last.checksum = last.checksum.wrapping_add(value);
    }

    /// `GetTotalChecksum`: the entries' 32-bit sum.
    pub fn total(&self) -> u32 {
        self.entries.iter().fold(0u32, |t, e| t.wrapping_add(e.checksum))
    }

    /// `Compare`: whether `other` matches this set entry by entry, and one error text per mismatch (the original's
    /// texts, with their final '\n').
    pub fn compare(&self, other: &EeErrorChecksum) -> (bool, Vec<Vec<u8>>) {
        let mut errors = Vec::new();
        if self.verbose != other.verbose {
            errors.push(
                b"ESChecksum::Compare(): One checksum is in verbose mode, the other isn't.  Compare failed.\n".to_vec(),
            );
        } else if self.entries.len() != other.entries.len() {
            errors.push(
                format!(
                    "ESChecksum::Compare(): Checksum sizes don't match!  Source size is {} and incoming size is {}.\n",
                    self.entries.len(),
                    other.entries.len()
                )
                .into_bytes(),
            );
        } else {
            for (mine, theirs) in self.entries.iter().zip(&other.entries) {
                if theirs.description != mine.description {
                    let mut t = b"ESChecksumEntry::Compare failed, tags don't match: ".to_vec();
                    t.extend_from_slice(&theirs.description);
                    t.extend_from_slice(b" != ");
                    t.extend_from_slice(&mine.description);
                    t.push(b'\n');
                    errors.push(t);
                } else if theirs.checksum != mine.checksum {
                    let mut t = format!(
                        "ESChecksumEntry::Compare failed, checksums don't match: {} != {} (",
                        theirs.checksum, mine.checksum
                    )
                    .into_bytes();
                    t.extend_from_slice(&theirs.description);
                    t.extend_from_slice(b")\n");
                    errors.push(t);
                }
            }
        }
        (errors.is_empty(), errors)
    }
}

/// Error texts from `ee_error_checksum_compare`, read with `ee_error_texts_count` / `_get`, freed with `_free`.
pub struct EeErrorTexts {
    texts: Vec<std::ffi::CString>,
}

unsafe fn set<'a>(c: *const EeErrorChecksum) -> &'a EeErrorChecksum {
    &*c
}

/// A new checksum set (`ESChecksum(verbose)`).
#[no_mangle]
pub extern "C" fn ee_error_checksum_new(verbose: bool) -> *mut EeErrorChecksum {
    Box::into_raw(Box::new(EeErrorChecksum::new(verbose)))
}

/// A copy of a set (`ESChecksum`'s copy constructor).
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_clone(c: *const EeErrorChecksum) -> *mut EeErrorChecksum {
    Box::into_raw(Box::new(set(c).clone()))
}

/// `to = from` (`ESChecksum`'s assignment).
///
/// # Safety
/// Both are live sets.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_assign(to: *mut EeErrorChecksum, from: *const EeErrorChecksum) {
    if !std::ptr::eq(to, from) {
        *to = set(from).clone();
    }
}

/// Frees a set (NULL: nothing).
///
/// # Safety
/// `c` is NULL or a set from `ee_error_checksum_new` / `_clone`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_free(c: *mut EeErrorChecksum) {
    if !c.is_null() {
        drop(Box::from_raw(c));
    }
}

/// `AddChecksum(description, value)`.
///
/// # Safety
/// `c` is a live set; `description` a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_add(c: *mut EeErrorChecksum, description: *const c_char, value: c_ulong) {
    #[allow(clippy::unnecessary_cast)] // `unsigned long` is 64-bit on LP64 systems: the entry keeps 32 bits
    (*c).add(CStr::from_ptr(description).to_bytes(), value as u32);
}

/// The protected `AddChecksum(value)`.
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_add_value(c: *mut EeErrorChecksum, value: c_ulong) {
    #[allow(clippy::unnecessary_cast)] // as above
    (*c).add_value(value as u32);
}

/// `GetTotalChecksum`.
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_total(c: *const EeErrorChecksum) -> u32 {
    set(c).total()
}

/// `GetIsVerboseMode`.
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_verbose(c: *const EeErrorChecksum) -> bool {
    set(c).verbose
}

/// The verbose flag a loaded game sets (`EEPChecksum::Load`).
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_set_verbose(c: *mut EeErrorChecksum, verbose: bool) {
    (*c).verbose = verbose;
}

/// The number of entries (`EEPChecksum::Save`).
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_count(c: *const EeErrorChecksum) -> usize {
    set(c).entries.len()
}

/// Entry `i`'s description into `out` (NUL-terminated, cut to `capacity - 1` bytes) -> its full length; its checksum
/// into `*out_value`. An index past the end: length 0, value 0.
///
/// # Safety
/// `c` is a live set; `out` holds `capacity` bytes (or is NULL with 0); `out_value` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_entry(
    c: *const EeErrorChecksum,
    i: usize,
    out: *mut c_char,
    capacity: usize,
    out_value: *mut u32,
) -> usize {
    let Some(e) = set(c).entries.get(i) else {
        *out_value = 0;
        if capacity > 0 {
            *out = 0;
        }
        return 0;
    };
    *out_value = e.checksum;
    if capacity > 0 {
        let n = e.description.len().min(capacity - 1);
        std::ptr::copy_nonoverlapping(e.description.as_ptr(), out as *mut u8, n);
        *out.add(n) = 0;
    }
    e.description.len()
}

/// Empties a set (`EEPChecksum::Load` before reading).
///
/// # Safety
/// `c` is a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_clear(c: *mut EeErrorChecksum) {
    (*c).entries.clear();
}

/// Appends a described entry whatever the mode (`EEPChecksum::Load`'s entries).
///
/// # Safety
/// `c` is a live set; `description` a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_push(c: *mut EeErrorChecksum, description: *const c_char, value: u32) {
    (*c).entries.push(Entry { description: CStr::from_ptr(description).to_bytes().to_vec(), checksum: value });
}

/// `Compare(other)`: whether they match; the error texts into `*out_texts` (free with `ee_error_texts_free`).
///
/// # Safety
/// Both are live sets; `out_texts` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_error_checksum_compare(
    c: *const EeErrorChecksum,
    other: *const EeErrorChecksum,
    out_texts: *mut *mut EeErrorTexts,
) -> bool {
    let (same, errors) = set(c).compare(set(other));
    let texts = errors
        .into_iter()
        .map(|mut t| {
            t.retain(|&b| b != 0);
            std::ffi::CString::new(t).unwrap_or_default()
        })
        .collect();
    *out_texts = Box::into_raw(Box::new(EeErrorTexts { texts }));
    same
}

/// The number of texts.
///
/// # Safety
/// `t` is from `ee_error_checksum_compare`.
#[no_mangle]
pub unsafe extern "C" fn ee_error_texts_count(t: *const EeErrorTexts) -> usize {
    (*t).texts.len()
}

/// Text `i` (valid until `ee_error_texts_free`).
///
/// # Safety
/// `t` is from `ee_error_checksum_compare`; `i` is below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_error_texts_get(t: *const EeErrorTexts, i: usize) -> *const c_char {
    (&(*t).texts)[i].as_ptr()
}

/// Frees the texts.
///
/// # Safety
/// `t` is from `ee_error_checksum_compare` or NULL, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_error_texts_free(t: *mut EeErrorTexts) {
    if !t.is_null() {
        drop(Box::from_raw(t));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn verbose_and_summed_sets() {
        let mut v = EeErrorChecksum::new(true);
        v.add(b"EEMap", 0xffff_fff0);
        v.add(b"EEPlayer", 0x20);
        assert_eq!(v.entries.len(), 2);
        assert_eq!(v.total(), 0x10);
        let mut s = EeErrorChecksum::new(false);
        s.add(b"EEMap", 5);
        s.add(b"EEPlayer", 7);
        assert_eq!(s.entries, vec![Entry { description: vec![], checksum: 12 }]);
        let (same, errors) = v.compare(&s);
        assert!(!same && errors.len() == 1);
        let mut w = v.clone();
        assert_eq!(v.compare(&w), (true, vec![]));
        w.entries[1].checksum = 1;
        w.entries[0].description = b"X".to_vec();
        let (_, errors) = v.compare(&w);
        assert_eq!(errors[0], b"ESChecksumEntry::Compare failed, tags don't match: X != EEMap\n");
        assert_eq!(errors[1], b"ESChecksumEntry::Compare failed, checksums don't match: 1 != 32 (EEPlayer)\n");
    }

    #[test]
    fn c_abi_entries_and_texts() {
        unsafe {
            let c = ee_error_checksum_new(true);
            ee_error_checksum_add(c, c"EEMap".as_ptr(), 3);
            let d = ee_error_checksum_clone(c);
            ee_error_checksum_add_value(d, 1);
            let mut buf = [0 as c_char; 4];
            let mut value = 0u32;
            assert_eq!(ee_error_checksum_entry(d, 0, buf.as_mut_ptr(), 4, &mut value), 5);
            assert_eq!(CStr::from_ptr(buf.as_ptr()).to_bytes(), b"EEM");
            assert_eq!(value, 4);
            let mut texts = std::ptr::null_mut();
            assert!(!ee_error_checksum_compare(c, d, &mut texts));
            assert_eq!(ee_error_texts_count(texts), 1);
            ee_error_texts_free(texts);
            ee_error_checksum_assign(c, d);
            assert_eq!(ee_error_checksum_total(c), 4);
            ee_error_checksum_free(c);
            ee_error_checksum_free(d);
        }
    }
}
