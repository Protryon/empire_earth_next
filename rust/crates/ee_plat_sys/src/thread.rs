//! PlatThread (`include/platform/PlatThread.h`): the last error, mutexes, events, waits, threads and atomics.

#[cfg(unix)]
#[path = "thread_posix.rs"]
pub mod imp;
#[cfg(windows)]
#[path = "thread_win32.rs"]
pub mod imp;

use std::sync::atomic::{AtomicI32, Ordering};

/// Atomic increment of a 32-bit value -> the new value (a full memory barrier).
///
/// # Safety
/// `value` must point to a live, aligned 32-bit integer accessed only through atomics while this runs.
#[no_mangle]
pub unsafe extern "C" fn PlatAtomicIncrement(value: *mut i32) -> i32 {
    AtomicI32::from_ptr(value).fetch_add(1, Ordering::SeqCst).wrapping_add(1)
}

/// Atomic decrement of a 32-bit value -> the new value (a full memory barrier).
///
/// # Safety
/// As `PlatAtomicIncrement`.
#[no_mangle]
pub unsafe extern "C" fn PlatAtomicDecrement(value: *mut i32) -> i32 {
    AtomicI32::from_ptr(value).fetch_sub(1, Ordering::SeqCst).wrapping_sub(1)
}

#[cfg(test)]
mod tests {
    use super::imp::*;
    use super::*;
    use crate::constants::*;
    use std::ffi::c_void;
    #[allow(unused_imports)]
    use std::ptr::{null, null_mut};
    use std::sync::atomic::AtomicU32;
    use std::time::{Duration, Instant};

    #[test]
    fn atomics_return_the_new_value() {
        let mut v = 41i32;
        unsafe {
            assert_eq!(PlatAtomicIncrement(&mut v), 42);
            assert_eq!(PlatAtomicDecrement(&mut v), 41);
            v = i32::MAX;
            assert_eq!(PlatAtomicIncrement(&mut v), i32::MIN); // wraps, as -fwrapv / Interlocked
        }
    }

    #[test]
    fn atomics_count_across_threads() {
        struct Shared(*mut i32);
        unsafe impl Send for Shared {}
        let mut v = 0i32;
        let p = &raw mut v;
        let handles: Vec<_> = (0..4)
            .map(|_| {
                let s = Shared(p);
                std::thread::spawn(move || {
                    let s = s;
                    for _ in 0..20000 {
                        unsafe { PlatAtomicIncrement(s.0) };
                    }
                    for _ in 0..5000 {
                        unsafe { PlatAtomicDecrement(s.0) };
                    }
                })
            })
            .collect();
        for h in handles {
            h.join().unwrap();
        }
        assert_eq!(v, 4 * 15000);
    }

    #[test]
    fn current_thread_id_is_nonzero_and_differs_per_thread() {
        let mine = unsafe { PlatCurrentThreadId() };
        assert_ne!(mine, 0);
        let other = std::thread::spawn(|| unsafe { PlatCurrentThreadId() }).join().unwrap();
        assert_ne!(other, 0);
        assert_ne!(mine, other);
    }

    #[test]
    fn mutex_is_recursive_and_excludes_other_threads() {
        unsafe {
            let m = PlatMutexCreate();
            assert!(!m.is_null());
            PlatMutexLock(m);
            PlatMutexLock(m);
            assert_eq!(PlatMutexTryLock(m), 1); // recursive
            struct Shared(*mut PlatMutex);
            unsafe impl Send for Shared {}
            let s = Shared(m);
            let held = std::thread::spawn(move || {
                let s = s;
                PlatMutexTryLock(s.0)
            })
            .join()
            .unwrap();
            assert_eq!(held, 0);
            PlatMutexUnlock(m);
            PlatMutexUnlock(m);
            // one lock still held
            let s = Shared(m);
            assert_eq!(
                std::thread::spawn(move || {
                    let s = s;
                    PlatMutexTryLock(s.0)
                })
                .join()
                .unwrap(),
                0
            );
            PlatMutexUnlock(m);
            let s = Shared(m);
            let free = std::thread::spawn(move || {
                let s = s;
                let r = PlatMutexTryLock(s.0);
                if r != 0 {
                    PlatMutexUnlock(s.0);
                }
                r
            })
            .join()
            .unwrap();
            assert_eq!(free, 1);
            PlatMutexDestroy(m);
            PlatMutexDestroy(null_mut()); // NULL is ignored
        }
    }

    #[cfg(windows)]
    #[test]
    fn mutex_is_a_critical_section() {
        // the harness casts a PlatMutex* to a CRITICAL_SECTION* (tools/harness/human/human_input.c)
        use windows_sys::Win32::System::Threading::{EnterCriticalSection, LeaveCriticalSection, CRITICAL_SECTION};
        unsafe {
            let m = PlatMutexCreate();
            assert_eq!(std::mem::size_of::<PlatMutex>(), std::mem::size_of::<CRITICAL_SECTION>());
            EnterCriticalSection(m.cast());
            PlatMutexUnlock(m);
            PlatMutexLock(m);
            LeaveCriticalSection(m.cast());
            PlatMutexDestroy(m);
        }
    }

    #[test]
    fn auto_reset_event_releases_one_wait_and_clears() {
        unsafe {
            let e = PlatEventCreate(0, 1);
            assert!(!e.is_null());
            assert_eq!(PlatEventWait(e, 0), 0);
            assert_eq!(PlatEventWait(e, 0), PLAT_WAIT_TIMEOUT); // cleared by the wait it released
            let t0 = Instant::now();
            assert_eq!(PlatEventWait(e, 60), PLAT_WAIT_TIMEOUT);
            assert!(
                t0.elapsed() >= Duration::from_millis(40),
                "the timeout was waited out (Windows may return a timer tick early)"
            );
            assert_eq!(PlatEventSet(e), 1);
            assert_eq!(PlatEventReset(e), 1);
            assert_eq!(PlatEventWait(e, 0), PLAT_WAIT_TIMEOUT);
            PlatEventDestroy(e);
        }
    }

    #[test]
    fn manual_reset_event_stays_set() {
        unsafe {
            let e = PlatEventCreate(1, 0);
            assert_eq!(PlatEventWait(e, 0), PLAT_WAIT_TIMEOUT);
            assert_eq!(PlatEventSet(e), 1);
            assert_eq!(PlatEventWait(e, 0), 0);
            assert_eq!(PlatEventWait(e, 0), 0);
            assert_eq!(PlatEventReset(e), 1);
            assert_eq!(PlatEventWait(e, 0), PLAT_WAIT_TIMEOUT);
            PlatEventDestroy(e);
        }
    }

    #[test]
    fn wait_any_returns_the_lowest_set_event() {
        unsafe {
            let auto = PlatEventCreate(0, 1);
            let manual = PlatEventCreate(1, 1);
            let third = PlatEventCreate(0, 1);
            let events = [auto, manual, third];
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 3, 0), 0);
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 3, 0), 1); // manual stays set, lowest of the rest
            assert_eq!(PlatEventReset(manual), 1);
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 3, 0), 2);
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 3, 0), PLAT_WAIT_TIMEOUT);
            for e in events {
                PlatEventDestroy(e);
            }
        }
    }

    #[test]
    fn wait_any_rejects_bad_counts() {
        unsafe {
            let e = PlatEventCreate(0, 1);
            let events = [e];
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 0, 0), PLAT_WAIT_FAILED);
            assert_eq!(PlatGetLastError(), 87); // ERROR_INVALID_PARAMETER
            assert_eq!(PlatEventWaitAny(events.as_ptr(), PLAT_WAIT_MAX + 1, 0), PLAT_WAIT_FAILED);
            assert_eq!(PlatGetLastError(), 87);
            PlatEventDestroy(e);
        }
    }

    #[test]
    fn set_from_another_thread_wakes_a_waiter() {
        unsafe {
            let e = PlatEventCreate(0, 0);
            struct Shared(*mut PlatEvent);
            unsafe impl Send for Shared {}
            let s = Shared(e);
            let setter = std::thread::spawn(move || {
                let s = s;
                std::thread::sleep(Duration::from_millis(30));
                PlatEventSet(s.0)
            });
            assert_eq!(PlatEventWait(e, 10_000), 0);
            assert_eq!(setter.join().unwrap(), 1);
            PlatEventDestroy(e);
        }
    }

    static STARTED: AtomicU32 = AtomicU32::new(0);

    unsafe extern "C" fn mark_started(_: *mut c_void) -> u32 {
        STARTED.store(1, Ordering::SeqCst);
        42
    }

    #[test]
    fn suspended_thread_runs_only_after_resume() {
        unsafe {
            STARTED.store(0, Ordering::SeqCst);
            let mut id = 0u32;
            let t = PlatThreadCreate(mark_started, null_mut(), 0, 1, &mut id);
            assert!(!t.is_null());
            assert_ne!(id, 0);
            std::thread::sleep(Duration::from_millis(30));
            assert_eq!(STARTED.load(Ordering::SeqCst), 0); // held
            let mut code = 0u32;
            assert_eq!(PlatThreadGetExitCode(t, &mut code), 1);
            assert_eq!(code, PLAT_THREAD_STILL_ACTIVE);
            assert_eq!(PlatThreadSetPriority(t, PLAT_PRIORITY_BELOW_NORMAL), 1);
            assert_eq!(PlatThreadGetPriority(t), PLAT_PRIORITY_BELOW_NORMAL);
            assert_eq!(PlatThreadWait(t, 0), PLAT_WAIT_TIMEOUT);
            assert_eq!(PlatThreadWait(t, 30), PLAT_WAIT_TIMEOUT);
            assert_eq!(PlatThreadResume(t), 1);
            assert_eq!(PlatThreadWait(t, 5000), 0);
            assert_eq!(PlatThreadWait(t, 0), 0); // stays ended
            assert_eq!(PlatThreadGetExitCode(t, &mut code), 1);
            assert_eq!(code, 42);
            assert_eq!(STARTED.load(Ordering::SeqCst), 1);
            PlatThreadClose(t);
        }
    }

    unsafe extern "C" fn return_argument(p: *mut c_void) -> u32 {
        p as usize as u32
    }

    #[test]
    fn thread_exit_code_is_the_return_value_and_a_stack_size_is_accepted() {
        unsafe {
            let mut id = 0u32;
            let t = PlatThreadCreate(return_argument, 7usize as *mut c_void, 64 * 1024, 0, &mut id);
            assert!(!t.is_null());
            assert_ne!(id, PlatCurrentThreadId());
            assert_eq!(PlatThreadWait(t, 10_000), 0);
            let mut code = 0u32;
            assert_eq!(PlatThreadGetExitCode(t, &mut code), 1);
            assert_eq!(code, 7);
            PlatThreadClose(t);
        }
    }

    #[test]
    fn open_current_is_a_live_handle() {
        unsafe {
            let t = PlatThreadOpenCurrent();
            assert!(!t.is_null());
            assert_ne!(PlatThreadGetPriority(t), PLAT_PRIORITY_ERROR);
            let mut code = 0u32;
            assert_eq!(PlatThreadGetExitCode(t, &mut code), 1);
            assert_eq!(code, PLAT_THREAD_STILL_ACTIVE);
            assert_eq!(PlatThreadWait(t, 0), PLAT_WAIT_TIMEOUT); // this thread has not ended
            PlatThreadClose(t);
        }
    }

    #[cfg(unix)]
    #[test]
    fn invalid_handles_fail_with_error_invalid_handle() {
        unsafe {
            assert_eq!(PlatEventSet(null_mut()), 0);
            assert_eq!(PlatGetLastError(), 6);
            assert_eq!(PlatEventReset(null_mut()), 0);
            let events: [*mut PlatEvent; 1] = [null_mut()];
            assert_eq!(PlatEventWaitAny(events.as_ptr(), 1, 0), PLAT_WAIT_FAILED);
            assert_eq!(PlatGetLastError(), 6);
            assert_eq!(PlatThreadWait(null_mut(), 0), PLAT_WAIT_FAILED);
            assert_eq!(PlatThreadGetPriority(null_mut()), PLAT_PRIORITY_ERROR);
            assert_eq!(PlatThreadSuspend(null_mut()), 0xFFFF_FFFF);
            assert_eq!(PlatThreadResume(null_mut()), 0xFFFF_FFFF);
            let mut code = 0u32;
            assert_eq!(PlatThreadGetExitCode(null_mut(), &mut code), 0);
            assert_eq!(PlatThreadSetPriority(null_mut(), 0), 0);
            PlatThreadClose(null_mut());
            let _: *const c_void = null();
        }
    }

    #[cfg(unix)]
    #[test]
    fn set_priority_rejects_out_of_range_values() {
        unsafe {
            let t = PlatThreadOpenCurrent();
            assert_eq!(PlatThreadSetPriority(t, 16), 0);
            assert_eq!(PlatGetLastError(), 87);
            assert_eq!(PlatThreadSetPriority(t, -16), 0);
            PlatThreadClose(t);
        }
    }

    #[cfg(unix)]
    #[test]
    fn suspending_a_running_thread_fails_and_terminate_is_unsupported() {
        unsafe {
            let t = PlatThreadOpenCurrent();
            assert_eq!(PlatThreadSuspend(t), 0xFFFF_FFFF);
            assert_eq!(PlatGetLastError(), 50); // ERROR_NOT_SUPPORTED
            assert_eq!(PlatThreadTerminate(t), 0);
            assert_eq!(PlatGetLastError(), 50);
            PlatThreadClose(t);
        }
    }

    #[cfg(unix)]
    #[test]
    fn extra_suspends_of_a_held_thread_need_as_many_resumes() {
        unsafe {
            let mut id = 0u32;
            let t = PlatThreadCreate(return_argument, 9usize as *mut c_void, 0, 1, &mut id);
            assert_eq!(PlatThreadSuspend(t), 1); // the count before the call
            assert_eq!(PlatThreadResume(t), 2);
            assert_eq!(PlatThreadWait(t, 30), PLAT_WAIT_TIMEOUT);
            assert_eq!(PlatThreadResume(t), 1);
            assert_eq!(PlatThreadWait(t, 5000), 0);
            PlatThreadClose(t);
        }
    }

    #[cfg(windows)]
    #[test]
    fn com_is_initialised_on_platform_threads() {
        // PlatThreadCreate promises COM around inMain; a second CoInitialize on the thread then says S_FALSE (1)
        use windows_sys::Win32::System::Com::{CoInitialize, CoUninitialize};
        unsafe extern "C" fn second_init(_: *mut c_void) -> u32 {
            let r = CoInitialize(null());
            if r >= 0 {
                CoUninitialize();
            }
            r as u32
        }
        unsafe {
            let mut id = 0u32;
            let t = PlatThreadCreate(second_init, null_mut(), 0, 0, &mut id);
            assert_eq!(PlatThreadWait(t, 10_000), 0);
            let mut code = 2u32;
            assert_eq!(PlatThreadGetExitCode(t, &mut code), 1);
            assert_eq!(code, 1, "S_FALSE: already initialised");
            PlatThreadClose(t);
        }
    }

    #[cfg(windows)]
    #[test]
    fn wait_result_maps_win32_codes() {
        use super::imp::wait_result;
        assert_eq!(wait_result(258, 1), PLAT_WAIT_TIMEOUT); // WAIT_TIMEOUT
        assert_eq!(wait_result(0xFFFF_FFFF, 1), PLAT_WAIT_FAILED);
        assert_eq!(wait_result(0, 1), 0);
        assert_eq!(wait_result(2, 3), 2);
        assert_eq!(wait_result(3, 3), PLAT_WAIT_FAILED); // out of range
        assert_eq!(wait_result(0x80, 1), 0); // WAIT_ABANDONED_0
        assert_eq!(wait_result(0x82, 3), 2);
    }

    #[cfg(windows)]
    #[test]
    fn thread_operations_on_windows_keep_win32_semantics() {
        unsafe {
            let t = PlatThreadOpenCurrent();
            // a resume of a thread that is not suspended returns the previous count 0
            assert_eq!(PlatThreadResume(t), 0);
            PlatThreadClose(t);
            assert_eq!(PlatThreadWait(null_mut(), 0), PLAT_WAIT_FAILED);
            assert_ne!(PlatGetLastError(), 0);
            let bogus: [*mut PlatEvent; 1] = [null_mut()];
            assert_eq!(PlatEventWaitAny(bogus.as_ptr(), 1, 0), PLAT_WAIT_FAILED);
            let _: *const c_void = null();
        }
    }
}
