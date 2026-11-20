//! TEST ONLY: the tech tree's capture records written where its walks run (were the C++ wrappers of
//! EETechTreeNode.cpp, EETechTreeEpoch.cpp, EETechTree.cpp and EETechCapture.cpp's TNWK and TRTE records;
//! ee/EETechCapture.h has their layouts): TNRS (a node's or an epoch's research), TNPR (a node's prerequisite),
//! TSOL (an epoch's counter and availability), TTRS (the tree's research), TEPO (the epoch setters), TRTE
//! (ResearchToEpoch's steps) and TNWK / TNMT (a walk's steps). Each record is the C++'s byte for byte: the state
//! before, the body, the state after; without a capture each is the body alone.
//!
//! The entry points the game calls (ee_tech_walk_node, _epoch, _tree) and the walks' own calls of those entry points
//! (the host operations the C++ answered through its wrappers) both come here, so the records keep their order.

#![allow(clippy::unnecessary_cast)] // c_long is 32 bits on Windows, 64 elsewhere

use std::ffi::c_void;
use std::os::raw::c_long;

use crate::app::capture;
use crate::tech::capture_rec::{self, node_bytes, tag, tree_bytes, Bytes};
use crate::tech::nodes::{
    epoch_add_sol, epoch_research, epoch_update_availability, node_research, node_set_prerequisite, tree_research,
    EeTechHostIo, EeTechNodeDataFields, EeTechNodeFields, EeTechNodeRef, EeTechTreeFields, TechHost, Tree,
};
use crate::tech::tree;

/// The tree's player index (`mPlayerIndex`) -> `v[0]`
pub const EE_TECH_HOST_PLAYER_INDEX: i32 = 7;
/// The researching unit's id (`unit`, an `EEComplexUnit*`; -1 without one) -> `v[0]`
pub const EE_TECH_HOST_UNIT_ID: i32 = 8;

fn active() -> bool {
    capture::ee_app_capture_active()
}

fn player_index<H: TechHost + ?Sized>(h: &mut H) -> i32 {
    h.value(EE_TECH_HOST_PLAYER_INDEX, -1)
}

/// `EETechCaptureNode`: the node's state with its tree's.
fn node_state<H: TechHost + ?Sized>(h: &mut H, r: &mut Bytes, t: Tree, n: EeTechNodeRef) {
    let count = h.epoch_count();
    // SAFETY: live records (the walks' contract); copied out
    let b = unsafe { node_bytes(&*n.node, n.data.as_ref(), t.0.as_ref(), count as c_long) };
    r.0.extend_from_slice(&b.0);
}

/// `EETechCaptureEpoch`: the node's state, then the epoch's counters.
fn epoch_state<H: TechHost + ?Sized>(h: &mut H, r: &mut Bytes, t: Tree, e: EeTechNodeRef) {
    node_state(h, r, t, e);
    // SAFETY: a live record
    let f = unsafe { *e.node };
    r.i32(f.mRequiredSOL as i64).i32(f.mCurrentSOL as i64).u32(f.mIsCountingSOL as u32);
}

/// `EETechCaptureTree`: the tree's state with its player index.
fn tree_state(r: &mut Bytes, t: Tree, player: i32) {
    // SAFETY: a live record
    let b = tree_bytes(unsafe { &*t.0 }, player as c_long);
    r.0.extend_from_slice(&b.0);
}

fn is_epoch<H: TechHost + ?Sized>(h: &mut H, n: EeTechNodeRef) -> bool {
    // SAFETY: a node of a loaded tree has its data
    let id = unsafe { (*n.data).mID } as i32;
    tree::is_epoch_id(id, h.epoch_count())
}

/// The virtual `ResearchNode`: a node's (TNRS 0) or an epoch's, which researches its own id (TNRS 1).
pub fn research_node<H: TechHost + ?Sized>(h: &mut H, t: Tree, n: EeTechNodeRef, unit: *mut c_void) -> i32 {
    if is_epoch(h, n) {
        // SAFETY: as is_epoch
        let id = unsafe { (*n.data).mID } as i32;
        return research_epoch(h, t, n, id, unit);
    }
    if !active() {
        return node_research(h, n, unit);
    }
    let mut r = Bytes::default();
    r.u32(0).i32(player_index(h) as i64);
    node_state(h, &mut r, t, n);
    let result = node_research(h, n, unit);
    r.i32(result as i64);
    node_state(h, &mut r, t, n);
    capture::commit(tag(b"TNRS"), &r.0);
    result
}

/// `EETechTreeEpoch::Research(id, unit)` (TNRS 1).
pub fn research_epoch<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef, id: i32, unit: *mut c_void) -> i32 {
    if !active() {
        return epoch_research(h, t, e, id, unit);
    }
    let mut r = Bytes::default();
    r.u32(1).i32(player_index(h) as i64).i32(id as i64);
    epoch_state(h, &mut r, t, e);
    let result = epoch_research(h, t, e, id, unit);
    r.i32(result as i64);
    epoch_state(h, &mut r, t, e);
    capture::commit(tag(b"TNRS"), &r.0);
    result
}

/// `EETechTreeNode::SetPrerequisite(id)` (TNPR).
pub fn set_prerequisite<H: TechHost + ?Sized>(h: &mut H, t: Tree, n: EeTechNodeRef, id: i32) {
    if !active() {
        node_set_prerequisite(h, n, id);
        return;
    }
    let mut r = Bytes::default();
    r.i32(player_index(h) as i64);
    node_state(h, &mut r, t, n);
    r.i32(id as i64);
    node_set_prerequisite(h, n, id);
    node_state(h, &mut r, t, n);
    capture::commit(tag(b"TNPR"), &r.0);
}

fn counter_step<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef, kind: u32) {
    let body = |h: &mut H| {
        if kind == 0 {
            epoch_add_sol(h, t, e)
        } else {
            epoch_update_availability(h, t, e)
        }
    };
    if !active() {
        body(h);
        return;
    }
    let mut r = Bytes::default();
    r.u32(kind).i32(player_index(h) as i64);
    epoch_state(h, &mut r, t, e);
    body(h);
    epoch_state(h, &mut r, t, e);
    capture::commit(tag(b"TSOL"), &r.0);
}

/// `EETechTreeEpoch::AddSOL` (TSOL 0).
pub fn add_sol<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    counter_step(h, t, e, 0);
}

/// `EETechTreeEpoch::UpdateAvailability` (TSOL 1).
pub fn update_availability<H: TechHost + ?Sized>(h: &mut H, t: Tree, e: EeTechNodeRef) {
    counter_step(h, t, e, 1);
}

/// `EETechTree::Research(id, unit)` (TTRS): the next epoch's counter before and after.
pub fn research<H: TechHost + ?Sized>(h: &mut H, t: Tree, id: i32, unit: *mut c_void) {
    if !active() {
        tree_research(h, t, id, unit);
        return;
    }
    // the next epoch as it was before the research (the C++ computed it once)
    // SAFETY: a live record
    let next = unsafe { (*t.0).mCurrentEpoch } as i32 + 1;
    let next_sol = |h: &mut H| {
        if next < h.epochs() {
            let e = h.epoch(next);
            // SAFETY: a live record
            unsafe { (*e.node).mCurrentSOL as i32 }
        } else {
            -1
        }
    };
    let mut io = EeTechHostIo { unit, ..Default::default() };
    io.v[0] = -1;
    h.call(EE_TECH_HOST_UNIT_ID, &mut io);
    let player = player_index(h);
    let mut r = Bytes::default();
    r.i32(id as i64).i32(io.v[0] as i64);
    tree_state(&mut r, t, player);
    let before = next_sol(h);
    r.i32(before as i64);
    tree_research(h, t, id, unit);
    tree_state(&mut r, t, player);
    let after = next_sol(h);
    r.i32(after as i64);
    capture::commit(tag(b"TTRS"), &r.0);
}

/// TEPO: `SetEpoch` (`what` 0), `ResearchToEpoch` (1), `SetEpochRange` (2, with `end`), before their walk.
pub fn epochs_record<H: TechHost + ?Sized>(h: &mut H, t: Tree, what: i32, epoch: i32, end: i32) {
    if !active() {
        return;
    }
    let mut r = Bytes::default();
    r.u32(what as u32).i32(epoch as i64);
    if what == 2 {
        r.i32(end as i64);
    }
    tree_state(&mut r, t, player_index(h));
    capture::commit(tag(b"TEPO"), &r.0);
}

/// TRTE: the record of the ResearchToEpoch whose steps were collected: their head, the tree, every epoch.
pub fn research_to_epoch_record<H: TechHost + ?Sized>(h: &mut H, t: Tree, epoch: i32, local: bool) {
    if !active() {
        return;
    }
    let epochs = h.epochs();
    // SAFETY: a null buffer asks the size; then a buffer of that size
    let size = unsafe {
        capture_rec::ee_tech_capture_research_to_epoch(epoch as c_long, epochs as c_long, local, std::ptr::null_mut())
    };
    let mut r = Bytes(vec![0u8; size as usize]);
    // SAFETY: as above
    unsafe {
        capture_rec::ee_tech_capture_research_to_epoch(epoch as c_long, epochs as c_long, local, r.0.as_mut_ptr())
    };
    tree_state(&mut r, t, player_index(h));
    for i in 0..epochs {
        let e = h.epoch(i);
        epoch_state(h, &mut r, t, e);
    }
    capture::commit(tag(b"TRTE"), &r.0);
}

/// TNWK / TNMT: the walk's record (its head with the steps taken off, the node's state, the tree's).
#[allow(clippy::too_many_arguments)]
pub fn walk_record(
    kind: i32,
    player: i32,
    epoch_count: i32,
    tree: Option<&EeTechTreeFields>,
    node: Option<(&EeTechNodeFields, Option<&EeTechNodeDataFields>, Option<&EeTechTreeFields>)>,
    a: i32,
    b: i32,
    result: i32,
) {
    if !active() {
        return;
    }
    let mut size = 0u32;
    let head = capture_rec::ee_tech_capture_walk(
        kind as c_long,
        if tree.is_some() { player as c_long } else { -1 },
        a as c_long,
        b as c_long,
        result as c_long,
        &mut size,
    );
    // SAFETY: the buffer and size ee_tech_capture_walk returned
    let mut r = Bytes(unsafe { std::slice::from_raw_parts(head, size as usize) }.to_vec());
    // SAFETY: as above
    unsafe { capture_rec::ee_tech_capture_free(head, size) };
    if let Some((n, d, nt)) = node {
        r.0.extend_from_slice(&node_bytes(n, d, nt, epoch_count as c_long).0);
    }
    if let Some(t) = tree {
        r.0.extend_from_slice(&tree_bytes(t, player as c_long).0);
    }
    capture::commit(capture_rec::ee_tech_capture_walk_tag(kind as c_long), &r.0);
}

/// The host's TNWK walk record (`node` may be empty; the tree's state when `with_tree`).
pub fn walk<H: TechHost + ?Sized>(h: &mut H, t: Tree, io: &EeTechHostIo) {
    if !active() {
        return;
    }
    let [kind, a, b, result, with_tree] = io.v;
    let player = player_index(h);
    let count = h.epoch_count();
    // SAFETY: live records or null
    let (tree, node) = unsafe {
        let tree = if with_tree != 0 { t.0.as_ref() } else { None };
        let node =
            if io.node.obj.is_null() { None } else { Some((&*io.node.node, io.node.data.as_ref(), t.0.as_ref())) };
        (tree, node)
    };
    walk_record(kind, player, count, tree, node, a, b, result);
}

/// TNWK / TNMT of a walk the C++ asks for (a node's NeedsUnitMedia, kind 10; an epoch's GetBuildTime, 6): `node` and
/// `data` may be null (no node's state), `tree` too (no tree's state, player -1).
///
/// # Safety
/// Each pointer is live or null.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_tech_capture_node_walk(
    kind: i32,
    tree: *const EeTechTreeFields,
    player: i32,
    node: *const EeTechNodeFields,
    data: *const EeTechNodeDataFields,
    node_tree: *const EeTechTreeFields,
    a: i32,
    b: i32,
    result: i32,
) {
    // SAFETY: the caller's contract
    let (tree, node) = unsafe { (tree.as_ref(), node.as_ref().map(|n| (n, data.as_ref(), node_tree.as_ref()))) };
    walk_record(kind, player, crate::tech::tech_ids::epoch_count(), tree, node, a, b, result);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tech::nodes::EE_TECH_HOST_EPOCH_COUNT;

    struct Host {
        log: Vec<i32>,
    }

    impl TechHost for Host {
        fn call(&mut self, op: i32, io: &mut EeTechHostIo) -> i32 {
            self.log.push(op);
            match op {
                EE_TECH_HOST_EPOCH_COUNT => io.v[0] = 3,
                EE_TECH_HOST_PLAYER_INDEX => io.v[0] = 2,
                _ => {}
            }
            0
        }
    }

    // without a capture (tests never set EE_SIM_CAPTURE) each wrapper is its body alone
    #[test]
    fn uncaptured_is_the_body() {
        let mut node = crate::tech::nodes::node_fields_new();
        let mut data = EeTechNodeDataFields { mID: 1000, mKind: 3, ..Default::default() };
        let mut fields = EeTechTreeFields::default();
        let n = EeTechNodeRef { obj: std::ptr::null_mut(), node: &mut node, data: &mut data };
        let t = Tree(&mut fields);
        let mut h = Host { log: Vec::new() };
        node.mIsAvailable = true;
        assert_eq!(research_node(&mut h, t, n, std::ptr::null_mut()), tree::RESULT_RESEARCHED);
        assert!(node.mIsResearched);
        assert!(!h.log.contains(&EE_TECH_HOST_PLAYER_INDEX));
        epochs_record(&mut h, t, 2, 1, 4);
        research_to_epoch_record(&mut h, t, 1, false);
        assert!(!h.log.contains(&EE_TECH_HOST_PLAYER_INDEX));
    }

    #[test]
    fn epoch_ids_research_as_epochs() {
        let mut node = crate::tech::nodes::node_fields_new();
        let mut data = EeTechNodeDataFields { mID: 1, ..Default::default() };
        let n = EeTechNodeRef { obj: std::ptr::null_mut(), node: &mut node, data: &mut data };
        let mut h = Host { log: Vec::new() };
        assert!(is_epoch(&mut h, n));
        // SAFETY: the test's record
        unsafe { (*n.data).mID = 1001 };
        assert!(!is_epoch(&mut h, n));
    }
}
