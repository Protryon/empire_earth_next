//! The Low-Level Engine's Time Manager whole in Rust (`src/Low-Level Engine/Time Manager` before; LLE lane round 3):
//! the polled countdown `TMPoll`, the system-timer wrapper `TMTimer`, and the clocks `TMGetTime`, `TMGetMicroTime`,
//! `TMGetTimeOfDay` with their initialisation. C++ keeps the class names callers use: `TMPoll` is a class holding
//! one [`EeTimePoll`] record by value (its size and alignment are the original's 0x20 / 8, so the classes that embed
//! a poll keep their layout) with inline forwarders; `TMTimer` owns an [`EeTimeTimer`]; `TMInitialize` registers
//! [`ee_time_shutdown`] with the engine's shutdown list.
//!
//! Kept bit for bit (the golden): `TMPoll::sCounterFrequency` is ONE shared value that every new poll overwrites
//! (ticks per millisecond, or per microsecond for a microsecond poll, or 1 without a counter), so a microsecond poll
//! would rescale every millisecond poll made before it. Every poll the game makes (20 constructions) is a millisecond
//! poll on the same counter, so the shared value never changes after the first poll: kept as it was rather than
//! fixed, which no caller would see. The arithmetic is the Windows build's: `unsigned long` is 32 bits, the times
//! are `int64_t`, a delay compares with the elapsed time signed.
//!
//! `golden_time.txt` is the trace of the original units (`tests/time/tm_golden.cpp`); `golden.rs` replays it.

#[cfg(test)]
mod golden;

use std::cell::Cell;
use std::ffi::{c_int, c_ulong, c_void};
use std::sync::atomic::{AtomicBool, AtomicI64, AtomicPtr, AtomicU32, AtomicUsize, Ordering::Relaxed};

use ee_plat_core::time as plat;

/// The platform clocks the Time Manager reads (`platform/PlatTime.h`).
pub trait Clock {
    /// `PlatCounterFrequency`: ticks per second, 0 without a counter.
    fn counter_frequency(&self) -> i64;
    /// `PlatCounterTicks`.
    fn counter_ticks(&self) -> i64;
    /// `PlatTimeMilliseconds`.
    fn milliseconds(&self) -> u32;
}

/// The platform's clocks.
pub struct PlatClock;

impl Clock for PlatClock {
    fn counter_frequency(&self) -> i64 {
        plat::PlatCounterFrequency()
    }
    fn counter_ticks(&self) -> i64 {
        plat::PlatCounterTicks()
    }
    fn milliseconds(&self) -> u32 {
        plat::PlatTimeMilliseconds()
    }
}

/// The Time Manager's shared values: `TMPoll::sCounterFrequency` and `TMInitialize`'s time base (the original's
/// `gCounterFrequency`, `gMicrosecondFrequency`, `gHasPerformanceCounter`).
pub struct TimeState {
    poll_frequency: AtomicI64,
    has_counter: AtomicBool,
    per_millisecond: AtomicI64,
    per_microsecond: AtomicU32,
}

impl TimeState {
    pub const fn new() -> Self {
        TimeState {
            poll_frequency: AtomicI64::new(0),
            has_counter: AtomicBool::new(false),
            per_millisecond: AtomicI64::new(0),
            per_microsecond: AtomicU32::new(0),
        }
    }
    /// `TMPoll::sCounterFrequency`.
    pub fn poll_frequency(&self) -> i64 {
        self.poll_frequency.load(Relaxed)
    }
    /// (counter ticks per millisecond, per microsecond as an f32, has a counter)
    pub fn base(&self) -> (i64, f32, bool) {
        (
            self.per_millisecond.load(Relaxed),
            f32::from_bits(self.per_microsecond.load(Relaxed)),
            self.has_counter.load(Relaxed),
        )
    }

    /// `TMInitialize` without the timer resolution and the shutdown registration: the counter's frequencies.
    pub fn initialize(&self, clock: &impl Clock) {
        let frequency = clock.counter_frequency();
        let has = frequency != 0;
        self.has_counter.store(has, Relaxed);
        if has {
            self.per_millisecond.store(frequency / 1000, Relaxed);
            self.per_microsecond.store((frequency as f32 * 0.000001f32).to_bits(), Relaxed);
        }
    }

    /// `TMGetTime`: milliseconds, `pinned` instead when it is not negative (EE-SIM's scripted editor).
    pub fn time(&self, clock: &impl Clock, pinned: i64) -> u32 {
        if pinned >= 0 {
            return pinned as u32;
        }
        if self.has_counter.load(Relaxed) {
            // 32-bit milliseconds on every data model: the times go into 32-bit message fields
            return (clock.counter_ticks() / self.per_millisecond.load(Relaxed)) as u32;
        }
        clock.milliseconds()
    }

    /// `TMGetMicroTime`: microseconds from the counter (an f32 division, truncated), milliseconds without one.
    pub fn micro_time(&self, clock: &impl Clock) -> i64 {
        if self.has_counter.load(Relaxed) {
            let per_us = f32::from_bits(self.per_microsecond.load(Relaxed));
            // cvttss2si: in range for any counter value (2^62 ticks over at least 1 tick per microsecond)
            return (clock.counter_ticks() as f32 / per_us) as i64;
        }
        clock.milliseconds() as i64
    }
}

impl Default for TimeState {
    fn default() -> Self {
        Self::new()
    }
}

/// The game's Time Manager state.
pub static TIME: TimeState = TimeState::new();

thread_local! {
    /// The time pinned on this thread (EE-SIM's scripted editor), -1 for none.
    static PINNED: Cell<i64> = const { Cell::new(-1) };
}

/// `TMPoll`'s data (the original's members, in its order): a polled countdown on the counter (or the millisecond
/// clock without one).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct EeTimePoll {
    /// Counter ticks (or milliseconds) at the start of the current period.
    pub start_time: i64,
    /// The time already elapsed in the period when paused (0: not paused).
    pub pause_remaining: i64,
    /// 0: milliseconds, 1: microseconds.
    pub time_unit: u32,
    pub time_delay: u32,
    /// The system has a high-resolution counter.
    pub use_counter: bool,
    pub running: bool,
}

impl EeTimePoll {
    /// The constructor: a poll firing every `delay` (ms, or us for unit 1); sets the shared frequency.
    pub fn new(state: &TimeState, clock: &impl Clock, mut delay: u32, unit: u32, start: bool) -> Self {
        let divisor: i64 = if unit != 0 { 1_000_000 } else { 1000 };
        let frequency = clock.counter_frequency();
        let mut poll = EeTimePoll::default();
        if frequency != 0 {
            poll.use_counter = true;
            state.poll_frequency.store(frequency / divisor, Relaxed);
            poll.start_time = clock.counter_ticks();
        } else {
            // the millisecond clock counts milliseconds: only a microsecond delay is converted (at least 1)
            if unit != 0 {
                delay /= 1000;
            }
            if delay == 0 {
                delay = 1;
            }
            state.poll_frequency.store(1, Relaxed);
            poll.start_time = clock.milliseconds() as i64;
            poll.use_counter = false;
        }
        poll.pause_remaining = 0;
        poll.time_unit = unit;
        poll.running = false;
        poll.time_delay = delay;
        if start {
            poll.start(clock);
        }
        poll
    }

    pub fn is_paused(&self) -> bool {
        self.running && self.pause_remaining > 0
    }

    pub fn set_time_delay(&mut self, delay: u32) {
        self.time_delay = if delay > 0 { delay } else { 1 };
    }

    /// (now, the result's scale): the millisecond clock reads a microsecond poll's time in thousands.
    fn now(&self, clock: &impl Clock) -> (i64, u32) {
        if self.use_counter {
            (clock.counter_ticks(), 1)
        } else {
            (clock.milliseconds() as i64, if self.time_unit == 1 { 1000 } else { 1 })
        }
    }

    /// The time left in the current period (0 when stopped).
    pub fn time_left(&self, state: &TimeState, clock: &impl Clock) -> u32 {
        let mut left: u32 = 0;
        let mut scale: u32 = 1;
        if self.running {
            let (now, s) = self.now(clock);
            scale = s;
            let elapsed = now.wrapping_sub(self.start_time) / state.poll_frequency();
            if elapsed < self.time_delay as i64 {
                left = self.time_delay.wrapping_sub(elapsed as u32);
            }
        }
        scale.wrapping_mul(left)
    }

    /// Remembers the time already elapsed in the current period.
    pub fn pause(&mut self, state: &TimeState, clock: &impl Clock) {
        self.pause_remaining = self.time_delay.wrapping_sub(self.time_left(state, clock)) as i64;
    }

    /// The whole periods passed since the last poll; the start advances by them (0 when stopped or paused).
    pub fn poll(&mut self, state: &TimeState, clock: &impl Clock) -> u32 {
        let mut count: u32 = 0;
        let mut scale: u32 = 1;
        if self.running && self.pause_remaining == 0 {
            let (now, s) = self.now(clock);
            scale = s;
            let frequency = state.poll_frequency();
            let start = self.start_time;
            let elapsed = now.wrapping_sub(start) / frequency;
            count = (elapsed / self.time_delay as i64) as u32;
            if count != 0 {
                let remainder = (elapsed as u32).wrapping_sub(self.time_delay.wrapping_mul(count));
                self.start_time = start.wrapping_add(elapsed.wrapping_sub(remainder as i64).wrapping_mul(frequency));
            }
        }
        scale.wrapping_mul(count)
    }

    /// Continues a paused poll's period from the part that had elapsed when it was paused.
    pub fn resume(&mut self, state: &TimeState, clock: &impl Clock) {
        if self.pause_remaining <= 0 || !self.running {
            return;
        }
        if self.use_counter {
            self.start_time =
                clock.counter_ticks().wrapping_sub(self.pause_remaining.wrapping_mul(state.poll_frequency()));
        } else {
            self.start_time = (clock.milliseconds() as i64).wrapping_sub(self.pause_remaining);
        }
        self.pause_remaining = 0;
    }

    /// Starts the poll if it is not running, its period beginning now.
    pub fn start(&mut self, clock: &impl Clock) {
        if self.running {
            return;
        }
        self.pause_remaining = 0;
        self.running = true;
        self.start_time = if self.use_counter { clock.counter_ticks() } else { clock.milliseconds() as i64 };
    }
}

/// `TMTimer`'s results (lle/LowLevelEngineCore.h).
pub const EE_TIME_OK: c_int = 0;
pub const EE_TIME_NOT_FOUND: c_int = 3;
pub const EE_TIME_BUSY: c_int = 5;

/// The system timers a [`EeTimeTimer`] starts (`platform/PlatTime.h`).
pub trait TimerHost {
    fn milliseconds(&self) -> u32;
    /// `PlatTimerStartCallback` with the timer's procedure ([`ee_time_timer_proc`]) and `user`.
    fn start_callback(&self, interval: u32, periodic: bool, user: *mut c_void) -> u32;
    fn start_event(&self, interval: u32, periodic: bool, event: *mut c_void) -> u32;
    fn stop(&self, id: u32) -> bool;
}

/// The platform's timers.
pub struct PlatTimers;

impl TimerHost for PlatTimers {
    fn milliseconds(&self) -> u32 {
        plat::PlatTimeMilliseconds()
    }
    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    fn start_callback(&self, interval: u32, periodic: bool, user: *mut c_void) -> u32 {
        // SAFETY: the procedure takes the timer the C++ owner keeps alive until it stops the timer
        unsafe { plat::PlatTimerStartCallback(interval, periodic as c_int, Some(ee_time_timer_proc), user) }
    }
    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    fn start_event(&self, interval: u32, periodic: bool, event: *mut c_void) -> u32 {
        // SAFETY: a live event of the Thread System (or NULL, which the platform refuses)
        unsafe { plat::PlatTimerStartEvent(interval, periodic as c_int, event) }
    }
    fn stop(&self, id: u32) -> bool {
        plat::PlatTimerStop(id) != 0
    }
}

/// A user callback of a callback timer.
pub type EeTimeCallback = Option<unsafe extern "C" fn(*mut c_void)>;

/// `TMTimer`: a system timer that calls a callback or sets an event, once or periodically. The procedure runs on the
/// platform's timer thread, so every field is atomic (the original raced on plain fields).
pub struct EeTimeTimer {
    callback: AtomicUsize,
    parameter: AtomicPtr<c_void>,
    /// The event's platform handle (the original kept the `TSEvent*` and read its handle at Start).
    event: AtomicPtr<c_void>,
    timer_type: AtomicU32,
    unknown10: AtomicU32,
    /// Milliseconds.
    interval: AtomicU32,
    start_time: AtomicU32,
    timer_id: AtomicU32,
    repeating: AtomicBool,
    running: AtomicBool,
}

impl EeTimeTimer {
    /// A stopped timer with an interval (ms, or us for unit 1, kept in ms) and no callback or event.
    pub fn new(interval: u32, unit: u32, repeating: bool) -> Self {
        EeTimeTimer {
            callback: AtomicUsize::new(0),
            parameter: AtomicPtr::new(std::ptr::null_mut()),
            event: AtomicPtr::new(std::ptr::null_mut()),
            timer_type: AtomicU32::new(0),
            unknown10: AtomicU32::new(0),
            interval: AtomicU32::new(interval / if unit != 0 { 1000 } else { 1 }),
            start_time: AtomicU32::new(0),
            timer_id: AtomicU32::new(0),
            repeating: AtomicBool::new(repeating),
            running: AtomicBool::new(false),
        }
    }

    fn running(&self) -> bool {
        self.running.load(Relaxed)
    }

    /// Runs `set` unless the timer runs (then `EE_TIME_BUSY`).
    fn when_stopped(&self, set: impl FnOnce()) -> c_int {
        if self.running() {
            return EE_TIME_BUSY;
        }
        set();
        EE_TIME_OK
    }

    pub fn set_callback(&self, callback: EeTimeCallback, parameter: *mut c_void) -> c_int {
        self.when_stopped(|| {
            self.callback.store(callback.map_or(0, |f| f as usize), Relaxed);
            self.parameter.store(parameter, Relaxed);
        })
    }
    pub fn set_event(&self, event: *mut c_void) -> c_int {
        self.when_stopped(|| self.event.store(event, Relaxed))
    }
    pub fn set_interval(&self, interval: u32, unit: u32) -> c_int {
        self.when_stopped(|| {
            self.interval.store(interval / if unit != 0 { 1000 } else { 1 }, Relaxed);
            self.unknown10.store(0, Relaxed);
        })
    }
    pub fn set_repeating(&self, repeating: bool) -> c_int {
        self.when_stopped(|| self.repeating.store(repeating, Relaxed))
    }
    pub fn set_timer_type(&self, timer_type: u32) -> c_int {
        self.when_stopped(|| self.timer_type.store(timer_type, Relaxed))
    }

    /// Starts the system timer; `EE_TIME_NOT_FOUND` when the platform refuses it.
    pub fn start(&self, host: &impl TimerHost) -> c_int {
        if self.running() {
            return EE_TIME_BUSY;
        }
        self.start_time.store(host.milliseconds(), Relaxed);
        let interval = self.interval.load(Relaxed);
        let repeating = self.repeating.load(Relaxed);
        let id = if self.timer_type.load(Relaxed) == 0 {
            host.start_callback(interval, repeating, self as *const Self as *mut c_void)
        } else {
            host.start_event(interval, repeating, self.event.load(Relaxed))
        };
        self.timer_id.store(id, Relaxed);
        if id != 0 {
            self.running.store(true, Relaxed);
            EE_TIME_OK
        } else {
            EE_TIME_NOT_FOUND
        }
    }

    /// Stops the timer; `EE_TIME_NOT_FOUND` when it does not run or cannot be stopped.
    pub fn stop(&self, host: &impl TimerHost) -> c_int {
        if self.running() && host.stop(self.timer_id.load(Relaxed)) {
            self.running.store(false, Relaxed);
            return EE_TIME_OK;
        }
        EE_TIME_NOT_FOUND
    }

    /// Stops the timer -> (result, the milliseconds since it last started, the remaining time: always 0); the
    /// outputs are written only on success.
    pub fn stop_elapsed(&self, host: &impl TimerHost) -> (c_int, Option<(u32, u32)>) {
        if !self.running() {
            return (EE_TIME_NOT_FOUND, None);
        }
        let now = host.milliseconds();
        if host.stop(self.timer_id.load(Relaxed)) {
            let elapsed = now.wrapping_sub(self.start_time.load(Relaxed));
            self.running.store(false, Relaxed);
            return (EE_TIME_OK, Some((elapsed, 0)));
        }
        (EE_TIME_NOT_FOUND, None)
    }

    /// The system timer fired: restart the elapsed clock (repeating) or mark the timer stopped, then the callback.
    pub fn fire(&self, host: &impl TimerHost) {
        if self.repeating.load(Relaxed) {
            self.start_time.store(host.milliseconds(), Relaxed);
        } else {
            self.running.store(false, Relaxed);
        }
        let callback = self.callback.load(Relaxed);
        if callback != 0 {
            // SAFETY: stored from an `EeTimeCallback` by set_callback
            let f: unsafe extern "C" fn(*mut c_void) = unsafe { std::mem::transmute(callback) };
            // SAFETY: the owner's callback with its parameter, as the original called it
            unsafe { f(self.parameter.load(Relaxed)) };
        }
    }

    /// (callback, parameter, event, type, unknown10, interval, start, id, repeating, running) for the golden.
    #[cfg(test)]
    fn fields(&self) -> (usize, usize, usize, u32, u32, u32, u32, u32, bool, bool) {
        (
            self.callback.load(Relaxed),
            self.parameter.load(Relaxed) as usize,
            self.event.load(Relaxed) as usize,
            self.timer_type.load(Relaxed),
            self.unknown10.load(Relaxed),
            self.interval.load(Relaxed),
            self.start_time.load(Relaxed),
            self.timer_id.load(Relaxed),
            self.repeating.load(Relaxed),
            self.running.load(Relaxed),
        )
    }
}

/// The system timer's procedure (the original's `TMTimer::TimerCallback`): `user` is the timer.
///
/// # Safety
/// `user` is a live [`EeTimeTimer`].
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_proc(user: *mut c_void) {
    (*(user as *const EeTimeTimer)).fire(&PlatTimers)
}

// ---------------------------------------------------------------------------------------------------------------
// The C interface (lle/TMPoll.h, lle/TMTimer.h, Time Manager.cpp)

/// `TMInitialize`'s part: the 1 ms timer resolution and the counter's frequencies (C++ registers
/// [`ee_time_shutdown`]).
#[no_mangle]
pub extern "C" fn ee_time_initialize() {
    plat::PlatTimeBeginPeriod(1);
    TIME.initialize(&PlatClock);
}

/// `TMInitialize`: [`ee_time_initialize`], then [`ee_time_shutdown`] registered with the engine's shutdown list
/// (ee_util; always 0, `kErrorOK`).
#[no_mangle]
pub extern "C" fn ee_time_system_initialize() -> std::ffi::c_long {
    ee_time_initialize();
    ee_util::ee_util_register_shutdown(ee_time_shutdown)
}

/// The Time Manager's shutdown (`URegisterShutdown`): the timer resolution restored.
#[no_mangle]
pub extern "C" fn ee_time_shutdown(_forced: bool) {
    plat::PlatTimeEndPeriod(1);
}

/// `TMGetTime`: milliseconds (32 bits on every data model), or the time pinned on this thread.
#[no_mangle]
pub extern "C" fn ee_time_get() -> u32 {
    TIME.time(&PlatClock, PINNED.with(Cell::get))
}

/// `TMGetMicroTime`.
#[no_mangle]
pub extern "C" fn ee_time_get_micro() -> i64 {
    TIME.micro_time(&PlatClock)
}

/// Pins `TMGetTime` on the calling thread to `time` (-1: the clock again). TEST ONLY: EE-SIM's scripted editor.
#[no_mangle]
pub extern "C" fn ee_time_pin(time: i64) {
    PINNED.with(|p| p.set(time))
}

/// `TMTime` (lle/LowLevelEngineCore.h): the local time of day, the month counted from 0.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct EeTimeOfDay {
    pub mMonth: c_ulong,
    pub mDay: c_ulong,
    pub mYear: c_ulong,
    pub mHour: c_ulong,
    pub mMinute: c_ulong,
    pub mSecond: c_ulong,
}

impl EeTimeOfDay {
    pub fn from_local(t: &plat::PlatDateTime) -> Self {
        EeTimeOfDay {
            mMonth: (t.month as i32 - 1) as c_ulong,
            mDay: t.day as c_ulong,
            mYear: t.year as c_ulong,
            mHour: t.hour as c_ulong,
            mMinute: t.minute as c_ulong,
            mSecond: t.second as c_ulong,
        }
    }
}

/// `TMGetTimeOfDay`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_time_of_day(out: *mut EeTimeOfDay) {
    let mut local = plat::PlatDateTime::default();
    plat::PlatGetLocalTime(&mut local);
    *out = EeTimeOfDay::from_local(&local);
}

/// `TMPoll`'s constructor.
///
/// # Safety
/// `poll` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_init(poll: *mut EeTimePoll, delay: u32, unit: u32, start: bool) {
    *poll = EeTimePoll::new(&TIME, &PlatClock, delay, unit, start);
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_is_paused(poll: *const EeTimePoll) -> bool {
    (*poll).is_paused()
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_is_running(poll: *const EeTimePoll) -> bool {
    (*poll).running
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_time_left(poll: *const EeTimePoll) -> u32 {
    (*poll).time_left(&TIME, &PlatClock)
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_set_time_delay(poll: *mut EeTimePoll, delay: u32) {
    (*poll).set_time_delay(delay)
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_start(poll: *mut EeTimePoll) {
    (*poll).start(&PlatClock)
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_stop(poll: *mut EeTimePoll) {
    (*poll).running = false;
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_pause(poll: *mut EeTimePoll) {
    (*poll).pause(&TIME, &PlatClock)
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_resume(poll: *mut EeTimePoll) {
    (*poll).resume(&TIME, &PlatClock)
}

/// # Safety
/// `poll` is a live poll.
#[no_mangle]
pub unsafe extern "C" fn ee_time_poll_poll(poll: *mut EeTimePoll) -> u32 {
    (*poll).poll(&TIME, &PlatClock)
}

/// `TMTimer`'s constructor -> the timer ([`ee_time_timer_free`] stops and frees it).
#[no_mangle]
pub extern "C" fn ee_time_timer_new(interval: u32, unit: u32, repeating: bool) -> *mut EeTimeTimer {
    Box::into_raw(Box::new(EeTimeTimer::new(interval, unit, repeating)))
}

/// `TMTimer`'s destructor: stops the timer, then frees it (NULL is ignored).
///
/// # Safety
/// `timer` came from [`ee_time_timer_new`] and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_free(timer: *mut EeTimeTimer) {
    if !timer.is_null() {
        (*timer).stop(&PlatTimers);
        drop(Box::from_raw(timer));
    }
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_set_callback(
    timer: *const EeTimeTimer,
    callback: EeTimeCallback,
    parameter: *mut c_void,
) -> c_int {
    (*timer).set_callback(callback, parameter)
}

/// `SetEvent`: `event` is the event's platform handle (`TSEvent::mHandle`; NULL for none).
///
/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_set_event(timer: *const EeTimeTimer, event: *mut c_void) -> c_int {
    (*timer).set_event(event)
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_set_interval(timer: *const EeTimeTimer, interval: u32, unit: u32) -> c_int {
    (*timer).set_interval(interval, unit)
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_set_repeating(timer: *const EeTimeTimer, repeating: bool) -> c_int {
    (*timer).set_repeating(repeating)
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_set_type(timer: *const EeTimeTimer, timer_type: u32) -> c_int {
    (*timer).set_timer_type(timer_type)
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_start(timer: *const EeTimeTimer) -> c_int {
    (*timer).start(&PlatTimers)
}

/// # Safety
/// `timer` is a live timer.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_stop(timer: *const EeTimeTimer) -> c_int {
    (*timer).stop(&PlatTimers)
}

/// `Stop(outElapsed, outRemaining)`: the outputs are written only on success.
///
/// # Safety
/// `timer` is a live timer; the outputs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_time_timer_stop_elapsed(
    timer: *const EeTimeTimer,
    elapsed: *mut u32,
    remaining: *mut u32,
) -> c_int {
    let (result, out) = (*timer).stop_elapsed(&PlatTimers);
    if let Some((e, r)) = out {
        *elapsed = e;
        *remaining = r;
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default)]
    pub(crate) struct FakeClock {
        pub frequency: Cell<i64>,
        pub ticks: Cell<i64>,
        pub ms: Cell<u32>,
    }
    impl Clock for FakeClock {
        fn counter_frequency(&self) -> i64 {
            self.frequency.get()
        }
        fn counter_ticks(&self) -> i64 {
            self.ticks.get()
        }
        fn milliseconds(&self) -> u32 {
            self.ms.get()
        }
    }

    #[test]
    fn poll_layout_is_the_originals() {
        assert_eq!(std::mem::size_of::<EeTimePoll>(), 0x20);
        assert_eq!(std::mem::align_of::<EeTimePoll>(), 8);
        assert_eq!(std::mem::offset_of!(EeTimePoll, time_unit), 0x10);
        assert_eq!(std::mem::offset_of!(EeTimePoll, running), 0x19);
    }

    #[test]
    fn a_millisecond_poll_counts_whole_periods() {
        let state = TimeState::new();
        let clock = FakeClock::default();
        clock.frequency.set(10_000_000);
        clock.ticks.set(5_000);
        let mut p = EeTimePoll::new(&state, &clock, 100, 0, true);
        assert_eq!(state.poll_frequency(), 10_000);
        clock.ticks.set(5_000 + 250 * 10_000);
        assert_eq!(p.time_left(&state, &clock), 0);
        assert_eq!(p.poll(&state, &clock), 2);
        assert_eq!(p.start_time, 5_000 + 200 * 10_000);
        assert_eq!(p.time_left(&state, &clock), 50);
        p.pause(&state, &clock);
        assert!(p.is_paused());
        assert_eq!(p.poll(&state, &clock), 0);
        clock.ticks.set(clock.ticks.get() + 1_000 * 10_000);
        p.resume(&state, &clock);
        assert_eq!(p.time_left(&state, &clock), 50);
    }

    #[test]
    fn a_microsecond_poll_rescales_the_shared_frequency() {
        let state = TimeState::new();
        let clock = FakeClock::default();
        clock.frequency.set(10_000_000);
        let _ms = EeTimePoll::new(&state, &clock, 100, 0, false);
        assert_eq!(state.poll_frequency(), 10_000);
        let _us = EeTimePoll::new(&state, &clock, 100, 1, false);
        assert_eq!(state.poll_frequency(), 10);
    }

    #[test]
    fn without_a_counter_a_microsecond_delay_becomes_milliseconds() {
        let state = TimeState::new();
        let clock = FakeClock::default();
        clock.ms.set(1000);
        let mut p = EeTimePoll::new(&state, &clock, 2500, 1, true);
        assert_eq!(p.time_delay, 2);
        clock.ms.set(1001);
        assert_eq!(p.time_left(&state, &clock), 1000);
        clock.ms.set(1004);
        assert_eq!(p.poll(&state, &clock), 2000);
        let p0 = EeTimePoll::new(&state, &clock, 999, 1, false);
        assert_eq!(p0.time_delay, 1);
    }

    #[test]
    fn time_follows_the_pin_and_the_counter() {
        let state = TimeState::new();
        let clock = FakeClock::default();
        clock.ms.set(77);
        assert_eq!(state.time(&clock, -1), 77);
        clock.frequency.set(3_579_545);
        clock.ticks.set(3_579_545 * 10);
        state.initialize(&clock);
        assert_eq!(state.time(&clock, -1), 10_001); // 3,579 ticks per millisecond
        assert_eq!(state.time(&clock, 42), 42);
        assert!((state.micro_time(&clock) - 10_000_000).abs() <= 1);
    }

    struct FakeTimers {
        ms: Cell<u32>,
        id: Cell<u32>,
        stop_ok: Cell<bool>,
        log: RefCell<Vec<String>>,
    }
    impl TimerHost for FakeTimers {
        fn milliseconds(&self) -> u32 {
            self.ms.get()
        }
        fn start_callback(&self, interval: u32, periodic: bool, _user: *mut c_void) -> u32 {
            self.log.borrow_mut().push(format!("c {interval} {periodic}"));
            self.id.get()
        }
        fn start_event(&self, interval: u32, periodic: bool, event: *mut c_void) -> u32 {
            self.log.borrow_mut().push(format!("e {interval} {periodic} {}", event as usize));
            self.id.get()
        }
        fn stop(&self, id: u32) -> bool {
            self.log.borrow_mut().push(format!("s {id}"));
            self.stop_ok.get()
        }
    }

    #[test]
    fn a_timer_is_busy_while_running_and_reports_its_elapsed_time() {
        let host =
            FakeTimers { ms: Cell::new(10), id: Cell::new(7), stop_ok: Cell::new(true), log: RefCell::default() };
        let t = EeTimeTimer::new(2500, 1, true);
        assert_eq!(t.fields().5, 2);
        assert_eq!(t.start(&host), EE_TIME_OK);
        assert_eq!(t.set_interval(5, 0), EE_TIME_BUSY);
        assert_eq!(t.start(&host), EE_TIME_BUSY);
        host.ms.set(40);
        assert_eq!(t.stop_elapsed(&host), (EE_TIME_OK, Some((30, 0))));
        assert_eq!(t.stop(&host), EE_TIME_NOT_FOUND);
        assert_eq!(*host.log.borrow(), ["c 2 true", "s 7"]);
        host.id.set(0);
        assert_eq!(t.start(&host), EE_TIME_NOT_FOUND);
    }
}
