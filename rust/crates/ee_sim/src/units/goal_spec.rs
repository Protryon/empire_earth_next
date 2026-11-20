//! The unit host's goal constructions (`ee_units_host_goal_new`, EEUnitHost.cpp until the units' round 4): a group
//! command's `EeGoalSpec` made into a goal object here, with the same `EEUGRust::New*` operations the C++ called
//! (each a [`goal_new`] and one body call), and the goal's release (`delete`: the kind's destroy, the object).
//!
//! [`plan`] is the pure part: what a spec makes (the kind, the argument, the body operation and its handles and
//! block), the units resolved by id; [`make`] carries a plan out.

use std::ffi::c_void;

use crate::combat::goal_dispatch::{EE_COMBAT_GOAL_NEW_ATTACK, EE_COMBAT_GOAL_NEW_GROUND, EE_COMBAT_GOAL_NEW_HEAL};
use crate::units::build::GOAL_BUILD_REPAIR;
use crate::units::gather::GOAL_GATHER;
#[cfg(not(test))]
use crate::units::host::EE_UNIT_ID_NONE;
use crate::units::host::{
    EeGoalSpec, EE_GOAL_CTOR_ATTACK_GROUND, EE_GOAL_CTOR_ATTACK_UNIT, EE_GOAL_CTOR_BUILD_REPAIR,
    EE_GOAL_CTOR_CAST_CALAMITY, EE_GOAL_CTOR_EXPLORE, EE_GOAL_CTOR_GATHER, EE_GOAL_CTOR_HEAL_UNIT,
    EE_GOAL_CTOR_LOCATION, EE_GOAL_CTOR_LOCATION_2D, EE_GOAL_CTOR_LOCATION_XY, EE_GOAL_CTOR_PATROL,
    EE_GOAL_CTOR_PATROL_ROUTE, EE_GOAL_CTOR_UNLOAD_TRANSPORT,
};
use crate::units::location::GOAL_LOCATION;
use crate::units::rust_goal::{
    EeUnitsGoalIo, EE_UNITS_GOAL_NEW_BUILD, EE_UNITS_GOAL_NEW_CALAMITY, EE_UNITS_GOAL_NEW_EXPLORE,
    EE_UNITS_GOAL_NEW_GATHER, EE_UNITS_GOAL_NEW_LOCATION, EE_UNITS_GOAL_NEW_PATROL, EE_UNITS_GOAL_NEW_UNLOAD,
    EE_UNITS_GOAL_PATROL_NEW_EMPTY, GOAL_CAST_CALAMITY, GOAL_EXPLORE, GOAL_HEAL_UNIT, GOAL_PATROL,
    GOAL_UNLOAD_TRANSPORT,
};

/// `kGoalAttackUnit`, `kGoalAttackGround` (ee/EEUnitGoal.h).
pub(crate) const GOAL_ATTACK_UNIT: u8 = 2;
pub(crate) const GOAL_ATTACK_GROUND: u8 = 3;

/// What a spec makes: `EEUnitGoal::Make(kind, arg)`, then the body `op` with `a`, `b` and the block (`i`, `f`); a
/// location goal's group movement set afterwards (`SetUseGroupMovement`) when given.
#[derive(Debug, Clone, PartialEq)]
pub struct GoalPlan {
    pub kind: u8,
    pub arg: bool,
    pub op: i32,
    pub a: u64,
    pub b: u64,
    pub i: [i64; 4],
    pub f: [f32; 4],
    pub group_movement: Option<bool>,
}

impl GoalPlan {
    fn new(kind: u8, arg: bool, op: i32, a: u64, b: u64) -> Self {
        GoalPlan { kind, arg, op, a, b, i: [0; 4], f: [0.0; 4], group_movement: None }
    }
}

/// The C++ `(long)` of a value as the block holds it (32-bit `long`, sign-extended).
fn long(v: i64) -> i64 {
    i64::from(v as i32)
}

/// The location goal's block (`NewLocationGoal`: forced move, facing, auto waypoint, group path state; the point
/// and the yaw).
fn location(arg: bool, forced: bool, face: bool, point: [f32; 3], yaw: f32) -> GoalPlan {
    let mut p = GoalPlan::new(GOAL_LOCATION, arg, EE_UNITS_GOAL_NEW_LOCATION, 0, 0);
    p.i = [i64::from(forced), i64::from(face), 0, 0];
    p.f = [point[0], point[1], point[2], yaw];
    p
}

/// The plan of `spec` (None: the C++ returned NULL, a unit the spec names is gone or the constructor unknown).
/// `unit(id)` is the unit's object (0 for none), `complex_of(object)` its `mComplexUnit`, `player_of(object)` its
/// `mPlayer`.
pub fn plan(
    spec: &EeGoalSpec,
    unit: &dyn Fn(u32) -> u64,
    complex_of: &dyn Fn(u64) -> u64,
    player_of: &dyn Fn(u64) -> u64,
) -> Option<GoalPlan> {
    let flag_a = spec.flag_a != 0;
    let flag_b = spec.flag_b != 0;
    let p = match spec.ctor {
        EE_GOAL_CTOR_EXPLORE => GoalPlan::new(GOAL_EXPLORE, flag_a, EE_UNITS_GOAL_NEW_EXPLORE, 0, 0),
        EE_GOAL_CTOR_ATTACK_GROUND => {
            let mut p = GoalPlan::new(GOAL_ATTACK_GROUND, flag_a, EE_COMBAT_GOAL_NEW_GROUND, 0, 0);
            p.f[..3].copy_from_slice(&spec.point);
            p
        }
        EE_GOAL_CTOR_ATTACK_UNIT => {
            let target = unit(spec.unit_b);
            if target == 0 {
                return None;
            }
            let mut p = GoalPlan::new(GOAL_ATTACK_UNIT, flag_a, EE_COMBAT_GOAL_NEW_ATTACK, unit(spec.unit_a), target);
            p.i[0] = i64::from(flag_b);
            p
        }
        EE_GOAL_CTOR_GATHER => {
            GoalPlan::new(GOAL_GATHER, flag_a, EE_UNITS_GOAL_NEW_GATHER, unit(spec.unit_a), unit(spec.unit_b))
        }
        EE_GOAL_CTOR_HEAL_UNIT => {
            let target = unit(spec.unit_a);
            if target == 0 {
                return None;
            }
            GoalPlan::new(GOAL_HEAL_UNIT, flag_a, EE_COMBAT_GOAL_NEW_HEAL, target, 0)
        }
        EE_GOAL_CTOR_PATROL => GoalPlan::new(GOAL_PATROL, flag_a, EE_UNITS_GOAL_PATROL_NEW_EMPTY, 0, 0),
        EE_GOAL_CTOR_PATROL_ROUTE => {
            GoalPlan::new(GOAL_PATROL, flag_a, EE_UNITS_GOAL_NEW_PATROL, spec.object as usize as u64, unit(spec.unit_a))
        }
        // EEMPathTo's: NewLocation(&point, true, forced, rotating, facing, false, 0), then its group movement
        EE_GOAL_CTOR_LOCATION => {
            let mut p = location(true, flag_a, flag_b, spec.point, spec.value);
            p.group_movement = Some(spec.flag_c != 0);
            p
        }
        // EEMUnloadTransport's: the first unit's complex unit, the tile
        EE_GOAL_CTOR_UNLOAD_TRANSPORT => {
            let first = unit(spec.unit_a);
            if first == 0 {
                return None;
            }
            let mut p = GoalPlan::new(GOAL_UNLOAD_TRANSPORT, flag_a, EE_UNITS_GOAL_NEW_UNLOAD, complex_of(first), 0);
            p.i[..2].copy_from_slice(&[i64::from(spec.point[0] as i32), i64::from(spec.point[1] as i32)]);
            p
        }
        // EEMUnloadTransport's move: NewLocation(x, y, 0, false, true, false, 0, false, 0)
        EE_GOAL_CTOR_LOCATION_XY => location(false, true, false, [spec.point[0], spec.point[1], 0.0], 0.0),
        // EEMSmartFarmers' move: NewLocation(GE2DPoint(x, y), true, true, false, 0, false, 0)
        EE_GOAL_CTOR_LOCATION_2D => location(true, true, false, [spec.point[0], spec.point[1], 0.0], 0.0),
        // EEMSetRallyGoal's: the building's owner, the building
        EE_GOAL_CTOR_BUILD_REPAIR => {
            let target = unit(spec.unit_a);
            if target == 0 {
                return None;
            }
            GoalPlan::new(GOAL_BUILD_REPAIR, flag_a, EE_UNITS_GOAL_NEW_BUILD, player_of(target), target)
        }
        // EEMCastCalamity's: the caster, the target or none, the tile, the technology
        EE_GOAL_CTOR_CAST_CALAMITY => {
            let caster = unit(spec.unit_a);
            if caster == 0 {
                return None;
            }
            let mut p =
                GoalPlan::new(GOAL_CAST_CALAMITY, flag_a, EE_UNITS_GOAL_NEW_CALAMITY, caster, unit(spec.unit_b));
            p.i[..3].copy_from_slice(&[long(spec.ints[0]), long(spec.ints[1]), long(spec.ints[2])]);
            p
        }
        _ => return None,
    };
    Some(p)
}

/// A unit's object by id (`RTSResolveUnitID`: none for the null id).
#[cfg(not(test))]
pub(crate) fn resolve(id: u32) -> u64 {
    if id == EE_UNIT_ID_NONE {
        return 0;
    }
    crate::units::ffi::ee_units_resolve(id) as usize as u64
}

/// One pointer member of a live object at a layout offset.
///
/// # Safety
/// `object` is a live object holding a pointer at `offset`.
#[cfg(not(test))]
unsafe fn member(object: u64, offset: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { *((object as usize + offset as usize) as *const usize) as u64 }
}

/// One pointer member of the unit `unit` (by id; 0 for no unit).
#[cfg(not(test))]
pub(crate) fn unit_member(unit: u32, offset: u64) -> u64 {
    let object = resolve(unit);
    if object == 0 {
        return 0;
    }
    // SAFETY: a registered unit is live while the host's caller holds the world
    unsafe { member(object, offset) }
}

/// The goal object `spec` makes (null when it makes none): [`plan`] over the registry and the units' layout,
/// carried out by [`make`].
///
/// # Safety
/// Called on the simulation's thread with the world held; the spec's object (a route) is live.
#[cfg(not(test))]
pub unsafe fn new_goal(spec: &EeGoalSpec) -> *mut c_void {
    let l = crate::world::layout::layout();
    // SAFETY: the units resolved are live (the registry's)
    let complex_of = |u: u64| unsafe { member(u, l.unit_complex_unit) };
    let player_of = |u: u64| unsafe { member(u, l.unit_player) };
    match plan(spec, &resolve, &complex_of, &player_of) {
        // SAFETY: the caller's contract
        Some(p) => (unsafe { make(&p) }) as usize as *mut c_void,
        None => std::ptr::null_mut(),
    }
}

/// `goal->Clone()`: the kind's clone with the goal's own argument. The copy's handle.
///
/// # Safety
/// `goal` is a live goal's handle; the registered host is the game's.
pub unsafe fn clone(goal: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe {
        let arg = crate::units::goal_obj::base(goal).b1c;
        crate::units::goal_obj::call(
            crate::units::rust_goal::EE_UNITS_GOAL_CLONE,
            goal,
            0,
            u64::from(arg),
            std::ptr::null_mut(),
        ) as u64
    }
}

/// The unload goal's `SetUnloadTransport(unit's mComplexUnit)` (`unit` by id).
///
/// # Safety
/// As [`new_goal`]; `goal` a live unload goal, `unit` a live unit's id.
#[cfg(not(test))]
pub unsafe fn set_transport(goal: u64, unit: u32) {
    // SAFETY: the caller's contract
    unsafe {
        let complex = member(resolve(unit), crate::world::layout::layout().unit_complex_unit);
        crate::units::goal_obj::call(
            crate::units::rust_goal::EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT,
            goal,
            complex,
            0,
            std::ptr::null_mut(),
        );
    }
}

/// The unload goal's `CanUnloadAtDestination()`.
///
/// # Safety
/// As [`clone`].
pub unsafe fn can_unload(goal: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe {
        crate::units::goal_obj::call(
            crate::units::rust_goal::EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD,
            goal,
            0,
            0,
            std::ptr::null_mut(),
        ) != 0
    }
}

/// Carries out a plan: the goal made, its body called, the group movement set. The goal's handle.
///
/// # Safety
/// The plan's handles are live objects; the registered host is the game's.
pub unsafe fn make(p: &GoalPlan) -> u64 {
    let goal = crate::units::goal_obj::goal_new(p.kind, p.arg);
    let mut io = EeUnitsGoalIo::default();
    io.i[..4].copy_from_slice(&p.i);
    io.f[..4].copy_from_slice(&p.f);
    // SAFETY: the goal just made; the caller's contract
    unsafe { crate::units::goal_obj::call(p.op, goal, p.a, p.b, &mut io) };
    if let Some(g) = p.group_movement {
        let mut record = crate::units::location::get(goal);
        record.group_movement = g;
        crate::units::location::put(goal, record);
    }
    goal
}

#[cfg(test)]
#[path = "goal_spec_tests.rs"]
mod tests;
