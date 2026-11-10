//! The state machines of WON's async-op machinery (`src/Empire Earth/WONAPI/WONCommon/AsyncOp.cpp`,
//! `AsyncOpContainer.cpp`, `TimerThread.cpp`, `WONSocket/SocketThread.cpp`, `QueueSocket.cpp`): an op's run
//! state and its transitions, the timer thread's deadline maps, the op container's queue and pump accounting, the
//! simple socket thread's op set, and the queue socket's decisions when an op finishes. The C++ classes keep their
//! objects, locks, threads, events and completions: they hold the ops alive (`AddRef`) while a Rust queue holds a
//! handle, and act on the decisions these functions return.

use crate::ops::MODE_ASYNC;
use crate::status::*;
use std::collections::{BTreeMap, BTreeSet, VecDeque};

/// An `AsyncOp*` the C++ keeps alive while a queue holds it. Never dereferenced here.
pub type OpHandle = usize;

/// `AsyncOp`'s run state: `mStatus`, `mRunMode`, `mStartTime`, `mTimeout`, `mCompletionPending`, `mKilled` and
/// `mRunAsyncImmediately`, copied in and out by the C++ under the op's lock.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OpState {
    /// The `WONStatus` (a 32-bit `long` in the C++).
    pub status: i32,
    pub run_mode: i32,
    pub start: u32,
    pub timeout: u32,
    pub completion_pending: bool,
    pub killed: bool,
    pub run_async_immediately: bool,
}

/// What `AsyncOp::Run` does after the state changed.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RunAction {
    /// Already running: `WS_Pending` is returned, nothing changes.
    Pending,
    /// A completion is pending: `WS_AwaitingCompletion` is returned, nothing changes.
    Awaiting,
    /// An async run that starts at once (`StartRunAsync`).
    StartAsync,
    /// An async run queued in the op container.
    Queue,
    /// A blocking run: the run hook, then the completion.
    Block,
}

impl RunAction {
    pub fn code(self) -> i32 {
        match self {
            RunAction::Pending => 0,
            RunAction::Awaiting => 1,
            RunAction::StartAsync => 2,
            RunAction::Queue => 3,
            RunAction::Block => 4,
        }
    }
}

/// What `AsyncOp::Finish` does.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FinishAction {
    /// Not running (or a completion pending, for `ForceFinish`): nothing.
    Ignored,
    /// Finished, an async run: queue the op in its container for the completion.
    Queue,
    /// Finished, a blocking run: the caller completes it.
    Done,
}

impl FinishAction {
    pub fn code(self) -> i32 {
        match self {
            FinishAction::Ignored => 0,
            FinishAction::Queue => 1,
            FinishAction::Done => 2,
        }
    }
}

/// What `AsyncOp::StartRunAsync` does once the run hook returned.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TimerAction {
    /// Finished in the hook, or an infinite timeout: no timer.
    None,
    /// Arm the timeout timer.
    Arm,
    /// A zero timeout: finish with `WS_TimedOut` now.
    TimeOutNow,
}

impl TimerAction {
    pub fn code(self) -> i32 {
        match self {
            TimerAction::None => 0,
            TimerAction::Arm => 1,
            TimerAction::TimeOutNow => 2,
        }
    }
}

impl Default for OpState {
    fn default() -> Self {
        Self::new()
    }
}

impl OpState {
    /// `AsyncOp::AsyncOp`: idle, an infinite timeout, async, not started.
    pub fn new() -> Self {
        OpState {
            status: WS_NONE as i32,
            run_mode: MODE_ASYNC,
            start: 0,
            timeout: 0xffff_ffff,
            completion_pending: false,
            killed: false,
            run_async_immediately: false,
        }
    }

    pub fn status(&self) -> Status {
        self.status as Status
    }

    pub fn is_pending(&self) -> bool {
        self.status() == WS_PENDING
    }

    /// `AsyncOp::SetTimeout`: unless the op is running.
    pub fn set_timeout(&mut self, timeout: u32) {
        if !self.is_pending() {
            self.timeout = timeout;
        }
    }

    /// `AsyncOp::SetMode`: unless the op is running.
    pub fn set_mode(&mut self, mode: i32) {
        if !self.is_pending() {
            self.run_mode = mode;
        }
    }

    /// `AsyncOp::Run` at `now`.
    pub fn run(&mut self, mode: i32, timeout: u32, now: u32) -> RunAction {
        if self.is_pending() {
            return RunAction::Pending;
        }
        if self.completion_pending {
            return RunAction::Awaiting;
        }
        self.status = WS_PENDING as i32;
        self.killed = false;
        self.start = now;
        self.timeout = timeout;
        self.run_mode = mode;
        if mode == MODE_ASYNC {
            if self.run_async_immediately {
                RunAction::StartAsync
            } else {
                RunAction::Queue
            }
        } else {
            RunAction::Block
        }
    }

    /// `AsyncOp::RunAsTimer` at `now`: true when the timer is to be armed (the op was idle).
    pub fn run_as_timer(&mut self, timeout: u32, now: u32) -> bool {
        if self.is_pending() || self.completion_pending {
            return false;
        }
        self.status = WS_PENDING as i32;
        self.killed = false;
        self.start = now;
        self.timeout = timeout;
        self.run_mode = MODE_ASYNC;
        true
    }

    /// `AsyncOp::Finish`.
    pub fn finish(&mut self, status: Status) -> FinishAction {
        if !self.is_pending() {
            return FinishAction::Ignored;
        }
        self.status = status as i32;
        self.completion_pending = true;
        if self.run_mode == MODE_ASYNC {
            FinishAction::Queue
        } else {
            FinishAction::Done
        }
    }

    /// `AsyncOp::ForceFinish`: finishes even a finished op, unless its completion is still pending.
    pub fn force_finish(&mut self, status: Status) -> FinishAction {
        if self.is_pending() {
            return self.finish(status);
        }
        if self.completion_pending {
            return FinishAction::Ignored;
        }
        self.status = WS_PENDING as i32;
        self.killed = false;
        self.finish(status)
    }

    /// `AsyncOp::Kill`.
    pub fn kill(&mut self, finish: bool) -> FinishAction {
        self.killed = true;
        if finish {
            self.finish(WS_KILLED)
        } else {
            FinishAction::Ignored
        }
    }

    /// `AsyncOp::Complete`: true when a completion was pending (it is delivered now).
    pub fn complete(&mut self) -> bool {
        if !self.completion_pending {
            return false;
        }
        self.completion_pending = false;
        true
    }

    /// `AsyncOp::StartRunAsync` after the run hook.
    pub fn after_run_hook(&self) -> TimerAction {
        if !self.is_pending() {
            return TimerAction::None;
        }
        if self.timeout > 0 {
            if self.timeout != 0xffff_ffff {
                TimerAction::Arm
            } else {
                TimerAction::None
            }
        } else {
            TimerAction::TimeOutNow
        }
    }
}

// ---- the timer thread's deadlines

/// `TimerThread`'s two maps: the deadlines of this tick-count era and, past the 32-bit wrap, of the next
/// (`mTimerMap`, `mWrapAroundMap`), each in deadline order and, among equal deadlines, in insertion order (the
/// `multimap`'s), plus `mLastTick`. An entry's key stays valid across the wrap (the C++ copied the wrap map over
/// the first map, which left the ops' iterators dangling).
#[derive(Debug, Default)]
pub struct TimerQueue {
    primary: BTreeMap<(u32, u64), OpHandle>,
    wrap: BTreeMap<(u32, u64), OpHandle>,
    keys: BTreeMap<u64, (bool, u32)>,
    next: u64,
    last_tick: u32,
}

impl TimerQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn len(&self) -> usize {
        self.keys.len()
    }

    pub fn is_empty(&self) -> bool {
        self.keys.is_empty()
    }

    pub fn last_tick(&self) -> u32 {
        self.last_tick
    }

    /// `TimerThread::ThreadFunc`'s start: the tick the wait times count from.
    pub fn set_last_tick(&mut self, now: u32) {
        self.last_tick = now;
    }

    /// `TimerThread::AddTimerOp` at `now`: the op's deadline (in the wrap map when `timeout` passes the tick wrap)
    /// -> the entry's key.
    pub fn add(&mut self, now: u32, timeout: u32, op: OpHandle) -> u64 {
        let key = self.next;
        self.next += 1;
        let until_wrap = 0xffff_ffffu32 - now;
        if timeout > until_wrap {
            let deadline = timeout - until_wrap;
            self.wrap.insert((deadline, key), op);
            self.keys.insert(key, (true, deadline));
        } else {
            let deadline = now + timeout;
            self.primary.insert((deadline, key), op);
            self.keys.insert(key, (false, deadline));
        }
        key
    }

    /// `TimerThread::RemoveTimerOp`: the entry's op, if the key is still there.
    pub fn remove(&mut self, key: u64) -> Option<OpHandle> {
        let (wrapped, deadline) = self.keys.remove(&key)?;
        if wrapped {
            self.wrap.remove(&(deadline, key))
        } else {
            self.primary.remove(&(deadline, key))
        }
    }

    /// `TimerThread::Pump` at `now`: the ops that time out, in order. After a tick wrap every op of the old era
    /// times out and the next era's map becomes the current one.
    pub fn pump(&mut self, now: u32) -> Vec<OpHandle> {
        let mut due = Vec::new();
        if now < self.last_tick {
            for ((_, key), op) in std::mem::take(&mut self.primary) {
                self.keys.remove(&key);
                due.push(op);
            }
            self.primary = std::mem::take(&mut self.wrap);
            for entry in self.keys.values_mut() {
                entry.0 = false;
            }
        }
        self.last_tick = now;
        while let Some((&(deadline, key), &op)) = self.primary.iter().next() {
            if deadline > self.last_tick {
                break;
            }
            self.primary.remove(&(deadline, key));
            self.keys.remove(&key);
            due.push(op);
        }
        due
    }

    /// `TimerThread::GetWaitTime`: until the next deadline, or until just past the tick wrap when none is set.
    pub fn wait_time(&self) -> u32 {
        match self.primary.keys().next() {
            Some(&(deadline, _)) => deadline.saturating_sub(self.last_tick),
            None => 9u32.wrapping_sub(self.last_tick),
        }
    }

    /// `TimerThread::PurgeOps`: every op (this era's in order, then the next's), the maps emptied, the tick reset.
    pub fn purge(&mut self) -> Vec<OpHandle> {
        let mut ops: Vec<OpHandle> = self.primary.values().copied().collect();
        ops.extend(self.wrap.values().copied());
        self.primary.clear();
        self.wrap.clear();
        self.keys.clear();
        self.last_tick = 0;
        ops
    }
}

// ---- the op container's queue

/// `AsyncOpContainer::QueueOp`'s result.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QueueResult {
    /// The op being pumped, finished or purging: not queued again.
    Refused,
    /// Queued behind others.
    Queued,
    /// Queued as the first entry: the pump's signal is set.
    QueuedSignal,
}

impl QueueResult {
    pub fn code(self) -> i32 {
        match self {
            QueueResult::Refused => 0,
            QueueResult::Queued => 1,
            QueueResult::QueuedSignal => 2,
        }
    }
}

/// `AsyncOpContainer::mOpQueue`.
#[derive(Debug, Default)]
pub struct OpQueue {
    ops: VecDeque<OpHandle>,
}

impl OpQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn len(&self) -> usize {
        self.ops.len()
    }

    pub fn is_empty(&self) -> bool {
        self.ops.is_empty()
    }

    /// `AsyncOpContainer::QueueOp`: `current` is the op being pumped (0: none), `current_pending` its status.
    pub fn queue(&mut self, op: OpHandle, current: OpHandle, current_pending: bool, purging: bool) -> QueueResult {
        if op == current && (!current_pending || purging) {
            return QueueResult::Refused;
        }
        self.ops.push_back(op);
        if self.ops.len() == 1 {
            QueueResult::QueuedSignal
        } else {
            QueueResult::Queued
        }
    }

    /// The front op, taken.
    pub fn pop(&mut self) -> Option<OpHandle> {
        self.ops.pop_front()
    }
}

/// `AsyncOpContainer::Pump`'s accounting: how many ops to run and how long to go on (a value the C++ keeps on its
/// stack across the pump).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PumpPlan {
    pub start: u32,
    pub wait: u32,
    pub remaining: i32,
}

impl PumpPlan {
    /// `Pump(waitTime, maxOps)` at `now` with `queued` ops: a negative `max_ops` means the ops queued now.
    pub fn new(now: u32, wait: u32, max_ops: i32, queued: usize) -> Self {
        PumpPlan { start: now, wait, remaining: if max_ops < 0 { queued as i32 } else { max_ops } }
    }

    /// Whether to wait for the signal before pumping: nothing queued and a wait time.
    pub fn should_wait(&self, queued: usize) -> bool {
        queued == 0 && self.wait > 0
    }

    /// After one op at `now`: whether to go on with the next.
    pub fn go_on(&mut self, now: u32) -> bool {
        if self.remaining > 0 {
            self.remaining -= 1;
            if self.remaining == 0 {
                return false;
            }
        }
        !(self.wait > 0 && now.wrapping_sub(self.start) >= self.wait)
    }
}

// ---- the simple socket thread

/// `SocketThreadSimple::mOpSet`: the ops in address order (the `SmartPtr` comparator's).
#[derive(Debug, Default)]
pub struct OpSet {
    ops: BTreeSet<OpHandle>,
}

impl OpSet {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn len(&self) -> usize {
        self.ops.len()
    }

    pub fn is_empty(&self) -> bool {
        self.ops.is_empty()
    }

    /// `AddSocketOp`: true when the op was not held yet.
    pub fn insert(&mut self, op: OpHandle) -> bool {
        self.ops.insert(op)
    }

    /// `RemoveSocketOp`: true when it was held.
    pub fn remove(&mut self, op: OpHandle) -> bool {
        self.ops.remove(&op)
    }

    /// The ops in their order (a copy: the pump calls back into the C++ for each).
    pub fn snapshot(&self) -> Vec<OpHandle> {
        self.ops.iter().copied().collect()
    }

    /// `PurgeOps`: every op, the set emptied.
    pub fn take_all(&mut self) -> Vec<OpHandle> {
        std::mem::take(&mut self.ops).into_iter().collect()
    }
}

/// `SocketThread::DoOp`: whether an op's `Continue` result finishes it (`WS_TimedOut` means still waiting).
pub fn socket_op_finishes(status: Status) -> bool {
    status != WS_TIMED_OUT
}

/// `SocketThreadSimple::Pump`'s wait: at most 20 ms.
pub fn socket_pump_wait(wait: u32) -> u32 {
    wait.min(20)
}

// ---- the queue socket

/// Which op of a `QueueSocket` finished.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QueueOpKind {
    /// The running queued op (`mQueueOp`).
    Queue,
    /// The repeating op (`mRepeatOp`).
    Repeat,
    /// Another op (`QueueOpFast`'s, run outside the queue).
    Other,
}

/// Whose completion a finished op gets.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QueueCompletion {
    None,
    /// The queued op's own (`mCurQueueCompletion`).
    Current,
    /// The repeat completion.
    Repeat,
    /// A fast op's original completion, when it has one.
    Original,
}

/// What the socket does next.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QueueNext {
    None,
    /// Start the next queued op.
    RunQueue,
    /// Duplicate the repeat op and run it again.
    RerunRepeat,
    /// Forget the repeat op.
    DropRepeat,
}

/// `QueueSocket::OpCallback`'s decisions.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct QueueDecision {
    /// Close the socket (and report to the close completion with the op's status).
    pub close: bool,
    pub completion: QueueCompletion,
    pub next: QueueNext,
    /// The queued op and its completion are forgotten (`mQueueOp`, `mCurQueueCompletion`).
    pub clear_current: bool,
}

/// `QueueSocket::OpCallback`: `datagram` is a UDP socket (type 1), whose failures do not close it.
pub fn queue_socket_callback(
    kind: QueueOpKind,
    status: Status,
    killed: bool,
    close_queued: bool,
    queue_empty: bool,
    socket_valid: bool,
    datagram: bool,
) -> QueueDecision {
    let mut d = QueueDecision {
        close: (status != WS_SUCCESS && !datagram) || !socket_valid,
        completion: QueueCompletion::None,
        next: QueueNext::None,
        clear_current: false,
    };
    match kind {
        QueueOpKind::Queue => {
            d.completion = QueueCompletion::Current;
            d.clear_current = true;
            if !d.close {
                if close_queued && queue_empty {
                    d.close = true;
                } else {
                    d.next = QueueNext::RunQueue;
                }
            }
        }
        QueueOpKind::Repeat => {
            d.completion = QueueCompletion::Repeat;
            if killed {
                d.close = true;
            }
            if !d.close {
                d.next = QueueNext::RerunRepeat;
            } else {
                if close_queued && killed {
                    d.close = false;
                    d.completion = QueueCompletion::None;
                }
                d.next = QueueNext::DropRepeat;
            }
        }
        QueueOpKind::Other => d.completion = QueueCompletion::Original,
    }
    d
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ops::MODE_BLOCK;

    #[test]
    fn op_runs_once_and_finishes_async() {
        let mut s = OpState::new();
        assert_eq!(s.status(), WS_NONE);
        assert_eq!(s.timeout, 0xffff_ffff);
        assert_eq!(s.run(MODE_ASYNC, 500, 1000), RunAction::Queue);
        assert_eq!((s.status(), s.start, s.timeout, s.run_mode), (WS_PENDING, 1000, 500, MODE_ASYNC));
        assert_eq!(s.run(MODE_ASYNC, 1, 2), RunAction::Pending);
        assert_eq!(s.start, 1000, "a refused run changes nothing");
        assert_eq!(s.after_run_hook(), TimerAction::Arm);
        assert_eq!(s.finish(WS_SUCCESS), FinishAction::Queue);
        assert!(s.completion_pending && s.status() == WS_SUCCESS);
        assert_eq!(s.finish(WS_KILLED), FinishAction::Ignored, "finishing twice is nothing");
        assert_eq!(s.run(MODE_ASYNC, 1, 2), RunAction::Awaiting);
        assert!(s.complete());
        assert!(!s.complete());
        assert_eq!(s.run(MODE_BLOCK, 7, 3), RunAction::Block);
        assert_eq!(s.finish(WS_TIMED_OUT), FinishAction::Done);
    }

    #[test]
    fn op_run_async_immediately_timeouts_and_modes() {
        let mut s = OpState::new();
        s.run_async_immediately = true;
        assert_eq!(s.run(MODE_ASYNC, 0, 5), RunAction::StartAsync);
        assert_eq!(s.after_run_hook(), TimerAction::TimeOutNow);
        s.set_timeout(99);
        assert_eq!(s.timeout, 0, "no timeout change while running");
        s.set_mode(MODE_BLOCK);
        assert_eq!(s.run_mode, MODE_ASYNC);
        s.finish(WS_TIMED_OUT);
        s.complete();
        s.set_timeout(99);
        s.set_mode(MODE_BLOCK);
        assert_eq!((s.timeout, s.run_mode), (99, MODE_BLOCK));
        s.run(MODE_ASYNC, 0xffff_ffff, 1);
        assert_eq!(s.after_run_hook(), TimerAction::None, "an infinite timeout arms nothing");
        s.finish(WS_SUCCESS);
        assert_eq!(s.after_run_hook(), TimerAction::None, "finished in the hook");
    }

    #[test]
    fn op_force_finish_kill_and_timer() {
        let mut s = OpState::new();
        assert_eq!(s.force_finish(WS_KILLED), FinishAction::Queue, "an idle op is finished anyway");
        assert_eq!(s.status(), WS_KILLED);
        assert_eq!(s.force_finish(WS_SUCCESS), FinishAction::Ignored, "but not while its completion waits");
        s.complete();
        assert!(s.run_as_timer(60, 10));
        assert_eq!((s.status(), s.timeout, s.start, s.run_mode), (WS_PENDING, 60, 10, MODE_ASYNC));
        assert!(!s.run_as_timer(60, 11), "not twice");
        assert_eq!(s.kill(false), FinishAction::Ignored);
        assert!(s.killed && s.is_pending());
        assert_eq!(s.kill(true), FinishAction::Queue);
        assert_eq!(s.status(), WS_KILLED);
        assert!(!s.run_as_timer(1, 1), "not while the completion waits either");
        let mut t = OpState::new();
        t.run(MODE_ASYNC, 1, 1);
        assert_eq!(t.force_finish(WS_SUCCESS), FinishAction::Queue, "a running op: a plain finish");
    }

    #[test]
    fn timer_queue_orders_and_times_out() {
        let mut q = TimerQueue::new();
        q.set_last_tick(1000);
        assert_eq!(q.wait_time(), 9u32.wrapping_sub(1000));
        let a = q.add(1000, 50, 1);
        let b = q.add(1000, 20, 2);
        let c = q.add(1000, 20, 3);
        assert_eq!(q.len(), 3);
        assert_eq!(q.wait_time(), 20);
        assert_eq!(q.remove(b), Some(2));
        assert_eq!(q.remove(b), None);
        assert!(q.pump(1019).is_empty());
        assert_eq!(q.wait_time(), 1);
        assert_eq!(q.pump(1020), vec![3]);
        assert_eq!(q.pump(1200), vec![1]);
        assert!(q.is_empty());
        assert_eq!(q.remove(a), None);
        assert_eq!(q.remove(c), None);
        // equal deadlines keep their insertion order
        q.add(1, 5, 10);
        q.add(1, 5, 11);
        q.add(1, 5, 12);
        assert_eq!(q.pump(6), vec![10, 11, 12]);
    }

    #[test]
    fn timer_queue_wraps() {
        let mut q = TimerQueue::new();
        let now = 0xffff_fff0;
        q.set_last_tick(now);
        let early = q.add(now, 5, 1); // this era
        let late = q.add(now, 0x20, 2); // 0x10 past the wrap
        let later = q.add(now, 0x30, 3); // 0x20 past
        assert_eq!(q.wait_time(), 5);
        assert_eq!(q.pump(now + 5), vec![1]);
        assert_eq!(q.remove(early), None);
        assert_eq!(q.wait_time(), 9u32.wrapping_sub(now + 5), "the wrap map does not set the wait");
        // the tick wrapped: the old era's ops time out, the wrap map takes over
        let now2 = 0x8;
        assert_eq!(q.pump(now2), Vec::<OpHandle>::new());
        assert_eq!(q.wait_time(), 0x11 - 0x8, "0x20 past 0xffff_fff0 (0xf until the wrap) is 0x11 of the next era");
        assert_eq!(q.remove(later), Some(3), "a key survives the wrap");
        assert!(q.pump(0x10).is_empty());
        assert_eq!(q.pump(0x11), vec![2]);
        assert_eq!(q.remove(late), None);
        // an op of the old era still due when the wrap comes times out with it
        let mut q = TimerQueue::new();
        q.set_last_tick(0xffff_ff00);
        q.add(0xffff_ff00, 0x80, 7);
        q.add(0xffff_ff00, 0x200, 8);
        assert_eq!(q.pump(3), vec![7]);
        assert_eq!(q.purge(), vec![8]);
        assert_eq!(q.last_tick(), 0);
        assert!(q.is_empty());
    }

    #[test]
    fn op_queue_and_pump_plan() {
        let mut q = OpQueue::new();
        assert_eq!(q.queue(5, 0, false, false), QueueResult::QueuedSignal);
        assert_eq!(q.queue(6, 0, false, false), QueueResult::Queued);
        assert_eq!(q.queue(6, 6, false, false), QueueResult::Refused, "the op being pumped, finished");
        assert_eq!(q.queue(6, 6, true, true), QueueResult::Refused, "or while purging");
        assert_eq!(q.queue(6, 6, true, false), QueueResult::Queued, "still running: queued again");
        assert_eq!(q.len(), 3);
        assert_eq!(q.pop(), Some(5));
        assert_eq!(q.pop(), Some(6));
        assert_eq!(q.pop(), Some(6));
        assert_eq!(q.pop(), None);
        let mut p = PumpPlan::new(100, 50, 0, 3);
        assert!(!p.should_wait(3));
        assert!(p.should_wait(0));
        assert!(p.go_on(110));
        assert!(p.go_on(149));
        assert!(!p.go_on(150), "the wait time ran out");
        let mut p = PumpPlan::new(100, 0, 2, 9);
        assert!(!p.should_wait(0), "no wait time: no wait");
        assert!(p.go_on(100_000));
        assert!(!p.go_on(100_001), "two ops ran");
        let mut p = PumpPlan::new(100, 0, -1, 2);
        assert!(p.go_on(1));
        assert!(!p.go_on(1), "the ops queued at the start");
        let mut p = PumpPlan::new(0xffff_fff0, 0x20, 0, 1);
        assert!(p.go_on(0x5), "the elapsed time wraps");
        assert!(!p.go_on(0x10));
    }

    #[test]
    fn op_set_in_address_order() {
        let mut s = OpSet::new();
        assert!(s.insert(30));
        assert!(s.insert(10));
        assert!(!s.insert(30));
        assert_eq!(s.snapshot(), vec![10, 30]);
        assert!(s.remove(30));
        assert!(!s.remove(30));
        s.insert(20);
        assert_eq!(s.take_all(), vec![10, 20]);
        assert!(s.is_empty());
        assert!(socket_op_finishes(WS_SUCCESS));
        assert!(socket_op_finishes(WS_KILLED));
        assert!(!socket_op_finishes(WS_TIMED_OUT));
        assert_eq!(socket_pump_wait(1000), 20);
        assert_eq!(socket_pump_wait(5), 5);
    }

    #[test]
    fn queue_socket_decisions() {
        use QueueCompletion as C;
        use QueueNext as N;
        use QueueOpKind as K;
        let d = queue_socket_callback(K::Queue, WS_SUCCESS, false, false, false, true, false);
        assert_eq!(d, QueueDecision { close: false, completion: C::Current, next: N::RunQueue, clear_current: true });
        let d = queue_socket_callback(K::Queue, WS_SUCCESS, false, true, true, true, false);
        assert!(d.close && d.next == N::None, "a queued close after the last op");
        let d = queue_socket_callback(K::Queue, WS_SUCCESS, false, true, false, true, false);
        assert!(!d.close && d.next == N::RunQueue, "more ops before the close");
        let d = queue_socket_callback(K::Queue, WS_TIMED_OUT, false, false, false, true, false);
        assert!(d.close && d.completion == C::Current && d.next == N::None, "a failure closes a stream socket");
        let d = queue_socket_callback(K::Queue, WS_TIMED_OUT, false, false, false, true, true);
        assert!(!d.close && d.next == N::RunQueue, "but not a datagram socket");
        let d = queue_socket_callback(K::Queue, WS_SUCCESS, false, false, false, false, true);
        assert!(d.close, "an invalid socket closes");
        let d = queue_socket_callback(K::Repeat, WS_SUCCESS, false, false, true, true, false);
        assert_eq!(
            d,
            QueueDecision { close: false, completion: C::Repeat, next: N::RerunRepeat, clear_current: false }
        );
        let d = queue_socket_callback(K::Repeat, WS_KILLED, true, false, true, true, false);
        assert!(d.close && d.completion == C::Repeat && d.next == N::DropRepeat, "a killed repeat op closes");
        let d = queue_socket_callback(K::Repeat, WS_KILLED, true, true, true, true, false);
        assert!(!d.close && d.completion == C::None && d.next == N::DropRepeat, "unless a close is queued");
        let d = queue_socket_callback(K::Repeat, WS_TIMED_OUT, false, true, true, true, true);
        assert!(!d.close && d.next == N::RerunRepeat, "a datagram failure reruns");
        let d = queue_socket_callback(K::Other, WS_SUCCESS, false, false, false, true, false);
        assert_eq!(d, QueueDecision { close: false, completion: C::Original, next: N::None, clear_current: false });
        let d = queue_socket_callback(K::Other, WS_KILLED, true, false, false, true, false);
        assert!(d.close && d.completion == C::Original);
    }
}
