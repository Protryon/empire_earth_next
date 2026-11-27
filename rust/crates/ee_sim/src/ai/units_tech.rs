//! The classic_lua hosts' tech tree operations whose bodies are Rust (player/tech lane, round 3; were
//! EECPNextUnits.cpp's and EECPNext.cpp's cases): the player's tree and a node of it, a node's data, cost table,
//! event, availability and obsolescence, the epoch ids. The trees and nodes are Rust records (ee_sim tech
//! tree_objects.rs); the computer player is its EEPlayer base (the first).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_void;
use std::os::raw::c_long;

use super::case_ops::{EE_AI_UNITS_HOST_NODE_COST_AT, EE_AI_UNITS_HOST_NODE_HAS_DATA, EE_AI_UNITS_HOST_NODE_OF};
use super::more::{
    EE_AI_UNITS_HOST_EPOCH_COUNT, EE_AI_UNITS_HOST_IS_EPOCH_ID, EE_AI_UNITS_HOST_NODE_AVAILABLE,
    EE_AI_UNITS_HOST_NODE_OBSOLETE,
};
use super::world_ops::EE_AI_UNITS_HOST_NODE_EVENT;
use super::{EE_AI_UNITS_HOST_HAS_NODE, EE_AI_UNITS_HOST_HAS_TREE};
use crate::ai::next_game::{EE_AI_NEXT_HOST_EPOCH_COUNT, EE_AI_NEXT_HOST_NODE_TEST};
use crate::tech::nodes::EeTechNodeOp;
use crate::tech::tree_objects as to;

/// The player's node of id `id` (0: none).
///
/// # Safety
/// `me` is a live player with its tree.
unsafe fn node_of(me: *mut c_void, id: i64) -> usize {
    // SAFETY: the contract
    unsafe { to::tree_get(to::player_tree(me as usize) as *mut _, id as c_long) as usize }
}

/// The node's walk answer (`GetIsAvailable`, `GetIsObsolete`).
///
/// # Safety
/// `n` is a live node of a live tree.
unsafe fn test(n: usize, obsolete: bool) -> i64 {
    // SAFETY: the contract
    unsafe {
        i64::from(if obsolete {
            to::is_obsolete(n)
        } else {
            to::walk_node(n, EeTechNodeOp::IsAvailable, 0, false, 0) != 0
        })
    }
}

/// The units host's operation `op`, `None` for the others.
///
/// # Safety
/// As the host's: the operation's objects are live.
pub(super) unsafe fn op(op: i32, me: *mut c_void, a: i64, b: i64) -> Option<i64> {
    crate::world::layout::get()?;
    let n = a as usize;
    // SAFETY (every arm): the contract (the C++ case dereferenced the same objects)
    unsafe {
        Some(match op {
            EE_AI_UNITS_HOST_HAS_NODE => i64::from(node_of(me, a) != 0),
            // player a's mUnitClassUpgrades: its size, its element b (player_upgrades.rs: the vector read in place)
            super::case_ops::EE_AI_UNITS_HOST_UPGRADE_COUNT => crate::units::player_upgrades::slots(a as u64) as i64,
            super::case_ops::EE_AI_UNITS_HOST_UPGRADE_AT => {
                crate::units::player_upgrades::at(a as u64, b as usize) as i64
            }
            EE_AI_UNITS_HOST_HAS_TREE => i64::from(to::player_tree(me as usize) != 0),
            EE_AI_UNITS_HOST_NODE_OF => node_of(me, a) as i64,
            EE_AI_UNITS_HOST_NODE_AVAILABLE => test(n, false),
            EE_AI_UNITS_HOST_NODE_OBSOLETE => test(n, true),
            EE_AI_UNITS_HOST_IS_EPOCH_ID => i64::from(crate::tech::tech_ids::is_epoch_id(a as c_long as i32)),
            EE_AI_UNITS_HOST_EPOCH_COUNT => crate::tech::tech_ids::epoch_count() as i64,
            EE_AI_UNITS_HOST_NODE_HAS_DATA => i64::from(!(*to::node(n)).mData.is_null()),
            EE_AI_UNITS_HOST_NODE_COST_AT => (*(*to::node(n)).mData).mCosts[(2 + b as c_long) as usize] as i64,
            EE_AI_UNITS_HOST_NODE_EVENT => (*(*to::node(n)).mData).mResearchedEvent as i64,
            _ => return None,
        })
    }
}

/// The next host's operation `op`, `None` for the others.
///
/// # Safety
/// As [`op`].
pub(crate) unsafe fn next(op: i32, a: i64, b: i64) -> Option<i64> {
    crate::world::layout::get()?;
    // SAFETY: the contract
    unsafe {
        Some(match op {
            EE_AI_NEXT_HOST_NODE_TEST => test(a as usize, b != 0),
            EE_AI_NEXT_HOST_EPOCH_COUNT => crate::tech::tech_ids::epoch_count() as i64,
            _ => return None,
        })
    }
}
