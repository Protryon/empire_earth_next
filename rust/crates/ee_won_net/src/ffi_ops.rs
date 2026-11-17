//! The C ABI of the async-op machinery (`asyncop`) for the C++ `QueueSocket` (AsyncOp, the socket thread, the timer
//! thread and the op container are Rust objects: `opobj`, `sockthread`, `thread`). Ops cross as opaque `void*` handles the C++ keeps alive (`AddRef`) while
//! a queue holds them; a queue hands them back through a sink called after the queue's own borrow ended, so a
//! sink may use the queue again. Statuses are `long` (the C++ `WONStatus`); the decisions are small integers
//! (the enums' `code`).

use crate::asyncop::{self, QueueCompletion, QueueNext, QueueOpKind};
use crate::status::Status;
use std::ffi::c_long;

// (AsyncOp is a Rust object: `opobj`; SocketThreadSimple: `sockthread`; TimerThread and AsyncOpContainer: `thread`)

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
mod tests {
    use super::*;
    use crate::status::WS_KILLED;

    #[test]
    fn queue_socket_through_the_abi() {
        let d = ee_won_queue_socket_callback(1, WS_KILLED as c_long, true, false, true, true, false);
        assert_eq!(d, QueueDecision { close: true, completion: 2, next: 3, clear_current: false });
    }
}
