//! The units hosts' tech tree statements answered in Rust (player/tech lane, round 4): the trees and nodes are Rust
//! records (ee_sim tech tree_objects.rs), so the lookups, reads, availability walks and research the class, complex
//! unit, people, building, air, doppelganger, ghost, unit and goal hosts made in C++ (EEUnitClass.cpp,
//! EEComplexUnit.cpp, EEUnitClassPeople.cpp, EEUnitClassBuildings.cpp, EEUnitClassAir.cpp, EEComplexDoppelganger.cpp,
//! EEGhostUnit.cpp, EEUnitHost.cpp, EEUGRust.cpp) are made here before the C++ is asked. None: the C++ answers.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64
#![cfg_attr(test, allow(dead_code))] // the hooks are the game hosts' (not(test))

use std::os::raw::c_long;
#[cfg(not(test))]
use std::os::raw::c_void;

use crate::tech::nodes::{EeTechNodeDataFields, EeTechNodeOp, EeTechTreeOp};
use crate::tech::tree_objects as to;
use crate::tech_tree_ids::EETechTreeTechID as T;
use crate::world::layout::{self, EeWorldLayout};

/// ee_game's exports (declared in its header)
/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use std::os::raw::{c_long, c_void};
    extern "C" {
        /// the virtual `LoadMedia` / `UnloadMedia` of a node or an epoch (ee_game tech_tree objects.rs)
        pub(super) fn ee_gamecpp_tech_obj_media(node: *mut c_void, player: *mut c_void, load: bool);
        /// the virtual `GetBuildTime` of a node or an epoch (as above)
        pub(super) fn ee_gamecpp_tech_obj_build_time(node: *mut c_void) -> c_long;
    }
}

/// `node->GetBuildTime()`.
fn build_time(n: usize) -> i64 {
    #[cfg(not(test))]
    // SAFETY: a live node
    return unsafe { sys::ee_gamecpp_tech_obj_build_time(n as *mut c_void) } as i64;
    #[cfg(test)]
    {
        let _ = n;
        0
    }
}

/// A node's record.
fn node<'a>(n: usize) -> &'a mut crate::tech::nodes::EeTechNodeFields {
    // SAFETY: a live node (the C++ statement dereferenced it)
    unsafe { &mut *to::node(n) }
}

/// `node->Contains(unit_type)`.
fn contains(n: usize, unit_type: i64) -> bool {
    // SAFETY: the node's data
    unsafe { crate::tech::nodes::ee_tech_node_classes_contains(data(n), unit_type as c_long) }
}

fn l() -> Option<&'static EeWorldLayout> {
    layout::get()
}

/// The pointer member at `object + offset`.
fn ptr_at(object: usize, offset: u64) -> usize {
    // SAFETY: a live object's pointer member (the C++ statement dereferenced it)
    unsafe { *((object + offset as usize) as *const usize) }
}

/// A unit's `mPlayer`.
fn unit_player(u: u64) -> Option<usize> {
    Some(ptr_at(u as usize, l()?.unit_player))
}

/// A unit's `mUnitClass`.
fn unit_class(u: u64) -> Option<usize> {
    Some(ptr_at(u as usize, l()?.unit_class))
}

/// A class's `mTechnology`.
fn class_technology(c: usize) -> Option<i64> {
    let at = c + l()?.unit_class_fields as usize;
    // SAFETY: a live class's record
    Some(unsafe { (*(at as *const crate::world::unit_class_fields::EeUnitsUnitClassFields)).mTechnology } as i64)
}

/// A class's `GetUnitType()`.
fn class_unit_type(c: usize) -> Option<i64> {
    let at = c + l()?.unit_class_fields as usize;
    // SAFETY: as above
    Some(unsafe { (*(at as *const crate::world::unit_class_fields::EeUnitsUnitClassFields)).mUnitType } as i64)
}

/// A class's `mAIKind`.
fn class_ai_kind(c: usize) -> Option<u64> {
    let at = c + l()?.unit_class_fields as usize;
    // SAFETY: as above
    Some(unsafe { (*(at as *const crate::world::unit_class_fields::EeUnitsUnitClassFields)).mAIKind } as u64)
}

/// A complex unit's `mCurrentResearch`.
fn current_research(u: u64) -> Option<i64> {
    let at = u as usize + l()?.complex_unit_fields as usize;
    // SAFETY: a live complex unit's record
    Some(unsafe { (*(at as *const crate::world::unit_fields::EeUnitsComplexUnitFields)).mCurrentResearch } as i64)
}

/// `player->mTechnologies`.
fn tree(player: usize) -> usize {
    // SAFETY: a live player
    unsafe { to::player_tree(player) }
}

/// `tree->Get(id)`.
fn get(t: usize, id: i64) -> usize {
    // SAFETY: a live tree
    unsafe { to::tree_get(t as *mut _, id as c_long) as usize }
}

/// `player->mTechnologies->Get(id)`.
fn node_of(player: usize, id: i64) -> usize {
    get(tree(player), id)
}

/// `node->mData`.
fn data<'a>(n: usize) -> &'a EeTechNodeDataFields {
    // SAFETY: a live node with its data
    unsafe { &*(*to::node(n)).mData }
}

/// `node->GetIsAvailable()`.
fn available(n: usize) -> bool {
    // SAFETY: a live node of a live tree
    unsafe { to::walk_node(n, EeTechNodeOp::IsAvailable, 0, false, 0) != 0 }
}

/// `node->mData->mUnitClass`.
fn node_class(n: usize) -> i64 {
    data(n).mUnitClass as i64
}

/// `gUnitClassTable[i]` (unchecked, as the C++'s `operator[]`).
fn class_table(i: i64) -> Option<usize> {
    let begin = ptr_at(l()?.unit_class_table as usize, 0);
    Some(ptr_at(begin, (i as u64).wrapping_mul(std::mem::size_of::<usize>() as u64)))
}

/// `tree->Research(id, unit)`.
fn research(t: usize, id: i64, unit: usize) {
    // SAFETY: a live tree
    unsafe { to::walk_tree(t, EeTechTreeOp::Research, id as i32, 0, unit) };
}

/// The complex unit host's statement `op` (EEComplexUnit.cpp `ee_units_cu_host`).
pub(crate) fn cu(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::cu_body::{
        EE_UNITS_CU_CLASS_TECH_NODE, EE_UNITS_CU_NODE_AVAILABLE, EE_UNITS_CU_NODE_BUILD_TIME,
        EE_UNITS_CU_NODE_MAKES_UNIT, EE_UNITS_CU_NODE_UNIT_CLASS, EE_UNITS_CU_TECH_NODE,
    };
    use crate::units::cu_cargo::{
        EE_UNITS_CU_CURRENT_NODE, EE_UNITS_CU_NODE_ALLOWED, EE_UNITS_CU_NODE_CLASS_INDEX, EE_UNITS_CU_NODE_KIND,
        EE_UNITS_CU_PLAYER_NODE, EE_UNITS_CU_SET_NODE_ALLOWED,
    };
    use crate::units::cu_misc::EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA;
    use crate::units::cu_world::EE_UNITS_CU_CITIZEN_TECH_HAS_TYPE;
    Some(match op {
        EE_UNITS_CU_TECH_NODE | EE_UNITS_CU_PLAYER_NODE => node_of(unit_player(unit)?, b) as i64,
        EE_UNITS_CU_CLASS_TECH_NODE => node_of(a as usize, class_technology(unit_class(unit)?)?) as i64,
        EE_UNITS_CU_CURRENT_NODE => node_of(unit_player(unit)?, current_research(unit)?) as i64,
        EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA => {
            let player = unit_player(unit)?;
            let n = node_of(player, current_research(unit)?);
            #[cfg(not(test))]
            // SAFETY: a live node and its player
            unsafe {
                sys::ee_gamecpp_tech_obj_media(n as *mut c_void, player as *mut c_void, false)
            };
            #[cfg(test)]
            let _ = (n, player);
            0
        }
        EE_UNITS_CU_CITIZEN_TECH_HAS_TYPE => {
            let n = node_of(unit_player(unit)?, T::kTechCitizen as i64);
            i64::from(contains(n, class_unit_type(unit_class(unit)?)?))
        }
        EE_UNITS_CU_NODE_AVAILABLE => i64::from(available(a as usize)),
        EE_UNITS_CU_NODE_MAKES_UNIT => i64::from(data(a as usize).mMakesUnit),
        EE_UNITS_CU_NODE_UNIT_CLASS | EE_UNITS_CU_NODE_CLASS_INDEX => node_class(a as usize),
        EE_UNITS_CU_NODE_BUILD_TIME => build_time(a as usize),
        EE_UNITS_CU_SET_NODE_ALLOWED => {
            node(a as usize).mIsAllowed = b != 0;
            0
        }
        EE_UNITS_CU_NODE_ALLOWED => i64::from(node(a as usize).mIsAllowed),
        EE_UNITS_CU_NODE_KIND => data(a as usize).mKind as i64,
        // unit a's GetPlayer()->GetUpgrade(its class's unit type) (player_upgrades.rs)
        crate::units::cu_life::EE_UNITS_CU_UNIT_UPGRADE => {
            let ty = class_unit_type(unit_class(a)?)?;
            crate::units::player_upgrades::at(unit_player(a)? as u64, ty as c_long as usize) as i64
        }
        // the unit's GetPlayer()->GetUpgrade(b)->mBuildTimeScale (its bits)
        crate::units::cu_body::EE_UNITS_CU_BUILD_TIME_SCALE => {
            let upgrade = crate::units::player_upgrades::at(unit_player(unit)? as u64, b as c_long as usize);
            i64::from(upgrade_fields(upgrade as usize)?.mBuildTimeScale.to_bits())
        }
        // player a's GetUpgrade(the unit's class's unit type)->mAreaEffectTemplates[b]
        crate::units::cu_life::EE_UNITS_CU_UPGRADE_AREA_TEMPLATE => {
            let ty = class_unit_type(unit_class(unit)?)?;
            let upgrade = crate::units::player_upgrades::at(a, ty as c_long as usize);
            upgrade_fields(upgrade as usize)?.mAreaEffectTemplates[b as usize] as i64
        }
        _ => return None,
    })
}

/// An upgrade's record (`EeUnitsUnitClassUpgradeFields`).
fn upgrade_fields<'a>(upgrade: usize) -> Option<&'a crate::world::stats_fields::EeUnitsUnitClassUpgradeFields> {
    let at = upgrade + l()?.stats_fields as usize;
    // SAFETY: a player's live upgrade (the C++ statement dereferenced it)
    Some(unsafe { &*(at as *const crate::world::stats_fields::EeUnitsUnitClassUpgradeFields) })
}

/// The class host's statement `op` (EEUnitClass.cpp `ee_units_uc_host`; every class kind's host passes these on to
/// it, so every class host's Rust caller asks here first).
pub(crate) fn uc(op: i32, unit: u64, a: u64) -> Option<i64> {
    use crate::units::uc_body::{EE_UNITS_UC_RESEARCH, EE_UNITS_UC_RESEARCH_CLASS, EE_UNITS_UC_RESEARCH_MAKES_UNIT};
    use crate::units::uc_class::EE_UNITS_UC_NODE_EPOCH;
    use crate::units::uc_media::{EE_UNITS_UC_NODE_CALAMITY, EE_UNITS_UC_NODE_KIND, EE_UNITS_UC_TECH_TREE};
    Some(match op {
        EE_UNITS_UC_RESEARCH_MAKES_UNIT => {
            i64::from(data(node_of(unit_player(unit)?, current_research(unit)?)).mMakesUnit)
        }
        EE_UNITS_UC_RESEARCH_CLASS => {
            class_table(node_class(node_of(unit_player(unit)?, current_research(unit)?)))? as i64
        }
        EE_UNITS_UC_RESEARCH => {
            research(tree(unit_player(unit)?), current_research(unit)?, unit as usize);
            0
        }
        EE_UNITS_UC_TECH_TREE => tree(a as usize) as i64,
        EE_UNITS_UC_NODE_EPOCH => node(a as usize).mEpoch as i64,
        EE_UNITS_UC_NODE_KIND => data(a as usize).mKind as i64,
        EE_UNITS_UC_NODE_CALAMITY => data(a as usize).mCalamity as i64,
        _ => return None,
    })
}

/// The people's host's statement `op` (EEUnitClassPeople.cpp `ee_units_ucp_host`), then the class host's.
pub(crate) fn ucp(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::ucp_people::{
        EE_UNITS_UCP_NODE_KIND, EE_UNITS_UCP_NODE_MAKES_UNIT, EE_UNITS_UCP_RESEARCH, EE_UNITS_UCP_RESEARCH_NODE,
    };
    Some(match op {
        EE_UNITS_UCP_RESEARCH => {
            research(tree(unit_player(unit)?), b, unit as usize);
            0
        }
        EE_UNITS_UCP_RESEARCH_NODE => node_of(unit_player(unit)?, current_research(unit)?) as i64,
        EE_UNITS_UCP_NODE_MAKES_UNIT => i64::from(data(a as usize).mMakesUnit),
        EE_UNITS_UCP_NODE_KIND => data(a as usize).mKind as i64,
        _ => return uc(op, unit, a),
    })
}

/// The buildings' host's statement `op` (EEUnitClassBuildings.cpp `ee_units_ucb_host`), then the class host's.
pub(crate) fn ucb(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::ucb_gate::{EE_UNITS_UCB_SPACE_GATES, EE_UNITS_UCB_TECH_CLASS};
    Some(match op {
        EE_UNITS_UCB_TECH_CLASS => class_table(node_class(node_of(a as usize, b)))? as i64,
        EE_UNITS_UCB_SPACE_GATES => i64::from(
            available(node_of(a as usize, T::kTechGateSpace as i64))
                && class_unit_type(b as usize)? != crate::unit_ids::EEUnitTypeID::kUnitTypeGatePalisades as i64,
        ),
        _ => return uc(op, unit, a),
    })
}

/// The fighters of the air host (EEUnitClassAir.cpp `kUcaFighters`).
const UCA_FIGHTERS: [i64; 3] =
    [T::kTechInf15Watchman as i64, T::kTechInf14Guardian as i64, T::kTechInf13Sentinel as i64];

/// The air host's statement `op` (EEUnitClassAir.cpp `ee_units_uca_host`), then the class host's.
pub(crate) fn uca(op: i32, unit: u64, a: u64, b: i64, c: i64) -> Option<i64> {
    use crate::units::uca_air::{EE_UNITS_UCA_FIGHTER_CLASS, EE_UNITS_UCA_FIGHTER_USABLE};
    Some(match op {
        EE_UNITS_UCA_FIGHTER_USABLE => {
            let n = node_of(to::world_player(b), *UCA_FIGHTERS.get(c as usize)?);
            // SAFETY: a live node
            i64::from(available(n) && unsafe { (*to::node(n)).mIsAllowed })
        }
        EE_UNITS_UCA_FIGHTER_CLASS => node_class(node_of(to::world_player(b), *UCA_FIGHTERS.get(c as usize)?)),
        _ => return uc(op, unit, a),
    })
}

/// The doppelganger host's statement `op` (EEComplexDoppelganger.cpp `ee_units_dop_host`).
pub(crate) fn dop(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::dop_repair::{
        EE_UNITS_DOP_NODE_CLASS_MISSING, EE_UNITS_DOP_NODE_DISALLOW, EE_UNITS_DOP_NODE_KIND, EE_UNITS_DOP_TECH_NODE,
    };
    Some(match op {
        EE_UNITS_DOP_TECH_NODE => node_of(unit_player(unit)?, b) as i64,
        EE_UNITS_DOP_NODE_CLASS_MISSING => {
            let c = node_class(a as usize);
            i64::from(c < 0 || class_table(c)? == 0)
        }
        EE_UNITS_DOP_NODE_DISALLOW => {
            node(a as usize).mIsAllowed = false;
            0
        }
        EE_UNITS_DOP_NODE_KIND => data(a as usize).mKind as i64,
        _ => return None,
    })
}

/// The active group host's statement `op` (EEActiveGroup.cpp `ee_units_ag_host`).
#[cfg(not(test))]
pub(crate) fn ag(op: i32, a: u64, b: u64) -> Option<i64> {
    (op == crate::units::ag_host::EE_UNITS_AG_NODE_HAS_TYPE)
        .then(|| Some(i64::from(contains(a as usize, class_unit_type(unit_class(b)?)?))))?
}

/// The ghost host's statement `op` (EEGhostUnit.cpp `ee_units_ghost_host`).
pub(crate) fn ghost(op: i32, a: u64, b: i64) -> Option<i64> {
    (op == crate::units::ghost_make::EE_UNITS_GHOST_TECH_CLASS)
        .then(|| Some(class_table(node_class(node_of(a as usize, b)))? as i64))?
}

/// The unit host's statement `op` (EEUnitHost.cpp `ee_units_uh_host`).
pub(crate) fn uh(op: i32, a: u64, b: u64, c: i64) -> Option<i64> {
    use crate::units::uh_body::{
        EE_UNITS_UH_NODE_HERO, EE_UNITS_UH_NODE_KIND, EE_UNITS_UH_TECH_CLASS, EE_UNITS_UH_TECH_NODE,
        EE_UNITS_UH_TREE_NODE,
    };
    Some(match op {
        EE_UNITS_UH_TECH_CLASS => class_table(node_class(node_of(a as usize, c)))? as i64,
        EE_UNITS_UH_TREE_NODE => node_of(a as usize, c) as i64,
        EE_UNITS_UH_TECH_NODE => node_of(a as usize, c as i32 as i64) as i64,
        EE_UNITS_UH_NODE_KIND => data(b as usize).mKind as i64,
        EE_UNITS_UH_NODE_HERO => {
            let c = node_class(b as usize);
            let hero = c > 0 && {
                let class = class_table(c)?;
                class != 0 && class_ai_kind(class)? == crate::unit_ids::EEUnitAIKind::kAIKindHero as u64
            };
            i64::from(hero)
        }
        _ => return None,
    })
}

/// The goal host's statement `op` (EEUGRust.cpp `GoalOp`; `i0` its first integer).
pub(crate) fn goal(op: i32, a: u64, b: u64, i0: i64) -> Option<i64> {
    use crate::units::goal_split::{
        EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE, EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME, EE_UNITS_GOAL_HOST_X_NODE_CALAMITY,
        EE_UNITS_GOAL_HOST_X_NODE_ID, EE_UNITS_GOAL_HOST_X_NODE_KIND, EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH,
        EE_UNITS_GOAL_HOST_X_PLAYER_TECH, EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE,
        EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS, EE_UNITS_GOAL_HOST_X_TECH_TREE_GET,
    };
    Some(match op {
        EE_UNITS_GOAL_HOST_X_PLAYER_TECH | EE_UNITS_GOAL_HOST_X_TECH_TREE_GET => node_of(unit_player(a)?, i0) as i64,
        EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH => i64::from(tree(a as usize) != 0),
        EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS => {
            research(tree(a as usize), class_technology(unit_class(b)?)?, 0);
            0
        }
        EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE => i64::from(available(node_of(a as usize, i0))),
        EE_UNITS_GOAL_HOST_X_NODE_CALAMITY => data(a as usize).mCalamity as i64,
        EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE => i64::from(available(a as usize)),
        EE_UNITS_GOAL_HOST_X_NODE_ID => data(a as usize).mID as i64,
        EE_UNITS_GOAL_HOST_X_NODE_KIND => data(a as usize).mKind as i64,
        EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME => build_time(a as usize),
        _ => return None,
    })
}

/// The search host's statement `op` (EEFOFindNewTargetNearOldTarget.cpp `ee_units_sr_host`).
pub(crate) fn sr(op: i32, unit: u64, a: u64) -> Option<i64> {
    use crate::units::uu_search::{EE_UNITS_SR_OWN_TECHNOLOGY, EE_UNITS_SR_TECH_NODE};
    let owner = match op {
        EE_UNITS_SR_TECH_NODE => a,
        EE_UNITS_SR_OWN_TECHNOLOGY => unit,
        _ => return None,
    };
    let technology = class_technology(unit_class(a)?)?;
    Some(node_of(unit_player(owner)?, technology) as i64)
}

/// The unit utilities' host's statement `op` (EEFOFindNewTargetNearOldTarget.cpp `ee_units_ub_host`).
pub(crate) fn ub(op: i32, a: u64, c: i64) -> Option<i64> {
    use crate::units::uu_body::{EE_UNITS_UB_TECHNOLOGY, EE_UNITS_UB_TECH_AVAILABLE, EE_UNITS_UB_UNIT_TECHNOLOGY};
    Some(match op {
        EE_UNITS_UB_TECH_AVAILABLE => i64::from(available(node_of(a as usize, c))),
        EE_UNITS_UB_TECHNOLOGY => node_of(a as usize, c) as i64,
        EE_UNITS_UB_UNIT_TECHNOLOGY => node_of(unit_player(a)?, c) as i64,
        // player a's mUnitClassUpgrades[c]->mModel (player_upgrades.rs; the upgrade's links record)
        crate::units::uu_body::EE_UNITS_UB_UPGRADE_MODEL => {
            let upgrade = crate::units::player_upgrades::at(a, c as usize) as usize;
            let links = upgrade + l()?.stats_links as usize;
            // SAFETY: the player's live upgrade's links (the C++ statement dereferenced it)
            unsafe {
                (*(links as *const crate::world::stats_fields::EeUnitsUnitClassUpgradeLinks)).mModel as usize as i64
            }
        }
        _ => return None,
    })
}

/// The unit utilities' world question 53 (`ee_units_uu_host_world`): player `b`'s space wall available.
pub(crate) fn world(which: i32, b: u64) -> Option<u64> {
    (which == 53).then(|| u64::from(available(node_of(b as usize, T::kTechWallSpace as i64))))
}
