//! The two task effects (`EESMEffect::TaskUnitsToObject`, `TaskUnitsToArea`): which of the object's units are
//! tasked, how they are dealt to the targets and grouped per player, and which order each group gets (gather,
//! attack, heal, garrison, load, repair, drop off, patrol, move, cast a calamity), over the world's calls
//! (`EE_SCENARIO_WORLD_*` of effect.rs and the group, goal and placement calls here). The C++ keeps the run's active
//! group, goal, load list, rejected units (never cleared in a run, as the original's), calamity technologies and
//! the placement's location, transformation, roll and pitch.

#![allow(clippy::unnecessary_cast, clippy::collapsible_match)]

use crate::scenario::effect::*;
use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::host::*;
use crate::scenario::placement::EE_SCENARIO_TILES_AREA;

/// Unit a: detached from its player's AI, enabled, commandable, deletable as its class.
pub const EE_SCENARIO_WORLD_TASK_PREPARE: u32 = 140;
/// A new active group of player a, in its pending groups: the run's group.
pub const EE_SCENARIO_WORLD_GROUP_NEW: u32 = 141;
/// Unit a joins the group (an idle aircraft refuelled and rearmed first).
pub const EE_SCENARIO_WORLD_GROUP_ADD_TASKED: u32 = 142;
/// Unit a's complex unit joins the group.
pub const EE_SCENARIO_WORLD_GROUP_ADD: u32 = 143;
/// The group's unit count.
pub const EE_SCENARIO_WORLD_GROUP_COUNT: u32 = 144;
/// The group's unit a's id.
pub const EE_SCENARIO_WORLD_GROUP_UNIT: u32 = 145;
/// The group's first unit's id, -1 for none.
pub const EE_SCENARIO_WORLD_GROUP_FIRST: u32 = 146;
/// The group's `KeepMobileUnits()`.
pub const EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE: u32 = 147;
/// Whether the group's citizen or fishing boat can gather from unit a.
pub const EE_SCENARIO_WORLD_GROUP_CAN_GATHER: u32 = 148;
/// The group gathers from unit a.
pub const EE_SCENARIO_WORLD_GROUP_GATHER: u32 = 149;
/// The group attacks unit a.
pub const EE_SCENARIO_WORLD_GROUP_ATTACK: u32 = 150;
/// `EECanHeal(unit a, unit b)`.
pub const EE_SCENARIO_WORLD_CAN_HEAL: u32 = 151;
/// Unit a heals unit b.
pub const EE_SCENARIO_WORLD_HEAL: u32 = 152;
/// The load list cleared.
pub const EE_SCENARIO_WORLD_LOAD_CLEAR: u32 = 153;
/// Unit a appended to the load list.
pub const EE_SCENARIO_WORLD_LOAD_PUSH: u32 = 154;
/// The load list's count.
pub const EE_SCENARIO_WORLD_LOAD_COUNT: u32 = 155;
/// The load list's entry a's id.
pub const EE_SCENARIO_WORLD_LOAD_AT: u32 = 156;
/// The run's goal: unit b's load goal of kind a (0 garrison, 1 transport) given the load list (1), or a new one
/// (0: its current goal was one, the list added to it).
pub const EE_SCENARIO_WORLD_LOAD_GOAL: u32 = 157;
/// The run's goal's `IsLoadGarrisonValid()`.
pub const EE_SCENARIO_WORLD_GOAL_VALID: u32 = 158;
/// Unit a: `ClearGoals(0)`, the run's goal set, `StartAction(b)`, `ActionChanged`.
pub const EE_SCENARIO_WORLD_SET_GOAL: u32 = 159;
/// Unit a: `ClearGoals(0)`, a clone of the run's goal set, `StartAction(b)`, `ActionChanged`.
pub const EE_SCENARIO_WORLD_SET_GOAL_CLONE: u32 = 160;
/// The run's goal deleted.
pub const EE_SCENARIO_WORLD_GOAL_DELETE: u32 = 161;
/// The load list = the group's units but transport a, filtered by the transport (the refused into the rejected).
pub const EE_SCENARIO_WORLD_LOAD_FILTER: u32 = 162;
/// The run's goal's `GetLoadUnitTile()`, packed.
pub const EE_SCENARIO_WORLD_GOAL_LOAD_TILE: u32 = 163;
/// The rejected units' count.
pub const EE_SCENARIO_WORLD_REJECTED_COUNT: u32 = 164;
/// The group finished, the rejected units its members, moving to tile (a, b).
pub const EE_SCENARIO_WORLD_REJECTED_MOVE: u32 = 165;
/// The group's `SpreadAroundTile(a, b)`.
pub const EE_SCENARIO_WORLD_GROUP_SPREAD: u32 = 166;
/// Unit a repairs unit b (`OrderRepair`, idle).
pub const EE_SCENARIO_WORLD_REPAIR: u32 = 167;
/// The group drops its resources off at unit a.
pub const EE_SCENARIO_WORLD_GROUP_DROPOFF: u32 = 168;
/// Player a's acknowledgement of the group's first unit, played for the local player.
pub const EE_SCENARIO_WORLD_ACKNOWLEDGE: u32 = 169;
/// The group patrols path point a's patrol group.
pub const EE_SCENARIO_WORLD_GROUP_PATROL_POINT: u32 = 170;
/// The group patrols between its first unit's tile and (b, c)'s f32s for player a (d: only when that tile is on the
/// map).
pub const EE_SCENARIO_WORLD_GROUP_PATROL_TO: u32 = 171;
/// Aircraft a in a container: both goal queues cleared, a move to (b, c)'s f32s queued in each (d: normal).
pub const EE_SCENARIO_WORLD_AIR_QUEUE: u32 = 172;
/// Unit a: a move to (b, c)'s f32s (d: normal) set, taking off (e 1: only outside a container).
pub const EE_SCENARIO_WORLD_LOCATION_GOAL: u32 = 173;
/// The group moves to (a, b)'s f32s (c: normal).
pub const EE_SCENARIO_WORLD_GROUP_MOVE: u32 = 174;
/// Player a's available technologies of calamity b, kept for the run (the node ids appended to the run's list).
pub const EE_SCENARIO_WORLD_CAL_NODES: u32 = 175;
/// Calamity class a kept for the run: its target kind, -1 for no class.
pub const EE_SCENARIO_WORLD_CAL_TARGET_KIND: u32 = 176;
/// The kept class as a unit calamity: `IsValidTarget(unit a, unit b)` (0 when it is none).
pub const EE_SCENARIO_WORLD_CAL_VALID_TARGET: u32 = 177;
/// The kept class as a location calamity: `IsValidLocation(unit a, b, c)` (0 when it is none).
pub const EE_SCENARIO_WORLD_CAL_VALID_LOCATION: u32 = 178;
/// Unit a casts the first kept technology of player d's whose node holds its type at unit b (-1: none) or tile
/// (c, e).
pub const EE_SCENARIO_WORLD_CAST: u32 = 179;
/// Unit a taken off the map (the Task-Area effect with an unset area).
pub const EE_SCENARIO_WORLD_AREA_TAKE_OFF_MAP: u32 = 180;
/// Off-map unit a placed in the task area (the object's area check pointed at it for the placement).
pub const EE_SCENARIO_WORLD_AREA_PLACE: u32 = 181;
/// Unit a unloads at tile (b, c)'s f32s when it is a loaded transport that can, else moves there.
pub const EE_SCENARIO_WORLD_UNLOAD: u32 = 182;
/// `gWorld.UpdateActiveUnits()`.
pub const EE_SCENARIO_WORLD_UPDATE_ACTIVE: u32 = 183;
/// The placement's location = (a, b)'s f32s.
pub const EE_SCENARIO_WORLD_AREA_LOCATION: u32 = 184;

const TASK_NORMAL: u64 = 0;
const TASK_PATROL: u64 = 1;
const TASK_ATTACK_MOVE: u64 = 2;
const TASK_LOAD: u64 = 3;
const TASK_UNLOAD: u64 = 4;
const TASK_GARRISON: u64 = 5;
/// `kSMTaskActionCalamityBase`: above it, calamity `mTaskAction - 6`.
const TASK_CALAMITY_BASE: u64 = 6;
/// No tile (`-10000`).
const OFF: i64 = -10000;

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

/// A unit the task effects take: living, complex, not rotting or dead, commandable or disabled.
fn taskable<H: Host>(h: &mut H, id: i64) -> bool {
    if !alive_complex(h, id) {
        return false;
    }
    let command = unit(h, id, EE_SCENARIO_UV_COMMAND);
    if matches!(
        command,
        EE_SCENARIO_K_ACTION_RESOURCES_ROTTING
            | EE_SCENARIO_K_ACTION_UNIT_ROTTING
            | EE_SCENARIO_K_ACTION_DEAD
            | EE_SCENARIO_K_ACTION_RESOURCES_DEPLETING
    ) {
        return false;
    }
    true
}

fn enabled<H: Host>(h: &mut H, id: i64) -> bool {
    unit(h, id, EE_SCENARIO_UV_DISABLED) != 0 || unit(h, id, EE_SCENARIO_UV_COMMANDABLE) != 0
}

/// A unit on a map tile, or an aircraft in a container.
fn on_map_or_carried<H: Host>(h: &mut H, id: i64) -> bool {
    let (x, y) = tile(h, id);
    (x != OFF && y != OFF)
        || (flies(unit(h, id, EE_SCENARIO_UV_CATEGORY)) && unit(h, id, EE_SCENARIO_UV_CONTAINER) != -1)
}

/// The group's units, each read when the loop reaches it (the group's count read each time, as the originals did).
fn for_group<H: Host>(h: &mut H, mut each: impl FnMut(&mut H, i64)) {
    let mut u = 0;
    while u < w(h, EE_SCENARIO_WORLD_GROUP_COUNT, &[]) {
        let id = w(h, EE_SCENARIO_WORLD_GROUP_UNIT, &[u]);
        each(h, id);
        u += 1;
    }
}

/// Splits units by their player's index (the originals' 16 lists; an index outside them is dropped).
fn by_player<H: Host>(h: &mut H, units: &[i64]) -> Vec<Vec<i64>> {
    let mut per = vec![Vec::new(); 16];
    for &id in units {
        let p = unit(h, id, EE_SCENARIO_UV_PLAYER_INDEX);
        if (0..16).contains(&p) {
            per[p as usize].push(id);
        }
    }
    per
}

/// Each unit of the group that may cast, given the kept class and technologies: `valid` tests it.
fn cast_group<H: Host>(h: &mut H, p: i64, target: i64, x: i64, y: i64, valid: impl Fn(&mut H, i64) -> bool) {
    for_group(h, |h, id| {
        if valid(h, id) {
            w(h, EE_SCENARIO_WORLD_CAST, &[id, target, x, p, y]);
        }
    });
}

enum Next {
    Done,
    Move,
}

/// Task-Object: the object's living units dealt round-robin to the task object's units, grouped per player.
pub fn task_units_to_object<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let (object, task) = (e.mObject, e.mTaskObject);
    if h.object(object as i32).is_none() || h.object(task as i32).is_none() {
        return;
    }
    evaluate(h, object);
    evaluate(h, task);
    let ta = e.mTaskAction as u64;
    let n = count(h, object);
    let mut sources = Vec::new();
    for i in 0..n {
        let id = unit_at(h, object, i);
        if !taskable(h, id) {
            continue;
        }
        if unit(h, id, EE_SCENARIO_UV_ON_TILE) == 0
            && (unit(h, id, EE_SCENARIO_UV_CONTAINER) == -1
                || unit(h, id, EE_SCENARIO_UV_CONTAINER_TYPE) != EE_SCENARIO_K_TYPE_AIRPORT)
        {
            continue;
        }
        if !enabled(h, id) {
            continue;
        }
        w(h, EE_SCENARIO_WORLD_TASK_PREPARE, &[id]);
        sources.push(id);
    }
    let nt = count(h, task);
    let mut targets = Vec::new();
    for j in 0..nt {
        let id = unit_at(h, task, j);
        if exists(h, id) {
            targets.push(id);
        }
    }
    if sources.is_empty() || targets.is_empty() {
        return;
    }
    let pairs = sources.len().min(targets.len());
    let mut buckets = vec![Vec::new(); pairs];
    for (s, &id) in sources.iter().enumerate() {
        buckets[s % pairs].push(id);
    }
    let normal = (ta == TASK_NORMAL) as i64;
    for (t, &target) in targets.iter().take(pairs).enumerate() {
        let (tx, ty) = tile(h, target);
        if tx == OFF || ty == OFF {
            continue;
        }
        let per = by_player(h, &buckets[t]);
        for (p, units) in per.iter().enumerate() {
            if units.is_empty() {
                continue;
            }
            let p = p as i64;
            if ta > TASK_CALAMITY_BASE {
                w(h, EE_SCENARIO_WORLD_CAL_NODES, &[p, ta as i64 - 6]);
            }
            w(h, EE_SCENARIO_WORLD_GROUP_NEW, &[p]);
            for &id in units {
                w(h, EE_SCENARIO_WORLD_GROUP_ADD_TASKED, &[id]);
            }
            let next = object_order(h, ta, p, target, (tx, ty));
            if let Next::Done = next {
                continue;
            }
            // move
            if unit(h, target, EE_SCENARIO_UV_ON_TILE) == 0 {
                continue;
            }
            let (x, y) = tile(h, target);
            let (x, y) = (bits(x as i32 as f32), bits(y as i32 as f32));
            w(h, EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE, &[]);
            let first = w(h, EE_SCENARIO_WORLD_GROUP_FIRST, &[]);
            if first == -1 {
                continue;
            }
            match ta {
                TASK_PATROL => {
                    w(h, EE_SCENARIO_WORLD_GROUP_PATROL_TO, &[p, x, y, 0]);
                }
                TASK_NORMAL | TASK_ATTACK_MOVE | TASK_LOAD | TASK_UNLOAD | TASK_GARRISON => {
                    if flies(unit(h, first, EE_SCENARIO_UV_CATEGORY))
                        && (unit(h, first, EE_SCENARIO_UV_COMPLEX) == 0
                            || unit(h, first, EE_SCENARIO_UV_COMMAND) == EE_SCENARIO_K_ACTION_IDLE)
                    {
                        for_group(h, |h, id| {
                            if unit(h, id, EE_SCENARIO_UV_CONTAINER) != -1 {
                                w(h, EE_SCENARIO_WORLD_AIR_QUEUE, &[id, x, y, normal]);
                            } else if unit(h, id, EE_SCENARIO_UV_ON_TILE) != 0 {
                                w(h, EE_SCENARIO_WORLD_LOCATION_GOAL, &[id, x, y, normal, 0]);
                            }
                        });
                    } else {
                        w(h, EE_SCENARIO_WORLD_GROUP_MOVE, &[x, y, normal]);
                    }
                }
                _ => {}
            }
        }
    }
}

/// The order of one player's group against `target` before any move: `Move` when it falls to the move.
fn object_order<H: Host>(h: &mut H, ta: u64, p: i64, target: i64, (tx, ty): (i64, i64)) -> Next {
    if ta == TASK_ATTACK_MOVE || ta == TASK_PATROL {
        return Next::Move;
    }
    if ta > TASK_CALAMITY_BASE {
        if w(h, EE_SCENARIO_WORLD_CAL_TARGET_KIND, &[ta as i64 - 6]) != 3 {
            return Next::Done;
        }
        cast_group(h, p, target, OFF, OFF, |h, id| w(h, EE_SCENARIO_WORLD_CAL_VALID_TARGET, &[id, target]) != 0);
        return Next::Done;
    }
    if w(h, EE_SCENARIO_WORLD_GROUP_CAN_GATHER, &[target]) != 0 {
        w(h, EE_SCENARIO_WORLD_GROUP_GATHER, &[target]);
        return Next::Done;
    }
    let target_player = unit(h, target, EE_SCENARIO_UV_PLAYER);
    let me = h.read(EE_SCENARIO_READ_PLAYER_ID, p as i32, 0);
    let first = w(h, EE_SCENARIO_WORLD_GROUP_FIRST, &[]);
    if target_player != me
        && h.read(EE_SCENARIO_READ_ENEMY, p as i32, target_player as i32) != 0
        && first != -1
        && unit(h, first, EE_SCENARIO_UV_CATEGORY) != EE_SCENARIO_K_CATEGORY_NAVAL_CITIZEN
    {
        if ta != TASK_NORMAL {
            return Next::Move;
        }
        w(h, EE_SCENARIO_WORLD_GROUP_ATTACK, &[target]);
        return Next::Done;
    }
    let target_complex = unit(h, target, EE_SCENARIO_UV_COMPLEX) != 0;
    if target_complex && first != -1 && w(h, EE_SCENARIO_WORLD_CAN_HEAL, &[first, target]) != 0 {
        for_group(h, |h, id| {
            if w(h, EE_SCENARIO_WORLD_CAN_HEAL, &[id, target]) != 0 {
                w(h, EE_SCENARIO_WORLD_HEAL, &[id, target]);
            }
        });
        return Next::Done;
    }
    if target_player != me {
        return Next::Move;
    }
    if ta == TASK_GARRISON && target_complex && unit(h, target, EE_SCENARIO_UV_GARRISON) > 0 {
        w(h, EE_SCENARIO_WORLD_GROUP_SPREAD, &[tx, ty]);
        w(h, EE_SCENARIO_WORLD_LOAD_CLEAR, &[]);
        for_group(h, |h, id| {
            w(h, EE_SCENARIO_WORLD_LOAD_PUSH, &[id]);
        });
        if w(h, EE_SCENARIO_WORLD_LOAD_COUNT, &[]) <= 0 {
            return Next::Done;
        }
        let mut delete = false;
        if w(h, EE_SCENARIO_WORLD_LOAD_GOAL, &[0, target]) != 0 {
            if w(h, EE_SCENARIO_WORLD_GOAL_VALID, &[]) != 0 {
                w(h, EE_SCENARIO_WORLD_SET_GOAL, &[target, EE_SCENARIO_K_ACTION_PREPARE_TO_MOVE]);
            } else {
                delete = true;
            }
        }
        if w(h, EE_SCENARIO_WORLD_GOAL_VALID, &[]) != 0 {
            load_units_follow(h);
            if w(h, EE_SCENARIO_WORLD_LOAD_COUNT, &[]) > 0 {
                delete = false;
            }
        }
        if delete {
            w(h, EE_SCENARIO_WORLD_GOAL_DELETE, &[]);
        }
        return Next::Done;
    }
    if ta == TASK_LOAD && target_complex {
        if unit(h, target, EE_SCENARIO_UV_TRANSPORT) == 0 {
            return Next::Move;
        }
        w(h, EE_SCENARIO_WORLD_LOAD_FILTER, &[target]);
        let mut done = false;
        let destination = if w(h, EE_SCENARIO_WORLD_LOAD_COUNT, &[]) > 0 {
            done = true;
            if w(h, EE_SCENARIO_WORLD_LOAD_GOAL, &[1, target]) != 0 {
                w(h, EE_SCENARIO_WORLD_SET_GOAL, &[target, EE_SCENARIO_K_ACTION_PREPARE_TO_MOVE]);
            }
            let d = unpack(w(h, EE_SCENARIO_WORLD_GOAL_LOAD_TILE, &[]));
            load_units_follow(h);
            d
        } else {
            tile(h, target)
        };
        if w(h, EE_SCENARIO_WORLD_REJECTED_COUNT, &[]) > 0 {
            w(h, EE_SCENARIO_WORLD_REJECTED_MOVE, &[destination.0, destination.1]);
        }
        return if done { Next::Done } else { Next::Move };
    }
    let target_type = unit(h, target, EE_SCENARIO_UV_TYPE);
    if unit(h, target, EE_SCENARIO_UV_LAND_BUILDING) != 0 && unit(h, target, EE_SCENARIO_UV_DAMAGED) != 0 {
        w(h, EE_SCENARIO_WORLD_GROUP_SPREAD, &[tx, ty]);
        let n = w(h, EE_SCENARIO_WORLD_GROUP_COUNT, &[]);
        for u in 0..n {
            let id = w(h, EE_SCENARIO_WORLD_GROUP_UNIT, &[u]);
            w(h, EE_SCENARIO_WORLD_REPAIR, &[id, target]);
        }
    } else if unit(h, target, EE_SCENARIO_UV_DROP_SITE) != 0
        || target_type == EE_SCENARIO_K_TYPE_DOCK
        || target_type == EE_SCENARIO_K_TYPE_NAVY_YARD
    {
        w(h, EE_SCENARIO_WORLD_GROUP_DROPOFF, &[target]);
        if w(h, EE_SCENARIO_WORLD_GROUP_COUNT, &[]) != 0 {
            w(h, EE_SCENARIO_WORLD_ACKNOWLEDGE, &[p]);
        }
        return Next::Done;
    } else if target_type == EE_SCENARIO_K_TYPE_PATH_POINT {
        w(h, EE_SCENARIO_WORLD_GROUP_PATROL_POINT, &[target]);
    } else {
        return Next::Move;
    }
    if w(h, EE_SCENARIO_WORLD_GROUP_FIRST, &[]) != -1 {
        w(h, EE_SCENARIO_WORLD_ACKNOWLEDGE, &[p]);
    }
    Next::Done
}

/// Each unit of the load list takes a clone of the run's goal (the list's count read each time).
fn load_units_follow<H: Host>(h: &mut H) {
    let mut k = 0;
    while k < w(h, EE_SCENARIO_WORLD_LOAD_COUNT, &[]) {
        let id = w(h, EE_SCENARIO_WORLD_LOAD_AT, &[k]);
        w(h, EE_SCENARIO_WORLD_SET_GOAL_CLONE, &[id, EE_SCENARIO_K_ACTION_PREPARE_TO_MOVE]);
        k += 1;
    }
}

/// Task-Area: the object's living units sent to a random tile of the task area per player (move, patrol, unload,
/// calamity); off-map units placed in the area; with the area unset, every unit taken off the map.
pub fn task_units_to_area<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let (object, area) = (e.mObject, e.mTaskArea);
    let state = area_state(h, area);
    if let (Some(_), Some((unset, off))) = (h.object(object as i32), state) {
        let mut target = (0i64, 0i64);
        if !unset && !off {
            let tiles = h.tiles(EE_SCENARIO_TILES_AREA, area as i32, 0, 0);
            // an area with no tiles (anchored to a unit that is gone) has no destination
            if tiles.is_empty() {
                return;
            }
            let index = h.read(EE_SCENARIO_READ_RANDOM, 0, tiles.len() as i32 - 1);
            let (x, y) = tiles[index as usize];
            target = (bits(x as f32), bits(y as f32));
            w(h, EE_SCENARIO_WORLD_AREA_LOCATION, &[target.0, target.1]);
        }
        evaluate(h, object);
        let n = count(h, object);
        let mut moving = Vec::new();
        for i in 0..n {
            let id = unit_at(h, object, i);
            if !taskable(h, id) || !enabled(h, id) {
                continue;
            }
            w(h, EE_SCENARIO_WORLD_TASK_PREPARE, &[id]);
            if unset {
                w(h, EE_SCENARIO_WORLD_AREA_TAKE_OFF_MAP, &[id]);
            } else if on_map_or_carried(h, id) {
                moving.push(id);
            } else {
                w(h, EE_SCENARIO_WORLD_AREA_PLACE, &[id]);
            }
        }
        let per = by_player(h, &moving);
        let ta = e.mTaskAction as u64;
        let normal = (ta == TASK_NORMAL) as i64;
        let (tx, ty) = target;
        let (ix, iy) = (f32::from_bits(tx as u32) as i64, f32::from_bits(ty as u32) as i64);
        for (p, units) in per.iter().enumerate() {
            if unset || area_state(h, area).is_some_and(|s| s.1) {
                break;
            }
            if units.is_empty() {
                continue;
            }
            let p = p as i64;
            if ta > TASK_CALAMITY_BASE {
                w(h, EE_SCENARIO_WORLD_CAL_NODES, &[p, ta as i64 - 6]);
            }
            w(h, EE_SCENARIO_WORLD_GROUP_NEW, &[p]);
            for &id in units {
                w(h, EE_SCENARIO_WORLD_GROUP_ADD, &[id]);
            }
            w(h, EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE, &[]);
            if w(h, EE_SCENARIO_WORLD_GROUP_COUNT, &[]) == 0 {
                continue;
            }
            let first = w(h, EE_SCENARIO_WORLD_GROUP_FIRST, &[]);
            match ta {
                TASK_NORMAL | TASK_ATTACK_MOVE => {
                    if flies(unit(h, first, EE_SCENARIO_UV_CATEGORY))
                        && (unit(h, first, EE_SCENARIO_UV_COMPLEX) == 0
                            || unit(h, first, EE_SCENARIO_UV_COMMAND) == EE_SCENARIO_K_ACTION_IDLE)
                    {
                        for_group(h, |h, id| {
                            w(h, EE_SCENARIO_WORLD_LOCATION_GOAL, &[id, tx, ty, normal, 1]);
                        });
                    } else {
                        w(h, EE_SCENARIO_WORLD_GROUP_MOVE, &[tx, ty, normal]);
                    }
                }
                TASK_PATROL => {
                    w(h, EE_SCENARIO_WORLD_GROUP_PATROL_TO, &[p, tx, ty, 1]);
                }
                TASK_UNLOAD => {
                    for_group(h, |h, id| {
                        w(h, EE_SCENARIO_WORLD_UNLOAD, &[id, tx, ty]);
                    });
                }
                _ if ta > TASK_CALAMITY_BASE => {
                    if w(h, EE_SCENARIO_WORLD_CAL_TARGET_KIND, &[ta as i64 - 6]) == 2 {
                        cast_group(h, p, -1, ix, iy, |h, id| {
                            w(h, EE_SCENARIO_WORLD_CAL_VALID_LOCATION, &[id, ix, iy]) != 0
                        });
                    }
                }
                _ => {}
            }
        }
    }
    w(h, EE_SCENARIO_WORLD_UPDATE_ACTIVE, &[]);
}
