//! WONGUI's cursors as one Rust object (R6 WON GUI round 35).
//!
//! The original had `Cursor` (no native cursor, no component) and three subclasses: the platform's (MSCursor; the
//! port's PlatCursor over [`crate::platcursor`]), `CursorRef` (holds another cursor and answers with it: what the
//! standard cursors' table holds, so setting a standard cursor changes it for everyone who took the table's) and
//! `ComponentCursor` (a component drawn at the mouse, with its hot spot). Here a cursor is its kind; the C++ class is
//! one handle (the reference count, this object). The table of standard cursors is here too.
//!
//! The cursors and components a cursor holds are C++ reference-counted objects: the host adds and drops the
//! references, makes the handle of a new `CursorRef` and asks another cursor's handle (`GetNative`,
//! `GetComponent`). No reference to a cursor lives across a call out.
use crate::platcursor::{
    ee_won_plat_cursor_free, ee_won_plat_cursor_pixels, ee_won_plat_cursor_system, EeWonPlatCursor, EeWonPlatCursorFns,
};
use std::cell::Cell;
use std::sync::Mutex;

/// What a cursor asks of the C++ (`a` a `Cursor*` unless said).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCursorOp {
    /// `((Cursor*)a)->AddRef()`.
    Hold = 1,
    /// `((Cursor*)a)->Release()`.
    Release,
    /// `((Component*)a)->AddRef()`.
    CompHold,
    /// `((Component*)a)->Release()`.
    CompRelease,
    /// `((Cursor*)a)->GetNative()`.
    Native,
    /// `((Cursor*)a)->GetComponent(*(int*)b, *(int*)c)`.
    Component,
    /// `new Cursor((EeWonCursor*)a)`: the handle of a new cursor (no reference yet).
    Wrap,
    /// `((Cursor*)a)->mRust`.
    RustOf,
}

/// The host of [`EeWonCursorOp`].
pub type EeWonCursorHost = Option<unsafe extern "C" fn(op: EeWonCursorOp, a: isize, b: isize, c: isize) -> isize>;

enum Kind {
    /// `Cursor` itself.
    Plain,
    /// The platform's cursor (owned; its address is the platform module's "applied" mark).
    Plat(*mut EeWonPlatCursor),
    /// `CursorRef`: `mCursor`, held (0: none).
    Ref(Cell<isize>),
    /// `ComponentCursor`: `mComponent` (held; 0: none), `mMouseX`, `mMouseY`.
    Component(isize, i32, i32),
}

/// One cursor.
pub struct EeWonCursor {
    host: EeWonCursorHost,
    kind: Kind,
}

impl EeWonCursor {
    fn host(&self, op: EeWonCursorOp, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host's contract (the constructors')
            Some(h) => unsafe { h(op, a, b, c) },
            None => 0,
        }
    }

    /// `GetNative()`: the platform cursor is itself (`me`, its handle); a reference answers with its cursor's;
    /// the others have none.
    pub fn native(&self, me: isize) -> isize {
        match &self.kind {
            Kind::Plat(_) => me,
            Kind::Ref(c) if c.get() != 0 => self.host(EeWonCursorOp::Native, c.get(), 0, 0),
            _ => 0,
        }
    }

    /// `GetComponent(x, y)`: a component cursor's component and hot spot; a reference answers with its cursor's;
    /// the others have none (the outs untouched).
    ///
    /// # Safety
    /// `x` and `y` are writable.
    pub unsafe fn component(&self, x: *mut i32, y: *mut i32) -> isize {
        match &self.kind {
            Kind::Component(comp, mx, my) => {
                // SAFETY: the contract
                unsafe {
                    *x = *mx;
                    *y = *my;
                }
                *comp
            }
            Kind::Ref(c) if c.get() != 0 => self.host(EeWonCursorOp::Component, c.get(), x as isize, y as isize),
            _ => 0,
        }
    }

    /// `CursorRef::SetCursor(cursor)`: the SmartPtr's assignment (nothing for the same cursor; the old one
    /// released, then the new one held).
    pub fn set_inner(&self, cursor: isize) {
        let Kind::Ref(c) = &self.kind else { return };
        let old = c.get();
        if old == cursor {
            return;
        }
        if old != 0 {
            self.host(EeWonCursorOp::Release, old, 0, 0);
        }
        if cursor != 0 {
            self.host(EeWonCursorOp::Hold, cursor, 0, 0);
        }
        c.set(cursor);
    }
}

fn boxed(host: EeWonCursorHost, kind: Kind) -> *mut EeWonCursor {
    Box::into_raw(Box::new(EeWonCursor { host, kind }))
}

/// `Cursor()`: no native cursor, no component.
#[no_mangle]
pub extern "C" fn ee_won_cursor_new() -> *mut EeWonCursor {
    boxed(None, Kind::Plain)
}

/// `PlatCursor(inSystem)`: a system cursor (`PlatSystemCursor`).
#[no_mangle]
pub extern "C" fn ee_won_cursor_new_system(fns: &EeWonPlatCursorFns, cursor: u32) -> *mut EeWonCursor {
    boxed(None, Kind::Plat(ee_won_plat_cursor_system(fns, cursor)))
}

/// `PlatCursor(inPixels, inWidth, inHeight, inHotX, inHotY)`.
///
/// # Safety
/// As `ee_won_plat_cursor_pixels`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_new_pixels(
    fns: &EeWonPlatCursorFns,
    pixels: *const u32,
    width: i32,
    height: i32,
    hot_x: i32,
    hot_y: i32,
) -> *mut EeWonCursor {
    // SAFETY: the contract
    boxed(None, Kind::Plat(unsafe { ee_won_plat_cursor_pixels(fns, pixels, width, height, hot_x, hot_y) }))
}

/// `CursorRef(cursor)`: holds the cursor (NULL: none).
///
/// # Safety
/// `host` answers [`EeWonCursorOp`] for as long as the cursor lives; `cursor` is NULL or a live `Cursor*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_new_ref(host: EeWonCursorHost, cursor: isize) -> *mut EeWonCursor {
    let c = EeWonCursor { host, kind: Kind::Ref(Cell::new(cursor)) };
    if cursor != 0 {
        c.host(EeWonCursorOp::Hold, cursor, 0, 0);
    }
    Box::into_raw(Box::new(c))
}

/// `ComponentCursor(component, mouseX, mouseY)`: holds the component (NULL: none).
///
/// # Safety
/// As `ee_won_cursor_new_ref`; `component` is NULL or a live `Component*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_new_component(
    host: EeWonCursorHost,
    component: isize,
    x: i32,
    y: i32,
) -> *mut EeWonCursor {
    let c = EeWonCursor { host, kind: Kind::Component(component, x, y) };
    if component != 0 {
        c.host(EeWonCursorOp::CompHold, component, 0, 0);
    }
    Box::into_raw(Box::new(c))
}

/// The cursor's destruction: what it holds given back (a reference's cursor, a component cursor's component, the
/// platform cursor).
///
/// # Safety
/// `c` is NULL or from a constructor above, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_free(c: *mut EeWonCursor) {
    if c.is_null() {
        return;
    }
    // SAFETY: the contract; owned from here, so nothing borrows it across the calls out
    let me = unsafe { Box::from_raw(c) };
    match &me.kind {
        Kind::Plain => {}
        // SAFETY: from the platform module's constructor, freed once
        Kind::Plat(p) => unsafe { ee_won_plat_cursor_free(*p) },
        Kind::Ref(inner) => {
            if inner.get() != 0 {
                me.host(EeWonCursorOp::Release, inner.get(), 0, 0);
            }
        }
        Kind::Component(comp, _, _) => {
            if *comp != 0 {
                me.host(EeWonCursorOp::CompRelease, *comp, 0, 0);
            }
        }
    }
}

/// `GetNative()` of the cursor whose handle is `me`.
///
/// # Safety
/// `c` is a live cursor.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_native(c: *const EeWonCursor, me: isize) -> isize {
    // SAFETY: the contract (the call out goes to another cursor's handle)
    unsafe { (*c).native(me) }
}

/// `GetComponent(x, y)`.
///
/// # Safety
/// `c` is a live cursor; `x` and `y` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_component(c: *const EeWonCursor, x: *mut i32, y: *mut i32) -> isize {
    // SAFETY: the contract
    unsafe { (*c).component(x, y) }
}

/// The platform window's cursor change (PlatGUIWindow's `dynamic_cast<PlatCursor*>`): a platform cursor is shown
/// (`show(1)`: PlatCursorShow) and made the pointer's shape; any other cursor, or none, changes nothing.
///
/// # Safety
/// `c` is NULL or a live cursor.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_apply(c: *const EeWonCursor, show: unsafe extern "C" fn(show: i32)) {
    if c.is_null() {
        return;
    }
    // SAFETY: live (the contract)
    if let Kind::Plat(p) = unsafe { &(*c).kind } {
        // SAFETY: the platform's call; the platform cursor is this cursor's own
        unsafe {
            show(1);
            (**p).apply();
        }
    }
}

/// `gStandardCursors`: one `CursorRef` handle per `StandardCursor` value (0: not set yet), each held once.
static STANDARD: Mutex<[isize; 7]> = Mutex::new([0; 7]);

fn slot(kind: i32) -> Option<usize> {
    usize::try_from(kind).ok().filter(|&i| i < 7)
}

/// `Cursor::GetStandardCursor(type)`: the table's cursor (a reference to the one last set), NULL when none was set.
#[no_mangle]
pub extern "C" fn ee_won_cursor_standard(kind: i32) -> isize {
    slot(kind).map_or(0, |i| STANDARD.lock().unwrap_or_else(|e| e.into_inner())[i])
}

/// `Cursor::SetStandardCursor(type, cursor)`: the first time the table gets a new reference to the cursor (held);
/// afterwards that reference is pointed at the new cursor, so everyone who took the table's cursor follows.
///
/// # Safety
/// `host` answers [`EeWonCursorOp`] for good (the table lives as long as the process); `cursor` is NULL or a live
/// `Cursor*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_set_standard(host: EeWonCursorHost, kind: i32, cursor: isize) {
    let (Some(i), Some(h)) = (slot(kind), host) else { return };
    let have = STANDARD.lock().unwrap_or_else(|e| e.into_inner())[i];
    if have == 0 {
        // SAFETY: the contract (new CursorRef(cursor), then the table's SmartPtr)
        let made = unsafe {
            let made = h(EeWonCursorOp::Wrap, ee_won_cursor_new_ref(host, cursor) as isize, 0, 0);
            h(EeWonCursorOp::Hold, made, 0, 0);
            made
        };
        STANDARD.lock().unwrap_or_else(|e| e.into_inner())[i] = made;
    } else {
        // SAFETY: the table's handle is live (held); its Rust cursor
        unsafe {
            let r = h(EeWonCursorOp::RustOf, have, 0, 0) as *const EeWonCursor;
            (*r).set_inner(cursor);
        }
    }
}

/// PlatWindowManager's constructor: the standard cursors, the WON hand from its bits (32 x 32, hot spot 12, 4;
/// platimg.rs), the others the system's (`PLAT_CURSOR_ARROW`, `_IBEAM`, `_SIZE_WE`, `_SIZE_NS`, `_SIZE_ALL`), set in
/// the original's order.
///
/// # Safety
/// As `ee_won_cursor_set_standard`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cursor_install_standard(host: EeWonCursorHost, fns: &EeWonPlatCursorFns) {
    let Some(h) = host else { return };
    let mut hand = vec![0u32; 32 * 32];
    // SAFETY: 1,024 pixels
    unsafe { crate::platimg::ee_won_plat_hand_cursor(hand.as_mut_ptr()) };
    // (StandardCursor, the cursor): Hand, Arrow, IBeam, EastWest, NorthSouth, NorthSouthEastWest
    // SAFETY: the pixels above
    let first = unsafe { ee_won_cursor_new_pixels(fns, hand.as_ptr(), 32, 32, 12, 4) };
    let rest = [(0, 0u32), (2, 2), (3, 3), (4, 4), (5, 5)];
    // SAFETY: the contract
    unsafe {
        ee_won_cursor_set_standard(host, 1, h(EeWonCursorOp::Wrap, first as isize, 0, 0));
        for (kind, system) in rest {
            let c = ee_won_cursor_new_system(fns, system);
            ee_won_cursor_set_standard(host, kind, h(EeWonCursorOp::Wrap, c as isize, 0, 0));
        }
    }
}

#[cfg(test)]
#[path = "cursorobj_tests.rs"]
mod tests;
