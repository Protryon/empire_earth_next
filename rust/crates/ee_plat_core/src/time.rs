//! `platform/PlatTime.h`: clocks, sleep, local time and system timers. Windows calls exactly the Win32 functions the
//! C++ called (`timeGetTime`, `GetTickCount64`, `QueryPerformanceCounter/Frequency`, `Sleep`, `timeBeginPeriod`,
//! `timeEndPeriod`, `GetLocalTime`, `GetSystemTimeAsFileTime`, `timeSetEvent`, `timeKillEvent`): the test harness's
//! time shim replaces those imports (tools/harness/timeshim), so they must stay the ones called, never std::time.
//! POSIX runs the clocks on CLOCK_MONOTONIC and the timers on one thread started on first use, as the C++ did.
//!
//! The harness clock (`PlatSimSetTimeFactor`, `sim_clock`): with a factor set, the clocks here run that many times
//! faster and the sleeps, timer intervals and wait timeouts last that much less real time. Without one, every
//! function calls exactly what it called before.

use crate::sim_clock;
use std::ffi::{c_int, c_void};

/// `PlatDateTime`: the header's layout.
#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct PlatDateTime {
    pub year: u16,
    pub month: u16,
    pub day_of_week: u16,
    pub day: u16,
    pub hour: u16,
    pub minute: u16,
    pub second: u16,
    pub milliseconds: u16,
}

const _: () = assert!(std::mem::size_of::<PlatDateTime>() == 16);

/// `PlatEvent`: opaque here.
pub type PlatEvent = c_void;
/// `PlatTimerCallback`: NULL is rejected by `PlatTimerStartCallback` on POSIX and never called.
pub type PlatTimerCallback = Option<unsafe extern "C" fn(*mut c_void)>;

/// FILETIME units (100 ns since 1601-01-01) of a Unix time.
#[cfg(any(unix, test))]
fn filetime_of_unix(seconds: u64, nanoseconds: u64) -> u64 {
    (seconds + 11_644_473_600) * 10_000_000 + nanoseconds / 100
}

/// A periodic timer's next due time: the last due time plus the interval (it keeps its phase), or from now when it
/// fell far behind (a suspended process: no burst of catch-ups).
fn next_due(due: u64, interval: u64, now: u64) -> u64 {
    let next = due + interval;
    if next < now {
        now + interval
    } else {
        next
    }
}

#[cfg(windows)]
mod imp {
    use super::*;
    use std::collections::BTreeMap;
    use std::mem::MaybeUninit;
    use std::ptr::{null, null_mut};
    use std::sync::{Mutex, PoisonError};
    use windows_sys::Win32::Foundation::FILETIME;
    use windows_sys::Win32::Foundation::SYSTEMTIME;
    use windows_sys::Win32::Foundation::{CloseHandle, HANDLE, WAIT_OBJECT_0, WAIT_TIMEOUT};
    use windows_sys::Win32::Media::{
        timeBeginPeriod, timeEndPeriod, timeGetTime, timeKillEvent, timeSetEvent, LPTIMECALLBACK, TIMERR_NOERROR,
        TIME_CALLBACK_EVENT_SET, TIME_CALLBACK_FUNCTION, TIME_ONESHOT, TIME_PERIODIC,
    };
    use windows_sys::Win32::System::Performance::{QueryPerformanceCounter, QueryPerformanceFrequency};
    use windows_sys::Win32::System::SystemInformation::{GetLocalTime, GetSystemTimeAsFileTime, GetTickCount64};
    use windows_sys::Win32::System::Threading::{
        CancelWaitableTimer, CreateEventW, CreateWaitableTimerExW, GetCurrentThread, SetEvent, SetThreadPriority,
        SetWaitableTimer, Sleep, WaitForMultipleObjects, WaitForSingleObject, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        INFINITE, THREAD_PRIORITY_TIME_CRITICAL, TIMER_ALL_ACCESS,
    };
    const MAXIMUM_WAIT_OBJECTS: usize = 64;

    pub fn milliseconds() -> u32 {
        // SAFETY: no arguments.
        unsafe { timeGetTime() }
    }

    pub fn milliseconds64() -> u64 {
        // SAFETY: no arguments.
        unsafe { GetTickCount64() }
    }

    pub fn counter_frequency() -> i64 {
        let mut value = 0;
        // SAFETY: `value` is writable; the result is checked.
        if unsafe { QueryPerformanceFrequency(&mut value) } == 0 {
            return 0;
        }
        value
    }

    pub fn counter_ticks() -> i64 {
        let mut value = 0;
        // SAFETY: `value` is writable; the result is checked.
        if unsafe { QueryPerformanceCounter(&mut value) } == 0 {
            return 0;
        }
        value
    }

    pub fn sleep(milliseconds: u32) {
        // SAFETY: no pointers.
        unsafe { Sleep(milliseconds) }
    }

    // ---- the harness clock's precise waits: a high-resolution waitable timer per thread (closed with the thread),
    // as tools/harness/timeshim.c waited, so that a scaled millisecond can last less than a real one

    struct ThreadTimer(HANDLE);

    impl Drop for ThreadTimer {
        fn drop(&mut self) {
            if !self.0.is_null() {
                // SAFETY: this thread's own timer.
                unsafe { CloseHandle(self.0) };
            }
        }
    }

    thread_local! {
        static TIMER: ThreadTimer = ThreadTimer(new_timer());
    }

    fn new_timer() -> HANDLE {
        // SAFETY: NULL attributes and name; a NULL result is handled by the callers.
        unsafe {
            let timer = CreateWaitableTimerExW(null(), null(), CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if !timer.is_null() {
                return timer;
            }
            CreateWaitableTimerExW(null(), null(), 0, TIMER_ALL_ACCESS) // before Windows 10 1803
        }
    }

    /// Arms `timer` to fire once, `nanoseconds` from now (100 ns units, rounded up).
    unsafe fn arm(timer: HANDLE, nanoseconds: u64) {
        let due = -(nanoseconds.div_ceil(100).max(1) as i64);
        SetWaitableTimer(timer, &due, 0, None, null(), 0);
    }

    pub fn sleep_nanoseconds(nanoseconds: u64) {
        TIMER.with(|timer| {
            // SAFETY: the thread's own timer, or a plain Sleep when there is none.
            unsafe {
                if timer.0.is_null() {
                    Sleep(nanoseconds.div_ceil(1_000_000) as u32);
                    return;
                }
                arm(timer.0, nanoseconds);
                WaitForSingleObject(timer.0, INFINITE);
            }
        })
    }

    /// WaitForMultipleObjects (any one) on `handles` for `nanoseconds` of real time -> its result, WAIT_TIMEOUT when
    /// the time ran out. None when it cannot (too many handles, no timer): the caller then waits as without a factor.
    pub unsafe fn wait_nanoseconds(handles: &[HANDLE], nanoseconds: u64) -> Option<u32> {
        if handles.is_empty() || handles.len() >= MAXIMUM_WAIT_OBJECTS {
            return None;
        }
        TIMER.with(|timer| {
            if timer.0.is_null() {
                return None;
            }
            let mut set = [null_mut(); MAXIMUM_WAIT_OBJECTS];
            set[..handles.len()].copy_from_slice(handles);
            set[handles.len()] = timer.0;
            arm(timer.0, nanoseconds);
            let count = handles.len() as u32;
            let result = WaitForMultipleObjects(count + 1, set.as_ptr(), 0, INFINITE);
            if result == WAIT_OBJECT_0 + count {
                return Some(WAIT_TIMEOUT);
            }
            CancelWaitableTimer(timer.0);
            Some(result)
        })
    }

    /// Real time in nanoseconds on the performance counter (the scaled timers' deadlines).
    fn real_nanoseconds_now() -> u64 {
        let frequency = counter_frequency().max(1) as i128;
        (counter_ticks() as i128 * 1_000_000_000 / frequency) as u64
    }

    // ---- the harness clock's timers: with a factor set, a timer is a thread of its own with deadlines on the real
    // performance counter (a multimedia timer fires at most once a millisecond). Their ids start at SCALED_FIRST, as
    // the time shim's did, so PlatTimerStop tells them apart.

    pub const SCALED_FIRST: u32 = 0x7e00_0001;

    struct StopEvent(HANDLE);

    // SAFETY: an event handle, usable from any thread.
    unsafe impl Send for StopEvent {}
    unsafe impl Sync for StopEvent {}

    impl Drop for StopEvent {
        fn drop(&mut self) {
            // SAFETY: the event this owns.
            unsafe { CloseHandle(self.0) };
        }
    }

    struct Scaled {
        stop: std::sync::Arc<StopEvent>,
        thread: Option<std::thread::JoinHandle<()>>,
    }

    static SCALED: Mutex<BTreeMap<u32, Scaled>> = Mutex::new(BTreeMap::new());
    static NEXT_SCALED: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(SCALED_FIRST);

    fn scaled_table() -> std::sync::MutexGuard<'static, BTreeMap<u32, Scaled>> {
        SCALED.lock().unwrap_or_else(PoisonError::into_inner)
    }

    #[derive(Clone, Copy)]
    enum Fire {
        Callback(PlatTimerCallback, *mut c_void),
        Event(*mut PlatEvent),
    }

    // SAFETY: as `Entry`: the pointers are only handed back to the callback, or to SetEvent.
    unsafe impl Send for Fire {}

    fn scaled_start(period_ns: u64, periodic: bool, fire: Fire) -> u32 {
        // SAFETY: an unnamed manual-reset event; a NULL result is checked.
        let stop = unsafe { CreateEventW(null(), 1, 0, null()) };
        if stop.is_null() {
            return 0;
        }
        let stop = std::sync::Arc::new(StopEvent(stop));
        let id = NEXT_SCALED.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        // the table is locked across the spawn, so a one-shot timer that fires at once finds its entry to remove
        let mut table = scaled_table();
        let theirs = stop.clone();
        let spawned = std::thread::Builder::new()
            .name("PlatSim timer".into())
            .spawn(move || scaled_main(id, period_ns, periodic, fire, &theirs));
        let Ok(thread) = spawned else {
            return 0;
        };
        table.insert(id, Scaled { stop, thread: Some(thread) });
        id
    }

    fn scaled_main(id: u32, period_ns: u64, periodic: bool, fire: Fire, stop: &StopEvent) {
        // SAFETY: the current thread's pseudo handle.
        unsafe { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) }; // as winmm's timer thread
        let mut deadline = real_nanoseconds_now() + period_ns;
        TIMER.with(|timer| loop {
            let now = real_nanoseconds_now();
            // SAFETY: the stop event lives while this thread holds it; the timer is this thread's.
            unsafe {
                if deadline > now && !timer.0.is_null() {
                    arm(timer.0, deadline - now);
                    let set = [stop.0, timer.0];
                    if WaitForMultipleObjects(2, set.as_ptr(), 0, INFINITE) != WAIT_OBJECT_0 + 1 {
                        return;
                    }
                } else if deadline > now {
                    Sleep((deadline - now).div_ceil(1_000_000) as u32);
                }
                if WaitForSingleObject(stop.0, 0) == WAIT_OBJECT_0 {
                    return;
                }
                match fire {
                    Fire::Callback(Some(callback), user) => callback(user),
                    Fire::Callback(None, _) => {}
                    Fire::Event(event) => {
                        SetEvent(event as HANDLE);
                    }
                }
            }
            if !periodic {
                // a one-shot timer's id is gone once it fired (as timeSetEvent's); this thread is not joined
                if let Some(mut entry) = scaled_table().remove(&id) {
                    entry.thread.take();
                }
                return;
            }
            deadline = next_due(deadline, period_ns, real_nanoseconds_now());
        })
    }

    /// Stops a timer of `scaled_start` -> 1, 0 when it is not running (any more).
    fn scaled_stop(id: u32) -> c_int {
        let Some(mut entry) = scaled_table().remove(&id) else {
            return 0;
        };
        // SAFETY: the event lives while `entry` holds it.
        unsafe { SetEvent(entry.stop.0) };
        if let Some(thread) = entry.thread.take() {
            if thread.thread().id() != std::thread::current().id() {
                let _ = thread.join(); // a callback that stops its own timer cannot wait for itself
            }
        }
        1
    }

    pub fn begin_period(milliseconds: u32) {
        // SAFETY: no pointers.
        unsafe { timeBeginPeriod(milliseconds) };
    }

    pub fn end_period(milliseconds: u32) {
        // SAFETY: no pointers.
        unsafe { timeEndPeriod(milliseconds) };
    }

    pub unsafe fn get_local_time(out: *mut PlatDateTime) {
        let mut time = MaybeUninit::<SYSTEMTIME>::uninit();
        GetLocalTime(time.as_mut_ptr());
        let time = time.assume_init();
        *out = PlatDateTime {
            year: time.wYear,
            month: time.wMonth,
            day_of_week: time.wDayOfWeek,
            day: time.wDay,
            hour: time.wHour,
            minute: time.wMinute,
            second: time.wSecond,
            milliseconds: time.wMilliseconds,
        };
    }

    pub fn wall_clock_100ns() -> u64 {
        let mut time = MaybeUninit::<FILETIME>::uninit();
        // SAFETY: `time` is written by the call.
        let time = unsafe {
            GetSystemTimeAsFileTime(time.as_mut_ptr());
            time.assume_init()
        };
        ((time.dwHighDateTime as u64) << 32) | time.dwLowDateTime as u64
    }

    // ---- system timers
    //
    // An event timer is a plain TIME_CALLBACK_EVENT_SET timer on the event's handle (platform/PlatThread.h: on Win32
    // a PlatEvent* is the event HANDLE). A callback timer needs its callback and user pointer: they are kept by timer
    // id in a table, which the multimedia-timer callback looks up by the id it is given (never through a pointer that
    // a concurrent Stop may have freed). A one-shot timer's entry goes when it fires, a stopped timer's when it is
    // killed. The table is a static: timers may fire during exit.

    #[derive(Clone, Copy)]
    struct Entry {
        callback: PlatTimerCallback,
        user: *mut c_void,
        periodic: bool,
    }

    // SAFETY: `user` is only handed back to the callback that came with it, as the C++ table did.
    unsafe impl Send for Entry {}

    static TIMERS: Mutex<BTreeMap<u32, Entry>> = Mutex::new(BTreeMap::new());

    fn table() -> std::sync::MutexGuard<'static, BTreeMap<u32, Entry>> {
        TIMERS.lock().unwrap_or_else(PoisonError::into_inner)
    }

    unsafe extern "system" fn thunk(id: u32, _message: u32, _user: usize, _reserved1: usize, _reserved2: usize) {
        let entry = {
            let mut table = table();
            let Some(entry) = table.get(&id).copied() else {
                return; // stopped meanwhile
            };
            if !entry.periodic {
                table.remove(&id);
            }
            entry
        };
        if let Some(callback) = entry.callback {
            callback(entry.user);
        }
    }

    pub unsafe fn start_callback(
        interval_ms: u32,
        periodic: c_int,
        callback: PlatTimerCallback,
        user: *mut c_void,
    ) -> u32 {
        if let Some(scale) = crate::sim_clock::scale() {
            let period = scale.real_nanoseconds(interval_ms.max(1));
            return scaled_start(period, periodic != 0, Fire::Callback(callback, user));
        }
        // the lock is held across timeSetEvent, so a timer that fires at once waits until its entry exists
        let mut table = table();
        let flags = TIME_CALLBACK_FUNCTION | if periodic != 0 { TIME_PERIODIC } else { TIME_ONESHOT };
        let id = timeSetEvent(interval_ms, 0, Some(thunk), 0, flags);
        if id != 0 {
            table.insert(id, Entry { callback, user, periodic: periodic != 0 });
        }
        id
    }

    pub unsafe fn start_event(interval_ms: u32, periodic: c_int, event: *mut PlatEvent) -> u32 {
        if let Some(scale) = crate::sim_clock::scale() {
            let period = scale.real_nanoseconds(interval_ms.max(1));
            return scaled_start(period, periodic != 0, Fire::Event(event));
        }
        let flags = TIME_CALLBACK_EVENT_SET | if periodic != 0 { TIME_PERIODIC } else { TIME_ONESHOT };
        // TIME_CALLBACK_EVENT_SET: the "callback" is the event handle
        let handle = std::mem::transmute::<*mut c_void, LPTIMECALLBACK>(event);
        timeSetEvent(interval_ms, 0, handle, 0, flags)
    }

    pub fn stop(id: u32) -> c_int {
        if id >= SCALED_FIRST {
            return scaled_stop(id);
        }
        // SAFETY: no pointers.
        if unsafe { timeKillEvent(id) } != TIMERR_NOERROR {
            return 0;
        }
        table().remove(&id);
        1
    }
}

#[cfg(unix)]
mod imp {
    use super::*;
    use std::collections::BTreeMap;
    use std::sync::{Condvar, Mutex, MutexGuard, PoisonError};
    use std::time::Duration;

    fn monotonic_nanoseconds() -> u64 {
        let mut now = libc::timespec { tv_sec: 0, tv_nsec: 0 };
        // SAFETY: `now` is writable.
        unsafe { libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut now) };
        now.tv_sec as u64 * 1_000_000_000 + now.tv_nsec as u64
    }

    pub fn milliseconds() -> u32 {
        (monotonic_nanoseconds() / 1_000_000) as u32
    }

    pub fn milliseconds64() -> u64 {
        monotonic_nanoseconds() / 1_000_000
    }

    pub fn counter_frequency() -> i64 {
        1_000_000_000
    }

    pub fn counter_ticks() -> i64 {
        monotonic_nanoseconds() as i64
    }

    pub fn sleep(milliseconds: u32) {
        // SAFETY: no pointers (sched_yield) or both valid (nanosleep).
        unsafe {
            if milliseconds == 0 {
                libc::sched_yield();
                return;
            }
            let mut left = libc::timespec {
                tv_sec: (milliseconds / 1000) as _,
                tv_nsec: ((milliseconds % 1000) * 1_000_000) as _,
            };
            while libc::nanosleep(&left, &mut left) != 0
                && std::io::Error::last_os_error().raw_os_error() == Some(libc::EINTR)
            {}
        }
    }

    pub fn sleep_nanoseconds(nanoseconds: u64) {
        let mut left =
            libc::timespec { tv_sec: (nanoseconds / 1_000_000_000) as _, tv_nsec: (nanoseconds % 1_000_000_000) as _ };
        // SAFETY: both valid.
        unsafe {
            while libc::nanosleep(&left, &mut left) != 0
                && std::io::Error::last_os_error().raw_os_error() == Some(libc::EINTR)
            {}
        }
    }

    pub fn begin_period(_milliseconds: u32) {} // the POSIX clocks and sleeps have no system-wide resolution setting

    pub fn end_period(_milliseconds: u32) {}

    pub unsafe fn get_local_time(out: *mut PlatDateTime) {
        let mut now = libc::timeval { tv_sec: 0, tv_usec: 0 };
        libc::gettimeofday(&mut now, std::ptr::null_mut());
        let seconds = now.tv_sec;
        let mut local: libc::tm = std::mem::zeroed();
        libc::localtime_r(&seconds, &mut local);
        *out = PlatDateTime {
            year: (local.tm_year + 1900) as u16,
            month: (local.tm_mon + 1) as u16,
            day_of_week: local.tm_wday as u16,
            day: local.tm_mday as u16,
            hour: local.tm_hour as u16,
            minute: local.tm_min as u16,
            second: local.tm_sec.min(59) as u16, // no leap second
            milliseconds: (now.tv_usec / 1000) as u16,
        };
    }

    pub fn wall_clock_100ns() -> u64 {
        let mut now = libc::timespec { tv_sec: 0, tv_nsec: 0 };
        // SAFETY: `now` is writable.
        unsafe { libc::clock_gettime(libc::CLOCK_REALTIME, &mut now) };
        filetime_of_unix(now.tv_sec as u64, now.tv_nsec as u64)
    }

    // ---- system timers: one thread, started on first use, keeps the timers and calls each on itself, as the Win32
    // multimedia timers do on their system thread. The tables are statics: timers may fire during exit.

    #[derive(Clone, Copy)]
    struct Timer {
        due: u64,      // ns, monotonic
        interval: u64, // ns
        periodic: bool,
        callback: PlatTimerCallback, // or
        user: *mut c_void,
        event: *mut PlatEvent,
    }

    // SAFETY: the pointers are only handed back to the callback or to PlatEventSet, as the C++ table did.
    unsafe impl Send for Timer {}

    struct Timers {
        table: BTreeMap<u32, Timer>,
        next_id: u32,
        thread_started: bool,
    }

    static TIMERS: Mutex<Timers> = Mutex::new(Timers { table: BTreeMap::new(), next_id: 1, thread_started: false });
    static WAKE: Condvar = Condvar::new();

    fn lock() -> MutexGuard<'static, Timers> {
        TIMERS.lock().unwrap_or_else(PoisonError::into_inner)
    }

    fn timer_thread() {
        let mut timers = lock();
        loop {
            // the earliest due timer (the lowest id among equals, as the C++ map's order)
            let now = monotonic_nanoseconds();
            let next = timers.table.iter().min_by_key(|(_, t)| t.due).map(|(id, t)| (*id, *t));
            let Some((id, timer)) = next else {
                timers = WAKE.wait(timers).unwrap_or_else(PoisonError::into_inner);
                continue;
            };
            if timer.due > now {
                let wait = Duration::from_nanos(timer.due - now);
                timers = WAKE.wait_timeout(timers, wait).unwrap_or_else(PoisonError::into_inner).0;
                continue; // timers may have been added or stopped meanwhile
            }
            if timer.periodic {
                timers.table.get_mut(&id).unwrap().due = next_due(timer.due, timer.interval, now);
            } else {
                timers.table.remove(&id);
            }
            drop(timers);
            // SAFETY: as the C++: a callback given to PlatTimerStartCallback with its user pointer, or a live event.
            unsafe {
                match timer.callback {
                    Some(callback) => callback(timer.user),
                    None => {
                        crate::seam::PlatEventSet(timer.event);
                    }
                }
            }
            timers = lock();
        }
    }

    fn start(timer: Timer) -> u32 {
        let mut timers = lock();
        if !timers.thread_started {
            timers.thread_started = std::thread::Builder::new().spawn(timer_thread).is_ok();
        }
        if !timers.thread_started {
            // SAFETY: a plain C function.
            unsafe { crate::seam::ee_plat_posix_set_last_error(8) }; // ERROR_NOT_ENOUGH_MEMORY
            return 0;
        }
        let id = timers.next_id;
        timers.next_id = if timers.next_id == u32::MAX { 1 } else { timers.next_id + 1 };
        timers.table.insert(id, timer);
        WAKE.notify_one();
        id
    }

    fn interval_nanoseconds(interval_ms: u32) -> u64 {
        let interval_ms = interval_ms.max(1);
        crate::sim_clock::real_timeout_nanoseconds(interval_ms).unwrap_or(u64::from(interval_ms) * 1_000_000)
    }

    pub unsafe fn start_callback(
        interval_ms: u32,
        periodic: c_int,
        callback: PlatTimerCallback,
        user: *mut c_void,
    ) -> u32 {
        if callback.is_none() {
            return 0;
        }
        let interval = interval_nanoseconds(interval_ms);
        start(Timer {
            due: monotonic_nanoseconds() + interval,
            interval,
            periodic: periodic != 0,
            callback,
            user,
            event: std::ptr::null_mut(),
        })
    }

    pub unsafe fn start_event(interval_ms: u32, periodic: c_int, event: *mut PlatEvent) -> u32 {
        if event.is_null() {
            return 0;
        }
        let interval = interval_nanoseconds(interval_ms);
        start(Timer {
            due: monotonic_nanoseconds() + interval,
            interval,
            periodic: periodic != 0,
            callback: None,
            user: std::ptr::null_mut(),
            event,
        })
    }

    pub fn stop(id: u32) -> c_int {
        let mut timers = lock();
        let existed = timers.table.remove(&id).is_some();
        if existed {
            WAKE.notify_one();
        }
        existed as c_int
    }
}

#[no_mangle]
pub extern "C" fn PlatTimeMilliseconds() -> u32 {
    match sim_clock::scale() {
        Some(scale) => scale.milliseconds(imp::counter_ticks()),
        None => imp::milliseconds(),
    }
}

#[no_mangle]
pub extern "C" fn PlatTimeMilliseconds64() -> u64 {
    match sim_clock::scale() {
        Some(scale) => scale.milliseconds64(imp::counter_ticks()),
        None => imp::milliseconds64(),
    }
}

#[no_mangle]
pub extern "C" fn PlatCounterFrequency() -> i64 {
    imp::counter_frequency()
}

#[no_mangle]
pub extern "C" fn PlatCounterTicks() -> i64 {
    match sim_clock::scale() {
        Some(scale) => scale.counter(imp::counter_ticks()),
        None => imp::counter_ticks(),
    }
}

#[no_mangle]
pub extern "C" fn PlatSleep(milliseconds: u32) {
    match sim_clock::real_timeout_nanoseconds(milliseconds) {
        Some(nanoseconds) => imp::sleep_nanoseconds(nanoseconds),
        None => imp::sleep(milliseconds),
    }
}

/// `PlatSimSetTimeFactor` (platform/PlatSim.h, TEST ONLY): the harness clock runs `num`/`den` times real time from
/// now on -> 1; 0 for a factor outside 1/1000..4096, and nothing changes.
#[no_mangle]
pub extern "C" fn PlatSimSetTimeFactor(num: u32, den: u32) -> c_int {
    let real = (imp::counter_ticks(), imp::counter_frequency());
    let now = (PlatCounterTicks(), PlatTimeMilliseconds(), PlatTimeMilliseconds64());
    sim_clock::set(num, den, real, now) as c_int
}

/// `PlatSimGetTimeFactor`: the factor in force (1/1 when none was set).
#[no_mangle]
pub unsafe extern "C" fn PlatSimGetTimeFactor(num: *mut u32, den: *mut u32) {
    let (n, d) = sim_clock::scale().map_or((1, 1), |s| (s.num, s.den));
    if !num.is_null() {
        *num = n;
    }
    if !den.is_null() {
        *den = d;
    }
}

/// A wait of the platform layer (ee_plat_sys: PlatEventWait, PlatEventWaitAny, PlatThreadWait) under the harness
/// clock: WaitForMultipleObjects (any) on `handles` with the timeout scaled -> its Win32 result. None when no factor
/// is set or the timeout is 0 or INFINITE: the caller then makes its own Win32 wait, exactly as without the harness.
#[cfg(windows)]
pub unsafe fn sim_wait(handles: &[*mut c_void], timeout_ms: u32) -> Option<u32> {
    let nanoseconds = sim_clock::real_timeout_nanoseconds(timeout_ms)?;
    imp::wait_nanoseconds(handles, nanoseconds)
}

#[no_mangle]
pub extern "C" fn PlatTimeBeginPeriod(milliseconds: u32) {
    imp::begin_period(milliseconds)
}

#[no_mangle]
pub extern "C" fn PlatTimeEndPeriod(milliseconds: u32) {
    imp::end_period(milliseconds)
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetLocalTime(out: *mut PlatDateTime) {
    imp::get_local_time(out)
}

#[no_mangle]
pub extern "C" fn PlatTimeWallClock100ns() -> u64 {
    imp::wall_clock_100ns()
}

#[no_mangle]
pub unsafe extern "C" fn PlatTimerStartCallback(
    interval_ms: u32,
    periodic: c_int,
    callback: PlatTimerCallback,
    user: *mut c_void,
) -> u32 {
    imp::start_callback(interval_ms, periodic, callback, user)
}

#[no_mangle]
pub unsafe extern "C" fn PlatTimerStartEvent(interval_ms: u32, periodic: c_int, event: *mut PlatEvent) -> u32 {
    imp::start_event(interval_ms, periodic, event)
}

#[no_mangle]
pub extern "C" fn PlatTimerStop(id: u32) -> c_int {
    imp::stop(id)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};

    #[test]
    fn date_time_layout_is_the_headers() {
        assert_eq!(std::mem::size_of::<PlatDateTime>(), 16);
        assert_eq!(std::mem::offset_of!(PlatDateTime, milliseconds), 14);
    }

    #[test]
    fn filetime_epoch_conversion() {
        // golden from the C++ formula: 1970-01-01 is 116444736000000000 in FILETIME units
        assert_eq!(filetime_of_unix(0, 0), 116_444_736_000_000_000);
        assert_eq!(filetime_of_unix(1, 500), 116_444_736_010_000_005);
        assert_eq!(filetime_of_unix(0, 99), 116_444_736_000_000_000);
    }

    #[test]
    fn periodic_due_times_keep_their_phase_without_catch_ups() {
        assert_eq!(next_due(100, 10, 105), 110);
        assert_eq!(next_due(100, 10, 110), 110);
        assert_eq!(next_due(100, 10, 111), 121); // behind: from now
    }

    /// Runs `body` with the harness clock at `num`/`den`, then back to real time.
    fn with_factor(num: u32, den: u32, body: impl FnOnce()) {
        let _guard = crate::test_globals();
        assert_eq!(PlatSimSetTimeFactor(num, den), 1);
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(body));
        sim_clock::reset();
        if let Err(panic) = result {
            std::panic::resume_unwind(panic);
        }
    }

    fn real_ms(start: std::time::Instant) -> u128 {
        start.elapsed().as_millis()
    }

    #[test]
    fn the_harness_clock_runs_factor_times_faster_and_sleeps_shorter() {
        with_factor(20, 1, || {
            let mut num = 0;
            let mut den = 0;
            unsafe { PlatSimGetTimeFactor(&mut num, &mut den) };
            assert_eq!((num, den), (20, 1));
            let (ms, ms64, ticks) = (PlatTimeMilliseconds(), PlatTimeMilliseconds64(), PlatCounterTicks());
            let start = std::time::Instant::now();
            PlatSleep(1000); // 50 real ms
            let real = real_ms(start);
            assert!((40..600).contains(&real), "{real} real ms");
            let virtual_ms = PlatTimeMilliseconds().wrapping_sub(ms);
            assert!(virtual_ms >= 950 && u128::from(virtual_ms) <= (real + 20) * 20, "{virtual_ms} virtual ms");
            assert!(PlatTimeMilliseconds64() - ms64 >= 950);
            assert!((PlatCounterTicks() - ticks) * 1000 / PlatCounterFrequency() >= 950);
        });
    }

    #[test]
    fn a_new_factor_continues_the_clocks_without_a_jump() {
        with_factor(1000, 1, || {
            PlatSleep(2000); // 2 real ms
            let before = PlatTimeMilliseconds64();
            assert_eq!(PlatSimSetTimeFactor(1, 1), 1);
            let after = PlatTimeMilliseconds64();
            assert!(after >= before && after - before < 50, "{before} -> {after}");
            assert_eq!(PlatSimSetTimeFactor(0, 1), 0, "refused");
            assert_eq!(PlatSimSetTimeFactor(5000, 1), 0, "refused");
        });
    }

    static SCALED_TICKS: AtomicU32 = AtomicU32::new(0);
    unsafe extern "C" fn scaled_tick(_user: *mut c_void) {
        SCALED_TICKS.fetch_add(1, Ordering::SeqCst);
    }

    #[test]
    fn timers_under_a_factor_fire_at_the_scaled_rate_until_stopped() {
        with_factor(50, 1, || {
            SCALED_TICKS.store(0, Ordering::SeqCst);
            // 25 virtual ms = 0.5 real ms: 200 real ms are 400 periods (a loaded machine reaches fewer)
            let id = unsafe { PlatTimerStartCallback(25, 1, Some(scaled_tick), std::ptr::null_mut()) };
            assert_ne!(id, 0);
            std::thread::sleep(std::time::Duration::from_millis(200));
            assert_eq!(PlatTimerStop(id), 1);
            let count = SCALED_TICKS.load(Ordering::SeqCst);
            assert!((40..=420).contains(&count), "{count} ticks: more than a 1 ms timer could fire");
            std::thread::sleep(std::time::Duration::from_millis(30));
            assert!(SCALED_TICKS.load(Ordering::SeqCst) - count <= 1, "stopped");
            assert_eq!(PlatTimerStop(id), 0);
            // a one-shot timer fires once and its id is gone
            SCALED_TICKS.store(0, Ordering::SeqCst);
            let id = unsafe { PlatTimerStartCallback(100, 0, Some(scaled_tick), std::ptr::null_mut()) };
            std::thread::sleep(std::time::Duration::from_millis(100));
            assert_eq!(SCALED_TICKS.load(Ordering::SeqCst), 1);
            assert_eq!(PlatTimerStop(id), 0);
        });
    }

    #[cfg(windows)]
    #[test]
    fn scaled_waits_time_out_early_and_still_see_their_events() {
        use windows_sys::Win32::Foundation::{CloseHandle, WAIT_OBJECT_0, WAIT_TIMEOUT};
        use windows_sys::Win32::System::Threading::{CreateEventA, SetEvent};
        assert!(unsafe { sim_wait(&[std::ptr::null_mut()], 100) }.is_none(), "no factor: the caller waits");
        with_factor(100, 1, || unsafe {
            let event = CreateEventA(std::ptr::null(), 0, 0, std::ptr::null());
            let start = std::time::Instant::now();
            assert_eq!(sim_wait(&[event], 5000), Some(WAIT_TIMEOUT)); // 50 real ms
            let real = real_ms(start);
            assert!((40..1000).contains(&real), "{real} real ms");
            SetEvent(event);
            assert_eq!(sim_wait(&[event], 5000), Some(WAIT_OBJECT_0));
            assert!(sim_wait(&[event], 0).is_none() && sim_wait(&[event], u32::MAX).is_none(), "0 and INFINITE");
            // an event timer under the factor
            let id = PlatTimerStartEvent(1000, 0, event);
            assert_ne!(id, 0);
            assert_eq!(sim_wait(&[event], 100_000), Some(WAIT_OBJECT_0), "set after 10 real ms");
            CloseHandle(event);
        });
    }

    #[test]
    fn clocks_advance_with_a_sleep() {
        let _guard = crate::test_globals();
        assert!(PlatCounterFrequency() > 0);
        let (ms, ms64, ticks) = (PlatTimeMilliseconds(), PlatTimeMilliseconds64(), PlatCounterTicks());
        PlatSleep(60);
        let elapsed = PlatTimeMilliseconds().wrapping_sub(ms);
        assert!((40..2000).contains(&elapsed), "{elapsed} ms");
        assert!((40..2000).contains(&(PlatTimeMilliseconds64() - ms64)));
        assert!((PlatCounterTicks() - ticks) * 1000 / PlatCounterFrequency() >= 40);
        PlatSleep(0);
        PlatTimeBeginPeriod(1);
        PlatTimeEndPeriod(1);
    }

    #[test]
    fn local_and_wall_clock() {
        let mut now = PlatDateTime::default();
        unsafe { PlatGetLocalTime(&mut now) };
        assert!(now.year >= 2024 && (1..=12).contains(&now.month) && (1..=31).contains(&now.day));
        assert!(
            now.day_of_week <= 6 && now.hour <= 23 && now.minute <= 59 && now.second <= 59 && now.milliseconds <= 999
        );
        // a FILETIME after 2020-01-01
        let wall = PlatTimeWallClock100ns();
        assert!(wall > 132_223_104_000_000_000);
    }

    static TICKS: AtomicU32 = AtomicU32::new(0);
    unsafe extern "C" fn tick(user: *mut c_void) {
        assert_eq!(user as usize, 0x1234);
        TICKS.fetch_add(1, Ordering::SeqCst);
    }

    #[test]
    fn periodic_callback_timers_tick_until_stopped() {
        let _guard = crate::test_globals();
        TICKS.store(0, Ordering::SeqCst);
        let id = unsafe { PlatTimerStartCallback(10, 1, Some(tick), 0x1234 as *mut c_void) };
        assert_ne!(id, 0);
        PlatSleep(200);
        assert_eq!(PlatTimerStop(id), 1);
        let count = TICKS.load(Ordering::SeqCst);
        assert!((5..=40).contains(&count), "{count} ticks");
        PlatSleep(60);
        assert!(TICKS.load(Ordering::SeqCst) - count <= 1, "stopped (one may have been in flight)");
        assert_eq!(PlatTimerStop(id), 0, "not a running timer any more");
    }

    #[test]
    fn a_one_shot_callback_fires_once_and_is_gone() {
        let _guard = crate::test_globals();
        TICKS.store(0, Ordering::SeqCst);
        let id = unsafe { PlatTimerStartCallback(5, 0, Some(tick), 0x1234 as *mut c_void) };
        assert_ne!(id, 0);
        PlatSleep(200);
        assert_eq!(TICKS.load(Ordering::SeqCst), 1);
        assert_eq!(PlatTimerStop(id), 0);
    }

    #[test]
    fn stop_of_an_unknown_timer_fails() {
        assert_eq!(PlatTimerStop(0xFFFF_FFF0), 0);
    }

    #[cfg(unix)]
    mod unix {
        use super::*;
        use crate::test_seam::EVENTS_SET;

        #[test]
        fn null_arguments_are_refused() {
            assert_eq!(unsafe { PlatTimerStartCallback(1, 0, None, std::ptr::null_mut()) }, 0);
            assert_eq!(unsafe { PlatTimerStartEvent(1, 0, std::ptr::null_mut()) }, 0);
        }

        #[test]
        fn event_timers_set_the_event() {
            let _guard = crate::test_globals();
            let before = EVENTS_SET.load(Ordering::SeqCst);
            let id = unsafe { PlatTimerStartEvent(5, 0, 0x10 as *mut PlatEvent) };
            assert_ne!(id, 0);
            PlatSleep(100);
            assert_eq!(EVENTS_SET.load(Ordering::SeqCst), before + 1);
        }
    }

    #[cfg(windows)]
    mod windows {
        use super::*;
        use windows_sys::Win32::Foundation::{CloseHandle, WAIT_OBJECT_0};
        use windows_sys::Win32::System::Threading::{CreateEventA, WaitForSingleObject};

        #[test]
        fn event_timers_signal_the_event_handle() {
            let _guard = crate::test_globals();
            // SAFETY: valid arguments; the handle is closed at the end.
            unsafe {
                let event = CreateEventA(std::ptr::null(), 0, 0, std::ptr::null());
                assert!(!event.is_null());
                let id = PlatTimerStartEvent(10, 0, event);
                assert_ne!(id, 0);
                assert_eq!(WaitForSingleObject(event, 5000), WAIT_OBJECT_0);
                CloseHandle(event);
            }
        }
    }
}
