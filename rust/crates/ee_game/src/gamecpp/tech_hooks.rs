//! The tech tree's capture hooks' C++ bodies in Rust (were EETechCapture.cpp's node, epoch and tree state records
//! and the EVUN unit step): the Rust records (ee_sim tech capture_rec.rs) over the nodes', trees' and units' members,
//! each a one-statement case (EETechTree.cpp's and EETechTreeNode.cpp's `Tk` cases, EETechCapture.cpp's `C11Tc`).
//! TEST ONLY (the simulation capture; off, the hooks are one cached test).

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_sim::app::capture::ee_app_capture_active;
use ee_sim::tech::capture_rec::*;
use ee_sim::tech::nodes::{EeTechNodeDataFields, EeTechNodeFields, EeTechTreeFields};
use ee_sim::tech::tech_ids::ee_tech_epoch_count;

use super::{h, hp, pa, O, P};

/// The record's bytes (`EESimCaptureRecord::Bytes`).
fn bytes(record: P, out: &[u8], n: u32) {
    h(O::TcBytes, record, &[pa(out.as_ptr() as P), n as i64]);
}

/// EVUN's unit flags: complex, built, on a tile, a multi-part unit's later part.
pub fn unit_flags(complex: bool, built: bool, tile_node: bool, previous_part: bool) -> u32 {
    complex as u32 | ((complex && built) as u32) << 1 | (tile_node as u32) << 2 | ((complex && previous_part) as u32) << 3
}

/// A node's state record (TNRS, TNPR, ...: id, epochs, flags, delay, prerequisites, the tree's epochs).
///
/// # Safety
/// `record` is the live capture record, `node` a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_capture_node_rec(record: *mut c_void, node: *mut c_void) {
    let n = node as P;
    let mut out = [0u8; EE_TECH_CAPTURE_NODE_SIZE as usize];
    let tree = hp(O::TkNodeTree, n, &[]);
    let size = ee_tech_capture_node(
        hp(O::TkNodeFields, n, &[]) as *const EeTechNodeFields,
        hp(O::TkNodeData, n, &[]) as *const EeTechNodeDataFields,
        if tree == 0 { std::ptr::null() } else { hp(O::TkTreeFields, tree, &[]) as *const EeTechTreeFields },
        ee_tech_epoch_count() as c_long,
        out.as_mut_ptr(),
    );
    bytes(record as P, &out, size);
}

/// An epoch's state record: its node's, then its SOL counters.
///
/// # Safety
/// As [`ee_gamecpp_tech_capture_node_rec`] (`epoch` a live epoch).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_capture_epoch_rec(record: *mut c_void, epoch: *mut c_void) {
    ee_gamecpp_tech_capture_node_rec(record, epoch);
    let mut out = [0u8; EE_TECH_CAPTURE_EPOCH_SIZE as usize];
    let size = ee_tech_capture_epoch(hp(O::TkNodeFields, epoch as P, &[]) as *const EeTechNodeFields, out.as_mut_ptr());
    bytes(record as P, &out, size);
}

/// A tree's state record (its epochs, the player).
///
/// # Safety
/// `record` is the live capture record, `tree` a live tree.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_capture_tree_rec(record: *mut c_void, tree: *mut c_void) {
    let t = tree as P;
    let mut out = [0u8; EE_TECH_CAPTURE_TREE_SIZE as usize];
    let player = h(O::TkPlayerIndex, t, &[]) as c_long;
    let size = ee_tech_capture_tree(hp(O::TkTreeFields, t, &[]) as *const EeTechTreeFields, player, out.as_mut_ptr());
    bytes(record as P, &out, size);
}

/// EVUN: one unit an effect walks (its id, type, hit points, flags, flight time), when capturing.
///
/// # Safety
/// `unit` is a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_tech_capture_unit_step(unit: *mut c_void, applies: bool, value: c_long) {
    if !ee_app_capture_active() {
        return;
    }
    let u = unit as P;
    let complex = hp(O::C11TcUnitComplex, u, &[]);
    let (built, previous, flight) = if complex != 0 {
        (
            h(O::C11TcComplexBuilt, complex, &[]) != 0,
            h(O::C11TcComplexPrevious, complex, &[]) != 0,
            h(O::C11TcComplexFlightTime, complex, &[]) as c_long,
        )
    } else {
        (false, false, 0)
    };
    let flags = unit_flags(complex != 0, built, h(O::C11TcUnitTileNode, u, &[]) != 0, previous);
    ee_tech_capture_unit_step(
        h(O::C11TcUnitId, u, &[]) as c_ulong,
        h(O::C11TcUnitType, u, &[]) as c_long,
        h(O::C11TcUnitHitPoints, u, &[]) as c_long,
        flags,
        flight,
        applies,
        value,
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_unit_flags_are_the_cpp_bits() {
        assert_eq!(unit_flags(false, true, false, true), 0);
        assert_eq!(unit_flags(true, true, false, false), 3);
        assert_eq!(unit_flags(false, false, true, false), 4);
        assert_eq!(unit_flags(true, false, true, true), 13);
    }
}
