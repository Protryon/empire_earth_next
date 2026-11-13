//! The combat goals on the core lane's generic `EEUGRust` (R5b.2, the twelfth round): ee_units' `ee_units_rust_goal`
//! hands the kinds it does not own to [`rust_goal`] (registered by ee_rust at start-up), with the same operation
//! numbers (`EE_UNITS_GOAL_*`) and the same C++ host (`EeUnitsGoalHost`, mirrored as `EeCombatGoalTable`; the combat
//! goals' own operations are its cases from 1000, EEUGRustCombat.cpp). Here each operation of the attack on the ground
//! (`kGoalAttackGround`), on a unit (`kGoalAttackUnit`) and of the heal goal (`kGoalHealUnit`) runs its body
//! (goal_objects.rs, goal_attack.rs, goal_heal.rs) and the texts are formatted (the debug line and text, the
//! out-of-synch dump, appended through the host).

use std::ffi::{c_void, CStr};

use crate::goal_attack::{self as attack, attack_of};
use crate::goal_heal::{self as heal, heal_of, ACTION_TURN_TO_FACE_HEALEE, GOAL_HEAL_UNIT};
use crate::goal_objects::{
    goal_drop, ground_checksum, ground_create, ground_destination, ground_load, ground_save, ground_target_bits,
    with_goals, EeCombatGoalArgs, EeCombatGoalIo, EeCombatGoalTable, GoalHost, TableHost, ACTION_CHECK_RANGE,
    ACTION_INITIAL_ATTACK_STATE,
};

// ee_units rust_goal.rs's operations (the same numbers; ee_rust's test checks them)
pub(crate) const OP_DESTROY: i32 = 1;
pub(crate) const OP_CLONE: i32 = 2;
pub(crate) const OP_DUMP: i32 = 3;
pub(crate) const OP_CHECKSUM: i32 = 4;
pub(crate) const OP_DEBUG_LINE: i32 = 5;
pub(crate) const OP_DEBUG_TEXT: i32 = 6;
pub(crate) const OP_LOCATION: i32 = 7;
pub(crate) const OP_DESTINATION: i32 = 8;
pub(crate) const OP_START_ACTION: i32 = 9;
pub(crate) const OP_SET_OWNER: i32 = 10;
pub(crate) const OP_STEP: i32 = 11;
pub(crate) const OP_LOAD: i32 = 12;
pub(crate) const OP_SAVE: i32 = 13;
pub(crate) const OP_REPAIR: i32 = 14;
pub(crate) const OP_START: i32 = 15;
pub(crate) const OP_EVALUATE: i32 = 16;
pub(crate) const OP_REPLACE: i32 = 17;
pub(crate) const OP_MAX_PATH_ATTEMPTS: i32 = 18;
pub(crate) const OP_SUITS: i32 = 19;
pub(crate) const OP_SET_PATH_ATTEMPTS: i32 = 22;
pub(crate) const OP_WORK: i32 = 300;
// and its host's: the new goal object, a text appended, the unit id, the unit's tile, its hit points
pub(crate) const HOST_NEW_GOAL: i32 = 16;
pub(crate) const HOST_APPEND: i32 = 20;
pub(crate) const HOST_UNIT_ID: i32 = 3;
pub(crate) const HOST_UNIT_TILE: i32 = 4;
pub(crate) const HOST_HIT_POINTS: i32 = 64;

// the combat goals' own operations (EEUGRustCombat.cpp's accessors call them): `io.f` the point
/// a new attack on the ground at `io.f`
pub const EE_COMBAT_GOAL_NEW_GROUND: i32 = 200;
/// a new attack: `a` the attacker, `b` the target, `io.i[0]` the convert flag; `io.i[0]` 2: the load's empty one
pub const EE_COMBAT_GOAL_NEW_ATTACK: i32 = 201;
/// `SetTarget(a)`, `RetargetGatePart`, `UpdateTargetTile`
pub const EE_COMBAT_GOAL_SET_TARGET: i32 = 202;
pub const EE_COMBAT_GOAL_RETARGET_GATE: i32 = 203;
pub const EE_COMBAT_GOAL_UPDATE_TILE: i32 = 204;
/// a new heal goal on the healee `a`; `io.i[0]` 2: the load's empty one
pub const EE_COMBAT_GOAL_NEW_HEAL: i32 = 205;

// the host operations of this module (goal_objects.rs's and goal_attack.rs's numbering goes on)
/// `EECombatStep(io.i[0], a, b, io.i[1])`: the kind, the unit, the goal, the target
pub const EE_COMBAT_G_STEP: i32 = 1050;
/// `EECombatCondition(io.i[0], a, io.i[1], io.i[2], b, NULL)`: the kind, the unit, the event, the target, the goal
pub const EE_COMBAT_G_EVALUATE: i32 = 1051;
/// the unit's class has a projectile class
pub const EE_COMBAT_G_PROJECTILE_CLASS: i32 = 1052;
/// TEST ONLY: `EECombatCaptureGoal(io.i[0], io.i[1], a, io.i[2], io.i[3], point io.i[4..7] (bits), extras io.i[7]
/// low and high words)`
pub const EE_COMBAT_G_CAPTURE: i32 = 1053;
/// the goal object `a`'s `mPathAttempts`
pub const EE_COMBAT_G_PATH_ATTEMPTS: i32 = 1054;
/// the save version `io.i[0]`.`io.i[1]`: 1 the live state (`EESaveHasLiveState`), 2 the goal ghosts
pub const EE_COMBAT_G_SAVE_FLAGS: i32 = 1055;
/// the unit's class's name (a `const char*`, 0 without a class)
pub const EE_COMBAT_G_CLASS_NAME: i32 = 1056;
/// the unit's `mStats->mMaxHitPoints`
pub const EE_COMBAT_G_MAX_HIT_POINTS: i32 = 1057;
/// the unit's `mActiveGroup != NULL`
pub const EE_COMBAT_G_HAS_GROUP: i32 = 1058;

/// `kGoalAttackUnit`, `kGoalAttackGround`.
pub const GOAL_ATTACK_UNIT: u8 = 2;
pub const GOAL_ATTACK_GROUND: u8 = 3;
// ee/EECombatGoals.h's kinds and CBGO functions
const COMBAT_GOAL_ATTACK_UNIT: i64 = crate::goals::EE_COMBAT_GOAL_ATTACK_UNIT as i64;
const COMBAT_GOAL_ATTACK_GROUND: i64 = crate::goals::EE_COMBAT_GOAL_ATTACK_GROUND as i64;
const COMBAT_GOAL_HEAL: i64 = crate::goals::EE_COMBAT_GOAL_HEAL as i64;
const GO_DESTINATION: i64 = 1;
const GO_SUITS: i64 = 2;

fn args(values: &[i64]) -> EeCombatGoalArgs {
    let mut a = [0i64; 8];
    a[..values.len()].copy_from_slice(values);
    a
}

fn bits_of(p: [f32; 3]) -> [u32; 3] {
    [p[0].to_bits(), p[1].to_bits(), p[2].to_bits()]
}

fn floats(p: [u32; 3]) -> [f32; 3] {
    [f32::from_bits(p[0]), f32::from_bits(p[1]), f32::from_bits(p[2])]
}

fn append<H: GoalHost + ?Sized>(h: &mut TextHost<'_, H>, text: &str) {
    (h.append)(text);
}

/// The texts' sink and the reads they make.
struct TextHost<'a, H: GoalHost + ?Sized> {
    host: &'a mut H,
    append: &'a mut dyn FnMut(&str),
}

impl<H: GoalHost + ?Sized> TextHost<'_, H> {
    fn id(&mut self, unit: u64) -> u32 {
        self.host.op(HOST_UNIT_ID, unit, &mut [0; 8]) as u32
    }
    fn name(&mut self, unit: u64) -> String {
        let p = self.host.op(EE_COMBAT_G_CLASS_NAME, unit, &mut [0; 8]) as usize;
        if p == 0 {
            return String::new();
        }
        // SAFETY: the host answers the class's name, a C string that lives as long as the class
        unsafe { CStr::from_ptr(p as *const std::ffi::c_char) }.to_string_lossy().into_owned()
    }
    fn tile(&mut self, unit: u64) -> (i32, i32) {
        let mut a = [0i64; 8];
        self.host.op(HOST_UNIT_TILE, unit, &mut a);
        (a[0] as i32, a[1] as i32)
    }
    fn hit_points(&mut self, unit: u64) -> i32 {
        self.host.op(HOST_HIT_POINTS, unit, &mut [0; 8]) as i32
    }
}

/// The ground goal's texts: the out-of-synch dump, the debug line and text.
fn ground_text<H: GoalHost + ?Sized>(op: i32, goal: u64, h: &mut TextHost<'_, H>) {
    let t = floats(ground_target_bits(goal));
    let text = match op {
        OP_DUMP => format!(
            "OUT OF SYNCH DUMP:  Attack Ground Location({:6.6}, {:6.6}, {:6.6})\n",
            f64::from(t[0]),
            f64::from(t[1]),
            f64::from(t[2])
        ),
        OP_DEBUG_LINE => format!(" attack grnd {:.2},{:.2}:", f64::from(t[0]), f64::from(t[1])),
        _ => format!("Attack Ground {:.2},{:.2}\n", f64::from(t[0]), f64::from(t[1])),
    };
    append(h, &text);
}

/// The attack goal's texts.
fn attack_text<H: GoalHost + ?Sized>(op: i32, goal: u64, h: &mut TextHost<'_, H>) {
    let a = attack_of(goal);
    match op {
        OP_DUMP => {
            let l = floats(a.location);
            append(
                h,
                &format!(
                    "EEUGAttackUnit::DumpOOSInformation - The Actual Destination is ({:6.6}, {:6.6}, {:6.6})\n",
                    f64::from(l[0]),
                    f64::from(l[1]),
                    f64::from(l[2])
                ),
            );
            let (id, name) = (h.id(a.attacker), h.name(a.attacker));
            append(h, &format!("EEUGAttackUnit::DumpOOSInformation - The attacking unit #{id:08x} is a {name}\n"));
            let (id, name) =
                if a.target != 0 { (h.id(a.target), h.name(a.target)) } else { (0xffff_ffff, "???".to_string()) };
            append(h, &format!("EEUGAttackUnit::DumpOOSInformation - The target unit #{id:08x} is a {name}\n"));
            append(
                h,
                &format!(
                    "EEUGAttackUnit::DumpOOSInformation - The last known target X,Y({}, {})\n",
                    a.tile[0], a.tile[1]
                ),
            );
            append(h, &format!("EEUGAttackUnit::DumpOOSInformation - Is this a convert goal? {:2}\n", a.convert != 0));
        }
        OP_DEBUG_LINE => {
            append(h, "attack");
            if a.target != 0 {
                let (id, hp) = (h.id(a.target), h.hit_points(a.target));
                append(h, &format!(" unit (uid {}, hp {}):", id & 0xff_ffff, hp));
            }
        }
        _ => {
            append(h, "Attack\n");
            if a.target == 0 {
                return;
            }
            let (name, tile, hp) = (h.name(a.target), h.tile(a.target), h.hit_points(a.target));
            let max = h.host.op(EE_COMBAT_G_MAX_HIT_POINTS, a.target, &mut [0; 8]) as i32;
            append(
                h,
                &format!(
                    "Target Unit: {} at ({},{})\nTarget Health: {}/{} ({:.2}%)\n",
                    name,
                    tile.0,
                    tile.1,
                    hp,
                    max,
                    f64::from(hp as f32 / max as f32)
                ),
            );
        }
    }
}

/// The heal goal's texts: the out-of-synch dump (the base goal's that followed is empty), the debug line (the
/// location goal's: `EEUGLocation::GetDebugLine`, its auto-waypoint flag false) and text.
fn heal_text<H: GoalHost + ?Sized>(op: i32, goal: u64, h: &mut TextHost<'_, H>) {
    let x = heal_of(goal);
    match op {
        OP_DUMP => {
            let id = if x.target != 0 { h.id(x.target) } else { 0xffff_ffff };
            append(
                h,
                &format!(
                    "OUT OF SYNCH DUMP:  EEUGHealUnit - Healee unit id is #{id:08x}
"
                ),
            );
            append(
                h,
                &format!(
                    "OUT OF SYNCH DUMP:  EEUGHealUnit - Elasped Time in Milliseconds {}
",
                    x.elapsed as i32
                ),
            );
        }
        OP_DEBUG_LINE => {
            let l = floats(x.location);
            append(h, &format!("loc {:.2},{:.2},{:.2}: auto(0)", f64::from(l[0]), f64::from(l[1]), f64::from(l[2])));
        }
        _ => {
            append(
                h, "Heal
",
            );
            if x.target == 0 {
                return;
            }
            let (name, tile, hp) = (h.name(x.target), h.tile(x.target), h.hit_points(x.target));
            let max = h.host.op(EE_COMBAT_G_MAX_HIT_POINTS, x.target, &mut [0; 8]) as i32;
            append(
                h,
                &format!(
                    "Target Unit: {} at ({},{})
Target Health: {}/{} ({:.2}%)
",
                    name,
                    tile.0,
                    tile.1,
                    hp,
                    max,
                    f64::from(100.0f32 * hp as f32 / max as f32)
                ),
            );
        }
    }
}

/// One operation of a heal goal (see [`body`]).
#[allow(clippy::too_many_arguments)]
fn heal_body<H: GoalHost>(
    op: i32,
    goal: u64,
    a: u64,
    b: u64,
    ints: &mut [i64; 8],
    point: &mut [f32; 3],
    h: &mut H,
    text: &mut dyn FnMut(&str),
) -> i64 {
    let target = heal_of(goal).target;
    match op {
        EE_COMBAT_GOAL_NEW_HEAL => {
            if ints[0] == 2 {
                heal::create_empty(goal);
            } else {
                heal::create(h, goal, a);
            }
            0
        }
        OP_DESTROY => {
            heal::destroy(h, goal);
            goal_drop(goal);
            0
        }
        OP_CLONE => {
            let copy = h.op_ab(HOST_NEW_GOAL, u64::from(GOAL_HEAL_UNIT), b, &mut [0; 8]) as u64;
            heal::create(h, copy, target);
            copy as i64
        }
        OP_DUMP | OP_DEBUG_LINE | OP_DEBUG_TEXT => {
            heal_text(op, goal, &mut TextHost { host: h, append: text });
            0
        }
        OP_CHECKSUM => i64::from(heal::checksum(h, goal)),
        OP_LOCATION => {
            *point = floats(heal::location(h, goal, bits_of(*point)));
            0
        }
        OP_DESTINATION => {
            *point = floats(heal::destination(h, goal, a, bits_of(*point)));
            0
        }
        OP_START_ACTION => ACTION_TURN_TO_FACE_HEALEE,
        OP_STEP => {
            h.op(EE_COMBAT_G_STEP, a, &mut args(&[COMBAT_GOAL_HEAL, target as i64, goal as i64]));
            0
        }
        OP_EVALUATE => {
            h.op(EE_COMBAT_G_EVALUATE, a, &mut args(&[COMBAT_GOAL_HEAL, b as i64, target as i64, goal as i64]))
        }
        OP_LOAD => i64::from(heal::load(h, goal)),
        OP_SAVE => i64::from(heal::save(h, goal)),
        OP_REPAIR => i64::from(heal::repair(h, goal)),
        OP_REPLACE => {
            heal::replace_unit(h, goal, a, b);
            0
        }
        OP_MAX_PATH_ATTEMPTS => 3,
        OP_SUITS => {
            let result = heal::suits(h, goal, a);
            h.op(
                EE_COMBAT_G_CAPTURE,
                a,
                &mut args(&[GO_SUITS, i64::from(GOAL_HEAL_UNIT), target as i64, i64::from(result)]),
            );
            i64::from(result)
        }
        OP_WORK => {
            heal::work(h, goal, a);
            0
        }
        // the start (empty), the owner, the path attempts, the base goal's empty slots
        _ => 0,
    }
}

/// One operation of a combat goal of `kind` over the goals' host `h` (the texts appended through `text`; the
/// location and destination in `point`). The operation's result.
#[allow(clippy::too_many_arguments)]
pub fn body<H: GoalHost>(
    op: i32,
    kind: u8,
    goal: u64,
    a: u64,
    b: u64,
    ints: &mut [i64; 8],
    point: &mut [f32; 3],
    h: &mut H,
    text: &mut dyn FnMut(&str),
) -> i64 {
    if kind == GOAL_HEAL_UNIT {
        return heal_body(op, goal, a, b, ints, point, h, text);
    }
    let ground = kind == GOAL_ATTACK_GROUND;
    let combat_kind = if ground { COMBAT_GOAL_ATTACK_GROUND } else { COMBAT_GOAL_ATTACK_UNIT };
    match op {
        EE_COMBAT_GOAL_NEW_GROUND => {
            ground_create(goal, *point);
            0
        }
        EE_COMBAT_GOAL_NEW_ATTACK => {
            if ints[0] == 2 {
                attack::create_empty(goal);
            } else {
                attack::create(h, goal, a, b, ints[0] != 0);
            }
            0
        }
        EE_COMBAT_GOAL_SET_TARGET => {
            attack::set_target(h, goal, a);
            0
        }
        EE_COMBAT_GOAL_RETARGET_GATE => {
            attack::retarget_gate_part(h, goal);
            0
        }
        EE_COMBAT_GOAL_UPDATE_TILE => {
            attack::update_target_tile(h, goal);
            0
        }
        OP_DESTROY => {
            if !ground {
                attack::destroy(h, goal);
            }
            goal_drop(goal);
            0
        }
        OP_CLONE => {
            let copy = h.op_ab(HOST_NEW_GOAL, u64::from(kind), b, &mut [0; 8]) as u64;
            if ground {
                ground_create(copy, floats(ground_target_bits(goal)));
            } else {
                let x = attack_of(goal);
                attack::create(h, copy, x.attacker, x.target, x.convert != 0);
            }
            copy as i64
        }
        OP_DUMP | OP_DEBUG_LINE | OP_DEBUG_TEXT => {
            let mut t = TextHost { host: h, append: text };
            if ground {
                ground_text(op, goal, &mut t);
            } else {
                // (the base goal's dump that followed is empty)
                attack_text(op, goal, &mut t);
            }
            0
        }
        OP_CHECKSUM => i64::from(if ground { ground_checksum(goal) } else { attack::checksum(h, goal) }),
        OP_LOCATION => {
            *point = floats(if ground { ground_target_bits(goal) } else { attack_of(goal).location });
            0
        }
        OP_DESTINATION => {
            let out =
                if ground { ground_destination(h, goal, a) } else { attack::destination(h, goal, a, bits_of(*point)) };
            *point = floats(out);
            let (target, tile) = if ground { (0, [0, 0]) } else { (attack_of(goal).target, attack_of(goal).tile) };
            let group = h.op(EE_COMBAT_G_HAS_GROUP, a, &mut [0; 8]);
            let extras = i64::from(tile[0] as u32) | (i64::from(tile[1] as u32) << 32);
            h.op(
                EE_COMBAT_G_CAPTURE,
                a,
                &mut args(&[
                    GO_DESTINATION,
                    i64::from(kind),
                    target as i64,
                    group,
                    i64::from(out[0]),
                    i64::from(out[1]),
                    i64::from(out[2]),
                    extras,
                ]),
            );
            0
        }
        OP_START_ACTION => i64::from(ACTION_CHECK_RANGE),
        OP_SET_OWNER => {
            if !ground {
                let mut x = attack_of(goal);
                x.attacker = a;
                with_goals(|g| g.attack.insert(goal, x));
            }
            0
        }
        OP_STEP => {
            let target = if ground { 0 } else { attack_of(goal).target };
            h.op(EE_COMBAT_G_STEP, a, &mut args(&[combat_kind, target as i64, goal as i64]));
            0
        }
        OP_EVALUATE => {
            let target = if ground { 0 } else { attack_of(goal).target };
            h.op(EE_COMBAT_G_EVALUATE, a, &mut args(&[combat_kind, b as i64, target as i64, goal as i64]))
        }
        OP_LOAD => {
            if ground {
                i64::from(ground_load(h, goal))
            } else {
                let flags = h.op(EE_COMBAT_G_SAVE_FLAGS, 0, &mut args(&[a as i64, b as i64]));
                i64::from(attack::load(h, goal, a as u32, b as u32, flags & 1 != 0))
            }
        }
        OP_SAVE => {
            if ground {
                i64::from(ground_save(h, goal))
            } else {
                // the version's flags (`a`, `b`: the major and minor version)
                let flags = h.op(EE_COMBAT_G_SAVE_FLAGS, 0, &mut args(&[a as i64, b as i64]));
                i64::from(attack::save(h, goal, flags & 1 != 0, flags & 2 != 0))
            }
        }
        OP_REPAIR => {
            if ground {
                0
            } else {
                let flags = h.op(EE_COMBAT_G_SAVE_FLAGS, 0, &mut args(&[a as i64, b as i64]));
                i64::from(attack::repair(h, goal, flags & 1 != 0))
            }
        }
        OP_START => {
            h.op(crate::goal_objects::EE_COMBAT_G_START_ACTION, a, &mut args(&[ACTION_INITIAL_ATTACK_STATE]));
            0
        }
        OP_REPLACE => {
            if !ground {
                attack::replace_unit(h, goal, a, b);
            }
            0
        }
        OP_MAX_PATH_ATTEMPTS => {
            if ground {
                3
            } else {
                h.op(EE_COMBAT_G_PATH_ATTEMPTS, goal, &mut [0; 8]) + 1
            }
        }
        OP_SUITS => {
            let (result, target, point) = if ground {
                let r = h.op(EE_COMBAT_G_PROJECTILE_CLASS, a, &mut [0; 8]) != 0;
                (r, 0, ground_target_bits(goal))
            } else {
                (attack::suits(h, goal, a), attack_of(goal).target, [0; 3])
            };
            h.op(
                EE_COMBAT_G_CAPTURE,
                a,
                &mut args(&[
                    GO_SUITS,
                    i64::from(kind),
                    target as i64,
                    i64::from(result),
                    i64::from(point[0]),
                    i64::from(point[1]),
                    i64::from(point[2]),
                ]),
            );
            i64::from(result)
        }
        OP_SET_PATH_ATTEMPTS => {
            if !ground && a == 0 {
                let mut x = attack_of(goal);
                x.sidestep = 0;
                with_goals(|g| g.attack.insert(goal, x));
            }
            0
        }
        _ => 0,
    }
}

/// ee_units' `ee_units_rust_goal` for the combat kinds (registered by ee_rust): `io` an `EeUnitsGoalIo`, `host` an
/// `EeUnitsGoalHost` (the same layouts as `EeCombatGoalIo`, `EeCombatGoalTable`).
///
/// # Safety
/// `io` and `host` point to valid ee_units blocks for the call.
pub unsafe fn rust_goal(op: i32, kind: u8, goal: u64, a: u64, b: u64, io: *mut c_void, host: *const c_void) -> i64 {
    // SAFETY: the caller's contract (the same layouts)
    let (io, table) = unsafe { (&mut *io.cast::<EeCombatGoalIo>(), &*host.cast::<EeCombatGoalTable>()) };
    let mut h = TableHost(table);
    let mut ints = io.i;
    let mut point = [io.f[0], io.f[1], io.f[2]];
    let mut texts: Vec<String> = Vec::new();
    let result = body(op, kind, goal, a, b, &mut ints, &mut point, &mut h, &mut |t: &str| texts.push(t.to_string()));
    for t in texts {
        let mut block = EeCombatGoalIo { text: t.as_ptr(), len: t.len(), ..Default::default() };
        h.call(HOST_APPEND, 0, 0, &mut block);
    }
    io.i = ints;
    io.f[..3].copy_from_slice(&point);
    result
}

#[cfg(test)]
#[path = "goal_dispatch_tests.rs"]
mod tests;
