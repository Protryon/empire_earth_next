//! The UI thread's action queues (`EEUserInterface`'s action, undo and redo queues): the actions other threads and
//! the screens post, run by the UI thread under the UI copy lock and the world's write lock; the undoable ones kept
//! (at most 100) for the scenario editor's Undo and Redo. The actions are C++ objects (`EEAction`, reference
//! counted), held here as handles; the host ([`ActionHost`], `User Interface Thread/EEUserInterface.cpp`) references,
//! releases, runs and undoes them and takes the locks.

use std::collections::VecDeque;
use std::sync::Mutex;

/// The host's ops (`ee_screenscpp_action`): an action's reference taken, released, run (`Virtual4`), undone
/// (`Virtual8`), asked whether it is undoable; the UI copy lock and the world's write lock taken and released.
pub const EE_SCREENS_ACTION_REFERENCE: u32 = 0;
pub const EE_SCREENS_ACTION_DEREFERENCE: u32 = 1;
pub const EE_SCREENS_ACTION_RUN: u32 = 2;
pub const EE_SCREENS_ACTION_UNDO: u32 = 3;
pub const EE_SCREENS_ACTION_UNDOABLE: u32 = 4;
pub const EE_SCREENS_ACTION_LOCK: u32 = 5;
pub const EE_SCREENS_ACTION_UNLOCK: u32 = 6;

/// Undo and Redo's answer when there is nothing to undo or redo.
pub const EE_SCREENS_ACTION_NONE: i32 = 7;

/// The undo list's length.
const ACTIONS_UNDO_LIMIT: usize = 100;

/// An action (`EEAction*`).
pub type Action = usize;

/// The action's calls and the locks.
pub trait ActionHost {
    fn call(&mut self, op: u32, action: Action) -> u32;
}

/// The three queues, each locked only around its own push or pop (never across a host call: running or releasing an
/// action may post another).
#[derive(Default)]
pub struct Actions {
    queue: Mutex<VecDeque<Action>>,
    undo: Mutex<VecDeque<Action>>,
    redo: Mutex<VecDeque<Action>>,
}

fn pop_front(q: &Mutex<VecDeque<Action>>) -> Option<Action> {
    q.lock().unwrap().pop_front()
}

fn pop_back(q: &Mutex<VecDeque<Action>>) -> Option<Action> {
    q.lock().unwrap().pop_back()
}

fn push_back(q: &Mutex<VecDeque<Action>>, action: Action) {
    q.lock().unwrap().push_back(action);
}

fn len(q: &Mutex<VecDeque<Action>>) -> usize {
    q.lock().unwrap().len()
}

/// Which queue `clear` empties.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Queue {
    Actions,
    Undo,
    Redo,
}

impl Actions {
    pub const fn new() -> Actions {
        Actions {
            queue: Mutex::new(VecDeque::new()),
            undo: Mutex::new(VecDeque::new()),
            redo: Mutex::new(VecDeque::new()),
        }
    }

    fn of(&self, queue: Queue) -> &Mutex<VecDeque<Action>> {
        match queue {
            Queue::Actions => &self.queue,
            Queue::Undo => &self.undo,
            Queue::Redo => &self.redo,
        }
    }

    /// `PostAction`: the action queued with a reference of its own.
    pub fn post<H: ActionHost + ?Sized>(&self, h: &mut H, action: Action) {
        h.call(EE_SCREENS_ACTION_REFERENCE, action);
        push_back(&self.queue, action);
    }

    /// `ProcessActions`: when actions wait, under the locks, each run in turn; one not undoable released; an undoable
    /// one clears the redo list and goes on the undo list (released instead when it is the undo list's last action
    /// already), the oldest released past 100.
    pub fn process<H: ActionHost + ?Sized>(&self, h: &mut H) {
        if len(&self.queue) == 0 {
            return;
        }
        h.call(EE_SCREENS_ACTION_LOCK, 0);
        let last = self.undo.lock().unwrap().back().copied();
        while let Some(action) = pop_front(&self.queue) {
            h.call(EE_SCREENS_ACTION_RUN, action);
            if h.call(EE_SCREENS_ACTION_UNDOABLE, action) == 0 {
                h.call(EE_SCREENS_ACTION_DEREFERENCE, action);
                continue;
            }
            while let Some(redo) = pop_front(&self.redo) {
                h.call(EE_SCREENS_ACTION_DEREFERENCE, redo);
            }
            if Some(action) == last {
                h.call(EE_SCREENS_ACTION_DEREFERENCE, action);
            } else {
                push_back(&self.undo, action);
                if len(&self.undo) > ACTIONS_UNDO_LIMIT {
                    if let Some(oldest) = pop_front(&self.undo) {
                        h.call(EE_SCREENS_ACTION_DEREFERENCE, oldest);
                    }
                }
            }
        }
        h.call(EE_SCREENS_ACTION_UNLOCK, 0);
    }

    /// `Undo`: the last undoable action undone onto the redo list; `EE_SCREENS_ACTION_NONE` when there is none.
    pub fn undo<H: ActionHost + ?Sized>(&self, h: &mut H) -> i32 {
        let Some(action) = pop_back(&self.undo) else { return EE_SCREENS_ACTION_NONE };
        h.call(EE_SCREENS_ACTION_UNDO, action);
        push_back(&self.redo, action);
        0
    }

    /// `Redo`: the last undone action run again onto the undo list; `EE_SCREENS_ACTION_NONE` when there is none.
    pub fn redo<H: ActionHost + ?Sized>(&self, h: &mut H) -> i32 {
        let Some(action) = pop_back(&self.redo) else { return EE_SCREENS_ACTION_NONE };
        h.call(EE_SCREENS_ACTION_RUN, action);
        push_back(&self.undo, action);
        0
    }

    /// `ClearQueue`: every action of the queue released, first to last.
    pub fn clear<H: ActionHost + ?Sized>(&self, h: &mut H, queue: Queue) {
        while let Some(action) = pop_front(self.of(queue)) {
            h.call(EE_SCREENS_ACTION_DEREFERENCE, action);
        }
    }

    /// The queue's length.
    pub fn len(&self, queue: Queue) -> usize {
        len(self.of(queue))
    }
}

#[cfg(test)]
mod golden;
