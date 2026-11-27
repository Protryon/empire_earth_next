//! The scenario editor's tech tree statements answered in Rust (player/tech lane, round 4): the trees are Rust records
//! (ee_sim tech nodes.rs), so a player's current, start and end epochs and the epoch range reset the editor's hosts
//! asked of EESERBUnitTypes.cpp and EESEARust.cpp are read and walked here before the C++ is asked. None: the C++
//! answers.

#![cfg_attr(test, allow(dead_code))]
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use crate::screens::editor_players::EE_SCREENS_SEPL_OP_EPOCH_RANGE;
use crate::screens::editor_seax::EE_SCREENS_SEAX_PLAYER_START_EPOCH;
use crate::screens::editor_sepl::{
    EE_SCREENS_SEPX_CURRENT_EPOCH, EE_SCREENS_SEPX_END_EPOCH, EE_SCREENS_SEPX_START_EPOCH,
};
use crate::tech_tree::objects as to;
use ee_sim::tech::nodes::{EeTechTreeFields, EeTechTreeOp};

/// `player->GetTechTree()`'s record.
fn tree<'a>(player: usize) -> &'a EeTechTreeFields {
    // SAFETY: a live player's tree (the C++ statement dereferenced it)
    unsafe { &*to::tree_fields(to::player_tree(player as _)) }
}

/// The players' statement `op` of `ee_screenscpp_sepx` (player `i0`).
pub(crate) fn sepx(op: u32, i: &[i32; 4]) -> Option<i32> {
    let t = || tree(to::world_player(i64::from(i[0])) as usize);
    Some(match op {
        EE_SCREENS_SEPX_CURRENT_EPOCH => t().mCurrentEpoch as i32,
        EE_SCREENS_SEPX_END_EPOCH => t().mEndEpoch as i32,
        EE_SCREENS_SEPX_START_EPOCH => t().mStartEpoch as i32,
        _ => return None,
    })
}

/// The players' statement `op` of `ee_screenscpp_sepl`: player i0's epoch range reset to i1..i2.
pub(crate) fn sepl(op: u32, i: &[i32; 4]) -> Option<i32> {
    (op == EE_SCREENS_SEPL_OP_EPOCH_RANGE).then(|| {
        let player = to::world_player(i64::from(i[0]));
        to::tree_walk(to::player_tree(player), EeTechTreeOp::ResetEpochRange, i[1], i[2]);
        0
    })
}

/// The actions' statement `op` of `ee_screenscpp_seax` (`p` the player).
pub(crate) fn seax(op: u32, p: usize) -> Option<isize> {
    (op == EE_SCREENS_SEAX_PLAYER_START_EPOCH).then(|| tree(p).mStartEpoch as i32 as isize)
}
