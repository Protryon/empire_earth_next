//! A re-entrant lock: the thread that holds it may take it again (a Win32 critical section, which `TSCriticalSection`
//! is). The engine's objects call out while they hold their locks, and a call out may come back into the same
//! object on the same thread (the server's transfer pass: a transfer's status reaches the game, which may queue
//! another transfer).

use std::sync::{Condvar, Mutex};
use std::thread::{self, ThreadId};

#[derive(Default)]
pub struct RLock {
    state: Mutex<(Option<ThreadId>, u32)>,
    free: Condvar,
}

pub struct RGuard<'a> {
    lock: &'a RLock,
}

impl RLock {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn lock(&self) -> RGuard<'_> {
        let me = thread::current().id();
        let mut s = self.state.lock().unwrap_or_else(|e| e.into_inner());
        loop {
            match s.0 {
                None => {
                    *s = (Some(me), 1);
                    break;
                }
                Some(owner) if owner == me => {
                    s.1 += 1;
                    break;
                }
                Some(_) => s = self.free.wait(s).unwrap_or_else(|e| e.into_inner()),
            }
        }
        RGuard { lock: self }
    }
}

impl Drop for RGuard<'_> {
    fn drop(&mut self) {
        let mut s = self.lock.state.lock().unwrap_or_else(|e| e.into_inner());
        s.1 -= 1;
        if s.1 == 0 {
            s.0 = None;
            self.lock.free.notify_one();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::RLock;
    use std::sync::Arc;

    #[test]
    fn the_holder_takes_it_again_and_others_wait() {
        let lock = Arc::new(RLock::new());
        let count = Arc::new(std::sync::atomic::AtomicU32::new(0));
        let threads: Vec<_> = (0..4)
            .map(|_| {
                let (lock, count) = (lock.clone(), count.clone());
                std::thread::spawn(move || {
                    for _ in 0..1000 {
                        let _a = lock.lock();
                        let _b = lock.lock();
                        // a plain load and store: only one thread is ever inside
                        let v = count.load(std::sync::atomic::Ordering::Relaxed);
                        count.store(v + 1, std::sync::atomic::Ordering::Relaxed);
                    }
                })
            })
            .collect();
        for t in threads {
            t.join().unwrap();
        }
        assert_eq!(count.load(std::sync::atomic::Ordering::Relaxed), 4000);
    }
}
