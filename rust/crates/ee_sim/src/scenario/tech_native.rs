//! The scenario hosts' tech tree statements answered in Rust (player/tech lane, round 3): the trees, epochs and
//! nodes are Rust records (ee_sim tech tree_objects.rs), so the reads and writes of their members, the lookups and the
//! walks the event, read and effect-run hosts made in C++ (EEEvent.cpp, EEScenarioHost.cpp, EESMEffect.cpp) are made
//! here before the C++ is asked. None: the C++ answers.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::os::raw::c_long;

use crate::scenario::event::{
    EE_SCENARIO_EV_IS_LATEST, EE_SCENARIO_EV_NODE_DELAY, EE_SCENARIO_EV_NODE_KIND, EE_SCENARIO_EV_NODE_UNIT_CLASS,
    EE_SCENARIO_EV_RESEARCH_NODE, EE_SCENARIO_EV_TREE_NODE, EE_SCENARIO_EV_TREE_SET_MEDIA,
};
use crate::scenario::host::EE_SCENARIO_READ_EPOCH;
use crate::scenario::read_compose::{
    EE_SCENARIO_RX_NODE_AVAILABLE, EE_SCENARIO_RX_NODE_MAKES_UNIT, EE_SCENARIO_RX_NODE_RESEARCHED,
    EE_SCENARIO_RX_NODE_SET_RESEARCHED, EE_SCENARIO_RX_TECH_NODE,
};
use crate::scenario::world_compose::*;
use crate::tech::nodes::{EeTechNodeFields, EeTechNodeOp, EeTechTreeFields, EeTechTreeOp};
use crate::tech::tree_objects as to;

fn tree<'a>(t: usize) -> &'a mut EeTechTreeFields {
    // SAFETY: a live player's tree (the C++ statement dereferenced it)
    unsafe { &mut *(t as *mut EeTechTreeFields) }
}

fn node<'a>(n: usize) -> &'a mut EeTechNodeFields {
    // SAFETY: a live node (as above)
    unsafe { &mut *to::node(n) }
}

fn data_of(n: usize) -> crate::tech::nodes::EeTechNodeDataFields {
    // SAFETY: a node with its data (as above)
    unsafe { *node(n).mData }
}

fn get(t: usize, id: i64) -> usize {
    // SAFETY: a live tree
    unsafe { to::tree_get(t as *mut _, id as c_long) as usize }
}

fn walk(n: usize, op: EeTechNodeOp, a: i32, b: bool) -> i64 {
    // SAFETY: a live node of a live tree
    unsafe { to::walk_node(n, op, a, b, 0) as i64 }
}

fn walk_tree(t: usize, op: EeTechTreeOp, a: i32, b: i32) {
    // SAFETY: a live tree
    unsafe { to::walk_tree(t, op, a, b, 0) };
}

fn player_tree(p: usize) -> usize {
    // SAFETY: a live player (as above)
    unsafe { to::player_tree(p) }
}

/// The event host's statement `op` (EEEvent.cpp; `a` its arguments).
pub fn event(op: u32, a: &[i64; 4]) -> Option<i64> {
    let p = a[0] as usize;
    Some(match op {
        EE_SCENARIO_EV_TREE_NODE => get(player_tree(p), a[1]) as i64,
        EE_SCENARIO_EV_NODE_UNIT_CLASS => data_of(p).mUnitClass as i64,
        EE_SCENARIO_EV_NODE_KIND => data_of(p).mKind as i64,
        EE_SCENARIO_EV_NODE_DELAY => {
            let n = node(p);
            let v = n.mResearchDelay;
            n.mResearchDelay = v.wrapping_add(1);
            v as i64
        }
        EE_SCENARIO_EV_RESEARCH_NODE => walk(p, EeTechNodeOp::Research, 0, false),
        EE_SCENARIO_EV_TREE_SET_MEDIA => {
            let t = tree(player_tree(p));
            t.mMediaID = a[1] as c_long;
            t.mMediaID as i64
        }
        EE_SCENARIO_EV_IS_LATEST => {
            let t = player_tree(p);
            let host = crate::world::layout::get()?.tech_host as usize;
            // SAFETY: the registered address of `EETechHost`, the walks' host; a live tree
            i64::from(unsafe {
                crate::tech::nodes::ee_tech_is_latest_effect(
                    t as *mut _,
                    std::mem::transmute::<usize, crate::tech::nodes::EeTechHostFn>(host),
                    t as *mut _,
                    a[1] as i32,
                    a[2] as i32,
                    a[3] as i32,
                )
            })
        }
        _ => return None,
    })
}

/// The host read `op` of player `a` (EEScenarioHost.cpp's `ee_scenariocpp_host_read`).
pub fn read(op: u32, a: i32) -> Option<i64> {
    match op {
        EE_SCENARIO_READ_EPOCH => {
            let p = if (0..17).contains(&a) { to::world_player(a as i64) } else { 0 };
            Some(tree(player_tree(p)).mCurrentEpoch as i64)
        }
        _ => None,
    }
}

/// The world statement `op` (`args` its arguments; `effect_tech` the run's effect's technology) of the read and the
/// effect run hosts.
pub fn world(op: u32, args: &[i64], effect_tech: &mut dyn FnMut() -> i64) -> Option<i64> {
    let a0 = args.first().copied().unwrap_or(0);
    match op {
        EE_SCENARIO_RX_TECH_NODE => {
            let p = if (0..17).contains(&(a0 as i32)) { to::world_player(a0 as i32 as i64) } else { 0 };
            return Some(get(player_tree(p), args[1] as c_long as i64) as i64);
        }
        EE_SCENARIO_RX_NODE_MAKES_UNIT => return Some(i64::from(data_of(a0 as usize).mMakesUnit)),
        EE_SCENARIO_RX_NODE_RESEARCHED => return Some(i64::from(node(a0 as usize).mIsResearched)),
        EE_SCENARIO_RX_NODE_SET_RESEARCHED => {
            node(a0 as usize).mIsResearched = args[1] != 0;
            return Some(i64::from(args[1] != 0));
        }
        EE_SCENARIO_RX_NODE_AVAILABLE => return Some(walk(a0 as usize, EeTechNodeOp::IsAvailable, 0, false)),
        _ => {}
    }
    // the effect run's statements on player a0's tree (SMEPlayer: gWorld.GetPlayer) and the effect's technology
    let tree_ops = [
        EE_SCENARIO_WX_TECH_MAKES_UNIT,
        EE_SCENARIO_WX_TECH_OBSOLETE,
        EE_SCENARIO_WX_TREE_END_EPOCH,
        EE_SCENARIO_WX_TREE_RESET_RANGE,
        EE_SCENARIO_WX_TECH_MAKE_AVAILABLE,
        EE_SCENARIO_WX_TECH_EPOCH_ZERO,
        EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH,
        EE_SCENARIO_WX_TECH_SCEN_OBSOLETED,
        EE_SCENARIO_WX_TECH_SET_OBSOLETE,
        EE_SCENARIO_WX_TECH_MEDIA_LOADED,
        EE_SCENARIO_WX_TECH_KIND,
        EE_SCENARIO_WX_TREE_WONDERS_ZERO,
        EE_SCENARIO_WX_TECH_RESEARCH,
    ];
    if !tree_ops.contains(&op) {
        return None;
    }
    let t = player_tree(to::world_player(a0));
    match op {
        EE_SCENARIO_WX_TREE_END_EPOCH => return Some(tree(t).mEndEpoch as i64),
        EE_SCENARIO_WX_TREE_WONDERS_ZERO => {
            tree(t).mEpochsBeforeWonders = 0;
            return Some(0);
        }
        _ => {}
    }
    let tech = effect_tech() as c_long;
    let n = || get(t, tech as i64);
    Some(match op {
        EE_SCENARIO_WX_TECH_MAKES_UNIT => i64::from(data_of(n()).mMakesUnit),
        EE_SCENARIO_WX_TECH_OBSOLETE => {
            walk_tree(t, EeTechTreeOp::MakeObsolete, tech as i32, 0);
            0
        }
        EE_SCENARIO_WX_TREE_RESET_RANGE => {
            let start = tree(t).mStartEpoch;
            walk_tree(t, EeTechTreeOp::ResetEpochRange, start as i32, tech as i32);
            0
        }
        EE_SCENARIO_WX_TECH_MAKE_AVAILABLE => {
            walk(n(), EeTechNodeOp::MakeAvailable, 0, false);
            0
        }
        EE_SCENARIO_WX_TECH_EPOCH_ZERO => {
            node(n()).mEpoch = 0;
            0
        }
        EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH => {
            let count = crate::tech::tech_ids::epoch_count() as c_long;
            node(n()).mObsoleteEpoch = count;
            count as i64
        }
        EE_SCENARIO_WX_TECH_SCEN_OBSOLETED => {
            walk(n(), EeTechNodeOp::SetEditorObsoleted, 0, false);
            0
        }
        EE_SCENARIO_WX_TECH_SET_OBSOLETE => {
            walk(n(), EeTechNodeOp::SetObsolete, 0, false);
            0
        }
        EE_SCENARIO_WX_TECH_MEDIA_LOADED => {
            node(n()).mIsUnitMediaLoaded = true;
            1
        }
        EE_SCENARIO_WX_TECH_KIND => data_of(n()).mKind as i64,
        _ => {
            // EE_SCENARIO_WX_TECH_RESEARCH
            walk_tree(t, EeTechTreeOp::Research, tech as i32, 0);
            0
        }
    })
}
