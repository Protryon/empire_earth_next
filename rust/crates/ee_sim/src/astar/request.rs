//! The path requests (`EEAutoPathRequest`, a C++ class of EEPathManager.cpp until the paths lane's round 2): a
//! player's graph path request between two tiles with its walker flags, its found path (the sub sectors' tiles),
//! its summed length and whether it is done. A request is a Rust record behind the `EEAutoPathRequest*` handle the
//! C++ keeps (an opaque type: ee/EEAutoPathRequest.h), reference counted as the `UReferenceCounter` was (one
//! reference at creation, freed at the last `Dereference`).
//!
//! Also the owners waiting for a loaded request (`EEAwaitAutoPathRequest`: the saved game's graph searches give
//! their requests back at the loaded game's first tick) and the request's saved record (`EESaveAutoPathRequest`,
//! `EELoadAutoPathRequest`; save 0.277).

use std::collections::BTreeMap;
use std::ffi::{c_long, c_void};
use std::sync::Mutex;

use crate::astar::pm::{EePathIo, EePathRequestRecord, RequestRecord};

/// `EECPASGFlags`' layout (ee/EECPASGFlags.h): the player and the 17 walker flags.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathAsgFlags {
    pub player: c_long,
    pub flags: [bool; 17],
}

/// A request: its reference count and its fields (the saved record's) and path.
#[derive(Debug)]
pub struct PathRequest {
    refs: u32,
    pub r: EePathRequestRecord,
    pub path: Vec<[i32; 2]>,
}

/// `new EEAutoPathRequest(id, player, from, to, flags)`: not done, no path; the flags' player 0 (the copy
/// constructor's) -> its handle (one reference, the caller's).
pub fn new(id: i32, player: i32, from: [i32; 2], to: [i32; 2], flags: &[bool; 17]) -> u64 {
    let r = EePathRequestRecord { from, to, player, id, length: 0, done: false, flags_player: 0, flags: *flags };
    Box::into_raw(Box::new(PathRequest { refs: 1, r, path: Vec::new() })) as u64
}

/// A loaded request from its saved record (every field) -> its handle (one reference, the caller's).
pub fn from_record(rr: &RequestRecord) -> u64 {
    Box::into_raw(Box::new(PathRequest { refs: 1, r: rr.r, path: rr.path.clone() })) as u64
}

/// The request of a handle.
///
/// # Safety
/// `h` a live request.
pub unsafe fn get<'a>(h: u64) -> &'a mut PathRequest {
    // SAFETY: the caller's contract
    unsafe { &mut *(h as *mut PathRequest) }
}

/// The request's saved record (`EERequestRecordOf`).
///
/// # Safety
/// `h` a live request.
pub unsafe fn record(h: u64) -> RequestRecord {
    // SAFETY: the caller's contract
    let q = unsafe { get(h) };
    RequestRecord { r: q.r, path: q.path.clone() }
}

/// `Reference()`.
///
/// # Safety
/// `h` a live request.
pub unsafe fn reference(h: u64) {
    // SAFETY: the caller's contract
    let q = unsafe { get(h) };
    q.refs = q.refs.wrapping_add(1);
}

/// `Dereference()`: freed at the last reference.
///
/// # Safety
/// `h` a live request; not used after its last reference.
pub unsafe fn dereference(h: u64) {
    // SAFETY: the caller's contract
    let q = unsafe { get(h) };
    q.refs = q.refs.wrapping_sub(1);
    if q.refs == 0 {
        // SAFETY: made by `new` or `from_record`; the last reference
        drop(unsafe { Box::from_raw(h as *mut PathRequest) });
    }
}

/// A found graph path's tiles, its summed length; done (`RequestUnitPaths`' request half).
///
/// # Safety
/// `h` a live request.
pub unsafe fn set_path(h: u64, tiles: &[(i32, i32)], length: i32) {
    // SAFETY: the caller's contract
    let q = unsafe { get(h) };
    q.path.clear();
    q.path.extend(tiles.iter().map(|t| [t.0, t.1]));
    q.r.length = length;
    q.r.done = true;
}

// ---- the owners waiting for their loaded requests

/// An owner's attach: the rebuilt request (one reference, the owner's) or 0 (it asks again).
pub type Attach = Box<dyn FnOnce(u64)>;

struct Waiter(Attach);
// SAFETY: the waits are made and resolved on the simulation thread (the load and the loaded game's first tick); the
// lock only keeps the static sound
unsafe impl Send for Waiter {}

#[derive(Default)]
struct Waits {
    awaited: BTreeMap<i64, Waiter>,
    resolving: BTreeMap<i64, Waiter>,
    queued: bool,
}

static WAITS: Mutex<Option<Waits>> = Mutex::new(None);

fn waits<R>(f: impl FnOnce(&mut Waits) -> R) -> R {
    let mut g = WAITS.lock().unwrap_or_else(|e| e.into_inner());
    f(g.get_or_insert_with(Waits::default))
}

/// Whether the resolve was queued already; queues it (`EEQueueGraphResolve`'s flag).
fn take_queue() -> bool {
    waits(|w| std::mem::replace(&mut w.queued, true))
}

/// `EEAwaitAutoPathRequest(id, attach)`: the owner of the running request `id` of a saved graph search waits for it
/// until the loaded game's first tick (ids are unique: a second owner of one makes the first ask again).
pub fn await_request(id: i64, attach: Attach) {
    let old = waits(|w| w.awaited.remove(&id));
    if let Some(Waiter(old)) = old {
        old(0);
    }
    waits(|w| w.awaited.insert(id, Waiter(attach)));
    queue_resolve();
}

/// The graph searches' resolve queued for the loaded game's first tick, after every owner's own fix-up (once).
pub fn queue_resolve() {
    if !take_queue() {
        after_load_last();
    }
}

/// `EE_PATH_AWAIT`'s operations: 0 the resolve takes the owners, 1 whether one waits for request id `i1`, 2 that
/// one takes request `h0` (the load's reference), 3 the others get none.
pub fn await_op(io: &EePathIo) -> i64 {
    match io.i[0] {
        0 => {
            waits(|w| {
                w.resolving = std::mem::take(&mut w.awaited);
            });
            0
        }
        1 => i64::from(waits(|w| w.resolving.contains_key(&io.i[1]))),
        2 => {
            if let Some(Waiter(attach)) = waits(|w| w.resolving.remove(&io.i[1])) {
                attach(io.h[0]);
            }
            0
        }
        _ => {
            let rest = waits(|w| std::mem::take(&mut w.resolving));
            for (_, Waiter(attach)) in rest {
                attach(0);
            }
            0
        }
    }
}

/// The resolve at the loaded game's first tick (`EEAfterLoadLast`'s fix-up): the game's manager's call 19.
#[no_mangle]
pub extern "C" fn ee_path_resolve_graph_searches() {
    waits(|w| w.queued = false);
    let p = crate::astar::pm::global();
    if !p.is_null() {
        let mut io = EePathIo::default();
        // SAFETY: the game's live manager
        unsafe { crate::astar::pm::ee_path_manager_call(p, 19, &mut io) };
    }
}

#[cfg(not(test))]
extern "C" {
    /// `EEAfterLoadLast(f)` (EEPathManager.cpp's one statement)
    fn ee_path_after_load_last(f: extern "C" fn());
}

#[cfg(not(test))]
fn after_load_last() {
    // SAFETY: the C++ queue of the loaded game's last fix-ups
    unsafe { ee_path_after_load_last(ee_path_resolve_graph_searches) }
}

#[cfg(test)]
fn after_load_last() {}

// ---- the C++ callers' accessors (the opaque `EEAutoPathRequest*`)

/// `Dereference()`.
///
/// # Safety
/// `h` a live request.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_dereference(h: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { dereference(h as u64) }
}

/// `mIsDone`.
///
/// # Safety
/// `h` a live request.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_done(h: *const c_void) -> bool {
    // SAFETY: the caller's contract
    unsafe { get(h as u64) }.r.done
}

/// `mPath.size()`; with `out` non-NULL its tiles (x, y pairs of `long`) up to `cap` into it.
///
/// # Safety
/// `h` a live request; `out` NULL or writable for `cap` pairs.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_tiles(h: *const c_void, out: *mut c_long, cap: u32) -> u32 {
    // SAFETY: the caller's contract
    let q = unsafe { get(h as u64) };
    if !out.is_null() {
        for (k, t) in q.path.iter().take(cap as usize).enumerate() {
            // SAFETY: within `cap`
            unsafe {
                *out.add(2 * k) = t[0] as c_long;
                *out.add(2 * k + 1) = t[1] as c_long;
            }
        }
    }
    q.path.len() as u32
}

// ---- the game's manager's calls for a request (pm.rs ee_path_manager_call)

/// `gPathManager.RequestPath(player, from, to, flags, kind, revs, modifier, unlimited)` -> the request (one
/// reference, the caller's).
#[allow(clippy::too_many_arguments)]
pub fn request_path(
    player: i64,
    from: (i64, i64),
    to: (i64, i64),
    flags: &EePathAsgFlags,
    kind: i64,
    revs: i64,
    modifier: u64,
    unlimited: bool,
) -> u64 {
    let mut io = EePathIo::default();
    io.h[0] = flags as *const EePathAsgFlags as u64;
    io.h[1] = modifier;
    let v = [player, from.0, from.1, to.0, to.1, kind, revs, i64::from(unlimited)];
    io.i[..8].copy_from_slice(&v);
    // SAFETY: the game's live manager; the flags live for the call
    unsafe { crate::astar::pm::ee_path_manager_call(crate::astar::pm::global(), 14, &mut io) as u64 }
}

/// `gPathManager.CancelAutoPathRequest(request)`.
pub fn cancel(h: u64) {
    let mut io = EePathIo::default();
    io.h[0] = h;
    // SAFETY: the game's live manager; `h` a live request
    unsafe { crate::astar::pm::ee_path_manager_call(crate::astar::pm::global(), 8, &mut io) };
}

/// `gPathManager.CancelAutoPathRequest(request)` then `request->Dereference()`.
///
/// # Safety
/// `h` a live request with the caller's reference.
pub unsafe fn cancel_and_release(h: u64) {
    cancel(h);
    // SAFETY: the caller's reference
    unsafe { dereference(h) };
}

/// The goal host's reach test (EEUGRust.cpp's `PATH_REACHABLE`): unit `unit`'s player's request from `from` to `to`
/// with the unit class's terrain flags (`RequestPath(player, from, to, flags, 1, 10, NULL, 1)`); whether its path's
/// last tile is in `to`'s sub sector, or in a neighbour whose links to it are blocked for the flags. The request is
/// released (not cancelled), as the C++ did.
///
/// # Safety
/// `unit` a live complex unit.
#[cfg(not(test))]
pub unsafe fn path_reachable(unit: u64, from: (i32, i32), to: (i32, i32)) -> bool {
    use crate::astar::pm_host::sub_sector_at;
    let (class, player) = {
        // SAFETY: the goal's calls run on the simulation thread, which holds the world
        let t = unsafe { crate::world::WorldRead::begin() };
        // SAFETY: the caller's contract
        let u = unsafe { crate::world::UnitView::of(&t, unit) }.expect("a live unit");
        (u.class().map_or(0, |c| c.handle()), u.player().map_or(0, |p| i64::from(p.id())))
    };
    let flags = EePathAsgFlags { player: 0, flags: crate::units::gather_spot::class_terrain_flags(class, unit) };
    let to64 = (i64::from(to.0), i64::from(to.1));
    let path = request_path(player, (i64::from(from.0), i64::from(from.1)), to64, &flags, 1, 10, 0, true);
    let mut reachable = false;
    // SAFETY: the reference just made
    if let Some(last) = unsafe { get(path) }.path.last().copied() {
        let first = sub_sector_at(i64::from(last[0]), i64::from(last[1]));
        let second = sub_sector_at(to64.0, to64.1);
        reachable = first == second;
        // SAFETY: listed sub sectors are live objects with their lists
        let neighbor = first != 0
            && second != 0
            && unsafe { crate::astar::cm_io::sub(first).lists.as_ref() }.is_some_and(|l| l.neighbors.contains(&second));
        if !reachable && neighbor {
            // SAFETY: as above; the 17 flags
            reachable = unsafe {
                crate::astar::modifier_obj::ee_astar_sub_sector_links_blocked(first, second, flags.flags.as_ptr())
            };
        }
    }
    // SAFETY: the reference made above
    unsafe { dereference(path) };
    reachable
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_new_request_is_not_done_and_its_flags_player_is_zero() {
        let mut f = [false; 17];
        f[3] = true;
        let h = new(7, 2, [1, 2], [3, 4], &f);
        // SAFETY: just made
        let rr = unsafe { record(h) };
        assert_eq!(rr.r.id, 7);
        assert_eq!(rr.r.player, 2);
        assert_eq!(rr.r.from, [1, 2]);
        assert_eq!(rr.r.to, [3, 4]);
        assert_eq!(rr.r.flags_player, 0);
        assert!(rr.r.flags[3] && !rr.r.flags[2]);
        assert!(!rr.r.done && rr.path.is_empty() && rr.r.length == 0);
        // SAFETY: the reference made
        unsafe { dereference(h) };
    }

    #[test]
    fn the_path_set_marks_it_done_and_the_tiles_read_back() {
        let h = new(1, 1, [0, 0], [9, 9], &[true; 17]);
        // SAFETY: live until the last dereference
        unsafe {
            reference(h);
            set_path(h, &[(1, 1), (5, 6), (9, 9)], 42);
            assert!(ee_path_request_done(h as *const c_void));
            assert_eq!(ee_path_request_tiles(h as *const c_void, std::ptr::null_mut(), 0), 3);
            let mut out = [0 as c_long; 4];
            assert_eq!(ee_path_request_tiles(h as *const c_void, out.as_mut_ptr(), 2), 3);
            assert_eq!(out, [1, 1, 5, 6]);
            assert_eq!(record(h).r.length, 42);
            dereference(h);
            // still one reference
            assert_eq!(get(h).refs, 1);
            dereference(h);
        }
    }

    #[test]
    fn a_loaded_request_keeps_every_field() {
        let rr = RequestRecord {
            r: EePathRequestRecord {
                from: [1, 2],
                to: [3, 4],
                player: 5,
                id: 6,
                length: 7,
                done: true,
                flags_player: 8,
                flags: [true; 17],
            },
            path: vec![[1, 2], [3, 4]],
        };
        let h = from_record(&rr);
        // SAFETY: just made
        unsafe {
            assert_eq!(record(h), rr);
            dereference(h);
        }
    }

    #[test]
    fn the_waits_resolve_in_id_order_and_a_second_owner_replaces_the_first() {
        use std::cell::RefCell;
        use std::rc::Rc;
        let got: Rc<RefCell<Vec<(i64, u64)>>> = Rc::default();
        let mk = |tag: i64| -> Attach {
            let g = got.clone();
            Box::new(move |r| g.borrow_mut().push((tag, r)))
        };
        await_request(30, mk(1));
        await_request(10, mk(2));
        await_request(30, mk(3));
        assert_eq!(*got.borrow(), vec![(1, 0)]);
        let mut io = EePathIo::default();
        await_op(&io);
        io.i[0] = 1;
        io.i[1] = 30;
        assert_eq!(await_op(&io), 1);
        io.i[1] = 20;
        assert_eq!(await_op(&io), 0);
        io.i[0] = 2;
        io.i[1] = 30;
        io.h[0] = 0x55;
        await_op(&io);
        io.i[0] = 3;
        await_op(&io);
        assert_eq!(*got.borrow(), vec![(1, 0), (3, 0x55), (2, 0)]);
        // the queue was taken once; the resolve clears it
        assert!(take_queue());
        waits(|w| w.queued = false);
    }
}
