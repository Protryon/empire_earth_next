//! A unit's AI step (RUST_PLAN R5b.4 leaf 6, "the updated plan" 2): `EEComplexUnit::AIStep` (0x5ff2e8) and the unit
//! AI script's decision (`EEUnitAction::Evaluate` 0x62cd8b, `EEUnitTrigger::Evaluate` 0x62b9a0) over the unit store
//! and the C++ statements of the calls they make (`ee_units_cls_cond_host`'s, EEUnitAI.cpp).
//!
//! The script (`Unit AI Scripts\*.tai`) is script.rs's (the twentieth round): an action is a handle, its triggers
//! are read one at a time as plain views (`EeUnitsTrigger`: the conditions, all of or any of, the true and false
//! actions). The conditions' answers are the classes' `EvaluateCondition` (virtual, C++; the attack goals' tests are
//! the combat lane's), the gather goal's activity (`IsActive`, virtual), `StartAction`, `ActionChanged`,
//! `DropFinishedGoals`, `UpdateCamouflage` and the command's write stay host calls. The command's type, the next
//! trigger time and the goal's kind are read from the unit's record, again after every host call that can change
//! them (the store's lock is never held across a host call: the holders write the store from inside them); the next
//! trigger time is written through its C++ holder.
//!
//! Determinism (RUST_PLAN 2.1): no float, no random draw; the next trigger time is compared as the Windows build's
//! 32-bit unsigned longs (`jb`).

use crate::units::store::EeUnitRecord;

/// `kActionIdle` (the idle action's id).
pub const EE_UNITS_ACTION_IDLE: u32 = 5000;
/// `kGoalGather` (`EEUnitGoal::mKind` of `EEUGGather`).
pub const EE_UNITS_GOAL_GATHER: i32 = 6;

/// A trigger of an action, read by the host: `conditions` points to `count` trigger ids (the C++ vector's data,
/// copied before any other host call).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsTrigger {
    pub conditions: *const i64,
    pub count: u32,
    pub all_of: u8,
    pub any_of: u8,
    pub true_id: i64,
    pub false_id: i64,
    pub true_action: u64,
    pub false_action: u64,
}

impl Default for EeUnitsTrigger {
    fn default() -> Self {
        EeUnitsTrigger {
            conditions: std::ptr::null(),
            count: 0,
            all_of: 0,
            any_of: 0,
            true_id: 0,
            false_id: 0,
            true_action: 0,
            false_action: 0,
        }
    }
}

/// What `AIStep` reads of the unit: from its record, or from the members when it has none (a killed unit still
/// stepped).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsAIState {
    /// `mCommand ? mCommand->mType : 0`
    pub command: i64,
    /// `mNextTriggerTime`
    pub next_trigger_time: i64,
    /// `mGoalState ? mGoalState->mKind : -1`
    pub goal_kind: i32,
}

impl EeUnitsAIState {
    pub fn of(r: &EeUnitRecord) -> Self {
        EeUnitsAIState { command: r.command, next_trigger_time: r.next_trigger_time, goal_kind: r.goal_kind }
    }
}

/// The C++ statements of `AIStep`'s calls (`ee_units_cls_cond_host`'s, EEUnitAI.cpp: one statement a case; `a` an
/// object, `b` a value), the round 9 replacement of the host table: the operations below.
pub trait AIOps {
    fn ai(&mut self, op: i32, a: u64, b: i64) -> i64;
}

/// `mCommand ? mCommand->mType : 0`
pub const EE_UNITS_AI_H_COMMAND_TYPE: i32 = 14200;
/// `mNextTriggerTime`
pub const EE_UNITS_AI_H_NEXT_TRIGGER_TIME: i32 = 14201;
/// `mGoalState ? mGoalState->mKind : -1`
pub const EE_UNITS_AI_H_GOAL_KIND: i32 = 14202;
/// `mCommand` as a handle (0: none)
pub const EE_UNITS_AI_H_COMMAND: i32 = 14203;
/// `mGoalState != NULL`
pub const EE_UNITS_AI_H_HAS_GOAL: i32 = 14204;
/// `theAction = 0` (the gather test's local)
pub const EE_UNITS_AI_H_GOAL_ACTION_RESET: i32 = 14205;
/// `mGoalState->IsActive(&theAction)`
pub const EE_UNITS_AI_H_GOAL_ACTIVE: i32 = 14206;
/// `theAction`
pub const EE_UNITS_AI_H_GOAL_ACTION: i32 = 14207;
/// `mUnitClass->EvaluateCondition(unit, b)`
pub const EE_UNITS_AI_H_CONDITION: i32 = 14208;
/// action `a`'s `mType`
pub const EE_UNITS_AI_H_ACTION_TYPE: i32 = 14209;
/// `StartAction(b)`
pub const EE_UNITS_AI_H_START_ACTION: i32 = 14210;
/// `GetUnitClass()->ActionChanged(unit)`
pub const EE_UNITS_AI_H_ACTION_CHANGED: i32 = 14211;
/// `DropFinishedGoals(false)`
pub const EE_UNITS_AI_H_DROP_FINISHED_GOALS: i32 = 14212;
/// `UpdateCamouflage(b)`
pub const EE_UNITS_AI_H_UPDATE_CAMOUFLAGE: i32 = 14213;
/// `mCommand = (EEUnitAction*)a`
pub const EE_UNITS_AI_H_SET_COMMAND: i32 = 14214;
/// `mNextTriggerTime = b`
pub const EE_UNITS_AI_H_SET_NEXT_TRIGGER_TIME: i32 = 14215;
/// `mUnitID`
pub const EE_UNITS_AI_H_UNIT_ID: i32 = 14216;
/// `mIsCitizen`
pub const EE_UNITS_AI_H_IS_CITIZEN: i32 = 14217;
/// `gPhysics->mGameTime`
pub const EE_UNITS_AI_H_GAME_TIME: i32 = 14218;

/// `kUnitStateAIStep`, `kUnitStateCondition` (ee/EEUnitStore.h: the USST kinds)
const STATE_AI_STEP: u32 = 1;
const STATE_CONDITION: u32 = 2;

/// The calls as a trait (the FFI table over the store, the tests' mock).
pub trait AIHost {
    /// the unit's state now (the record's, re-read; the store's lock not held after it returns)
    fn state(&mut self) -> EeUnitsAIState;
    fn command(&mut self) -> u64;
    fn gather_inactive(&mut self) -> Option<u32>;
    /// the trigger's view and its conditions (copied)
    fn trigger(&mut self, action: u64, index: u32) -> Option<(EeUnitsTrigger, Vec<i64>)>;
    fn condition(&mut self, trigger: i64) -> i64;
    fn action_type(&mut self, action: u64) -> u32;
    fn start_action(&mut self, action: u32);
    fn drop_finished_goals(&mut self);
    fn set_command(&mut self, action: u64, action_type: u32);
    fn set_next_trigger_time(&mut self, value: u32);
}

/// `EEUnitTrigger::Evaluate`: the trigger's conditions, all of them (stopping at the first that answers 0) or any of
/// them (stopping at the first that does not), pick its true or false action when that action's id is set. 0 for no
/// action; else 1 and the action, or 2 when a condition answered 2 (a goal handled the event).
pub fn evaluate_trigger(host: &mut impl AIHost, t: &EeUnitsTrigger, ids: &[i64]) -> (i64, Option<u64>) {
    let pick = |id: i64, action: u64, status: i64| -> (i64, Option<u64>) {
        if id != 0 {
            (if status == 0 { 1 } else { status }, Some(action))
        } else {
            (0, None)
        }
    };
    let count = ids.len();
    let mut status = 0i64;
    if t.all_of != 0 {
        let mut i = 0;
        while i < count {
            let e = host.condition(ids[i]);
            if e == 0 {
                break;
            }
            if e == 2 {
                status = 2;
            }
            i += 1;
        }
        if i == count {
            pick(t.true_id, t.true_action, status)
        } else {
            pick(t.false_id, t.false_action, status)
        }
    } else if t.any_of != 0 {
        let mut i = 0;
        while i < count {
            let e = host.condition(ids[i]);
            if e != 0 {
                if e == 2 {
                    status = 2;
                }
                break;
            }
            i += 1;
        }
        if i < count {
            pick(t.true_id, t.true_action, status)
        } else {
            pick(t.false_id, t.false_action, status)
        }
    } else {
        (0, None)
    }
}

/// `EEUnitAction::Evaluate`: the action's triggers in order; the first non-zero result and its action.
pub fn evaluate_action(host: &mut impl AIHost, action: u64) -> (i64, Option<u64>) {
    let mut index = 0u32;
    while let Some((t, ids)) = host.trigger(action, index) {
        let r = evaluate_trigger(host, &t, &ids);
        if r.0 != 0 {
            return r;
        }
        index += 1;
    }
    (0, None)
}

/// What `AIStep` did (the USST words): the path (1 the gather goal restarted its action, 2 no command, 3 an idle
/// unit waiting, 4 evaluated), the gather's action, the two evaluations' results (`u32::MAX`: not evaluated).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeUnitsAIStepOut {
    pub path: u32,
    pub action: u32,
    pub first: u32,
    pub second: u32,
}

/// `EEComplexUnit::AIStep`. `is_citizen`: `mIsCitizen`; `game_time`: `gPhysics->mGameTime`.
pub fn ai_step(host: &mut impl AIHost, is_citizen: bool, game_time: i64) -> EeUnitsAIStepOut {
    let mut out = EeUnitsAIStepOut { path: 0, action: 0, first: u32::MAX, second: u32::MAX };
    let s = host.state();
    if is_citizen && s.goal_kind == EE_UNITS_GOAL_GATHER {
        if let Some(action) = host.gather_inactive() {
            host.start_action(action);
            out.path = 1;
            out.action = action;
            return out;
        }
    }
    let command = host.command();
    if command == 0 {
        out.path = 2;
        return out;
    }
    if s.command as u32 == EE_UNITS_ACTION_IDLE && (s.next_trigger_time as u32) < (game_time as u32) {
        out.path = 3;
        return out;
    }
    out.path = 4;
    let (mut result, mut next) = evaluate_action(host, command);
    out.first = result as u32;
    if result != 0 {
        if result == 2 {
            host.drop_finished_goals();
            if host.state().goal_kind >= 0 {
                let command = host.command();
                (result, next) = evaluate_action(host, command);
                out.second = result as u32;
            } else {
                host.start_action(EE_UNITS_ACTION_IDLE);
                result = 0;
            }
        }
        if result == 1 {
            // the C++ dereferences the action (a script's trigger always links its actions)
            let action = next.unwrap_or(0);
            let kind = host.action_type(action);
            host.set_command(action, kind);
        }
    }
    if host.state().command as u32 == EE_UNITS_ACTION_IDLE {
        host.set_next_trigger_time((game_time as u32).wrapping_add(350));
    }
    out
}

/// The host's statements over the store: `state` reads the members, then the record (under the store's lock,
/// released before it returns) and counts a record that differs (`step::READS`).
struct StoreHost<'a, O: AIOps + ?Sized> {
    ops: &'a mut O,
    id: u32,
    object: u64,
}

impl<O: AIOps + ?Sized> StoreHost<'_, O> {
    fn q(&mut self, op: i32) -> i64 {
        self.ops.ai(op, 0, 0)
    }
}

impl<O: AIOps + ?Sized> AIHost for StoreHost<'_, O> {
    fn state(&mut self) -> EeUnitsAIState {
        let m = EeUnitsAIState {
            command: self.q(EE_UNITS_AI_H_COMMAND_TYPE),
            next_trigger_time: self.q(EE_UNITS_AI_H_NEXT_TRIGGER_TIME),
            goal_kind: self.q(EE_UNITS_AI_H_GOAL_KIND) as i32,
        };
        let rec = crate::units::store::with_store(|s| {
            s.get(self.id).filter(|r| r.object == self.object).map(EeUnitsAIState::of)
        });
        crate::units::step::count_read(rec.map(|r| r != m));
        rec.unwrap_or(m)
    }
    fn command(&mut self) -> u64 {
        self.q(EE_UNITS_AI_H_COMMAND) as u64
    }
    fn gather_inactive(&mut self) -> Option<u32> {
        self.q(EE_UNITS_AI_H_GOAL_ACTION_RESET);
        if self.q(EE_UNITS_AI_H_HAS_GOAL) == 0 || self.q(EE_UNITS_AI_H_GOAL_ACTIVE) != 0 {
            return None;
        }
        Some(self.q(EE_UNITS_AI_H_GOAL_ACTION) as u32)
    }
    fn trigger(&mut self, action: u64, index: u32) -> Option<(EeUnitsTrigger, Vec<i64>)> {
        crate::units::script::trigger_view(action, index)
    }
    fn condition(&mut self, trigger: i64) -> i64 {
        let event = self.ops.ai(EE_UNITS_AI_H_CONDITION, 0, trigger);
        if crate::units::notes::noting() {
            // TEST ONLY (USST): the condition and its event
            let id = self.q(EE_UNITS_AI_H_UNIT_ID) as u32;
            crate::units::notes::state(STATE_CONDITION, id, &[trigger as u32, event as u32]);
        }
        event
    }
    fn action_type(&mut self, action: u64) -> u32 {
        self.ops.ai(EE_UNITS_AI_H_ACTION_TYPE, action, 0) as u32
    }
    fn start_action(&mut self, action: u32) {
        self.ops.ai(EE_UNITS_AI_H_START_ACTION, 0, i64::from(action));
        self.q(EE_UNITS_AI_H_ACTION_CHANGED);
    }
    fn drop_finished_goals(&mut self) {
        self.q(EE_UNITS_AI_H_DROP_FINISHED_GOALS);
    }
    fn set_command(&mut self, action: u64, action_type: u32) {
        self.ops.ai(EE_UNITS_AI_H_UPDATE_CAMOUFLAGE, 0, i64::from(action_type));
        self.ops.ai(EE_UNITS_AI_H_SET_COMMAND, action, 0);
        self.q(EE_UNITS_AI_H_ACTION_CHANGED);
    }
    fn set_next_trigger_time(&mut self, value: u32) {
        self.ops.ai(EE_UNITS_AI_H_SET_NEXT_TRIGGER_TIME, 0, i64::from(value));
    }
}

/// `EEComplexUnit::AIStep()` of the unit at `object` over the store and the host's statements, with its USST note
/// (the command, the next trigger time and the game time before; the path, the gather's action and the two
/// evaluations; the command and the next trigger time after).
pub fn ai_step_noted<O: AIOps + ?Sized>(ops: &mut O, object: u64) -> EeUnitsAIStepOut {
    let noting = crate::units::notes::noting();
    let mut words = [
        ops.ai(EE_UNITS_AI_H_COMMAND_TYPE, 0, 0) as u32,
        ops.ai(EE_UNITS_AI_H_NEXT_TRIGGER_TIME, 0, 0) as u32,
        ops.ai(EE_UNITS_AI_H_GAME_TIME, 0, 0) as u32,
        0,
        0,
        u32::MAX,
        u32::MAX,
        0,
        0,
    ];
    let id = ops.ai(EE_UNITS_AI_H_UNIT_ID, 0, 0) as u32;
    let is_citizen = ops.ai(EE_UNITS_AI_H_IS_CITIZEN, 0, 0) != 0;
    let game_time = ops.ai(EE_UNITS_AI_H_GAME_TIME, 0, 0);
    let out = ai_step(&mut StoreHost { ops: &mut *ops, id, object }, is_citizen, game_time);
    if noting {
        words[3] = out.path;
        words[4] = out.action;
        words[5] = out.first;
        words[6] = out.second;
        words[7] = ops.ai(EE_UNITS_AI_H_COMMAND_TYPE, 0, 0) as u32;
        words[8] = ops.ai(EE_UNITS_AI_H_NEXT_TRIGGER_TIME, 0, 0) as u32;
        crate::units::notes::state(STATE_AI_STEP, id, &words);
    }
    out
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::ffi::c_void;

    use crate::units::cls_host::ee_units_cls_cond_host;

    /// The unit stepped and `IsActive`'s action (the gather test's local; units round 14: was EEUnitAction.cpp's
    /// static)
    struct Game(*mut c_void, std::os::raw::c_ulong);

    impl AIOps for Game {
        fn ai(&mut self, op: i32, a: u64, b: i64) -> i64 {
            let unit = self.0 as usize as u64;
            // SAFETY: the unit stepped, live for the step (its C++ AIStep's; the plain record reads and stores
            // first: cu_view.rs; the calls are the trigger host's, EEUnitAI.cpp)
            unsafe {
                match op {
                    EE_UNITS_AI_H_GOAL_ACTION_RESET => {
                        self.1 = 0;
                        return 0;
                    }
                    EE_UNITS_AI_H_GOAL_ACTION => return self.1 as i64,
                    EE_UNITS_AI_H_GOAL_ACTIVE => {
                        let action = &mut self.1 as *mut std::os::raw::c_ulong as usize as u64;
                        return ee_units_cls_cond_host(op, unit, action, 0, 0, std::ptr::null_mut());
                    }
                    _ => {}
                }
                if let Some(v) = crate::units::cu_view::ai_view(op, unit, a, b) {
                    return v;
                }
                ee_units_cls_cond_host(op, unit, a, b, 0, std::ptr::null_mut())
            }
        }
    }

    /// `EEComplexUnit::AIStep()` on `unit` (see [`ai_step_noted`]).
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_ai_step(unit: *mut c_void) {
        ai_step_noted(&mut Game(unit, 0), unit as usize as u64);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// a trigger's view and its conditions
    type Trigger = (EeUnitsTrigger, Vec<i64>);

    /// A unit, its script (actions by handle: their triggers) and a log of the calls.
    #[derive(Default)]
    struct Mock {
        state: EeUnitsAIState,
        command: u64,
        gather_inactive: Option<u32>,
        scripts: Vec<(u64, Vec<Trigger>)>,
        answers: Vec<(i64, i64)>,
        types: Vec<(u64, u32)>,
        /// DropFinishedGoals leaves a goal (its kind) or none (-1)
        goal_after_drop: i32,
        log: Vec<String>,
    }

    impl AIHost for Mock {
        fn state(&mut self) -> EeUnitsAIState {
            self.state
        }
        fn command(&mut self) -> u64 {
            self.command
        }
        fn gather_inactive(&mut self) -> Option<u32> {
            self.log.push("is-active".into());
            self.gather_inactive
        }
        fn trigger(&mut self, action: u64, index: u32) -> Option<(EeUnitsTrigger, Vec<i64>)> {
            let s = self.scripts.iter().find(|(a, _)| *a == action)?;
            s.1.get(index as usize).cloned()
        }
        fn condition(&mut self, trigger: i64) -> i64 {
            self.log.push(format!("cond {trigger}"));
            self.answers.iter().find(|(t, _)| *t == trigger).map(|(_, a)| *a).unwrap_or(0)
        }
        fn action_type(&mut self, action: u64) -> u32 {
            self.types.iter().find(|(a, _)| *a == action).map(|(_, t)| *t).unwrap()
        }
        fn start_action(&mut self, action: u32) {
            self.log.push(format!("start {action}"));
            self.state.command = i64::from(action);
        }
        fn drop_finished_goals(&mut self) {
            self.log.push("drop".into());
            self.state.goal_kind = self.goal_after_drop;
        }
        fn set_command(&mut self, action: u64, action_type: u32) {
            self.log.push(format!("command {action:#x} {action_type}"));
            self.command = action;
            self.state.command = i64::from(action_type);
        }
        fn set_next_trigger_time(&mut self, value: u32) {
            self.log.push(format!("trigger time {value}"));
            self.state.next_trigger_time = i64::from(value);
        }
    }

    fn trig(all: bool, ids: &[i64], t: (i64, u64), f: (i64, u64)) -> (EeUnitsTrigger, Vec<i64>) {
        (
            EeUnitsTrigger {
                count: ids.len() as u32,
                all_of: u8::from(all),
                any_of: u8::from(!all),
                true_id: t.0,
                true_action: t.1,
                false_id: f.0,
                false_action: f.1,
                ..Default::default()
            },
            ids.to_vec(),
        )
    }

    #[test]
    fn all_of_stops_at_the_first_false_and_any_of_at_the_first_true() {
        let mut m = Mock { answers: vec![(1, 1), (2, 0), (3, 2)], ..Default::default() };
        let (t, ids) = trig(true, &[1, 2, 3], (7, 0x70), (8, 0x80));
        assert_eq!(evaluate_trigger(&mut m, &t, &ids), (1, Some(0x80)));
        assert_eq!(m.log, ["cond 1", "cond 2"]);
        m.log.clear();
        let (t, ids) = trig(true, &[1, 3], (7, 0x70), (8, 0x80));
        assert_eq!(evaluate_trigger(&mut m, &t, &ids), (2, Some(0x70)));
        m.log.clear();
        let (t, ids) = trig(false, &[2, 3, 1], (7, 0x70), (0, 0x80));
        assert_eq!(evaluate_trigger(&mut m, &t, &ids), (2, Some(0x70)));
        assert_eq!(m.log, ["cond 2", "cond 3"]);
        // none true and no false action: nothing
        let (t, ids) = trig(false, &[2], (7, 0x70), (0, 0x80));
        assert_eq!(evaluate_trigger(&mut m, &t, &ids), (0, None));
        // an all-of with no conditions is true
        let (t, ids) = trig(true, &[], (7, 0x70), (0, 0));
        assert_eq!(evaluate_trigger(&mut m, &t, &ids), (1, Some(0x70)));
        // neither flag: nothing evaluated
        let t = EeUnitsTrigger { true_id: 1, ..Default::default() };
        m.log.clear();
        assert_eq!(evaluate_trigger(&mut m, &t, &[1]), (0, None));
        assert!(m.log.is_empty());
    }

    #[test]
    fn an_action_returns_its_first_trigger_that_picks() {
        let mut m = Mock {
            answers: vec![(1, 1)],
            scripts: vec![(0x10, vec![trig(true, &[2], (5, 0x50), (0, 0)), trig(false, &[1], (6, 0x60), (0, 0))])],
            ..Default::default()
        };
        assert_eq!(evaluate_action(&mut m, 0x10), (1, Some(0x60)));
        assert_eq!(evaluate_action(&mut m, 0x99), (0, None));
    }

    #[test]
    fn ai_step_paths() {
        // an inactive gather goal restarts its action
        let mut m = Mock {
            state: EeUnitsAIState { command: 3, next_trigger_time: 0, goal_kind: EE_UNITS_GOAL_GATHER },
            command: 0x10,
            gather_inactive: Some(5000),
            ..Default::default()
        };
        let o = ai_step(&mut m, true, 1000);
        assert_eq!((o.path, o.action), (1, 5000));
        assert_eq!(m.log, ["is-active", "start 5000"]);
        // not a citizen: no test; no command
        let mut m = Mock {
            state: EeUnitsAIState { goal_kind: EE_UNITS_GOAL_GATHER, ..Default::default() },
            ..Default::default()
        };
        assert_eq!(ai_step(&mut m, false, 1000).path, 2);
        assert!(m.log.is_empty());
        // idle and past its trigger time: waits
        let mut m = Mock {
            state: EeUnitsAIState { command: 5000, next_trigger_time: 999, goal_kind: -1 },
            command: 0x10,
            ..Default::default()
        };
        assert_eq!(ai_step(&mut m, false, 1000).path, 3);
        // idle, its time not reached: evaluated (nothing picks), the trigger time set again
        m.state.next_trigger_time = 1000;
        let o = ai_step(&mut m, false, 1000);
        assert_eq!((o.path, o.first, o.second), (4, 0, u32::MAX));
        assert_eq!(m.log, ["trigger time 1350"]);
    }

    #[test]
    fn ai_step_switches_and_drops_finished_goals() {
        let script = vec![
            (0x10, vec![trig(false, &[1], (6, 0x60), (0, 0))]),
            (0x60, vec![trig(false, &[2], (7, 0x70), (0, 0))]),
        ];
        let mut m = Mock {
            state: EeUnitsAIState { command: 12, next_trigger_time: 0, goal_kind: 1 },
            command: 0x10,
            answers: vec![(1, 1)],
            scripts: script.clone(),
            types: vec![(0x60, 5000), (0x70, 40)],
            ..Default::default()
        };
        let o = ai_step(&mut m, false, 2000);
        assert_eq!((o.path, o.first), (4, 1));
        assert_eq!(m.log, ["cond 1", "command 0x60 5000", "trigger time 2350"]);
        // a goal answered: its finished goals dropped, then evaluated again with the goal left
        let mut m = Mock {
            state: EeUnitsAIState { command: 12, next_trigger_time: 0, goal_kind: 1 },
            command: 0x10,
            answers: vec![(1, 2)],
            scripts: script.clone(),
            types: vec![(0x60, 40)],
            goal_after_drop: 1,
            ..Default::default()
        };
        let o = ai_step(&mut m, false, 2000);
        assert_eq!((o.first, o.second), (2, 2));
        assert_eq!(m.log, ["cond 1", "drop", "cond 1"]);
        // no goal left: idle
        let mut m = Mock {
            state: EeUnitsAIState { command: 12, next_trigger_time: 0, goal_kind: 1 },
            command: 0x10,
            answers: vec![(1, 2)],
            scripts: script,
            goal_after_drop: -1,
            ..Default::default()
        };
        let o = ai_step(&mut m, false, 2000);
        assert_eq!((o.first, o.second), (2, u32::MAX));
        assert_eq!(m.log, ["cond 1", "drop", "start 5000", "trigger time 2350"]);
    }
}

/// The host statements of the round 9 op host (`StoreHost` over `AIOps`), recorded.
#[cfg(test)]
mod ops_tests {
    use super::*;

    #[derive(Default)]
    struct Rec {
        log: Vec<(i32, u64, i64)>,
        answers: Vec<(i32, i64)>,
    }

    impl AIOps for Rec {
        fn ai(&mut self, op: i32, a: u64, b: i64) -> i64 {
            self.log.push((op, a, b));
            self.answers.iter().find(|e| e.0 == op).map_or(0, |e| e.1)
        }
    }

    fn host(r: &mut Rec) -> StoreHost<'_, Rec> {
        StoreHost { ops: r, id: 0xfff0_0001, object: 0x5555 }
    }

    #[test]
    fn the_members_are_read_when_the_store_has_no_record() {
        let mut r = Rec {
            answers: vec![
                (EE_UNITS_AI_H_COMMAND_TYPE, 5000),
                (EE_UNITS_AI_H_NEXT_TRIGGER_TIME, 7),
                (EE_UNITS_AI_H_GOAL_KIND, -1),
            ],
            ..Default::default()
        };
        let s = host(&mut r).state();
        assert_eq!(s, EeUnitsAIState { command: 5000, next_trigger_time: 7, goal_kind: -1 });
    }

    #[test]
    fn the_gather_test_resets_its_action_and_asks_activity_only_with_a_goal() {
        let mut r =
            Rec { answers: vec![(EE_UNITS_AI_H_HAS_GOAL, 1), (EE_UNITS_AI_H_GOAL_ACTION, 42)], ..Default::default() };
        assert_eq!(host(&mut r).gather_inactive(), Some(42));
        let ops: Vec<i32> = r.log.iter().map(|e| e.0).collect();
        assert_eq!(
            ops,
            [
                EE_UNITS_AI_H_GOAL_ACTION_RESET,
                EE_UNITS_AI_H_HAS_GOAL,
                EE_UNITS_AI_H_GOAL_ACTIVE,
                EE_UNITS_AI_H_GOAL_ACTION
            ]
        );
        let mut r = Rec::default();
        assert_eq!(host(&mut r).gather_inactive(), None);
        assert_eq!(r.log.len(), 2);
        let mut r =
            Rec { answers: vec![(EE_UNITS_AI_H_HAS_GOAL, 1), (EE_UNITS_AI_H_GOAL_ACTIVE, 1)], ..Default::default() };
        assert_eq!(host(&mut r).gather_inactive(), None);
    }

    #[test]
    fn a_command_is_set_after_the_camouflage_and_announced() {
        let mut r = Rec::default();
        host(&mut r).set_command(0x77, 2001);
        assert_eq!(
            r.log,
            [
                (EE_UNITS_AI_H_UPDATE_CAMOUFLAGE, 0, 2001),
                (EE_UNITS_AI_H_SET_COMMAND, 0x77, 0),
                (EE_UNITS_AI_H_ACTION_CHANGED, 0, 0)
            ]
        );
        let mut r = Rec::default();
        host(&mut r).start_action(5000);
        assert_eq!(r.log, [(EE_UNITS_AI_H_START_ACTION, 0, 5000), (EE_UNITS_AI_H_ACTION_CHANGED, 0, 0)]);
    }

    #[test]
    fn a_condition_is_the_classes_answer() {
        let mut r = Rec { answers: vec![(EE_UNITS_AI_H_CONDITION, 2)], ..Default::default() };
        assert_eq!(host(&mut r).condition(13000), 2);
        assert_eq!(r.log[0], (EE_UNITS_AI_H_CONDITION, 0, 13000));
    }

    #[test]
    fn a_step_without_a_command_reads_its_words_and_stops() {
        let mut r =
            Rec { answers: vec![(EE_UNITS_AI_H_GAME_TIME, 900), (EE_UNITS_AI_H_GOAL_KIND, -1)], ..Default::default() };
        let out = ai_step_noted(&mut r, 0x5555);
        assert_eq!(out.path, 2);
        let ops: Vec<i32> = r.log.iter().map(|e| e.0).collect();
        assert_eq!(
            &ops[..6],
            [
                EE_UNITS_AI_H_COMMAND_TYPE,
                EE_UNITS_AI_H_NEXT_TRIGGER_TIME,
                EE_UNITS_AI_H_GAME_TIME,
                EE_UNITS_AI_H_UNIT_ID,
                EE_UNITS_AI_H_IS_CITIZEN,
                EE_UNITS_AI_H_GAME_TIME
            ]
        );
    }
}
