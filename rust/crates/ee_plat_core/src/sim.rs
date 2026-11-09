//! The headless simulation's switches (`platform/PlatSim.h`): plain C variables the game reads and writes, and
//! `PlatDebugOutput`'s handler. TEST ONLY; all off in EE-AOC.exe.

use std::cell::UnsafeCell;
use std::ffi::{c_char, c_int, c_long};
use std::sync::atomic::{AtomicUsize, Ordering};

/// `PlatDebugHandler`: NULL is the default.
pub type PlatDebugHandler = Option<unsafe extern "C" fn(*const c_char)>;

/// A C variable: the same layout as `T`, with no synchronisation of its own (as the C++ `int` was).
#[repr(transparent)]
pub struct CVar<T>(UnsafeCell<T>);

// SAFETY: as the C++ globals these replace: the game sets them before the threads that read them start, or accepts a
// racy read of a flag.
unsafe impl<T> Sync for CVar<T> {}

impl<T: Copy> CVar<T> {
    pub(crate) const fn new(value: T) -> Self {
        Self(UnsafeCell::new(value))
    }

    /// The current value, read the way C reads a `volatile`.
    pub fn get(&self) -> T {
        // SAFETY: the pointer is valid for the life of the program; the C side writes it without a lock (see `Sync`).
        unsafe { self.0.get().read_volatile() }
    }

    /// Sets the value, written the way C writes a `volatile`.
    pub fn set(&self, value: T) {
        // SAFETY: as for `get`.
        unsafe { self.0.get().write_volatile(value) }
    }
}

#[no_mangle]
pub static gPlatSimMode: CVar<c_int> = CVar::new(0);
#[no_mangle]
pub static gPlatSimRun: CVar<c_int> = CVar::new(0);
#[no_mangle]
pub static gPlatSimAfterTick: CVar<Option<unsafe extern "C" fn()>> = CVar::new(None);
#[no_mangle]
pub static gPlatSimSeedSet: CVar<c_int> = CVar::new(0);
#[no_mangle]
pub static gPlatSimSeed: CVar<u32> = CVar::new(0);
#[no_mangle]
pub static gPlatSimNoHumanDefeat: CVar<c_int> = CVar::new(0);
#[no_mangle]
pub static gPlatSimPortOffset: CVar<c_int> = CVar::new(0);
#[no_mangle]
pub static gPlatSimNoHumanDefeatHits: CVar<c_long> = CVar::new(0);

static DEBUG_HANDLER: AtomicUsize = AtomicUsize::new(0);

#[no_mangle]
pub extern "C" fn PlatDebugSetHandler(handler: PlatDebugHandler) {
    DEBUG_HANDLER.store(handler.map_or(0, |f| f as usize), Ordering::SeqCst);
}

#[no_mangle]
pub extern "C" fn PlatDebugGetHandler() -> PlatDebugHandler {
    match DEBUG_HANDLER.load(Ordering::SeqCst) {
        0 => None,
        // SAFETY: only `PlatDebugSetHandler` stores here, and only such a function pointer.
        f => Some(unsafe { std::mem::transmute::<usize, unsafe extern "C" fn(*const c_char)>(f) }),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_globals;
    use std::mem::{align_of, size_of};
    use std::sync::atomic::AtomicU32;

    #[test]
    fn switches_start_off_and_have_c_layouts() {
        assert_eq!(gPlatSimMode.get(), 0);
        assert_eq!(gPlatSimRun.get(), 0);
        assert!(gPlatSimAfterTick.get().is_none());
        assert_eq!(gPlatSimSeed.get(), 0);
        assert_eq!(gPlatSimNoHumanDefeatHits.get(), 0);
        assert_eq!(size_of::<CVar<c_long>>(), size_of::<c_long>());
        assert_eq!(align_of::<CVar<c_int>>(), align_of::<c_int>());
        assert_eq!(size_of::<CVar<Option<unsafe extern "C" fn()>>>(), size_of::<usize>());
    }

    #[test]
    fn set_is_seen_by_get() {
        gPlatSimSeedSet.set(1);
        assert_eq!(gPlatSimSeedSet.get(), 1);
        gPlatSimSeedSet.set(0);
    }

    #[test]
    fn debug_handler_set_get_and_clear() {
        static CALLS: AtomicU32 = AtomicU32::new(0);
        unsafe extern "C" fn handler(_text: *const c_char) {
            CALLS.fetch_add(1, Ordering::SeqCst);
        }
        let _guard = test_globals();
        assert!(PlatDebugGetHandler().is_none());
        PlatDebugSetHandler(Some(handler));
        unsafe { PlatDebugGetHandler().unwrap()(c"x".as_ptr()) };
        assert_eq!(CALLS.load(Ordering::SeqCst), 1);
        PlatDebugSetHandler(None);
        assert!(PlatDebugGetHandler().is_none());
    }
}
