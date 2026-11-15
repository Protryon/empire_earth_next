//! The AI effects that steer a computer player's military (`EESMEffect::BuildObject`, `CreateAIMilitaryGroup`,
//! `ModifyAIMilitaryTarget`): the players, areas, units and counts they pick, over the world's calls
//! (`EE_SCENARIO_WORLD_AI_*`). The C++ keeps the run's task, location, strategy, requirement and unit id list (as
//! the originals kept them across the players: what one player's pass leaves, the next one starts from).

#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::effect::*;
use crate::fields::{EeScenarioEffectFields, EeScenarioObjectFields};
use crate::host::*;

/// Computer player a's first town's serial (`mTowns[0]`), -1 without one.
pub const EE_SCENARIO_WORLD_AI_TOWN: u32 = 100;
/// The serial of the largest continent of category a (`GetContinentByID(GetLargestContinent(a))`), -1 for none.
pub const EE_SCENARIO_WORLD_AI_LARGEST_CONTINENT: u32 = 185;
/// The effect's object list's count.
pub const EE_SCENARIO_WORLD_EFFECT_OBJECT_COUNT: u32 = 101;
/// The effect's object list's entry a.
pub const EE_SCENARIO_WORLD_EFFECT_OBJECT_AT: u32 = 102;
/// The strategy `Reset()`, `mKind = a`, `mAddFavorites = b`.
pub const EE_SCENARIO_WORLD_AI_STRATEGY: u32 = 103;
/// The requirement's `Clear()`.
pub const EE_SCENARIO_WORLD_AI_REQUIREMENT_CLEAR: u32 = 104;
/// The strategy's unit count and remaining count 0 (any count).
pub const EE_SCENARIO_WORLD_AI_ANY_COUNT: u32 = 105;
/// The requirement's left and wanted counts = a; with b, the strategy's unit count raised by a (and the remaining
/// count = it).
pub const EE_SCENARIO_WORLD_AI_COUNT: u32 = 106;
/// The requirement's unit class (a 0) = b, family (1) = b, unit kind (2) = b, or (3) computer player b's favourite
/// land type as the unit class.
pub const EE_SCENARIO_WORLD_AI_REQUIREMENT_SET: u32 = 107;
/// The requirement already defined and added to the strategy.
pub const EE_SCENARIO_WORLD_AI_REQUIREMENT_PUSH: u32 = 108;
/// Computer player a: `IsValidTask(task, strategy, ids)`, then its attack commander's `CreateGroupFromStrategy`.
pub const EE_SCENARIO_WORLD_AI_CREATE_GROUP: u32 = 109;
/// The task's mode = combat.
pub const EE_SCENARIO_WORLD_AI_TASK_COMBAT: u32 = 110;
/// The AI rectangle area of scenario area a (the last in the list): its serial, -1 for none.
pub const EE_SCENARIO_WORLD_AI_AREA_RECTANGLE: u32 = 111;
/// The continent at scenario area a's anchor (when it has one): its serial, -1 for none.
pub const EE_SCENARIO_WORLD_AI_AREA_CONTINENT: u32 = 112;
/// The task's unit = unit a.
pub const EE_SCENARIO_WORLD_AI_TASK_UNIT: u32 = 113;
/// The location's serial a and type b, reset to the area, as the task's location.
pub const EE_SCENARIO_WORLD_AI_LOCATION_AREA: u32 = 114;
/// Computer player a's intelligence's `GetRandomEnemy`: the player's id, -1 for none.
pub const EE_SCENARIO_WORLD_AI_RANDOM_ENEMY: u32 = 115;
/// The location's tile = unit a's (or its container's), as the task's location.
pub const EE_SCENARIO_WORLD_AI_LOCATION_UNIT: u32 = 116;
/// The task's player = `GetPlayer(a)`.
pub const EE_SCENARIO_WORLD_AI_TASK_PLAYER: u32 = 117;
/// The task active, of type a (0 location, 1 territory, 2 unit).
pub const EE_SCENARIO_WORLD_AI_TASK_TYPE: u32 = 118;
/// Whether the task's unit belongs to another player that computer player a is at war with.
pub const EE_SCENARIO_WORLD_AI_TASK_UNIT_ENEMY: u32 = 119;
/// Object a's units appended to the id list.
pub const EE_SCENARIO_WORLD_AI_IDS_PUSH_OBJECT: u32 = 120;
/// The id list's count.
pub const EE_SCENARIO_WORLD_AI_IDS_COUNT: u32 = 121;
/// The id list's entry a.
pub const EE_SCENARIO_WORLD_AI_IDS_AT: u32 = 122;
/// The id list's entry a erased.
pub const EE_SCENARIO_WORLD_AI_IDS_ERASE: u32 = 123;
/// Computer player a's combat manager's `IsMilitaryUnit(unit b)`.
pub const EE_SCENARIO_WORLD_AI_IS_MILITARY: u32 = 124;
/// Unit a's `mIsDetachedFromAI = false`.
pub const EE_SCENARIO_WORLD_AI_ATTACH: u32 = 125;
/// Computer player a's `IsValidTarget(serial b, unit c, d)`.
pub const EE_SCENARIO_WORLD_AI_VALID_TARGET: u32 = 126;
/// Computer player a's `AddAttackTask(unit b, serial c)`.
pub const EE_SCENARIO_WORLD_AI_ADD_ATTACK: u32 = 127;
/// Computer player a's `AddDefendTask(unit b, serial c)`.
pub const EE_SCENARIO_WORLD_AI_ADD_DEFEND: u32 = 128;
/// Computer player a's `DropAreaTask(serial b, false)`.
pub const EE_SCENARIO_WORLD_AI_DROP_AREA: u32 = 129;
/// Computer player a's `DropUnitTask(unit b, true)`.
pub const EE_SCENARIO_WORLD_AI_DROP_UNIT: u32 = 130;

/// `(unsigned long)-1`.
const NONE_ID: i64 = 0xffff_ffff;

/// The requirement for an object (the original's block in Build Object and Create AI Military Group): a random count
/// between its counts (none for both 0: any count), and its class, the player's favourite land type, a family or a
/// unit kind by its family setting.
fn requirement<H: Host>(h: &mut H, p: i64, f: &EeScenarioObjectFields, any_count: &mut bool) {
    if f.mMinCount == 0 && f.mMaxCount == 0 {
        *any_count = true;
        w(h, EE_SCENARIO_WORLD_AI_ANY_COUNT, &[]);
    } else {
        let low = f.mMinCount.min(f.mMaxCount);
        let high = f.mMinCount.max(f.mMaxCount);
        let n = h.read(EE_SCENARIO_READ_RANDOM, low as c_long as i32, high as c_long as i32);
        w(h, EE_SCENARIO_WORLD_AI_COUNT, &[n, !*any_count as i64]);
    }
    let family = f.mFamily;
    let set = if family == 0 {
        (h.read(EE_SCENARIO_READ_CLASS_SELECTABLE, f.mClass as i32, 0) != 0).then_some((0, f.mClass as i64))
    } else if family < 0x10 {
        Some((3, p))
    } else if family < 0x4b {
        Some((1, (family - 0xf) as i64))
    } else if family <= 0x74 {
        Some((2, (family - 0x4a) as i64))
    } else {
        None
    };
    if let Some((what, value)) = set {
        w(h, EE_SCENARIO_WORLD_AI_REQUIREMENT_SET, &[what, value]);
        w(h, EE_SCENARIO_WORLD_AI_REQUIREMENT_PUSH, &[]);
    }
}

/// The chosen players, the last first (the originals' loops ran backwards).
fn players_backwards<H: Host>(h: &mut H, kind: c_ulong) -> Vec<i64> {
    players(h, kind).into_iter().rev().map(|p| p as i64).collect()
}

/// Build Object's task for computer player `p`: at its first town, else on the largest land (or any) continent,
/// against an enemy its intelligence picks (else player 0); false when it has neither a town nor a continent.
pub(crate) fn build_task<H: Host>(h: &mut H, p: i64) -> bool {
    let town = w(h, EE_SCENARIO_WORLD_AI_TOWN, &[p]);
    let (serial, kind) = if town >= 0 {
        (town, AREA_TOWN)
    } else {
        let mut continent = w(h, EE_SCENARIO_WORLD_AI_LARGEST_CONTINENT, &[CONTINENT_LAND]);
        if continent < 0 {
            continent = w(h, EE_SCENARIO_WORLD_AI_LARGEST_CONTINENT, &[CONTINENT_ANY]);
            if continent < 0 {
                return false;
            }
        }
        (continent, AREA_CONTINENT)
    };
    w(h, EE_SCENARIO_WORLD_AI_LOCATION_AREA, &[serial, kind]);
    let enemy = w(h, EE_SCENARIO_WORLD_AI_RANDOM_ENEMY, &[p]);
    w(h, EE_SCENARIO_WORLD_AI_TASK_PLAYER, &[if enemy >= 0 { enemy } else { 0 }]);
    w(h, EE_SCENARIO_WORLD_AI_TASK_TYPE, &[0]);
    true
}

/// `kAIAreaTown`, `kAIAreaContinent`; `kContinentCategoryLand`, `kContinentCategoryNone` (any).
const AREA_TOWN: i64 = 5;
const AREA_CONTINENT: i64 = 1;
const CONTINENT_LAND: i64 = 1;
const CONTINENT_ANY: i64 = 0;

/// Build Object: each chosen computer player raises a military group of the target object's unit kind (a random
/// count) at its first town or the largest continent, aimed at a random enemy.
pub fn build_object<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    for p in players_backwards(h, e.mPlayer) {
        if w(h, EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER, &[p]) == 0 {
            continue;
        }
        if !build_task(h, p) {
            continue;
        }
        w(h, EE_SCENARIO_WORLD_AI_STRATEGY, &[1, 0]);
        w(h, EE_SCENARIO_WORLD_AI_REQUIREMENT_CLEAR, &[]);
        let Some(f) = h.object(e.mTargetObject as i32) else { continue };
        let mut any_count = false;
        requirement(h, p, &f, &mut any_count);
        w(h, EE_SCENARIO_WORLD_AI_CREATE_GROUP, &[p]);
    }
}

/// The serial of a scenario area for the AI: its AI rectangle area's, else (`serial` still 0) the continent's at its
/// anchor (then `continent` is set). Both carry over from the player before when nothing is found.
fn area_serial<H: Host>(h: &mut H, area: c_ulong, serial: &mut i64, continent: &mut bool) {
    if !area_usable(h, area) {
        return;
    }
    let s = w(h, EE_SCENARIO_WORLD_AI_AREA_RECTANGLE, &[area as i64]);
    if s >= 0 {
        *serial = s;
    }
    if *serial == 0 {
        let c = w(h, EE_SCENARIO_WORLD_AI_AREA_CONTINENT, &[area as i64]);
        if c >= 0 {
            *serial = c;
            *continent = true;
        }
    }
}

/// Create AI Military Group: each chosen computer player forms a group from the listed objects' units (existing ones
/// join, the rest are requested), aimed at the target area or the target object's unit; an attack or a defence group.
pub fn create_ai_military_group<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    // what the original kept across the players
    let mut target_player: i64 = -1;
    let mut area_serial_value: i64 = 0;
    let mut unit_id: i64 = 0;
    let mut continent = false;
    for p in players_backwards(h, e.mPlayer) {
        if w(h, EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER, &[p]) == 0 {
            continue;
        }
        let mut any_count = false;
        w(h, EE_SCENARIO_WORLD_AI_TASK_COMBAT, &[]);
        if e.mUsesTargetArea {
            area_serial(h, e.mTargetArea, &mut area_serial_value, &mut continent);
        } else if h.object(e.mTargetObject as i32).is_some() {
            let n = count(h, e.mTargetObject);
            if n != 0 {
                unit_id = unit_at(h, e.mTargetObject, 0);
                for k in (0..n).rev() {
                    let id = unit_at(h, e.mTargetObject, k);
                    if alive_complex(h, id) {
                        unit_id = unit(h, id, EE_SCENARIO_UV_ID);
                        w(h, EE_SCENARIO_WORLD_AI_TASK_UNIT, &[id]);
                    }
                }
            }
        }
        if area_serial_value != 0 {
            w(h, EE_SCENARIO_WORLD_AI_LOCATION_AREA, &[area_serial_value, if continent { 1 } else { 8 }]);
            target_player = w(h, EE_SCENARIO_WORLD_AI_RANDOM_ENEMY, &[p]);
        } else if unit_id != 0 {
            if exists(h, unit_id) && unit(h, unit_id, EE_SCENARIO_UV_COMPLEX) != 0 {
                target_player = unit(h, unit_id, EE_SCENARIO_UV_PLAYER);
                w(h, EE_SCENARIO_WORLD_AI_LOCATION_UNIT, &[unit_id]);
            } else {
                target_player = w(h, EE_SCENARIO_WORLD_AI_RANDOM_ENEMY, &[p]);
            }
        }
        let me = h.read(EE_SCENARIO_READ_PLAYER_ID, p as i32, 0);
        let task_player = if target_player >= 0 && target_player != me { target_player } else { 0 };
        w(h, EE_SCENARIO_WORLD_AI_TASK_PLAYER, &[task_player]);
        w(h, EE_SCENARIO_WORLD_AI_TASK_TYPE, &[0]);
        if e.mIsAttack && unit_id == 0 {
            w(h, EE_SCENARIO_WORLD_AI_TASK_TYPE, &[1]);
        } else if e.mIsAttack && w(h, EE_SCENARIO_WORLD_AI_TASK_UNIT_ENEMY, &[p]) != 0 {
            w(h, EE_SCENARIO_WORLD_AI_TASK_TYPE, &[2]);
        }
        w(h, EE_SCENARIO_WORLD_AI_STRATEGY, &[5 - e.mIsAttack as i64, e.mUsesFavorites as i64]);
        let objects = w(h, EE_SCENARIO_WORLD_EFFECT_OBJECT_COUNT, &[]);
        for n in (0..objects).rev() {
            w(h, EE_SCENARIO_WORLD_AI_REQUIREMENT_CLEAR, &[]);
            let object = w(h, EE_SCENARIO_WORLD_EFFECT_OBJECT_AT, &[n]) as c_ulong;
            if h.object(object as i32).is_none() {
                continue;
            }
            if evaluate(h, object) == 1 && count(h, object) != 0 {
                w(h, EE_SCENARIO_WORLD_AI_IDS_PUSH_OBJECT, &[object as i64]);
                continue;
            }
            let Some(f) = h.object(object as i32) else { continue };
            requirement(h, p, &f, &mut any_count);
        }
        let ids = w(h, EE_SCENARIO_WORLD_AI_IDS_COUNT, &[]);
        for v in (0..ids).rev() {
            let id = w(h, EE_SCENARIO_WORLD_AI_IDS_AT, &[v]);
            let keep = exists(h, id)
                && unit(h, id, EE_SCENARIO_UV_COMPLEX) != 0
                && unit(h, id, EE_SCENARIO_UV_PLAYER) == me
                && unit(h, id, EE_SCENARIO_UV_HIT_POINTS) >= 1
                && w(h, EE_SCENARIO_WORLD_AI_IS_MILITARY, &[p, id]) != 0;
            if !keep {
                w(h, EE_SCENARIO_WORLD_AI_IDS_ERASE, &[v]);
            } else if unit(h, id, EE_SCENARIO_UV_DETACHED) != 0 {
                w(h, EE_SCENARIO_WORLD_AI_ATTACH, &[id]);
            }
        }
        w(h, EE_SCENARIO_WORLD_AI_CREATE_GROUP, &[p]);
    }
}

/// Modify AI Military Target: an attack or defend target (the target area's AI area or continent, or the target
/// object's living unit) added or dropped for each chosen computer player; a new one must be valid.
pub fn modify_ai_military_target<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let mut serial: i64 = 0; // kept across the players, as the original's
    let mut continent = false;
    for p in players_backwards(h, e.mPlayer) {
        if w(h, EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER, &[p]) == 0 {
            continue;
        }
        area_serial(h, e.mTargetArea, &mut serial, &mut continent);
        let mut unit_id = NONE_ID;
        if h.object(e.mTargetObject as i32).is_some() {
            let n = count(h, e.mTargetObject);
            if n > 0 {
                unit_id = unit_at(h, e.mTargetObject, 0);
                for k in (0..n).rev() {
                    let id = unit_at(h, e.mTargetObject, k);
                    if alive_complex(h, id) {
                        unit_id = unit(h, id, EE_SCENARIO_UV_ID);
                    }
                }
            }
        }
        if e.mIsAddTarget && w(h, EE_SCENARIO_WORLD_AI_VALID_TARGET, &[p, serial, unit_id, e.mIsAttack as i64]) == 0 {
            continue;
        }
        let op = match (e.mIsAddTarget, e.mIsAttack, unit_id == NONE_ID) {
            (true, true, _) => EE_SCENARIO_WORLD_AI_ADD_ATTACK,
            (true, false, _) => EE_SCENARIO_WORLD_AI_ADD_DEFEND,
            (false, _, true) => EE_SCENARIO_WORLD_AI_DROP_AREA,
            (false, _, false) => EE_SCENARIO_WORLD_AI_DROP_UNIT,
        };
        match op {
            EE_SCENARIO_WORLD_AI_DROP_AREA => w(h, op, &[p, serial]),
            EE_SCENARIO_WORLD_AI_DROP_UNIT => w(h, op, &[p, unit_id]),
            _ => w(h, op, &[p, unit_id, serial]),
        };
    }
}
