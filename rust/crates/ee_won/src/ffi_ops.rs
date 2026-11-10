//! The C ABI of the async-op machinery (`asyncop`) for the C++ `AsyncOp`, `TimerThread`, `AsyncOpContainer`,
//! `SocketThreadSimple` and `QueueSocket`. Ops cross as opaque `void*` handles the C++ keeps alive (`AddRef`) while
//! a queue holds them; a queue hands them back through a sink called after the queue's own borrow ended, so a
//! sink may use the queue again. Statuses are `long` (the C++ `WONStatus`); the decisions are small integers
//! (the enums' `code`).

use crate::asyncop::{
    self, OpHandle, OpQueue, OpSet, OpState, PumpPlan, QueueCompletion, QueueNext, QueueOpKind, TimerQueue,
};
use crate::ops;
use crate::status::Status;
use std::ffi::{c_long, c_void};

/// A sink for the ops a queue hands back.
pub type EeWonOpSink = unsafe extern "C" fn(context: *mut c_void, op: *mut c_void);

// ---- AsyncOp

/// `AsyncOp::AsyncOp`'s state.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_state_init(state: *mut OpState) {
    unsafe { *state = OpState::new() };
}

/// `AsyncOp::Run` now: 0 already running (`WS_Pending`), 1 awaiting completion, 2 start the async run at once,
/// 3 queue it in the container, 4 run blocking.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_run(state: *mut OpState, mode: i32, timeout: u32) -> i32 {
    unsafe { (*state).run(mode, timeout, ops::tick()).code() }
}

/// `AsyncOp::RunAsTimer` now: true when the timer is to be armed.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_run_as_timer(state: *mut OpState, timeout: u32) -> bool {
    unsafe { (*state).run_as_timer(timeout, ops::tick()) }
}

/// `AsyncOp::Finish`: 0 nothing, 1 queue the op in its container, 2 complete it now (blocking).
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_finish(state: *mut OpState, status: c_long) -> i32 {
    unsafe { (*state).finish(status as Status).code() }
}

/// `AsyncOp::ForceFinish`, as `ee_won_op_finish`.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_force_finish(state: *mut OpState, status: c_long) -> i32 {
    unsafe { (*state).force_finish(status as Status).code() }
}

/// `AsyncOp::Kill`, as `ee_won_op_finish`.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_kill(state: *mut OpState, finish: bool) -> i32 {
    unsafe { (*state).kill(finish).code() }
}

/// `AsyncOp::Complete`: true when a completion was pending (deliver it).
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_complete(state: *mut OpState) -> bool {
    unsafe { (*state).complete() }
}

/// `AsyncOp::StartRunAsync` after the run hook: 0 nothing, 1 arm the timeout, 2 time out now.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_after_run_hook(state: *const OpState) -> i32 {
    unsafe { (*state).after_run_hook().code() }
}

/// `AsyncOp::SetTimeout`.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_set_timeout(state: *mut OpState, timeout: u32) {
    unsafe { (*state).set_timeout(timeout) }
}

/// `AsyncOp::SetMode`.
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_set_mode(state: *mut OpState, mode: i32) {
    unsafe { (*state).set_mode(mode) }
}

// ---- TimerThread

#[no_mangle]
pub extern "C" fn ee_won_timers_new() -> *mut TimerQueue {
    Box::into_raw(Box::new(TimerQueue::new()))
}

/// # Safety
/// `timers` came from `ee_won_timers_new` (null: nothing) and is used no more.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_destroy(timers: *mut TimerQueue) {
    if !timers.is_null() {
        drop(unsafe { Box::from_raw(timers) });
    }
}

/// The tick the wait times count from (`ThreadFunc`'s start).
///
/// # Safety
/// `timers` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_set_last_tick(timers: *mut TimerQueue) {
    unsafe { (*timers).set_last_tick(ops::tick()) }
}

/// `TimerThread::AddTimerOp` now -> the entry's key.
///
/// # Safety
/// `timers` is live; `op` stays alive while the entry exists.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_add(timers: *mut TimerQueue, timeout: u32, op: *mut c_void) -> u64 {
    unsafe { (*timers).add(ops::tick(), timeout, op as OpHandle) }
}

/// `TimerThread::RemoveTimerOp`: the entry's op, or null when the key is gone.
///
/// # Safety
/// `timers` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_remove(timers: *mut TimerQueue, key: u64) -> *mut c_void {
    unsafe { (*timers).remove(key) }.map_or(std::ptr::null_mut(), |op| op as *mut c_void)
}

/// `TimerThread::Pump` now: the ops that time out go to the sink, in order.
///
/// # Safety
/// `timers` is live during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_pump(timers: *mut TimerQueue, sink: EeWonOpSink, context: *mut c_void) {
    let due = unsafe { (*timers).pump(ops::tick()) };
    for op in due {
        unsafe { sink(context, op as *mut c_void) };
    }
}

/// `TimerThread::GetWaitTime`.
///
/// # Safety
/// `timers` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_wait_time(timers: *const TimerQueue) -> u32 {
    unsafe { (*timers).wait_time() }
}

/// `TimerThread::PurgeOps`: every op to the sink, the maps emptied.
///
/// # Safety
/// `timers` is live during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timers_purge(timers: *mut TimerQueue, sink: EeWonOpSink, context: *mut c_void) {
    let ops = unsafe { (*timers).purge() };
    for op in ops {
        unsafe { sink(context, op as *mut c_void) };
    }
}

// ---- AsyncOpContainer

#[no_mangle]
pub extern "C" fn ee_won_opqueue_new() -> *mut OpQueue {
    Box::into_raw(Box::new(OpQueue::new()))
}

/// # Safety
/// `queue` came from `ee_won_opqueue_new` (null: nothing) and is used no more.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opqueue_destroy(queue: *mut OpQueue) {
    if !queue.is_null() {
        drop(unsafe { Box::from_raw(queue) });
    }
}

/// `AsyncOpContainer::QueueOp`: 0 refused (the op being pumped, finished or purging), 1 queued, 2 queued first
/// (signal the pump). `current` is the op being pumped (null: none), `current_pending` its status.
///
/// # Safety
/// `queue` is live; a queued `op` stays alive until popped.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opqueue_queue(
    queue: *mut OpQueue,
    op: *mut c_void,
    current: *mut c_void,
    current_pending: bool,
    purging: bool,
) -> i32 {
    unsafe { (*queue).queue(op as OpHandle, current as OpHandle, current_pending, purging).code() }
}

/// The front op, taken (null: none).
///
/// # Safety
/// `queue` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opqueue_pop(queue: *mut OpQueue) -> *mut c_void {
    unsafe { (*queue).pop() }.map_or(std::ptr::null_mut(), |op| op as *mut c_void)
}

/// # Safety
/// `queue` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opqueue_len(queue: *const OpQueue) -> usize {
    unsafe { (*queue).len() }
}

/// `AsyncOpContainer::Pump`'s accounting, started now.
#[no_mangle]
pub extern "C" fn ee_won_pump_plan_new(wait: u32, max_ops: i32, queued: usize) -> PumpPlan {
    PumpPlan::new(ops::tick(), wait, max_ops, queued)
}

/// Whether to wait for the signal before pumping.
///
/// # Safety
/// `plan` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pump_plan_should_wait(plan: *const PumpPlan, queued: usize) -> bool {
    unsafe { (*plan).should_wait(queued) }
}

/// After one op: whether to go on with the next.
///
/// # Safety
/// `plan` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pump_plan_go_on(plan: *mut PumpPlan) -> bool {
    unsafe { (*plan).go_on(ops::tick()) }
}

// ---- SocketThreadSimple

#[no_mangle]
pub extern "C" fn ee_won_opset_new() -> *mut OpSet {
    Box::into_raw(Box::new(OpSet::new()))
}

/// # Safety
/// `set` came from `ee_won_opset_new` (null: nothing) and is used no more.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opset_destroy(set: *mut OpSet) {
    if !set.is_null() {
        drop(unsafe { Box::from_raw(set) });
    }
}

/// `AddSocketOp`: true when the op was not held yet (the C++ references it then).
///
/// # Safety
/// `set` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opset_insert(set: *mut OpSet, op: *mut c_void) -> bool {
    unsafe { (*set).insert(op as OpHandle) }
}

/// `RemoveSocketOp`: true when the op was held (the C++ releases it then).
///
/// # Safety
/// `set` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opset_remove(set: *mut OpSet, op: *mut c_void) -> bool {
    unsafe { (*set).remove(op as OpHandle) }
}

/// The held ops, in order, to the sink (a copy: the sink may remove from the set).
///
/// # Safety
/// `set` is live during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opset_snapshot(set: *const OpSet, sink: EeWonOpSink, context: *mut c_void) {
    let ops = unsafe { (*set).snapshot() };
    for op in ops {
        unsafe { sink(context, op as *mut c_void) };
    }
}

/// `PurgeOps`: every op to the sink, the set emptied.
///
/// # Safety
/// `set` is live during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_opset_take_all(set: *mut OpSet, sink: EeWonOpSink, context: *mut c_void) {
    let ops = unsafe { (*set).take_all() };
    for op in ops {
        unsafe { sink(context, op as *mut c_void) };
    }
}

/// `SocketThread::DoOp`: whether an op's `Continue` result finishes it.
#[no_mangle]
pub extern "C" fn ee_won_socket_op_finishes(status: c_long) -> bool {
    asyncop::socket_op_finishes(status as Status)
}

/// `SocketThreadSimple::Pump`'s wait.
#[no_mangle]
pub extern "C" fn ee_won_socket_pump_wait(wait: u32) -> u32 {
    asyncop::socket_pump_wait(wait)
}

// ---- QueueSocket

/// `QueueSocket::OpCallback`'s decisions: `completion` 0 none, 1 the queued op's own, 2 the repeat completion,
/// 3 a fast op's original; `next` 0 nothing, 1 run the next queued op, 2 duplicate and rerun the repeat op,
/// 3 forget the repeat op.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct QueueDecision {
    pub close: bool,
    pub completion: i32,
    pub next: i32,
    pub clear_current: bool,
}

/// `QueueSocket::OpCallback` for an op of `kind` (0 the queued op, 1 the repeat op, 2 another).
#[no_mangle]
pub extern "C" fn ee_won_queue_socket_callback(
    kind: i32,
    status: c_long,
    killed: bool,
    close_queued: bool,
    queue_empty: bool,
    socket_valid: bool,
    datagram: bool,
) -> QueueDecision {
    let kind = match kind {
        0 => QueueOpKind::Queue,
        1 => QueueOpKind::Repeat,
        _ => QueueOpKind::Other,
    };
    let d = asyncop::queue_socket_callback(
        kind,
        status as Status,
        killed,
        close_queued,
        queue_empty,
        socket_valid,
        datagram,
    );
    QueueDecision {
        close: d.close,
        completion: match d.completion {
            QueueCompletion::None => 0,
            QueueCompletion::Current => 1,
            QueueCompletion::Repeat => 2,
            QueueCompletion::Original => 3,
        },
        next: match d.next {
            QueueNext::None => 0,
            QueueNext::RunQueue => 1,
            QueueNext::RerunRepeat => 2,
            QueueNext::DropRepeat => 3,
        },
        clear_current: d.clear_current,
    }
}

#[cfg(test)]
#[allow(clippy::manual_dangling_ptr)] // small integers stand in for the ops' addresses
mod tests {
    use super::*;
    use crate::status::{WS_KILLED, WS_PENDING, WS_SUCCESS};

    unsafe extern "C" fn collect(context: *mut c_void, op: *mut c_void) {
        unsafe { (*context.cast::<Vec<usize>>()).push(op as usize) };
    }

    #[test]
    fn op_state_through_the_abi() {
        let mut s = OpState::new();
        unsafe {
            ee_won_op_state_init(&mut s);
            assert_eq!(ee_won_op_run(&mut s, 1, 10), 3);
            assert_eq!(s.status as Status, WS_PENDING);
            assert_eq!(ee_won_op_after_run_hook(&s), 1);
            assert_eq!(ee_won_op_finish(&mut s, WS_SUCCESS as c_long), 1);
            assert!(ee_won_op_complete(&mut s));
            assert!(ee_won_op_run_as_timer(&mut s, 5));
            assert_eq!(ee_won_op_kill(&mut s, true), 1);
            assert_eq!(s.status as Status, WS_KILLED);
            assert_eq!(ee_won_op_force_finish(&mut s, 0), 0);
            ee_won_op_set_timeout(&mut s, 3);
            ee_won_op_set_mode(&mut s, 2);
            assert_eq!(ee_won_op_run(&mut s, 2, 1), 1);
        }
    }

    #[test]
    fn queues_through_the_abi() {
        unsafe {
            let t = ee_won_timers_new();
            ee_won_timers_set_last_tick(t);
            let k = ee_won_timers_add(t, 0, 7 as *mut c_void);
            let mut got = Vec::<usize>::new();
            ee_won_timers_pump(t, collect, (&mut got as *mut Vec<usize>).cast());
            assert_eq!(got, vec![7]);
            assert!(ee_won_timers_remove(t, k).is_null());
            let k2 = ee_won_timers_add(t, 100_000, 8 as *mut c_void);
            assert!(ee_won_timers_wait_time(t) > 0);
            assert_eq!(ee_won_timers_remove(t, k2), 8 as *mut c_void);
            ee_won_timers_add(t, 100_000, 9 as *mut c_void);
            got.clear();
            ee_won_timers_purge(t, collect, (&mut got as *mut Vec<usize>).cast());
            assert_eq!(got, vec![9]);
            ee_won_timers_destroy(t);

            let q = ee_won_opqueue_new();
            assert_eq!(ee_won_opqueue_queue(q, 1 as *mut c_void, std::ptr::null_mut(), false, false), 2);
            assert_eq!(ee_won_opqueue_queue(q, 2 as *mut c_void, std::ptr::null_mut(), false, false), 1);
            assert_eq!(ee_won_opqueue_len(q), 2);
            assert_eq!(ee_won_opqueue_pop(q), 1 as *mut c_void);
            assert_eq!(ee_won_opqueue_pop(q), 2 as *mut c_void);
            assert!(ee_won_opqueue_pop(q).is_null());
            ee_won_opqueue_destroy(q);
            let mut plan = ee_won_pump_plan_new(0, 1, 0);
            assert!(!ee_won_pump_plan_should_wait(&plan, 0));
            assert!(!ee_won_pump_plan_go_on(&mut plan));

            let s = ee_won_opset_new();
            assert!(ee_won_opset_insert(s, 5 as *mut c_void));
            assert!(ee_won_opset_insert(s, 3 as *mut c_void));
            got.clear();
            ee_won_opset_snapshot(s, collect, (&mut got as *mut Vec<usize>).cast());
            assert_eq!(got, vec![3, 5]);
            assert!(ee_won_opset_remove(s, 3 as *mut c_void));
            got.clear();
            ee_won_opset_take_all(s, collect, (&mut got as *mut Vec<usize>).cast());
            assert_eq!(got, vec![5]);
            ee_won_opset_destroy(s);
        }
        let d = ee_won_queue_socket_callback(1, WS_KILLED as c_long, true, false, true, true, false);
        assert_eq!(d, QueueDecision { close: true, completion: 2, next: 3, clear_current: false });
    }
}
