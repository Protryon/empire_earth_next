//! TEST ONLY: the scripted editor's steps on the UI thread (were EESimEditor.cpp's EESimEditorStep,
//! EESimEditorMarker, EESimEditorDone and RunStep; game core lane, round 12, fork D). The C++ keeps one action class
//! and one event class whose bodies call these (`ee_sim_driver_sed_action`, `ee_sim_driver_sed_done`).
//!
//! A step: an action in the UI thread's action queue (under the UI copy lock and the world's write lock, as every
//! editor action runs), the clock pinned, the step's code, then the marker. The marker posts itself again three
//! times (an action run by a step's action, e.g. the blank map's terrain paint, is queued after it), then posts the
//! step's end to the UI thread's message queue (the events a step posted there, e.g. the hide-UI box's viewport
//! change, are processed first: one queue, in order). The end makes its rounds more trips through the action queue
//! first (a step makes one), then lets the clock run and wakes the driver. The round: a control's trigger event (a
//! list's selection, a question's Yes, a check box's change) is processed by a UI update, and what it posts reached
//! the queue after the first end, so the next step read the state before or after them depending on when the update
//! came (the gates' flakes under load). After the round every such event is processed before the end.

use std::ffi::c_void;
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Condvar, Mutex};
use std::time::Duration;

use super::sed_host::{h, EeSimDriverSed as O};
use crate::gamecpp as gc;

/// What an action or the end does when the UI thread runs it.
pub enum Act {
    /// a step: its code and context, the end's rounds
    Step { code: extern "C" fn(*mut c_void), ctx: *mut c_void, rounds: i64 },
    /// the step's last action: reposts left, then the end with its rounds
    Marker { left: i64, rounds: i64 },
    /// the step's end (an event on the UI thread's message queue)
    Done { rounds: i64 },
    /// a call on the UI thread (the UI fixture's marker and sends)
    Call { code: fn(usize), arg: usize },
}

/// The driver's wait: the step finished.
static FINISHED: Mutex<bool> = Mutex::new(false);
static DONE: Condvar = Condvar::new();
/// The time TMGetTime answers on the UI thread for the next step.
static CLOCK: AtomicI64 = AtomicI64::new(1000);

/// An action (or, for [`Act::Done`], an event) of the act posted to the UI thread; the act is freed after its last run.
fn post(act: Act) {
    let done = matches!(act, Act::Done { .. });
    let state = Box::into_raw(Box::new(act)) as usize;
    let item = h(if done { O::C12DoneNew } else { O::C12ActionNew }, state, &[]) as usize;
    gc::h(if done { gc::O::UiPostEvent } else { gc::O::EntryPostAction }, item, &[]);
    h(O::C12Dereference, item, &[i64::from(done)]);
}

/// The marker's run: one fewer left and true to repost it, else the end's rounds.
pub fn marker_step(left: &mut i64) -> bool {
    let repost = *left > 0;
    *left -= 1;
    repost
}

/// The end's run: true when the step is over (no round left), else the next marker's rounds.
pub fn done_step(rounds: i64) -> Option<i64> {
    if rounds > 0 {
        Some(rounds - 1)
    } else {
        None
    }
}

#[cfg(not(test))]
fn pin(time: i64) {
    ee_base::time::ee_time_pin(time);
}
#[cfg(test)]
fn pin(_: i64) {}

/// An action's run (EEAction::Virtual4 of the C++ action class): the step's code then its marker, or the marker's
/// repost or end.
///
/// # Safety
/// `action` is the C++ action, `state` its act from [`post`].
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_sed_action(action: *mut c_void, state: *mut c_void) {
    let act = state as *mut Act;
    // SAFETY: the act this module posted; the UI thread runs one action at a time
    match unsafe { &mut *act } {
        Act::Step { code, ctx, rounds } => {
            let (code, ctx, rounds) = (*code, *ctx, *rounds);
            // SAFETY: the act's last run
            drop(unsafe { Box::from_raw(act) });
            pin(CLOCK.load(Ordering::SeqCst));
            code(ctx);
            post(Act::Marker { left: 3, rounds });
        }
        Act::Marker { left, rounds } => {
            if marker_step(left) {
                gc::h(gc::O::EntryPostAction, action as usize, &[]);
                return;
            }
            let rounds = *rounds;
            // SAFETY: the act's last run
            drop(unsafe { Box::from_raw(act) });
            post(Act::Done { rounds });
        }
        Act::Call { code, arg } => {
            let (code, arg) = (*code, *arg);
            // SAFETY: the act's only run
            drop(unsafe { Box::from_raw(act) });
            code(arg);
        }
        Act::Done { .. } => {}
    }
}

/// `code(arg)` run by the UI thread as an action of its own (the UI fixture's: was EETestFixture.cpp's FixtureAction).
pub fn post_call(code: fn(usize), arg: usize) {
    post(Act::Call { code, arg });
}

/// The end's run (UIEvent::ProcessEvent of the C++ event class): another round, or the clock let run and the driver
/// woken. Always 0.
///
/// # Safety
/// `state` is the event's act from [`post`].
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_sed_done(state: *mut c_void) -> i64 {
    // SAFETY: the act this module posted, run once
    let act = unsafe { Box::from_raw(state as *mut Act) };
    let Act::Done { rounds } = *act else { return 0 };
    match done_step(rounds) {
        Some(next) => post(Act::Marker { left: 3, rounds: next }),
        None => {
            pin(-1);
            *FINISHED.lock().unwrap_or_else(|e| e.into_inner()) = true;
            DONE.notify_all();
        }
    }
    0
}

/// RunStep: the code run as a step and its end waited for; false after a minute without it.
pub fn run(code: extern "C" fn(*mut c_void), ctx: *mut c_void) -> bool {
    *FINISHED.lock().unwrap_or_else(|e| e.into_inner()) = false;
    CLOCK.fetch_add(1000, Ordering::SeqCst);
    post(Act::Step { code, ctx, rounds: 1 });
    let guard = FINISHED.lock().unwrap_or_else(|e| e.into_inner());
    let (guard, _) = DONE
        .wait_timeout_while(guard, Duration::from_secs(60), |finished| !*finished)
        .unwrap_or_else(|e| e.into_inner());
    *guard
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_marker_reposts_three_times() {
        let mut left = 3;
        let runs: Vec<bool> = (0..4).map(|_| marker_step(&mut left)).collect();
        assert_eq!(runs, [true, true, true, false]);
    }

    #[test]
    fn the_end_makes_its_rounds() {
        assert_eq!(done_step(1), Some(0));
        assert_eq!(done_step(0), None);
    }
}
