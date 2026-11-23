//! `extern "C"` for the UI thread's action queues (`actions.rs`): `EEUserInterface`'s `PostAction`,
//! `ProcessActions`, `Undo`, `Redo` and the queues' clears call these; the host is `ee_screenscpp_action`
//! (`User Interface Thread/EEUserInterface.cpp`).

use core::ffi::{c_long, c_void};

use crate::screens::actions::{Action, ActionHost, Actions, Queue};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_action_reference(action: *mut c_void);
        pub fn ee_screenscpp_action_release(action: *mut c_void);
        pub fn ee_screenscpp_action_run(action: *mut c_void);
        pub fn ee_screenscpp_action_undo(action: *mut c_void);
        pub fn ee_screenscpp_action_undoable(action: *mut c_void) -> bool;
    }
}

/// The UI thread's queues (one user interface).
static ACTIONS: Actions = Actions::new();

struct Host;

impl ActionHost for Host {
    fn call(&mut self, op: u32, action: Action) -> u32 {
        if let Some(r) = crate::screens::ui_calls::action(&mut crate::screens::game_cpp::Ffi, op) {
            return r;
        }
        use crate::screens::actions::{
            EE_SCREENS_ACTION_DEREFERENCE, EE_SCREENS_ACTION_REFERENCE, EE_SCREENS_ACTION_RUN, EE_SCREENS_ACTION_UNDO,
            EE_SCREENS_ACTION_UNDOABLE,
        };
        let a = action as *mut c_void;
        // SAFETY: the queues' live action
        unsafe {
            match op {
                EE_SCREENS_ACTION_REFERENCE => sys::ee_screenscpp_action_reference(a),
                EE_SCREENS_ACTION_DEREFERENCE => sys::ee_screenscpp_action_release(a),
                EE_SCREENS_ACTION_RUN => sys::ee_screenscpp_action_run(a),
                EE_SCREENS_ACTION_UNDO => sys::ee_screenscpp_action_undo(a),
                EE_SCREENS_ACTION_UNDOABLE => return u32::from(sys::ee_screenscpp_action_undoable(a)),
                _ => {}
            }
        }
        0
    }
}

/// `EEUserInterface::PostAction`: queues the action (`EEAction*`) with a reference of its own.
#[no_mangle]
pub extern "C" fn ee_screens_actions_post(action: *mut c_void) {
    ACTIONS.post(&mut Host, action as Action);
}

/// `EEUserInterface::ProcessActions`: runs the queued actions under the UI copy lock and the world's write lock.
#[no_mangle]
pub extern "C" fn ee_screens_actions_process() {
    ACTIONS.process(&mut Host);
}

/// `EEUserInterface::Undo`: 0, or 7 when there is nothing to undo.
#[no_mangle]
pub extern "C" fn ee_screens_actions_undo() -> c_long {
    ACTIONS.undo(&mut Host) as c_long
}

/// `EEUserInterface::Redo`: 0, or 7 when there is nothing to redo.
#[no_mangle]
pub extern "C" fn ee_screens_actions_redo() -> c_long {
    ACTIONS.redo(&mut Host) as c_long
}

/// `EEUserInterface::ClearQueue`: releases every action of a queue (0 the action queue, 1 the undo list, 2 the redo
/// list).
#[no_mangle]
pub extern "C" fn ee_screens_actions_clear(queue: u32) {
    let queue = match queue {
        0 => Queue::Actions,
        1 => Queue::Undo,
        _ => Queue::Redo,
    };
    ACTIONS.clear(&mut Host, queue);
}
