//! The game's file combo box as a kind over the combo box shell (UI cluster round 5; once `EEUIFileComboBox`, the
//! scenario editor's map, sound and elevation file lists): "none" first, then each file of its path that matches
//! its pattern, without the four characters of its extension; a file name selected by its line, else "none". The
//! path (a referenced `FSPath`), its listing and the list box's lines are the host's.

use core::ffi::{c_char, CStr};

use crate::screens::shells::*;

/// `EEUIFileComboBox` (a combo box; args: the `FSPath*` (referenced by the maker), the pattern (a C string, copied)).
pub const EE_SCREENS_SK_FILE_COMBO: u32 = 0x82;

/// `CreateEntries()`.
pub const EE_SCREENS_SKOP_FILE_ENTRIES: u32 = 0x82;
/// `SelectFile(*(const UWideString*)a)` -> `kErrorOK` (0) when a line is the name, else `kErrorNotFound` (3).
pub const EE_SCREENS_SKOP_FILE_SELECT: u32 = 0x83;

/// The files of the path `a` matching the pattern `b` (a C string), each appended without its last four
/// characters (`FSPath::ListFiles` into a `std::set<UString>`: their order).
pub const EE_SCREENS_SH_FILE_APPEND_LISTED: u32 = 0xc0;
/// -> the first line of the combo box's list equal to `*(const UWideString*)a`, or -1.
pub const EE_SCREENS_SH_FILE_FIND_LINE: u32 = 0xc1;
/// The path `a` dereferenced.
pub const EE_SCREENS_SH_FILE_PATH_RELEASE: u32 = 0xc2;

/// `IDS_SE_MAP_NONE` (ee/EEStringIDs.h).
const IDS_SE_MAP_NONE: i64 = 20_009;
/// `kErrorOK`, `kErrorNotFound` (lle/LowLevelEngineCore.h).
const ERROR_OK: i64 = 0;
const ERROR_NOT_FOUND: i64 = 3;

/// `EEUIFileComboBox`: its path and pattern.
struct FileCombo {
    path: i64,
    /// the pattern, NUL-terminated
    pattern: Vec<u8>,
}

impl FileCombo {
    /// `CreateEntries`: emptied, "none", the listed files, the first line selected.
    fn entries(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_DELETE_ALL, 0, 0);
        c.call(EE_SCREENS_SH_APPEND_STRING, IDS_SE_MAP_NONE, 0);
        c.call(EE_SCREENS_SH_FILE_APPEND_LISTED, self.path, self.pattern.as_ptr() as i64);
        c.call(EE_SCREENS_SH_SET_SELECTION, 0, 0);
    }
}

impl Kind for FileCombo {
    fn name(&self) -> &'static CStr {
        c"EEUIFileComboBox"
    }
    fn init(&self, c: &Ctx) {
        self.entries(c);
    }
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_FILE_PATH_RELEASE, self.path, 0);
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_FILE_ENTRIES => {
                self.entries(c);
                0
            }
            EE_SCREENS_SKOP_FILE_SELECT => {
                let line = c.call(EE_SCREENS_SH_FILE_FIND_LINE, a, 0);
                c.call(EE_SCREENS_SH_SET_SELECTION, line.max(0), 0);
                if line >= 0 {
                    ERROR_OK
                } else {
                    ERROR_NOT_FOUND
                }
            }
            _ => 0,
        }
    }
}

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    match kind {
        EE_SCREENS_SK_FILE_COMBO => {
            let at = args.get(1).copied().unwrap_or(0) as *const c_char;
            let pattern = if at.is_null() {
                vec![0]
            } else {
                // SAFETY: the maker's C string, live for the constructor
                unsafe { CStr::from_ptr(at) }.to_bytes_with_nul().to_vec()
            };
            Some(Box::new(FileCombo { path: args.first().copied().unwrap_or(0), pattern }))
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::RefCell;

    struct Rec {
        calls: RefCell<Vec<(u32, i64, i64)>>,
        line: i64,
    }

    impl ShellHost for Rec {
        fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, _f: &mut [f32; 4]) -> i64 {
            self.calls.borrow_mut().push((op, a, b));
            if op == EE_SCREENS_SH_FILE_FIND_LINE {
                self.line
            } else {
                0
            }
        }
    }

    fn kind() -> Box<dyn Kind> {
        let pattern = c"*.dat";
        make(EE_SCREENS_SK_FILE_COMBO, &[0x1234, pattern.as_ptr() as i64]).unwrap()
    }

    #[test]
    fn the_constructor_lists_none_then_the_files_and_selects_the_first() {
        let h = Rec { calls: RefCell::new(Vec::new()), line: -1 };
        let k = kind();
        assert_eq!(k.name().to_str().unwrap(), "EEUIFileComboBox");
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        k.init(&c);
        let calls = h.calls.borrow().clone();
        assert_eq!(calls.len(), 4);
        assert_eq!(calls[0], (EE_SCREENS_SH_DELETE_ALL, 0, 0));
        assert_eq!(calls[1], (EE_SCREENS_SH_APPEND_STRING, IDS_SE_MAP_NONE, 0));
        assert_eq!((calls[2].0, calls[2].1), (EE_SCREENS_SH_FILE_APPEND_LISTED, 0x1234));
        // SAFETY: the kind's own NUL-terminated pattern
        assert_eq!(unsafe { CStr::from_ptr(calls[2].2 as *const c_char) }, c"*.dat");
        assert_eq!(calls[3], (EE_SCREENS_SH_SET_SELECTION, 0, 0));
        k.free(&c);
        assert_eq!(*h.calls.borrow().last().unwrap(), (EE_SCREENS_SH_FILE_PATH_RELEASE, 0x1234, 0));
    }

    #[test]
    fn a_listed_name_is_selected_and_a_missing_one_selects_none() {
        let k = kind();
        let found = Rec { calls: RefCell::new(Vec::new()), line: 3 };
        let c = Ctx { ctl: core::ptr::null_mut(), host: &found };
        assert_eq!(k.op(&c, EE_SCREENS_SKOP_FILE_SELECT, 77, 0, &mut [0.0; 4]), ERROR_OK);
        assert_eq!(
            *found.calls.borrow(),
            vec![(EE_SCREENS_SH_FILE_FIND_LINE, 77, 0), (EE_SCREENS_SH_SET_SELECTION, 3, 0)]
        );
        let missing = Rec { calls: RefCell::new(Vec::new()), line: -1 };
        let c = Ctx { ctl: core::ptr::null_mut(), host: &missing };
        assert_eq!(k.op(&c, EE_SCREENS_SKOP_FILE_SELECT, 77, 0, &mut [0.0; 4]), ERROR_NOT_FOUND);
        assert_eq!(missing.calls.borrow()[1], (EE_SCREENS_SH_SET_SELECTION, 0, 0));
    }
}
