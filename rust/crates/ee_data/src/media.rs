//! The media use count of the database objects that load their media on first use and release it with the last
//! user: `EEButtonObject`, `EESoundObject`, `EEGraphicObject`, `EETerrainTable` (its mesh textures), `EEMusicObject`
//! (its score) and `EEUnitClassUpgrade` (its media and its buttons). Each class repeated the same pattern around its
//! unit's database lock; here it is once.
//!
//! - **acquire**: under the lock, the first user sets the count to 1 and the loading flag, leaves the lock and loads;
//!   a failed load may reset the count (the buttons, the scores); back under the lock the `after` step runs (the
//!   terrain attaches its textures to the mesh) and the flag clears. A later user counts itself and waits (polling the
//!   flag, a 1 ms sleep each time) until the first load is done, then passes through the lock once.
//! - **release**: waits for a load to end, then under the lock the last user's `unload` step runs (it may keep the
//!   use: a score the conductor cannot unload yet) and the count goes to 0; an earlier user counts down.
//! - **locked**: waits for a load to end and runs a step under the lock (the setters that swap a loaded medium).
//!
//! The lock stays the C++'s (each unit's `sDatabaseLock`, a reentrant critical section: an unload releases other
//! objects' media under its own unit's lock), passed in as `EeDataMediaLock`; the steps are the C++'s.
#![allow(non_snake_case)]

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicU8, Ordering};

/// A use count and its loading flag (the classes' `mMediaCount` and `mIsLoading`: the classes' base).
#[repr(C)]
#[derive(Debug, Default)]
pub struct EeDataMediaUse {
    pub mMediaCount: c_ulong,
    /// Nonzero while the first user loads (other threads poll it).
    pub mIsLoading: u8,
}

/// The unit's lock and the platform's sleep.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeDataMediaLock {
    pub lock: *mut c_void,
    pub enter: extern "C" fn(lock: *mut c_void),
    pub leave: extern "C" fn(lock: *mut c_void),
    /// `PlatSleep(1)`.
    pub sleep: extern "C" fn(),
}

/// A C++ step: `call(context)` (a load's status; an unload's "released", nonzero).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeDataMediaStep {
    pub context: *mut c_void,
    pub call: Option<extern "C" fn(context: *mut c_void) -> c_long>,
}

impl EeDataMediaStep {
    fn run(&self) -> c_long {
        self.call.map_or(0, |f| f(self.context))
    }
}

/// The use's fields, reached through the C++ object's pointer.
struct Use(*mut EeDataMediaUse);

impl Use {
    fn loading(&self) -> &AtomicU8 {
        // SAFETY: the caller of the entry points hands a live use; the flag is one byte, AtomicU8's layout
        unsafe { AtomicU8::from_ptr(std::ptr::addr_of_mut!((*self.0).mIsLoading)) }
    }
    fn count(&self) -> c_ulong {
        // SAFETY: as above; read under the lock (other readers are the C++'s racy dumps)
        unsafe { std::ptr::read_volatile(std::ptr::addr_of!((*self.0).mMediaCount)) }
    }
    fn set_count(&self, count: c_ulong) {
        // SAFETY: as above, written under the lock
        unsafe { std::ptr::write_volatile(std::ptr::addr_of_mut!((*self.0).mMediaCount), count) }
    }
    fn wait(&self, lock: &EeDataMediaLock) {
        while self.loading().load(Ordering::Acquire) != 0 {
            (lock.sleep)();
        }
    }
}

/// The first use loads: see the module. Returns the load's status (0 for a later user).
pub fn acquire(
    usage: *mut EeDataMediaUse,
    lock: &EeDataMediaLock,
    load: &EeDataMediaStep,
    after: &EeDataMediaStep,
    reset_on_failure: bool,
) -> c_long {
    let u = Use(usage);
    let mut result = 0;
    (lock.enter)(lock.lock);
    if u.count() == 0 {
        u.set_count(1);
        u.loading().store(1, Ordering::Release);
        (lock.leave)(lock.lock);
        result = load.run();
        if result != 0 && reset_on_failure {
            (lock.enter)(lock.lock);
            u.set_count(0);
            (lock.leave)(lock.lock);
        }
        (lock.enter)(lock.lock);
        after.run();
        u.loading().store(0, Ordering::Release);
    } else {
        u.set_count(u.count().wrapping_add(1));
        (lock.leave)(lock.lock);
        u.wait(lock);
        (lock.enter)(lock.lock);
    }
    (lock.leave)(lock.lock);
    result
}

/// The last use unloads: see the module.
pub fn release(usage: *mut EeDataMediaUse, lock: &EeDataMediaLock, unload: &EeDataMediaStep) {
    let u = Use(usage);
    u.wait(lock);
    (lock.enter)(lock.lock);
    match u.count() {
        0 => {}
        1 => {
            if unload.run() != 0 {
                u.set_count(0);
            }
        }
        n => u.set_count(n - 1),
    }
    (lock.leave)(lock.lock);
}

/// Waits for a load to end and runs `step` under the lock.
pub fn locked(usage: *mut EeDataMediaUse, lock: &EeDataMediaLock, step: &EeDataMediaStep) {
    let u = Use(usage);
    u.wait(lock);
    (lock.enter)(lock.lock);
    step.run();
    (lock.leave)(lock.lock);
}

/// [`acquire`] for the C++.
///
/// # Safety
/// `usage` is a live use; `lock`, `load` and `after` are readable and their callbacks valid.
#[no_mangle]
pub unsafe extern "C" fn ee_data_media_acquire(
    usage: *mut EeDataMediaUse,
    lock: *const EeDataMediaLock,
    load: *const EeDataMediaStep,
    after: *const EeDataMediaStep,
    reset_on_failure: bool,
) -> c_long {
    // SAFETY: the caller's guarantee
    unsafe { acquire(usage, &*lock, &*load, &*after, reset_on_failure) }
}

/// [`release`] for the C++.
///
/// # Safety
/// As [`ee_data_media_acquire`].
#[no_mangle]
pub unsafe extern "C" fn ee_data_media_release(
    usage: *mut EeDataMediaUse,
    lock: *const EeDataMediaLock,
    unload: *const EeDataMediaStep,
) {
    // SAFETY: the caller's guarantee
    unsafe { release(usage, &*lock, &*unload) }
}

/// [`locked`] for the C++.
///
/// # Safety
/// As [`ee_data_media_acquire`].
#[no_mangle]
pub unsafe extern "C" fn ee_data_media_locked(
    usage: *mut EeDataMediaUse,
    lock: *const EeDataMediaLock,
    step: *const EeDataMediaStep,
) {
    // SAFETY: the caller's guarantee
    unsafe { locked(usage, &*lock, &*step) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    /// A recording lock: a reentrant one would be the C++'s; the tests' steps never nest.
    static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
    static GUARD: Mutex<()> = Mutex::new(());

    extern "C" fn enter(_: *mut c_void) {
        LOG.lock().unwrap().push("enter".into());
    }
    extern "C" fn leave(_: *mut c_void) {
        LOG.lock().unwrap().push("leave".into());
    }
    extern "C" fn sleep() {
        LOG.lock().unwrap().push("sleep".into());
    }
    extern "C" fn ok(_: *mut c_void) -> c_long {
        LOG.lock().unwrap().push("load".into());
        0
    }
    extern "C" fn fail(_: *mut c_void) -> c_long {
        LOG.lock().unwrap().push("load".into());
        9
    }
    extern "C" fn after(_: *mut c_void) -> c_long {
        LOG.lock().unwrap().push("after".into());
        0
    }
    extern "C" fn unload(_: *mut c_void) -> c_long {
        LOG.lock().unwrap().push("unload".into());
        1
    }
    extern "C" fn keep(_: *mut c_void) -> c_long {
        LOG.lock().unwrap().push("keep".into());
        0
    }

    fn lock() -> EeDataMediaLock {
        EeDataMediaLock { lock: std::ptr::null_mut(), enter, leave, sleep }
    }
    fn step(f: extern "C" fn(*mut c_void) -> c_long) -> EeDataMediaStep {
        EeDataMediaStep { context: std::ptr::null_mut(), call: Some(f) }
    }
    const NONE: EeDataMediaStep = EeDataMediaStep { context: std::ptr::null_mut(), call: None };

    fn take() -> String {
        std::mem::take(&mut *LOG.lock().unwrap()).join(" ")
    }

    #[test]
    fn the_first_use_loads_outside_the_lock_and_the_last_unloads_inside() {
        let _g = GUARD.lock().unwrap();
        take();
        let mut u = EeDataMediaUse::default();
        let l = lock();
        assert_eq!(acquire(&mut u, &l, &step(ok), &step(after), false), 0);
        assert_eq!(take(), "enter leave load enter after leave");
        assert_eq!((u.mMediaCount, u.mIsLoading), (1, 0));
        assert_eq!(acquire(&mut u, &l, &step(ok), &NONE, false), 0);
        assert_eq!(take(), "enter leave enter leave");
        assert_eq!(u.mMediaCount, 2);
        release(&mut u, &l, &step(unload));
        assert_eq!((take().as_str(), u.mMediaCount), ("enter leave", 1));
        release(&mut u, &l, &step(unload));
        assert_eq!((take().as_str(), u.mMediaCount), ("enter unload leave", 0));
        release(&mut u, &l, &step(unload));
        assert_eq!((take().as_str(), u.mMediaCount), ("enter leave", 0));
    }

    #[test]
    fn a_failed_load_resets_the_count_when_asked_and_a_kept_unload_keeps_it() {
        let _g = GUARD.lock().unwrap();
        take();
        let mut u = EeDataMediaUse::default();
        let l = lock();
        assert_eq!(acquire(&mut u, &l, &step(fail), &NONE, true), 9);
        assert_eq!(take(), "enter leave load enter leave enter leave");
        assert_eq!(u.mMediaCount, 0);
        assert_eq!(acquire(&mut u, &l, &step(fail), &NONE, false), 9);
        assert_eq!(u.mMediaCount, 1);
        take();
        release(&mut u, &l, &step(keep));
        assert_eq!((take().as_str(), u.mMediaCount), ("enter keep leave", 1));
    }

    #[test]
    fn a_later_user_waits_while_the_first_loads() {
        let _g = GUARD.lock().unwrap();
        take();
        let mut u = EeDataMediaUse { mMediaCount: 1, mIsLoading: 1 };
        let p = &mut u as *mut EeDataMediaUse as usize;
        let t = std::thread::spawn(move || {
            std::thread::sleep(std::time::Duration::from_millis(20));
            // SAFETY: the use outlives the thread (joined below)
            unsafe { AtomicU8::from_ptr(std::ptr::addr_of_mut!((*(p as *mut EeDataMediaUse)).mIsLoading)) }
                .store(0, Ordering::Release);
        });
        acquire(&mut u, &lock(), &step(ok), &NONE, false);
        t.join().unwrap();
        let log = take();
        assert!(log.starts_with("enter leave sleep") && log.ends_with("enter leave") && !log.contains("load"), "{log}");
        assert_eq!(u.mMediaCount, 2);
        locked(&mut u, &lock(), &step(after));
        assert_eq!(take(), "enter after leave");
    }
}
