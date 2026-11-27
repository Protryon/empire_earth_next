//! The classic_lua units host's operations whose bodies moved to Rust in the AI Thread cluster's round 9 (were
//! EECPNextUnits.cpp's cases with more than one statement): the map's size, a unit's attach, a player's upgrade of a
//! class, a gatherer's target, the selection's start, a command's message under the borrowed slot, the building
//! block test, the affordability of a tech, the garrison fill. What only C++ holds is one `ee_ai_units_host_cpp` case
//! each (EE_AI_UNITS_HOST_* from 141).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_char, c_void, CStr};
use std::os::raw::c_long;

use super::more;
use super::*;
use crate::ai::names_db;
use crate::world::unit_fields::EeUnitsComplexUnitFields;
use crate::world::view::{UnitView, WorldRead, WorldView};

// ---- the C++ host's cases these bodies ask (EECPNextUnits.cpp ee_ai_units_host_cpp)
/// player `a`'s `mUnitClassUpgrades.size()`, its element `b`
pub const EE_AI_UNITS_HOST_UPGRADE_COUNT: i32 = 141;
pub const EE_AI_UNITS_HOST_UPGRADE_AT: i32 = 142;
/// `EEGatherGoalCast(a->GetGoalState())` of unit `a`; the gather goal `a`'s `GetGatherResource()` (an `RTSUnit*`)
pub const EE_AI_UNITS_HOST_GATHER_GOAL: i32 = 143;
pub const EE_AI_UNITS_HOST_GATHER_RESOURCE: i32 = 144;
/// the player's `mActiveGroup`; its `Clear()`
pub const EE_AI_UNITS_HOST_ACTIVE_GROUP: i32 = 145;
pub const EE_AI_UNITS_HOST_GROUP_CLEAR: i32 = 146;
/// the message record `out` of type `a` built and run as the human's, from the slot `b` (0: as it is)
pub const EE_AI_UNITS_HOST_EXEC_RUN: i32 = 147;
/// the player's `mSlot`; `mSlot = a`; `gWorld.GetPlayerBySlot(a) == me`
pub const EE_AI_UNITS_HOST_SLOT: i32 = 148;
pub const EE_AI_UNITS_HOST_SET_SLOT: i32 = 149;
pub const EE_AI_UNITS_HOST_SLOT_IS_ME: i32 = 150;
/// the player's `mTechnologies->Get(a)`; `CanAfford(a, 0x20, 1)`
pub const EE_AI_UNITS_HOST_NODE_OF: i32 = 151;
pub const EE_AI_UNITS_HOST_CAN_AFFORD: i32 = 152;
/// unit `a`'s `AddGarrison(b, c)`
pub const EE_AI_UNITS_HOST_ADD_GARRISON: i32 = 153;
/// the tech node `a`'s `mData != NULL`; its `mData->mCosts[kResourceFood + b]`
pub const EE_AI_UNITS_HOST_NODE_HAS_DATA: i32 = 154;
pub const EE_AI_UNITS_HOST_NODE_COST_AT: i32 = 155;
/// `gUnitClassTable[a]->mImprovements`; the table `a`'s `GetMaxLevel()`, its `mEntries[b].mCostFactor` (a float into
/// `out`); the player's `GetImprovementLevel(a, b)`; the upgrade `a`'s `mCostScale` (a float into `out`)
pub const EE_AI_UNITS_HOST_CLASS_IMPROVEMENTS: i32 = 156;
pub const EE_AI_UNITS_HOST_IMPROVE_MAX: i32 = 157;
pub const EE_AI_UNITS_HOST_IMPROVE_FACTOR: i32 = 158;
pub const EE_AI_UNITS_HOST_IMPROVE_LEVEL: i32 = 159;
pub const EE_AI_UNITS_HOST_COST_SCALE: i32 = 160;

/// The slot id a computer player answers to while its order runs.
const BORROWED_SLOT: i64 = 0xfd;
/// `EE_AI_UNITS_HOST_EXEC`'s modes: 0 on the selection under the borrowed slot (the selection cleared after), 1 as it
/// is, 2 under the borrowed slot unchecked, 3 under it when it names the player (the selection kept)
const MODE_SELECTION: i32 = 0;
const MODE_AS_IS: i32 = 1;
const MODE_BORROWED: i32 = 2;

/// The message types of `EE_AI_UNITS_MSG_*` (ee/EEMessageID.h kMessage*: path to, attack unit, attack ground,
/// gather, build, repair, load garrison, load transport, unload, research, behaviour, stop, explore, kill, heal,
/// rally, remove tech, formation, pack, drop off, replant, smart farmers, cast, patrol point, lock, build wall,
/// improve, hack a new unit, build gate, exchange).
pub const MESSAGE_TYPES: [u8; 31] = [
    0x00, 0x4d, 0x4c, 0x9f, 0x51, 0x56, 0x57, 0x7b, 0x67, 0x68, 0x52, 0x9e, 0x8a, 0x79, 0x5c, 0x92, 0x66, 0x53, 0x78,
    0xab, 0x61, 0x81, 0x8f, 0x75, 0x72, 0x8c, 0x59, 0x77, 0x6b, 0x86, 0xae,
];

fn cpp(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64 {
    // SAFETY: the C++ switch over the live game, on the simulation thread
    unsafe { super::tech_ops::op(op, me, a, b).unwrap_or_else(|| ee_ai_units_host_cpp(op, me, a, b, c, out)) }
}

/// The operations whose bodies are Rust (None: the others').
///
/// # Safety
/// As `ee_ai_units_host`.
pub(super) unsafe fn op(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> Option<i64> {
    let null = std::ptr::null_mut();
    Some(match op {
        EE_AI_UNITS_HOST_MAP_SIZE => {
            // `gMapWidth | gMapHeight << 32` (the continent manager's record holds them), 0 without a map
            let t = WorldRead::begin();
            let has_map = WorldView::global(&t).is_some_and(|w| w.map().is_some());
            let (w, h) = match crate::astar::world_view::cm() {
                Some(r) if has_map => (r.width as c_long, r.height as c_long),
                _ => (0, 0),
            };
            pack(w, h)
        }
        EE_AI_UNITS_HOST_CLASS_BY_NAME => {
            names_db::class_by_name(CStr::from_ptr(a as usize as *const c_char).to_bytes())
        }
        EE_AI_UNITS_HOST_TECH_BY_NAME => names_db::tech_by_name(CStr::from_ptr(a as usize as *const c_char).to_bytes()),
        EE_AI_UNITS_HOST_CLASS_NAME => name_into(out, &names_db::class_name(i64::from(a as c_long))),
        more::EE_AI_UNITS_HOST_TECH_NAME => name_into(out, &names_db::tech_name(i64::from(a as c_long))),
        // the gather safety (ee_ai next_game.rs)
        #[cfg(not(test))]
        EE_AI_UNITS_HOST_ASSIGNED => i64::from(crate::ai::next_game::ee_ai_next_assigned(me, a as u64)),
        #[cfg(not(test))]
        EE_AI_UNITS_HOST_HAS_ROOM => i64::from(crate::ai::next_game::ee_ai_next_has_room(me, a as u64)),
        #[cfg(not(test))]
        more::EE_AI_UNITS_HOST_SITE_BLOCKED => {
            i64::from(crate::ai::next_game::ee_ai_next_site_blocked(me, a as u64, std::ptr::null_mut()))
        }
        #[cfg(not(test))]
        more::EE_AI_UNITS_HOST_BUILD_UNSAFE => {
            i64::from(crate::ai::next_game::ee_ai_next_build_unsafe(me, a as c_long, b as c_long))
        }
        #[cfg(not(test))]
        more::EE_AI_UNITS_HOST_SITE_THREAT => i64::from(crate::ai::next_game::ee_ai_next_site_threat(
            me,
            a as c_long,
            b as c_long,
            false,
            std::ptr::null_mut(),
        )),
        more::EE_AI_UNITS_HOST_IMPROVE_COST => {
            i64::from(improve_cost(me, a as c_long, b as c_long, &mut *(out as *mut more::EeAiUnitsImproveCost)))
        }
        EE_AI_UNITS_HOST_ATTACH => {
            // `mIsDetachedFromAI` cleared when set
            if let Some(f) = complex_mut(a as u64) {
                if (*f).mIsDetachedFromAI {
                    (*f).mIsDetachedFromAI = false;
                }
            }
            0
        }
        EE_AI_UNITS_HOST_UPGRADE => {
            if b < 0 || b >= cpp(EE_AI_UNITS_HOST_UPGRADE_COUNT, me, a, 0, 0, null) {
                0
            } else {
                cpp(EE_AI_UNITS_HOST_UPGRADE_AT, me, a, b, 0, null)
            }
        }
        EE_AI_UNITS_HOST_GATHER_TARGET => {
            // the gather goal's resource, when it is valid: its id
            let goal = cpp(EE_AI_UNITS_HOST_GATHER_GOAL, me, a, 0, 0, null);
            let resource = if goal != 0 { cpp(EE_AI_UNITS_HOST_GATHER_RESOURCE, me, goal, 0, 0, null) } else { 0 };
            if resource == 0 {
                return Some(0);
            }
            let t = WorldRead::begin();
            UnitView::of(&t, resource as u64).filter(|u| u.valid()).map_or(0, |u| i64::from(u.id()))
        }
        EE_AI_UNITS_HOST_SELECT_BEGIN => {
            if cpp(EE_AI_UNITS_HOST_ACTIVE_GROUP, me, 0, 0, 0, null) == 0 {
                0
            } else {
                cpp(EE_AI_UNITS_HOST_GROUP_CLEAR, me, 0, 0, 0, null);
                1
            }
        }
        EE_AI_UNITS_HOST_EXEC => {
            exec(me, out);
            0
        }
        EE_AI_UNITS_HOST_BLOCK_TAKES => {
            // a building of type `a` and size `*out` with its margin at (b, c)
            let size = *(out as *const c_long) as i32;
            let me64 = me as usize as u64;
            let x = (b as c_long as i32).wrapping_sub(1);
            let y = (c as c_long as i32).wrapping_sub(1);
            let span = size.wrapping_add(2);
            i64::from(crate::units::uu_ffi::ee_units_uu_block_takes(me64, a as c_long as i32, x, y, span, span, 0) != 0)
        }
        EE_AI_UNITS_HOST_NODE_AFFORD => {
            let node = cpp(EE_AI_UNITS_HOST_NODE_OF, me, a, 0, 0, null);
            if node == 0 {
                -1
            } else {
                cpp(EE_AI_UNITS_HOST_CAN_AFFORD, me, node, 0, 0, null)
            }
        }
        more::EE_AI_UNITS_HOST_GARRISON_FILL => {
            // `AddGarrison(mGarrisonCount + b, mGarrisonPopulation)`
            let Some(f) = complex_mut(a as u64) else { return Some(0) };
            let count = ((*f).mGarrisonCount as c_long).wrapping_add(b as c_long);
            cpp(EE_AI_UNITS_HOST_ADD_GARRISON, me, a, count as i64, (*f).mGarrisonPopulation as i64, null)
        }
        _ => return None,
    })
}

/// The next level's cost of class `ty`'s improvement `imp`, as the improve verb pays it (ai.improve_cost): false for
/// no class, no tech tree, no available node now or no such improvement.
fn improve_cost(me: *mut c_void, ty: c_long, imp: c_long, out: &mut more::EeAiUnitsImproveCost) -> bool {
    let null = std::ptr::null_mut();
    let technology = {
        // SAFETY: the unit classes are the database's, live on the simulation thread
        let t = unsafe { WorldRead::begin() };
        let class = if ty < 0 { None } else { crate::world::ClassView::by_index(&t, ty as usize) };
        match class {
            Some(c) => c.technology(),
            None => return false,
        }
    };
    if cpp(EE_AI_UNITS_HOST_HAS_TREE, me, 0, 0, 0, null) == 0 {
        return false;
    }
    let node = cpp(EE_AI_UNITS_HOST_NODE_OF, me, technology, 0, 0, null);
    if node == 0
        || cpp(EE_AI_UNITS_HOST_NODE_HAS_DATA, me, node, 0, 0, null) == 0
        || cpp(more::EE_AI_UNITS_HOST_NODE_AVAILABLE, me, node, 0, 0, null) == 0
        || cpp(more::EE_AI_UNITS_HOST_NODE_OBSOLETE, me, node, 0, 0, null) != 0
    {
        return false;
    }
    let table = cpp(EE_AI_UNITS_HOST_CLASS_IMPROVEMENTS, me, ty as i64, 0, 0, null);
    if !(0..16).contains(&imp) || table == 0 {
        return false;
    }
    let upgrade = cpp(EE_AI_UNITS_HOST_UPGRADE_AT, me, me as usize as i64, ty as i64, 0, null);
    out.level = cpp(EE_AI_UNITS_HOST_IMPROVE_LEVEL, me, ty as i64, imp as i64, 0, null) as c_long;
    out.max = cpp(EE_AI_UNITS_HOST_IMPROVE_MAX, me, table, 0, 0, null) as c_long;
    let mut cost_factor = 0.0f32;
    cpp(EE_AI_UNITS_HOST_IMPROVE_FACTOR, me, table, imp as i64, 0, (&mut cost_factor as *mut f32).cast());
    let factor = cost_factor * out.level as f32;
    let mut scale = 0.0f32;
    cpp(EE_AI_UNITS_HOST_COST_SCALE, me, upgrade, 0, 0, (&mut scale as *mut f32).cast());
    for i in 0..5 {
        let base = cpp(EE_AI_UNITS_HOST_NODE_COST_AT, me, node, i, 0, null) as c_long;
        let value = ee_base::math::um::lrintf(base as f32 * scale * factor);
        out.cost[i as usize] = cpp(more::EE_AI_UNITS_HOST_SCALE_COST, me, value as i64, 0, 0, null) as c_long;
    }
    true
}

/// A name into the 1024-byte record `out` (at most 1023 bytes, then a NUL) -> its length.
///
/// # Safety
/// `out` is the operation's 1024-byte record.
unsafe fn name_into(out: *mut c_void, name: &[u8]) -> i64 {
    let n = name.len().min(1023);
    std::ptr::copy_nonoverlapping(name.as_ptr(), out as *mut u8, n);
    *(out as *mut u8).add(n) = 0;
    n as i64
}

fn pack(w: c_long, h: c_long) -> i64 {
    (((h as u32 as u64) << 32) | (w as u32 as u64)) as i64
}

fn complex_mut(unit: u64) -> Option<*mut EeUnitsComplexUnitFields> {
    // SAFETY: the callers hand live units, on the simulation thread
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    unsafe { UnitView::of(&t, unit) }
        .filter(|u| u.is_complex())
        .map(|u| u.complex().ptr() as *mut EeUnitsComplexUnitFields)
}

/// A command's message (`EE_AI_UNITS_HOST_EXEC`, the record `out`): run as it is, or under the borrowed slot (the
/// player's own slot put back after; on the selection the selection is cleared after).
fn exec(me: *mut c_void, out: *mut c_void) {
    // SAFETY: the caller's record
    let m = unsafe { &*(out as *const EeAiUnitsMessage) };
    let kind = i64::from(MESSAGE_TYPES.get(m.message as usize).copied().unwrap_or(0));
    if m.mode == MODE_AS_IS {
        cpp(EE_AI_UNITS_HOST_EXEC_RUN, me, kind, 0, 0, out);
        return;
    }
    let null = std::ptr::null_mut();
    let slot = cpp(EE_AI_UNITS_HOST_SLOT, me, 0, 0, 0, null);
    cpp(EE_AI_UNITS_HOST_SET_SLOT, me, BORROWED_SLOT, 0, 0, null);
    if m.mode == MODE_BORROWED || cpp(EE_AI_UNITS_HOST_SLOT_IS_ME, me, BORROWED_SLOT, 0, 0, null) != 0 {
        cpp(EE_AI_UNITS_HOST_EXEC_RUN, me, kind, BORROWED_SLOT, 0, out);
    }
    cpp(EE_AI_UNITS_HOST_SET_SLOT, me, slot, 0, 0, null);
    if m.mode == MODE_SELECTION {
        cpp(EE_AI_UNITS_HOST_GROUP_CLEAR, me, 0, 0, 0, null);
    }
}

#[cfg(test)]
mod tests {
    use super::more;
    use super::*;

    #[test]
    fn message_types_cover_every_message() {
        assert_eq!(MESSAGE_TYPES.len() as i32, more::EE_AI_UNITS_MSG_EXCHANGE + 1);
        assert_eq!(MESSAGE_TYPES[EE_AI_UNITS_MSG_PATH_TO as usize], 0x4d);
        assert_eq!(MESSAGE_TYPES[more::EE_AI_UNITS_MSG_EXCHANGE as usize], 0xae);
        // every message but the empty slot 0 is a game message type (0x41..0xfd)
        assert!(MESSAGE_TYPES[1..].iter().all(|&t| (0x41..=0xfd).contains(&t)));
    }

    #[test]
    fn packs_width_low() {
        assert_eq!(pack(160, 120), (120i64 << 32) | 160);
        assert_eq!(pack(0, 0), 0);
    }
}
