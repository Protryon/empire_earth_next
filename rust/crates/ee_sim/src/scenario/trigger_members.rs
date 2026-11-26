//! `EESMTrigger`'s members (Scenario Manager/EESMObject.cpp; the scenario editor lane's round 4): the constructor and
//! destructor's entry lists, `Copy`, the condition and effect entries' `Delete*`, `Set*`, `Set*Operator`, `Copy*`,
//! `Get*`, and `EvaluateUncaptured`'s entries and state around trigger.rs `evaluate`. The C++ class keeps its strings
//! and its two vectors of wrapper pointers; each touch of them is one C++ statement (`EE_SCENARIO_TR_*`,
//! `ee_scenariocpp_trigger`), in the C++'s order; the trigger's numbers are the record it derives from
//! (`EeScenarioTriggerFields`).

// c_long/c_ulong are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::scenario::fields::EeScenarioTriggerFields;
use crate::scenario::host::{EeScenarioHost, Host};
use crate::scenario::trigger::{self, EeScenarioConditionEntry, EeScenarioEffectEntry, EeScenarioTriggerState};

// ---- the statements: `x(op, trigger, i, v)`. The condition list's are `EE_SCENARIO_TR_CONDITIONS + n`, the effect
// list's `EE_SCENARIO_TR_EFFECTS + n`, for n:
/// `size()`; `push_back(new wrapper)`; `back() != NULL`; `delete back()`; `pop_back()`; `delete [i]`;
/// `erase(begin() + i)`; `[i]`'s first number set to v (a condition's `mIsNot = v != 0`, an effect's `mDelay`), its
/// second (`mValue`, `mEffectIndex`), its `mOperator`; the same three read; `[i]` itself.
pub const EE_SCENARIO_TR_COUNT: u32 = 0;
pub const EE_SCENARIO_TR_PUSH_NEW: u32 = 1;
pub const EE_SCENARIO_TR_BACK_EXISTS: u32 = 2;
pub const EE_SCENARIO_TR_DELETE_BACK: u32 = 3;
pub const EE_SCENARIO_TR_POP: u32 = 4;
pub const EE_SCENARIO_TR_DELETE_AT: u32 = 5;
pub const EE_SCENARIO_TR_ERASE: u32 = 6;
pub const EE_SCENARIO_TR_SET_FIRST: u32 = 7;
pub const EE_SCENARIO_TR_SET_SECOND: u32 = 8;
pub const EE_SCENARIO_TR_SET_OPERATOR: u32 = 9;
pub const EE_SCENARIO_TR_FIRST: u32 = 10;
pub const EE_SCENARIO_TR_SECOND: u32 = 11;
pub const EE_SCENARIO_TR_OPERATOR: u32 = 12;
pub const EE_SCENARIO_TR_AT: u32 = 13;
/// The lists' bases.
pub const EE_SCENARIO_TR_CONDITIONS: u32 = 0;
pub const EE_SCENARIO_TR_EFFECTS: u32 = 20;
/// `SetName("")`, `SetText("")`; `mName = (trigger i)->mName`, `mText = ...`; the trigger as its
/// `EeScenarioTriggerFields*`; `gWorld.mScenarioManager->AddTimedEffect(i, v, mIsInCinematic)`; `gWorld.mInCinematic`.
pub const EE_SCENARIO_TR_NAME_EMPTY: u32 = 40;
pub const EE_SCENARIO_TR_TEXT_EMPTY: u32 = 41;
pub const EE_SCENARIO_TR_COPY_NAME: u32 = 42;
pub const EE_SCENARIO_TR_COPY_TEXT: u32 = 43;
pub const EE_SCENARIO_TR_FIELDS: u32 = 44;
pub const EE_SCENARIO_TR_ADD_TIMED: u32 = 45;
pub const EE_SCENARIO_TR_WORLD_CINEMATIC: u32 = 46;

// ---- the members (`ee_scenario_trigger_member`'s `member`; k the list: 0 the conditions, 1 the effects)
/// The constructor; the destructor; `Copy(*from i)`; `Delete*(i)`; `Set*(i, first a, second b)`; `Set*Operator(i,
/// a)`; `Get*(i)` (the wrapper or null).
pub const EE_SCENARIO_TRM_CONSTRUCT: u32 = 0;
pub const EE_SCENARIO_TRM_DESTROY: u32 = 1;
pub const EE_SCENARIO_TRM_COPY: u32 = 2;
pub const EE_SCENARIO_TRM_DELETE: u32 = 3;
pub const EE_SCENARIO_TRM_SET: u32 = 4;
pub const EE_SCENARIO_TRM_SET_OPERATOR: u32 = 5;
pub const EE_SCENARIO_TRM_GET: u32 = 6;

/// `kSMTriggerOperatorEnd`
const OPERATOR_END: i64 = 0;

/// One statement: `x(op, trigger, i, v)`.
pub type TrX<'a> = dyn FnMut(u32, i64, i64, i64) -> i64 + 'a;

/// List k's statement base (0 the conditions, 1 the effects).
pub fn list(k: i64) -> u32 {
    if k == 0 {
        EE_SCENARIO_TR_CONDITIONS
    } else {
        EE_SCENARIO_TR_EFFECTS
    }
}

fn fields(x: &mut TrX, t: i64) -> *mut EeScenarioTriggerFields {
    x(EE_SCENARIO_TR_FIELDS, t, 0, 0) as usize as *mut EeScenarioTriggerFields
}

/// `EESMTrigger::EESMTrigger`: empty texts, on, no condition duration, one entry of each kind (all 0), id 0.
pub fn construct(x: &mut TrX, t: i64) {
    x(EE_SCENARIO_TR_NAME_EMPTY, t, 0, 0);
    x(EE_SCENARIO_TR_TEXT_EMPTY, t, 0, 0);
    let f = fields(x, t);
    // SAFETY: the trigger's live record, written before the next C++ call
    unsafe {
        (*f).mIsOn = true;
        (*f).mHasFired = false;
        (*f).mIsInCinematic = false;
        (*f).mIsLooping = false;
        (*f).mConditionDuration = 0;
        (*f).mConditionStartTime = 0;
    }
    for k in 0..2 {
        x(list(k) + EE_SCENARIO_TR_PUSH_NEW, t, 0, 0);
        set_entry(x, t, k, 0, 0, 0);
        set_operator(x, t, k, 0, 0);
    }
    // SAFETY: as above
    unsafe { (*fields(x, t)).mID = 0 };
}

/// `EESMTrigger::~EESMTrigger`: every wrapper deleted, the last first.
pub fn destroy(x: &mut TrX, t: i64) {
    for k in 0..2 {
        let b = list(k);
        while x(b + EE_SCENARIO_TR_COUNT, t, 0, 0) > 0 {
            if x(b + EE_SCENARIO_TR_BACK_EXISTS, t, 0, 0) != 0 {
                x(b + EE_SCENARIO_TR_DELETE_BACK, t, 0, 0);
            }
            x(b + EE_SCENARIO_TR_POP, t, 0, 0);
        }
    }
}

/// `EESMTrigger::Copy`: the texts, the entries, and the numbers but the condition start time.
pub fn copy(x: &mut TrX, t: i64, from: i64) {
    x(EE_SCENARIO_TR_COPY_NAME, t, from, 0);
    x(EE_SCENARIO_TR_COPY_TEXT, t, from, 0);
    copy_entries(x, t, 0, from);
    copy_entries(x, t, 1, from);
    let (f, g) = (fields(x, t), fields(x, from));
    // SAFETY: the two triggers' live records (the same one when a trigger copies itself)
    unsafe {
        let s = *g;
        (*f).mIsOn = s.mIsOn;
        (*f).mHasFired = s.mHasFired;
        (*f).mIsLooping = s.mIsLooping;
        (*f).mConditionDuration = s.mConditionDuration;
        (*f).mID = s.mID;
        (*f).mIsInCinematic = s.mIsInCinematic;
    }
}

/// `DeleteCondition` / `DeleteEffect`: the wrapper freed and erased; the new last one ends the chain.
pub fn delete_entry(x: &mut TrX, t: i64, k: i64, i: i64) {
    let b = list(k);
    x(b + EE_SCENARIO_TR_DELETE_AT, t, i, 0);
    x(b + EE_SCENARIO_TR_ERASE, t, i, 0);
    if i as c_ulong as u64 == x(b + EE_SCENARIO_TR_COUNT, t, 0, 0) as u64 {
        x(b + EE_SCENARIO_TR_SET_OPERATOR, t, i - 1, OPERATOR_END);
    }
}

/// `SetCondition(i, not, condition)` / `SetEffect(i, delay, effect)`.
pub fn set_entry(x: &mut TrX, t: i64, k: i64, i: i64, first: i64, second: i64) {
    let b = list(k);
    x(b + EE_SCENARIO_TR_SET_FIRST, t, i, first);
    x(b + EE_SCENARIO_TR_SET_SECOND, t, i, second);
}

/// `SetConditionOperator` / `SetEffectOperator`: a new wrapper after the last when it gets an operator.
pub fn set_operator(x: &mut TrX, t: i64, k: i64, i: i64, op: i64) {
    let b = list(k);
    // (the index an unsigned long, the size a size_t minus one)
    let last = (x(b + EE_SCENARIO_TR_COUNT, t, 0, 0) as u64).wrapping_sub(1);
    if i as c_ulong as u64 == last && op as c_long != 0 {
        x(b + EE_SCENARIO_TR_PUSH_NEW, t, 0, 0);
    }
    x(b + EE_SCENARIO_TR_SET_OPERATOR, t, i, op);
}

/// `CopyConditions` / `CopyEffects` of trigger `from`'s list: down to one entry, then each entry and operator.
pub fn copy_entries(x: &mut TrX, t: i64, k: i64, from: i64) {
    let b = list(k);
    while x(b + EE_SCENARIO_TR_COUNT, t, 0, 0) > 1 {
        delete_entry(x, t, k, 1);
    }
    let mut i = 0;
    while (i as u64) < x(b + EE_SCENARIO_TR_COUNT, from, 0, 0) as u64 {
        let first = x(b + EE_SCENARIO_TR_FIRST, from, i, 0);
        let second = x(b + EE_SCENARIO_TR_SECOND, from, i, 0);
        set_entry(x, t, k, i, first, second);
        let op = x(b + EE_SCENARIO_TR_OPERATOR, from, i, 0);
        set_operator(x, t, k, i, op);
        i += 1;
    }
}

/// `GetCondition` / `GetEffect`: the wrapper, or null past the end.
pub fn get_entry(x: &mut TrX, t: i64, k: i64, i: i64) -> i64 {
    let b = list(k);
    if (i as c_ulong as u64) < x(b + EE_SCENARIO_TR_COUNT, t, 0, 0) as u64 {
        x(b + EE_SCENARIO_TR_AT, t, i, 0)
    } else {
        0
    }
}

/// `EvaluateUncaptured`: the trigger's state and entries through trigger.rs `evaluate`; the new state stored, the
/// timed effects added.
pub fn evaluate<H: Host>(host: &mut H, x: &mut TrX, t: i64, time: f32) {
    let f = fields(x, t);
    // SAFETY: the trigger's live record, read at once
    let n = unsafe { *f };
    let mut state = EeScenarioTriggerState {
        is_on: n.mIsOn,
        has_fired: n.mHasFired,
        is_looping: n.mIsLooping,
        is_in_cinematic: n.mIsInCinematic,
        condition_duration: n.mConditionDuration as u32,
        condition_start_time: n.mConditionStartTime as u32,
    };
    let conditions: Vec<EeScenarioConditionEntry> = (0..x(EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_COUNT, t, 0, 0))
        .map(|i| EeScenarioConditionEntry {
            condition: x(EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SECOND, t, i, 0) as u32,
            op: x(EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_OPERATOR, t, i, 0) as i32,
            is_not: x(EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_FIRST, t, i, 0) != 0,
        })
        .collect();
    let effects: Vec<EeScenarioEffectEntry> = (0..x(EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_COUNT, t, 0, 0))
        .map(|i| EeScenarioEffectEntry {
            op: x(EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_OPERATOR, t, i, 0) as i32,
            delay: x(EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_FIRST, t, i, 0) as u32,
            effect: x(EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SECOND, t, i, 0) as u32,
        })
        .collect();
    let cinematic = x(EE_SCENARIO_TR_WORLD_CINEMATIC, t, 0, 0) != 0;
    let mut timed = Vec::new();
    trigger::evaluate(host, &mut state, cinematic, &conditions, &effects, time, &mut timed);
    let f = fields(x, t);
    // SAFETY: as above, written before the next C++ call
    unsafe {
        (*f).mIsOn = state.is_on;
        (*f).mHasFired = state.has_fired;
        (*f).mConditionStartTime = state.condition_start_time as _;
    }
    for e in timed {
        x(EE_SCENARIO_TR_ADD_TIMED, t, e.effect as i64, e.time as i64);
    }
}

/// One member by `member` (`EE_SCENARIO_TRM_*`) over the statements `x`.
pub fn member(x: &mut TrX, t: i64, member: u32, k: i64, i: i64, a: i64, b: i64) -> i64 {
    match member {
        EE_SCENARIO_TRM_CONSTRUCT => construct(x, t),
        EE_SCENARIO_TRM_DESTROY => destroy(x, t),
        EE_SCENARIO_TRM_COPY => copy(x, t, i),
        EE_SCENARIO_TRM_DELETE => delete_entry(x, t, k, i),
        EE_SCENARIO_TRM_SET => set_entry(x, t, k, i, a, b),
        EE_SCENARIO_TRM_SET_OPERATOR => set_operator(x, t, k, i, a),
        EE_SCENARIO_TRM_GET => return get_entry(x, t, k, i),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod sys {
    extern "C" {
        pub fn ee_scenariocpp_trigger(op: u32, trigger: *mut core::ffi::c_void, i: i64, v: i64) -> i64;
    }
}

/// The C++ statements (`ee_scenariocpp_trigger`, EESMObject.cpp).
pub fn game(op: u32, t: i64, i: i64, v: i64) -> i64 {
    #[cfg(not(test))]
    {
        // SAFETY: the statement host's contract: `t` a live trigger, `i` and `v` as its op names
        unsafe {
            match op {
                // the trigger's record and the world's flag (entry.rs, world_views.rs)
                EE_SCENARIO_TR_FIELDS => crate::scenario::entry::element_record(
                    crate::scenario::entry::EE_SCENARIO_EN_TRIGGER_FIELDS,
                    t as u64,
                ) as i64,
                EE_SCENARIO_TR_WORLD_CINEMATIC => crate::scenario::world_views::in_cinematic() as i64,
                _ => sys::ee_scenariocpp_trigger(op, t as usize as *mut c_void, i, v),
            }
        }
    }
    #[cfg(test)]
    {
        let _ = (op, t, i, v);
        unreachable!("the trigger's game statements")
    }
}

/// `EESMTrigger`'s member `member` (`EE_SCENARIO_TRM_*`) on `trigger`; k the list (0 conditions, 1 effects).
#[no_mangle]
pub extern "C" fn ee_scenario_trigger_member(trigger: *mut c_void, member: u32, k: i64, i: i64, a: i64, b: i64) -> i64 {
    self::member(&mut game, trigger as usize as i64, member, k, i, a, b)
}

/// `EESMTrigger::EvaluateUncaptured` on `trigger` with the conditions' host `host`.
///
/// # Safety
/// `host` must be valid for the call, `trigger` a live trigger.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_trigger_evaluate_member(
    host: *const EeScenarioHost,
    trigger: *mut c_void,
    time: f32,
) {
    // SAFETY: the caller's contract
    let mut host = unsafe {
        let h = &*host;
        EeScenarioHost {
            ctx: h.ctx,
            read: h.read,
            players: h.players,
            write: h.write,
            tiles: h.tiles,
            world: h.world,
            object: h.object,
        }
    };
    evaluate(&mut host, &mut game, trigger as usize as i64, time);
}

#[cfg(test)]
#[path = "trigger_members_tests.rs"]
mod tests;
