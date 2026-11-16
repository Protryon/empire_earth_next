//! WONGUI's GUIStringData's buffer as Rust (R6 WONGUI B round 1): every lobby text's storage. The original's
//! GUIStringData.cpp, GUIStringDataMutations.cpp and the header's inline `Reserve` / `resize` / `append`: the
//! constructors' fills, the reservation (the terminator's unit, doubled when growing, the old text and terminator
//! kept), the resize and its terminator, the wide and narrow assignments, `Fill`, `assign`, both `append`s, `erase`,
//! `insert`, `setAt` and the destructor's free. The comparisons, searches and case maps are `guistring`'s.
//!
//! The C++ class keeps its three members (`mText`, `mLength`, `mCapacity`: 100+ readers read them inline) as one
//! block the Rust edits in place, `EeWonGuiStrBuf`; the buffer is Rust's allocation (its real size kept in front of
//! it, because `mCapacity` is the original's `unsigned short` and wraps past 65,535 units).
//!
//! Corners kept: the length and capacity are `unsigned short` (a resize past 65,535 wraps the length and writes the
//! terminator at the wrapped index); `Reserve` compares the capacity as an int; the offsets of `Fill`, `assign`,
//! `append` and the substring constructor are `unsigned short`; `erase` and `insert` take full ints and their range
//! rules; the other string of `assign`, `append` and `insert` is read where the original read it (after this
//! string's resize: an assign or append of itself sees its own terminator and new buffer, as the original did). Not
//! kept: the original's undefined behaviour (a copy or terminator past the allocation, reached by a wrapped length or
//! capacity, an insert into itself or an erase of a negative length, is cut to the allocation), and its
//! uninitialised units (a new allocation is zeroed).

use std::alloc::{alloc_zeroed, dealloc, Layout};
use std::ffi::c_void;

/// The C++ `GUIStringData`'s members as one block (`mText`, `mLength`, `mCapacity`, in that order).
#[repr(C)]
#[derive(Debug)]
pub struct EeWonGuiStrBuf {
    /// `wchar_t* mText`: units of `wchar_size` bytes, NULL before the first reservation.
    pub text: *mut c_void,
    pub length: u16,
    pub capacity: u16,
}

/// The edit `ee_won_gstr` runs (each the original's method; `a`, `b`, `c` its arguments, `other` its other string
/// or its units).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGstrOp {
    /// `GUIStringData(unsigned short a)`: room for `a` units, empty.
    NewCapacity,
    /// The copy constructor: `other`'s text and terminator.
    NewCopy,
    /// `GUIStringData(other, a, b)`: `b` units of `other` from `a`.
    NewSub,
    /// `Reserve(a, b != 0)`.
    Reserve,
    /// `resize(a, b != 0)`.
    Resize,
    /// `AssignWStr(other, a)`: `a` units at `other` (wchar_t*).
    AssignUnits,
    /// `AssignStr(other, a)`: `a` bytes of UTF-8 at `other` (char*).
    AssignNarrow,
    /// `Fill(a, b, c)`: units `b..b+c` set to `a`, the length `b + c`.
    Fill,
    /// `assign(other, a, b)`.
    Assign,
    /// `append(a)`: one unit.
    AppendUnit,
    /// `append(other, a, b)`.
    Append,
    /// `erase(a, b)`.
    Erase,
    /// `insert(a, other)`.
    Insert,
    /// `setAt(a, b)`: answers `b`.
    SetAt,
    /// The destructor: the buffer freed.
    Free,
}

// ---- the allocation: the units after a header holding their byte size

const HEAD: usize = 16;

fn layout(bytes: usize) -> Layout {
    Layout::from_size_align(HEAD + bytes, HEAD).expect("GUIStringData layout")
}

/// A zeroed buffer of `units` units of `ws` bytes.
fn allocate(units: usize, ws: usize) -> *mut c_void {
    let bytes = units.max(1) * ws;
    // SAFETY: the layout is never zero-sized
    let p = unsafe { alloc_zeroed(layout(bytes)) };
    assert!(!p.is_null(), "GUIStringData: out of memory");
    // SAFETY: HEAD bytes in front, aligned for a usize
    unsafe {
        (p as *mut usize).write(bytes);
        p.add(HEAD).cast()
    }
}

/// The allocation's size in units (0 for NULL).
fn room(text: *const c_void, ws: usize) -> usize {
    if text.is_null() {
        return 0;
    }
    // SAFETY: from `allocate`
    unsafe { ((text as *const u8).sub(HEAD) as *const usize).read() / ws }
}

fn release(text: *mut c_void) {
    if text.is_null() {
        return;
    }
    // SAFETY: from `allocate`; its size in front
    unsafe {
        let p = (text as *mut u8).sub(HEAD);
        let bytes = (p as *const usize).read();
        dealloc(p, layout(bytes));
    }
}

/// Unit `i` of a buffer (0 outside the allocation).
fn get(text: *const c_void, ws: usize, i: usize) -> u32 {
    if i >= room(text, ws) {
        return 0;
    }
    // SAFETY: inside the allocation
    unsafe {
        if ws == 2 {
            u32::from(*(text as *const u16).add(i))
        } else {
            *(text as *const u32).add(i)
        }
    }
}

/// Unit `i` of a buffer set (nothing outside the allocation).
fn set(text: *mut c_void, ws: usize, i: usize, v: u32) {
    if i >= room(text, ws) {
        return;
    }
    // SAFETY: inside the allocation
    unsafe {
        if ws == 2 {
            *(text as *mut u16).add(i) = v as u16;
        } else {
            *(text as *mut u32).add(i) = v;
        }
    }
}

fn read(text: *const c_void, ws: usize, from: usize, n: usize) -> Vec<u32> {
    (from..from + n).map(|i| get(text, ws, i)).collect()
}

fn write(text: *mut c_void, ws: usize, at: usize, v: &[u32]) {
    for (k, &x) in v.iter().enumerate() {
        set(text, ws, at + k, x);
    }
}

impl EeWonGuiStrBuf {
    /// `Init()`.
    pub fn new() -> Self {
        EeWonGuiStrBuf { text: std::ptr::null_mut(), length: 0, capacity: 0 }
    }

    /// The text's units (to the length).
    pub fn units(&self, ws: usize) -> Vec<u32> {
        read(self.text, ws, 0, usize::from(self.length))
    }

    /// The unit at `i` (the terminator at the length), 0 outside the allocation.
    pub fn unit(&self, i: usize, ws: usize) -> u32 {
        get(self.text, ws, i)
    }

    /// `Reserve(length, grow)`: room for `length` units and the terminator (doubled when growing); the old text and
    /// terminator copied.
    pub fn reserve(&mut self, length: i32, grow: bool, ws: usize) {
        let mut n = length.wrapping_add(1);
        if i32::from(self.capacity) < n {
            if grow {
                n = n.wrapping_add(n);
            }
            let old = self.text;
            let fresh = allocate(n.max(0) as usize, ws);
            if !old.is_null() {
                let v = read(old, ws, 0, usize::from(self.length) + 1);
                write(fresh, ws, 0, &v);
                release(old);
            }
            self.text = fresh;
            self.capacity = n as u16;
        }
    }

    /// `resize(length, grow)`: the reservation, the length (an `unsigned short`) and its terminator.
    pub fn resize(&mut self, length: i32, grow: bool, ws: usize) {
        self.reserve(length, grow, ws);
        self.length = length as u16;
        set(self.text, ws, usize::from(self.length), 0);
    }

    /// `AssignWStr(units, length)`.
    pub fn assign_units(&mut self, v: &[u32], ws: usize) {
        let length = v.len() as i32;
        self.resize(length, false, ws);
        self.length = length as u16;
        write(self.text, ws, 0, v);
    }

    /// `AssignStr` (UTF-8, D6): emptied, then the decoded units (`guistring::widen`), none for no bytes.
    pub fn assign_narrow(&mut self, bytes: &[u8], ws: usize) {
        self.resize(0, false, ws);
        if !bytes.is_empty() {
            let v = crate::guistring::widen(bytes, ws as u32);
            self.resize(v.len() as i32, false, ws);
            write(self.text, ws, 0, &v);
        }
    }

    /// `Fill(ch, offset, length)`.
    pub fn fill(&mut self, ch: i32, offset: u16, length: u16, ws: usize) {
        let end = i32::from(offset) + i32::from(length);
        self.resize(end, false, ws);
        for i in i32::from(offset)..end {
            set(self.text, ws, i as usize, ch as u32);
        }
    }

    /// `append(ch)`.
    pub fn append_unit(&mut self, ch: i32, ws: usize) {
        self.resize(i32::from(self.length) + 1, true, ws);
        set(self.text, ws, usize::from(self.length.wrapping_sub(1)), ch as u32);
    }

    /// `erase(start, length)`: 0 or past the end erases the rest; a start past the length or negative does nothing.
    pub fn erase(&mut self, start: i32, length: i32, ws: usize) {
        let len = i32::from(self.length);
        if start > len || start < 0 {
            return;
        }
        let mut end = start.wrapping_add(length);
        if length == 0 || end > len {
            end = len;
        }
        if end != len {
            let tail = read(self.text, ws, end as usize, (len - end) as usize);
            write(self.text, ws, start as usize, &tail);
        }
        self.resize(len - (end - start), false, ws);
    }

    /// `setAt(pos, ch)`: the unit set when `pos` is below the length (as an `unsigned short`); answers `ch`.
    pub fn set_at(&mut self, pos: i32, ch: i32, ws: usize) -> i32 {
        if pos < i32::from(self.length) {
            set(self.text, ws, pos as usize, u32::from(ch as u16));
        }
        ch
    }

    /// The destructor.
    pub fn free(&mut self) {
        release(self.text);
        self.text = std::ptr::null_mut();
    }
}

impl Default for EeWonGuiStrBuf {
    fn default() -> Self {
        Self::new()
    }
}

// ---- the edits with another string (which may be this one): raw pointers, the other read where the original read it

/// `assign(other, offset, length)`.
///
/// # Safety
/// `b` and `o` are live blocks (possibly the same).
pub unsafe fn assign(b: *mut EeWonGuiStrBuf, o: *const EeWonGuiStrBuf, offset: u16, length: u16, ws: usize) {
    let n = i32::from(length);
    // SAFETY: the contract; no reference outlives its statement
    unsafe {
        (*b).reserve(n, false, ws);
        (*b).resize(0, false, ws);
        let at = i32::from((*b).length);
        (*b).resize(at + n, true, ws);
        let v = read((*o).text, ws, usize::from(offset), usize::from(length));
        write((*b).text, ws, at as usize, &v);
    }
}

/// `append(other, start, length)`.
///
/// # Safety
/// As `assign`.
pub unsafe fn append(b: *mut EeWonGuiStrBuf, o: *const EeWonGuiStrBuf, start: u16, length: u16, ws: usize) {
    // SAFETY: the contract
    unsafe {
        let at = i32::from((*b).length);
        (*b).resize(at + i32::from(length), true, ws);
        let v = read((*o).text, ws, usize::from(start), usize::from(length));
        write((*b).text, ws, at as usize, &v);
    }
}

/// `insert(start, other)`: nothing when the start is past the length.
///
/// # Safety
/// As `assign`.
pub unsafe fn insert(b: *mut EeWonGuiStrBuf, o: *const EeWonGuiStrBuf, start: i32, ws: usize) {
    // SAFETY: the contract
    unsafe {
        let old = i32::from((*b).length);
        if start > old {
            return;
        }
        (*b).resize(old + i32::from((*o).length), false, ws);
        if start != old {
            let tail = read((*b).text, ws, start as usize, (old - start) as usize);
            write((*b).text, ws, (start + i32::from((*o).length)) as usize, &tail);
        }
        let v = read((*o).text, ws, 0, usize::from((*o).length));
        write((*b).text, ws, start as usize, &v);
    }
}

/// The copy constructor (`NewCopy`) or the substring constructor (`NewSub`, `n` units from `offset`) into a new
/// block.
///
/// # Safety
/// `o` is a live block other than `b`; `b` points to writable room for a block (its old contents unread).
pub unsafe fn construct_from(b: *mut EeWonGuiStrBuf, o: *const EeWonGuiStrBuf, sub: Option<(u16, u16)>, ws: usize) {
    // SAFETY: the contract
    unsafe {
        b.write(EeWonGuiStrBuf::new());
        match sub {
            None => {
                (*b).resize(i32::from((*o).length), false, ws);
                let v = read((*o).text, ws, 0, usize::from((*b).length) + 1);
                write((*b).text, ws, 0, &v);
            }
            Some((offset, n)) => {
                (*b).resize(i32::from(n), false, ws);
                let v = read((*o).text, ws, usize::from(offset), usize::from(n));
                write((*b).text, ws, 0, &v);
            }
        }
    }
}

/// One of GUIStringData's edits on `b` (the C++ object's members); `other` is the other string's members
/// (`NewCopy`, `NewSub`, `Assign`, `Append`, `Insert`), `wchar_t` units (`AssignUnits`) or UTF-8 bytes
/// (`AssignNarrow`); answers `SetAt`'s character, else 0.
///
/// # Safety
/// `b` is a live block whose `text` is NULL or this module's (for the `New*` ops: writable room, unread);
/// `other` as the op needs it, live for the call (it may be `b` itself for `Assign`, `Append` and `Insert`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gstr(
    op: EeWonGstrOp,
    b: *mut EeWonGuiStrBuf,
    other: *const c_void,
    a: i32,
    bb: i32,
    c: i32,
    wchar_size: u32,
) -> i32 {
    let ws = wchar_size as usize;
    let o = other as *const EeWonGuiStrBuf;
    // SAFETY: the contract, op by op
    unsafe {
        match op {
            EeWonGstrOp::NewCapacity => {
                b.write(EeWonGuiStrBuf::new());
                (*b).resize(a as u16 as i32, false, ws);
                (*b).resize(0, false, ws);
            }
            EeWonGstrOp::NewCopy => construct_from(b, o, None, ws),
            EeWonGstrOp::NewSub => construct_from(b, o, Some((a as u16, bb as u16)), ws),
            EeWonGstrOp::Reserve => (*b).reserve(a, bb != 0, ws),
            EeWonGstrOp::Resize => (*b).resize(a, bb != 0, ws),
            EeWonGstrOp::AssignUnits => {
                let v: Vec<u32> =
                    (0..a.max(0) as usize)
                        .map(|i| {
                            if ws == 2 {
                                u32::from(*(other as *const u16).add(i))
                            } else {
                                *(other as *const u32).add(i)
                            }
                        })
                        .collect();
                (*b).assign_units(&v, ws);
            }
            EeWonGstrOp::AssignNarrow => {
                let bytes: &[u8] = if other.is_null() || a <= 0 {
                    &[]
                } else {
                    std::slice::from_raw_parts(other.cast::<u8>(), a as usize)
                };
                (*b).assign_narrow(bytes, ws);
            }
            EeWonGstrOp::Fill => (*b).fill(a, bb as u16, c as u16, ws),
            EeWonGstrOp::Assign => assign(b, o, a as u16, bb as u16, ws),
            EeWonGstrOp::AppendUnit => (*b).append_unit(a, ws),
            EeWonGstrOp::Append => append(b, o, a as u16, bb as u16, ws),
            EeWonGstrOp::Erase => (*b).erase(a, bb, ws),
            EeWonGstrOp::Insert => insert(b, o, a, ws),
            EeWonGstrOp::SetAt => return (*b).set_at(a, bb, ws),
            EeWonGstrOp::Free => (*b).free(),
        }
    }
    0
}

#[cfg(test)]
#[path = "guistrobj_tests.rs"]
mod tests;
