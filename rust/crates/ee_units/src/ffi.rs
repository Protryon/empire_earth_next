//! The C ABI behind `RTSRegisterUnit` / `RTSUnregisterUnit` / `RTSResolveUnitID` (`src/RTS Engine/Units/RTSUnit.cpp`).
//! One process-wide registry; the C++ keeps the "no id" value (0xffffffff) out of it.

use crate::Registry;
use std::ffi::c_void;

static REGISTRY: Registry<c_void> = Registry::new();

/// Makes `id` resolve to `unit` (`RTSPlayer::InsertUnit`).
#[no_mangle]
pub extern "C" fn ee_units_register(id: u32, unit: *mut c_void) {
    REGISTRY.register(id, unit);
}

/// Clears `id`'s entry if it still names `unit` (`RTSUnit::FreeNode`); returns whether it did.
#[no_mangle]
pub extern "C" fn ee_units_unregister(id: u32, unit: *mut c_void) -> bool {
    REGISTRY.unregister(id, unit)
}

/// The object registered under `id`, or null (`EEUnitHandle<T>::Get`).
#[no_mangle]
pub extern "C" fn ee_units_resolve(id: u32) -> *mut c_void {
    REGISTRY.resolve(id)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_process_registry_round_trips() {
        // ids no other test of this crate uses (the static is shared by the test binary's threads)
        let id = 0xfe00_0042;
        let unit = 0x1000usize as *mut c_void;
        assert!(ee_units_resolve(id).is_null());
        ee_units_register(id, unit);
        assert_eq!(ee_units_resolve(id), unit);
        assert!(!ee_units_unregister(id, 0x2000usize as *mut c_void));
        assert!(ee_units_unregister(id, unit));
        assert!(ee_units_resolve(id).is_null());
    }
}
