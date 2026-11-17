//! TEST ONLY: which read/write locks the calling thread holds (the screens' SGVW trace, `ee_game screens/sgvw.rs`,
//! notes the world lock's state at each read it records). When it is on, the four lock entry points
//! (`ee_ts_lock_read`, `_read_unlock`, `_write`, `_write_unlock`) count, per thread and per lock, the read and write
//! holds taken and not yet released. It is on from the process's first lock call when `EE_SGVW` is set (the trace's
//! file), so every hold is counted from its take.

use std::cell::RefCell;
use std::sync::atomic::{AtomicU8, Ordering::Relaxed};

/// 0 not yet asked, 1 off, 2 on
static STATE: AtomicU8 = AtomicU8::new(0);

thread_local! {
    /// (the lock's address, its read holds, its write holds) of this thread, in first-taken order
    static HELD: RefCell<Vec<(usize, i32, i32)>> = const { RefCell::new(Vec::new()) };
}

/// Turns the counting on (it stays on).
pub fn track() {
    STATE.store(2, Relaxed);
}

/// The counting is on (asked of `EE_SGVW` the first time).
pub fn tracking() -> bool {
    match STATE.load(Relaxed) {
        1 => false,
        2 => true,
        _ => {
            let on = std::env::var_os("EE_SGVW").is_some();
            STATE.store(if on { 2 } else { 1 }, Relaxed);
            on
        }
    }
}

/// A change of this thread's holds of `lock`: `read` and `write` are +1 for a take, -1 for a release.
pub(crate) fn note(lock: usize, read: i32, write: i32) {
    if !tracking() {
        return;
    }
    HELD.with(|h| {
        let mut h = h.borrow_mut();
        match h.iter_mut().find(|e| e.0 == lock) {
            Some(e) => {
                e.1 += read;
                e.2 += write;
            }
            None => h.push((lock, read, write)),
        }
    });
}

/// This thread's (read, write) holds of `lock` (0, 0 when it holds none or the counting is off).
pub fn held(lock: usize) -> (i32, i32) {
    HELD.with(|h| h.borrow().iter().find(|e| e.0 == lock).map_or((0, 0), |e| (e.1, e.2)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_per_thread_and_lock() {
        // (off: nothing counted; the state is process-wide, so the off case runs only before another test's track)
        if !tracking() {
            note(0x10, 1, 0);
            assert_eq!(held(0x10), (0, 0));
        }
        track();
        note(0x20, 1, 0);
        note(0x20, 1, 0);
        note(0x30, 0, 1);
        assert_eq!(held(0x20), (2, 0));
        assert_eq!(held(0x30), (0, 1));
        assert_eq!(held(0x40), (0, 0));
        // another thread sees none of this thread's holds
        std::thread::spawn(|| assert_eq!(held(0x20), (0, 0))).join().unwrap();
        note(0x20, -2, 0);
        note(0x30, 0, -1);
        assert_eq!(held(0x20), (0, 0));
        assert_eq!(held(0x30), (0, 0));
    }
}
