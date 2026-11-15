//! `UIFileListBox` and `UIFileComboBox` whole in Rust (R6 ui round 21): a string list box and a combo box listing the
//! files of an `FSPath` whose extension is one of the valid ones. The two classes' own members (the valid extensions,
//! the path) are this object; the C++ classes keep one pointer to it and every method of theirs forwards here:
//! `AddValidExtension`, `CreateEntries` (the directory search), `DeleteSelectedFile`, `SetSearchPaths`, `GetFilePath`
//! and the destructor's release. The list box lists names without their extension, the combo box full names.
//!
//! Every call out is an [`EeUiOp`] (650..664) through the C++ object's host (`UIFileListBox.cpp`'s
//! `UIFileListHost<T>`): the list's own methods (`DeleteAllEntries`, `AppendLine`, the sorted `InsertLine`,
//! `GetCurrentSelection`, `GetData`, `DeleteLine`), a directory search (a C++ pair of `FSFileSpec`s by address), the
//! path's lookup and the file's delete, the path's references.

use core::cell::{Cell, RefCell};
use core::ffi::{c_long, c_ulong, c_void};

use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

const ERROR_OK: c_long = 0;

/// The inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiFileListCall {
    /// `AddValidExtension(a)`: `a` NUL-terminated bytes.
    AddExtension = 1,
    /// `CreateEntries(a)`: 0 appended, otherwise sorted (case-folded).
    CreateEntries = 2,
    /// `DeleteSelectedFile()` -> the lookup's or the delete's result.
    DeleteSelected = 3,
    /// `SetSearchPaths(a)`: the old path released, `a` referenced (NULL for none).
    SetSearchPaths = 4,
    /// `GetFilePath()`.
    FilePath = 5,
    /// The destructor's release of the path.
    Release = 6,
}

/// The Rust file list (both classes'; `full_names` the combo box's).
pub struct EeUiFileList {
    host: Host,
    full_names: bool,
    extensions: RefCell<Vec<Vec<u8>>>,
    path: Cell<isize>,
}

/// Bytes with their NUL, for an op that takes NUL-terminated bytes.
fn cstr(b: &[u8]) -> Vec<u8> {
    let mut v = b.to_vec();
    v.push(0);
    v
}

impl EeUiFileList {
    pub fn new(host: Host, full_names: bool) -> Self {
        EeUiFileList { host, full_names, extensions: RefCell::new(Vec::new()), path: Cell::new(0) }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        self.host.op(op, a, b, c, 0)
    }

    /// The bytes a host op copies out: its length first (`len_op`), then the bytes into a buffer that long.
    fn bytes(&self, len_op: Op, copy_op: Op, a: isize) -> Vec<u8> {
        let n = self.op(len_op, a, 0, 0).max(0) as usize;
        let mut buf = vec![0u8; n + 1];
        self.op(copy_op, a, buf.as_mut_ptr() as isize, (n + 1) as isize);
        buf.truncate(n);
        buf
    }

    /// `CreateEntries`: every directory of the path searched, each file whose last four bytes are a valid extension
    /// listed once (the list box without the extension).
    pub fn create_entries(&self, sort: c_ulong) {
        // the original's locals (the file and the directory) first
        let search = self.op(Op::FlSearchNew, 0, 0, 0);
        self.op(Op::FlDeleteAll, 0, 0, 0);
        let path = self.path.get();
        let mut i: c_ulong = 0;
        while i < self.op(Op::FlDirCount, path, 0, 0) as c_ulong {
            let mut result = self.op(Op::FlDirFirst, search, path, i as isize) as c_long;
            while result == ERROR_OK {
                let name = self.bytes(Op::FlNameLength, Op::FlNameCopy, search);
                // a name shorter than its extension matches none (the original compared from before the name)
                if name.len() >= 4 {
                    let stem = name.len() - 4;
                    let found = self.extensions.borrow().iter().any(|e| e[..] == name[stem..]);
                    if found {
                        let line = if self.full_names { &name[..] } else { &name[..stem] };
                        let line = cstr(line);
                        let op = if sort == 0 { Op::FlAppend } else { Op::FlInsertSorted };
                        self.op(op, line.as_ptr() as isize, 0, 0);
                    }
                }
                result = self.op(Op::FlNext, search, 0, 0) as c_long;
            }
            self.op(Op::FlRewind, search, 0, 0);
            i += 1;
        }
        self.op(Op::FlSearchDelete, search, 0, 0);
    }

    /// `DeleteSelectedFile`: the selected line's file found on the path and deleted, its line removed. The list box
    /// tries the line plus each valid extension until a file is deleted; the combo box the line itself.
    pub fn delete_selected(&self) -> c_long {
        let mut f = [0.0f32; 4];
        let mut result = self.host.op_f(Op::FlSelection, 0, 0, 0, 0, &mut f) as c_long;
        if result != ERROR_OK {
            return result;
        }
        let selection = f[0].to_bits() as isize;
        let name = self.bytes(Op::FlLineLength, Op::FlLineCopy, selection);
        let names: Vec<Vec<u8>> = if self.full_names {
            vec![name]
        } else {
            self.extensions.borrow().iter().map(|e| [&name[..], &e[..]].concat()).collect()
        };
        for n in names {
            let n = cstr(&n);
            let mut f = [0.0f32; 4];
            result = self.host.op_f(Op::FlDeleteFile, n.as_ptr() as isize, self.path.get(), 0, 0, &mut f) as c_long;
            if f[0].to_bits() == 0 {
                continue;
            }
            if result == ERROR_OK {
                self.op(Op::FlDeleteLine, selection, 0, 0);
                break;
            }
        }
        result
    }

    fn set_path(&self, path: isize) {
        if self.path.get() != 0 {
            self.op(Op::FlPathRelease, self.path.get(), 0, 0);
        }
        self.path.set(path);
        if path != 0 {
            self.op(Op::FlPathReference, path, 0, 0);
        }
    }

    /// One inward call.
    ///
    /// # Safety
    /// For `AddExtension`, `a` is NUL-terminated bytes.
    pub unsafe fn call(&self, call: EeUiFileListCall, a: isize) -> isize {
        type C = EeUiFileListCall;
        match call {
            C::AddExtension => {
                // SAFETY: the contract
                let e = unsafe { core::ffi::CStr::from_ptr(a as *const core::ffi::c_char) }.to_bytes().to_vec();
                self.extensions.borrow_mut().push(e);
            }
            C::CreateEntries => self.create_entries(a as c_ulong),
            C::DeleteSelected => return self.delete_selected() as isize,
            C::SetSearchPaths => self.set_path(a),
            C::FilePath => return self.path.get(),
            C::Release => {
                if self.path.get() != 0 {
                    self.op(Op::FlPathRelease, self.path.get(), 0, 0);
                }
            }
        }
        0
    }
}

// ---- the C ABI

/// A new Rust file list for the C++ list box or combo box `obj` (`full_names`: the combo box's).
#[no_mangle]
pub extern "C" fn ee_ui_file_list_new(obj: *mut c_void, host: EeUiHostFn, full_names: bool) -> *mut EeUiFileList {
    Box::into_raw(Box::new(EeUiFileList::new(Host { obj, f: host }, full_names)))
}

/// The destructor: the path released, the object freed.
///
/// # Safety
/// `list` came from `ee_ui_file_list_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_file_list_free(list: *mut EeUiFileList) {
    if list.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*list).call(EeUiFileListCall::Release, 0) };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(list) });
}

/// One of the file lists' methods ([`EeUiFileListCall`]).
///
/// # Safety
/// `list` is live; `a` as [`EeUiFileList::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_file_list_call(list: *const EeUiFileList, call: EeUiFileListCall, a: isize) -> isize {
    // SAFETY: the contract; a shared reference: the calls out may re-enter
    unsafe { (*list).call(call, a) }
}

#[cfg(test)]
#[path = "filelistobj_tests.rs"]
mod tests;
