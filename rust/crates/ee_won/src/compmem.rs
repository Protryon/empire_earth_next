//! Component's members as a Rust block (R6 WONGUI B round 2): the parent, the bounds, the flags, the control id,
//! the focus id, the invalid state, the timer flags and the extra data's pointer of every lobby control.
//!
//! The C++ `Component` keeps its members by name (the lobby's classes read `mBounds`, `mComponentBits` and
//! `mParent` inline at ~300 sites) and lays them out as this block, at `EE_WON_COMP_MEMBERS_AT` from the object
//! (after RefCount's vtable pointer and count; ComponentLifetime.cpp asserts each offset). The Rust objects read
//! and write them here in place instead of asking their hosts: a component is a `Component*` as `isize`, its block
//! at that address plus the offset.
//!
//! The drivers' and tests' worlds name their components by small ids, not addresses: they register their table of
//! blocks (`ee_won_comp_members_table`: the ids `first..first + count` at `base`, `stride` bytes apart), and an id in
//! that range resolves to its entry. The table is per thread (a test's world is its thread's); the game registers
//! none.
//!
//! No reference into a block lives across a call: each accessor reads or writes one field and returns, so a virtual
//! the C++ runs between two of them may change any member.

use std::cell::Cell;

use crate::guilayout::{intersects as rects_intersect, EeWonRect};

/// Component's members, in their C++ order (`mParent` .. `mExtraStuff`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonCompMembers {
    /// `mParent` (a `Container*`, which is its `Component*`: Container's only base), 0 for none.
    pub parent: isize,
    /// `mBounds`.
    pub bounds: EeWonRect,
    /// `mComponentBits`.
    pub bits: i32,
    /// `mControlId`.
    pub control_id: i32,
    /// `mFocusId` (`unsigned char`).
    pub focus_id: u8,
    /// `mInvalidState` (`char`).
    pub invalid: i8,
    /// `mTimerFlags` (`char`).
    pub timer: i8,
    /// `mExtraStuff` (owned by the C++; the Rust tests only whether it is made).
    pub extra: isize,
}

/// The block's offset in a C++ `Component` (RefCount's vtable pointer and its 32-bit count, padded).
pub const EE_WON_COMP_MEMBERS_AT: usize = 16;

#[derive(Clone, Copy)]
struct Table {
    base: usize,
    first: isize,
    count: isize,
    stride: usize,
}

/// A world's own lookup of a component's block (`ee_won_comp_members_resolver`).
pub type EeWonCompResolveFn = Option<unsafe extern "C" fn(c: isize) -> *mut EeWonCompMembers>;

thread_local! {
    static TABLE: Cell<Table> = const { Cell::new(Table { base: 0, first: 0, count: 0, stride: 0 }) };
    static RESOLVE: Cell<EeWonCompResolveFn> = const { Cell::new(None) };
}

/// A world's components by id (drivers and tests): the ids `first..first + count` resolve to the blocks at
/// `base + (id - first) * stride`. `base` NULL removes the table.
#[no_mangle]
pub extern "C" fn ee_won_comp_members_table(base: *mut EeWonCompMembers, first: isize, count: isize, stride: usize) {
    TABLE.with(|t| t.set(Table { base: base as usize, first, count, stride }));
}

/// A world whose components live in a map (drivers and tests): `f` answers a component's block, an id outside a
/// registered table. A world whose Rust objects only read the members may answer a block filled from its own state
/// at each call (the block is read at once, before any other call). NULL removes it.
#[no_mangle]
pub extern "C" fn ee_won_comp_members_resolver(f: EeWonCompResolveFn) {
    RESOLVE.with(|r| r.set(f));
}

/// The block of the component `c`.
pub fn at(c: isize) -> *mut EeWonCompMembers {
    let t = TABLE.with(|t| t.get());
    if t.base != 0 && c >= t.first && c < t.first + t.count {
        return (t.base + (c - t.first) as usize * t.stride) as *mut EeWonCompMembers;
    }
    if let Some(f) = RESOLVE.with(|r| r.get()) {
        // SAFETY: the world's lookup, registered for this thread
        return unsafe { f(c) };
    }
    (c as usize).wrapping_add(EE_WON_COMP_MEMBERS_AT) as *mut EeWonCompMembers
}

/// A copy of `c`'s members.
pub fn get(c: isize) -> EeWonCompMembers {
    // SAFETY: the caller's contract: `c` is a live component (or an id of the registered table)
    unsafe { std::ptr::read(at(c)) }
}

/// Edits `c`'s members in place (`f` must not call out).
pub fn edit<R>(c: isize, f: impl FnOnce(&mut EeWonCompMembers) -> R) -> R {
    // SAFETY: as `get`; the reference ends with `f`, which makes no call back into the C++
    unsafe { f(&mut *at(c)) }
}

/// `mComponentBits`.
pub fn bits(c: isize) -> i32 {
    get(c).bits
}
/// `mComponentBits = v`.
pub fn set_bits(c: isize, v: i32) {
    edit(c, |m| m.bits = v);
}
/// `SetComponentFlags(flags, on)`.
pub fn set_flags(c: isize, flags: i32, on: bool) {
    edit(c, |m| m.bits = if on { m.bits | flags } else { m.bits & !flags });
}
/// `mParent` (as a `Component*`).
pub fn parent(c: isize) -> isize {
    get(c).parent
}
/// `mParent = p`.
pub fn set_parent(c: isize, p: isize) {
    edit(c, |m| m.parent = p);
}
/// `mBounds`.
pub fn bounds(c: isize) -> EeWonRect {
    get(c).bounds
}
/// `mBounds.x = x; mBounds.y = y`.
pub fn set_xy(c: isize, x: i32, y: i32) {
    edit(c, |m| {
        m.bounds.x = x;
        m.bounds.y = y;
    });
}
/// `mBounds.height = h; mBounds.width = w`.
pub fn set_wh(c: isize, w: i32, h: i32) {
    edit(c, |m| {
        m.bounds.height = h;
        m.bounds.width = w;
    });
}
/// `Left()`.
pub fn left(c: isize) -> i32 {
    get(c).bounds.x
}
/// `Top()`.
pub fn top(c: isize) -> i32 {
    get(c).bounds.y
}
/// `Width()`.
pub fn width(c: isize) -> i32 {
    get(c).bounds.width
}
/// `Height()`.
pub fn height(c: isize) -> i32 {
    get(c).bounds.height
}
/// `mControlId`.
pub fn control_id(c: isize) -> i32 {
    get(c).control_id
}
/// `mFocusId`.
pub fn focus_id(c: isize) -> u8 {
    get(c).focus_id
}
/// `mFocusId = v`.
pub fn set_focus_id(c: isize, v: u8) {
    edit(c, |m| m.focus_id = v);
}
/// `mInvalidState`.
pub fn invalid(c: isize) -> i8 {
    get(c).invalid
}
/// `mInvalidState = v`.
pub fn set_invalid(c: isize, v: i8) {
    edit(c, |m| m.invalid = v);
}
/// `mTimerFlags`.
pub fn timer(c: isize) -> i8 {
    get(c).timer
}
/// `mTimerFlags = v`.
pub fn set_timer(c: isize, v: i8) {
    edit(c, |m| m.timer = v);
}
/// `mExtraStuff != NULL`.
pub fn has_extra(c: isize) -> bool {
    get(c).extra != 0
}
/// `IsVisible()` (flag 0x1 clear).
pub fn is_visible(c: isize) -> bool {
    bits(c) & 1 == 0
}
/// `Contains(x, y)` (in the parent's coordinates; the right and bottom edges outside).
pub fn contains(c: isize, x: i32, y: i32) -> bool {
    let r = bounds(c);
    x >= r.x && x < r.x.wrapping_add(r.width) && y >= r.y && y < r.y.wrapping_add(r.height)
}
/// `Intersects(other)`.
pub fn intersects(c: isize, other: isize) -> bool {
    rects_intersect(bounds(c), bounds(other))
}

/// A test world of zeroed components (ids `0..count`), registered on this thread until it is dropped.
#[cfg(test)]
pub(crate) struct TestWorld(#[allow(dead_code)] Box<[EeWonCompMembers]>);

#[cfg(test)]
impl Drop for TestWorld {
    fn drop(&mut self) {
        ee_won_comp_members_table(std::ptr::null_mut(), 0, 0, 0);
    }
}

#[cfg(test)]
pub(crate) fn zeroed_world(count: usize) -> TestWorld {
    let mut w = vec![EeWonCompMembers::default(); count].into_boxed_slice();
    ee_won_comp_members_table(w.as_mut_ptr(), 0, count as isize, std::mem::size_of::<EeWonCompMembers>());
    TestWorld(w)
}

#[cfg(test)]
thread_local! {
    static FILL: Cell<Option<fn(isize) -> EeWonCompMembers>> = const { Cell::new(None) };
    static SCRATCH: std::cell::UnsafeCell<EeWonCompMembers> = std::cell::UnsafeCell::new(EeWonCompMembers::default());
}

#[cfg(test)]
unsafe extern "C" fn fill_resolve(c: isize) -> *mut EeWonCompMembers {
    let f = FILL.with(|f| f.get()).expect("compmem: no filled world");
    let m = f(c);
    SCRATCH.with(|s| {
        // SAFETY: the thread's scratch block, read at once by the accessor that asked
        unsafe { *s.get() = m };
        s.get()
    })
}

/// A test world whose Rust objects only read the members: `f` makes a component's block from the world's state at
/// each read, until the guard is dropped.
#[cfg(test)]
pub(crate) struct FilledWorld;

#[cfg(test)]
impl Drop for FilledWorld {
    fn drop(&mut self) {
        ee_won_comp_members_resolver(None);
        FILL.with(|x| x.set(None));
    }
}

#[cfg(test)]
pub(crate) fn filled_world(f: fn(isize) -> EeWonCompMembers) -> FilledWorld {
    FILL.with(|x| x.set(Some(f)));
    ee_won_comp_members_resolver(Some(fill_resolve));
    FilledWorld
}

/// A block of these bounds (the filled worlds').
#[cfg(test)]
pub(crate) fn block(x: i32, y: i32, w: i32, h: i32) -> EeWonCompMembers {
    EeWonCompMembers { bounds: EeWonRect::new(x, y, w, h), ..Default::default() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_block_is_the_c_layout() {
        assert_eq!(std::mem::size_of::<EeWonCompMembers>(), 48);
        let m = EeWonCompMembers::default();
        let base = &m as *const _ as usize;
        assert_eq!(&m.bounds as *const _ as usize - base, 8);
        assert_eq!(&m.bits as *const _ as usize - base, 24);
        assert_eq!(&m.control_id as *const _ as usize - base, 28);
        assert_eq!(&m.focus_id as *const _ as usize - base, 32);
        assert_eq!(&m.invalid as *const _ as usize - base, 33);
        assert_eq!(&m.timer as *const _ as usize - base, 34);
        assert_eq!(&m.extra as *const _ as usize - base, 40);
    }

    #[test]
    fn an_address_resolves_past_the_refcount() {
        #[repr(C)]
        struct Obj {
            vtbl: usize,
            refs: i32,
            m: EeWonCompMembers,
        }
        let mut o = Obj { vtbl: 0, refs: 0, m: EeWonCompMembers::default() };
        let c = &mut o as *mut Obj as isize;
        set_xy(c, 3, 4);
        set_wh(c, 5, 6);
        set_flags(c, 0x11, true);
        set_flags(c, 0x1, false);
        set_parent(c, 77);
        set_timer(c, -1);
        set_invalid(c, 2);
        set_focus_id(c, 200);
        assert_eq!(o.m.bounds, EeWonRect::new(3, 4, 5, 6));
        assert_eq!((o.m.bits, o.m.parent, o.m.timer, o.m.invalid, o.m.focus_id), (0x10, 77, -1, 2, 200));
        assert!(contains(c, 3, 4) && contains(c, 7, 9) && !contains(c, 8, 4) && !contains(c, 3, 10));
        assert!(!has_extra(c) && is_visible(c));
        assert_eq!((o.vtbl, o.refs), (0, 0));
    }

    #[test]
    fn a_table_resolves_ids() {
        #[repr(C)]
        #[derive(Default, Clone, Copy)]
        struct W {
            m: EeWonCompMembers,
            other: i64,
        }
        let mut w = [W::default(); 4];
        ee_won_comp_members_table(&mut w[0].m, 1, 4, std::mem::size_of::<W>());
        set_xy(1, 100, 200);
        set_wh(4, 10, 20);
        set_bits(3, 0x80);
        assert_eq!(w[0].m.bounds, EeWonRect::new(100, 200, 0, 0));
        assert_eq!(w[3].m.bounds, EeWonRect::new(0, 0, 10, 20));
        assert_eq!(bits(3), 0x80);
        assert!(intersects(4, 4) && !intersects(1, 4));
        assert_eq!(w[2].other, 0);
        ee_won_comp_members_table(std::ptr::null_mut(), 0, 0, 0);
    }
}
