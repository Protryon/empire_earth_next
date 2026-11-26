//! TEST ONLY: the tech tree's sweep record (TSWP) in Rust (game core lane, round 10; was EETechCapture.cpp's
//! EETechCaptureSweepImpl, NodeIndexesSafe and SumCostsSafe): every five game minutes, per player its tree's state
//! and checksum, and per node its availability, obsolescence, costs at three scales, SumCosts and its prerequisite
//! closure (DependsOn over every id). Reads the world, writes nothing to it. The node's and tree's bytes are ee_sim
//! tech's (capture_rec.rs); each C++ call is a case of EETechTreeEpoch.cpp's `EECore10OpB7` or of round 9's tech host.
//! Also the tree's, the upgrade's and the player's records of the civilization capture (screens civ_capture.rs).

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::collections::BTreeSet;
use std::ffi::{c_long, c_ulong, c_void};

use ee_base::error::checksum::EeErrorChecksum;
use ee_sim::tech::capture_rec::{
    ee_tech_capture_node, ee_tech_capture_tree, EE_TECH_CAPTURE_NODE_SIZE, EE_TECH_CAPTURE_TREE_SIZE,
};
use ee_sim::tech::tech_ids::ee_tech_epoch_count;

use super::persistent::Longs;
use super::{h, hf, hp, O, P};
use crate::tech_tree::host::ee_gamecpp_tech_node_ids;
use crate::tech_tree::load::{
    data_of, ee_gamecpp_tech_sum_costs_captured, node_data, node_fields, tree_fields, TbRecord,
};

/// `kTechEpochPrehistoric`, `kTechNone` (static_assert'd in EETechTreeEpoch.cpp): SumCosts walks the ids between.
const TC_FIRST_ID: c_long = 0;
const TC_NONE: c_long = 100000;

/// The tree's bytes (EETechCaptureTree): its player index and epochs.
fn tree_bytes(r: &mut TbRecord, tree: P) {
    let mut bytes = [0u8; EE_TECH_CAPTURE_TREE_SIZE as usize];
    // SAFETY: the tree's record and a buffer of the record's size
    let n = unsafe {
        ee_tech_capture_tree(tree_fields(tree), crate::tech_tree::load::player_index(tree), bytes.as_mut_ptr())
    };
    r.bytes(&bytes[..n as usize]);
}

/// The node's bytes (EETechCaptureNode): its state and its tree's epochs.
fn node_bytes(r: &mut TbRecord, node: P) {
    let mut bytes = [0u8; EE_TECH_CAPTURE_NODE_SIZE as usize];
    let tree = hp(O::TkNodeTree, node, &[]);
    // SAFETY: the node's, its data's and its tree's records (NULL ones allowed) and a buffer of the record's size
    let n = unsafe {
        ee_tech_capture_node(
            node_fields(node),
            node_data(node),
            tree_fields(tree),
            ee_tech_epoch_count() as c_long,
            bytes.as_mut_ptr(),
        )
    };
    r.bytes(&bytes[..n as usize]);
}

/// Whether GetCost and SumCosts index gUnitClassTable and the upgrades in range for the node (the sweep calls them on
/// every node; the game only on the ones it offers).
fn node_indexes_safe(node: P) -> bool {
    let count = h(O::TkClassCount, 0, &[]) as c_ulong;
    if h(O::TkNodeUnitClass0, node, &[]) as c_ulong >= count {
        return false;
    }
    let d = data_of(node);
    !d.mMakesUnit || (d.mUnitClass as c_ulong) < count
}

/// Whether SumCosts can walk from the id without meeting a missing node (it dereferences every id in 0..99999).
fn sum_costs_safe(tree: P, id: c_long, visiting: &mut BTreeSet<c_long>) -> bool {
    if !(TC_FIRST_ID..TC_NONE).contains(&id) {
        return true;
    }
    let node = hp(O::TkGet, tree, &[id as i64]);
    if node == 0 {
        return false;
    }
    if visiting.contains(&id) {
        return true;
    }
    if !node_indexes_safe(node) {
        return false;
    }
    visiting.insert(id);
    let d = data_of(node);
    let mut safe = sum_costs_safe(tree, d.mPrerequisite1, visiting)
        && sum_costs_safe(tree, d.mPrerequisite2, visiting)
        && sum_costs_safe(tree, d.mPrerequisite3, visiting)
        && sum_costs_safe(tree, d.mPrerequisite4, visiting);
    let class_index = h(O::TkNodeUnitClass0, node, &[]);
    if safe && hp(O::TkClassAt, 0, &[class_index]) != 0 {
        safe = sum_costs_safe(tree, h(O::TkClassTechnology, 0, &[class_index]) as c_long, visiting);
    }
    visiting.remove(&id);
    safe
}

/// TSWP (EETechCaptureSweep): per player (1 on) its tree's head record, then one record per node and epoch.
#[no_mangle]
pub extern "C" fn ee_gamecpp_tech_capture_sweep() {
    let players = h(O::TbWorldPlayers, 0, &[]) as c_ulong;
    for p in 1..players {
        let player = hp(O::TbWorldPlayer, 0, &[p as i64]);
        if player == 0 || hp(O::TbPlayerTree, player, &[]) == 0 {
            continue;
        }
        let tree = hp(O::TbPlayerTree, player, &[]);
        let ids = {
            let list = Longs::new();
            ee_gamecpp_tech_node_ids(tree as *mut c_void, true, 0, list.0 as *mut c_void);
            let mut ids = list.to_vec();
            for e in 0..h(O::TkEpochCount, tree, &[]) {
                ids.push(data_of(hp(O::TkEpochAt, tree, &[e])).mID);
            }
            ids
        };
        let mut head = TbRecord::default();
        head.u32(0);
        tree_bytes(&mut head, tree);
        head.u32(ids.len() as u32);
        let mut checksum = EeErrorChecksum::new(true);
        checksum.add(b"EETechTree", h(O::TbTreeChecksum, tree, &[]) as u32);
        head.u32(checksum.total());
        head.commit(b"TSWP");
        for &id in ids.iter() {
            let node = hp(O::TkGet, tree, &[id as i64]);
            if node == 0 {
                continue;
            }
            let mut r = TbRecord::default();
            r.u32(1);
            r.i32(p as i32);
            node_bytes(&mut r, node);
            r.u32((h(O::TkNodeIsAvailable, node, &[]) != 0) as u32);
            r.u32((h(O::TbNodeIsObsolete, node, &[]) != 0) as u32);
            let class = h(O::TkNodeUnitClass0, node, &[]);
            r.i32(class as i32);
            let safe = node_indexes_safe(node);
            r.u32(safe as u32);
            if safe {
                for resource in 0..7i64 {
                    for scale in [1.0f32, 0.75, 1.5] {
                        r.i32(hf(O::TbNodeGetCost, node, &[resource, class], &[scale]) as i32);
                    }
                }
            }
            let mut visiting = BTreeSet::new();
            if sum_costs_safe(tree, id, &mut visiting) {
                let mut costs: [c_long; 7] = [0; 7];
                // SAFETY: the tree and seven costs
                unsafe {
                    ee_gamecpp_tech_sum_costs_captured(tree as *mut c_void, id, class as c_long, costs.as_mut_ptr())
                };
                r.u32(1);
                for c in costs {
                    r.i32(c as i32);
                }
            } else {
                r.u32(0);
            }
            // the prerequisite closure, one bit per id of the list
            let mut bits: u32 = 0;
            for (j, &other) in ids.iter().enumerate() {
                if h(O::TbNodeDependsOn, node, &[other as i64]) != 0 {
                    bits |= 1u32 << (j % 32);
                }
                if j % 32 == 31 || j + 1 == ids.len() {
                    r.u32(bits);
                    bits = 0;
                }
            }
            r.commit(b"TSWP");
        }
    }
}

/// The player's tree's part of the civilization capture's CVAP (nothing without a tree): the tree's record, then its
/// node ids (GetNodeIDs) and each node's flags (researched, obsolete, available, allowed, 16 for a node; 0 for none).
pub(crate) fn tree_nodes_record(player: P) -> Vec<u8> {
    let mut r = TbRecord::default();
    let tree = hp(O::TbPlayerTree, player, &[]);
    if tree == 0 {
        return r.0;
    }
    tree_bytes(&mut r, tree);
    let list = Longs::new();
    ee_gamecpp_tech_node_ids(tree as *mut c_void, true, 0, list.0 as *mut c_void);
    let ids = list.to_vec();
    r.u32(ids.len() as u32);
    for id in ids {
        let node = hp(O::TkGet, tree, &[id as i64]);
        r.i32(id as i32);
        if node == 0 {
            r.u32(0);
            continue;
        }
        // SAFETY: the live node's record
        let f = unsafe { *node_fields(node) };
        r.u32(
            f.mIsResearched as u32
                | (f.mIsObsolete as u32) << 1
                | (f.mIsAvailable as u32) << 2
                | (f.mIsAllowed as u32) << 3
                | 16,
        );
    }
    r.0
}

/// The upgrade's record (EVAT, CVAP): its numeric block (mAreaEffectScale .. the conversion flags; no pointers), its
/// size first; 0 for no upgrade. mMediaCount and mButtonLoadCount count the presentation's media loads
/// (EEPlayer::LoadUnitClassMedia, SwapClassButtonMedia), which run when the loader gets to them: written as 0, so the
/// record carries simulation state only.
pub(crate) fn upgrade_record(u: P) -> Vec<u8> {
    if u == 0 {
        return 0u32.to_le_bytes().to_vec();
    }
    use std::mem::offset_of;
    type F = ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
    let first = u + ee_sim::world::layout::layout().stats_fields as usize;
    // SAFETY: the upgrade's record from mAreaEffectScale (its first member) up to mIsInitialized
    let mut bytes = unsafe { std::slice::from_raw_parts(first as *const u8, offset_of!(F, mIsInitialized)) }.to_vec();
    for at in [offset_of!(F, mMediaCount), offset_of!(F, mButtonLoadCount)] {
        bytes[at..at + std::mem::size_of::<c_long>()].fill(0);
    }
    let mut out = (bytes.len() as u32).to_le_bytes().to_vec();
    out.extend_from_slice(&bytes);
    out
}

/// The player's record (EVAT, CVAP): the fields the effects change (population bonus, tribute fee, mBB44); -1 for no
/// player.
pub(crate) fn player_record(pl: P) -> Vec<u8> {
    let mut r = TbRecord::default();
    if pl == 0 {
        r.i32(-1);
    } else {
        // SAFETY: the live player's records
        let (rts, f) = unsafe { (&*crate::gamecpp::records::pl_rts(pl), &*crate::gamecpp::records::pl_fields(pl)) };
        r.i32(rts.mPlayerID as i32);
        r.i32(f.mPopulationLimitBonus as i32);
        r.bytes(&f.mTributeFee.to_le_bytes());
        r.u32(f.mBB44 as u32);
    }
    r.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_id_outside_the_tech_range_is_safe_without_a_lookup() {
        let mut visiting = BTreeSet::new();
        assert!(sum_costs_safe(0, -1, &mut visiting));
        assert!(sum_costs_safe(0, TC_NONE, &mut visiting));
    }

    #[test]
    fn the_checksum_of_one_entry_is_its_value() {
        let mut c = EeErrorChecksum::new(true);
        c.add(b"EETechTree", 0xdead_beef);
        assert_eq!(c.total(), 0xdead_beef);
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn a_node_check_asks_the_classes() {
        node_indexes_safe(1);
    }
}
