//! `extern "C"` for the UI thread's action queues (`actions.rs`): `EEUserInterface`'s `PostAction`,
//! `ProcessActions`, `Undo`, `Redo` and the queues' clears call these; the host is `ee_screenscpp_action`
//! (`User Interface Thread/EEUserInterface.cpp`).

use core::ffi::{c_long, c_void};

use crate::actions::{Action, ActionHost, Actions, Queue};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_action(op: u32, action: *mut c_void) -> u32;
    }
}

/// The UI thread's queues (one user interface).
static ACTIONS: Actions = Actions::new();

struct Host;

impl ActionHost for Host {
    fn call(&mut self, op: u32, action: Action) -> u32 {
        unsafe { sys::ee_screenscpp_action(op, action as *mut c_void) }
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
