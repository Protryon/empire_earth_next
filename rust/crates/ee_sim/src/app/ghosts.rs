//! EEWorld's ghost units (game core lane, round 15; was the C++'s `EEWorld::mGhostUnits`, a
//! `std::vector<EEGhostUnit*>` at +0x9c0 whose standard-library size moved every later member of the world): the
//! placement ghosts of the build gate and the scenario editor, in the order they were added. The ghosts are C++
//! objects (EEGhostUnit); the list holds their addresses. The ghosts' own lock (`mGhostUnitsLock`) stays the callers'
//! to hold around a walk, as before; the list's mutex only keeps each call whole.

use core::ffi::c_void;
use std::sync::{Mutex, MutexGuard};

static GHOSTS: Mutex<Vec<usize>> = Mutex::new(Vec::new());

fn ghosts() -> MutexGuard<'static, Vec<usize>> {
    GHOSTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The number of ghosts (`EEWorld::GetNumGhostUnits`).
pub fn count() -> usize {
    ghosts().len()
}

/// The ghost `i` (an `EEGhostUnit*`), 0 past the end.
pub fn at(i: usize) -> usize {
    ghosts().get(i).copied().unwrap_or(0)
}

/// `mGhostUnits.size()`.
#[no_mangle]
pub extern "C" fn ee_app_ghost_count() -> usize {
    count()
}

/// `mGhostUnits[i]`; NULL past the end.
#[no_mangle]
pub extern "C" fn ee_app_ghost_at(i: usize) -> *mut c_void {
    at(i) as *mut c_void
}

/// `mGhostUnits.push_back(ghost)` (EEWorld::AddGhostUnit).
#[no_mangle]
pub extern "C" fn ee_app_ghost_push(ghost: *mut c_void) {
    ghosts().push(ghost as usize);
}

/// `mGhostUnits.back()` then `pop_back()`: the last ghost taken off the list (NULL when there is none).
#[no_mangle]
pub extern "C" fn ee_app_ghost_pop() -> *mut c_void {
    ghosts().pop().unwrap_or(0) as *mut c_void
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ghosts_keep_their_order_and_pop_from_the_back() {
        assert_eq!(ee_app_ghost_count(), 0);
        assert!(ee_app_ghost_pop().is_null());
        ee_app_ghost_push(0x10 as *mut c_void);
        ee_app_ghost_push(0x20 as *mut c_void);
        assert_eq!((count(), at(0), at(1), at(2)), (2, 0x10, 0x20, 0));
        assert_eq!(ee_app_ghost_pop() as usize, 0x20);
        assert_eq!(ee_app_ghost_pop() as usize, 0x10);
        assert_eq!(count(), 0);
    }
}
