//! Where a record base sits in its C++ class, computed by Rust (the data records pass, docs/progress/headers.md
//! "Record bases"). A class whose bases before a record are only a vtable pointer, records and classes made of those
//! has a *head*: a `#[repr(C)]` struct of the same things in the same order ([`record_head!`]), and the record's
//! offset in the class is the field's `offset_of!` in the head. Rust code reads a record in a class object at that
//! constant instead of an offset the C++ registers at start-up.
//!
//! The C++ checks every head once at start-up: it hands each record base's offset as its compiler placed it
//! (`EERecordBaseOffset<Class, Record>()`, `ee/EERecordBase.h`) to [`check`] through its crate's FFI, which refuses a
//! difference (a C++ base or member before the record that the head lacks, or an ABI that places a base otherwise),
//! and a head the C++ did not check.

use std::ffi::{c_char, CStr};

/// A record base's place in its class: the C++ class and record names and the record's offset in the class.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RecordBase {
    pub class: &'static str,
    pub record: &'static str,
    pub offset: usize,
}

/// A record base as the C++ compiler placed it (`EE_RECORD_BASE(Class, Record)` in `ee/EERecordBase.h`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeRecordBase {
    /// the class's and the record's names (NUL-terminated)
    pub class: *const c_char,
    pub record: *const c_char,
    pub offset: u64,
}

/// The vtable pointer at the start of a polymorphic class with no polymorphic base, in a head.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct VTablePointer(pub *const core::ffi::c_void);

/// `UReferenceCounter` in a head (`include/lle/UReferenceCounter.h`): its vtable pointer and `mReferenceCount`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct UReferenceCounterHead {
    pub vptr: VTablePointer,
    pub reference_count: core::ffi::c_ulong,
}

/// Declares a class's head: the `#[repr(C)]` struct of what precedes its record bases (`head { vptr: VTablePointer }`,
/// a base class's head, ...), then the record bases in the class's order, with `BASES`, every record's [`RecordBase`].
///
/// ```ignore
/// ee_base::record_head! {
///     /// `EEPlayer`'s record bases.
///     pub struct PlayerHead for "EEPlayer" {
///         head { vptr: VTablePointer }
///         records { rts: EeUnitsRtsPlayerFields, saved: EeUnitsPlayerSaved, fields: EeUnitsPlayerFields }
///     }
/// }
/// const SAVED: usize = std::mem::offset_of!(PlayerHead, saved);
/// ```
#[macro_export]
macro_rules! record_head {
    ($(#[$meta:meta])* $vis:vis struct $head:ident for $class:literal {
        $(head { $($pre:ident : $pre_ty:ty),* $(,)? })?
        records { $($field:ident : $ty:ident),+ $(,)? }
    }) => {
        $(#[$meta])*
        #[repr(C)]
        #[allow(dead_code)]
        $vis struct $head {
            $($(pub $pre: $pre_ty,)*)?
            $(pub $field: $ty,)+
        }

        impl $head {
            /// The record bases' places in the class, as this head computes them.
            pub const BASES: &'static [$crate::record_head::RecordBase] = &[$(
                $crate::record_head::RecordBase {
                    class: $class,
                    record: stringify!($ty),
                    offset: ::core::mem::offset_of!($head, $field),
                },
            )+];
        }
    };
}

/// Compares the C++ compiler's record bases with the heads' (`rust`, every head's `BASES`): an error naming each
/// difference, each base the heads lack and each head base the C++ did not check.
pub fn check(cpp: &[(String, String, u64)], rust: &[&[RecordBase]]) -> Result<(), String> {
    let mut errors = Vec::new();
    let all: Vec<&RecordBase> = rust.iter().flat_map(|h| h.iter()).collect();
    for (class, record, offset) in cpp {
        match all.iter().find(|b| b.class == class && b.record == record) {
            Some(b) if b.offset as u64 == *offset => {}
            Some(b) => {
                errors.push(format!("{class}'s {record} is at {offset:#x} in C++, at {:#x} in its head", b.offset))
            }
            None => errors.push(format!("{class}'s {record}: no Rust head declares it")),
        }
    }
    for b in &all {
        if !cpp.iter().any(|(c, r, _)| c == b.class && r == b.record) {
            errors.push(format!("{}'s {}: the C++ did not check its head", b.class, b.record));
        }
    }
    if errors.is_empty() {
        Ok(())
    } else {
        Err(errors.join("; "))
    }
}

/// The C++ record bases of an FFI array.
///
/// # Safety
/// `bases` points to `count` records whose names are NUL-terminated strings (or is null with `count` 0).
pub unsafe fn from_ffi(bases: *const EeRecordBase, count: usize) -> Vec<(String, String, u64)> {
    if bases.is_null() || count == 0 {
        return Vec::new();
    }
    // SAFETY: the caller's contract
    let bases = unsafe { std::slice::from_raw_parts(bases, count) };
    let name = |p: *const c_char| {
        if p.is_null() {
            String::new()
        } else {
            // SAFETY: the caller's contract (NUL-terminated)
            unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
        }
    };
    bases.iter().map(|b| (name(b.class), name(b.record), b.offset)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct RecA {
        pub a: u8,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct RecB {
        pub b: u64,
    }

    crate::record_head! {
        /// a test head
        pub struct TestHead for "Test" {
            head { counter: UReferenceCounterHead }
            records { a: RecA, b: RecB }
        }
    }

    #[test]
    fn a_head_places_its_records_as_a_c_struct() {
        assert_eq!(TestHead::BASES[0], RecordBase { class: "Test", record: "RecA", offset: 16 });
        assert_eq!(TestHead::BASES[1], RecordBase { class: "Test", record: "RecB", offset: 24 });
    }

    #[test]
    fn the_check_names_every_difference() {
        let ok = vec![("Test".to_string(), "RecA".to_string(), 16), ("Test".to_string(), "RecB".to_string(), 24)];
        assert_eq!(check(&ok, &[TestHead::BASES]), Ok(()));
        let moved = vec![("Test".to_string(), "RecA".to_string(), 16), ("Test".to_string(), "RecB".to_string(), 32)];
        assert!(check(&moved, &[TestHead::BASES]).unwrap_err().contains("RecB is at 0x20 in C++, at 0x18"));
        let unchecked = vec![("Test".to_string(), "RecA".to_string(), 16)];
        assert!(check(&unchecked, &[TestHead::BASES]).unwrap_err().contains("did not check"));
        let extra = vec![("Other".to_string(), "RecA".to_string(), 0)];
        assert!(check(&extra, &[]).unwrap_err().contains("no Rust head"));
    }

    #[test]
    fn ffi_names_are_read() {
        let bases = [EeRecordBase { class: c"Test".as_ptr(), record: c"RecA".as_ptr(), offset: 16 }];
        // SAFETY: one record with NUL-terminated names
        let read = unsafe { from_ffi(bases.as_ptr(), 1) };
        assert_eq!(read, vec![("Test".to_string(), "RecA".to_string(), 16)]);
    }
}
