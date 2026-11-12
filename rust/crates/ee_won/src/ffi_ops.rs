//! The C ABI of the async-op machinery (`asyncop`) for the C++ `AsyncOp`, `SocketThreadSimple` and `QueueSocket`
//! (the timer thread and the op container are Rust objects: `thread`). Ops cross as opaque `void*` handles the C++ keeps alive (`AddRef`) while
//! a queue holds them; a queue hands them back through a sink called after the queue's own borrow ended, so a
//! sink may use the queue again. Statuses are `long` (the C++ `WONStatus`); the decisions are small integers
//! (the enums' `code`).

use crate::asyncop::{self, OpHandle, OpSet, QueueCompletion, QueueNext, QueueOpKind};
use crate::status::Status;
use std::ffi::{c_long, c_void};

/// A sink for the ops a queue hands back.
pub type EeWonOpSink = unsafe extern "C" fn(context: *mut c_void, op: *mut c_void);

// (AsyncOp is a Rust object: `opobj`; TimerThread and AsyncOpContainer: `thread`)

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
    use crate::status::WS_KILLED;

    unsafe extern "C" fn collect(context: *mut c_void, op: *mut c_void) {
        unsafe { (*context.cast::<Vec<usize>>()).push(op as usize) };
    }

    #[test]
    fn queues_through_the_abi() {
        unsafe {
            let mut got = Vec::<usize>::new();
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
