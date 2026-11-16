//! WONCommon's `PerfTimer` (net round 26; PerfTimer.cpp is deleted, WONPerfTimer.h forwards here): a global timing
//! run (`GlobalStart` 0x2a3940, `GlobalStop` 0x2a3990) and the time the timers counted inside it (`GetCountedTime`
//! 0x2a3820, `GetElapsedTime` 0x2a3900, `GetPercentTime` 0x2a3910). Unnamed timers nest: the outermost one's start
//! tick is taken, and only when the last one stops, inside a run, is its time added. The original read the CPU's
//! time-stamp counter calibrated against a sleep; the port reads the platform's high-resolution counter (its rate
//! cached once known, as the original's calibration was). Named timers do nothing (the SDK's).

use std::sync::Mutex;

/// The globals (the original's RVA 565ec0..565ee4 and the outermost start tick).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct State {
    /// `gPerfTotalTime`: counter ticks counted
    pub total: i64,
    /// `gPerfResetTick`
    pub reset_tick: i64,
    /// `gPerfElapsedTime`: milliseconds of the last run
    pub elapsed: f64,
    /// `gPerfTimerStartCount`
    pub start_count: i32,
    /// `gPerfTimerStarted`
    pub started: bool,
    /// the outermost running timer's start tick
    pub start_tick: i64,
    /// the cached counter rate (0: not known yet)
    pub frequency: i64,
}

impl State {
    /// `MyQueryPerformanceFrequency`: the rate, cached once nonzero.
    fn rate(&mut self, query: impl Fn() -> i64) -> i64 {
        if self.frequency == 0 {
            self.frequency = query();
        }
        self.frequency
    }
    /// `GetCountedTime`: the counted milliseconds (raw ticks while the rate is unknown).
    pub fn counted(&mut self, query: impl Fn() -> i64) -> f64 {
        let f = self.rate(query);
        if f == 0 {
            return self.total as f64;
        }
        self.total as f64 / f as f64 * 1000.0
    }
    /// `GetPercentTime`: the counted time over the elapsed time, 0 when nothing elapsed.
    pub fn percent(&mut self, query: impl Fn() -> i64) -> f64 {
        if self.elapsed == 0.0 {
            return 0.0;
        }
        self.counted(query) / self.elapsed
    }
    /// `GlobalStart`: the counted time cleared, the start tick noted.
    pub fn global_start(&mut self, tick: i64) {
        self.started = true;
        self.total = 0;
        self.start_count = 0;
        self.elapsed = 0.0;
        self.reset_tick = tick;
    }
    /// `GlobalStop`: the run's elapsed milliseconds (at the counter's rate read now).
    pub fn global_stop(&mut self, tick: i64, frequency: i64) {
        self.elapsed = (tick - self.reset_tick) as f64 / frequency as f64 * 1000.0;
        self.started = false;
    }
    /// `PerfTimer::Start` of an unnamed timer not running: the outermost one notes its start tick.
    pub fn start(&mut self, named: bool, running: &mut bool, tick: impl Fn() -> i64) {
        if named || *running {
            return;
        }
        *running = true;
        self.start_count += 1;
        if self.start_count == 1 {
            self.start_tick = tick();
        }
    }
    /// `PerfTimer::Stop` of a running unnamed timer: the last one inside a run adds its time.
    pub fn stop(&mut self, named: bool, running: &mut bool, tick: impl Fn() -> i64) {
        if named || !*running {
            return;
        }
        self.start_count -= 1;
        if self.start_count <= 0 && self.started {
            self.total += tick() - self.start_tick;
            self.start_count = 0;
        }
        *running = false;
    }
}

static STATE: Mutex<State> = Mutex::new(State {
    total: 0,
    reset_tick: 0,
    elapsed: 0.0,
    start_count: 0,
    started: false,
    start_tick: 0,
    frequency: 0,
});

fn ticks() -> i64 {
    ee_plat_core::time::PlatCounterTicks()
}
fn frequency() -> i64 {
    ee_plat_core::time::PlatCounterFrequency()
}
fn with<R>(f: impl FnOnce(&mut State) -> R) -> R {
    f(&mut STATE.lock().unwrap_or_else(|e| e.into_inner()))
}

/// `PerfTimer::GetCountedTime()`.
#[no_mangle]
pub extern "C" fn ee_won_perf_counted_time() -> f64 {
    with(|s| s.counted(frequency))
}
/// `PerfTimer::GetElapsedTime()`.
#[no_mangle]
pub extern "C" fn ee_won_perf_elapsed_time() -> f64 {
    with(|s| s.elapsed)
}
/// `PerfTimer::GetPercentTime()`.
#[no_mangle]
pub extern "C" fn ee_won_perf_percent_time() -> f64 {
    with(|s| s.percent(frequency))
}
/// `PerfTimer::GlobalStart()`.
#[no_mangle]
pub extern "C" fn ee_won_perf_global_start() {
    let t = ticks();
    with(|s| s.global_start(t))
}
/// `PerfTimer::GlobalStop()`.
#[no_mangle]
pub extern "C" fn ee_won_perf_global_stop() {
    let t = ticks();
    let f = frequency();
    with(|s| s.global_stop(t, f))
}
/// `PerfTimer::Start()` of a timer (`named`: it has a name; `running`: its `mStarted`).
///
/// # Safety
/// `running` is the timer's flag.
#[no_mangle]
pub unsafe extern "C" fn ee_won_perf_start(named: bool, running: *mut bool) {
    // SAFETY: the caller's flag
    with(|s| s.start(named, unsafe { &mut *running }, ticks))
}
/// `PerfTimer::Stop()` of a timer.
///
/// # Safety
/// As `ee_won_perf_start`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_perf_stop(named: bool, running: *mut bool) {
    // SAFETY: the caller's flag
    with(|s| s.stop(named, unsafe { &mut *running }, ticks))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nested_timers_count_the_outermost_span_inside_a_run() {
        let mut s = State::default();
        let (mut a, mut b) = (false, false);
        // outside a run: nothing counted
        s.start(false, &mut a, || 100);
        s.stop(false, &mut a, || 200);
        assert_eq!((s.total, s.start_count, a), (0, 0, false));
        s.global_start(1000);
        s.start(false, &mut a, || 1100);
        s.start(false, &mut b, || 1150);
        s.start(false, &mut b, || 1160); // already running: nothing
        assert_eq!((s.start_count, s.start_tick), (2, 1100));
        s.stop(false, &mut b, || 1200);
        assert_eq!(s.total, 0);
        s.stop(false, &mut a, || 1300);
        assert_eq!((s.total, s.start_count), (200, 0));
        s.stop(false, &mut a, || 1400); // not running: nothing
        assert_eq!(s.total, 200);
        s.global_stop(2000, 1000);
        assert_eq!((s.elapsed, s.started), (1000.0, false));
        assert_eq!(s.counted(|| 1000), 200.0);
        assert_eq!(s.percent(|| 1000), 0.2);
    }

    #[test]
    fn named_timers_do_nothing_and_an_unknown_rate_counts_ticks() {
        let mut s = State::default();
        let mut a = false;
        s.global_start(0);
        s.start(true, &mut a, || 5);
        assert_eq!((a, s.start_count), (false, 0));
        s.start(false, &mut a, || 5);
        s.stop(true, &mut a, || 9);
        assert!(a);
        s.stop(false, &mut a, || 9);
        assert_eq!(s.counted(|| 0), 4.0);
        // the rate cached once known
        assert_eq!(s.counted(|| 2), 2000.0);
        assert_eq!(s.counted(|| 4), 2000.0);
        assert_eq!(s.percent(|| 4), 0.0);
    }

    #[test]
    fn the_c_abi_runs_on_the_platform_counter() {
        let mut a = false;
        ee_won_perf_global_start();
        // SAFETY: a local flag
        unsafe {
            ee_won_perf_start(false, &mut a);
            ee_won_perf_stop(false, &mut a);
        }
        ee_won_perf_global_stop();
        assert!(!a);
        assert!(ee_won_perf_counted_time() >= 0.0);
        assert!(ee_won_perf_elapsed_time() >= ee_won_perf_counted_time());
        assert!(ee_won_perf_percent_time() >= 0.0);
    }
}
