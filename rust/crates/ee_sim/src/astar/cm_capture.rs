//! TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): CNTQ, one record per outermost call of the continent
//! manager's queries and changes (a u32 kind, the inputs, the result: an area as its serial and type, 0xffffffff
//! and -1 for none; a change ends with the FNV-1a 64 of the whole state CNTR records), and CNTN, every
//! `GetTilesNextTo`'s inputs and outputs. Was EEAIContinentManager.cpp's `EEContinentQueryCapture`; the scope's
//! depth is one per thread for the Rust queries and the C++ ones that are left (`ee_astar_cntq_*`), so a query a
//! query makes is not recorded, whichever side either is.

#![allow(clippy::unnecessary_cast)]

use std::cell::{Cell, RefCell};
use std::ffi::c_long;

use crate::app::capture_record::{self, Record};
use crate::astar::cm::EeAStarCmRecord;
use crate::astar::cm_io;

/// "CNTQ", "CNTN"
pub(crate) const CNTQ: u32 = 0x5154_4e43;
pub(crate) const CNTN: u32 = 0x4e54_4e43;

// the query kinds (the C++'s enum, in its order; 6 CanGetTo, 13 AreConnectedForUnit and 24 UpdateWallUnit are the
// C++'s)
pub(crate) const Q_CONNECTED_IDS: u32 = 1;
pub(crate) const Q_ADJACENT_IDS: u32 = 2;
pub(crate) const Q_HAS_SHORE_TILE: u32 = 3;
pub(crate) const Q_IN_SAME_CONTINENT: u32 = 4;
pub(crate) const Q_IN_SAME_AREA: u32 = 5;
pub(crate) const Q_CHOKE_POINT_AT: u32 = 7;
pub(crate) const Q_CHOKE_POINTS: u32 = 8;
pub(crate) const Q_CONTINENT_CENTER: u32 = 9;
pub(crate) const Q_CONTINENT_TILES: u32 = 10;
pub(crate) const Q_FOREST_AT: u32 = 11;
pub(crate) const Q_HILL_AT: u32 = 12;
pub(crate) const Q_CONNECTED: u32 = 14;
pub(crate) const Q_LARGEST_CONTINENT: u32 = 15;
pub(crate) const Q_PATH_AREA_AT: u32 = 16;
pub(crate) const Q_CONTINENT: u32 = 17;
pub(crate) const Q_SUB_SECTOR_AT: u32 = 18;
pub(crate) const Q_RANDOM_AREA: u32 = 19;
pub(crate) const Q_RANDOM_CONTINENT: u32 = 20;
pub(crate) const Q_NEXT_TO_AREA: u32 = 21;
pub(crate) const Q_REPAIR_PATH_AREA: u32 = 22;
pub(crate) const Q_REMOVE_FOREST_TILE: u32 = 23;

thread_local! {
    static DEPTH: Cell<i32> = const { Cell::new(0) };
    static OPEN: RefCell<Vec<Record>> = const { RefCell::new(Vec::new()) };
}

/// One query's scope (`EEContinentQueryCapture`): `on` when it is its thread's outermost and the capture is on.
pub struct Query {
    on: bool,
}

impl Query {
    pub fn new(kind: u32) -> Query {
        Query { on: enter(kind) }
    }
    pub fn i(&self, v: i64) {
        if self.on {
            push(v as i32);
        }
    }
    pub fn tile(&self, x: i64, y: i64) {
        self.i(x);
        self.i(y);
    }
    /// An area as its serial and type, -1 -1 for none.
    pub fn area(&self, h: u64) {
        if !self.on {
            return;
        }
        if h == 0 {
            self.i(-1);
            self.i(-1);
        } else {
            // SAFETY: a live area (an object of the manager's or a town or herd)
            let a = unsafe { cm_io::area(h) };
            self.i(a.serial as i64);
            self.i(a.area_type as i64);
        }
    }
    pub fn shorts(&self, v: &[u16]) {
        self.i(v.len() as i64);
        for &s in v {
            self.i(s as i64);
        }
    }
    pub fn points(&self, v: &[(c_long, c_long)]) {
        self.i(v.len() as i64);
        for &(x, y) in v {
            self.tile(x as i64, y as i64);
        }
    }
    pub fn areas(&self, v: &[u64]) {
        self.i(v.len() as i64);
        for &h in v {
            self.area(h);
        }
    }
    /// A change's end: the state's digest (CNTR's).
    pub fn state(&self, r: &EeAStarCmRecord) {
        if self.on {
            let d = cm_io::capture_digest(r);
            push(d as u32 as i32);
            push((d >> 32) as u32 as i32);
        }
    }
}

impl Drop for Query {
    fn drop(&mut self) {
        leave(self.on);
    }
}

fn enter(kind: u32) -> bool {
    let depth = DEPTH.with(|d| {
        let v = d.get();
        d.set(v + 1);
        v
    });
    let on = depth == 0 && capture_record::active();
    if on {
        let mut r = Record::new(CNTQ);
        r.u32(kind);
        OPEN.with(|o| o.borrow_mut().push(r));
    }
    on
}

fn push(v: i32) {
    OPEN.with(|o| {
        if let Some(r) = o.borrow_mut().last_mut() {
            r.i32(v);
        }
    });
}

fn leave(on: bool) {
    DEPTH.with(|d| d.set(d.get() - 1));
    if on {
        if let Some(r) = OPEN.with(|o| o.borrow_mut().pop()) {
            r.commit();
        }
    }
}

/// CNTN: a `GetTilesNextTo` call's inputs and outputs.
pub fn next_to(x: c_long, y: c_long, id: u16, result: bool, tiles: &[(c_long, c_long)]) {
    if !capture_record::active() {
        return;
    }
    let mut r = Record::new(CNTN);
    r.i32(x as i32);
    r.i32(y as i32);
    r.u32(id as u32);
    r.u32(result as u32);
    r.u32(tiles.len() as u32);
    for &(tx, ty) in tiles {
        r.i32(tx as i32);
        r.i32(ty as i32);
    }
    r.commit();
}

/// A C++ query's scope opens (the queries left in C++: CanGetTo, AreConnectedForUnit, UpdateWallUnit); whether it
/// records.
#[no_mangle]
pub extern "C" fn ee_astar_cntq_enter(kind: u32) -> bool {
    enter(kind)
}

/// A value of the open record (only a scope that records calls it).
#[no_mangle]
pub extern "C" fn ee_astar_cntq_i(v: i32) {
    push(v)
}

/// The scope closes (`on`: what `ee_astar_cntq_enter` answered).
#[no_mangle]
pub extern "C" fn ee_astar_cntq_leave(on: bool) {
    leave(on)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_the_outermost_scope_counts_and_the_depth_returns() {
        // the capture is off in the tests: no scope records, the depth still nests
        {
            let q = Query::new(Q_CONTINENT);
            assert!(!q.on);
            q.tile(1, 2);
            {
                let inner = Query::new(Q_PATH_AREA_AT);
                assert!(!inner.on);
            }
            assert_eq!(DEPTH.with(|d| d.get()), 1);
        }
        assert_eq!(DEPTH.with(|d| d.get()), 0);
        assert!(OPEN.with(|o| o.borrow().is_empty()));
    }
}
