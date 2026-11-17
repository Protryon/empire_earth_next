//! `EEScenarioManager::Update`, each physics tick: the dialog queue, the clocks, and every 500 ms while a human
//! plays the trigger round (each trigger's `Evaluate`, the due timed effects run once each, the run ones erased,
//! the chat text cleared), then the end of a cinematic once its letterbox is gone. The world through one operation
//! callback (`EE_SCENARIO_ROUND_*`); the manager's numbers and timed effects through the host, read again after
//! every callback (an effect may start or end a cinematic, show a dialog, schedule or re-arm a timed effect).

#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};

/// `gWorld.mGameSpeed`'s bits (an `f32`).
pub const EE_SCENARIO_ROUND_GAME_SPEED: u32 = 0;
/// `gPhysics->GetTickSeconds()`'s bits (an `f32`).
pub const EE_SCENARIO_ROUND_TICK_SECONDS: u32 = 1;
/// `mMessages.size()`.
pub const EE_SCENARIO_ROUND_MESSAGE_COUNT: u32 = 2;
/// Whether the dialogs wait: no communications, or the game paused (1/0).
pub const EE_SCENARIO_ROUND_DIALOG_PAUSED: u32 = 3;
/// Whether a player is human and not an observer (1/0).
pub const EE_SCENARIO_ROUND_HUMAN_PLAYING: u32 = 4;
/// `gWorld.mInCinematic` (1/0).
pub const EE_SCENARIO_ROUND_IN_CINEMATIC: u32 = 5;
/// `MoveCameraToStart()`.
pub const EE_SCENARIO_ROUND_MOVE_CAMERA: u32 = 6;
/// `mTriggers.size()`.
pub const EE_SCENARIO_ROUND_TRIGGER_COUNT: u32 = 7;
/// `GetTrigger(a)->Evaluate(time)`.
pub const EE_SCENARIO_ROUND_EVALUATE_TRIGGER: u32 = 8;
/// `mEffects[a] != NULL` (1/0; the original's unchecked index).
pub const EE_SCENARIO_ROUND_EFFECT_EXISTS: u32 = 9;
/// `mEffects[a]->Execute(b != 0)`.
pub const EE_SCENARIO_ROUND_EXECUTE: u32 = 10;
/// `mLastChatText.Clear()`.
pub const EE_SCENARIO_ROUND_CLEAR_CHAT: u32 = 11;
/// TEST ONLY: the SMUP record (`a`: the human flag) while the capture is on.
pub const EE_SCENARIO_ROUND_CAPTURE: u32 = 12;
/// The letterbox's state `gF4BE374`'s bits (an `f32`; 1 once it is gone).
pub const EE_SCENARIO_ROUND_LETTERBOX: u32 = 13;
/// `ExitCinematic()`.
pub const EE_SCENARIO_ROUND_EXIT_CINEMATIC: u32 = 14;
/// `mMessages[a].mIsCinematic` (1/0).
pub const EE_SCENARIO_ROUND_MESSAGE_CINEMATIC: u32 = 15;
/// Dialog message a shown and erased: the one-shot sounds stopped for a voiced one, posted to the interface, the
/// camera moved to its unit (when it exists and the scroll speed is not negative).
pub const EE_SCENARIO_ROUND_SHOW_MESSAGE: u32 = 16;
/// No message to show: the interface's messages hidden, the one-shot sounds stopped.
pub const EE_SCENARIO_ROUND_HIDE_MESSAGES: u32 = 17;

/// The trigger interval (ms; `kTriggerInterval`).
pub const TRIGGER_INTERVAL: f32 = 500.0;

/// The manager and the world as the round sees them.
pub trait Round {
    /// One operation (`EE_SCENARIO_ROUND_*`).
    fn op(&mut self, op: u32, a: i64, b: i64, time: f32) -> i64;
    /// The manager's numbers, for one short look or change (no callback runs inside).
    fn fields<R>(&mut self, f: impl FnOnce(&mut EeScenarioManagerFields) -> R) -> R;
    /// The timed effects, for one short look or change.
    fn queue<R>(&mut self, f: impl FnOnce(&mut EeScenarioTimedQueue) -> R) -> R;
}

fn bits(v: i64) -> f32 {
    f32::from_bits(v as u32)
}

/// `EEScenarioManager::Update`.
pub fn update<H: Round>(h: &mut H) {
    let speed = bits(h.op(EE_SCENARIO_ROUND_GAME_SPEED, 0, 0, 0.0));
    let mut tick = bits(h.op(EE_SCENARIO_ROUND_TICK_SECONDS, 0, 0, 0.0)) * 1000.0;
    if h.op(EE_SCENARIO_ROUND_MESSAGE_COUNT, 0, 0, 0.0) > 0 && !h.fields(|f| f.mIsShowingDialog) {
        show_dialog(h);
    }
    let human = h.op(EE_SCENARIO_ROUND_HUMAN_PLAYING, 0, 0, 0.0) != 0;
    if h.op(EE_SCENARIO_ROUND_IN_CINEMATIC, 0, 0, 0.0) != 0 {
        tick /= speed;
    }
    h.fields(|f| {
        f.mTime += tick;
        f.mTriggerTimer += tick;
    });
    if human {
        if h.fields(|f| f.mIsMovingCamera && f.mTriggerTimer >= 0.0) {
            h.op(EE_SCENARIO_ROUND_MOVE_CAMERA, 0, 0, 0.0);
        }
        if h.fields(|f| f.mTriggerTimer >= TRIGGER_INTERVAL) {
            round(h, human);
        }
    }
    if h.fields(|f| f.mIsEndingCinematic) && bits(h.op(EE_SCENARIO_ROUND_LETTERBOX, 0, 0, 0.0)) == 1.0 {
        h.fields(|f| f.mIsEndingCinematic = false);
        h.op(EE_SCENARIO_ROUND_EXIT_CINEMATIC, 0, 0, 0.0);
    }
}

/// `EEScenarioManager::ShowDialogMessage`: nothing while the game waits; the next dialog message (in a cinematic the
/// first cinematic one) shown and `mIsShowingDialog` set, or, with none to show, the messages hidden.
pub fn show_dialog<H: Round>(h: &mut H) {
    h.fields(|f| f.mIsShowingDialog = false);
    if h.op(EE_SCENARIO_ROUND_DIALOG_PAUSED, 0, 0, 0.0) != 0 {
        return;
    }
    let count = h.op(EE_SCENARIO_ROUND_MESSAGE_COUNT, 0, 0, 0.0);
    if count > 0 {
        let index = if h.op(EE_SCENARIO_ROUND_IN_CINEMATIC, 0, 0, 0.0) != 0 {
            (0..count).find(|&i| h.op(EE_SCENARIO_ROUND_MESSAGE_CINEMATIC, i, 0, 0.0) != 0)
        } else {
            Some(0)
        };
        if let Some(i) = index {
            h.op(EE_SCENARIO_ROUND_SHOW_MESSAGE, i, 0, 0.0);
            h.fields(|f| f.mIsShowingDialog = true);
            return;
        }
    }
    h.op(EE_SCENARIO_ROUND_HIDE_MESSAGES, 0, 0, 0.0);
}

/// The trigger round: the triggers (their count read once, as a `long`), then the timed effects in order (the list's
/// size read again each step: an effect may add or re-arm one), each due one (its time as a float at most the clock)
/// whose effect exists, not run, and in the cinematic when one plays, marked run and executed; then the run ones
/// erased, the timer reset and the chat text cleared.
// the negated test is the original's: a NaN clock skips every entry
#[allow(clippy::neg_cmp_op_on_partial_ord)]
fn round<H: Round>(h: &mut H, human: bool) {
    let count = h.op(EE_SCENARIO_ROUND_TRIGGER_COUNT, 0, 0, 0.0) as i32;
    for t in 0..count {
        let time = h.fields(|f| f.mTime);
        h.op(EE_SCENARIO_ROUND_EVALUATE_TRIGGER, t as i64, 0, time);
    }
    let mut e: i32 = 0;
    while e < h.queue(|q| q.entries.len() as i32) {
        let entry = h.queue(|q| q.entries[e as usize]);
        if !(entry.mTime as f32 <= h.fields(|f| f.mTime)) {
            e += 1;
            continue;
        }
        if h.op(EE_SCENARIO_ROUND_EFFECT_EXISTS, entry.mEffectIndex as i64, 0, 0.0) != 0
            && !entry.mHasRun
            && (h.op(EE_SCENARIO_ROUND_IN_CINEMATIC, 0, 0, 0.0) == 0 || entry.mIsInCinematic)
        {
            h.queue(|q| q.entries[e as usize].mHasRun = true);
            h.op(EE_SCENARIO_ROUND_EXECUTE, entry.mEffectIndex as i64, entry.mIsInCinematic as i64, 0.0);
        }
        e += 1;
    }
    h.queue(|q| q.erase_run());
    h.fields(|f| f.mTriggerTimer = 0.0);
    h.op(EE_SCENARIO_ROUND_CLEAR_CHAT, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_CAPTURE, human as i64, 0, 0.0);
}

/// `EEScenarioManager::MoveCameraToStart`'s step: the camera's focus `at` moved by up to `step` (shared between x and
/// y by their distances, each step at most its distance) towards the scenario's start; at the start already,
/// `mIsMovingCamera` cleared and `false` (the focus unmoved).
pub fn camera_step(m: &mut EeScenarioManagerFields, at: &mut [f32; 2], step: f32) -> bool {
    if at[0] == m.mStartX && at[1] == m.mStartY {
        m.mIsMovingCamera = false;
        return false;
    }
    let dx = at[0] - m.mStartX;
    let abs_dx = dx.abs();
    let dy = at[1] - m.mStartY;
    let abs_dy = dy.abs();
    let total = abs_dy + abs_dx;
    let mut step_x = abs_dx * step / total;
    let mut step_y = abs_dy * step / total;
    if step_x > dx.abs() {
        step_x = dx.abs();
    }
    if at[0] > m.mStartX {
        step_x = -step_x;
    }
    if step_y > dy.abs() {
        step_y = dy.abs();
    }
    if at[1] > m.mStartY {
        step_y = -step_y;
    }
    at[0] += step_x;
    at[1] += step_y;
    true
}

/// The C ABI's round host: the manager's numbers and queue, and the operation callback.
#[repr(C)]
pub struct EeScenarioRoundHost {
    pub ctx: *mut c_void,
    pub fields: *mut EeScenarioManagerFields,
    pub queue: *mut EeScenarioTimedQueue,
    pub op: unsafe extern "C" fn(ctx: *mut c_void, op: u32, a: i64, b: i64, time: f32) -> i64,
}

impl Round for EeScenarioRoundHost {
    fn op(&mut self, op: u32, a: i64, b: i64, time: f32) -> i64 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed the table over, `op` callable with it
        unsafe { (self.op)(self.ctx, op, a, b, time) }
    }
    fn fields<R>(&mut self, f: impl FnOnce(&mut EeScenarioManagerFields) -> R) -> R {
        // SAFETY: the table's contract: `fields` is the live manager's; the borrow ends before the next callback
        f(unsafe { &mut *self.fields })
    }
    fn queue<R>(&mut self, f: impl FnOnce(&mut EeScenarioTimedQueue) -> R) -> R {
        // SAFETY: the table's contract: `queue` is the live manager's; the borrow ends before the next callback
        f(unsafe { &mut *self.queue })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_camera_steps_towards_the_start() {
        let mut m =
            EeScenarioManagerFields { mStartX: 10.0, mStartY: 20.0, mIsMovingCamera: true, ..Default::default() };
        let mut at = [13.0f32, 24.0];
        assert!(camera_step(&mut m, &mut at, 5.0));
        // 3 + 4 = 7: x by 5 * 3 / 7, y by 5 * 4 / 7, both back towards the start
        assert_eq!(at, [13.0 - 3.0 * 5.0 / 7.0, 24.0 - 4.0 * 5.0 / 7.0]);
        let mut at = [11.0f32, 20.0];
        assert!(camera_step(&mut m, &mut at, 5.0));
        assert_eq!(at, [10.0, 20.0]);
        assert!(m.mIsMovingCamera);
        assert!(!camera_step(&mut m, &mut at, 5.0));
        assert!(!m.mIsMovingCamera);
    }
    use crate::scenario::manager::EeScenarioTimedEntry;
    use std::os::raw::c_ulong;

    /// A world of `triggers` triggers whose effects log their runs; `on_execute` runs inside an effect.
    struct Mock {
        f: EeScenarioManagerFields,
        q: EeScenarioTimedQueue,
        speed: f32,
        tick: f32,
        messages: i64,
        paused: bool,
        cinematic_messages: Vec<i64>,
        human: bool,
        cinematic: bool,
        triggers: i64,
        letterbox: f32,
        log: Vec<String>,
        on_execute: fn(&mut Mock, i64),
    }

    impl Mock {
        fn new() -> Self {
            Mock {
                f: EeScenarioManagerFields::default(),
                q: EeScenarioTimedQueue::default(),
                speed: 1.0,
                tick: 0.1,
                messages: 0,
                paused: false,
                cinematic_messages: Vec::new(),
                human: true,
                cinematic: false,
                triggers: 2,
                letterbox: 0.0,
                log: Vec::new(),
                on_execute: |_, _| {},
            }
        }
    }

    impl Round for Mock {
        fn op(&mut self, op: u32, a: i64, b: i64, time: f32) -> i64 {
            match op {
                EE_SCENARIO_ROUND_GAME_SPEED => self.speed.to_bits() as i64,
                EE_SCENARIO_ROUND_TICK_SECONDS => self.tick.to_bits() as i64,
                EE_SCENARIO_ROUND_MESSAGE_COUNT => self.messages,
                EE_SCENARIO_ROUND_DIALOG_PAUSED => self.paused as i64,
                EE_SCENARIO_ROUND_MESSAGE_CINEMATIC => self.cinematic_messages.contains(&a) as i64,
                EE_SCENARIO_ROUND_SHOW_MESSAGE => {
                    self.log.push(format!("dialog {a}"));
                    0
                }
                EE_SCENARIO_ROUND_HIDE_MESSAGES => {
                    self.log.push("hide".into());
                    0
                }
                EE_SCENARIO_ROUND_HUMAN_PLAYING => self.human as i64,
                EE_SCENARIO_ROUND_IN_CINEMATIC => self.cinematic as i64,
                EE_SCENARIO_ROUND_MOVE_CAMERA => {
                    self.log.push("camera".into());
                    0
                }
                EE_SCENARIO_ROUND_TRIGGER_COUNT => self.triggers,
                EE_SCENARIO_ROUND_EVALUATE_TRIGGER => {
                    self.log.push(format!("trigger {a} {time}"));
                    0
                }
                EE_SCENARIO_ROUND_EFFECT_EXISTS => (a != 9) as i64,
                EE_SCENARIO_ROUND_EXECUTE => {
                    self.log.push(format!("execute {a} {b}"));
                    let f = self.on_execute;
                    f(self, a);
                    0
                }
                EE_SCENARIO_ROUND_CLEAR_CHAT => {
                    self.log.push("chat".into());
                    0
                }
                EE_SCENARIO_ROUND_CAPTURE => {
                    self.log.push(format!("capture {a}"));
                    0
                }
                EE_SCENARIO_ROUND_LETTERBOX => self.letterbox.to_bits() as i64,
                EE_SCENARIO_ROUND_EXIT_CINEMATIC => {
                    self.log.push("exit".into());
                    0
                }
                _ => panic!("op {op}"),
            }
        }
        fn fields<R>(&mut self, f: impl FnOnce(&mut EeScenarioManagerFields) -> R) -> R {
            f(&mut self.f)
        }
        fn queue<R>(&mut self, f: impl FnOnce(&mut EeScenarioTimedQueue) -> R) -> R {
            f(&mut self.q)
        }
    }

    fn entry(effect: c_ulong, time: c_ulong, cinematic: bool) -> EeScenarioTimedEntry {
        EeScenarioTimedEntry { mEffectIndex: effect, mTime: time, mIsInCinematic: cinematic, mHasRun: false }
    }

    #[test]
    fn the_clocks_run_every_tick_and_the_round_every_500_ms() {
        let mut m = Mock::new();
        let tick = 0.1f32 * 1000.0;
        for _ in 0..4 {
            update(&mut m);
        }
        assert!(m.log.is_empty());
        assert_eq!(m.f.mTime, tick + tick + tick + tick);
        update(&mut m);
        let t = tick * 5.0;
        assert_eq!(m.log, [format!("trigger 0 {t}"), format!("trigger 1 {t}"), "chat".into(), "capture 1".into()]);
        assert_eq!(m.f.mTriggerTimer, 0.0);
        // in a cinematic the tick is divided by the speed
        let mut m = Mock::new();
        m.cinematic = true;
        m.speed = 0.25;
        update(&mut m);
        assert_eq!(m.f.mTime, tick / 0.25);
        // no human: the clocks only
        let mut m = Mock::new();
        m.human = false;
        m.f.mTriggerTimer = 900.0;
        m.f.mIsMovingCamera = true;
        update(&mut m);
        assert!(m.log.is_empty());
    }

    #[test]
    fn due_effects_run_once_in_order_and_the_run_ones_go() {
        let mut m = Mock::new();
        m.triggers = 0;
        m.f.mTriggerTimer = 450.0;
        m.f.mTime = 1900.0;
        m.q.entries = vec![entry(1, 1000, false), entry(9, 1500, false), entry(2, 2000, false), entry(3, 3000, true)];
        update(&mut m);
        assert_eq!(m.log, ["execute 1 0", "execute 2 0", "chat", "capture 1"]);
        assert_eq!(m.q.entries, vec![entry(9, 1500, false), entry(3, 3000, true)]);
        // in a cinematic only the cinematic's
        let mut m = Mock::new();
        m.triggers = 0;
        m.cinematic = true;
        m.f.mTriggerTimer = 500.0;
        m.f.mTime = 5000.0;
        m.q.entries = vec![entry(1, 1000, false), entry(3, 3000, true)];
        update(&mut m);
        assert_eq!(m.log, ["execute 3 1", "chat", "capture 1"]);
        assert_eq!(m.q.entries, vec![entry(1, 1000, false)]);
    }

    #[test]
    fn an_effect_may_add_or_rearm_timed_effects_inside_the_round() {
        let mut m = Mock::new();
        m.triggers = 0;
        m.f.mTriggerTimer = 500.0;
        m.f.mTime = 1000.0;
        m.q.entries = vec![entry(1, 100, false), entry(2, 200, false)];
        // effect 1's first run schedules effect 4 earlier than itself and re-arms itself (as End Cinematic waiting
        // for its dialogs does)
        m.on_execute = |m, a| {
            if a == 1 && !m.q.entries.iter().any(|e| e.mEffectIndex == 4) {
                m.q.add(4, 50, false);
                m.q.set_has_run(Some(1), false);
            }
        };
        update(&mut m);
        // the new entry sorts first and shifts the rest: entry 1 comes up again (re-armed, it runs again), then 2;
        // the new 4 was passed over and waits for the next round
        assert_eq!(m.log, ["execute 1 0", "execute 1 0", "execute 2 0", "chat", "capture 1"]);
        assert_eq!(m.q.entries, vec![entry(4, 50, false)]);
    }

    #[test]
    fn the_dialog_the_camera_and_the_end_of_a_cinematic() {
        let mut m = Mock::new();
        m.messages = 2;
        m.f.mIsMovingCamera = true;
        m.f.mIsEndingCinematic = true;
        m.letterbox = 0.5;
        update(&mut m);
        assert_eq!(m.log, ["dialog 0", "camera"]);
        assert!(m.f.mIsEndingCinematic);
        assert!(m.f.mIsShowingDialog);
        m.letterbox = 1.0;
        update(&mut m);
        assert_eq!(m.log, ["dialog 0", "camera", "camera", "exit"]);
        assert!(!m.f.mIsEndingCinematic);
    }

    #[test]
    fn a_cinematic_shows_only_its_own_messages() {
        let mut m = Mock::new();
        m.messages = 3;
        m.cinematic = true;
        m.cinematic_messages = vec![2];
        show_dialog(&mut m);
        assert_eq!(m.log, ["dialog 2"]);
        assert!(m.f.mIsShowingDialog);
        m.cinematic_messages.clear();
        show_dialog(&mut m);
        assert_eq!(m.log, ["dialog 2", "hide"]);
        assert!(!m.f.mIsShowingDialog);
        m.paused = true;
        m.f.mIsShowingDialog = true;
        show_dialog(&mut m);
        assert_eq!(m.log.len(), 2);
        assert!(!m.f.mIsShowingDialog);
    }
}
