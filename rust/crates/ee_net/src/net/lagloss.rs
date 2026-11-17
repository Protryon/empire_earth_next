//! `NELagLossSimulator` whole (the original NELagLossSimulator.cpp, LLE 0x1003d84c..0x1003ddda, with its inline
//! members and the `NETimestampedMessage` it queued): simulated packet loss on the client's sends and receipts, and
//! simulated lag on its receipts. A lagged message waits in a list sorted by its release time (a later one after
//! every equal one) until the lag thread ("CommLagLoss") hands it to the receive callback.
//!
//! The original's critical section is a re-entrant lock, held where it held it: around the list's insertion, and in
//! the lag thread across each delivery (the callback runs under it) and the release time's reading. The settings the
//! original read and wrote without a lock (the switches, percentages and lag range, the events, the thread) are
//! atomics; the random series (reseeded with the clock at construction, drawn by `process` and `send`, which run on
//! different threads) is behind its own mutex, never held across a call out.
//!
//! The rolls are the original's f32 expressions: `Generate() * 99 + 1 < percentage`, and a lag of
//! `Generate(min, max)` truncated toward zero (a C cast) added to the clock with 32-bit wrapping.
//! Kept as the original did: messages still lagged when the lag is turned off without a flush, or when the
//! simulator is freed, are never released.

use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicUsize, Ordering::SeqCst};
use std::sync::Mutex;

use ee_base::random::URandom;

use crate::net::qhost::*;
use crate::net::rlock::RLock;

const LOSS_SCALE: f32 = 99.0;
const UNIT: u32 = 0;

pub struct LagLoss<H: QHost> {
    pub host: H,
    lock: RLock,
    /// (release time, message), sorted by time
    list: UnsafeCell<Vec<(u32, Msg)>>,
    random: Mutex<URandom>,
    thread: AtomicUsize,
    queue_changed: AtomicUsize,
    thread_exit: AtomicUsize,
    thread_started: AtomicUsize,
    thread_finished: AtomicUsize,
    receive_loss: AtomicU32,
    send_loss: AtomicU32,
    lag_min: AtomicU32,
    lag_max: AtomicU32,
    lag_enabled: AtomicBool,
    receive_loss_enabled: AtomicBool,
    send_loss_enabled: AtomicBool,
    flush_on_disable: AtomicBool,
}

// The list is reached only under `lock`, never across a call out except where the original held its lock.
unsafe impl<H: QHost + Sync> Sync for LagLoss<H> {}

impl<H: QHost> LagLoss<H> {
    /// The constructor: lag and loss off, the series seeded with the clock (as stored: a 0 stays 0).
    pub fn new(host: H) -> Self {
        let seed = host.time();
        LagLoss {
            host,
            lock: RLock::new(),
            list: UnsafeCell::new(Vec::new()),
            random: Mutex::new(URandom { seed }),
            thread: AtomicUsize::new(0),
            queue_changed: AtomicUsize::new(0),
            thread_exit: AtomicUsize::new(0),
            thread_started: AtomicUsize::new(0),
            thread_finished: AtomicUsize::new(0),
            receive_loss: AtomicU32::new(0f32.to_bits()),
            send_loss: AtomicU32::new(0f32.to_bits()),
            lag_min: AtomicU32::new(0),
            lag_max: AtomicU32::new(0),
            lag_enabled: AtomicBool::new(false),
            receive_loss_enabled: AtomicBool::new(false),
            send_loss_enabled: AtomicBool::new(false),
            flush_on_disable: AtomicBool::new(false),
        }
    }

    /// The destructor's work: the lag thread stopped without a flush.
    pub fn release(&self) {
        if self.lag_enabled.load(SeqCst) {
            self.disable_lag(false);
        }
    }

    fn with_list<R>(&self, f: impl FnOnce(&mut Vec<(u32, Msg)>) -> R) -> R {
        // SAFETY: callers hold `lock`; `f` makes no call out
        f(unsafe { &mut *self.list.get() })
    }

    fn roll(&self) -> f32 {
        let r = self.random.lock().unwrap_or_else(|e| e.into_inner()).generate();
        r * LOSS_SCALE + 1.0
    }

    fn verify(&self, ok: bool, line: u32) {
        if !ok {
            self.host.verify(UNIT, line);
        }
    }

    fn new_event(&self, slot: &AtomicUsize, line: u32) {
        let e = self.host.event_new();
        slot.store(e, SeqCst);
        self.verify(e != 0, line);
    }

    pub fn disable_receive_loss(&self) {
        self.receive_loss_enabled.store(false, SeqCst);
    }
    pub fn disable_send_loss(&self) {
        self.send_loss_enabled.store(false, SeqCst);
    }
    pub fn enable_receive_loss(&self, percentage: f32) {
        self.receive_loss.store(percentage.to_bits(), SeqCst);
        self.receive_loss_enabled.store(true, SeqCst);
    }
    pub fn enable_send_loss(&self, percentage: f32) {
        self.send_loss.store(percentage.to_bits(), SeqCst);
        self.send_loss_enabled.store(true, SeqCst);
    }
    pub fn lag_range(&self) -> (u32, u32) {
        (self.lag_min.load(SeqCst), self.lag_max.load(SeqCst))
    }
    pub fn receive_loss_percentage(&self) -> f32 {
        f32::from_bits(self.receive_loss.load(SeqCst))
    }
    pub fn send_loss_percentage(&self) -> f32 {
        f32::from_bits(self.send_loss.load(SeqCst))
    }
    pub fn set_lag_range(&self, minimum: u32, maximum: u32) {
        self.lag_min.store(minimum, SeqCst);
        self.lag_max.store(maximum, SeqCst);
    }
    pub fn set_receive_loss_percentage(&self, percentage: f32) {
        self.receive_loss.store(percentage.to_bits(), SeqCst);
    }
    pub fn set_send_loss_percentage(&self, percentage: f32) {
        self.send_loss.store(percentage.to_bits(), SeqCst);
    }

    /// `EnableLag`: the range set, four events made, the lag thread started (waited for) and named. `me` is this
    /// object as the thread body's argument.
    pub fn enable_lag(&self, me: usize, minimum: u32, maximum: u32) {
        self.set_lag_range(minimum, maximum);
        self.lag_enabled.store(true, SeqCst);
        self.new_event(&self.queue_changed, 96);
        self.new_event(&self.thread_exit, 97);
        self.new_event(&self.thread_started, 98);
        self.new_event(&self.thread_finished, 99);
        let mut t = 0;
        self.host.thread_new(THREAD_LAG, me, &mut t);
        self.thread.store(t, SeqCst);
        self.host.event_wait(self.thread_started.load(SeqCst), WAIT_INFINITE);
        self.host.thread_name(self.thread.load(SeqCst), "CommLagLoss");
    }

    /// `DisableLag`: the lag thread told to exit (delivering what is lagged when `flush`) and waited for; its events
    /// and the thread freed.
    pub fn disable_lag(&self, flush: bool) {
        if !self.lag_enabled.load(SeqCst) {
            return;
        }
        self.flush_on_disable.store(flush, SeqCst);
        self.host.event_set(self.thread_exit.load(SeqCst));
        self.host.event_set(self.queue_changed.load(SeqCst));
        self.host.event_wait(self.thread_finished.load(SeqCst), WAIT_INFINITE);
        self.host.event_free(self.queue_changed.load(SeqCst));
        self.host.event_free(self.thread_exit.load(SeqCst));
        self.host.event_free(self.thread_finished.load(SeqCst));
        self.host.event_free(self.thread_started.load(SeqCst));
        self.queue_changed.store(0, SeqCst);
        self.thread_exit.store(0, SeqCst);
        self.thread_finished.store(0, SeqCst);
        self.thread_started.store(0, SeqCst);
        self.lag_enabled.store(false, SeqCst);
        self.host.thread_free(self.thread.load(SeqCst));
        self.thread.store(0, SeqCst);
    }

    /// One lagged message out of the list, referenced for the callback, handed to it and given back (the
    /// timestamped message's destructor), all under the lock.
    fn deliver_front(&self) -> bool {
        let front = self.with_list(|l| if l.is_empty() { None } else { Some(l.remove(0)) });
        let Some((_, m)) = front else { return false };
        // `process` never queues a NULL message (the original's destructor would have dereferenced it)
        if m != 0 {
            self.host.msg_reference(m);
            self.host.deliver(DELIVER_LAG_RECEIVE, m);
            self.host.msg_dereference(m);
        }
        true
    }

    /// The lag thread's body (`LagThreadFunction`): waits for the list's change or its first message's release
    /// time, delivers that message, and on exit delivers the rest when told to flush.
    pub fn lag_thread(&self) -> u32 {
        let mut timeout = WAIT_INFINITE;
        let mut exiting = false;
        let mut running = true;
        self.host.event_set(self.thread_started.load(SeqCst));
        self.host.fpu_start();
        while running {
            self.host.fpu_check();
            let result = self.host.event_wait(self.queue_changed.load(SeqCst), timeout);
            if result == ERROR_OK {
                self.host.event_clear(self.queue_changed.load(SeqCst));
                if self.host.event_wait(self.thread_exit.load(SeqCst), 0) == ERROR_OK {
                    self.host.event_clear(self.thread_exit.load(SeqCst));
                    exiting = true;
                    running = false;
                }
            } else if result == ERROR_TIMEOUT {
                let _g = self.lock.lock();
                self.deliver_front();
            } else {
                running = false;
            }
            let _g = self.lock.lock();
            let due = self.with_list(|l| l.first().map(|&(t, _)| t));
            timeout = match due {
                // a release time already past: no wait
                Some(due) => due.saturating_sub(self.host.time()),
                None => WAIT_INFINITE,
            };
        }
        if exiting && self.flush_on_disable.load(SeqCst) {
            let _g = self.lock.lock();
            while self.deliver_front() {}
        }
        self.host.event_set(self.thread_finished.load(SeqCst));
        0
    }

    /// `Process`: a received message dropped by the receive loss roll (given back), queued with a random lag, or
    /// handed to the receive callback.
    pub fn process(&self, m: Msg) -> i32 {
        if m == 0 {
            return ERROR_OK;
        }
        if self.receive_loss_enabled.load(SeqCst) && self.roll() < self.receive_loss_percentage() {
            self.host.msg_dereference(m);
        } else if self.lag_enabled.load(SeqCst) {
            let (minimum, maximum) = self.lag_range();
            let time = if minimum == maximum {
                self.host.time().wrapping_add(maximum)
            } else {
                let lag = self
                    .random
                    .lock()
                    .unwrap_or_else(|e| e.into_inner())
                    .generate_range(minimum as f32, maximum as f32);
                // the original's `(long)theLag`: a truncating cast
                self.host.time().wrapping_add(lag as i32 as u32)
            };
            {
                let _g = self.lock.lock();
                self.with_list(|l| {
                    let at = l.iter().position(|&(t, _)| time < t).unwrap_or(l.len());
                    l.insert(at, (time, m));
                });
            }
            self.host.event_set(self.queue_changed.load(SeqCst));
        } else {
            self.host.deliver(DELIVER_LAG_RECEIVE, m);
        }
        ERROR_OK
    }

    /// `Send`: a message dropped by the send loss roll (given back) or handed to the send callback, whose result
    /// this returns.
    pub fn send(&self, m: Msg) -> i32 {
        let mut result = ERROR_OK;
        if m != 0 {
            if self.send_loss_enabled.load(SeqCst) && self.roll() < self.send_loss_percentage() {
                self.host.msg_dereference(m);
            } else {
                result = self.host.deliver(DELIVER_LAG_SEND, m);
            }
        }
        result
    }

    /// The messages still lagged (the tests).
    pub fn lagged(&self) -> Vec<(u32, Msg)> {
        let _g = self.lock.lock();
        self.with_list(|l| l.clone())
    }
}
