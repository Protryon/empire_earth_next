//! NetStats (`include/ee/wonapi/WONRouting.h`): WON's socket and traffic counters, counted only while enabled (off
//! by default; nothing in the game turns them on). The C++ class's static functions call these.

use std::sync::atomic::{AtomicBool, AtomicI32, AtomicI64, Ordering::Relaxed};

static ENABLED: AtomicBool = AtomicBool::new(false);
static CONSTRUCTED_SOCKETS: AtomicI32 = AtomicI32::new(0);
static OPEN_SOCKETS: AtomicI32 = AtomicI32::new(0);
static BYTES_SENT: AtomicI64 = AtomicI64::new(0);
static BYTES_RECEIVED: AtomicI64 = AtomicI64::new(0);
static MESSAGES_SENT: AtomicI64 = AtomicI64::new(0);
static MESSAGES_RECEIVED: AtomicI64 = AtomicI64::new(0);

/// Which counter.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Counter {
    ConstructedSockets = 0,
    OpenSockets = 1,
    BytesSent = 2,
    BytesReceived = 3,
    MessagesSent = 4,
    MessagesReceived = 5,
}

impl Counter {
    pub fn from_u32(v: u32) -> Option<Counter> {
        use Counter::*;
        [ConstructedSockets, OpenSockets, BytesSent, BytesReceived, MessagesSent, MessagesReceived]
            .get(v as usize)
            .copied()
    }
}

pub fn enable(on: bool) {
    ENABLED.store(on, Relaxed);
}

pub fn enabled() -> bool {
    ENABLED.load(Relaxed)
}

/// Adds `amount` to a counter when counting is on.
pub fn add(counter: Counter, amount: i64) {
    if !enabled() {
        return;
    }
    match counter {
        Counter::ConstructedSockets => drop(CONSTRUCTED_SOCKETS.fetch_add(amount as i32, Relaxed)),
        Counter::OpenSockets => drop(OPEN_SOCKETS.fetch_add(amount as i32, Relaxed)),
        Counter::BytesSent => drop(BYTES_SENT.fetch_add(amount, Relaxed)),
        Counter::BytesReceived => drop(BYTES_RECEIVED.fetch_add(amount, Relaxed)),
        Counter::MessagesSent => drop(MESSAGES_SENT.fetch_add(amount, Relaxed)),
        Counter::MessagesReceived => drop(MESSAGES_RECEIVED.fetch_add(amount, Relaxed)),
    }
}

pub fn get(counter: Counter) -> i64 {
    match counter {
        Counter::ConstructedSockets => CONSTRUCTED_SOCKETS.load(Relaxed) as i64,
        Counter::OpenSockets => OPEN_SOCKETS.load(Relaxed) as i64,
        Counter::BytesSent => BYTES_SENT.load(Relaxed),
        Counter::BytesReceived => BYTES_RECEIVED.load(Relaxed),
        Counter::MessagesSent => MESSAGES_SENT.load(Relaxed),
        Counter::MessagesReceived => MESSAGES_RECEIVED.load(Relaxed),
    }
}

pub fn add_constructed_sockets(n: i32) {
    add(Counter::ConstructedSockets, n as i64)
}
pub fn add_open_sockets(n: i32) {
    add(Counter::OpenSockets, n as i64)
}
pub fn add_bytes_sent(n: i64) {
    add(Counter::BytesSent, n)
}
pub fn add_bytes_received(n: i64) {
    add(Counter::BytesReceived, n)
}

#[cfg(test)]
mod tests {
    use super::*;

    // the counters are process-wide: one test changes them
    #[test]
    fn counts_only_while_enabled() {
        let before = get(Counter::MessagesSent);
        add(Counter::MessagesSent, 5);
        assert_eq!(get(Counter::MessagesSent), before);
        enable(true);
        add(Counter::MessagesSent, 5);
        add(Counter::MessagesReceived, 2);
        add(Counter::MessagesReceived, -1);
        enable(false);
        assert_eq!(get(Counter::MessagesSent), before + 5);
        assert!(get(Counter::MessagesReceived) >= 1);
        assert_eq!(Counter::from_u32(3), Some(Counter::BytesReceived));
        assert_eq!(Counter::from_u32(6), None);
    }
}
