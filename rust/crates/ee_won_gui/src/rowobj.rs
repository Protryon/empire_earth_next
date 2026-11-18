//! A list row whose state and virtuals are another crate's Rust object (R6 WONLobby round 16).
//!
//! WONGUI's lists hold their rows as C++ `ListItem`s and call their virtuals (Paint, CalcDimensions, Compare,
//! CanSelect, MouseDown). The lobby's own rows (the room tree's, the friends and user lists', the game room's
//! players') were C++ subclasses with a few members each. They are now two generic C++ shells, `RustRow : ListItem`
//! and `RustTextRow : TextListItem` (WONLobbyGUI.h), each holding one `EeWonRow`: a callback table the owning crate
//! registers with the row (`EeWonRowTable`, one per row kind; ee_won_lobby's rows.rs) and that crate's state. The
//! shells' virtuals are `ee_won_row_call`, their destructor `ee_won_row_free`.
//!
//! The list's other users reach a row's state from its `ListItem*` through `of`: the row is bound to its shell when
//! the owner made it (`bind`) and unbound when it is freed. Rows live on the window thread only; the registry is
//! that thread's.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

/// What a row's shell calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRowCall {
    /// `Paint(*(Graphics*)a, list)`.
    Paint = 0,
    /// `CalcDimensions(list)`.
    CalcDimensions = 1,
    /// `Compare(other, list)`: `a` the other row's `EeWonRow*` (the original's `static_cast` of the other item),
    /// `b` the other item.
    Compare = 2,
    /// `CanSelect(list, a, b)` (0 or 1).
    CanSelect = 3,
    /// `MouseDown(list, a, b, c)`.
    MouseDown = 4,
}

/// One row kind's calls (the owning crate's).
#[repr(C)]
pub struct EeWonRowTable {
    /// a virtual of the row: (the owner's state, the call, the shell, the list, a, b, c)
    pub call: unsafe fn(*mut c_void, EeWonRowCall, *mut c_void, *mut c_void, isize, isize, isize) -> isize,
    /// the end of the row (its shell's destructor): the state's members released, the state freed
    pub free: unsafe fn(*mut c_void),
}

/// A Rust row: its kind's table and its state. Made by the owner (`new`), freed by the shell's destructor.
pub struct EeWonRow {
    table: &'static EeWonRowTable,
    state: *mut c_void,
    item: *mut c_void,
}

thread_local! {
    static ROWS: RefCell<BTreeMap<usize, *mut EeWonRow>> = const { RefCell::new(BTreeMap::new()) };
}

/// A row of a kind over its state (not yet bound to a shell).
pub fn new(table: &'static EeWonRowTable, state: *mut c_void) -> *mut EeWonRow {
    Box::into_raw(Box::new(EeWonRow { table, state, item: std::ptr::null_mut() }))
}

/// Binds the row to the shell the C++ made for it.
///
/// # Safety
/// `row` came from `new` and is live.
pub unsafe fn bind(row: *mut EeWonRow, item: *mut c_void) {
    // SAFETY: the contract
    unsafe { (*row).item = item };
    ROWS.with(|r| r.borrow_mut().insert(item as usize, row));
}

/// The state of the row whose shell is `item`, when it is a Rust row of this table.
pub fn of(item: *mut c_void, table: &'static EeWonRowTable) -> Option<*mut c_void> {
    let row = ROWS.with(|r| r.borrow().get(&(item as usize)).copied())?;
    // SAFETY: a bound row is live until its shell's destructor unbinds it
    let row = unsafe { &*row };
    std::ptr::eq(row.table, table).then_some(row.state)
}

/// The state of a row (`ee_won_row_call`'s argument), when it is of this table.
///
/// # Safety
/// `row` is NULL or a live row.
pub unsafe fn state(row: *const EeWonRow, table: &'static EeWonRowTable) -> Option<*mut c_void> {
    if row.is_null() {
        return None;
    }
    // SAFETY: the contract
    let row = unsafe { &*row };
    std::ptr::eq(row.table, table).then_some(row.state)
}

/// The row whose shell is `item` (NULL when it is none): a driver's way to its rows' calls.
#[no_mangle]
pub extern "C" fn ee_won_row_of(item: *mut c_void) -> *mut EeWonRow {
    ROWS.with(|r| r.borrow().get(&(item as usize)).copied()).unwrap_or(std::ptr::null_mut())
}

/// One virtual of a row's shell.
///
/// # Safety
/// `row` is live (its shell's); the call's arguments as `EeWonRowCall` documents.
#[no_mangle]
pub unsafe extern "C" fn ee_won_row_call(
    row: *mut EeWonRow,
    call: EeWonRowCall,
    item: *mut c_void,
    list: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: the contract; the table and the state are copied out before the call, which may re-enter
    let (table, state) = unsafe { ((*row).table, (*row).state) };
    // SAFETY: the table's own contract over its state
    unsafe { (table.call)(state, call, item, list, a, b, c) }
}

/// The end of a row (its shell's destructor): unbound, its state freed by its table, the row freed.
///
/// # Safety
/// `row` is NULL or live, and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_row_free(row: *mut EeWonRow) {
    if row.is_null() {
        return;
    }
    // SAFETY: the contract
    let boxed = unsafe { Box::from_raw(row) };
    ROWS.with(|r| {
        let mut r = r.borrow_mut();
        if r.get(&(boxed.item as usize)).is_some_and(|p| std::ptr::eq(*p, row)) {
            r.remove(&(boxed.item as usize));
        }
    });
    // SAFETY: the table's own contract over its state
    unsafe { (boxed.table.free)(boxed.state) };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;

    thread_local! {
        static LOG: RefCell<Vec<(u32, isize)>> = const { RefCell::new(Vec::new()) };
        static FREED: Cell<usize> = const { Cell::new(0) };
    }

    unsafe fn call(
        s: *mut c_void,
        c: EeWonRowCall,
        _i: *mut c_void,
        _l: *mut c_void,
        a: isize,
        _: isize,
        _: isize,
    ) -> isize {
        LOG.with(|l| l.borrow_mut().push((c as u32, a)));
        s as isize + a
    }
    unsafe fn free(s: *mut c_void) {
        FREED.with(|f| f.set(s as usize));
    }
    static TABLE: EeWonRowTable = EeWonRowTable { call, free };
    static OTHER: EeWonRowTable = EeWonRowTable { call, free };

    #[test]
    fn a_row_dispatches_to_its_table_and_frees_once() {
        let row = new(&TABLE, 0x40 as *mut c_void);
        // SAFETY: a new row
        unsafe { bind(row, 0x1234 as *mut c_void) };
        assert_eq!(ee_won_row_of(0x1234 as *mut c_void), row);
        assert_eq!(of(0x1234 as *mut c_void, &TABLE), Some(0x40 as *mut c_void));
        assert_eq!(of(0x1234 as *mut c_void, &OTHER), None);
        assert_eq!(of(0x9999 as *mut c_void, &TABLE), None);
        // SAFETY: the row is live
        assert_eq!(unsafe { state(row, &TABLE) }, Some(0x40 as *mut c_void));
        // SAFETY: the row is live
        let r = unsafe {
            ee_won_row_call(row, EeWonRowCall::Compare, 0x1234 as *mut c_void, std::ptr::null_mut(), 2, 0, 0)
        };
        assert_eq!(r, 0x42);
        assert_eq!(LOG.with(|l| l.borrow().clone()), vec![(2, 2)]);
        // SAFETY: the row is live and not used again
        unsafe { ee_won_row_free(row) };
        assert_eq!(FREED.with(|f| f.get()), 0x40);
        assert!(ee_won_row_of(0x1234 as *mut c_void).is_null());
        // SAFETY: NULL is allowed
        unsafe { ee_won_row_free(std::ptr::null_mut()) };
    }
}
