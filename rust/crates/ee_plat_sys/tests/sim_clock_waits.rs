//! The waits under the harness clock (platform/PlatSim.h PlatSimSetTimeFactor). A test binary of its own: the factor
//! is process-wide, and the unit tests time real waits.

extern crate ee_plat_sys;

use std::ffi::c_void;
use std::time::{Duration, Instant};

const PLAT_WAIT_TIMEOUT: i32 = -1; // platform/PlatThread.h

extern "C" {
    fn PlatEventCreate(manual_reset: i32, initially_set: i32) -> *mut c_void;
    fn PlatEventDestroy(event: *mut c_void);
    fn PlatEventSet(event: *mut c_void) -> i32;
    fn PlatEventWait(event: *mut c_void, timeout_ms: u32) -> i32;
    fn PlatEventWaitAny(events: *const *mut c_void, count: u32, timeout_ms: u32) -> i32;
}

#[test]
fn finite_timeouts_last_one_factor_th_and_events_still_end_them() {
    unsafe {
        let (a, b) = (PlatEventCreate(0, 0), PlatEventCreate(0, 0));
        assert!(!a.is_null() && !b.is_null());
        assert_eq!(ee_plat_core::time::PlatSimSetTimeFactor(100, 1), 1);
        let start = Instant::now();
        assert_eq!(PlatEventWait(a, 5_000), PLAT_WAIT_TIMEOUT); // 50 real ms
        let real = start.elapsed();
        assert!(real >= Duration::from_millis(40) && real < Duration::from_millis(2_000), "{real:?}");
        let start = Instant::now();
        assert_eq!(PlatEventWaitAny([a, b].as_ptr(), 2, 3_000), PLAT_WAIT_TIMEOUT); // 30 real ms
        assert!(start.elapsed() < Duration::from_millis(2_000));
        assert_eq!(PlatEventSet(b), 1);
        assert_eq!(PlatEventWaitAny([a, b].as_ptr(), 2, 100_000), 1, "the set event, not the timer");
        assert_eq!(PlatEventSet(a), 1);
        assert_eq!(PlatEventWait(a, 0), 0, "a zero timeout polls as always");
        assert_eq!(PlatEventWait(a, 0), PLAT_WAIT_TIMEOUT);
        // a factor below 1 makes timeouts longer
        assert_eq!(ee_plat_core::time::PlatSimSetTimeFactor(1, 2), 1);
        let start = Instant::now();
        assert_eq!(PlatEventWait(a, 30), PLAT_WAIT_TIMEOUT);
        assert!(start.elapsed() >= Duration::from_millis(50), "{:?}", start.elapsed());
        assert_eq!(ee_plat_core::time::PlatSimSetTimeFactor(1, 1), 1);
        PlatEventDestroy(a);
        PlatEventDestroy(b);
    }
}
